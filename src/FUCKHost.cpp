#include "FUCKHost.h"
#include "FUCKMan.h"
#include "ImGui/Graphics.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Overlays.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"
#include "System/Input.h"
#include "System/Utils.h"

namespace FUCK::Host
{
	// ==========================================
	// Framework Registration
	// ==========================================
	static void RegisterTool_Impl(ITool* t) { FUCKMan::GetSingleton()->RegisterTool(t); }
	static void RegisterWindow_Impl(IWindow* w) { FUCKMan::GetSingleton()->RegisterWindow(w); }

	// ==========================================
	// Display & Rendering
	// ==========================================
	static float GetResolutionScale_Impl() { return ImGui::Renderer::GetResolutionScale(); }
	static void GetDisplaySize_Impl(float* x, float* y)
	{
		auto size = ImGui::GetIO().DisplaySize;
		if (x)
			*x = size.x;
		if (y)
			*y = size.y;
	}
	static ImFont* GetFont_Impl(FUCK_Font f)
	{
		auto mgr = IconFont::Manager::GetSingleton();
		return (f == FUCK_Font::kLarge) ? mgr->GetLargeFont() : mgr->GetRegularFont();
	}
	static void PushFont_Impl(ImFont* f) { ImGui::PushFont(f); }
	static void PopFont_Impl() { ImGui::PopFont(); }
	static void SuspendRendering_Impl(bool suspend) { FUCKMan::GetSingleton()->SuspendRendering(suspend); }
	static void SetMenuOpen_Impl(bool open) { open ? FUCKMan::GetSingleton()->Open() : FUCKMan::GetSingleton()->Close(); }

	// ==========================================
	// Style & Stack
	// ==========================================
	static void PushStyleColor_Impl(ImGuiCol idx, const ImVec4& col) { ImGui::PushStyleColor(idx, col); }
	static void PopStyleColor_Impl(int count) { ImGui::PopStyleColor(count); }
	static void PushStyleVar_Impl(ImGuiStyleVar idx, float val) { ImGui::PushStyleVar(idx, val); }
	static void PushStyleVarVec_Impl(ImGuiStyleVar idx, const ImVec2& val) { ImGui::PushStyleVar(idx, val); }
	static void PopStyleVar_Impl(int count) { ImGui::PopStyleVar(count); }
	static float GetStyleVar_Impl(ImGuiStyleVar idx)
	{
		auto& style = ImGui::GetStyle();
		switch (idx) {
		case ImGuiStyleVar_Alpha:
			return style.Alpha;
		case ImGuiStyleVar_DisabledAlpha:
			return style.DisabledAlpha;
		case ImGuiStyleVar_WindowRounding:
			return style.WindowRounding;
		case ImGuiStyleVar_WindowBorderSize:
			return style.WindowBorderSize;
		case ImGuiStyleVar_ChildRounding:
			return style.ChildRounding;
		case ImGuiStyleVar_ChildBorderSize:
			return style.ChildBorderSize;
		case ImGuiStyleVar_PopupRounding:
			return style.PopupRounding;
		case ImGuiStyleVar_PopupBorderSize:
			return style.PopupBorderSize;
		case ImGuiStyleVar_FrameRounding:
			return style.FrameRounding;
		case ImGuiStyleVar_FrameBorderSize:
			return style.FrameBorderSize;
		case ImGuiStyleVar_IndentSpacing:
			return style.IndentSpacing;
		case ImGuiStyleVar_ScrollbarSize:
			return style.ScrollbarSize;
		case ImGuiStyleVar_ScrollbarRounding:
			return style.ScrollbarRounding;
		case ImGuiStyleVar_GrabMinSize:
			return style.GrabMinSize;
		case ImGuiStyleVar_GrabRounding:
			return style.GrabRounding;
		case ImGuiStyleVar_TabRounding:
			return style.TabRounding;
		default:
			return 0.0f;
		}
	}
	static void GetStyleVarVec_Impl(ImGuiStyleVar idx, float* x, float* y)
	{
		auto& style = ImGui::GetStyle();
		ImVec2 val(0, 0);
		switch (idx) {
		case ImGuiStyleVar_WindowPadding:
			val = style.WindowPadding;
			break;
		case ImGuiStyleVar_WindowMinSize:
			val = style.WindowMinSize;
			break;
		case ImGuiStyleVar_WindowTitleAlign:
			val = style.WindowTitleAlign;
			break;
		case ImGuiStyleVar_FramePadding:
			val = style.FramePadding;
			break;
		case ImGuiStyleVar_ItemSpacing:
			val = style.ItemSpacing;
			break;
		case ImGuiStyleVar_ItemInnerSpacing:
			val = style.ItemInnerSpacing;
			break;
		case ImGuiStyleVar_CellPadding:
			val = style.CellPadding;
			break;
		case ImGuiStyleVar_ButtonTextAlign:
			val = style.ButtonTextAlign;
			break;
		case ImGuiStyleVar_SelectableTextAlign:
			val = style.SelectableTextAlign;
			break;
		case ImGuiStyleVar_SeparatorTextAlign:
			val = style.SeparatorTextAlign;
			break;
		case ImGuiStyleVar_SeparatorTextPadding:
			val = style.SeparatorTextPadding;
			break;
		default:
			break;
		}
		if (x)
			*x = val.x;
		if (y)
			*y = val.y;
	}
	static void GetStyleColorVec4_Impl(ImGuiCol idx, float* r, float* g, float* b, float* a)
	{
		const auto& col = ImGui::GetStyle().Colors[idx];
		if (r)
			*r = col.x;
		if (g)
			*g = col.y;
		if (b)
			*b = col.z;
		if (a)
			*a = col.w;
	}

