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

struct WindowCollapseState
{
	bool isCollapsed = false;
	bool wasCollapsed = false;
	ImVec2 preCollapseSize{};
};
static StringMap<WindowCollapseState> s_windowStates;

// Auto-Close list for Game Menus
static const std::vector<std::string> s_closeOnOpen = {
	RE::Console::MENU_NAME.data(), RE::ContainerMenu::MENU_NAME.data(),
	RE::JournalMenu::MENU_NAME.data(), RE::InventoryMenu::MENU_NAME.data(),
	RE::MapMenu::MENU_NAME.data(), RE::DialogueMenu::MENU_NAME.data(),
	RE::MagicMenu::MENU_NAME.data(), RE::StatsMenu::MENU_NAME.data(),
	RE::TweenMenu::MENU_NAME.data(), RE::FavoritesMenu::MENU_NAME.data()
};

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
	RegisterWindow(&_themeEditorWindow);
}

// ==========================================
// Registration & Callbacks
// ==========================================

void FUCKMan::RegisterTool(FUCK::ITool* a_tool)
{
	if (!a_tool)
		return;

	// 1. Pointer Check
	if (std::find(_tools.begin(), _tools.end(), a_tool) != _tools.end()) {
		return;
	}

	// 2. Name Collision Check
	auto it = std::find_if(_tools.begin(), _tools.end(), [&](FUCK::ITool* existing) {
		return existing && (strcmp(existing->Name(), a_tool->Name()) == 0);
	});

	if (it != _tools.end()) {
		return;
	}

	_tools.push_back(a_tool);
}

void FUCKMan::RegisterWindow(FUCK::IWindow* a_window)
{
	if (!a_window)
		return;

	// 1. Pointer Check
	if (std::find(_windows.begin(), _windows.end(), a_window) != _windows.end()) {
		return;
	}

	// 2. Title Collision Check
	auto it = std::find_if(_windows.begin(), _windows.end(), [&](FUCK::IWindow* existing) {
		return existing && (strcmp(existing->Title(), a_window->Title()) == 0);
	});

	if (it != _windows.end()) {
		return;
	}

	_windows.push_back(a_window);
}

void FUCKMan::UnregisterWindow(FUCK::IWindow* a_window)
{
	if (!a_window)
		return;

	auto it = std::find(_windows.begin(), _windows.end(), a_window);
	if (it != _windows.end()) {
		// Clean up the persistent collapse/geometry state
		s_windowStates.erase((*it)->Title());

		// Remove from render list
		_windows.erase(it);
	}

	// Clean up from suspended windows
	std::erase(_suspendedWindows, a_window);
}

// ==========================================
// Input Processing
// ==========================================

bool FUCKMan::ProcessAsyncInput(const RE::InputEvent* const* a_event)
{
	bool consumed = false;

	// Active Tool Input (Priority)
	if (_activeTool && _activeTool->OnAsyncInput(a_event)) {
		consumed = true;
	}

	// Make sure we skip the global Escape-to-Close override if rebinding
	if (!consumed && !MANAGER(Input)->IsBinding() && (_isOpen || IsInputBlocked())) {
		// ESC / Close Logic (Priority over Global Hotkeys)
		if (MANAGER(Input)->IsInputPressed(a_event, Hotkeys::Manager::EscapeKey())) {
			bool handled = false;

			// A. Close Child Windows with kCloseOnEsc flag
			for (auto* win : _windows) {
				if (win->IsOpen() && (win->GetFlags() & FUCK::WindowFlags::kCloseOnEsc)) {
					win->SetOpen(false);
					handled = true;
				}
			}

			// B. Close Main Menu
			if (!handled && _isOpen) {
				Close();
				handled = true;
			}

			if (handled) {
				consumed = true;
			}
		}
	}

	if (!consumed) {
		// Framework Global Hotkeys
		if (MANAGER(Hotkeys)->ProcessInput(a_event)) {
			consumed = true;
		}
	}

	if (!consumed) {
		// Background Tool Input
		for (auto* tool : _tools) {
			if (tool != _activeTool && tool->OnAsyncInput(a_event)) {
				consumed = true;
				break;
			}
		}
	}

	if (!consumed) {
		// Window Input
		for (auto* win : _windows) {
			if (win->OnAsyncInput(a_event)) {
				consumed = true;
				break;
			}
		}
	}

	UpdateGameState();

	// 5. Block Game Input if Menu/Windows are blocking
	return consumed || IsInputBlocked();
}

// ==========================================
// Settings & State Management
// ==========================================

