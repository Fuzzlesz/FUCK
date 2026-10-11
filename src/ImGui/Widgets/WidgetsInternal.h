#pragma once

#include "ImGui/Audio.h"
#include "ImGui/IconsFontAwesome6.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Renderer.h"
#include "ImGui/Widgets.h"

#include "System/Input.h"

// Shared between the files in ImGui/Widgets. Not part of Widgets.h, so plugins and the rest of FUCK don't see it
namespace ImGui::WidgetsDetail
{
	// --- Corners.cpp ---
	void PathInvertedCorner(ImDrawList* dl, ImVec2 center, float radius, float a_min, float a_max, int num_segments = 12);
	void AddInvertedRect(ImDrawList* dl, const ImRect& bb, ImU32 col, float rounding, float thickness);

	// Inverted corners from opposite sides cross past half the shorter side, folding the shape over itself
	float CapInvertedRounding(const ImRect& a_bb, float a_rounding);

	// How far a cut-away corner reaches in from a frame's side, a_depth in from its top or bottom edge. 0 when corners aren't inverted
	float InvertedCornerReach(const ImRect& a_bb, float a_rounding, float a_depth);

	// --- Common.cpp ---
	// Round rather than floor, so scaled-down borders don't lose up to a whole pixel.
	// The black outline is three quarters of the coloured border, matching vanilla's buttons
	void GetBorderThickness(float a_borderSize, float& a_colour, float& a_black);

	// A border at least as thick as its rounding leaves the inside corner square, so keep rounding to
	// twice the border, giving an inner curve about as wide as the border. Zero stays square
	float KeepInnerCurve(float a_rounding);

	// Scaled the same way as button rounding, so frames meant to match buttons curve alike at any scale
	float GetFrameRounding();

	// --- Colours.cpp, Sliders.cpp ---
	template <typename TWidgetFunc>
	bool OutsetFramedWidget(const char* label, TWidgetFunc drawWidget, bool insetSides = false, bool trailingSwatch = false)
	{
		std::string id = std::format("##{}", label);

		float scale      = GetDynamicWidgetScale();
		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		float rounding   = GetFrameRounding();

		float padX     = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float tallPadY = 7.0f * scale;

		float tColor    = std::max(1.0f, std::round(borderSize));
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

		// insetSides matches the top/bottom inset, for widgets that draw up to their edges
		float offX = insetSides ? tColor : 0.0f;

		SetCursorScreenPos(screenPos + ImVec2(offX, offY));
		SetNextItemWidth(widgetWidth - offX * 2.0f);

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, innerPadY));
		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

		auto* drawList = GetWindowDrawList();
		drawList->ChannelsSplit(2);
		drawList->ChannelsSetCurrent(1);

		bool result = drawWidget(id.c_str());

		// Snapshot inner widget item data, as the Dummy() below would otherwise replace it as the last item
		ImGuiLastItemData innerItem = GImGui->LastItemData;

		bool hovered = bgHovered || IsItemHovered();
		bool active  = IsItemActive();
		bool focused = IsWidgetFocused(GetID(id.c_str()));

		if (bgClicked) {
			SetFocusID(GetID(id.c_str()), GetCurrentWindow());
		}

		drawList->ChannelsSetCurrent(0);

		// Stops before a trailing swatch, so the background doesn't show through its transparency
		ImRect      bgRect  = bb;
		ImDrawFlags bgFlags = ImDrawFlags_None;
		if (trailingSwatch) {
			bgRect.Max.x -= offX + GetFontSize() + innerPadY * 2.0f;
			bgFlags = ImDrawFlags_RoundCornersLeft;
		}

		AddThemedRectFilled(drawList, bgRect, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), rounding, bgFlags);

		if (borderSize > 0.0f) {
			DrawWidgetBorder(drawList, bb, hovered || active || focused, rounding);
		}

		drawList->ChannelsMerge();

		PopStyleVar(2);
		PopStyleColor(3);

		SetCursorPos(nextItemCursor);

		Dummy(ImVec2(0.0f, 0.0f));

		// Restore inner widget state, but report the framed bounds as the item rect
		innerItem.Rect       = bb;
		GImGui->LastItemData = innerItem;

		return result;
	}
}
