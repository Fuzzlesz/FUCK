#pragma once

class ThemeEditorWindow : public FUCK::IWindow
{
public:
	const char* Id() const override { return "ThemeEditor"; }
	const char* PluginName() const override { return "FUCK"; }
	const char* Title() const override { return "$FUCK_ThemeEditor_Title"_T; }
	void        Draw() override;
	bool        IsOpen() const override { return _isOpen; }
	void        SetOpen(bool a_open) override;

	FUCK::WindowFlags GetFlags() const override { return FUCK::WindowFlags::kExtendBorder | FUCK::WindowFlags::kCloseOnEsc; }

	ImVec2 GetDefaultSize() const override
	{
		return FUCK::UIScale(450.0f, 600.0f);
	}
	ImVec2 GetDefaultPos() const override
	{
		return FUCK::Scale(1050.0f, 450.0f);
	}

	bool _isOpen = false;
};
