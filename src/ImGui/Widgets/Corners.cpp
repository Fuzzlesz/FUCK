#include "WidgetsInternal.h"

namespace ImGui::WidgetsDetail
{
	void PathInvertedCorner(ImDrawList* dl, ImVec2 center, float radius, float a_min, float a_max, int num_segments)
	{
		if (radius <= 0.0f) {
			dl->PathLineTo(center);
			return;
		}
		for (int i = 0; i <= num_segments; i++) {
			float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
			dl->PathLineTo(ImVec2(center.x + cosf(a) * radius, center.y + sinf(a) * radius));
		}
	}

	void AddInvertedRect(ImDrawList* dl, const ImRect& bb, ImU32 col, float rounding, float thickness)
	{
		if (rounding <= 0.0f) {
			dl->AddRect(bb.Min, bb.Max, col, 0.0f, 0, thickness);
			return;
		}
		dl->PathClear();
		PathInvertedCorner(dl, { bb.Min.x, bb.Min.y }, rounding, IM_PI / 2, 0.0f);
		PathInvertedCorner(dl, { bb.Max.x, bb.Min.y }, rounding, IM_PI, IM_PI / 2);
		PathInvertedCorner(dl, { bb.Max.x, bb.Max.y }, rounding, 3.0f * IM_PI / 2, IM_PI);
		PathInvertedCorner(dl, { bb.Min.x, bb.Max.y }, rounding, 0.0f, -IM_PI / 2);
		dl->PathStroke(col, ImDrawFlags_Closed, thickness);
	}

	// Inverted corners from opposite sides cross past half the shorter side, folding the shape over itself
	float CapInvertedRounding(const ImRect& a_bb, float a_rounding)
	{
		return std::min(a_rounding, std::min(a_bb.GetWidth(), a_bb.GetHeight()) * 0.5f);
	}

	// How far a cut-away corner reaches in from a frame's side, a_depth in from its top or bottom edge. 0 when corners aren't inverted
	float InvertedCornerReach(const ImRect& a_bb, float a_rounding, float a_depth)
	{
		if (!UseInvertedCorners())
			return 0.0f;

		const float r = CapInvertedRounding(a_bb, a_rounding);
		return a_depth < r ? std::sqrt(r * r - a_depth * a_depth) : 0.0f;
	}
}

namespace ImGui
{
	using namespace WidgetsDetail;

	namespace
	{
		// Corner flags work as they do for AddRectFilled, so corners left out stay square
		void AddInvertedRectFilled(ImDrawList* dl, const ImRect& bb, ImU32 col, float rounding, ImDrawFlags a_flags = ImDrawFlags_None)
		{
			if ((a_flags & ImDrawFlags_RoundCornersMask_) == 0)
				a_flags |= ImDrawFlags_RoundCornersAll;

			if (rounding <= 0.0f || (a_flags & ImDrawFlags_RoundCornersMask_) == ImDrawFlags_RoundCornersNone) {
				dl->AddRectFilled(bb.Min, bb.Max, col, 0.0f);
				return;
			}
			auto prev_flags = dl->Flags;
			dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;

			auto corner = [&](ImVec2 a_point, ImDrawFlags a_corner, float a_min, float a_max) {
				if (a_flags & a_corner)
					PathInvertedCorner(dl, a_point, rounding, a_min, a_max);
				else
					dl->PathLineTo(a_point);
			};

			dl->PathClear();
			dl->PathLineTo(ImVec2((bb.Min.x + bb.Max.x) * 0.5f, (bb.Min.y + bb.Max.y) * 0.5f));
			corner({ bb.Min.x, bb.Min.y }, ImDrawFlags_RoundCornersTopLeft, IM_PI / 2, 0.0f);
			corner({ bb.Max.x, bb.Min.y }, ImDrawFlags_RoundCornersTopRight, IM_PI, IM_PI / 2);
			corner({ bb.Max.x, bb.Max.y }, ImDrawFlags_RoundCornersBottomRight, 3.0f * IM_PI / 2, IM_PI);
			corner({ bb.Min.x, bb.Max.y }, ImDrawFlags_RoundCornersBottomLeft, 0.0f, -IM_PI / 2);

			// Close the fan on the first edge point. Copied first, as adding to the path can move it
			const ImVec2 first = dl->_Path[1];
			dl->PathLineTo(first);

			dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size, col);
			dl->PathClear();

