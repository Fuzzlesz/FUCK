#include "FUCK-Man.h"
#include "FUCK-Host.h"

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
#include "System/Utils.h"

struct WindowState
{
	bool   isCollapsed  = false;
	bool   wasCollapsed = false;
	ImVec2 preCollapseSize{};
	ImVec2 pos{ -1.0f, -1.0f };
	ImVec2 size{ -1.0f, -1.0f };
	bool   hasLoadedPos = false;
};
static StringMap<WindowState> s_windowStates;  // Maps using "PluginName|WindowId"

// Auto-Close list for Game Menus
static constexpr std::array<std::string_view, 10> s_closeOnOpen = {
	RE::Console::MENU_NAME.data(),     RE::ContainerMenu::MENU_NAME.data(),
	RE::JournalMenu::MENU_NAME.data(), RE::InventoryMenu::MENU_NAME.data(),
	RE::MapMenu::MENU_NAME.data(),     RE::DialogueMenu::MENU_NAME.data(),
	RE::MagicMenu::MENU_NAME.data(),   RE::StatsMenu::MENU_NAME.data(),
	RE::TweenMenu::MENU_NAME.data(),   RE::FavoritesMenu::MENU_NAME.data()
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

// ==================================================
// Glaze JSON Serialization Structs
// ==================================================
struct WindowSaveData
{
	float x         = -1.0f;
	float y         = -1.0f;
	float w         = -1.0f;
	float h         = -1.0f;
	bool  collapsed = false;
};

struct ToolSaveData
{
	bool        bFavourited = false;
	bool        bHidden     = false;
	std::string sName       = "";
	std::string sGroup      = "";
};

struct PluginSaveData
{
	StringMap<ToolSaveData>   tools;
	StringMap<WindowSaveData> windows;
};

struct GroupSaveData
{
	bool        bFavourited = false;
	std::string sName       = "";
	int         iOrder      = 0;
};

struct WorkspaceSaveData
{
	WindowSaveData           mainWindow;
	StringMap<int>           toolOrder;
	StringMap<GroupSaveData> groups;
};

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
	// Pointer & Null-String Check
	if (!a_tool || !a_tool->Name() || !a_tool->PluginName()) {
		logger::info("FUCK: Attempted to register a Tool with a null Name or PluginName.");
		return;
	}

	if (std::find(_tools.begin(), _tools.end(), a_tool) != _tools.end()) {
		return;
	}

	// Name Collision Check
	auto it = std::find_if(_tools.begin(), _tools.end(), [&](FUCK::ITool* existing) {
		return existing && (strcmp(existing->Name(), a_tool->Name()) == 0) && (strcmp(existing->PluginName(), a_tool->PluginName()) == 0);
	});

	if (it != _tools.end()) {
		return;
	}

	_tools.push_back(a_tool);
}

void FUCKMan::RegisterWindow(FUCK::IWindow* a_window)
{
	// Pointer & Null-String Check
	if (!a_window || !a_window->Id() || !a_window->PluginName()) {
		logger::info("FUCK: Attempted to register a Window with a null Id or PluginName.");
		return;
	}

	if (std::find(_windows.begin(), _windows.end(), a_window) != _windows.end())
		return;

	// Check for collisions based on PluginName + Id
	auto it = std::find_if(_windows.begin(), _windows.end(), [&](FUCK::IWindow* existing) {
		return existing && (strcmp(existing->Id(), a_window->Id()) == 0) && (strcmp(existing->PluginName(), a_window->PluginName()) == 0);
	});

	if (it != _windows.end())
		return;

	_windows.push_back(a_window);
}

void FUCKMan::UnregisterWindow(FUCK::IWindow* a_window)
{
	if (!a_window)
		return;

	auto it = std::find(_windows.begin(), _windows.end(), a_window);
	if (it != _windows.end()) {
		std::string key = std::format("{}|{}", a_window->PluginName(), a_window->Id());
		// Clean up the persistent collapse/geometry state
		s_windowStates.erase(key);

		// Remove from render list
		_windows.erase(it);
	}

	// Clean up from suspended windows
	std::erase(_suspendedWindows, a_window);
}

// ==================================================
// Sidebar & Geometry Management
// ==================================================

