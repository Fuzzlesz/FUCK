#pragma once

class ThemeEditorWindow : public FUCK::IWindow
{
public:
	const char* PluginName() const override { return "FUCK"; }
	const char* Title() const override { return "$FUCK_ThemeEditor_Title"_T; }
	void        Draw() override;
	bool        IsOpen() const override { return _isOpen; }
	void        SetOpen(bool a_open) override;

	FUCK::WindowFlags GetFlags() const override { return FUCK::WindowFlags::kExtendBorder | FUCK::WindowFlags::kCloseOnEsc; }

	ImVec2 GetDefaultSize() const override
	{
		return FUCK::Scale(450.0f, 600.0f);
	}
	ImVec2 GetDefaultPos() const override
	{
		return FUCK::Scale(1050.0f, 450.0f);
	}

	bool GetRequestedPos(ImVec2& outPos) override
	{
		if (_lastPos.x != -1.0f && _lastPos.y != -1.0f) {
			outPos = _lastPos;
			return true;
		}
		return false;
	}

	void UpdateState(const ImVec2& currentPos, const ImVec2& currentSize) override
	{
		_lastPos = currentPos;
		_lastSize = currentSize;
	}

	bool _isOpen = false;
	ImVec2 _lastPos{ -1.0f, -1.0f };
	ImVec2 _lastSize{ -1.0f, -1.0f };
};
