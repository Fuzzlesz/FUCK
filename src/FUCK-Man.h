#pragma once

#include "FUCK-Settings.h"
#include "FUCK-Sidebar.h"
#include "FUCK-Styles.h"

struct WindowState;

class FUCKMan :
	public REX::Singleton<FUCKMan>,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
	enum class PauseType : int
	{
		kNone = 0,
		kSoft = 1,
		kHard = 2
	};

	enum class JournalMenuType : int
	{
		kUnknown = 0,
		kSkyUIv5 = 1, // covers SkyUI v5 and Vanilla
		kSafe    = 2  // covers SkyUI v6
	};

	enum class InjectSystemMenuType : int
	{
		kYes            = 0,
		kControllerOnly = 1,
		kNo             = 2
	};

	struct FUCKConfig
	{
		ImVec2               windowPos{ 130.0f, 75.0f };
		ImVec2               windowSize{ 1120.0f, 800.0f };
		PauseType            globalPauseType{ PauseType::kNone };
		float                userScale{ 1.0f };
		bool                 sidebarOnRight{ false };
		InjectSystemMenuType injectSystemMenu{ InjectSystemMenuType::kControllerOnly };
		bool                 replaceHelpMenu{ false };
		bool                 injectSettingsSubmenu{ false };
		bool                 showSidebarFilter{ true };
		bool                 showSidebarFavourites{ true };
		bool                 groupFavourites{ true };
		bool                 muteAudio{ false };
		std::string          currentFont{ "Default" };
		std::string          customSystemMenuName{ "$FUCK_Title" };
	};

	struct ToolOverrideState
	{
		bool        isFavourited = false;
		bool        isHidden     = false;
		std::string customName   = "";
		std::string customGroup  = "";
		int         sortOrder    = 0;
	};

	// Keyed by the group's full path, so each nested level has its own
	struct GroupOverrideState
	{
		bool        isFavourited = false;
		std::string customName   = "";
		int         sortOrder    = 0;
	};

	// --- Sidebar (FUCK-Sidebar.cpp) ---
	std::string GetToolGroup(FUCK::ITool* a_tool);              // with the user's override, normalised
	std::string GetGroupDisplayName(const std::string& a_path);  // the user's name for it, or its last level

	const Sidebar::ToolLayout& GetToolLayout();  // rebuilds first, if anything changed
	void                       MarkLayoutDirty() { _layoutDirty = true; }
	void                       OnOverridesChanged() { MarkLayoutDirty(); SaveWorkspace(); }  // after any change to a tool's or group's overrides
	const std::string&         GetToolKey(FUCK::ITool* a_tool);
	const std::string&         GetToolLabel(FUCK::ITool* a_tool);
	int                        GetEntryOrder(const Sidebar::Entry& a_entry);  // its own order, or its baseline

	// Moves a tool into a group. a_index places it among everything at that level, groups and tools together,
	// counted without the tool itself (past the end = last). Only the orders that need to change are written
	void MoveTool(FUCK::ITool* a_tool, const std::string& a_group, std::size_t a_index);
	// Moves a group among everything at its level, the same way
	void MoveGroup(const std::string& a_path, std::size_t a_index);
	// Moves a group, and everything in it, into another group ("" for the top level), at a_index among what's there.
	// Its tools are given the new path, and its names, favourites and orders go with it. Returns false if it can't go there
	bool MoveGroupInto(const std::string& a_path, const std::string& a_parent, std::size_t a_index);
	// Whether a group can be moved into another: not into itself or anything in it, not where it already is,
	// and not so deep that something in it would pass the depth limit
	bool CanMoveGroupInto(const std::string& a_path, const std::string& a_parent);
	// Sets the group a tool goes in, stored relative to its own group (after any group moves). It takes its default place there.
	// Doesn't renumber the groups it leaves and joins, so for a move the user makes, use MoveTool, which does
	void SetToolGroup(FUCK::ITool* a_tool, const std::string& a_group);

	friend class SettingsTool;
	friend class ThemeEditorWindow;

	FUCKMan();

	// --- Tool & Window Registration ---
	void RegisterTool(FUCK::ITool* tool);
	void RegisterWindow(FUCK::IWindow* window);
	void UnregisterWindow(FUCK::IWindow* window);
	void RefreshTools();  // a plugin's tools changed their names, groups or visibility. Safe from any thread

	// --- Rendering & Input ---
	bool ShouldRender() const;
	// True when something needs the VR helper's exclusive-focus panel: the
	// main menu, or any open window that isn't kPassInputToGame.
	bool ShouldRenderExclusive() const;
	// True when any kPassInputToGame window is open — these render on their
	// own always-on VR HUD panel, independent of whether the main menu or
	// other windows are also open (see ShouldRenderExclusive).
	bool ShouldRenderPassthrough() const;
	// a_passthroughOnly draws just the open kPassInputToGame windows (used for
	// the VR HUD panel); the main menu, tool overlays, and other windows are
	// skipped since they belong on the exclusive-focus panel instead.
	void Draw(bool a_passthroughOnly = false);
	bool ProcessAsyncInput(const RE::InputEvent* const* a_event);

	// --- State Queries ---
	bool IsInputBlocked() const;
	bool IsCursorForced() const;
	bool IsIgnoringUserScale() const { return _isIgnoringUserScale; }
	bool HasWindowWithFlag(FUCK::WindowFlags a_flag) const;
	bool IsOpen() const { return _isOpen; }

	// --- Menu Controls ---
	void Open();
	void Close();
	void Toggle();

	// --- Settings & State ---
	void LoadSettings(const CSimpleIniA& a_ini);
	void SaveSettings(CSimpleIniA& a_ini);
	void ResetSettings();

	void LoadWorkspace();
	// Asks for the workspace to be written. The write waits until nothing has asked for half a second, or the menu
	// closes, so a burst of changes is one write. Files whose contents haven't changed aren't written at all
	void SaveWorkspace();
	void FlushWorkspace(bool a_force = false);

	void Save();
	void SaveKeybinds();

	bool  GetMuteAudio() const { return _cfg.muteAudio; }
	float GetActiveScale() const { return _activeScale; }

	// kEditable windows: scale and rotation of the window being drawn
	float GetCurrentWindowScale() const;
	float GetCurrentWindowRotation() const;
	float GetUserScale() const { return _cfg.userScale; }
	void  SetCurrentFont(const std::string& a_font);

	InjectSystemMenuType GetInjectSystemMenu() const { return _cfg.injectSystemMenu; }
	bool                 GetReplaceHelpMenu() const { return _cfg.replaceHelpMenu; }
	bool                 GetInjectSettingsSubmenu() const { return _cfg.injectSettingsSubmenu; }
	const char*          GetSystemMenuName() const { return _cfg.customSystemMenuName.c_str(); }
	JournalMenuType      GetJournalMenuType() const { return _journalMenuType; }
	void                 SetJournalMenuType(JournalMenuType type) { _journalMenuType = type; }

	// --- API Overrides ---
	void SetVanityBlocked(bool blocked);
	void SuspendRendering(bool suspend);
	void SetManualHardPause(bool paused);
	void SetManualSoftPause(bool paused);
	void SetForceCursor(bool force);

	// --- Menu Event Listeners ---
	void AddMenuListener(void* userdata, void (*callback)(const char*, bool, void*))
	{
		_menuListeners.push_back({ userdata, callback });
	}

	void RemoveMenuListener(void* userdata)
	{
		std::erase_if(_menuListeners, [userdata](const MenuListenerEntry& e) {
			return e.userdata == userdata;
		});
	}

	void DispatchMenuEvent(const char* menuName, bool opening)
	{
		// Copy so listeners can safely remove themselves during the callback
		auto listenersCopy = _menuListeners;
		for (auto& e : listenersCopy) {
			e.callback(menuName, opening, e.userdata);
		}
	}

	// --- Window Event Listeners ---
	struct WindowListenerEntry
	{
		void* userdata;
		void (*callback)(const char*, const char*, bool, void*);
	};
	std::vector<WindowListenerEntry> _windowListeners;

	void AddWindowListener(void* userdata, void (*callback)(const char*, const char*, bool, void*))
	{
		_windowListeners.push_back({ userdata, callback });
	}

	void RemoveWindowListener(void* userdata)
	{
		std::erase_if(_windowListeners, [userdata](const WindowListenerEntry& e) {
			return e.userdata == userdata;
		});
	}

	void DispatchWindowEvent(const char* pluginName, const char* windowId, bool opening)
	{
		auto listenersCopy = _windowListeners;
		for (auto& e : listenersCopy) {
			e.callback(pluginName, windowId, opening, e.userdata);
		}
	}

	bool IsPluginWindowOpen(const char* pluginName, const char* windowId) const
	{
		for (auto* win : _windows) {
			if (strcmp(win->PluginName(), pluginName) == 0 && strcmp(win->Id(), windowId) == 0) {
				return win->IsOpen();
			}
		}
		return false;
	}

	const char* GetCurrentRenderingPlugin() const { return _currentRenderingPlugin.c_str(); }

