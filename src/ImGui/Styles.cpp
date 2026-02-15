#include "Styles.h"
#include "FUCKMan.h"
#include "IconsFonts.h"
#include "Renderer.h"
#include "System/Settings.h"

namespace ImGui
{
	void Styles::ConvertVec4StylesToU32()
	{
		frameBG_WidgetU32 = ColorConvertFloat4ToU32(user.frameBG_Widget);
		frameBG_WidgetActiveU32 = ColorConvertFloat4ToU32(user.frameBG_WidgetActive);
		gridLinesU32 = ColorConvertFloat4ToU32(user.gridLines);
		sliderBorderU32 = ColorConvertFloat4ToU32(user.sliderBorder);
		sliderBorderActiveU32 = ColorConvertFloat4ToU32(user.sliderBorderActive);
		iconDisabledU32 = ColorConvertFloat4ToU32(user.iconDisabled);
		textHeaderU32 = ColorConvertFloat4ToU32(user.textHeader);
		widgetToggleActiveU32 = ColorConvertFloat4ToU32(user.widgetToggleActive);
		widgetFlashU32 = ColorConvertFloat4ToU32(user.widgetFlash);

		tabBorderU32 = ColorConvertFloat4ToU32(user.tabBorder);
		tabBorderActiveU32 = ColorConvertFloat4ToU32(user.tabBorderActive);
		toggleRailU32 = ColorConvertFloat4ToU32(user.toggleRail);
		toggleRailFilledU32 = ColorConvertFloat4ToU32(user.toggleRailFilled);
		toggleKnobU32 = ColorConvertFloat4ToU32(user.toggleKnob);

		scrollbarBGU32 = ColorConvertFloat4ToU32(user.scrollbarBG);
		navHighlightU32 = ColorConvertFloat4ToU32(user.navHighlight);
	}

	ImU32 Styles::GetColorU32(USER_STYLE a_style) const
	{
		switch (a_style) {
		case USER_STYLE::kIconDisabled:
			return iconDisabledU32;
		case USER_STYLE::kGridLines:
			return gridLinesU32;
		case USER_STYLE::kSeparator:
			return ColorConvertFloat4ToU32(user.separator);
		case USER_STYLE::kSliderBorder:
			return sliderBorderU32;
		case USER_STYLE::kSliderBorderActive:
			return sliderBorderActiveU32;
		case USER_STYLE::kFrameBG_Widget:
			return frameBG_WidgetU32;
		case USER_STYLE::kFrameBG_WidgetActive:
			return frameBG_WidgetActiveU32;
		case USER_STYLE::kHeaderText:
			return textHeaderU32;
		case USER_STYLE::kTextHovered:
			return ColorConvertFloat4ToU32(user.textHovered);
		case USER_STYLE::kWidgetToggleActive:
			return widgetToggleActiveU32;
		case USER_STYLE::kWidgetFlash:
			return widgetFlashU32;
		case USER_STYLE::kComboBoxText:
			return ColorConvertFloat4ToU32(user.comboBoxText);
		case USER_STYLE::kComboBoxTextBox:
			return ColorConvertFloat4ToU32(user.comboBoxTextBox);

		case USER_STYLE::kTabBorder:
			return tabBorderU32;
		case USER_STYLE::kTabBorderActive:
			return tabBorderActiveU32;
		case USER_STYLE::kToggleRail:
			return toggleRailU32;
		case USER_STYLE::kToggleRailFilled:
			return toggleRailFilledU32;
		case USER_STYLE::kToggleKnob:
			return toggleKnobU32;

		case USER_STYLE::kScrollbarBG:
			return scrollbarBGU32;
		case USER_STYLE::kNavHighlight:
			return navHighlightU32;

		default:
			return ImU32();
		}
	}

	ImVec4 Styles::GetColorVec4(USER_STYLE a_style) const
	{
		switch (a_style) {
		case USER_STYLE::kTextButton:
			return user.textButton;
		case USER_STYLE::kHeaderText:
			return user.textHeader;
		case USER_STYLE::kIconDisabled:
			return user.iconDisabled;
		case USER_STYLE::kComboBoxTextBox:
			return user.comboBoxTextBox;
		case USER_STYLE::kComboBoxText:
			return user.comboBoxText;
		case USER_STYLE::kFrameBG_Widget:
			return user.frameBG_Widget;
		case USER_STYLE::kFrameBG_WidgetActive:
			return user.frameBG_WidgetActive;
		case USER_STYLE::kButtons:
			return user.button;
		case USER_STYLE::kTextHovered:
			return user.textHovered;
		case USER_STYLE::kWidgetToggleActive:
			return user.widgetToggleActive;
		case USER_STYLE::kWidgetFlash:
			return user.widgetFlash;

		case USER_STYLE::kTabBorder:
			return user.tabBorder;
		case USER_STYLE::kTabBorderActive:
			return user.tabBorderActive;
		case USER_STYLE::kToggleRail:
			return user.toggleRail;
		case USER_STYLE::kToggleRailFilled:
			return user.toggleRailFilled;
		case USER_STYLE::kToggleKnob:
			return user.toggleKnob;

		case USER_STYLE::kScrollbarBG:
			return user.scrollbarBG;
		case USER_STYLE::kNavHighlight:
			return user.navHighlight;

		default:
			return ImVec4();
		}
	}

