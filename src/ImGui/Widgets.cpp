#include "Widgets.h"
#include "FUCKMan.h"
#include "FormComboBox.h"
#include "IconsFonts.h"
#include "Renderer.h"
#include "System/Input.h"

	namespace ImGui
{
	// =========================================================================================
	// HELPER IMPLEMENTATION
	// =========================================================================================

	void DrawArrowIcon(ImDrawList* drawList, ImVec2 pos, ImVec2 size, ImU32 color, IconDirection direction)
	{
		static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
		if (!iconArrow || !iconArrow->srView)
			return;

		if (!drawList)
			drawList = ImGui::GetWindowDrawList();

		ImVec2 p_min = pos;
		ImVec2 p_max = pos + size;

		if (direction == IconDirection::kRight) {
			// Normal (>): UVs {0,0} -> {1,1}
			drawList->AddImage((ImTextureID)iconArrow->srView.Get(), p_min, p_max, { 0, 0 }, { 1, 1 }, color);
		} else if (direction == IconDirection::kDown) {
			// Rotate 90 CW (v): UVs {0,1}, {0,0}, {1,0}, {1,1}
			drawList->AddImageQuad((ImTextureID)iconArrow->srView.Get(),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 }, color);
		} else if (direction == IconDirection::kLeft) {
			// Mirror Horizontal (<): Swap U {1,0} -> {0,0}
			drawList->AddImageQuad((ImTextureID)iconArrow->srView.Get(),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, color);
		} else if (direction == IconDirection::kUp) {
			// Rotate 270 CW / 90 CCW (^): UVs {1,0}, {1,1}, {0,1}, {0,0}
			drawList->AddImageQuad((ImTextureID)iconArrow->srView.Get(),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 }, color);
		}
	}
}

namespace
{
	// =========================================================================================
	// INTERNAL HELPERS
	// =========================================================================================

	void DrawTreeIcon(ImDrawList* drawList, const ImVec2& pos, float frameHeight, bool isOpen, bool isHovered)
	{
		static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
		if (!iconArrow)
			return;

		ImU32 col = ImGui::GetDynamicTextColor(isHovered);

		ImVec2 size = iconArrow->size;
		ImVec2 drawSize = isOpen ? ImVec2(size.y, size.x) : size;

		float offY = (frameHeight - drawSize.y) * 0.5f;
		ImVec2 drawPos = { pos.x, pos.y + offY };

		// TreeNode: Closed = Right, Open = Down
		ImGui::DrawArrowIcon(drawList, drawPos, drawSize, col, isOpen ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
	}

	void DrawDropdownIcon(ImDrawList* drawList, ImVec2 bPos, ImVec2 bSize, bool isOpen, bool opensUp, bool isHovered)
	{
		static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
		if (!iconArrow)
			return;

		ImU32 col = ImGui::GetDynamicTextColor(isHovered || isOpen);

		ImVec2 size = iconArrow->size;
		ImVec2 drawSize = isOpen ? ImVec2(size.y, size.x) : size;

		ImVec2 iconPos = {
			bPos.x + (bSize.x - drawSize.x) * 0.5f,
			bPos.y + (bSize.y - drawSize.y) * 0.5f
		};

		// Combo/Window: Closed = Left, Open = Down/Up
		ImGui::IconDirection dir;
		if (isOpen) {
			dir = opensUp ? ImGui::IconDirection::kUp : ImGui::IconDirection::kDown;
		} else {
			dir = ImGui::IconDirection::kLeft;
		}

		ImGui::DrawArrowIcon(drawList, iconPos, drawSize, col, dir);
	}

	ImVec4 GetHighlightTint(bool active, bool hovered, bool focused)
	{
		if (hovered || focused)
			return ImVec4(1, 1, 1, 1);
		if (active)
			return ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kWidgetToggleActive);
		return ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kIconDisabled);
	}

	bool DrawTransparentButton(const char* id, void* tex, const ImVec2& size, const ImVec4& tint)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0, 0, 0, 0 });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0, 0, 0, 0 });
		ImGui::PushStyleColor(ImGuiCol_Border, { 0, 0, 0, 0 });
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });
		bool result = ImGui::ImageButton(id, (ImTextureID)tex, size, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, tint);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
		return result;
	}

	void AlignedWidgetLayout(const char* label, bool alignFar, bool labelLeft, float contentWidth, std::function<void()> drawContent, float targetHeight = -1.0f)
	{
		ImGui::BeginGroup();
		ImGui::PushID(label);

		ImGuiContext& g = *GImGui;
		float startX = ImGui::GetCursorPosX();
		float startY = ImGui::GetCursorPosY();
		float availWidth = ImGui::GetContentRegionAvail().x;

		float rightPaneStart = startX + ImGui::CalcItemWidth() * 0.5f + g.Style.ItemInnerSpacing.x;
		float rightPaneEnd = startX + availWidth;
		float rightPaneCenter = rightPaneStart + (rightPaneEnd - rightPaneStart) * 0.5f;

		float splitPoint = rightPaneCenter - (contentWidth * 0.5f);

		std::string_view labelView(label);
		auto doubleHash = labelView.find("##");
		if (doubleHash != std::string_view::npos)
			labelView = labelView.substr(0, doubleHash);

		float labelWidth = ImGui::CalcTextSize(labelView.data(), labelView.data() + labelView.size()).x;

		float defaultTargetHeight = targetHeight > 0.0f ? targetHeight : ImGui::GetFrameHeight();

		auto DrawLabel = [&]() {
			float textH = ImGui::GetTextLineHeight();
			float offY = (defaultTargetHeight - textH) * 0.5f;

			ImGui::SetCursorPosY(startY + std::max(0.0f, offY));
			ImGui::TextUnformatted(labelView.data(), labelView.data() + labelView.size());
		};

		auto DrawWidget = [&]() {
			ImGui::SetCursorPosY(startY);
			drawContent();
		};

		if (alignFar) {
			if (labelLeft) {
				ImGui::SetCursorPosX(startX);
				DrawLabel();
				ImGui::SameLine();
				ImGui::SetCursorPosX(splitPoint);
				DrawWidget();
			} else {
				ImGui::SetCursorPosX(startX);
				DrawWidget();

				ImGui::SameLine();

				float rightAnchorX = (splitPoint + contentWidth) - labelWidth;
				ImGui::SetCursorPosX(rightAnchorX);
				DrawLabel();
			}
		} else {
			if (labelLeft) {
				ImGui::SetCursorPosX(startX);
				DrawLabel();
				ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
				DrawWidget();
			} else {
				ImGui::SetCursorPosX(startX);
				DrawWidget();
				ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
				DrawLabel();
			}
		}

		ImGui::PopID();
		ImGui::EndGroup();
	}
}

