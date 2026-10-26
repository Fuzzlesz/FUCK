#include "FUCK-Host.h"
#include "FUCK-Man.h"
#include "FUCK-Windows.h"

#include "Camera/Manager.h"

#include "ImGui/Audio.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

#include "System/Hotkeys.h"
#include "System/Input.h"
#include "System/Settings.h"
#include "System/Utils.h"

#include "API/IconsFontAwesome6.h"

// Auto-Close list for Game Menus
static constexpr std::array<std::string_view, 18> s_closeOnOpen = {
	RE::ContainerMenu::MENU_NAME.data(),   RE::JournalMenu::MENU_NAME.data(),
	RE::InventoryMenu::MENU_NAME.data(),   RE::MapMenu::MENU_NAME.data(),
	RE::DialogueMenu::MENU_NAME.data(),    RE::MagicMenu::MENU_NAME.data(),
	RE::StatsMenu::MENU_NAME.data(),       RE::TweenMenu::MENU_NAME.data(),
	RE::FavoritesMenu::MENU_NAME.data(),   RE::MainMenu::MENU_NAME.data(),
	RE::TrainingMenu::MENU_NAME.data(),    RE::MessageBoxMenu::MENU_NAME.data(),
	RE::SleepWaitMenu::MENU_NAME.data(),   RE::TutorialMenu::MENU_NAME.data(),
	RE::LoadingMenu::MENU_NAME.data(),     RE::LockpickingMenu::MENU_NAME.data(),
	RE::BookMenu::MENU_NAME.data(),        RE::RaceSexMenu::MENU_NAME.data()
};

namespace
{
	using Config = FUCKMan::FUCKConfig;

	// FUCK's settings and their INI keys. Only values that differ from the shipped settings.ini are written to the user's INI
	const FUCK::INI::Fields<Config> kSettingsFields{ "Settings", {
		{ "iGlobalPauseType",       &Config::globalPauseType       },
		{ "iInjectSystemMenu",      &Config::injectSystemMenu      },
		{ "fUserScale",             &Config::userScale             },
		{ "bSidebarOnRight",        &Config::sidebarOnRight        },
		{ "bReplaceHelpMenu",       &Config::replaceHelpMenu       },
		{ "bInjectSettingsSubmenu", &Config::injectSettingsSubmenu },
		{ "bShowSidebarFilter",     &Config::showSidebarFilter     },
		{ "bShowSidebarFavourites", &Config::showSidebarFavourites },
		{ "bGroupFavourites",       &Config::groupFavourites       },
		{ "bMuteAudio",             &Config::muteAudio             },
		{ "sSystemMenuName",        &Config::customSystemMenuName  },
	} };
}

FUCKMan::FUCKMan()
{
	FUCK::GetInterface() = FUCK::Host::CreateInterface();

	Settings::Core.LoadDefaults([this](CSimpleIniA& ini) {
		this->LoadSettings(ini);
		this->_def = this->_cfg;
	});

	RegisterWindow(&_themeEditorWindow);
}

// ==================================================
// Registration & Callbacks
// ==================================================

void FUCKMan::RegisterTool(FUCK::ITool* a_tool)
{
	if (!a_tool || !a_tool->Name() || !a_tool->PluginName()) {
		logger::info("FUCK: Attempted to register a Tool with a null Name or PluginName.");
		return;
	}
	logger::info("FUCK: Registering Tool '{}' for plugin '{}'", a_tool->Name(), a_tool->PluginName());
	std::lock_guard lock(_pendingLock);
	_pendingCommands.push_back({ PendingCommand::Type::kAddTool, a_tool, nullptr });
}

void FUCKMan::RegisterWindow(FUCK::IWindow* a_window)
{
	if (!a_window || !a_window->Id() || !a_window->PluginName()) {
		logger::info("FUCK: Attempted to register a Window with a null Id or PluginName.");
		return;
	}
	logger::info("FUCK: Registering Window '{}' for plugin '{}'", a_window->Id(), a_window->PluginName());
	std::lock_guard lock(_pendingLock);
	_pendingCommands.push_back({ PendingCommand::Type::kAddWindow, nullptr, a_window });
}

void FUCKMan::UnregisterWindow(FUCK::IWindow* a_window)
{
	if (!a_window)
		return;

	// Out of the snapshot straight away, so EnumerateHUDWidgets() doesn't hand it out before the flush
	{
		std::lock_guard lock(_snapshotLock);
		std::erase(_windowSnapshot, a_window);
	}

	std::lock_guard lock(_pendingLock);
	_pendingCommands.push_back({ PendingCommand::Type::kRemoveWindow, nullptr, a_window });
}

// Queued with registrations, so it's safe from any thread and the rebuild includes tools registered alongside it
void FUCKMan::RefreshTools()
{
	std::lock_guard lock(_pendingLock);
	_pendingCommands.push_back({ PendingCommand::Type::kRefreshTools, nullptr, nullptr });
}

