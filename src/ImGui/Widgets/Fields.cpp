#include "WidgetsInternal.h"

#include "FormComboBox.h"

namespace ImGui
{
	using namespace WidgetsDetail;

	namespace
	{
		void DrawDropdownIcon(ImDrawList* drawList, ImVec2 bPos, ImVec2 bSize, bool isOpen, bool opensUp, bool isHovered)
		{
			static auto iconArrow = MANAGER(IconFont)->GetStepperRight();
			if (!iconArrow)
				return;

			ImU32 col    = GetDynamicTextColor(isHovered || isOpen);
			float scale  = GetDynamicWidgetScale();
			float aspect = iconArrow->imageSize.y > 0.0f ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

			auto ap = CalcArrowIconParams(aspect, isOpen, bSize.y, 30.0f * scale, 1.0f);

			ImVec2 iconPos = {
				bPos.x + (bSize.x - ap.drawSize.x) * 0.5f,
				bPos.y + ap.offsetY
			};

			// Combo/Window: Closed = Left, Open = Down/Up
			IconDirection dir;
			if (isOpen) {
				dir = opensUp ? IconDirection::kUp : IconDirection::kDown;
			} else {
				dir = IconDirection::kLeft;
			}

			DrawArrowIcon(drawList, iconPos, ap.drawSize, col, dir);
		}
	}

	struct ComboFilterState
	{
		char pattern[256] = { 0 };
	};

	static Map<ImGuiID, ComboFilterState>                  s_comboFilterStates;

	static Map<ImGuiID, FormComboBoxFiltered<RE::TESForm>> s_FormCaches;

	void ClearFormCaches()
	{
		s_comboFilterStates.clear();
		s_FormCaches.clear();
	}

	// A combo's closed preview text. Drawn here rather than by BeginCombo(), which runs it to the frame's edge,
	// under the dropdown icon and into any cut-away corners
	static void DrawComboPreview(ImDrawList* a_drawList, const ImRect& a_frame, const char* a_text, float a_padX, float a_padY)
	{
		if (!a_text || !a_text[0])
			return;

		const float  reach = InvertedCornerReach(a_frame, GetFrameRounding(), a_padY);
		const ImVec2 min   = { std::floor(a_frame.Min.x + std::max(a_padX, reach)), a_frame.Min.y + a_padY };
		const ImVec2 max   = { a_frame.Max.x - a_frame.GetHeight(), a_frame.Max.y };  // stops at the dropdown icon's square
		if (max.x <= min.x)
			return;

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		RenderTextClippedEx(a_drawList, min, max, a_text, nullptr, nullptr, ImVec2(0.0f, 0.0f), nullptr);
		PopStyleColor();
	}

	static bool FilterBoxImpl(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags, bool* inputFocused);