namespace ImGui
{
	// =========================================================================================
	// CORE DRAWING & WIDGETS
	// =========================================================================================

	void DrawWidgetBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding)
	{
		float thickness = ImGui::GetStyle().FrameBorderSize;

		if (thickness < 1.0f && thickness > 0.0f)
			thickness = 1.0f;

		ImU32 col = isActiveOrHovered ?
		                GetUserStyleColorU32(USER_STYLE::kSliderBorderActive) :
		                GetUserStyleColorU32(USER_STYLE::kSliderBorder);

		drawList->AddRect(bb.Min, bb.Max, col, rounding, 0, thickness);
	}

	bool CheckBox(const char* label, bool* a_toggle, bool alignFar, bool labelLeft)
	{
		bool selected = false;
		auto icon = MANAGER(IconFont)->GetCheckbox();
		auto iconFilled = MANAGER(IconFont)->GetCheckboxFilled();
		std::string idStr = std::format("##{}", label);

		auto DrawContent = [&]() {
			ImVec2 p = ImGui::GetCursorScreenPos();
			bool h = ImGui::IsMouseHoveringRect(p, p + icon->size) || IsWidgetFocused(ImGui::GetID(idStr.c_str()));
			ImTextureID tex = (ImTextureID)(*a_toggle ? iconFilled->srView.Get() : icon->srView.Get());
			if (DrawTransparentButton(idStr.c_str(), (void*)tex, icon->size, GetHighlightTint(*a_toggle, h, false))) {
				*a_toggle = !*a_toggle;
				selected = true;
			}
		};
		AlignedWidgetLayout(label, alignFar, labelLeft, icon->size.x, DrawContent, icon->size.y);
		if (selected)
			RE::PlaySound("UIMenuFocus");
		return selected;
	}

	bool ToggleButton(const char* label, bool* v, bool alignFar, bool labelLeft)
	{
		bool pressed = false;
		std::string idStr = std::format("##{}", label);

		float frameH = ImGui::GetFrameHeight();
		float width = frameH * 1.25f;

		auto DrawContent = [&]() {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;

			ImGuiContext& g = *GImGui;
			const ImGuiID id = window->GetID(idStr.c_str());
			float scale = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();

			ImVec2 p = ImGui::GetCursorScreenPos();
			ImRect bb(p, p + ImVec2(width, frameH));

			ItemSize(bb);
			if (!ItemAdd(bb, id))
				return;

			bool hovered, held;
			if (ButtonBehavior(bb, id, &hovered, &held)) {
				*v = !*v;
				pressed = true;
				MarkItemEdited(id);
			}

			float* t_anim = window->DC.StateStorage->GetFloatRef(id, *v ? 1.0f : 0.0f);
			float target = *v ? 1.0f : 0.0f;
			float anim_speed = 12.0f;
			float dt = g.IO.DeltaTime;
			if (*t_anim < target) {
				*t_anim += dt * anim_speed;
				if (*t_anim > target)
					*t_anim = target;
			} else if (*t_anim > target) {
				*t_anim -= dt * anim_speed;
				if (*t_anim < target)
					*t_anim = target;
			}
			float t = *t_anim;

			ImDrawList* draw_list = window->DrawList;
			bool isInputGamepad = MANAGER(Input)->IsInputGamepad();
			bool isFocused = IsWidgetFocused(id);
			bool showFrame = isFocused && isInputGamepad;

			ImU32 col_rail_fill = GetUserStyleColorU32(USER_STYLE::kToggleRailFilled);
			ImU32 col_knob_fill = GetUserStyleColorU32(USER_STYLE::kToggleKnob);

			ImU32 col_knob_ring;
			if (*v) {
				col_knob_ring = GetColorU32(ImGuiCol_Text);
			} else {
				col_knob_ring = showFrame ? GetUserStyleColorU32(USER_STYLE::kWidgetToggleActive) : GetUserStyleColorU32(USER_STYLE::kSliderBorder);
			}

			if (showFrame) {
				ImU32 col_frame = GetColorU32(ImGuiCol_NavHighlight);
				draw_list->AddRect(bb.Min, bb.Max, col_frame, ImGui::GetStyle().FrameRounding, 0, 2.0f * scale);
			}

			float railH = frameH * 0.25f;
			float railY = p.y + (frameH - railH) * 0.5f;

			ImVec2 railMin = { p.x + 8.0f * scale, railY };
			ImVec2 railMax = { p.x + width - 8.0f * scale, railY + railH };
			ImRect railBB(railMin, railMax);

			draw_list->AddRectFilled(railMin, railMax, col_rail_fill, ImGui::GetStyle().FrameRounding);
			DrawWidgetBorder(draw_list, railBB, hovered, ImGui::GetStyle().FrameRounding);

			float knobRadius = frameH * 0.32f;
			float knobStart = railMin.x - (knobRadius * 0.1f);
			float knobEnd = railMax.x + (knobRadius * 0.1f);
			float knobRange = knobEnd - knobStart;

			float knobX = knobStart + (t * knobRange);
			ImVec2 knobCenter = { knobX, p.y + (frameH * 0.5f) };

			draw_list->AddCircleFilled(knobCenter, knobRadius, col_knob_fill);

			float ringThick = 2.0f * scale;
			float ringPadding = 4.0f * scale;
			float ringRadius = knobRadius - ringPadding;

			if (ringRadius > 0.5f) {
				draw_list->AddCircle(knobCenter, ringRadius, col_knob_ring, 0, ringThick);
			}
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, width, DrawContent);
		if (pressed)
			RE::PlaySound("UIMenuFocus");
		return pressed;
	}

	bool ComboWithFilter(const char* label, int* current_item, const std::vector<std::string>& items, int popup_max_height_in_items)
	{
		float scale = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();

		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		PushStyleVar(ImGuiStyleVar_FramePadding, { GetStyle().FramePadding.x, 4.0f * scale });

		const bool isHidden = std::string_view(label).starts_with("##");
		std::string idStr = isHidden ? label : "##"s + label;
		if (!isHidden)
			LeftAlignedTextImpl(label, idStr);

		ImVec2 widgetPos = GetCursorScreenPos();
		float width = CalcItemWidth();

		struct State
		{
			char pattern[256] = { 0 };
		};
		static std::unordered_map<ImGuiID, State> states;
		ImGuiID id = window->GetID(idStr.c_str());

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;

		float listH_Max = GetTextLineHeightWithSpacing() * popup_max_height_in_items;
		float constraintH = GetFrameHeight() + g.Style.ItemSpacing.y + listH_Max + (g.Style.WindowPadding.y * 2.0f) + 20.0f;

		if (!(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
			SetNextWindowSizeConstraints({ width, 0.0f }, { width, constraintH });
		}

		const char* preview = (*current_item >= 0 && *current_item < (int)items.size()) ? items[*current_item].c_str() : "";

		ImVec4 textBoxColor = GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox);
		PushStyleColor(ImGuiCol_FrameBg, textBoxColor);
		PushStyleColor(ImGuiCol_FrameBgHovered, textBoxColor);
		PushStyleColor(ImGuiCol_FrameBgActive, textBoxColor);

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, textBoxColor);

		// Capture height while padding is active
		float frameH = GetFrameHeight();

		ImDrawList* parentDrawList = GetWindowDrawList();

		bool isOpen = BeginCombo(idStr.c_str(), preview, ImGuiComboFlags_NoArrowButton);

		PopStyleVar();
		PopStyleColor(5);

		// Detect if opened Upwards
		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}

		DrawDropdownIcon(parentDrawList, { widgetPos.x + width - frameH, widgetPos.y }, { frameH, frameH }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { widgetPos, widgetPos + ImVec2(width, frameH) }, isOpen || IsItemHovered() || IsWidgetFocused(id), ImGui::GetStyle().FrameRounding);

		if (!isOpen)
			return false;

		float fontSize = std::round(ImGui::GetStyle().FontSizeBase * FUCKMan::GetSingleton()->GetUserScale() * 2.0f) / 2.0f;
		ImGui::PushFont(nullptr, fontSize);

		if (IsWindowAppearing())
			ImGui::SetKeyboardFocusHere();

		// Filter input styling
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f)));
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetUserStyleColorU32(USER_STYLE::kComboBoxText));
		ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));

		ImGui::PushItemWidth(-FLT_MIN);
		ImGui::Dummy(ImVec2(0.0f, 1.0f));
		InputText("##filter", states[id].pattern, 256, ImGuiInputTextFlags_AutoSelectAll);
		ImGui::PopItemWidth();

		ImGui::PopStyleColor(3);

		std::vector<std::pair<int, double>> itemScoreVector;
		bool filtering = states[id].pattern[0] != '\0';
		if (filtering) {
			for (int i = 0; i < (int)items.size(); i++) {
				auto score = rapidfuzz::fuzz::partial_token_ratio(states[id].pattern, items[i].c_str());
				if (score >= 65.0)
					itemScoreVector.push_back({ i, score });
			}
			std::ranges::sort(itemScoreVector, [](const auto& a, const auto& b) { return b.second < a.second; });
		}

		bool changed = false;
		int show_count = filtering ? (int)itemScoreVector.size() : (int)items.size();

		// Calculate height for list
		int heightInItems = show_count;
		if (heightInItems > popup_max_height_in_items)
			heightInItems = popup_max_height_in_items;
		if (heightInItems < 2)
			heightInItems = 2;

		float listH = GetTextLineHeightWithSpacing() * heightInItems + g.Style.FramePadding.y * 2.0f;
		ImVec2 listSize(-FLT_MIN, listH);

		ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));

		if (BeginListBox("##List", listSize)) {
			for (int i = 0; i < show_count; i++) {
				int idx = filtering ? itemScoreVector[i].first : i;
				if (Selectable(items[idx].c_str(), *current_item == idx)) {
					*current_item = idx;
					changed = true;
					CloseCurrentPopup();
					RE::PlaySound("UIMenuFocus");
				}
				if (*current_item == idx)
					SetItemDefaultFocus();
			}
			EndListBox();
		}
		ImGui::PopStyleColor();
		ImGui::PopFont();
		EndCombo();
		return changed;
	}

	bool ComboStyled(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height_in_items)
	{
		float scale = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();

		PushStyleVar(ImGuiStyleVar_FramePadding, { GetStyle().FramePadding.x, 4.0f * scale });

		std::string idStr = "##"s + label;
		LeftAlignedTextImpl(label, idStr);
		ImVec2 widgetPos = GetCursorScreenPos();
		float width = CalcItemWidth();

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;
		SetNextWindowSizeConstraints({ width, 0 }, { width, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items) });

		PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));

		float frameH = GetFrameHeight();

		ImDrawList* parentDrawList = GetWindowDrawList();

		const char* preview = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";

		parentDrawList->AddRectFilled(widgetPos, widgetPos + ImVec2(width, frameH), GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), ImGui::GetStyle().FrameRounding);

		bool isOpen = BeginCombo(idStr.c_str(), preview, ImGuiComboFlags_NoArrowButton);

		PopStyleVar();
		PopStyleColor(6);

		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}

		DrawDropdownIcon(parentDrawList, { widgetPos.x + width - frameH, widgetPos.y }, { frameH, frameH }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { widgetPos, widgetPos + ImVec2(width, frameH) }, isOpen || IsItemHovered() || IsWidgetFocused(GetID(idStr.c_str())), ImGui::GetStyle().FrameRounding);

		bool changed = false;
		if (isOpen) {
			float fontSize = std::round(ImGui::GetStyle().FontSizeBase * FUCKMan::GetSingleton()->GetUserScale() * 2.0f) / 2.0f;
			ImGui::PushFont(nullptr, fontSize);

			for (int i = 0; i < items_count; i++) {
				if (Selectable(items[i], *current_item == i)) {
					*current_item = i;
					changed = true;
					RE::PlaySound("UIMenuFocus");
				}
				if (*current_item == i)
					SetItemDefaultFocus();
			}

			ImGui::PopFont();
			EndCombo();
		}
		return changed;
	}

	void DrawTabBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered)
	{
		float borderSize = ImGui::GetStyle().FrameBorderSize;
		float round = ImGui::GetStyle().TabRounding;
		float scale = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();
		float outlining = borderSize + (2.5f * scale);

		auto buildPath = [&](ImDrawList* dl, float botY) {
			dl->PathLineTo({ bb.Min.x, botY });
			dl->PathArcToFast({ bb.Min.x + round, bb.Min.y + round }, round, 6, 9);
			dl->PathArcToFast({ bb.Max.x - round, bb.Min.y + round }, round, 9, 12);
			dl->PathLineTo({ bb.Max.x, botY });
		};

		buildPath(drawList, bb.Max.y);
		drawList->PathFillConvex(GetColorU32(ImGuiCol_Button));

		buildPath(drawList, bb.Max.y);
		drawList->PathStroke(IM_COL32(0, 0, 0, 255), 0, outlining);

		ImU32 col = isActiveOrHovered ?
		                GetUserStyleColorU32(USER_STYLE::kTabBorderActive) :
		                GetUserStyleColorU32(USER_STYLE::kTabBorder);
		buildPath(drawList, bb.Max.y);
		drawList->PathStroke(col, 0, borderSize);
	}

	bool BeginTabItemEx(const char* label, ImGuiTabItemFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID id = ImGui::GetID(label);
		ImGuiID storageKey = ImGui::GetID("##LastActiveTab");
		ImGuiID lastActive = window->StateStorage.GetInt(storageKey, 0);

		bool wasActive = (lastActive == id);
		float scale = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();

		PushStyleColor(ImGuiCol_Tab, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabActive, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0, 0, 0, 0));

		if (!wasActive)
			PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled));

		PushStyleVar(ImGuiStyleVar_FramePadding, { ImGui::GetStyle().FramePadding.x, 6.0f * scale });

		window->DrawList->ChannelsSplit(2);
		window->DrawList->ChannelsSetCurrent(1);

		bool active = BeginTabItem(label, nullptr, flags);

		PopStyleVar();

		if (!wasActive)
			PopStyleColor();
		PopStyleColor(5);

		// Switch to background channel for custom border
		window->DrawList->ChannelsSetCurrent(0);

		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();
		float borderSize = ImGui::GetStyle().FrameBorderSize;
		float outlineInset = borderSize * 0.5f;
		float clampedMaxY = min.y + GetFrameHeight() - 2.0f;  // hug the bar tighter

		ImRect drawBb = {
			ImVec2(min.x + outlineInset, min.y),
			ImVec2(max.x - outlineInset, clampedMaxY)
		};

		DrawTabBorder(window->DrawList, drawBb, active || IsItemHovered());

		// Merge channels back to normal
		window->DrawList->ChannelsMerge();

		if (active) {
			window->StateStorage.SetInt(storageKey, id);

			if (!wasActive) {
				RE::PlaySound("UIJournalTabsSD");
			}
			ActivateOnHover();
		}
		return active;
	}

	bool OutlineButton(const char* label, bool* wasFocused)
	{
		ImGuiWindow* window = GetCurrentWindow();
		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();
		float rounding = GetUserStyleVar(USER_STYLE::kButtonRounding) * scale;
		float borderSize = ImGui::GetStyle().FrameBorderSize;

		ImVec2 textSize = CalcTextSize(label);
		ImVec2 textPad = ImVec2(5.0f * scale, 4.0f * scale);

		float outlining = borderSize + (2.5f * scale);
		float outlineInset = outlining * 0.5f;

		// Consistent vertical size based on line height
		ImVec2 contentSize = ImVec2(textSize.x, GetTextLineHeight());

		ImVec2 sz = contentSize + (textPad * 2.0f) + ImVec2(outlineInset * 2.0f, outlineInset * 2.0f);
		sz.x = ImMax(sz.x, sz.y);  // minimum square

		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + sz);
		ItemSize(sz);
		if (!ItemAdd(bb, window->GetID(label)))
			return false;

		ImRect drawBb = { bb.Min + ImVec2(outlineInset, outlineInset), bb.Max - ImVec2(outlineInset, outlineInset) };

		bool h, held;
		bool p = ButtonBehavior(bb, window->GetID(label), &h, &held);

		RenderFrame(drawBb.Min, drawBb.Max, GetColorU32(ImGuiCol_Button), true, rounding);

		window->DrawList->AddRect(drawBb.Min, drawBb.Max, IM_COL32(0, 0, 0, 255), rounding, 0, outlining);
		DrawWidgetBorder(window->DrawList, drawBb, h || held, rounding);

		bool dim = MANAGER(Input)->IsInputGamepad() && !h;
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));

		// Center using contentSize so vertical position is consistent across all glyphs
		RenderTextClipped(bb.Min, bb.Max, label, NULL, &contentSize, { 0.5f, 0.5f });

		if (dim)
			PopStyleColor();

		if (p)
			RE::PlaySound("UIMenuOK");
		if (wasFocused)
			*wasFocused = h;
		return p;
	}
	
	bool ButtonIconWithLabelStyled(const char* label, void* tex, const ImVec2& size, bool alignFar, bool labelLeft)
	{
		bool clicked = false;
		std::string idStr = std::format("##BTN_{}", label);
		auto DrawContent = [&]() {
			ImVec2 p = ImGui::GetCursorScreenPos();
			bool h = ImGui::IsMouseHoveringRect(p, p + size) || IsWidgetFocused(GetID(idStr.c_str()));

			ImVec4 tint = GetHighlightTint(true, h, false);

			if (DrawTransparentButton(idStr.c_str(), tex, size, tint))
				clicked = true;
		};
		AlignedWidgetLayout(label, alignFar, labelLeft, size.x, DrawContent, size.y);
		if (clicked)
			RE::PlaySound("UIMenuOK");
		return clicked;
	}

	bool ImGui::Hotkey(const char* label, std::uint32_t key, std::int32_t m1, std::int32_t m2, bool alignFar, bool labelLeft, bool flashing)
	{
		bool clicked = false;
		std::string baseId = std::format("##HOTKEY_{}", label);
		auto* iconFont = MANAGER(IconFont);

		ImGuiContext& g = *GImGui;
		const float frameH = ImGui::GetFrameHeight();
		const float spacing = g.Style.ItemInnerSpacing.x;

		const auto* kIcon = iconFont->GetIcon(key);
		const auto* m1Icon = (m1 != -1) ? iconFont->GetIcon(static_cast<uint32_t>(m1)) : nullptr;
		const auto* m2Icon = (m2 != -1) ? iconFont->GetIcon(static_cast<uint32_t>(m2)) : nullptr;

		// Generic item to unify rendering loop
		struct RenderItem
		{
			enum Type
			{
				kIcon,
				kText
			} type;
			const IconFont::IconTexture* icon = nullptr;
			const char* text = nullptr;
			const char* idSuffix = nullptr;
			ImVec2 size;
		};

		// Build list: Anchor (Key) -> Leftward items
		std::vector<RenderItem> items;
		items.reserve(5);

		auto AddIcon = [&](const IconFont::IconTexture* icon, const char* suffix) {
			if (icon)
				items.push_back({ RenderItem::kIcon, icon, nullptr, suffix, icon->size });
		};

		auto AddText = [&](const char* text) {
			items.push_back({ RenderItem::kText, nullptr, text, nullptr, ImGui::CalcTextSize(text) });
		};

		// 1. Primary Key (The Anchor)
		if (kIcon) {
			AddIcon(kIcon, "key");
		} else {
			AddText("None");
		}

		// 2. Mod 2 ( Grows Left )
		if (m2Icon) {
			AddText("+");
			AddIcon(m2Icon, "m2");
		}

		// 3. Mod 1 ( Grows Left )
		if (m1Icon) {
			AddText("+");
			AddIcon(m1Icon, "m1");
		}

		// --- Rendering Phase ---

		// Pass just the primary key width to AlignedWidgetLayout
		float anchorWidth = items[0].size.x;

		auto DrawContent = [&]() {
			ImGuiID id = ImGui::GetID(baseId.c_str());
			const float lineTop = ImGui::GetCursorPosY();

			// AlignedWidgetLayout starts cursor where the primary key should be centered
			float currentX = ImGui::GetCursorPosX();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

			for (size_t i = 0; i < items.size(); ++i) {
				const auto& item = items[i];

				// Move the cursor leftwards for modifiers, leaving the primary key at the initial (centered) currentX
				if (i > 0) {
					currentX -= (item.size.x + spacing);
				}

				// Vertical Center Logic
				float offY = (frameH - item.size.y) * 0.5f;

				ImGui::SetCursorPosX(currentX);
				ImGui::SetCursorPosY(lineTop + offY);

				if (item.type == RenderItem::kIcon) {
					ImVec2 p = ImGui::GetCursorScreenPos();
					bool isHovered = ImGui::IsMouseHoveringRect(p, { p.x + item.size.x, p.y + item.size.y }) || IsWidgetFocused(id);

					ImVec4 tint;
					if (flashing) {
						float alpha = 0.4f + (0.6f * (float)fabs(sin(ImGui::GetTime() * 5.0f)));
						tint = ImVec4(1.0f, 0.8f, 0.2f, alpha);
					} else {
						tint = GetHighlightTint(true, isHovered, false);
					}

					if (DrawTransparentButton(std::format("##{}", item.idSuffix).c_str(), (void*)item.icon->srView.Get(), item.size, tint)) {
						clicked = true;
					}
				} else {
					ImGui::TextDisabled("%s", item.text);
				}
			}

			ImGui::PopStyleVar();
			
			ImGui::SetCursorPosY(lineTop + frameH);
			ImGui::Dummy(ImVec2(0.0f, 0.0f));
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, anchorWidth, DrawContent, frameH);

		if (clicked)
			RE::PlaySound("UIMenuFocus");
		return clicked;
	}

	bool DragScalarEx(const char* label, ImGuiDataType type, void* data, float speed, const void* min, const void* max, const char* fmt, ImGuiSliderFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		float w = CalcItemWidth();
		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, GetFrameHeight()));

		const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
		ItemSize(bb);
		if (!ItemAdd(bb, window->GetID(label), &bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiID id = window->GetID(label);

		bool h = ItemHoverable(bb, id, g.LastItemData.ItemFlags);
		bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);

		// Logic for Activating Input via Click/Gamepad
		if (!temp_input_is_active) {
			const bool mouse_clicked = h && IsMouseClicked(0, ImGuiInputFlags_None, id);
			const bool make_active = (mouse_clicked || g.NavActivateId == id);

			if (make_active && temp_input_allowed) {
				if (mouse_clicked && g.IO.KeyCtrl)
					temp_input_is_active = true;
			}

			if (make_active && !temp_input_is_active) {
				SetActiveID(id, window);
				SetFocusID(id, window);
				FocusWindow(window);
				g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
			}
		}

		if (temp_input_is_active) {
			return TempInputScalar(bb, id, label, type, data, fmt, min, max);
		}

		bool active = (g.ActiveId == id);

		RenderFrame(bb.Min, bb.Max, GetColorU32(active ? ImGuiCol_FrameBgActive : h ? ImGuiCol_FrameBgHovered :
																					  ImGuiCol_FrameBg),
			true, ImGui::GetStyle().FrameRounding);
		DrawWidgetBorder(window->DrawList, bb, active || h || IsWidgetFocused(id), ImGui::GetStyle().FrameRounding);

		bool changed = DragBehavior(id, type, data, speed, min, max, fmt, flags);
		if (changed)
			MarkItemEdited(id);

		bool dim = MANAGER(Input)->IsInputGamepad() && !IsWidgetFocused(id);
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));

		char buf[64];
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

		if (dim)
			PopStyleColor();
		return changed;
	}

	bool ThinSliderScalar(const char* label, ImGuiDataType type, void* data, const void* min, const void* max, const char* fmt, ImGuiSliderFlags flags, float thick)
	{
		ImGuiWindow* window = GetCurrentWindow();
		float w = CalcItemWidth();
		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, GetFrameHeight()));

		const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
		ItemSize(bb);
		if (!ItemAdd(bb, window->GetID(label), &bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiID id = window->GetID(label);

		bool h = ItemHoverable(bb, id, g.LastItemData.ItemFlags);
		bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);

		// Logic for Activating Slider via Click/Gamepad
		if (!temp_input_is_active) {
			const bool mouse_clicked = h && IsMouseClicked(0, ImGuiInputFlags_None, id);
			const bool make_active = (mouse_clicked || g.NavActivateId == id);

			if (make_active && temp_input_allowed) {
				if (mouse_clicked && g.IO.KeyCtrl)
					temp_input_is_active = true;
			}

			if (make_active && !temp_input_is_active) {
				SetActiveID(id, window);
				SetFocusID(id, window);
				FocusWindow(window);
				g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
			}
		}

		if (temp_input_is_active) {
			return TempInputScalar(bb, id, label, type, data, fmt, min, max);
		}

		ImRect grab;
		bool changed = SliderBehavior(bb, id, type, data, min, max, fmt, flags, &grab);
		if (changed)
			MarkItemEdited(id);

		ImRect track = bb;
		float s = track.GetHeight() * (1.0f - thick) * 0.5f;
		track.Min.y += s;
		track.Max.y -= s;
		bool active = (g.ActiveId == id);

		window->DrawList->AddRectFilled(track.Min, track.Max, GetColorU32(active ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
		DrawWidgetBorder(window->DrawList, track, IsWidgetFocused(id) || active, ImGui::GetStyle().FrameRounding);

		if (grab.Max.x > grab.Min.x)
			window->DrawList->AddRectFilled(grab.Min, grab.Max, GetColorU32(active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), ImGui::GetStyle().GrabRounding);

		bool dim = MANAGER(Input)->IsInputGamepad() && !IsWidgetFocused(id);
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));

		char buf[64];
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

		if (dim)
			PopStyleColor();
		return changed;
	}

	static std::map<RE::FormType, FormComboBoxFiltered<RE::TESForm>> s_FormCaches;
	void ClearFormCaches() { s_FormCaches.clear(); }
	bool ComboForm(const char* label, RE::FormID* currentFormID, RE::FormType formType)
	{
		auto [it, inserted] = s_FormCaches.try_emplace(formType, label);
		it->second.InitForms(formType);
		bool changed = false;

		it->second.GetFormResultFromCombo([&](RE::TESForm* form) { if (form) { *currentFormID = form->GetFormID(); changed = true; } });

		return changed;
	}

	bool CollapsingHeaderIcon(const char* label, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID id = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);

		// Calculate total layout size
		ImVec2 pos = window->DC.CursorPos;
		float frameHeight = CalcTextSize(label).y + GImGui->Style.FramePadding.y * 2.0f;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, frameHeight));

		ItemSize(bb);
		if (!ItemAdd(bb, id))
			return is_open;

		bool h, held;
		if (ButtonBehavior(bb, id, &h, &held)) {
			is_open = !is_open;
			window->DC.StateStorage->SetInt(id, is_open);
			RE::PlaySound(is_open ? "UIMenuFocus" : "UIMenuCancel");
		}

		// Draw Background
		if (h || is_open)
			RenderFrame(bb.Min, bb.Max, GetColorU32(is_open ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered), true, GImGui->Style.FrameRounding);

		// Draw Arrow Icon using helper
		DrawTreeIcon(window->DrawList, { bb.Min.x + GImGui->Style.ItemInnerSpacing.x, bb.Min.y }, frameHeight, is_open, h);

		float fontSize = GImGui->FontSize;
		float textOff = fontSize + GImGui->Style.ItemInnerSpacing.x * 2.0f;

		// Vertically center text
		ImVec2 textSize = CalcTextSize(label);
		float textY = bb.Min.y + (frameHeight - textSize.y) * 0.5f;

		RenderText({ bb.Min.x + textOff, textY }, label);
		return is_open;
	}

	bool TreeNodeIcon(const char* label, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID id = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);

		ImVec2 pos = window->DC.CursorPos;
		float frameHeight = CalcTextSize(label).y + GImGui->Style.FramePadding.y * 2.0f;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, frameHeight));

		ItemSize(bb);
		if (!ItemAdd(bb, id)) {
			if (is_open)
				TreePush(label);
			return is_open;
		}

		bool h, held;
		if (ButtonBehavior(bb, id, &h, &held, flags)) {
			is_open = !is_open;
			window->DC.StateStorage->SetInt(id, is_open);
			RE::PlaySound(is_open ? "UIMenuFocus" : "UIMenuCancel");
		}
		if (h)
			RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_HeaderHovered), false, GImGui->Style.FrameRounding);

		// Draw Arrow Icon using helper
		float padding = GImGui->Style.ItemInnerSpacing.x;
		DrawTreeIcon(window->DrawList, { pos.x + padding, pos.y }, frameHeight, is_open, h);

		// Align text
		float fontSize = GImGui->FontSize;
		float textOff = fontSize + padding * 3.0f;  // Indent text past icon
		float textY = bb.Min.y + (frameHeight - CalcTextSize(label).y) * 0.5f;

		RenderText({ pos.x + textOff, textY }, label);

		if (is_open)
			TreePush(label);
		return is_open;
	}

	std::tuple<bool, bool, bool> CenteredTextWithArrows(const char* label, std::string_view centerText)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID id = window->GetID(label);
		float w = CalcItemWidth();
		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, ImGui::GetFrameHeight()));
		ItemSize(bb);
		if (!ItemAdd(bb, id))
			return { false, false, false };
		bool hovered = IsWidgetFocused(id) || (MANAGER(Input)->CanNavigateWithMouse() && ItemHoverable(bb, id, GImGui->LastItemData.ItemFlags));

		bool dim = MANAGER(Input)->IsInputGamepad() && !hovered;
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));
		auto largeFont = MANAGER(IconFont)->GetLargeFont();
		PushFont(largeFont, largeFont ? largeFont->LegacySize : ImGui::GetStyle().FontSizeBase);
		RenderTextClipped(bb.Min, bb.Max, centerText.data(), NULL, NULL, { 0.5f, 0.5f });
		PopFont();
		if (dim)
			PopStyleColor();

		auto lA = MANAGER(IconFont)->GetStepperLeft();
		auto rA = MANAGER(IconFont)->GetStepperRight();
		ImU32 col = hovered ? IM_COL32_WHITE : GetUserStyleColorU32(USER_STYLE::kIconDisabled);
		return { hovered, AlignedImage(lA->srView.Get(), lA->size, bb.Min, bb.Max, { 0, 0.5f }, col), AlignedImage(rA->srView.Get(), rA->size, bb.Min, bb.Max, { 1.0f, 0.5f }, col) };
	}

	bool SelectableStyled(const char* label, bool selected, int flags, const ImVec2& size)
	{
		bool pressed = ImGui::Selectable(label, selected, flags, size);
		if (pressed)
			RE::PlaySound("UIMenuFocus");
		return pressed;
	}

	template <typename TWidgetFunc>
	bool OutsetFramedWidget(const char* label, TWidgetFunc drawWidget)
	{
		std::string id = "##"s + label;
		LeftAlignedTextImpl(label, id);

		float borderSize = ImGui::GetStyle().FrameBorderSize;
		float rounding = ImGui::GetStyle().FrameRounding;

		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

		bool result = drawWidget(id.c_str());

		PopStyleVar();
		PopStyleColor(3);

		if (borderSize > 0.0f) {
			ImRect bb = GImGui->LastItemData.Rect;
			bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
			bool active = ImGui::IsItemActive();
			bool focused = IsWidgetFocused(id);

			ImU32 borderColor = (hovered || active || focused) ?
			                        GetUserStyleColorU32(USER_STYLE::kSliderBorderActive) :
			                        GetUserStyleColorU32(USER_STYLE::kSliderBorder);

			float outset = borderSize * 0.5f;
			ImRect drawBb = { bb.Min - ImVec2(outset, outset), bb.Max + ImVec2(outset, outset) };

			ImGui::GetWindowDrawList()->AddRect(drawBb.Min, drawBb.Max, borderColor, rounding, 0, borderSize);
		}
		ActivateOnHover();

		return result;
	}

	bool ColorEdit3Styled(const char* label, float col[3], int flags)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ImGui::ColorEdit3(id, col, flags | ImGuiColorEditFlags_NoLabel);
		});
		if (IsItemActivated())
			RE::PlaySound("UIMenuFocus");
		return res;
	}

	bool ColorEdit4Styled(const char* label, float col[4], int flags)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ImGui::ColorEdit4(id, col, flags | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		});
		if (IsItemActivated())
			RE::PlaySound("UIMenuFocus");
		return res;
	}

	bool InputTextStyled(const char* label, char* buf, size_t buf_size, int flags)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
			PushStyleColor(ImGuiCol_Text, GetUserStyleColorU32(USER_STYLE::kComboBoxText));
			bool internalRes = ImGui::InputText(id, buf, buf_size, flags);
			PopStyleColor(2);
			return internalRes;
		});
		if (IsItemActivated())
			RE::PlaySound("UIMenuFocus");
		return res;
	}

	bool DragFloat2Styled(const char* label, float v[2], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ImGui::DragFloat2(id, v, speed, min, max, fmt);
		});
		if (res)
			RE::PlaySound("UIMenuPrevNext");
		return res;
	}

	bool DragFloat3Styled(const char* label, float v[3], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ImGui::DragFloat3(id, v, speed, min, max, fmt);
		});
		if (res)
			RE::PlaySound("UIMenuPrevNext");
		return res;
	}

	bool DragFloat4Styled(const char* label, float v[4], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ImGui::DragFloat4(id, v, speed, min, max, fmt);
		});
		if (res)
			RE::PlaySound("UIMenuPrevNext");
		return res;
	}

	void Stepper(const char* label, const char* text, bool* outLeft, bool* outRight)
	{
		LeftLabel(label);
		auto [hovered, l, r] = CenteredTextWithArrows(label, text);
		*outLeft = false;
		*outRight = false;
		if (hovered || IsWidgetFocused(label)) {
			bool pL = l || IsKeyPressed(ImGuiKey_LeftArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadLeft, false);
			bool pR = r || IsKeyPressed(ImGuiKey_RightArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadRight, false);
			if (pL) {
				*outLeft = true;
				RE::PlaySound("UIMenuPrevNext");
			}
			if (pR) {
				*outRight = true;
				RE::PlaySound("UIMenuPrevNext");
			}
		}
	}
}
