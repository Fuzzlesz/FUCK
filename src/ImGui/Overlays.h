#pragma once
#include "FUCK_API.h"

namespace ImGui
{
	class Overlays
	{
	public:
		static void Draw(FUCK_Overlay type, float thickness, ImU32 color, float paramA, float paramB, float paramC, float paramD);

	private:
		static void DrawGrid(ImDrawList* dl, const ImVec2& size, ImU32 col, float thick, float rows, float cols, float rotationDeg);
		static void DrawCrosshair(ImDrawList* dl, const ImVec2& size, ImU32 col, float thick, float rows, float cols);
		static void DrawGoldenSpiral(ImDrawList* dl, const ImVec2& size, ImU32 col, float thick, float anchor, float turns, float rot, float scale, bool showSquares);
		static void DrawGoldenRatioGrid(ImDrawList* dl, const ImVec2& size, ImU32 col, float thick, float subdivisions);
		static void DrawTriangle(ImDrawList* dl, const ImVec2& size, ImU32 col, float thick, bool mirror);
	};
}
