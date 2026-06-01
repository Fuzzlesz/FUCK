#include "FUCKSettings.h"
#include "FUCKMan.h"

#include "ImGui/IconsFonts.h"
#include "ImGui/Styles.h"
#include "System/Hotkeys.h"
#include "System/Settings.h"
#include "System/Translation.h"
#include "System/Utils.h"

const char* SettingsTool::Name() const
{
	return "$FUCK_Settings_Title"_T;
}

bool SettingsTool::OnAsyncInput(const void* a_event)
{
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();
	if (FUCK::UpdateManagedHotkey(a_event, hotkey)) {
		FUCKMan::GetSingleton()->SaveKeybinds();
		return true;
	}
	return false;
}

void SettingsTool::OnClose()
{
	auto& hotkey = MANAGER(Hotkeys)->GetToggleHotkey();
	FUCK::AbortManagedHotkey(hotkey);
}

void SettingsTool::Draw()
{
	auto  manager = FUCKMan::GetSingleton();
	auto  style   = ImGui::Styles::GetSingleton();
	auto& hotkey  = MANAGER(Hotkeys)->GetToggleHotkey();

	if (FUCK::BeginTabBar("SettingsTabs")) {
		// --------------------------------------------------
		// TAB 1: GENERAL
		// --------------------------------------------------
		if (FUCK::BeginTabItem("$FUCK_Settings_Tab"_T)) {

			FUCK::Header("$FUCK_Settings_Behaviour"_T);
			FUCK::Spacing(2);

			FUCK::DrawManagedHotkey("$FUCK_Settings_Hotkey"_T, hotkey);
			FUCK::Spacing(2);

			if (FUCK::Checkbox("$FUCK_Settings_InjectSystemMenu"_T, &manager->_cfg.injectSystemMenu, true, true))
				manager->Save();
			FUCK::SetTooltip("$FUCK_Settings_InjectSystemMenuTT"_T);

			FUCK::BeginDisabled(!manager->_cfg.injectSystemMenu);
			if (FUCK::Checkbox("$FUCK_Settings_ReplaceHelpMenu"_T, &manager->_cfg.replaceHelpMenu, true, true))
				manager->Save();
			FUCK::SetTooltip("$FUCK_Settings_ReplaceHelpMenuTT"_T);
			FUCK::EndDisabled();

			FUCK::Spacing(2);

			const char* pauseTypes[]    = { "$FUCK_Settings_PauseNone"_T, "$FUCK_Settings_PauseSoft"_T, "$FUCK_Settings_PauseHard"_T };
			int         currentPauseIdx = static_cast<int>(manager->_cfg.globalPauseType);
			FUCK::SetNextItemWidth(-1);
			std::string pauseLabel = std::format("{}##GlobalPauseType", "$FUCK_Settings_GlobalPause"_T);

			if (FUCK::Combo(pauseLabel.c_str(), &currentPauseIdx, pauseTypes, IM_ARRAYSIZE(pauseTypes))) {
				manager->_cfg.globalPauseType = static_cast<FUCKMan::PauseType>(currentPauseIdx);
				manager->Save();
			}
			FUCK::Spacing();
			FUCK::TextColoredWrapped(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "$FUCK_Settings_PauseDesc"_T);

			FUCK::Header("$FUCK_Settings_Appearance"_T);
			FUCK::Spacing(2);

			static std::vector<std::string> fonts = style->GetAvailableFonts();
			std::vector<std::string>        displayFonts;
			std::vector<const char*>        fontPtrs;
			displayFonts.reserve(fonts.size());
			fontPtrs.reserve(fonts.size());

			int currentFontIdx = -1;

			for (size_t i = 0; i < fonts.size(); ++i) {
				std::string disp = (fonts[i] == "Default") ? std::string("$FUCK_Styles_FontDefault"_T) : fonts[i];
				displayFonts.push_back(disp);
				fontPtrs.push_back(displayFonts.back().c_str());

				if (fonts[i] == manager->_cfg.currentFont) {
					currentFontIdx = static_cast<int>(i);
				}
			}

			FUCK::SetNextItemWidth(-1);
			if (FUCK::Combo("$FUCK_Styles_Typeface"_T, &currentFontIdx, fontPtrs.data(), (int)fontPtrs.size())) {
				if (currentFontIdx >= 0 && currentFontIdx < fonts.size()) {
					manager->SetCurrentFont(fonts[currentFontIdx]);

					Settings::GetSingleton()->Save(FileType::kStyle, [&](CSimpleIniA& ini) {
						ini.SetValue("Style", "sFont", manager->_cfg.currentFont.c_str());
					});
				}
			}
			FUCK::Spacing(2);

			if (FUCK::SliderFloat("$FUCK_Settings_UIScale"_T, &manager->_cfg.userScale, 0.5f, 2.0f, "%.2f")) {
				style->RefreshStyle();
			}
			if (FUCK::IsItemDeactivatedAfterEdit())
				manager->Save();

			if (FUCK::Checkbox("$FUCK_Settings_SidebarOnRight"_T, &manager->_cfg.sidebarOnRight, true, true))
				manager->Save();
			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Settings_Reset"_T)) {
				manager->ResetSettings();
			}
			FUCK::Spacing(4);

			FUCK::EndTabItem();
		}

		// --------------------------------------------------
		// TAB 2: STYLES
		// --------------------------------------------------------
		// --------------------------------------------------
		if (FUCK::BeginTabItem("$FUCK_Styles_Title"_T)) {

			FUCK::Header("$FUCK_Styles_Presets"_T);
			FUCK::Spacing();

			const auto& presets           = style->GetPresets();
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

			if (FUCK::Combo("$FUCK_Styles_SelectPreset"_T, &currentIdx, comboItems.data(), static_cast<int>(comboItems.size()))) {
				if (currentIdx == 0) {
					// User selected "----" -> Reset
					style->ResetToDefaults();
				} else if (currentIdx > 0 && currentIdx <= static_cast<int>(presets.size())) {
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

			if (FUCK::Button("$FUCK_Settings_Reset"_T)) {
				style->ResetToDefaults();
			}

			FUCK::EndTabItem();
		}

		// --------------------------------------------------
		// TAB 3: INFO
		// --------------------------------------------------
		static bool                     s_wasInfoTabOpen = false;
		static bool                     s_infoCached     = false;
		static std::vector<std::string> pluginsList;
		static std::vector<std::string> iniList;
		static std::vector<std::string> transList;

		bool isInfoTabOpen = FUCK::BeginTabItem("$FUCK_Settings_TabInfo"_T);

		if (isInfoTabOpen) {
			if (!s_wasInfoTabOpen) {
				s_infoCached = false;
			}
			s_wasInfoTabOpen = true;

			FUCK::Header("$FUCK_Settings_Info"_T);
			FUCK::Spacing(2);

			FUCK::PushStyleColor(ImGuiCol_Text, FUCK::GetStyleColorVec4(ImGuiCol_CheckMark));
			FUCK::TextWrapped("$FUCK_Settings_Desc"_T);
			FUCK::PopStyleColor();
			FUCK::Spacing(2);
			FUCK::TextDisabled("FUCK API Version: %d", FUCK_API_VERSION);
			FUCK::Spacing(4);

			// --- ON-DEMAND SCAN CACHING ---
			if (!s_infoCached) {
				pluginsList.clear();
				iniList.clear();

				// 1. Gather DLLs via the VTable Poly Pointer Trick
				std::set<std::string> consumerDLLs;
				auto                  extractDLL = [&](void* polyPtr) {
                    if (!polyPtr)
                        return;
                    void*   vtableAddr = *reinterpret_cast<void**>(polyPtr);
                    HMODULE hModule    = nullptr;

                    if (::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
											 static_cast<LPCSTR>(vtableAddr), &hModule)) {
                        char path[MAX_PATH];
                        if (::GetModuleFileNameA(hModule, path, MAX_PATH)) {
                            std::string filename = std::filesystem::path(path).filename().string();
                            if (!Utils::IContains(filename, "FUCK.dll")) {
                                consumerDLLs.insert(filename);
                            }
                        }
                    }
				};

				for (auto* tool : manager->_tools) {
					extractDLL(dynamic_cast<void*>(tool));
				}
				for (auto* win : manager->_windows) {
					extractDLL(dynamic_cast<void*>(win));
				}
				pluginsList.assign(consumerDLLs.begin(), consumerDLLs.end());

				// 2. Gather INIs — single list, labelled as "PluginName/filename"
				{
					std::lock_guard<std::mutex> iniLock(Settings::GetSingleton()->trackingMutex);
					for (const auto& iniPath : Settings::GetSingleton()->trackedINIs) {
						namespace fs = std::filesystem;
						fs::path    p(iniPath);
						std::string filename  = p.filename().string();
						std::string parentDir = p.parent_path().filename().string();

						if (Utils::IContains(filename, "SSEDisplayTweaks"))
							continue;

						std::string label = parentDir.empty() ? filename : parentDir + "/" + filename;
						iniList.push_back(label);
					}
				}
				s_infoCached = true;
			}

			// Helper Lambda: Print a sorted list inside a dynamic scrolling panel
			auto PrintListInPanel = [&](std::vector<std::string>& items, const char* panelId) {
				if (items.empty()) {
					FUCK::TextDisabled("$FUCK_Info_NoneFound"_T);
				} else {
					// Case-Insensitive Alphabetical Sort
					std::sort(items.begin(), items.end(), [](const std::string& a, const std::string& b) {
						return _stricmp(a.c_str(), b.c_str()) < 0;
					});

					// Calculate how many columns we can fit inside this specific panel
					float availWidth  = FUCK::GetContentRegionAvail().x;
					float minColWidth = 280.0f * FUCK::GetGlobalScale();
					int   numCols     = std::max(1, static_cast<int>(availWidth / minColWidth));

					// Let the panel stretch to the bottom of the window
					float childHeight = FUCK::GetContentRegionAvail().y;

					// Draw an inset scrolling panel
					FUCK::PushStyleColor(ImGuiCol_ChildBg, FUCK::GetStyleColorVec4(ImGuiCol_FrameBg));
					FUCK::BeginChild(panelId, ImVec2(0, childHeight), true, 0);

					if (FUCK::BeginTable(panelId, numCols, FUCK::TableFlags::kNone)) {
						for (const auto& item : items) {
							FUCK::TableNextColumn();
							FUCK::Text("  - %s", item.c_str());
						}
						FUCK::EndTable();
					}

					FUCK::EndChild();
					FUCK::PopStyleColor();
				}
			};

			// --- 2-COLUMN SIDE-BY-SIDE LAYOUT ---
			if (FUCK::BeginTable("InfoLayoutTable", 2, FUCK::TableFlags::kNone)) {
				// Column 1: Plugins
				FUCK::TableNextColumn();
				FUCK::Header("$FUCK_Info_RegisteredMods"_T);
				FUCK::Spacing(2);
				PrintListInPanel(pluginsList, "PluginsPanel");

				// Column 2: INIs
				FUCK::TableNextColumn();
				FUCK::Header("$FUCK_Info_SettingsFiles"_T);
				FUCK::Spacing(2);
				PrintListInPanel(iniList, "InisPanel");

				FUCK::EndTable();
			}

			FUCK::EndTabItem();
		} else {
			s_wasInfoTabOpen = false;
		}

		FUCK::EndTabBar();
	}
}
