#include "WidgetsInternal.h"

namespace ImGui::WidgetsDetail
{
	// Round rather than floor, so scaled-down borders don't lose up to a whole pixel.
	// The black outline is three quarters of the coloured border, matching vanilla's buttons
	void GetBorderThickness(float a_borderSize, float& a_colour, float& a_black)
	{
		constexpr float BLACK_RATIO = 0.75f;

		a_colour = std::max(1.0f, std::round(a_borderSize));
		a_black  = std::max(1.0f, std::round(a_colour * BLACK_RATIO));
	}

	// A border at least as thick as its rounding leaves the inside corner square, so keep rounding to
	// twice the border, giving an inner curve about as wide as the border. Zero stays square
	float KeepInnerCurve(float a_rounding)
	{
		if (a_rounding <= 0.0f)
			return a_rounding;

		float tColor, tBlack;
		GetBorderThickness(GetUserStyleVar(USER_STYLE::kButtonBorderSize), tColor, tBlack);
		return std::max(a_rounding, tColor * 2.0f);
	}

	// Scaled the same way as button rounding, so frames meant to match buttons curve alike at any scale
	float GetFrameRounding()
	{
		return KeepInnerCurve(GetUserStyleVar(USER_STYLE::kFrameRounding));
	}
}

namespace ImGui
{
	using namespace WidgetsDetail;

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

		bool inverted = UseInvertedCorners();
		if (inverted)
			rounding = CapInvertedRounding(bb, rounding);

		float tColor, tBlack;
		GetBorderThickness(borderSize, tColor, tBlack);

		ImU32 col = isActiveOrHovered ?
		                GetUserStyleColorU32(USER_STYLE::kWidgetBorderActive) :
		                GetUserStyleColorU32(USER_STYLE::kWidgetBorder);

		// Floor the bounding box to ensure pixel-perfect strokes
		ImVec2 min = { std::floor(bb.Min.x), std::floor(bb.Min.y) };
		ImVec2 max = { std::floor(bb.Max.x), std::floor(bb.Max.y) };

		// Outer Black: Expands strictly outwards
		float halfB = tBlack * 0.5f;
		// Inner Colour: Shrinks strictly inwards
		float halfC = tColor * 0.5f;

		if (inverted && rounding > 0.0f) {
			ImRect outerBB(min - ImVec2(halfB, halfB), max + ImVec2(halfB, halfB));
			AddInvertedRect(drawList, outerBB, IM_COL32(0, 0, 0, 255), rounding, tBlack);

			ImRect innerBB(min + ImVec2(halfC, halfC), max - ImVec2(halfC, halfC));
			AddInvertedRect(drawList, innerBB, col, rounding, tColor);
		} else {
			drawList->AddRect(min - ImVec2(halfB, halfB), max + ImVec2(halfB, halfB), IM_COL32(0, 0, 0, 255), rounding + halfB, 0, tBlack);
			drawList->AddRect(min + ImVec2(halfC, halfC), max - ImVec2(halfC, halfC), col, std::max(0.0f, rounding - halfC), 0, tColor);
		}
	}
}
