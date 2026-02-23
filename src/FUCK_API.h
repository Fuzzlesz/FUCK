#pragma once
#include <imgui.h>

#define FUCK_API_VERSION 1

// ==========================================
// ENUMS & FLAGS
// ==========================================

enum class FUCK_Font
{
	kRegular,
	kLarge
};

enum class FUCK_Overlay
{
	kGrid,
	kCrosshair,
	kGoldenSpiral,
	kGoldenRatio,
	kTriangle
};

enum class WindowFlags
{
	kNone = 0,
	kBlurBackground = 1 << 0,
	kHideHUD = 1 << 1,
	kPauseHard = 1 << 2,
	kCloseOnEsc = 1 << 3,
	kCloseOnGameMenu = 1 << 4,
	kPassInputToGame = 1 << 5,
	kPauseSoft = 1 << 6,
	kBlockVanity = 1 << 7,
	kNoBackground = 1 << 8,
	kNoDecoration = 1 << 9,
	kCustomRender = 1 << 10,
	kExtendBorder = 1 << 11
};

enum class TableFlags
{
	kNone = 0,
	kResizable = 1 << 0,
	kReorderable = 1 << 1,
	kHideable = 1 << 2,
	kSortable = 1 << 3,
	kNoSavedSettings = 1 << 4,
	kContextMenuInBody = 1 << 5,
	kRowBg = 1 << 6,
	kBordersInnerH = 1 << 7,
	kBordersOuterH = 1 << 8,
	kBordersInnerV = 1 << 9,
	kBordersOuterV = 1 << 10,
	kBordersH = kBordersInnerH | kBordersOuterH,
	kBordersV = kBordersInnerV | kBordersOuterV,
	kBorders = kBordersInnerH | kBordersOuterH | kBordersInnerV | kBordersOuterV,
	kSizingFixedFit = 1 << 13,
	kSizingFixedSame = 2 << 13,
	kSizingStretchProp = 3 << 13,
	kSizingStretchSame = 4 << 13,
};

enum class TableColumnFlags
{
	kNone = 0,
	kDefaultSort = 1 << 0,
	kWidthStretch = 1 << 1,
	kWidthFixed = 1 << 2,
	kNoResize = 1 << 3,
	kNoReorder = 1 << 4,
	kNoHide = 1 << 5,
	kNoClip = 1 << 6,
	kNoSort = 1 << 7,
	kNoSortAscending = 1 << 8,
	kNoSortDescending = 1 << 9,
	kPreferSortAscending = 1 << 10,
	kPreferSortDescending = 1 << 11,
	kIndentEnable = 1 << 12,
	kIndentDisable = 1 << 13
};

enum class ItemFlags
{
	kNone = 0,
	kNoTabStop = 1 << 0,
	kButtonRepeat = 1 << 1,
	kDisabled = 1 << 2,
	kNoNav = 1 << 3
};

enum class BindResult
{
	kNone,
	kBound,
	kCancelled,
	kError
};

inline WindowFlags operator|(WindowFlags a, WindowFlags b) { return static_cast<WindowFlags>(static_cast<int>(a) | static_cast<int>(b)); }
inline bool operator&(WindowFlags a, WindowFlags b) { return (static_cast<int>(a) & static_cast<int>(b)) != 0; }
inline TableFlags operator|(TableFlags a, TableFlags b) { return static_cast<TableFlags>(static_cast<int>(a) | static_cast<int>(b)); }
inline TableColumnFlags operator|(TableColumnFlags a, TableColumnFlags b) { return static_cast<TableColumnFlags>(static_cast<int>(a) | static_cast<int>(b)); }

enum class Modifier
{
	kShift = 0,
	kCtrl = 1,
	kAlt = 2
};

// ==========================================
// EXTENSION INTERFACES
// ==========================================

class ITool
{
public:
	virtual ~ITool() = default;
	virtual const char* Name() const = 0;
	virtual const char* Group() const { return nullptr; }
	virtual void Draw() = 0;
	virtual void OnOpen() {}
	virtual void OnClose() {}
	virtual bool OnAsyncInput(const void*) { return false; }
	virtual bool ShowInSidebar() const { return true; }
};

class IWindow
{
public:
	virtual ~IWindow() = default;
	virtual const char* Title() const = 0;
	virtual void Draw() = 0;
	virtual bool IsOpen() const = 0;
	virtual void SetOpen(bool a_open) = 0;
	virtual WindowFlags GetFlags() const { return WindowFlags::kNone; }
	virtual ImVec2 GetDefaultSize() const { return ImVec2(400.0f, 300.0f); }
	virtual ImVec2 GetDefaultPos() const { return ImVec2(0.0f, 0.0f); }
	virtual bool GetRequestedPos(ImVec2& /*outPos*/) { return false; }
	virtual void UpdateState(const ImVec2& /*currentPos*/, const ImVec2& /*currentSize*/) {}
};

// ==========================================
// INTERFACE STRUCT
// ==========================================
#pragma pack(push, 1)
struct FUCK_Interface
{
	unsigned int version;

	// Registration
	void (*RegisterTool)(ITool*);
	void (*RegisterWindow)(IWindow*);

	// Display
	float (*GetResolutionScale)();
	void (*GetDisplaySize)(float*, float*);
	ImFont* (*GetFont)(FUCK_Font);
	void (*PushFont)(ImFont*, float);
	void (*PopFont)();
	void (*SuspendRendering)(bool);
	void (*SetMenuOpen)(bool);

	// IO
	float (*GetDeltaTime)();
	void (*GetMouseDelta)(float*, float*);
	void (*GetMousePos)(float*, float*);

	// Styling
	void (*PushStyleColor)(ImGuiCol, const ImVec4&);
	void (*PopStyleColor)(int);
	void (*PushStyleVar)(ImGuiStyleVar, float);
	void (*PushStyleVarVec)(ImGuiStyleVar, const ImVec2&);
	void (*PopStyleVar)(int);
	float (*GetStyleVar)(ImGuiStyleVar);
	void (*GetStyleVarVec)(ImGuiStyleVar, float*, float*);
	void (*GetStyleColorVec4)(ImGuiCol, float*, float*, float*, float*);

