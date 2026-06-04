#include "FUCK-Man.h"

#include "IconsFonts.h"
#include "Renderer.h"
#include "Styles.h"

#include "System/Settings.h"
#include "System/Utils.h"

#include <shellapi.h>

namespace ImGui
{
	void Styles::ConvertVec4StylesToU32()
	{
		// Global
		textHeaderU32   = ColorConvertFloat4ToU32(user.textHeader);
		navHighlightU32 = ColorConvertFloat4ToU32(user.navHighlight);

		// Widgets
		frameBG_WidgetU32       = ColorConvertFloat4ToU32(user.frameBG_Widget);
		frameBG_WidgetActiveU32 = ColorConvertFloat4ToU32(user.frameBG_WidgetActive);
		tabBorderU32            = ColorConvertFloat4ToU32(user.tabBorder);
		tabBorderActiveU32      = ColorConvertFloat4ToU32(user.tabBorderActive);
		widgetFlashU32          = ColorConvertFloat4ToU32(user.widgetFlash);
		iconDisabledU32         = ColorConvertFloat4ToU32(user.iconDisabled);
		gridLinesU32            = ColorConvertFloat4ToU32(user.gridLines);

		// Controls
		sliderBorderU32       = ColorConvertFloat4ToU32(user.sliderBorder);
		sliderBorderActiveU32 = ColorConvertFloat4ToU32(user.sliderBorderActive);
		toggleRailFilledU32   = ColorConvertFloat4ToU32(user.toggleRailFilled);
		toggleKnobU32         = ColorConvertFloat4ToU32(user.toggleKnob);
		widgetToggleActiveU32 = ColorConvertFloat4ToU32(user.widgetToggleActive);
		scrollbarBGU32        = ColorConvertFloat4ToU32(user.scrollbarBG);
	}

	ImU32 Styles::GetColorU32(USER_STYLE a_style) const
	{
		switch (a_style) {
		case USER_STYLE::kTextHovered:
			return ColorConvertFloat4ToU32(user.textHovered);
		case USER_STYLE::kHeaderText:
			return textHeaderU32;
		case USER_STYLE::kNavHighlight:
			return navHighlightU32;
		case USER_STYLE::kFrameBG_Widget:
			return frameBG_WidgetU32;
		case USER_STYLE::kFrameBG_WidgetActive:
			return frameBG_WidgetActiveU32;
		case USER_STYLE::kSeparator:
			return ColorConvertFloat4ToU32(user.separator);
		case USER_STYLE::kTabBorder:
			return tabBorderU32;
		case USER_STYLE::kTabBorderActive:
			return tabBorderActiveU32;
		case USER_STYLE::kComboBoxTextBox:
			return ColorConvertFloat4ToU32(user.comboBoxTextBox);
		case USER_STYLE::kComboBoxText:
			return ColorConvertFloat4ToU32(user.comboBoxText);
		case USER_STYLE::kWidgetFlash:
			return widgetFlashU32;
		case USER_STYLE::kIconDisabled:
			return iconDisabledU32;
		case USER_STYLE::kGridLines:
			return gridLinesU32;
		case USER_STYLE::kSliderBorder:
			return sliderBorderU32;
		case USER_STYLE::kSliderBorderActive:
			return sliderBorderActiveU32;
		case USER_STYLE::kToggleRailFilled:
			return toggleRailFilledU32;
		case USER_STYLE::kToggleKnob:
			return toggleKnobU32;
		case USER_STYLE::kWidgetToggleActive:
			return widgetToggleActiveU32;
		case USER_STYLE::kScrollbarBG:
			return scrollbarBGU32;
		default:
			return ImU32();
		}
	}

