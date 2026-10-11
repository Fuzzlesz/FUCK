#include "WidgetsInternal.h"

namespace ImGui
{
	using namespace WidgetsDetail;

	namespace
	{
		// Depth counters for PushGamepadTweakFastDisabled / PushGamepadTweakSlowDisabled
		int g_gamepadTweakFastDisabled = 0;
		int g_gamepadTweakSlowDisabled = 0;

		// Masks ImGui's gamepad tweak modifiers (RB = x10 "fast", LB = x0.1 "slow") for the
		// lifetime of the scope, but only while a plugin has pushed them disabled.
		// Wrap only the behaviour update of a slider/drag so nothing else sees the masked state.
		struct ScopedGamepadTweakMask
		{
			ScopedGamepadTweakMask()
			{
				if (g_gamepadTweakFastDisabled > 0)
					Mask(_fast, ImGuiKey_NavGamepadTweakFast);
				if (g_gamepadTweakSlowDisabled > 0)
					Mask(_slow, ImGuiKey_NavGamepadTweakSlow);
			}

			~ScopedGamepadTweakMask()
			{
				Restore(_fast);
				Restore(_slow);
			}

			ScopedGamepadTweakMask(const ScopedGamepadTweakMask&)            = delete;
			ScopedGamepadTweakMask& operator=(const ScopedGamepadTweakMask&) = delete;

		private:
			struct SavedKey
			{
				ImGuiKeyData* data    = nullptr;
				bool          wasDown = false;
			};

			static void Mask(SavedKey& a_saved, ImGuiKey a_key)
			{
				a_saved.data       = GetKeyData(a_key);
				a_saved.wasDown    = a_saved.data->Down;
				a_saved.data->Down = false;
			}

			static void Restore(SavedKey& a_saved)
			{
				if (a_saved.data)
					a_saved.data->Down = a_saved.wasDown;
			}

			SavedKey _fast;
			SavedKey _slow;
		};

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