	// Layout
	void (*SetCursorPosX)(float);
	void (*SetCursorPosY)(float);
	void (*GetCursorPos)(float*, float*);
	void (*SetCursorPos)(float, float);
	void (*GetCursorScreenPos)(float*, float*);
	void (*SetCursorScreenPos)(float, float);
	void (*GetContentRegionAvail)(float*, float*);
	float (*CalcItemWidth)();
	void (*CalcTextSize)(const char*, const char*, bool, float, float*, float*);
	void (*GetItemRectMin)(float*, float*);
	void (*GetItemRectMax)(float*, float*);
	void (*SetNextItemWidth)(float);
	void (*SetNextItemOpen)(bool, int); 
	void (*Dummy)(float, float);
	void (*Spacing)();
	void (*Separator)();
	void (*SeparatorThick)();
	void (*SeparatorText)(const char*);

	// Metrics
	float (*GetTextLineHeight)();
	float (*GetTextLineHeightWithSpacing)();
	float (*GetFrameHeight)();
	float (*GetFrameHeightWithSpacing)();

	// Utils
	void (*LoadTranslation)(const char*);
	const char* (*GetTranslation)(const char*);
	void (*SanitizePath)(char*, const char*, size_t);
	void (*PushItemFlag)(ItemFlags, bool);
	void (*PopItemFlag)();
	void (*HelpMarker)(const char*);
	void (*PushID_Str)(const char*);
	void (*PushID_Int)(int);
	void (*PopID)();

	// Assets
	void* (*LoadImage)(const char*, bool);
	void (*ReleaseImage)(void*);
	void (*GetImageInfo)(void*, float*, float*);
	void* (*GetIconForKey)(std::uint32_t);
	void (*GetIconSizeForKey)(std::uint32_t, float*, float*);
	void (*Spinner)(const char*, float, float, const ImVec4&);
	void (*DrawOverlay)(FUCK_Overlay, float, ImU32, float, float, float, float);

	// Game Control
	void (*SetGameTimeFrozen)(bool);
	void (*SetAutoVanityBlocked)(bool);
	void (*SetHardPause)(bool);
	void (*SetSoftPause)(bool);
	void (*ForceCursor)(bool);

	// Input
	bool (*IsInputPressed)(const void*, std::uint32_t);
	bool (*IsInputDown)(std::uint32_t);
	float (*GetAnalogInput)(std::uint32_t);
	bool (*IsModifierPressed)(Modifier);
	int (*GetInputDevice)();  // Returns int cast of InputDevice enum
	const char* (*GetKeyName)(std::uint32_t);
	bool (*IsGamepadKey)(std::uint32_t);

	bool (*IsBinding)();
	void (*AbortBinding)();
	void (*StartBinding)(std::uint32_t, std::int32_t, std::int32_t);
	BindResult (*UpdateBinding)(const void*, std::uint32_t*, std::int32_t*, std::int32_t*);
	BindResult (*GetInputBind)(const void*, std::uint32_t*, std::int32_t*, std::int32_t*);

	// Interaction
	bool (*IsItemHovered)(int);
	bool (*IsItemClicked)(int);
	bool (*IsItemActive)();
	bool (*IsAnyItemActive)();
	bool (*IsAnyItemHovered)();
	bool (*IsWindowFocused)(int);
	bool (*IsWindowHovered)(int);
	bool (*IsMouseDown)(int);
	bool (*IsMouseReleased)(int);
	void (*SetKeyboardFocusHere)(int);
	void (*SetItemDefaultFocus)();

	// Drawing Primitives
	void (*DrawRect)(const ImVec2&, const ImVec2&, const ImVec4&, float, float);
	void (*DrawRectFilled)(const ImVec2&, const ImVec2&, const ImVec4&, float);
	void (*DrawImage)(void*, const ImVec2&, const ImVec2&, const ImVec2&, const ImVec4&);
	void (*AddImage)(void*, const ImVec2&, const ImVec2&, const ImVec2&, const ImVec2&, const ImVec4&);
	void (*DrawBackgroundImage)(void*, float);
	void (*DrawBackgroundLine)(float, float, float, float, unsigned int, float);
	void (*DrawBackgroundRect)(const ImVec2&, const ImVec2&, ImU32, float);

	// Screen primitives
	void (*DrawScreenRect)(const ImVec2&, const ImVec2&, ImU32, float, float);
	void (*DrawScreenRectFilled)(const ImVec2&, const ImVec2&, ImU32, float);
	void (*DrawScreenLine)(float, float, float, float, ImU32, float);

	// Windows
	void (*SetNextWindowPos)(float, float, int, float, float);
	void (*SetNextWindowSize)(float, float, int);
	void (*GetWindowPos)(float*, float*);
	void (*GetWindowSize)(float*, float*);
	bool (*BeginWindow)(const char*, bool*, int);
	void (*EndWindow)();
	void (*ExtendWindowPastBorder)();
	void (*BeginChild)(const char*, float, float, bool, int);
	void (*EndChild)();
	bool (*TreeNode)(const char*);
	void (*TreePop)();

	// Widgets
	bool (*Button)(const char*);
	bool (*Checkbox)(const char*, bool*, bool, bool);
	bool (*Hotkey)(const char*, std::uint32_t, std::int32_t, std::int32_t, bool, bool, bool);
	bool (*ToggleButton)(const char*, bool*, bool, bool);
	bool (*InputText)(const char*, char*, size_t, int);
	bool (*ColorEdit3)(const char*, float[3], int);
	bool (*SliderFloat)(const char*, float*, float, float, const char*);
	bool (*SliderInt)(const char*, int*, int, int, const char*);
	bool (*DragFloat)(const char*, float*, float, float, float, const char*);
	bool (*DragInt)(const char*, int*, float, int, int, const char*);
	bool (*Combo)(const char*, int*, const char* const*, int);
	bool (*ComboWithFilter)(const char*, int*, const char* const*, int, int);
	bool (*ComboForm)(const char*, std::uint32_t*, std::uint8_t);
	ImGuiTableSortSpecs* (*GetTableSortSpecs)();
	bool (*Selectable)(const char*, bool, int, const ImVec2&);
	void (*Header)(const char*);
	void (*LeftLabel)(const char*);

	void (*TextColored)(const ImVec4&, const char*);
	void (*TextColoredWrapped)(const ImVec4&, const char*);
	void (*TextDisabled)(const char*);
	void (*CenteredText)(const char*, bool);
	void (*CenteredTextWithArrows)(const char*, const char*, bool*, bool*, bool*);

	bool (*ButtonIconWithLabel)(const char*, void*, float, float, bool, bool);
	bool (*ImageButton)(const char*, void*, float, float, const ImVec4*);
	void (*Stepper)(const char*, const char*, bool*, bool*);

