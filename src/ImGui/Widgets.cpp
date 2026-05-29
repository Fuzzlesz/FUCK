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
			drawList->AddImage(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()), p_min, p_max, { 0, 0 }, { 1, 1 }, color);
		} else if (direction == IconDirection::kDown) {
			// Rotate 90 CW (v): UVs {0,1}, {0,0}, {1,0}, {1,1}
			drawList->AddImageQuad(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 }, color);
		} else if (direction == IconDirection::kLeft) {
			// Mirror Horizontal (<): Swap U {1,0} -> {0,0}
			drawList->AddImageQuad(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, color);
		} else if (direction == IconDirection::kUp) {
			// Rotate 270 CW / 90 CCW (^): UVs {1,0}, {1,1}, {0,1}, {0,0}
			drawList->AddImageQuad(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()),
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

	void DrawTreeIcon(ImDrawList* drawList, const ImVec2& pos, float frameHeight, bool isOpen, bool isHovered, float baseIconSize = 30.0f)
	{
		static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
		if (!iconArrow)
			return;

		ImU32 col       = ImGui::GetDynamicTextColor(isHovered);
		float uiScale   = ImGui::Renderer::GetResolutionScale();
		float userScale = (FUCKMan::GetSingleton()->GetActiveScale());
		float aspect    = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

		auto ap = ImGui::CalcArrowIconParams(aspect, isOpen, frameHeight, baseIconSize * uiScale, userScale);

		// Center horizontally within its own max dimension to prevent shifting on open/close
		float maxIconDim  = std::max(ap.drawSize.x, ap.drawSize.y);
		float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

		ImVec2 drawPos = { pos.x + iconOffsetX, pos.y + ap.offsetY };
		ImGui::DrawArrowIcon(drawList, drawPos, ap.drawSize, col,
			isOpen ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
	}

	void DrawDropdownIcon(ImDrawList* drawList, ImVec2 bPos, ImVec2 bSize, bool isOpen, bool opensUp, bool isHovered)
	{
		static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
		if (!iconArrow)
			return;

		ImU32 col       = ImGui::GetDynamicTextColor(isHovered || isOpen);
		float uiScale   = ImGui::Renderer::GetResolutionScale();
		float userScale = (FUCKMan::GetSingleton()->GetActiveScale());
		float aspect    = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

		auto ap = ImGui::CalcArrowIconParams(aspect, isOpen, bSize.y, 30.0f * uiScale, userScale);

		ImVec2 iconPos = {
			bPos.x + (bSize.x - ap.drawSize.x) * 0.5f,
			bPos.y + ap.offsetY
		};

		// Combo/Window: Closed = Left, Open = Down/Up
		ImGui::IconDirection dir;
		if (isOpen) {
			dir = opensUp ? ImGui::IconDirection::kUp : ImGui::IconDirection::kDown;
		} else {
			dir = ImGui::IconDirection::kLeft;
		}

		ImGui::DrawArrowIcon(drawList, iconPos, ap.drawSize, col, dir);
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
		bool result = ImGui::ImageButton(id, reinterpret_cast<ImTextureID>(tex), size, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, tint);
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(4);
		return result;
	}

	void AlignedWidgetLayout(const char* label, bool alignFar, bool labelLeft, float contentWidth, std::function<void()> drawContent, float targetHeight = -1.0f)
	{
		ImGui::BeginGroup();
		ImGui::PushID(label);

		ImGuiContext& g      = *GImGui;
		float         startX = ImGui::GetCursorPosX();
		float         startY = ImGui::GetCursorPosY();

		float fullAvailX = ImGui::GetContentRegionAvail().x;
		auto& style      = ImGui::Styles::GetSingleton()->user;

		float rightPaneStart = startX + (fullAvailX * style.widgetSplit) + g.Style.ItemInnerSpacing.x;
		float rightPaneEnd   = startX + fullAvailX;

		// If you used SetNextItemWidth(-x), shrink the right pane bounds
		if (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) {
			float reqW = g.NextItemData.Width;
			if (reqW < 0.0f) {
				rightPaneEnd += reqW;
			}
			// Consume the flag for our custom widgets so it doesn't leak
			g.NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasWidth;
		}

		// Strip hidden ## IDs from the label before measuring text
		std::string_view labelView(label);
		auto             doubleHash = labelView.find("##");
		if (doubleHash != std::string_view::npos)
			labelView = labelView.substr(0, doubleHash);

		float labelWidth = ImGui::CalcTextSize(labelView.data(), labelView.data() + labelView.size()).x;

		// Calculate true dynamic centering, supporting widgets (like Icons) that are taller than a standard frame
		float actualTargetH = targetHeight > 0.0f ? targetHeight : ImGui::GetFrameHeight();
		float textH         = ImGui::GetTextLineHeight();
		float offY          = std::floor((actualTargetH - textH) * style.labelAlign.y);

		bool  isFocused = ImGui::IsWidgetFocused(ImGui::GetID(label));
		bool  dim       = MANAGER(Input)->IsInputGamepad() && !isFocused;
		ImU32 textColor = dim ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : ImGui::GetColorU32(ImGuiCol_Text);

		// Natively draw the label to the screen without disrupting ImGui's internal cursor bounds.
		// This prevents SameLine() and ItemSize() desyncs.
		auto DrawLabel = [&](float xPos) {
			ImVec2 screenPos = ImGui::GetCursorScreenPos();
			screenPos.x      = std::floor(screenPos.x + (xPos - startX));
			screenPos.y      = std::floor(screenPos.y + std::max(0.0f, offY));
			ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), screenPos, textColor, labelView.data(), labelView.data() + labelView.size());
		};

		// Tighter gap globally for near-aligned widgets to closely mimic vanilla grouping
		float nearSpacing = std::max(1.0f, std::floor(g.Style.ItemInnerSpacing.x * 0.25f));
		// Near (Tightly Coupled) - Uses LabelAlign.X as a rigid spacer pushing the label away (Adds 0 to 50px of adjustable spacing)
		float labelSpacer = style.labelAlign.x * 50.0f * FUCKMan::GetSingleton()->GetActiveScale() * ImGui::Renderer::GetResolutionScale();

		float widgetX = startX;
		float maxX    = startX;

		if (alignFar) {
			// Center the widget inside the right-hand pane
			float rightPaneCenter = rightPaneStart + (rightPaneEnd - rightPaneStart) * 0.5f;
			float splitPoint      = rightPaneCenter - (contentWidth * 0.5f);

			if (labelLeft) {
				DrawLabel(startX);
				widgetX = splitPoint;
			} else {
				widgetX = startX;
				DrawLabel((splitPoint + contentWidth) - labelWidth);
			}
			maxX = rightPaneEnd;
		} else {
			// Pack the widget tightly against the label
			if (labelLeft) {
				DrawLabel(startX);
				widgetX = startX + labelWidth + nearSpacing + labelSpacer;
				maxX    = widgetX + contentWidth;
			} else {
				widgetX      = startX;
				float labelX = startX + contentWidth + nearSpacing + labelSpacer;
				DrawLabel(labelX);
				maxX = labelX + labelWidth;
			}
		}

		ImGui::SetCursorPosX(widgetX);
		ImGui::SetCursorPosY(startY);

		drawContent();

		// Explicitly register the furthest X bound so AutoResize windows don't clip
		ImGui::SetCursorPosX(maxX);
		ImGui::SetCursorPosY(startY);
		ImGui::ItemSize(ImVec2(0.0f, actualTargetH));

		ImGui::PopID();
		ImGui::EndGroup();
	}
}