	ImVec4 Styles::GetColorVec4(USER_STYLE a_style) const
	{
		switch (a_style) {
		case USER_STYLE::kTextHovered:
			return user.textHovered;
		case USER_STYLE::kHeaderText:
			return user.textHeader;
		case USER_STYLE::kNavHighlight:
			return user.navHighlight;
		case USER_STYLE::kFrameBG_Widget:
			return user.frameBG_Widget;
		case USER_STYLE::kFrameBG_WidgetActive:
			return user.frameBG_WidgetActive;
		case USER_STYLE::kButtons:
			return user.button;
		case USER_STYLE::kTabBorder:
			return user.tabBorder;
		case USER_STYLE::kTabBorderActive:
			return user.tabBorderActive;
		case USER_STYLE::kComboBoxTextBox:
			return user.comboBoxTextBox;
		case USER_STYLE::kComboBoxText:
			return user.comboBoxText;
		case USER_STYLE::kWidgetFlash:
			return user.widgetFlash;
		case USER_STYLE::kTextButton:
			return user.textButton;
		case USER_STYLE::kIconDisabled:
			return user.iconDisabled;
		case USER_STYLE::kToggleRailFilled:
			return user.toggleRailFilled;
		case USER_STYLE::kToggleKnob:
			return user.toggleKnob;
		case USER_STYLE::kWidgetToggleActive:
			return user.widgetToggleActive;
		case USER_STYLE::kScrollbarBG:
			return user.scrollbarBG;
		default:
			return ImVec4();
		}
	}

	float Styles::GetVar(USER_STYLE a_style) const
	{
		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();

		switch (a_style) {
		case USER_STYLE::kButtonRounding:
			return user.buttonRounding * scale;
		case USER_STYLE::kButtonBorderSize:
			return user.buttonBorderSize * scale;
		case USER_STYLE::kGridLines:
			return user.gridThickness * scale;
		case USER_STYLE::kDisabledTextAlpha:
			return user.textDisabled.w;
		default:
			return 1.0f;
		}
	}

	// ==================================================
	// FONTS
	// ==================================================

	std::vector<std::string> Styles::GetAvailableFonts() const
	{
		auto settings = Settings::GetSingleton();

		// Passing 'true' enables recursion
		auto userFonts = Utils::GetDirectoryFiles(settings->GetUserFontsPath(), ".ttf", true);
		auto gameFonts = Utils::GetDirectoryFiles(settings->GetLegacyFontsPath(), ".ttf", true);
		auto csFonts   = Utils::GetDirectoryFiles(settings->GetCSFontsPath(), ".ttf", true);

		auto userOtf = Utils::GetDirectoryFiles(settings->GetUserFontsPath(), ".otf", true);
		auto gameOtf = Utils::GetDirectoryFiles(settings->GetLegacyFontsPath(), ".otf", true);
		auto csOtf   = Utils::GetDirectoryFiles(settings->GetCSFontsPath(), ".otf", true);

		userFonts.insert(userFonts.end(), gameFonts.begin(), gameFonts.end());
		userFonts.insert(userFonts.end(), csFonts.begin(), csFonts.end());

		userFonts.insert(userFonts.end(), userOtf.begin(), userOtf.end());
		userFonts.insert(userFonts.end(), gameOtf.begin(), gameOtf.end());
		userFonts.insert(userFonts.end(), csOtf.begin(), csOtf.end());

		std::sort(userFonts.begin(), userFonts.end());
		userFonts.erase(std::unique(userFonts.begin(), userFonts.end()), userFonts.end());

		userFonts.insert(userFonts.begin(), "Default");
		return userFonts;
	}

	const std::vector<std::string>& Styles::GetPresets()
	{
		if (cachedPresets.empty()) {
			cachedPresets      = Utils::GetDirectoryFiles(Settings::GetSingleton()->GetStylesPath());
			static bool logged = false;
			if (!logged) {
				logger::info("Found [{}] style presets.", cachedPresets.size());
				logged = true;
			}
		}
		return cachedPresets;
	}

	// ==================================================
	// PRESET LOGIC
	// ==================================================

	void Styles::ResetToDefaults(bool a_saveToIni)
	{
		user = def;
		if (a_saveToIni) {
			currentPresetName = "";
			Settings::GetSingleton()->Save(FileType::kStyle, [](auto& sIni) {
				sIni.SetValue("Style", "sLastPreset", "");
			});
		}

		RefreshStyle();
	}