	bool (*BeginTabBar)(const char*, int);
	void (*EndTabBar)();
	bool (*BeginTabItem)(const char*, int);
	void (*EndTabItem)();

	bool (*BeginTable)(const char*, int, int, const ImVec2&, float);
	void (*EndTable)();
	void (*TableSetupColumn)(const char*, int, float, std::uint32_t);
	void (*TableNextRow)(int, float);
	bool (*TableNextColumn)();
	void (*TableHeadersRow)();

	void (*Columns)(int, const char*, bool);
	void (*NextColumn)();

	void (*SameLine)(float, float);
	bool (*CollapsingHeader)(const char*, int);
	void (*BeginGroup)();
	void (*EndGroup)();
	void (*BeginDisabled)(bool);
	void (*EndDisabled)();

	bool (*IsWidgetFocused)(const char*);
	void (*SetTooltip)(const char*);
	void (*Indent)(float);
	void (*Unindent)(float);
	void (*Text)(const char*);
	void (*TextWrapped)(const char*);
	void (*TextUnformatted)(const char*, const char*);
};
#pragma pack(pop)

// ==========================================
// PUBLIC API NAMESPACE
// ==========================================

namespace FUCK
{
	enum class InputDevice
	{
		kMouseKeyboard,
		kGamepad
	};

	inline FUCK_Interface*& GetInterface()
	{
		static FUCK_Interface* s = nullptr;
		return s;
	}

	inline bool Connect(unsigned int a_minVersion = FUCK_API_VERSION)
	{
		auto handle = GetModuleHandleW(L"FUCK.dll");
		if (!handle)
			return false;
		auto fetcher = (void* (*)())GetProcAddress(handle, "RequestFUCK");
		if (!fetcher)
			return false;
		auto* iface = static_cast<FUCK_Interface*>(fetcher());
		if (!iface || iface->version < a_minVersion) {
			logger::error("FUCK API Version Mismatch: Expected {}, found {}", a_minVersion, iface ? iface->version : 0);
			return false;
		}

		GetInterface() = iface;
		logger::info("Connected to FUCK API version {}", iface->version);

		return true;
	}

	// --- Registration ---
	inline void RegisterTool(ITool* tool)
	{
		if (auto i = GetInterface())
			i->RegisterTool(tool);
	}
	inline void RegisterWindow(IWindow* window)
	{
		if (auto i = GetInterface())
			i->RegisterWindow(window);
	}

	// --- Input Binding ---
	inline void AbortBinding()
	{
		if (auto i = GetInterface())
			i->AbortBinding();
	}
	inline bool IsBinding() { return GetInterface() ? GetInterface()->IsBinding() : false; }
	inline void StartBinding(std::uint32_t key, std::int32_t mod1, std::int32_t mod2)
	{
		if (auto i = GetInterface())
			i->StartBinding(key, mod1, mod2);
	}
	inline BindResult UpdateBinding(const void* inputEvent, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2) { return GetInterface() ? GetInterface()->UpdateBinding(inputEvent, outKey, outMod1, outMod2) : BindResult::kNone; }
	inline BindResult GetInputBind(const void* inputEvent, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2) { return GetInterface() ? GetInterface()->GetInputBind(inputEvent, outKey, outMod1, outMod2) : BindResult::kNone; }

	// --- Display & Style ---
	inline float GetResolutionScale() { return GetInterface() ? GetInterface()->GetResolutionScale() : 1.0f; }
	inline ImVec2 GetDisplaySize()
	{
		if (auto i = GetInterface()) {
			ImVec2 s;
			i->GetDisplaySize(&s.x, &s.y);
			return s;
		}
		return ImVec2(0, 0);
	}
	inline ImFont* GetFont(FUCK_Font font) { return GetInterface() ? GetInterface()->GetFont(font) : nullptr; }
	inline void PushFont(ImFont* font, float size = 0.0f)
	{
		if (auto i = GetInterface())
			i->PushFont(font, size);
	}
	inline void PopFont()
	{
		if (auto i = GetInterface())
			i->PopFont();
	}
	inline void SuspendRendering(bool suspend)
	{
		if (auto i = GetInterface())
			i->SuspendRendering(suspend);
	}
	inline void SetMenuOpen(bool open)
	{
		if (auto i = GetInterface())
			i->SetMenuOpen(open);
	}

	inline void PushStyleColor(ImGuiCol idx, const ImVec4& col)
	{
		if (auto i = GetInterface())
			i->PushStyleColor(idx, col);
	}
	inline void PopStyleColor(int count = 1)
	{
		if (auto i = GetInterface())
			i->PopStyleColor(count);
	}
	inline void PushStyleVar(ImGuiStyleVar idx, float val)
	{
		if (auto i = GetInterface())
			i->PushStyleVar(idx, val);
	}
	inline void PushStyleVar(ImGuiStyleVar idx, const ImVec2& val)
	{
		if (auto i = GetInterface())
			i->PushStyleVarVec(idx, val);
	}
	inline void PopStyleVar(int count = 1)
	{
		if (auto i = GetInterface())
			i->PopStyleVar(count);
	}
	inline float GetStyleVar(ImGuiStyleVar idx) { return GetInterface() ? GetInterface()->GetStyleVar(idx) : 0.0f; }
	inline ImVec2 GetStyleVarVec(ImGuiStyleVar idx)
	{
		ImVec2 v(0, 0);
		if (auto i = GetInterface())
			i->GetStyleVarVec(idx, &v.x, &v.y);
		return v;
	}
	inline ImVec4 GetStyleColorVec4(ImGuiCol idx)
	{
		ImVec4 v(0, 0, 0, 0);
		if (auto i = GetInterface())
			i->GetStyleColorVec4(idx, &v.x, &v.y, &v.z, &v.w);
		return v;
	}

