#include "FUCK-Settings.h"
#include "FUCK-Man.h"

#include "ImGui/IconsFontAwesome6.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

#include "System/Hotkeys.h"
#include "System/Settings.h"
#include "System/Translation.h"
#include "System/Utils.h"

#include <functional>
#include <unordered_set>

// ==================================================
// Sidebar Table
// ==================================================

namespace
{
	// Case-insensitive search, for the table's filter. a_lowerNeedle is already lower case
	bool ContainsNoCase(std::string_view a_text, std::string_view a_lowerNeedle)
	{
		if (a_lowerNeedle.empty())
			return true;
		auto it = std::search(a_text.begin(), a_text.end(), a_lowerNeedle.begin(), a_lowerNeedle.end(), [](char c, char n) {
			return static_cast<char>(std::tolower(static_cast<unsigned char>(c))) == n;
		});
		return it != a_text.end();
	}

	// One row of the table, flattened from the tool layout
	struct TableRow
	{
		FUCK::ITool* tool = nullptr;  // null for a group
		std::string  path;            // a group's own path, or the group a tool is in
		std::string  key;             // names the row: the tool's override key, or "##grp|" + the group's path
		std::string  name;            // the tool's own name, or the group's last level
		std::string  plugin;
		int          depth     = 0;
		bool         collapsed = false;
	};

	// Everything the table keeps between frames. The rows are rebuilt only when the layout, the filter, or
	// which groups are collapsed changes, and only the rows on screen are drawn
	struct TableState
	{
		std::vector<TableRow>           rows;
		std::vector<std::string>        groupPaths;  // every group in display order, for "Move to Group"
		std::unordered_set<std::string> collapsed;   // for this session
		char                            filter[128]  = "";
		std::string                     builtFilter;
		std::uint32_t                   builtVersion = 0;
		bool                            dirty        = true;
		bool                            isModified   = false;
		std::size_t                     shownTools   = 0;
		std::size_t                     totalTools   = 0;

		// Click to edit: one cell at a time
		std::string editKey;     // the row's key + "|name" or "|group"
		std::string editRowKey;  // the row's key
		int         editRow      = -1;
		int         editWait     = 0;  // frames left for the box to take focus
		char        editBuf[128] = "";

		// The row being dragged, kept drawn while it's scrolled out of view so the drag carries on
		std::string dragRowKey;
		int         dragRow = -1;

		// The right-click menu
		FUCK::ITool* menuTool = nullptr;
		std::string  menuGroup;
		bool         menuRequested = false;

		int FindRow(const std::string& a_key) const
		{
			if (a_key.empty())
				return -1;
			for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
				if (rows[i].key == a_key)
					return i;
			}
			return -1;
		}

