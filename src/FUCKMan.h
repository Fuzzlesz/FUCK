#pragma once

#include "FUCK_API.h"
#include <imgui.h>

class FUCKMan :
	public REX::Singleton<FUCKMan>,
	public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
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

	enum class PauseType : int
	{
		kNone = 0,
		kSoft = 1,
		kHard = 2
	};

	PauseType _globalPauseType = PauseType::kNone;
	float _userScale = 1.0f;
	bool _sidebarOnRight = false;

	ImVec2 _windowPos{ 100.0f, 100.0f };
	ImVec2 _windowSize{ 1000.0f, 600.0f };
	bool _settingsLoaded = false;
	ImVec2 _lastSavedPos{ 100.0f, 100.0f };
	ImVec2 _lastSavedSize{ 1000.0f, 600.0f };

	bool _isCollapsed = false;

	class SettingsTool : public ITool
	{
	public:
		const char* Name() const override;
		void Draw() override;
		bool OnAsyncInput(const void* a_event) override;
		void OnClose() override;
	};

	class ThemeEditorWindow : public IWindow
	{
	public:
		const char*	Title() const override { return "$FUCK_ThemeEditor_Title"_T; }
		void		Draw() override;
		bool		IsOpen() const override { return _isOpen; }
		void		SetOpen(bool a_open) override { _isOpen = a_open; }

		ImVec2 GetDefaultSize() const override { 
			float s = FUCK::GetResolutionScale();
			return { 450.0f * s, 600.0f * s }; 
		}
		ImVec2 GetDefaultPos() const override { 
			float s = FUCK::GetResolutionScale();
			return { 1050.0f * s, 450.0f * s }; 
		}

		void UpdateState(const ImVec2& currentPos, const ImVec2& currentSize) override
		{
			_lastPos = currentPos;
			_lastSize = currentSize;
		}

		bool _isOpen = false;
		ImVec2 _lastPos{ 1050.0f, 450.0f };
		ImVec2 _lastSize{ 450.0f, 600.0f };
	};

	SettingsTool _settingsTool;
	ThemeEditorWindow _themeEditorWindow;
};