	// ==========================================
	// Layout & Cursor
	// ==========================================
	static void SetCursorPosX_Impl(float x) { ImGui::SetCursorPosX(x); }
	static void SetCursorPosY_Impl(float y) { ImGui::SetCursorPosY(y); }
	static void GetCursorPos_Impl(float* x, float* y)
	{
		ImVec2 p = ImGui::GetCursorPos();
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}
	static void SetCursorPos_Impl(float x, float y) { ImGui::SetCursorPos({ x, y }); }
	static void GetCursorScreenPos_Impl(float* x, float* y)
	{
		ImVec2 p = ImGui::GetCursorScreenPos();
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}
	static void SetCursorScreenPos_Impl(float x, float y) { ImGui::SetCursorScreenPos({ x, y }); }
	static void GetContentRegionAvail_Impl(float* x, float* y)
	{
		ImVec2 p = ImGui::GetContentRegionAvail();
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}
	static void GetItemRectMin_Impl(float* x, float* y)
	{
		ImVec2 p = ImGui::GetItemRectMin();
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}
	static void GetItemRectMax_Impl(float* x, float* y)
	{
		ImVec2 p = ImGui::GetItemRectMax();
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}
	static float CalcItemWidth_Impl() { return ImGui::CalcItemWidth(); }
	static void CalcTextSize_Impl(const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width, float* x, float* y)
	{
		ImVec2 s = ImGui::CalcTextSize(text, text_end, hide_text_after_double_hash, wrap_width);
		if (x)
			*x = s.x;
		if (y)
			*y = s.y;
	}
	static void SetNextItemWidth_Impl(float w) { ImGui::SetNextItemWidth(w); }
	static void Dummy_Impl(float w, float h) { ImGui::Dummy(ImVec2(w, h)); }
	static void Spacing_Impl() { ImGui::Spacing(); }
	static void Separator_Impl() { ImGui::Separator(); }
	static void SeparatorThick_Impl() { ImGui::SeparatorThick(); }
	static void SeparatorText_Impl(const char* label) { ImGui::SeparatorText(label); }

	static float GetTextLineHeight_Impl() { return ImGui::GetTextLineHeight(); }
	static float GetTextLineHeightWithSpacing_Impl() { return ImGui::GetTextLineHeightWithSpacing(); }
	static float GetFrameHeight_Impl() { return ImGui::GetFrameHeight(); }
	static float GetFrameHeightWithSpacing_Impl() { return ImGui::GetFrameHeightWithSpacing(); }

	// ==========================================
	// IO Helpers
	// ==========================================
	static float GetDeltaTime_Impl() { return ImGui::GetIO().DeltaTime; }
	static void GetMouseDelta_Impl(float* x, float* y)
	{
		auto d = ImGui::GetIO().MouseDelta;
		if (x)
			*x = d.x;
		if (y)
			*y = d.y;
	}
	static void GetMousePos_Impl(float* x, float* y)
	{
		auto p = ImGui::GetIO().MousePos;
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}