namespace ImGui
{
	// =========================================================================================
	// CORE DRAWING & WIDGETS
	// =========================================================================================

	// Helper function for adjusting hovered sliders with WASD
	static bool ApplyWASDNudge(ImGuiDataType type, void* data, const void* min, const void* max, float speed)
	{
		float moveX = 0.0f;
		float moveY = 0.0f;

		if (ImGui::IsKeyPressed(ImGuiKey_A, true))
			moveX -= 1.0f;
		if (ImGui::IsKeyPressed(ImGuiKey_D, true))
			moveX += 1.0f;
		if (ImGui::IsKeyPressed(ImGuiKey_W, true))
			moveY += 1.0f;
		if (ImGui::IsKeyPressed(ImGuiKey_S, true))
			moveY -= 1.0f;

		if (moveX == 0.0f && moveY == 0.0f)
			return false;

		float stepSmall = (speed > 0.0f) ? speed : 1.0f;
		float stepLarge = stepSmall * 10.0f;

		float totalMove = (moveX * stepSmall) + (moveY * stepLarge);

		if (type == ImGuiDataType_Float || type == ImGuiDataType_Double) {
			auto* v = static_cast<float*>(data);
			*v += totalMove;
			if (min && max) {
				float mn = *static_cast<const float*>(min);
				float mx = *static_cast<const float*>(max);
				if (mn < mx)
					*v = ImClamp(*v, mn, mx);
			}
			return true;
		} else if (type == ImGuiDataType_S32) {
			auto* v = static_cast<int*>(data);
			*v += static_cast<int>(totalMove);
			if (min && max) {
				int mn = *static_cast<const int*>(min);
				int mx = *static_cast<const int*>(max);
				if (mn < mx)
					*v = ImClamp(*v, mn, mx);
			}
			return true;
		}
		return false;
	}

