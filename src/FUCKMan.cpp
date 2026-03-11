#include "FUCKMan.h"
#include "FUCKHost.h"

#include "ImGui/IconsFontAwesome6.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"
#include "System/Compat.h"
#include "System/Hotkeys.h"
#include "System/Input.h"
#include "System/Settings.h"

static std::unordered_map<std::string, bool> s_windowCollapseStates;
static std::unordered_map<std::string, bool> s_windowWasCollapsed;
static std::unordered_map<std::string, ImVec2> s_windowPreCollapseSizes;

// Helper to keep windows within the visible viewport
static void ClampWindowToScreen(ImVec2& pos, const ImVec2& size)
{
	const ImGuiIO& io = ImGui::GetIO();
	// Simple AABB clamping
	if (pos.x + size.x > io.DisplaySize.x)
		pos.x = std::max(0.0f, io.DisplaySize.x - size.x);
	if (pos.y + size.y > io.DisplaySize.y)
		pos.y = std::max(0.0f, io.DisplaySize.y - size.y);
	if (pos.x < 0.0f)
		pos.x = 0.0f;
	if (pos.y < 0.0f)
		pos.y = 0.0f;
}

FUCKMan::FUCKMan()
{
	FUCK::GetInterface() = FUCK::Host::CreateInterface();
	_activeTool = nullptr;

	RegisterWindow(&_themeEditorWindow);
}

// ==========================================
// Registration & Callbacks
// ==========================================

void FUCKMan::RegisterTool(ITool* a_tool)
{
	if (!a_tool)
		return;

	// 1. Pointer Check
	if (std::find(_tools.begin(), _tools.end(), a_tool) != _tools.end()) {
		return;
	}

	// 2. Name Collision Check
	auto it = std::find_if(_tools.begin(), _tools.end(), [&](ITool* existing) {
		return existing && (strcmp(existing->Name(), a_tool->Name()) == 0);
	});

	if (it != _tools.end()) {
		return;
	}

	_tools.push_back(a_tool);
}

void FUCKMan::RegisterWindow(IWindow* a_window)
{
	if (!a_window)
		return;

	// 1. Pointer Check
	if (std::find(_windows.begin(), _windows.end(), a_window) != _windows.end()) {
		return;
	}

	// 2. Title Collision Check
	auto it = std::find_if(_windows.begin(), _windows.end(), [&](IWindow* existing) {
		return existing && (strcmp(existing->Title(), a_window->Title()) == 0);
	});

	if (it != _windows.end()) {
		return;
	}

	_windows.push_back(a_window);
}

// ==========================================
// Input Processing
// ==========================================

bool FUCKMan::ProcessAsyncInput(const RE::InputEvent* const* a_event)
{
	// 1. Active Tool Input (Priority)
	if (_activeTool && _activeTool->OnAsyncInput(a_event)) {
		return true;
	}

	if (_isOpen || IsInputBlocked()) {
		// 2. ESC / Close Logic (Priority over Global Hotkeys)
		if (_isOpen || IsInputBlocked()) {
			if (MANAGER(Input)->IsInputPressed(a_event, Hotkeys::Manager::EscapeKey())) {
				bool handled = false;

				// A. Close Child Windows with kCloseOnEsc flag
				for (auto* win : _windows) {
					if (win->IsOpen() && (win->GetFlags() & WindowFlags::kCloseOnEsc)) {
						win->SetOpen(false);
						UpdateGameState();
						handled = true;
					}
				}

				// B. Close Main Menu
				if (!handled && _isOpen) {
					Close();
					handled = true;
				}

				if (handled)
					return true;
			}
		}
	}

	// 3. Framework Global Hotkeys (Toggle Menu)
	MANAGER(Hotkeys)->ProcessInput(a_event);

	// 4. Background Tool Input
	for (auto* tool : _tools) {
		if (tool != _activeTool && tool->OnAsyncInput(a_event)) {
			return true;
		}
	}

	// 5. Block Game Input if Menu/Windows are blocking
	return IsInputBlocked();
}

// ==========================================
// Settings & State Management
// ==========================================

void FUCKMan::ResetSettings()
{
	auto scale = FUCK::GetResolutionScale();
	if (scale < 0.1f)
		scale = 1.0f;

	_windowPos = { 100.0f * scale, 100.0f * scale };
	_windowSize = { 1280.0f * scale, 800.0f * scale };
	_globalPauseType = PauseType::kNone;

	_userScale = 1.0f;
	_sidebarOnRight = false;
	_injectSystemMenu = true;
	_replaceHelpMenu = false;

	if (_isOpen) {
		ClampWindowToScreen(_windowPos, _windowSize);
		ImGui::SetWindowPos(_windowPos);
		ImGui::SetWindowSize(_windowSize);
		ImGui::Styles::GetSingleton()->RefreshStyle();
		MANAGER(IconFont)->ReloadFonts();
	}
}