protected:
	EventResult ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

private:
	// Sizes for the chrome and content, worked out at the top of each Draw()
	struct LayoutMetrics
	{
		float  uiScale;
		float  textH;
		ImVec2 pad;  // the theme's window padding, at resolution scale

		float iconAspect;
		float chromeIconBaseH;  // = 20.0f * uiScale  (userScale intentionally excluded for chrome)
		float headerPadding;

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
	};

	// --- Drawing (FUCK-Man.cpp) ---
	LayoutMetrics CalcLayoutMetrics() const;
	void          PushContentScale(const LayoutMetrics& a_m, bool a_ignoreUserScale = false);
	void          PopContentScale();
	void          DrawOverlays(const LayoutMetrics& a_m, bool a_menusHidden);
	void          DrawMainWindow(const LayoutMetrics& a_m);

	// --- Plugin windows (FUCK-Windows.cpp) ---
	void DrawPluginWindows(const LayoutMetrics& a_m, bool a_passthroughOnly, bool a_globalMouseClick);
	void UpdateEditableWindow(WindowState& a_state, const ImVec2& a_pos, const ImVec2& a_size, const ImVec2& a_margin, bool a_scalable, bool a_rotatable);
	void ResetEditableLayout(WindowState& a_state);
	bool DrawResetIcon(float a_rowHeight);

	struct PendingCommand
	{
		enum class Type
		{
			kAddTool,
			kAddWindow,
			kRemoveWindow,
			kRefreshTools
		} type;
		FUCK::ITool*   tool   = nullptr;
		FUCK::IWindow* window = nullptr;
	};

	std::mutex                  _pendingLock;
	std::vector<PendingCommand> _pendingCommands;

	void FlushPendingRegistrations();
	void WriteWorkspace();

	// --- Sidebar (FUCK-Sidebar.cpp) ---
	void                    DrawSidebar(const SidebarMetrics& a_metrics, float a_height);
	void                    RebuildToolLayout();
	Sidebar::LevelBaselines ComputeLevelBaselines(const std::string& a_level, const std::vector<Sidebar::Entry>& a_entries);
	void                    RenumberLevel(const std::vector<Sidebar::Entry>& a_entries, const Sidebar::LevelBaselines& a_baselines);
	void                    SetEntryOrder(const Sidebar::Entry& a_entry, int a_order);
	void                    PlaceInLevel(const std::string& a_level, const Sidebar::Entry& a_entry, std::size_t a_index);
	void                    KeepShownOrder(const StringMap<std::vector<Sidebar::Entry>>& a_shown);
	std::string             GetNativeGroup(FUCK::ITool* a_tool);  // the tool's own group, after the user's group moves
	std::string             ApplyGroupMoves(const std::string& a_path) const;
	void UpdateGameState();
	bool IsWindowSuppressed(const FUCK::IWindow* win) const;

	struct MenuListenerEntry
	{
		void* userdata;
		void (*callback)(const char*, bool, void*);
	};

	std::vector<MenuListenerEntry> _menuListeners;
	std::vector<FUCK::ITool*>      _tools;
	std::vector<FUCK::IWindow*>    _windows;
	std::set<std::string>          _pausedMenus;

	StringMap<ToolOverrideState>  _toolOverrides;
	StringMap<GroupOverrideState> _groupOverrides;

	FUCK::ITool* _activeTool      = nullptr;
	bool         _isOpen          = false;
	bool         _workspaceLoaded = false;

	Sidebar::ToolLayout _layout;
	bool                _layoutDirty = true;

	// Groups moved into other groups (old path, new path), for the sidebar to carry their open state across
	std::vector<std::pair<std::string, std::string>> _movedGroups;

	// Saved: groups the user has moved, from the path the plugins give them to where they are now. A plugin's tools,
	// including ones it adds after the move, follow their group there
	StringMap<std::string> _groupMoves;

	bool                                  _workspaceDirty = false;
	std::chrono::steady_clock::time_point _workspaceDirtyAt{};

	JournalMenuType _journalMenuType{ JournalMenuType::kUnknown };

	// Game State
	bool _isGameHardPaused = false;
	bool _isGameSoftPaused = false;
	bool _isGameBlurred    = false;
	bool _isHudHidden      = false;
	bool _isVanityBlocked  = false;

	// API Driven Overrides
	bool _apiHardPause     = false;
	bool _apiSoftPause     = false;
	bool _forceCursor      = false;
	bool _suspendRendering = false;

	// Config / Settings State
	FUCKConfig _cfg;
	FUCKConfig _def;  // defaults

	float _activeScale         = 1.0f;
	bool  _isIgnoringUserScale = false;

	// Window Metrics
	bool   _pendingWindowRestore = false;
	ImVec2 _lastSavedPos{ 0.0f, 0.0f };
	ImVec2 _lastSavedSize{ 0.0f, 0.0f };
	bool   _isCollapsed  = false;
	bool   _wasCollapsed = false;

	// Built-in Tools
	SettingsTool      _settingsTool;
	ThemeEditorWindow _themeEditorWindow;

	std::string _currentRenderingPlugin = "";
};
