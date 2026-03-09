#include "FUCKSettings.h"
#include "FUCKMan.h"

#include "ImGui/IconsFonts.h"
#include "ImGui/Styles.h"
#include "System/Hotkeys.h"

const char* SettingsTool::Name() const
{
	return "$FUCK_Settings_Title"_T;
}

bool SettingsTool::OnAsyncInput(const void* a_event)
{
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();
	return FUCK::UpdateManagedHotkey(a_event, hotkey);
}

void SettingsTool::OnClose()
{
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();
	FUCK::AbortManagedHotkey(hotkey);
}

void SettingsTool::Draw()
{
	auto manager = FUCKMan::GetSingleton();
	auto style = ImGui::Styles::GetSingleton();
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();

	if (FUCK::BeginTabBar("SettingsTabs")) {
		// --------------------------------------------------------
		// TAB 1: GENERAL
		// --------------------------------------------------------
		if (FUCK::BeginTabItem("$FUCK_Settings_Tab"_T)) {
			FUCK::Spacing(2);

			FUCK::Header("$FUCK_Settings_Behavior"_T);
			FUCK::Spacing(2);

			const char* pauseTypes[] = { "$FUCK_Settings_PauseNone"_T, "$FUCK_Settings_PauseSoft"_T, "$FUCK_Settings_PauseHard"_T };
			int currentPauseIdx = (int)manager->_globalPauseType;
			FUCK::SetNextItemWidth(-1);
			std::string pauseLabel = std::format("{}##GlobalPauseType", "$FUCK_Settings_GlobalPause"_T);

			if (FUCK::Combo(pauseLabel.c_str(), &currentPauseIdx, pauseTypes, IM_ARRAYSIZE(pauseTypes))) {
				manager->_globalPauseType = (FUCKMan::PauseType)currentPauseIdx;
			}
			FUCK::Spacing();
			FUCK::TextColoredWrapped(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "$FUCK_Settings_PauseDesc"_T);
			FUCK::Spacing(2);

			FUCK::DrawManagedHotkey("$FUCK_Settings_Hotkey"_T, hotkey);

			FUCK::Spacing(2);

			FUCK::Header("$FUCK_Settings_Appearance"_T);
			FUCK::Spacing(2);

			if (FUCK::SliderFloat("$FUCK_Settings_UIScale"_T, &manager->_userScale, 0.5f, 2.0f, "%.2f")) {
				style->RefreshStyle();
				MANAGER(IconFont)->ReloadFonts();
			}

			FUCK::Checkbox("$FUCK_Settings_SidebarOnRight"_T, &manager->_sidebarOnRight, true, true);
			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Settings_Reset"_T)) {
				manager->ResetSettings();
			}
			FUCK::Spacing(4);

			FUCK::Header("$FUCK_Settings_Info"_T);
			FUCK::Spacing(2);

			FUCK::PushStyleColor(ImGuiCol_Text, FUCK::GetStyleColorVec4(ImGuiCol_CheckMark));
			FUCK::TextWrapped("$FUCK_Settings_Desc"_T);
			FUCK::PopStyleColor();
			FUCK::Spacing(2);
			FUCK::TextDisabled("FUCK API Version: %d", FUCK_API_VERSION);

			FUCK::EndTabItem();
		}

		// --------------------------------------------------------
		// TAB 2: STYLES
		// --------------------------------------------------------
		if (FUCK::BeginTabItem("$FUCK_Styles_Title"_T)) {
			FUCK::Spacing(2);

			FUCK::Header("$FUCK_Styles_Presets"_T);
			FUCK::Spacing();

			const auto& presets = style->GetPresets();
			std::string currentPresetName = style->GetCurrentPresetName();

			// Build Combo Items with "----" at the top
			std::vector<const char*> comboItems;
			comboItems.reserve(presets.size() + 1);
			comboItems.push_back("----");

			int currentIdx = 0;

			for (size_t i = 0; i < presets.size(); ++i) {
				comboItems.push_back(presets[i].c_str());
				if (presets[i] == currentPresetName) {
					currentIdx = static_cast<int>(i) + 1;
				}
			}

			if (FUCK::Combo("$FUCK_Styles_SelectPreset"_T, &currentIdx, comboItems.data(), (int)comboItems.size())) {
				if (currentIdx == 0) {
					// User selected "----" -> Reset
					style->ResetToDefaults();
				} else if (currentIdx > 0 && currentIdx <= presets.size()) {
					// User selected a file
					style->LoadPreset(presets[currentIdx - 1]);
				}
			}

			FUCK::Spacing(2);
			FUCK::TextDisabled("$FUCK_Styles_EditorDesc"_T);
			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Styles_OpenThemeEditor"_T)) {
				FUCKMan::GetSingleton()->_themeEditorWindow.SetOpen(true);
			}

			FUCK::Spacing(2);
			FUCK::SeparatorThick();
			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Styles_ResetDefaults"_T)) {
				style->ResetToDefaults();
			}

			FUCK::EndTabItem();
		}

		FUCK::EndTabBar();
	}
}
