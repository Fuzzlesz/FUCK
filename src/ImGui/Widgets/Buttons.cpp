#include "WidgetsInternal.h"

namespace ImGui
{
	using namespace WidgetsDetail;

	namespace
	{
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
			float scale = GetDynamicWidgetScale();

			// Give it a gap so it actually breathes,
			float nearSpacing = std::max(8.0f * scale, g.Style.ItemInnerSpacing.x);

			// Near (Tightly Coupled) - Uses LabelAlign.X as a rigid spacer pushing the label away
			float labelSpacer = style.labelAlign.x * 50.0f * scale;

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
				// Add trailing padding to the bounding box so adjacent SameLine() widgets don't blur together
				float trailingPadding = 16.0f * scale;

				if (labelLeft) {
					DrawLabel(startX);
					widgetX = startX + labelWidth + nearSpacing + labelSpacer;
					maxX    = widgetX + contentWidth + trailingPadding;
				} else {
					widgetX      = startX;
					float labelX = startX + contentWidth + nearSpacing + labelSpacer;
					DrawLabel(labelX);
					maxX = labelX + labelWidth + trailingPadding;
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
	}

	// The theme's nudge for button and tab labels, scaled like the rest of the widget.
	// Whole pixels, so the text stays crisp
	static ImVec2 GetButtonTextOffset()
	{
		const ImVec2& offset = Styles::GetSingleton()->user.buttonTextOffset;
		const float   scale  = GetDynamicWidgetScale();
		return { std::round(offset.x * scale), std::round(offset.y * scale) };
	}

	bool CheckBox(const char* label, bool* a_toggle, bool alignFar, bool labelLeft)
	{
		bool        selected   = false;
		auto        icon       = MANAGER(IconFont)->GetCheckbox();
		auto        iconFilled = MANAGER(IconFont)->GetCheckboxFilled();
		std::string idStr      = std::format("##{}", label);

		float scale         = GetDynamicWidgetScale();
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

		float scale         = GetDynamicWidgetScale();
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
				AddThemedRect(draw_list, visBB, GetColorU32(ImGuiCol_NavHighlight), GetFrameRounding(), 2.0f * scale);
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

			AddThemedRectFilled(draw_list, railBB, col_rail_fill, GetFrameRounding());
			DrawWidgetBorder(draw_list, railBB, hovered || isFocused, GetFrameRounding());

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

	void DrawTabBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding)
	{
		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);

		if (borderSize <= 0.0f)
			return;

		float tColor, tBlack;
		GetBorderThickness(borderSize, tColor, tBlack);

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
				if (UseInvertedCorners()) {
					ImVec2 centerL = { min.x + inset, min.y + inset };
					ImVec2 centerR = { max.x - inset, min.y + inset };
					PathInvertedCorner(dl, centerL, rounding, IM_PI / 2, 0.0f);
					PathInvertedCorner(dl, centerR, rounding, IM_PI, IM_PI / 2);
				} else {
					dl->PathArcTo({ min.x + rounding, min.y + rounding }, r, IM_PI, IM_PI * 1.5f, 24);
					dl->PathArcTo({ max.x - rounding, min.y + rounding }, r, IM_PI * 1.5f, IM_PI * 2.0f, 24);
				}
			} else {
				dl->PathLineTo({ min.x + inset, min.y + inset });
				dl->PathLineTo({ max.x - inset, min.y + inset });
			}