	// ==========================================
	// Translation & Utils
	// ==========================================
	static void LoadTranslation_Impl(const char* n) { Translation::Manager::GetSingleton()->LoadCustomTranslation(n); }
	static const char* GetTranslation_Impl(const char* k) { return Translation::Manager::GetSingleton()->GetTranslation(k); }
	static void SanitizePath_Impl(char* dest, const char* source, size_t size) { Utils::SanitizePath(dest, source, size); }
	static void PushItemFlag_Impl(ItemFlags flag, bool enabled) { ImGui::PushItemFlag(static_cast<ImGuiItemFlags>(flag), enabled); }
	static void PopItemFlag_Impl() { ImGui::PopItemFlag(); }
	static void HelpMarker_Impl(const char* desc)
	{
		ImGui::TextDisabled("(?)");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
			const float offset = 32.0f * ImGui::Renderer::GetResolutionScale();
			ImGui::SetNextWindowPos(ImGui::GetMousePos() + ImVec2(offset, offset), ImGuiCond_Always);
			if (ImGui::BeginTooltip()) {
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
				ImGui::TextUnformatted(desc);
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}
	}
	static void PushID_Str_Impl(const char* str_id) { ImGui::PushID(str_id); }
	static void PushID_Int_Impl(int int_id) { ImGui::PushID(int_id); }
	static void PopID_Impl() { ImGui::PopID(); }

