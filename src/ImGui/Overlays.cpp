#include "Overlays.h"
#include "Styles.h"

namespace ImGui
{
	// ------------------------------------------------------------
	// Entry point
	// ------------------------------------------------------------
	void Overlays::Draw(FUCK_Overlay type, float thickness, ImU32 color, float paramA, float paramB, float paramC, float paramD)
	{
		auto* drawList = ImGui::GetBackgroundDrawList();
		ImVec2 displaySize = ImGui::GetIO().DisplaySize;

		if (color == 0) {
			color = ImGui::GetUserStyleColorU32(USER_STYLE::kGridLines);
		}
		if (thickness <= 0.0f) {
			thickness = ImGui::GetUserStyleVar(USER_STYLE::kGridLines);
		}

		switch (type) {
		case FUCK_Overlay::kGrid:
			DrawGrid(drawList, displaySize, color, thickness, paramA, paramB, paramC);
			break;
		case FUCK_Overlay::kCrosshair:
			DrawCrosshair(drawList, displaySize, color, thickness, paramA, paramB);
			break;
		case FUCK_Overlay::kGoldenSpiral:
			{
				bool showSquares = false;
				float anchor = paramA;
				if (anchor >= 10.0f) {
					anchor -= 10.0f;
					showSquares = true;
				}
				DrawGoldenSpiral(drawList, displaySize, color, thickness, anchor, paramB, paramC, paramD, showSquares);
				break;
			}
		case FUCK_Overlay::kGoldenRatio:
			DrawGoldenRatioGrid(drawList, displaySize, color, thickness, paramA);
			break;
		case FUCK_Overlay::kTriangle:
			DrawTriangle(drawList, displaySize, color, thickness, paramA > 0.0f);
			break;
		}
	}

	// ------------------------------------------------------------
	// Grid
	// ------------------------------------------------------------
	void Overlays::DrawGrid(ImDrawList * dl, const ImVec2& size, ImU32 col, float thick, float rows, float cols, float rotationDeg)
	{
		float r = (rows > 0.0f) ? rows : 3.0f;
		float c = (cols > 0.0f) ? cols : 3.0f;

		float stepX = size.x / c;
		float stepY = size.y / r;

		ImVec2 center = { size.x * 0.5f, size.y * 0.5f };
		float diag = sqrtf(size.x * size.x + size.y * size.y);
		float radius = diag * 0.5f;

		float rad = rotationDeg * (IM_PI / 180.0f);
		float sinA = sinf(rad);
		float cosA = cosf(rad);

		auto transform = [&](float x, float y) {
			return ImVec2{
				x * cosA - y * sinA + center.x,
				x * sinA + y * cosA + center.y
			};
		};

		int linesX = (int)ceilf(radius / stepX);
		for (int i = -linesX - 1; i <= linesX + 1; ++i) {

			float x = (float)i * stepX;

			ImVec2 p1 = transform(x, -radius);
			ImVec2 p2 = transform(x, radius);
			dl->AddLine(p1, p2, col, thick);
		}

		int linesY = (int)ceilf(radius / stepY);
		for (int i = -linesY - 1; i <= linesY + 1; ++i) {
			float y = (float)i * stepY;

			ImVec2 p1 = transform(-radius, y);
			ImVec2 p2 = transform(radius, y);
			dl->AddLine(p1, p2, col, thick);
		}
	}

	// ------------------------------------------------------------
	// Crosshair
	// ------------------------------------------------------------
	void Overlays::DrawCrosshair(ImDrawList* dl, const ImVec2& size, ImU32 col, float thick, float rows, float cols)
	{
		float countX = std::max(cols, 1.0f);
		float countY = std::max(rows, 1.0f);

		float stepX = size.x / (countX + 1.0f);
		float stepY = size.y / (countY + 1.0f);

		float crossSize = 10.0f * std::max(1.0f, thick * 0.5f);

		for (int i = 1; i <= (int)countX; ++i) {
			for (int j = 1; j <= (int)countY; ++j) {
				float cx = i * stepX;
				float cy = j * stepY;
				dl->AddLine({ cx - crossSize, cy }, { cx + crossSize, cy }, col, thick);
				dl->AddLine({ cx, cy - crossSize }, { cx, cy + crossSize }, col, thick);
			}
		}
	}

