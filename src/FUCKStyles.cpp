#include "FUCKStyles.h"
#include "FUCKMan.h"
#include "ImGui/Styles.h"

void ThemeEditorWindow::Draw()
{
	auto style = ImGui::Styles::GetSingleton();

	auto ColorPick = [&](const char* label, ImVec4& col) {
		if (FUCK::ColorEdit4(label, (float*)&col)) {
			style->RefreshStyle();
		}
	};

	if (FUCK::CollapsingHeader("$FUCK_Styles_Presets"_T, ImGuiTreeNodeFlags_DefaultOpen)) {
		FUCK::Spacing();

		static char presetNameBuf[64] = "";
		static std::string lastSeenPreset = "\xFF";

		// Auto-fill the text box if the user selected a new preset from the Settings tab
		std::string activePreset = style->GetCurrentPresetName();
		if (activePreset != lastSeenPreset) {
			lastSeenPreset = activePreset;
			std::string display = activePreset;
			if (display.empty()) {
				display = "MyNewPreset";
			} else if (display.ends_with(".ini")) {
				display = display.substr(0, display.length() - 4);
			}
			strncpy_s(presetNameBuf, display.c_str(), sizeof(presetNameBuf));
		}

		FUCK::InputText("$FUCK_Styles_PresetName"_T, presetNameBuf, 64);
		FUCK::Spacing();

		// Save Button
		if (FUCK::Button("$FUCK_Styles_Save"_T)) {
			if (presetNameBuf[0] != '\0') {
				style->SavePreset(presetNameBuf);
				style->RefreshStyle();
				lastSeenPreset = style->GetCurrentPresetName();
			}
		}

		// Reload Button
		FUCK::SameLine();
		if (FUCK::Button("$FUCK_Styles_Reload"_T)) {
			if (presetNameBuf[0] != '\0' && !lastSeenPreset.empty()) {
				style->LoadPreset(lastSeenPreset);
			} else {
				style->ResetToDefaults();
				presetNameBuf[0] = '\0';
				lastSeenPreset = "";
			}
		}

		// Delete Button
		FUCK::SameLine();
		FUCK::Dummy(ImVec2(20.0f, 0.0f));
		FUCK::SameLine();

		FUCK::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
		FUCK::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
		FUCK::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
		FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.8f, 1.0f));

		ImFont* defaultFont = FUCK::GetFont(FUCK::Font::kRegular);
		float baseFontSize = defaultFont ? defaultFont->LegacySize : ImGui::GetStyle().FontSizeBase;

		FUCK::PushFont(defaultFont, baseFontSize * 0.85f);

		if (FUCK::Button("$FUCK_Styles_Delete"_T)) {
			if (presetNameBuf[0] != '\0') {
				style->DeletePreset(presetNameBuf);
				style->ResetToDefaults();
				presetNameBuf[0] = '\0';
				lastSeenPreset = "";
			}
		}
		FUCK::SetTooltip("$FUCK_Styles_Delete_TT"_T);

		FUCK::PopFont();
		FUCK::PopStyleColor(4);

		FUCK::Spacing(2);
		FUCK::Separator();
		FUCK::Spacing(2);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_ColoursGlobal"_T)) {
		ColorPick("$FUCK_Styles_Background"_T, style->user.background);
		ColorPick("$FUCK_Styles_Border"_T, style->user.border);
		FUCK::Separator();
		ColorPick("$FUCK_Styles_Primary"_T, style->user.text);
		ColorPick("$FUCK_Styles_Header"_T, style->user.textHeader);
		ColorPick("$FUCK_Styles_HoverActive"_T, style->user.textHovered);
		ColorPick("$FUCK_Styles_Disabled"_T, style->user.textDisabled);
		ColorPick("$FUCK_Styles_NavHighlight"_T, style->user.navHighlight);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_ColoursWidgets"_T)) {
		ColorPick("$FUCK_Styles_FrameBG"_T, style->user.frameBG_Widget);
		ColorPick("$FUCK_Styles_FrameBGActive"_T, style->user.frameBG_WidgetActive);
		ColorPick("$FUCK_Styles_ButtonColor"_T, style->user.button);
		ColorPick("$FUCK_Styles_SeparatorColor"_T, style->user.separator);
		FUCK::Separator();
		ColorPick("$FUCK_Styles_TabBG"_T, style->user.tab);
		ColorPick("$FUCK_Styles_TabBGActive"_T, style->user.tabHovered);
		ColorPick("$FUCK_Styles_TabBorder"_T, style->user.tabBorder);
		ColorPick("$FUCK_Styles_TabBorderActive"_T, style->user.tabBorderActive);
		FUCK::Separator();
		ColorPick("$FUCK_Styles_ListBG"_T, style->user.frameBG);
		ColorPick("$FUCK_Styles_TextBoxBG"_T, style->user.comboBoxTextBox);
		ColorPick("$FUCK_Styles_ComboText"_T, style->user.comboBoxText);
		FUCK::Separator();
		ColorPick("$FUCK_Styles_Flash"_T, style->user.widgetFlash);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_ColoursControls"_T)) {
		ColorPick("$FUCK_Styles_SliderIdle"_T, style->user.sliderBorder);
		ColorPick("$FUCK_Styles_SliderActive"_T, style->user.sliderBorderActive);
		ColorPick("$FUCK_Styles_SliderGrab"_T, style->user.sliderGrab);
		ColorPick("$FUCK_Styles_SliderGrabActive"_T, style->user.sliderGrabActive);
		FUCK::Separator();
		ColorPick("$FUCK_Styles_ToggleRail"_T, style->user.toggleRail);
		ColorPick("$FUCK_Styles_ToggleRailFill"_T, style->user.toggleRailFilled);
		ColorPick("$FUCK_Styles_ToggleKnob"_T, style->user.toggleKnob);
		ColorPick("$FUCK_Styles_ToggleActive"_T, style->user.widgetToggleActive);
		FUCK::Separator();
		ColorPick("$FUCK_Styles_ScrollbarBG"_T, style->user.scrollbarBG);
		ColorPick("$FUCK_Styles_ScrollbarGrab"_T, style->user.scrollbarGrab);
		ColorPick("$FUCK_Styles_ScrollbarGrabActive"_T, style->user.scrollbarGrabActive);
	}

	if (FUCK::CollapsingHeader("$FUCK_Styles_Cat_LayoutGeometry"_T)) {
		FUCK::Header("$FUCK_Styles_PaddingSpacing"_T);
		if (FUCK::DragFloat2("$FUCK_Styles_WindowPadding"_T, &style->user.windowPadding.x, 0.5f, 0.0f, 30.0f, "%.0f"))
			style->RefreshStyle();
		if (FUCK::DragFloat2("$FUCK_Styles_ItemSpacing"_T, &style->user.itemSpacing.x, 0.5f, 0.0f, 30.0f, "%.0f"))
			style->RefreshStyle();
		if (FUCK::SliderFloat("$FUCK_Styles_IndentSpacing"_T, &style->user.indentSpacing, 0.0f, 150.0f))
			style->RefreshStyle();

		FUCK::Spacing();
		FUCK::Header("Widget Alignment");

		static float tempSplit = style->user.widgetSplit;
		FUCK::DragFloat("Split Ratio", &tempSplit, 0.005f, 0.1f, 0.9f, "%.2f");
		if (FUCK::IsItemDeactivatedAfterEdit()) {
			style->user.widgetSplit = tempSplit;
			style->RefreshStyle();
		} else if (!FUCK::IsItemActive() && tempSplit != style->user.widgetSplit) {
			tempSplit = style->user.widgetSplit;
		}

		if (FUCK::DragFloat2("Label Align (X/Y)", &style->user.labelAlign.x, 0.01f, 0.0f, 1.0f, "%.2f"))
			style->RefreshStyle();

		FUCK::Spacing();
		FUCK::Header("$FUCK_Styles_Borders"_T);
		if (FUCK::SliderFloat("$FUCK_Styles_WindowBorderSize"_T, &style->user.borderSize, 0.0f, 5.0f))
			style->RefreshStyle();
		if (FUCK::SliderFloat("$FUCK_Styles_ButtonTabBorderSize"_T, &style->user.buttonBorderSize, 0.0f, 5.0f))
			style->RefreshStyle();

		FUCK::Spacing();
		FUCK::Header("Icons");
		if (FUCK::SliderFloat("Icon Scale", &style->user.iconScale, 0.5f, 3.0f, "%.2f"))
			style->RefreshStyle();

		FUCK::Spacing();
		FUCK::Header("$FUCK_Styles_Rounding"_T);
		auto DragRound = [&](const char* label, float* v) {
			if (FUCK::SliderFloat(label, v, 0.0f, 20.0f, "%.0f"))
				style->RefreshStyle();
		};
		DragRound("$FUCK_Styles_WindowRounding"_T, &style->user.windowRounding);
		DragRound("$FUCK_Styles_FrameRounding"_T, &style->user.frameRounding);
		DragRound("$FUCK_Styles_ButtonTabRounding"_T, &style->user.buttonRounding);
		DragRound("$FUCK_Styles_PopupRounding"_T, &style->user.popupRounding);
		DragRound("$FUCK_Styles_ScrollbarRounding"_T, &style->user.scrollbarRounding);
		DragRound("$FUCK_Styles_GrabRounding"_T, &style->user.grabRounding);
	}
}