	// --- Layout & Cursor ---
	inline void SetCursorPosX(float x)
	{
		if (auto i = GetInterface())
			i->SetCursorPosX(x);
	}
	inline void SetCursorPosY(float y)
	{
		if (auto i = GetInterface())
			i->SetCursorPosY(y);
	}
	inline ImVec2 GetCursorPos()
	{
		ImVec2 p;
		if (auto i = GetInterface())
			i->GetCursorPos(&p.x, &p.y);
		return p;
	}
	inline void SetCursorPos(const ImVec2& pos)
	{
		if (auto i = GetInterface())
			i->SetCursorPos(pos.x, pos.y);
	}
	inline ImVec2 GetCursorScreenPos()
	{
		ImVec2 p;
		if (auto i = GetInterface())
			i->GetCursorScreenPos(&p.x, &p.y);
		return p;
	}
	inline void SetCursorScreenPos(const ImVec2& pos)
	{
		if (auto i = GetInterface())
			i->SetCursorScreenPos(pos.x, pos.y);
	}
	inline ImVec2 GetContentRegionAvail()
	{
		ImVec2 s;
		if (auto i = GetInterface())
			i->GetContentRegionAvail(&s.x, &s.y);
		return s;
	}
	inline float CalcItemWidth() { return GetInterface() ? GetInterface()->CalcItemWidth() : 0.0f; }
	inline ImVec2 CalcTextSize(const char* text, const char* text_end = nullptr, bool hide_text_after_double_hash = false, float wrap_width = -1.0f)
	{
		ImVec2 s;
		if (auto i = GetInterface())
			i->CalcTextSize(text, text_end, hide_text_after_double_hash, wrap_width, &s.x, &s.y);
		return s;
	}
	inline ImVec2 GetItemRectMin()
	{
		ImVec2 p;
		if (auto i = GetInterface())
			i->GetItemRectMin(&p.x, &p.y);
		return p;
	}
	inline ImVec2 GetItemRectMax()
	{
		ImVec2 p;
		if (auto i = GetInterface())
			i->GetItemRectMax(&p.x, &p.y);
		return p;
	}
	inline ImVec2 GetItemRectSize()
	{
		ImVec2 min = GetItemRectMin();
		ImVec2 max = GetItemRectMax();
		return ImVec2(max.x - min.x, max.y - min.y);
	}

	inline void SetNextItemWidth(float width)
	{
		if (auto i = GetInterface())
			i->SetNextItemWidth(width);
	}
	inline void SetNextItemOpen(bool is_open, int cond = 0)
	{
		if (auto i = GetInterface())
			i->SetNextItemOpen(is_open, cond);
	}
	inline void Dummy(const ImVec2& size)
	{
		if (auto i = GetInterface())
			i->Dummy(size.x, size.y);
	}
	inline void Spacing(int count = 1)
	{
		if (auto i = GetInterface()) {
			for (int k = 0; k < count; ++k) i->Spacing();
		}
	}
	inline void Indent(float width = 0.0f)
	{
		if (auto i = GetInterface())
			i->Indent(width);
	}
	inline void Unindent(float width = 0.0f)
	{
		if (auto i = GetInterface())
			i->Unindent(width);
	}
	inline void SameLine(float offset_from_start_x = 0.0f, float spacing = -1.0f)
	{
		if (auto i = GetInterface())
			i->SameLine(offset_from_start_x, spacing);
	}

	inline void Separator()
	{
		if (auto i = GetInterface())
			i->Separator();
	}
	inline void SeparatorThick()
	{
		if (auto i = GetInterface())
			i->SeparatorThick();
	}
	inline void SeparatorText(const char* label)
	{
		if (auto i = GetInterface())
			i->SeparatorText(label);
	}

	inline void BeginGroup()
	{
		if (auto i = GetInterface())
			i->BeginGroup();
	}
	inline void EndGroup()
	{
		if (auto i = GetInterface())
			i->EndGroup();
	}
	inline void BeginDisabled(bool disabled = true)
	{
		if (auto i = GetInterface())
			i->BeginDisabled(disabled);
	}
	inline void EndDisabled()
	{
		if (auto i = GetInterface())
			i->EndDisabled();
	}

	inline void PushID(const char* str_id)
	{
		if (auto i = GetInterface())
			i->PushID_Str(str_id);
	}
	inline void PushID(int int_id)
	{
		if (auto i = GetInterface())
			i->PushID_Int(int_id);
	}
	inline void PopID()
	{
		if (auto i = GetInterface())
			i->PopID();
	}

	inline void BeginChild(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = false, int flags = 0)
	{
		if (auto i = GetInterface())
			i->BeginChild(str_id, size.x, size.y, border, flags);
	}
	inline void EndChild()
	{
		if (auto i = GetInterface())
			i->EndChild();
	}

	inline bool TreeNode(const char* label) { return GetInterface() ? GetInterface()->TreeNode(label) : false; }
	inline void TreePop()
	{
		if (auto i = GetInterface())
			i->TreePop();
	}

	inline void PushItemFlag(ItemFlags flag, bool enabled)
	{
		if (auto i = GetInterface())
			i->PushItemFlag(flag, enabled);
	}
	inline void PopItemFlag()
	{
		if (auto i = GetInterface())
			i->PopItemFlag();
	}

	// --- Metrics ---
	inline float GetTextLineHeight() { return GetInterface() ? GetInterface()->GetTextLineHeight() : 0.0f; }
	inline float GetTextLineHeightWithSpacing() { return GetInterface() ? GetInterface()->GetTextLineHeightWithSpacing() : 0.0f; }
	inline float GetFrameHeight() { return GetInterface() ? GetInterface()->GetFrameHeight() : 0.0f; }
	inline float GetFrameHeightWithSpacing() { return GetInterface() ? GetInterface()->GetFrameHeightWithSpacing() : 0.0f; }

	// --- IO & Game State ---
	inline float GetDeltaTime() { return GetInterface() ? GetInterface()->GetDeltaTime() : 0.0f; }
	inline ImVec2 GetMouseDelta()
	{
		ImVec2 p(0, 0);
		if (auto i = GetInterface())
			i->GetMouseDelta(&p.x, &p.y);
		return p;
	}
	inline ImVec2 GetMousePos()
	{
		ImVec2 p(0, 0);
		if (auto i = GetInterface())
			i->GetMousePos(&p.x, &p.y);
		return p;
	}
	inline void SetGameTimeFrozen(bool frozen)
	{
		if (auto i = GetInterface())
			i->SetGameTimeFrozen(frozen);
	}
	inline void SetAutoVanityBlocked(bool blocked)
	{
		if (auto i = GetInterface())
			i->SetAutoVanityBlocked(blocked);
	}
	inline void SetHardPause(bool paused)
	{
		if (auto i = GetInterface())
			i->SetHardPause(paused);
	}
	inline void SetSoftPause(bool paused)
	{
		if (auto i = GetInterface())
			i->SetSoftPause(paused);
	}
	inline void ForceCursor(bool force)
	{
		if (auto i = GetInterface())
			i->ForceCursor(force);
	}

