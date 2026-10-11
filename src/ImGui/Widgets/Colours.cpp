#include "WidgetsInternal.h"

namespace ImGui
{
	using namespace WidgetsDetail;

	// Colour edits fire every frame while dragging, so only play on grab and release (popup picker included)
	static void PlayGrabReleaseAudio()
	{
		if (IsItemActivated())
			PlayAudio(Audio::kFocus);
		else if (IsItemDeactivatedAfterEdit())
			PlayAudio(Audio::kPrevNext);
	}

	// Enforce the alpha bar unless the caller explicitly opted out with kNoAlpha.
	static ImGuiColorEditFlags WithAlphaBar(int flags)
	{
		auto f = static_cast<ImGuiColorEditFlags>(flags);
		if (!(f & ImGuiColorEditFlags_NoAlpha))
			f |= ImGuiColorEditFlags_AlphaBar;
		return f;
	}

	bool ColorEdit3Styled(const char* label, float col[3], int flags)
	{
		auto drawEdit = [&](const char* id) {
			return ColorEdit3(id, col, flags | ImGuiColorEditFlags_NoLabel);
		};
		bool hasSwatch = !(flags & ImGuiColorEditFlags_NoSmallPreview);

		bool res = OutsetFramedWidget(label, drawEdit, true, hasSwatch);
		PlayGrabReleaseAudio();
		return res;
	}

	bool ColorEdit4Styled(const char* label, float col[4], int flags)
	{
		auto drawEdit = [&](const char* id) {
			return ColorEdit4(id, col, WithAlphaBar(flags) | ImGuiColorEditFlags_NoLabel);
		};
		bool hasSwatch = !(flags & ImGuiColorEditFlags_NoSmallPreview);

		bool res = OutsetFramedWidget(label, drawEdit, true, hasSwatch);
		PlayGrabReleaseAudio();
		return res;
	}

	bool ColorButtonStyled(const char* desc_id, const ImVec4& col, int flags, const ImVec2& size, bool selected)
	{
		float borderSize   = GetUserStyleVar(USER_STYLE::kButtonBorderSize);
		bool  customBorder = borderSize > 0.0f && !(flags & ImGuiColorEditFlags_NoBorder);

		// Custom border doubles as the focus indicator, so suppress the native border and nav cursor
		if (customBorder) {
			flags |= ImGuiColorEditFlags_NoBorder;
			PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
		}

		// Snap to whole pixels, as DrawWidgetBorder floors its rect and the fill would otherwise peek out
		ImVec2 snapped = size;
		if (customBorder) {
			const float frameH = GetFrameHeight();
			snapped            = ImVec2(std::ceil(size.x > 0.0f ? size.x : frameH), std::ceil(size.y > 0.0f ? size.y : frameH));
			SetCursorScreenPos(ImFloor(GetCursorScreenPos()));
		}

		PushStyleVar(ImGuiStyleVar_FrameRounding, GetFrameRounding());
		bool pressed = ColorButton(desc_id, col, static_cast<ImGuiColorEditFlags>(flags), snapped);
		PopStyleVar();

		if (customBorder) {
			PopStyleColor();

			bool highlighted = selected || IsItemHovered() || IsItemActive() || IsWidgetFocused(GetItemID());
			DrawWidgetBorder(GetWindowDrawList(), ImRect(GetItemRectMin(), GetItemRectMax()), highlighted, GetFrameRounding());
		}

		if (pressed)
			PlayAudio(Audio::kOk);
		return pressed;
	}

	bool ColorPicker3Styled(const char* label, float col[3], int flags)
	{
		bool res = ColorPicker3(label, col, static_cast<ImGuiColorEditFlags>(flags));
		PlayGrabReleaseAudio();
		return res;
	}

	bool ColorPicker4Styled(const char* label, float col[4], int flags, const float* ref_col)
	{
		bool res = ColorPicker4(label, col, WithAlphaBar(flags), ref_col);
		PlayGrabReleaseAudio();
		return res;
	}
}
