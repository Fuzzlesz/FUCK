#include "FUCKStyles.h"
#include "FUCKMan.h"
#include "ImGui/Styles.h"

void ThemeEditorWindow::Draw()
{
	auto style = ImGui::Styles::GetSingleton();

	auto ColorPick = [&](const char* label, ImVec4& col) {
		FUCK::LeftLabel(label);
		std::string id = std::format("##{}", label);

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kFrameBG_Widget));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kFrameBG_WidgetActive));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kFrameBG_WidgetActive));
		ImGui::PushStyleColor(ImGuiCol_Border, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kSliderBorder));

		if (ImGui::ColorEdit4(id.c_str(), (float*)&col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_NoLabel)) {
			style->RefreshStyle();
		}

		if (ImGui::IsItemActivated())
			RE::PlaySound("UIMenuFocus");

		ImGui::PopStyleColor(4);
	};

	if (FUCK::CollapsingHeader("$FUCK_Styles_Presets"_T, ImGuiTreeNodeFlags_DefaultOpen)) {
		FUCK::Spacing();

		static std::vector<std::string> fonts = style->GetAvailableFonts();
		std::string currentFont = style->GetCurrentFont();

		std::vector<const char*> fontPtrs;
		fontPtrs.reserve(fonts.size());
		int currentFontIdx = -1;

		for (size_t i = 0; i < fonts.size(); ++i) {
			fontPtrs.push_back(fonts[i].c_str());
			if (fonts[i] == currentFont) {
				currentFontIdx = static_cast<int>(i);
			}
		}

		if (FUCK::Combo("$FUCK_Styles_Typeface"_T, &currentFontIdx, fontPtrs.data(), (int)fontPtrs.size())) {
			if (currentFontIdx >= 0 && currentFontIdx < fonts.size()) {
				style->SetCurrentFont(fonts[currentFontIdx]);
			}
		}
		FUCK::Spacing();

		static char newPresetNameBuf[64] = "MyNewPreset";
		FUCK::InputText("$FUCK_Styles_NewPresetName"_T, newPresetNameBuf, 64);

		if (FUCK::Button("$FUCK_Styles_SaveNew"_T)) {
			style->SavePreset(newPresetNameBuf);
			style->RefreshStyle();
		}

		FUCK::SameLine();

		std::string currentPreset = style->GetCurrentPresetName();
		if (!currentPreset.empty()) {
			std::string btnLabel = std::format("{} ({})", "$FUCK_Styles_SaveCurrent"_T, currentPreset);
			if (FUCK::Button(btnLabel.c_str())) {
				style->SavePreset(currentPreset);
			}
		}

		FUCK::Spacing(2);
		FUCK::Separator();
		FUCK::Spacing(2);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Window"_T)) {
		ColorPick("$FUCK_Styles_Background"_T, style->user.background);
		ColorPick("$FUCK_Styles_Border"_T, style->user.border);
		if (FUCK::SliderFloat("$FUCK_Styles_BorderSize"_T, &style->user.borderSize, 0.0f, 5.0f)) {
			style->RefreshStyle();
		}

		FUCK::Separator();
		FUCK::LeftLabel("$FUCK_Styles_WindowPadding"_T);
		if (ImGui::DragFloat2("##WinPad", &style->user.windowPadding.x, 0.5f, 0.0f, 30.0f, "%.0f"))
			style->RefreshStyle();

		FUCK::LeftLabel("$FUCK_Styles_ItemSpacing"_T);
		if (ImGui::DragFloat2("##ItemSpace", &style->user.itemSpacing.x, 0.5f, 0.0f, 30.0f, "%.0f"))
			style->RefreshStyle();

		FUCK::LeftLabel("$FUCK_Styles_WindowRounding"_T);
		if (FUCK::SliderFloat("##WinRound", &style->user.windowRounding, 0.0f, 20.0f, "%.0f"))
			style->RefreshStyle();
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Text"_T)) {
		ColorPick("$FUCK_Styles_Primary"_T, style->user.text);
		ColorPick("$FUCK_Styles_Header"_T, style->user.textHeader);
		ColorPick("$FUCK_Styles_HoverActive"_T, style->user.textHovered);
		ColorPick("$FUCK_Styles_Disabled"_T, style->user.textDisabled);
		ColorPick("$FUCK_Styles_NavHighlight"_T, style->user.navHighlight);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Widgets"_T)) {
		ColorPick("$FUCK_Styles_FrameBG"_T, style->user.frameBG_Widget);
		ColorPick("$FUCK_Styles_FrameBGActive"_T, style->user.frameBG_WidgetActive);
		ColorPick("$FUCK_Styles_ButtonColor"_T, style->user.button);

		FUCK::Separator();
		ColorPick("$FUCK_Styles_TabBG"_T, style->user.tab);
		ColorPick("$FUCK_Styles_TabBGActive"_T, style->user.tabHovered);
		ColorPick("$FUCK_Styles_TabBorder"_T, style->user.tabBorder);
		ColorPick("$FUCK_Styles_TabBorderActive"_T, style->user.tabBorderActive);

		FUCK::Separator();
		ColorPick("$FUCK_Styles_ToggleRail"_T, style->user.toggleRail);
		ColorPick("$FUCK_Styles_ToggleRailFill"_T, style->user.toggleRailFilled);
		ColorPick("$FUCK_Styles_ToggleKnob"_T, style->user.toggleKnob);

		FUCK::Separator();
		ColorPick("$FUCK_Styles_SeparatorColor"_T, style->user.separator);
		if (FUCK::SliderFloat("$FUCK_Styles_SeparatorThick"_T, &style->user.separatorThickness, 1.0f, 10.0f, "%.1f")) {
			style->RefreshStyle();
		}

		FUCK::Separator();
		auto DragRound = [&](const char* label, float* v) {
			if (FUCK::SliderFloat(label, v, 0.0f, 12.0f, "%.0f"))
				style->RefreshStyle();
		};

		DragRound("$FUCK_Styles_FrameRounding"_T, &style->user.frameRounding);
		DragRound("$FUCK_Styles_ButtonRounding"_T, &style->user.buttonRounding);
		DragRound("$FUCK_Styles_TabRounding"_T, &style->user.tabRounding);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Combos"_T)) {
		ColorPick("$FUCK_Styles_ListBG"_T, style->user.frameBG);
		ColorPick("$FUCK_Styles_TextBoxBG"_T, style->user.comboBoxTextBox);
		ColorPick("$FUCK_Styles_ComboText"_T, style->user.comboBoxText);
		if (FUCK::SliderFloat("$FUCK_Styles_PopupRounding"_T, &style->user.popupRounding, 0.0f, 12.0f, "%.0f"))
			style->RefreshStyle();
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Sliders"_T)) {
		ColorPick("$FUCK_Styles_SliderIdle"_T, style->user.sliderBorder);
		ColorPick("$FUCK_Styles_SliderActive"_T, style->user.sliderBorderActive);
		ColorPick("$FUCK_Styles_SliderGrab"_T, style->user.sliderGrab);
		ColorPick("$FUCK_Styles_SliderGrabActive"_T, style->user.sliderGrabActive);

		FUCK::Separator();

		ColorPick("$FUCK_Styles_ScrollbarBG"_T, style->user.scrollbarBG);
		ColorPick("$FUCK_Styles_ScrollbarGrab"_T, style->user.scrollbarGrab);
		ColorPick("$FUCK_Styles_ScrollbarGrabActive"_T, style->user.scrollbarGrabActive);

		if (FUCK::SliderFloat("$FUCK_Styles_ScrollbarRounding"_T, &style->user.scrollbarRounding, 0.0f, 12.0f, "%.0f"))
			style->RefreshStyle();
		if (FUCK::SliderFloat("$FUCK_Styles_GrabRounding"_T, &style->user.grabRounding, 0.0f, 12.0f, "%.0f"))
			style->RefreshStyle();
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_Misc"_T)) {
		ColorPick("$FUCK_Styles_Flash"_T, style->user.widgetFlash);
		ColorPick("$FUCK_Styles_ToggleActive"_T, style->user.widgetToggleActive);
		if (FUCK::SliderFloat("$FUCK_Styles_IndentSpacing"_T, &style->user.indentSpacing, 0.0f, 50.0f))
			style->RefreshStyle();
	}
}