void FUCKMan::LoadSettings(const CSimpleIniA& a_ini)
{
	double x = a_ini.GetDoubleValue("Window", "X", -1.0);
	double y = a_ini.GetDoubleValue("Window", "Y", -1.0);
	double w = a_ini.GetDoubleValue("Window", "Width", -1.0);
	double h = a_ini.GetDoubleValue("Window", "Height", -1.0);

	if (x == -1.0 || y == -1.0 || w == -1.0 || h == -1.0) {
		ResetSettings();
	} else {
		_windowPos = { (float)x, (float)y };
		_windowSize = { (float)w, (float)h };
	}

	_globalPauseType = static_cast<PauseType>(a_ini.GetLongValue("Settings", "iGlobalPauseType", (int)_globalPauseType));
	_sidebarOnRight = a_ini.GetBoolValue("Settings", "bSidebarOnRight", _sidebarOnRight);
	_injectSystemMenu = a_ini.GetBoolValue("Settings", "bInjectSystemMenu", _injectSystemMenu);
	_replaceHelpMenu = a_ini.GetBoolValue("Settings", "bReplaceHelpMenu", _replaceHelpMenu);

	// Theme Editor State
	_themeEditorWindow._lastPos.x = (float)a_ini.GetDoubleValue("ThemeEditor", "X", _themeEditorWindow._lastPos.x);
	_themeEditorWindow._lastPos.y = (float)a_ini.GetDoubleValue("ThemeEditor", "Y", _themeEditorWindow._lastPos.y);
	_themeEditorWindow._lastSize.x = (float)a_ini.GetDoubleValue("ThemeEditor", "Width", _themeEditorWindow._lastSize.x);
	_themeEditorWindow._lastSize.y = (float)a_ini.GetDoubleValue("ThemeEditor", "Height", _themeEditorWindow._lastSize.y);

	// Check for first-run sentinel (-1.0)
	float loadedScale = (float)a_ini.GetDoubleValue("Settings", "fUserScale", -1.0);
	if (loadedScale == -1.0f) {
		_userScale = 1.0f;
	} else {
		_userScale = std::clamp(loadedScale, 0.5f, 2.0f);
	}

	_lastSavedPos = _windowPos;
	_lastSavedSize = _windowSize;
	_settingsLoaded = true;
}

void FUCKMan::SaveSettings(CSimpleIniA& a_ini)
{
	a_ini.SetDoubleValue("Window", "X", _windowPos.x);
	a_ini.SetDoubleValue("Window", "Y", _windowPos.y);
	a_ini.SetDoubleValue("Window", "Width", _windowSize.x);
	a_ini.SetDoubleValue("Window", "Height", _windowSize.y);
	a_ini.SetLongValue("Settings", "iGlobalPauseType", (int)_globalPauseType);
	a_ini.SetDoubleValue("Settings", "fUserScale", _userScale);
	a_ini.SetBoolValue("Settings", "bSidebarOnRight", _sidebarOnRight);
	a_ini.SetBoolValue("Settings", "bInjectSystemMenu", _injectSystemMenu);
	a_ini.SetBoolValue("Settings", "bReplaceHelpMenu", _replaceHelpMenu);

	// Theme Editor State
	a_ini.SetDoubleValue("ThemeEditor", "X", _themeEditorWindow._lastPos.x);
	a_ini.SetDoubleValue("ThemeEditor", "Y", _themeEditorWindow._lastPos.y);
	a_ini.SetDoubleValue("ThemeEditor", "Width", _themeEditorWindow._lastSize.x);
	a_ini.SetDoubleValue("ThemeEditor", "Height", _themeEditorWindow._lastSize.y);

	MANAGER(Hotkeys)->SaveHotKeys(a_ini);
}

void FUCKMan::SetVanityBlocked(bool blocked) { _isVanityBlocked = blocked; }
void FUCKMan::SetManualHardPause(bool paused) { _apiHardPause = paused; }
void FUCKMan::SetManualSoftPause(bool paused) { _apiSoftPause = paused; }
void FUCKMan::SetForceCursor(bool force) { _forceCursor = force; }
void FUCKMan::SuspendRendering(bool suspend) { _suspendRendering = suspend; }