void FUCKMan::FlushPendingRegistrations()
{
	std::vector<PendingCommand> commands;
	{
		std::lock_guard lock(_pendingLock);
		if (_pendingCommands.empty())
			return;
		commands = std::move(_pendingCommands);
		_pendingCommands.clear();
	}

	for (auto& cmd : commands) {
		switch (cmd.type) {
		case PendingCommand::Type::kAddTool:
			{
				auto* tool = cmd.tool;
				if (std::find(_tools.begin(), _tools.end(), tool) != _tools.end())
					break;
				auto it = std::find_if(_tools.begin(), _tools.end(), [&](FUCK::ITool* existing) {
					return existing && (strcmp(existing->Name(), tool->Name()) == 0) && (strcmp(existing->PluginName(), tool->PluginName()) == 0);
				});
				if (it == _tools.end()) {
					_tools.push_back(tool);
					MarkLayoutDirty();
				}
				break;
			}
		case PendingCommand::Type::kAddWindow:
			{
				auto* win = cmd.window;
				if (std::find(_windows.begin(), _windows.end(), win) != _windows.end())
					break;
				auto it = std::find_if(_windows.begin(), _windows.end(), [&](FUCK::IWindow* existing) {
					return existing && (strcmp(existing->Id(), win->Id()) == 0) && (strcmp(existing->PluginName(), win->PluginName()) == 0);
				});
				if (it == _windows.end())
					_windows.push_back(win);
				break;
			}
		case PendingCommand::Type::kRemoveWindow:
			{
				auto* win = cmd.window;
				auto  it  = std::find(_windows.begin(), _windows.end(), win);
				if (it != _windows.end()) {
					// Only what the window was doing is dropped. Where the user put it is kept, so it's still saved,
					// and a window registered again later comes back where it was
					std::string key = std::format("{}|{}", win->PluginName(), win->Id());
					if (auto st = s_windowStates.find(key); st != s_windowStates.end()) {
						WindowState kept;
						kept.pos             = st->second.pos;
						kept.size            = st->second.size;
						kept.hasLoadedPos    = st->second.hasLoadedPos;
						kept.isCollapsed     = st->second.isCollapsed;
						kept.preCollapseSize = st->second.preCollapseSize;
						kept.scale           = st->second.scale;
						kept.rotation        = st->second.rotation;
						st->second           = kept;
					}
					_windows.erase(it);
				}
				break;
			}
		case PendingCommand::Type::kRefreshTools:
			// The layout reads each tool's name, group and visibility when it's rebuilt
			MarkLayoutDirty();
			break;
		}
	}

	// The windows may have changed, so refresh the copy other threads list them from
	std::lock_guard lock(_snapshotLock);
	_windowSnapshot = _windows;
}

// ==================================================
// HUD Widgets
// ==================================================

void FUCKMan::EnumerateHUDWidgets(void* a_userdata, void (*a_callback)(const char*, const char*, const char*, void*))
{
	if (!a_callback)
		return;

	// Copied out, so the callback can call back into FUCK without holding the lock
	std::vector<FUCK::IWindow*> windows;
	{
		std::lock_guard lock(_snapshotLock);
		windows = _windowSnapshot;
	}

	for (auto* win : windows) {
		if (win && (win->GetFlags() & FUCK::WindowFlags::kHideWithHUD))
			a_callback(win->PluginName(), win->Id(), win->Title(), a_userdata);
	}
}

void FUCKMan::SetWindowAlphaOverride(const char* a_pluginName, const char* a_windowId, float a_alpha)
{
	if (!a_pluginName || !a_windowId)
		return;

	std::string     key = std::format("{}|{}", a_pluginName, a_windowId);
	std::lock_guard lock(_alphaLock);
	if (a_alpha < 0.0f)
		_alphaOverrides.erase(key);
	else
		_alphaOverrides[key] = std::min(a_alpha, 1.0f);
}

float FUCKMan::GetHUDWidgetAlpha(const FUCK::IWindow* a_win, FUCK::WindowFlags a_flags) const
{
	// Only HUD widgets take it, and an editable one stays fully shown while the FUCK menu is open, so it can be placed
	if (!(a_flags & FUCK::WindowFlags::kHideWithHUD))
		return 1.0f;
	if (_isOpen && (a_flags & FUCK::WindowFlags::kEditable))
		return 1.0f;

	std::lock_guard lock(_alphaLock);
	if (_alphaOverrides.empty())
		return 1.0f;

	auto it = _alphaOverrides.find(std::format("{}|{}", a_win->PluginName(), a_win->Id()));
	return it != _alphaOverrides.end() ? it->second : 1.0f;
}

// ==================================================
// Engine State Synchronization
// ==================================================

