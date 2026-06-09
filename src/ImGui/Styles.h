#pragma once

namespace ImGui
{
	enum class USER_STYLE
	{
		// Colours - Global
		kTextHovered,
		kHeaderText,
		kDisabledTextAlpha,
		kNavHighlight,

		// Colours - Widgets
		kFrameBG_Widget,
		kFrameBG_WidgetActive,
		kButtons,
		kSeparator,
		kTabBorder,
		kTabBorderActive,
		kComboBoxTextBox,
		kComboBoxText,
		kWidgetFlash,
		kIconDisabled,
		kGridLines,

		// Colours - Controls
		kWidgetBorder,
		kWidgetBorderActive,
		kToggleRailFilled,
		kToggleKnob,
		kWidgetToggleActive,
		kScrollbarBG,

		// Layout & Geometry
		kWindowPadding,
		kItemSpacing,
		kButtonRounding,
		kButtonBorderSize,
	};

	class Styles : public REX::Singleton<Styles>
	{
	public:
		struct Style
		{
			// --------------------------------------------------
			// Helpers / Vars
			// --------------------------------------------------
			float bgAlpha{ 0.68f };
			float disabledAlpha{ 0.30f };

			// --------------------------------------------------
			// Colours - Global
			// --------------------------------------------------
			ImVec4 background{ 0.0f, 0.0f, 0.0f, bgAlpha };
			ImVec4 border{ 0.25f, 0.25f, 0.25f, 1.0f };
			ImVec4 text{ 0.733f, 0.741f, 0.749f, 1.0f };
			ImVec4 textHeader{ 0.90f, 0.90f, 0.90f, 0.90f };
			ImVec4 textHovered{ 1.0f, 1.0f, 1.0f, 1.0f };
			ImVec4 textDisabled{ 0.733f, 0.741f, 0.749f, disabledAlpha };
			ImVec4 navHighlight{ 1.0f, 1.0f, 1.0f, 1.0f };

			// --------------------------------------------------
			// Colours - Widgets
			// --------------------------------------------------
			ImVec4 frameBG_Widget{ 1.0f, 1.0f, 1.0f, 0.063f };
			ImVec4 frameBG_WidgetActive{ 1.0f, 1.0f, 1.0f, 0.2f };
			ImVec4 button{ 0.0f, 0.0f, 0.0f, 1.0f };
			ImVec4 separator{ 0.25f, 0.25f, 0.25f, 1.0f };
			ImVec4 tab{ 0.0f, 0.0f, 0.0f, 0.0f };
			ImVec4 tabHovered{ 0.0f, 0.0f, 0.0f, 1.0f };
			ImVec4 tabBorder{ 0.25f, 0.25f, 0.25f, 1.0f };
			ImVec4 tabBorderActive{ 0.60f, 0.60f, 0.60f, 1.0f };
			ImVec4 comboBoxTextBox{ 0.0f, 0.0f, 0.0f, 1.0f };
			ImVec4 comboBoxText{ 1.0f, 1.0f, 1.0f, 0.8f };
			ImVec4 widgetFlash{ 1.0f, 0.8f, 0.2f, 1.0f };

			// Other Widget Colours
			ImVec4 header{ 1.0f, 1.0f, 1.0f, 0.1f };
			ImVec4 headerHovered{ 1.0f, 1.0f, 1.0f, 0.15f };
			ImVec4 gridLines{ 1.0f, 1.0f, 1.0f, 0.329f };
			ImVec4 iconDisabled{ 1.0f, 1.0f, 1.0f, disabledAlpha };

			// --------------------------------------------------
			// Colours - Controls
			// --------------------------------------------------
			ImVec4 widgetBorder{ 0.25f, 0.25f, 0.25f, 1.0f };
			ImVec4 widgetBorderActive{ 0.60f, 0.60f, 0.60f, 1.0f };
			ImVec4 sliderGrab{ 0.60f, 0.60f, 0.60f, 1.0f };
			ImVec4 sliderGrabActive{ 0.90f, 0.90f, 0.90f, 1.0f };
			ImVec4 toggleRailFilled{ 0.0f, 0.0f, 0.0f, 1.0f };
			ImVec4 toggleKnob{ 0.0f, 0.0f, 0.0f, 1.0f };
			ImVec4 widgetToggleActive{ 1.0f, 1.0f, 1.0f, 0.65f };
			ImVec4 scrollbarBG{ 0.0f, 0.0f, 0.0f, 0.0f };
			ImVec4 scrollbarGrab{ 0.31f, 0.31f, 0.31f, 1.0f };
			ImVec4 scrollbarGrabHovered{ 0.408f, 0.408f, 0.408f, 1.0f };
			ImVec4 scrollbarGrabActive{ 0.51f, 0.51f, 0.51f, 1.0f };

			// --------------------------------------------------
			// Layout & Geometry
			// --------------------------------------------------
			ImVec2 windowPadding{ 8.0f, 8.0f };
			ImVec2 itemSpacing{ 8.0f, 4.0f };
			float  indentSpacing{ 80.0f };

			float  widgetSplit{ 0.5f };
			ImVec2 labelAlign{ 0.0f, 0.5f };

			float borderSize{ 4.0f };
			float buttonBorderSize{ 3.0f };
			float gridThickness{ 2.5f };

			float iconScale{ 1.2f };

			float windowRounding{ 0.0f };
			float frameRounding{ 0.0f };
			float buttonRounding{ 6.0f };
			float tabRounding{ 6.0f };
			float popupRounding{ 0.0f };
			float scrollbarRounding{ 0.0f };
			float grabRounding{ 0.0f };

