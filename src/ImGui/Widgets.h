#pragma once

#include "FUCK-Man.h"

#include "Audio.h"
#include "Renderer.h"
#include "Styles.h"
#include "Util.h"

namespace IconFont
{
	struct IconTexture;
}

namespace ImGui
{
	// --- Safe Type Mappers for Templates ---
	namespace detail
	{
		template <typename T>
		constexpr ImGuiDataType GetDataType()
		{
			if constexpr (std::is_same_v<T, float>)
				return ImGuiDataType_Float;
			else if constexpr (std::is_same_v<T, double>)
				return ImGuiDataType_Double;
			else if constexpr (std::is_same_v<T, int8_t>)
				return ImGuiDataType_S8;
			else if constexpr (std::is_same_v<T, uint8_t>)
				return ImGuiDataType_U8;
			else if constexpr (std::is_same_v<T, int16_t>)
				return ImGuiDataType_S16;
			else if constexpr (std::is_same_v<T, uint16_t>)
				return ImGuiDataType_U16;
			else if constexpr (std::is_same_v<T, int32_t>)
				return ImGuiDataType_S32;
			else if constexpr (std::is_same_v<T, uint32_t>)
				return ImGuiDataType_U32;
			else if constexpr (std::is_same_v<T, int64_t>)
				return ImGuiDataType_S64;
			else if constexpr (std::is_same_v<T, uint64_t>)
				return ImGuiDataType_U64;
			else
				return ImGuiDataType_S32;
		}

		template <typename T>
		constexpr const char* GetDefaultFormat()
		{
			if constexpr (std::is_floating_point_v<T>)
				return "%.2f";
			else if constexpr (std::is_signed_v<T>)
				return "%d";
			else
				return "%u";
		}
	}

	enum class IconDirection
	{
		kRight,  // >
		kDown,   // v
		kLeft,   // <
		kUp      // ^
	};

	void DrawArrowIcon(ImDrawList* drawList, ImVec2 pos, ImVec2 size, ImU32 color, IconDirection direction);

	struct ArrowIconParams
	{
		ImVec2 drawSize;  // final draw dimensions, orientation-swapped
		float  offsetY;   // vertical centering offset within rowH
	};

	inline ArrowIconParams CalcArrowIconParams(float iconAspect, bool pointsDown, float rowH,
		float baseSize = 30.0f, float userScale = 1.0f)
	{
		float scaledH = baseSize * userScale;

		// Prevent the icon from vertically overflowing the row it's placed in
		float maxH = std::max(1.0f, rowH - 4.0f);
		if (scaledH > maxH) {
			scaledH = maxH;
		}

		float scaledW = scaledH * iconAspect;

		ArrowIconParams p;
		// When pointing down/up the texture is rotated 90°, so physical W and H
		// are swapped to keep the icon from stretching.
		p.drawSize = pointsDown ? ImVec2(scaledH, scaledW) : ImVec2(scaledW, scaledH);
		p.offsetY  = (rowH - p.drawSize.y) * 0.5f;
		return p;
	}

	// Cache Management
	void ClearFormCaches();

	// Themed Corners: whether the theme turns rounded corners inwards ("Invert Corners"),
	// and AddRectFilled / AddRect that follow it. Exactly the ImDrawList versions while it's off
	bool UseInvertedCorners();
	void AddThemedRectFilled(ImDrawList* a_drawList, const ImRect& a_bb, ImU32 a_col, float a_rounding, ImDrawFlags a_flags = ImDrawFlags_None);
	void AddThemedRect(ImDrawList* a_drawList, const ImRect& a_bb, ImU32 a_col, float a_rounding, float a_thickness = 1.0f);

	// ImGui only rounds a window's corners outwards. With inverted corners, call PushThemedWindowBg() just before Begin()
	// (or BeginChild(), with a_child) and PopThemedWindowBg() straight after it, whatever it returned. ImGui then leaves
	// the background, border and resize grip out, and they're drawn inverted instead. Returns false when there's nothing to invert
	bool PushThemedWindowBg(bool a_child = false);
	void PopThemedWindowBg(bool a_pushed);

	// BeginChild() whose corners follow the theme. Pair with EndChild() as usual
	bool BeginChildThemed(const char* a_id, const ImVec2& a_size = ImVec2(0, 0), ImGuiChildFlags a_childFlags = ImGuiChildFlags_None, ImGuiWindowFlags a_windowFlags = ImGuiWindowFlags_None);

	// How far inverted corners cut into the current window's edges, so things along them can move in clear. 0 otherwise
	float GetWindowCornerInset();

	// Fills a_rect, leaving out any part of it in the current window's cut-away corners. A plain fill otherwise
	void AddRectFilledInWindow(ImDrawList* a_drawList, const ImRect& a_rect, ImU32 a_col);

	// Combo Boxes
	bool ComboStyled(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height_in_items = -1);
	bool ComboWithFilter(const char* label, int* current_item, std::span<const std::string> items, int popup_max_height_in_items = -1, const Set<std::string>* a_favourites = nullptr, std::string* a_favToggled = nullptr);
	bool ComboForm(const char* label, RE::FormID* currentFormID, RE::FormType formType);
	bool ComboFormStr(const char* label, std::string* currentEdid, RE::FormType formType);