void FUCKMan::LoadWorkspace()
{
	float scale            = ImGui::Renderer::GetResolutionScale();
	bool  mainWindowLoaded = false;

	// 1. Load global Workspace settings (Main Window, Groups & Tool Order)
	std::string workspacePath = Settings::GetSingleton()->GetWorkspacePath();
	if (fs::exists(workspacePath)) {
		WorkspaceSaveData sd;
		std::string       wBuffer;
		if (auto err = glz::read_file_json(sd, workspacePath, wBuffer); !err) {
			// Apply Main Window state
			if (sd.mainWindow.x != -1.0f && sd.mainWindow.y != -1.0f) {
				_cfg.windowPos  = { sd.mainWindow.x * scale, sd.mainWindow.y * scale };
				_cfg.windowSize = { sd.mainWindow.w * scale, sd.mainWindow.h * scale };
				_isCollapsed    = sd.mainWindow.collapsed;

				_lastSavedPos         = _cfg.windowPos;
				_lastSavedSize        = _cfg.windowSize;
				_pendingWindowRestore = true;
				mainWindowLoaded      = true;
			}

			for (const auto& [k, v] : sd.toolOrder) {
				_toolOverrides[k].sortOrder = v;
			}
			for (const auto& [k, v] : sd.groups) {
				_groupOverrides[k].isFavourited = v.bFavourited;
				_groupOverrides[k].customName   = v.sName;
				_groupOverrides[k].sortOrder    = v.iOrder;
			}
		}
	}

	if (!mainWindowLoaded) {
		_cfg.windowPos  = { _def.windowPos.x * scale, _def.windowPos.y * scale };
		_cfg.windowSize = { _def.windowSize.x * scale, _def.windowSize.y * scale };
		_lastSavedPos   = _cfg.windowPos;
		_lastSavedSize  = _cfg.windowSize;
	}

	// 2. Load per-plugin tool states and window geometries
	std::string toolsPath = Settings::GetSingleton()->GetToolsPath();
	if (fs::exists(toolsPath)) {
		for (const auto& entry : fs::directory_iterator(toolsPath)) {
			if (entry.path().extension() == ".json") {
				std::string pluginName = entry.path().stem().string();

				PluginSaveData pd;
				std::string    pBuffer;
				if (auto err = glz::read_file_json(pd, entry.path().string(), pBuffer); !err) {
					for (const auto& [toolName, toolData] : pd.tools) {
						std::string key                  = std::format("{}|{}", pluginName, toolName);
						_toolOverrides[key].isFavourited = toolData.bFavourited;
						_toolOverrides[key].isHidden     = toolData.bHidden;
						_toolOverrides[key].customName   = toolData.sName;
						_toolOverrides[key].customGroup  = toolData.sGroup;
					}

					for (const auto& [winId, winData] : pd.windows) {
						std::string key = std::format("{}|{}", pluginName, winId);
						auto&       st  = s_windowStates[key];

						if (winData.x != -1.0f && winData.y != -1.0f) {
							st.pos          = { winData.x * scale, winData.y * scale };
							st.hasLoadedPos = true;
						}
						if (winData.w != -1.0f && winData.h != -1.0f) {
							st.size = { winData.w * scale, winData.h * scale };
						}
						st.isCollapsed = winData.collapsed;
					}
				}
			}
		}
	}
}

void FUCKMan::SaveWorkspace()
{
	WorkspaceSaveData         sd;
	StringMap<PluginSaveData> pdMap;

	// Distribute Tool Overrides
	for (const auto& [key, over] : _toolOverrides) {
		auto pipe = key.find('|');
		if (pipe != std::string::npos) {
			std::string plugin   = key.substr(0, pipe);
			std::string toolName = key.substr(pipe + 1);

			if (over.isFavourited || over.isHidden || !over.customName.empty() || !over.customGroup.empty()) {
				ToolSaveData td;
				td.bFavourited = over.isFavourited;
				td.bHidden     = over.isHidden;
				td.sName       = over.customName;
				td.sGroup      = over.customGroup;

				pdMap[plugin].tools[toolName] = td;
			}

			if (over.sortOrder != 0) {
				sd.toolOrder[key] = over.sortOrder;
			}
		}
	}

	// Distribute Group Overrides
	for (const auto& [grp, over] : _groupOverrides) {
		if (over.isFavourited || !over.customName.empty() || over.sortOrder != 0) {
			GroupSaveData gd;
			gd.bFavourited = over.isFavourited;
			gd.sName       = over.customName;
			gd.iOrder      = over.sortOrder;

			sd.groups[grp] = gd;
		}
	}

	// Distribute Main Window Geometry
	float scale = ImGui::Renderer::GetResolutionScale();
	if (scale < 0.1f)
		scale = 1.0f;  // Safety clamp

	sd.mainWindow.x         = _cfg.windowPos.x / scale;
	sd.mainWindow.y         = _cfg.windowPos.y / scale;
	sd.mainWindow.w         = _cfg.windowSize.x / scale;
	sd.mainWindow.h         = _cfg.windowSize.y / scale;
	sd.mainWindow.collapsed = _isCollapsed;

	// Distribute Window Geometries (External plugins)
	for (const auto& [key, st] : s_windowStates) {
		auto pipe = key.find('|');
		if (pipe != std::string::npos) {
			std::string plugin = key.substr(0, pipe);
			std::string winId  = key.substr(pipe + 1);

			WindowSaveData wd;
			wd.x         = st.pos.x / scale;
			wd.y         = st.pos.y / scale;
			wd.w         = (st.size.x > 0.0f) ? st.size.x / scale : -1.0f;
			wd.h         = (st.size.y > 0.0f) ? st.size.y / scale : -1.0f;
			wd.collapsed = st.isCollapsed;

			pdMap[plugin].windows[winId] = wd;
		}
	}

	// Commit Workspace Settings
	std::string workspacePath = Settings::GetSingleton()->GetWorkspacePath();
	fs::create_directories(Settings::Core.GetConfigDirectory());
	std::string sBuffer;
	if (auto err = glz::write_file_json(sd, workspacePath, sBuffer); err) {
		logger::warn("Failed to write to {}", workspacePath);
	}

	// Commit Per-Plugin Settings
	std::string toolsPath = Settings::GetSingleton()->GetToolsPath();
	fs::create_directories(toolsPath);

	for (const auto& [plugin, pd] : pdMap) {
		std::string path = std::format("{}/{}.json", toolsPath, plugin);

		if (pd.tools.empty() && pd.windows.empty()) {
			if (fs::exists(path)) {
				fs::remove(path);
			}
		} else {
			std::string pBuffer;
			if (auto err = glz::write_file_json(pd, path, pBuffer); err) {
				logger::warn("Failed to write to {}", path);
			}
		}
	}
}

