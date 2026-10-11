#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

namespace ImGui
{
	namespace
	{
		struct Section
		{
			std::string label;
			int         count    = -1;       // Drawn on the right of the header, unless negative
			ImGuiID     stateId  = 0;        // Where CollapsingHeaderIcon keeps the open state
			bool*       userOpen = nullptr;  // The caller's own open state, if they passed one
			ImVec2      min;                 // Top left of the header
			float       height = 0.0f;       // Height of the header
		};

		struct StickyList
		{
			ImGuiWindow* window  = nullptr;
			float        viewTop = 0.0f;  // Top of the visible area, where a stuck header sits

			// Section being drawn
			Section current;
			bool    inSection  = false;
			bool    canStick   = false;  // Open, with its header scrolled past the top
			bool    clipPushed = false;
			float   indent     = 0.0f;  // Undone when the section ends

			// Section whose header is stuck, drawn last by EndStickyList()
			Section stuck;
			float   stuckBottom = 0.0f;  // Where its rows end, so the next header can push it up
			bool    hasStuck    = false;
		};

		// A stack, so a list can sit inside another list's section
		std::vector<StickyList> s_lists;

		// Matches the height CollapsingHeaderIcon gives itself
		float CalcHeaderHeight(const char* a_label)
		{
			return CalcTextSize(a_label).y + 7.0f * GetDynamicWidgetScale() * 2.0f;
		}

		// Matches CollapsingHeaderIcon's side padding
		float CalcHeaderPadX()
		{
			return std::max(GImGui->Style.FramePadding.x, 10.0f * GetDynamicWidgetScale());
		}

		// Count dimmed on the right of the last header. Drawn rather than added to the label, so the ID never changes
		void DrawCount(int a_count)
		{
			if (a_count < 0)
				return;

			char buf[16];
			snprintf(buf, sizeof(buf), "%d", a_count);

			const ImVec2 min  = GetItemRectMin();
			const ImVec2 max  = GetItemRectMax();
			const ImVec2 size = CalcTextSize(buf);
			const ImVec2 pos(std::floor(max.x - CalcHeaderPadX() - size.x), std::floor(min.y + (max.y - min.y - size.y) * 0.5f));

			GetWindowDrawList()->AddText(pos, GetColorU32(ImGuiCol_TextDisabled), buf);
		}

		void EndSection(StickyList& a_list)
		{
			if (!a_list.inSection)
				return;

			PopID();

			if (a_list.indent > 0.0f) {
				Unindent(a_list.indent);
				a_list.indent = 0.0f;
			}

			if (a_list.clipPushed) {
				PopClipRect();
				a_list.clipPushed = false;
			}

			// The next item starts here, so a stuck header pushed up by the next one sits flush against it
			const float bottom = a_list.window->DC.CursorPos.y;
			if (a_list.canStick && bottom > a_list.viewTop) {
				a_list.stuck       = std::move(a_list.current);
				a_list.stuckBottom = bottom;
				a_list.hasStuck    = true;
			}

			a_list.inSection = false;
		}

		// Nav scrolls a newly focused row into view, but can leave it under the stuck header, so scroll on until it clears.
		// The stuck header itself is left alone, as it belongs at the top
		void KeepNavItemClear(ImGuiWindow* a_window, float a_coveredTo, ImGuiID a_stuckId)
		{
			ImGuiContext& g = *GImGui;
			if (g.NavWindow != a_window || g.NavJustMovedToId == 0 || g.NavJustMovedToId != g.NavId || g.NavId == a_stuckId)
				return;

			// Rounded up, as scroll moves in whole pixels and could leave it a fraction under
			const ImRect rect = WindowRectRelToAbs(a_window, a_window->NavRectRel[g.NavLayer]);
			if (rect.Min.y < a_coveredTo)
				SetScrollY(a_window, a_window->Scroll.y - ImCeil(a_coveredTo - rect.Min.y));
		}