void FUCKMan::ResetSettings()
{
	_cfg = _def;

	MANAGER(IconFont)->SetFontName(_cfg.currentFont);

	auto hotkeys = MANAGER(Hotkeys);
	hotkeys->GetToggleHotkey().kKey = hotkeys->_defToggle.kKey;
	hotkeys->GetToggleHotkey().kMod1 = hotkeys->_defToggle.kMod1;
	hotkeys->GetToggleHotkey().kMod2 = hotkeys->_defToggle.kMod2;
	hotkeys->GetToggleHotkey().gKey = hotkeys->_defToggle.gKey;
	hotkeys->GetToggleHotkey().gMod1 = hotkeys->_defToggle.gMod1;
	hotkeys->GetToggleHotkey().gMod2 = hotkeys->_defToggle.gMod2;

	if (_isOpen) {
		ClampWindowToScreen(_cfg.windowPos, _cfg.windowSize);
		_pendingWindowRestore = true;
		ImGui::Styles::GetSingleton()->RefreshStyle();
		MANAGER(IconFont)->ReloadFonts();
	}

	Save();
	SaveKeybinds();
}

void FUCKMan::LoadSettings(const CSimpleIniA& a_ini)
{
	float scale = FUCK::GetResolutionScale();
	if (scale < 0.1f) scale = 1.0f;

	// Apply resolution scale to Window size/pos defaults gracefully
	ImVec2 scaledDefPos = { _def.windowPos.x * scale, _def.windowPos.y * scale };
	ImVec2 scaledDefSize = { _def.windowSize.x * scale, _def.windowSize.y * scale };

	_cfg.windowPos.x = static_cast<float>(a_ini.GetDoubleValue("Window", "X", scaledDefPos.x));
	_cfg.windowPos.y = static_cast<float>(a_ini.GetDoubleValue("Window", "Y", scaledDefPos.y));
	_cfg.windowSize.x = static_cast<float>(a_ini.GetDoubleValue("Window", "Width", scaledDefSize.x));
	_cfg.windowSize.y = static_cast<float>(a_ini.GetDoubleValue("Window", "Height", scaledDefSize.y));

	_cfg.globalPauseType = static_cast<PauseType>(a_ini.GetLongValue("Settings", "iGlobalPauseType", static_cast<long>(_def.globalPauseType)));

	float loadedScale = static_cast<float>(a_ini.GetDoubleValue("Settings", "fUserScale", _def.userScale));
	_cfg.userScale = std::clamp(loadedScale, 0.5f, 2.0f);

	_cfg.sidebarOnRight = a_ini.GetBoolValue("Settings", "bSidebarOnRight", _def.sidebarOnRight);
	_cfg.injectSystemMenu = a_ini.GetBoolValue("Settings", "bInjectSystemMenu", _def.injectSystemMenu);
	_cfg.replaceHelpMenu = a_ini.GetBoolValue("Settings", "bReplaceHelpMenu", _def.replaceHelpMenu);

	_cfg.currentFont = a_ini.GetValue("Appearance", "sFont", _def.currentFont.c_str());
	if (!_cfg.currentFont.empty()) {
		MANAGER(IconFont)->SetFontName(_cfg.currentFont);
	}

	_cfg.themeEditorPos.x = static_cast<float>(a_ini.GetDoubleValue("ThemeEditor", "X", _def.themeEditorPos.x));
	_cfg.themeEditorPos.y = static_cast<float>(a_ini.GetDoubleValue("ThemeEditor", "Y", _def.themeEditorPos.y));
	_cfg.themeEditorSize.x = static_cast<float>(a_ini.GetDoubleValue("ThemeEditor", "Width", _def.themeEditorSize.x));
	_cfg.themeEditorSize.y = static_cast<float>(a_ini.GetDoubleValue("ThemeEditor", "Height", _def.themeEditorSize.y));

	_themeEditorWindow._lastPos = _cfg.themeEditorPos;
	_themeEditorWindow._lastSize = _cfg.themeEditorSize;

	_lastSavedPos = _cfg.windowPos;
	_lastSavedSize = _cfg.windowSize;
	_pendingWindowRestore = true;
}

void FUCKMan::SaveSettings(CSimpleIniA& a_ini)
{
	FUCK::INI::SaveDouble(a_ini, "Window", "X", _cfg.windowPos.x, _def.windowPos.x);
	FUCK::INI::SaveDouble(a_ini, "Window", "Y", _cfg.windowPos.y, _def.windowPos.y);
	FUCK::INI::SaveDouble(a_ini, "Window", "Width", _cfg.windowSize.x, _def.windowSize.x);
	FUCK::INI::SaveDouble(a_ini, "Window", "Height", _cfg.windowSize.y, _def.windowSize.y);

	FUCK::INI::SaveInt   (a_ini, "Settings", "iGlobalPauseType", static_cast<int>(_cfg.globalPauseType), static_cast<int>(_def.globalPauseType));
	FUCK::INI::SaveDouble(a_ini, "Settings", "fUserScale", _cfg.userScale, _def.userScale);
	FUCK::INI::SaveBool  (a_ini, "Settings", "bSidebarOnRight", _cfg.sidebarOnRight, _def.sidebarOnRight);
	FUCK::INI::SaveBool  (a_ini, "Settings", "bInjectSystemMenu", _cfg.injectSystemMenu, _def.injectSystemMenu);
	FUCK::INI::SaveBool  (a_ini, "Settings", "bReplaceHelpMenu", _cfg.replaceHelpMenu, _def.replaceHelpMenu);

	FUCK::INI::SaveString(a_ini, "Appearance", "sFont", _cfg.currentFont.c_str(), _def.currentFont.c_str());

	// Theme Editor State (Only save if initialized/changed from -1)
	if (_themeEditorWindow._lastPos.x != -1.0f) {
		FUCK::INI::SaveDouble(a_ini, "ThemeEditor", "X", _themeEditorWindow._lastPos.x, _def.themeEditorPos.x);
		FUCK::INI::SaveDouble(a_ini, "ThemeEditor", "Y", _themeEditorWindow._lastPos.y, _def.themeEditorPos.y);
		FUCK::INI::SaveDouble(a_ini, "ThemeEditor", "Width", _themeEditorWindow._lastSize.x, _def.themeEditorSize.x);
		FUCK::INI::SaveDouble(a_ini, "ThemeEditor", "Height", _themeEditorWindow._lastSize.y, _def.themeEditorSize.y);
	}
}

