#include "Util.h"

#include "FUCKMan.h"
#include "IconsFonts.h"
#include "Renderer.h"
#include "Styles.h"
#include "System/Input.h"

namespace ImGui
{
	int IndexOfKey(const std::vector<std::pair<int, double>>& pair_list, const int key)
	{
		for (size_t i = 0; i < pair_list.size(); ++i) {
			auto& p = pair_list[i];
			if (p.first == key) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	// Copied from imgui_widgets.cpp
	float CalcMaxPopupHeightFromItemCount(const int items_count)
	{
		ImGuiContext& g = *GImGui;
		if (items_count <= 0)
			return FLT_MAX;
		return (g.FontSize + g.Style.ItemSpacing.y) * items_count - g.Style.ItemSpacing.y + (g.Style.WindowPadding.y * 2);
	}

	// https://github.com/ocornut/imgui/discussions/3862
	void AlignForWidth(float width, float alignment)
	{
		float avail = GetContentRegionAvail().x;
		float off   = (avail - width) * alignment;

		if (off > 0.0f) {
			SetCursorPosX(GetCursorPosX() + off);
		}
	}

	void ExtendWindowPastBorder()
	{
		const ImGuiWindow* window     = GetCurrentWindowRead();
		const float        borderSize = window->WindowBorderSize;

		if (borderSize <= 0.0f) {
			return;
		}

		auto*      drawList     = GetBackgroundDrawList();
		const auto newWindowPos = ImVec2{ window->Pos.x - borderSize, window->Pos.y - borderSize };

		float extendedRounding = window->WindowRounding > 0.0f ? window->WindowRounding + borderSize : 0.0f;

		drawList->AddRect(
			newWindowPos,
			newWindowPos + ImVec2(window->Size.x + 2 * borderSize, window->Size.y + 2 * borderSize),
			GetColorU32(ImGuiCol_WindowBg),
			extendedRounding,
			0, borderSize);
	}

	void AlignedButtonLabel(const char* label, const ImVec2& size, float alignment)
	{
		const auto  textSize = CalcTextSize(label);
		const float offY     = (size.y - textSize.y) * alignment;

		if (offY > 0.0f) {
			SetCursorPosY(GetCursorPosY() + offY);
		}
		TextUnformatted(label);
	}

	void SeparatorThick()
	{
		auto* window = GetCurrentWindow();
		if (window->SkipItems)
			return;

		const float thickness = std::max(1.0f, std::round(ImGui::GetStyle().WindowBorderSize));

		const ImVec2 pos = window->DC.CursorPos;
		const float  w   = GetContentRegionAvail().x;
		const ImRect bb(pos, ImVec2(pos.x + w, pos.y + thickness));

		ItemSize(ImVec2(0.0f, thickness));
		if (!ItemAdd(bb, 0))
			return;

		window->DrawList->AddRectFilled(bb.Min, bb.Max, GetUserStyleColorU32(USER_STYLE::kSeparator));
	}

	void LeftAlignedTextImpl(const char* label, const std::string& newLabel)
	{
		float fullAvailX = ImGui::GetContentRegionAvail().x;
		float startX     = ImGui::GetCursorPosX();
		float startY     = ImGui::GetCursorPosY();

		const bool hovered = IsWidgetFocused(newLabel.empty() ? label : newLabel.c_str());
		const bool dim     = MANAGER(Input)->IsInputGamepad() && !hovered;

		std::string_view labelView(label);
		auto             hashPos = labelView.find("##");
		if (hashPos != std::string_view::npos) {
			labelView = labelView.substr(0, hashPos);
		}

		bool  hasLabel       = !labelView.empty();
		float rightPaneStart = startX;
		auto& style          = ImGui::Styles::GetSingleton()->user;

		if (hasLabel) {
			rightPaneStart = startX + (fullAvailX * style.widgetSplit) + ImGui::GetStyle().ItemInnerSpacing.x;

			float targetHeight = ImGui::GetFrameHeight();
			float textH        = ImGui::GetTextLineHeight();

			// 0.5f uses ImGui's native FramePadding identically to AlignTextToFramePadding()
			float offY = style.labelAlign.y == 0.5f ? ImGui::GetStyle().FramePadding.y : (targetHeight - textH) * style.labelAlign.y;

			ImU32 textColor = dim ? ImGui::GetColorU32(ImGuiCol_TextDisabled) : ImGui::GetColorU32(ImGuiCol_Text);

			// Calculate exact screen position
			ImVec2 screenPos = ImGui::GetCursorScreenPos();
			screenPos.y += std::max(0.0f, offY);

			// Draw text directly to bypass layout engine interference and protect multi-widgets
			ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), screenPos, textColor, labelView.data(), labelView.data() + labelView.size());
		}

		// Advance cursor to the start of the right pane safely
		ImGui::SetCursorPos(ImVec2(rightPaneStart, startY));

		if (!(GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth)) {
			float rightPaneAvail = (startX + fullAvailX) - rightPaneStart;
			ImGui::SetNextItemWidth(std::max(1.0f, rightPaneAvail));
		}
	}