		void DrawStuckHeader(StickyList& a_list)
		{
			ImGuiWindow*   window  = a_list.window;
			ImGuiStorage*  storage = window->DC.StateStorage;
			const Section& sec     = a_list.stuck;

			// Pinned to the top, until the next header reaches it and pushes it up
			const float y = std::min(a_list.viewTop, a_list.stuckBottom - sec.height);

			// Drawn last so it's on top and takes the clicks, then the layout is put back as if it was never there
			auto&        dc           = window->DC;
			const ImVec2 cursor       = dc.CursorPos;
			const ImVec2 cursorMax    = dc.CursorMaxPos;
			const ImVec2 prevLinePos  = dc.CursorPosPrevLine;
			const ImVec2 prevLineSize = dc.PrevLineSize;
			const ImVec2 currLineSize = dc.CurrLineSize;
			const float  prevBase     = dc.PrevLineTextBaseOffset;
			const float  currBase     = dc.CurrLineTextBaseOffset;
			const bool   isSameLine   = dc.IsSameLine;

			dc.CursorPos = ImVec2(sec.min.x, y);

			// Under its own ID, as the real header may still be partly in view.
			// The copy is always open, and closing it closes the real section
			PushID("StuckHeader");
			const ImGuiID stuckId = GetID(sec.label.c_str());
			KeepNavItemClear(window, y + sec.height, stuckId);
			storage->SetInt(stuckId, 1);
			const bool stillOpen = CollapsingHeaderIcon(sec.label.c_str());
			DrawCount(sec.count);
			PopID();

			dc.CursorPos              = cursor;
			dc.CursorMaxPos           = cursorMax;
			dc.CursorPosPrevLine      = prevLinePos;
			dc.PrevLineSize           = prevLineSize;
			dc.CurrLineSize           = currLineSize;
			dc.PrevLineTextBaseOffset = prevBase;
			dc.CurrLineTextBaseOffset = currBase;
			dc.IsSameLine             = isSameLine;

			if (!stillOpen) {
				storage->SetInt(sec.stateId, 0);
				if (sec.userOpen)
					*sec.userOpen = false;

				// Collapsing from here would leave the view where its rows were, so scroll back to its header
				SetScrollY(window, window->Scroll.y - (a_list.viewTop - sec.min.y));
			}
		}
	}

	void BeginStickyList(const char* str_id, const ImVec2& size, bool border)
	{
		BeginChildThemed(str_id, size, border ? ImGuiChildFlags_Borders : ImGuiChildFlags_None);

		auto& list   = s_lists.emplace_back();
		list.window  = GetCurrentWindow();
		list.viewTop = list.window->ClipRect.Min.y;
	}

	bool StickyHeader(const char* label, bool* open, int count, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();

		// Outside a list it's just a collapsing header
		if (s_lists.empty() || s_lists.back().window != window) {
			if (open)
				SetNextItemOpen(*open);
			const bool result = CollapsingHeaderIcon(label, flags);
			DrawCount(count);
			if (open)
				*open = result;
			return result;
		}

		auto& list = s_lists.back();
		EndSection(list);

		ImGuiContext& g       = *GImGui;
		ImGuiStorage* storage = window->DC.StateStorage;
		const ImGuiID id      = window->GetID(label);

		// Need the open state before drawing the header, to know whether to clip it.
		// The caller's bool comes first, then SetNextItemOpen, then whatever was stored last frame
		if (open) {
			storage->SetInt(id, *open);
			g.NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasOpen;
		} else if (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			storage->SetInt(id, g.NextItemData.OpenVal);
			g.NextItemData.HasFlags &= ~ImGuiNextItemDataFlags_HasOpen;
		}

		const bool isLeaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
		const bool isOpen = storage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0) != 0;

		Section& sec = list.current;
		sec.label    = label;
		sec.count    = count;
		sec.stateId  = id;
		sec.userOpen = open;
		sec.min      = window->DC.CursorPos;
		sec.height   = CalcHeaderHeight(label);

		list.inSection = true;
		list.canStick  = isOpen && !isLeaf && sec.min.y < list.viewTop;

		// The stuck copy covers the top of the list, so clip this section under it, header included.
		// Then clipped rows can't show through its background or take its hover and clicks
		if (list.canStick) {
			PushClipRect(ImVec2(window->ClipRect.Min.x, list.viewTop + sec.height), window->ClipRect.Max, true);
			list.clipPushed = true;
		}

		const bool result = CollapsingHeaderIcon(label, flags);
		DrawCount(count);
		if (open)
			*open = result;

		// Rows get the style's indent, and IDs scoped to this section so the same text in two sections doesn't clash.
		// Both end with the section
		if (result) {
			list.indent = g.Style.IndentSpacing;
			Indent(list.indent);
		}
		PushID(static_cast<int>(id));

		return result;
	}

	void EndStickyList()
	{
		IM_ASSERT(!s_lists.empty() && "EndStickyList() called without a matching BeginStickyList()");
		if (s_lists.empty())
			return;

		auto& list = s_lists.back();
		EndSection(list);

		if (list.hasStuck && !list.window->SkipItems)
			DrawStuckHeader(list);

		s_lists.pop_back();
		EndChild();
	}
}
