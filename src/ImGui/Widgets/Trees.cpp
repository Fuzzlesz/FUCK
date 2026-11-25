#include "WidgetsInternal.h"

namespace ImGui
{
	using namespace WidgetsDetail;

	namespace
	{
		void DrawTreeIcon(ImDrawList* drawList, const ImVec2& pos, float frameHeight, bool isOpen, bool isHovered, float baseIconSize = 30.0f)
		{
			static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
			if (!iconArrow)
				return;

			ImU32 col    = GetDynamicTextColor(isHovered);
			float scale  = GetDynamicWidgetScale();
			float aspect = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

			auto ap = CalcArrowIconParams(aspect, isOpen, frameHeight, baseIconSize * scale, 1.0f);

			// Center horizontally within its own max dimension to prevent shifting on open/close
			float maxIconDim  = std::max(ap.drawSize.x, ap.drawSize.y);
			float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

			ImVec2 drawPos = { pos.x + iconOffsetX, pos.y + ap.offsetY };
			DrawArrowIcon(drawList, drawPos, ap.drawSize, col,
				isOpen ? IconDirection::kDown : IconDirection::kRight);
		}

		// Expand all or collapse all, picked from any header's right-click menu. It reaches every header in the same
		// root window the next frame and then expires, so each header's own state takes over again
		struct FoldAll
		{
			ImGuiID root    = 0;
			int     pending = -1;  // 0 collapse, 1 expand, for the next frame
			int     live    = -1;  // the request headers act on this frame
			int     frame   = -1;
		};
		FoldAll s_foldAll;

		int FoldAllRequest(ImGuiWindow* a_window)
		{
			if (s_foldAll.frame != GImGui->FrameCount) {
				s_foldAll.frame   = GImGui->FrameCount;
				s_foldAll.live    = s_foldAll.pending;
				s_foldAll.pending = -1;
			}
			return s_foldAll.live >= 0 && a_window->RootWindow && a_window->RootWindow->ID == s_foldAll.root ? s_foldAll.live : -1;
		}

		void FoldAllMenu(ImGuiWindow* a_window, ImGuiID a_id, bool a_hovered)
		{
			PushID(static_cast<int>(a_id));
			if (a_hovered && IsMouseClicked(ImGuiMouseButton_Right))
				OpenPopup("##fold_all");
			if (BeginPopupThemed("##fold_all")) {
				int pick = -1;
				if (SelectableStyled(TRANSLATE("$FUCK_ExpandAll")))
					pick = 1;
				if (SelectableStyled(TRANSLATE("$FUCK_CollapseAll")))
					pick = 0;
				if (pick >= 0) {
					s_foldAll.root    = a_window->RootWindow ? a_window->RootWindow->ID : 0;
					s_foldAll.pending = pick;
					CloseCurrentPopup();
				}
				EndPopup();
			}
			PopID();
		}
	}

	bool CollapsingHeaderIcon(const char* label, int flags, float a_rightReserve, bool a_addRowSpacing)
	{
		// A row like the labelled widgets, spaced as they are, so a column of headers lines up with one of checkboxes beside it
		if (a_addRowSpacing)
			AddRowSpacing();
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID      id     = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);
		bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;

		if (const int fold = FoldAllRequest(window); fold >= 0 && !is_leaf) {
			is_open = fold == 1;
			window->DC.StateStorage->SetInt(id, is_open);
		}

		// Use the same row height as OutlineButton
		float scale       = GetDynamicWidgetScale();
		float frameHeight = std::max(GetFrameHeight(), CalcTextSize(label).y + 14.0f * scale);

