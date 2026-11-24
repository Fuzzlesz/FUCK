#include "WidgetsInternal.h"

namespace ImGui
{
	using namespace WidgetsDetail;

	// Use native outward rounding for colour popups and tooltips. Pad their rectangular previews
	// clear of the curved border when the rest of the theme uses inverted corners.
	class ColourPopupStyle
	{
	public:
		ColourPopupStyle() : _pushed(UseInvertedCorners())
		{
			if (_pushed) {
				const auto& style = GetStyle();
				const ImVec2 padding = style.WindowPadding;
				const float fallbackRounding = std::max(2.0f, GetFrameRounding());
				const float popupRounding = style.PopupRounding > 0.0f ? style.PopupRounding : fallbackRounding;
				const float tooltipRounding = style.WindowRounding > 0.0f ? style.WindowRounding : fallbackRounding;

				// At the diagonal, a rounded corner needs about 30% of its radius in padding on both axes.
				const float inset = std::ceil(std::max(popupRounding, tooltipRounding) * 0.3f) +
				                    std::ceil(style.PopupBorderSize) + 1.0f;
				PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(std::max(padding.x, inset), std::max(padding.y, inset)));
				PushStyleVar(ImGuiStyleVar_PopupRounding, popupRounding);
				PushStyleVar(ImGuiStyleVar_WindowRounding, tooltipRounding);
			}
		}
		~ColourPopupStyle()
		{
			if (_pushed)
				PopStyleVar(3);
		}

		ColourPopupStyle(const ColourPopupStyle&) = delete;
		ColourPopupStyle& operator=(const ColourPopupStyle&) = delete;

	private:
		bool _pushed;
	};

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
		const ColourPopupStyle popupStyle;
		auto drawEdit = [&](const char* id) {
			return ColorEdit3(id, col, flags | ImGuiColorEditFlags_NoLabel);
		};
		bool hasSwatch = !(flags & ImGuiColorEditFlags_NoSmallPreview);

		bool res = OutsetFramedWidget(label, drawEdit, true, hasSwatch, false);
		PlayGrabReleaseAudio();
		return res;
	}

	bool ColorEdit4Styled(const char* label, float col[4], int flags)
	{
		const ColourPopupStyle popupStyle;
		auto drawEdit = [&](const char* id) {
			return ColorEdit4(id, col, WithAlphaBar(flags) | ImGuiColorEditFlags_NoLabel);
		};
		bool hasSwatch = !(flags & ImGuiColorEditFlags_NoSmallPreview);

		bool res = OutsetFramedWidget(label, drawEdit, true, hasSwatch, false);
		PlayGrabReleaseAudio();
		return res;
	}

	bool ColorButtonStyled(const char* desc_id, const ImVec4& col, int flags, const ImVec2& size, bool selected)
	{
		const ColourPopupStyle popupStyle;
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
			DrawWidgetBorder(GetWindowDrawList(), ImRect(GetItemRectMin(), GetItemRectMax()), highlighted, GetFrameRounding(), false);
		}

		if (pressed)
			PlayAudio(Audio::kOk);
		return pressed;
	}

	bool ColorPicker3Styled(const char* label, float col[3], int flags)
	{
		const ColourPopupStyle popupStyle;
		bool res = ColorPicker3(label, col, static_cast<ImGuiColorEditFlags>(flags));
		PlayGrabReleaseAudio();
		return res;
	}

	bool ColorPicker4Styled(const char* label, float col[4], int flags, const float* ref_col)
	{
		const ColourPopupStyle popupStyle;
		bool res = ColorPicker4(label, col, WithAlphaBar(flags), ref_col);
		PlayGrabReleaseAudio();
		return res;
	}
}