	void DrawWidgetBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding)
	{
		float borderSize = ImGui::GetUserStyleVar(USER_STYLE::kButtonBorderSize);

		if (borderSize <= 0.0f)
			return;

		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();

		float tColor = std::max(1.0f, std::round(borderSize));
		float tBlack = std::max(1.0f, std::round(1.0f * scale));

		ImU32 col = isActiveOrHovered ?
		                GetUserStyleColorU32(USER_STYLE::kSliderBorderActive) :
		                GetUserStyleColorU32(USER_STYLE::kSliderBorder);

		// Outer Black: Expands strictly outwards
		float halfB = tBlack * 0.5f;
		drawList->AddRect(bb.Min - ImVec2(halfB, halfB), bb.Max + ImVec2(halfB, halfB), IM_COL32(0, 0, 0, 255), rounding + halfB, 0, tBlack);

		// Inner Colour: Shrinks strictly inwards
		float halfC = tColor * 0.5f;
		drawList->AddRect(bb.Min + ImVec2(halfC, halfC), bb.Max - ImVec2(halfC, halfC), col, rounding, 0, tColor);
	}

	bool CheckBox(const char* label, bool* a_toggle, bool alignFar, bool labelLeft)
	{
		bool        selected   = false;
		auto        icon       = MANAGER(IconFont)->GetCheckbox();
		auto        iconFilled = MANAGER(IconFont)->GetCheckboxFilled();
		std::string idStr      = std::format("##{}", label);

		float scale         = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
		float userIconScale = FUCKMan::GetSingleton()->IsIgnoringUserScale() ? 1.0f : ImGui::Styles::GetSingleton()->user.iconScale;

		// Apply the iconScale setting to match Hotkey sizing
		float visualH = std::round(36.0f * scale * userIconScale);
		float iconH   = visualH;
		float iconW   = iconH * (icon->imageSize.y > 0.0f ? (icon->imageSize.x / icon->imageSize.y) : 1.0f);

		float layoutH       = ImGui::GetFrameHeight();
		float actualLayoutH = std::max(layoutH, iconH);
		float offY          = std::floor((actualLayoutH - iconH) * 0.5f);

		auto DrawContent = [&]() {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiID id = window->GetID(idStr.c_str());
			ImVec2  p  = window->DC.CursorPos;

			// Register the full layout height as the hitbox
			ImRect bb(p, p + ImVec2(iconW, actualLayoutH));
			ItemSize(bb);
			if (!ItemAdd(bb, id))
				return;

			bool hovered, held;
			bool pressed = ButtonBehavior(bb, id, &hovered, &held);
			if (pressed) {
				*a_toggle = !*a_toggle;
				selected  = true;
			}

			ImVec4 tint = GetHighlightTint(*a_toggle, hovered || IsWidgetFocused(id), false);
			void*  tex  = *a_toggle ? iconFilled->srView.Get() : icon->srView.Get();

			ImVec2 drawPos(std::floor(p.x), std::floor(p.y + offY));
			window->DrawList->AddImage((ImTextureID)tex, drawPos, drawPos + ImVec2(iconW, iconH), ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(tint));
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, iconW, DrawContent, actualLayoutH);

		if (selected)
			RE::PlaySound("UIMenuFocus");
		return selected;
	}

	bool ToggleButton(const char* label, bool* v, bool alignFar, bool labelLeft)
	{
		bool        pressed = false;
		std::string idStr   = std::format("##{}", label);

		float scale         = ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
		float userIconScale = FUCKMan::GetSingleton()->IsIgnoringUserScale() ? 1.0f : ImGui::Styles::GetSingleton()->user.iconScale;

		// Apply the iconScale setting to match Hotkey sizing
		float visualH = std::round(36.0f * scale * userIconScale);
		float visualW = visualH * 1.35f;

		float layoutH       = ImGui::GetFrameHeight();
		float actualLayoutH = std::max(layoutH, visualH);
		float offY          = std::floor((actualLayoutH - visualH) * 0.5f);

		auto DrawContent = [&]() {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiContext& g  = *GImGui;
			const ImGuiID id = window->GetID(idStr.c_str());
			ImVec2        p  = window->DC.CursorPos;

			// Hitbox matches actualLayoutH to play nice with other widgets
			ImRect bb(p, p + ImVec2(visualW, actualLayoutH));
			ItemSize(bb);
			if (!ItemAdd(bb, id))
				return;

			bool hovered, held;
			if (ButtonBehavior(bb, id, &hovered, &held)) {
				*v      = !*v;
				pressed = true;
				MarkItemEdited(id);
			}

			// Handle smooth animation state
			float* t_anim     = window->DC.StateStorage->GetFloatRef(id, *v ? 1.0f : 0.0f);
			float  target     = *v ? 1.0f : 0.0f;
			float  anim_speed = 12.0f;
			float  dt         = g.IO.DeltaTime;
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
			bool        isFocused = IsWidgetFocused(id);
			bool        showFrame = isFocused && MANAGER(Input)->IsInputGamepad();

			ImU32 col_rail_fill = GetUserStyleColorU32(USER_STYLE::kToggleRailFilled);
			ImU32 col_knob_fill = GetUserStyleColorU32(USER_STYLE::kToggleKnob);
			ImU32 col_knob_ring = *v ? GetColorU32(ImGuiCol_Text) : (showFrame ? GetUserStyleColorU32(USER_STYLE::kWidgetToggleActive) : GetUserStyleColorU32(USER_STYLE::kSliderBorder));

			// Visual bounds for the toggle graphic, pushed down by offY
			ImRect visBB(p.x, p.y + offY, p.x + visualW, p.y + offY + visualH);

			if (showFrame) {
				draw_list->AddRect(visBB.Min, visBB.Max, GetColorU32(ImGuiCol_NavHighlight), ImGui::GetStyle().FrameRounding, 0, 2.0f * scale);
			}

			float  knobRadius = visualH * 0.28f;
			float  knobMinX   = visBB.Min.x + knobRadius + (2.0f * scale);
			float  knobMaxX   = visBB.Max.x - knobRadius - (2.0f * scale);
			float  knobX      = knobMinX + (t * (knobMaxX - knobMinX));
			ImVec2 knobCenter = { knobX, visBB.Min.y + (visualH * 0.5f) };

			float  railH   = visualH * 0.35f;
			ImVec2 railMin = { knobMinX, visBB.Min.y + (visualH - railH) * 0.5f };
			ImVec2 railMax = { knobMaxX, visBB.Min.y + (visualH + railH) * 0.5f };
			ImRect railBB(railMin, railMax);

			draw_list->AddRectFilled(railMin, railMax, col_rail_fill, ImGui::GetStyle().FrameRounding);
			DrawWidgetBorder(draw_list, railBB, hovered || isFocused, ImGui::GetStyle().FrameRounding);

			draw_list->AddCircleFilled(knobCenter, knobRadius, col_knob_fill);

			float ringThick  = 3.0f * scale;
			float ringRadius = knobRadius - (4.0f * scale);
			if (ringRadius > 0.5f) {
				draw_list->AddCircle(knobCenter, ringRadius, col_knob_ring, 0, ringThick);
			}
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, visualW, DrawContent, actualLayoutH);

		if (pressed)
			RE::PlaySound("UIMenuFocus");
		return pressed;
	}

	bool ComboWithFilter(const char* label, int* current_item, const std::vector<std::string>& items, int popup_max_height_in_items)
	{
		float scale = ImGui::Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());

		ImGuiContext& g      = *GImGui;
		ImGuiWindow*  window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float padX       = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float padY       = 7.0f * scale;

		PushStyleVar(ImGuiStyleVar_FramePadding, { padX, padY });
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Prevents double frame

		const bool  isHidden = std::string_view(label).starts_with("##");
		std::string idStr    = isHidden ? label : "##"s + label;
		if (!isHidden)
			LeftAlignedTextImpl(label, idStr);

		ImVec2 widgetPos = GetCursorScreenPos();
		float  width     = CalcItemWidth();

		struct State
		{
			char pattern[256] = { 0 };
		};
		static Map<ImGuiID, State> states;
		ImGuiID                    id = window->GetID(idStr.c_str());

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;

		float listH_Max   = GetTextLineHeightWithSpacing() * popup_max_height_in_items;
		float constraintH = GetFrameHeight() + g.Style.ItemSpacing.y + listH_Max + (g.Style.WindowPadding.y * 2.0f) + 20.0f;

		if (!(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
			SetNextWindowSizeConstraints({ width, 0.0f }, { width, constraintH });
		}

		const char* preview = (*current_item >= 0 && *current_item < static_cast<int>(items.size())) ? items[*current_item].c_str() : "";

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

		PopStyleVar(2);
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

		float activeScale = FUCKMan::GetSingleton()->GetActiveScale();
		float fontSize    = std::round(ImGui::GetStyle().FontSizeBase * activeScale * 2.0f) / 2.0f;
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
		bool                                filtering = states[id].pattern[0] != '\0';
		if (filtering) {
			for (int i = 0; i < static_cast<int>(items.size()); i++) {
				auto score = rapidfuzz::fuzz::partial_token_ratio(states[id].pattern, items[i].c_str());
				if (score >= 65.0)
					itemScoreVector.push_back({ i, score });
			}
			std::ranges::sort(itemScoreVector, [](const auto& a, const auto& b) { return b.second < a.second; });
		}

		bool changed    = false;
		int  show_count = filtering ? static_cast<int>(itemScoreVector.size()) : static_cast<int>(items.size());

		// Calculate height for list
		int heightInItems = show_count;
		if (heightInItems > popup_max_height_in_items)
			heightInItems = popup_max_height_in_items;
		if (heightInItems < 2)
			heightInItems = 2;

		float  listH = GetTextLineHeightWithSpacing() * heightInItems + g.Style.FramePadding.y * 2.0f;
		ImVec2 listSize(-FLT_MIN, listH);

		ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));

		if (BeginListBox("##List", listSize)) {
			for (int i = 0; i < show_count; i++) {
				int idx = filtering ? itemScoreVector[i].first : i;
				if (Selectable(items[idx].c_str(), *current_item == idx)) {
					*current_item = idx;
					changed       = true;
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
		float scale = ImGui::Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float padX       = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float padY       = 7.0f * scale;

		PushStyleVar(ImGuiStyleVar_FramePadding, { padX, padY });
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Prevents double frame

		std::string idStr = "##"s + label;
		LeftAlignedTextImpl(label, idStr);
		ImVec2 widgetPos = GetCursorScreenPos();
		float  width     = CalcItemWidth();

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

		PopStyleVar(2);
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
			float activeScale = FUCKMan::GetSingleton()->GetActiveScale();
			float fontSize    = std::round(ImGui::GetStyle().FontSizeBase * activeScale * 2.0f) / 2.0f;
			ImGui::PushFont(nullptr, fontSize);

			for (int i = 0; i < items_count; i++) {
				if (Selectable(items[i], *current_item == i)) {
					*current_item = i;
					changed       = true;
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
		float borderSize = ImGui::GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float round      = ImGui::GetStyle().TabRounding;
		float scale      = ImGui::Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());

		if (borderSize <= 0.0f)
			return;

		float tColor = std::max(1.0f, std::round(borderSize));
		float tBlack = std::max(1.0f, std::round(1.0f * scale));

		ImU32 col = isActiveOrHovered ?
		                GetUserStyleColorU32(USER_STYLE::kTabBorderActive) :
		                GetUserStyleColorU32(USER_STYLE::kTabBorder);

		auto buildPath = [&](ImDrawList* dl, float botY, float inset) {
			float r = std::max(0.0f, round - inset);
			dl->PathLineTo({ bb.Min.x + inset, botY });
			dl->PathArcToFast({ bb.Min.x + inset + r, bb.Min.y + inset + r }, r, 6, 9);
			dl->PathArcToFast({ bb.Max.x - inset - r, bb.Min.y + inset + r }, r, 9, 12);
			dl->PathLineTo({ bb.Max.x - inset, botY });
		};

		// Outer Black stroke (No bottom line)
		buildPath(drawList, bb.Max.y, -tBlack * 0.5f);
		drawList->PathStroke(IM_COL32(0, 0, 0, 255), 0, tBlack);

		// Inner Color stroke (No bottom line)
		buildPath(drawList, bb.Max.y, tColor * 0.5f);
		drawList->PathStroke(col, 0, tColor);
	}

	bool BeginTabItemEx(const char* label, ImGuiTabItemFlags flags)
	{
		ImGuiWindow* window     = GetCurrentWindow();
		ImGuiID      id         = ImGui::GetID(label);
		ImGuiID      storageKey = ImGui::GetID("##LastActiveTab");
		ImGuiID      lastActive = window->StateStorage.GetInt(storageKey, 0);

		bool  wasActive = (lastActive == id);
		float scale     = ImGui::Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());

		PushStyleColor(ImGuiCol_Tab, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabActive, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0, 0, 0, 0));

		// Hide the native text rendering by passing a transparent color
		PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));

		// Widen the X padding to match OutlineButtons, but LEAVE Y ALONE so we don't break the layout engine
		float padX = 8.0f * scale;
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, ImGui::GetStyle().FramePadding.y));

		window->DrawList->ChannelsSplit(2);
		window->DrawList->ChannelsSetCurrent(1);

		bool active = BeginTabItem(label, nullptr, flags);

		PopStyleVar();
		PopStyleColor(6);  // Transparent text + 5 tab colors

		// Switch to background channel for custom border
		window->DrawList->ChannelsSetCurrent(0);

		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();

		// Mathematically match OutlineButton's visual geometry
		ImVec2 textSize = CalcTextSize(label);
		float  padY     = 7.0f * scale;
		float  visualH  = textSize.y + (padY * 2.0f);
		float  logicalH = GetFrameHeight();
		float  offY     = (logicalH - visualH) * 0.5f;

		// This ideal bounding box is identical to OutlineButton in size and vertical position
		ImRect idealBb = { min.x, min.y + offY, max.x, min.y + offY + visualH };

		// Chop the bottom of the drawn rect at max.y so it rests flush on the separator line
		ImRect drawBb = { idealBb.Min.x, idealBb.Min.y, idealBb.Max.x, max.y };

		// Draw standard button fill (Only rounding the TOP corners so the bottom sits flush against the separator)
		float rounding = ImGui::GetStyle().TabRounding;
		window->DrawList->AddRectFilled(drawBb.Min, drawBb.Max, GetColorU32(ImGuiCol_Button), rounding, ImDrawFlags_RoundCornersTop);

		DrawTabBorder(window->DrawList, drawBb, active || IsItemHovered());

		// Switch to foreground channel to render our custom centered text
		window->DrawList->ChannelsSetCurrent(1);

		ImU32 textColor;
		if (active) {
			textColor = GetColorU32(ImGuiCol_Text);
		} else if (IsItemHovered()) {
			textColor = GetUserStyleColorU32(USER_STYLE::kTextHovered);
		} else {
			textColor = GetColorU32(ImGuiCol_TextDisabled);
		}

		PushStyleColor(ImGuiCol_Text, textColor);

		// Mathematically center the text inside the IDEAL un-chopped box, locking it to the OutlineButton baseline
		RenderTextClipped(idealBb.Min, idealBb.Max, label, NULL, &textSize, { 0.5f, 0.5f });

		PopStyleColor();

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
		if (window->SkipItems)
			return false;

		float scale    = Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
		float rounding = GetUserStyleVar(USER_STYLE::kButtonRounding);

		ImVec2 textSize = CalcTextSize(label);

		// Tight Vertical padding so it hugs the text
		float padY = 7.0f * scale;
		float padX = 8.0f * scale;

		float visualH = textSize.y + (padY * 2.0f);
		float width   = textSize.x + (padX * 2.0f);

		// Force the layout slot to encompass the true visual height
		float  actualLayoutH = std::max(GetFrameHeight(), visualH);
		ImVec2 logical_sz    = ImVec2(width, actualLayoutH);

		ImVec2 pos = window->DC.CursorPos;
		ImRect bb(pos, pos + logical_sz);

		ItemSize(logical_sz);
		if (!ItemAdd(bb, window->GetID(label)))
			return false;

		// Shift visual rect vertically to center it cleanly
		float  offY = (logical_sz.y - visualH) * 0.5f;
		ImRect bbVisual(pos.x, pos.y + offY, pos.x + width, pos.y + offY + visualH);

		bool h, held;
		bool p = ButtonBehavior(bb, window->GetID(label), &h, &held);

		// Background stays solid; hover feedback is handled by the Border
		window->DrawList->AddRectFilled(bbVisual.Min, bbVisual.Max, GetColorU32(ImGuiCol_Button), rounding);
		DrawWidgetBorder(window->DrawList, bbVisual, h || held, rounding);

		bool dim = MANAGER(Input)->IsInputGamepad() && !h;
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));

		RenderTextClipped(bbVisual.Min, bbVisual.Max, label, NULL, &textSize, { 0.5f, 0.5f });

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
		bool        clicked = false;
		std::string idStr   = std::format("##BTN_{}", label);

		// Calculate actual layout height, supporting oversized icons
		float layoutH       = ImGui::GetFrameHeight();
		float actualLayoutH = std::max(layoutH, size.y);
		float offY          = std::floor((actualLayoutH - size.y) * 0.5f);

		auto DrawContent = [&]() {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiID id = window->GetID(idStr.c_str());
			ImVec2  p  = window->DC.CursorPos;

			ImRect bb(p, p + ImVec2(size.x, actualLayoutH));
			ItemSize(bb);
			if (!ItemAdd(bb, id))
				return;

			bool hovered, held;
			bool pressed = ButtonBehavior(bb, id, &hovered, &held);
			if (pressed)
				clicked = true;

			ImVec4 tint = GetHighlightTint(true, hovered || IsWidgetFocused(id), false);

			ImVec2 drawPos(std::floor(p.x), std::floor(p.y + offY));
			window->DrawList->AddImage((ImTextureID)tex, drawPos, drawPos + size, ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(tint));
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, size.x, DrawContent, actualLayoutH);

		if (clicked)
			RE::PlaySound("UIMenuOK");
		return clicked;
	}

	bool ImGui::Hotkey(const char* label, std::uint32_t key, std::int32_t m1, std::int32_t m2, bool alignFar, bool labelLeft, bool flashing, bool alwaysHighlight, float iconScale)
	{
		bool        clicked  = false;
		std::string baseId   = std::format("##HOTKEY_{}", label);
		auto*       iconFont = MANAGER(IconFont);

		float activeScale = FUCKMan::GetSingleton()->GetActiveScale();
		float resScale    = ImGui::Renderer::GetResolutionScale();

		ImGuiContext& g       = *GImGui;
		const float   layoutH = ImGui::GetFrameHeight();
		const float   spacing = std::max(1.0f, std::floor(g.Style.ItemInnerSpacing.x * 0.5f));

		const float baseFrameH = 38.0f * resScale * activeScale;

		const auto* kIcon  = iconFont->GetIcon(key);
		const auto* m1Icon = (m1 != -1) ? iconFont->GetIcon(static_cast<uint32_t>(m1)) : nullptr;
		const auto* m2Icon = (m2 != -1) ? iconFont->GetIcon(static_cast<uint32_t>(m2)) : nullptr;

		struct RenderItem
		{
			enum Type
			{
				kIcon,
				kText
			} type;
			const IconFont::IconTexture* icon = nullptr;
			const char*                  text = nullptr;
			ImVec2                       size;
		};

		// Build render sequence (Rendered Right-to-Left)
		std::vector<RenderItem> items;
		items.reserve(5);

		auto AddIcon = [&](const IconFont::IconTexture* icon) {
			if (icon) {
				float userIconScale = FUCKMan::GetSingleton()->IsIgnoringUserScale() ? 1.0f : ImGui::Styles::GetSingleton()->user.iconScale;
				float iconTargetH   = std::round(baseFrameH * userIconScale);
				float targetH       = std::round(iconTargetH * iconScale);
				float targetW       = std::round(targetH * (icon->imageSize.y > 0.0f ? (icon->imageSize.x / icon->imageSize.y) : 1.0f));
				items.push_back({ RenderItem::kIcon, icon, nullptr, ImVec2(targetW, targetH) });
			}
		};

		auto AddText = [&](const char* text) {
			items.push_back({ RenderItem::kText, nullptr, text, ImGui::CalcTextSize(text) });
		};

		// 1. Primary Key ( The Anchor )
		if (kIcon) {
			AddIcon(kIcon);
		} else {
			AddText("None");
		}

		// 2. Mod 2 ( Grows Left )
		if (m2Icon) {
			AddText("+");
			AddIcon(m2Icon);
		}

		// 3. Mod 1 ( Grows Left )
		if (m1Icon) {
			AddText("+");
			AddIcon(m1Icon);
		}

		// Compute total dimensions
		float anchorWidth = items.empty() ? 0.0f : items[0].size.x;
		float totalWidth  = 0.0f;
		float maxItemH    = 0.0f;

		for (const auto& item : items) {
			totalWidth += item.size.x + spacing;
			if (item.size.y > maxItemH)
				maxItemH = item.size.y;
		}
		if (!items.empty())
			totalWidth -= spacing;

		// Determine the reported width for the layout engine:
		// - Right-aligned: Report only the anchor width (modifiers spill to the left).
		// - Left-aligned: Report the total block width so the adjacent label positions correctly.
		float contentWidth = alignFar ? anchorWidth : totalWidth;

		// Scale frame height to accommodate the largest rendered item
		float actualLayoutH = std::max(layoutH, maxItemH);

		auto DrawContent = [&]() {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiID id = window->GetID(baseId.c_str());
			ImVec2  p  = window->DC.CursorPos;

			// If alignFar is true, the hitbox stretches backwards to encompass the modifiers properly
			float  leftEdge = alignFar ? p.x - (totalWidth - anchorWidth) : p.x;
			ImRect bb(ImVec2(leftEdge, p.y), ImVec2(leftEdge + totalWidth, p.y + actualLayoutH));

			ItemSize(bb);
			if (!ItemAdd(bb, id))
				return;

			bool hovered, held;
			bool pressed = ButtonBehavior(bb, id, &hovered, &held);
			if (pressed)
				clicked = true;

			bool isFocused = IsWidgetFocused(id);

			// Determine the starting X coordinate:
			// For right-aligned layouts, the anchor key starts at p.x.
			// For left-aligned layouts, the entire block begins at p.x.
			float currentX = alignFar ? p.x : p.x + (totalWidth - anchorWidth);

			for (size_t i = 0; i < items.size(); ++i) {
				const auto& item = items[i];

				// Shift the cursor leftwards for modifier keys while keeping the primary key anchored
				if (i > 0) {
					currentX -= (item.size.x + spacing);
				}

				float offY         = std::floor((actualLayoutH - item.size.y) * ImGui::Styles::GetSingleton()->user.labelAlign.y);
				float visualNudgeY = (item.type == RenderItem::kText) ? std::floor(1.5f * resScale) : 0.0f;

				ImVec2 drawPos(std::floor(currentX), std::floor(p.y + offY + visualNudgeY));

				if (item.type == RenderItem::kIcon) {
					ImVec4 tint = flashing ? ImVec4(1.0f, 0.8f, 0.2f, 0.4f + (0.6f * (float)fabs(sin(ImGui::GetTime() * 5.0f)))) : GetHighlightTint(true, hovered || isFocused || alwaysHighlight, false);

					// Draw graphic natively
					window->DrawList->AddImage((ImTextureID)item.icon->srView.Get(), drawPos, drawPos + item.size, ImVec2(0, 0), ImVec2(1, 1), ImGui::ColorConvertFloat4ToU32(tint));
				} else {
					// Draw text manually to prevent ImGui from advancing the cursor and breaking the inline layout
					window->DrawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), drawPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), item.text);
				}
			}

			// Restore the cursor position to sit exactly at the edge of the required content width
			ImGui::SetCursorPosX(p.x + contentWidth);
			ImGui::SetCursorPosY(p.y);
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, contentWidth, DrawContent, actualLayoutH);

		if (clicked)
			RE::PlaySound("UIMenuFocus");
		return clicked;
	}

	bool DrawManagedHotkey(const char* label, FUCK::ManagedHotkey& h, FUCK::HotkeyFlags flags, float iconScale, float labelScale)
	{
		auto* regularFont = IconFont::Manager::GetSingleton()->GetRegularFont();
		bool  popFont     = false;

		if (labelScale != 1.0f) {
			float activeScale = FUCKMan::GetSingleton()->GetActiveScale();
			float targetSize  = regularFont->LegacySize * labelScale * activeScale;
			ImGui::PushFont(regularFont, targetSize);
			popFont = true;
		}

		bool alignFar        = !(flags & FUCK::HotkeyFlags::kAlignNear);
		bool labelLeft       = !(flags & FUCK::HotkeyFlags::kLabelRight);
		bool ctrlToRebind    = (flags & FUCK::HotkeyFlags::kCtrlToRebind);
		bool alwaysHighlight = (flags & FUCK::HotkeyFlags::kAlwaysHighlight);
		bool noModifiers     = (flags & FUCK::HotkeyFlags::kNoModifiers);

		h.disallowModifiers = noModifiers;

		auto inputMgr  = Input::Manager::GetSingleton();
		bool inputIsGP = (inputMgr->GetInputDevice() == Input::DEVICE::kGamepadDirectX || inputMgr->GetInputDevice() == Input::DEVICE::kGamepadOrbis);

		bool gpSlotValid = (h.gKey != 0) && (h.gKey >= Input::Keymap::kGPBase);
		bool kbSlotValid = (h.kKey != 0) && (h.kKey < Input::Keymap::kGPBase);

		bool showGP = inputIsGP;
		if (showGP && !gpSlotValid && kbSlotValid)
			showGP = false;
		if (!showGP && !kbSlotValid && gpSlotValid)
			showGP = true;

		std::uint32_t k  = showGP ? h.gKey : h.kKey;
		std::int32_t  m1 = showGP ? h.gMod1 : h.kMod1;
		std::int32_t  m2 = showGP ? h.gMod2 : h.kMod2;

		bool triggered = false;
		bool clicked   = false;

		std::string dynamicLabel;
		const char* finalLabel = label;

		if (h.isBinding) {
			dynamicLabel = TRANSLATE_S("$FUCK_Settings_PressKeyBind") + "###" + label;
			finalLabel   = dynamicLabel.c_str();
		}

		// Disable interactions completely if another hotkey is currently binding
		bool disabled = inputMgr->IsBinding() && !h.isBinding;
		if (disabled)
			ImGui::BeginDisabled();

		if (ImGui::Hotkey(finalLabel, k, m1, m2, alignFar, labelLeft, h.isBinding, alwaysHighlight, iconScale)) {
			clicked = true;
		}

		if (disabled)
			ImGui::EndDisabled();

		if (clicked) {
			if (ctrlToRebind) {
				if (inputMgr->IsModifierPressed(FUCK::Modifier::kCtrl)) {
					if (!h.isBinding) {
						h.isBinding = true;
						inputMgr->StartBinding(k, m1, m2, h.disallowModifiers);
					}
				} else {
					if (!h.isBinding) {
						triggered = true;  // Act as button!
					}
				}
			} else {
				if (!h.isBinding) {
					h.isBinding = true;
					inputMgr->StartBinding(k, m1, m2, h.disallowModifiers);
				}
			}
		}

		if (popFont) {
			ImGui::PopFont();
		}
		return triggered;
	}

	bool DragScalarEx(const char* label, ImGuiDataType type, void* data, float speed, const void* min, const void* max, const char* fmt, ImGuiSliderFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		float        w      = CalcItemWidth();
		ImRect       bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, GetFrameHeight()));

		const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
		ItemSize(bb);
		if (!ItemAdd(bb, window->GetID(label), &bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
			return false;

		ImGuiContext& g  = *GImGui;
		const ImGuiID id = window->GetID(label);

		bool h                    = ItemHoverable(bb, id, g.LastItemData.ItemFlags);
		bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);

		if (!temp_input_is_active) {
			const bool mouse_clicked = h && IsMouseClicked(0, ImGuiInputFlags_None, id);
			const bool make_active   = (mouse_clicked || g.NavActivateId == id);

			if (make_active && temp_input_allowed) {
				if ((mouse_clicked && g.IO.KeyCtrl) || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput))) {
					temp_input_is_active = true;
				}
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

		bool changed = false;
		if (h && !temp_input_is_active && MANAGER(Input)->IsInputKBM()) {
			if (ApplyWASDNudge(type, data, min, max, speed)) {
				changed = true;
				MarkItemEdited(id);
			}
		}

		bool active = (g.ActiveId == id);
		RenderFrame(bb.Min, bb.Max, GetColorU32(active ? ImGuiCol_FrameBgActive : h ? ImGuiCol_FrameBgHovered :
																					  ImGuiCol_FrameBg),
			true, ImGui::GetStyle().FrameRounding);
		DrawWidgetBorder(window->DrawList, bb, active || h || IsWidgetFocused(id), ImGui::GetStyle().FrameRounding);

		if (DragBehavior(id, type, data, speed, min, max, fmt, flags)) {
			changed = true;
		}
		if (changed)
			MarkItemEdited(id);

		bool dim = MANAGER(Input)->IsInputGamepad() && !IsWidgetFocused(id);
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));

		char        buf[64];
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

		if (dim)
			PopStyleColor();
		return changed;
	}

	bool ThinSliderScalar(const char* label, ImGuiDataType type, void* data, const void* min, const void* max, const char* fmt, ImGuiSliderFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		float        w      = CalcItemWidth();
		ImRect       bb(window->DC.CursorPos, window->DC.CursorPos + ImVec2(w, GetFrameHeight()));

		const bool temp_input_allowed = (flags & ImGuiSliderFlags_NoInput) == 0;
		ItemSize(bb);
		if (!ItemAdd(bb, window->GetID(label), &bb, temp_input_allowed ? ImGuiItemFlags_Inputable : 0))
			return false;

		ImGuiContext& g  = *GImGui;
		const ImGuiID id = window->GetID(label);

		bool h                    = ItemHoverable(bb, id, g.LastItemData.ItemFlags);
		bool temp_input_is_active = temp_input_allowed && TempInputIsActive(id);

		if (!temp_input_is_active) {
			const bool mouse_clicked = h && IsMouseClicked(0, ImGuiInputFlags_None, id);
			const bool make_active   = (mouse_clicked || g.NavActivateId == id);

			if (make_active && temp_input_allowed) {
				if ((mouse_clicked && g.IO.KeyCtrl) || (g.NavActivateId == id && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput))) {
					temp_input_is_active = true;
				}
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

		bool changed = false;
		if (h && !temp_input_is_active && MANAGER(Input)->IsInputKBM()) {
			float inferredSpeed = 1.0f;
			if (type == ImGuiDataType_Float && min && max) {
				inferredSpeed = (*(const float*)max - *(const float*)min) * 0.01f;
			} else if (type == ImGuiDataType_S32 && min && max) {
				inferredSpeed = std::max(1.0f, (*(const int*)max - *(const int*)min) * 0.01f);
			}

			if (ApplyWASDNudge(type, data, min, max, inferredSpeed)) {
				changed = true;
				MarkItemEdited(id);
			}
		}

		ImRect grab;
		if (SliderBehavior(bb, id, type, data, min, max, fmt, flags, &grab)) {
			changed = true;
		}
		if (changed)
			MarkItemEdited(id);

		bool active = (g.ActiveId == id);

		ImRect track = bb;
		// Shrink Y-axis significantly so the track is narrow
		float trackH = std::max(4.0f, 6.0f * ImGui::Renderer::GetResolutionScale());
		float s      = (track.GetHeight() - trackH) * 0.8f;
		track.Min.y += s;
		track.Max.y -= s;

		window->DrawList->AddRectFilled(track.Min, track.Max, GetColorU32(active ? ImGuiCol_FrameBgActive : h ? ImGuiCol_FrameBgHovered :
																												ImGuiCol_FrameBg),
			ImGui::GetStyle().FrameRounding);
		DrawWidgetBorder(window->DrawList, track, IsWidgetFocused(id) || active || h, ImGui::GetStyle().FrameRounding);

		if (grab.Max.x > grab.Min.x)
			window->DrawList->AddRectFilled(grab.Min, grab.Max, GetColorU32(active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), ImGui::GetStyle().GrabRounding);

		bool dim = MANAGER(Input)->IsInputGamepad() && !IsWidgetFocused(id);
		if (dim)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));

		char        buf[64];
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

		if (dim)
			PopStyleColor();
		return changed;
	}

	static Map<ImGuiID, FormComboBoxFiltered<RE::TESForm>> s_FormCaches;
	void                                                   ClearFormCaches() { s_FormCaches.clear(); }

	bool ComboForm(const char* label, RE::FormID* currentFormID, RE::FormType formType)
	{
		// Caches forms based on requested type to prevent continuous expensive lookups
		ImGuiID id       = ImGui::GetID(label);
		ImGuiID cacheKey = ImHashData(&formType, sizeof(formType), id);

		auto [it, inserted] = s_FormCaches.try_emplace(cacheKey, label);
		it->second.InitForms(formType);

		if (currentFormID && *currentFormID != 0) {
			it->second.Sync(*currentFormID);
		}

		bool changed = false;

		it->second.GetFormResultFromCombo([&](RE::TESForm* form) {
			if (form) {
				*currentFormID = form->GetFormID();
				changed        = true;
			}
		});

		return changed;
	}

	bool CollapsingHeaderIcon(const char* label, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID      id     = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);

		// Calculate total layout size using same padY as combo boxes
		float scale       = ImGui::Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
		float padY        = 7.0f * scale;
		float frameHeight = CalcTextSize(label).y + padY * 2.0f;

		ImVec2 pos = window->DC.CursorPos;
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
		if (h || is_open) {
			ImU32 bgColor;
			if (is_open) {
				bgColor = GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox);
			} else {
				bgColor = GetColorU32(ImGuiCol_HeaderHovered);
			}
			window->DrawList->AddRectFilled(bb.Min, bb.Max, bgColor, GImGui->Style.FrameRounding);

			if (is_open) {
				DrawWidgetBorder(window->DrawList, bb, h || IsWidgetFocused(id), GImGui->Style.FrameRounding);
			}
		}

		// Push the icon inwards to give it breathing room from the edge
		float padX = std::max(GImGui->Style.FramePadding.x, 10.0f * scale);

		float baseArrowSize = 30.0f;

		// Scale icon for the layout footprint
		float maxIconDim = baseArrowSize * scale;
		float textOff    = padX + maxIconDim + GImGui->Style.ItemInnerSpacing.x;

		// Pass the unscaled base icon
		DrawTreeIcon(window->DrawList, { bb.Min.x + padX, bb.Min.y }, frameHeight, is_open, h, baseArrowSize);

		// Vertically centre text
		ImVec2 textSize = CalcTextSize(label);
		float  textY    = bb.Min.y + (frameHeight - textSize.y) * 0.5f;

		RenderText({ bb.Min.x + textOff, textY }, label);
		return is_open;
	}

	bool TreeNodeIcon(const char* label, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID      id     = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);

		float scale       = ImGui::Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
		float padY        = 3.0f * scale;
		float frameHeight = CalcTextSize(label).y + padY * 2.0f;

		ImVec2 pos = window->DC.CursorPos;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, frameHeight));

		ItemSize(bb);

		float padX = std::max(GImGui->Style.FramePadding.x, 10.0f * scale);

		float baseArrowSize = 20.0f;

		// Scale icon for the layout footprint
		float maxIconDim = baseArrowSize * scale;
		float textOff    = padX + maxIconDim + GImGui->Style.ItemInnerSpacing.x;

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
		if (h) {
			window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(ImGuiCol_HeaderHovered), GImGui->Style.FrameRounding);
		}

		// Pass the unscaled base icon
		DrawTreeIcon(window->DrawList, { pos.x + padX, pos.y }, frameHeight, is_open, h, baseArrowSize);

		// Align text
		float textY = bb.Min.y + (frameHeight - CalcTextSize(label).y) * 0.5f;

		RenderText({ pos.x + textOff, textY }, label);

		if (is_open)
			TreePush(label);
		return is_open;
	}

	std::tuple<bool, bool, bool> CenteredTextWithArrows(const char* label, std::string_view centerText)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID      id     = window->GetID(label);
		ImRect       widgetBounds(window->DC.CursorPos, window->DC.CursorPos + ImVec2(CalcItemWidth(), GetFrameHeight()));
		ItemSize(widgetBounds);

		if (!ItemAdd(widgetBounds, id))
			return { false, false, false };

		auto  arrowIcon = MANAGER(IconFont)->GetStepperRight();
		float uiScale   = ImGui::Renderer::GetResolutionScale();
		float userScale = FUCKMan::GetSingleton()->GetActiveScale();

		// Extract ratio (steppers always point left/right, so height is height)
		float targetH = 30.0f * uiScale * userScale;
		float aspect  = (arrowIcon && arrowIcon->imageSize.y > 0.0f) ? (arrowIcon->imageSize.x / arrowIcon->imageSize.y) : 1.0f;
		float targetW = targetH * aspect;

		ImVec2 arrowSize(targetW, targetH);

		bool isFocused = (GImGui->NavId == id);
		bool isHovered = ItemHoverable(widgetBounds, id, GImGui->LastItemData.ItemFlags);

		bool hoveredLeft = false, hoveredRight = false, hoveredCenter = false;
		bool clickedLeft = false, clickedRight = false;

		// If mouse is actively over the widget, just check the X coordinate to split the hitboxes
		// This lets a single ImGui item act as a complex 3-part interactable area (Left | Center | Right)
		if (isHovered && MANAGER(Input)->CanNavigateWithMouse()) {
			float mouseX = GetIO().MousePos.x;

			hoveredLeft   = mouseX < (widgetBounds.Min.x + arrowSize.x);
			hoveredRight  = mouseX > (widgetBounds.Max.x - arrowSize.x);
			hoveredCenter = !hoveredLeft && !hoveredRight;

			if (IsMouseClicked(0)) {
				SetFocusID(id, window);
				clickedLeft  = hoveredLeft;
				clickedRight = hoveredRight;
			}
		}

		bool showNavHighlight = isFocused && (MANAGER(Input)->GetInputDevice() != Input::DEVICE::kMouse);
		bool dimText          = MANAGER(Input)->IsInputGamepad() && !(isFocused || isHovered);

		if (dimText)
			PushStyleColor(ImGuiCol_Text, GetColorU32(ImGuiCol_TextDisabled));

		auto  largeFont = MANAGER(IconFont)->GetLargeFont();
		float fontScale = FUCKMan::GetSingleton()->GetActiveScale();
		float fontSize  = (largeFont ? largeFont->LegacySize : GetStyle().FontSizeBase) * fontScale;

		PushFont(largeFont, fontSize);
		RenderTextClipped(widgetBounds.Min + ImVec2(arrowSize.x, 0), widgetBounds.Max - ImVec2(arrowSize.x, 0), centerText.data(), nullptr, nullptr, { 0.5f, 0.5f });
		PopFont();

		if (dimText)
			PopStyleColor();

		if (arrowIcon) {
			ImU32 colorLeft   = (showNavHighlight || hoveredLeft || hoveredCenter) ? IM_COL32_WHITE : GetUserStyleColorU32(USER_STYLE::kIconDisabled);
			ImU32 colorRight  = (showNavHighlight || hoveredRight || hoveredCenter) ? IM_COL32_WHITE : GetUserStyleColorU32(USER_STYLE::kIconDisabled);
			float iconOffsetY = widgetBounds.Min.y + (widgetBounds.GetHeight() - arrowSize.y) * 0.5f;

			DrawArrowIcon(window->DrawList, { widgetBounds.Min.x, iconOffsetY }, arrowSize, colorLeft, IconDirection::kLeft);
			DrawArrowIcon(window->DrawList, { widgetBounds.Max.x - arrowSize.x, iconOffsetY }, arrowSize, colorRight, IconDirection::kRight);
		}

		return { isHovered || isFocused, clickedLeft, clickedRight };
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

		// Creates a visual "framed" background box to encapsulate an inner widget visually
		float scale      = ImGui::Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
		float borderSize = ImGui::GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float rounding   = ImGui::GetStyle().FrameRounding;

		float padX = std::max(ImGui::GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float padY = 7.0f * scale;

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));

		LeftAlignedTextImpl(label, id);

		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

		ImGui::BeginGroup();
		bool result = drawWidget(id.c_str());
		ImGui::EndGroup();

		PopStyleVar(2);
		PopStyleColor(3);

		if (borderSize > 0.0f) {
			ImRect bb      = GImGui->LastItemData.Rect;
			bool   hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
			bool   active  = ImGui::IsItemActive();
			bool   focused = IsWidgetFocused(id);

			DrawWidgetBorder(ImGui::GetWindowDrawList(), bb, hovered || active || focused, rounding);
		}
		ActivateOnHover();

		return result;
	}

	bool ColorEdit3Styled(const char* label, float col[3], int flags)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ImGui::ColorEdit3(id, col, flags | ImGuiColorEditFlags_NoLabel);
		});
		if (res)
			RE::PlaySound("UIMenuPrevNext");
		else if (IsItemActivated())
			RE::PlaySound("UIMenuFocus");
		return res;
	}

	bool ColorEdit4Styled(const char* label, float col[4], int flags)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ImGui::ColorEdit4(id, col, flags | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		});
		if (res)
			RE::PlaySound("UIMenuPrevNext");
		else if (IsItemActivated())
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
		*outLeft             = false;
		*outRight            = false;
		if (hovered || IsWidgetFocused(label)) {
			bool pL = l || IsKeyPressed(ImGuiKey_A, false) || IsKeyPressed(ImGuiKey_GamepadDpadLeft, false);
			bool pR = r || IsKeyPressed(ImGuiKey_D, false) || IsKeyPressed(ImGuiKey_GamepadDpadRight, false);
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
