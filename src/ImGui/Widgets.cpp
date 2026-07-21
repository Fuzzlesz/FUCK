#include "Audio.h"
#include "FormComboBox.h"
#include "IconsFonts.h"
#include "Renderer.h"
#include "Widgets.h"

#include "System/Input.h"

namespace ImGui
{
	namespace
	{
		// ==================================================
		// INTERNAL HELPERS
		// ==================================================

		void DrawTreeIcon(ImDrawList* drawList, const ImVec2& pos, float frameHeight, bool isOpen, bool isHovered, float baseIconSize = 30.0f)
		{
			static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
			if (!iconArrow)
				return;

			ImU32 col       = GetDynamicTextColor(isHovered);
			float uiScale   = Renderer::GetResolutionScale();
			float userScale = FUCKMan::GetSingleton()->GetActiveScale();
			float aspect    = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

			auto ap = CalcArrowIconParams(aspect, isOpen, frameHeight, baseIconSize * uiScale, userScale);

			// Center horizontally within its own max dimension to prevent shifting on open/close
			float maxIconDim  = std::max(ap.drawSize.x, ap.drawSize.y);
			float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

			ImVec2 drawPos = { pos.x + iconOffsetX, pos.y + ap.offsetY };
			DrawArrowIcon(drawList, drawPos, ap.drawSize, col,
				isOpen ? IconDirection::kDown : IconDirection::kRight);
		}

		void DrawDropdownIcon(ImDrawList* drawList, ImVec2 bPos, ImVec2 bSize, bool isOpen, bool opensUp, bool isHovered)
		{
			static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
			if (!iconArrow)
				return;

			ImU32 col       = GetDynamicTextColor(isHovered || isOpen);
			float uiScale   = Renderer::GetResolutionScale();
			float userScale = FUCKMan::GetSingleton()->GetActiveScale() * GetCurrentWindow()->FontWindowScale;
			float aspect    = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

			auto ap = CalcArrowIconParams(aspect, isOpen, bSize.y, 30.0f * uiScale, userScale);

			ImVec2 iconPos = {
				bPos.x + (bSize.x - ap.drawSize.x) * 0.5f,
				bPos.y + ap.offsetY
			};

			// Combo/Window: Closed = Left, Open = Down/Up
			IconDirection dir;
			if (isOpen) {
				dir = opensUp ? IconDirection::kUp : IconDirection::kDown;
			} else {
				dir = IconDirection::kLeft;
			}

			DrawArrowIcon(drawList, iconPos, ap.drawSize, col, dir);
		}

		ImVec4 GetHighlightTint(bool active, bool hovered, bool focused)
		{
			if (hovered || focused)
				return ImVec4(1, 1, 1, 1);
			if (active)
				return GetUserStyleColorVec4(USER_STYLE::kWidgetToggleActive);
			return GetUserStyleColorVec4(USER_STYLE::kIconDisabled);
		}

