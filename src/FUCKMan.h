#pragma once

#include "FUCKSettings.h"
#include "FUCKStyles.h"

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

	struct FUCKConfig
	{
		ImVec2      windowPos{ 132.0f, 74.0f };
		ImVec2      windowSize{ 1020.0f, 800.0f };
		PauseType   globalPauseType{ PauseType::kNone };
		float       userScale{ 1.0f };
		bool        sidebarOnRight{ false };
		bool        injectSystemMenu{ true };
		bool        replaceHelpMenu{ false };
		bool        showSidebarFilter{ true };
		bool        showSidebarFavourites{ true };
		bool        groupFavourites{ true };
		std::string currentFont{ "Default" };
	};

	struct ToolOverrideState
	{
		bool        isFavourited = false;
		bool        isHidden     = false;
		std::string customName   = "";
		std::string customGroup  = "";
		int         sortOrder    = 0;
	};

	struct GroupOverrideState
	{
		bool        isFavourited = false;
		std::string customName   = "";
		int         sortOrder    = 0;
	};

	friend class SettingsTool;
	friend class ThemeEditorWindow;

	FUCKMan();

	// --- Tool & Window Registration ---
	void RegisterTool(FUCK::ITool* tool);
	void RegisterWindow(FUCK::IWindow* window);
	void UnregisterWindow(FUCK::IWindow* window);

	// --- Rendering & Input ---
	bool ShouldRender() const;
	void Draw();
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
	void SaveWorkspace();

	void Save();
	void SaveKeybinds();

	float GetActiveScale() const { return _activeScale; }
	float GetUserScale() const { return _cfg.userScale; }
	bool  GetInjectSystemMenu() const { return _cfg.injectSystemMenu; }
	bool  GetReplaceHelpMenu() const { return _cfg.replaceHelpMenu; }
	void  SetCurrentFont(const std::string& a_font);

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

protected:
	EventResult ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

private:
	void UpdateGameState();

	struct MenuListenerEntry
	{
		void* userdata;
		void (*callback)(const char*, bool, void*);
	};

	std::vector<MenuListenerEntry> _menuListeners;
	std::vector<FUCK::ITool*>      _tools;
	std::vector<FUCK::IWindow*>    _windows;
	std::vector<FUCK::IWindow*>    _suspendedWindows;
	std::set<std::string>          _pausedMenus;

	StringMap<ToolOverrideState>  _toolOverrides;
	StringMap<GroupOverrideState> _groupOverrides;

	FUCK::ITool* _activeTool      = nullptr;
	bool         _isOpen          = false;
	bool         _workspaceLoaded = false;

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
	ImVec2 _lastSavedPos{ 100.0f, 100.0f };
	ImVec2 _lastSavedSize{ 1020.0f, 800.0f };
	bool   _isCollapsed  = false;
	bool   _wasCollapsed = false;

	// Built-in Tools
	SettingsTool      _settingsTool;
	ThemeEditorWindow _themeEditorWindow;
};
