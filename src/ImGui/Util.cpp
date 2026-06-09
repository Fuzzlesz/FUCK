#include "FUCK-Man.h"

#include "IconsFonts.h"
#include "Renderer.h"
#include "Styles.h"
#include "Util.h"

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

		const float thickness = std::max(1.0f, std::round(GetStyle().WindowBorderSize));

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
		float fullAvailX = GetContentRegionAvail().x;
		float startX     = GetCursorPosX();
		float startY     = GetCursorPosY();

		const bool hovered = IsWidgetFocused(newLabel.empty() ? label : newLabel.c_str());
		const bool dim     = MANAGER(Input)->IsInputGamepad() && !hovered;

		// Strip hidden ## IDs from the label before measuring text
		std::string_view labelView(label);
		auto             hashPos = labelView.find("##");
		if (hashPos != std::string_view::npos) {
			labelView = labelView.substr(0, hashPos);
		}

		bool  hasLabel       = !labelView.empty();
		float rightPaneStart = startX;
		auto& style          = Styles::GetSingleton()->user;

		if (hasLabel) {
			rightPaneStart = startX + (fullAvailX * style.widgetSplit) + GetStyle().ItemInnerSpacing.x;

			float targetHeight = GetFrameHeight();
			float textH        = GetTextLineHeight();

			// 0.5f uses ImGui's native FramePadding identically to AlignTextToFramePadding()
			float offY = style.labelAlign.y == 0.5f ? GetStyle().FramePadding.y : (targetHeight - textH) * style.labelAlign.y;

			ImU32 textColor = dim ? GetColorU32(ImGuiCol_TextDisabled) : GetColorU32(ImGuiCol_Text);

			// Calculate exact screen position
			ImVec2 screenPos = GetCursorScreenPos();
			screenPos.y += std::max(0.0f, offY);

			// Draw text directly to bypass layout engine interference and protect multi-widgets
			GetWindowDrawList()->AddText(GetFont(), GetFontSize(), screenPos, textColor, labelView.data(), labelView.data() + labelView.size());
		}

		// Advance cursor to the start of the right pane safely
		SetCursorPos(ImVec2(rightPaneStart, startY));

		if (!(GImGui->NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth)) {
			float rightPaneAvail = (startX + fullAvailX) - rightPaneStart;
			SetNextItemWidth(std::max(1.0f, rightPaneAvail));
		}
	}

	std::string LeftAlignedText(const char* label)
	{
		const auto newLabel = std::format("##{}", label);
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
		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
		if (GetContentRegionAvail().x < 15.0f) {
			PushStyleColor(ImGuiCol_Text, col);
			PushTextWrapPos(GetCursorPos().x + (350.0f * scale));
			TextUnformatted(text.data(), text.data() + text.size());
			PopTextWrapPos();
			PopStyleColor();
		} else {
			PushStyleColor(ImGuiCol_Text, col);
			PushTextWrapPos(0.0f);
			TextUnformatted(text.data(), text.data() + text.size());
			PopTextWrapPos();
			PopStyleColor();
		}
	}

	void TextWrappedEx(const char* text)
	{
		float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
		if (GetContentRegionAvail().x < 15.0f) {
			PushTextWrapPos(GetCursorPos().x + (350.0f * scale));
			TextUnformatted(text);
			PopTextWrapPos();
		} else {
			PushTextWrapPos(0.0f);
			TextUnformatted(text);
			PopTextWrapPos();
		}
	}

	void Spinner(const char* label, float radius, float thickness, const ImVec4& color)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return;
		ImGuiContext& g   = *GImGui;
		const ImGuiID id  = window->GetID(label);
		const ImVec2  pos = window->DC.CursorPos;
		const ImVec2  size((radius) * 2, (radius + g.Style.FramePadding.y) * 2);
		const ImRect  bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ItemSize(bb, g.Style.FramePadding.y);
		if (!ItemAdd(bb, id))
			return;
		window->DrawList->PathClear();
		int          num_segments = 30;
		int          start        = static_cast<int>(ImAbs(ImSin(static_cast<float>(g.Time) * 1.8f) * (num_segments - 5)));
		const float  a_min        = IM_PI * 2.0f * ((float)start) / (float)num_segments;
		const float  a_max        = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;
		const ImVec2 centre       = ImVec2(pos.x + radius, pos.y + radius + g.Style.FramePadding.y);
		for (int i = 0; i < num_segments; i++) {
			const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
			window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + static_cast<float>(g.Time) * 8.0f) * radius,
				centre.y + ImSin(a + static_cast<float>(g.Time) * 8.0f) * radius));
		}
		window->DrawList->PathStroke(ColorConvertFloat4ToU32(color), false, thickness);
	}

	void HelpMarker(const char* desc)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return;

		const ImGuiID id = window->GetID(desc);

		ImVec2 label_size = CalcTextSize("(?)", nullptr, true);
		float  scale      = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
		ImVec2 padding(6.0f * scale, 4.0f * scale);
		ImVec2 size(label_size.x + padding.x * 2.0f, label_size.y + padding.y * 2.0f);

		ImVec2 pos = window->DC.CursorPos;
		ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

		ItemSize(size, padding.y);
		if (!ItemAdd(bb, id)) {
			return;
		}

		bool hovered, held;
		ButtonBehavior(bb, id, &hovered, &held);

		bool isFocused = IsItemFocused();

		ImU32 textColor = (isFocused || hovered) ? GetColorU32(ImGuiCol_Text) : GetColorU32(ImGuiCol_TextDisabled);

		window->DrawList->AddText(ImVec2(pos.x + padding.x, pos.y + padding.y), textColor, "(?)");

		if (isFocused) {
			ImU32 navColor = GetColorU32(ImGuiCol_NavHighlight);
			float rounding = GetStyle().FrameRounding;
			window->DrawList->AddRect(bb.Min, bb.Max, navColor, rounding, 0, 2.0f * scale);
		}

		if (IsItemHovered(ImGuiHoveredFlags_ForTooltip) || isFocused) {
			PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale, 12.0f * scale));

			ImVec2 origMousePos = GetIO().MousePos;
			float  offset       = 20.0f * scale;
			GetIO().MousePos.x += offset;
			GetIO().MousePos.y += offset;

			if (BeginTooltip()) {
				PushTextWrapPos(GetFontSize() * 35.0f);
				DrawFormattedText(desc);
				PopTextWrapPos();
				EndTooltip();
			}

			GetIO().MousePos = origMousePos;
			PopStyleVar();
		}
	}

	void SetTooltipEx(const char* fmt)
	{
		if (IsItemHovered(ImGuiHoveredFlags_ForTooltip) || IsItemFocused()) {
			float scale = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale();
			PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale, 12.0f * scale));

			// Temporarily spoof the mouse position so ImGui spawns the tooltip offset,
			// while still utilising its native screen-edge clamping
			ImVec2 origMousePos = GetIO().MousePos;
			float  offset       = 20.0f * scale;
			GetIO().MousePos.x += offset;
			GetIO().MousePos.y += offset;

			if (BeginTooltip()) {
				DrawFormattedText(fmt);
				EndTooltip();
			}

			GetIO().MousePos = origMousePos;
			PopStyleVar();
		}
	}

	// Parses \n into linebreaks and [Brackets] into yellow
	void DrawFormattedText(const char* text)
	{
		if (!text)
			return;

		std::string s(text);

		// Un-escape literal "\\n" from translation files into actual newlines
		size_t pos = 0;
		while ((pos = s.find("\\n", pos)) != std::string::npos) {
			s.replace(pos, 2, "\n");
			pos += 1;  // Advance past the newly inserted '\n'
		}

		// If there are no brackets, draw normally so native ImGui auto-wrap still functions optimally
		if (s.find('[') == std::string::npos && s.find(']') == std::string::npos) {
			TextUnformatted(s.c_str());
			return;
		}

		ImVec4 cHighlight = ImVec4(1.0f, 0.85f, 0.2f, 1.0f);  // Yellow
		ImVec4 cNormal    = GetStyleColorVec4(ImGuiCol_Text);

		size_t line_start = 0;
		while (line_start < s.length()) {
			size_t      line_end = s.find('\n', line_start);
			std::string line     = (line_end == std::string::npos) ? s.substr(line_start) : s.substr(line_start, line_end - line_start);

			std::vector<std::pair<std::string, bool>> tokens;
			const char*                               p            = line.c_str();
			const char*                               start        = p;
			bool                                      is_highlight = false;

			while (*p) {
				if (*p == '[' || *p == ']') {
					if (p > start) {
						tokens.push_back({ std::string(start, p - start), is_highlight });
					}
					is_highlight = (*p == '[');
					start        = p + 1;
				}
				p++;
			}
			if (p > start) {
				tokens.push_back({ std::string(start, p - start), is_highlight });
			}

			if (tokens.empty()) {
				TextUnformatted("");
			} else {
				for (size_t i = 0; i < tokens.size(); ++i) {
					PushStyleColor(ImGuiCol_Text, tokens[i].second ? cHighlight : cNormal);
					TextUnformatted(tokens[i].first.c_str());
					PopStyleColor();

					if (i < tokens.size() - 1) {
						SameLine(0.0f, 0.0f);  // Zero spacing keeps the highlighted word attached to punctuation
					}
				}
			}

			if (line_end == std::string::npos)
				break;
			line_start = line_end + 1;
		}
	}

	void Header(const char* label)
	{
		auto* manager   = FUCKMan::GetSingleton();
		auto  largeFont = MANAGER(IconFont)->GetLargeFont();
		float scale     = manager->GetActiveScale() * Renderer::GetResolutionScale();
		float size      = (largeFont ? largeFont->LegacySize : GetStyle().FontSizeBase) * scale;

		Dummy(ImVec2(0.0f, 12.0f * scale));

		PushFont(largeFont, size);
		PushStyleColor(ImGuiCol_Text, GetUserStyleColorU32(USER_STYLE::kHeaderText));
		TextUnformatted(label);
		PopStyleColor();
		PopFont();
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

		SetWindowFocus(nullptr);

		GetIO().ClearInputKeys();
		GetIO().ClearEventsQueue();
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