		bool        active   = (g.ActiveId == id);
		const ImU32 frameCol = GetColorU32(active ? ImGuiCol_FrameBgActive : h ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
		if (UseInvertedCorners())
			AddThemedRectFilled(window->DrawList, bb, frameCol, GetFrameRounding());
		else
			RenderFrame(bb.Min, bb.Max, frameCol, true, GetFrameRounding());
		DrawWidgetBorder(window->DrawList, bb, active || h || IsWidgetFocused(id), GetFrameRounding());

		{
			ScopedGamepadTweakMask tweakMask;
			if (DragBehavior(id, type, data, speed, min, max, fmt, flags)) {
				changed = true;
			}
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
		{
			ScopedGamepadTweakMask tweakMask;
			if (SliderBehavior(bb, id, type, data, min, max, fmt, flags, &grab)) {
				changed = true;
			}
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
			float scale     = GetDynamicWidgetScale();
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

			AddThemedRectFilled(window->DrawList, customGrab, GetColorU32(active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), GetStyle().GrabRounding);
		}

		char        buf[64] = "";
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

		return changed;
	}

	bool ThinVSliderScalar(const char* label, const ImVec2& size, ImGuiDataType type, void* data, const void* min, const void* max, const char* fmt, ImGuiSliderFlags flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImRect       bb(window->DC.CursorPos, window->DC.CursorPos + size);

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
				g.ActiveIdUsingNavDirMask = (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
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
		{
			ScopedGamepadTweakMask tweakMask;
			if (SliderBehavior(bb, id, type, data, min, max, fmt, flags | ImGuiSliderFlags_Vertical, &grab)) {
				changed = true;
			}
		}
		if (changed)
			MarkItemEdited(id);

		bool active = (g.ActiveId == id);

		// Calculate exact integer center for the column to guarantee pixel alignment
		float centerX = std::floor(bb.Min.x + (bb.GetWidth() * 0.5f));

		ImRect track = bb;

		// Force track width to an even number so it splits around centerX
		float trackW = std::floor(bb.GetWidth() * 0.50f);
		if (static_cast<int>(trackW) % 2 != 0)
			trackW -= 1.0f;

		track.Min.x = centerX - (trackW * 0.5f);
		track.Max.x = track.Min.x + trackW;

		window->DrawList->AddRectFilled(track.Min, track.Max, GetColorU32(active ? ImGuiCol_FrameBgActive : h ? ImGuiCol_FrameBgHovered :
																												ImGuiCol_FrameBg),
			0.0f);
		DrawWidgetBorder(window->DrawList, track, IsWidgetFocused(id) || active || h, 0.0f);

		if (grab.Max.y > grab.Min.y) {
			float scale      = GetDynamicWidgetScale();
			float knobHeight = 10.0f * scale;

			float centerY = std::floor(grab.Min.y + (grab.Max.y - grab.Min.y) * 0.5f);
			centerY       = ImClamp(centerY, bb.Min.y + knobHeight * 0.5f, bb.Max.y - knobHeight * 0.5f);

			// Force knob width to an even number
			float knobW = std::floor(bb.GetWidth() * 0.70f);
			if (static_cast<int>(knobW) % 2 != 0)
				knobW -= 1.0f;

			float knobMinX = centerX - (knobW * 0.5f);

			ImRect customGrab(
				std::floor(knobMinX),
				std::floor(centerY - (knobHeight * 0.5f)),
				std::floor(knobMinX + knobW),
				std::floor(centerY + (knobHeight * 0.5f)));

			AddThemedRectFilled(window->DrawList, customGrab, GetColorU32(active ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab), GetStyle().GrabRounding);
		}

		char        buf[64] = "";
		const char* buf_end = buf + DataTypeFormatString(buf, 64, type, data, fmt);
		RenderTextClipped(bb.Min, bb.Max, buf, buf_end, NULL, { 0.5f, 0.5f });

		return changed;
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
		float scale     = GetDynamicWidgetScale();

		// Extract ratio (steppers always point left/right, so height is height)
		float targetH = 30.0f * scale;
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
		float fontSize  = (largeFont ? largeFont->LegacySize : GetStyle().FontSizeBase) * scale;

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

	bool DragFloat2Styled(const char* label, float v[2], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			ScopedGamepadTweakMask tweakMask;
			return DragFloat2(id, v, speed, min, max, fmt);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
		return res;
	}

	bool DragFloat3Styled(const char* label, float v[3], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			ScopedGamepadTweakMask tweakMask;
			return DragFloat3(id, v, speed, min, max, fmt);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
		return res;
	}

	bool DragFloat4Styled(const char* label, float v[4], float speed, float min, float max, const char* fmt)
	{
		bool res = OutsetFramedWidget(label, [&](const char* id) {
			ScopedGamepadTweakMask tweakMask;
			return DragFloat4(id, v, speed, min, max, fmt);
		});
		if (res)
			PlayAudio(Audio::kPrevNext);
		return res;
	}

	bool SliderAngleStyled(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format)
	{
		if (!format)
			format = "%.0f deg";
		float v_deg         = (*v_rad) * 360.0f / (2.0f * IM_PI);
		bool  value_changed = Slider(label, &v_deg, v_degrees_min, v_degrees_max, format, ImGuiSliderFlags_AlwaysClamp);
		if (value_changed) {
			*v_rad = v_deg * (2.0f * IM_PI) / 360.0f;
		}
		return value_changed;
	}

	bool VSliderFloatStyled(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float scale = GetDynamicWidgetScale();

		PushStyleColor(ImGuiCol_FrameBg,        GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgActive,  GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));

		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_GrabMinSize, std::max(10.0f * scale, GetStyle().GrabMinSize));

		bool changed = ThinVSliderScalar(label, size, ImGuiDataType_Float, v, &v_min, &v_max, format, ImGuiSliderFlags_AlwaysClamp);

		PopStyleVar(2);
		PopStyleColor(3);

		if (changed)
			PlayAudio(Audio::kPrevNext);

		return changed;
	}

	bool VSliderButtonStyled(const char* str_id, const ImVec2& slider_size, float* v, float v_min, float v_max, const char* format, const char* top_label, const char* bottom_label, bool* out_top_pressed, bool* out_bottom_pressed)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float scale = GetDynamicWidgetScale();
		float padX  = 8.0f * scale;  // Matches OutlineButton's internal padX
		float padY  = 7.0f * scale;  // Matches OutlineButton's internal padY

		// Text after "##" isn't drawn, so isn't measured
		auto buttonWidth = [&](const char* a_label) {
			return a_label ? CalcTextSize(a_label, nullptr, true).x + (padX * 2.0f) : 0.0f;
		};

		float topWidth  = buttonWidth(top_label);
		float botWidth  = buttonWidth(bottom_label);
		float btnHeight = std::max(GetFrameHeight(), GetFontSize() + (padY * 2.0f));

		// The column is as wide as its largest element
		float colWidth = std::max({ slider_size.x, topWidth, botWidth });

		BeginGroup();
		PushID(str_id);

		bool slider_changed = false;
		bool top_pressed    = false;
		bool bot_pressed    = false;

		auto DrawSlider = [&]() {
			float offX = (colWidth - slider_size.x) * 0.5f;
			SetCursorPosX(GetCursorPosX() + offX);
			slider_changed = VSliderFloatStyled("##vslider", slider_size, v, v_min, v_max, format);
		};

		// A button without a label still keeps its space, so sliders of the same size line up in a row
		auto DrawBtn = [&](const char* a_label, float a_width, const char* a_id) {
			if (!a_label) {
				Dummy(ImVec2(colWidth, btnHeight));
				return false;
			}

			SetCursorPosX(GetCursorPosX() + (colWidth - a_width) * 0.5f);

			// Prevent ID collisions when both buttons share a label
			PushID(a_id);
			bool pressed = OutlineButton(a_label);
			PopID();

			return pressed;
		};

		top_pressed = DrawBtn(top_label, topWidth, "top_btn");
		Spacing();

		DrawSlider();

		Spacing();
		bot_pressed = DrawBtn(bottom_label, botWidth, "bot_btn");

		PopID();
		EndGroup();

		if (out_top_pressed)
			*out_top_pressed = top_pressed;
		if (out_bottom_pressed)
			*out_bottom_pressed = bot_pressed;

		return slider_changed;
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

	void PushGamepadTweakFastDisabled() { ++g_gamepadTweakFastDisabled; }

	void PopGamepadTweakFastDisabled()
	{
		IM_ASSERT(g_gamepadTweakFastDisabled > 0 && "PopGamepadTweakFastDisabled() called without a matching push");
		if (g_gamepadTweakFastDisabled > 0)
			--g_gamepadTweakFastDisabled;
	}

	void PushGamepadTweakSlowDisabled() { ++g_gamepadTweakSlowDisabled; }

	void PopGamepadTweakSlowDisabled()
	{
		IM_ASSERT(g_gamepadTweakSlowDisabled > 0 && "PopGamepadTweakSlowDisabled() called without a matching push");
		if (g_gamepadTweakSlowDisabled > 0)
			--g_gamepadTweakSlowDisabled;
	}
}
