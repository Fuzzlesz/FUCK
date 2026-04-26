#pragma once

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
		float offsetY;    // vertical centering offset within rowH
	};

	inline ArrowIconParams CalcArrowIconParams(float iconAspect, bool pointsDown, float rowH,
		float baseSize = 16.0f, float userScale = 1.0f)
	{
		float scaledH = baseSize * userScale;
		float scaledW = scaledH * iconAspect;

		ArrowIconParams p;
		// When pointing down/up the texture is rotated 90°, so physical W and H
		// are swapped to keep the icon from stretching.
		p.drawSize = pointsDown ? ImVec2(scaledH, scaledW) : ImVec2(scaledW, scaledH);
		p.offsetY = (rowH - p.drawSize.y) * 0.5f;
		return p;
	}

	// Cache Management
	void ClearFormCaches();

	// Combo Boxes
	bool ComboStyled(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height_in_items = -1);
	bool ComboWithFilter(const char* label, int* current_item, const std::vector<std::string>& items, int popup_max_height_in_items = -1);
	bool ComboForm(const char* label, RE::FormID* currentFormID, RE::FormType formType);

	// Core Widgets
	bool SelectableStyled(const char* label, bool selected = false, int flags = 0, const ImVec2& size = ImVec2(0, 0));
	bool CheckBox(const char* label, bool* a_toggle, bool alignFar = true, bool labelLeft = true);
	bool ToggleButton(const char* label, bool* v, bool alignFar = true, bool labelLeft = true);
	bool ButtonIconWithLabelStyled(const char* label, void* tex, const ImVec2& size, bool alignFar = true, bool labelLeft = true);
	bool Hotkey(const char* label, std::uint32_t key, std::int32_t modifier, std::int32_t modifier2, bool alignFar = true, bool labelLeft = true, bool flashing = false);

	// Specialized Inputs
	bool InputTextStyled(const char* label, char* buf, size_t buf_size, int flags = 0);
	bool ColorEdit3Styled(const char* label, float col[3], int flags = 0);
	bool ColorEdit4Styled(const char* label, float col[4], int flags = 0);
	void Stepper(const char* label, const char* text, bool* outLeft, bool* outRight);

	bool DragFloat2Styled(const char* label, float v[2], float speed, float min, float max, const char* fmt);
	bool DragFloat3Styled(const char* label, float v[3], float speed, float min, float max, const char* fmt);
	bool DragFloat4Styled(const char* label, float v[4], float speed, float min, float max, const char* fmt);

	// Navigation
	bool BeginTabItemEx(const char* label, ImGuiTabItemFlags flags = 0);
	bool OutlineButton(const char* label, bool* wasFocused = nullptr);
	bool CollapsingHeaderIcon(const char* label, int flags = 0);
	bool TreeNodeIcon(const char* label, int flags = 0);

	// Helpers
	std::tuple<bool, bool, bool> CenteredTextWithArrows(const char* label, std::string_view centerText);
	void DrawWidgetBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered, float rounding = 0.0f);
	void DrawTabBorder(ImDrawList* drawList, const ImRect& bb, bool isActiveOrHovered);
	inline void LeftLabel(const char* label) { LeftAlignedTextImpl(label); }

	bool DragScalarEx(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags);
	template <class T>
	bool DragOnHover(const char* label, T* v, float v_speed = 1.0f, T v_min = 0, T v_max = 100, const char* format = nullptr, ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp)
	{
		const auto newLabel = LeftAlignedText(label);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));

		bool result = DragScalarEx(newLabel.c_str(), detail::GetDataType<T>(), v, v_speed, &v_min, &v_max, format ? format : detail::GetDefaultFormat<T>(), flags);
		if (result)
			RE::PlaySound("UIMenuPrevNext");

		ActivateOnHover();
		PopStyleColor(3);
		return result;
	}

	bool ThinSliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags, float sliderThickness);
	template <class T>
	bool Slider(const char* label, T* v, T v_min, T v_max, const char* format = nullptr, ImGuiSliderFlags flags = ImGuiSliderFlags_AlwaysClamp)
	{
		const auto newLabel = LeftAlignedText(label);
		PushStyleColor(ImGuiCol_FrameBg, GetUserStyleColorU32(USER_STYLE::kFrameBG_Widget));
		PushStyleColor(ImGuiCol_FrameBgHovered, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));
		PushStyleColor(ImGuiCol_FrameBgActive, GetUserStyleColorU32(USER_STYLE::kFrameBG_WidgetActive));

		bool result = ThinSliderScalar(newLabel.c_str(), detail::GetDataType<T>(), v, &v_min, &v_max, format ? format : detail::GetDefaultFormat<T>(), flags, 0.5f);
		if (result)
			RE::PlaySound("UIMenuPrevNext");

		ActivateOnHover();
		PopStyleColor(3);
		return result;
	}
}
