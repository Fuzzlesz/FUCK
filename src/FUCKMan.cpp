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

	const auto resScale = FUCK::GetResolutionScale();
	const float uiScale = resScale;

	const float scaledTextH = FUCK::GetTextLineHeight();
	const float uiTextH = scaledTextH / _userScale;

	const float headerPadding = 3.0f * uiScale;
	const float titleH = uiTextH + (headerPadding * 2.0f);
	const float padBase = 15.0f * uiScale;

	// ------------------------------------------------------------------------
	// 1. Draw Registered External Windows (Overlays)
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
					// --- Decorated Window Logic ---
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
					// Center vertically using the unscaled text height
					float textY = (titleH - uiTextH) * 0.5f;
					FUCK::SetCursorPos({ iconW, textY });

					// Use base font size manually scaled to UI scale (ignoring user zoom)
					ImFont* baseFont = ImGui::GetFont();
					float uiFontSize = ImGui::GetFontSize() / _userScale;
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

						float targetFontSize = ImGui::GetFontSize() / _userScale;

						ImGui::SetWindowFontScale(1.0f / _userScale);
						ImVec2 textSize = ImGui::CalcTextSize(xIcon);
						ImGui::SetWindowFontScale(1.0f);

						ImVec2 btnScreenPos = ImGui::GetItemRectMin();
						ImVec2 textPos = {
							btnScreenPos.x + (btnSize - textSize.x) * 0.5f,
							btnScreenPos.y + (btnSize - textSize.y) * 0.5f
						};

						ImU32 xColor = btnHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

						ImGui::GetWindowDrawList()->AddText(
							ImGui::GetFont(),
							targetFontSize,
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

					// 5. Content Child (Applies internal padding)
					if (!isCollapsed) {
						float childY = titleH + (1.0f * uiScale);
						FUCK::SetCursorPos({ 0, childY });
						FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padBase, 0.0f));

						std::string childId = "##Content_" + title;
						ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding;

						if (ImGui::BeginChild(childId.c_str(), ImVec2(0, 0), false, childFlags)) {
							FUCK::Dummy(ImVec2(0, padBase));

							ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
							win->Draw();
							ImGui::PopItemWidth();

							FUCK::Dummy(ImVec2(0, padBase));
						}
						ImGui::EndChild();

						FUCK::PopStyleVar();
					}
				} else {
					// --- No Decoration Logic ---
					win->Draw();
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

		// -- Custom Title Bar --
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

			// 2. Close Button (Right)
			float btnSize = titleH;
			float btnY = 0.0f;
			float xPos = winWidth - btnSize - headerPadding;

			FUCK::SetCursorPos({ xPos, btnY });
			ImVec2 btnCursor = FUCK::GetCursorScreenPos();

			if (ImGui::InvisibleButton("##CloseBtn", ImVec2(btnSize, btnSize))) {
				wantsOpen = false;
			}

			// X button with fixed size
			{
				const char* xIcon = ICON_FA_XMARK;
				float textOffsetY = 1.0f * uiScale;

				float targetFontSize = ImGui::GetFontSize() / _userScale;

				ImGui::SetWindowFontScale(1.0f / _userScale);
				ImVec2 textSize = ImGui::CalcTextSize(xIcon);
				ImGui::SetWindowFontScale(1.0f);

				ImVec2 textPos = {
					btnCursor.x + (btnSize - textSize.x) * 0.5f,
					btnCursor.y + (btnSize - textSize.y) * 0.5f + textOffsetY
				};

				ImU32 xColor = ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				ImGui::GetWindowDrawList()->AddText(
					ImGui::GetFont(),
					targetFontSize,
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

			auto renderSidebar = [&]() {
				const float itemHeight = 30.0f * uiScale;
				const float topPadding = 2.0f * uiScale;
				const float bottomPadding = 2.0f * uiScale;
				const float indent = 15.0f * uiScale;
				const float textVisualOffset = 1.0f * uiScale;

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

				FUCK::BeginChild("Sidebar", ImVec2(sidebarWidth, availHeight), true, ImGuiWindowFlags_None);
				{
					FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
					ImVec2 headerStart = FUCK::GetCursorPos();
					headerStart.y += topPadding;

					// --- HEADER: TOOLS (Centered) ---
					FUCK::SetCursorPos(headerStart);
					FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), 22.0f * resScale * 0.9f);
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

					auto RenderSidebarItem = [&](ITool* tool, const char* label) {
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
						FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), 22.0f * resScale * 0.9f);
						float textY = cursorPos.y + (itemHeight - textH) * 0.5f + textVisualOffset;
						FUCK::SetCursorPos({ cursorPos.x + indent, textY });
						FUCK::Text(label);
						FUCK::PopFont();
						FUCK::SetCursorPos(endPos);

						ImGui::PopID();
					};

					for (auto* tool : looseTools) RenderSidebarItem(tool, tool->Name());

					for (auto& [groupName, tools] : toolGroups) {
						FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), 22.0f * resScale * 0.9f);

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
							static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
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

							// Draw text
							// Calculate proper text offset accounting for fixed chevron size
							float baseIconWidth = iconArrow ? (iconArrow->size.x / _userScale) : 20.0f;
							float textOff = (indent * 0.5f) + baseIconWidth + headerPadding;

							float textY = bb.Min.y + (frameHeight - ImGui::CalcTextSize(groupName.c_str()).y) * 0.5f + textVisualOffset;
							ImGui::RenderText({ pos.x + textOff, textY }, groupName.c_str());
						}

						if (isOpen) {
							ImGui::TreePush(groupName.c_str());
							for (auto* tool : tools) {
								FUCK::Indent(indent);
								RenderSidebarItem(tool, tool->Name());
								FUCK::Unindent(indent);
							}
							FUCK::TreePop();
						}

						ImGui::PopID();
						FUCK::PopFont();
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
						FUCK::PushFont(FUCK::GetFont(FUCK_Font::kRegular), 22.0f * resScale * 0.9f);

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

			auto renderContent = [&](float width) {
				FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padBase, 0.0f));
				FUCK::BeginChild("Content", ImVec2(width, availHeight), true, 0);
				{
					FUCK::Dummy(ImVec2(0, padBase));
					if (_activeTool)
						_activeTool->Draw();
					else
						FUCK::CenteredText("$FUCK_NoToolSelected"_T, true);

					FUCK::Dummy(ImVec2(0, 1));
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

// ==========================================
// Settings Tool Implementation
// ==========================================

const char* FUCKMan::SettingsTool::Name() const
{
	return "$FUCK_Settings_Title"_T;
}

bool FUCKMan::SettingsTool::OnAsyncInput(const void* a_event)
{
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();
	return FUCK::UpdateManagedHotkey(a_event, hotkey);
}

void FUCKMan::SettingsTool::OnClose()
{
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();
	FUCK::AbortManagedHotkey(hotkey);
}

void FUCKMan::SettingsTool::Draw()
{
	auto manager = FUCKMan::GetSingleton();
	auto style = ImGui::Styles::GetSingleton();
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();

	if (FUCK::BeginTabBar("SettingsTabs")) {
		// --------------------------------------------------------
		// TAB 1: GENERAL
		// --------------------------------------------------------
		if (FUCK::BeginTabItem("$FUCK_Settings_Tab"_T)) {
			FUCK::Spacing(2);

			FUCK::Header("$FUCK_Settings_Behavior"_T);
			FUCK::Spacing(2);

			const char* pauseTypes[] = { "$FUCK_Settings_PauseNone"_T, "$FUCK_Settings_PauseSoft"_T, "$FUCK_Settings_PauseHard"_T };
			int currentPauseIdx = (int)manager->_globalPauseType;
			FUCK::SetNextItemWidth(-1);
			std::string pauseLabel = std::format("{}##GlobalPauseType", "$FUCK_Settings_GlobalPause"_T);

			if (FUCK::Combo(pauseLabel.c_str(), &currentPauseIdx, pauseTypes, IM_ARRAYSIZE(pauseTypes))) {
				manager->_globalPauseType = (FUCKMan::PauseType)currentPauseIdx;
			}
			FUCK::Spacing();
			FUCK::TextColoredWrapped(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "$FUCK_Settings_PauseDesc"_T);
			FUCK::Spacing(2);

			FUCK::DrawManagedHotkey("$FUCK_Settings_Hotkey"_T, hotkey);

			FUCK::Spacing(2);

			FUCK::Header("$FUCK_Settings_Appearance"_T);
			FUCK::Spacing(2);

			if (FUCK::SliderFloat("$FUCK_Settings_UIScale"_T, &manager->_userScale, 0.5f, 2.0f, "%.2f")) {
				style->RefreshStyle();
				MANAGER(IconFont)->ReloadFonts();
			}

			FUCK::Checkbox("$FUCK_Settings_SidebarOnRight"_T, &manager->_sidebarOnRight, true, true);
			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Settings_Reset"_T)) {
				manager->ResetSettings();
			}
			FUCK::Spacing(4);

			FUCK::Header("$FUCK_Settings_Info"_T);
			FUCK::Spacing(2);

			FUCK::PushStyleColor(ImGuiCol_Text, FUCK::GetStyleColorVec4(ImGuiCol_CheckMark));
			FUCK::TextWrapped("$FUCK_Settings_Desc"_T);
			FUCK::PopStyleColor();
			FUCK::Spacing(2);
			FUCK::TextDisabled("FUCK API Version: %d", FUCK_API_VERSION);

			FUCK::EndTabItem();
		}

		// --------------------------------------------------------
		// TAB 2: STYLES
		// --------------------------------------------------------
		if (FUCK::BeginTabItem("$FUCK_Styles_Title"_T)) {
			FUCK::Spacing(2);

			FUCK::Header("$FUCK_Styles_Presets"_T);
			FUCK::Spacing();

			const auto& presets = style->GetPresets();
			std::string currentPresetName = style->GetCurrentPresetName();

			// Build Combo Items with "----" at the top
			std::vector<const char*> comboItems;
			comboItems.reserve(presets.size() + 1);
			comboItems.push_back("----");

			int currentIdx = 0;

			for (size_t i = 0; i < presets.size(); ++i) {
				comboItems.push_back(presets[i].c_str());
				if (presets[i] == currentPresetName) {
					currentIdx = static_cast<int>(i) + 1;
				}
			}

			if (FUCK::Combo("$FUCK_Styles_SelectPreset"_T, &currentIdx, comboItems.data(), (int)comboItems.size())) {
				if (currentIdx == 0) {
					// User selected "----" -> Reset
					style->ResetToDefaults();
				} else if (currentIdx > 0 && currentIdx <= presets.size()) {
					// User selected a file
					style->LoadPreset(presets[currentIdx - 1]);
				}
			}

			FUCK::Spacing(2);
			FUCK::TextDisabled("$FUCK_Styles_EditorDesc"_T);
			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Styles_OpenThemeEditor"_T)) {
				FUCKMan::GetSingleton()->_themeEditorWindow.SetOpen(true);
			}

			FUCK::Spacing(2);
			FUCK::SeparatorThick();
			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Styles_ResetDefaults"_T)) {
				style->ResetToDefaults();
			}

			FUCK::EndTabItem();
		}

		FUCK::EndTabBar();
	}
}