// ==================================================
// Input Processing
// ==================================================

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
	float loadedScale = FUCK::INI::LoadFloat(a_ini, "Settings", "fUserScale", _def.userScale);
	_cfg.userScale    = std::clamp(loadedScale, 0.5f, 2.0f);

	_cfg.globalPauseType = FUCK::INI::LoadInt(a_ini, "Settings", "iGlobalPauseType", _def.globalPauseType);

	_cfg.sidebarOnRight        = FUCK::INI::LoadBool(a_ini, "Settings", "bSidebarOnRight", _def.sidebarOnRight);
	_cfg.injectSystemMenu      = FUCK::INI::LoadBool(a_ini, "Settings", "bInjectSystemMenu", _def.injectSystemMenu);
	_cfg.replaceHelpMenu       = FUCK::INI::LoadBool(a_ini, "Settings", "bReplaceHelpMenu", _def.replaceHelpMenu);
	_cfg.showSidebarFilter     = FUCK::INI::LoadBool(a_ini, "Settings", "bShowSidebarFilter", _def.showSidebarFilter);
	_cfg.showSidebarFavourites = FUCK::INI::LoadBool(a_ini, "Settings", "bShowSidebarFavourites", _def.showSidebarFavourites);
	_cfg.groupFavourites       = FUCK::INI::LoadBool(a_ini, "Settings", "bGroupFavourites", _def.groupFavourites);
}

