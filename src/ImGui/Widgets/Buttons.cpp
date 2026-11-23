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

		// Set around a hotkey drawn with HotkeyFlags::kRegularLabel, so its label's in the regular face
		bool s_regularLabel = false;

		// Lays out a label and a widget on one row. drawContent is given the whole row, so a widget can take clicks anywhere along it.
		// labelLit keeps the label lit, as a hotkey with kAlwaysHighlight has it
		template <typename TDrawFunc>
		void AlignedWidgetLayout(const char* label, bool alignFar, bool labelLeft, float contentWidth, TDrawFunc drawContent, float targetHeight = -1.0f, bool labelLit = false)
		{
			if (label && label[0] && !std::string_view(label).starts_with("##"))
				AddRowSpacing();
			BeginGroup();
			PushID(label);

			ImGuiContext& g      = *GImGui;
			float         startX = GetCursorPosX();
			float         startY = GetCursorPosY();

			float fullAvailX = GetContentRegionAvail().x;
			auto& style      = Styles::GetSingleton()->user;

			// The split is measured from where the region starts rather than the cursor, so an indented row's widget lines up with those
			// above it, the indent coming out of the gap after its label
			ImGuiWindow* window      = GetCurrentWindow();
			float        regionStart = window->WorkRect.Min.x - window->Pos.x + window->Scroll.x;
			float        regionWidth = (startX + fullAvailX) - regionStart;

			float rightPaneStart = regionStart + (regionWidth * style.widgetSplit) + g.Style.ItemInnerSpacing.x;
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

			ImFont* labelFont  = s_regularLabel ? GetFont() : GetLabelFont();
			float   labelWidth = labelFont->CalcTextSizeA(GetFontSize(), FLT_MAX, 0.0f, labelView.data(), labelView.data() + labelView.size()).x;

			// Calculate true dynamic centring, supporting widgets (like Icons) that are taller than a standard frame
			float actualTargetH = targetHeight > 0.0f ? targetHeight : GetFrameHeight();
			float textH         = GetTextLineHeight();
			float offY          = std::floor((actualTargetH - textH) * style.labelAlign.y);

			// Positions in the row as screen coordinates, the cursor still being at the row's start
			const ImVec2 rowScreen = GetCursorScreenPos();
			auto         ScreenX   = [&](float a_x) { return std::floor(rowScreen.x + (a_x - startX)); };

			// Tighter gap globally for near-aligned widgets to closely mimic vanilla grouping
			float scale = GetDynamicWidgetScale();

			// Give it a gap so it actually breathes,
			float nearSpacing = std::max(8.0f * scale, g.Style.ItemInnerSpacing.x);

			// Near (Tightly Coupled) - Uses LabelAlign.X as a rigid spacer pushing the label away
			float labelSpacer = style.labelAlign.x * 50.0f * scale;

			float widgetX = startX;
			float labelX  = startX;
			float maxX    = startX;

			if (alignFar) {
				// Centre the widget inside the right-hand pane
				float rightPaneCenter = rightPaneStart + (rightPaneEnd - rightPaneStart) * 0.5f;
				float splitPoint      = rightPaneCenter - (contentWidth * 0.5f);

				if (labelLeft) {
					labelX  = startX;
					widgetX = splitPoint;
				} else {
					widgetX = startX;
					labelX  = (splitPoint + contentWidth) - labelWidth;
				}
				maxX = rightPaneEnd;
			} else {
				// Pack the widget tightly against the label
				// Add trailing padding to the bounding box so adjacent SameLine() widgets don't blur together
				float trailingPadding = 16.0f * scale;

				if (labelLeft) {
					labelX  = startX;
					widgetX = startX + labelWidth + nearSpacing + labelSpacer;
					maxX    = widgetX + contentWidth + trailingPadding;
				} else {
					widgetX = startX;
					labelX  = startX + contentWidth + nearSpacing + labelSpacer;
					maxX    = labelX + labelWidth + trailingPadding;
				}
			}

			// The label is drawn straight to the list, so ImGui's cursor and item bounds aren't disturbed (SameLine() and ItemSize() stay
			// in step). It's lit, with a faint band behind the row, while the row is under the mouse or holds the focus
			const ImRect row({ ScreenX(startX), rowScreen.y }, { ScreenX(maxX), rowScreen.y + actualTargetH });
			const bool   rowLit = IsRowLit(row);
			if (rowLit)
				DrawRowHighlight(row);
			const ImVec2 labelPos(ScreenX(labelX), std::floor(rowScreen.y + std::max(0.0f, offY)));
			GetWindowDrawList()->AddText(labelFont, GetFontSize(), labelPos, GetLabelColour(rowLit || labelLit), labelView.data(), labelView.data() + labelView.size());

			SetCursorPosX(widgetX);
			SetCursorPosY(startY);

			drawContent(row);
			const ImGuiID contentId = g.LastItemData.ID;

			// Explicitly register the furthest X bound so AutoResize windows don't clip
			SetCursorPosX(maxX);
			SetCursorPosY(startY);
			ItemSize(ImVec2(0.0f, actualTargetH));

			PopID();
			EndGroup();

			// The group reports the widget inside as its item, so a tooltip's hover delay keeps one ID (the group's ID otherwise changes
			// while the widget is held, restarting the delay, so the tooltip blinks on every click)
			if (contentId != 0)
				g.LastItemData.ID = contentId;
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

		float layoutH       = std::max(GetFrameHeight(), GetFontSize() + 14.0f * scale);  // a button's height, so rows stack alike
		float actualLayoutH = std::max(layoutH, iconH);
		float offY          = std::floor((actualLayoutH - iconH) * 0.5f);

		auto DrawContent = [&](const ImRect& a_row) {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiID id = window->GetID(idStr.c_str());
			ImVec2  p  = window->DC.CursorPos;

			// Laid out at the icon's size, and clicked anywhere along the row, label included
			ImRect bb(p, p + ImVec2(iconW, actualLayoutH));
			ItemSize(bb);
			if (!ItemAdd(a_row, id))
				return;

			bool hovered, held;
			bool pressed = ButtonBehavior(a_row, id, &hovered, &held);
			if (pressed) {
				*a_toggle = !*a_toggle;
				selected  = true;
			}

			// Always at full brightness, ticked or not, as the label is what lights up
			ImVec4 tint = GetHighlightTint(*a_toggle, true, false);
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

		float layoutH       = std::max(GetFrameHeight(), GetFontSize() + 14.0f * scale);
		float actualLayoutH = std::max(layoutH, visualH);
		float offY          = std::floor((actualLayoutH - visualH) * 0.5f);

		auto DrawContent = [&](const ImRect& a_row) {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiContext& g  = *GImGui;
			const ImGuiID id = window->GetID(idStr.c_str());
			ImVec2        p  = window->DC.CursorPos;

			// Laid out at the toggle's size, and clicked anywhere along the row, label included
			ImRect bb(p, p + ImVec2(visualW, actualLayoutH));
			ItemSize(bb);
			if (!ItemAdd(a_row, id))
				return;

			bool hovered, held;
			if (ButtonBehavior(a_row, id, &hovered, &held)) {
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

		// Inner Colour stroke (No bottom line)
		drawList->PathClear();
		buildPath(drawList, halfC, rInner, max.y);
		drawList->PathStroke(col, 0, tColor);
	}

	bool BeginTabBarEx(const char* str_id, ImGuiTabBarFlags flags)
	{
		// A row's gap above it, as the other widgets leave
		AddRowSpacing();
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

	// The bar takes its width from the work rect as it begins, so narrowing that for the call alone leaves the room
	bool BeginTabBarWithTrailing(const char* str_id, float a_trailingWidth, ImGuiTabBarFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		const float  right  = window->WorkRect.Max.x;
		window->WorkRect.Max.x = std::max(window->DC.CursorPos.x, right - a_trailingWidth - GetStyle().ItemSpacing.x);
		const bool open = BeginTabBarEx(str_id, flags);
		window->WorkRect.Max.x = right;
		return open;
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

		// Hide the native text rendering by passing a transparent colour
		PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));

		// Widen the X padding to match OutlineButtons, but LEAVE Y ALONE so we don't break the layout engine
		float padX = 8.0f * scale;
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, GetStyle().FramePadding.y));

		window->DrawList->ChannelsSplit(2);
		window->DrawList->ChannelsSetCurrent(1);

		bool active = BeginTabItem(label, nullptr, flags);

		PopStyleVar();
		PopStyleColor(6);  // Transparent text + 5 tab colours

		// Switch to background channel for custom border
		window->DrawList->ChannelsSetCurrent(0);

		// Kept within the bar across, as ImGui clips its own tabs, so one scrolled off an end can't spill onto the frame or
		// under what's drawn beside a trailing bar
		const ImVec4 clip = window->DrawList->_CmdHeader.ClipRect;
		ImVec2       clipMin(clip.x, clip.y), clipMax(clip.z, clip.w);
		if (const ImGuiTabBar* bar = GImGui->CurrentTabBar) {
			clipMin.x = std::max(clipMin.x, bar->BarRect.Min.x);
			clipMax.x = std::min(clipMax.x, bar->BarRect.Max.x);
		}
		window->DrawList->PushClipRect(clipMin, ImVec2(std::max(clipMin.x, clipMax.x), clipMax.y), false);

		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();

		// Mathematically match OutlineButton's visual geometry. Measured without any ## id, as it's drawn
		ImVec2 textSize = CalcTextSize(label, nullptr, true);
		float  padY     = 7.0f * scale;
		float  visualH  = textSize.y + (padY * 2.0f);
		float  logicalH = GetFrameHeight();
		float  offY     = (logicalH - visualH) * 0.5f;

		// This ideal bounding box is identical to OutlineButton in size and vertical position
		ImRect idealBb = { min.x, min.y + offY, max.x, min.y + offY + visualH };

		// Chop the bottom of the drawn rect at max.y so it rests flush on the separator line
		// Use std::floor to prevent subpixel geometry artefacts
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

		// Switch to foreground channel to render our custom centred text
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

		window->DrawList->PopClipRect();

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

	bool OutlineButton(const char* label, bool* wasFocused, bool selected, float minTextWidth)
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

		// Sized for the wider of its label and minTextWidth, so a button flipping between labels keeps one width
		const float textW   = std::max(textSize.x, minTextWidth);
		float       visualH = textSize.y + (padY * 2.0f);
		float       width   = textW + (padX * 2.0f);

		// An icon alone, from either icon set's private-use codes, makes a square button
		const char*  shownEnd = FindRenderedTextEnd(label);
		unsigned int icon     = 0;
		const char*  afterOne = label + ImTextCharFromUtf8(&icon, label, shownEnd);
		const bool   iconOnly = afterOne == shownEnd && icon >= 0xE000 && icon <= 0xF8FF;
		if (iconOnly)
			width = std::max(visualH, textW + padY * 2.0f);

		// Force the layout slot to encompass the true visual height
		float  actualLayoutH = std::max(GetFrameHeight(), visualH);
		ImVec2 logical_sz    = ImVec2(width, actualLayoutH);

		ImVec2 pos = window->DC.CursorPos;
		ImRect bb(pos, pos + logical_sz);

		ItemSize(logical_sz);
		if (!ItemAdd(bb, window->GetID(label)))
			return false;

		// Shift visual rect vertically to centre it cleanly and strictly align to pixel boundaries
		float  offY = (logical_sz.y - visualH) * 0.5f;
		ImRect bbVisual(std::floor(pos.x), std::floor(pos.y + offY), std::floor(pos.x + width), std::floor(pos.y + offY + visualH));

		bool h, held;
		bool p = ButtonBehavior(bb, window->GetID(label), &h, &held);

		// Background stays solid; hover feedback is handled by the Border, which a selected button keeps lit
		AddThemedRectFilled(window->DrawList, bbVisual, GetColorU32(ImGuiCol_Button), rounding);
		DrawWidgetBorder(window->DrawList, bbVisual, h || held || selected, rounding);

		// Centred, then nudged by the theme. An icon alone is centred by its own drawing, as icons sit on the middle of the text's
		// capitals, which is above the middle of the line the text is centred by
		ImVec2             textOff = GetButtonTextOffset();
		const ImFontGlyph* glyph   = iconOnly ? GetFontBaked()->FindGlyph(static_cast<ImWchar>(icon)) : nullptr;
		if (glyph && glyph->Visible) {
			const ImVec2 middle = (bbVisual.Min + bbVisual.Max) * 0.5f + textOff;
			const ImVec2 at(std::round(middle.x - (glyph->X0 + glyph->X1) * 0.5f), std::round(middle.y - (glyph->Y0 + glyph->Y1) * 0.5f));
			RenderText(at, label, shownEnd, false);
		} else {
			RenderTextClipped(bbVisual.Min + textOff, bbVisual.Max + textOff, label, NULL, &textSize, { 0.5f, 0.5f });
		}

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
		float layoutH       = std::max(GetFrameHeight(), GetFontSize() + 14.0f * GetDynamicWidgetScale());
		float actualLayoutH = std::max(layoutH, size.y);
		float offY          = std::floor((actualLayoutH - size.y) * 0.5f);

		auto DrawContent = [&](const ImRect& a_row) {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiID id = window->GetID(idStr.c_str());
			ImVec2  p  = window->DC.CursorPos;

			// Laid out at the icon's size, and clicked anywhere along the row, label included
			ImRect bb(p, p + ImVec2(size.x, actualLayoutH));
			ItemSize(bb);
			if (!ItemAdd(a_row, id))
				return;

			bool hovered, held;
			bool pressed = ButtonBehavior(a_row, id, &hovered, &held);
			if (pressed)
				clicked = true;

			// Always at full brightness, as the label is what lights up
			ImVec4 tint = GetHighlightTint(true, true, false);

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
		const float   layoutH = std::max(GetFrameHeight(), GetFontSize() + 14.0f * scale);
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

		auto DrawContent = [&](const ImRect& a_row) {
			ImGuiWindow* window = GetCurrentWindow();
			if (window->SkipItems)
				return;
			ImGuiID id = window->GetID(baseId.c_str());
			ImVec2  p  = window->DC.CursorPos;

			// If alignFar is true, the keys stretch backwards to take in the modifiers
			float  leftEdge = alignFar ? p.x - (totalWidth - anchorWidth) : p.x;
			ImRect bb(ImVec2(leftEdge, p.y), ImVec2(leftEdge + totalWidth, p.y + actualLayoutH));

			// Clicked anywhere along the row, label included, so clicking the label starts rebinding as MCM Helper's hotkeys do
			ImRect hit = a_row;
			hit.Add(bb);
			ItemSize(bb);
			if (!ItemAdd(hit, id))
				return;

			bool hovered, held;
			bool pressed = ButtonBehavior(hit, id, &hovered, &held);
			if (pressed)
				clicked = true;

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
					ImVec4 tint     = flashing ? flashCol : GetHighlightTint(true, true, false);

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

		AlignedWidgetLayout(label, alignFar, labelLeft, contentWidth, DrawContent, actualLayoutH, alwaysHighlight);

		if (clicked)
			PlayAudio(Audio::kFocus);
		return clicked;
	}

	bool DrawManagedHotkey(const char* label, FUCK::ManagedHotkey& h, FUCK::HotkeyFlags flags, float iconScale, float labelScale)
	{
		// The current font at a fraction of its size, as fonts scale freely, so a label's size follows labelScale evenly from 1 down
		bool popFont = false;
		if (labelScale != 1.0f) {
			PushFont(nullptr, GetFontSize() * labelScale);
			popFont = true;
		}

		bool alignFar        = !(flags & FUCK::HotkeyFlags::kAlignNear);
		bool labelLeft       = !(flags & FUCK::HotkeyFlags::kLabelRight);
		bool ctrlToRebind    = (flags & FUCK::HotkeyFlags::kCtrlToRebind);
		bool alwaysHighlight = (flags & FUCK::HotkeyFlags::kAlwaysHighlight);
		bool noModifiers     = (flags & FUCK::HotkeyFlags::kNoModifiers);
		bool regularLabel    = (flags & FUCK::HotkeyFlags::kRegularLabel);

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

		s_regularLabel = regularLabel;
		if (Hotkey(finalLabel, k, m1, m2, alignFar, labelLeft, h.isBinding, alwaysHighlight, iconScale)) {
			clicked = true;
		}
		s_regularLabel = false;

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

	namespace
	{
		// A card's name shrinks with the card, so a small one still shows more than a few letters before the cut
		float CardNameScale(float a_side)
		{
			const float font = GetFontSize();
			if (font <= 0.0f || a_side <= 0.0f)
				return 1.0f;
			return std::clamp((a_side / font) / 7.0f, 0.55f, 1.0f);
		}

		float CardNameHeight(float a_side) { return GetFontSize() * CardNameScale(a_side) * 1.4f; }

		// Text with a dark ring, so it reads on any picture
		void AddRingedText(ImDrawList* a_drawList, float a_size, const ImVec2& a_pos, ImU32 a_col, const char* a_text)
		{
			const float ring = std::max(1.0f, a_size * 0.045f);
			for (int dy = -1; dy <= 1; dy += 2) {
				for (int dx = -1; dx <= 1; dx += 2)
					a_drawList->AddText(GetFont(), a_size, ImVec2(a_pos.x + dx * ring, a_pos.y + dy * ring), IM_COL32(0, 0, 0, 150), a_text);
			}
			a_drawList->AddText(GetFont(), a_size, a_pos, a_col, a_text);
		}
	}

	float GetCardHeight(float a_side, float a_aspect, bool a_withName)
	{
		return a_side * a_aspect + (a_withName ? CardNameHeight(a_side) : 0.0f);
	}

	bool Card(const FUCK::CardDesc& a_desc, ImTextureID a_tex, FUCK::CardResult& a_out)
	{
		a_out = {};
		if (!a_desc.id || a_desc.side <= 0.0f)
			return false;

		const bool  withName = a_desc.name && a_desc.name[0] != '\0';
		const float pictureH = a_desc.side * std::max(0.01f, a_desc.aspect);
		const float height   = GetCardHeight(a_desc.side, a_desc.aspect, withName);

		const ImVec2 min = GetCursorScreenPos();
		const ImVec2 max(min.x + a_desc.side, min.y + height);
		const ImRect picture(min, ImVec2(max.x, min.y + pictureH));

		// Reachable by the keys and the pad, which an invisible button isn't unless asked
		a_out.clicked    = InvisibleButton(a_desc.id, ImVec2(a_desc.side, height), ImGuiButtonFlags_EnableNav);
		a_out.hovered    = IsItemHovered();
		const bool focus = IsItemFocused();
		a_out.pictureMin = picture.Min;
		a_out.pictureMax = picture.Max;

		// Its picture shown large over the card's tooltip, when the caller sets one
		if ((a_out.hovered || focus) && a_tex && a_desc.tooltipPicture > 0.0f)
			SetTooltipPicture(a_tex, ImVec2(a_desc.tooltipPicture, a_desc.tooltipPicture * std::max(0.01f, a_desc.aspect)));

		auto*       drawList = GetWindowDrawList();
		const float rounding = std::min(GetFrameRounding(), a_desc.side * 0.5f);
		const float line     = std::max(1.0f, Renderer::GetResolutionScale());

		AddThemedRectFilled(drawList, ImRect(min, max), GetColorU32(ImGuiCol_FrameBg), rounding);

		// The picture, else what's happening to it
		const ImU32  faint  = GetColorU32(ImGuiCol_TextDisabled);
		const ImVec2 centre = picture.GetCenter();
		const float  mark   = std::min(a_desc.side, pictureH) * 0.3f;
		if (a_tex) {
			const ImU32 tint = GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, std::clamp(a_desc.alpha, 0.0f, 1.0f)));
			AddThemedImage(drawList, a_tex, picture, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint, rounding, withName ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);
		} else if (a_desc.state == FUCK::ImageState::kLoading) {
			const float radius = mark * 0.4f;
			const float start  = static_cast<float>(GetTime()) * 6.0f;
			drawList->PathArcTo(centre, radius, start, start + IM_PI * 1.5f, 24);
			drawList->PathStroke(faint, ImDrawFlags_None, line * 2.0f);
		} else if (a_desc.state == FUCK::ImageState::kFailed) {
			const float h = mark * 0.5f;
			drawList->AddLine(ImVec2(centre.x - h, centre.y - h), ImVec2(centre.x + h, centre.y + h), faint, line * 2.0f);
			drawList->AddLine(ImVec2(centre.x - h, centre.y + h), ImVec2(centre.x + h, centre.y - h), faint, line * 2.0f);
		} else if (a_desc.glyph && a_desc.glyph[0] != '\0') {
			const float  size = mark;
			const ImVec2 gsz  = GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, a_desc.glyph);
			drawList->AddText(GetFont(), size, ImVec2(centre.x - gsz.x * 0.5f, centre.y - gsz.y * 0.5f), faint, a_desc.glyph);
		}

		// The star, hit-tested by hand as it sits on the card's own item
		if (a_desc.showStar) {
			const float  size = GetFontSize() * CardNameScale(a_desc.side);  // shrinks with the card, as its name does
			const float  pad  = std::max(2.0f, a_desc.side * 0.04f);
			const ImVec2 sMin(picture.Min.x + pad, picture.Min.y + pad);
			const ImRect star(sMin, ImVec2(sMin.x + size, sMin.y + size));

			const bool starHovered = a_out.hovered && star.Contains(GetMousePos());
			a_out.starHovered      = starHovered;
			a_out.favouriteAsked   = (starHovered && a_out.clicked) || ((a_out.hovered || focus) && IsKeyPressed(ImGuiKey_GamepadFaceUp, false));
			if (starHovered)
				a_out.clicked = false;
			if (a_out.favouriteAsked)
				PlayAudio(Audio::kFocus);

			if (a_desc.favourite || a_out.hovered || focus) {
				const ImU32  col = starHovered ? IM_COL32(255, 255, 100, 255) : (a_desc.favourite ? IM_COL32(255, 215, 0, 255) : faint);
				const ImVec2 ssz = GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, ICON_FA_STAR);
				AddRingedText(drawList, size, ImVec2(sMin.x + (size - ssz.x) * 0.5f, sMin.y), col, ICON_FA_STAR);
			}
		}

		if (withName) {
			const float size  = GetFontSize() * CardNameScale(a_desc.side);
			const float avail = a_desc.side - GetStyle().FramePadding.x * 2.0f;
			PushFont(nullptr, size);
			bool              cut   = false;
			const std::string label = FitTextToWidth(a_desc.name, nullptr, avail, &cut);
			const ImVec2      tsz   = CalcTextSize(label.c_str());
			PopFont();
			a_out.nameCut = cut;

			const ImU32 col = a_desc.nameColor.w > 0.0f                         ? GetColorU32(a_desc.nameColor) :
			                  (a_out.hovered || focus || a_desc.selected) ? GetUserStyleColorU32(USER_STYLE::kTextHovered) :
			                                                                GetColorU32(ImGuiCol_Text);
			drawList->AddText(GetFont(), size, ImVec2(min.x + std::max(0.0f, (a_desc.side - tsz.x) * 0.5f), picture.Max.y + (max.y - picture.Max.y - tsz.y) * 0.5f), col, label.c_str());
		}

		// Every card faintly edged, as its name band is close to the window's colour. The picked one edged as the lit tab is
		ImVec4 border = GetStyleColorVec4(ImGuiCol_Border);
		border.w *= 0.45f;
		AddThemedRect(drawList, ImRect(min, max), GetColorU32(border), rounding, line);
		if (a_desc.selected || a_out.hovered || focus)
			AddThemedRect(drawList, ImRect(min, max), GetUserStyleColorU32(a_desc.selected ? USER_STYLE::kTabBorderActive : USER_STYLE::kTextHovered), rounding, line * 2.0f);

		if (a_out.clicked)
			PlayAudio(Audio::kFocus);
		return a_out.clicked;
	}

	bool SelectableStyled(const char* label, bool selected, int flags, const ImVec2& size)
	{
		// A label wider than its row is cut short with "...", as clipped text reads as broken
		const char* shownEnd = FindRenderedTextEnd(label);
		const float avail    = GetContentRegionAvail().x;
		const float rowW     = size.x > 0.0f ? size.x : avail + std::min(size.x, 0.0f);

		bool pressed = false;
		if (rowW > 0.0f && CalcTextSize(label, shownEnd).x > rowW) {
			// "###" keeps the ID off the shown text, so the row stays the same item as it's cut. A label's own "###" ID
			// hashes the same either way
			const char* ownId  = std::strstr(label, "###");
			std::string fitted = FitTextToWidth(label, shownEnd, rowW);
			fitted += "###";
			fitted += ownId ? ownId + 3 : label;
			pressed = Selectable(fitted.c_str(), selected, flags, size);
		} else {
			pressed = Selectable(label, selected, flags, size);
		}
		if (pressed)
			PlayAudio(Audio::kFocus);
		return pressed;
	}

	FavouriteStar PlaceFavouriteStar(const ImRect& a_row, float a_size, float a_rightInset, float a_hitPad, ImFont* a_font)
	{
		ImFont* font = a_font ? a_font : GetFont();

		FavouriteStar star;
		star.size  = a_size;
		star.width = font->CalcTextSizeA(a_size, FLT_MAX, 0.0f, ICON_FA_STAR).x;

		// Centred on the glyph's own ink, as an icon sits on the middle of the capitals rather than of the line
		unsigned int starChar = 0;
		ImTextCharFromUtf8(&starChar, ICON_FA_STAR, nullptr);
		PushFont(font, a_size);
		const ImFontGlyph* glyph  = GetFontBaked()->FindGlyph(static_cast<ImWchar>(starChar));
		const float        inkMid = glyph && glyph->Visible ? (glyph->Y0 + glyph->Y1) * 0.5f : a_size * 0.5f;
		PopFont();

		star.pos     = ImVec2(std::round(a_row.Max.x - a_rightInset - star.width), std::round((a_row.Min.y + a_row.Max.y) * 0.5f - inkMid));
		star.hovered = IsMouseHoveringRect(ImVec2(star.pos.x - a_hitPad, a_row.Min.y), ImVec2(star.pos.x + star.width + a_hitPad, a_row.Max.y));
		return star;
	}

	void DrawFavouriteStar(ImDrawList* a_drawList, const FavouriteStar& a_star, bool a_favourite, ImFont* a_font)
	{
		const ImU32 col = a_star.hovered ? IM_COL32(255, 255, 100, 255) : (a_favourite ? IM_COL32(255, 215, 0, 255) : IM_COL32(150, 150, 150, 150));
		a_drawList->AddText(a_font ? a_font : GetFont(), a_star.size, a_star.pos, col, ICON_FA_STAR);
	}

	bool FavouriteSelectable(const char* label, bool selected, bool* a_favourite, int flags, const ImVec2& size)
	{
		if (!a_favourite)
			return SelectableStyled(label, selected, flags, size);

		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		// The star a little under the text, as the sidebar's are, with room kept for it whether it shows or not, so a name doesn't
		// change length as the row's hovered
		const float scale    = GetDynamicWidgetScale();
		const float starSize = GetFontSize() * 0.85f;
		const float starW    = GetFont()->CalcTextSizeA(starSize, FLT_MAX, 0.0f, ICON_FA_STAR).x;
		const float inset    = 10.0f * scale;
		const float rowW     = size.x > 0.0f ? size.x : GetContentRegionAvail().x + std::min(size.x, 0.0f);
		const float textW    = std::max(0.0f, rowW - starW - inset * 2.0f);

		// "###" keeps the ID off the shown text, so the row stays the same item as it's cut, as SelectableStyled has it
		const char* shownEnd = FindRenderedTextEnd(label);
		bool        pressed  = false;
		if (CalcTextSize(label, shownEnd).x > textW) {
			const char* ownId  = std::strstr(label, "###");
			std::string fitted = FitTextToWidth(label, shownEnd, textW);
			fitted += "###";
			fitted += ownId ? ownId + 3 : label;
			pressed = Selectable(fitted.c_str(), selected, flags, size);
		} else {
			pressed = Selectable(label, selected, flags, size);
		}

		const bool hovered = IsItemHovered();
		const bool focused = IsItemFocused();

		// Hit-tested by hand, as the star sits on the row's own item
		FavouriteStar star = PlaceFavouriteStar(ImRect(GetItemRectMin(), GetItemRectMax()), starSize, inset, inset * 0.5f);
		star.hovered       = star.hovered && hovered;
		const bool toggled = (pressed && star.hovered) || ((hovered || focused) && IsKeyPressed(ImGuiKey_GamepadFaceUp, false));
		if (toggled)
			*a_favourite = !*a_favourite;
		if (pressed || toggled)
			PlayAudio(Audio::kFocus);

		if (*a_favourite || hovered || focused || selected)
			DrawFavouriteStar(window->DrawList, star, *a_favourite);
		return pressed && !star.hovered;
	}
}
