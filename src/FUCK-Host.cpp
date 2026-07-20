#include "FUCK-Host.h"
#include "FUCK-Man.h"

#include "ImGui/Graphics.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Overlays.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

#include "System/Input.h"
#include "System/Hotkeys.h"
#include "System/Settings.h"
#include "System/Utils.h"

namespace FUCK::Host
{
	// ==================================================
	// Registration
	// ==================================================
	static void RegisterTool_Impl(ITool* t) { FUCKMan::GetSingleton()->RegisterTool(t); }
	static void RegisterWindow_Impl(IWindow* w) { FUCKMan::GetSingleton()->RegisterWindow(w); }
	static void UnregisterWindow_Impl(IWindow* w) { FUCKMan::GetSingleton()->UnregisterWindow(w); }

	// ==================================================
	// Display
	// ==================================================
	static float GetResolutionScale_Impl() { return ImGui::Renderer::GetResolutionScale(); }
	static float GetGlobalScale_Impl() { return ImGui::Renderer::GetResolutionScale() * FUCKMan::GetSingleton()->GetActiveScale(); }
	static float GetUserScale_Impl() { return FUCKMan::GetSingleton()->GetActiveScale(); }

	static void GetDisplaySize_Impl(float* x, float* y)
	{
		auto size = ImGui::GetIO().DisplaySize;
		if (x)
			*x = size.x;
		if (y)
			*y = size.y;
	}
	static void TranslateScaleformToScreen_Impl(float stageX, float stageY, float* screenX, float* screenY)
	{
		ImVec2 pos = ImGui::TranslateScaleformToScreen(stageX, stageY);
		if (screenX)
			*screenX = pos.x;
		if (screenY)
			*screenY = pos.y;
	}
	static ImFont* GetFont_Impl(FUCK::Font f)
	{
		auto mgr = IconFont::Manager::GetSingleton();
		return (f == FUCK::Font::kLarge) ? mgr->GetLargeFont() : mgr->GetRegularFont();
	}
	static void PushFont_Impl(ImFont* f, float size)
	{
		// Use the combined Resolution + User scale
		float activeScale = GetGlobalScale_Impl();

		if (size <= 0.0f) {
			float defaultSize = f ? f->LegacySize : ImGui::GetStyle().FontSizeBase;
			ImGui::PushFont(f, defaultSize * activeScale);
		} else {
			// If the user explicitly defines a size via FUCK::PushFont,
			// they should use FUCK::UIScale() on their end. Pass it directly.
			ImGui::PushFont(f, size);
		}
	}
	static void PopFont_Impl() { ImGui::PopFont(); }
	static void SetWindowFontScale_Impl(float scale) { ImGui::SetWindowFontScale(scale); }

	static void SuspendRendering_Impl(bool suspend) { FUCKMan::GetSingleton()->SuspendRendering(suspend); }
	static void SetMenuOpen_Impl(bool open) { open ? FUCKMan::GetSingleton()->Open() : FUCKMan::GetSingleton()->Close(); }
	static bool IsMenuOpen_Impl() { return FUCKMan::GetSingleton()->IsOpen(); }

	// ==================================================
	// IO
	// ==================================================
	static float  GetDeltaTime_Impl() { return ImGui::GetIO().DeltaTime; }
	static double GetTime_Impl() { return ImGui::GetTime(); }

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
	static float GetMouseWheel_Impl() { return ImGui::GetIO().MouseWheel; }

	// ==================================================
	// Styling
	// ==================================================
	static void  PushStyleColor_Impl(ImGuiCol idx, const ImVec4& col) { ImGui::PushStyleColor(idx, col); }
	static void  PopStyleColor_Impl(int count) { ImGui::PopStyleColor(count); }
	static void  PushStyleVar_Impl(ImGuiStyleVar idx, float val) { ImGui::PushStyleVar(idx, val); }
	static void  PushStyleVarVec_Impl(ImGuiStyleVar idx, const ImVec2& val) { ImGui::PushStyleVar(idx, val); }
	static void  PopStyleVar_Impl(int count) { ImGui::PopStyleVar(count); }
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
		auto&  style = ImGui::GetStyle();
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