	// ------------------------------------------------------------
	// Golden Spiral
	// ------------------------------------------------------------
	void Overlays::DrawGoldenSpiral(ImDrawList * dl, const ImVec2& size, ImU32 col, float thick, float anchor, float turns, float rot, float scale, bool showSquares)
	{
		const float phi = 1.61803398875f;
		const float b = logf(phi) / (IM_PI * 0.5f);

		int orientation = static_cast<int>(anchor) % 5;
		float nTurns = (turns > 0.1f) ? turns : 6.0f;
		float userRot = rot * (IM_PI / 180.0f);
		float userScale = (scale > 0.01f) ? scale : 1.0f;

		float invPhi = 1.0f / phi;
		float invPhi2 = 1.0f - invPhi;

		ImVec2 origin;
		float baseRot = 0.0f;

		switch (orientation) {
		default:
		case 0:  // BR Focus
			origin = { size.x * invPhi, size.y * invPhi };
			baseRot = 0.0f;
			break;
		case 1:  // BL Focus
			origin = { size.x * invPhi2, size.y * invPhi };
			baseRot = IM_PI * 0.5f;
			break;
		case 2:  // TL Focus
			origin = { size.x * invPhi2, size.y * invPhi2 };
			baseRot = IM_PI;
			break;
		case 3:  // TR Focus
			origin = { size.x * invPhi, size.y * invPhi2 };
			baseRot = IM_PI * 1.5f;
			break;
		case 4:  // Center
			origin = { size.x * 0.5f, size.y * 0.5f };
			baseRot = 0.0f;
			break;
		}

		float d1 = powf(0 - origin.x, 2) + powf(0 - origin.y, 2);
		float d2 = powf(size.x - origin.x, 2) + powf(0 - origin.y, 2);
		float d3 = powf(0 - origin.x, 2) + powf(size.y - origin.y, 2);
		float d4 = powf(size.x - origin.x, 2) + powf(size.y - origin.y, 2);
		float targetRadius = sqrtf(std::max({ d1, d2, d3, d4 }));

		float maxTheta = nTurns * IM_PI * 2.0f;
		float a = (targetRadius * userScale) / expf(b * maxTheta);

		dl->PathClear();

		int segments = static_cast<int>(nTurns * 64);
		float step = maxTheta / segments;

		for (int i = 0; i <= segments; ++i) {
			float theta = i * step;
			float r = a * expf(b * theta);
			float ang = theta + baseRot + userRot;

			dl->PathLineTo({ origin.x + r * cosf(ang),
				origin.y + r * sinf(ang) });
		}
		dl->PathStroke(col, false, thick);

		if (showSquares) {
			float theta = maxTheta;
			int count = static_cast<int>(nTurns * 4);

			for (int i = 0; i < count; ++i) {
				float r0 = a * expf(b * theta);
				float r1 = a * expf(b * (theta - IM_PI * 0.5f));
				float ang = theta + baseRot + userRot;

				ImVec2 p0 = {
					origin.x + r0 * cosf(ang),
					origin.y + r0 * sinf(ang)
				};

				ImVec2 p1 = {
					origin.x + r1 * cosf(ang - IM_PI * 0.5f),
					origin.y + r1 * sinf(ang - IM_PI * 0.5f)
				};

				dl->AddLine(p0, p1, col, thick * 0.5f);
				dl->AddLine(p1, origin, col, thick * 0.2f);

				theta -= IM_PI * 0.5f;
			}
		}
	}

	// ------------------------------------------------------------
	// Golden Ratio Grid
	// ------------------------------------------------------------
	void Overlays::DrawGoldenRatioGrid(ImDrawList * dl, const ImVec2& size, ImU32 col, float thick, float subdivisions)
	{
		const float invPhi = 1.0f / 1.61803398875f;
		const float invPhi2 = 1.0f - invPhi;

		auto drawPhiRect = [&](ImVec2 min, ImVec2 max) {
			float w = max.x - min.x;
			float h = max.y - min.y;

			float x1 = min.x + w * invPhi2;
			float x2 = min.x + w * invPhi;
			float y1 = min.y + h * invPhi2;
			float y2 = min.y + h * invPhi;

			dl->AddLine({ x1, min.y }, { x1, max.y }, col, thick);
			dl->AddLine({ x2, min.y }, { x2, max.y }, col, thick);
			dl->AddLine({ min.x, y1 }, { max.x, y1 }, col, thick);
			dl->AddLine({ min.x, y2 }, { max.x, y2 }, col, thick);

			return ImVec4(x1, y1, x2, y2);
		};

		ImVec4 center = drawPhiRect({ 0, 0 }, size);

		int levels = static_cast<int>(subdivisions);
		if (levels > 4)
			levels = 4;

		for (int i = 0; i < levels; ++i) {
			center = drawPhiRect({ center.x, center.y }, { center.z, center.w });
		}
	}

	// ------------------------------------------------------------
	// Triangle Composition
	// ------------------------------------------------------------
	void Overlays::DrawTriangle(ImDrawList * dl, const ImVec2& size, ImU32 col, float thick, bool mirror)
	{
		ImVec2 tl = { 0, 0 };
		ImVec2 tr = { size.x, 0 };
		ImVec2 bl = { 0, size.y };
		ImVec2 br = { size.x, size.y };

		if (!mirror) {
			dl->AddLine(tl, br, col, thick);									// Main Diagonal
			dl->AddLine(bl, { size.x * 0.382f, size.y * 0.382f }, col, thick);	// Perpendicular 1
			dl->AddLine(tr, { size.x * 0.618f, size.y * 0.618f }, col, thick);	// Perpendicular 2
		} else {
			dl->AddLine(bl, tr, col, thick);
			dl->AddLine(tl, { size.x * 0.382f, size.y * 0.618f }, col, thick);
			dl->AddLine(br, { size.x * 0.618f, size.y * 0.382f }, col, thick);
		}
	}
}