	void Styles::LoadStyles()
	{
		std::string lastPreset = "";
		std::string userFont   = "Default";

		Settings::GetSingleton()->Load(FileType::kStyle, [&](auto& ini) {
			lastPreset = ini.GetValue("Style", "sLastPreset", "");
			userFont   = ini.GetValue("Style", "sFont", "Default");
		});

		FUCKMan::GetSingleton()->SetCurrentFont(userFont);

		ResetToDefaults(false);
		GetPresets();

		if (!lastPreset.empty()) {
			LoadPreset(lastPreset, false);
		}

		RefreshStyle();
	}

	void Styles::SavePreset(const std::string& a_name)
	{
		fs::path p(Settings::GetSingleton()->GetStylesPath());
		if (!fs::exists(p))
			fs::create_directories(p);
		std::string filename = a_name;
		if (filename.length() < 4 || filename.substr(filename.length() - 4) != ".ini")
			filename += ".ini";
		p /= filename;
		CSimpleIniA ini;
		ini.SetUnicode();
		SaveStylesToIni(ini);

		if (ini.SaveFile(p.string().c_str()) >= 0) {
			currentPresetName = filename;
			Settings::GetSingleton()->Save(FileType::kStyle, [this](auto& sIni) {
				sIni.SetValue("Style", "sLastPreset", currentPresetName.c_str());
			});
			cachedPresets.clear();
		}
	}

	void Styles::LoadPreset(const std::string& a_name, bool a_saveToIni)
	{
		fs::path p(Settings::GetSingleton()->GetStylesPath());
		p /= a_name;
		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile(p.string().c_str()) < 0)
			return;

		logger::info("Loaded style preset: {}", a_name);

		user = def;
		LoadStylesFromIni(ini);

		if (currentPresetName != a_name) {
			currentPresetName = a_name;
			if (a_saveToIni) {
				Settings::GetSingleton()->Save(FileType::kStyle, [this](auto& sIni) {
					sIni.SetValue("Style", "sLastPreset", currentPresetName.c_str());
				});
			}
		}