			dl->Flags = prev_flags;
		}
	}

	bool UseInvertedCorners()
	{
		return Styles::GetSingleton()->user.invertCorners;
	}

	void AddThemedRectFilled(ImDrawList* a_drawList, const ImRect& a_bb, ImU32 a_col, float a_rounding, ImDrawFlags a_flags)
	{
		if (UseInvertedCorners() && a_rounding > 0.0f)
			AddInvertedRectFilled(a_drawList, a_bb, a_col, CapInvertedRounding(a_bb, a_rounding), a_flags);
		else
			a_drawList->AddRectFilled(a_bb.Min, a_bb.Max, a_col, a_rounding, a_flags);
	}

	void AddThemedRect(ImDrawList* a_drawList, const ImRect& a_bb, ImU32 a_col, float a_rounding, float a_thickness)
	{
		if (UseInvertedCorners() && a_rounding > 0.0f)
			AddInvertedRect(a_drawList, a_bb, a_col, CapInvertedRounding(a_bb, a_rounding), a_thickness);
		else
			a_drawList->AddRect(a_bb.Min, a_bb.Max, a_col, a_rounding, 0, a_thickness);
	}

	// The colours ImGui uses for the parts of a window it draws in Begin(). Cleared for a themed window, which draws them itself
	static constexpr ImGuiCol s_themedWindowCols[] = {
		ImGuiCol_Border,
		ImGuiCol_ResizeGrip, ImGuiCol_ResizeGripHovered, ImGuiCol_ResizeGripActive,                   // in the cut-away corner
		ImGuiCol_SeparatorHovered, ImGuiCol_SeparatorActive,                                         // the resize edge, rounded into the corners
		ImGuiCol_ScrollbarBg, ImGuiCol_ScrollbarGrab, ImGuiCol_ScrollbarGrabHovered, ImGuiCol_ScrollbarGrabActive  // full length, into the corners
	};

	// Shortens a scrollbar's track along a_axis so it stays out of a_outer's cut-away corners of radius a_r
	static void KeepTrackClearOfCorners(ImRect& a_track, ImGuiAxis a_axis, const ImRect& a_outer, float a_r)
	{
		if (a_r <= 0.0f)
			return;

		const ImGuiAxis across = a_axis == ImGuiAxis_X ? ImGuiAxis_Y : ImGuiAxis_X;
		for (const float side : { a_outer.Min[across], a_outer.Max[across] }) {
			// How close the track comes to this side, and so how far the corners on it reach along the track
			const float gap = side < a_track.Min[across] ? a_track.Min[across] - side :
			                  side > a_track.Max[across] ? side - a_track.Max[across] :
			                                               0.0f;
			if (gap >= a_r)
				continue;

			const float reach    = std::sqrt(a_r * a_r - gap * gap);
			a_track.Min[a_axis] = std::max(a_track.Min[a_axis], std::ceil(a_outer.Min[a_axis] + reach));
			a_track.Max[a_axis] = std::min(a_track.Max[a_axis], std::floor(a_outer.Max[a_axis] - reach));
		}
	}

	// ImGui's scrollbar, redrawn over the themed background and shortened where it would run into a cut corner,
	// its own or the window's it sits in. ImGui still handles it, along its full length
	static void DrawThemedScrollbar(ImGuiWindow* a_window, ImGuiAxis a_axis, float a_cornerR)
	{
		ImGuiContext&     g     = *GImGui;
		const ImGuiStyle& style = g.Style;

		const ImRect full  = GetWindowScrollbarRect(a_window, a_axis);
		ImRect       frame = full;
		KeepTrackClearOfCorners(frame, a_axis, a_window->Rect(), a_cornerR);

		const ImGuiWindow* root = a_window->RootWindow;
		if (root && root != a_window && root->WindowRounding > 0.0f)
			KeepTrackClearOfCorners(frame, a_axis, root->Rect(), CapInvertedRounding(root->Rect(), root->WindowRounding) + root->WindowBorderSize);

		if (frame.Max[a_axis] - frame.Min[a_axis] <= 2.0f)
			return;

		ImDrawList* dl = a_window->DrawList;
		AddThemedRectFilled(dl, frame, GetColorU32(ImGuiCol_ScrollbarBg), style.ScrollbarRounding);

		// The same sums as ImGui's ScrollbarEx(), on the shortened track
		ImRect bb = frame;
		bb.Expand(ImVec2(-ImClamp(IM_TRUNC((frame.GetWidth() - 2.0f) * 0.5f), 0.0f, 3.0f), -ImClamp(IM_TRUNC((frame.GetHeight() - 2.0f) * 0.5f), 0.0f, 3.0f)));
		const float trackLen = bb.Max[a_axis] - bb.Min[a_axis];
		if (trackLen <= 0.0f)
			return;

		const float visible   = a_window->InnerRect.Max[a_axis] - a_window->InnerRect.Min[a_axis];
		const float contents  = a_window->ContentSize[a_axis] + a_window->WindowPadding[a_axis] * 2.0f;
		const float total     = std::max({ contents, visible, 1.0f });
		const float grabLen   = ImClamp(trackLen * (visible / total), style.GrabMinSize, trackLen);
		const float scrollMax = std::max(1.0f, contents - visible);
		const float ratio     = ImSaturate(a_window->Scroll[a_axis] / scrollMax);

		ImRect grab        = bb;
		grab.Min[a_axis]   = bb.Min[a_axis] + ratio * (trackLen - grabLen);
		grab.Max[a_axis]   = grab.Min[a_axis] + grabLen;

		const ImGuiID id      = GetWindowScrollbarID(a_window, a_axis);
		const bool    held    = g.ActiveId == id;
		const bool    hovered = g.HoveredId == id || (g.ActiveId == 0 && g.HoveredWindow == a_window && full.Contains(g.IO.MousePos));
		const ImGuiCol col    = held ? ImGuiCol_ScrollbarGrabActive : hovered ? ImGuiCol_ScrollbarGrabHovered : ImGuiCol_ScrollbarGrab;
		AddThemedRectFilled(dl, grab, GetColorU32(col), style.ScrollbarRounding);
	}

	// ImGui's highlight on the edge being hovered or dragged, stopping short of the cut corners
	static void DrawThemedResizeEdge(ImGuiWindow* a_window, float a_cornerR)
	{
		ImGuiContext& g    = *GImGui;
		const int     edge = (g.ActiveIdWindow == a_window) ? a_window->ResizeBorderHeld : a_window->ResizeBorderHovered;
		if (edge < 0)
			return;

		const ImRect bb   = a_window->Rect();
		const float  t    = std::max(2.0f, a_window->WindowBorderSize);
		const float  half = t * 0.5f;

		// Left, right, up, down, as ImGui numbers them
		ImVec2 a, b;
		switch (edge) {
		case 0:
			a = { bb.Min.x + half, bb.Min.y + a_cornerR };
			b = { bb.Min.x + half, bb.Max.y - a_cornerR };
			break;
		case 1:
			a = { bb.Max.x - half, bb.Min.y + a_cornerR };
			b = { bb.Max.x - half, bb.Max.y - a_cornerR };
			break;
		case 2:
			a = { bb.Min.x + a_cornerR, bb.Min.y + half };
			b = { bb.Max.x - a_cornerR, bb.Min.y + half };
			break;
		default:
			a = { bb.Min.x + a_cornerR, bb.Max.y - half };
			b = { bb.Max.x - a_cornerR, bb.Max.y - half };
			break;
		}

		const ImGuiCol col = (a_window->ResizeBorderHeld == edge) ? ImGuiCol_SeparatorActive : ImGuiCol_SeparatorHovered;
		a_window->DrawList->AddLine(a, b, GetColorU32(col), t);
	}

	// ImGui's resize grips, tucked into the curve of the bottom corners. ImGui still takes the drag from its own
	// square in the corner, which these overlap, so they light up from that same square
	static void DrawThemedResizeGrips(ImGuiWindow* a_window, float a_cornerR)
	{
		ImGuiContext& g  = *GImGui;
		const ImRect  bb = a_window->Rect();

		// ImGui's sizes for the grip and the square that takes it
		const float drawSize  = IM_TRUNC(std::max(g.FontSize * 1.35f, a_window->WindowRounding + 1.0f + g.FontSize * 0.2f));
		const float hoverSize = IM_TRUNC(drawSize * 0.75f);
		const bool  overUs    = g.ActiveId == 0 && g.HoveredWindow && g.HoveredWindow->RootWindow == a_window;

		const float in   = a_cornerR * 0.70710678f + a_window->WindowBorderSize;
		const float size = std::floor(g.FontSize * 0.6f);

		// Bottom-right, then bottom-left, which ImGui only has when windows resize from their edges
		const int grips = g.IO.ConfigWindowsResizeFromEdges ? 2 : 1;
		for (int n = 0; n < grips; ++n) {
			const float  dir    = n == 0 ? -1.0f : 1.0f;  // inwards from the corner
			const ImVec2 corner = { n == 0 ? bb.Max.x : bb.Min.x, bb.Max.y };
			const ImRect square(ImMin(corner.x, corner.x + dir * hoverSize), corner.y - hoverSize, ImMax(corner.x, corner.x + dir * hoverSize), corner.y);

			const ImGuiID id      = GetWindowResizeCornerID(a_window, n);
			const bool    held    = g.ActiveId == id;
			const bool    hovered = g.HoveredId == id || (overUs && square.Contains(g.IO.MousePos));

			// Like ImGui, the bottom-left one only shows while it's in use
			if (n == 1 && !held && !hovered)
				continue;

			const ImGuiCol col = held ? ImGuiCol_ResizeGripActive : hovered ? ImGuiCol_ResizeGripHovered : ImGuiCol_ResizeGrip;
			const ImVec2   tip = { std::floor(corner.x + dir * in), std::floor(corner.y - in) };
			a_window->DrawList->AddTriangleFilled(tip, { tip.x + dir * size, tip.y }, { tip.x, tip.y - size }, GetColorU32(col));
		}
	}

	bool PushThemedWindowBg(bool a_child)
	{
		// A child that isn't rounded itself is still taken over inside a rounded window, so its scrollbar can stay
		// clear of the window's corners. Its square corners are drawn as ImGui would
		const ImGuiStyle& style    = GetStyle();
		const float       rounding = a_child ? std::max(style.ChildRounding, style.WindowRounding) : style.WindowRounding;
		if (!UseInvertedCorners() || rounding <= 0.0f)
			return false;

		PushStyleColor(a_child ? ImGuiCol_ChildBg : ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
		for (ImGuiCol col : s_themedWindowCols)
			PushStyleColor(col, ImVec4(0, 0, 0, 0));
		return true;
	}

	void PopThemedWindowBg(bool a_pushed)
	{
		if (!a_pushed)
			return;
		PopStyleColor(1 + IM_ARRAYSIZE(s_themedWindowCols));

		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return;

		// Drawn before anything else in the window, so its contents go on top. Begin() has already clipped
		// the draw list to the inside of the window, so open it back up to the whole window.
		// A child's rounding and border size are already its own, from ChildRounding and ChildBorderSize
		const bool   isChild  = (window->Flags & ImGuiWindowFlags_ChildWindow) != 0;
		const ImRect bb       = window->Rect();
		const float  rounding = window->WindowRounding;
		const float  cornerR  = CapInvertedRounding(bb, rounding);
		ImDrawList*  dl       = window->DrawList;

		dl->PushClipRect(bb.Min, bb.Max, false);
		if (!(window->Flags & ImGuiWindowFlags_NoBackground)) {
			AddThemedRectFilled(dl, bb, GetColorU32(isChild ? ImGuiCol_ChildBg : ImGuiCol_WindowBg), rounding);
			if (window->WindowBorderSize > 0.0f)
				AddThemedRect(dl, bb, GetColorU32(ImGuiCol_Border), rounding, window->WindowBorderSize);
		}

		if (window->ScrollbarY)
			DrawThemedScrollbar(window, ImGuiAxis_Y, cornerR);
		if (window->ScrollbarX)
			DrawThemedScrollbar(window, ImGuiAxis_X, cornerR);

		if (!isChild && !(window->Flags & (ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))) {
			DrawThemedResizeEdge(window, cornerR);
			DrawThemedResizeGrips(window, cornerR);
		}
		dl->PopClipRect();
	}

	bool BeginChildThemed(const char* a_id, const ImVec2& a_size, ImGuiChildFlags a_childFlags, ImGuiWindowFlags a_windowFlags)
	{
		const bool themed  = PushThemedWindowBg(true);
		const bool visible = BeginChild(a_id, a_size, a_childFlags, a_windowFlags);
		PopThemedWindowBg(themed);
		return visible;
	}

	float GetWindowCornerInset()
	{
		if (!UseInvertedCorners())
			return 0.0f;

		const ImGuiWindow* window = GetCurrentWindowRead();
		return std::floor(CapInvertedRounding(window->Rect(), window->WindowRounding));
	}

	void AddRectFilledInWindow(ImDrawList* a_drawList, const ImRect& a_rect, ImU32 a_col)
	{
		const ImGuiWindow* window = GetCurrentWindowRead();
		const ImRect       outer  = window->Rect();

		// The corner's curve plus the border drawn along it
		float r = 0.0f;
		if (UseInvertedCorners() && window->WindowRounding > 0.0f)
			r = CapInvertedRounding(outer, window->WindowRounding) + window->WindowBorderSize;

		if (r <= 0.0f) {
			a_drawList->AddRectFilled(a_rect.Min, a_rect.Max, a_col);
			return;
		}

		// The top and bottom of the fill at x, pushed in by any corner it's under
		auto span = [&](float a_x, float& a_top, float& a_bottom) {
			a_top    = a_rect.Min.y;
			a_bottom = a_rect.Max.y;
			for (const float cx : { outer.Min.x, outer.Max.x }) {
				const float dx = a_x - cx;
				if (std::abs(dx) >= r)
					continue;
				const float dy = std::sqrt(r * r - dx * dx);
				a_top          = std::max(a_top, outer.Min.y + dy);
				a_bottom       = std::min(a_bottom, outer.Max.y - dy);
			}
		};

		// Away from the window's sides the corners can't reach, so that part is one plain fill. Near them it's filled
		// a pixel column at a time, each cut to the curve, so the corners can take any amount of it without seams
		const float plainMin = std::max(a_rect.Min.x, std::ceil(outer.Min.x + r));
		const float plainMax = std::min(a_rect.Max.x, std::floor(outer.Max.x - r));
		if (plainMax > plainMin)
			a_drawList->AddRectFilled({ plainMin, a_rect.Min.y }, { plainMax, a_rect.Max.y }, a_col);

		auto columns = [&](float a_from, float a_to) {
			for (float x = std::floor(a_from); x < a_to; x += 1.0f) {
				const float x0 = std::max(x, a_from);
				const float x1 = std::min(x + 1.0f, a_to);
				if (x1 <= x0)
					continue;

				float top, bottom;
				span((x0 + x1) * 0.5f, top, bottom);
				if (bottom > top)
					a_drawList->AddRectFilled({ x0, top }, { x1, bottom }, a_col);
			}
		};

		if (plainMax > plainMin) {
			columns(a_rect.Min.x, plainMin);
			columns(plainMax, a_rect.Max.x);
		} else {
			columns(a_rect.Min.x, a_rect.Max.x);
		}
	}
}