		ImVec2 pos = window->DC.CursorPos;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, frameHeight));

		ItemSize(bb);
		if (!ItemAdd(bb, id))
			return is_open && !is_leaf;

		bool h, held;
		if (ButtonBehavior(bb, id, &h, &held)) {
			if (!is_leaf) {
				is_open = !is_open;
				window->DC.StateStorage->SetInt(id, is_open);
				PlayAudio(is_open ? Audio::kFocus : Audio::kCancel);
			}
		}
		if (!is_leaf)
			FoldAllMenu(window, id, h);

		// Check selection flag
		bool is_selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;

		// Draw Background
		if (h || is_open || is_selected) {
			ImU32 bgColor;
			if (is_selected) {
				bgColor = GetColorU32(ImGuiCol_Header);
			} else if (is_open && !is_leaf) {
				bgColor = GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox);
			} else {
				bgColor = GetColorU32(ImGuiCol_HeaderHovered);
			}

			// Only draw bg if active/hovered/selected
			if (h || is_selected || (is_open && !is_leaf)) {
				AddThemedRectFilled(window->DrawList, bb, bgColor, GetFrameRounding());
			}

			if (is_open && !is_leaf) {
				DrawWidgetBorder(window->DrawList, bb, h || IsWidgetFocused(id), GetFrameRounding());
			}
		}

		// Push the icon inwards to give it breathing room from the edge
		float padX = std::max(GImGui->Style.FramePadding.x, 10.0f * scale);

		float baseArrowSize = 30.0f;

		// Scale icon for the layout footprint
		float maxIconDim = baseArrowSize * scale;
		float textOff    = padX + maxIconDim + (8.0f * scale) + GImGui->Style.ItemInnerSpacing.x;

		// Pass the unscaled base icon (only if not a leaf)
		if (!is_leaf) {
			DrawTreeIcon(window->DrawList, { bb.Min.x + padX, bb.Min.y }, frameHeight, is_open, h, baseArrowSize);
		}

		// Vertically centre text, cut short with "..." to clear the right end and any room kept there, the whole of it in a tooltip
		const char*       labelEnd = FindRenderedTextEnd(label);
		const float       textMaxW = bb.Max.x - (bb.Min.x + textOff) - padX - a_rightReserve;
		bool              cut      = false;
		const std::string shown    = FitTextToWidth(label, labelEnd, std::max(0.0f, textMaxW), &cut);
		ImVec2            textSize = CalcTextSize(shown.c_str());
		float             textY    = bb.Min.y + (frameHeight - textSize.y) * 0.5f;

		RenderText({ bb.Min.x + textOff, textY }, shown.c_str());
		if (cut && h)
			SetTooltipEx(std::string(label, labelEnd).c_str());
		return is_open && !is_leaf;
	}

	bool TreeNodeIcon(const char* label, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID      id     = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open     = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);
		bool is_leaf     = (flags & ImGuiTreeNodeFlags_Leaf) != 0;
		bool no_push     = (flags & ImGuiTreeNodeFlags_NoTreePushOnOpen) != 0;
		bool is_selected = (flags & ImGuiTreeNodeFlags_Selected) != 0;

		float scale       = GetDynamicWidgetScale();
		float padY        = 3.0f * scale;
		float frameHeight = CalcTextSize(label).y + padY * 2.0f;

		ImVec2 pos = window->DC.CursorPos;
		ImRect bb(pos, pos + ImVec2(GetContentRegionAvail().x, frameHeight));

		ItemSize(bb);

		float padX = std::max(GImGui->Style.FramePadding.x, 10.0f * scale);

		float baseArrowSize = 20.0f;

		// Scale icon for the layout footprint
		float maxIconDim = baseArrowSize * scale;
		float textOff    = padX + maxIconDim + (8.0f * scale) + GImGui->Style.ItemInnerSpacing.x;

		if (!ItemAdd(bb, id)) {
			if (is_open && !is_leaf && !no_push)
				TreePush(label);
			return is_open && !is_leaf;
		}

		bool h, held;
		if (ButtonBehavior(bb, id, &h, &held, flags)) {
			if (!is_leaf) {
				is_open = !is_open;
				window->DC.StateStorage->SetInt(id, is_open);
				PlayAudio(is_open ? Audio::kFocus : Audio::kCancel);
			}
		}

		if (h || is_selected) {
			ImU32 bgColor = GetColorU32(is_selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered);
			AddThemedRectFilled(window->DrawList, bb, bgColor, GetFrameRounding());
		}

		// Pass the unscaled base icon (only if not a leaf)
		if (!is_leaf) {
			DrawTreeIcon(window->DrawList, { pos.x + padX, pos.y }, frameHeight, is_open, h, baseArrowSize);
		} else if (flags & ImGuiTreeNodeFlags_Bullet) {
			// Optional: draw a small bullet point for leaves if requested
			window->DrawList->AddCircleFilled({ pos.x + padX + (maxIconDim * 0.5f), pos.y + (frameHeight * 0.5f) }, 3.0f * scale, GetColorU32(ImGuiCol_Text));
		}

		// Align text
		float textY = bb.Min.y + (frameHeight - CalcTextSize(label).y) * 0.5f;

		RenderText({ pos.x + textOff, textY }, label);

		if (is_open && !is_leaf && !no_push)
			TreePush(label);

		return is_open && !is_leaf;
	}
}