bool FUCKMan::IsWindowSuppressed(const FUCK::IWindow* win) const
{
	if (!win->IsOpen())
		return true;

	FUCK::WindowFlags userFlags = win->GetFlags();

	// Check 'tm' console command. Flat only: see the same VR gate in
	// Draw()'s main workspace menu render pass.
	if (!REL::Module::IsVR() && !(userFlags & FUCK::WindowFlags::kRenderDuringTM)) {
		if (auto ui = RE::UI::GetSingleton()) {
			if (!ui->IsShowingMenus())
				return true;
		}
	}

	// Check game menu and camera states.
	if (userFlags & FUCK::WindowFlags::kCloseOnGameMenu) {
		if (auto ui = RE::UI::GetSingleton()) {
			for (const auto& m : s_closeOnOpen) {
				if (ui->IsMenuOpen(m)) {
					return true;
				}
			}
		}

		if (auto camera = RE::PlayerCamera::GetSingleton(); camera && camera->currentState) {
			const auto activeID = camera->currentState->id;
			// VR inserts kVR before kThirdPerson, shifting kBleedout; kVATS/kAutoVanity are unshifted
			const auto bleedoutID = REL::Module::IsVR() ? RE::CameraState::kVRBleedout : RE::CameraState::kBleedout;
			if (activeID == RE::CameraState::kVATS ||
				activeID == bleedoutID             ||
				activeID == RE::CameraState::kAutoVanity) {
				return true;
			}
		}
	}

	// Hide while the game hides its HUD, or while a HUD manager has faded it right out.
	// An editable window stays while the FUCK menu is open, so it can still be placed
	if ((userFlags & FUCK::WindowFlags::kHideWithHUD) && !(_isOpen && (userFlags & FUCK::WindowFlags::kEditable))) {
		if (IsGameHUDHidden() || GetHUDWidgetAlpha(win, userFlags) <= 0.001f)
			return true;
	}

	return false;
}

// The vanilla HUD's modes in which it shows as normal. The same set SkyUI widgets show in by default
static bool IsGameplayHUDMode(std::string_view a_mode)
{
	return a_mode == "All" ||
	       a_mode == "StealthMode" ||
	       a_mode == "Favor" ||
	       a_mode == "Swimming" ||
	       a_mode == "HorseMode" ||
	       a_mode == "WarHorseMode";
}

// Whether the game has hidden its HUD. Reads Scaleform, so call it on the UI thread only
static bool ReadGameHUDHidden(bool a_skipOwnPush)
{
	auto* ui = RE::UI::GetSingleton();
	if (!ui)
		return false;

	// No HUD at all, or the game has hidden its movie outright
	auto hud = ui->GetMenu<RE::HUDMenu>();
	if (!hud || !hud->uiMovie || !hud->uiMovie->GetVisible())
		return true;

	// The intro cart ride and scenes don't open menus, they switch the HUD's mode ("CartMode", "MovementDisabled", ...).
	// The last entry is the current one
	RE::GFxValue modes;
	if (!hud->uiMovie->GetVariable(&modes, "_root.HUDMovieBaseInstance.HUDModes") || !modes.IsArray())
		return false;  // Can't tell, so hide nothing

	bool skip = a_skipOwnPush;
	for (std::uint32_t i = modes.GetArraySize(); i-- > 0;) {
		RE::GFxValue entry;
		if (!modes.GetElement(i, &entry) || !entry.IsString())
			return false;

		const std::string_view mode = entry.GetString();

		// FUCK's own push, hiding the HUD under its menu. The mode beneath it is the game's
		if (skip && mode == "WorldMapMode") {
			skip = false;
			continue;
		}

		static std::string s_lastMode;
		if (s_lastMode != mode) {
			s_lastMode = mode;
			logger::debug("FUCK: HUD mode {}", s_lastMode);
		}

		return !IsGameplayHUDMode(mode);
	}

	return false;
}

void FUCKMan::RefreshGameHUDState()
{
	// One reading in flight at a time, so the result is at most a frame old
	if (_hudReadQueued.exchange(true))
		return;

	auto* tasks = SKSE::GetTaskInterface();
	if (!tasks) {
		_hudReadQueued.store(false);
		return;
	}

	// FUCK pushes WorldMapMode itself while its menu or a kHideHUD window is open (flat only). Read past it
	const bool skipOwnPush = _isHudHidden && !REL::Module::IsVR();

	tasks->AddUITask([this, skipOwnPush]() {
		_isGameHUDHidden.store(ReadGameHUDHidden(skipOwnPush));
		_hudReadQueued.store(false);
	});
}

