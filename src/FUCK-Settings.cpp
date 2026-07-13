#include "FUCK-Settings.h"
#include "FUCK-Man.h"

#include "ImGui/IconsFontAwesome6.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Styles.h"
#include "ImGui/Widgets.h"

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

			if (FUCK::Checkbox("$FUCK_Settings_MuteAudio"_T, &manager->_cfg.muteAudio, true, true))
				manager->Save();

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

			FUCK::Header("$FUCK_Settings_Controller"_T);
			FUCK::Spacing(2);

			FUCK::Header("$FUCK_Settings_Controller"_T);
			FUCK::Spacing(2);

			if (FUCK::Checkbox("$FUCK_Settings_InjectSystemMenu"_T, &manager->_cfg.injectSystemMenu, true, true))
				manager->Save();
			FUCK::SetTooltip("$FUCK_Settings_InjectSystemMenuTT"_T);

			FUCK::BeginDisabled(!manager->_cfg.injectSystemMenu);

			if (manager->GetJournalMenuType() != FUCKMan::JournalMenuType::kSafe) {
				if (FUCK::Checkbox("$FUCK_Settings_ReplaceHelpMenu"_T, &manager->_cfg.replaceHelpMenu, true, true)) {
					if (manager->_cfg.replaceHelpMenu) {
						manager->_cfg.injectSettingsSubmenu = false;
					}
					manager->Save();
				}
				FUCK::SetTooltip("$FUCK_Settings_ReplaceHelpMenuTT"_T);
			}

			if (FUCK::Checkbox("$FUCK_Settings_InjectSettingsSubmenu"_T, &manager->_cfg.injectSettingsSubmenu, true, true)) {
				if (manager->_cfg.injectSettingsSubmenu) {
					manager->_cfg.replaceHelpMenu = false;
				}
				manager->Save();
			}
			FUCK::SetTooltip("$FUCK_Settings_InjectSettingsSubmenuTT"_T);

			FUCK::InputText("$FUCK_Settings_SystemMenuName"_T, &manager->_cfg.customSystemMenuName);
			if (FUCK::IsItemDeactivatedAfterEdit())
				manager->Save();
			FUCK::SetTooltip("$FUCK_Settings_SystemMenuNameTT"_T);
			FUCK::EndDisabled();

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

			FUCK::Spacing(2);

			if (FUCK::Button("$FUCK_Settings_Unmap"_T)) {
				hotkey.Clear();
				manager->SaveKeybinds();
			}
			FUCK::SameLine();
			if (FUCK::Button("$FUCK_Settings_Reset"_T)) {
				manager->ResetSettings();
			}
			FUCK::Spacing(4);

			FUCK::EndTabItem();
		}

		// --------------------------------------------------
		// TAB 2: SIDEBAR
		// --------------------------------------------------
		static bool   s_wasSidebarTabOpen = false;
		static int    s_tableResetCounter = 0;
		static bool   s_needsRebuild      = true;
		static size_t s_lastToolCount     = 0;

		static std::vector<FUCK::ITool*>            looseTools;
		static StringMap<std::vector<FUCK::ITool*>> groupedTools;
		static std::vector<std::string>             groupNames;

		bool isSidebarTabOpen = FUCK::BeginTabItem("$FUCK_Settings_TabSidebar"_T);

		if (isSidebarTabOpen) {
			if (!s_wasSidebarTabOpen) {
				s_tableResetCounter++;
				s_needsRebuild = true;
			}
			s_wasSidebarTabOpen = true;

			FUCK::Header("$FUCK_Sidebar_Title"_T);
			FUCK::Spacing(2);

			if (FUCK::Checkbox("$FUCK_Settings_SidebarOnRight"_T, &manager->_cfg.sidebarOnRight, true, true))
				manager->Save();
			if (FUCK::Checkbox("$FUCK_Sidebar_ShowFilter"_T, &manager->_cfg.showSidebarFilter, true, true))
				manager->Save();
			if (FUCK::Checkbox("$FUCK_Sidebar_ShowFavourites"_T, &manager->_cfg.showSidebarFavourites, true, true))
				manager->Save();

			FUCK::BeginDisabled(!manager->_cfg.showSidebarFavourites);
			if (FUCK::Checkbox("$FUCK_Sidebar_GroupFavourites"_T, &manager->_cfg.groupFavourites, true, true))
				manager->Save();
			FUCK::EndDisabled();

			FUCK::Spacing(4);

			// --- Tools Header with inline Workspace Reset Button ---
			float startX     = FUCK::GetCursorPos().x;
			float availW     = FUCK::GetContentRegionAvail().x;
			float headerTopY = FUCK::GetCursorPos().y;

			FUCK::Header("$FUCK_Tools"_T);

			// Check if layout is modified
			bool isModified = false;
			for (const auto& [k, v] : manager->_toolOverrides) {
				if (!v.customName.empty() || !v.customGroup.empty() || v.sortOrder != 0 || v.isHidden) {
					isModified = true;
					break;
				}
			}
			if (!isModified) {
				for (const auto& [k, v] : manager->_groupOverrides) {
					if (!v.customName.empty() || v.sortOrder != 0) {
						isModified = true;
						break;
					}
				}
			}

			if (isModified) {
				float afterHeaderY = FUCK::GetCursorPos().y;
				float btnW         = FUCK::CalcTextSize("$FUCK_Settings_Reset"_T).x + (ImGui::GetStyle().FramePadding.x * 2.0f);

				FUCK::SetCursorPos({ startX + availW - btnW, headerTopY });
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f));
				if (FUCK::Button("$FUCK_Settings_Reset"_T)) {
					for (auto& [k, v] : manager->_toolOverrides) {
						v.customName  = "";
						v.customGroup = "";
						v.sortOrder   = 0;
						v.isHidden    = false;
					}
					for (auto& [k, v] : manager->_groupOverrides) {
						v.customName = "";
						v.sortOrder  = 0;
					}
					manager->SaveWorkspace();
					s_needsRebuild = true;
				}
				ImGui::PopStyleVar();

				FUCK::SetCursorPosY(afterHeaderY);
			}

			FUCK::Spacing(2);

			// --- Rebuild Active Tool Arrays ---
			if (s_needsRebuild || manager->_tools.size() != s_lastToolCount) {
				s_lastToolCount = manager->_tools.size();
				looseTools.clear();
				groupedTools.clear();
				groupNames.clear();

				for (auto* t : manager->_tools) {
					if (!t->ShowInSidebar())
						continue;

					std::string key  = std::format("{}|{}", t->PluginName(), t->Name());
					auto&       over = manager->_toolOverrides[key];

					std::string grp = over.customGroup;
					if (grp.empty())
						grp = t->Group() ? t->Group() : "";
					if (grp == "##ROOT")
						grp = "";

					if (grp.empty())
						looseTools.push_back(t);
					else
						groupedTools[grp].push_back(t);
				}

				for (const auto& [grp, tools] : groupedTools) {
					groupNames.push_back(grp);
				}

				Map<std::string, int>  groupBaseline;
				Map<FUCK::ITool*, int> toolBaseline;

				auto alphaGroups = groupNames;
				std::sort(alphaGroups.begin(), alphaGroups.end(), [](const std::string& a, const std::string& b) {
					return _stricmp(a.c_str(), b.c_str()) < 0;
				});

				for (size_t i = 0; i < alphaGroups.size(); ++i) {
					groupBaseline[alphaGroups[i]] = static_cast<int>((i + 1) * 10000);
				}

				auto computeToolBaseline = [&](std::vector<FUCK::ITool*>& list, bool registrationOrder) {
					std::vector<FUCK::ITool*> ordered = list;
					if (!registrationOrder) {
						std::sort(ordered.begin(), ordered.end(), [&](FUCK::ITool* a, FUCK::ITool* b) {
							auto&       oa = manager->_toolOverrides[std::format("{}|{}", a->PluginName(), a->Name())];
							auto&       ob = manager->_toolOverrides[std::format("{}|{}", b->PluginName(), b->Name())];
							const char* na = oa.customName.empty() ? a->Name() : oa.customName.c_str();
							const char* nb = ob.customName.empty() ? b->Name() : ob.customName.c_str();
							return _stricmp(na, nb) < 0;
						});
					}
					for (size_t i = 0; i < ordered.size(); ++i) {
						toolBaseline[ordered[i]] = static_cast<int>((i + 1) * 10000);
					}
				};

				for (auto& [grp, tools] : groupedTools) computeToolBaseline(tools, true);
				computeToolBaseline(looseTools, false);

				auto getGroupEffectiveOrder = [&](const std::string& g) {
					auto& over = manager->_groupOverrides[g];
					return over.sortOrder != 0 ? over.sortOrder : groupBaseline[g];
				};

				auto getToolEffectiveOrder = [&](FUCK::ITool* t) {
					auto& over = manager->_toolOverrides[std::format("{}|{}", t->PluginName(), t->Name())];
					return over.sortOrder != 0 ? over.sortOrder : toolBaseline[t];
				};

				std::sort(groupNames.begin(), groupNames.end(), [&](const std::string& a, const std::string& b) {
					int effA = getGroupEffectiveOrder(a);
					int effB = getGroupEffectiveOrder(b);
					if (effA != effB)
						return effA < effB;
					return _stricmp(a.c_str(), b.c_str()) < 0;
				});

				auto sortTools = [&](FUCK::ITool* a, FUCK::ITool* b) {
					int effA = getToolEffectiveOrder(a);
					int effB = getToolEffectiveOrder(b);
					if (effA != effB)
						return effA < effB;
					auto&       oa = manager->_toolOverrides[std::format("{}|{}", a->PluginName(), a->Name())];
					auto&       ob = manager->_toolOverrides[std::format("{}|{}", b->PluginName(), b->Name())];
					const char* na = oa.customName.empty() ? a->Name() : oa.customName.c_str();
					const char* nb = ob.customName.empty() ? b->Name() : ob.customName.c_str();
					return _stricmp(na, nb) < 0;
				};

				for (auto& [grp, tools] : groupedTools) std::sort(tools.begin(), tools.end(), sortTools);
				std::sort(looseTools.begin(), looseTools.end(), sortTools);

				s_needsRebuild = false;
			}

			// --- Setup Reorderable List Table ---
			float tableScale = FUCK::GetGlobalScale();
			FUCK::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding).x, 7.0f * tableScale));

			float tableRightEdge = FUCK::GetCursorScreenPos().x + FUCK::GetContentRegionAvail().x;

			FUCK::PushID(s_tableResetCounter);

			if (FUCK::BeginTable("WorkspaceToolTable", 6, FUCK::TableFlags::kBorders | FUCK::TableFlags::kRowBg | FUCK::TableFlags::kResizable | FUCK::TableFlags::kSizingStretchProp | FUCK::TableFlags::kNoSavedSettings)) {
				// Base Columns
				FUCK::TableSetupColumn("##Hide", FUCK::TableColumnFlags::kWidthFixed | FUCK::TableColumnFlags::kNoSort, 35.0f * tableScale);
				FUCK::TableSetupColumn("$FUCK_Sidebar_TableOrig"_T, FUCK::TableColumnFlags::kWidthStretch | FUCK::TableColumnFlags::kNoSort, 2.0f);
				FUCK::TableSetupColumn("$FUCK_Sidebar_TablePlugin"_T, FUCK::TableColumnFlags::kWidthStretch | FUCK::TableColumnFlags::kNoSort, 1.5f);
				FUCK::TableSetupColumn("$FUCK_Sidebar_TableCustom"_T, FUCK::TableColumnFlags::kWidthStretch | FUCK::TableColumnFlags::kNoSort, 2.0f);
				FUCK::TableSetupColumn("$FUCK_Sidebar_TableGroup"_T, FUCK::TableColumnFlags::kWidthStretch | FUCK::TableColumnFlags::kNoSort, 2.0f);
				FUCK::TableSetupColumn("##Handle", FUCK::TableColumnFlags::kWidthFixed | FUCK::TableColumnFlags::kNoSort, 35.0f * tableScale);

				// --- Header Row ---
				FUCK::TableNextRow(ImGuiTableRowFlags_Headers);

				// Header: Eye Icon (Col 0)
				FUCK::TableNextColumn();
				FUCK::TableSetBgColor(FUCK::TableBgTarget::kCellBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
				ImGui::PushFont(ImGui::GetFont(), ImGui::GetFontSize() * 0.8f);
				float iconEyeW = FUCK::CalcTextSize(ICON_FA_EYE).x;
				float offEyeX  = (FUCK::GetColumnWidth() - iconEyeW) * 0.5f;
				FUCK::SetCursorPosX(FUCK::GetCursorPos().x + std::max(0.0f, offEyeX));
				FUCK::SetCursorPosY(FUCK::GetCursorPos().y + (3.0f * tableScale));

				FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
				FUCK::Text(ICON_FA_EYE);
				FUCK::PopStyleColor();
				ImGui::PopFont();
				FUCK::SetTooltip("$FUCK_Sidebar_TableHideTT"_T);

				// Header: Text Columns
				auto DrawCenterHeader = [](const char* label) {
					FUCK::TableNextColumn();
					FUCK::TableSetBgColor(FUCK::TableBgTarget::kCellBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
					float textW = FUCK::CalcTextSize(label).x;
					FUCK::SetCursorPosX(FUCK::GetCursorPos().x + (FUCK::GetColumnWidth() - textW) * 0.5f);
					FUCK::Text(label);
				};

				DrawCenterHeader("$FUCK_Sidebar_TableOrig"_T);
				DrawCenterHeader("$FUCK_Sidebar_TablePlugin"_T);
				DrawCenterHeader("$FUCK_Sidebar_TableCustom"_T);
				DrawCenterHeader("$FUCK_Sidebar_TableGroup"_T);

				// Header: Hand Icon (Col 5)
				FUCK::TableNextColumn();
				FUCK::TableSetBgColor(FUCK::TableBgTarget::kCellBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));

				// Bypassing FUCK::PushFont scaling
				ImGui::PushFont(ImGui::GetFont(), ImGui::GetFontSize() * 0.8f);

				float iconW = FUCK::CalcTextSize(ICON_FA_HAND).x;
				float offX  = (FUCK::GetColumnWidth() - iconW) * 0.5f;

				FUCK::SetCursorPosX(FUCK::GetCursorPos().x + std::max(0.0f, offX));

				FUCK::SetCursorPosY(FUCK::GetCursorPos().y + (2.0f * tableScale));

				FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
				FUCK::Text(ICON_FA_HAND);
				FUCK::PopStyleColor();

				ImGui::PopFont();
				FUCK::SetTooltip("$FUCK_Sidebar_TableDragTT"_T);

				// --- Table Context Generators ---
				auto RenderToolRow = [&](FUCK::ITool* tool, bool indented, std::vector<FUCK::ITool*>& parentList) {
					std::string key  = std::format("{}|{}", tool->PluginName(), tool->Name());
					auto&       over = manager->_toolOverrides[key];

					FUCK::PushID(key.c_str());
					FUCK::TableNextRow();

					// Col 0: Row Background Selectable (Spans all columns for Drag & Drop) & Hide Button
					FUCK::TableNextColumn();
					ImVec2 startPos = FUCK::GetCursorPos();

					FUCK::Selectable("##Drag", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, FUCK::GetFrameHeight()));
					bool   rowHovered = FUCK::IsItemHovered();
					ImVec2 rowMin     = FUCK::GetItemRectMin();
					ImVec2 rowMax     = FUCK::GetItemRectMax();
					rowMax.x          = tableRightEdge;

					bool isHoveringDrop = false;

					// --- Drag & Drop: Drag Source ---
					if (FUCK::BeginDragDropSource(FUCK::DragDropFlags::kSourceAllowNullID)) {
						FUCK::SetDragDropPayload("TOOL_ORDER", &tool, sizeof(FUCK::ITool*));
						FUCK::Text(std::format("Move {}", tool->Name()).c_str());
						FUCK::EndDragDropSource();
					}

					// --- Drag & Drop: Drop Target (Tool Reordering) ---
					if (FUCK::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = FUCK::AcceptDragDropPayload("TOOL_ORDER", FUCK::DragDropFlags::kAcceptPeekOnly)) {
							isHoveringDrop = true;
						}

						if (const ImGuiPayload* payload = FUCK::AcceptDragDropPayload("TOOL_ORDER", FUCK::DragDropFlags::kAcceptNoDrawDefaultRect)) {
							FUCK::ITool* srcTool = *static_cast<FUCK::ITool**>(payload->Data);

							if (srcTool != tool) {
								std::string srcKey = std::format("{}|{}", srcTool->PluginName(), srcTool->Name());

								std::string dstGrp = over.customGroup;
								if (dstGrp.empty())
									dstGrp = tool->Group() ? tool->Group() : "";
								if (dstGrp == "##ROOT")
									dstGrp = "";

								std::string srcNativeGrp = srcTool->Group() ? srcTool->Group() : "";

								if (dstGrp == srcNativeGrp) {
									manager->_toolOverrides[srcKey].customGroup = "";
								} else if (dstGrp.empty() && !srcNativeGrp.empty()) {
									manager->_toolOverrides[srcKey].customGroup = "##ROOT";
								} else {
									manager->_toolOverrides[srcKey].customGroup = dstGrp;
								}

								// Execute sequential insertion to guarantee deterministic reordering
								std::vector<FUCK::ITool*> newList = parentList;
								std::erase(newList, srcTool);

								auto it = std::find(newList.begin(), newList.end(), tool);
								if (it != newList.end()) {
									newList.insert(it, srcTool);
								} else {
									newList.push_back(srcTool);
								}

								int order = 10000;
								for (auto* t : newList) {
									manager->_toolOverrides[std::format("{}|{}", t->PluginName(), t->Name())].sortOrder = order;
									order += 10000;
								}

								manager->SaveWorkspace();
								s_needsRebuild = true;
							}
						}
						FUCK::EndDragDropTarget();
					}

					// Visual Drop Indicator
					if (isHoveringDrop) {
						ImGui::GetForegroundDrawList()->AddLine(rowMin, ImVec2(rowMax.x, rowMin.y), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
					}

					// Overlap with actual Hide Button logic
					FUCK::SetCursorPos(startPos);
					{
						float       cellH   = FUCK::GetFrameHeight();
						float       cellW   = FUCK::GetColumnWidth();
						const char* icon    = over.isHidden ? ICON_FA_EYE_SLASH : ICON_FA_EYE;
						ImU32       iconCol = over.isHidden ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : ImGui::GetColorU32(ImGuiCol_Text);

						if (FUCK::InvisibleButton("##HideBtn", ImVec2(cellW, cellH))) {
							over.isHidden = !over.isHidden;
							manager->SaveWorkspace();
							s_needsRebuild = true;
						}
						if (FUCK::IsItemHovered()) {
							iconCol = ImGui::GetColorU32(ImGuiCol_Text);
						}

						ImVec2 curPos = FUCK::GetItemRectMin();

						float scaledFontSize = ImGui::GetFontSize() * 0.8f;
						ImGui::PushFont(nullptr, scaledFontSize);
						ImVec2 textSize = FUCK::CalcTextSize(icon);
						ImGui::PopFont();

						// Center mathematically based on the newly scaled text size
						ImVec2 textPos(curPos.x + (cellW - textSize.x) * 0.5f, curPos.y + (cellH - textSize.y) * 0.5f);
						ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), scaledFontSize, textPos, iconCol, icon);
					}

					// Col 1: Original Name
					FUCK::TableNextColumn();
					FUCK::AlignTextToFramePadding();

					if (indented) {
						ImVec2 p     = FUCK::GetCursorScreenPos();
						float  lineX = p.x + 10.0f * tableScale;
						float  lineY = p.y - ImGui::GetStyle().FramePadding.y;
						float  midY  = p.y + FUCK::GetFrameHeight() * 0.5f;

						ImU32 lineCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
						ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, lineY), ImVec2(lineX, midY), lineCol);
						ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, midY), ImVec2(lineX + 10.0f * tableScale, midY), lineCol);

						FUCK::SetCursorPosX(FUCK::GetCursorPos().x + (25.0f * tableScale));
					}

					if (over.isHidden)
						ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
					FUCK::Text(tool->Name());
					if (over.isHidden)
						ImGui::PopStyleColor();

					// Col 2: Plugin Name
					FUCK::TableNextColumn();
					FUCK::AlignTextToFramePadding();
					FUCK::TextDisabled(tool->PluginName());

					// Col 3: Custom Name Override
					FUCK::TableNextColumn();
					FUCK::SetNextItemWidth(-1.0f);
					char nameBuf[64] = "";
					FUCK::StringCopy(nameBuf, over.customName);
					if (FUCK::InputText("##tname", nameBuf, sizeof(nameBuf))) {
						over.customName = nameBuf;
					}
					if (FUCK::IsItemDeactivatedAfterEdit()) {
						manager->SaveWorkspace();
						s_needsRebuild = true;
					}

					// Col 4: Custom Group Override
					FUCK::TableNextColumn();
					FUCK::SetNextItemWidth(-1.0f);
					char grpBuf[64] = "";
					FUCK::StringCopy(grpBuf, over.customGroup == "##ROOT" ? "" : over.customGroup);
					if (FUCK::InputText("##tgrp", grpBuf, sizeof(grpBuf))) {
						if (grpBuf[0] == '\0' && tool->Group() && tool->Group()[0] != '\0') {
							over.customGroup = "##ROOT";
						} else {
							over.customGroup = grpBuf;
						}
					}
					if (FUCK::IsItemDeactivatedAfterEdit()) {
						manager->SaveWorkspace();
						s_needsRebuild = true;
					}

					// Col 5: Drag Handle
					FUCK::TableNextColumn();
					ImU32  gripCol = rowHovered ? IM_COL32(50, 205, 50, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
					float  gripW   = FUCK::CalcTextSize(ICON_FA_GRIP_LINES).x;
					float  gripH   = FUCK::CalcTextSize(ICON_FA_GRIP_LINES).y;
					ImVec2 gripPos(FUCK::GetCursorScreenPos().x + (FUCK::GetColumnWidth() - gripW) * 0.5f, FUCK::GetCursorScreenPos().y + (FUCK::GetFrameHeight() - gripH) * 0.5f);
					ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), gripPos, gripCol, ICON_FA_GRIP_LINES);

					FUCK::PopID();
				};

				auto RenderGroupRow = [&](const std::string& grp) {
					FUCK::PushID(grp.c_str());
					FUCK::TableNextRow();

					// Col 0: Row Background Selectable (Spans all columns for Drag & Drop)
					FUCK::TableNextColumn();
					ImVec2 startPos = FUCK::GetCursorPos();

					FUCK::Selectable("##Drag", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, FUCK::GetFrameHeight()));
					bool   rowHovered = FUCK::IsItemHovered();
					ImVec2 rowMin     = FUCK::GetItemRectMin();
					ImVec2 rowMax     = FUCK::GetItemRectMax();
					rowMax.x          = tableRightEdge;

					bool isHoveringGroupDrop       = false;
					bool isHoveringToolDropOnGroup = false;

					// --- Drag & Drop: Drag Source ---
					if (FUCK::BeginDragDropSource(FUCK::DragDropFlags::kSourceAllowNullID)) {
						FUCK::SetDragDropPayload("GROUP_ORDER", grp.c_str(), grp.size() + 1);
						FUCK::Text(std::format("Move Group: {}", grp).c_str());
						FUCK::EndDragDropSource();
					}

					// --- Drag & Drop: Drop Target (Group Reordering & Tool Assignment) ---
					if (FUCK::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = FUCK::AcceptDragDropPayload("GROUP_ORDER", FUCK::DragDropFlags::kAcceptPeekOnly)) {
							isHoveringGroupDrop = true;
						}

						if (const ImGuiPayload* payload = FUCK::AcceptDragDropPayload("GROUP_ORDER", FUCK::DragDropFlags::kAcceptNoDrawDefaultRect)) {
							std::string srcGrp = static_cast<const char*>(payload->Data);

							if (srcGrp != grp) {
								std::vector<std::string> newGroups = groupNames;
								std::erase(newGroups, srcGrp);

								auto it = std::find(newGroups.begin(), newGroups.end(), grp);
								if (it != newGroups.end()) {
									newGroups.insert(it, srcGrp);
								} else {
									newGroups.push_back(srcGrp);
								}

								int order = 10000;
								for (const auto& g : newGroups) {
									manager->_groupOverrides[g].sortOrder = order;
									order += 10000;
								}

								manager->SaveWorkspace();
								s_needsRebuild = true;
							}
						}

						if (const ImGuiPayload* payload = FUCK::AcceptDragDropPayload("TOOL_ORDER", FUCK::DragDropFlags::kAcceptPeekOnly)) {
							isHoveringToolDropOnGroup = true;
						}

						if (const ImGuiPayload* payload = FUCK::AcceptDragDropPayload("TOOL_ORDER", FUCK::DragDropFlags::kAcceptNoDrawDefaultRect)) {
							FUCK::ITool* srcTool      = *static_cast<FUCK::ITool**>(payload->Data);
							std::string  srcKey       = std::format("{}|{}", srcTool->PluginName(), srcTool->Name());
							std::string  srcNativeGrp = srcTool->Group() ? srcTool->Group() : "";

							if (grp == srcNativeGrp) {
								manager->_toolOverrides[srcKey].customGroup = "";
							} else if (grp.empty() && !srcNativeGrp.empty()) {
								manager->_toolOverrides[srcKey].customGroup = "##ROOT";
							} else {
								manager->_toolOverrides[srcKey].customGroup = grp;
							}

							// Sequentially place the tool at the top of the destination group
							std::vector<FUCK::ITool*> newList = groupedTools[grp];
							std::erase(newList, srcTool);
							newList.insert(newList.begin(), srcTool);

							int order = 10000;
							for (auto* t : newList) {
								manager->_toolOverrides[std::format("{}|{}", t->PluginName(), t->Name())].sortOrder = order;
								order += 10000;
							}

							manager->SaveWorkspace();
							s_needsRebuild = true;
						}
						FUCK::EndDragDropTarget();
					}

					// Visual Drop Indicator
					if (isHoveringGroupDrop) {
						ImGui::GetForegroundDrawList()->AddLine(rowMin, ImVec2(rowMax.x, rowMin.y), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
					} else if (isHoveringToolDropOnGroup) {
						ImGui::GetForegroundDrawList()->AddRect(rowMin, rowMax, ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, 2.0f);
					}

					// Col 1: Group Name & Collapse Indicator
					FUCK::TableNextColumn();
					FUCK::AlignTextToFramePadding();

					ImVec2 curPos    = FUCK::GetCursorScreenPos();
					float  rowH      = FUCK::GetFrameHeight();
					auto   iconArrow = IconFont::Manager::GetSingleton()->GetStepperRight();
					if (iconArrow) {
						float  aspect  = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;
						auto   ap      = ImGui::CalcArrowIconParams(aspect, true, rowH, 20.0f * tableScale, 1.0f);
						ImVec2 drawPos = { curPos.x, curPos.y + ap.offsetY };
						ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, ImGui::GetColorU32(ImGuiCol_TextDisabled), ImGui::IconDirection::kDown);
						FUCK::SetCursorPosX(FUCK::GetCursorPos().x + ap.drawSize.x + (6.0f * tableScale));
					}

					FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
					FUCK::Text(grp.c_str());
					FUCK::PopStyleColor();

					// Col 2: Blank (no plugin name for groups)
					FUCK::TableNextColumn();

					// Col 3: Custom Name
					FUCK::TableNextColumn();
					FUCK::SetNextItemWidth(-1.0f);
					auto& gOver       = manager->_groupOverrides[grp];
					char  nameBuf[64] = "";
					FUCK::StringCopy(nameBuf, gOver.customName);
					if (FUCK::InputText("##gname", nameBuf, sizeof(nameBuf))) {
						gOver.customName = nameBuf;
					}
					if (FUCK::IsItemDeactivatedAfterEdit()) {
						manager->SaveWorkspace();
						s_needsRebuild = true;
					}

					// Col 4: Blank (groups cannot override their own group)
					FUCK::TableNextColumn();

					// Col 5: Drag Handle
					FUCK::TableNextColumn();
					ImU32  gripCol = rowHovered ? IM_COL32(50, 205, 50, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
					float  gripW   = FUCK::CalcTextSize(ICON_FA_GRIP_LINES).x;
					float  gripH   = FUCK::CalcTextSize(ICON_FA_GRIP_LINES).y;
					ImVec2 gripPos(FUCK::GetCursorScreenPos().x + (FUCK::GetColumnWidth() - gripW) * 0.5f, FUCK::GetCursorScreenPos().y + (FUCK::GetFrameHeight() - gripH) * 0.5f);
					ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), gripPos, gripCol, ICON_FA_GRIP_LINES);

					FUCK::PopID();

					for (auto* tool : groupedTools[grp]) {
						RenderToolRow(tool, true, groupedTools[grp]);
					}
				};

				// --- Render Active Rows ---
				for (const auto& grp : groupNames) {
					RenderGroupRow(grp);
				}

				for (auto* tool : looseTools) {
					RenderToolRow(tool, false, looseTools);
				}

				FUCK::EndTable();
				FUCK::PopID();
			}

			FUCK::PopStyleVar();

			FUCK::Spacing(4);
			FUCK::EndTabItem();
		} else {
			s_wasSidebarTabOpen = false;
		}

		// --------------------------------------------------
		// TAB 3: STYLES
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
		// TAB 4: INFO
		// --------------------------------------------------
		static bool s_wasInfoTabOpen = false;
		static bool s_infoCached     = false;

		struct PluginDetails
		{
			std::vector<std::string> tools;
			std::vector<std::string> windows;
		};
		static StringMap<PluginDetails>                         pluginMap;
		static std::vector<std::pair<std::string, std::string>> iniList;

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

			// Show API version and Root Config Path
			FUCK::TextDisabled("$FUCK_Info_APIVersion"_T, FUCK_API_VERSION);
			FUCK::TextDisabled("$FUCK_Info_ConfigDir"_T, R"(Data\FUCKs\)");
			FUCK::Spacing(4);

			static std::vector<std::string> sortedPluginNames;

			if (!s_infoCached) {
				pluginMap.clear();
				iniList.clear();
				sortedPluginNames.clear();

				for (auto* tool : manager->_tools) {
					if (!string::is_empty(tool->PluginName())) {
						pluginMap[tool->PluginName()].tools.push_back(tool->Name());
					}
				}
				for (auto* win : manager->_windows) {
					if (!string::is_empty(win->PluginName())) {
						pluginMap[win->PluginName()].windows.push_back(win->Id());
					}
				}

				for (auto& [pluginName, details] : pluginMap) {
					std::sort(details.tools.begin(), details.tools.end(),
						[](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });
					std::sort(details.windows.begin(), details.windows.end(),
						[](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });

					sortedPluginNames.push_back(pluginName);
				}

				std::sort(sortedPluginNames.begin(), sortedPluginNames.end(),
					[](const std::string& a, const std::string& b) { return _stricmp(a.c_str(), b.c_str()) < 0; });

				{
					std::lock_guard<std::mutex> iniLock(Settings::GetSingleton()->trackingMutex);
					for (const auto& iniPath : Settings::GetSingleton()->trackedINIs) {
						fs::path    p(iniPath);
						std::string filename  = p.filename().string();
						std::string parentDir = p.parent_path().filename().string();

						if (string::icontains(filename, "SSEDisplayTweaks"))
							continue;

						// Add separated to the list so we can sort properly by parent
						iniList.push_back({ parentDir, filename });
					}
					// Sort specifically by parentDir (plugin) first, then filename
					std::sort(iniList.begin(), iniList.end(),
						[](const auto& a, const auto& b) {
							int cmp = _stricmp(a.first.c_str(), b.first.c_str());
							if (cmp != 0)
								return cmp < 0;
							return _stricmp(a.second.c_str(), b.second.c_str()) < 0;
						});
				}
				s_infoCached = true;
			}

			// --- Helper Lambda: Draw a tagged entry row (tool or window) ---
			auto DrawEntry = [&](const std::string& name, const char* tag, float tagW, float maxTagW, const ImVec4& color) {
				float startX = FUCK::GetCursorPos().x;
				FUCK::SetCursorPosX(startX + (maxTagW - tagW));
				FUCK::TextColored(color, "%s", tag);
				FUCK::SameLine();
				FUCK::TextUnformatted(name.c_str());
			};

			// --- Helper Lambda: Print a simple vertical list (Used for Settings Files) ---
			auto PrintListInPanel = [&](std::vector<std::pair<std::string, std::string>>& items, const char* panelId) {
				if (items.empty()) {
					FUCK::TextDisabled("$FUCK_Info_NoneFound"_T);
				} else {
					float childHeight = FUCK::GetContentRegionAvail().y;
					FUCK::PushStyleColor(ImGuiCol_ChildBg, FUCK::GetStyleColorVec4(ImGuiCol_FrameBg));
					FUCK::BeginChild(panelId, ImVec2(0, childHeight), true, 0);

					if (FUCK::BeginTable(panelId, 1, FUCK::TableFlags::kNone)) {
						FUCK::TableSetupColumn("", FUCK::TableColumnFlags::kWidthStretch);
						for (const auto& item : items) {
							FUCK::TableNextRow();
							FUCK::TableNextColumn();
							FUCK::SetCursorPosX(FUCK::GetCursorPos().x + (5.0f * FUCK::GetGlobalScale()));
							std::string label = item.first.empty() ? item.second : item.first + "\\" + item.second;
							FUCK::TextUnformatted(label.c_str());
						}
						FUCK::EndTable();
					}

					FUCK::EndChild();
					FUCK::PopStyleColor();
				}
			};

			// --- 2-COLUMN SIDE-BY-SIDE LAYOUT ---
			if (FUCK::BeginTable("InfoLayoutTable", 2, FUCK::TableFlags::kNone)) {
				// Column 1: Plugins Breakdown
				FUCK::TableNextColumn();
				FUCK::Header("$FUCK_Info_RegisteredMods"_T);
				FUCK::Spacing(2);

				if (pluginMap.empty()) {
					FUCK::TextDisabled("$FUCK_Info_NoneFound"_T);
				} else {
					float childHeight = FUCK::GetContentRegionAvail().y;
					FUCK::PushStyleColor(ImGuiCol_ChildBg, FUCK::GetStyleColorVec4(ImGuiCol_FrameBg));

					FUCK::BeginChild("PluginsPanelChild", ImVec2(0, childHeight), true, 0);

					const char* toolTag  = "$FUCK_Info_Tool"_T;
					const char* winTag   = "$FUCK_Info_Window"_T;
					float       toolTagW = FUCK::CalcTextSize(toolTag).x;
					float       winTagW  = FUCK::CalcTextSize(winTag).x;
					float       maxTagW  = std::max(toolTagW, winTagW);

					for (const auto& plugin : sortedPluginNames) {
						const auto& details = pluginMap.at(plugin);
						FUCK::PushID(plugin.c_str());
						if (FUCK::TreeNode(plugin.c_str())) {
							for (const auto& tool : details.tools) {
								DrawEntry(tool, toolTag, toolTagW, maxTagW, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
							}
							for (const auto& win : details.windows) {
								DrawEntry(win, winTag, winTagW, maxTagW, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
							}
							FUCK::TreePop();
						}
						FUCK::PopID();
					}

					FUCK::EndChild();
					FUCK::PopStyleColor();
				}

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