	// --- Input Queries ---
	inline bool IsInputPressed(const void* inputEvent, std::uint32_t keyId) { return GetInterface() ? GetInterface()->IsInputPressed(inputEvent, keyId) : false; }
	inline bool IsInputDown(std::uint32_t keyId) { return GetInterface() ? GetInterface()->IsInputDown(keyId) : false; }
	inline float GetAnalogInput(std::uint32_t keyId) { return GetInterface() ? GetInterface()->GetAnalogInput(keyId) : 0.0f; }
	inline bool IsModifierPressed(Modifier mod) { return GetInterface() ? GetInterface()->IsModifierPressed(mod) : false; }
	inline InputDevice GetInputDevice() { return GetInterface() ? static_cast<InputDevice>(GetInterface()->GetInputDevice()) : InputDevice::kMouseKeyboard; }
	inline const char* GetKeyName(std::uint32_t k) { return GetInterface() ? GetInterface()->GetKeyName(k) : ""; }
	inline bool IsGamepadKey(std::uint32_t k) { return GetInterface() ? GetInterface()->IsGamepadKey(k) : false; }

	// --- Interaction Queries ---
	inline bool IsItemHovered(int flags = 0) { return GetInterface() ? GetInterface()->IsItemHovered(flags) : false; }
	inline bool IsItemClicked(int mouse_button = 0) { return GetInterface() ? GetInterface()->IsItemClicked(mouse_button) : false; }
	inline bool IsItemActive() { return GetInterface() ? GetInterface()->IsItemActive() : false; }
	inline bool IsAnyItemActive() { return GetInterface() ? GetInterface()->IsAnyItemActive() : false; }
	inline bool IsAnyItemHovered() { return GetInterface() ? GetInterface()->IsAnyItemHovered() : false; }
	inline bool IsWindowFocused(int flags = 0) { return GetInterface() ? GetInterface()->IsWindowFocused(flags) : false; }
	inline bool IsWindowHovered(int flags = 0) { return GetInterface() ? GetInterface()->IsWindowHovered(flags) : false; }
	inline bool IsMouseDown(int button) { return GetInterface() ? GetInterface()->IsMouseDown(button) : false; }
	inline bool IsMouseReleased(int button) { return GetInterface() ? GetInterface()->IsMouseReleased(button) : false; }
	inline void SetKeyboardFocusHere(int offset = 0)
	{
		if (auto i = GetInterface())
			i->SetKeyboardFocusHere(offset);
	}
	inline void SetItemDefaultFocus()
	{
		if (auto i = GetInterface())
			i->SetItemDefaultFocus();
	}

	// --- Utilities ---
	inline const char* Translate(const char* key) { return GetInterface() ? GetInterface()->GetTranslation(key) : key; }
	inline void LoadTranslation(const char* pluginName)
	{
		if (auto i = GetInterface())
			i->LoadTranslation(pluginName);
	}
	inline void SanitizePath(char* dest, const char* source, size_t size)
	{
		if (auto i = GetInterface())
			i->SanitizePath(dest, source, size);
	}
	inline void HelpMarker(const char* desc)
	{
		if (auto i = GetInterface())
			i->HelpMarker(desc);
	}