void FUCKMan::UpdateGameState()
{
	bool targetSoft    = _apiSoftPause || MANAGER(Camera)->IsPausing();
	bool targetHard    = _apiHardPause;
	bool targetBlur    = false;
	bool targetHideHUD = false;
	bool targetVanity  = _isVanityBlocked || MANAGER(Camera)->BlocksVanity();

	// Main menu state.
	if (_isOpen) {
		targetVanity  = true;
		targetHideHUD = true;
		if (_cfg.globalPauseType == PauseType::kSoft)
			targetSoft = true;
		if (_cfg.globalPauseType == PauseType::kHard)
			targetHard = true;
	}

	// Window overrides (skip if suppressed by game state).
	for (auto* win : _windows) {
		if (!IsWindowSuppressed(win)) {
			FUCK::WindowFlags f = win->GetFlags();

			if (f & FUCK::WindowFlags::kPauseSoft)
				targetSoft = true;
			if (f & FUCK::WindowFlags::kPauseHard)
				targetHard = true;
			if (f & FUCK::WindowFlags::kBlurBackground)
				targetBlur = true;
			if (f & FUCK::WindowFlags::kBlockVanity)
				targetVanity = true;
			if (f & FUCK::WindowFlags::kHideHUD)
				targetHideHUD = true;
		}
	}

	// Apply States
	// HUD
	if (targetHideHUD != _isHudHidden) {
		// Flat only: there the menu draws over the game HUD, so it borrows the
		// WorldMapMode push to hide it. In VR the menu lives on ImGuiVRHelper's
		// in-scene panel instead, and mods that watch HUD-mode messages (e.g.
		// KillFeed) would misread the push as the world map opening.
		if (!REL::Module::IsVR()) {
			targetHideHUD ? RE::SendHUDMessage::PushHUDMode("WorldMapMode") : RE::SendHUDMessage::PopHUDMode("WorldMapMode");
		}
		_isHudHidden = targetHideHUD;
	}

	// Soft Pause
	if (targetSoft != _isGameSoftPaused) {
		if (auto main = RE::Main::GetSingleton())
			main->GetRuntimeData().freezeTime = targetSoft;
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
		RE::PlayerCamera::GetSingleton()->GetRuntimeData2().idleTimer = 0.0f;
	}

	// Scaleform Movie Pausing (to prevent background hover detection)
	bool blockInput = IsInputBlocked();
	if (auto ui = RE::UI::GetSingleton()) {
		if (blockInput) {
			for (auto& [name, entry] : ui->menuMap) {
				if (entry.menu && entry.menu->OnStack() && entry.menu->uiMovie) {
					if (name == RE::CursorMenu ::MENU_NAME ||
						name == RE::Console ::MENU_NAME ||
						name == RE::LoadingMenu::MENU_NAME ||
						name == RE::HUDMenu ::MENU_NAME ||
						name == RE::FaderMenu ::MENU_NAME) {
						continue;
					}
					if (_pausedMenus.find(name.c_str()) == _pausedMenus.end()) {
						entry.menu->uiMovie->SetPause(true);
						_pausedMenus.insert(name.c_str());
					}
				}
			}
		} else if (!_pausedMenus.empty()) {
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

// ==================================================
// Input Processing
// ==================================================

bool FUCKMan::ProcessAsyncInput(const RE::InputEvent* const* a_event)
{
	FlushPendingRegistrations();
	bool consumed   = false;
	bool wasBinding = MANAGER(Input)->IsBinding();

	// Active Tool Input (Priority)
	if (_activeTool && _activeTool->OnAsyncInput(a_event)) {
		consumed = true;
	}

	// Make sure we skip the global Escape-to-Close override if rebinding
	if (!consumed && !wasBinding && (_isOpen || IsInputBlocked())) {
		// ESC / Close Logic (Priority over Global Hotkeys)
		if (MANAGER(Input)->IsInputReleased(a_event, Hotkeys::Manager::EscapeKey())) {
			bool handled = false;

			// A. Close Child Windows with kCloseOnEsc flag
			for (auto* win : _windows) {
				if (!IsWindowSuppressed(win) && (win->GetFlags() & FUCK::WindowFlags::kCloseOnEsc)) {
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
		// Window input (skip suppressed windows).
		for (auto* win : _windows) {
			if (!IsWindowSuppressed(win) && win->OnAsyncInput(a_event)) {
				consumed = true;
				break;
			}
		}
	}

	UpdateGameState();

	// Block Game Input if Menu/Windows are blocking
	return consumed || IsInputBlocked();
}

// ==================================================
// Settings & State Management
// ==================================================

void FUCKMan::ResetSettings()
{
	_cfg = _def;

	auto hotkeys = MANAGER(Hotkeys);
	hotkeys->GetToggleHotkey().Clear();

	Settings::Core.LoadKeybindsDefaults([](CSimpleIniA& ini) {
		MANAGER(Hotkeys)->LoadHotKeys(ini);
	});

	SetCurrentFont(_cfg.currentFont);

	if (_isOpen) {
		ClampWindowToScreen(_cfg.windowPos, _cfg.windowSize);
		_pendingWindowRestore = true;
		ImGui::Styles::GetSingleton()->RefreshStyle();
	}

	_lastSavedPos  = _cfg.windowPos;
	_lastSavedSize = _cfg.windowSize;

	Save();
	SaveWorkspace();
	SaveKeybinds();

	Settings::GetSingleton()->Save(FileType::kStyle, [](CSimpleIniA& ini) {
		ini.Delete("Style", "sFont", true);
	});
}

void FUCKMan::LoadSettings(const CSimpleIniA& a_ini)
{
	kSettingsFields.Load(a_ini, _cfg, _def);
	_cfg.userScale = std::clamp(_cfg.userScale, 0.5f, 2.0f);
}

void FUCKMan::SaveSettings(CSimpleIniA& a_ini)
{
	kSettingsFields.Save(a_ini, _cfg, _def);
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

void FUCKMan::SetCurrentFont(const std::string& a_font)
{
	_cfg.currentFont = a_font;
	IconFont::Manager::GetSingleton()->SetFontName(a_font);
}

// ==================================================
// Accessors & Queries
// ==================================================

bool FUCKMan::ShouldRender() const
{
	if (_suspendRendering)
		return false;

	// Keep rendering if the main menu is open.
	if (_isOpen)
		return true;

	// Keep render loop active for open windows to maintain ImGui context across load screens.
	for (const auto* win : _windows) {
		if (win->IsOpen())
			return true;
	}

	return false;
}

bool FUCKMan::ShouldRenderExclusive() const
{
	if (_suspendRendering)
		return false;

	if (_isOpen)
		return true;

	for (const auto* win : _windows) {
		if (win->IsOpen() && !(win->GetFlags() & FUCK::WindowFlags::kPassInputToGame))
			return true;
	}

	return false;
}

bool FUCKMan::ShouldRenderPassthrough() const
{
	// Independent of _isOpen: these live on their own always-on HUD panel in
	// VR, so they render whether or not the main menu happens to be open too.
	if (_suspendRendering)
		return false;

	for (const auto* win : _windows) {
		if (win->IsOpen() && (win->GetFlags() & FUCK::WindowFlags::kPassInputToGame))
			return true;
	}

	return false;
}

bool FUCKMan::IsInputBlocked() const
{
	if (_isOpen || MANAGER(Camera)->IsBlockingInput())
		return true;

	ImVec2 mouse = ImGui::GetMousePos();

	for (const auto* win : _windows) {
		if (IsWindowSuppressed(win)) {
			continue;
		}

		std::string key = std::format("{}|{}", win->PluginName(), win->Id());
		auto        it  = s_windowStates.find(key);
		if (it != s_windowStates.end()) {
			const auto& winState = it->second;

			// Protect active interactions (dragging window or using widgets)
			if (winState.isFocused && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				return true;
			}

			// Protect Host Chrome
			if (winState.chromePos.x != -1.0f) {
				if (mouse.x >= winState.chromePos.x && mouse.x <= winState.chromePos.x + winState.chromeSize.x &&
					mouse.y >= winState.chromePos.y && mouse.y <= winState.chromePos.y + winState.chromeSize.y) {
					return true;
				}
			}
		}

		// Trust the plugin's requested flags for the content area
		if (!(win->GetFlags() & FUCK::WindowFlags::kPassInputToGame)) {
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
		// Deliberately skips IsWindowSuppressed. Used internally to check intended window behaviors.
		if (win->IsOpen() && (win->GetFlags() & a_flag)) {
			return true;
		}
	}
	return false;
}

// ==================================================
// Open / Close Logic
// ==================================================

void FUCKMan::Open()
{
	if (_isOpen)
		return;

	_isOpen = true;

	_isCollapsed = false;

	_forceCursor = false;

	ImGui::ClearNavState();

	ImGui::Styles::GetSingleton()->OnStyleRefresh();

	if (_activeTool)
		_activeTool->OnOpen();

	_pendingWindowRestore = true;

	ImGui::PlayAudio(ImGui::Audio::kOk);
}

void FUCKMan::Close()
{
	FUCK::AbortBinding();

	if (!_isOpen)
		return;
	_isOpen = false;

	if (_activeTool)
		_activeTool->OnClose();

	_forceCursor = false;

	if (!IsInputBlocked()) {
		MANAGER(Input)->ClearState();
	}

	UpdateGameState();

	ImGui::ClearNavState();
	ImGui::PlayAudio(ImGui::Audio::kCancel);

	// Anything still waiting is written now, rather than half a second from now
	FlushWorkspace(true);
}

void FUCKMan::Toggle()
{
	_isOpen ? Close() : Open();
}

// Vanilla DirectionHandler repeat timings. Some weird interactions have led to keys spamming. A lot of logging and testing of input code
// has led seemingly to the conclusion that the DirectionHandler is being zeroed out by some other code, and this is a workaround to restore it when it's caught.
namespace DirectionRepeat
{
	static constexpr std::ptrdiff_t kDelayOffset    = 0x14;
	static constexpr std::ptrdiff_t kIntervalOffset = 0x18;

	static float s_delay    = 0.0f;
	static float s_interval = 0.0f;

	static RE::MenuEventHandler* GetHandler()
	{
		// Layout only verified on flat.
		if (REL::Module::IsVR())
			return nullptr;

		auto mc = RE::MenuControls::GetSingleton();
		if (!mc)
			return nullptr;

		const auto vtbl = RE::VTABLE_DirectionHandler[0].address();
		for (auto* h : mc->handlers) {
			if (h && *reinterpret_cast<const std::uintptr_t*>(h) == vtbl)
				return h;
		}
		return nullptr;
	}

	static void Sync()
	{
		auto handler = GetHandler();
		if (!handler)
			return;

		auto  base     = reinterpret_cast<std::uint8_t*>(handler);
		auto& delay    = *reinterpret_cast<float*>(base + kDelayOffset);
		auto& interval = *reinterpret_cast<float*>(base + kIntervalOffset);

		if (delay > 0.0f && interval > 0.0f) {
			s_delay    = delay;
			s_interval = interval;
		} else if (s_delay > 0.0f && s_interval > 0.0f) {
			delay    = s_delay;
			interval = s_interval;
			logger::info("FUCK: Restored menu direction repeat ({}, {})", s_delay, s_interval);
		}
	}
}

EventResult FUCKMan::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (!a_event)
		return EventResult::kContinue;

	DirectionRepeat::Sync();

	DispatchMenuEvent(a_event->menuName.c_str(), a_event->opening);

	if (a_event->opening) {
		if (a_event->menuName == RE::MainMenu::MENU_NAME) {
			ImGui::ClearFormCaches();
		}

		// Close automatically behind dominant fullscreen game menus
		if (std::ranges::find(s_closeOnOpen, a_event->menuName.data()) != s_closeOnOpen.end()) {
			bool closedSomething = false;

			if (_isOpen && a_event->menuName != RE::Console::MENU_NAME) {
				Close();
				closedSomething = true;
			}

			if (closedSomething) {
				UpdateGameState();
				ImGui::ClearNavState();
			}
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}

// ==================================================
// CORE RENDER LOOP
// ==================================================

FUCKMan::LayoutMetrics FUCKMan::CalcLayoutMetrics() const
{
	auto iconArrow = MANAGER(IconFont)->GetStepperRight();

	LayoutMetrics m;
	m.uiScale = FUCK::GetResolutionScale();
	m.textH   = FUCK::GetTextLineHeight();

	const ImVec2& themePadding = ImGui::Styles::GetSingleton()->user.windowPadding;
	m.pad                      = { themePadding.x * m.uiScale, themePadding.y * m.uiScale };

	m.iconAspect = (iconArrow && iconArrow->imageSize.y > 0.0f) ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

	m.chromeIconBaseH = 20.0f * m.uiScale;

	m.headerPadding = 3.0f * m.uiScale;

	const float kChromeFontSize = 22.0f * 0.9f * m.uiScale;

	m.titleFontSize    = kChromeFontSize;
	m.titleH           = m.titleFontSize + (m.headerPadding * 4.0f);
	m.titleIconPadX    = 8.0f * m.uiScale;
	m.titleIconNudgeY  = 1.0f * m.uiScale;  // positive = down, negative = up
	m.titleTextOffsetY = (m.titleH - m.titleFontSize) * 0.5f + (1.0f * m.uiScale);

	m.sidebarWidth    = 250.0f * m.uiScale;
	m.sidebarItemH    = 30.0f * m.uiScale;
	m.sidebarFontSize = kChromeFontSize;
	m.sidebarIndent   = 15.0f * m.uiScale;

	return m;
}

// Content scale, respecting kIgnoreUserScale
void FUCKMan::PushContentScale(const LayoutMetrics& a_m, bool a_ignoreUserScale)
{
	_isIgnoringUserScale = a_ignoreUserScale;
	_activeScale         = a_ignoreUserScale ? 1.0f : _cfg.userScale;

	float targetFontSize = ImGui::GetStyle().FontSizeBase * _activeScale * a_m.uiScale;
	ImGui::PushFont(nullptr, targetFontSize);

	auto styleUser = ImGui::Styles::GetSingleton()->user;

	float spaceX = styleUser.itemSpacing.x * a_m.uiScale * _activeScale;
	float spaceY = styleUser.itemSpacing.y * a_m.uiScale * _activeScale;

	float innerSpaceX = 4.0f * a_m.uiScale * _activeScale;
	float innerSpaceY = 4.0f * a_m.uiScale * _activeScale;

	float indentSpacing = styleUser.indentSpacing;

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * a_m.uiScale * _activeScale, 3.0f * a_m.uiScale * _activeScale));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spaceX, spaceY));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(innerSpaceX, innerSpaceY));
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indentSpacing * a_m.uiScale * _activeScale);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12.0f * a_m.uiScale);
	ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 10.0f * a_m.uiScale);
}

void FUCKMan::PopContentScale()
{
	ImGui::PopFont();
	ImGui::PopStyleVar(6);
	_activeScale         = _cfg.userScale;
	_isIgnoringUserScale = false;
}

// ==================================================
// CORE RENDER LOOP
// ==================================================

void FUCKMan::Draw(bool a_passthroughOnly)
{
	FlushPendingRegistrations();

	if (!_workspaceLoaded) {
		LoadWorkspace();
		_workspaceLoaded = true;
	}

	// Writes the workspace once changes have settled
	FlushWorkspace();

	// Draw stops being called once nothing is shown, so whatever's still waiting is written as the last thing closes
	struct FlushWhenHidden
	{
		FUCKMan* manager;
		~FlushWhenHidden()
		{
			if (!manager->ShouldRender())
				manager->FlushWorkspace(true);
		}
	} flushWhenHidden{ this };

	if (auto ui = RE::UI::GetSingleton(); ui && ui->closingAllMenus) {
		if (_isOpen) {
			Close();
			ImGui::ClearNavState();
		}
		return;
	}

	// The game's HUD state, for kHideWithHUD windows
	RefreshGameHUDState();

	UpdateGameState();

	// Before any window is drawn, so it decides who gets the mouse this frame
	if (_isOpen)
		ClaimEditHandleHover();

	// If the user hides menus via 'tm', the Skyrim Scaleform cursor becomes invisible.
	// We force ImGui to draw its own cursor so KBM users aren't clicking blind. Flatrim only.
	bool menusHidden = false;
	if (!REL::Module::IsVR()) {
		if (auto ui = RE::UI::GetSingleton()) {
			menusHidden = !ui->IsShowingMenus();
		}
	}

	if (menusHidden && IsInputBlocked() && Input::Manager::GetSingleton()->IsInputKBM()) {
		ImGui::GetIO().MouseDrawCursor = true;
	} else {
		ImGui::GetIO().MouseDrawCursor = false;
	}

	// ImGui's tooltips sit past the mouse by an offset it scales with this, so they clear the game cursor. Only while the
	// cursor is what places them, as gamepad navigation anchors them to the item instead
	{
		const auto* input        = Input::Manager::GetSingleton();
		const bool  cursorPlaces = input->IsInputKBM() || input->IsCursorMovedByJoystick();

		ImGui::GetStyle().MouseCursorScale = cursorPlaces ? input->GetCursorScale() : 1.0f;
	}

	// Global mouse click detection for auto-closing flyout windows
	bool        isAnyMouseDown    = MANAGER(Input)->IsInputDown(Input::Keymap::kMBBase) || MANAGER(Input)->IsInputDown(Input::Keymap::kMBBase + 1);
	static bool s_wasAnyMouseDown = false;
	bool        globalMouseClick  = isAnyMouseDown && !s_wasAnyMouseDown;
	s_wasAnyMouseDown             = isAnyMouseDown;

	const LayoutMetrics m = CalcLayoutMetrics();

	// Belongs on the exclusive-focus panel only.
	if (!a_passthroughOnly && (_activeTool || ShouldRender()))
		DrawOverlays(m, menusHidden);

	DrawPluginWindows(m, a_passthroughOnly, globalMouseClick);

	if (a_passthroughOnly || !_isOpen || menusHidden)
		return;

	DrawMainWindow(m);
}

// Fullscreen transparent layer for every tool's and window's RenderOverlay()
void FUCKMan::DrawOverlays(const LayoutMetrics& a_m, bool a_menusHidden)
{
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
		PushContentScale(a_m);

		if (_activeTool && !a_menusHidden) {
			_currentRenderingPlugin = _activeTool->PluginName();
			_activeTool->RenderOverlay();
		}

		for (auto* tool : _tools) {
			if (tool != _activeTool && !a_menusHidden) {
				_currentRenderingPlugin = tool->PluginName();
				tool->RenderOverlay();
			}
		}

		for (auto* win : _windows) {
			// Skip overlays for suppressed windows.
			if (!IsWindowSuppressed(win)) {
				_currentRenderingPlugin = win->PluginName();
				win->RenderOverlay();
			}
		}

		_currentRenderingPlugin.clear();

		PopContentScale();
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void FUCKMan::DrawMainWindow(const LayoutMetrics& a_m)
{
	auto iconArrow   = MANAGER(IconFont)->GetStepperRight();
	auto chromeArrow = [&](bool pointsDown, float rowH) {
		return ImGui::CalcArrowIconParams(a_m.iconAspect, pointsDown, rowH, a_m.chromeIconBaseH);
	};

	ImGui::GetIO().MouseDrawCursor = false;

	const float collapsedH = a_m.titleH * 1.02f;

	if (_pendingWindowRestore) {
		ClampWindowToScreen(_cfg.windowPos, _cfg.windowSize);
		ImGui::SetNextWindowPos(_cfg.windowPos, ImGuiCond_Always);
		if (!_isCollapsed) {
			ImGui::SetNextWindowSize(_cfg.windowSize, ImGuiCond_Always);
		} else {
			ImGui::SetNextWindowSize(ImVec2(_cfg.windowSize.x, collapsedH), ImGuiCond_Always);
		}
		_pendingWindowRestore = false;
	} else {
		if (_isCollapsed) {
			ImGui::SetNextWindowSize(ImVec2(_cfg.windowSize.x, collapsedH));
		} else if (_wasCollapsed && !_isCollapsed) {
			ImGui::SetNextWindowSize(_cfg.windowSize);
		} else {
			ImGui::SetNextWindowSize(_cfg.windowSize, ImGuiCond_FirstUseEver);
		}
	}

	_wasCollapsed = _isCollapsed;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	std::string windowTitle = std::format("##{}", "$FUCK_Title"_T);
	bool        wantsOpen   = true;

	ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
	if (_isCollapsed)
		winFlags |= ImGuiWindowFlags_NoResize;

	// ImGui only rounds window corners outwards, so with inverted corners the background and border are drawn here
	const bool themedBg = ImGui::PushThemedWindowBg();
	const bool visible  = ImGui::Begin(windowTitle.c_str(), &wantsOpen, winFlags);
	ImGui::PopThemedWindowBg(themedBg);

	if (visible) {
		ImGui::ExtendWindowPastBorder();

		if (!_isCollapsed) {
			_cfg.windowPos  = ImGui::GetWindowPos();
			_cfg.windowSize = ImGui::GetWindowSize();

			// Auto-save settings on move/resize end
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				if (_cfg.windowPos.x != _lastSavedPos.x || _cfg.windowPos.y != _lastSavedPos.y ||
					_cfg.windowSize.x != _lastSavedSize.x || _cfg.windowSize.y != _lastSavedSize.y) {
					_lastSavedPos  = _cfg.windowPos;
					_lastSavedSize = _cfg.windowSize;
					SaveWorkspace();
				}
			}
		}

		// -- Custom Title Bar (unscaled) --
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
			ImGui::BeginGroup();

			float winWidth = ImGui::GetWindowSize().x;

			// Inverted corners cut into the ends of the title bar, so its icons move in clear of them
			const float cornerInset = themedBg ? ImGui::GetWindowCornerInset() : 0.0f;

			ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
			cursorScreen.x += cornerInset;

			// 1. Collapse Icon
			if (iconArrow) {
				// Calculate exact physical dimensions of the arrow first
				bool pointsDown = !_isCollapsed;
				auto ap         = chromeArrow(pointsDown, a_m.titleH);

				float maxIconDim = std::max(ap.drawSize.x, ap.drawSize.y);
				float btnWidth   = (a_m.titleIconPadX * 2.0f) + maxIconDim;

				ImGui::SetCursorPos({ cornerInset, 0.0f });
				// Tightly wrap the invisible button around the graphic
				if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(btnWidth, a_m.titleH))) {
					_isCollapsed = !_isCollapsed;
				}

				bool  isHovered = ImGui::IsItemHovered();
				ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

				ImVec2 drawPos = cursorScreen;
				drawPos.x += a_m.titleIconPadX + iconOffsetX;
				drawPos.y += ap.offsetY + a_m.titleIconNudgeY;

				ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
					pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
			}

			// 2. Close Button
			float btnSize = a_m.titleH;
			float xPos    = winWidth - btnSize - a_m.headerPadding - cornerInset;

			ImGui::SetCursorPos({ xPos, 0.0f });
			ImVec2 btnCursor = ImGui::GetCursorScreenPos();

			if (ImGui::InvisibleButton("##CloseBtn", ImVec2(btnSize, btnSize))) {
				wantsOpen = false;
			}

			{
				const char* xIcon      = ICON_FA_XMARK;
				float       uiFontSize = ImGui::GetStyle().FontSizeBase * a_m.uiScale;

				ImGui::PushFont(nullptr, uiFontSize);
				ImVec2 textSize = ImGui::CalcTextSize(xIcon);
				ImGui::PopFont();

				ImVec2 textPos = {
					btnCursor.x + (btnSize - textSize.x) * 0.5f,
					btnCursor.y + (btnSize - textSize.y) * 0.5f + (1.0f * a_m.uiScale)
				};

				ImU32 xColor = ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				ImGui::GetWindowDrawList()->AddText(
					ImGui::GetFont(),
					uiFontSize,
					textPos,
					xColor,
					xIcon);
			}

			ImGui::EndGroup();
			ImGui::PopStyleVar();

			// Double Click Header
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
				ImVec2 mousePos = ImGui::GetMousePos();
				ImVec2 wP       = ImGui::GetWindowPos();
				if (mousePos.x >= wP.x && mousePos.x <= wP.x + winWidth &&
					mousePos.y >= wP.y && mousePos.y <= wP.y + a_m.titleH) {
					_isCollapsed = !_isCollapsed;
				}
			}
		}

		// -- Content --
		if (!_isCollapsed && !_wasCollapsed) {
			float contentY = a_m.titleH;
			ImGui::SetCursorPos({ 0, contentY });

			float availHeight = ImGui::GetContentRegionAvail().y;

			// -- Sidebar (unscaled), drawn in FUCK-Sidebar.cpp --
			auto renderSidebar = [&]() {
				DrawSidebar({ a_m.uiScale, a_m.iconAspect, a_m.chromeIconBaseH, a_m.sidebarWidth, a_m.sidebarItemH, a_m.sidebarFontSize, a_m.sidebarIndent }, availHeight);
			};

			// -- Content (scaled) --
			auto renderContent = [&](float width) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, a_m.pad);
				// Padded even when the theme's border size is 0, as ImGui drops a child's padding when it has no border
				ImGui::BeginChildThemed("Content", ImVec2(width, availHeight), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
				{
					PushContentScale(a_m);
					if (_activeTool)
						_activeTool->Draw();
					else
						ImGui::CenteredText("$FUCK_NoToolSelected"_T, true);

					PopContentScale();
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
			};

			if (_cfg.sidebarOnRight) {
				float contentWidth = ImGui::GetContentRegionAvail().x - a_m.sidebarWidth - ImGui::GetStyle().ItemSpacing.x;
				renderContent(contentWidth);
				ImGui::SameLine();
				renderSidebar();
			} else {
				renderSidebar();
				ImGui::SameLine();
				renderContent(0.0f);
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	if (!wantsOpen)
		Close();
}