	// ==================================================
	// Layout
	// ==================================================
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
	static void AlignTextToFramePadding_Impl()
	{
		float scale                      = GetGlobalScale_Impl();
		float padY                       = 7.0f * scale;
		float oldPadY                    = ImGui::GetStyle().FramePadding.y;
		ImGui::GetStyle().FramePadding.y = padY;
		ImGui::AlignTextToFramePadding();
		ImGui::GetStyle().FramePadding.y = oldPadY;
	}
	static void GetContentRegionAvail_Impl(float* x, float* y)
	{
		ImVec2 p = ImGui::GetContentRegionAvail();
		if (x)
			*x = p.x;
		if (y)
			*y = p.y;
	}
	static float CalcItemWidth_Impl() { return ImGui::CalcItemWidth(); }
	static void  CalcTextSize_Impl(const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width, float* x, float* y)
	{
		ImVec2 s = ImGui::CalcTextSize(text, text_end, hide_text_after_double_hash, wrap_width);
		if (x)
			*x = s.x;
		if (y)
			*y = s.y;
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
	static void  SetNextItemWidth_Impl(float w) { ImGui::SetNextItemWidth(w); }
	static void  SetNextItemOpen_Impl(bool is_open, int cond) { ImGui::SetNextItemOpen(is_open, cond); }
	static void  Dummy_Impl(float w, float h) { ImGui::Dummy(ImVec2(w, h)); }
	static void  Spacing_Impl() { ImGui::Spacing(); }
	static void  Separator_Impl() { ImGui::Separator(); }
	static void  SeparatorThick_Impl() { ImGui::SeparatorThick(); }
	static void  SeparatorText_Impl(const char* label) { ImGui::SeparatorText(label); }
	static float GetColumnWidth_Impl(int column_index) { return ImGui::GetColumnWidth(column_index); }

	// ==================================================
	// Metrics
	// ==================================================
	static float GetTextLineHeight_Impl() { return ImGui::GetTextLineHeight(); }
	static float GetTextLineHeightWithSpacing_Impl() { return ImGui::GetTextLineHeightWithSpacing(); }
	static float GetFrameHeight_Impl() { return ImGui::GetFrameHeight(); }
	static float GetFrameHeightWithSpacing_Impl() { return ImGui::GetFrameHeightWithSpacing(); }

	// ==================================================
	// Utils
	// ==================================================
	static void        LoadTranslation_Impl(const char* n) { Translation::Manager::GetSingleton()->LoadCustomTranslation(n); }
	static const char* GetTranslation_Impl(const char* k) { return Translation::Manager::GetSingleton()->GetTranslation(k); }
	static void        SanitizePath_Impl(char* dest, const char* source, size_t size) { Utils::SanitizePath(dest, source, size); }

	static void GetPluginConfigPath_Impl(const char* pluginName, char* dest, size_t size)
	{
		if (!pluginName || !dest || size == 0)
			return;
		std::string path = std::format(R"(Data\FUCKs\{}\)", pluginName);
		strncpy_s(dest, size, path.c_str(), _TRUNCATE);
	}

	static void LoadPluginINI_Impl(const char* pluginName, void* userdata, void (*callback)(CSimpleIniA&, void*))
	{
		if (!callback || !pluginName)
			return;
		std::string defaultPath = std::format(R"(Data\FUCKs\{}\settings.ini)", pluginName);
		std::string userPath    = std::format(R"(Data\FUCKs\{}\settings_user.ini)", pluginName);

		Settings::GetSingleton()->LoadINI(defaultPath.c_str(), userPath.c_str(), [userdata, callback](CSimpleIniA& ini) {
			callback(ini, userdata);
		});
	}

	static void SavePluginINI_Impl(const char* pluginName, void* userdata, void (*callback)(CSimpleIniA&, void*))
	{
		if (!callback || !pluginName)
			return;
		std::string userPath = std::format(R"(Data\FUCKs\{}\settings_user.ini)", pluginName);

		Settings::GetSingleton()->LoadINI(userPath.c_str(), [userdata, callback](CSimpleIniA& ini) { callback(ini, userdata); }, true);
	}

	static void LoadPluginINIDefaults_Impl(const char* pluginName, void* userdata, void (*callback)(CSimpleIniA&, void*))
	{
		if (!callback || !pluginName)
			return;
		std::string defaultPath = std::format(R"(Data\FUCKs\{}\settings.ini)", pluginName);

		Settings::GetSingleton()->LoadINI(defaultPath.c_str(), [userdata, callback](CSimpleIniA& ini) {
			callback(ini, userdata);
		});
	}

	static void LoadPluginKeybinds_Impl(const char* pluginName, void* userdata, void (*callback)(CSimpleIniA&, void*))
	{
		if (!callback || !pluginName)
			return;
		std::string defaultPath = std::format(R"(Data\FUCKs\{}\keybinds.ini)", pluginName);
		std::string userPath    = std::format(R"(Data\FUCKs\{}\keybinds_user.ini)", pluginName);

		Settings::GetSingleton()->LoadINI(defaultPath.c_str(), userPath.c_str(), [userdata, callback](CSimpleIniA& ini) {
			callback(ini, userdata);
		});
	}

	static void SavePluginKeybinds_Impl(const char* pluginName, void* userdata, void (*callback)(CSimpleIniA&, void*))
	{
		if (!callback || !pluginName)
			return;
		std::string userPath = std::format(R"(Data\FUCKs\{}\keybinds_user.ini)", pluginName);

		Settings::GetSingleton()->LoadINI(userPath.c_str(), [userdata, callback](CSimpleIniA& ini) { callback(ini, userdata); }, true);
	}

	static void LoadPluginKeybindsDefaults_Impl(const char* pluginName, void* userdata, void (*callback)(CSimpleIniA&, void*))
	{
		if (!callback || !pluginName)
			return;
		std::string defaultPath = std::format(R"(Data\FUCKs\{}\keybinds.ini)", pluginName);

		Settings::GetSingleton()->LoadINI(defaultPath.c_str(), [userdata, callback](CSimpleIniA& ini) {
			callback(ini, userdata);
		});
	}

	static void PushItemFlag_Impl(ItemFlags flag, bool enabled) { ImGui::PushItemFlag(static_cast<ImGuiItemFlags>(flag), enabled); }
	static void PopItemFlag_Impl() { ImGui::PopItemFlag(); }

	static void PushID_Str_Impl(const char* str_id) { ImGui::PushID(str_id); }
	static void PushID_Int_Impl(int int_id) { ImGui::PushID(int_id); }
	static void PushID_Ptr_Impl(const void* ptr_id) { ImGui::PushID(ptr_id); }
	static void PopID_Impl() { ImGui::PopID(); }

	// ==================================================
	// Menu Events
	// ==================================================
	static void AddMenuListener_Impl(void* userdata, void (*callback)(const char*, bool, void*)) { FUCKMan::GetSingleton()->AddMenuListener(userdata, callback); }
	static void RemoveMenuListener_Impl(void* userdata) { FUCKMan::GetSingleton()->RemoveMenuListener(userdata); }

	// ==================================================
	// Assets
	// ==================================================
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
			float userIconScale = FUCKMan::GetSingleton()->IsIgnoringUserScale() ? 1.0f : ImGui::Styles::GetSingleton()->user.iconScale;

			// Lock base size to 38.0f to match the Hotkey widget internal math
			float baseFrameH = 38.0f * GetGlobalScale_Impl();
			float targetH    = std::round(baseFrameH * userIconScale);

			float targetW = std::round(targetH * (icon->imageSize.y > 0.0f ? (icon->imageSize.x / icon->imageSize.y) : 1.0f));

			if (w)
				*w = targetW;
			if (h)
				*h = targetH;
		} else {
			if (w)
				*w = 0.0f;
			if (h)
				*h = 0.0f;
		}
	}
	static void Spinner_Impl(const char* label, float radius, float thickness, const ImVec4& color) { ImGui::Spinner(label, radius, thickness, color); }
	static void DrawOverlay_Impl(FUCK::Overlay type, float thickness, ImU32 color, float paramA, float paramB, float paramC, float paramD) { ImGui::Overlays::Draw(type, thickness, color, paramA, paramB, paramC, paramD); }

	// ==================================================
	// Game Control
	// ==================================================
	static void SetGameTimeFrozen_Impl(bool frozen)
	{
		if (auto main = RE::Main::GetSingleton())
			main->GetRuntimeData().freezeTime = frozen;
	}
	static void SetAutoVanityBlocked_Impl(bool blocked) { FUCKMan::GetSingleton()->SetVanityBlocked(blocked); }
	static void SetHardPause_Impl(bool paused) { FUCKMan::GetSingleton()->SetManualHardPause(paused); }
	static void SetSoftPause_Impl(bool paused) { FUCKMan::GetSingleton()->SetManualSoftPause(paused); }
	static void ForceCursor_Impl(bool force) { FUCKMan::GetSingleton()->SetForceCursor(force); }

	// ==================================================
	// Input
	// ==================================================
	static bool  IsInputPressed_Impl(const void* evt, std::uint32_t key) { return Input::Manager::GetSingleton()->IsInputPressed(static_cast<const RE::InputEvent* const*>(evt), key); }
	static bool  IsInputDown_Impl(std::uint32_t key) { return Input::Manager::GetSingleton()->IsInputDown(key); }
	static float GetAnalogInput_Impl(std::uint32_t key) { return Input::Manager::GetSingleton()->GetAnalogInput(key); }
	static bool  IsModifierPressed_Impl(FUCK::Modifier m) { return Input::Manager::GetSingleton()->IsModifierPressed(m); }
	static int   GetInputDevice_Impl()
	{
		auto device = Input::Manager::GetSingleton()->GetInputDevice();
		if (device == Input::DEVICE::kGamepadDirectX || device == Input::DEVICE::kGamepadOrbis)
			return static_cast<int>(FUCK::InputDevice::kGamepad);
		return static_cast<int>(FUCK::InputDevice::kMouseKeyboard);
	}
	static const char*      GetKeyName_Impl(std::uint32_t key) { return Input::Manager::GetSingleton()->GetKeyName(key); }
	static bool             IsGamepadKey_Impl(std::uint32_t k) { return k >= Input::Keymap::kGPBase; }
	static bool             IsBinding_Impl() { return Input::Manager::GetSingleton()->IsBinding(); }
	static void             AbortBinding_Impl() { Input::Manager::GetSingleton()->AbortBinding(); }
	static void             StartBinding_Impl(std::uint32_t k, std::int32_t m1, std::int32_t m2, bool disallowModifiers) { Input::Manager::GetSingleton()->StartBinding(k, m1, m2, disallowModifiers); }
	static FUCK::BindResult UpdateBinding_Impl(const void* evt, std::uint32_t* k, std::int32_t* m1, std::int32_t* m2) { return Input::Manager::GetSingleton()->UpdateBinding(static_cast<const RE::InputEvent* const*>(evt), k, m1, m2); }
	static FUCK::BindResult GetInputBind_Impl(const void* evt, std::uint32_t* k, std::int32_t* m1, std::int32_t* m2) { return Input::Manager::GetSingleton()->GetInputBind(static_cast<const RE::InputEvent* const*>(evt), k, m1, m2); }

	static bool DrawManagedHotkey_Impl(const char* label, FUCK::ManagedHotkey* h, int flags, float iconScale, float labelScale)
	{
		if (h)
			return ImGui::DrawManagedHotkey(label, *h, static_cast<FUCK::HotkeyFlags>(flags), iconScale, labelScale);
		return false;
	}
	static bool UpdateManagedHotkey_Impl(const void* evt, FUCK::ManagedHotkey* h)
	{
		return h ? Input::Manager::GetSingleton()->UpdateManagedHotkey(static_cast<const RE::InputEvent* const*>(evt), *h) : false;
	}
	static bool ProcessManagedHotkey_Impl(const void* evt, FUCK::ManagedHotkey* h)
	{
		return h ? Input::Manager::GetSingleton()->ProcessManagedHotkey(static_cast<const RE::InputEvent* const*>(evt), *h) : false;
	}
	static bool IsManagedHotkeyDown_Impl(FUCK::ManagedHotkey* h)
	{
		return h ? Input::Manager::GetSingleton()->IsManagedHotkeyDown(*h) : false;
	}

	// ==================================================
	// Interaction
	// ==================================================
	static bool IsPopupOpen_Impl(const char* str_id, int flags) { return ImGui::IsPopupOpen(str_id, static_cast<ImGuiPopupFlags>(flags)); }
	static bool IsItemHovered_Impl(int flags) { return ImGui::IsItemHovered(flags); }
	static bool IsItemClicked_Impl(int btn) { return ImGui::IsItemClicked(btn); }
	static bool IsItemActive_Impl() { return ImGui::IsItemActive(); }
	static bool IsItemFocused_Impl() { return ImGui::IsItemFocused(); }
	static bool IsItemDeactivated_Impl() { return ImGui::IsItemDeactivated(); }
	static bool IsItemDeactivatedAfterEdit_Impl() { return ImGui::IsItemDeactivatedAfterEdit(); }
	static bool IsAnyItemActive_Impl() { return ImGui::IsAnyItemActive(); }
	static bool IsAnyItemHovered_Impl() { return ImGui::IsAnyItemHovered(); }
	static bool IsWindowFocused_Impl(int flags) { return ImGui::IsWindowFocused(flags); }
	static bool IsWindowHovered_Impl(int flags) { return ImGui::IsWindowHovered(flags); }
	static bool IsMouseDown_Impl(int button) { return ImGui::IsMouseDown(button); }
	static bool IsMouseClicked_Impl(int button, bool repeat) { return ImGui::IsMouseClicked(button, repeat); }
	static bool IsMouseReleased_Impl(int button) { return ImGui::IsMouseReleased(button); }
	static bool IsKeyDown_Impl(ImGuiKey key) { return ImGui::IsKeyDown(key); }
	static bool IsKeyPressed_Impl(ImGuiKey key, bool repeat) { return ImGui::IsKeyPressed(key, repeat); }
	static void SetKeyboardFocusHere_Impl(int offset) { ImGui::SetKeyboardFocusHere(offset); }
	static void SetItemDefaultFocus_Impl() { ImGui::SetItemDefaultFocus(); }

	static bool                BeginDragDropSource_Impl(int flags) { return ImGui::BeginDragDropSource(flags); }
	static bool                SetDragDropPayload_Impl(const char* type, const void* data, size_t sz, int cond) { return ImGui::SetDragDropPayload(type, data, sz, cond); }
	static void                EndDragDropSource_Impl() { ImGui::EndDragDropSource(); }
	static bool                BeginDragDropTarget_Impl() { return ImGui::BeginDragDropTarget(); }
	static const ImGuiPayload* AcceptDragDropPayload_Impl(const char* type, int flags) { return ImGui::AcceptDragDropPayload(type, flags); }
	static void                EndDragDropTarget_Impl() { ImGui::EndDragDropTarget(); }

	// ==================================================
	// Drawing Primitives
	// ==================================================
	static void DrawRect_Impl(const ImVec2& min, const ImVec2& max, const ImVec4& col, float r, float t) { ImGui::GetWindowDrawList()->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(col), r, 0, t); }
	static void DrawRectFilled_Impl(const ImVec2& min, const ImVec2& max, const ImVec4& col, float r) { ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(col), r); }
	static void DrawLine_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec4& col, float t)
	{
		ImGui::GetWindowDrawList()->AddLine(p1, p2, ImGui::ColorConvertFloat4ToU32(col), t);
	}
	static void DrawImage_Impl(void* tex, const ImVec2& s, const ImVec2& u0, const ImVec2& u1, const ImVec4& tint)
	{
		if (auto srv = GetSRV(tex))
			ImGui::ImageWithBg(reinterpret_cast<ImTextureID>(srv), s, u0, u1, ImVec4(0, 0, 0, 0), tint);
	}
	static void DrawImageQuad_Impl(void* tex, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& u1, const ImVec2& u2, const ImVec2& u3, const ImVec2& u4, const ImVec4& tint)
	{
		if (auto srv = GetSRV(tex))
			ImGui::GetWindowDrawList()->AddImageQuad(reinterpret_cast<ImTextureID>(srv), p1, p2, p3, p4, u1, u2, u3, u4, ImGui::ColorConvertFloat4ToU32(tint));
	}
	static void AddImage_Impl(void* tex, const ImVec2& min, const ImVec2& max, const ImVec2& u0, const ImVec2& u1, const ImVec4& col)
	{
		if (auto srv = GetSRV(tex))
			ImGui::GetWindowDrawList()->AddImage(reinterpret_cast<ImTextureID>(srv), min, max, u0, u1, ImGui::ColorConvertFloat4ToU32(col));
	}
	static void DrawBackgroundImage_Impl(void* tex, float alpha)
	{
		if (auto srv = GetSRV(tex))
			ImGui::GetBackgroundDrawList()->AddImage(reinterpret_cast<ImTextureID>(srv), { 0, 0 }, ImGui::GetIO().DisplaySize, { 0, 0 }, { 1, 1 }, IM_COL32(255, 255, 255, static_cast<int>(alpha * 255)));
	}
	static void DrawBackgroundLine_Impl(float x1, float y1, float x2, float y2, unsigned int col, float t) { ImGui::GetBackgroundDrawList()->AddLine({ x1, y1 }, { x2, y2 }, col, t); }
	static void DrawBackgroundRect_Impl(const ImVec2& min, const ImVec2& max, ImU32 col, float thickness) { ImGui::GetBackgroundDrawList()->AddRect(min, max, col, 0.0f, 0, thickness); }

	// ==================================================
	// Screen primitives
	// ==================================================
	static void DrawScreenRect_Impl(const ImVec2& min, const ImVec2& max, ImU32 col, float rounding, float thickness) { ImGui::GetForegroundDrawList()->AddRect(min, max, col, rounding, 0, thickness); }
	static void DrawScreenRectFilled_Impl(const ImVec2& min, const ImVec2& max, ImU32 col, float rounding) { ImGui::GetForegroundDrawList()->AddRectFilled(min, max, col, rounding); }
	static void DrawScreenLine_Impl(float x1, float y1, float x2, float y2, ImU32 col, float thickness) { ImGui::GetForegroundDrawList()->AddLine({ x1, y1 }, { x2, y2 }, col, thickness); }

	// ==================================================
	// Windows
	// ==================================================
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
	static void SetWindowPos_Impl(float x, float y, int c) { ImGui::SetWindowPos({ x, y }, c); }
	static void SetWindowSize_Impl(float x, float y, int c) { ImGui::SetWindowSize({ x, y }, c); }
	static bool BeginWindow_Impl(const char* n, bool* o, int f) { return ImGui::Begin(n, o, f); }
	static void EndWindow_Impl() { ImGui::End(); }
	static void ExtendWindowPastBorder_Impl() { ImGui::ExtendWindowPastBorder(); }
	static void BeginChild_Impl(const char* id, float w, float h, bool border, int flags) { ImGui::BeginChild(id, ImVec2(w, h), border, flags); }
	static void EndChild_Impl() { ImGui::EndChild(); }
	static bool TreeNode_Impl(const char* label) { return ImGui::TreeNodeIcon(label, 0); }
	static void TreePop_Impl() { ImGui::TreePop(); }
	static bool BeginPopupContextItem_Impl(const char* str_id, int mb) { return ImGui::BeginPopupContextItem(str_id, mb); }
	static void EndPopup_Impl() { ImGui::EndPopup(); }

	// ==================================================
	// Widgets
	// ==================================================
	static bool Button_Impl(const char* label) { return ImGui::OutlineButton(label); }
	static bool InvisibleButton_Impl(const char* str_id, const ImVec2& size, int flags) { return ImGui::InvisibleButton(str_id, size, static_cast<ImGuiButtonFlags>(flags)); }
	static bool Checkbox_Impl(const char* label, bool* v, bool alignFar, bool labelLeft) { return ImGui::CheckBox(label, v, alignFar, labelLeft); }
	static bool Hotkey_Impl(const char* label, std::uint32_t key, std::int32_t m1, std::int32_t m2, bool alignFar, bool labelLeft, bool flashing) { return ImGui::Hotkey(label, key, m1, m2, alignFar, labelLeft, flashing); }
	static bool ToggleButton_Impl(const char* label, bool* v, bool alignFar, bool labelLeft) { return ImGui::ToggleButton(label, v, alignFar, labelLeft); }

	static bool InputText_Impl(const char* label, char* buf, size_t buf_size, int flags)
	{
		if (!buf || buf_size == 0)
			return false;
		return ImGui::InputTextStyled(label, buf, buf_size, flags);
	}

	static bool ColorEdit3_Impl(const char* label, float col[3], int flags) { return ImGui::ColorEdit3Styled(label, col, flags); }
	static bool ColorEdit4_Impl(const char* label, float col[4], int flags) { return ImGui::ColorEdit4Styled(label, col, flags); }

	static bool SliderFloat_Impl(const char* label, float* v, float min, float max, const char* fmt) { return ImGui::Slider(label, v, min, max, fmt); }
	static bool SliderInt_Impl(const char* label, int* v, int min, int max, const char* fmt) { return ImGui::Slider(label, v, min, max, fmt); }

	static bool DragInt_Impl(const char* label, int* v, float s, int min, int max, const char* fmt) { return ImGui::DragOnHover(label, v, s, min, max, fmt); }
	static bool DragFloat_Impl(const char* label, float* v, float s, float min, float max, const char* fmt) { return ImGui::DragOnHover(label, v, s, min, max, fmt); }
	static bool DragFloat2_Impl(const char* label, float v[2], float s, float min, float max, const char* fmt) { return ImGui::DragFloat2Styled(label, v, s, min, max, fmt); }
	static bool DragFloat3_Impl(const char* label, float v[3], float s, float min, float max, const char* fmt) { return ImGui::DragFloat3Styled(label, v, s, min, max, fmt); }
	static bool DragFloat4_Impl(const char* label, float v[4], float s, float min, float max, const char* fmt) { return ImGui::DragFloat4Styled(label, v, s, min, max, fmt); }

	static bool Combo_Impl(const char* label, int* current_item, const char* const* items, int items_count) { return ImGui::ComboStyled(label, current_item, items, items_count); }
	static bool ComboWithFilter_Impl(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height)
	{
		// Internal conversion from raw C-array to std::vector for ImGui::ComboWithFilter implementation
		std::vector<std::string> vecItems;
		vecItems.reserve(items_count);
		for (int i = 0; i < items_count; ++i) vecItems.emplace_back(items[i]);
		return ImGui::ComboWithFilter(label, current_item, vecItems, popup_max_height);
	}
	static bool ComboForm_Impl(const char* label, std::uint32_t* id, std::uint8_t t) { return ImGui::ComboForm(label, reinterpret_cast<RE::FormID*>(id), static_cast<RE::FormType>(t)); }

	static bool ComboFormStr_Impl(const char* label, char* buf, size_t buf_size, std::uint8_t t)
	{
		std::string edid(buf);
		if (ImGui::ComboFormStr(label, &edid, static_cast<RE::FormType>(t))) {
			strncpy_s(buf, buf_size, edid.c_str(), _TRUNCATE);
			return true;
		}
		return false;
	}

	static bool Selectable_Impl(const char* label, bool selected, int flags, const ImVec2& size) { return ImGui::SelectableStyled(label, selected, flags, size); }

	static ImGuiTableSortSpecs* GetTableSortSpecs_Impl() { return ImGui::TableGetSortSpecs(); }

	static void Header_Impl(const char* label) { ImGui::Header(label); }
	static void LeftLabel_Impl(const char* label) { ImGui::LeftAlignedTextImpl(label); }
	static void HelpMarker_Impl(const char* desc) { ImGui::HelpMarker(desc); }

	static void TextColored_Impl(const ImVec4& col, const char* text) { ImGui::TextColored(col, "%s", text); }
	static void TextColoredWrapped_Impl(const ImVec4& col, const char* text) { ImGui::TextColoredWrapped(col, text); }
	static void TextDisabled_Impl(const char* text) { ImGui::TextDisabled("%s", text); }
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
			return ImGui::ButtonIconWithLabelStyled(label, reinterpret_cast<void*>(srv), size, alignFar, labelLeft);
		return false;
	}
	static bool ImageButton_Impl(const char* str_id, void* user_texture_id, float x, float y, const ImVec4* tint)
	{
		if (auto srv = GetSRV(user_texture_id))
			return ImGui::ImageButton(str_id, reinterpret_cast<ImTextureID>(srv), ImVec2(x, y), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0), tint ? *tint : ImVec4(1, 1, 1, 1));
		return false;
	}
	static void Stepper_Impl(const char* label, const char* text, bool* outLeft, bool* outRight) { ImGui::Stepper(label, text, outLeft, outRight); }

	static bool BeginTabBar_Impl(const char* s, int f) { return ImGui::BeginTabBar(s, f | ImGuiTabBarFlags_NoTabListScrollingButtons); }
	static void EndTabBar_Impl() { ImGui::EndTabBar(); }
	static bool BeginTabItem_Impl(const char* label, int flags) { return ImGui::BeginTabItemEx(label, static_cast<ImGuiTabItemFlags>(flags)); }
	static void EndTabItem_Impl() { ImGui::EndTabItem(); }

	static bool BeginTable_Impl(const char* id, int col, int f, const ImVec2& os, float iw) { return ImGui::BeginTable(id, col, f, os, iw); }
	static void EndTable_Impl() { ImGui::EndTable(); }
	static void TableSetupColumn_Impl(const char* label, int flags, float init_width, std::uint32_t user_id) { ImGui::TableSetupColumn(label, flags, init_width, user_id); }
	static void TableNextRow_Impl(int f, float h) { ImGui::TableNextRow(f, h); }
	static bool TableNextColumn_Impl() { return ImGui::TableNextColumn(); }
	static void TableHeadersRow_Impl() { ImGui::TableHeadersRow(); }
	static void TableSetBgColor_Impl(int t, ImU32 c, int col_n) { ImGui::TableSetBgColor(static_cast<ImGuiTableBgTarget>(t), c, col_n); }

	static void Columns_Impl(int count, const char* id, bool border) { ImGui::Columns(count, id, border); }
	static void NextColumn_Impl() { ImGui::NextColumn(); }

	static void SameLine_Impl(float offset, float spacing) { ImGui::SameLine(offset, spacing); }
	static bool CollapsingHeader_Impl(const char* label, int flags) { return ImGui::CollapsingHeaderIcon(label, flags); }
	static void BeginGroup_Impl() { ImGui::BeginGroup(); }
	static void EndGroup_Impl() { ImGui::EndGroup(); }
	static void BeginDisabled_Impl(bool disabled) { ImGui::BeginDisabled(disabled); }
	static void EndDisabled_Impl() { ImGui::EndDisabled(); }

	static bool IsWidgetFocused_Impl(const char* label) { return ImGui::IsWidgetFocused(label); }
	static void SetTooltip_Impl(const char* fmt) { ImGui::SetTooltipEx(fmt); }
	static void Indent_Impl(float w) { ImGui::Indent(w); }
	static void Unindent_Impl(float w) { ImGui::Unindent(w); }

	static void Text_Impl(const char* text) { ImGui::TextUnformatted(text); }
	static void TextWrapped_Impl(const char* text) { ImGui::TextWrappedEx(text); }
	static void TextUnformatted_Impl(const char* text, const char* text_end) { ImGui::TextUnformatted(text, text_end); }

	// ==================================================
	// Version 2
	// ==================================================
	static void SeparatorVertical_Impl() { ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical); }
	static void PushItemWidth_Impl(float item_width) { ImGui::PushItemWidth(item_width); }
	static void PopItemWidth_Impl() { ImGui::PopItemWidth(); }
	static bool BeginTooltip_Impl() { return ImGui::BeginTooltip(); }
	static void EndTooltip_Impl() { ImGui::EndTooltip(); }
	static void SetScrollHereY_Impl(float center_y_ratio) { ImGui::SetScrollHereY(center_y_ratio); }

	static bool InputTextMultiline_Impl(const char* label, char* buf, size_t buf_size, const ImVec2& size, int flags)
	{
		if (!buf || buf_size == 0)
			return false;

		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kComboBoxTextBox));
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kComboBoxText));
		bool res = ImGui::InputTextMultiline(label, buf, buf_size, size, flags);
		ImGui::PopStyleColor(2);
		return res;
	}

	// ==================================================
	// Version 3
	// ==================================================
	static void SetHotkeyEnabled_Impl(bool enabled) { Hotkeys::Manager::GetSingleton()->Enable(enabled); }
	static void SetWindowFocus_Impl() { ImGui::SetWindowFocus(); }
	static void CloseCurrentPopup_Impl() { ImGui::CloseCurrentPopup(); }
	static void OpenPopup_Impl(const char* str_id, int flags) { ImGui::OpenPopup(str_id, static_cast<ImGuiPopupFlags>(flags)); }
	static bool BeginPopup_Impl(const char* str_id, int /*flags*/)
	{
		// "flags" is currently ignored to prevent FUCK::WindowFlags clashing with ImGuiWindowFlags.
		// Reserved for future translation logic.
		return ImGui::BeginPopup(str_id, ImGuiWindowFlags_None);
	}

	static bool BeginPopupModal_Impl(const char* name, bool* p_open, int /*flags*/)
	{
		// "flags" is currently ignored for the same reason.
		// Enforcing NoResize globally for modals to remove grab handle that doesn't work.
		return ImGui::BeginPopupModal(name, p_open, ImGuiWindowFlags_NoResize);
	}
	static bool IsWindowAppearing_Impl() { return ImGui::IsWindowAppearing(); }
	static void PushTextWrapPos_Impl(float wrap_pos_x) { ImGui::PushTextWrapPos(wrap_pos_x); }
	static void PopTextWrapPos_Impl() { ImGui::PopTextWrapPos(); }
	static void SetNavCursorVisible_Impl(bool visible)
	{
		ImGuiContext& g    = *GImGui;
		g.NavCursorVisible = visible;
		if (visible) {
			g.NavHighlightItemUnderNav = true;
		}
	}
	static void DrawCircle_Impl(const ImVec2& center, float radius, const ImVec4& col, int num_segments, float thickness) { ImGui::GetWindowDrawList()->AddCircle(center, radius, ImGui::ColorConvertFloat4ToU32(col), num_segments, thickness); }
	static void DrawCircleFilled_Impl(const ImVec2& center, float radius, const ImVec4& col, int num_segments) { ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(col), num_segments); }
	static void DrawScreenCircle_Impl(const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) { ImGui::GetForegroundDrawList()->AddCircle(center, radius, col, num_segments, thickness); }
	static void DrawScreenCircleFilled_Impl(const ImVec2& center, float radius, ImU32 col, int num_segments) { ImGui::GetForegroundDrawList()->AddCircleFilled(center, radius, col, num_segments); }
	
	static void DrawQuad_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec4& col, float t) { ImGui::GetWindowDrawList()->AddQuad(p1, p2, p3, p4, ImGui::ColorConvertFloat4ToU32(col), t); }
	static void DrawQuadFilled_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec4& col) { ImGui::GetWindowDrawList()->AddQuadFilled(p1, p2, p3, p4, ImGui::ColorConvertFloat4ToU32(col)); }
	static void DrawScreenQuad_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float t) { ImGui::GetForegroundDrawList()->AddQuad(p1, p2, p3, p4, col, t); }
	static void DrawScreenQuadFilled_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col) { ImGui::GetForegroundDrawList()->AddQuadFilled(p1, p2, p3, p4, col); }
	
	static void DrawTriangle_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec4& col, float t) { ImGui::GetWindowDrawList()->AddTriangle(p1, p2, p3, ImGui::ColorConvertFloat4ToU32(col), t); }
	static void DrawTriangleFilled_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec4& col) { ImGui::GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, ImGui::ColorConvertFloat4ToU32(col)); }
	static void DrawScreenTriangle_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float t) { ImGui::GetForegroundDrawList()->AddTriangle(p1, p2, p3, col, t); }
	static void DrawScreenTriangleFilled_Impl(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col) { ImGui::GetForegroundDrawList()->AddTriangleFilled(p1, p2, p3, col); }

	static bool TreeNodeEx_Impl(const char* label, int flags) { return ImGui::TreeNodeIcon(label, flags); }

	// ==================================================
	// CreateInterface
	// ==================================================
	FUCK_Interface* CreateInterface()
	{
		static FUCK_Interface api = {
			.version = FUCK_API_VERSION,
			// Registration
			.RegisterTool     = RegisterTool_Impl,
			.RegisterWindow   = RegisterWindow_Impl,
			.UnregisterWindow = UnregisterWindow_Impl,
			// Display
			.GetResolutionScale         = GetResolutionScale_Impl,
			.GetGlobalScale             = GetGlobalScale_Impl,
			.GetUserScale               = GetUserScale_Impl,
			.GetDisplaySize             = GetDisplaySize_Impl,
			.TranslateScaleformToScreen = TranslateScaleformToScreen_Impl,
			.GetFont                    = GetFont_Impl,
			.PushFont                   = PushFont_Impl,
			.PopFont                    = PopFont_Impl,
			.SuspendRendering           = SuspendRendering_Impl,
			.SetMenuOpen                = SetMenuOpen_Impl,
			.IsMenuOpen                 = IsMenuOpen_Impl,
			// IO
			.GetDeltaTime  = GetDeltaTime_Impl,
			.GetTime       = GetTime_Impl,
			.GetMouseDelta = GetMouseDelta_Impl,
			.GetMousePos   = GetMousePos_Impl,
			.GetMouseWheel = GetMouseWheel_Impl,
			// Styling
			.PushStyleColor     = PushStyleColor_Impl,
			.PopStyleColor      = PopStyleColor_Impl,
			.PushStyleVar       = PushStyleVar_Impl,
			.PushStyleVarVec    = PushStyleVarVec_Impl,
			.PopStyleVar        = PopStyleVar_Impl,
			.GetStyleVar        = GetStyleVar_Impl,
			.GetStyleVarVec     = GetStyleVarVec_Impl,
			.GetStyleColorVec4  = GetStyleColorVec4_Impl,
			.SetWindowFontScale = SetWindowFontScale_Impl,
			// Layout
			.SetCursorPosX           = SetCursorPosX_Impl,
			.SetCursorPosY           = SetCursorPosY_Impl,
			.GetCursorPos            = GetCursorPos_Impl,
			.SetCursorPos            = SetCursorPos_Impl,
			.GetCursorScreenPos      = GetCursorScreenPos_Impl,
			.SetCursorScreenPos      = SetCursorScreenPos_Impl,
			.AlignTextToFramePadding = AlignTextToFramePadding_Impl,
			.GetContentRegionAvail   = GetContentRegionAvail_Impl,
			.CalcItemWidth           = CalcItemWidth_Impl,
			.CalcTextSize            = CalcTextSize_Impl,
			.GetItemRectMin          = GetItemRectMin_Impl,
			.GetItemRectMax          = GetItemRectMax_Impl,
			.SetNextItemWidth        = SetNextItemWidth_Impl,
			.SetNextItemOpen         = SetNextItemOpen_Impl,
			.Dummy                   = Dummy_Impl,
			.Spacing                 = Spacing_Impl,
			.Separator               = Separator_Impl,
			.SeparatorThick          = SeparatorThick_Impl,
			.SeparatorText           = SeparatorText_Impl,
			.GetColumnWidth          = GetColumnWidth_Impl,
			// Metrics
			.GetTextLineHeight            = GetTextLineHeight_Impl,
			.GetTextLineHeightWithSpacing = GetTextLineHeightWithSpacing_Impl,
			.GetFrameHeight               = GetFrameHeight_Impl,
			.GetFrameHeightWithSpacing    = GetFrameHeightWithSpacing_Impl,
			// Utils
			.LoadTranslation            = LoadTranslation_Impl,
			.GetTranslation             = GetTranslation_Impl,
			.SanitizePath               = SanitizePath_Impl,
			.GetPluginConfigPath        = GetPluginConfigPath_Impl,
			.LoadPluginINI              = LoadPluginINI_Impl,
			.SavePluginINI              = SavePluginINI_Impl,
			.LoadPluginINIDefaults      = LoadPluginINIDefaults_Impl,
			.LoadPluginKeybinds         = LoadPluginKeybinds_Impl,
			.SavePluginKeybinds         = SavePluginKeybinds_Impl,
			.LoadPluginKeybindsDefaults = LoadPluginKeybindsDefaults_Impl,
			.PushItemFlag               = PushItemFlag_Impl,
			.PopItemFlag                = PopItemFlag_Impl,
			.HelpMarker                 = HelpMarker_Impl,
			.PushID_Str                 = PushID_Str_Impl,
			.PushID_Int                 = PushID_Int_Impl,
			.PushID_Ptr                 = PushID_Ptr_Impl,
			.PopID                      = PopID_Impl,
			// Menu Events
			.AddMenuListener    = AddMenuListener_Impl,
			.RemoveMenuListener = RemoveMenuListener_Impl,
			// Assets
			.LoadImage         = LoadImage_Impl,
			.ReleaseImage      = ReleaseImage_Impl,
			.GetImageInfo      = GetImageInfo_Impl,
			.GetIconForKey     = GetIconForKey_Impl,
			.GetIconSizeForKey = GetIconSizeForKey_Impl,
			.Spinner           = Spinner_Impl,
			.DrawOverlay       = DrawOverlay_Impl,
			// Game Control
			.SetGameTimeFrozen    = SetGameTimeFrozen_Impl,
			.SetAutoVanityBlocked = SetAutoVanityBlocked_Impl,
			.SetHardPause         = SetHardPause_Impl,
			.SetSoftPause         = SetSoftPause_Impl,
			.ForceCursor          = ForceCursor_Impl,
			// Input
			.IsInputPressed       = IsInputPressed_Impl,
			.IsInputDown          = IsInputDown_Impl,
			.GetAnalogInput       = GetAnalogInput_Impl,
			.IsModifierPressed    = IsModifierPressed_Impl,
			.GetInputDevice       = GetInputDevice_Impl,
			.GetKeyName           = GetKeyName_Impl,
			.IsGamepadKey         = IsGamepadKey_Impl,
			.IsBinding            = IsBinding_Impl,
			.AbortBinding         = AbortBinding_Impl,
			.StartBinding         = StartBinding_Impl,
			.UpdateBinding        = UpdateBinding_Impl,
			.GetInputBind         = GetInputBind_Impl,
			.DrawManagedHotkey    = DrawManagedHotkey_Impl,
			.UpdateManagedHotkey  = UpdateManagedHotkey_Impl,
			.ProcessManagedHotkey = ProcessManagedHotkey_Impl,
			.IsManagedHotkeyDown  = IsManagedHotkeyDown_Impl,
			// Interaction
			.IsPopupOpen                = IsPopupOpen_Impl,
			.IsItemHovered              = IsItemHovered_Impl,
			.IsItemClicked              = IsItemClicked_Impl,
			.IsItemActive               = IsItemActive_Impl,
			.IsItemFocused              = IsItemFocused_Impl,
			.IsItemDeactivated          = IsItemDeactivated_Impl,
			.IsItemDeactivatedAfterEdit = IsItemDeactivatedAfterEdit_Impl,
			.IsAnyItemActive            = IsAnyItemActive_Impl,
			.IsAnyItemHovered           = IsAnyItemHovered_Impl,
			.IsWindowFocused            = IsWindowFocused_Impl,
			.IsWindowHovered            = IsWindowHovered_Impl,
			.IsMouseDown                = IsMouseDown_Impl,
			.IsMouseClicked             = IsMouseClicked_Impl,
			.IsMouseReleased            = IsMouseReleased_Impl,
			.IsKeyDown                  = IsKeyDown_Impl,
			.IsKeyPressed               = IsKeyPressed_Impl,
			.SetKeyboardFocusHere       = SetKeyboardFocusHere_Impl,
			.SetItemDefaultFocus        = SetItemDefaultFocus_Impl,
			.BeginDragDropSource        = BeginDragDropSource_Impl,
			.SetDragDropPayload         = SetDragDropPayload_Impl,
			.EndDragDropSource          = EndDragDropSource_Impl,
			.BeginDragDropTarget        = BeginDragDropTarget_Impl,
			.AcceptDragDropPayload      = AcceptDragDropPayload_Impl,
			.EndDragDropTarget          = EndDragDropTarget_Impl,
			// Drawing Primitives
			.DrawRect            = DrawRect_Impl,
			.DrawRectFilled      = DrawRectFilled_Impl,
			.DrawLine            = DrawLine_Impl,
			.DrawImage           = DrawImage_Impl,
			.DrawImageQuad       = DrawImageQuad_Impl,
			.AddImage            = AddImage_Impl,
			.DrawBackgroundImage = DrawBackgroundImage_Impl,
			.DrawBackgroundLine  = DrawBackgroundLine_Impl,
			.DrawBackgroundRect  = DrawBackgroundRect_Impl,
			// Screen primitives
			.DrawScreenRect       = DrawScreenRect_Impl,
			.DrawScreenRectFilled = DrawScreenRectFilled_Impl,
			.DrawScreenLine       = DrawScreenLine_Impl,
			// Windows
			.SetNextWindowPos       = SetNextWindowPos_Impl,
			.SetNextWindowSize      = SetNextWindowSize_Impl,
			.GetWindowPos           = GetWindowPos_Impl,
			.GetWindowSize          = GetWindowSize_Impl,
			.SetWindowPos           = SetWindowPos_Impl,
			.SetWindowSize          = SetWindowSize_Impl,
			.BeginWindow            = BeginWindow_Impl,
			.EndWindow              = EndWindow_Impl,
			.ExtendWindowPastBorder = ExtendWindowPastBorder_Impl,
			.BeginChild             = BeginChild_Impl,
			.EndChild               = EndChild_Impl,
			.TreeNode               = TreeNode_Impl,
			.TreePop                = TreePop_Impl,
			.BeginPopupContextItem  = BeginPopupContextItem_Impl,
			.EndPopup               = EndPopup_Impl,
			// Widgets
			.Button                 = Button_Impl,
			.InvisibleButton        = InvisibleButton_Impl,
			.Checkbox               = Checkbox_Impl,
			.Hotkey                 = Hotkey_Impl,
			.ToggleButton           = ToggleButton_Impl,
			.InputText              = InputText_Impl,
			.ColorEdit3             = ColorEdit3_Impl,
			.ColorEdit4             = ColorEdit4_Impl,
			.SliderFloat            = SliderFloat_Impl,
			.SliderInt              = SliderInt_Impl,
			.DragInt                = DragInt_Impl,
			.DragFloat              = DragFloat_Impl,
			.DragFloat2             = DragFloat2_Impl,
			.DragFloat3             = DragFloat3_Impl,
			.DragFloat4             = DragFloat4_Impl,
			.Combo                  = Combo_Impl,
			.ComboWithFilter        = ComboWithFilter_Impl,
			.ComboForm              = ComboForm_Impl,
			.ComboFormStr           = ComboFormStr_Impl,
			.Selectable             = Selectable_Impl,
			.GetTableSortSpecs      = GetTableSortSpecs_Impl,
			.Header                 = Header_Impl,
			.LeftLabel              = LeftLabel_Impl,
			.TextColored            = TextColored_Impl,
			.TextColoredWrapped     = TextColoredWrapped_Impl,
			.TextDisabled           = TextDisabled_Impl,
			.CenteredText           = CenteredText_Impl,
			.CenteredTextWithArrows = CenteredTextWithArrows_Impl,
			.ButtonIconWithLabel    = ButtonIconWithLabel_Impl,
			.ImageButton            = ImageButton_Impl,
			.Stepper                = Stepper_Impl,
			.BeginTabBar            = BeginTabBar_Impl,
			.EndTabBar              = EndTabBar_Impl,
			.BeginTabItem           = BeginTabItem_Impl,
			.EndTabItem             = EndTabItem_Impl,
			.BeginTable             = BeginTable_Impl,
			.EndTable               = EndTable_Impl,
			.TableSetupColumn       = TableSetupColumn_Impl,
			.TableNextRow           = TableNextRow_Impl,
			.TableNextColumn        = TableNextColumn_Impl,
			.TableHeadersRow        = TableHeadersRow_Impl,
			.TableSetBgColor        = TableSetBgColor_Impl,
			.Columns                = Columns_Impl,
			.NextColumn             = NextColumn_Impl,
			.SameLine               = SameLine_Impl,
			.CollapsingHeader       = CollapsingHeader_Impl,
			.BeginGroup             = BeginGroup_Impl,
			.EndGroup               = EndGroup_Impl,
			.BeginDisabled          = BeginDisabled_Impl,
			.EndDisabled            = EndDisabled_Impl,
			.IsWidgetFocused        = IsWidgetFocused_Impl,
			.SetTooltip             = SetTooltip_Impl,
			.Indent                 = Indent_Impl,
			.Unindent               = Unindent_Impl,
			.Text                   = Text_Impl,
			.TextWrapped            = TextWrapped_Impl,
			.TextUnformatted        = TextUnformatted_Impl,
			
			// Version 2
			.SeparatorVertical      = SeparatorVertical_Impl,
			.PushItemWidth          = PushItemWidth_Impl,
			.PopItemWidth           = PopItemWidth_Impl,
			.BeginTooltip           = BeginTooltip_Impl,
			.EndTooltip             = EndTooltip_Impl,
			.SetScrollHereY         = SetScrollHereY_Impl,
			.InputTextMultiline     = InputTextMultiline_Impl,

			// Version 3
			.SetHotkeyEnabled         = SetHotkeyEnabled_Impl,
			.SetWindowFocus           = SetWindowFocus_Impl,
			.CloseCurrentPopup        = CloseCurrentPopup_Impl,
			.OpenPopup                = OpenPopup_Impl,
			.BeginPopup               = BeginPopup_Impl,
			.BeginPopupModal          = BeginPopupModal_Impl,
			.IsWindowAppearing        = IsWindowAppearing_Impl,
			.PushTextWrapPos          = PushTextWrapPos_Impl,
			.PopTextWrapPos           = PopTextWrapPos_Impl,
			.SetNavCursorVisible      = SetNavCursorVisible_Impl,
			.DrawCircle               = DrawCircle_Impl,
			.DrawCircleFilled         = DrawCircleFilled_Impl,
			.DrawScreenCircle         = DrawScreenCircle_Impl,
			.DrawScreenCircleFilled   = DrawScreenCircleFilled_Impl,
			.DrawQuad                 = DrawQuad_Impl,
			.DrawQuadFilled           = DrawQuadFilled_Impl,
			.DrawScreenQuad           = DrawScreenQuad_Impl,
			.DrawScreenQuadFilled     = DrawScreenQuadFilled_Impl,
			.DrawTriangle             = DrawTriangle_Impl,
			.DrawTriangleFilled       = DrawTriangleFilled_Impl,
			.DrawScreenTriangle       = DrawScreenTriangle_Impl,
			.DrawScreenTriangleFilled = DrawScreenTriangleFilled_Impl,
			.TreeNodeEx               = TreeNodeEx_Impl
		};
		return &api;
	}
}