	// --- Assets & Overlays ---
	inline void* LoadImage(const char* path, bool resizeToScreen = false) { return GetInterface() ? GetInterface()->LoadImage(path, resizeToScreen) : nullptr; }
	inline void ReleaseImage(void* texID)
	{
		if (auto i = GetInterface())
			i->ReleaseImage(texID);
	}
	inline void GetImageInfo(void* texID, float* width, float* height)
	{
		if (auto i = GetInterface())
			i->GetImageInfo(texID, width, height);
	}
	inline void* GetIconForKey(std::uint32_t key, ImVec2* outSize = nullptr)
	{
		if (auto i = GetInterface()) {
			if (outSize)
				i->GetIconSizeForKey(key, &outSize->x, &outSize->y);
			return i->GetIconForKey(key);
		}
		return nullptr;
	}
	inline void Spinner(const char* label, float radius, float thickness, const ImVec4& color)
	{
		if (auto i = GetInterface())
			i->Spinner(label, radius, thickness, color);
	}
	inline void DrawOverlay(FUCK_Overlay type, float thickness = 1.0f, ImU32 color = 0, float paramA = 0.0f, float paramB = 0.0f, float paramC = 0.0f, float paramD = 0.0f)
	{
		if (auto i = GetInterface())
			i->DrawOverlay(type, thickness, color, paramA, paramB, paramC, paramD);
	}
	inline void DrawGrid(float thickness = 0.0f, ImU32 color = 0, float rows = 3.0f, float cols = 3.0f, float rotationDeg = 0.0f)
	{
		DrawOverlay(FUCK_Overlay::kGrid, thickness, color, rows, cols, rotationDeg, 0.0f);
	}
	inline void DrawCrosshair(float thickness = 1.0f, ImU32 color = 0, float rows = 1.0f, float cols = 1.0f)
	{
		DrawOverlay(FUCK_Overlay::kCrosshair, thickness, color, rows, cols, 0.0f, 0.0f);
	}
	inline void DrawGoldenSpiral(float thickness = 1.0f, ImU32 color = 0, int anchorIndex = 0, float turns = 6.0f, float rotationDeg = 0.0f, float scale = 1.0f, bool showSquares = false)
	{
		float pA = static_cast<float>(anchorIndex) + (showSquares ? 10.0f : 0.0f);
		DrawOverlay(FUCK_Overlay::kGoldenSpiral, thickness, color, pA, turns, rotationDeg, scale);
	}
	inline void DrawGoldenGrid(float thickness = 1.0f, ImU32 color = 0, int subdivisions = 0)
	{
		DrawOverlay(FUCK_Overlay::kGoldenRatio, thickness, color, static_cast<float>(subdivisions), 0.0f, 0.0f, 0.0f);
	}
	inline void DrawTriangle(float thickness = 1.0f, ImU32 color = 0, bool mirror = false)
	{
		DrawOverlay(FUCK_Overlay::kTriangle, thickness, color, mirror ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
	}

	// --- Drawing Primitives ---
	inline void DrawRect(const ImVec2& min, const ImVec2& max, const ImVec4& col, float rounding = 0.0f, float thickness = 1.0f)
	{
		if (auto i = GetInterface())
			i->DrawRect(min, max, col, rounding, thickness);
	}
	inline void DrawRectFilled(const ImVec2& min, const ImVec2& max, const ImVec4& col, float rounding = 0.0f)
	{
		if (auto i = GetInterface())
			i->DrawRectFilled(min, max, col, rounding);
	}
	inline void DrawImage(void* textureId, const ImVec2& size, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& tint_col = ImVec4(1, 1, 1, 1))
	{
		if (auto i = GetInterface())
			i->DrawImage(textureId, size, uv0, uv1, tint_col);
	}
	inline void AddImage(void* textureId, const ImVec2& min, const ImVec2& max, const ImVec2& uv0 = ImVec2(0, 0), const ImVec2& uv1 = ImVec2(1, 1), const ImVec4& col = ImVec4(1, 1, 1, 1))
	{
		if (auto i = GetInterface())
			i->AddImage(textureId, min, max, uv0, uv1, col);
	}
	inline void DrawBackgroundImage(void* tex, float alpha)
	{
		if (auto i = GetInterface())
			i->DrawBackgroundImage(tex, alpha);
	}
	inline void DrawBackgroundLine(const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness)
	{
		if (auto i = GetInterface())
			i->DrawBackgroundLine(p1.x, p1.y, p2.x, p2.y, col, thickness);
	}
	inline void DrawBackgroundRect(const ImVec2& min, const ImVec2& max, ImU32 col, float thickness)
	{
		if (auto i = GetInterface())
			i->DrawBackgroundRect(min, max, col, thickness);
	}

	// --- Screen Primitives ---
	inline void DrawScreenRect(const ImVec2& min, const ImVec2& max, ImU32 col, float rounding = 0.0f, float thickness = 1.0f)
	{
		if (auto i = GetInterface())
			i->DrawScreenRect(min, max, col, rounding, thickness);
	}
	inline void DrawScreenRectFilled(const ImVec2& min, const ImVec2& max, ImU32 col, float rounding = 0.0f)
	{
		if (auto i = GetInterface())
			i->DrawScreenRectFilled(min, max, col, rounding);
	}
	inline void DrawScreenLine(const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness = 1.0f)
	{
		if (auto i = GetInterface())
			i->DrawScreenLine(p1.x, p1.y, p2.x, p2.y, col, thickness);
	}

	// --- Windows ---
	inline bool BeginWindow(const char* name, bool* p_open = nullptr, int flags = 0) { return GetInterface() ? GetInterface()->BeginWindow(name, p_open, flags) : false; }
	inline void EndWindow()
	{
		if (auto i = GetInterface())
			i->EndWindow();
	}
	inline void ExtendWindowPastBorder()
	{
		if (auto i = GetInterface())
			i->ExtendWindowPastBorder();
	}
	inline ImVec2 GetWindowPos()
	{
		if (auto i = GetInterface()) {
			ImVec2 p;
			i->GetWindowPos(&p.x, &p.y);
			return p;
		}
		return ImVec2(0, 0);
	}
	inline ImVec2 GetWindowSize()
	{
		if (auto i = GetInterface()) {
			ImVec2 p;
			i->GetWindowSize(&p.x, &p.y);
			return p;
		}
		return ImVec2(0, 0);
	}
	inline void SetNextWindowPos(const ImVec2& pos, int cond = 0, const ImVec2& pivot = ImVec2(0, 0))
	{
		if (auto i = GetInterface())
			i->SetNextWindowPos(pos.x, pos.y, cond, pivot.x, pivot.y);
	}
	inline void SetNextWindowSize(const ImVec2& size, int cond = 0)
	{
		if (auto i = GetInterface())
			i->SetNextWindowSize(size.x, size.y, cond);
	}

	// --- Tabs, Tables & Groups ---
	inline bool BeginTabBar(const char* str_id, int flags = 0) { return GetInterface() ? GetInterface()->BeginTabBar(str_id, flags) : false; }
	inline void EndTabBar()
	{
		if (auto i = GetInterface())
			i->EndTabBar();
	}
	inline bool BeginTabItem(const char* label, int flags = 0) { return GetInterface() ? GetInterface()->BeginTabItem(label, flags) : false; }
	inline void EndTabItem()
	{
		if (auto i = GetInterface())
			i->EndTabItem();
	}

	inline bool BeginTable(const char* str_id, int column, TableFlags flags = TableFlags::kNone, const ImVec2& outer_size = ImVec2(0.0f, 0.0f), float inner_width = 0.0f) { return GetInterface() ? GetInterface()->BeginTable(str_id, column, static_cast<int>(flags), outer_size, inner_width) : false; }
	inline void EndTable()
	{
		if (auto i = GetInterface())
			i->EndTable();
	}
	inline void TableSetupColumn(const char* label, TableColumnFlags flags = TableColumnFlags::kNone, float init_width_or_weight = 0.0f, std::uint32_t user_id = 0)
	{
		if (auto i = GetInterface())
			i->TableSetupColumn(label, static_cast<int>(flags), init_width_or_weight, user_id);
	}
	inline void TableNextRow(int row_flags = 0, float min_row_height = 0.0f)
	{
		if (auto i = GetInterface())
			i->TableNextRow(row_flags, min_row_height);
	}
	inline bool TableNextColumn() { return GetInterface() ? GetInterface()->TableNextColumn() : false; }
	inline void TableHeadersRow()
	{
		if (auto i = GetInterface())
			i->TableHeadersRow();
	}

	inline void Columns(int count = 1, const char* id = nullptr, bool border = true)
	{
		if (auto i = GetInterface())
			i->Columns(count, id, border);
	}
	inline void NextColumn()
	{
		if (auto i = GetInterface())
			i->NextColumn();
	}

	inline bool CollapsingHeader(const char* label, int flags = 0) { return GetInterface() ? GetInterface()->CollapsingHeader(label, flags) : false; }

	// --- Widgets ---
	inline bool Button(const char* label) { return GetInterface() ? GetInterface()->Button(label) : false; }
	inline bool Checkbox(const char* label, bool* v, bool alignFar = true, bool labelLeft = true) { return GetInterface() ? GetInterface()->Checkbox(label, v, alignFar, labelLeft) : false; }
	inline bool Hotkey(const char* label, std::uint32_t key, std::int32_t modifier, std::int32_t modifier2, bool alignFar = true, bool labelLeft = true, bool flashing = false) { return GetInterface() ? GetInterface()->Hotkey(label, key, modifier, modifier2, alignFar, labelLeft, flashing) : false; }
	inline bool ToggleButton(const char* label, bool* v, bool alignFar = true, bool labelLeft = true) { return GetInterface() ? GetInterface()->ToggleButton(label, v, alignFar, labelLeft) : false; }
	inline bool InputText(const char* label, char* buf, size_t buf_size, int flags = 0) { return GetInterface() ? GetInterface()->InputText(label, buf, buf_size, flags) : false; }
	inline bool ColorEdit3(const char* label, float col[3], int flags = 0) { return GetInterface() ? GetInterface()->ColorEdit3(label, col, flags) : false; }
	inline bool SliderFloat(const char* label, float* v, float min, float max, const char* fmt = "%.3f") { return GetInterface() ? GetInterface()->SliderFloat(label, v, min, max, fmt) : false; }
	inline bool SliderInt(const char* label, int* v, int min, int max, const char* fmt = "%d") { return GetInterface() ? GetInterface()->SliderInt(label, v, min, max, fmt) : false; }
	inline bool DragFloat(const char* label, float* v, float speed = 1.0f, float min = 0.0f, float max = 0.0f, const char* fmt = "%.3f") { return GetInterface() ? GetInterface()->DragFloat(label, v, speed, min, max, fmt) : false; }
	inline bool DragInt(const char* label, int* v, float speed = 1.0f, int min = 0, int max = 0, const char* fmt = "%d") { return GetInterface() ? GetInterface()->DragInt(label, v, speed, min, max, fmt) : false; }
	inline bool Combo(const char* label, int* current_item, const char* const* items, int items_count) { return GetInterface() ? GetInterface()->Combo(label, current_item, items, items_count) : false; }
	inline bool ComboWithFilter(const char* label, int* current_item, const char* const* items, int items_count, int popup_max_height_in_items = -1) { return GetInterface() ? GetInterface()->ComboWithFilter(label, current_item, items, items_count, popup_max_height_in_items) : false; }
	inline bool ComboForm(const char* label, std::uint32_t* currentFormID, std::uint8_t formType) { return GetInterface() ? GetInterface()->ComboForm(label, currentFormID, formType) : false; }
	inline ImGuiTableSortSpecs* GetTableSortSpecs() { return GetInterface() ? GetInterface()->GetTableSortSpecs() : nullptr; }
	inline bool Selectable(const char* label, bool selected = false, int flags = 0, const ImVec2& size = ImVec2(0, 0)) { return GetInterface() ? GetInterface()->Selectable(label, selected, flags, size) : false; }

	inline void Header(const char* label)
	{
		if (auto i = GetInterface())
			i->Header(label);
	}
	inline void LeftLabel(const char* label)
	{
		if (auto i = GetInterface())
			i->LeftLabel(label);
	}
	inline void TextColored(const ImVec4& color, const char* fmt, ...)
	{
		auto i = GetInterface();
		if (!i)
			return;
		va_list args;
		va_start(args, fmt);
		char buf[1024];
		vsnprintf(buf, 1024, fmt, args);
		va_end(args);
		i->TextColored(color, buf);
	}
	inline void TextColoredWrapped(const ImVec4& col, const char* fmt, ...)
	{
		auto i = GetInterface();
		if (!i)
			return;
		va_list args;
		va_start(args, fmt);
		char buf[1024];
		vsnprintf(buf, 1024, fmt, args);
		va_end(args);
		i->TextColoredWrapped(col, buf);
	}
	inline void TextDisabled(const char* fmt, ...)
	{
		auto i = GetInterface();
		if (!i)
			return;
		va_list args;
		va_start(args, fmt);
		char buf[1024];
		vsnprintf(buf, 1024, fmt, args);
		va_end(args);
		i->TextDisabled(buf);
	}
	inline void Text(const char* fmt, ...)
	{
		auto i = GetInterface();
		if (!i)
			return;
		va_list args;
		va_start(args, fmt);
		char buf[1024];
		vsnprintf(buf, 1024, fmt, args);
		va_end(args);
		i->Text(buf);
	}
	inline void TextWrapped(const char* fmt, ...)
	{
		auto i = GetInterface();
		if (!i)
			return;
		va_list args;
		va_start(args, fmt);
		char buf[1024];
		vsnprintf(buf, 1024, fmt, args);
		va_end(args);
		i->TextWrapped(buf);
	}
	inline void TextUnformatted(const char* text, const char* text_end = nullptr)
	{
		if (auto i = GetInterface())
			i->TextUnformatted(text, text_end);
	}
	inline void CenteredText(const char* label, bool vertical = false)
	{
		if (auto i = GetInterface())
			i->CenteredText(label, vertical);
	}
	inline void SetTooltip(const char* fmt)
	{
		if (auto i = GetInterface())
			i->SetTooltip(fmt);
	}
	inline bool IsWidgetFocused(const char* label) { return GetInterface() ? GetInterface()->IsWidgetFocused(label) : false; }
	inline bool ButtonIconWithLabel(const char* label, void* textureID, const ImVec2& size, bool alignFar = true, bool labelLeft = true) { return GetInterface() ? GetInterface()->ButtonIconWithLabel(label, textureID, size.x, size.y, alignFar, labelLeft) : false; }
	inline bool ImageButton(const char* str_id, void* user_texture_id, const ImVec2& image_size, const ImVec4* tint = nullptr) { return GetInterface() ? GetInterface()->ImageButton(str_id, user_texture_id, image_size.x, image_size.y, tint) : false; }
	inline void Stepper(const char* label, const char* text, bool* outLeft, bool* outRight)
	{
		if (auto i = GetInterface())
			i->Stepper(label, text, outLeft, outRight);
	}

	// ==========================================
	// CONVENIENCE OVERLOADS
	// ==========================================

	// Convert vector<string> to const char** locally on stack for ABI safety
	inline bool Combo(const char* label, int* current_item, const std::vector<std::string>& items)
	{
		std::vector<const char*> ptrs(items.size());
		for (size_t i = 0; i < items.size(); ++i) ptrs[i] = items[i].c_str();
		return Combo(label, current_item, ptrs.data(), static_cast<int>(ptrs.size()));
	}

	inline bool ComboWithFilter(const char* label, int* current_item, const std::vector<std::string>& items, int popup_max_height_in_items = -1)
	{
		std::vector<const char*> ptrs(items.size());
		for (size_t i = 0; i < items.size(); ++i) ptrs[i] = items[i].c_str();
		return ComboWithFilter(label, current_item, ptrs.data(), static_cast<int>(ptrs.size()), popup_max_height_in_items);
	}

	inline bool InputText(const char* label, std::string* str, int flags = 0)
	{
		// Ensure the string has a reasonable minimum capacity
		constexpr size_t kMinCapacity = 256;
		if (str->capacity() < kMinCapacity)
			str->reserve(kMinCapacity);

		// Resize to capacity to provide full buffer
		const size_t oldSize = str->size();
		str->resize(str->capacity());

		// Call the base InputText with the expanded buffer
		const bool changed = InputText(label, str->data(), str->capacity() + 1, flags);

		if (changed) {
			// Trim to actual string length
			str->resize(std::strlen(str->c_str()));
		} else {
			// Restore original size if unchanged
			str->resize(oldSize);
		}

		return changed;
	}

	// ==========================================
	// TEMPLATES
	// ==========================================

	template <typename T>
	bool EnumStepper(const char* label, T* current_val, const std::vector<std::string>& items)
	{
		if (items.empty())
			return false;
		int idx = static_cast<int>(*current_val);
		if (idx < 0)
			idx = 0;
		if (idx >= (int)items.size())
			idx = (int)items.size() - 1;
		bool l = false, r = false;
		Stepper(label, Translate(items[idx].c_str()), &l, &r);
		if (l) {
			*current_val = static_cast<T>((idx - 1 + (int)items.size()) % (int)items.size());
			return true;
		}
		if (r) {
			*current_val = static_cast<T>((idx + 1) % (int)items.size());
			return true;
		}
		return false;
	}

	template <class E>
	bool EnumSlider(const char* label, E* index, const std::vector<std::string>& enum_names, bool a_translate = true)
	{
		bool value_changed = false;
		std::size_t uIndex = (std::is_enum_v<E>) ? static_cast<std::size_t>(*index) : *index;

		LeftLabel(label);

		std::string centerText = a_translate ? std::string(Translate(enum_names[uIndex].c_str())) : enum_names[uIndex];

		bool hovered, clickedLeft, clickedRight;
		if (auto i = GetInterface()) {
			i->CenteredTextWithArrows(label, centerText.c_str(), &hovered, &clickedLeft, &clickedRight);
		} else {
			return false;
		}

		if (hovered || IsWidgetFocused(label)) {
			// Using ImGui standard keys for navigation
			const bool pL = clickedLeft || ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft);
			const bool pR = clickedRight || ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight);
			if (pL)
				uIndex = (uIndex - 1 + enum_names.size()) % enum_names.size();
			if (pR)
				uIndex = (uIndex + 1) % enum_names.size();
			if (pL || pR) {
				value_changed = true;
				*index = static_cast<E>(uIndex);
			}
		}
		return value_changed;
	}

	// Managed Hotkey struct
	struct ManagedHotkey
	{
		std::uint32_t kKey = 0, gKey = 0;
		std::int32_t kMod1 = -1, gMod1 = -1;
		std::int32_t kMod2 = -1, gMod2 = -1;
		bool isBinding = false;
		bool wasTriggered = false;

		void Clear()
		{
			kKey = gKey = 0;
			kMod1 = gMod1 = kMod2 = gMod2 = -1;
			isBinding = false;
		}
	};

	inline void DrawManagedHotkey(const char* label, ManagedHotkey& h, bool alignFar = true)
	{
		bool inputIsGP = (GetInputDevice() == InputDevice::kGamepad);
		bool gpSlotValid = (h.gKey != 0) && IsGamepadKey(h.gKey);
		bool kbSlotValid = (h.kKey != 0) && !IsGamepadKey(h.kKey);
		bool showGP = inputIsGP;
		if (showGP && !gpSlotValid && kbSlotValid)
			showGP = false;
		if (!showGP && !kbSlotValid && gpSlotValid)
			showGP = true;

		std::uint32_t k = showGP ? h.gKey : h.kKey;
		std::int32_t m1 = showGP ? h.gMod1 : h.kMod1;
		std::int32_t m2 = showGP ? h.gMod2 : h.kMod2;

		std::string dynamicLabel;
		const char* finalLabel = label;
		if (h.isBinding) {
			dynamicLabel = std::string(Translate("$FUCK_Settings_PressKeyBind")) + "###" + label;
			finalLabel = dynamicLabel.c_str();
		}

		if (Hotkey(finalLabel, k, m1, m2, alignFar, true, h.isBinding)) {
			if (!h.isBinding) {
				h.isBinding = true;
				StartBinding(k, m1, m2);
			}
		}
	}

	inline bool UpdateManagedHotkey(const void* e, ManagedHotkey& h)
	{
		if (!h.isBinding)
			return false;
		if (!IsBinding()) {
			h.isBinding = false;
			return false;
		}

		std::uint32_t k;
		std::int32_t m1, m2;
		BindResult res = UpdateBinding(e, &k, &m1, &m2);

		if (res == BindResult::kBound) {
			bool isGamepadCode = IsGamepadKey(k);
			if (isGamepadCode) {
				h.gKey = k;
				h.gMod1 = m1;
				h.gMod2 = m2;
			} else {
				h.kKey = k;
				h.kMod1 = m1;
				h.kMod2 = m2;
			}
			h.isBinding = false;
			h.wasTriggered = true;
			return true;
		} else if (res == BindResult::kCancelled) {
			h.isBinding = false;
			return true;
		}
		return true;
	}

	inline bool ProcessManagedHotkey(const void*, ManagedHotkey& h)
	{
		if (h.isBinding)
			return false;
		bool pressed = false;
		auto checkMod = [](std::int32_t mod) -> bool {
			if (mod <= 0)
				return true;
			if (mod == 281 || mod == 282)
				return GetAnalogInput(static_cast<std::uint32_t>(mod)) > 0.4f;
			return IsInputDown(static_cast<std::uint32_t>(mod));
		};

		if (h.kKey != 0) {
			if (IsInputDown(h.kKey) && checkMod(h.kMod1) && checkMod(h.kMod2))
				pressed = true;
		}
		if (!pressed && h.gKey != 0) {
			if (IsInputDown(h.gKey) && checkMod(h.gMod1) && checkMod(h.gMod2))
				pressed = true;
		}

		if (pressed) {
			if (!h.wasTriggered) {
				h.wasTriggered = true;
				return true;
			}
		} else {
			h.wasTriggered = false;
		}
		return false;
	}

	inline void AbortManagedHotkey(ManagedHotkey& h)
	{
		if (h.isBinding) {
			AbortBinding();
			h.isBinding = false;
		}
	}
}

inline const char* operator""_T(const char* str, std::size_t)
{
	return FUCK::Translate(str);
}
