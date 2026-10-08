#pragma once

// ==================================================
// FLAG CHECKS
// ==================================================

namespace FUCK::Host
{
	// Fails the build if ImGui renumbers a flag
#define FUCK_ASS_FLAG(a, b) static_assert(static_cast<int>(a) == static_cast<int>(b), #a " no longer matches " #b)

	FUCK_ASS_FLAG(TableBgTarget::kRowBg0, ImGuiTableBgTarget_RowBg0);
	FUCK_ASS_FLAG(TableBgTarget::kRowBg1, ImGuiTableBgTarget_RowBg1);
	FUCK_ASS_FLAG(TableBgTarget::kCellBg, ImGuiTableBgTarget_CellBg);

	FUCK_ASS_FLAG(TableFlags::kResizable,         ImGuiTableFlags_Resizable);
	FUCK_ASS_FLAG(TableFlags::kReorderable,       ImGuiTableFlags_Reorderable);
	FUCK_ASS_FLAG(TableFlags::kHideable,          ImGuiTableFlags_Hideable);
	FUCK_ASS_FLAG(TableFlags::kSortable,          ImGuiTableFlags_Sortable);
	FUCK_ASS_FLAG(TableFlags::kNoSavedSettings,   ImGuiTableFlags_NoSavedSettings);
	FUCK_ASS_FLAG(TableFlags::kContextMenuInBody, ImGuiTableFlags_ContextMenuInBody);
	FUCK_ASS_FLAG(TableFlags::kRowBg,             ImGuiTableFlags_RowBg);
	FUCK_ASS_FLAG(TableFlags::kBordersInnerH,     ImGuiTableFlags_BordersInnerH);
	FUCK_ASS_FLAG(TableFlags::kBordersOuterH,     ImGuiTableFlags_BordersOuterH);
	FUCK_ASS_FLAG(TableFlags::kBordersInnerV,     ImGuiTableFlags_BordersInnerV);
	FUCK_ASS_FLAG(TableFlags::kBordersOuterV,     ImGuiTableFlags_BordersOuterV);
	FUCK_ASS_FLAG(TableFlags::kSizingFixedFit,    ImGuiTableFlags_SizingFixedFit);
	FUCK_ASS_FLAG(TableFlags::kSizingFixedSame,   ImGuiTableFlags_SizingFixedSame);
	FUCK_ASS_FLAG(TableFlags::kSizingStretchProp, ImGuiTableFlags_SizingStretchProp);
	FUCK_ASS_FLAG(TableFlags::kSizingStretchSame, ImGuiTableFlags_SizingStretchSame);

	FUCK_ASS_FLAG(TableColumnFlags::kDisabled,             ImGuiTableColumnFlags_Disabled);
	FUCK_ASS_FLAG(TableColumnFlags::kDefaultHide,          ImGuiTableColumnFlags_DefaultHide);
	FUCK_ASS_FLAG(TableColumnFlags::kDefaultSort,          ImGuiTableColumnFlags_DefaultSort);
	FUCK_ASS_FLAG(TableColumnFlags::kWidthStretch,         ImGuiTableColumnFlags_WidthStretch);
	FUCK_ASS_FLAG(TableColumnFlags::kWidthFixed,           ImGuiTableColumnFlags_WidthFixed);
	FUCK_ASS_FLAG(TableColumnFlags::kNoResize,             ImGuiTableColumnFlags_NoResize);
	FUCK_ASS_FLAG(TableColumnFlags::kNoReorder,            ImGuiTableColumnFlags_NoReorder);
	FUCK_ASS_FLAG(TableColumnFlags::kNoHide,               ImGuiTableColumnFlags_NoHide);
	FUCK_ASS_FLAG(TableColumnFlags::kNoClip,               ImGuiTableColumnFlags_NoClip);
	FUCK_ASS_FLAG(TableColumnFlags::kNoSort,               ImGuiTableColumnFlags_NoSort);
	FUCK_ASS_FLAG(TableColumnFlags::kNoSortAscending,      ImGuiTableColumnFlags_NoSortAscending);
	FUCK_ASS_FLAG(TableColumnFlags::kNoSortDescending,     ImGuiTableColumnFlags_NoSortDescending);
	FUCK_ASS_FLAG(TableColumnFlags::kNoHeaderLabel,        ImGuiTableColumnFlags_NoHeaderLabel);
	FUCK_ASS_FLAG(TableColumnFlags::kNoHeaderWidth,        ImGuiTableColumnFlags_NoHeaderWidth);
	FUCK_ASS_FLAG(TableColumnFlags::kPreferSortAscending,  ImGuiTableColumnFlags_PreferSortAscending);
	FUCK_ASS_FLAG(TableColumnFlags::kPreferSortDescending, ImGuiTableColumnFlags_PreferSortDescending);
	FUCK_ASS_FLAG(TableColumnFlags::kIndentEnable,         ImGuiTableColumnFlags_IndentEnable);
	FUCK_ASS_FLAG(TableColumnFlags::kIndentDisable,        ImGuiTableColumnFlags_IndentDisable);

