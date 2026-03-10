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

	void RegisterTool(ITool* tool);
	void RegisterWindow(IWindow* window);

	bool ShouldRender() const;
	void Draw();

	bool IsInputBlocked() const;
	bool IsCursorForced() const;

	bool ProcessAsyncInput(const RE::InputEvent* const* a_event);

	void Open();
	void Close();
	void Toggle();
	bool IsOpen() const { return _isOpen; }

	void LoadSettings(const CSimpleIniA& a_ini);
	void SaveSettings(CSimpleIniA& a_ini);
	void ResetSettings();
	float GetUserScale() const { return _userScale; }

	bool GetInjectSystemMenu() const { return _injectSystemMenu; }
	bool GetReplaceHelpMenu() const { return _replaceHelpMenu; }

	void SetVanityBlocked(bool blocked);
	void SuspendRendering(bool suspend);

	void SetManualHardPause(bool paused);
	void SetManualSoftPause(bool paused);
	void SetForceCursor(bool force);

protected:
	RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override;

private:
	void UpdateGameState();

	std::vector<ITool*> _tools;
	std::vector<IWindow*> _windows;

	ITool* _activeTool = nullptr;
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

	PauseType _globalPauseType = PauseType::kNone;
	float _userScale = 1.0f;
	bool _sidebarOnRight = false;
	bool _injectSystemMenu = true;
	bool _replaceHelpMenu = false;

	ImVec2 _windowPos{ 100.0f, 100.0f };
	ImVec2 _windowSize{ 1000.0f, 600.0f };
	bool _settingsLoaded = false;
	ImVec2 _lastSavedPos{ 100.0f, 100.0f };
	ImVec2 _lastSavedSize{ 1000.0f, 600.0f };

	bool _isCollapsed = false;

	SettingsTool _settingsTool;
	ThemeEditorWindow _themeEditorWindow;
};