void FUCKMan::UpdateGameState()
{
	bool targetSoft = _apiSoftPause;
	bool targetHard = _apiHardPause;
	bool targetBlur = false;
	bool targetHideHUD = false;
	bool targetVanity = _isVanityBlocked;

	// 1. Main Menu State
	bool targetiHUDDisabled = _isOpen;

	if (_isOpen) {
		targetVanity = true;
		targetHideHUD = true;
		if (_globalPauseType == PauseType::kSoft)
			targetSoft = true;
		if (_globalPauseType == PauseType::kHard)
			targetHard = true;
	}

	// 2. Window Overrides
	for (auto* win : _windows) {
		if (win->IsOpen()) {
			WindowFlags f = win->GetFlags();

			if (f & WindowFlags::kPauseSoft)
				targetSoft = true;
			if (f & WindowFlags::kPauseHard)
				targetHard = true;
			if (f & WindowFlags::kBlurBackground)
				targetBlur = true;
			if (f & WindowFlags::kBlockVanity)
				targetVanity = true;
			if (f & WindowFlags::kHideHUD) {
				targetHideHUD = true;
				targetiHUDDisabled = true;
			}
		}
	}

	// 3. Apply States

	Compat::ImmersiveHUD::SetDisabled(targetiHUDDisabled);

	// HUD
	if (targetHideHUD != _isHudHidden) {
		targetHideHUD ? RE::SendHUDMessage::PushHUDMode("WorldMapMode") : RE::SendHUDMessage::PopHUDMode("WorldMapMode");
		_isHudHidden = targetHideHUD;
	}

	// Soft Pause
	if (targetSoft != _isGameSoftPaused) {
		if (auto main = RE::Main::GetSingleton())
			main->freezeTime = targetSoft;
		_isGameSoftPaused = targetSoft;
	}

	// Hard Pause
	if (targetHard != _isGameHardPaused) {
		if (auto ui = RE::UI::GetSingleton()) {
			targetHard ? ui->numPausesGame++ : ui->numPausesGame--;
		}
		_isGameHardPaused = targetHard;
	}

	// Blur
	if (targetBlur != _isGameBlurred) {
		auto bm = RE::UIBlurManager::GetSingleton();
		targetBlur ? bm->IncrementBlurCount() : bm->DecrementBlurCount();
		_isGameBlurred = targetBlur;
	}

	// Vanity
	if (targetVanity && RE::PlayerCamera::GetSingleton()) {
		RE::PlayerCamera::GetSingleton()->idleTimer = 0.0f;
	}
}

// ==========================================
// Accessors & Queries
// ==========================================

bool FUCKMan::ShouldRender() const
{
	if (_suspendRendering)
		return false;
	if (_isOpen)
		return true;
	for (const auto* win : _windows) {
		if (win->IsOpen())
			return true;
	}
	return false;
}

bool FUCKMan::IsInputBlocked() const
{
	if (_isOpen)
		return true;
	for (const auto* win : _windows) {
		if (win->IsOpen() && !(win->GetFlags() & WindowFlags::kPassInputToGame)) {
			return true;
		}
	}
	return false;
}

bool FUCKMan::IsCursorForced() const
{
	return _forceCursor;
}

// ==========================================
// Open / Close Logic
// ==========================================

void FUCKMan::Open()
{
	if (_isOpen)
		return;

	_isOpen = true;

	_isCollapsed = false;

	Input::Manager::GetSingleton()->PushContext({ "FUCK", 100, true });
	_forceCursor = false;

	MANAGER(Input)->ResetCursorState();

	ImGui::Styles::GetSingleton()->OnStyleRefresh();

	if (_activeTool)
		_activeTool->OnOpen();

	_settingsLoaded = true;

	RE::PlaySound("UIMenuOK");
}

void FUCKMan::Close()
{
	FUCK::AbortBinding();

	if (!_isOpen)
		return;
	_isOpen = false;

	if (_activeTool)
		_activeTool->OnClose();

	Input::Manager::GetSingleton()->PopContext("FUCK");
	_forceCursor = false;
	MANAGER(Input)->ResetCursorState();

	UpdateGameState();

	// Save to FUCK_Custom.ini
	Settings::GetSingleton()->Save(FileType::kSettings, [](CSimpleIniA& ini) {
		FUCKMan::GetSingleton()->SaveSettings(ini);
	});

	RE::PlaySound("UIMenuCancel");
}

void FUCKMan::Toggle()
{
	_isOpen ? Close() : Open();
}

RE::BSEventNotifyControl FUCKMan::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (!a_event)
		return RE::BSEventNotifyControl::kContinue;

	if (a_event->opening) {
		if (a_event->menuName == RE::MainMenu::MENU_NAME) {
			ImGui::ClearFormCaches();
		}

		// Auto-Close list
		static const std::vector<std::string> closeOnOpen = {
			RE::Console::MENU_NAME.data(), RE::ContainerMenu::MENU_NAME.data(),
			RE::JournalMenu::MENU_NAME.data(), RE::InventoryMenu::MENU_NAME.data(),
			RE::MapMenu::MENU_NAME.data(), RE::DialogueMenu::MENU_NAME.data()
		};

		if (std::ranges::find(closeOnOpen, a_event->menuName.data()) != closeOnOpen.end()) {
			if (_isOpen)
				Close();
			for (auto* win : _windows) {
				if (win->IsOpen() && (win->GetFlags() & WindowFlags::kCloseOnGameMenu)) {
					win->SetOpen(false);
					UpdateGameState();
				}
			}
		}
	}
	return RE::BSEventNotifyControl::kContinue;
}