void FUCKMan::Save()
{
	Settings::Core.Save([this](CSimpleIniA& ini) { SaveSettings(ini); });
}

void FUCKMan::SaveKeybinds()
{
	Settings::Core.SaveKeybinds([](CSimpleIniA& ini) { MANAGER(Hotkeys)->SaveHotKeys(ini); });
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
		if (_cfg.globalPauseType == PauseType::kSoft)
			targetSoft = true;
		if (_cfg.globalPauseType == PauseType::kHard)
			targetHard = true;
	}

	// 2. Window Overrides
	for (auto* win : _windows) {
		if (win->IsOpen()) {
			FUCK::WindowFlags f = win->GetFlags();

			if (f & FUCK::WindowFlags::kPauseSoft)
				targetSoft = true;
			if (f & FUCK::WindowFlags::kPauseHard)
				targetHard = true;
			if (f & FUCK::WindowFlags::kBlurBackground)
				targetBlur = true;
			if (f & FUCK::WindowFlags::kBlockVanity)
				targetVanity = true;
			if (f & FUCK::WindowFlags::kHideHUD) {
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

	// Scaleform Movie Pausing (to prevent background hover detection)
	bool blockInput = IsInputBlocked();
	if (auto ui = RE::UI::GetSingleton()) {
		if (blockInput) {
			for (auto& [name, entry] : ui->menuMap) {
				// pause menus that are actively on the screen
				if (entry.menu && entry.menu->OnStack() && entry.menu->uiMovie) {
					if (name == RE::CursorMenu::MENU_NAME ||
						name == RE::Console::MENU_NAME ||
						name == RE::LoadingMenu::MENU_NAME ||
						name == RE::HUDMenu::MENU_NAME ||
						name == RE::FaderMenu::MENU_NAME) {
						continue;
					}
					if (_pausedMenus.find(name.c_str()) == _pausedMenus.end()) {
						entry.menu->uiMovie->SetPause(true);
						_pausedMenus.insert(name.c_str());
					}
				}
			}
		} else {
			for (const auto& name : _pausedMenus) {
				auto menu = ui->GetMenu(name);
				if (menu && menu->uiMovie) {
					menu->uiMovie->SetPause(false);
				}
			}
			_pausedMenus.clear();
		}
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
		if (win->IsOpen() && !(win->GetFlags() & FUCK::WindowFlags::kPassInputToGame)) {
			return true;
		}
	}
	return false;
}

bool FUCKMan::IsCursorForced() const
{
	return _forceCursor;
}

bool FUCKMan::HasWindowWithFlag(FUCK::WindowFlags a_flag) const
{
	for (const auto* win : _windows) {
		if (win->IsOpen() && (win->GetFlags() & a_flag)) {
			return true;
		}
	}
	return false;
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

	ImGui::ClearNavState();

	ImGui::Styles::GetSingleton()->OnStyleRefresh();

	if (_activeTool)
		_activeTool->OnOpen();

	_pendingWindowRestore = true;

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

	if (!IsInputBlocked()) {
		MANAGER(Input)->ClearState();
	}

	UpdateGameState();

	ImGui::ClearNavState();
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

	DispatchMenuEvent(a_event->menuName.c_str(), a_event->opening);

	if (a_event->opening) {
		if (a_event->menuName == RE::MainMenu::MENU_NAME) {
			ImGui::ClearFormCaches();
		}

		if (std::ranges::find(s_closeOnOpen, a_event->menuName.data()) != s_closeOnOpen.end()) {
			bool closedSomething = false;

			if (_isOpen && a_event->menuName != RE::Console::MENU_NAME) {
				Close();
				closedSomething = true;
			}

			for (auto* win : _windows) {
				if (win->IsOpen() && (win->GetFlags() & FUCK::WindowFlags::kCloseOnGameMenu)) {
					win->SetOpen(false);
					_suspendedWindows.push_back(win);  // Track the suspended window
					closedSomething = true;
				}
			}

			if (closedSomething) {
				UpdateGameState();
				ImGui::ClearNavState();
			}
		}
	} else {
		if (std::ranges::find(s_closeOnOpen, a_event->menuName.data()) != s_closeOnOpen.end()) {
			bool anyOpen = false;
			if (auto ui = RE::UI::GetSingleton()) {
				for (const auto& m : s_closeOnOpen) {
					if (ui->IsMenuOpen(m)) {
						anyOpen = true;
						break;
					}
				}
			}

			if (!anyOpen && !_suspendedWindows.empty()) {
				for (auto* win : _suspendedWindows) {
					win->SetOpen(true);
				}
				_suspendedWindows.clear();
				UpdateGameState();
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
	if (auto ui = RE::UI::GetSingleton(); ui && ui->closingAllMenus) {
		bool closedSomething = false;

		if (_isOpen) {
			Close();
			closedSomething = true;
		}

		for (auto* win : _windows) {
			if (win->IsOpen()) {
				win->SetOpen(false);
				closedSomething = true;
			}
		}

		if (closedSomething) {
			ImGui::ClearNavState();
		}

		_suspendedWindows.clear();
		return;
	}

	// --- Auto-Suspend on forced camera states ---
	if (auto camera = RE::PlayerCamera::GetSingleton(); camera && camera->currentState) {
		auto* activeState = camera->currentState.get();

		bool isForcedCamera = (activeState == camera->cameraStates[RE::CameraState::kVATS].get() ||
							   activeState == camera->cameraStates[RE::CameraState::kBleedout].get() ||
							   activeState == camera->cameraStates[RE::CameraState::kAutoVanity].get()
		);

		if (isForcedCamera) {
			bool closedSomething = false;

			if (_isOpen) {
				Close();
				closedSomething = true;
			}

			for (auto* win : _windows) {
				if (win->IsOpen()) {
					win->SetOpen(false);
					if (std::find(_suspendedWindows.begin(), _suspendedWindows.end(), win) == _suspendedWindows.end()) {
						_suspendedWindows.push_back(win);
					}
					closedSomething = true;
				}
			}

			if (closedSomething) {
				ImGui::ClearNavState();
			}
		} else if (!_suspendedWindows.empty()) {
			bool blockingMenuOpen = false;

			if (auto ui = RE::UI::GetSingleton()) {
				for (const auto& m : s_closeOnOpen) {
					if (ui->IsMenuOpen(m)) {
						blockingMenuOpen = true;
						break;
					}
				}
			}

			if (!blockingMenuOpen) {
				for (auto* win : _suspendedWindows) {
					win->SetOpen(true);
				}
				_suspendedWindows.clear();
				UpdateGameState();
			}
		}
	}

	UpdateGameState();

	// ==========================================
	// LAYOUT METRICS (Chrome / Unscaled)
	// ==========================================
	auto iconArrow = MANAGER(IconFont)->GetStepperRight();

	struct LayoutMetrics
	{
		float uiScale;
		float textH;
		float padBase;

		float iconAspect;
		float chromeIconBaseH;  // = 16.0f * uiScale  (userScale intentionally excluded for chrome)

		// Titlebar
		float titleH;
		float titleFontSize;
		float titleIconPadX;
		float titleIconNudgeY;
		float titleTextOffsetY;

		// Sidebar
		float sidebarWidth;
		float sidebarItemH;
		float sidebarFontSize;
		float sidebarIndent;
	} m;

	m.uiScale  = FUCK::GetResolutionScale();
	m.textH    = FUCK::GetTextLineHeight();
	m.padBase  = 15.0f * m.uiScale;

	m.iconAspect = (iconArrow && iconArrow->imageSize.y > 0.0f) ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;
	
	m.chromeIconBaseH = 16.0f * m.uiScale;

	float headerPadding = 3.0f * m.uiScale;

	const float kChromeFontSize = 22.0f * 0.9f;

	m.titleH           = m.textH + (headerPadding * 2.0f);
	m.titleFontSize    = kChromeFontSize;
	m.titleIconPadX    = 8.0f * m.uiScale;
	m.titleIconNudgeY  = 1.0f * m.uiScale;  // positive = down, negative = up
	m.titleTextOffsetY = (m.titleH - m.titleFontSize) * 0.5f + (2.0f * m.uiScale);

	m.sidebarWidth    = 250.0f * m.uiScale;
	m.sidebarItemH    = 30.0f * m.uiScale;
	m.sidebarFontSize = kChromeFontSize;
	m.sidebarIndent   = 15.0f * m.uiScale;

	auto chromeArrow = [&](bool pointsDown, float rowH) {
		return ImGui::CalcArrowIconParams(m.iconAspect, pointsDown, rowH, m.chromeIconBaseH);
	};

	// Content scale helper — respects kIgnoreUserScale flag
	auto pushContentScale = [&](bool ignoreUserScale = false) {
		_isIgnoringUserScale = ignoreUserScale;
		_activeScale = ignoreUserScale ? 1.0f : _cfg.userScale;

		float targetFontSize = ImGui::GetStyle().FontSizeBase * _activeScale;
		ImGui::PushFont(nullptr, targetFontSize);

		float borderSize = ImGui::GetStyle().FrameBorderSize;
		float spaceX = (8.0f * m.uiScale * _activeScale) + borderSize;
		float spaceY = (4.0f * m.uiScale * _activeScale) + borderSize;
		float innerSpaceX = (4.0f * m.uiScale * _activeScale) + borderSize;
		float innerSpaceY = (4.0f * m.uiScale * _activeScale) + borderSize;

		float indentSpacing = ImGui::Styles::GetSingleton()->user.indentSpacing;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * m.uiScale * _activeScale, 3.0f * m.uiScale * _activeScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spaceX, spaceY));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(innerSpaceX, innerSpaceY));
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indentSpacing * m.uiScale * _activeScale);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12.0f * m.uiScale);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 10.0f * m.uiScale);
	};

	auto popContentScale = [&]() {
		ImGui::PopFont();
		ImGui::PopStyleVar(6);
		_activeScale = _cfg.userScale;
		_isIgnoringUserScale = false;
	};

	// ------------------------------------------------------------------------
	// Overlay Render Pass
	// ------------------------------------------------------------------------
	if (_activeTool || ShouldRender()) {
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
			pushContentScale(false);
			if (_activeTool)
				_activeTool->RenderOverlay();

			for (auto* tool : _tools) {
				if (tool != _activeTool)
					tool->RenderOverlay();
			}
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
			if (win->GetFlags() & FUCK::WindowFlags::kCustomRender) {
				win->Draw();
				continue;
			}

			const char* title = win->Title();
			ImGuiWindowFlags flags = ImGuiWindowFlags_None;

			// --- Flags Setup ---
			bool noDecoration    = (win->GetFlags() & FUCK::WindowFlags::kNoDecoration);
			bool ignoreUserScale = (win->GetFlags() & FUCK::WindowFlags::kIgnoreUserScale) != 0;
			bool noResize        = (win->GetFlags() & FUCK::WindowFlags::kNoResize) != 0;
			bool autoResize      = (win->GetFlags() & FUCK::WindowFlags::kAutoResize) != 0;

			auto it = s_windowStates.find(title);
			if (it == s_windowStates.end()) {
				it = s_windowStates.emplace(std::string(title), WindowCollapseState{}).first;
			}
			auto& winState = it->second;

			flags |= ImGuiWindowFlags_NoTitleBar;
			if (noDecoration) {
				winState.isCollapsed = false;
				winState.wasCollapsed = false;
				flags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
			} else {
				flags |= ImGuiWindowFlags_NoScrollbar;
			}

			if (noResize) {
				flags |= ImGuiWindowFlags_NoResize;
			}
			if (autoResize) {
				flags |= ImGuiWindowFlags_AlwaysAutoResize;
			}

			bool poppedInvisibleBg = false;
			if (win->GetFlags() & FUCK::WindowFlags::kNoBackground) {
				if (!IsInputBlocked()) {
					flags |= ImGuiWindowFlags_NoBackground;
				} else {
					// Give kNoBackground windows a 0.1% invisible background so they can be easily dragged.
					// We also suppress the Border, otherwise ImGui draws a 1px grey frame.
					ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.001f));
					ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
					poppedInvisibleBg = true;
				}
			}

			if ((win->GetFlags() & FUCK::WindowFlags::kPassInputToGame) && !IsInputBlocked())
				flags |= ImGuiWindowFlags_NoInputs;

			// --- Collapse Logic ---
			bool isCollapsed = winState.isCollapsed;
			bool wasCollapsed = winState.wasCollapsed;
			winState.wasCollapsed = isCollapsed;

			// Get Metrics from Interface
			ImVec2 targetSize = win->GetDefaultSize();

			// Handle collapse override
			if (isCollapsed) {
				float targetW = (winState.preCollapseSize.x > 0.0f) ? winState.preCollapseSize.x : targetSize.x;
				targetSize = ImVec2(targetW, m.titleH);
				flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar;
			} else if (wasCollapsed) {
				if (winState.preCollapseSize.x > 0.0f) {
					targetSize = winState.preCollapseSize;
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

			// If AutoResize is active, ImGui shrinks the window dynamically. We bypass hard size forcing.
			if (!autoResize) {
				ImGuiCond sizeCond = (isCollapsed || wasCollapsed || noResize) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
				FUCK::SetNextWindowSize(targetSize, sizeCond);
			}

			bool open = true;

			if (!noDecoration) {
				// Push 0 padding here because the custom chrome is drawn flush to the edges,
				// and the inner child window handles the actual content padding.
				FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			} else {
				// Give standard padding to all frameless windows
				FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m.padBase, m.padBase));
			}

			if (FUCK::BeginWindow(title, &open, flags)) {
				if (poppedInvisibleBg) {
					ImGui::PopStyleColor(2);  // Clear WindowBg and Border
				}
				win->UpdateState(FUCK::GetWindowPos(), FUCK::GetWindowSize());

				if (win->GetFlags() & FUCK::WindowFlags::kExtendBorder)
					FUCK::ExtendWindowPastBorder();

				if (!noDecoration) {
					// --- Window Chrome Decoration (unscaled) ---
					float winWidth = FUCK::GetWindowSize().x;

					ImVec2 headerStartCursor = FUCK::GetCursorPos();
					ImVec2 cursorScreen = FUCK::GetCursorScreenPos();

					FUCK::BeginGroup();

					// 1. Collapse Icon
					float iconW = 0.0f;

					if (iconArrow) {
						if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(m.titleH + 20.0f, m.titleH))) {
							winState.isCollapsed = !isCollapsed;
							if (!isCollapsed) {
								winState.preCollapseSize = FUCK::GetWindowSize();
							}
						}
						bool isHovered = ImGui::IsItemHovered();
						ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

						bool pointsDown = !isCollapsed;
						auto ap = chromeArrow(pointsDown, m.titleH);

						ImVec2 drawPos = cursorScreen;
						drawPos.x += m.titleIconPadX;
						drawPos.y += ap.offsetY + m.titleIconNudgeY;

						ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
							pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);

						iconW = m.titleH + 20.0f;
					}

					// 2. Title Text
					ImFont* baseFont = ImGui::GetFont();

					FUCK::SetCursorPos({ iconW, m.titleTextOffsetY });
					ImGui::GetWindowDrawList()->AddText(baseFont, m.titleFontSize,
						FUCK::GetCursorScreenPos(), ImGui::GetColorU32(ImGuiCol_Text), title);

					// 3. Close Button
					const float btnSize = m.titleH;
					const float btnX = winWidth - btnSize - (4.0f * m.uiScale);

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
							winState.isCollapsed = !isCollapsed;
							if (!isCollapsed) {
								winState.preCollapseSize = FUCK::GetWindowSize();
							}
						}
					}

					// 4. Separator
					FUCK::SetCursorPos({ headerStartCursor.x, m.titleH });

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
						float childY = m.titleH + (1.0f * m.uiScale);
						FUCK::SetCursorPos({ 0, childY });

						FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m.padBase, m.padBase));

						ImGuiChildFlags childFlags = ImGuiChildFlags_AlwaysUseWindowPadding;
						ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground;

						if (ImGui::BeginChild("##Content", ImVec2(0, 0), childFlags, windowFlags)) {
							ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
							pushContentScale(ignoreUserScale);
							win->Draw();
							popContentScale();
							ImGui::PopItemWidth();
						}
						ImGui::EndChild();

						FUCK::PopStyleVar();
					}
				} else {
					// --- No Chrome Decoration ---
					pushContentScale(ignoreUserScale);
					win->Draw();
					popContentScale();
				}
			}
			FUCK::EndWindow();

			FUCK::PopStyleVar();

			if (!open) {
				win->SetOpen(false);
				UpdateGameState();

				if (!IsInputBlocked()) {
					MANAGER(Input)->ClearState();
				}
			}

			// Specific save hook for ThemeEditor bounds tracking upon move/resize edits completing
			if (win == &_themeEditorWindow) {
				static ImVec2 s_lastThemePos = _themeEditorWindow._lastPos;
				static ImVec2 s_lastThemeSize = _themeEditorWindow._lastSize;
				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					if (s_lastThemePos.x != _themeEditorWindow._lastPos.x || s_lastThemePos.y != _themeEditorWindow._lastPos.y ||
						s_lastThemeSize.x != _themeEditorWindow._lastSize.x || s_lastThemeSize.y != _themeEditorWindow._lastSize.y) {
						s_lastThemePos = _themeEditorWindow._lastPos;
						s_lastThemeSize = _themeEditorWindow._lastSize;
						Save();
					}
				}
			}
		}
	}

	// ------------------------------------------------------------------------
	// 2. Main FUCK Menu (Sidebar & Settings)
	// ------------------------------------------------------------------------
	if (!_isOpen)
		return;

	ImGui::GetIO().MouseDrawCursor = false;

	if (_pendingWindowRestore) {
		ClampWindowToScreen(_cfg.windowPos, _cfg.windowSize);
		FUCK::SetNextWindowPos(_cfg.windowPos, ImGuiCond_Always);
		if (!_isCollapsed) {
			FUCK::SetNextWindowSize(_cfg.windowSize, ImGuiCond_Always);
		}
		_pendingWindowRestore = false;
	} else {
		if (_isCollapsed) {
			FUCK::SetNextWindowSize(ImVec2(_cfg.windowSize.x, m.titleH));
		} else if (_wasCollapsed && !_isCollapsed) {
			FUCK::SetNextWindowSize(_cfg.windowSize);
		} else {
			FUCK::SetNextWindowSize(ImVec2(1000.0f * m.uiScale, 600.0f * m.uiScale), ImGuiCond_FirstUseEver);
		}
	}

	_wasCollapsed = _isCollapsed;

	FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	std::string windowTitle = std::format("##{}", "$FUCK_Title"_T);
	bool wantsOpen = true;

	ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
	if (_isCollapsed)
		winFlags |= ImGuiWindowFlags_NoResize;

	if (FUCK::BeginWindow(windowTitle.c_str(), &wantsOpen, winFlags)) {
		FUCK::ExtendWindowPastBorder();

		if (!_isCollapsed) {
			_cfg.windowPos = FUCK::GetWindowPos();
			_cfg.windowSize = FUCK::GetWindowSize();

			// Auto-save settings on move/resize end
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				if (_cfg.windowPos.x != _lastSavedPos.x || _cfg.windowPos.y != _lastSavedPos.y ||
					_cfg.windowSize.x != _lastSavedSize.x || _cfg.windowSize.y != _lastSavedSize.y) {
					_lastSavedPos = _cfg.windowPos;
					_lastSavedSize = _cfg.windowSize;
					Save();
				}
			}
		}

		// -- Custom Title Bar (unscaled) --
		{
			FUCK::BeginGroup();

			float winWidth = FUCK::GetWindowSize().x;

			ImVec2 cursorScreen = FUCK::GetCursorScreenPos();

			// 1. Collapse Icon (Left)
			if (iconArrow) {
				FUCK::SetCursorPos({ 0, 0 });
				if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(m.titleH + 20.0f, m.titleH))) {
					_isCollapsed = !_isCollapsed;
				}

				bool isHovered = ImGui::IsItemHovered();
				ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				bool pointsDown = !_isCollapsed;
				auto ap = chromeArrow(pointsDown, m.titleH);

				ImVec2 drawPos = cursorScreen;
				drawPos.x += m.titleIconPadX;
				drawPos.y += ap.offsetY + m.titleIconNudgeY;

				ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
					pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
			}

			// 2. Close Button
			float btnSize = m.titleH;
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
				ImVec2 mousePos = ImGui::GetMousePos();
				ImVec2 wP = FUCK::GetWindowPos();
				if (mousePos.x >= wP.x && mousePos.x <= wP.x + winWidth &&
					mousePos.y >= wP.y && mousePos.y <= wP.y + m.titleH) {
					_isCollapsed = !_isCollapsed;
				}
			}
		}

		// -- Content --
		if (!_isCollapsed) {
			float contentY = m.titleH;
			FUCK::SetCursorPos({ 0, contentY });

			float availHeight = FUCK::GetContentRegionAvail().y;

			// -- Sidebar (unscaled) --
			auto renderSidebar = [&]() {
				const float topPadding = 2.0f * m.uiScale;
				const float bottomPadding = 2.0f * m.uiScale;
				const float textVisualOffset = 1.0f * m.uiScale;

				ImFont* regularFont = MANAGER(IconFont)->GetRegularFont();

				std::vector<FUCK::ITool*> looseTools;
				StringMap<std::vector<FUCK::ITool*>> toolGroups;

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
					bool isGroup = false;
					FUCK::ITool* tool = nullptr;
					std::vector<FUCK::ITool*>* tools = nullptr;
				};

				std::vector<SidebarEntry> entries;
				entries.reserve(looseTools.size() + toolGroups.size());

				for (auto* t : looseTools) {
					entries.push_back({ t->Name(), false, t, nullptr });
				}

				for (auto& [name, tools] : toolGroups) {
					std::sort(tools.begin(), tools.end(), [](FUCK::ITool* a, FUCK::ITool* b) {
						return _stricmp(a->Name(), b->Name()) < 0;
					});
					entries.push_back({ name, true, nullptr, &tools });
				}

				std::sort(entries.begin(), entries.end(), [](const SidebarEntry& a, const SidebarEntry& b) {
					return _stricmp(a.label.c_str(), b.label.c_str()) < 0;
				});

				auto apRight = chromeArrow(false, m.sidebarItemH);
				float alignedTextOffset = (m.sidebarIndent * 0.5f) + apRight.drawSize.x + (10.0f * m.uiScale);

				FUCK::BeginChild("Sidebar", ImVec2(m.sidebarWidth, availHeight), true, ImGuiWindowFlags_None);
				{
					FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
					ImVec2 headerStart = FUCK::GetCursorPos();
					headerStart.y += topPadding;

					// --- HEADER: TOOLS (Centred) ---
					FUCK::SetCursorPos(headerStart);

					// Bypassing FUCK::PushFont scaling
					ImGui::PushFont(regularFont, m.sidebarFontSize);
					float textHeightCalc = ImGui::GetTextLineHeight();
					float sidebarAvailW = FUCK::GetContentRegionAvail().x;
					const char* headerText = "$FUCK_Tools"_T;
					float headerW = ImGui::CalcTextSize(headerText).x;

					FUCK::SetCursorPosX(headerStart.x + (sidebarAvailW - headerW) * 0.5f);
					FUCK::SetCursorPosY(headerStart.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset);
					FUCK::Text(headerText);
					ImGui::PopFont();

					FUCK::SetCursorPos(ImVec2(headerStart.x, headerStart.y + m.sidebarItemH));
					FUCK::SeparatorThick();

					auto RenderSidebarItem = [&](FUCK::ITool* tool, const char* label, float extraIndent = 0.0f) {
						// Push ID to prevent conflicts if multiple tools have same name
						ImGui::PushID(tool);

						bool isSelected = (_activeTool == tool);
						const auto cursorPos = FUCK::GetCursorPos();
						std::string idLabel = std::format("##{}", label);

						if (FUCK::Selectable(idLabel.c_str(), isSelected, 0, ImVec2(0, m.sidebarItemH))) {
							if (_activeTool && _activeTool != tool) {
								FUCK::AbortBinding();
								_activeTool->OnClose();
							}
							RE::PlaySound("UIMenuOK");
							_activeTool = tool;
							_activeTool->OnOpen();
						}

						ImVec2 endPos = FUCK::GetCursorPos();

						// Bypassing FUCK::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);
						float textY = cursorPos.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset;

						FUCK::SetCursorPos({ cursorPos.x + alignedTextOffset + extraIndent, textY });
						FUCK::Text(label);
						ImGui::PopFont();
						FUCK::SetCursorPos(endPos);

						ImGui::PopID();
					};

					auto RenderSidebarGroup = [&](const std::string& groupName, std::vector<FUCK::ITool*>& tools) {
						
						// Bypassing FUCK::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);

						// Custom TreeNode rendering
						ImGui::PushID(groupName.c_str());
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						ImGuiID id = window->GetID(groupName.c_str());
						bool isOpen = window->DC.StateStorage->GetInt(id, 0);

						ImVec2 pos = window->DC.CursorPos;
						ImRect bb(pos, pos + ImVec2(FUCK::GetContentRegionAvail().x, m.sidebarItemH));

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
								bool pDown = isOpen;
								auto ap = chromeArrow(pDown, m.sidebarItemH);

								ImVec2 drawPos = {
									pos.x + (m.sidebarIndent * 0.5f),
									pos.y + ap.offsetY + textVisualOffset
								};
								ImGui::DrawArrowIcon(window->DrawList, drawPos, ap.drawSize, col,
									pDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
							}

							float textY = bb.Min.y + (m.sidebarItemH - ImGui::CalcTextSize(groupName.c_str()).y) * 0.5f + textVisualOffset;
							ImGui::RenderText({ pos.x + alignedTextOffset, textY }, groupName.c_str());
						}

						if (isOpen) {
							for (auto* tool : tools) {
								RenderSidebarItem(tool, tool->Name(), m.sidebarIndent);
							}
						}

						ImGui::PopID();
						ImGui::PopFont();
					};

					for (auto& entry : entries) {
						if (entry.isGroup) {
							RenderSidebarGroup(entry.label, *entry.tools);
						} else {
							RenderSidebarItem(entry.tool, entry.label.c_str());
						}
					}

					// --- FOOTER: SETTINGS (Centred) ---
					float childHeight = FUCK::GetWindowSize().y;
					float separatorHeight = 1.0f;
					float settingsY = childHeight - m.sidebarItemH - bottomPadding;

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

						if (FUCK::Selectable("##SETTINGS", isSelected, 0, ImVec2(0, m.sidebarItemH))) {
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

						// Bypassing FUCK::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);

						const char* settingText = "$FUCK_Settings"_T;
						float setW = ImGui::CalcTextSize(settingText).x;

						FUCK::SetCursorPosX(cursorPos.x + (sidebarAvailW - setW) * 0.5f);
						FUCK::SetCursorPosY(cursorPos.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset);
						FUCK::Text(settingText);

						ImGui::PopFont();
						FUCK::SetCursorPos(endPos);
					}
					FUCK::PopStyleVar();
					FUCK::Dummy(ImVec2(0, bottomPadding));
				}
				FUCK::EndChild();
			};

			// -- Content (scaled) --
			auto renderContent = [&](float width) {
				FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m.padBase, m.padBase));
				FUCK::BeginChild("Content", ImVec2(width, availHeight), true, ImGuiChildFlags_AlwaysUseWindowPadding);
				{
					pushContentScale(false);
					if (_activeTool)
						_activeTool->Draw();
					else
						FUCK::CenteredText("$FUCK_NoToolSelected"_T, true);

					popContentScale();
				}
				FUCK::EndChild();
				FUCK::PopStyleVar();
			};

			if (_cfg.sidebarOnRight) {
				float contentWidth = FUCK::GetContentRegionAvail().x - m.sidebarWidth - FUCK::GetStyleVarVec(ImGuiStyleVar_ItemSpacing).x;
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
