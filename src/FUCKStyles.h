#pragma once

class ThemeEditorWindow : public IWindow
{
public:
	const char* Title() const override { return "$FUCK_ThemeEditor_Title"_T; }
	void Draw() override;
	bool IsOpen() const override { return _isOpen; }
	void SetOpen(bool a_open) override { _isOpen = a_open; }

	WindowFlags GetFlags() const override { return WindowFlags::kExtendBorder | WindowFlags::kCloseOnEsc; }

	ImVec2 GetDefaultSize() const override
	{
		float s = FUCK::GetResolutionScale();
		return { 450.0f * s, 600.0f * s };
	}
	ImVec2 GetDefaultPos() const override
	{
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