		void CancelEdit()
		{
			editKey.clear();
			editRowKey.clear();
			editRow  = -1;
			editWait = 0;
		}
	};

	TableState s_table;
}

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

			FUCK::Header("$FUCK_Settings_SystemMenu"_T);
			FUCK::Spacing(2);

			const char* injectTypes[]    = { "$FUCK_Settings_InjectYes"_T, "$FUCK_Settings_InjectControllerOnly"_T, "$FUCK_Settings_InjectNo"_T };
			int         currentInjectIdx = static_cast<int>(manager->_cfg.injectSystemMenu);
			FUCK::SetNextItemWidth(-1);
			std::string injectLabel = std::format("{}##InjectSystemMenu", "$FUCK_Settings_InjectSystemMenu"_T);

			if (FUCK::Combo(injectLabel.c_str(), &currentInjectIdx, injectTypes, IM_ARRAYSIZE(injectTypes))) {
				manager->_cfg.injectSystemMenu = static_cast<FUCKMan::InjectSystemMenuType>(currentInjectIdx);
				manager->Save();
			}
			FUCK::SetTooltip("$FUCK_Settings_InjectSystemMenuTT"_T);

			FUCK::BeginDisabled(manager->_cfg.injectSystemMenu == FUCKMan::InjectSystemMenuType::kNo);

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
		static bool s_wasSidebarTabOpen = false;
		static int  s_tableResetCounter = 0;

		bool isSidebarTabOpen = FUCK::BeginTabItem("$FUCK_Settings_TabSidebar"_T);

		if (isSidebarTabOpen) {
			auto& T = s_table;

			if (!s_wasSidebarTabOpen) {
				s_tableResetCounter++;
				T.dirty = true;
				T.CancelEdit();
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

			const auto& layout = manager->GetToolLayout();

			// --- Rebuild the rows, only when something they show has changed ---
			if (T.dirty || layout.version != T.builtVersion || T.builtFilter != T.filter) {
				T.dirty        = false;
				T.builtVersion = layout.version;
				T.builtFilter  = T.filter;
				T.rows.clear();
				T.groupPaths.clear();
				T.shownTools = 0;
				T.totalTools = layout.keys.size();

				std::string needle = T.filter;
				std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				const bool filtering = !needle.empty();

				auto toolMatches = [&](FUCK::ITool* t) {
					if (ContainsNoCase(t->Name(), needle) || ContainsNoCase(t->PluginName(), needle))
						return true;
					if (auto it = layout.labels.find(t); it != layout.labels.end() && ContainsNoCase(it->second, needle))
						return true;
					if (auto it = layout.groups.find(t); it != layout.groups.end() && ContainsNoCase(it->second, needle))
						return true;
					return false;
				};

				auto groupMatches = [&](const std::string& path) {
					return ContainsNoCase(path, needle) || ContainsNoCase(manager->GetGroupDisplayName(path), needle);
				};

				// Adds a level's groups and tools, in the layout's order. While filtering, a group shows when it or anything
				// in it matches, and a group that matches shows everything in it. Returns whether anything was added
				std::function<bool(const std::string&, int, bool)> addLevel = [&](const std::string& path, int depth, bool showAll) -> bool {
					auto it = layout.tree.find(path);
					if (it == layout.tree.end())
						return false;

					bool added = false;
					for (const auto& e : it->second.entries) {
						if (e.IsGroup()) {
							const std::string& g         = e.group;
							const std::size_t  at        = T.rows.size();
							const bool         matchAll  = showAll || (filtering && groupMatches(g));
							const bool         collapsed = !filtering && T.collapsed.contains(g);

							T.rows.push_back({ nullptr, g, "##grp|" + g, Sidebar::GetGroupLeaf(g), std::string(), depth, collapsed });

							const bool hasContent = collapsed || addLevel(g, depth + 1, matchAll);
							if (!hasContent && !matchAll) {
								T.rows.resize(at);
								continue;
							}
							added = true;
						} else {
							FUCK::ITool* t = e.tool;
							if (filtering && !showAll && !toolMatches(t))
								continue;

							auto key = layout.keys.find(t);
							T.rows.push_back({ t, path, key != layout.keys.end() ? key->second : std::string(), t->Name(), t->PluginName(), depth });
							++T.shownTools;
							added = true;
						}
					}
					return added;
				};

				addLevel(std::string(), 0, false);

				std::function<void(const std::string&)> collectGroups = [&](const std::string& path) {
					auto it = layout.tree.find(path);
					if (it == layout.tree.end())
						return;
					for (const auto& g : it->second.groups) {
						T.groupPaths.push_back(g);
						collectGroups(g);
					}
				};
				collectGroups(std::string());

				T.isModified = !manager->_groupMoves.empty();
				for (const auto& [k, v] : manager->_toolOverrides) {
					if (!v.customName.empty() || !v.customGroup.empty() || v.sortOrder != 0 || v.isHidden) {
						T.isModified = true;
						break;
					}
				}
				if (!T.isModified) {
					for (const auto& [k, v] : manager->_groupOverrides) {
						if (!v.customName.empty() || v.sortOrder != 0) {
							T.isModified = true;
							break;
						}
					}
				}

				// Rows move about on a rebuild, so find the edited and dragged ones again
				T.editRow = T.FindRow(T.editRowKey);
				if (T.editRow < 0)
					T.CancelEdit();
				T.dragRow = T.FindRow(T.dragRowKey);
			}

			// The drag has ended, one way or another
			const ImGuiPayload* activePayload = ImGui::GetDragDropPayload();
			const bool          rowDragging   = activePayload && (activePayload->IsDataType("TOOL_ORDER") || activePayload->IsDataType("GROUP_ORDER"));
			if (!rowDragging) {
				T.dragRowKey.clear();
				T.dragRow = -1;
			}

			// --- Tools Header with inline Workspace Reset Button ---
			float startX     = FUCK::GetCursorPos().x;
			float availW     = FUCK::GetContentRegionAvail().x;
			float headerTopY = FUCK::GetCursorPos().y;

			FUCK::Header("$FUCK_Tools"_T);

			if (T.isModified) {
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
					// Moved groups go back to where their plugins put them, and their favourites go with them
					for (const auto& [from, to] : manager->_groupMoves) {
						std::vector<std::string> favourites;
						for (const auto& [k, v] : manager->_groupOverrides) {
							if (v.isFavourited && Sidebar::IsInGroup(k, to))
								favourites.push_back(k);
						}
						for (const auto& k : favourites) {
							manager->_groupOverrides[k].isFavourited = false;
							manager->_groupOverrides[from + k.substr(to.size())].isFavourited = true;
						}
					}
					manager->_groupMoves.clear();
					T.CancelEdit();
					manager->OnOverridesChanged();
				}
				ImGui::PopStyleVar();

				FUCK::SetCursorPosY(afterHeaderY);
			}

			FUCK::Spacing(2);

			// --- Filter, with Expand All / Collapse All beside it ---
			{
				const ImGuiStyle& st       = ImGui::GetStyle();
				const char*       expand   = "$FUCK_Sidebar_ExpandAll"_T;
				const char*       collapse = "$FUCK_Sidebar_CollapseAll"_T;

				// Sized like the buttons beside it, which pad their text by 7px a side and take at least a frame's height,
				// so the box and the buttons share a top and a bottom
				const float widgetScale = ImGui::GetDynamicWidgetScale();
				const float buttonPadX  = 8.0f * widgetScale;
				const float buttonH     = std::max(ImGui::GetFrameHeight(), ImGui::GetFontSize() + 14.0f * widgetScale);
				const float expandW     = FUCK::CalcTextSize(expand).x + buttonPadX * 2.0f;
				const float collW       = FUCK::CalcTextSize(collapse).x + buttonPadX * 2.0f;
				const float filterW     = std::max(100.0f, FUCK::GetContentRegionAvail().x - expandW - collW - st.ItemSpacing.x * 2.0f);

				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(st.FramePadding.x, std::max(0.0f, (buttonH - ImGui::GetFontSize()) * 0.5f)));
				ImGui::SetNextItemWidth(filterW);
				ImGui::InputTextWithHint("##SidebarTableFilter", "$FUCK_Sidebar_FilterHint"_T, T.filter, sizeof(T.filter));
				ImGui::PopStyleVar();

				const bool filtering = T.filter[0] != '\0';
				FUCK::BeginDisabled(filtering || T.groupPaths.empty());
				FUCK::SameLine();
				if (FUCK::Button(expand)) {
					T.collapsed.clear();
					T.dirty = true;
				}
				FUCK::SameLine();
				if (FUCK::Button(collapse)) {
					T.collapsed.insert(T.groupPaths.begin(), T.groupPaths.end());
					T.dirty = true;
				}
				FUCK::EndDisabled();

				if (filtering) {
					FUCK::Spacing();
					if (T.rows.empty())
						FUCK::TextDisabled("$FUCK_Sidebar_NoMatches"_T);
					else
						FUCK::TextDisabled("$FUCK_Sidebar_MatchCount"_T, static_cast<int>(T.shownTools), static_cast<int>(T.totalTools));
				}
			}

			FUCK::Spacing(2);

			// --- Reorderable List Table ---
			float tableScale = FUCK::GetGlobalScale();
			float indentStep = 25.0f * tableScale;
			FUCK::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(FUCK::GetStyleVarVec(ImGuiStyleVar_FramePadding).x, 7.0f * tableScale));

			const float rowH   = FUCK::GetFrameHeight();
			const float tableH = std::max(FUCK::GetContentRegionAvail().y - 4.0f * tableScale, rowH * 12.0f);

			FUCK::PushID(s_tableResetCounter);

			// Scrolls inside itself with the header frozen at the top, so only the rows on screen are drawn
			const auto tableFlags = static_cast<FUCK::TableFlags>(
				static_cast<int>(FUCK::TableFlags::kBorders | FUCK::TableFlags::kRowBg | FUCK::TableFlags::kResizable | FUCK::TableFlags::kSizingStretchProp | FUCK::TableFlags::kNoSavedSettings) |
				ImGuiTableFlags_ScrollY);

			if (FUCK::BeginTable("WorkspaceToolTable", 7, tableFlags, ImVec2(0.0f, tableH))) {
				ImGui::TableSetupScrollFreeze(0, 1);

				// Base Columns
				FUCK::TableSetupColumn("##Hide", FUCK::TableColumnFlags::kWidthFixed | FUCK::TableColumnFlags::kNoSort, 35.0f * tableScale);
				FUCK::TableSetupColumn("##Fav", FUCK::TableColumnFlags::kWidthFixed | FUCK::TableColumnFlags::kNoSort, 35.0f * tableScale);
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

				// Header: Star Icon (Col 1)
				FUCK::TableNextColumn();
				FUCK::TableSetBgColor(FUCK::TableBgTarget::kCellBg, ImGui::GetColorU32(ImGuiCol_TableHeaderBg));
				ImGui::PushFont(ImGui::GetFont(), ImGui::GetFontSize() * 0.8f);
				float iconStarW = FUCK::CalcTextSize(ICON_FA_STAR).x;
				FUCK::SetCursorPosX(FUCK::GetCursorPos().x + std::max(0.0f, (FUCK::GetColumnWidth() - iconStarW) * 0.5f));
				FUCK::SetCursorPosY(FUCK::GetCursorPos().y + (3.0f * tableScale));

				FUCK::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.84f, 0.0f, 1.0f));
				FUCK::Text(ICON_FA_STAR);
				FUCK::PopStyleColor();
				ImGui::PopFont();
				FUCK::SetTooltip("$FUCK_Sidebar_TableFavTT"_T);

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
				FUCK::SetTooltip("$FUCK_Sidebar_TableGroupTT"_T);

				// Where the frozen header ends, so nothing drawn for the rows spills over it
				const float headerBottom = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 0).Max.y;

				// Header: Hand Icon (Col 6)
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

				ImGuiWindow* tableWindow = ImGui::GetCurrentWindow();
				const ImRect bodyRect(ImVec2(tableWindow->InnerClipRect.Min.x, std::max(headerBottom, tableWindow->InnerClipRect.Min.y)), tableWindow->InnerClipRect.Max);
				const float  rightEdge = tableWindow->InnerClipRect.Max.x;

				// --- Scroll while dragging near the top or bottom of the list ---
				if (rowDragging) {
					const ImGuiIO& io    = ImGui::GetIO();
					const ImVec2   mouse = io.MousePos;
					const float    zone  = rowH * 1.5f;

					if (mouse.x >= bodyRect.Min.x && mouse.x <= bodyRect.Max.x && mouse.y > bodyRect.Min.y - zone * 2.0f && mouse.y < bodyRect.Max.y + zone * 2.0f) {
						float speed = 0.0f;
						if (mouse.y < bodyRect.Min.y + zone)
							speed = -std::min(1.5f, (bodyRect.Min.y + zone - mouse.y) / zone);
						else if (mouse.y > bodyRect.Max.y - zone)
							speed = std::min(1.5f, (mouse.y - (bodyRect.Max.y - zone)) / zone);

						if (speed != 0.0f)
							ImGui::SetScrollY(tableWindow, tableWindow->Scroll.y + speed * rowH * 15.0f * io.DeltaTime);
					}
				}

				// --- Row Helpers ---

				// Moves a group into another, and keeps the groups it carries collapsed if they were
				auto MoveGroupKeepingState = [&](const std::string& a_src, const std::string& a_parent, std::size_t a_index) {
					const std::string parent  = Sidebar::NormaliseGroupPath(a_parent);
					const std::string newPath = parent.empty() ? Sidebar::GetGroupLeaf(a_src) : parent + std::string(Sidebar::kGroupSeparator) + Sidebar::GetGroupLeaf(a_src);
					if (!manager->MoveGroupInto(a_src, parent, a_index))
						return;

					std::vector<std::string> carried;
					for (const auto& c : T.collapsed) {
						if (Sidebar::IsInGroup(c, a_src))
							carried.push_back(c);
					}
					for (const auto& c : carried) {
						T.collapsed.erase(c);
						T.collapsed.insert(newPath + c.substr(a_src.size()));
					}
					T.dirty = true;
				};


				auto findToolOver = [&](const std::string& a_key) -> const FUCKMan::ToolOverrideState* {
					auto it = manager->_toolOverrides.find(a_key);
					return it != manager->_toolOverrides.end() ? &it->second : nullptr;
				};

				auto findGroupOver = [&](const std::string& a_path) -> const FUCKMan::GroupOverrideState* {
					auto it = manager->_groupOverrides.find(a_path);
					return it != manager->_groupOverrides.end() ? &it->second : nullptr;
				};

				// Draws the tree connector for a row a_depth levels in, and moves the cursor past the indent
				auto DrawIndent = [&](int a_depth) {
					if (a_depth <= 0)
						return;

					ImVec2 p     = FUCK::GetCursorScreenPos();
					float  baseX = p.x + (a_depth - 1) * indentStep;
					float  lineX = baseX + 10.0f * tableScale;
					float  lineY = p.y - ImGui::GetStyle().FramePadding.y;
					float  midY  = p.y + rowH * 0.5f;

					ImU32 lineCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
					ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, lineY), ImVec2(lineX, midY), lineCol);
					ImGui::GetWindowDrawList()->AddLine(ImVec2(lineX, midY), ImVec2(lineX + 10.0f * tableScale, midY), lineCol);

					FUCK::SetCursorPosX(FUCK::GetCursorPos().x + a_depth * indentStep);
				};

				auto DrawGrip = [&](bool a_rowHovered) {
					ImU32  gripCol = a_rowHovered ? IM_COL32(50, 205, 50, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
					ImVec2 gripSz  = FUCK::CalcTextSize(ICON_FA_GRIP_LINES);
					ImVec2 gripPos(FUCK::GetCursorScreenPos().x + (FUCK::GetColumnWidth() - gripSz.x) * 0.5f, FUCK::GetCursorScreenPos().y + (rowH - gripSz.y) * 0.5f);
					ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), gripPos, gripCol, ICON_FA_GRIP_LINES);
				};

				// Makes the last item a drag source for its row, so a row can be dragged from any of its cells
				auto RowDragSource = [&](int a_row) {
					if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
						return;

					const TableRow& row = T.rows[a_row];
					if (row.tool) {
						ImGui::SetDragDropPayload("TOOL_ORDER", &row.tool, sizeof(FUCK::ITool*));
						ImGui::TextUnformatted(manager->GetToolLabel(row.tool).c_str());
					} else {
						ImGui::SetDragDropPayload("GROUP_ORDER", row.path.c_str(), row.path.size() + 1);
						ImGui::TextUnformatted(manager->GetGroupDisplayName(row.path).c_str());
					}
					T.dragRowKey = row.key;
					T.dragRow    = a_row;
					ImGui::EndDragDropSource();
				};

				// A cell showing a_text that turns into a text box when clicked. Enter or clicking away keeps the edit,
				// Escape drops it. Returns true when an edit was kept, with the text in T.editBuf
				auto EditableCell = [&](int a_row, const char* a_field, const std::string& a_text, bool a_dim, const char* a_tooltip) -> bool {
					const TableRow&   row     = T.rows[a_row];
					const std::string editKey = row.key + "|" + a_field;
					bool              kept    = false;

					ImGui::PushID(a_field);

					if (T.editKey == editKey) {
						ImGui::SetNextItemWidth(-FLT_MIN);
						if (T.editWait > 0)
							ImGui::SetKeyboardFocusHere();

						ImGui::InputText("##edit", T.editBuf, sizeof(T.editBuf), ImGuiInputTextFlags_AutoSelectAll);

						if (ImGui::IsItemDeactivated()) {
							kept = !ImGui::IsKeyPressed(ImGuiKey_Escape, false);
							T.CancelEdit();
						} else if (ImGui::IsItemActive()) {
							T.editWait = 0;
						} else if (T.editWait > 0) {
							--T.editWait;
						} else {
							T.CancelEdit();  // never took focus, or lost it some other way
						}
					} else {
						const ImVec2 p = ImGui::GetCursorScreenPos();
						const float  w = std::max(1.0f, ImGui::GetContentRegionAvail().x);

						if (ImGui::InvisibleButton("##cell", ImVec2(w, rowH))) {
							T.editKey    = editKey;
							T.editRowKey = row.key;
							T.editRow    = a_row;
							T.editWait   = 3;
							FUCK::StringCopy(T.editBuf, a_text);
						}
						RowDragSource(a_row);

						const bool hovered = ImGui::IsItemHovered();
						auto*      dl      = ImGui::GetWindowDrawList();
						if (hovered) {
							ImGui::AddThemedRectFilled(dl, ImRect(p, ImVec2(p.x + w, p.y + rowH)), ImGui::GetColorU32(ImGuiCol_FrameBgHovered), ImGui::GetStyle().FrameRounding);
							FUCK::SetTooltip(a_tooltip ? a_tooltip : "$FUCK_Sidebar_EditTT"_T);
						}

						const float padX = ImGui::GetStyle().FramePadding.x;
						if (!a_text.empty()) {
							ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(a_dim ? ImGuiCol_TextDisabled : ImGuiCol_Text));
							ImGui::RenderTextClipped(ImVec2(p.x + padX, p.y), ImVec2(p.x + w - padX, p.y + rowH), a_text.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.5f));
							ImGui::PopStyleColor();
						}

						if (hovered) {
							ImVec2 penSz = ImGui::CalcTextSize(ICON_FA_PEN);
							dl->AddText(ImVec2(p.x + w - padX - penSz.x, p.y + (rowH - penSz.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), ICON_FA_PEN);
						}
					}

					ImGui::PopID();
					return kept;
				};

				auto DrawDropLine = [&](const ImVec2& a_min, const ImVec2& a_max, bool a_after) {
					auto*       fg = ImGui::GetForegroundDrawList();
					const float y  = a_after ? a_max.y : a_min.y;
					fg->PushClipRect(bodyRect.Min, bodyRect.Max, false);
					fg->AddLine(ImVec2(a_min.x, y), ImVec2(a_max.x, y), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
					fg->PopClipRect();
				};

				auto DrawDropRect = [&](const ImVec2& a_min, const ImVec2& a_max) {
					auto* fg = ImGui::GetForegroundDrawList();
					fg->PushClipRect(bodyRect.Min, bodyRect.Max, false);
					fg->AddRect(a_min, a_max, ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, 2.0f);
					fg->PopClipRect();
				};

				auto RenderRow = [&](int i) {
					const TableRow& row = T.rows[i];

					FUCK::PushID(row.key.c_str());
					FUCK::TableNextRow();

					// Col 0: Row Background Selectable (spans all columns, for dragging and the menu) & Hide Button
					FUCK::TableNextColumn();
					ImVec2 startPos = FUCK::GetCursorPos();

					ImGui::Selectable("##Row", false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0, rowH));
					RowDragSource(i);

					const bool rowFocused = ImGui::IsItemFocused();
					ImVec2     rowMin     = ImGui::GetItemRectMin();
					ImVec2     rowMax     = ImGui::GetItemRectMax();
					rowMax.x              = rightEdge;
					// Tested against the whole row, not the cell's clip rect, which is only the current column's.
					// Kept below the frozen header, so a row scrolled under it can't be hovered
					const ImVec2 mousePos   = ImGui::GetIO().MousePos;
					const bool   rowHovered = ImGui::IsMouseHoveringRect(rowMin, rowMax, false) && bodyRect.Contains(mousePos) &&
					                        ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

					// Copied out, as a change below can add to the overrides and move what a pointer would point at
					const auto*       toolOver       = row.tool ? findToolOver(row.key) : nullptr;
					const bool        isHidden       = toolOver && toolOver->isHidden;
					const std::string customName     = toolOver ? toolOver->customName : std::string();
					const bool        hasCustomGroup = toolOver && !toolOver->customGroup.empty();
					const auto*       groupOver      = row.tool ? nullptr : findGroupOver(row.path);
					const bool        isFavourited   = row.tool ? (toolOver && toolOver->isFavourited) : (groupOver && groupOver->isFavourited);

					if (row.tool) {
						FUCK::SetCursorPos(startPos);

						float       cellW   = FUCK::GetColumnWidth();
						const char* icon    = isHidden ? ICON_FA_EYE_SLASH : ICON_FA_EYE;
						ImU32       iconCol = isHidden ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : ImGui::GetColorU32(ImGuiCol_Text);

						if (ImGui::InvisibleButton("##HideBtn", ImVec2(cellW, rowH))) {
							manager->_toolOverrides[row.key].isHidden = !isHidden;
							manager->OnOverridesChanged();
						}
						if (ImGui::IsItemHovered()) {
							iconCol = ImGui::GetColorU32(ImGuiCol_Text);
						}

						ImVec2 curPos = ImGui::GetItemRectMin();

						float scaledFontSize = ImGui::GetFontSize() * 0.8f;
						ImGui::PushFont(nullptr, scaledFontSize);
						ImVec2 textSize = FUCK::CalcTextSize(icon);
						ImGui::PopFont();

						ImVec2 textPos(curPos.x + (cellW - textSize.x) * 0.5f, curPos.y + (rowH - textSize.y) * 0.5f);
						ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), scaledFontSize, textPos, iconCol, icon);
					}

					// Col 1: Favourite star, for tools and groups alike
					FUCK::TableNextColumn();
					{
						const float cellW = FUCK::GetColumnWidth();
						if (ImGui::InvisibleButton("##FavBtn", ImVec2(cellW, rowH))) {
							if (row.tool)
								manager->_toolOverrides[row.key].isFavourited = !isFavourited;
							else
								manager->_groupOverrides[row.path].isFavourited = !isFavourited;
							manager->OnOverridesChanged();
						}

						const bool   starHovered = ImGui::IsItemHovered();
						const ImU32  starCol     = starHovered ? IM_COL32(255, 255, 100, 255) : (isFavourited ? IM_COL32(255, 215, 0, 255) : ImGui::GetColorU32(ImGuiCol_TextDisabled));
						const ImVec2 cellPos     = ImGui::GetItemRectMin();

						const float starFontSize = ImGui::GetFontSize() * 0.8f;
						ImGui::PushFont(nullptr, starFontSize);
						const ImVec2 starSize = FUCK::CalcTextSize(ICON_FA_STAR);
						ImGui::PopFont();

						ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), starFontSize,
							ImVec2(cellPos.x + (cellW - starSize.x) * 0.5f, cellPos.y + (rowH - starSize.y) * 0.5f), starCol, ICON_FA_STAR);
					}

					// Col 2: Original Name, or the group's name with its collapse arrow
					FUCK::TableNextColumn();
					if (row.tool) {
						FUCK::AlignTextToFramePadding();
						DrawIndent(row.depth);

						if (isHidden)
							ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
						FUCK::Text("%s", row.name.c_str());
						if (isHidden)
							ImGui::PopStyleColor();
					} else {
						DrawIndent(row.depth);

						const ImVec2 p = ImGui::GetCursorScreenPos();
						const float  w = std::max(1.0f, ImGui::GetContentRegionAvail().x);
						const bool   filtering = T.filter[0] != '\0';

						if (ImGui::InvisibleButton("##Collapse", ImVec2(w, rowH)) && !filtering) {
							if (row.collapsed)
								T.collapsed.erase(row.path);
							else
								T.collapsed.insert(row.path);
							T.dirty = true;
						}
						RowDragSource(i);

						float textX     = p.x;
						auto  iconArrow = IconFont::Manager::GetSingleton()->GetStepperRight();
						if (iconArrow) {
							float  aspect  = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;
							// Sized for the way it points, as the texture turns to point down. Centred in the wider of the two,
							// so the arrow and the name stay put when the group opens or closes
							auto   ap      = ImGui::CalcArrowIconParams(aspect, !row.collapsed, rowH, 20.0f * tableScale, 1.0f);
							float  slotW   = std::max(ap.drawSize.x, ap.drawSize.y);
							ImVec2 drawPos = { p.x + (slotW - ap.drawSize.x) * 0.5f, p.y + ap.offsetY };
							ImU32  col     = ImGui::GetColorU32(ImGui::IsItemHovered() ? ImGuiCol_Text : ImGuiCol_TextDisabled);
							ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, col, row.collapsed ? ImGui::IconDirection::kRight : ImGui::IconDirection::kDown);
							textX += slotW + (6.0f * tableScale);
						}

						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
						ImGui::RenderTextClipped(ImVec2(textX, p.y), ImVec2(p.x + w, p.y + rowH), row.name.c_str(), nullptr, nullptr, ImVec2(0.0f, 0.5f));
						ImGui::PopStyleColor();
					}

					// Col 3: Plugin Name
					FUCK::TableNextColumn();
					if (row.tool) {
						FUCK::AlignTextToFramePadding();
						FUCK::TextDisabled("%s", row.plugin.c_str());
					}

					// Col 4: Custom Name, keyed by the tool, or by the group's full path
					FUCK::TableNextColumn();
					if (row.tool) {
						if (EditableCell(i, "name", customName, false, nullptr) && customName != T.editBuf) {
							manager->_toolOverrides[row.key].customName = T.editBuf;
							manager->OnOverridesChanged();
						}
					} else {
						const auto*       gOver   = findGroupOver(row.path);
						const std::string current = gOver ? gOver->customName : std::string();
						if (EditableCell(i, "name", current, false, nullptr) && current != T.editBuf) {
							manager->_groupOverrides[row.path].customName = T.editBuf;
							manager->OnOverridesChanged();
						}
					}

					// Col 5: The tool's group as a path such as "Camera::Presets", dimmed while it's the one it came with.
					// For a nested group, the group it sits in
					FUCK::TableNextColumn();
					if (row.tool) {
						if (EditableCell(i, "group", row.path, !hasCustomGroup, "$FUCK_Sidebar_TableGroupTT"_T)) {
							// Moved to the end of the group it's typed into. MoveTool keeps everything else where it is
							const std::string typed = Sidebar::NormaliseGroupPath(T.editBuf);
							if (typed != row.path)
								manager->MoveTool(row.tool, typed, SIZE_MAX);
						}
					} else {
						// The group it sits in. Typing another moves the group, and everything in it, there
						const std::string parent = Sidebar::GetGroupParent(row.path);
						if (EditableCell(i, "group", parent, true, "$FUCK_Sidebar_TableParentTT"_T)) {
							const std::string typed = Sidebar::NormaliseGroupPath(T.editBuf);
							if (typed != parent)
								MoveGroupKeepingState(row.path, typed, SIZE_MAX);
						}
					}

					// Col 6: Drag Handle
					FUCK::TableNextColumn();
					DrawGrip(rowHovered);

					// --- Drop target: the whole row ---
					// The target's hover test uses the clip rect, so it's set to the list's body for the whole row
					ImGui::PushClipRect(bodyRect.Min, bodyRect.Max, false);
					if (rowDragging && ImGui::BeginDragDropTargetCustom(ImRect(rowMin, rowMax), ImGui::GetID("##Drop"))) {
						const ImGuiPayload* peek = ImGui::GetDragDropPayload();

						// Where in the row the mouse is, from 0 at the top to 1 at the bottom
						const float along = std::clamp((ImGui::GetIO().MousePos.y - rowMin.y) / std::max(1.0f, rowMax.y - rowMin.y), 0.0f, 1.0f);

						// The level this row sits at: a tool's group, or a group's parent
						const std::string    level = row.tool ? row.path : Sidebar::GetGroupParent(row.path);
						const Sidebar::Entry target{ row.tool, row.tool ? std::string() : row.path };

						// Where an entry dropped before or after this row lands among that level's groups and tools
						auto IndexAtRow = [&](const Sidebar::Entry& a_moving, bool a_after) {
							std::vector<Sidebar::Entry> list;
							if (auto it = layout.tree.find(level); it != layout.tree.end())
								list = it->second.entries;
							std::erase(list, a_moving);
							auto at = std::find(list.begin(), list.end(), target);
							return static_cast<std::size_t>(at - list.begin()) + (a_after && at != list.end() ? 1 : 0);
						};

						// Before or after a tool by its halves. A group's top quarter is before it, and the bottom quarter after it
						// while it's closed, as after an open group means after everything in it
						bool before = false;
						bool after  = false;
						if (row.tool) {
							before = along < 0.5f;
							after  = !before;
						} else {
							before = along < 0.25f;
							after  = along > 0.75f && row.collapsed;
						}

						if (peek && peek->IsDataType("TOOL_ORDER")) {
							FUCK::ITool*         src = *static_cast<FUCK::ITool* const*>(peek->Data);
							const Sidebar::Entry moving{ src, std::string() };

							if (src != row.tool) {
								if (before || after) {
									DrawDropLine(rowMin, rowMax, after);
									if (ImGui::AcceptDragDropPayload("TOOL_ORDER", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
										manager->MoveTool(src, level, IndexAtRow(moving, after));
								} else {
									// Into this group, at the top
									DrawDropRect(rowMin, rowMax);
									if (ImGui::AcceptDragDropPayload("TOOL_ORDER", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
										manager->MoveTool(src, row.path, 0);
								}
							}
						} else if (peek && peek->IsDataType("GROUP_ORDER")) {
							// Groups go before or after a row, at that row's level, or into a group. Never into themselves
							const std::string    src = static_cast<const char*>(peek->Data);
							const Sidebar::Entry moving{ nullptr, src };

							if (src != row.path && !Sidebar::IsInGroup(row.path, src)) {
								if (before || after) {
									const bool sameLevel = Sidebar::GetGroupParent(src) == level;
									if (sameLevel || manager->CanMoveGroupInto(src, level)) {
										DrawDropLine(rowMin, rowMax, after);
										if (ImGui::AcceptDragDropPayload("GROUP_ORDER", ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
											if (sameLevel)
												manager->MoveGroup(src, IndexAtRow(moving, after));
											else
												MoveGroupKeepingState(src, level, IndexAtRow(moving, after));
										}
									}
								} else if (!row.tool && manager->CanMoveGroupInto(src, row.path)) {
									// Into this group, at the top
									DrawDropRect(rowMin, rowMax);
									if (ImGui::AcceptDragDropPayload("GROUP_ORDER", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
										MoveGroupKeepingState(src, row.path, 0);
								}
							}
						}
						ImGui::EndDragDropTarget();
					}
					ImGui::PopClipRect();

					// --- Right-click, or Y on a gamepad, opens the row's menu ---
					const bool rightClicked = rowHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) && !rowDragging;
					const bool padMenu      = rowFocused && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false);
					if (rightClicked || padMenu) {
						T.menuTool      = row.tool;
						T.menuGroup     = row.tool ? std::string() : row.path;
						T.menuRequested = true;
					}

					FUCK::PopID();
				};

				// --- Rows, only those on screen, plus any being dragged or edited so they carry on off screen ---
				ImGuiListClipper clipper;
				clipper.Begin(static_cast<int>(T.rows.size()));  // measures the row height from the first row
				if (T.dragRow >= 0 && T.dragRow < static_cast<int>(T.rows.size()))
					clipper.IncludeItemByIndex(T.dragRow);
				if (T.editRow >= 0 && T.editRow < static_cast<int>(T.rows.size()))
					clipper.IncludeItemByIndex(T.editRow);

				while (clipper.Step()) {
					for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
						RenderRow(i);
					}
				}
				clipper.End();

				// --- The row menu ---
				if (T.menuRequested) {
					T.menuRequested = false;
					ImGui::OpenPopup("##SidebarRowMenu");
				}

				if (ImGui::BeginPopup("##SidebarRowMenu")) {
					if (T.menuTool) {
						FUCK::ITool*      tool    = T.menuTool;
						const std::string key     = manager->GetToolKey(tool);
						const std::string current = manager->GetToolGroup(tool);
						const auto*       over    = findToolOver(key);
						const bool        hidden  = over && over->isHidden;
						const bool        fav     = over && over->isFavourited;

						ImGui::TextDisabled("%s", manager->GetToolLabel(tool).c_str());
						ImGui::Separator();

						if (ImGui::MenuItem("$FUCK_Sidebar_MoveTop"_T))
							manager->MoveTool(tool, current, 0);
						if (ImGui::MenuItem("$FUCK_Sidebar_MoveBottom"_T))
							manager->MoveTool(tool, current, SIZE_MAX);

						ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, 420.0f * tableScale));
						if (ImGui::BeginMenu("$FUCK_Sidebar_MoveToGroup"_T)) {
							if (ImGui::MenuItem("$FUCK_Sidebar_TopLevel"_T, nullptr, current.empty()) && !current.empty())
								manager->MoveTool(tool, std::string(), SIZE_MAX);

							for (const auto& g : T.groupPaths) {
								ImGui::PushID(g.c_str());
								std::string label(static_cast<std::size_t>(Sidebar::GetGroupDepth(g)) * 3, ' ');
								label += manager->GetGroupDisplayName(g);
								if (ImGui::MenuItem(label.c_str(), nullptr, g == current) && g != current)
									manager->MoveTool(tool, g, SIZE_MAX);
								ImGui::PopID();
							}
							ImGui::EndMenu();
						}

						if (ImGui::MenuItem(hidden ? "$FUCK_Sidebar_ShowTool"_T : "$FUCK_Sidebar_TableHideTT"_T)) {
							manager->_toolOverrides[key].isHidden = !hidden;
							manager->OnOverridesChanged();
						}
						if (ImGui::MenuItem(fav ? "$FUCK_Sidebar_RemoveFavourite"_T : "$FUCK_Sidebar_AddFavourite"_T)) {
							manager->_toolOverrides[key].isFavourited = !fav;
							manager->OnOverridesChanged();
						}

						ImGui::Separator();
						if (ImGui::MenuItem("$FUCK_Sidebar_ResetRow"_T)) {
							auto& o       = manager->_toolOverrides[key];
							o.customName  = "";
							o.customGroup = "";
							o.sortOrder   = 0;
							o.isHidden    = false;
							T.CancelEdit();
							manager->OnOverridesChanged();
						}
					} else if (!T.menuGroup.empty()) {
						const std::string path     = T.menuGroup;
						const auto*       groupFav = findGroupOver(path);
						const bool        fav      = groupFav && groupFav->isFavourited;

						ImGui::TextDisabled("%s", manager->GetGroupDisplayName(path).c_str());
						ImGui::Separator();

						if (ImGui::MenuItem("$FUCK_Sidebar_MoveTop"_T))
							manager->MoveGroup(path, 0);
						if (ImGui::MenuItem("$FUCK_Sidebar_MoveBottom"_T))
							manager->MoveGroup(path, SIZE_MAX);

						// Places it can go: not itself or anything in it, and not so deep it would pass the depth limit
						ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, 420.0f * tableScale));
						if (ImGui::BeginMenu("$FUCK_Sidebar_MoveToGroup"_T)) {
							const std::string current = Sidebar::GetGroupParent(path);

							if (ImGui::MenuItem("$FUCK_Sidebar_TopLevel"_T, nullptr, current.empty(), manager->CanMoveGroupInto(path, std::string())))
								MoveGroupKeepingState(path, std::string(), SIZE_MAX);

							for (const auto& g : T.groupPaths) {
								if (g == path || Sidebar::IsInGroup(g, path))
									continue;

								ImGui::PushID(g.c_str());
								std::string label(static_cast<std::size_t>(Sidebar::GetGroupDepth(g)) * 3, ' ');
								label += manager->GetGroupDisplayName(g);
								if (ImGui::MenuItem(label.c_str(), nullptr, g == current, manager->CanMoveGroupInto(path, g)))
									MoveGroupKeepingState(path, g, SIZE_MAX);
								ImGui::PopID();
							}
							ImGui::EndMenu();
						}

						if (ImGui::MenuItem(fav ? "$FUCK_Sidebar_RemoveFavourite"_T : "$FUCK_Sidebar_AddFavourite"_T)) {
							manager->_groupOverrides[path].isFavourited = !fav;
							manager->OnOverridesChanged();
						}

						ImGui::Separator();
						if (ImGui::MenuItem("$FUCK_Sidebar_ResetRow"_T)) {
							auto& o      = manager->_groupOverrides[path];
							o.customName = "";
							o.sortOrder  = 0;
							T.CancelEdit();
							manager->OnOverridesChanged();
						}
					}
					ImGui::EndPopup();
				}

				FUCK::EndTable();
			}

			FUCK::PopID();
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