		RefreshStyle();
	}

	void Styles::DeletePreset(const std::string& a_name)
	{
		if (a_name.empty())
			return;

		fs::path    p(Settings::GetSingleton()->GetStylesPath());
		std::string filename = a_name;

		if (filename.length() < 4 || filename.substr(filename.length() - 4) != ".ini")
			filename += ".ini";

		p /= filename;

		if (fs::exists(p)) {
			std::wstring widePath = p.wstring();
			widePath.push_back(L'\0');

			SHFILEOPSTRUCTW fileOp = { 0 };
			fileOp.wFunc           = FO_DELETE;
			fileOp.pFrom           = widePath.c_str();
			fileOp.fFlags          = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

			SHFileOperationW(&fileOp);

			cachedPresets.clear();
		}

		if (currentPresetName == filename || currentPresetName == a_name) {
			currentPresetName = "";
			Settings::GetSingleton()->Save(FileType::kStyle, [](auto& sIni) {
				sIni.SetValue("Style", "sLastPreset", "");
			});
		}
	}

	// ==================================================
	// SERIALIZATION
	// ==================================================

	void Styles::SaveStylesToIni(CSimpleIniA& a_ini)
	{
#define SET_VALUE(a_value, a_section, a_key)                \
	do {                                                    \
		std::string uStr = ToString(user.a_value, true);    \
		if (uStr == ToString(def.a_value, true)) {          \
			a_ini.Delete(a_section, a_key, true);           \
		} else {                                            \
			a_ini.SetValue(a_section, a_key, uStr.c_str()); \
		}                                                   \
	} while (0)

		// Colours - Global
		SET_VALUE(background, "Window", "rBackgroundColor");
		SET_VALUE(border, "Window", "rBorderColor");
		SET_VALUE(text, "Text", "rColor");
		SET_VALUE(textHeader, "Header", "rTextColor");
		SET_VALUE(textHovered, "Text", "rHoveredTextColor");
		SET_VALUE(textDisabled, "Text", "rDisabledColor");
		SET_VALUE(navHighlight, "Header", "rNavHighlightColor");

		// Colours - Widgets
		SET_VALUE(frameBG_Widget, "Widget", "rBackgroundColor");
		SET_VALUE(frameBG_WidgetActive, "Widget", "rBackgroundActiveColor");
		SET_VALUE(button, "ComboBox", "rArrowButtonColor");
		SET_VALUE(separator, "Widget", "rSeparatorColor");
		SET_VALUE(tab, "Widget", "rTabColor");
		SET_VALUE(tabHovered, "Widget", "rTabActiveColor");
		SET_VALUE(tabBorder, "Widget", "rTabBorderColor");
		SET_VALUE(tabBorderActive, "Widget", "rTabBorderActiveColor");
		SET_VALUE(frameBG, "ComboBox", "rListBoxColor");
		SET_VALUE(comboBoxTextBox, "ComboBox", "rTextBoxColor");
		SET_VALUE(comboBoxText, "ComboBox", "rTextColor");
		SET_VALUE(widgetFlash, "Icon", "rFlashColor");

		SET_VALUE(textButton, "Text", "rButtonTextColor");
		SET_VALUE(header, "Header", "rHighlightColor");
		SET_VALUE(headerHovered, "Header", "rHighlightHoveredColor");
		SET_VALUE(gridLines, "Widget", "rGridColor");
		SET_VALUE(iconDisabled, "Icon", "rDisabledColor");

		// Colours - Controls
		SET_VALUE(sliderBorder, "Slider", "rBorderColor");
		SET_VALUE(sliderBorderActive, "Slider", "rBorderActiveColor");
		SET_VALUE(sliderGrab, "Slider", "rColor");
		SET_VALUE(sliderGrabActive, "Slider", "rActiveColor");
		SET_VALUE(toggleRail, "Slider", "rToggleRailColor");
		SET_VALUE(toggleRailFilled, "Slider", "rToggleRailFillColor");
		SET_VALUE(toggleKnob, "Slider", "rToggleKnobColor");
		SET_VALUE(widgetToggleActive, "Icon", "rToggleActiveColor");
		SET_VALUE(scrollbarBG, "Widget", "rScrollbarBGColor");
		SET_VALUE(scrollbarGrab, "Widget", "rScrollbarGrabColor");
		SET_VALUE(scrollbarGrabHovered, "Widget", "rScrollbarGrabHoveredColor");
		SET_VALUE(scrollbarGrabActive, "Widget", "rScrollbarGrabActiveColor");

		// Layout & Geometry
		SET_VALUE(windowPadding, "Window", "vPadding");
		SET_VALUE(itemSpacing, "Widget", "vItemSpacing");
		SET_VALUE(indentSpacing, "Widget", "fIndentSpacing");
		SET_VALUE(widgetSplit, "Widget", "fWidgetSplit");
		SET_VALUE(labelAlign, "Widget", "vLabelAlign");
		SET_VALUE(borderSize, "Window", "fBorderSize");
		SET_VALUE(buttonBorderSize, "Widget", "fButtonBorderSize");
		SET_VALUE(gridThickness, "Widget", "fGridThickness");
		SET_VALUE(iconScale, "Icon", "fScale");
		SET_VALUE(windowRounding, "Window", "fRounding");
		SET_VALUE(frameRounding, "Widget", "fFrameRounding");
		SET_VALUE(buttonRounding, "Widget", "fButtonRounding");
		SET_VALUE(tabRounding, "Widget", "fTabRounding");
		SET_VALUE(popupRounding, "ComboBox", "fPopupRounding");
		SET_VALUE(scrollbarRounding, "Widget", "fScrollbarRounding");
		SET_VALUE(grabRounding, "Slider", "fGrabRounding");

#undef SET_VALUE
	}

	void Styles::LoadStylesFromIni(CSimpleIniA& a_ini)
	{
#define GET_VALUE(a_value, a_section, a_key)                                   \
	do {                                                                       \
		std::string defStr = ToString(def.a_value, true);                      \
		const char* valStr = a_ini.GetValue(a_section, a_key, defStr.c_str()); \
		user.a_value       = ToStyle<decltype(user.a_value)>(valStr).first;    \
	} while (0)

		// Colours - Global
		GET_VALUE(background, "Window", "rBackgroundColor");
		GET_VALUE(border, "Window", "rBorderColor");
		GET_VALUE(text, "Text", "rColor");
		GET_VALUE(textHeader, "Header", "rTextColor");
		GET_VALUE(textHovered, "Text", "rHoveredTextColor");
		GET_VALUE(textDisabled, "Text", "rDisabledColor");
		GET_VALUE(navHighlight, "Header", "rNavHighlightColor");

		// Colours - Widgets
		GET_VALUE(frameBG_Widget, "Widget", "rBackgroundColor");
		GET_VALUE(frameBG_WidgetActive, "Widget", "rBackgroundActiveColor");
		GET_VALUE(button, "ComboBox", "rArrowButtonColor");
		GET_VALUE(separator, "Widget", "rSeparatorColor");
		GET_VALUE(tab, "Widget", "rTabColor");
		GET_VALUE(tabHovered, "Widget", "rTabActiveColor");
		GET_VALUE(tabBorder, "Widget", "rTabBorderColor");
		GET_VALUE(tabBorderActive, "Widget", "rTabBorderActiveColor");
		GET_VALUE(frameBG, "ComboBox", "rListBoxColor");
		GET_VALUE(comboBoxTextBox, "ComboBox", "rTextBoxColor");
		GET_VALUE(comboBoxText, "ComboBox", "rTextColor");
		GET_VALUE(widgetFlash, "Icon", "rFlashColor");

		GET_VALUE(textButton, "Text", "rButtonTextColor");
		GET_VALUE(header, "Header", "rHighlightColor");
		GET_VALUE(headerHovered, "Header", "rHighlightHoveredColor");
		GET_VALUE(gridLines, "Widget", "rGridColor");
		GET_VALUE(iconDisabled, "Icon", "rDisabledColor");

		// Colours - Controls
		GET_VALUE(sliderBorder, "Slider", "rBorderColor");
		GET_VALUE(sliderBorderActive, "Slider", "rBorderActiveColor");
		GET_VALUE(sliderGrab, "Slider", "rColor");
		GET_VALUE(sliderGrabActive, "Slider", "rActiveColor");
		GET_VALUE(toggleRail, "Slider", "rToggleRailColor");
		GET_VALUE(toggleRailFilled, "Slider", "rToggleRailFillColor");
		GET_VALUE(toggleKnob, "Slider", "rToggleKnobColor");
		GET_VALUE(widgetToggleActive, "Icon", "rToggleActiveColor");
		GET_VALUE(scrollbarBG, "Widget", "rScrollbarBGColor");
		GET_VALUE(scrollbarGrab, "Widget", "rScrollbarGrabColor");
		GET_VALUE(scrollbarGrabHovered, "Widget", "rScrollbarGrabHoveredColor");
		GET_VALUE(scrollbarGrabActive, "Widget", "rScrollbarGrabActiveColor");

		// Layout & Geometry
		GET_VALUE(windowPadding, "Window", "vPadding");
		GET_VALUE(itemSpacing, "Widget", "vItemSpacing");
		GET_VALUE(indentSpacing, "Widget", "fIndentSpacing");
		GET_VALUE(widgetSplit, "Widget", "fWidgetSplit");
		GET_VALUE(labelAlign, "Widget", "vLabelAlign");
		GET_VALUE(borderSize, "Window", "fBorderSize");
		GET_VALUE(buttonBorderSize, "Widget", "fButtonBorderSize");
		GET_VALUE(gridThickness, "Widget", "fGridThickness");
		GET_VALUE(iconScale, "Icon", "fScale");
		GET_VALUE(windowRounding, "Window", "fRounding");
		GET_VALUE(frameRounding, "Widget", "fFrameRounding");
		GET_VALUE(buttonRounding, "Widget", "fButtonRounding");
		GET_VALUE(tabRounding, "Widget", "fTabRounding");
		GET_VALUE(popupRounding, "ComboBox", "fPopupRounding");
		GET_VALUE(scrollbarRounding, "Widget", "fScrollbarRounding");
		GET_VALUE(grabRounding, "Slider", "fGrabRounding");

#undef GET_VALUE
		ConvertVec4StylesToU32();
	}

	// ==================================================
	// REFRESH
	// ==================================================

	void Styles::OnStyleRefresh()
	{
		if (!refreshStyle) {
			return;
		}

		ConvertVec4StylesToU32();

		ImGuiStyle& style  = GetStyle();
		auto&       colors = style.Colors;

		// Base scale is resolution only — userScale is applied per-content-region in FUCKMan
		float scale = Renderer::GetResolutionScale();

		style.WindowPadding     = ImVec2(user.windowPadding.x * scale, user.windowPadding.y * scale);
		style.FramePadding      = ImVec2(6.0f * scale, 3.0f * scale);
		style.ItemSpacing       = ImVec2(user.itemSpacing.x * scale, user.itemSpacing.y * scale);
		style.ItemInnerSpacing  = ImVec2(4.0f * scale, 4.0f * scale);
		style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
		style.IndentSpacing     = user.indentSpacing * scale;
		style.ColumnsMinSpacing = 6.0f * scale;
		style.ScrollbarSize     = 12.0f * scale;
		style.GrabMinSize       = 10.0f * scale;

		style.WindowBorderSize = user.borderSize * scale;
		style.ChildBorderSize  = user.borderSize * scale;
		style.FrameBorderSize  = user.borderSize * scale;
		style.PopupBorderSize  = user.borderSize * scale;
		style.TabBorderSize    = 0.0f;

		style.WindowRounding    = user.windowRounding * scale;
		style.ChildRounding     = user.windowRounding * scale;
		style.FrameRounding     = user.frameRounding * scale;
		style.PopupRounding     = user.popupRounding * scale;
		style.ScrollbarRounding = user.scrollbarRounding * scale;
		style.GrabRounding      = user.grabRounding * scale;
		style.TabRounding       = user.tabRounding * scale;

		colors[ImGuiCol_WindowBg] = user.background;
		colors[ImGuiCol_ChildBg]  = user.background;
		colors[ImGuiCol_PopupBg]  = user.background;

		colors[ImGuiCol_Text]         = user.text;
		colors[ImGuiCol_TextDisabled] = user.textDisabled;

		colors[ImGuiCol_Header]        = user.header;
		colors[ImGuiCol_HeaderHovered] = user.headerHovered;
		colors[ImGuiCol_HeaderActive]  = user.headerHovered;

		colors[ImGuiCol_FrameBg]        = user.frameBG_Widget;
		colors[ImGuiCol_FrameBgHovered] = user.frameBG_WidgetActive;
		colors[ImGuiCol_FrameBgActive]  = user.frameBG_WidgetActive;

		colors[ImGuiCol_Button]        = user.button;
		colors[ImGuiCol_ButtonHovered] = user.frameBG_WidgetActive;
		colors[ImGuiCol_ButtonActive]  = user.sliderBorderActive;

		colors[ImGuiCol_Tab]                = user.tab;
		colors[ImGuiCol_TabHovered]         = user.tabHovered;
		colors[ImGuiCol_TabActive]          = user.tab;
		colors[ImGuiCol_TabUnfocused]       = user.tab;
		colors[ImGuiCol_TabUnfocusedActive] = user.tab;

		colors[ImGuiCol_ResizeGrip]        = user.sliderBorderActive;
		colors[ImGuiCol_ResizeGripHovered] = user.sliderBorderActive;
		colors[ImGuiCol_ResizeGripActive]  = user.sliderBorderActive;

		colors[ImGuiCol_Border]           = user.border;
		colors[ImGuiCol_Separator]        = user.separator;
		colors[ImGuiCol_SeparatorHovered] = user.sliderBorderActive;
		colors[ImGuiCol_SeparatorActive]  = user.sliderBorderActive;

		colors[ImGuiCol_SliderGrab]       = user.sliderGrab;
		colors[ImGuiCol_SliderGrabActive] = user.sliderGrabActive;

		colors[ImGuiCol_ScrollbarBg]  = user.scrollbarBG;
		colors[ImGuiCol_NavHighlight] = user.navHighlight;

		refreshStyle = false;
	}

	void Styles::RefreshStyle()
	{
		refreshStyle = true;
	}

	ImU32  GetUserStyleColorU32(USER_STYLE a_style) { return Styles::GetSingleton()->GetColorU32(a_style); }
	ImVec4 GetUserStyleColorVec4(USER_STYLE a_style) { return Styles::GetSingleton()->GetColorVec4(a_style); }
	float  GetUserStyleVar(USER_STYLE a_style) { return Styles::GetSingleton()->GetVar(a_style); }
}