// ==========================================
// Theme Editor Window Implementation
// ==========================================

void FUCKMan::ThemeEditorWindow::Draw()
{
	auto style = ImGui::Styles::GetSingleton();

	auto ColorPick = [&](const char* label, ImVec4& col) {
		FUCK::LeftLabel(label);
		std::string id = std::format("##{}", label);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kFrameBG_Widget));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kFrameBG_WidgetActive));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kFrameBG_WidgetActive));
		ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kSliderBorder));

		if (ImGui::ColorEdit4(id.c_str(), (float*)&col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoLabel)) {
			style->RefreshStyle();
		}

		if (ImGui::IsItemActivated())
			RE::PlaySound("UIMenuFocus");

		ImGui::PopStyleColor(4);
	};

	if (FUCK::CollapsingHeader("$FUCK_Styles_Presets"_T, ImGuiTreeNodeFlags_DefaultOpen)) {
		FUCK::Spacing();

		static std::vector<std::string> fonts = style->GetAvailableFonts();
		std::string currentFont = style->GetCurrentFont();

		std::vector<const char*> fontPtrs;
		fontPtrs.reserve(fonts.size());
		int currentFontIdx = -1;

		for (size_t i = 0; i < fonts.size(); ++i) {
			fontPtrs.push_back(fonts[i].c_str());
			if (fonts[i] == currentFont) {
				currentFontIdx = static_cast<int>(i);
			}
		}

		if (FUCK::Combo("$FUCK_Styles_Typeface"_T, &currentFontIdx, fontPtrs.data(), (int)fontPtrs.size())) {
			if (currentFontIdx >= 0 && currentFontIdx < fonts.size()) {
				style->SetCurrentFont(fonts[currentFontIdx]);
			}
		}
		FUCK::Spacing();

		static char newPresetNameBuf[64] = "MyNewPreset";
		FUCK::InputText("$FUCK_Styles_NewPresetName"_T, newPresetNameBuf, 64);

		if (FUCK::Button("$FUCK_Styles_SaveNew"_T)) {
			style->SavePreset(newPresetNameBuf);
			style->RefreshStyle();
		}

		FUCK::SameLine();

		std::string currentPreset = style->GetCurrentPresetName();
		if (!currentPreset.empty()) {
			std::string btnLabel = std::format("{} ({})", "$FUCK_Styles_SaveCurrent"_T, currentPreset);
			if (FUCK::Button(btnLabel.c_str())) {
				style->SavePreset(currentPreset);
			}
		}

		FUCK::Spacing(2);
		FUCK::Separator();
		FUCK::Spacing(2);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Window"_T)) {
		ColorPick("$FUCK_Styles_Background"_T, style->user.background);
		ColorPick("$FUCK_Styles_Border"_T, style->user.border);
		if (FUCK::SliderFloat("$FUCK_Styles_BorderSize"_T, &style->user.borderSize, 0.0f, 5.0f)) {
			style->RefreshStyle();
		}

		FUCK::Separator();
		FUCK::LeftLabel("$FUCK_Styles_WindowPadding"_T);
		if (ImGui::DragFloat2("##WinPad", &style->user.windowPadding.x, 0.5f, 0.0f, 30.0f, "%.0f"))
			style->RefreshStyle();

		FUCK::LeftLabel("$FUCK_Styles_ItemSpacing"_T);
		if (ImGui::DragFloat2("##ItemSpace", &style->user.itemSpacing.x, 0.5f, 0.0f, 30.0f, "%.0f"))
			style->RefreshStyle();

		FUCK::LeftLabel("$FUCK_Styles_WindowRounding"_T);
		if (FUCK::SliderFloat("##WinRound", &style->user.windowRounding, 0.0f, 20.0f, "%.0f"))
			style->RefreshStyle();
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Text"_T)) {
		ColorPick("$FUCK_Styles_Primary"_T, style->user.text);
		ColorPick("$FUCK_Styles_Header"_T, style->user.textHeader);
		ColorPick("$FUCK_Styles_HoverActive"_T, style->user.textHovered);
		ColorPick("$FUCK_Styles_Disabled"_T, style->user.textDisabled);
		ColorPick("$FUCK_Styles_NavHighlight"_T, style->user.navHighlight);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Widgets"_T)) {
		ColorPick("$FUCK_Styles_FrameBG"_T, style->user.frameBG_Widget);
		ColorPick("$FUCK_Styles_FrameBGActive"_T, style->user.frameBG_WidgetActive);
		ColorPick("$FUCK_Styles_ButtonColor"_T, style->user.button);

		FUCK::Separator();
		ColorPick("$FUCK_Styles_TabBG"_T, style->user.tab);
		ColorPick("$FUCK_Styles_TabBGActive"_T, style->user.tabHovered);
		ColorPick("$FUCK_Styles_TabBorder"_T, style->user.tabBorder);
		ColorPick("$FUCK_Styles_TabBorderActive"_T, style->user.tabBorderActive);

		FUCK::Separator();
		ColorPick("$FUCK_Styles_ToggleRail"_T, style->user.toggleRail);
		ColorPick("$FUCK_Styles_ToggleRailFill"_T, style->user.toggleRailFilled);
		ColorPick("$FUCK_Styles_ToggleKnob"_T, style->user.toggleKnob);

		FUCK::Separator();
		ColorPick("$FUCK_Styles_SeparatorColor"_T, style->user.separator);
		if (FUCK::SliderFloat("$FUCK_Styles_SeparatorThick"_T, &style->user.separatorThickness, 1.0f, 10.0f, "%.1f")) {
			style->RefreshStyle();
		}

		FUCK::Separator();
		auto DragRound = [&](const char* label, float* v) {
			if (FUCK::SliderFloat(label, v, 0.0f, 12.0f, "%.0f"))
				style->RefreshStyle();
		};

		DragRound("$FUCK_Styles_FrameRounding"_T, &style->user.frameRounding);
		DragRound("$FUCK_Styles_ButtonRounding"_T, &style->user.buttonRounding);
		DragRound("$FUCK_Styles_TabRounding"_T, &style->user.tabRounding);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Combos"_T)) {
		ColorPick("$FUCK_Styles_ListBG"_T, style->user.frameBG);
		ColorPick("$FUCK_Styles_TextBoxBG"_T, style->user.comboBoxTextBox);
		ColorPick("$FUCK_Styles_ComboText"_T, style->user.comboBoxText);
		if (FUCK::SliderFloat("$FUCK_Styles_PopupRounding"_T, &style->user.popupRounding, 0.0f, 12.0f, "%.0f"))
			style->RefreshStyle();
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Sliders"_T)) {
		ColorPick("$FUCK_Styles_SliderIdle"_T, style->user.sliderBorder);
		ColorPick("$FUCK_Styles_SliderActive"_T, style->user.sliderBorderActive);
		ColorPick("$FUCK_Styles_SliderGrab"_T, style->user.sliderGrab);
		ColorPick("$FUCK_Styles_SliderGrabActive"_T, style->user.sliderGrabActive);

		FUCK::Separator();

		ColorPick("$FUCK_Styles_ScrollbarBG"_T, style->user.scrollbarBG);
		ColorPick("$FUCK_Styles_ScrollbarGrab"_T, style->user.scrollbarGrab);
		ColorPick("$FUCK_Styles_ScrollbarGrabActive"_T, style->user.scrollbarGrabActive);

		if (FUCK::SliderFloat("$FUCK_Styles_ScrollbarRounding"_T, &style->user.scrollbarRounding, 0.0f, 12.0f, "%.0f"))
			style->RefreshStyle();
		if (FUCK::SliderFloat("$FUCK_Styles_GrabRounding"_T, &style->user.grabRounding, 0.0f, 12.0f, "%.0f"))
			style->RefreshStyle();
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Misc"_T)) {
		ColorPick("$FUCK_Styles_Flash"_T, style->user.widgetFlash);
		ColorPick("$FUCK_Styles_ToggleActive"_T, style->user.widgetToggleActive);
		if (FUCK::SliderFloat("$FUCK_Styles_IndentSpacing"_T, &style->user.indentSpacing, 0.0f, 50.0f))
			style->RefreshStyle();
	}
}