	// ==========================================
	// Textures & Icons
	// ==========================================
	static ID3D11ShaderResourceView* GetSRV(void* tex)
	{
		if (!tex)
			return nullptr;
		return static_cast<ImGui::Texture*>(tex)->GetSRView();
	}
	static void* LoadImage_Impl(const char* path, bool resize)
	{
		auto* tex = new ImGui::Texture(SKSE::stl::utf8_to_utf16(path).value_or(L""));
		if (tex->Load(resize))
			return tex;
		delete tex;
		return nullptr;
	}
	static void ReleaseImage_Impl(void* tex)
	{
		if (tex)
			delete static_cast<ImGui::Texture*>(tex);
	}
	static void GetImageInfo_Impl(void* tex, float* w, float* h)
	{
		if (tex) {
			auto* t = static_cast<ImGui::Texture*>(tex);
			if (w)
				*w = t->size.x;
			if (h)
				*h = t->size.y;
		}
	}
	static void* GetIconForKey_Impl(std::uint32_t k)
	{
		auto icon = IconFont::Manager::GetSingleton()->GetIcon(k);
		return icon ? (void*)icon : nullptr;
	}
	static void GetIconSizeForKey_Impl(std::uint32_t k, float* w, float* h)
	{
		auto icon = IconFont::Manager::GetSingleton()->GetIcon(k);
		if (icon) {
			if (w)
				*w = icon->size.x;
			if (h)
				*h = icon->size.y;
		} else {
			if (w)
				*w = 0.0f;
			if (h)
				*h = 0.0f;
		}
	}
	static void Spinner_Impl(const char* label, float radius, float thickness, const ImVec4& color)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return;
		ImGuiContext& g = *GImGui;
		const ImGuiID id = window->GetID(label);
		const ImVec2 pos = window->DC.CursorPos;
		const ImVec2 size((radius) * 2, (radius + g.Style.FramePadding.y) * 2);
		const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
		ImGui::ItemSize(bb, g.Style.FramePadding.y);
		if (!ImGui::ItemAdd(bb, id))
			return;
		window->DrawList->PathClear();
		int num_segments = 30;
		int start = static_cast<int>(ImAbs(ImSin(static_cast<float>(g.Time) * 1.8f) * (num_segments - 5)));
		const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
		const float a_max = IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;
		const ImVec2 centre = ImVec2(pos.x + radius, pos.y + radius + g.Style.FramePadding.y);
		for (int i = 0; i < num_segments; i++) {
			const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
			window->DrawList->PathLineTo(ImVec2(centre.x + ImCos(a + static_cast<float>(g.Time) * 8.0f) * radius,
				centre.y + ImSin(a + static_cast<float>(g.Time) * 8.0f) * radius));
		}
		window->DrawList->PathStroke(ImGui::ColorConvertFloat4ToU32(color), false, thickness);
	}
	static void DrawOverlay_Impl(FUCK_Overlay type, float thickness, ImU32 color, float paramA, float paramB, float paramC, float paramD) { ImGui::Overlays::Draw(type, thickness, color, paramA, paramB, paramC, paramD); }

	// ==========================================
	// Game Control
	// ==========================================
	static void SetGameTimeFrozen_Impl(bool frozen)
	{
		if (auto main = RE::Main::GetSingleton())
			main->freezeTime = frozen;
	}
	static void SetAutoVanityBlocked(bool blocked) { FUCKMan::GetSingleton()->SetVanityBlocked(blocked); }
	static void SetHardPause_Impl(bool paused) { FUCKMan::GetSingleton()->SetManualHardPause(paused); }
	static void SetSoftPause_Impl(bool paused) { FUCKMan::GetSingleton()->SetManualSoftPause(paused); }
	static void ForceCursor_Impl(bool force) { FUCKMan::GetSingleton()->SetForceCursor(force); }

	// ==========================================
	// Input
	// ==========================================
	static bool IsInputPressed_Impl(const void* evt, std::uint32_t key) { return Input::Manager::GetSingleton()->IsInputPressed(static_cast<const RE::InputEvent* const*>(evt), key); }
	static bool IsInputDown_Impl(std::uint32_t key) { return Input::Manager::GetSingleton()->IsInputDown(key); }
	static float GetAnalogInput_Impl(std::uint32_t key) { return Input::Manager::GetSingleton()->GetAnalogInput(key); }
	static bool IsModifierPressed_Impl(Modifier m) { return Input::Manager::GetSingleton()->IsModifierPressed(m); }
	static int GetInputDevice_Impl()
	{
		auto device = Input::Manager::GetSingleton()->GetInputDevice();
		if (device == Input::DEVICE::kGamepadDirectX || device == Input::DEVICE::kGamepadOrbis)
			return (int)FUCK::InputDevice::kGamepad;
		return (int)FUCK::InputDevice::kMouseKeyboard;
	}
	static const char* GetKeyName_Impl(std::uint32_t key) { return Input::Manager::GetSingleton()->GetKeyName(key); }
	static bool IsGamepadKey_Impl(std::uint32_t k) { return k >= static_cast<std::uint32_t>(SKSE::InputMap::kMacro_GamepadOffset); }
	static bool IsBinding_Impl() { return Input::Manager::GetSingleton()->IsBinding(); }
	static void AbortBinding_Impl() { Input::Manager::GetSingleton()->AbortBinding(); }
	static void StartBinding_Impl(std::uint32_t k, std::int32_t m1, std::int32_t m2) { Input::Manager::GetSingleton()->StartBinding(k, m1, m2); }
	static BindResult UpdateBinding_Impl(const void* evt, std::uint32_t* k, std::int32_t* m1, std::int32_t* m2) { return Input::Manager::GetSingleton()->UpdateBinding(static_cast<const RE::InputEvent* const*>(evt), k, m1, m2); }
	static BindResult GetInputBind_Impl(const void* evt, std::uint32_t* k, std::int32_t* m1, std::int32_t* m2) { return Input::Manager::GetSingleton()->GetInputBind(static_cast<const RE::InputEvent* const*>(evt), k, m1, m2); }

	// ==========================================
	// Interaction
	// ==========================================
	static bool IsItemHovered_Impl(int flags) { return ImGui::IsItemHovered(flags); }
	static bool IsItemClicked_Impl(int btn) { return ImGui::IsItemClicked(btn); }
	static bool IsItemActive_Impl() { return ImGui::IsItemActive(); }
	static bool IsAnyItemActive_Impl() { return ImGui::IsAnyItemActive(); }
	static bool IsWindowFocused_Impl(int flags) { return ImGui::IsWindowFocused(flags); }
	static void SetKeyboardFocusHere_Impl(int offset) { ImGui::SetKeyboardFocusHere(offset); }
	static void SetItemDefaultFocus_Impl() { ImGui::SetItemDefaultFocus(); }

	// ==========================================
	// Drawing
	// ==========================================
	static void DrawRect_Impl(const ImVec2& min, const ImVec2& max, const ImVec4& col, float r, float t) { ImGui::GetWindowDrawList()->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(col), r, 0, t); }
	static void DrawRectFilled_Impl(const ImVec2& min, const ImVec2& max, const ImVec4& col, float r) { ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(col), r); }
	static void DrawImage_Impl(void* tex, const ImVec2& s, const ImVec2& u0, const ImVec2& u1, const ImVec4& tint)
	{
		if (auto srv = GetSRV(tex))
			ImGui::Image((ImTextureID)srv, s, u0, u1, tint, ImVec4(0, 0, 0, 0));
	}
	static void AddImage_Impl(void* tex, const ImVec2& min, const ImVec2& max, const ImVec2& u0, const ImVec2& u1, const ImVec4& col)
	{
		if (auto srv = GetSRV(tex))
			ImGui::GetWindowDrawList()->AddImage((ImTextureID)srv, min, max, u0, u1, ImGui::ColorConvertFloat4ToU32(col));
	}
	static void DrawBackgroundImage_Impl(void* tex, float alpha)
	{
		if (auto srv = GetSRV(tex))
			ImGui::GetBackgroundDrawList()->AddImage((ImTextureID)srv, { 0, 0 }, ImGui::GetIO().DisplaySize, { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, (int)(alpha * 255)));
	}
	static void DrawBackgroundLine_Impl(float x1, float y1, float x2, float y2, unsigned int col, float t) { ImGui::GetBackgroundDrawList()->AddLine({ x1, y1 }, { x2, y2 }, col, t); }
	static void DrawBackgroundRect_Impl(const ImVec2& min, const ImVec2& max, ImU32 col, float thickness) { ImGui::GetBackgroundDrawList()->AddRect(min, max, col, 0.0f, 0, thickness); }

	// ==========================================
	// Windows & Containers
	// ==========================================
	static void SetNextWindowPos_Impl(float x, float y, int c, float px, float py) { ImGui::SetNextWindowPos({ x, y }, c, { px, py }); }
	static void SetNextWindowSize_Impl(float x, float y, int c) { ImGui::SetNextWindowSize({ x, y }, c); }
	static void GetWindowPos_Impl(float* x, float* y)
	{
		ImVec2 p = ImGui::GetWindowPos();
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}
	static void GetWindowSize_Impl(float* x, float* y)
	{
		ImVec2 s = ImGui::GetWindowSize();
		if (x)
			*x = s.x;
		if (y)
			*y = s.y;
	}
	static bool BeginWindow_Impl(const char* n, bool* o, int f) { return ImGui::Begin(n, o, f); }
	static void EndWindow_Impl() { ImGui::End(); }
	static void ExtendWindowPastBorder_Impl() { ImGui::ExtendWindowPastBorder(); }
	static void BeginChild_Impl(const char* id, float w, float h, bool border, int flags) { ImGui::BeginChild(id, ImVec2(w, h), border, flags); }
	static void EndChild_Impl() { ImGui::EndChild(); }
	static bool TreeNode_Impl(const char* label) { return ImGui::TreeNodeIcon(label, 0); }
	static void TreePop_Impl() { ImGui::TreePop(); }

	// ==========================================
	// Widgets
	// ==========================================
	static bool Button_Impl(const char* label) { return ImGui::OutlineButton(label); }
	static bool Checkbox_Impl(const char* label, bool* v, bool alignFar, bool labelLeft) { return ImGui::CheckBox(label, v, alignFar, labelLeft); }
	static bool Hotkey_Impl(const char* label, std::uint32_t key, std::int32_t m1, std::int32_t m2, bool alignFar, bool labelLeft, bool flashing) { return ImGui::Hotkey(label, key, m1, m2, alignFar, labelLeft, flashing); }
	static bool ToggleButton_Impl(const char* label, bool* v, bool alignFar, bool labelLeft) { return ImGui::ToggleButton(label, v, alignFar, labelLeft); }
	static ImGuiTableSortSpecs* GetTableSortSpecs_Impl() { return ImGui::TableGetSortSpecs(); }
	static bool InputText_Impl(const char* label, char* buf, size_t buf_size, int flags) { return ImGui::InputTextStyled(label, buf, buf_size, flags); }
	static bool ColorEdit3_Impl(const char* label, float col[3], int flags) { return ImGui::ColorEdit3Styled(label, col, flags); }
	static bool SliderFloat_Impl(const char* label, float* v, float min, float max, const char* fmt) { return ImGui::Slider(label, v, min, max, fmt); }
	static bool SliderInt_Impl(const char* label, int* v, int min, int max, const char* fmt) { return ImGui::Slider(label, v, min, max, fmt); }
	static bool DragFloat_Impl(const char* label, float* v, float s, float min, float max, const char* fmt) { return ImGui::DragOnHover(label, v, s, min, max, fmt); }
	static bool DragInt_Impl(const char* label, int* v, float s, int min, int max, const char* fmt) { return ImGui::DragOnHover(label, v, s, min, max, fmt); }
	static bool Combo_Impl(const char* label, int* current_item, const char* const* items, int items_count) { return ImGui::ComboStyled(label, current_item, items, items_count); }
	static bool ComboWithFilter_Impl(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height)
	{
		// Internal conversion from raw C-array to std::vector for ImGui::ComboWithFilter implementation
		std::vector<std::string> vecItems;
		vecItems.reserve(items_count);
		for (int i = 0; i < items_count; ++i) vecItems.emplace_back(items[i]);
		return ImGui::ComboWithFilter(label, current_item, vecItems, popup_max_height);
	}
	static bool ComboForm_Impl(const char* label, std::uint32_t* id, std::uint8_t t) { return ImGui::ComboForm(label, (RE::FormID*)id, (RE::FormType)t); }
	static bool Selectable_Impl(const char* label, bool selected, int flags, const ImVec2& size) { return ImGui::SelectableStyled(label, selected, flags, size); }
	static void Header_Impl(const char* label) { ImGui::Header(label); }
	static void LeftLabel_Impl(const char* label) { ImGui::LeftAlignedTextImpl(label); }
	static void TextColored_Impl(const ImVec4& col, const char* text) { ImGui::TextColored(col, "%s", text); }
	static void TextDisabled_Impl(const char* text) { ImGui::TextDisabled("%s", text); }
	static void Text_Impl(const char* text) { ImGui::TextUnformatted(text); }
	static void TextWrapped_Impl(const char* text) { ImGui::TextWrapped("%s", text); }
	static void TextUnformatted_Impl(const char* text, const char* text_end) { ImGui::TextUnformatted(text, text_end); }
	static void TextColoredWrapped_Impl(const ImVec4& col, const char* text) { ImGui::TextColoredWrapped(col, "%s", text); }
	static void CenteredText_Impl(const char* label, bool v) { ImGui::CenteredText(label, v); }
	static void CenteredTextWithArrows_Impl(const char* label, const char* text, bool* h, bool* l, bool* r)
	{
		auto [bH, bL, bR] = ImGui::CenteredTextWithArrows(label, text);
		if (h)
			*h = bH;
		if (l)
			*l = bL;
		if (r)
			*r = bR;
	}
	static bool ButtonIconWithLabel_Impl(const char* label, void* tex, float x, float y, bool alignFar, bool labelLeft)
	{
		ImVec2 size(x, y);
		if (auto srv = GetSRV(tex))
			return ImGui::ButtonIconWithLabelStyled(label, (void*)srv, size, alignFar, labelLeft);
		return false;
	}
	static bool ImageButton_Impl(const char* str_id, void* user_texture_id, float x, float y, const ImVec4* tint)
	{
		if (auto srv = GetSRV(user_texture_id))
			return ImGui::ImageButton(str_id, (ImTextureID)srv, ImVec2(x, y), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tint ? *tint : ImVec4(1, 1, 1, 1));
		return false;
	}
	static void Stepper_Impl(const char* label, const char* text, bool* outLeft, bool* outRight) { ImGui::Stepper(label, text, outLeft, outRight); }

	static bool BeginTabBar_Impl(const char* s, int f) { return ImGui::BeginTabBar(s, f); }
	static void EndTabBar_Impl() { ImGui::EndTabBar(); }
	static bool BeginTabItem_Impl(const char* label, int flags) { return ImGui::BeginTabItemEx(label, static_cast<ImGuiTabItemFlags>(flags)); }
	static void EndTabItem_Impl() { ImGui::EndTabItem(); }
	static bool BeginTable_Impl(const char* id, int col, int f, const ImVec2& os, float iw) { return ImGui::BeginTable(id, col, f, os, iw); }
	static void EndTable_Impl() { ImGui::EndTable(); }
	static void TableSetupColumn_Impl(const char* label, int flags, float init_width, std::uint32_t user_id) { ImGui::TableSetupColumn(label, flags, init_width, user_id); }
	static void TableNextRow_Impl(int f, float h) { ImGui::TableNextRow(f, h); }
	static bool TableNextColumn_Impl() { return ImGui::TableNextColumn(); }
	static void TableHeadersRow_Impl() { ImGui::TableHeadersRow(); }
	static void Columns_Impl(int count, const char* id, bool border) { ImGui::Columns(count, id, border); }
	static void NextColumn_Impl() { ImGui::NextColumn(); }
	static void SameLine_Impl(float offset, float spacing) { ImGui::SameLine(offset, spacing); }
	static void Indent_Impl(float w) { ImGui::Indent(w); }
	static void Unindent_Impl(float w) { ImGui::Unindent(w); }
	static bool CollapsingHeader_Impl(const char* label, int flags) { return ImGui::CollapsingHeaderIcon(label, flags); }
	static void BeginGroup_Impl() { ImGui::BeginGroup(); }
	static void EndGroup_Impl() { ImGui::EndGroup(); }
	static void BeginDisabled_Impl(bool disabled) { ImGui::BeginDisabled(disabled); }
	static void EndDisabled_Impl() { ImGui::EndDisabled(); }
	static bool IsWidgetFocused_Impl(const char* label) { return ImGui::IsWidgetFocused(label); }
	static void SetTooltip_Impl(const char* fmt)
	{
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
			const float offset = 32.0f * ImGui::Renderer::GetResolutionScale();
			ImGui::SetNextWindowPos(ImGui::GetMousePos() + ImVec2(offset, offset), ImGuiCond_Always);
			if (ImGui::BeginTooltip()) {
				ImGui::TextUnformatted(fmt);
				ImGui::EndTooltip();
			}
		}
	}

	FUCK_Interface* CreateInterface()
	{
		static FUCK_Interface api = {
			.version = FUCK_API_VERSION,
			.RegisterTool = RegisterTool_Impl,
			.RegisterWindow = RegisterWindow_Impl,
			.GetResolutionScale = GetResolutionScale_Impl,
			.GetDisplaySize = GetDisplaySize_Impl,
			.GetFont = GetFont_Impl,
			.PushFont = PushFont_Impl,
			.PopFont = PopFont_Impl,
			.SuspendRendering = SuspendRendering_Impl,
			.SetMenuOpen = SetMenuOpen_Impl,
			.GetDeltaTime = GetDeltaTime_Impl,
			.GetMouseDelta = GetMouseDelta_Impl,
			.GetMousePos = GetMousePos_Impl,
			.PushStyleColor = PushStyleColor_Impl,
			.PopStyleColor = PopStyleColor_Impl,
			.PushStyleVar = PushStyleVar_Impl,
			.PushStyleVarVec = PushStyleVarVec_Impl,
			.PopStyleVar = PopStyleVar_Impl,
			.GetStyleVar = GetStyleVar_Impl,
			.GetStyleVarVec = GetStyleVarVec_Impl,
			.GetStyleColorVec4 = GetStyleColorVec4_Impl,
			.SetCursorPosX = SetCursorPosX_Impl,
			.SetCursorPosY = SetCursorPosY_Impl,
			.GetCursorPos = GetCursorPos_Impl,
			.SetCursorPos = SetCursorPos_Impl,
			.GetCursorScreenPos = GetCursorScreenPos_Impl,
			.SetCursorScreenPos = SetCursorScreenPos_Impl,
			.GetContentRegionAvail = GetContentRegionAvail_Impl,
			.CalcItemWidth = CalcItemWidth_Impl,
			.CalcTextSize = CalcTextSize_Impl,
			.GetItemRectMin = GetItemRectMin_Impl,
			.GetItemRectMax = GetItemRectMax_Impl,
			.SetNextItemWidth = SetNextItemWidth_Impl,
			.Dummy = Dummy_Impl,
			.Spacing = Spacing_Impl,
			.Separator = Separator_Impl,
			.SeparatorThick = SeparatorThick_Impl,
			.SeparatorText = SeparatorText_Impl,
			.GetTextLineHeight = GetTextLineHeight_Impl,
			.GetTextLineHeightWithSpacing = GetTextLineHeightWithSpacing_Impl,
			.GetFrameHeight = GetFrameHeight_Impl,
			.GetFrameHeightWithSpacing = GetFrameHeightWithSpacing_Impl,
			.LoadTranslation = LoadTranslation_Impl,
			.GetTranslation = GetTranslation_Impl,
			.SanitizePath = SanitizePath_Impl,
			.PushItemFlag = PushItemFlag_Impl,
			.PopItemFlag = PopItemFlag_Impl,
			.HelpMarker = HelpMarker_Impl,
			.PushID_Str = PushID_Str_Impl,
			.PushID_Int = PushID_Int_Impl,
			.PopID = PopID_Impl,
			.LoadImage = LoadImage_Impl,
			.ReleaseImage = ReleaseImage_Impl,
			.GetImageInfo = GetImageInfo_Impl,
			.GetIconForKey = GetIconForKey_Impl,
			.GetIconSizeForKey = GetIconSizeForKey_Impl,
			.Spinner = Spinner_Impl,
			.DrawOverlay = DrawOverlay_Impl,
			.SetGameTimeFrozen = SetGameTimeFrozen_Impl,
			.SetAutoVanityBlocked = SetAutoVanityBlocked,
			.SetHardPause = SetHardPause_Impl,
			.SetSoftPause = SetSoftPause_Impl,
			.ForceCursor = ForceCursor_Impl,
			.IsInputPressed = IsInputPressed_Impl,
			.IsInputDown = IsInputDown_Impl,
			.GetAnalogInput = GetAnalogInput_Impl,
			.IsModifierPressed = IsModifierPressed_Impl,
			.GetInputDevice = GetInputDevice_Impl,
			.GetKeyName = GetKeyName_Impl,
			.IsGamepadKey = IsGamepadKey_Impl,
			.IsBinding = IsBinding_Impl,
			.AbortBinding = AbortBinding_Impl,
			.StartBinding = StartBinding_Impl,
			.UpdateBinding = UpdateBinding_Impl,
			.GetInputBind = GetInputBind_Impl,
			.IsItemHovered = IsItemHovered_Impl,
			.IsItemClicked = IsItemClicked_Impl,
			.IsItemActive = IsItemActive_Impl,
			.IsAnyItemActive = IsAnyItemActive_Impl,
			.IsWindowFocused = IsWindowFocused_Impl,
			.SetKeyboardFocusHere = SetKeyboardFocusHere_Impl,
			.SetItemDefaultFocus = SetItemDefaultFocus_Impl,
			.DrawRect = DrawRect_Impl,
			.DrawRectFilled = DrawRectFilled_Impl,
			.DrawImage = DrawImage_Impl,
			.AddImage = AddImage_Impl,
			.DrawBackgroundImage = DrawBackgroundImage_Impl,
			.DrawBackgroundLine = DrawBackgroundLine_Impl,
			.DrawBackgroundRect = DrawBackgroundRect_Impl,
			.SetNextWindowPos = SetNextWindowPos_Impl,
			.SetNextWindowSize = SetNextWindowSize_Impl,
			.GetWindowPos = GetWindowPos_Impl,
			.GetWindowSize = GetWindowSize_Impl,
			.BeginWindow = BeginWindow_Impl,
			.EndWindow = EndWindow_Impl,
			.ExtendWindowPastBorder = ExtendWindowPastBorder_Impl,
			.BeginChild = BeginChild_Impl,
			.EndChild = EndChild_Impl,
			.TreeNode = TreeNode_Impl,
			.TreePop = TreePop_Impl,
			.Button = Button_Impl,
			.Checkbox = Checkbox_Impl,
			.Hotkey = Hotkey_Impl,
			.ToggleButton = ToggleButton_Impl,
			.InputText = InputText_Impl,
			.ColorEdit3 = ColorEdit3_Impl,
			.SliderFloat = SliderFloat_Impl,
			.SliderInt = SliderInt_Impl,
			.DragFloat = DragFloat_Impl,
			.DragInt = DragInt_Impl,
			.Combo = Combo_Impl,
			.ComboWithFilter = ComboWithFilter_Impl,
			.ComboForm = ComboForm_Impl,
			.GetTableSortSpecs = GetTableSortSpecs_Impl,
			.Selectable = Selectable_Impl,
			.Header = Header_Impl,
			.LeftLabel = LeftLabel_Impl,
			.TextColored = TextColored_Impl,
			.TextColoredWrapped = TextColoredWrapped_Impl,
			.TextDisabled = TextDisabled_Impl,
			.CenteredText = CenteredText_Impl,
			.CenteredTextWithArrows = CenteredTextWithArrows_Impl,
			.ButtonIconWithLabel = ButtonIconWithLabel_Impl,
			.ImageButton = ImageButton_Impl,
			.Stepper = Stepper_Impl,
			.BeginTabBar = BeginTabBar_Impl,
			.EndTabBar = EndTabBar_Impl,
			.BeginTabItem = BeginTabItem_Impl,
			.EndTabItem = EndTabItem_Impl,
			.BeginTable = BeginTable_Impl,
			.EndTable = EndTable_Impl,
			.TableSetupColumn = TableSetupColumn_Impl,
			.TableNextRow = TableNextRow_Impl,
			.TableNextColumn = TableNextColumn_Impl,
			.TableHeadersRow = TableHeadersRow_Impl,
			.Columns = Columns_Impl,
			.NextColumn = NextColumn_Impl,
			.SameLine = SameLine_Impl,
			.CollapsingHeader = CollapsingHeader_Impl,
			.BeginGroup = BeginGroup_Impl,
			.EndGroup = EndGroup_Impl,
			.BeginDisabled = BeginDisabled_Impl,
			.EndDisabled = EndDisabled_Impl,
			.IsWidgetFocused = IsWidgetFocused_Impl,
			.SetTooltip = SetTooltip_Impl,
			.Indent = Indent_Impl,
			.Unindent = Unindent_Impl,
			.Text = Text_Impl,
			.TextWrapped = TextWrapped_Impl,
			.TextUnformatted = TextUnformatted_Impl
		};
		return &api;
	}
}