void FUCKMan::SaveSettings(CSimpleIniA& a_ini)
{
	FUCK::INI::SaveInt(a_ini, "Settings", "iGlobalPauseType", static_cast<int>(_cfg.globalPauseType), static_cast<int>(_def.globalPauseType));

	FUCK::INI::SaveDouble(a_ini, "Settings", "fUserScale", _cfg.userScale, _def.userScale);

	FUCK::INI::SaveBool(a_ini, "Settings", "bSidebarOnRight", _cfg.sidebarOnRight, _def.sidebarOnRight);
	FUCK::INI::SaveBool(a_ini, "Settings", "bInjectSystemMenu", _cfg.injectSystemMenu, _def.injectSystemMenu);
	FUCK::INI::SaveBool(a_ini, "Settings", "bReplaceHelpMenu", _cfg.replaceHelpMenu, _def.replaceHelpMenu);
	FUCK::INI::SaveBool(a_ini, "Settings", "bShowSidebarFilter", _cfg.showSidebarFilter, _def.showSidebarFilter);
	FUCK::INI::SaveBool(a_ini, "Settings", "bShowSidebarFavourites", _cfg.showSidebarFavourites, _def.showSidebarFavourites);
	FUCK::INI::SaveBool(a_ini, "Settings", "bGroupFavourites", _cfg.groupFavourites, _def.groupFavourites);
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

void FUCKMan::UpdateGameState()
{
	bool targetSoft    = _apiSoftPause;
	bool targetHard    = _apiHardPause;
	bool targetBlur    = false;
	bool targetHideHUD = false;
	bool targetVanity  = _isVanityBlocked;

	// 1. Main Menu State
	bool targetiHUDDisabled = _isOpen;

	if (_isOpen) {
		targetVanity  = true;
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
				targetHideHUD      = true;
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
					// but not deez
					if (name == RE::CursorMenu ::MENU_NAME ||
						name == RE::Console    ::MENU_NAME ||
						name == RE::LoadingMenu::MENU_NAME ||
						name == RE::HUDMenu    ::MENU_NAME ||
						name == RE::FaderMenu  ::MENU_NAME) {
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

// ==================================================
// Accessors & Queries
// ==================================================

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

// ==================================================
// Open / Close Logic
// ==================================================

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

EventResult FUCKMan::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (!a_event)
		return EventResult::kContinue;

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
		// Restore any suspended windows once all blocking game menus have closed
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

// ==================================================
// CORE RENDER LOOP
// ==================================================

void FUCKMan::Draw()
{
	if (!_workspaceLoaded) {
		LoadWorkspace();
		_workspaceLoaded = true;
	}

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

		bool isForcedCamera = (activeState == camera->cameraStates[RE::CameraState::kVATS].get()      ||
							   activeState == camera->cameraStates[RE::CameraState::kBleedout].get()  ||
							   activeState == camera->cameraStates[RE::CameraState::kAutoVanity].get());

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

	// ==================================================
	// LAYOUT METRICS SETUP (Chrome / Unscaled)
	// ==================================================
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

	m.uiScale = FUCK::GetResolutionScale();
	m.textH   = FUCK::GetTextLineHeight();
	m.padBase = 15.0f * m.uiScale;

	m.iconAspect = (iconArrow && iconArrow->imageSize.y > 0.0f) ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

	m.chromeIconBaseH = 20.0f * m.uiScale;

	float headerPadding = 3.0f * m.uiScale;

	const float kChromeFontSize = 22.0f * 0.9f * m.uiScale;

	m.titleH           = m.textH + (headerPadding * 2.0f);
	m.titleFontSize    = kChromeFontSize;
	m.titleH           = m.titleFontSize + (headerPadding * 4.0f);
	m.titleIconPadX    = 8.0f * m.uiScale;
	m.titleIconNudgeY  = 1.0f * m.uiScale;  // positive = down, negative = up
	m.titleTextOffsetY = (m.titleH - m.titleFontSize) * 0.5f + (1.0f * m.uiScale);

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
		_activeScale         = ignoreUserScale ? 1.0f : _cfg.userScale;

		float targetFontSize = ImGui::GetStyle().FontSizeBase * _activeScale * m.uiScale;
		ImGui::PushFont(nullptr, targetFontSize);

		auto styleUser = ImGui::Styles::GetSingleton()->user;

		float spaceX = styleUser.itemSpacing.x * m.uiScale * _activeScale;
		float spaceY = styleUser.itemSpacing.y * m.uiScale * _activeScale;

		float innerSpaceX = 4.0f * m.uiScale * _activeScale;
		float innerSpaceY = 4.0f * m.uiScale * _activeScale;

		float indentSpacing = styleUser.indentSpacing;

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
		_activeScale         = _cfg.userScale;
		_isIgnoringUserScale = false;
	};

	// ==================================================
	// OVERLAY RENDER PASS (Fullscreen Transparent Layer)
	// ==================================================
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

			for (auto* win : _windows) {
				if (win->IsOpen())
					win->RenderOverlay();
			}

			popContentScale();
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	// ==================================================
	// EXTERNAL WINDOWS RENDER PASS
	// ==================================================
	for (auto* win : _windows) {
		if (win->IsOpen()) {
			const char*       title     = win->Title();
			FUCK::WindowFlags userFlags = win->GetFlags();

			// --- Setup & State ---
			std::string key      = std::format("{}|{}", win->PluginName(), win->Id());
			auto&       winState = s_windowStates[key];

			ImGuiWindowFlags flags = ImGuiWindowFlags_None;

			// --- Flags Setup ---
			bool noDecoration    = (userFlags & FUCK::WindowFlags::kNoDecoration);
			bool ignoreUserScale = (userFlags & FUCK::WindowFlags::kIgnoreUserScale);
			bool noResize        = (userFlags & FUCK::WindowFlags::kNoResize);
			bool autoResize      = (userFlags & FUCK::WindowFlags::kAutoResize);
			bool noMove          = (userFlags & FUCK::WindowFlags::kNoMove);

			flags |= ImGuiWindowFlags_NoTitleBar;
			if (noDecoration) {
				winState.isCollapsed  = false;
				winState.wasCollapsed = false;
				flags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
			} else {
				flags |= ImGuiWindowFlags_NoScrollbar;
			}

			if (noResize)
				flags |= ImGuiWindowFlags_NoResize;
			if (autoResize)
				flags |= ImGuiWindowFlags_AlwaysAutoResize;
			if (noMove)
				flags |= ImGuiWindowFlags_NoMove;

			bool poppedInvisibleBg = false;
			if (userFlags & FUCK::WindowFlags::kNoBackground) {
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

			if ((userFlags & FUCK::WindowFlags::kPassInputToGame) && !IsInputBlocked())
				flags |= ImGuiWindowFlags_NoInputs;

			// --- Collapse Logic ---
			bool isCollapsed      = winState.isCollapsed;
			bool wasCollapsed     = winState.wasCollapsed;
			winState.wasCollapsed = isCollapsed;

			// Get Metrics from Interface
			ImVec2 targetSize = win->GetDefaultSize();

			// Handle collapse override
			if (isCollapsed) {
				float targetW = (winState.size.x > 0.0f) ? winState.size.x : (winState.preCollapseSize.x > 0.0f) ? winState.preCollapseSize.x :
				                                                                                                   targetSize.x;
				targetSize.x  = targetW;
				flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
			} else if (wasCollapsed) {
				// Prefer the in-session captured pre-collapse size; fall back to persisted size
				if (winState.preCollapseSize.x > 0.0f)
					targetSize = winState.preCollapseSize;
				else if (winState.size.x > 0.0f)
					targetSize = winState.size;
			}

			// --- Position Logic ---
			ImVec2 requestedPos;
			if (win->GetRequestedPos(requestedPos)) {
				ClampWindowToScreen(requestedPos, targetSize);
				FUCK::SetNextWindowPos(requestedPos, ImGuiCond_Appearing);
			} else if (winState.hasLoadedPos) {
				ClampWindowToScreen(winState.pos, targetSize);
				FUCK::SetNextWindowPos(winState.pos, ImGuiCond_FirstUseEver);  // Applies at game launch
			} else {
				// Default position for new windows
				ImVec2 defPos = win->GetDefaultPos();
				ClampWindowToScreen(defPos, targetSize);
				FUCK::SetNextWindowPos(defPos, ImGuiCond_FirstUseEver);
			}

			// If AutoResize is active, ImGui shrinks the window dynamically. We bypass hard size forcing.
			if (!autoResize) {
				if (isCollapsed) {
					ImGui::SetNextWindowSizeConstraints(ImVec2(targetSize.x, 0.0f), ImVec2(targetSize.x, FLT_MAX));
				} else {
					ImVec2    sz       = ((winState.size.x != -1.0f) ? winState.size : targetSize);
					ImGuiCond sizeCond = (wasCollapsed || noResize) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
					FUCK::SetNextWindowSize(sz, sizeCond);
				}
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

				ImVec2 curPos  = FUCK::GetWindowPos();
				ImVec2 curSize = FUCK::GetWindowSize();

				// Auto-save settings on move/resize end preventing overwrite during collapse
				if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
					bool posChanged  = std::abs(curPos.x - winState.pos.x) > 1.0f || std::abs(curPos.y - winState.pos.y) > 1.0f;
					bool sizeChanged = !isCollapsed && (std::abs(curSize.x - winState.size.x) > 1.0f || std::abs(curSize.y - winState.size.y) > 1.0f);

					if (posChanged || sizeChanged) {
						winState.pos = curPos;
						if (!isCollapsed) {
							winState.size = curSize;
						}
						winState.hasLoadedPos = true;
						SaveWorkspace();
					}
				}

				if (win->GetFlags() & FUCK::WindowFlags::kExtendBorder)
					FUCK::ExtendWindowPastBorder();

				if (!noDecoration) {
					// --- Window Chrome Decoration (unscaled) ---
					float winWidth = FUCK::GetWindowSize().x;

					ImVec2 headerStartCursor = FUCK::GetCursorPos();
					ImVec2 cursorScreen      = FUCK::GetCursorScreenPos();

					FUCK::BeginGroup();

					// 1. Collapse Icon
					float iconW = 0.0f;

					if (iconArrow) {
						// Calculate exact physical dimensions of the arrow first
						bool pointsDown = !isCollapsed;
						auto ap         = chromeArrow(pointsDown, m.titleH);

						// Lock horizontal container width to its maximum dimension to prevent shifting on rotation
						float maxIconDim = std::max(ap.drawSize.x, ap.drawSize.y);
						float btnWidth   = (m.titleIconPadX * 2.0f) + maxIconDim;

						// Tightly wrap the invisible button around the graphic
						if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(btnWidth, m.titleH))) {
							winState.isCollapsed = !isCollapsed;
							if (!isCollapsed) {
								// Capture the true live size before we force it to titleH next frame
								winState.preCollapseSize = FUCK::GetWindowSize();
								// Also persist it as the canonical size so cross-session restore works
								winState.size = winState.preCollapseSize;
							}
							SaveWorkspace();
						}
						bool  isHovered = ImGui::IsItemHovered();
						ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

						float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

						ImVec2 drawPos = cursorScreen;
						drawPos.x += m.titleIconPadX + iconOffsetX;
						drawPos.y += ap.offsetY + m.titleIconNudgeY;

						ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
							pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);

						// Give the title text slightly more breathing room past the exact button width
						iconW = btnWidth + m.titleIconPadX;
					}

					// 2. Title Text
					ImFont* baseFont = ImGui::GetFont();

					FUCK::SetCursorPos({ iconW, m.titleTextOffsetY });
					ImGui::GetWindowDrawList()->AddText(baseFont, m.titleFontSize,
						FUCK::GetCursorScreenPos(), ImGui::GetColorU32(ImGuiCol_Text), title);

					// 3. Close Button
					const float btnSize = m.titleH;
					const float btnX    = winWidth - btnSize - headerPadding;

					FUCK::SetCursorPos({ btnX, 0 });
					if (ImGui::InvisibleButton("##WinClose", ImVec2(btnSize, btnSize))) {
						open = false;
					}

					{
						const char* xIcon      = ICON_FA_XMARK;
						float       uiFontSize = ImGui::GetStyle().FontSizeBase * m.uiScale;

						ImGui::PushFont(nullptr, uiFontSize);
						ImVec2 textSize = ImGui::CalcTextSize(xIcon);
						ImGui::PopFont();

						ImVec2 btnScreenPos = ImGui::GetItemRectMin();
						ImVec2 textPos      = {
                            btnScreenPos.x + (btnSize - textSize.x) * 0.5f,
                            btnScreenPos.y + (btnSize - textSize.y) * 0.5f + (1.0f * m.uiScale)
						};

						ImU32 xColor = ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

						ImGui::GetWindowDrawList()->AddText(
							baseFont,
							uiFontSize,
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
								winState.size            = winState.preCollapseSize;
							}
							SaveWorkspace();
						}
					}

					// 4. Separator
					FUCK::SetCursorPos({ headerStartCursor.x, m.titleH });
					FUCK::SeparatorThick();

					// 5. Content Child (scaled)
					if (!winState.isCollapsed && !isCollapsed) {
						FUCK::SetCursorPosX(0.0f);

						FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m.padBase, m.padBase));

						ImGuiChildFlags  childFlags  = ImGuiChildFlags_AlwaysUseWindowPadding;
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
		}
	}

	// ==================================================
	// MAIN FUCK WORKSPACE MENU RENDER PASS
	// ==================================================
	if (!_isOpen)
		return;

	ImGui::GetIO().MouseDrawCursor = false;

	const float collapsedH = m.titleH * 1.02f;

	if (_pendingWindowRestore) {
		ClampWindowToScreen(_cfg.windowPos, _cfg.windowSize);
		FUCK::SetNextWindowPos(_cfg.windowPos, ImGuiCond_Always);
		if (!_isCollapsed) {
			FUCK::SetNextWindowSize(_cfg.windowSize, ImGuiCond_Always);
		} else {
			FUCK::SetNextWindowSize(ImVec2(_cfg.windowSize.x, collapsedH), ImGuiCond_Always);
		}
		_pendingWindowRestore = false;
	} else {
		if (_isCollapsed) {
			FUCK::SetNextWindowSize(ImVec2(_cfg.windowSize.x, collapsedH));
		} else if (_wasCollapsed && !_isCollapsed) {
			FUCK::SetNextWindowSize(_cfg.windowSize);
		} else {
			FUCK::SetNextWindowSize(_cfg.windowSize, ImGuiCond_FirstUseEver);
		}
	}

	_wasCollapsed = _isCollapsed;

	FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	std::string windowTitle = std::format("##{}", "$FUCK_Title"_T);
	bool        wantsOpen   = true;

	ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
	if (_isCollapsed)
		winFlags |= ImGuiWindowFlags_NoResize;

	if (FUCK::BeginWindow(windowTitle.c_str(), &wantsOpen, winFlags)) {
		FUCK::ExtendWindowPastBorder();

		if (!_isCollapsed) {
			_cfg.windowPos  = FUCK::GetWindowPos();
			_cfg.windowSize = FUCK::GetWindowSize();

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
			FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
			FUCK::BeginGroup();

			float winWidth = FUCK::GetWindowSize().x;

			ImVec2 cursorScreen = FUCK::GetCursorScreenPos();

			// 1. Collapse Icon
			if (iconArrow) {
				// Calculate exact physical dimensions of the arrow first
				bool pointsDown = !_isCollapsed;
				auto ap         = chromeArrow(pointsDown, m.titleH);

				float maxIconDim = std::max(ap.drawSize.x, ap.drawSize.y);
				float btnWidth   = (m.titleIconPadX * 2.0f) + maxIconDim;

				FUCK::SetCursorPos({ 0, 0 });
				// Tightly wrap the invisible button around the graphic
				if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(btnWidth, m.titleH))) {
					_isCollapsed = !_isCollapsed;
				}

				bool  isHovered = ImGui::IsItemHovered();
				ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

				ImVec2 drawPos = cursorScreen;
				drawPos.x += m.titleIconPadX + iconOffsetX;
				drawPos.y += ap.offsetY + m.titleIconNudgeY;

				ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
					pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
			}

			// 2. Close Button
			float btnSize = m.titleH;
			float xPos    = winWidth - btnSize - headerPadding;

			FUCK::SetCursorPos({ xPos, 0.0f });
			ImVec2 btnCursor = FUCK::GetCursorScreenPos();

			if (ImGui::InvisibleButton("##CloseBtn", ImVec2(btnSize, btnSize))) {
				wantsOpen = false;
			}

			{
				const char* xIcon      = ICON_FA_XMARK;
				float       uiFontSize = ImGui::GetStyle().FontSizeBase * m.uiScale;

				ImGui::PushFont(nullptr, uiFontSize);
				ImVec2 textSize = ImGui::CalcTextSize(xIcon);
				ImGui::PopFont();

				ImVec2 textPos = {
					btnCursor.x + (btnSize - textSize.x) * 0.5f,
					btnCursor.y + (btnSize - textSize.y) * 0.5f + (1.0f * m.uiScale)
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
			FUCK::PopStyleVar();

			// Double Click Header
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
				ImVec2 mousePos = ImGui::GetMousePos();
				ImVec2 wP       = FUCK::GetWindowPos();
				if (mousePos.x >= wP.x && mousePos.x <= wP.x + winWidth &&
					mousePos.y >= wP.y && mousePos.y <= wP.y + m.titleH) {
					_isCollapsed = !_isCollapsed;
				}
			}
		}

		// -- Content --
		if (!_isCollapsed && !_wasCollapsed) {
			float contentY = m.titleH;
			FUCK::SetCursorPos({ 0, contentY });

			float availHeight = FUCK::GetContentRegionAvail().y;

			// -- Sidebar (unscaled) --
			auto renderSidebar = [&]() {
				const float topPadding       = 2.0f * m.uiScale;
				const float bottomPadding    = 2.0f * m.uiScale;
				const float textVisualOffset = 1.0f * m.uiScale;

				ImFont* regularFont = MANAGER(IconFont)->GetRegularFont();

				auto GetOverrides = [&](FUCK::ITool* t) -> ToolOverrideState& {
					return _toolOverrides[std::format("{}|{}", t->PluginName(), t->Name())];
				};

				std::vector<FUCK::ITool*>            favTools;
				std::vector<FUCK::ITool*>            looseTools;
				StringMap<std::vector<FUCK::ITool*>> toolGroups;

				FUCK::BeginChild("Sidebar", ImVec2(m.sidebarWidth, availHeight), true, ImGuiWindowFlags_None);
				{
					FUCK::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
					ImVec2 headerStart = FUCK::GetCursorPos();
					headerStart.y += topPadding;

					// --- HEADER: TOOLS (Centred) ---
					FUCK::SetCursorPos(headerStart);

					// Bypassing FUCK::PushFont scaling
					ImGui::PushFont(regularFont, m.sidebarFontSize);
					float       textHeightCalc = ImGui::GetTextLineHeight();
					float       sidebarAvailW  = FUCK::GetContentRegionAvail().x;
					const char* headerText     = "$FUCK_Tools"_T;
					float       headerW        = ImGui::CalcTextSize(headerText).x;

					FUCK::SetCursorPosX(headerStart.x + (sidebarAvailW - headerW) * 0.5f);
					FUCK::SetCursorPosY(headerStart.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset);
					FUCK::Text(headerText);
					ImGui::PopFont();

					FUCK::SetCursorPos(ImVec2(headerStart.x, headerStart.y + m.sidebarItemH));
					FUCK::SeparatorThick();

					// --- FILTER BOX ---
					static char filterBuf[128] = "";
					if (_cfg.showSidebarFilter) {
						ImGui::PushFont(regularFont, m.sidebarFontSize * 0.8f);
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f * m.uiScale, 6.0f * m.uiScale));
						ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * m.uiScale);
						ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f * m.uiScale);

						// Sync background with the active theme's input box styling
						ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kComboBoxTextBox));

						ImGui::SetNextItemWidth(-1.0f);
						ImGui::InputTextWithHint("##SidebarFilter", "$FUCK_Sidebar_FilterHint"_T, filterBuf, sizeof(filterBuf));

						ImGui::PopStyleColor(1);
						ImGui::PopStyleVar(3);
						ImGui::PopFont();

						FUCK::Dummy(ImVec2(0.0f, 4.0f * m.uiScale));
					}

					bool        filtering = filterBuf[0] != '\0';
					std::string lowerFilter;
					if (filtering) {
						lowerFilter = string::tolower(filterBuf);
					}

					// Evaluate and distribute tools into collections
					for (auto* tool : _tools) {
						if (!tool->ShowInSidebar())
							continue;

						auto& over = GetOverrides(tool);
						if (over.isHidden)
							continue;

						std::string resolvedName = over.customName.empty() ? tool->Name() : over.customName;

						if (filtering) {
							std::string lowerName = string::tolower(resolvedName);
							auto        score     = rapidfuzz::fuzz::partial_token_ratio(lowerFilter, lowerName);
							if (score < 65.0)
								continue;
						}

						if (_cfg.showSidebarFavourites && over.isFavourited) {
							favTools.push_back(tool);
						} else {
							// Use the exact same resolution logic as the Settings Table
							std::string resolvedGrp = over.customGroup;
							if (resolvedGrp.empty())
								resolvedGrp = tool->Group() ? tool->Group() : "";
							if (resolvedGrp == "##ROOT")
								resolvedGrp = "";

							if (resolvedGrp.empty())
								looseTools.push_back(tool);
							else
								toolGroups[resolvedGrp].push_back(tool);
						}
					}

					auto sortTools = [&](FUCK::ITool* a, FUCK::ITool* b) {
						auto& oa = GetOverrides(a);
						auto& ob = GetOverrides(b);
						if (oa.sortOrder != ob.sortOrder)
							return oa.sortOrder < ob.sortOrder;
						std::string na = oa.customName.empty() ? a->Name() : oa.customName;
						std::string nb = ob.customName.empty() ? b->Name() : ob.customName;
						return _stricmp(na.c_str(), nb.c_str()) < 0;
					};

					struct SidebarEntry
					{
						std::string                label;
						std::string                origGroup;
						bool                       isGroup   = false;
						FUCK::ITool*               tool      = nullptr;
						std::vector<FUCK::ITool*>* tools     = nullptr;
						int                        sortOrder = 0;
					};

					std::vector<SidebarEntry> entries;

					if (!favTools.empty()) {
						std::sort(favTools.begin(), favTools.end(), sortTools);
						if (_cfg.groupFavourites) {
							entries.push_back({ TRANSLATE_S("$FUCK_Sidebar_FavouritesGroup"), "", true, nullptr, &favTools, -1000000 });
						} else {
							for (auto* t : favTools) {
								entries.push_back({ GetOverrides(t).customName.empty() ? t->Name() : GetOverrides(t).customName, "", false, t, nullptr, -1000000 });
							}
						}
					}

					for (auto* t : looseTools) {
						entries.push_back({ GetOverrides(t).customName.empty() ? t->Name() : GetOverrides(t).customName, "", false, t, nullptr, GetOverrides(t).sortOrder });
					}

					for (auto& [name, tools] : toolGroups) {
						std::sort(tools.begin(), tools.end(), sortTools);
						auto&       gOver      = _groupOverrides[name];
						std::string dispName   = gOver.customName.empty() ? name : gOver.customName;
						int         groupOrder = (_cfg.showSidebarFavourites && gOver.isFavourited) ? -999999 : gOver.sortOrder;
						entries.push_back({ dispName, name, true, nullptr, &tools, groupOrder });
					}

					std::sort(entries.begin(), entries.end(), [](const SidebarEntry& a, const SidebarEntry& b) {
						if (a.sortOrder != b.sortOrder)
							return a.sortOrder < b.sortOrder;
						return _stricmp(a.label.c_str(), b.label.c_str()) < 0;
					});

					auto  apRight           = chromeArrow(false, m.sidebarItemH);
					float alignedTextOffset = (m.sidebarIndent * 0.5f) + apRight.drawSize.x + (10.0f * m.uiScale);

					auto RenderSidebarItem = [&](FUCK::ITool* tool, const char* label, float extraIndent = 0.0f) {
						std::string idLabel = std::format("##{}", label);
						FUCK::PushID(idLabel.c_str());

						bool  isSelected = (_activeTool == tool);
						auto& over       = GetOverrides(tool);

						// Cache both Screen and Window positions before the Selectable moves the cursor!
						const auto startScreenPos = FUCK::GetCursorScreenPos();
						const auto startPos       = FUCK::GetCursorPos();

						// Draw Selectable (Full width background & highlight)
						if (FUCK::Selectable(idLabel.c_str(), isSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, m.sidebarItemH))) {
							if (_activeTool && _activeTool != tool) {
								FUCK::AbortBinding();
								_activeTool->OnClose();
							}
							RE::PlaySound("UIMenuOK");
							_activeTool = tool;
							_activeTool->OnOpen();
						}

						bool   itemHovered = FUCK::IsItemHovered(0);
						ImVec2 endPos      = FUCK::GetCursorPos();  // Save position after selectable

						// Bypassing FUCK::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);
						float textY = startPos.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset;

						FUCK::SetCursorPos({ startPos.x + alignedTextOffset + extraIndent, textY });
						FUCK::Text(label);

						// Draw Star
						if (_cfg.showSidebarFavourites && (over.isFavourited || itemHovered)) {
							const char* starIcon     = ICON_FA_STAR;
							float       starScale    = 0.85f;
							float       starFontSize = m.sidebarFontSize * starScale;

							ImGui::PushFont(regularFont, starFontSize);
							ImVec2 starSize = ImGui::CalcTextSize(starIcon);
							ImGui::PopFont();

							float starX = startScreenPos.x + sidebarAvailW - starSize.x - (15.0f * m.uiScale);
							float starY = startScreenPos.y + (m.sidebarItemH - starFontSize) * 0.5f - (1.0f * m.uiScale);

							// Hit-testing logic bypassing ImGui Item overlap clashing
							ImRect starBB(
								ImVec2(starX - (5.0f * m.uiScale), startScreenPos.y),
								ImVec2(starX + starSize.x + (5.0f * m.uiScale), startScreenPos.y + m.sidebarItemH));
							bool starHovered = ImGui::IsMouseHoveringRect(starBB.Min, starBB.Max);

							if (starHovered && FUCK::IsMouseClicked(0)) {
								over.isFavourited = !over.isFavourited;
								SaveWorkspace();
							}

							ImU32 starCol = starHovered ? IM_COL32(255, 255, 100, 255) : (over.isFavourited ? IM_COL32(255, 215, 0, 255) : IM_COL32(150, 150, 150, 150));

							ImGui::GetWindowDrawList()->AddText(regularFont, starFontSize,
								ImVec2(starX, starY),
								starCol, starIcon);
						}

						ImGui::PopFont();
						FUCK::SetCursorPos(endPos);  // Restore cursor to next line
						FUCK::PopID();
					};

					auto RenderSidebarGroup = [&](const std::string& groupName, const std::string& origGroup, std::vector<FUCK::ITool*>& tools) {
						// Bypassing FUCK::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);

						// Custom TreeNode rendering
						ImGui::PushID(groupName.c_str());
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						ImGuiID      id     = window->GetID(groupName.c_str());
						bool         isOpen = window->DC.StateStorage->GetInt(id, 0);

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
								ImU32 col   = ImGui::GetDynamicTextColor(hovered);
								bool  pDown = isOpen;
								auto  ap    = chromeArrow(pDown, m.sidebarItemH);

								ImVec2 drawPos = {
									pos.x + (m.sidebarIndent * 0.5f),
									pos.y + ap.offsetY + textVisualOffset
								};
								ImGui::DrawArrowIcon(window->DrawList, drawPos, ap.drawSize, col,
									pDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
							}

							float textY = bb.Min.y + (m.sidebarItemH - ImGui::CalcTextSize(groupName.c_str()).y) * 0.5f + textVisualOffset;
							ImGui::RenderText({ pos.x + alignedTextOffset, textY }, groupName.c_str());

							// Draw Star
							if (!origGroup.empty()) {
								auto& gOver = _groupOverrides[origGroup];
								if (_cfg.showSidebarFavourites && (gOver.isFavourited || hovered)) {
									const char* starIcon     = ICON_FA_STAR;
									float       starScale    = 0.85f;
									float       starFontSize = m.sidebarFontSize * starScale;

									ImGui::PushFont(regularFont, starFontSize);
									ImVec2 starSize = ImGui::CalcTextSize(starIcon);
									ImGui::PopFont();

									float starX = bb.Min.x + sidebarAvailW - starSize.x - (15.0f * m.uiScale);
									float starY = bb.Min.y + (m.sidebarItemH - starFontSize) * 0.5f - (1.0f * m.uiScale);

									// Hit-testing logic bypassing ImGui Item overlap clashing
									ImRect starBB(
										ImVec2(starX - (5.0f * m.uiScale), bb.Min.y),
										ImVec2(starX + starSize.x + (5.0f * m.uiScale), bb.Min.y + m.sidebarItemH));
									bool starHovered = ImGui::IsMouseHoveringRect(starBB.Min, starBB.Max);

									if (starHovered && FUCK::IsMouseClicked(0)) {
										gOver.isFavourited = !gOver.isFavourited;
										SaveWorkspace();
									}

									ImU32 starCol = starHovered ? IM_COL32(255, 255, 100, 255) : (gOver.isFavourited ? IM_COL32(255, 215, 0, 255) : IM_COL32(150, 150, 150, 150));

									ImGui::GetWindowDrawList()->AddText(regularFont, starFontSize,
										ImVec2(starX, starY),
										starCol, starIcon);
								}
							}
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
							RenderSidebarGroup(entry.label, entry.origGroup, *entry.tools);
						} else {
							RenderSidebarItem(entry.tool, entry.label.c_str());
						}
					}

					// --- FOOTER: SETTINGS (Centred) ---
					float childHeight     = FUCK::GetWindowSize().y;
					float separatorHeight = 1.0f;
					float settingsY       = childHeight - m.sidebarItemH - bottomPadding;

					float minSettingY = FUCK::GetCursorPos().y + separatorHeight;
					if (settingsY < minSettingY)
						settingsY = minSettingY;

					FUCK::SetCursorPosY(settingsY - separatorHeight);
					FUCK::SeparatorThick();

					FUCK::SetCursorPosY(settingsY);
					{
						auto*      settingsTool = &_settingsTool;
						bool       isSelected   = (_activeTool == settingsTool);
						const auto cursorPos    = FUCK::GetCursorPos();

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
						float       setW        = ImGui::CalcTextSize(settingText).x;

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
