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

	void AlignedWidgetLayout(const char* label, bool alignFar, bool labelLeft, std::function<void()> drawContent, float targetHeight = -1.0f)
	{
		ImGui::BeginGroup();
		ImGui::PushID(label);

		ImGuiContext& g = *GImGui;
		float availWidth = ImGui::GetContentRegionAvail().x;
		float startX = ImGui::GetCursorPosX();
		float startY = ImGui::GetCursorPosY();

		float splitPoint = startX + floorf(availWidth * 0.65f);

		std::string_view labelView(label);
		auto doubleHash = labelView.find("##");
		if (doubleHash != std::string_view::npos)
			labelView = labelView.substr(0, doubleHash);

		auto DrawLabel = [&]() {
			if (targetHeight > 0.0f) {
				float offY = (targetHeight - g.FontSize) * 0.5f;
				if (offY > 0.0f)
					ImGui::SetCursorPosY(startY + offY);
			}
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(labelView.data(), labelView.data() + labelView.size());
		};

		if (alignFar) {
			if (labelLeft) {
				ImGui::SetCursorPosX(startX);
				DrawLabel();
				ImGui::SameLine();
				if (targetHeight > 0.0f)
					ImGui::SetCursorPosY(startY);
				ImGui::SetCursorPosX(splitPoint);
				drawContent();
			} else {
				ImGui::SetCursorPosX(startX);
				if (targetHeight > 0.0f)
					ImGui::SetCursorPosY(startY);
				drawContent();
				ImRect lastItemRect = g.LastItemData.Rect;
				float contentW = lastItemRect.Max.x - lastItemRect.Min.x;
				ImGui::SameLine();
				ImVec2 textSize = ImGui::CalcTextSize(labelView.data(), labelView.data() + labelView.size());
				ImGui::SetCursorPosX((splitPoint + contentW) - textSize.x);
				DrawLabel();
			}
		} else {
			if (labelLeft) {
				DrawLabel();
				ImGui::SameLine(0, g.Style.ItemInnerSpacing.x);
				if (targetHeight > 0.0f)
					ImGui::SetCursorPosY(startY);
				drawContent();
			} else {
				if (targetHeight > 0.0f)
					ImGui::SetCursorPosY(startY);
				drawContent();
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

	static void DrawLastItemBorder(float rounding = 0.0f)
	{
		ImGuiContext& g = *GImGui;
		ImRect bb = g.LastItemData.Rect;
		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();
		bool focused = ImGui::IsItemFocused();
		DrawWidgetBorder(ImGui::GetWindowDrawList(), bb, hovered || active || focused, rounding);
	}

	void DrawTabBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered)
	{
		float scale = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();
		float thick = 2.0f * scale;
		float rnd = ImGui::GetStyle().TabRounding;

		ImU32 col = isActiveOrHovered ? GetUserStyleColorU32(USER_STYLE::kTabBorderActive) : GetUserStyleColorU32(USER_STYLE::kTabBorder);

		drawList->PathLineTo({ bb.Min.x, bb.Max.y });
		drawList->PathArcToFast({ bb.Min.x + rnd, bb.Min.y + rnd }, rnd, 6, 9);
		drawList->PathArcToFast({ bb.Max.x - rnd, bb.Min.y + rnd }, rnd, 9, 12);
		drawList->PathLineTo({ bb.Max.x, bb.Max.y });
		drawList->PathStroke(col, 0, thick);
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
		AlignedWidgetLayout(label, alignFar, labelLeft, DrawContent, icon->size.y);
		if (selected)
			RE::PlaySound("UIMenuFocus");
		return selected;
	}

	bool ToggleButton(const char* label, bool* v, bool alignFar, bool labelLeft)
	{
		bool pressed = false;
		std::string idStr = std::format("##{}", label);

		auto DrawContent = [&]() {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;

			ImGuiContext& g = *GImGui;
			const ImGuiID id = window->GetID(idStr.c_str());
			float scale = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();

			float frameH = ImGui::GetFrameHeight();
			float width = frameH * 1.55f;

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

		AlignedWidgetLayout(label, alignFar, labelLeft, DrawContent);
		if (pressed)
			RE::PlaySound("UIMenuFocus");
		return pressed;
	}

	bool ComboWithFilter(const char* label, int* current_item, const std::vector<std::string>& items, int popup_max_height_in_items)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

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

		// Explicit constraint calculation
		float filterBoxH = GetFrameHeight();
		float listH_Max = GetTextLineHeightWithSpacing() * popup_max_height_in_items;
		float constraintH = filterBoxH + g.Style.ItemSpacing.y + listH_Max + (g.Style.WindowPadding.y * 2.0f) + 20.0f;

		if (!(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
			SetNextWindowSizeConstraints({ width, 0.0f }, { width, constraintH });
		}

		const char* preview = (*current_item >= 0 && *current_item < items.size()) ? items[*current_item].c_str() : "";

		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));

		ImDrawList* parentDrawList = GetWindowDrawList();

		bool isOpen = BeginCombo(idStr.c_str(), preview, ImGuiComboFlags_NoArrowButton);
		PopStyleColor(3);

		// Detect if opened Upwards
		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}

		DrawDropdownIcon(parentDrawList, { widgetPos.x + width - GetFrameHeight(), widgetPos.y }, { GetFrameHeight(), GetFrameHeight() }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { widgetPos, widgetPos + ImVec2(width, GetFrameHeight()) }, isOpen || IsItemHovered() || IsWidgetFocused(id));

		if (!isOpen)
			return false;

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

		EndCombo();
		return changed;
	}

	bool ComboStyled(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height_in_items)
	{
		std::string idStr = "##"s + label;
		LeftAlignedTextImpl(label, idStr);
		ImVec2 widgetPos = GetCursorScreenPos();
		float width = CalcItemWidth();

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;
		SetNextWindowSizeConstraints({ width, 0 }, { width, CalcMaxPopupHeightFromItemCount(popup_max_height_in_items) });

		PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));

		ImDrawList* parentDrawList = GetWindowDrawList();

		const char* preview = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";

		parentDrawList->AddRectFilled(widgetPos, widgetPos + ImVec2(width, GetFrameHeight()), GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), ImGui::GetStyle().FrameRounding);

		bool isOpen = BeginCombo(idStr.c_str(), preview, ImGuiComboFlags_NoArrowButton);
		PopStyleColor(4);

		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}


		DrawDropdownIcon(parentDrawList, { widgetPos.x + width - GetFrameHeight(), widgetPos.y }, { GetFrameHeight(), GetFrameHeight() }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { widgetPos, widgetPos + ImVec2(width, GetFrameHeight()) }, isOpen || IsItemHovered() || IsWidgetFocused(GetID(idStr.c_str())), ImGui::GetStyle().FrameRounding);

		bool changed = false;
		if (isOpen) {
			for (int i = 0; i < items_count; i++) {
				if (Selectable(items[i], *current_item == i)) {
					*current_item = i;
					changed = true;
					RE::PlaySound("UIMenuFocus");
				}
				if (*current_item == i)
					SetItemDefaultFocus();
			}
			EndCombo();
		}
		return changed;
	}

	bool BeginTabItemEx(const char* label, ImGuiTabItemFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID id = ImGui::GetID(label);
		ImGuiID storageKey = ImGui::GetID("##LastActiveTab");
		ImGuiID lastActive = window->StateStorage.GetInt(storageKey, 0);

		bool wasActive = (lastActive == id);
		if (!wasActive)
			PushStyleColor(ImGuiCol_Text, GetStyleColorVec4(ImGuiCol_TextDisabled));

		PushStyleVar(ImGuiStyleVar_FramePadding, { ImGui::GetStyle().FramePadding.x, 0.0f });
		bool active = BeginTabItem(label, nullptr, flags);
		PopStyleVar();

		if (!wasActive)
			PopStyleColor();
		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();
		float inset = 2.0f;
		ImRect insetRect = {
			ImVec2(min.x + inset, min.y + inset),
			ImVec2(max.x - inset, max.y - inset)
		};

		DrawTabBorder(GetWindowDrawList(), insetRect, active || IsItemHovered());

		if (active)
			window->StateStorage.SetInt(storageKey, id);
		if (active && ActivateOnHover())
			RE::PlaySound("UIJournalTabsSD");
		return active;
	}

	bool OutlineButton(const char* label, bool* wasFocused)
	{
		ImGuiWindow* window = GetCurrentWindow();
		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetUserScale();
		float rounding = GetUserStyleVar(USER_STYLE::kButtonRounding) * scale;

		ImVec2 sz = CalcTextSize(label) + ImGui::GetStyle().FramePadding * 2.0f;
		ImRect bb(window->DC.CursorPos, window->DC.CursorPos + sz);
		ItemSize(sz);
		if (!ItemAdd(bb, window->GetID(label)))
			return false;

		bool h, held;
		bool p = ButtonBehavior(bb, window->GetID(label), &h, &held);

		ImU32 frameCol = GetColorU32(h || held ? ImGuiCol_ButtonHovered : ImGuiCol_Button);

	    RenderFrame(bb.Min, bb.Max, frameCol, true, rounding); 
		DrawWidgetBorder(window->DrawList, bb, h, rounding);

		bool dim = MANAGER(Input)->IsInputGamepad() && !h;
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));
		RenderTextClipped(bb.Min, bb.Max, label, NULL, NULL, { 0.5f, 0.5f });
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

			ImVec4 tint = GetInteractiveColorVec4(ImGuiCol_Text, ImGuiCol_ButtonHovered, ImGuiCol_ButtonActive);
			if (!h)
				tint = GetUserStyleColorVec4(USER_STYLE::kTextButton);
			if (h)
				tint = GetUserStyleColorVec4(USER_STYLE::kTextHovered);

			if (DrawTransparentButton(idStr.c_str(), tex, size, tint))
				clicked = true;
		};
		AlignedWidgetLayout(label, alignFar, labelLeft, DrawContent, size.y);
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

		auto DrawContent = [&]() {
			ImGuiID id = ImGui::GetID(baseId.c_str());

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
			const float lineTop = ImGui::GetCursorPosY();

			// We start at the current cursor X (The "Split Point")
			// The Primary Key sits here. Modifiers grow to the left (negative X).
			float anchorX = ImGui::GetCursorPosX();
			float currentX = anchorX;

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

			for (size_t i = 0; i < items.size(); ++i) {
				const auto& item = items[i];

				// If this is NOT the first item (Key), we must move LEFT to make space for it
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

			// Restore Y baseline for layout system
			ImGui::SetCursorPosY(lineTop + frameH);
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, DrawContent, frameH);

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
			RenderFrame(bb.Min, bb.Max, GetColorU32(ImGuiCol_HeaderHovered), false);

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
		PushFont(MANAGER(IconFont)->GetLargeFont());
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

	bool ColorEdit3Styled(const char* label, float col[3], int flags)
	{
		std::string id = "##"s + label;
		LeftAlignedTextImpl(label, id);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_Border, GetUserStyleColorU32(USER_STYLE::kSliderBorder));
		bool result = ImGui::ColorEdit3(id.c_str(), col, flags | ImGuiColorEditFlags_NoLabel);
		DrawLastItemBorder(ImGui::GetStyle().FrameRounding);
		if (IsItemActivated())
			RE::PlaySound("UIMenuFocus");
		ActivateOnHover();
		PopStyleColor(4);
		return result;
	}

	bool ColorEdit4Styled(const char* label, float col[4], int flags)
	{
		std::string id = "##"s + label;
		LeftAlignedTextImpl(label, id);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_Border, GetUserStyleColorU32(USER_STYLE::kSliderBorder));
		bool result = ImGui::ColorEdit4(id.c_str(), col, flags | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		DrawLastItemBorder(ImGui::GetStyle().FrameRounding);
		if (IsItemActivated())
			RE::PlaySound("UIMenuFocus");
		ActivateOnHover();
		PopStyleColor(4);
		return result;
	}

	bool InputTextStyled(const char* label, char* buf, size_t buf_size, int flags)
	{
		std::string id = "##"s + label;
		LeftAlignedTextImpl(label, id);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_Text, GetUserStyleColorU32(USER_STYLE::kComboBoxText));
		bool res = ImGui::InputText(id.c_str(), buf, buf_size, flags);
		DrawLastItemBorder(ImGui::GetStyle().FrameRounding);
		if (IsItemActivated())
			RE::PlaySound("UIMenuFocus");
		PopStyleColor(2);
		return res;
	}

	bool DragFloat2Styled(const char* label, float v[2], float speed, float min, float max, const char* fmt)
	{
		std::string id = "##"s + label;
		LeftAlignedTextImpl(label, id);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		bool result = ImGui::DragFloat2(id.c_str(), v, speed, min, max, fmt);
		DrawLastItemBorder(ImGui::GetStyle().FrameRounding);
		if (result)
			RE::PlaySound("UIMenuPrevNext");
		ActivateOnHover();
		PopStyleColor(3);
		return result;
	}

	bool DragFloat3Styled(const char* label, float v[3], float speed, float min, float max, const char* fmt)
	{
		std::string id = "##"s + label;
		LeftAlignedTextImpl(label, id);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		bool result = ImGui::DragFloat3(id.c_str(), v, speed, min, max, fmt);
		DrawLastItemBorder(ImGui::GetStyle().FrameRounding);
		if (result)
			RE::PlaySound("UIMenuPrevNext");
		ActivateOnHover();
		PopStyleColor(3);
		return result;
	}

	bool DragFloat4Styled(const char* label, float v[4], float speed, float min, float max, const char* fmt)
	{
		std::string id = "##"s + label;
		LeftAlignedTextImpl(label, id);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		bool result = ImGui::DragFloat4(id.c_str(), v, speed, min, max, fmt);
		DrawLastItemBorder(ImGui::GetStyle().FrameRounding);
		if (result)
			RE::PlaySound("UIMenuPrevNext");
		ActivateOnHover();
		PopStyleColor(3);
		return result;
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