	bool ComboWithFilter(const char* label, int* current_item, std::span<const std::string> items, int popup_max_height_in_items, const Set<std::string>* a_favourites, std::string* a_favToggled, std::span<const std::uint32_t> a_badgeKeys, int* a_hovered)
	{
		if (a_hovered)
			*a_hovered = -1;

		ImGuiContext& g      = *GImGui;
		ImGuiWindow*  window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float currentFontScale = window->FontWindowScale;
		float scale            = Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale() * currentFontScale;

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize) * currentFontScale;
		float padX       = std::floor(std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale)));
		float padY       = 7.0f * GetDynamicWidgetScale();  // a button's, so it's the same height

		auto CalcListHeight = [&](int items_count) -> float {
			if (items_count <= 0)
				return 0.0f;
			float actualFontSize = GetFontSize();
			float spacingY       = GetStyle().ItemSpacing.y;
			// Round to ensure integer height
			return std::round((actualFontSize + spacingY) * items_count - spacingY + (padY * 2.0f) + (GetStyle().ChildBorderSize * 2.0f));
		};

		// Capture the default frame height BEFORE we push our custom padding
		float defaultFrameH = std::round(GetFrameHeight());

		PushStyleVar(ImGuiStyleVar_FramePadding, { padX, padY });
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Prevents double frame
		PushStyleVar(ImGuiStyleVar_FrameRounding, GetFrameRounding());

		const bool  isHidden = std::string_view(label).starts_with("##");
		std::string idStr    = isHidden ? std::string(label) : std::format("##{}", label);
		if (!isHidden)
			LeftAlignedTextImpl(label);

		ImVec2  widgetPos = GetCursorScreenPos();
		float   width     = std::round(CalcItemWidth());
		ImGuiID id        = window->GetID(idStr.c_str());

		float designPadY = 15.0f * Renderer::GetResolutionScale();
		float popupPad   = std::round(std::max(0.0f, designPadY - (2.0f * scale)));
		// Deeper, so the filter row sits clear of the dropdown's cut-away corners. ImGui keeps the sides at padX
		popupPad = std::max(popupPad, InvertedCornerClearance(GetStyle().PopupRounding, padX));

		float       frameH         = GetFrameHeight();
		ImDrawList* parentDrawList = GetWindowDrawList();

		// nonListH = filter input row + spacing between it and the child + outer popup top+bottom padding + border
		float nonListH = std::round(defaultFrameH + GetStyle().ItemSpacing.y + (popupPad * 2.0f) + (GetStyle().PopupBorderSize * 2.0f));

		// Pre-compute the popup height before BeginCombo
		float committedPopupH = 0.0f;
		if (!(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
			int   heightInItemsPre = std::max(2, popup_max_height_in_items);
			float idealListH_pre   = CalcListHeight(std::min(static_cast<int>(items.size()), heightInItemsPre));

			// Use the larger of above/below space since we don't know direction yet
			ImVec2 displaySize_pre = GetIO().DisplaySize;
			float  maxSpace_pre    = std::max(
                displaySize_pre.y - (widgetPos.y + frameH) - (8.0f * scale),
                widgetPos.y - (8.0f * scale));

			float maxListH_pre = std::floor(maxSpace_pre - nonListH) - 1.0f;
			if (maxListH_pre > 0.0f && idealListH_pre > maxListH_pre)
				idealListH_pre = maxListH_pre;

			committedPopupH = std::round(idealListH_pre + nonListH);
			SetNextWindowSizeConstraints({ width, committedPopupH }, { width, committedPopupH });
		}

		// Strip ## hidden tags from the preview text so it displays cleanly when closed
		std::string previewStr;
		if (*current_item >= 0 && *current_item < static_cast<int>(items.size())) {
			previewStr   = items[*current_item];
			auto hashPos = previewStr.find("##");
			if (hashPos != std::string::npos) {
				previewStr.erase(hashPos);
			}
		}
		const char* preview = previewStr.c_str();

		ImVec4 textBoxColor = GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox);

		// Filled here rather than by BeginCombo, so the frame follows the theme's corners
		AddThemedRectFilled(parentDrawList, ImRect(std::floor(widgetPos.x), std::floor(widgetPos.y), std::floor(widgetPos.x + width), std::floor(widgetPos.y + frameH)), ColorConvertFloat4ToU32(textBoxColor), GetFrameRounding());
		DrawComboPreview(parentDrawList, ImRect(widgetPos, widgetPos + ImVec2(width, frameH)), preview, padX, padY);
		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, textBoxColor);

		// Focus shows on the border, as on drags and sliders, rather than ImGui's nav cursor
		PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));

		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(popupPad, popupPad));

		bool isOpen = BeginComboThemed(idStr.c_str(), nullptr, ImGuiComboFlags_NoArrowButton | ImGuiComboFlags_HeightLarge);

		PopStyleVar(4);
		PopStyleColor(6);

		// Detect if opened Upwards
		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}

		DrawDropdownIcon(parentDrawList, { std::floor(widgetPos.x + width - frameH), std::floor(widgetPos.y) }, { frameH, frameH }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { std::floor(widgetPos.x), std::floor(widgetPos.y), std::floor(widgetPos.x + width), std::floor(widgetPos.y + frameH) }, isOpen || IsItemHovered() || IsWidgetFocused(id), GetFrameRounding());

		if (!isOpen)
			return false;

		SetWindowFontScale(currentFontScale);

		if (IsWindowAppearing())
			SetKeyboardFocusHere();

		auto& pattern        = s_comboFilterStates[id].pattern;
		bool  isInputFocused = false;
		SetNextItemWidth(GetContentRegionAvail().x);
		FilterBoxImpl("##filter", nullptr, pattern, sizeof(pattern), ImGuiInputTextFlags_AutoSelectAll, &isInputFocused);

		bool navigateToItems = false;
		bool enterPressed    = false;
		if (isInputFocused) {
			if (IsKeyPressed(ImGuiKey_DownArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadDown, false) ||
				IsKeyPressed(ImGuiKey_UpArrow, false) || IsKeyPressed(ImGuiKey_GamepadDpadUp, false)) {
				navigateToItems = true;
			} else if (IsKeyPressed(ImGuiKey_Enter, false) || IsKeyPressed(ImGuiKey_KeypadEnter, false) || IsKeyPressed(ImGuiKey_GamepadFaceDown, false)) {
				enterPressed = true;
			}
		}

		std::vector<int> displayIndices;
		bool             filtering      = pattern[0] != '\0';
		const bool       has_favourites = a_favourites != nullptr;

		// The badges' slot, as wide as the widest of their icons for the row's height, kept at every row's end whenever any row has one,
		// so the badges and the favourite stars beside them line up down the list whichever rows have them
		float badgeAspect = 0.0f;
		for (const auto key : a_badgeKeys) {
			if (key == 0)
				continue;
			const auto* icon = MANAGER(IconFont)->GetIcon(key);
			if (icon && icon->IsLoaded() && icon->imageSize.y > 0.0f)
				badgeAspect = std::max(badgeAspect, icon->imageSize.x / icon->imageSize.y);
		}

		if (filtering) {
			std::string        lowerFilter = clib_util::string::tolower(pattern);
			static std::string lowerItemBuffer;

			std::vector<std::pair<int, double>> scoredItems;
			for (int i = 0; i < static_cast<int>(items.size()); i++) {
				if (items[i].find("##NOFILTER") != std::string::npos || items[i].starts_with("##HEADER:")) {
					continue;
				}

				lowerItemBuffer = items[i];
				std::transform(lowerItemBuffer.begin(), lowerItemBuffer.end(), lowerItemBuffer.begin(),
					[](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });

				auto score = rapidfuzz::fuzz::partial_token_ratio(lowerFilter, lowerItemBuffer);

				if (score >= 65.0)
					scoredItems.push_back({ i, score });
			}

			std::ranges::sort(scoredItems, [&](const auto& a, const auto& b) {
				if (std::abs(a.second - b.second) < 0.0001) {
					if (has_favourites) {
						bool a_fav = a_favourites->contains(items[a.first]);
						bool b_fav = a_favourites->contains(items[b.first]);
						if (a_fav != b_fav)
							return a_fav > b_fav;
					}
					return a.first < b.first;
				}
				return a.second > b.second;
			});

			displayIndices.reserve(scoredItems.size());
			for (const auto& pair : scoredItems) {
				displayIndices.push_back(pair.first);
			}
		} else {
			displayIndices.reserve(items.size());
			for (int i = 0; i < static_cast<int>(items.size()); i++) {
				displayIndices.push_back(i);
			}

			if (has_favourites) {
				std::stable_sort(displayIndices.begin(), displayIndices.end(), [&](int a, int b) {
					bool a_noFilter = items[a].find("##NOFILTER") != std::string::npos;
					bool b_noFilter = items[b].find("##NOFILTER") != std::string::npos;
					if (a_noFilter != b_noFilter)
						return a_noFilter > b_noFilter;

					bool a_fav = a_favourites->contains(items[a]);
					bool b_fav = a_favourites->contains(items[b]);
					if (a_fav != b_fav)
						return a_fav > b_fav;

					return false;
				});
			}
		}

		bool changed    = false;
		int  show_count = static_cast<int>(displayIndices.size());

		if (enterPressed && show_count > 0) {
			*current_item = displayIndices[0];
			changed       = true;
			CloseCurrentPopup();
			PlayAudio(Audio::kFocus);
		} else if (IsKeyPressed(ImGuiKey_Escape, false) || IsKeyPressed(ImGuiKey_NavGamepadCancel, false)) {
			CloseCurrentPopup();
			PlayAudio(Audio::kCancel);
		}

		// --- Inner List Setup ---
		PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_ChildBg, textBoxColor);
		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padX, padY));

		int   heightInItems = std::max(2, popup_max_height_in_items);
		float requiredListH = CalcListHeight(std::min(show_count, heightInItems));

		float idealListH = std::max(0.0f, std::round(GetContentRegionAvail().y));

		ImGuiChildFlags  childFlags = ImGuiChildFlags_Borders;
		ImGuiWindowFlags winFlags   = ImGuiWindowFlags_NoMove;

		if (show_count <= heightInItems && idealListH >= requiredListH - 1.0f)
			winFlags |= ImGuiWindowFlags_NoScrollbar;

		if (BeginChildThemed("##List", ImVec2(0.0f, idealListH), childFlags, winFlags)) {
			SetWindowFontScale(currentFontScale);

			ImGuiListClipper clipper;
			clipper.Begin(show_count);

			// Ensure active item is always clipped in so SetScrollHereY resolves accurately
			int focus_idx = -1;
			if (navigateToItems && show_count > 0) {
				focus_idx = 0;
			} else {
				for (int i = 0; i < show_count; ++i) {
					if (*current_item == displayIndices[i]) {
						focus_idx = i;
						break;
					}
				}
			}
			if (focus_idx >= 0) {
				clipper.IncludeItemByIndex(focus_idx);
			}

			while (clipper.Step()) {
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
					int idx = displayIndices[i];

					// Push the index to guarantee unique IDs for identical labels
					PushID(idx);

					std::string_view itemStr(items[idx]);
					if (!filtering && itemStr.starts_with("##HEADER:")) {
						PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kWidgetFlash));
						if (i > 0) {
							Dummy(ImVec2(0, 4.0f * scale));
						}
						SetCursorPosX(GetCursorPosX() + padX);
						TextUnformatted(itemStr.data() + 9, itemStr.data() + itemStr.size());
						if (i < show_count - 1) {
							Dummy(ImVec2(0, 2.0f * scale));
						}
						PopStyleColor();
					} else {
						if (navigateToItems) {
							if (i == focus_idx) {
								SetKeyboardFocusHere(0);
							}
						}

						bool isSelected = (*current_item == idx);

						ImVec2 startPos = GetCursorPos();
						float  availW   = GetContentRegionAvail().x;

						bool mainClicked = Selectable(items[idx].c_str(), isSelected, ImGuiSelectableFlags_AllowOverlap);

						bool itemHovered = IsItemHovered();
						bool itemFocused = IsItemFocused();
						bool starHovered = false;

						if (a_hovered && (itemHovered || itemFocused)) {
							*a_hovered = idx;
						}

						// Badge centred in its slot at the row's end, the favourites star to the slot's left
						const float rowH   = GetItemRectSize().y;
						const float badgeH = std::round(rowH * 0.8f);
						const float slotW  = std::round(badgeH * badgeAspect);
						if (idx < static_cast<int>(a_badgeKeys.size()) && a_badgeKeys[idx] != 0) {
							auto icon = MANAGER(IconFont)->GetIcon(a_badgeKeys[idx]);
							if (icon && icon->IsLoaded()) {
								const float  badgeW = std::round(badgeH * (icon->imageSize.y > 0.0f ? (icon->imageSize.x / icon->imageSize.y) : 1.0f));
								const ImVec2 badgeMin(std::round(GetItemRectMin().x + availW - padX - slotW + (slotW - badgeW) * 0.5f), GetItemRectMin().y + std::round((rowH - badgeH) * 0.5f));
								GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(icon->GetSRView()), badgeMin, badgeMin + ImVec2(badgeW, badgeH));
							}
						}

						// Show tooltip if the text is too wide to fit in the available combo box space
						if (itemHovered && (CalcTextSize(items[idx].c_str()).x > availW - padX)) {
							SetTooltipEx(items[idx].c_str());
						}

						if (has_favourites) {
							bool isFav = a_favourites->contains(items[idx]);

							const bool pressedFavGamepad  = IsKeyPressed(ImGuiKey_GamepadFaceUp, false);
							const bool pressedFavKeyboard = IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_F);

							if ((itemHovered || itemFocused) && (pressedFavGamepad || pressedFavKeyboard)) {
								if (a_favToggled) {
									*a_favToggled = items[idx];
								}
								PlayAudio(Audio::kFocus);
							}

							if (isFav || itemHovered) {
								// Hit-tested by hand, bypassing ImGui's item overlap, as the star sits on the row's own item
								const ImRect row(GetItemRectMin(), ImVec2(GetItemRectMin().x + availW, GetItemRectMax().y));
								const float  inset = padX + (slotW > 0.0f ? slotW + 5.0f * scale : 0.0f);
								const auto   star  = PlaceFavouriteStar(row, GetFontSize() * 0.85f, inset, 5.0f * scale);

								starHovered = star.hovered;
								if (starHovered && IsMouseClicked(0)) {
									if (a_favToggled) {
										*a_favToggled = items[idx];
									}
									PlayAudio(Audio::kFocus);
								}
								DrawFavouriteStar(GetWindowDrawList(), star, isFav);
							}
						}

						// If the user clicked the row but was hovering the star, don't trigger the item selection
						if (mainClicked && !starHovered) {
							*current_item = idx;
							changed       = true;
							CloseCurrentPopup();
							PlayAudio(Audio::kFocus);
						}

						if (isSelected && IsWindowAppearing()) {
							SetScrollHereY();
						}
					}

					PopID();
				}
			}
		}
		EndChild();

		PopStyleVar();
		PopStyleColor(2);
		EndCombo();
		return changed;
	}

	bool ComboStyled(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height_in_items)
	{
		ImGuiContext& g      = *GImGui;
		ImGuiWindow*  window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float currentFontScale = window->FontWindowScale;
		float scale            = GetDynamicWidgetScale();

		if (popup_max_height_in_items == -1)
			popup_max_height_in_items = 8;

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize) * currentFontScale;
		float padX       = std::floor(std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale)));
		float padY       = 7.0f * GetDynamicWidgetScale();  // a button's, so it's the same height

		auto CalcListHeight = [&](int num_items) -> float {
			if (num_items <= 0)
				return 0.0f;
			float actualFontSize = GetFontSize();
			float spacingY       = GetStyle().ItemSpacing.y;
			// Round to ensure integer height
			return std::round((actualFontSize + spacingY) * num_items - spacingY);
		};

		PushStyleVar(ImGuiStyleVar_FramePadding, { padX, padY });
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);  // Prevents double frame
		PushStyleVar(ImGuiStyleVar_FrameRounding, GetFrameRounding());

		std::string idStr = std::format("##{}", label);
		LeftAlignedTextImpl(label);

		ImVec2 widgetPos = GetCursorScreenPos();
		float  width     = std::round(CalcItemWidth());
		float  frameH    = std::round(GetFrameHeight());

		// Pushed for the dropdown, so it matches the height worked out from it. Deeper with deep cut-away corners, so
		// the first and last rows' highlight clears them, as it reaches half the item spacing past each row
		const ImVec2 spacing   = GetStyle().ItemSpacing;
		const float  corner    = InvertedCornerClearance(GetStyle().PopupRounding, padX - std::floor(spacing.x * 0.5f));
		float        popupPadY = std::round(15.0f * Renderer::GetResolutionScale());
		if (corner > 0.0f)
			popupPadY = std::max(popupPadY, corner + std::ceil(spacing.y * 0.5f));

		// Compute an exact popup height so ImGui never allocates outer scrollbar space.
		float maxPopupH = std::round((popupPadY * 2.0f) + CalcListHeight(std::min(items_count, popup_max_height_in_items)) + (GetStyle().PopupBorderSize * 2.0f));

		if (!(g.NextWindowData.HasFlags & ImGuiNextWindowDataFlags_HasSizeConstraint)) {
			SetNextWindowSizeConstraints({ width, maxPopupH }, { width, maxPopupH });
		}

		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));

		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));

		// Focus shows on the border, as on drags and sliders, rather than ImGui's nav cursor
		PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));

		ImDrawList* parentDrawList = GetWindowDrawList();

		// Strip ## hidden tags from the preview text so it displays cleanly when closed
		std::string previewStr;
		if (*current_item >= 0 && *current_item < items_count) {
			previewStr   = items[*current_item];
			auto hashPos = previewStr.find("##");
			if (hashPos != std::string::npos) {
				previewStr.erase(hashPos);
			}
		}
		const char* preview = previewStr.c_str();

		AddThemedRectFilled(parentDrawList, ImRect(widgetPos, widgetPos + ImVec2(width, frameH)), GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), GetFrameRounding());
		DrawComboPreview(parentDrawList, ImRect(widgetPos, widgetPos + ImVec2(width, frameH)), preview, padX, padY);

		PushStyleVar(ImGuiStyleVar_WindowPadding, { padX, popupPadY });
		bool isOpen = BeginComboThemed(idStr.c_str(), nullptr, ImGuiComboFlags_NoArrowButton);

		PopStyleVar(4);
		PopStyleColor(6);

		// Detect if opened Upwards
		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			if (popupWindow && popupWindow->Pos.y < widgetPos.y)
				opensUp = true;
		}

		DrawDropdownIcon(parentDrawList, { std::floor(widgetPos.x + width - frameH), std::floor(widgetPos.y) }, { frameH, frameH }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { std::floor(widgetPos.x), std::floor(widgetPos.y), std::floor(widgetPos.x + width), std::floor(widgetPos.y + frameH) }, isOpen || IsItemHovered() || IsWidgetFocused(GetID(idStr.c_str())), GetFrameRounding());

		bool changed = false;
		if (isOpen) {
			SetWindowFontScale(currentFontScale);

			for (int i = 0; i < items_count; i++) {
				// Push the index to guarantee unique IDs for identical labels
				PushID(i);

				std::string_view itemStr(items[i]);
				if (itemStr.starts_with("##HEADER:")) {
					PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kWidgetFlash));
					if (i > 0)
						Dummy(ImVec2(0, 4.0f * scale));
					SetCursorPosX(GetCursorPosX() + padX);
					TextUnformatted(itemStr.data() + 9, itemStr.data() + itemStr.size());
					if (i < items_count - 1)
						Dummy(ImVec2(0, 2.0f * scale));
					PopStyleColor();
				} else {
					bool isSelected = (*current_item == i);
					if (Selectable(items[i], isSelected)) {
						*current_item = i;
						changed       = true;
						PlayAudio(Audio::kFocus);
					}

					if (IsWindowAppearing()) {
						if (isSelected || (*current_item < 0 && i == 0)) {
							SetKeyboardFocusHere(-1);
						}
					}
				}

				PopID();
			}
			EndCombo();
		}
		return changed;
	}

	bool BeginComboStyled(const char* a_id, const char* a_preview, float a_width, int a_itemCount)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		const float currentFontScale = window->FontWindowScale;
		const float scale            = GetDynamicWidgetScale();
		const float borderSize       = GetUserStyleVar(USER_STYLE::kButtonBorderSize) * currentFontScale;
		const float padX             = std::floor(std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale)));
		const float padY             = 7.0f * scale;  // a button's, so it's the same height

		PushStyleVar(ImGuiStyleVar_FramePadding, { padX, padY });
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_FrameRounding, GetFrameRounding());

		const ImVec2 widgetPos = GetCursorScreenPos();
		const float  width     = std::round(a_width > 0.0f ? a_width : CalcItemWidth());
		const float  frameH    = std::round(GetFrameHeight());

		// The dropdown's tallest, for a_itemCount rows as ComboStyled sizes its own, deeper with deep cut-away corners
		const ImVec2 spacing   = GetStyle().ItemSpacing;
		const float  corner    = InvertedCornerClearance(GetStyle().PopupRounding, padX - std::floor(spacing.x * 0.5f));
		float        popupPadY = std::round(15.0f * Renderer::GetResolutionScale());
		if (corner > 0.0f)
			popupPadY = std::max(popupPadY, corner + std::ceil(spacing.y * 0.5f));
		const int   rows   = std::clamp(a_itemCount, 1, 8);
		const float listH  = std::round((GetFontSize() + spacing.y) * rows - spacing.y);
		const float popupH = std::round(popupPadY * 2.0f + listH + GetStyle().PopupBorderSize * 2.0f);
		// Fitted to its rows up to that height, as the caller's rows may be fewer than it said, or taller
		SetNextWindowSizeConstraints({ width, 0.0f }, { FLT_MAX, popupH });

		PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_Text, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_PopupBg, GetUserStyleColorVec4(USER_STYLE::kComboBoxTextBox));
		PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));

		ImDrawList* parentDrawList = GetWindowDrawList();
		const ImRect box(widgetPos, widgetPos + ImVec2(width, frameH));
		AddThemedRectFilled(parentDrawList, box, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), GetFrameRounding());
		DrawComboPreview(parentDrawList, box, a_preview ? a_preview : "", padX, padY);

		// The width for this item alone, as an open dropdown is the current window after BeginCombo, so nothing pushed on the
		// parent can be popped here
		SetNextItemWidth(width);
		PushStyleVar(ImGuiStyleVar_WindowPadding, { padX, popupPadY });
		const bool isOpen = BeginComboThemed(a_id, nullptr, ImGuiComboFlags_NoArrowButton);
		PopStyleVar(4);
		PopStyleColor(6);

		bool opensUp = false;
		if (isOpen) {
			ImGuiWindow* popupWindow = GetCurrentWindow();
			opensUp                  = popupWindow && popupWindow->Pos.y < widgetPos.y;
		}
		DrawDropdownIcon(parentDrawList, { std::floor(widgetPos.x + width - frameH), std::floor(widgetPos.y) }, { frameH, frameH }, isOpen, opensUp, IsItemHovered());
		DrawWidgetBorder(parentDrawList, { std::floor(widgetPos.x), std::floor(widgetPos.y), std::floor(widgetPos.x + width), std::floor(widgetPos.y + frameH) },
			isOpen || IsItemHovered() || IsWidgetFocused(GetID(a_id)), GetFrameRounding());

		if (isOpen)
			SetWindowFontScale(currentFontScale);
		return isOpen;
	}

	static bool FilterBoxImpl(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags, bool* inputFocused)
	{
		if (GetCurrentWindow()->SkipItems || !buf || buf_size == 0)
			return false;

		const auto   id           = LeftAlignedText(label);
		const float  width        = CalcItemWidth();
		const float  height       = GetFrameHeight();
		const float  rounding     = GetFrameRounding();
		const float  iconFontSize = GetFontSize() * 0.75f;
		const float  iconWidth    = std::min(std::floor(width * 0.5f), std::ceil(height));
		const bool   showIcon     = iconWidth >= std::ceil(iconFontSize + GetStyle().FramePadding.x);
		const ImVec2 pos          = GetCursorScreenPos();
		const ImRect frame(pos, pos + ImVec2(width, height));
		auto*        drawList     = GetWindowDrawList();
		AddThemedRectFilled(drawList, frame, GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), rounding);

		// A focused results child belongs to its filter's panel. An open popup owns its own focus.
		// Lock the key to the first eligible field in a shared focus scope.
		constexpr auto focusFlags = ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_NoPopupHierarchy;
		if (GetActiveID() == 0 && !(GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) &&
			!(flags & ImGuiInputTextFlags_ReadOnly) && IsWindowFocused(focusFlags) &&
			IsRectVisible(frame.Min, frame.Max) && Shortcut(ImGuiMod_Ctrl | ImGuiKey_F, ImGuiInputFlags_RouteFocused)) {
			SetKeyOwner(ImGuiKey_F, GetID(id.c_str()), ImGuiInputFlags_LockThisFrame);
			SetKeyboardFocusHere();
		}

		BeginGroup();
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		PushStyleColor(ImGuiCol_FrameBg,   ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_Text,      GetUserStyleColorU32(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_NavCursor, ImVec4(0, 0, 0, 0));
		SetNextItemWidth(showIcon ? width - iconWidth : width);
		const bool changed     = InputTextWithHint(id.c_str(), hint ? hint : TRANSLATE("$FUCK_Sidebar_FilterHint"), buf, buf_size, flags);
		bool       highlighted = IsItemHovered() || IsItemActive() || IsWidgetFocused(GetID(id.c_str()));
		// Read the field's focus before submitting the clear button, which has its own navigation ID.
		if (inputFocused)
			*inputFocused = IsItemFocused() || IsItemActive();
		if (IsItemActivated())
			PlayAudio(Audio::kFocus);
		PopStyleColor(3);
		PopStyleVar();

		bool cleared = false;
		if (showIcon) {
			SameLine(0.0f, 0.0f);
			const ImVec2 slot         = GetCursorScreenPos();
			const bool   canClear     = buf[0] != '\0' && !(flags & ImGuiInputTextFlags_ReadOnly);
			bool         clearHovered = false;
			if (canClear) {
				PushID(id.c_str());
				cleared      = InvisibleButton("##Clear", ImVec2(iconWidth, height), ImGuiButtonFlags_EnableNav);
				clearHovered = IsItemHovered() || IsItemFocused();
				PopID();
				if (cleared)
					buf[0] = '\0';
			} else {
				Dummy(ImVec2(iconWidth, height));
			}
			highlighted |= clearHovered;

			// Centre the visible glyph, as the icon sits higher than the middle of the text line.
			const bool   showClear = canClear && !cleared;
			const char*  icon      = showClear ? ICON_FA_XMARK : ICON_FA_MAGNIFYING_GLASS;
			unsigned int codepoint = 0;
			ImTextCharFromUtf8(&codepoint, icon, nullptr);
			PushFont(nullptr, iconFontSize);
			const auto* glyph = GetFontBaked()->FindGlyph(static_cast<ImWchar>(codepoint));
			if (glyph && glyph->Visible) {
				const ImVec2 middle = slot + ImVec2(iconWidth, height) * 0.5f;
				const ImVec2 at(std::round(middle.x - (glyph->X0 + glyph->X1) * 0.5f), std::round(middle.y - (glyph->Y0 + glyph->Y1) * 0.5f));
				const ImU32 color = showClear ?
					GetColorU32(clearHovered ? ImVec4(1.0f, 0.46f, 0.46f, 1.0f) : ImVec4(0.88f, 0.27f, 0.27f, 1.0f)) :
					GetColorU32(ImGuiCol_TextDisabled);
				drawList->AddText(GetFont(), GetFontSize(), at, color, icon);
			}
			PopFont();
		}
		EndGroup();
		DrawWidgetBorder(drawList, frame, highlighted, rounding);
		if (inputFocused && cleared)
			*inputFocused = false;
		return changed || cleared;
	}

	bool FilterBox(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags)
	{
		return FilterBoxImpl(label, hint, buf, buf_size, flags, nullptr);
	}

	bool InputTextStyled(const char* label, char* buf, size_t buf_size, int flags)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		float currentFontScale = window->FontWindowScale;
		float scale            = GetDynamicWidgetScale();

		float borderSize = GetUserStyleVar(USER_STYLE::kButtonBorderSize) * currentFontScale;
		float padX       = std::max(GetStyle().FramePadding.x, borderSize + (8.0f * scale));
		float padY       = 7.0f * scale;

		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padX, padY));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_FrameRounding, GetFrameRounding());

		std::string idStr = LeftAlignedText(label);

		// Filled here rather than by InputText, so the frame follows the theme's corners
		const ImVec2 framePos = GetCursorScreenPos();
		AddThemedRectFilled(GetWindowDrawList(), ImRect(framePos, framePos + ImVec2(CalcItemWidth(), GetFrameHeight())), GetUserStyleColorU32(USER_STYLE::kComboBoxTextBox), GetFrameRounding());

		PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0, 0, 0, 0));
		PushStyleColor(ImGuiCol_Text,           GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
		PushStyleColor(ImGuiCol_NavCursor,      ImVec4(0, 0, 0, 0));  // Focus shows on the border, as on drags and sliders

		bool res = InputText(idStr.c_str(), buf, buf_size, flags);

		ImRect bb(GetItemRectMin(), GetItemRectMax());
		DrawWidgetBorder(GetWindowDrawList(), bb, IsItemHovered() || IsItemActive() || IsWidgetFocused(GetID(idStr.c_str())), GetFrameRounding());

		PopStyleColor(5);
		PopStyleVar(3);

		if (IsItemActivated())
			PlayAudio(Audio::kFocus);
		return res;
	}

	bool ComboForm(const char* label, RE::FormID* currentFormID, RE::FormType formType)
	{
		// Caches forms based on requested type to prevent continuous expensive lookups
		ImGuiID id       = GetID(label);
		ImGuiID cacheKey = ImHashData(&formType, sizeof(formType), id);

		auto [it, inserted] = s_FormCaches.try_emplace(cacheKey, label);
		it->second.InitForms(formType);

		if (currentFormID && *currentFormID != 0) {
			it->second.Sync(*currentFormID);
		}

		bool changed = false;

		it->second.GetFormResultFromCombo([&](RE::TESForm* form) {
			if (form) {
				*currentFormID = form->GetFormID();
				changed        = true;
			}
		});

		return changed;
	}

	bool ComboFormStr(const char* label, std::string* currentEdid, RE::FormType formType)
	{
		ImGuiID id       = GetID(label);
		ImGuiID cacheKey = ImHashData(&formType, sizeof(formType), id);

		auto [it, inserted] = s_FormCaches.try_emplace(cacheKey, label);
		it->second.InitForms(formType);

		if (currentEdid && !currentEdid->empty()) {
			it->second.Sync(*currentEdid);
		}

		bool changed = false;

		it->second.GetFormResultFromCombo([&](RE::TESForm* form) {
			if (form) {
				std::string edid = clib_util::editorID::get_editorID(form);
				if (!edid.empty()) {
					*currentEdid = edid;
					changed      = true;
				}
			}
		});

		return changed;
	}
}