		bool DrawTransparentButton(const char* id, void* tex, const ImVec2& size, const ImVec4& tint)
		{
			PushStyleColor(ImGuiCol_Button, { 0, 0, 0, 0 });
			PushStyleColor(ImGuiCol_ButtonHovered, { 0, 0, 0, 0 });
			PushStyleColor(ImGuiCol_ButtonActive, { 0, 0, 0, 0 });
			PushStyleColor(ImGuiCol_Border, { 0, 0, 0, 0 });
			PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });
			bool result = ImageButton(id, reinterpret_cast<ImTextureID>(tex), size, { 0, 0 }, { 1, 1 }, { 0, 0, 0, 0 }, tint);
			PopStyleVar();
			PopStyleColor(4);
			return result;
		}

		template <typename TDrawFunc>
		void AlignedWidgetLayout(const char* label, bool alignFar, bool labelLeft, float contentWidth, TDrawFunc drawContent, float targetHeight = -1.0f)
		{
			BeginGroup();
			PushID(label);

			ImGuiContext& g      = *GImGui;
			float         startX = GetCursorPosX();
			float         startY = GetCursorPosY();

			float fullAvailX = GetContentRegionAvail().x;
			auto& style      = Styles::GetSingleton()->user;

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

			float labelWidth = CalcTextSize(labelView.data(), labelView.data() + labelView.size()).x;

			// Calculate true dynamic centering, supporting widgets (like Icons) that are taller than a standard frame
			float actualTargetH = targetHeight > 0.0f ? targetHeight : GetFrameHeight();
			float textH         = GetTextLineHeight();
			float offY          = std::floor((actualTargetH - textH) * style.labelAlign.y);

			ImU32 textColor = GetColorU32(ImGuiCol_Text);

			// Natively draw the label to the screen without disrupting ImGui's internal cursor bounds.
			// This prevents SameLine() and ItemSize() desyncs.
			auto DrawLabel = [&](float xPos) {
				ImVec2 screenPos = GetCursorScreenPos();
				screenPos.x      = std::floor(screenPos.x + (xPos - startX));
				screenPos.y      = std::floor(screenPos.y + std::max(0.0f, offY));
				GetWindowDrawList()->AddText(GetFont(), GetFontSize(), screenPos, textColor, labelView.data(), labelView.data() + labelView.size());
			};

			// Tighter gap globally for near-aligned widgets to closely mimic vanilla grouping
			float nearSpacing = std::max(1.0f, std::floor(g.Style.ItemInnerSpacing.x * 0.25f));
			// Near (Tightly Coupled) - Uses LabelAlign.X as a rigid spacer pushing the label away (Adds 0 to 50px of adjustable spacing)
			float labelSpacer = style.labelAlign.x * 50.0f * FUCKMan::GetSingleton()->GetActiveScale() * Renderer::GetResolutionScale();

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

			SetCursorPosX(widgetX);
			SetCursorPosY(startY);

			drawContent();

			// Explicitly register the furthest X bound so AutoResize windows don't clip
			SetCursorPosX(maxX);
			SetCursorPosY(startY);
			ItemSize(ImVec2(0.0f, actualTargetH));

			Dummy(ImVec2(0.0f, 0.0f));

			PopID();
			EndGroup();
		}

		// Helper function for adjusting hovered sliders with WASD
		bool ApplyWASDNudge(ImGuiDataType type, void* data, const void* min, const void* max, float speed)
		{
			float moveX = 0.0f;
			float moveY = 0.0f;

			if (IsKeyPressed(ImGuiKey_A, true))
				moveX -= 1.0f;
			if (IsKeyPressed(ImGuiKey_D, true))
				moveX += 1.0f;
			if (IsKeyPressed(ImGuiKey_W, true))
				moveY += 1.0f;
			if (IsKeyPressed(ImGuiKey_S, true))
				moveY -= 1.0f;

			if (moveX == 0.0f && moveY == 0.0f)
				return false;

			float stepSmall = (speed > 0.0f) ? speed : 1.0f;
			float stepLarge = stepSmall * 10.0f;

			float totalMove = (moveX * stepSmall) + (moveY * stepLarge);

			auto ClampedAdd = [](auto* v, float delta, const void* mn, const void* mx) {
				using T = std::remove_pointer_t<decltype(v)>;
				*v += static_cast<T>(delta);
				if (mn && mx) {
					T lo = *static_cast<const T*>(mn);
					T hi = *static_cast<const T*>(mx);
					if (lo < hi)
						*v = ImClamp(*v, lo, hi);
				}
			};

			switch (type) {
			case ImGuiDataType_Float:
				ClampedAdd(static_cast<float*>(data), totalMove, min, max);
				return true;
			case ImGuiDataType_Double:
				ClampedAdd(static_cast<double*>(data), totalMove, min, max);
				return true;
			case ImGuiDataType_S32:
				ClampedAdd(static_cast<int32_t*>(data), totalMove, min, max);
				return true;
			case ImGuiDataType_U32:
				ClampedAdd(static_cast<uint32_t*>(data), totalMove, min, max);
				return true;
			default:
				return false;
			}
		}
	}

	// ==================================================
	// CACHES
	// ==================================================
	struct ComboFilterState
	{
		char pattern[256] = { 0 };
	};

	static Map<ImGuiID, ComboFilterState>                  s_comboFilterStates;
	static Map<ImGuiID, FormComboBoxFiltered<RE::TESForm>> s_FormCaches;

	void ClearFormCaches()
	{
		s_comboFilterStates.clear();
		s_FormCaches.clear();
	}

	// ==================================================
	// CORE DRAWING & WIDGETS
	// ==================================================

	void DrawArrowIcon(ImDrawList* drawList, ImVec2 pos, ImVec2 size, ImU32 color, IconDirection direction)
	{
		static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
		if (!iconArrow || !iconArrow->srView)
			return;

		if (!drawList)
			drawList = GetWindowDrawList();

		ImVec2 p_min = pos;
		ImVec2 p_max = pos + size;

		switch (direction) {
		case IconDirection::kRight:
			// Normal (>): UVs {0,0} -> {1,1}
			drawList->AddImage(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()), p_min, p_max, { 0, 0 }, { 1, 1 }, color);
			break;
		case IconDirection::kDown:
			// Rotate 90 CW (v): UVs {0,1}, {0,0}, {1,0}, {1,1}
			drawList->AddImageQuad(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 0, 1 }, { 0, 0 }, { 1, 0 }, { 1, 1 }, color);
			break;
		case IconDirection::kLeft:
			// Mirror Horizontal (<): Swap U {1,0} -> {0,0}
			drawList->AddImageQuad(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 1, 0 }, { 0, 0 }, { 0, 1 }, { 1, 1 }, color);
			break;
		case IconDirection::kUp:
			// Rotate 270 CW / 90 CCW (^): UVs {1,0}, {1,1}, {0,1}, {0,0}
			drawList->AddImageQuad(reinterpret_cast<ImTextureID>(iconArrow->srView.Get()),
				p_min, { p_max.x, p_min.y }, p_max, { p_min.x, p_max.y },
				{ 1, 0 }, { 1, 1 }, { 0, 1 }, { 0, 0 }, color);
			break;
		}
	}

	void DrawWidgetBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding)
	{
		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);

		if (borderSize <= 0.0f)
			return;

		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();

		// Floor thickness
		float tColor = std::max(1.0f, std::floor(borderSize));
		float tBlack = std::max(1.0f, std::floor(1.0f * scale));

		ImU32 col = isActiveOrHovered ?
		                GetUserStyleColorU32(USER_STYLE::kWidgetBorderActive) :
		                GetUserStyleColorU32(USER_STYLE::kWidgetBorder);

		// Floor the bounding box to ensure pixel-perfect strokes
		ImVec2 min = { std::floor(bb.Min.x), std::floor(bb.Min.y) };
		ImVec2 max = { std::floor(bb.Max.x), std::floor(bb.Max.y) };

		// Outer Black: Expands strictly outwards
		float halfB = tBlack * 0.5f;
		drawList->AddRect(min - ImVec2(halfB, halfB), max + ImVec2(halfB, halfB), IM_COL32(0, 0, 0, 255), rounding + halfB, 0, tBlack);

		// Inner Colour: Shrinks strictly inwards
		float halfC = tColor * 0.5f;
		drawList->AddRect(min + ImVec2(halfC, halfC), max - ImVec2(halfC, halfC), col, std::max(0.0f, rounding - halfC), 0, tColor);
	}

	bool CheckBox(const char* label, bool* a_toggle, bool alignFar, bool labelLeft)
	{
		bool        selected   = false;
		auto        icon       = MANAGER(IconFont)->GetCheckbox();
		auto        iconFilled = MANAGER(IconFont)->GetCheckboxFilled();
		std::string idStr      = std::format("##{}", label);

		float scale         = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
		float userIconScale = FUCKMan::GetSingleton()->IsIgnoringUserScale() ? 1.0f : Styles::GetSingleton()->user.iconScale;

		// Apply the iconScale setting to match Hotkey sizing
		float visualH = std::round(36.0f * scale * userIconScale);
		float iconH   = visualH;
		float iconW   = iconH * (icon->imageSize.y > 0.0f ? (icon->imageSize.x / icon->imageSize.y) : 1.0f);

		float layoutH       = GetFrameHeight();
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
			window->DrawList->AddImage((ImTextureID)tex, drawPos, drawPos + ImVec2(iconW, iconH), ImVec2(0, 0), ImVec2(1, 1), ColorConvertFloat4ToU32(tint));
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, iconW, DrawContent, actualLayoutH);

		if (selected)
			PlayAudio(Audio::kFocus);
		return selected;
	}

	bool ToggleButton(const char* label, bool* v, bool alignFar, bool labelLeft)
	{
		bool        pressed = false;
		std::string idStr   = std::format("##{}", label);

		float scale         = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
		float userIconScale = FUCKMan::GetSingleton()->IsIgnoringUserScale() ? 1.0f : Styles::GetSingleton()->user.iconScale;

		// Apply the iconScale setting to match Hotkey sizing
		float visualH = std::round(36.0f * scale * userIconScale);
		float visualW = visualH * 1.35f;

		float layoutH       = GetFrameHeight();
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
			ImU32 col_knob_ring = *v ? GetColorU32(ImGuiCol_Text) : (showFrame ? GetUserStyleColorU32(USER_STYLE::kWidgetToggleActive) : GetUserStyleColorU32(USER_STYLE::kWidgetBorder));

			// Visual bounds for the toggle graphic, pushed down by offY
			ImRect visBB(std::floor(p.x), std::floor(p.y + offY), std::floor(p.x + visualW), std::floor(p.y + offY + visualH));

			if (showFrame) {
				draw_list->AddRect(visBB.Min, visBB.Max, GetColorU32(ImGuiCol_NavHighlight), GetStyle().FrameRounding, 0, 2.0f * scale);
			}

			float  knobRadius = visualH * 0.28f;
			float  knobMinX   = visBB.Min.x + knobRadius + (2.0f * scale);
			float  knobMaxX   = visBB.Max.x - knobRadius - (2.0f * scale);
			float  knobX      = knobMinX + (t * (knobMaxX - knobMinX));
			ImVec2 knobCenter = { knobX, visBB.Min.y + (visualH * 0.5f) };

			float  railH   = visualH * 0.35f;
			ImVec2 railMin = { std::floor(knobMinX), std::floor(visBB.Min.y + (visualH - railH) * 0.5f) };
			ImVec2 railMax = { std::floor(knobMaxX), std::floor(visBB.Min.y + (visualH + railH) * 0.5f) };
			ImRect railBB(railMin, railMax);

			draw_list->AddRectFilled(railMin, railMax, col_rail_fill, GetStyle().FrameRounding);
			DrawWidgetBorder(draw_list, railBB, hovered || isFocused, GetStyle().FrameRounding);

			draw_list->AddCircleFilled(knobCenter, knobRadius, col_knob_fill);

			float ringThick  = 3.0f * scale;
			float ringRadius = knobRadius - (4.0f * scale);
			if (ringRadius > 0.5f) {
				draw_list->AddCircle(knobCenter, ringRadius, col_knob_ring, 0, ringThick);
			}
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, visualW, DrawContent, actualLayoutH);

		if (pressed)
			PlayAudio(Audio::kFocus);
		return pressed;
	}

	bool ComboWithFilter(const char* label, int* current_item, std::span<const std::string> items, int popup_max_height_in_items)
	{
		ImGuiContext& g      = *GImGui;
		ImGuiWindow*  window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float currentFontScale = window->FontWindowScale;
		float scale            = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale() * currentFontScale;

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize) * currentFontScale;
		float padX       = std::floor(std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale)));
		float padY       = std::floor(7.0f * scale);

		auto CalcListHeight = [&](int items_count) -> float {
			if (items_count <= 0)
				return 0.0f;
			float actualFontSize = GetFontSize();
			float spacingY       = GetStyle().ItemSpacing.y;
			// Round to ensure integer height
			return std::round((actualFontSize + spacingY) * items_count - spacingY + (padY * 2.0f) + (GetStyle().ChildBorderSize * 2.0f));
		};

		// Capture the default frame height BEFORE we push our custom padding
		float defaultFrameH = std::round(GetFrameHeight());

		PushStyleVar(ImGuiStyleVar_FramePadding, { padX, padY });
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Prevents double frame

		const bool  isHidden = std::string_view(label).starts_with("##");
		std::string idStr    = isHidden ? std::string(label) : std::format("##{}", label);
		if (!isHidden)
			LeftAlignedTextImpl(label);

		ImVec2  widgetPos = GetCursorScreenPos();
		float   width     = std::round(CalcItemWidth());
		ImGuiID id        = window->GetID(idStr.c_str());

		float       designPadY     = 15.0f * Renderer::GetResolutionScale();
		float       popupPad       = std::round(std::max(0.0f, designPadY - (2.0f * scale)));

		float       frameH         = GetFrameHeight();
		ImDrawList* parentDrawList = GetWindowDrawList();

		// nonListH = filter input row + spacing between it and the child + outer popup top+bottom padding + border
		float nonListH = std::round(defaultFrameH + GetStyle().ItemSpacing.y + (popupPad * 2.0f) + (GetStyle().PopupBorderSize * 2.0f));

		// Pre-compute the popup height before BeginCombo
		float committedPopupH = 0.0f;
		if (!(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
			int   heightInItemsPre = std::max(2, popup_max_height_in_items);
			float idealListH_pre   = CalcListHeight(std::min(static_cast<int>(items.size()), heightInItemsPre));

			// Use the larger of above/below space since we don't know direction yet
			ImVec2 displaySize_pre = GetIO().DisplaySize;
			float  maxSpace_pre    = std::max(
                displaySize_pre.y - (widgetPos.y + frameH) - (8.0f * scale),
                widgetPos.y - (8.0f * scale));

			float maxListH_pre = std::floor(maxSpace_pre - nonListH) - 1.0f;
			if (maxListH_pre > 0.0f && idealListH_pre > maxListH_pre)
				idealListH_pre = maxListH_pre;

			committedPopupH = std::round(idealListH_pre + nonListH);
			SetNextWindowSizeConstraints({ width, committedPopupH }, { width, committedPopupH });
		}

		// Strip ## hidden tags from the preview text so it displays cleanly when closed
		std::string previewStr;
		if (*current_item >= 0 && *current_item < static_cast<int>(items.size())) {
			previewStr   = items[*current_item];
			auto hashPos = previewStr.find("##");
			if (hashPos != std::string::npos) {
				previewStr.erase(hashPos);
			}
		}
		const char* preview = previewStr.c_str();

		ImVec4 textBoxColor = GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox);
		PushStyleColor(ImGuiCol_FrameBg, textBoxColor);
		PushStyleColor(ImGuiCol_FrameBgHovered, textBoxColor);
		PushStyleColor(ImGuiCol_FrameBgActive, textBoxColor);

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, textBoxColor);

		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(popupPad, popupPad));

		bool isOpen = BeginCombo(idStr.c_str(), preview, ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge);

		PopStyleVar(3);
		PopStyleColor(5);

		// Detect if opened Upwards
		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}

		DrawDropdownIcon(parentDrawList, { std::floor(widgetPos.x + width - frameH), std::floor(widgetPos.y) }, { frameH, frameH }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { std::floor(widgetPos.x), std::floor(widgetPos.y), std::floor(widgetPos.x + width), std::floor(widgetPos.y + frameH) }, isOpen || IsItemHovered() || IsWidgetFocused(id), GetStyle().FrameRounding);

		if (!isOpen)
			return false;

		SetWindowFontScale(currentFontScale);

		if (IsWindowAppearing())
			SetKeyboardFocusHere();

		// Filter input styling
		PushStyleColor(ImGuiCol_FrameBg, GetColorU32(ImVec4(0.1f, 0.1f, 0.1f, 1.0f)));
		PushStyleColor(ImGuiCol_Text, GetUserStyleColorU32(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
		PushItemWidth(GetContentRegionAvail().x);
		InputText("##filter", s_comboFilterStates[id].pattern, 256, ImGuiInputTextFlags_AutoSelectAll);
		bool isInputFocused = IsItemFocused() || IsItemActive();
		PopItemWidth();

		PopStyleColor(3);

		bool navigateToItems = false;
		bool enterPressed    = false;
		if (isInputFocused) {
			if (IsKeyPressed(ImGuiKey_DownArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadDown, false) ||
				IsKeyPressed(ImGuiKey_UpArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadUp, false)) {
				navigateToItems = true;
			} else if (IsKeyPressed(ImGuiKey_Enter, false) || IsKeyPressed(ImGuiKey_KeypadEnter, false) || IsKeyPressed(ImGuiKey_GamepadFaceDown, false)) {
				enterPressed = true;
			}
		}

		std::vector<std::pair<int, double>> itemScoreVector;
		bool                                filtering = s_comboFilterStates[id].pattern[0] != '\0';
		if (filtering) {
			std::string lowerFilter = clib_util::string::tolower(s_comboFilterStates[id].pattern);

			// Use a static buffer to prevent thousands of memory allocations per frame
			static std::string lowerItemBuffer;

			for (int i = 0; i < static_cast<int>(items.size()); i++) {
				// Exclude items marked with ##NOFILTER and headers from search results
				if (items[i].find("##NOFILTER") != std::string::npos || items[i].starts_with("##HEADER:")) {
					continue;
				}

				// Copy into buffer and convert to lowercase in-place
				lowerItemBuffer = items[i];
				std::transform(lowerItemBuffer.begin(), lowerItemBuffer.end(), lowerItemBuffer.begin(),
					[](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });

				auto score = rapidfuzz::fuzz::partial_token_ratio(lowerFilter, lowerItemBuffer);

				if (score >= 65.0)
					itemScoreVector.push_back({ i, score });
			}

			std::ranges::sort(itemScoreVector, [](const auto& a, const auto& b) {
				// If the fuzzy match scores are identical, fallback to their original index (alphabetical order)
				if (std::abs(a.second - b.second) < 0.0001)
					return a.first < b.first;

				// Otherwise, float the best fuzzy matches to the top
				return a.second > b.second;
			});
		}

		bool changed    = false;
		int  show_count = filtering ? static_cast<int>(itemScoreVector.size()) : static_cast<int>(items.size());

		if (enterPressed && show_count > 0) {
			*current_item = filtering ? itemScoreVector[0].first : 0;
			changed       = true;
			CloseCurrentPopup();
			PlayAudio(Audio::kFocus);
		}

		// --- Inner List Setup ---
		PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_ChildBg, textBoxColor);
		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padX, padY));

		int   heightInItems = std::max(2, popup_max_height_in_items);
		float requiredListH = CalcListHeight(std::min(show_count, heightInItems));

		float idealListH = std::max(0.0f, std::round(GetContentRegionAvail().y));

		ImGuiChildFlags  childFlags = ImGuiChildFlags_Borders;
		ImGuiWindowFlags winFlags   = ImGuiWindowFlags_NoMove;

		if (show_count <= heightInItems && idealListH >= requiredListH - 1.0f)
			winFlags |= ImGuiWindowFlags_NoScrollbar;

		if (BeginChild("##List", ImVec2(0.0f, idealListH), childFlags, winFlags)) {
			SetWindowFontScale(currentFontScale);

			for (int i = 0; i < show_count; i++) {
				int idx = filtering ? itemScoreVector[i].first : i;

				// Push the index to guarantee unique IDs for identical labels
				PushID(idx);

				std::string_view itemStr(items[idx]);
				if (!filtering && itemStr.starts_with("##HEADER:")) {
					PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kWidgetFlash));
					if (i > 0)
						Dummy(ImVec2(0, 4.0f * scale));
					SetCursorPosX(GetCursorPosX() + padX);
					TextUnformatted(itemStr.data() + 9, itemStr.data() + itemStr.size());
					if (i < show_count - 1)
						Dummy(ImVec2(0, 2.0f * scale));
					PopStyleColor();
				} else {
					if (navigateToItems) {
						if (i == 0 || *current_item == idx) {
							SetKeyboardFocusHere(0);
						}
					}

					bool isSelected = (*current_item == idx);
					if (Selectable(items[idx].c_str(), isSelected)) {
						*current_item = idx;
						changed       = true;
						CloseCurrentPopup();
						PlayAudio(Audio::kFocus);
					}

					if (isSelected && IsWindowAppearing())
						SetScrollHereY();
				}

				PopID();
			}
		}
		EndChild();

		PopStyleVar();
		PopStyleColor(2);
		EndCombo();
		return changed;
	}

	bool ComboStyled(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height_in_items)
	{
		ImGuiContext& g      = *GImGui;
		ImGuiWindow*  window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float currentFontScale = window->FontWindowScale;
		float scale            = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale() * currentFontScale;

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize) * currentFontScale;
		float padX       = std::floor(std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale)));
		float padY       = std::floor(7.0f * scale);

		auto CalcListHeight = [&](int num_items) -> float {
			if (num_items <= 0)
				return 0.0f;
			float actualFontSize = GetFontSize();
			float spacingY       = GetStyle().ItemSpacing.y;
			// Round to ensure integer height
			return std::round((actualFontSize + spacingY) * num_items - spacingY);
		};

		PushStyleVar(ImGuiStyleVar_FramePadding, { padX, padY });
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Prevents double frame

		std::string idStr = std::format("##{}", label);
		LeftAlignedTextImpl(label);

		ImVec2 widgetPos = GetCursorScreenPos();
		float  width     = std::round(CalcItemWidth());
		float  frameH    = std::round(GetFrameHeight());

		float popupPadY = std::round(15.0f * Renderer::GetResolutionScale());

		// Compute an exact popup height so ImGui never allocates outer scrollbar space.
		float maxPopupH = std::round((popupPadY * 2.0f) + CalcListHeight(std::min(items_count, popup_max_height_in_items)) + (GetStyle().PopupBorderSize * 2.0f));

		if (!(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
			SetNextWindowSizeConstraints({ width, maxPopupH }, { width, maxPopupH });
		}

		PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));

		ImDrawList* parentDrawList = GetWindowDrawList();

		// Strip ## hidden tags from the preview text so it displays cleanly when closed
		std::string previewStr;
		if (*current_item >= 0 && *current_item < items_count) {
			previewStr   = items[*current_item];
			auto hashPos = previewStr.find("##");
			if (hashPos != std::string::npos) {
				previewStr.erase(hashPos);
			}
		}
		const char* preview = previewStr.c_str();

		parentDrawList->AddRectFilled(widgetPos, widgetPos + ImVec2(width, frameH), GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), GetStyle().FrameRounding);

		bool isOpen = BeginCombo(idStr.c_str(), preview, ImGuiComboFlags_NoArrowButton);

		PopStyleVar(2);
		PopStyleColor(6);

		// Detect if opened Upwards
		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}

		DrawDropdownIcon(parentDrawList, { std::floor(widgetPos.x + width - frameH), std::floor(widgetPos.y) }, { frameH, frameH }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { std::floor(widgetPos.x), std::floor(widgetPos.y), std::floor(widgetPos.x + width), std::floor(widgetPos.y + frameH) }, isOpen || IsItemHovered() || IsWidgetFocused(GetID(idStr.c_str())), GetStyle().FrameRounding);

		bool changed = false;
		if (isOpen) {
			SetWindowFontScale(currentFontScale);

			for (int i = 0; i < items_count; i++) {
				// Push the index to guarantee unique IDs for identical labels
				PushID(i);

				std::string_view itemStr(items[i]);
				if (itemStr.starts_with("##HEADER:")) {
					PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kWidgetFlash));
					if (i > 0)
						Dummy(ImVec2(0, 4.0f * scale));
					SetCursorPosX(GetCursorPosX() + padX);
					TextUnformatted(itemStr.data() + 9, itemStr.data() + itemStr.size());
					if (i < items_count - 1)
						Dummy(ImVec2(0, 2.0f * scale));
					PopStyleColor();
				} else {
					bool isSelected = (*current_item == i);
					if (Selectable(items[i], isSelected)) {
						*current_item = i;
						changed       = true;
						PlayAudio(Audio::kFocus);
					}

					if (IsWindowAppearing()) {
						if (isSelected || (*current_item < 0 && i == 0)) {
							SetKeyboardFocusHere(-1);
						}
					}
				}

				PopID();
			}
			EndCombo();
		}
		return changed;
	}

	bool InputTextStyled(const char* label, char* buf, size_t buf_size, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float currentFontScale = window->FontWindowScale;
		float scale            = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale() * currentFontScale;

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize) * currentFontScale;
		float padX       = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float padY       = 7.0f * scale;

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

		std::string idStr = LeftAlignedText(label);

		PushStyleColor(ImGuiCol_FrameBg,        GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgActive,  GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_Text,           GetUserStyleColorVec4(USER_STYLE::kComboBoxText));

		bool res = InputText(idStr.c_str(), buf, buf_size, flags);

		ImRect bb(GetItemRectMin(), GetItemRectMax());
		DrawWidgetBorder(GetWindowDrawList(), bb, IsItemHovered() || IsItemActive() || IsWidgetFocused(GetID(idStr.c_str())), GetStyle().FrameRounding);

		PopStyleColor(4);
		PopStyleVar(2);

		if (IsItemActivated())
			PlayAudio(Audio::kFocus);
		return res;
	}

	void DrawTabBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding)
	{
		float scale      = Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);

		if (borderSize <= 0.0f)
			return;

		// Floor thickness
		float tColor = std::max(1.0f, std::floor(borderSize));
		float tBlack = std::max(1.0f, std::floor(1.0f * scale));

		ImU32 col = isActiveOrHovered ?
		                GetUserStyleColorU32(USER_STYLE::kTabBorderActive) :
		                GetUserStyleColorU32(USER_STYLE::kTabBorder);

		float halfB = tBlack * 0.5f;
		float halfC = tColor * 0.5f;

		// Floor Bounding Box
		ImVec2 min = { std::floor(bb.Min.x), std::floor(bb.Min.y) };
		ImVec2 max = { std::floor(bb.Max.x), std::floor(bb.Max.y) };

		auto buildPath = [&](ImDrawList* dl, float inset, float r, float botY) {
			dl->PathLineTo({ min.x + inset, botY });

			if (r > 0.0f) {
				dl->PathArcTo({ min.x + rounding, min.y + rounding }, r, IM_PI, IM_PI * 1.5f, 24);
				dl->PathArcTo({ max.x - rounding, min.y + rounding }, r, IM_PI * 1.5f, IM_PI * 2.0f, 24);
			} else {
				dl->PathLineTo({ min.x + inset, min.y + inset });
				dl->PathLineTo({ max.x - inset, min.y + inset });
			}

			dl->PathLineTo({ max.x - inset, botY });
		};

		// Outer Black stroke (No bottom line)
		drawList->PathClear();
		buildPath(drawList, -halfB, rounding + halfB, max.y + halfB);  // Extend down past max.y slightly so it connects to separator cleanly
		drawList->PathStroke(IM_COL32(0, 0, 0, 255), 0, tBlack);

		// Inner Color stroke (No bottom line)
		drawList->PathClear();
		buildPath(drawList, halfC, std::max(0.0f, rounding - halfC), max.y);
		drawList->PathStroke(col, 0, tColor);
	}

	bool BeginTabItemEx(const char* label, ImGuiTabItemFlags flags)
	{
		ImGuiWindow* window     = GetCurrentWindow();
		ImGuiID      id         = GetID(label);
		ImGuiID      storageKey = GetID("##LastActiveTab");
		ImGuiID      lastActive = window->StateStorage.GetInt(storageKey, 0);

		bool  wasActive = (lastActive == id);
		float scale     = Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());

		PushStyleColor(ImGuiCol_Tab, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabActive, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabUnfocused, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_TabUnfocusedActive, ImVec4(0, 0, 0, 0));

		// Hide the native text rendering by passing a transparent color
		PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));

		// Widen the X padding to match OutlineButtons, but LEAVE Y ALONE so we don't break the layout engine
		float padX = 8.0f * scale;
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, GetStyle().FramePadding.y));

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
		// Use std::floor to prevent subpixel geometry artifacts
		ImRect drawBb = { std::floor(idealBb.Min.x), std::floor(idealBb.Min.y), std::floor(idealBb.Max.x), std::floor(max.y) };

		// Fetch the raw user value and scale it
		float rounding = Styles::GetSingleton()->user.tabRounding * scale;

		// Clamp rounding to prevent geometry artifacting at extreme scale
		rounding = std::min(rounding, drawBb.GetWidth() * 0.5f);
		rounding = std::min(rounding, drawBb.GetHeight());

		window->DrawList->PathClear();
		window->DrawList->PathLineTo({ drawBb.Max.x, drawBb.Max.y });
		window->DrawList->PathLineTo({ drawBb.Min.x, drawBb.Max.y });
		if (rounding > 0.0f) {
			window->DrawList->PathArcTo({ drawBb.Min.x + rounding, drawBb.Min.y + rounding }, rounding, IM_PI, IM_PI * 1.5f, 24);
			window->DrawList->PathArcTo({ drawBb.Max.x - rounding, drawBb.Min.y + rounding }, rounding, IM_PI * 1.5f, IM_PI * 2.0f, 24);
		} else {
			window->DrawList->PathLineTo({ drawBb.Min.x, drawBb.Min.y });
			window->DrawList->PathLineTo({ drawBb.Max.x, drawBb.Min.y });
		}
		window->DrawList->PathFillConvex(GetColorU32(ImGuiCol_Button));

		DrawTabBorder(window->DrawList, drawBb, active || IsItemHovered(), rounding);

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

			if (!wasActive || window->Appearing) {
				PlayAudio(Audio::kTab);
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

		// Shift visual rect vertically to center it cleanly and strictly align to pixel boundaries
		float  offY = (logical_sz.y - visualH) * 0.5f;
		ImRect bbVisual(std::floor(pos.x), std::floor(pos.y + offY), std::floor(pos.x + width), std::floor(pos.y + offY + visualH));

		bool h, held;
		bool p = ButtonBehavior(bb, window->GetID(label), &h, &held);

		// Background stays solid; hover feedback is handled by the Border
		window->DrawList->AddRectFilled(bbVisual.Min, bbVisual.Max, GetColorU32(ImGuiCol_Button), rounding);
		DrawWidgetBorder(window->DrawList, bbVisual, h || held, rounding);

		RenderTextClipped(bbVisual.Min, bbVisual.Max, label, NULL, &textSize, { 0.5f, 0.5f });

		if (p)
			PlayAudio(Audio::kOk);
		if (wasFocused)
			*wasFocused = h;
		return p;
	}

	bool ButtonIconWithLabelStyled(const char* label, void* tex, const ImVec2& size, bool alignFar, bool labelLeft)
	{
		bool        clicked = false;
		std::string idStr   = std::format("##BTN_{}", label);

		// Calculate actual layout height, supporting oversized icons
		float layoutH       = GetFrameHeight();
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
			window->DrawList->AddImage((ImTextureID)tex, drawPos, drawPos + size, ImVec2(0, 0), ImVec2(1, 1), ColorConvertFloat4ToU32(tint));
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, size.x, DrawContent, actualLayoutH);

		if (clicked)
			PlayAudio(Audio::kOk);
		return clicked;
	}

	bool Hotkey(const char* label, std::uint32_t key, std::int32_t m1, std::int32_t m2, bool alignFar, bool labelLeft, bool flashing, bool alwaysHighlight, float iconScale)
	{
		bool        clicked  = false;
		std::string baseId   = std::format("##HOTKEY_{}", label);
		auto*       iconFont = MANAGER(IconFont);

		float activeScale = FUCKMan::GetSingleton()->GetActiveScale();
		float resScale    = Renderer::GetResolutionScale();

		ImGuiContext& g       = *GImGui;
		const float   layoutH = GetFrameHeight();
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
				float userIconScale = FUCKMan::GetSingleton()->IsIgnoringUserScale() ? 1.0f : Styles::GetSingleton()->user.iconScale;
				float iconTargetH   = std::round(baseFrameH * userIconScale);
				float targetH       = std::round(iconTargetH * iconScale);
				float targetW       = std::round(targetH * (icon->imageSize.y > 0.0f ? (icon->imageSize.x / icon->imageSize.y) : 1.0f));
				items.push_back({ RenderItem::kIcon, icon, nullptr, ImVec2(targetW, targetH) });
			}
		};

		auto AddText = [&](const char* text) {
			items.push_back({ RenderItem::kText, nullptr, text, CalcTextSize(text) });
		};

		// 1. Primary Key ( The Anchor )
		if (kIcon) {
			AddIcon(kIcon);
		} else {
			AddText(Input::Manager::GetSingleton()->GetKeyName(key));
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

				float offY         = std::floor((actualLayoutH - item.size.y) * Styles::GetSingleton()->user.labelAlign.y);
				float visualNudgeY = (item.type == RenderItem::kText) ? std::floor(1.5f * resScale) : 0.0f;

				ImVec2 drawPos(std::floor(currentX), std::floor(p.y + offY + visualNudgeY));

				if (item.type == RenderItem::kIcon) {
					ImVec4 flashCol = GetUserStyleColorVec4(USER_STYLE::kWidgetFlash);
					flashCol.w      = 0.4f + (0.6f * (float)fabs(sin(GetTime() * 5.0f)));
					ImVec4 tint     = flashing ? flashCol : GetHighlightTint(true, hovered || isFocused || alwaysHighlight, false);

					// Draw graphic natively
					window->DrawList->AddImage((ImTextureID)item.icon->srView.Get(), drawPos, drawPos + item.size, ImVec2(0, 0), ImVec2(1, 1), ColorConvertFloat4ToU32(tint));
				} else {
					// Draw text manually to prevent ImGui from advancing the cursor and breaking the inline layout
					window->DrawList->AddText(GetFont(), GetFontSize(), drawPos, GetColorU32(ImGuiCol_TextDisabled), item.text);
				}
			}

			// Restore the cursor position to sit exactly at the edge of the required content width
			SetCursorPosX(p.x + contentWidth);
			SetCursorPosY(p.y);
		};

		AlignedWidgetLayout(label, alignFar, labelLeft, contentWidth, DrawContent, actualLayoutH);

		if (clicked)
			PlayAudio(Audio::kFocus);
		return clicked;
	}

	bool DrawManagedHotkey(const char* label, FUCK::ManagedHotkey& h, FUCK::HotkeyFlags flags, float iconScale, float labelScale)
	{
		auto* regularFont = IconFont::Manager::GetSingleton()->GetRegularFont();
		bool  popFont     = false;

		if (labelScale != 1.0f) {
			float globalScale = FUCKMan::GetSingleton()->GetActiveScale() * Renderer::GetResolutionScale();
			float targetSize  = regularFont->LegacySize * labelScale * globalScale;
			PushFont(regularFont, targetSize);
			popFont = true;
		}

		bool alignFar        = !(flags & FUCK::HotkeyFlags::kAlignNear);
		bool labelLeft       = !(flags & FUCK::HotkeyFlags::kLabelRight);
		bool ctrlToRebind    = (flags & FUCK::HotkeyFlags::kCtrlToRebind);
		bool alwaysHighlight = (flags & FUCK::HotkeyFlags::kAlwaysHighlight);
		bool noModifiers     = (flags & FUCK::HotkeyFlags::kNoModifiers);

		h.disallowModifiers = noModifiers;

		auto inputMgr = Input::Manager::GetSingleton();
		bool showGP   = (inputMgr->GetInputDevice() == Input::DEVICE::kGamepadDirectX || inputMgr->GetInputDevice() == Input::DEVICE::kGamepadOrbis);

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
			BeginDisabled();

		if (Hotkey(finalLabel, k, m1, m2, alignFar, labelLeft, h.isBinding, alwaysHighlight, iconScale)) {
			clicked = true;
		}

		if (disabled)
			EndDisabled();

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
			PopFont();
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
			true, GetStyle().FrameRounding);
		DrawWidgetBorder(window->DrawList, bb, active || h || IsWidgetFocused(id), GetStyle().FrameRounding);

		if (DragBehavior(id, type, data, speed, min, max, fmt, flags)) {
			changed = true;
		}
		if (changed)
			MarkItemEdited(id);

		char        buf[64] = "";
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

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

		// Calculate exact integer center for the row to guarantee pixel alignment
		float centerY = std::floor(bb.Min.y + (bb.GetHeight() * 0.5f));

		ImRect track = bb;

		// Force track height to an even number so it splits around centerY
		float trackH = std::floor(bb.GetHeight() * 0.50f);
		if (static_cast<int>(trackH) % 2 != 0)
			trackH -= 1.0f;

		track.Min.y = centerY - (trackH * 0.5f);
		track.Max.y = track.Min.y + trackH;

		window->DrawList->AddRectFilled(track.Min, track.Max, GetColorU32(active ? ImGuiCol_FrameBgActive : h ? ImGuiCol_FrameBgHovered :
																												ImGuiCol_FrameBg),
			0.0f);
		DrawWidgetBorder(window->DrawList, track, IsWidgetFocused(id) || active || h, 0.0f);

		if (grab.Max.x > grab.Min.x) {
			float scale     = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
			float knobWidth = 10.0f * scale;

			float centerX = std::floor(grab.Min.x + (grab.Max.x - grab.Min.x) * 0.5f);
			centerX       = ImClamp(centerX, bb.Min.x + knobWidth * 0.5f, bb.Max.x - knobWidth * 0.5f);

			// Force knob height to an even number
			float knobH = std::floor(bb.GetHeight() * 0.70f);
			if (static_cast<int>(knobH) % 2 != 0)
				knobH -= 1.0f;

			float knobMinY = centerY - (knobH * 0.5f);

			ImRect customGrab(
				std::floor(centerX - (knobWidth * 0.5f)),
				knobMinY,
				std::floor(centerX + (knobWidth * 0.5f)),
				knobMinY + knobH);

			window->DrawList->AddRectFilled(customGrab.Min, customGrab.Max, GetColorU32(active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), GetStyle().GrabRounding);
		}

		char        buf[64] = "";
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

		return changed;
	}

	bool ComboForm(const char* label, RE::FormID* currentFormID, RE::FormType formType)
	{
		// Caches forms based on requested type to prevent continuous expensive lookups
		ImGuiID id       = GetID(label);
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

	bool ComboFormStr(const char* label, std::string* currentEdid, RE::FormType formType)
	{
		ImGuiID id       = GetID(label);
		ImGuiID cacheKey = ImHashData(&formType, sizeof(formType), id);

		auto [it, inserted] = s_FormCaches.try_emplace(cacheKey, label);
		it->second.InitForms(formType);

		if (currentEdid && !currentEdid->empty()) {
			it->second.Sync(*currentEdid);
		}

		bool changed = false;

		it->second.GetFormResultFromCombo([&](RE::TESForm* form) {
			if (form) {
				std::string edid = clib_util::editorID::get_editorID(form);
				if (!edid.empty()) {
					*currentEdid = edid;
					changed      = true;
				}
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
		bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;

		// Calculate total layout size using same padY as combo boxes
		float scale       = Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
		float padY        = 7.0f * scale;
		float frameHeight = CalcTextSize(label).y + padY * 2.0f;

		ImVec2 pos = window->DC.CursorPos;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, frameHeight));

		ItemSize(bb);
		if (!ItemAdd(bb, id))
			return is_open && !is_leaf;

		bool h, held;
		if (ButtonBehavior(bb, id, &h, &held)) {
			if (!is_leaf) {
				is_open = !is_open;
				window->DC.StateStorage->SetInt(id, is_open);
				PlayAudio(is_open ? Audio::kFocus : Audio::kCancel);
			}
		}

		// Check selection flag
		bool is_selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;

		// Draw Background
		if (h || is_open || is_selected) {
			ImU32 bgColor;
			if (is_selected) {
				bgColor = GetColorU32(ImGuiCol_Header);
			} else if (is_open && !is_leaf) {
				bgColor = GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox);
			} else {
				bgColor = GetColorU32(ImGuiCol_HeaderHovered);
			}

			// Only draw bg if active/hovered/selected
			if (h || is_selected || (is_open && !is_leaf)) {
				window->DrawList->AddRectFilled(bb.Min, bb.Max, bgColor, GImGui->Style.FrameRounding);
			}

			if (is_open && !is_leaf) {
				DrawWidgetBorder(window->DrawList, bb, h || IsWidgetFocused(id), GImGui->Style.FrameRounding);
			}
		}

		// Push the icon inwards to give it breathing room from the edge
		float padX = std::max(GImGui->Style.FramePadding.x, 10.0f * scale);

		float baseArrowSize = 30.0f;

		// Scale icon for the layout footprint
		float maxIconDim = baseArrowSize * scale;
		float textOff    = padX + maxIconDim + GImGui->Style.ItemInnerSpacing.x;

		// Pass the unscaled base icon (only if not a leaf)
		if (!is_leaf) {
			DrawTreeIcon(window->DrawList, { bb.Min.x + padX, bb.Min.y }, frameHeight, is_open, h, baseArrowSize);
		}

		// Vertically centre text
		ImVec2 textSize = CalcTextSize(label);
		float  textY    = bb.Min.y + (frameHeight - textSize.y) * 0.5f;

		RenderText({ bb.Min.x + textOff, textY }, label);
		return is_open && !is_leaf;
	}

	bool TreeNodeIcon(const char* label, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID      id     = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open     = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);
		bool is_leaf     = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
		bool no_push     = (flags & ImGuiTreeNodeFlags_NoTreePushOnOpen) != 0;
		bool is_selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;

		float scale       = Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
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
			if (is_open && !is_leaf && !no_push)
				TreePush(label);
			return is_open && !is_leaf;
		}

		bool h, held;
		if (ButtonBehavior(bb, id, &h, &held, flags)) {
			if (!is_leaf) {
				is_open = !is_open;
				window->DC.StateStorage->SetInt(id, is_open);
				PlayAudio(is_open ? Audio::kFocus : Audio::kCancel);
			}
		}

		if (h || is_selected) {
			ImU32 bgColor = GetColorU32(is_selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered);
			window->DrawList->AddRectFilled(bb.Min, bb.Max, bgColor, GImGui->Style.FrameRounding);
		}

		// Pass the unscaled base icon (only if not a leaf)
		if (!is_leaf) {
			DrawTreeIcon(window->DrawList, { pos.x + padX, pos.y }, frameHeight, is_open, h, baseArrowSize);
		} else if (flags & ImGuiTreeNodeFlags_Bullet) {
			// Optional: draw a small bullet point for leaves if requested
			window->DrawList->AddCircleFilled({ pos.x + padX + (maxIconDim * 0.5f), pos.y + (frameHeight * 0.5f) }, 3.0f * scale, GetColorU32(ImGuiCol_Text));
		}

		// Align text
		float textY = bb.Min.y + (frameHeight - CalcTextSize(label).y) * 0.5f;

		RenderText({ pos.x + textOff, textY }, label);

		if (is_open && !is_leaf && !no_push)
			TreePush(label);

		return is_open && !is_leaf;
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
		float uiScale   = Renderer::GetResolutionScale();
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

		auto  largeFont = MANAGER(IconFont)->GetLargeFont();
		float fontScale = FUCKMan::GetSingleton()->GetActiveScale() * Renderer::GetResolutionScale();
		float fontSize  = (largeFont ? largeFont->LegacySize : GetStyle().FontSizeBase) * fontScale;

		PushFont(largeFont, fontSize);
		RenderTextClipped(widgetBounds.Min + ImVec2(arrowSize.x, 0), widgetBounds.Max - ImVec2(arrowSize.x, 0), centerText.data(), nullptr, nullptr, { 0.5f, 0.5f });
		PopFont();

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
		bool pressed = Selectable(label, selected, flags, size);
		if (pressed)
			PlayAudio(Audio::kFocus);
		return pressed;
	}

	template <typename TWidgetFunc>
	bool OutsetFramedWidget(const char* label, TWidgetFunc drawWidget)
	{
		std::string id = std::format("##{}", label);

		float scale      = Renderer::GetResolutionScale() * (FUCKMan::GetSingleton()->GetActiveScale());
		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float rounding   = GetStyle().FrameRounding;

		float padX     = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float tallPadY = 7.0f * scale;

		float tColor    = std::max(1.0f, std::floor(borderSize));
		float innerPadY = std::max(0.0f, tallPadY - tColor);
		float offY      = tallPadY - innerPadY;

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, tallPadY));
		LeftAlignedTextImpl(label);
		PopStyleVar();

		float widgetWidth = CalcItemWidth();

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) {
			GImGui->NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasWidth;
		}

		float fullHeight = GetFontSize() + (tallPadY * 2.0f);

		ImVec2 screenPos = GetCursorScreenPos();
		ImRect bb(screenPos, screenPos + ImVec2(widgetWidth, fullHeight));

		SetNextItemAllowOverlap();
		InvisibleButton((id + "_bg").c_str(), bb.GetSize());
		bool bgHovered = IsItemHovered();
		bool bgClicked = IsItemClicked();

		ImVec2 nextItemCursor = GetCursorPos();

		SetCursorScreenPos(screenPos + ImVec2(0.0f, offY));
		SetNextItemWidth(widgetWidth);

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, innerPadY));
		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

		auto* drawList = GetWindowDrawList();
		drawList->ChannelsSplit(2);
		drawList->ChannelsSetCurrent(1);

		bool result = drawWidget(id.c_str());

		bool hovered = bgHovered || IsItemHovered();
		bool active  = IsItemActive();
		bool focused = IsWidgetFocused(GetID(id.c_str()));

		if (bgClicked) {
			SetFocusID(GetID(id.c_str()), GetCurrentWindow());
		}

		drawList->ChannelsSetCurrent(0);

		drawList->AddRectFilled(bb.Min, bb.Max, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), rounding);

		if (borderSize > 0.0f) {
			DrawWidgetBorder(drawList, bb, hovered || active || focused, rounding);
		}

		drawList->ChannelsMerge();

		PopStyleVar(2);
		PopStyleColor(3);

		SetCursorPos(nextItemCursor);

		Dummy(ImVec2(0.0f, 0.0f));

		return result;
	}

	bool ColorEdit3Styled(const char* label, float col[3], int flags)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ColorEdit3(id, col, flags | ImGuiColorEditFlags_NoLabel);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
		else if (IsItemActivated())
			PlayAudio(Audio::kFocus);
		return res;
	}

	bool ColorEdit4Styled(const char* label, float col[4], int flags)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return ColorEdit4(id, col, flags | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
		else if (IsItemActivated())
			PlayAudio(Audio::kFocus);
		return res;
	}

	bool DragFloat2Styled(const char* label, float v[2], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return DragFloat2(id, v, speed, min, max, fmt);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
		return res;
	}

	bool DragFloat3Styled(const char* label, float v[3], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return DragFloat3(id, v, speed, min, max, fmt);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
		return res;
	}

	bool DragFloat4Styled(const char* label, float v[4], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			return DragFloat4(id, v, speed, min, max, fmt);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
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
				PlayAudio(Audio::kPrevNext);
			}
			if (pR) {
				*outRight = true;
				PlayAudio(Audio::kPrevNext);
			}
		}
	}
}
