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
	}

	bool CollapsingHeaderIcon(const char* label, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		ImGuiID      id     = window->GetID(label);

		if (GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasOpen) {
			window->DC.StateStorage->SetInt(id, GImGui->NextItemData.OpenVal);
			GImGui->NextItemData.ClearFlags();
		}

		bool is_open = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) != 0);
		bool is_leaf = (flags & ImGuiTreeNodeFlags_Leaf) != 0;

		// Calculate total layout size using same padY as combo boxes
		float scale       = GetDynamicWidgetScale();
		float padY        = 7.0f * scale;
		float frameHeight = CalcTextSize(label).y + padY * 2.0f;

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

		// Vertically centre text
		ImVec2 textSize = CalcTextSize(label);
		float  textY    = bb.Min.y + (frameHeight - textSize.y) * 0.5f;

		RenderText({ bb.Min.x + textOff, textY }, label);
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