	float Styles::GetVar(USER_STYLE a_style) const
	{
		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();

		switch (a_style) {
		case USER_STYLE::kButtons:
			return user.buttonScale;
		case USER_STYLE::kButtonRounding:
			return user.buttonRounding;
		case USER_STYLE::kCheckbox:
			return user.checkboxScale;
		case USER_STYLE::kStepper:
			return user.stepperScale;
		case USER_STYLE::kSeparatorThickness:
			return user.separatorThickness;
		case USER_STYLE::kGridLines:
			return user.gridThickness * scale;
		case USER_STYLE::kDisabledTextAlpha:
			return user.textDisabled.w;
		default:
			return 1.0f;
		}
	}

	// ==========================================
	// FONTS
	// ==========================================

	std::vector<std::string> Styles::GetAvailableFonts() const
	{
		auto settings = Settings::GetSingleton();
		auto userFonts = Settings::GetConfigs(settings->GetUserFontsPath(), ".ttf");
		auto gameFonts = Settings::GetConfigs(settings->GetLegacyFontsPath(), ".ttf");
		auto userOtf = Settings::GetConfigs(settings->GetUserFontsPath(), ".otf");
		auto gameOtf = Settings::GetConfigs(settings->GetLegacyFontsPath(), ".otf");

		userFonts.insert(userFonts.end(), gameFonts.begin(), gameFonts.end());
		userFonts.insert(userFonts.end(), userOtf.begin(), userOtf.end());
		userFonts.insert(userFonts.end(), gameOtf.begin(), gameOtf.end());

		std::sort(userFonts.begin(), userFonts.end());
		userFonts.erase(std::unique(userFonts.begin(), userFonts.end()), userFonts.end());
		return userFonts;
	}

	const std::vector<std::string>& Styles::GetPresets()
	{
		if (cachedPresets.empty()) {
			cachedPresets = Settings::GetConfigs(Settings::GetSingleton()->GetPresetsPath());
		}
		return cachedPresets;
	}

	void Styles::SetCurrentFont(const std::string& a_fontName)
	{
		currentFont = a_fontName;
		MANAGER(IconFont)->SetFontName(currentFont);
	}

	// ==========================================
	// PRESET LOGIC
	// ==========================================

	void Styles::ResetToDefaults()
	{
		user = def;
		currentPresetName = "";

		Settings::GetSingleton()->Save(FileType::kSettings, [this](auto& sIni) {
			sIni.SetValue("Settings", "sLastPreset", "");
		});

		SetCurrentFont("Jost-Regular.ttf");
		RefreshStyle();
		MANAGER(IconFont)->ReloadFonts();
	}

	void Styles::LoadStyles()
	{
		ResetToDefaults();

		std::string lastPreset = "";
		Settings::GetSingleton()->Load(FileType::kSettings, [&](auto& ini) {
			lastPreset = ini.GetValue("Settings", "sLastPreset", "");
		});

		if (!lastPreset.empty()) {
			LoadPreset(lastPreset);
		}

		if (currentFont.empty()) {
			SetCurrentFont("Jost-Regular.ttf");
		}

		RefreshStyle();
		if (!lastPreset.empty()) {
			MANAGER(IconFont)->ReloadFonts();
		}
	}

	void Styles::SavePreset(const std::string& a_name)
	{
		std::filesystem::path p(Settings::GetSingleton()->GetPresetsPath());
		if (!std::filesystem::exists(p))
			std::filesystem::create_directories(p);
		std::string filename = a_name;
		if (filename.length() < 4 || filename.substr(filename.length() - 4) != ".ini")
			filename += ".ini";
		p /= filename;
		CSimpleIniA ini;
		ini.SetUnicode();
		SaveStylesToIni(ini);
		ini.SetValue("Font", "sFontName", currentFont.c_str());
		if (ini.SaveFile(p.string().c_str()) >= 0) {
			currentPresetName = filename;
			Settings::GetSingleton()->Save(FileType::kSettings, [this](auto& sIni) {
				sIni.SetValue("Settings", "sLastPreset", currentPresetName.c_str());
			});
			cachedPresets.clear();
		}
	}