			// --------------------------------------------------
			// Audio
			// --------------------------------------------------
			std::string soundOk{ "UIMenuOKSD" };
			std::string soundCancel{ "UIMenuCancelSD" };
			std::string soundFocus{ "UIMenuFocus" };
			std::string soundPrevNext{ "UIMenuPrevNextSD" };
			std::string soundTab{ "UIJournalTabsSD" };
		};

		// --- API ---
		ImU32  GetColorU32(USER_STYLE a_style) const;
		ImVec4 GetColorVec4(USER_STYLE a_style) const;
		float  GetVar(USER_STYLE a_style) const;

		void        ResetToDefaults(bool a_saveToIni = true);
		void        LoadStyles();
		void        SavePreset(const std::string& a_name);
		void        LoadPreset(const std::string& a_name, bool a_saveToIni = true);
		void        DeletePreset(const std::string& a_name);
		std::string GetCurrentPresetName() const { return currentPresetName; }

		// Font Management
		std::vector<std::string> GetAvailableFonts() const;

		const std::vector<std::string>& GetPresets();

		void OnStyleRefresh();
		void RefreshStyle();

		Style user;
		Style def;

	private:
		void LoadStylesFromIni(CSimpleIniA& a_ini);
		void SaveStylesToIni(CSimpleIniA& a_ini);
		void ConvertVec4StylesToU32();

		template <class T>
		std::pair<T, bool> ToStyle(const std::string& a_str);
		template <class T>
		std::string ToString(const T& a_style, bool a_hex);

		// Cached U32
		ImU32 textHeaderU32;
		ImU32 navHighlightU32;
		ImU32 frameBG_WidgetU32;
		ImU32 frameBG_WidgetActiveU32;
		ImU32 tabBorderU32;
		ImU32 tabBorderActiveU32;
		ImU32 widgetFlashU32;
		ImU32 iconDisabledU32;
		ImU32 gridLinesU32;
		ImU32 widgetBorderU32;
		ImU32 widgetBorderActiveU32;
		ImU32 toggleRailFilledU32;
		ImU32 toggleKnobU32;
		ImU32 widgetToggleActiveU32;
		ImU32 scrollbarBGU32;

		bool        refreshStyle{ false };
		std::string currentPresetName{ "" };

		std::vector<std::string> cachedPresets;
	};

	// Free functions
	ImU32  GetUserStyleColorU32(USER_STYLE a_style);
	ImVec4 GetUserStyleColorVec4(USER_STYLE a_style);
	float  GetUserStyleVar(USER_STYLE a_style);

	// Template Implementations
	template <class T>
	inline std::pair<T, bool> Styles::ToStyle(const std::string& a_str)
	{
		if constexpr (std::is_same_v<std::string, T>) {
			return { a_str, false };
		} else if constexpr (std::is_same_v<ImVec4, T>) {
			static srell::regex rgb_pattern("([0-9]+),([0-9]+),([0-9]+),([0-9]+)");
			static srell::regex hex_pattern("#([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})");

			srell::smatch rgb_matches;
			srell::smatch hex_matches;

			if (srell::regex_match(a_str, rgb_matches, rgb_pattern)) {
				auto red   = std::stoi(rgb_matches[1]);
				auto green = std::stoi(rgb_matches[2]);
				auto blue  = std::stoi(rgb_matches[3]);
				auto alpha = std::stoi(rgb_matches[4]);
				return { { red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f }, false };
			} else if (srell::regex_match(a_str, hex_matches, hex_pattern)) {
				auto red   = std::stoi(hex_matches[1], 0, 16);
				auto green = std::stoi(hex_matches[2], 0, 16);
				auto blue  = std::stoi(hex_matches[3], 0, 16);
				auto alpha = std::stoi(hex_matches[4], 0, 16);
				return { { red / 255.0f, green / 255.0f, blue / 255.0f, alpha / 255.0f }, true };
			}
			return { T(), false };
		} else if constexpr (std::is_same_v<ImVec2, T>) {
			static srell::regex vec2_pattern("([0-9\\.]+),([0-9\\.]+)");
			srell::smatch       matches;
			if (srell::regex_match(a_str, matches, vec2_pattern)) {
				auto x = std::stof(matches[1]);
				auto y = std::stof(matches[2]);
				return { { x, y }, false };
			}
			return { T(), false };
		} else {
			return { string::to_num<T>(a_str), false };
		}
	}

	template <class T>
	inline std::string Styles::ToString(const T& a_style, bool a_hex)
	{
		if constexpr (std::is_same_v<std::string, T>) {
			return a_style;
		} else if constexpr (std::is_same_v<ImVec4, T>) {
			if (a_hex) {
				return std::format("#{:02X}{:02X}{:02X}{:02X}",
					std::uint8_t(255.0f * a_style.x), std::uint8_t(255.0f * a_style.y),
					std::uint8_t(255.0f * a_style.z), std::uint8_t(255.0f * a_style.w));
			}
			return std::format("{},{},{},{}",
				std::uint8_t(255.0f * a_style.x), std::uint8_t(255.0f * a_style.y),
				std::uint8_t(255.0f * a_style.z), std::uint8_t(255.0f * a_style.w));
		} else if constexpr (std::is_same_v<ImVec2, T>) {
			return std::format("{:.3f},{:.3f}", a_style.x, a_style.y);
		} else {
			return std::format("{:.3f}", a_style);
		}
	}
}