// ==========================================
// Rendering Loop
// ==========================================

void FUCKMan::Draw()
{
	UpdateGameState();

	const float resScale = FUCK::GetResolutionScale();
	const float uiScale = resScale;

	// Chrome metrics — always based on resScale only, never _userScale
	const float scaledTextH = FUCK::GetTextLineHeight();
	const float uiTextH = scaledTextH;  // text height at base scale

	const float headerPadding = 3.0f * uiScale;
	const float titleH = uiTextH + (headerPadding * 2.0f);
	const float padBase = 15.0f * uiScale;

	// Content scale helper — pushes _userScale on top of the base style for content regions only
	auto pushContentScale = [&]() {
		ImGui::SetWindowFontScale(_userScale);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * uiScale * _userScale, 3.0f * uiScale * _userScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * uiScale * _userScale, 4.0f * uiScale * _userScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4.0f * uiScale * _userScale, 4.0f * uiScale * _userScale));
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 20.0f * uiScale * _userScale);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12.0f * uiScale * _userScale);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 10.0f * uiScale * _userScale);
	};

	auto popContentScale = [&]() {
		ImGui::PopStyleVar(6);
		ImGui::SetWindowFontScale(1.0f);
	};

	// ------------------------------------------------------------------------
	// Overlay Render Pass
	// ------------------------------------------------------------------------
	if (_activeTool) {
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
		ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
		                         ImGuiWindowFlags_NoInputs |
		                         ImGuiWindowFlags_NoBackground |
		                         ImGuiWindowFlags_NoNav |
		                         ImGuiWindowFlags_NoBringToFrontOnFocus |
		                         ImGuiWindowFlags_NoFocusOnAppearing;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (ImGui::Begin("##ToolOverlayLayer", nullptr, flags)) {
			pushContentScale();
			_activeTool->RenderOverlay();
			popContentScale();
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	// ------------------------------------------------------------------------
	// 1. Draw Registered External Windows
	// ------------------------------------------------------------------------
	for (auto* win : _windows) {
		if (win->IsOpen()) {
			if (win->GetFlags() & WindowFlags::kCustomRender) {
				win->Draw();
				continue;
			}

			const std::string title = win->Title();
			int flags = 0;

			// --- Flags Setup ---
			bool noDecoration = (win->GetFlags() & WindowFlags::kNoDecoration);

			flags |= ImGuiWindowFlags_NoTitleBar;
			if (noDecoration) {
				s_windowCollapseStates[title] = false;
				s_windowWasCollapsed[title] = false;
				flags |= ImGuiWindowFlags_NoDecoration;
			} else {
				flags |= ImGuiWindowFlags_NoScrollbar;
			}

			if (win->GetFlags() & WindowFlags::kNoBackground)
				flags |= ImGuiWindowFlags_NoBackground;

			if ((win->GetFlags() & WindowFlags::kPassInputToGame) && !IsInputBlocked())
				flags |= ImGuiWindowFlags_NoInputs;

			// --- Collapse Logic ---
			bool isCollapsed = s_windowCollapseStates[title];
			bool wasCollapsed = s_windowWasCollapsed[title];
			s_windowWasCollapsed[title] = isCollapsed;

			// Get Metrics from Interface
			ImVec2 targetSize = win->GetDefaultSize();

			// Handle collapse override
			if (isCollapsed) {
				float targetW = targetSize.x;
				if (s_windowPreCollapseSizes.find(title) != s_windowPreCollapseSizes.end()) {
					targetW = s_windowPreCollapseSizes[title].x;
				}
				targetSize = ImVec2(targetW, titleH);
				flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
			} else if (wasCollapsed) {
				if (s_windowPreCollapseSizes.find(title) != s_windowPreCollapseSizes.end()) {
					targetSize = s_windowPreCollapseSizes[title];
				}
			}

			// --- Position Logic ---
			ImVec2 requestedPos;
			if (win->GetRequestedPos(requestedPos)) {
				ClampWindowToScreen(requestedPos, targetSize);
				FUCK::SetNextWindowPos(requestedPos, ImGuiCond_Appearing);
			} else {
				// Default position for new windows
				ImVec2 defPos = win->GetDefaultPos();
				ClampWindowToScreen(defPos, targetSize);
				FUCK::SetNextWindowPos(defPos, ImGuiCond_FirstUseEver);
			}

			// Set size
			FUCK::SetNextWindowSize(targetSize, isCollapsed || wasCollapsed ? ImGuiCond_Always : ImGuiCond_FirstUseEver);

			bool open = true;

			if (!noDecoration) {
				FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			} else {
				FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padBase, padBase));
			}

			if (FUCK::BeginWindow(title.c_str(), &open, flags)) {
				if (!isCollapsed) {
					s_windowPreCollapseSizes[title] = FUCK::GetWindowSize();
				}

				win->UpdateState(FUCK::GetWindowPos(), FUCK::GetWindowSize());

				if (win->GetFlags() & WindowFlags::kExtendBorder)
					FUCK::ExtendWindowPastBorder();

				if (!noDecoration) {
					// --- Window Chrome Decoration (unscaled) ---
					float midY = titleH * 0.5f;
					float winWidth = FUCK::GetWindowSize().x;

					ImVec2 headerStartCursor = FUCK::GetCursorPos();
					ImVec2 cursorScreen = FUCK::GetCursorScreenPos();

					FUCK::BeginGroup();

					// 1. Collapse Icon
					static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
					float iconW = 0.0f;

					if (iconArrow) {
						// A. Draw Button
						if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(titleH + 20.0f, titleH))) {
							s_windowCollapseStates[title] = !isCollapsed;
						}

						// B. Determine Color
						bool isHovered = ImGui::IsItemHovered();
						ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

						// C. Draw Icon with fixed size (compensate for user scale)
						ImVec2 size = iconArrow->size;
						size.x /= _userScale;
						size.y /= _userScale;
						ImVec2 drawSize = !isCollapsed ? ImVec2(size.y, size.x) : size;
						float iconYOffset = midY - (drawSize.y * 0.5f);

						ImVec2 drawPos = cursorScreen;
						drawPos.x += (8.0f * uiScale);
						drawPos.y += iconYOffset;

						ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, drawSize, iconColor,
							!isCollapsed ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);

						iconW = titleH + 20.0f;
					}

					// 2. Title Text										
					ImFont* baseFont = ImGui::GetFont();
					float uiFontSize = 22.0f * 0.9f;
					float textY = (titleH - uiFontSize) * 0.5f;

					FUCK::SetCursorPos({ iconW, textY });

					ImGui::GetWindowDrawList()->AddText(baseFont, uiFontSize,
						FUCK::GetCursorScreenPos(), ImGui::GetColorU32(ImGuiCol_Text), title.c_str());

					// 3. Close Button
					const float btnSize = titleH;
					const float btnX = winWidth - btnSize - (4.0f * uiScale);

					FUCK::SetCursorPos({ btnX, 0 });
					if (ImGui::InvisibleButton("##WinClose", ImVec2(btnSize, btnSize))) {
						open = false;
					}

					{
						bool btnHovered = ImGui::IsItemHovered();
						const char* xIcon = ICON_FA_XMARK;

						float uiFontSize2 = ImGui::GetStyle().FontSizeBase;

						ImGui::PushFont(nullptr, uiFontSize2);
						ImVec2 textSize = ImGui::CalcTextSize(xIcon);
						ImGui::PopFont();

						ImVec2 btnScreenPos = ImGui::GetItemRectMin();
						ImVec2 textPos = {
							btnScreenPos.x + (btnSize - textSize.x) * 0.5f,
							btnScreenPos.y + (btnSize - textSize.y) * 0.5f
						};

						ImU32 xColor = btnHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

						ImGui::GetWindowDrawList()->AddText(
							baseFont,
							uiFontSize2,
							textPos,
							xColor,
							xIcon);
					}

					FUCK::EndGroup();

					// Double Click Header Interaction
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
						if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
							s_windowCollapseStates[title] = !isCollapsed;
						}
					}

					// 4. Separator
					FUCK::SetCursorPos({ headerStartCursor.x, titleH });

					if (isCollapsed) {
						ImVec2 sepStart = FUCK::GetCursorScreenPos();
						ImVec2 sepEnd = { sepStart.x + winWidth, sepStart.y };
						ImGui::GetWindowDrawList()->AddLine(sepStart, sepEnd,
							ImGui::GetColorU32(ImGuiCol_Separator),
							ImGui::GetStyle().SeparatorTextBorderSize);
					} else {
						FUCK::SeparatorThick();
					}

					// 5. Content Child (scaled)
					if (!isCollapsed) {
						float childY = titleH + (1.0f * uiScale);
						FUCK::SetCursorPos({ 0, childY });
						FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padBase, 0.0f));

						std::string childId = "##Content_" + title;
						ImGuiChildFlags childFlags = ImGuiChildFlags_AlwaysUseWindowPadding;
						ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground;

						if (ImGui::BeginChild(childId.c_str(), ImVec2(0, 0), childFlags, windowFlags)) {
							FUCK::Dummy(ImVec2(0, padBase));

							ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
							pushContentScale();
							win->Draw();
							popContentScale();
							ImGui::PopItemWidth();

							FUCK::Dummy(ImVec2(0, padBase));
						}
						ImGui::EndChild();

						FUCK::PopStyleVar();
					}
				} else {
					// --- No Chrome Decoration ---
					pushContentScale();
					win->Draw();
					popContentScale();
				}
			}
			FUCK::EndWindow();

			FUCK::PopStyleVar();

			if (!open) {
				win->SetOpen(false);
				UpdateGameState();
			}
		}
	}

	// ------------------------------------------------------------------------
	// 2. Main FUCK Menu (Sidebar & Settings)
	// ------------------------------------------------------------------------
	if (!_isOpen)
		return;

	ImGui::GetIO().MouseDrawCursor = false;

	static bool isCollapsed = false;
	static bool wasCollapsed = false;

	if (_settingsLoaded) {
		ClampWindowToScreen(_windowPos, _windowSize);
		FUCK::SetNextWindowPos(_windowPos, ImGuiCond_Always);
		if (!isCollapsed) {
			FUCK::SetNextWindowSize(_windowSize, ImGuiCond_Always);
		}
		_settingsLoaded = false;
	} else {
		if (isCollapsed) {
			FUCK::SetNextWindowSize(ImVec2(_windowSize.x, titleH));
		} else if (wasCollapsed && !isCollapsed) {
			FUCK::SetNextWindowSize(_windowSize);
		} else {
			FUCK::SetNextWindowSize(ImVec2(1000.0f * uiScale, 600.0f * uiScale), ImGuiCond_FirstUseEver);
		}
	}

	wasCollapsed = isCollapsed;

	FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	std::string windowTitle = std::format("##{}", "$FUCK_Title"_T);
	bool wantsOpen = true;

	ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
	if (isCollapsed)
		winFlags |= ImGuiWindowFlags_NoResize;

	if (FUCK::BeginWindow(windowTitle.c_str(), &wantsOpen, winFlags)) {
		FUCK::ExtendWindowPastBorder();

		if (!isCollapsed) {
			_windowPos = FUCK::GetWindowPos();
			_windowSize = FUCK::GetWindowSize();

			// Auto-save settings on move/resize end
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				if (_windowPos.x != _lastSavedPos.x || _windowPos.y != _lastSavedPos.y ||
					_windowSize.x != _lastSavedSize.x || _windowSize.y != _lastSavedSize.y) {
					_lastSavedPos = _windowPos;
					_lastSavedSize = _windowSize;
					Settings::GetSingleton()->Save(FileType::kSettings, [](CSimpleIniA& ini) {
						FUCKMan::GetSingleton()->SaveSettings(ini);
					});
				}
			}
		}

		// -- Custom Title Bar (unscaled) --
		{
			ImVec2 cursorScreen = FUCK::GetCursorScreenPos();

			FUCK::BeginGroup();

			float winWidth = FUCK::GetWindowSize().x;
			float midY = titleH * 0.5f;

			// 1. Collapse Icon (Left)
			static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
			if (iconArrow) {
				// A. Draw Button
				FUCK::SetCursorPos({ 0, 0 });
				if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(titleH + 20.0f, titleH))) {
					isCollapsed = !isCollapsed;
				}

				// B. Hover Color
				bool isHovered = ImGui::IsItemHovered();
				ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				// C. Draw Icon with fixed size
				ImVec2 size = iconArrow->size;
				size.x /= _userScale;
				size.y /= _userScale;

				bool pointDown = !isCollapsed;
				float actualIconHeight = pointDown ? size.x : size.y;
				float iconY = midY - (actualIconHeight * 0.5f);

				ImVec2 drawPos = cursorScreen;
				drawPos.x += (8.0f * uiScale);
				drawPos.y += iconY;

				ImVec2 drawSize = pointDown ? ImVec2(size.y, size.x) : size;

				ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, drawSize, iconColor,
					!isCollapsed ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
			}

			// 2. Close Button
			float btnSize = titleH;
			float xPos = winWidth - btnSize - headerPadding;

			FUCK::SetCursorPos({ xPos, 0.0f });
			ImVec2 btnCursor = FUCK::GetCursorScreenPos();

			if (ImGui::InvisibleButton("##CloseBtn", ImVec2(btnSize, btnSize))) {
				wantsOpen = false;
			}

			{
				const char* xIcon = ICON_FA_XMARK;
				float uiFontSize = ImGui::GetStyle().FontSizeBase;

				ImGui::PushFont(nullptr, uiFontSize);
				ImVec2 textSize = ImGui::CalcTextSize(xIcon);
				ImGui::PopFont();

				ImVec2 textPos = {
					btnCursor.x + (btnSize - textSize.x) * 0.5f,
					btnCursor.y + (btnSize - textSize.y) * 0.5f
				};

				ImU32 xColor = ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				ImGui::GetWindowDrawList()->AddText(
					ImGui::GetFont(),
					uiFontSize,
					textPos,
					xColor,
					xIcon);
			}

			FUCK::EndGroup();

			// Double Click Header
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
				ImVec2 m = ImGui::GetMousePos();
				ImVec2 wP = FUCK::GetWindowPos();
				if (m.x >= wP.x && m.x <= wP.x + winWidth && m.y >= wP.y && m.y <= wP.y + titleH) {
					isCollapsed = !isCollapsed;
				}
			}
		}

		// -- Content --
		if (!isCollapsed) {
			float contentY = titleH;
			FUCK::SetCursorPos({ 0, contentY });

			float availHeight = FUCK::GetContentRegionAvail().y;
			const float sidebarWidth = 250.0f * uiScale;

			// -- Sidebar (unscaled) --
			auto renderSidebar = [&]() {
				const float itemHeight = 30.0f * uiScale;
				const float topPadding = 2.0f * uiScale;
				const float bottomPadding = 2.0f * uiScale;
				const float indent = 15.0f * uiScale;
				const float textVisualOffset = 1.0f * uiScale;

				const float sidebarFontSize = 22.0f * 0.9f;

				std::vector<ITool*> looseTools;
				std::map<std::string, std::vector<ITool*>> toolGroups;

				for (auto* tool : _tools) {
					if (!tool->ShowInSidebar())
						continue;
					const char* grp = tool->Group();
					if (grp && *grp)
						toolGroups[grp].push_back(tool);
					else
						looseTools.push_back(tool);
				}

				struct SidebarEntry
				{
					std::string label;
					bool isGroup;
					ITool* tool = nullptr;
					std::vector<ITool*>* tools = nullptr;
				};

				std::vector<SidebarEntry> entries;
				entries.reserve(looseTools.size() + toolGroups.size());

				for (auto* t : looseTools) {
					entries.push_back({ t->Name(), false, t, nullptr });
				}

				for (auto& [name, tools] : toolGroups) {
					std::sort(tools.begin(), tools.end(), [](ITool* a, ITool* b) {
						return _stricmp(a->Name(), b->Name()) < 0;
					});
					entries.push_back({ name, true, nullptr, &tools });
				}

				std::sort(entries.begin(), entries.end(), [](const SidebarEntry& a, const SidebarEntry& b) {
					return _stricmp(a.label.c_str(), b.label.c_str()) < 0;
				});

				static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
				
				// Draw Icon with fixed size
				float baseIconWidth = iconArrow ? (iconArrow->size.x / _userScale) : 20.0f;
				float alignedTextOffset = (indent * 0.5f) + baseIconWidth + headerPadding;

				FUCK::BeginChild("Sidebar", ImVec2(sidebarWidth, availHeight), true, ImGuiWindowFlags_None);
				{
					FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
					ImVec2 headerStart = FUCK::GetCursorPos();
					headerStart.y += topPadding;

					// --- HEADER: TOOLS (Centered) ---
					FUCK::SetCursorPos(headerStart);
					FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), sidebarFontSize);
					float textH = FUCK::GetTextLineHeight();
					float sidebarW = FUCK::GetContentRegionAvail().x;
					const char* headerText = "$FUCK_Tools"_T;
					float headerW = ImGui::CalcTextSize(headerText).x;

					FUCK::SetCursorPosX(headerStart.x + (sidebarW - headerW) * 0.5f);
					FUCK::SetCursorPosY(headerStart.y + (itemHeight - textH) * 0.5f + textVisualOffset);
					FUCK::Text(headerText);
					FUCK::PopFont();

					FUCK::SetCursorPos(ImVec2(headerStart.x, headerStart.y + itemHeight));
					FUCK::SeparatorThick();

					auto RenderSidebarItem = [&](ITool* tool, const char* label, float extraIndent = 0.0f) {
						// Push ID to prevent conflicts if multiple tools have same name
						ImGui::PushID(tool);

						bool isSelected = (_activeTool == tool);
						const auto cursorPos = FUCK::GetCursorPos();
						std::string idLabel = std::format("##{}", label);

						if (FUCK::Selectable(idLabel.c_str(), isSelected, 0, ImVec2(0, itemHeight))) {
							if (_activeTool && _activeTool != tool) {
								FUCK::AbortBinding();
								_activeTool->OnClose();
							}
							RE::PlaySound("UIMenuOK");
							_activeTool = tool;
							_activeTool->OnOpen();
						}

						ImVec2 endPos = FUCK::GetCursorPos();
						FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), sidebarFontSize);
						float textY = cursorPos.y + (itemHeight - textH) * 0.5f + textVisualOffset;

						FUCK::SetCursorPos({ cursorPos.x + alignedTextOffset + extraIndent, textY });
						FUCK::Text(label);
						FUCK::PopFont();
						FUCK::SetCursorPos(endPos);

						ImGui::PopID();
					};

					auto RenderSidebarGroup = [&](const std::string& groupName, std::vector<ITool*>& tools) {
						FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), sidebarFontSize);

						// Custom TreeNode rendering
						ImGui::PushID(groupName.c_str());
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						ImGuiID id = window->GetID(groupName.c_str());
						bool isOpen = window->DC.StateStorage->GetInt(id, 0);

						ImVec2 pos = window->DC.CursorPos;
						float frameHeight = itemHeight;
						ImRect bb(pos, pos + ImVec2(FUCK::GetContentRegionAvail().x, frameHeight));

						ImGui::ItemSize(bb);
						if (ImGui::ItemAdd(bb, id)) {
							bool hovered, held;
							if (ImGui::ButtonBehavior(bb, id, &hovered, &held, 0)) {
								isOpen = !isOpen;
								window->DC.StateStorage->SetInt(id, isOpen);
								RE::PlaySound(isOpen ? "UIMenuFocus" : "UIMenuCancel");
							}
							if (hovered)
								ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_HeaderHovered), false);

							// Draw chevron
							if (iconArrow) {
								ImU32 col = ImGui::GetDynamicTextColor(hovered);
								ImVec2 iconSize = iconArrow->size;
								// Divide by user scale to maintain constant size
								iconSize.x /= _userScale;
								iconSize.y /= _userScale;

								ImVec2 drawSize = isOpen ? ImVec2(iconSize.y, iconSize.x) : iconSize;
								float offY = (frameHeight - drawSize.y) * 0.5f;
								ImVec2 drawPos = { pos.x + (15.0f * uiScale) * 0.5f, pos.y + offY };

								ImGui::DrawArrowIcon(window->DrawList, drawPos, drawSize, col,
									isOpen ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
							}

							float textY = bb.Min.y + (frameHeight - ImGui::CalcTextSize(groupName.c_str()).y) * 0.5f + textVisualOffset;
							ImGui::RenderText({ pos.x + alignedTextOffset, textY }, groupName.c_str());
						}

						if (isOpen) {
							for (auto* tool : tools) {
								RenderSidebarItem(tool, tool->Name(), indent);
							}
						}

						ImGui::PopID();
						FUCK::PopFont();
					};

					for (auto& entry : entries) {
						if (entry.isGroup) {
							RenderSidebarGroup(entry.label, *entry.tools);
						} else {
							RenderSidebarItem(entry.tool, entry.label.c_str());
						}
					}

					// --- FOOTER: SETTINGS (Centered) ---
					float childHeight = FUCK::GetWindowSize().y;
					float separatorHeight = 1.0f;
					float settingsY = childHeight - itemHeight - bottomPadding;

					float minSettingY = FUCK::GetCursorPos().y + separatorHeight;
					if (settingsY < minSettingY)
						settingsY = minSettingY;

					FUCK::SetCursorPosY(settingsY - separatorHeight);
					FUCK::SeparatorThick();

					FUCK::SetCursorPosY(settingsY);
					{
						auto* settingsTool = &_settingsTool;
						bool isSelected = (_activeTool == settingsTool);
						const auto cursorPos = FUCK::GetCursorPos();

						if (FUCK::Selectable("##SETTINGS", isSelected, 0, ImVec2(0, itemHeight))) {
							if (_activeTool && _activeTool != settingsTool) {
								FUCK::AbortBinding();
								_activeTool->OnClose();
							}
							RE::PlaySound("UIMenuOK");
							_activeTool = settingsTool;
							_activeTool->OnOpen();
						}

						ImVec2 endPos = FUCK::GetCursorPos();
						FUCK::SetCursorPos(cursorPos);
						FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), sidebarFontSize);

						const char* settingText = "$FUCK_Settings"_T;
						float setW = ImGui::CalcTextSize(settingText).x;

						FUCK::SetCursorPosX(cursorPos.x + (sidebarW - setW) * 0.5f);
						FUCK::SetCursorPosY(cursorPos.y + (itemHeight - textH) * 0.5f + textVisualOffset);
						FUCK::Text(settingText);

						FUCK::PopFont();
						FUCK::SetCursorPos(endPos);
					}
					FUCK::PopStyleVar();
					FUCK::Dummy(ImVec2(0, bottomPadding));
				}
				FUCK::EndChild();
			};

			// -- Content (scaled) --
			auto renderContent = [&](float width) {
				FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padBase, 0.0f));
				FUCK::BeginChild("Content", ImVec2(width, availHeight), true, 0);
				{
					pushContentScale();
					FUCK::Dummy(ImVec2(0, padBase));
					if (_activeTool)
						_activeTool->Draw();
					else
						FUCK::CenteredText("$FUCK_NoToolSelected"_T, true);

					FUCK::Dummy(ImVec2(0, 1));
					popContentScale();
				}
				FUCK::EndChild();
				FUCK::PopStyleVar();
			};

			if (_sidebarOnRight) {
				float contentWidth = FUCK::GetContentRegionAvail().x - sidebarWidth - FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing).x;
				renderContent(contentWidth);
				FUCK::SameLine();
				renderSidebar();
			} else {
				renderSidebar();
				FUCK::SameLine();
				renderContent(0.0f);
			}
		}
	}
	FUCK::EndWindow();
	FUCK::PopStyleVar();

	if (!wantsOpen)
		Close();
}