			dl->PathLineTo({ max.x - inset, botY });
		};

		bool  inverted = UseInvertedCorners();
		float rOuter   = inverted ? rounding : rounding + halfB;
		float rInner   = inverted ? rounding : std::max(0.0f, rounding - halfC);

		// Outer Black stroke (No bottom line)
		drawList->PathClear();
		buildPath(drawList, -halfB, rOuter, max.y + halfB);  // Extend down past max.y slightly so it connects to separator cleanly
		drawList->PathStroke(IM_COL32(0, 0, 0, 255), 0, tBlack);

		// Inner Color stroke (No bottom line)
		drawList->PathClear();
		buildPath(drawList, halfC, rInner, max.y);
		drawList->PathStroke(col, 0, tColor);
	}

	bool BeginTabBarEx(const char* str_id, ImGuiTabBarFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();

		// BeginTabItemEx() draws tabs as tall as an OutlineButton, centred on ImGui's shorter tab row, so they
		// reach up above the bar. Make room for that, unless the bar is the first thing in the window
		if (!window->SkipItems && window->DC.CursorPos.y > window->DC.CursorStartPos.y) {
			const float padY     = 7.0f * GetDynamicWidgetScale();
			float       overflow = std::max(0.0f, (GetFontSize() + padY * 2.0f - GetFrameHeight()) * 0.5f);

			const float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
			if (borderSize > 0.0f) {
				float tColor, tBlack;
				GetBorderThickness(borderSize, tColor, tBlack);
				overflow += tBlack * 0.5f;
			}

			SetCursorPosY(GetCursorPosY() + std::ceil(overflow));
		}

		return BeginTabBar(str_id, flags | ImGuiTabBarFlags_NoTabListScrollingButtons);
	}

	bool BeginTabItemEx(const char* label, ImGuiTabItemFlags flags)
	{
		ImGuiWindow* window     = GetCurrentWindow();
		ImGuiID      id         = GetID(label);
		ImGuiID      storageKey = GetID("##LastActiveTab");
		ImGuiID      lastActive = window->StateStorage.GetInt(storageKey, 0);

		bool  wasActive = (lastActive == id);
		float scale     = GetDynamicWidgetScale();

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

		// Tabs share button rounding, so they curve the same
		float rounding = KeepInnerCurve(GetUserStyleVar(USER_STYLE::kButtonRounding));

		// Clamp rounding to prevent geometry artifacting at extreme scale
		rounding = std::min(rounding, drawBb.GetWidth() * 0.5f);
		rounding = std::min(rounding, drawBb.GetHeight());

		bool inverted = UseInvertedCorners();

		window->DrawList->PathClear();

		if (inverted && rounding > 0.0f) {
			auto prev_flags = window->DrawList->Flags;
			window->DrawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;

			window->DrawList->PathLineTo(ImVec2((drawBb.Min.x + drawBb.Max.x) * 0.5f, drawBb.Max.y));
			PathInvertedCorner(window->DrawList, { drawBb.Min.x, drawBb.Min.y }, rounding, IM_PI / 2, 0.0f);
			PathInvertedCorner(window->DrawList, { drawBb.Max.x, drawBb.Min.y }, rounding, IM_PI, IM_PI / 2);
			window->DrawList->PathLineTo({ drawBb.Max.x, drawBb.Max.y });
			window->DrawList->PathLineTo({ drawBb.Min.x, drawBb.Max.y });
			window->DrawList->PathLineTo({ drawBb.Min.x, drawBb.Min.y + rounding });

			window->DrawList->AddConvexPolyFilled(window->DrawList->_Path.Data, window->DrawList->_Path.Size, GetColorU32(ImGuiCol_Button));
			window->DrawList->PathClear();

			window->DrawList->Flags = prev_flags;
		} else {
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
		}

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

		// Centred inside the un-chopped box, locking it to the OutlineButton baseline, then nudged by the theme
		ImVec2 textOff = GetButtonTextOffset();
		RenderTextClipped(idealBb.Min + textOff, idealBb.Max + textOff, label, NULL, &textSize, { 0.5f, 0.5f });

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

		float scale    = GetDynamicWidgetScale();
		float rounding = KeepInnerCurve(GetUserStyleVar(USER_STYLE::kButtonRounding));

		// Measure only the visible part, RenderTextClipped below hides "##id" as well
		ImVec2 textSize = CalcTextSize(label, nullptr, true);

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
		AddThemedRectFilled(window->DrawList, bbVisual, GetColorU32(ImGuiCol_Button), rounding);
		DrawWidgetBorder(window->DrawList, bbVisual, h || held, rounding);

		// Centred, then nudged by the theme
		ImVec2 textOff = GetButtonTextOffset();
		RenderTextClipped(bbVisual.Min + textOff, bbVisual.Max + textOff, label, NULL, &textSize, { 0.5f, 0.5f });

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

		float scale = GetDynamicWidgetScale();

		ImGuiContext& g       = *GImGui;
		const float   layoutH = GetFrameHeight();
		const float   spacing = std::max(1.0f, std::floor(g.Style.ItemInnerSpacing.x * 0.5f));

		const float baseFrameH = 38.0f * scale;

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
				float visualNudgeY = (item.type == RenderItem::kText) ? std::floor(1.5f * scale) : 0.0f;

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

	bool SelectableStyled(const char* label, bool selected, int flags, const ImVec2& size)
	{
		bool pressed = Selectable(label, selected, flags, size);
		if (pressed)
			PlayAudio(Audio::kFocus);
		return pressed;
	}
}