	// Sticky List
	void BeginStickyList(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = true);
	bool StickyHeader(const char* label, bool* open = nullptr, int count = -1, int flags = 0);  // Draw the rows when true
	void EndStickyList();

	// Core Widgets
	bool SelectableStyled(const char* label, bool selected = false, int flags = 0, const ImVec2& size = ImVec2(0, 0));
	bool CheckBox(const char* label, bool* a_toggle, bool alignFar = true, bool labelLeft = true);
	bool ToggleButton(const char* label, bool* v, bool alignFar = true, bool labelLeft = true);
	bool ButtonIconWithLabelStyled(const char* label, void* tex, const ImVec2& size, bool alignFar = true, bool labelLeft = true);
	bool Hotkey(const char* label, std::uint32_t key, std::int32_t modifier, std::int32_t modifier2, bool alignFar = true, bool labelLeft = true, bool flashing = false, bool alwaysHighlight = false, float iconScale = 1.0f);
	bool DrawManagedHotkey(const char* label, FUCK::ManagedHotkey& h, FUCK::HotkeyFlags flags, float iconScale = 1.0f, float labelScale = 1.0f);

	// Specialized Inputs
	bool InputTextStyled(const char* label, char* buf, size_t buf_size, int flags = 0);
	bool ColorEdit3Styled(const char* label, float col[3], int flags = 0);
	bool ColorEdit4Styled(const char* label, float col[4], int flags = 0);
	bool ColorButtonStyled(const char* desc_id, const ImVec4& col, int flags = 0, const ImVec2& size = ImVec2(0, 0), bool selected = false);
	bool ColorPicker3Styled(const char* label, float col[3], int flags = 0);
	bool ColorPicker4Styled(const char* label, float col[4], int flags = 0, const float* ref_col = nullptr);
	void Stepper(const char* label, const char* text, bool* outLeft, bool* outRight);

	bool DragFloat2Styled(const char* label, float v[2], float speed, float min, float max, const char* fmt);
	bool DragFloat3Styled(const char* label, float v[3], float speed, float min, float max, const char* fmt);
	bool DragFloat4Styled(const char* label, float v[4], float speed, float min, float max, const char* fmt);
	bool SliderAngleStyled(const char* label, float* v_rad, float v_degrees_min = -360.0f, float v_degrees_max = +360.0f, const char* format = "%.0f deg");
	bool VSliderFloatStyled(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format);
	bool VSliderButtonStyled(const char* str_id, const ImVec2& slider_size, float* v, float v_min, float v_max, const char* format, const char* top_label, const char* bottom_label, bool* out_top_pressed, bool* out_bottom_pressed);

	// Gamepad Tweak Modifiers (RB = fast, LB = slow) for sliders and drags
	void PushGamepadTweakFastDisabled();
	void PopGamepadTweakFastDisabled();
	void PushGamepadTweakSlowDisabled();
	void PopGamepadTweakSlowDisabled();

	// Navigation
	bool BeginTabItemEx(const char* label, ImGuiTabItemFlags flags = 0);
	bool OutlineButton(const char* label, bool* wasFocused = nullptr);
	bool CollapsingHeaderIcon(const char* label, int flags = 0);
	bool TreeNodeIcon(const char* label, int flags = 0);

	// Helpers
	std::tuple<bool, bool, bool> CenteredTextWithArrows(const char* label, std::string_view centerText);
	void                         DrawWidgetBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding);
	void                         DrawTabBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding);
	inline void                  LeftLabel(const char* label) { LeftAlignedTextImpl(label); }

	bool DragScalarEx(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);

	template <class T>
	bool DragOnHover(const char* label, T* v, float v_speed = 1.0f, T v_min = 0, T v_max = 100, const char* format = nullptr, ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp)
	{
		float scale      = ImGui::GetFontSize() / 30.0f;
		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float padX       = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float padY       = 7.0f * scale;

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));

		const auto newLabel = LeftAlignedText(label);

		// Force black background to match Combos
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Suppress Native Borders

		bool result = DragScalarEx(newLabel.c_str(), detail::GetDataType<T>(), v, v_speed, &v_min, &v_max, format ? format : detail::GetDefaultFormat<T>(), flags);
		if (result)
			PlayAudio(Audio::kPrevNext);

		ActivateOnHover();
		PopStyleVar(2);
		PopStyleColor(3);
		return result;
	}

	bool ThinSliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);

	template <class T>
	bool Slider(const char* label, T* v, T v_min, T v_max, const char* format = nullptr, ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp)
	{
		float scale      = ImGui::GetFontSize() / 30.0f;
		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float padX       = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float padY       = 7.0f * scale;

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));

		const auto newLabel = LeftAlignedText(label);

		// Force black background to match Combos
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Suppress Native Borders

		bool result = ThinSliderScalar(newLabel.c_str(), detail::GetDataType<T>(), v, &v_min, &v_max, format ? format : detail::GetDefaultFormat<T>(), flags);
		if (result)
			PlayAudio(Audio::kPrevNext);

		ActivateOnHover();
		PopStyleVar(2);
		PopStyleColor(3);
		return result;
	}
}