	FUCK_ASS_FLAG(DragDropFlags::kSourceNoPreviewTooltip,   ImGuiDragDropFlags_SourceNoPreviewTooltip);
	FUCK_ASS_FLAG(DragDropFlags::kSourceNoDisableHover,     ImGuiDragDropFlags_SourceNoDisableHover);
	FUCK_ASS_FLAG(DragDropFlags::kSourceNoHoldToOpenOthers, ImGuiDragDropFlags_SourceNoHoldToOpenOthers);
	FUCK_ASS_FLAG(DragDropFlags::kSourceAllowNullID,        ImGuiDragDropFlags_SourceAllowNullID);
	FUCK_ASS_FLAG(DragDropFlags::kSourceExtern,             ImGuiDragDropFlags_SourceExtern);
	FUCK_ASS_FLAG(DragDropFlags::kSourceAutoExpirePayload,  ImGuiDragDropFlags_PayloadAutoExpire);
	FUCK_ASS_FLAG(DragDropFlags::kAcceptBeforeDelivery,     ImGuiDragDropFlags_AcceptBeforeDelivery);
	FUCK_ASS_FLAG(DragDropFlags::kAcceptNoDrawDefaultRect,  ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
	FUCK_ASS_FLAG(DragDropFlags::kAcceptNoPreviewTooltip,   ImGuiDragDropFlags_AcceptNoPreviewTooltip);

	FUCK_ASS_FLAG(PopupFlags::kAnyPopupId,    ImGuiPopupFlags_AnyPopupId);
	FUCK_ASS_FLAG(PopupFlags::kAnyPopupLevel, ImGuiPopupFlags_AnyPopupLevel);

	FUCK_ASS_FLAG(ColorEditFlags::kNoAlpha,         ImGuiColorEditFlags_NoAlpha);
	FUCK_ASS_FLAG(ColorEditFlags::kNoPicker,         ImGuiColorEditFlags_NoPicker);
	FUCK_ASS_FLAG(ColorEditFlags::kNoOptions,        ImGuiColorEditFlags_NoOptions);
	FUCK_ASS_FLAG(ColorEditFlags::kNoSmallPreview,   ImGuiColorEditFlags_NoSmallPreview);
	FUCK_ASS_FLAG(ColorEditFlags::kNoInputs,         ImGuiColorEditFlags_NoInputs);
	FUCK_ASS_FLAG(ColorEditFlags::kNoTooltip,        ImGuiColorEditFlags_NoTooltip);
	FUCK_ASS_FLAG(ColorEditFlags::kNoLabel,          ImGuiColorEditFlags_NoLabel);
	FUCK_ASS_FLAG(ColorEditFlags::kNoSidePreview,    ImGuiColorEditFlags_NoSidePreview);
	FUCK_ASS_FLAG(ColorEditFlags::kNoDragDrop,       ImGuiColorEditFlags_NoDragDrop);
	FUCK_ASS_FLAG(ColorEditFlags::kNoBorder,         ImGuiColorEditFlags_NoBorder);
	FUCK_ASS_FLAG(ColorEditFlags::kAlphaOpaque,      ImGuiColorEditFlags_AlphaOpaque);
	FUCK_ASS_FLAG(ColorEditFlags::kAlphaNoBg,        ImGuiColorEditFlags_AlphaNoBg);
	FUCK_ASS_FLAG(ColorEditFlags::kAlphaPreviewHalf, ImGuiColorEditFlags_AlphaPreviewHalf);
	FUCK_ASS_FLAG(ColorEditFlags::kHDR,              ImGuiColorEditFlags_HDR);
	FUCK_ASS_FLAG(ColorEditFlags::kDisplayRGB,       ImGuiColorEditFlags_DisplayRGB);
	FUCK_ASS_FLAG(ColorEditFlags::kDisplayHSV,       ImGuiColorEditFlags_DisplayHSV);
	FUCK_ASS_FLAG(ColorEditFlags::kDisplayHex,       ImGuiColorEditFlags_DisplayHex);
	FUCK_ASS_FLAG(ColorEditFlags::kUint8,            ImGuiColorEditFlags_Uint8);
	FUCK_ASS_FLAG(ColorEditFlags::kFloat,            ImGuiColorEditFlags_Float);
	FUCK_ASS_FLAG(ColorEditFlags::kPickerHueBar,     ImGuiColorEditFlags_PickerHueBar);
	FUCK_ASS_FLAG(ColorEditFlags::kPickerHueWheel,   ImGuiColorEditFlags_PickerHueWheel);
	FUCK_ASS_FLAG(ColorEditFlags::kInputRGB,         ImGuiColorEditFlags_InputRGB);
	FUCK_ASS_FLAG(ColorEditFlags::kInputHSV,         ImGuiColorEditFlags_InputHSV);

#undef FUCK_ASS_FLAG
}