	std::string LeftAlignedText(const char* label)
	{
		const auto newLabel = "##"s + label;
		LeftAlignedTextImpl(label, newLabel);
		return newLabel;
	}

	void CenteredText(const char* label, bool vertical)
	{
		const auto textSize = CalcTextSize(label);
		const auto avail    = GetContentRegionAvail();

		if (vertical) {
			float offY = (avail.y - textSize.y) * 0.5f;
			if (offY > 0.0f) {
				SetCursorPosY(GetCursorPosY() + offY);
			}
		} else {
			float offX = (avail.x - textSize.x) * 0.5f;
			if (offX > 0.0f) {
				SetCursorPosX(GetCursorPosX() + offX);
			}
		}
		TextUnformatted(label);
	}

	void TextColoredWrapped(const ImVec4& col, std::string_view text)
	{
		PushStyleColor(ImGuiCol_Text, col);
		PushTextWrapPos(0.0f);
		TextUnformatted(text.data(), text.data() + text.size());
		PopTextWrapPos();
		PopStyleColor();
	}

	void Header(const char* label)
	{
		auto  largeFont = MANAGER(IconFont)->GetLargeFont();
		float scale     = FUCKMan::GetSingleton()->GetActiveScale() * ImGui::Renderer::GetResolutionScale();
		float size      = (largeFont ? largeFont->LegacySize : ImGui::GetStyle().FontSizeBase) * scale;

		ImGui::Dummy(ImVec2(0.0f, 12.0f * scale));

		ImGui::PushFont(largeFont, size);
		PushStyleColor(ImGuiCol_Text, GetUserStyleColorU32(USER_STYLE::kHeaderText));
		TextUnformatted(label);
		PopStyleColor();
		ImGui::PopFont();
		Separator();
	}

	ImU32 GetInteractiveColor(ImGuiCol base, ImGuiCol hovered, ImGuiCol active)
	{
		if (IsItemActive())
			return GetColorU32(active);
		if (IsItemHovered())
			return GetColorU32(hovered);
		return GetColorU32(base);
	}

	ImVec4 GetInteractiveColorVec4(ImGuiCol base, ImGuiCol hovered, ImGuiCol active)
	{
		if (IsItemActive())
			return GetStyleColorVec4(active);
		if (IsItemHovered())
			return GetStyleColorVec4(hovered);
		return GetStyleColorVec4(base);
	}

	ImU32 GetDynamicTextColor(bool highlighted)
	{
		// Returns Highlighted style if true, otherwise standard disabled/dim text
		return highlighted ? GetUserStyleColorU32(USER_STYLE::kTextHovered) : GetColorU32(ImGuiCol_TextDisabled);
	}

	bool FramelessImageButton(const char* str_id, ImTextureID user_texture_id, const ImVec2& image_size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col)
	{
		bool hasBG = (bg_col.w > 0.0f);
		if (!hasBG) {
			PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		}

		auto result = ImageButton(str_id, user_texture_id, image_size, uv0, uv1, bg_col, tint_col);

		if (!hasBG) {
			PopStyleColor(1);
		}
		return result;
	}