	void Styles::LoadPreset(const std::string& a_name)
	{
		std::filesystem::path p(Settings::GetSingleton()->GetPresetsPath());
		p /= a_name;
		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile(p.string().c_str()) < 0)
			return;
		user = def;
		LoadStylesFromIni(ini);
		const char* fontName = ini.GetValue("Font", "sFontName");
		if (fontName && *fontName)
			SetCurrentFont(fontName);
		MANAGER(IconFont)->LoadSettings(ini);
		currentPresetName = a_name;
		Settings::GetSingleton()->Save(FileType::kSettings, [this](auto& sIni) {
			sIni.SetValue("Settings", "sLastPreset", currentPresetName.c_str());
		});
		RefreshStyle();
		MANAGER(IconFont)->ReloadFonts();
	}

	// ==========================================
	// SERIALIZATION
	// ==========================================

	void Styles::SaveStylesToIni(CSimpleIniA& a_ini)
	{
#define SET_VALUE(a_value, a_section, a_key) a_ini.SetValue(a_section, a_key, ToString(user.a_value, true).c_str())

		SET_VALUE(iconDisabled, "Icon", "rDisabledColor");
		SET_VALUE(buttonScale, "Icon", "fButtonScale");
		SET_VALUE(checkboxScale, "Icon", "fCheckboxScale");
		SET_VALUE(stepperScale, "Icon", "fStepperScale");
		SET_VALUE(widgetToggleActive, "Icon", "rToggleActiveColor");
		SET_VALUE(widgetFlash, "Icon", "rFlashColor");

		SET_VALUE(background, "Window", "rBackgroundColor");
		SET_VALUE(border, "Window", "rBorderColor");
		SET_VALUE(borderSize, "Window", "fBorderSize");
		SET_VALUE(windowRounding, "Window", "fRounding");
		SET_VALUE(windowPadding, "Window", "vPadding");

		SET_VALUE(comboBoxTextBox, "ComboBox", "rTextBoxColor");
		SET_VALUE(frameBG, "ComboBox", "rListBoxColor");
		SET_VALUE(comboBoxText, "ComboBox", "rTextColor");
		SET_VALUE(button, "ComboBox", "rArrowButtonColor");
		SET_VALUE(popupRounding, "ComboBox", "fPopupRounding");

		SET_VALUE(sliderGrab, "Slider", "rColor");
		SET_VALUE(sliderGrabActive, "Slider", "rActiveColor");
		SET_VALUE(sliderBorder, "Slider", "rBorderColor");
		SET_VALUE(sliderBorderActive, "Slider", "rBorderActiveColor");
		SET_VALUE(grabRounding, "Slider", "fGrabRounding");

		SET_VALUE(toggleRail, "Slider", "rToggleRailColor");
		SET_VALUE(toggleRailFilled, "Slider", "rToggleRailFillColor");
		SET_VALUE(toggleKnob, "Slider", "rToggleKnobColor");

		SET_VALUE(textHeader, "Header", "rTextColor");
		SET_VALUE(header, "Header", "rHighlightColor");
		SET_VALUE(headerHovered, "Header", "rHighlightHoveredColor");
		SET_VALUE(navHighlight, "Header", "rNavHighlightColor");

		SET_VALUE(text, "Text", "rColor");
		SET_VALUE(textDisabled, "Text", "rDisabledColor");
		SET_VALUE(textButton, "Text", "rButtonTextColor");
		SET_VALUE(textHovered, "Text", "rHoveredTextColor");

		SET_VALUE(frameBG_Widget, "Widget", "rBackgroundColor");
		SET_VALUE(frameBG_WidgetActive, "Widget", "rBackgroundActiveColor");
		SET_VALUE(gridLines, "Widget", "rGridColor");
		SET_VALUE(gridThickness, "Widget", "fGridThickness");
		SET_VALUE(indentSpacing, "Widget", "fIndentSpacing");
		SET_VALUE(separator, "Widget", "rSeparatorColor");
		SET_VALUE(separatorThickness, "Widget", "fSeparatorThickness");

		SET_VALUE(tab, "Widget", "rTabColor");
		SET_VALUE(tabHovered, "Widget", "rTabActiveColor");
		SET_VALUE(tabBorder, "Widget", "rTabBorderColor");
		SET_VALUE(tabBorderActive, "Widget", "rTabBorderActiveColor");

		SET_VALUE(frameRounding, "Widget", "fFrameRounding");
		SET_VALUE(buttonRounding, "Widget", "fButtonRounding");
		SET_VALUE(tabRounding, "Widget", "fTabRounding");
		SET_VALUE(itemSpacing, "Widget", "vItemSpacing");

		SET_VALUE(scrollbarBG, "Widget", "rScrollbarBGColor");
		SET_VALUE(scrollbarGrab, "Widget", "rScrollbarGrabColor");
		SET_VALUE(scrollbarGrabHovered, "Widget", "rScrollbarGrabHoveredColor");
		SET_VALUE(scrollbarGrabActive, "Widget", "rScrollbarGrabActiveColor");
		SET_VALUE(scrollbarRounding, "Widget", "fScrollbarRounding");

#undef SET_VALUE
	}

	void Styles::LoadStylesFromIni(CSimpleIniA& a_ini)
	{
#define GET_VALUE(a_value, a_section, a_key) \
	bool a_value##_hex = false;              \
	std::tie(user.a_value, a_value##_hex) = ToStyle<decltype(user.a_value)>(a_ini.GetValue(a_section, a_key, ToString(def.a_value, true).c_str()));

		GET_VALUE(iconDisabled, "Icon", "rDisabledColor");
		GET_VALUE(buttonScale, "Icon", "fButtonScale");
		GET_VALUE(checkboxScale, "Icon", "fCheckboxScale");
		GET_VALUE(stepperScale, "Icon", "fStepperScale");
		GET_VALUE(widgetToggleActive, "Icon", "rToggleActiveColor");
		GET_VALUE(widgetFlash, "Icon", "rFlashColor");

		GET_VALUE(background, "Window", "rBackgroundColor");
		GET_VALUE(border, "Window", "rBorderColor");
		GET_VALUE(borderSize, "Window", "fBorderSize");
		GET_VALUE(windowRounding, "Window", "fRounding");
		GET_VALUE(windowPadding, "Window", "vPadding");

		GET_VALUE(comboBoxTextBox, "ComboBox", "rTextBoxColor");
		GET_VALUE(frameBG, "ComboBox", "rListBoxColor");
		GET_VALUE(comboBoxText, "ComboBox", "rTextColor");
		GET_VALUE(button, "ComboBox", "rArrowButtonColor");
		GET_VALUE(popupRounding, "ComboBox", "fPopupRounding");

		GET_VALUE(sliderGrab, "Slider", "rColor");
		GET_VALUE(sliderGrabActive, "Slider", "rActiveColor");
		GET_VALUE(sliderBorder, "Slider", "rBorderColor");
		GET_VALUE(sliderBorderActive, "Slider", "rBorderActiveColor");
		GET_VALUE(grabRounding, "Slider", "fGrabRounding");

		GET_VALUE(toggleRail, "Slider", "rToggleRailColor");
		GET_VALUE(toggleRailFilled, "Slider", "rToggleRailFillColor");
		GET_VALUE(toggleKnob, "Slider", "rToggleKnobColor");

		GET_VALUE(textHeader, "Header", "rTextColor");
		GET_VALUE(header, "Header", "rHighlightColor");
		GET_VALUE(headerHovered, "Header", "rHighlightHoveredColor");
		GET_VALUE(navHighlight, "Header", "rNavHighlightColor");

		GET_VALUE(text, "Text", "rColor");
		GET_VALUE(textDisabled, "Text", "rDisabledColor");
		GET_VALUE(textButton, "Text", "rButtonTextColor");
		GET_VALUE(textHovered, "Text", "rHoveredTextColor");

		GET_VALUE(frameBG_Widget, "Widget", "rBackgroundColor");
		GET_VALUE(frameBG_WidgetActive, "Widget", "rBackgroundActiveColor");
		GET_VALUE(gridLines, "Widget", "rGridColor");
		GET_VALUE(gridThickness, "Widget", "fGridThickness");
		GET_VALUE(indentSpacing, "Widget", "fIndentSpacing");
		GET_VALUE(separator, "Widget", "rSeparatorColor");
		GET_VALUE(separatorThickness, "Widget", "fSeparatorThickness");

		GET_VALUE(tab, "Widget", "rTabColor");
		GET_VALUE(tabHovered, "Widget", "rTabActiveColor");
		GET_VALUE(tabBorder, "Widget", "rTabBorderColor");
		GET_VALUE(tabBorderActive, "Widget", "rTabBorderActiveColor");

		GET_VALUE(frameRounding, "Widget", "fFrameRounding");
		GET_VALUE(buttonRounding, "Widget", "fButtonRounding");
		GET_VALUE(tabRounding, "Widget", "fTabRounding");
		GET_VALUE(itemSpacing, "Widget", "vItemSpacing");

		GET_VALUE(scrollbarBG, "Widget", "rScrollbarBGColor");
		GET_VALUE(scrollbarGrab, "Widget", "rScrollbarGrabColor");
		GET_VALUE(scrollbarGrabHovered, "Widget", "rScrollbarGrabHoveredColor");
		GET_VALUE(scrollbarGrabActive, "Widget", "rScrollbarGrabActiveColor");
		GET_VALUE(scrollbarRounding, "Widget", "fScrollbarRounding");

#undef GET_VALUE
		ConvertVec4StylesToU32();
	}

	// ==========================================
	// REFRESH
	// ==========================================

	void Styles::OnStyleRefresh()
	{
		if (!refreshStyle) {
			return;
		}

		ConvertVec4StylesToU32();

		ImGuiStyle style{};
		auto& colors = style.Colors;

		style.WindowPadding = user.windowPadding;
		style.FramePadding = ImVec2(6.0f, 2.0f);
		style.ItemSpacing = user.itemSpacing;
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing = user.indentSpacing;
		style.ColumnsMinSpacing = 6.0f;
		style.ScrollbarSize = 12.0f;
		style.GrabMinSize = 10.0f;

		style.WindowBorderSize = user.borderSize;
		style.ChildBorderSize = user.borderSize;
		style.FrameBorderSize = user.borderSize;
		style.PopupBorderSize = user.borderSize;
		style.TabBorderSize = 0.0f;

		style.WindowRounding = user.windowRounding;
		style.ChildRounding = user.windowRounding;
		style.FrameRounding = user.frameRounding;
		style.PopupRounding = user.popupRounding;
		style.ScrollbarRounding = user.scrollbarRounding;
		style.GrabRounding = user.grabRounding;
		style.TabRounding = user.tabRounding;

		colors[ImGuiCol_WindowBg] = user.background;
		colors[ImGuiCol_ChildBg] = user.background;
		colors[ImGuiCol_PopupBg] = user.background;

		colors[ImGuiCol_Text] = user.text;
		colors[ImGuiCol_TextDisabled] = user.textDisabled;

		colors[ImGuiCol_Header] = user.header;
		colors[ImGuiCol_HeaderHovered] = user.headerHovered;
		colors[ImGuiCol_HeaderActive] = user.headerHovered;

		colors[ImGuiCol_FrameBg] = user.frameBG_Widget;
		colors[ImGuiCol_FrameBgHovered] = user.frameBG_WidgetActive;
		colors[ImGuiCol_FrameBgActive] = user.frameBG_WidgetActive;

		colors[ImGuiCol_Button] = user.button;
		colors[ImGuiCol_ButtonHovered] = user.frameBG_WidgetActive;
		colors[ImGuiCol_ButtonActive] = user.sliderBorderActive;

		colors[ImGuiCol_Tab] = user.tab;
		colors[ImGuiCol_TabHovered] = user.tabHovered;
		colors[ImGuiCol_TabActive] = user.tab;
		colors[ImGuiCol_TabUnfocused] = user.tab;
		colors[ImGuiCol_TabUnfocusedActive] = user.tab;

		colors[ImGuiCol_ResizeGrip] = user.sliderBorderActive;
		colors[ImGuiCol_ResizeGripHovered] = user.sliderBorderActive;
		colors[ImGuiCol_ResizeGripActive] = user.sliderBorderActive;

		colors[ImGuiCol_Border] = user.sliderBorder;
		colors[ImGuiCol_Separator] = user.separator;
		colors[ImGuiCol_SeparatorHovered] = user.sliderBorderActive;
		colors[ImGuiCol_SeparatorActive] = user.sliderBorderActive;

		colors[ImGuiCol_SliderGrab] = user.sliderGrab;
		colors[ImGuiCol_SliderGrabActive] = user.sliderGrabActive;

		colors[ImGuiCol_ScrollbarBg] = user.scrollbarBG;
		colors[ImGuiCol_NavHighlight] = user.navHighlight;

		ImGui::GetStyle() = style;

		MANAGER(IconFont)->ResizeIcons();
		refreshStyle = false;
	}

	void Styles::RefreshStyle()
	{
		refreshStyle = true;
	}

	ImU32 GetUserStyleColorU32(USER_STYLE a_style) { return Styles::GetSingleton()->GetColorU32(a_style); }
	ImVec4 GetUserStyleColorVec4(USER_STYLE a_style) { return Styles::GetSingleton()->GetColorVec4(a_style); }
	float GetUserStyleVar(USER_STYLE a_style) { return Styles::GetSingleton()->GetVar(a_style); }
}
