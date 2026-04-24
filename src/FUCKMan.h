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

	float GetUserScale() const { return _userScale; }
	bool GetInjectSystemMenu() const { return _injectSystemMenu; }
	bool GetReplaceHelpMenu() const { return _replaceHelpMenu; }

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
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

private:
	void UpdateGameState();

	struct MenuListenerEntry
	{
		void* userdata;
		void (*callback)(const char*, bool, void*);
	};

	std::vector<MenuListenerEntry> _menuListeners;
	std::vector<FUCK::ITool*> _tools;
	std::vector<FUCK::IWindow*> _windows;

	FUCK::ITool* _activeTool = nullptr;
	bool _isOpen = false;

	// Game State
	bool _isGameHardPaused = false;
	bool _isGameSoftPaused = false;
	bool _isGameBlurred = false;
	bool _isHudHidden = false;
	bool _isVanityBlocked = false;

	// API Driven Overrides
	bool _apiHardPause = false;
	bool _apiSoftPause = false;
	bool _forceCursor = false;
	bool _suspendRendering = false;

	// Config / Settings
	PauseType _globalPauseType = PauseType::kNone;
	float _userScale = 1.0f;
	bool _sidebarOnRight = false;
	bool _injectSystemMenu = true;
	bool _replaceHelpMenu = false;

	// Window Metrics
	ImVec2 _windowPos{ 100.0f, 100.0f };
	ImVec2 _windowSize{ 1000.0f, 600.0f };
	bool _pendingWindowRestore = false;
	ImVec2 _lastSavedPos{ 100.0f, 100.0f };
	ImVec2 _lastSavedSize{ 1000.0f, 600.0f };
	bool _isCollapsed = false;
	bool _wasCollapsed = false;

	// Built-in Tools
	SettingsTool _settingsTool;
	ThemeEditorWindow _themeEditorWindow;
};