	bool AlignedImage(ID3D11ShaderResourceView* texID, const ImVec2& texture_size, const ImVec2& min, const ImVec2& max, const ImVec2& align, ImU32 color)
	{
		ImVec2 pos = min;

		if (align.x > 0.0f)
			pos.x = ImMax(pos.x, pos.x + (max.x - pos.x - texture_size.x) * align.x);
		if (align.y > 0.0f)
			pos.y = ImMax(pos.y, pos.y + (max.y - pos.y - texture_size.y) * align.y);

		GetCurrentWindow()->DrawList->AddImage(reinterpret_cast<ImTextureID>(texID), pos, pos + texture_size, ImVec2(0, 0), ImVec2(1, 1), color);

		return MANAGER(Input)->CanNavigateWithMouse() ? IsMouseHoveringRect(pos, pos + texture_size) && IsMouseClicked(0) && (GetItemFlags() & ImGuiItemFlags_Disabled) == 0 : false;
	}

	bool IsItemSelected()
	{
		if (IsItemHovered() || IsItemFocused()) {
			return IsMouseClicked(ImGuiMouseButton_Left) ||
			       IsKeyPressed(ImGuiKey_GamepadFaceDown) ||
			       IsKeyPressed(ImGuiKey_Enter) ||
			       IsKeyPressed(ImGuiKey_Space);
		}
		return false;
	}

	bool IsWidgetFocused()
	{
		return IsWidgetFocused(GetItemID());
	}

	bool IsWidgetFocused(std::string_view label)
	{
		return IsWidgetFocused(GetCurrentWindow()->GetID(label.data()));
	}

	bool IsWidgetFocused(ImGuiID id)
	{
		if (GetFocusID() == id)
			return true;

		if (MANAGER(Input)->CanNavigateWithMouse() && GetHoveredID() == id)
			return true;

		return false;
	}

	bool ActivateOnHover()
	{
		if (MANAGER(Input)->IsInputGamepad() || !MANAGER(Input)->CanNavigateWithMouse()) {
			if (!IsItemActive()) {
				if (IsItemFocused()) {
					ActivateItemByID(GetItemID());
					return true;
				}
			} else {
				UnfocusOnEscape();
			}
		}

		return false;
	}

	void UnfocusOnEscape()
	{
		if (MANAGER(Input)->IsInputGamepad()) {
			ImGuiContext& g = *GImGui;
			if (IsKeyDown(ImGuiKey_NavGamepadCancel)) {
				g.NavId            = 0;
				g.NavCursorVisible = false;
				SetWindowFocus(nullptr);
			}
		}
	}

	void ClearNavState()
	{
		ImGuiContext& g = *GImGui;

		g.ActiveId        = 0;
		g.NavId           = 0;
		g.NavFocusScopeId = 0;
		g.NavWindow       = nullptr;
		g.NavInitRequest  = false;

		g.NavCursorVisible         = false;
		g.NavHighlightItemUnderNav = false;

		g.NavInputSource = ImGuiInputSource_Mouse;

		for (int i = 0; i < g.Windows.Size; i++) {
			ImGuiWindow* window   = g.Windows[i];
			window->NavLastIds[0] = 0;
			window->NavLastIds[1] = 0;
		}

		ImGui::SetWindowFocus(nullptr);

		ImGui::GetIO().ClearInputKeys();
		ImGui::GetIO().ClearEventsQueue();
	}

	void Spacing(std::uint32_t a_numSpaces)
	{
		for (std::uint32_t i = 0; i < a_numSpaces; i++) {
			Spacing();
		}
	}

	ImVec2 GetNativeViewportPos()
	{
		return GetMainViewport()->Pos;  // always 0, 0
	}

	ImVec2 GetNativeViewportSize()
	{
		return GetMainViewport()->Size;
	}

	ImVec2 GetNativeViewportCenter()
	{
		const auto Size = GetNativeViewportSize();
		return { Size.x * 0.5f, Size.y * 0.5f };
	}
}
