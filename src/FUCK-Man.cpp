#include "FUCK-Man.h"
#include "FUCK-Host.h"

#include "ImGui/Audio.h"
#include "ImGui/IconsFontAwesome6.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

#include "System/Compat.h"
#include "System/Hotkeys.h"
#include "System/Input.h"
#include "System/Settings.h"
#include "System/Utils.h"

// Where an edited window's handles were drawn last frame, so they can take the mouse over windows in front
struct EditHandles
{
	ImGuiContext* context = nullptr;  // VR draws its HUD panel in a context of its own
	ImGuiWindow*  window  = nullptr;
	int           frame   = -1;
	ImVec2        min;  // the outline
	ImVec2        max;
	float         rotation  = 0.0f;
	bool          scalable  = false;
	bool          rotatable = false;
	ImVec2        resetMin;  // the reset icon, when it's shown
	ImVec2        resetMax;
};

struct WindowState
{
	bool   wasOpen      = false;
	bool   isCollapsed  = false;
	bool   wasCollapsed = false;
	ImVec2 preCollapseSize{};
	ImVec2 pos{ -1.0f, -1.0f };
	ImVec2 size{ -1.0f, -1.0f };
	bool   hasLoadedPos = false;

	// Live state tracking to prevent host chrome from losing input
	ImVec2 livePos{ -1.0f, -1.0f };
	ImVec2 liveSize{ -1.0f, -1.0f };
	bool   isFocused = false;

	// Chrome hit-testing
	ImVec2 chromePos{ -1.0f, -1.0f };
	ImVec2 chromeSize{ -1.0f, -1.0f };

	// kEditable: pos is the top-left corner, or top-right with kAnchorRight
	float                   scale    = 1.0f;
	float                   rotation = 0.0f;
	ImVec2                  bodySize{};             // last frame's content size, without the edit margin
	bool                    dragging = false;
	FUCK::EditorResizeState resize;
	float                   resizeStartScale = 1.0f;
	FUCK::EditorRotateState rotate;
	float                   rotateStart = 0.0f;
	EditHandles             handles;

	// While scaling, the body's corner opposite the grabbed one stays put
	int    pinCorner   = -1;     // 0-3 clockwise from top-left, -1 = none
	ImVec2 pinPoint{};           // Where it's held, in screen space
	int    pinFrames   = 0;      // Frames left to hold it after letting go, while the size catches up
	bool   anchorRight = false;  // kAnchorRight, so pos is the top-right corner

	bool IsLayoutModified() const { return hasLoadedPos || std::abs(scale - 1.0f) > 0.001f || std::abs(rotation) > 0.001f; }
};
static StringMap<WindowState> s_windowStates;  // Maps using "PluginName|WindowId"

// kEditable windows, sizes at 1080p
static constexpr float kEditMargin   = 12.0f;  // room past the outline for the corner handles
static constexpr float kEditOutline  = 4.0f;   // outline distance from the window body
static constexpr float kRotateReach  = 36.0f;  // rotation handle's reach past the body's top edge
static constexpr float kMinEditScale = 0.5f;
static constexpr float kMaxEditScale = 10.0f;

// Set while a window is drawn, for GetWindowScale() and GetWindowRotation()
static float s_windowScale    = 1.0f;
static float s_windowRotation = 0.0f;

// Auto-Close list for Game Menus
static constexpr std::array<std::string_view, 18> s_closeOnOpen = {
	RE::ContainerMenu::MENU_NAME.data(),   RE::JournalMenu::MENU_NAME.data(),
	RE::InventoryMenu::MENU_NAME.data(),   RE::MapMenu::MENU_NAME.data(),
	RE::DialogueMenu::MENU_NAME.data(),    RE::MagicMenu::MENU_NAME.data(),
	RE::StatsMenu::MENU_NAME.data(),       RE::TweenMenu::MENU_NAME.data(),
	RE::FavoritesMenu::MENU_NAME.data(),   RE::MainMenu::MENU_NAME.data(),
	RE::TrainingMenu::MENU_NAME.data(),    RE::MessageBoxMenu::MENU_NAME.data(),
	RE::SleepWaitMenu::MENU_NAME.data(),   RE::TutorialMenu::MENU_NAME.data(),
	RE::LoadingMenu::MENU_NAME.data(),     RE::LockpickingMenu::MENU_NAME.data(),
	RE::BookMenu::MENU_NAME.data(),        RE::RaceSexMenu::MENU_NAME.data()
};

// Helper to keep windows within the visible viewport
static void ClampWindowToScreen(ImVec2& pos, const ImVec2& size)
{
	const ImGuiIO& io = ImGui::GetIO();

	if (io.DisplaySize.x < 100.0f || io.DisplaySize.y < 100.0f) {
		return;
	}

	// Simple AABB clamping
	if (pos.x + size.x > io.DisplaySize.x)
		pos.x = std::max(0.0f, io.DisplaySize.x - size.x);
	if (pos.y + size.y > io.DisplaySize.y)
		pos.y = std::max(0.0f, io.DisplaySize.y - size.y);
	if (pos.x < 0.0f)
		pos.x = 0.0f;
	if (pos.y < 0.0f)
		pos.y = 0.0f;
}

// ==================================================
// Glaze JSON Serialization Structs
// ==================================================
struct WindowSaveData
{
	float x         = -1.0f;
	float y         = -1.0f;
	float w         = -1.0f;
	float h         = -1.0f;
	bool  collapsed = false;
	float scale     = 1.0f;
	float rotation  = 0.0f;
};

struct ToolSaveData
{
	bool        bFavourited = false;
	bool        bHidden     = false;
	std::string sName       = "";
	std::string sGroup      = "";
};

struct PluginSaveData
{
	StringMap<ToolSaveData>   tools;
	StringMap<WindowSaveData> windows;
};

struct GroupSaveData
{
	bool        bFavourited = false;
	std::string sName       = "";
	int         iOrder      = 0;
};

struct WorkspaceSaveData
{
	WindowSaveData           mainWindow;
	StringMap<int>           toolOrder;
	StringMap<GroupSaveData> groups;
};

FUCKMan::FUCKMan()
{
	FUCK::GetInterface() = FUCK::Host::CreateInterface();

	Settings::Core.LoadDefaults([this](CSimpleIniA& ini) {
		this->LoadSettings(ini);
		this->_def = this->_cfg;
	});

	RegisterWindow(&_themeEditorWindow);
}

// ==================================================
// Registration & Callbacks
// ==================================================

void FUCKMan::RegisterTool(FUCK::ITool* a_tool)
{
	if (!a_tool || !a_tool->Name() || !a_tool->PluginName()) {
		logger::info("FUCK: Attempted to register a Tool with a null Name or PluginName.");
		return;
	}
	logger::info("FUCK: Registering Tool '{}' for plugin '{}'", a_tool->Name(), a_tool->PluginName());
	std::lock_guard lock(_pendingLock);
	_pendingCommands.push_back({ PendingCommand::Type::kAddTool, a_tool, nullptr });
}

void FUCKMan::RegisterWindow(FUCK::IWindow* a_window)
{
	if (!a_window || !a_window->Id() || !a_window->PluginName()) {
		logger::info("FUCK: Attempted to register a Window with a null Id or PluginName.");
		return;
	}
	logger::info("FUCK: Registering Window '{}' for plugin '{}'", a_window->Id(), a_window->PluginName());
	std::lock_guard lock(_pendingLock);
	_pendingCommands.push_back({ PendingCommand::Type::kAddWindow, nullptr, a_window });
}

void FUCKMan::UnregisterWindow(FUCK::IWindow* a_window)
{
	if (!a_window)
		return;
	std::lock_guard lock(_pendingLock);
	_pendingCommands.push_back({ PendingCommand::Type::kRemoveWindow, nullptr, a_window });
}

void FUCKMan::FlushPendingRegistrations()
{
	std::vector<PendingCommand> commands;
	{
		std::lock_guard lock(_pendingLock);
		if (_pendingCommands.empty())
			return;
		commands = std::move(_pendingCommands);
		_pendingCommands.clear();
	}

	for (auto& cmd : commands) {
		switch (cmd.type) {
		case PendingCommand::Type::kAddTool:
			{
				auto* tool = cmd.tool;
				if (std::find(_tools.begin(), _tools.end(), tool) != _tools.end())
					break;
				auto it = std::find_if(_tools.begin(), _tools.end(), [&](FUCK::ITool* existing) {
					return existing && (strcmp(existing->Name(), tool->Name()) == 0) && (strcmp(existing->PluginName(), tool->PluginName()) == 0);
				});
				if (it == _tools.end())
					_tools.push_back(tool);
				break;
			}
		case PendingCommand::Type::kAddWindow:
			{
				auto* win = cmd.window;
				if (std::find(_windows.begin(), _windows.end(), win) != _windows.end())
					break;
				auto it = std::find_if(_windows.begin(), _windows.end(), [&](FUCK::IWindow* existing) {
					return existing && (strcmp(existing->Id(), win->Id()) == 0) && (strcmp(existing->PluginName(), win->PluginName()) == 0);
				});
				if (it == _windows.end())
					_windows.push_back(win);
				break;
			}
		case PendingCommand::Type::kRemoveWindow:
			{
				auto* win = cmd.window;
				auto  it  = std::find(_windows.begin(), _windows.end(), win);
				if (it != _windows.end()) {
					std::string key = std::format("{}|{}", win->PluginName(), win->Id());
					s_windowStates.erase(key);
					_windows.erase(it);
				}
				break;
			}
		}
	}
}

// ==================================================
// Editable Windows
// ==================================================

float FUCKMan::GetCurrentWindowScale() const
{
	return s_windowScale;
}

float FUCKMan::GetCurrentWindowRotation() const
{
	return s_windowRotation;
}

void FUCKMan::ResetEditableLayout(WindowState& a_state)
{
	a_state.pos          = { -1.0f, -1.0f };
	a_state.hasLoadedPos = false;
	a_state.scale        = 1.0f;
	a_state.rotation     = 0.0f;
	a_state.pinCorner    = -1;
	SaveWorkspace();
}

// Red reset icon at the cursor, as text over an invisible button so it takes the click. Returns true when clicked
bool FUCKMan::DrawResetIcon(float a_rowHeight)
{
	ImVec2 iconSize = ImGui::CalcTextSize(ICON_FA_ARROW_ROTATE_LEFT);
	ImVec2 pos      = ImGui::GetCursorScreenPos();

	bool clicked = ImGui::InvisibleButton("##FUCK_LayoutReset", ImVec2(iconSize.x, a_rowHeight));
	bool hovered = ImGui::IsItemHovered();
	if (hovered)
		FUCK::SetTooltip("$FUCK_LayoutReset"_T);

	ImU32 col = hovered ? IM_COL32(255, 89, 89, 255) : IM_COL32(217, 51, 51, 255);
	ImGui::GetWindowDrawList()->AddText({ pos.x, pos.y + (a_rowHeight - iconSize.y) * 0.5f }, col, ICON_FA_ARROW_ROTATE_LEFT);
	return clicked;
}

// Matches where EditorResizeHandles and EditorRotateHandle in FUCK_API.h put their handles
static bool IsOverEditHandle(const EditHandles& a_handles, const ImVec2& a_mouse)
{
	const float  half   = FUCK::Scale(6.0f);  // corner half size, and the rotation handle's radius
	const ImVec2 center = { (a_handles.min.x + a_handles.max.x) * 0.5f, (a_handles.min.y + a_handles.max.y) * 0.5f };

	if (a_handles.scalable) {
		const ImVec2 corners[4] = { a_handles.min, { a_handles.max.x, a_handles.min.y }, a_handles.max, { a_handles.min.x, a_handles.max.y } };
		for (const auto& corner : corners) {
			ImVec2 c = FUCK::RotatePoint(corner, center, a_handles.rotation);
			if (std::abs(a_mouse.x - c.x) <= half && std::abs(a_mouse.y - c.y) <= half)
				return true;
		}
	}

	if (a_handles.rotatable) {
		ImVec2 c  = FUCK::RotatePoint({ center.x, a_handles.min.y - FUCK::Scale(24.0f) }, center, a_handles.rotation);
		float  dx = a_mouse.x - c.x;
		float  dy = a_mouse.y - c.y;
		if (dx * dx + dy * dy <= half * half)
			return true;
	}

	return a_handles.resetMax.x > a_handles.resetMin.x && ImRect(a_handles.resetMin, a_handles.resetMax).Contains(a_mouse);
}

// Handles are drawn over windows in front, but ImGui gives the mouse to the front window before anything is drawn.
// So before any window is drawn, give the mouse to an edited window whose handle it's over. What you see is what you grab
static void ClaimEditHandleHover()
{
	ImGuiContext& g = *GImGui;

	// Leave the mouse alone while something else has it, or a popup is open
	if (g.ActiveId != 0 || g.MovingWindow || ImGui::GetTopMostPopupModal())
		return;
	if (g.HoveredWindow && (g.HoveredWindow->RootWindow->Flags & ImGuiWindowFlags_Popup))
		return;

	const ImVec2 mouse = g.IO.MousePos;
	for (auto& [key, state] : s_windowStates) {
		const EditHandles& handles = state.handles;

		// Only handles drawn in this context last frame, so a closed window's old spots don't count
		if (handles.context != &g || !handles.window || handles.frame < g.FrameCount - 1 || !IsOverEditHandle(handles, mouse))
			continue;

		g.HoveredWindow                  = handles.window;
		g.HoveredWindowUnderMovingWindow = handles.window;
		return;
	}
}

// A rect's corner, 0-3 clockwise from top-left
static ImVec2 CornerOf(const ImVec2& a_min, const ImVec2& a_max, int a_index)
{
	return { a_index == 1 || a_index == 2 ? a_max.x : a_min.x, a_index >= 2 ? a_max.y : a_min.y };
}

// Places the body so its pinned corner sits on the pin, at the body's current size and angle.
// Rounded to whole pixels, as ImGui snaps the window to a pixel anyway, and a fractional pos would leave the body off-pixel
static void PinBodyCorner(WindowState& a_state, float a_rotation)
{
	const int    k      = a_state.pinCorner;
	const ImVec2 half   = { a_state.bodySize.x * 0.5f, a_state.bodySize.y * 0.5f };
	const ImVec2 offset = FUCK::RotatePoint({ (k == 1 || k == 2) ? half.x : -half.x, k >= 2 ? half.y : -half.y }, { 0.0f, 0.0f }, a_rotation);
	const ImVec2 center = { a_state.pinPoint.x - offset.x, a_state.pinPoint.y - offset.y };

	// pos is the body's top-left, or top-right with kAnchorRight
	a_state.pos          = { std::round(a_state.anchorRight ? center.x + half.x : center.x - half.x), std::round(center.y - half.y) };
	a_state.hasLoadedPos = true;
}

void FUCKMan::UpdateEditableWindow(WindowState& a_state, const ImVec2& a_pos, const ImVec2& a_size, const ImVec2& a_margin, bool a_scalable, bool a_rotatable)
{
	float  res      = FUCK::GetResolutionScale();
	float  outline  = kEditOutline * res;
	float  rotation = a_rotatable ? a_state.rotation : 0.0f;
	ImVec2 outMin   = { a_pos.x + a_margin.x - outline, a_pos.y + a_margin.y - outline };
	ImVec2 outMax   = { a_pos.x + a_size.x - a_margin.x + outline, a_pos.y + a_size.y - a_margin.y + outline };
	ImVec2 center   = { (outMin.x + outMax.x) * 0.5f, (outMin.y + outMax.y) * 0.5f };

	bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

	// Editor items sit in the margin, outside the content. Restored at the end so they don't count towards the
	// auto-sized window, as each frame's outline would otherwise grow around the last frame's handles
	ImGuiWindow* window       = ImGui::GetCurrentWindow();
	ImVec2       contentMax   = window->DC.CursorMaxPos;
	ImVec2       contentIdeal = window->DC.IdealMaxPos;

	bool busy        = a_state.resize.IsActive() || a_state.rotate.active || a_state.dragging;
	auto boundsState = busy    ? FUCK::EditorBoundsState::kSelected :
	                   hovered ? FUCK::EditorBoundsState::kHovered :
	                             FUCK::EditorBoundsState::kNormal;
	FUCK::DrawEditorBounds(outMin, outMax, boundsState, 2.0f, true, nullptr, rotation);

	if (a_scalable) {
		ImVec2 newMin, newMax;
		auto   result = FUCK::EditorResizeHandles(outMin, outMax, a_state.resize, newMin, newMax, hovered, true, { 50.0f, 50.0f }, { 100000.0f, 100000.0f }, rotation);

		if (result == FUCK::EditorHandleResult::kStarted) {
			a_state.resizeStartScale = a_state.scale;

			// Hold the body's corner rather than the outline's, as the outline sits a little outside it
			ImVec2 bodyMin    = { a_pos.x + a_margin.x, a_pos.y + a_margin.y };
			ImVec2 bodyMax    = { bodyMin.x + a_state.bodySize.x, bodyMin.y + a_state.bodySize.y };
			a_state.pinCorner = (a_state.resize.corner + 2) % 4;
			a_state.pinPoint  = FUCK::RotatePoint(CornerOf(bodyMin, bodyMax, a_state.pinCorner), center, rotation);
		}

		if (a_state.resize.IsActive())
			a_state.scale = std::clamp(a_state.resizeStartScale * a_state.resize.scale.x, kMinEditScale, kMaxEditScale);

		if (result == FUCK::EditorHandleResult::kFinished)
			a_state.pinFrames = 3;
	}

	// The body is sized from what was drawn, so it reaches its new size a frame or two after the scale changes.
	// Keep its held corner in place until then, and save once it's settled
	if (a_state.pinCorner >= 0) {
		PinBodyCorner(a_state, rotation);
		if (!a_state.resize.IsActive() && --a_state.pinFrames <= 0) {
			a_state.pinCorner = -1;
			SaveWorkspace();
		}
	}

	// Reset icon above the outline's top-right, once the window has been moved, scaled or rotated
	ImVec2 resetMin, resetMax;
	if (a_state.IsLayoutModified()) {
		float  rowH     = ImGui::GetFontSize();
		ImVec2 iconSize = ImGui::CalcTextSize(ICON_FA_ARROW_ROTATE_LEFT);
		resetMin        = { outMax.x - iconSize.x, outMin.y - outline - rowH };
		resetMax        = { outMax.x, outMin.y - outline };
		ImGui::SetCursorScreenPos(resetMin);
		if (DrawResetIcon(rowH))
			ResetEditableLayout(a_state);
	}

	// Drawn last, as it leaves the cursor after its handle
	if (a_rotatable) {
		auto result = FUCK::EditorRotateHandle(outMin, outMax, center, a_state.rotate, hovered, 15.0f, rotation);

		if (result == FUCK::EditorHandleResult::kStarted)
			a_state.rotateStart = a_state.rotation;
		if (a_state.rotate.active)
			a_state.rotation = std::remainder(a_state.rotateStart + a_state.rotate.angle, 360.0f);
		if (result == FUCK::EditorHandleResult::kFinished)
			SaveWorkspace();
	}

	// Remember where the handles are, so next frame they can take the mouse even with a window in front
	a_state.handles = { GImGui, window->RootWindow, ImGui::GetFrameCount(), outMin, outMax, rotation, a_scalable, a_rotatable, resetMin, resetMax };

	// Drag from any empty part of the window. Items and handles keep their clicks
	bool onHandle = a_state.resize.hovered || a_state.rotate.hovered;
	bool canDrag  = hovered && !busy && !onHandle && !ImGui::IsAnyItemHovered();
	if (canDrag && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		a_state.dragging = true;

	if (a_state.dragging) {
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ImVec2 delta = ImGui::GetIO().MouseDelta;
			a_state.pos.x += delta.x;
			a_state.pos.y += delta.y;
			a_state.hasLoadedPos = true;
		} else {
			a_state.dragging = false;
			SaveWorkspace();
		}
	}

	float nx = 0.0f, ny = 0.0f;
	if (!a_state.dragging && FUCK::WASDNudge(nx, ny, hovered)) {
		a_state.pos.x += nx * res;
		a_state.pos.y += ny * res;
		a_state.hasLoadedPos = true;
		SaveWorkspace();
	}

	window->DC.CursorMaxPos = contentMax;
	window->DC.IdealMaxPos  = contentIdeal;
}

// ==================================================
// Sidebar & Geometry Management
// ==================================================

void FUCKMan::LoadWorkspace()
{
	float scale            = ImGui::Renderer::GetResolutionScale();
	bool  mainWindowLoaded = false;

	// Load global workspace settings. (Main Window, Groups & Tool Order)
	std::string workspacePath = Settings::GetSingleton()->GetWorkspacePath();
	if (fs::exists(workspacePath)) {
		WorkspaceSaveData sd;
		std::string       wBuffer;
		if (auto err = glz::read_file_json(sd, workspacePath, wBuffer); !err) {
			// Apply Main Window state
			if (sd.mainWindow.x != -1.0f && sd.mainWindow.y != -1.0f) {
				_cfg.windowPos  = { sd.mainWindow.x * scale, sd.mainWindow.y * scale };
				_cfg.windowSize = { sd.mainWindow.w * scale, sd.mainWindow.h * scale };
				_isCollapsed    = sd.mainWindow.collapsed;

				_lastSavedPos         = _cfg.windowPos;
				_lastSavedSize        = _cfg.windowSize;
				_pendingWindowRestore = true;
				mainWindowLoaded      = true;
			}

			for (const auto& [k, v] : sd.toolOrder) {
				_toolOverrides[k].sortOrder = v;
			}
			for (const auto& [k, v] : sd.groups) {
				_groupOverrides[k].isFavourited = v.bFavourited;
				_groupOverrides[k].customName   = v.sName;
				_groupOverrides[k].sortOrder    = v.iOrder;
			}
		}
	}

	if (!mainWindowLoaded) {
		_cfg.windowPos  = { _def.windowPos.x * scale, _def.windowPos.y * scale };
		_cfg.windowSize = { _def.windowSize.x * scale, _def.windowSize.y * scale };
		_lastSavedPos   = _cfg.windowPos;
		_lastSavedSize  = _cfg.windowSize;
	}

	// Load per-plugin tool states and window geometries.
	std::string toolsPath = Settings::GetSingleton()->GetToolsPath();
	if (fs::exists(toolsPath)) {
		for (const auto& entry : fs::directory_iterator(toolsPath)) {
			if (entry.path().extension() == ".json") {
				std::string pluginName = entry.path().stem().string();

				PluginSaveData pd;
				std::string    pBuffer;
				if (auto err = glz::read_file_json(pd, entry.path().string(), pBuffer); !err) {
					for (const auto& [toolName, toolData] : pd.tools) {
						std::string key                  = std::format("{}|{}", pluginName, toolName);
						_toolOverrides[key].isFavourited = toolData.bFavourited;
						_toolOverrides[key].isHidden     = toolData.bHidden;
						_toolOverrides[key].customName   = toolData.sName;
						_toolOverrides[key].customGroup  = toolData.sGroup;
					}

					for (const auto& [winId, winData] : pd.windows) {
						std::string key = std::format("{}|{}", pluginName, winId);
						auto&       st  = s_windowStates[key];

						if (winData.x >= 0.0f && winData.y >= 0.0f) {
							st.pos          = { winData.x * scale, winData.y * scale };
							st.hasLoadedPos = true;
						}
						if (winData.w >= 0.0f && winData.h >= 0.0f) {
							st.size = { winData.w * scale, winData.h * scale };
						}
						st.isCollapsed = winData.collapsed;
						st.scale       = std::clamp(winData.scale, kMinEditScale, kMaxEditScale);
						st.rotation    = winData.rotation;
					}
				}
			}
		}
	}
}

void FUCKMan::SaveWorkspace()
{
	WorkspaceSaveData         sd;
	StringMap<PluginSaveData> pdMap;

	// Distribute Tool Overrides
	for (const auto& [key, over] : _toolOverrides) {
		auto pipe = key.find('|');
		if (pipe != std::string::npos) {
			std::string plugin   = key.substr(0, pipe);
			std::string toolName = key.substr(pipe + 1);

			if (over.isFavourited || over.isHidden || !over.customName.empty() || !over.customGroup.empty()) {
				ToolSaveData td;
				td.bFavourited = over.isFavourited;
				td.bHidden     = over.isHidden;
				td.sName       = over.customName;
				td.sGroup      = over.customGroup;

				pdMap[plugin].tools[toolName] = td;
			}

			if (over.sortOrder != 0) {
				sd.toolOrder[key] = over.sortOrder;
			}
		}
	}

	// Distribute Group Overrides
	for (const auto& [grp, over] : _groupOverrides) {
		if (over.isFavourited || !over.customName.empty() || over.sortOrder != 0) {
			GroupSaveData gd;
			gd.bFavourited = over.isFavourited;
			gd.sName       = over.customName;
			gd.iOrder      = over.sortOrder;

			sd.groups[grp] = gd;
		}
	}

	// Distribute Main Window Geometry
	float scale = ImGui::Renderer::GetResolutionScale();
	if (scale < 0.1f)
		scale = 1.0f;  // Safety clamp

	sd.mainWindow.x         = _cfg.windowPos.x / scale;
	sd.mainWindow.y         = _cfg.windowPos.y / scale;
	sd.mainWindow.w         = _cfg.windowSize.x / scale;
	sd.mainWindow.h         = _cfg.windowSize.y / scale;
	sd.mainWindow.collapsed = _isCollapsed;

	// Distribute Window Geometries (External plugins)
	for (const auto& [key, st] : s_windowStates) {
		auto pipe = key.find('|');
		if (pipe != std::string::npos) {
			std::string plugin = key.substr(0, pipe);
			std::string winId  = key.substr(pipe + 1);

			bool isCustomPos   = false;
			bool isUndecorated = false;
			bool isNoMove      = false;
			bool isEditable    = false;
			for (auto* w : _windows) {
				if (w->PluginName() == plugin && w->Id() == winId) {
					if (w->GetFlags() & FUCK::WindowFlags::kEditable)
						isEditable = true;
					if (w->GetFlags() & FUCK::WindowFlags::kCustomPosition)
						isCustomPos = true;
					if (w->GetFlags() & FUCK::WindowFlags::kNoDecoration)
						isUndecorated = true;
					if (w->GetFlags() & FUCK::WindowFlags::kNoMove)
						isNoMove = true;
					break;
				}
			}

			// Editable widgets only save what the user changed, so an untouched one keeps following its default
			if (isEditable) {
				if (!st.IsLayoutModified())
					continue;

				WindowSaveData wd;
				wd.x     = st.hasLoadedPos ? st.pos.x / scale : -1.0f;
				wd.y     = st.hasLoadedPos ? st.pos.y / scale : -1.0f;
				wd.scale    = st.scale;
				wd.rotation = st.rotation;

				pdMap[plugin].windows[winId] = wd;
				continue;
			}

			// Skip saving to JSON entirely for custom-positioned or unmovable widgets
			if (isCustomPos || isNoMove) {
				continue;
			}

			WindowSaveData wd;
			wd.x         = (st.pos.x  >= 0.0f) ? st.pos.x  / scale : -1.0f;
			wd.y         = (st.pos.y  >= 0.0f) ? st.pos.y  / scale : -1.0f;
			wd.w         = (st.size.x >= 0.0f) ? st.size.x / scale : -1.0f;
			wd.h         = (st.size.y >= 0.0f) ? st.size.y / scale : -1.0f;
			wd.collapsed = !isUndecorated && st.isCollapsed;

			pdMap[plugin].windows[winId] = wd;
		}
	}

	// Commit Workspace Settings
	std::string workspacePath = Settings::GetSingleton()->GetWorkspacePath();
	fs::create_directories(Settings::Core.GetConfigDirectory());
	std::string sBuffer;
	if (auto err = glz::write_file_json(sd, workspacePath, sBuffer); err) {
		logger::warn("Failed to write to {}", workspacePath);
	}

	// Commit Per-Plugin Settings
	std::string toolsPath = Settings::GetSingleton()->GetToolsPath();
	fs::create_directories(toolsPath);

	for (const auto& [plugin, pd] : pdMap) {
		std::string path = std::format("{}/{}.json", toolsPath, plugin);

		if (pd.tools.empty() && pd.windows.empty()) {
			if (fs::exists(path)) {
				fs::remove(path);
			}
		} else {
			std::string pBuffer;
			if (auto err = glz::write_file_json(pd, path, pBuffer); err) {
				logger::warn("Failed to write to {}", path);
			}
		}
	}
}

// ==================================================
// Engine State Synchronization
// ==================================================

bool FUCKMan::IsWindowSuppressed(const FUCK::IWindow* win) const
{
	if (!win->IsOpen())
		return true;

	FUCK::WindowFlags userFlags = win->GetFlags();

	// Check 'tm' console command. Flat only: see the same VR gate in
	// Draw()'s main workspace menu render pass.
	if (!REL::Module::IsVR() && !(userFlags & FUCK::WindowFlags::kRenderDuringTM)) {
		if (auto ui = RE::UI::GetSingleton()) {
			if (!ui->IsShowingMenus())
				return true;
		}
	}

	// Check game menu and camera states.
	if (userFlags & FUCK::WindowFlags::kCloseOnGameMenu) {
		if (auto ui = RE::UI::GetSingleton()) {
			for (const auto& m : s_closeOnOpen) {
				if (ui->IsMenuOpen(m)) {
					return true;
				}
			}
		}

		if (auto camera = RE::PlayerCamera::GetSingleton(); camera && camera->currentState) {
			const auto activeID = camera->currentState->id;
			// VR inserts kVR before kThirdPerson, shifting kBleedout; kVATS/kAutoVanity are unshifted
			const auto bleedoutID = REL::Module::IsVR() ? RE::CameraState::kVRBleedout : RE::CameraState::kBleedout;
			if (activeID == RE::CameraState::kVATS ||
				activeID == bleedoutID             ||
				activeID == RE::CameraState::kAutoVanity) {
				return true;
			}
		}
	}

	return false;
}

void FUCKMan::UpdateGameState()
{
	bool targetSoft    = _apiSoftPause;
	bool targetHard    = _apiHardPause;
	bool targetBlur    = false;
	bool targetHideHUD = false;
	bool targetVanity  = _isVanityBlocked;

	// Main menu state.
	bool targetiHUDDisabled = _isOpen;

	if (_isOpen) {
		targetVanity  = true;
		targetHideHUD = true;
		if (_cfg.globalPauseType == PauseType::kSoft)
			targetSoft = true;
		if (_cfg.globalPauseType == PauseType::kHard)
			targetHard = true;
	}

	// Window overrides (skip if suppressed by game state).
	for (auto* win : _windows) {
		if (!IsWindowSuppressed(win)) {
			FUCK::WindowFlags f = win->GetFlags();

			if (f & FUCK::WindowFlags::kPauseSoft)
				targetSoft = true;
			if (f & FUCK::WindowFlags::kPauseHard)
				targetHard = true;
			if (f & FUCK::WindowFlags::kBlurBackground)
				targetBlur = true;
			if (f & FUCK::WindowFlags::kBlockVanity)
				targetVanity = true;
			if (f & FUCK::WindowFlags::kHideHUD) {
				targetHideHUD      = true;
				targetiHUDDisabled = true;
			}
		}
	}

	// Apply States
	Compat::ImmersiveHUD::SetDisabled(targetiHUDDisabled);

	// HUD
	if (targetHideHUD != _isHudHidden) {
		// Flat only: there the menu draws over the game HUD, so it borrows the
		// WorldMapMode push to hide it. In VR the menu lives on ImGuiVRHelper's
		// in-scene panel instead, and mods that watch HUD-mode messages (e.g.
		// KillFeed) would misread the push as the world map opening.
		if (!REL::Module::IsVR()) {
			targetHideHUD ? RE::SendHUDMessage::PushHUDMode("WorldMapMode") : RE::SendHUDMessage::PopHUDMode("WorldMapMode");
		}
		_isHudHidden = targetHideHUD;
	}

	// Soft Pause
	if (targetSoft != _isGameSoftPaused) {
		if (auto main = RE::Main::GetSingleton())
			main->GetRuntimeData().freezeTime = targetSoft;
		_isGameSoftPaused = targetSoft;
	}

	// Hard Pause
	if (targetHard != _isGameHardPaused) {
		if (auto ui = RE::UI::GetSingleton()) {
			targetHard ? ui->numPausesGame++ : ui->numPausesGame--;
		}
		_isGameHardPaused = targetHard;
	}

	// Blur
	if (targetBlur != _isGameBlurred) {
		auto bm = RE::UIBlurManager::GetSingleton();
		targetBlur ? bm->IncrementBlurCount() : bm->DecrementBlurCount();
		_isGameBlurred = targetBlur;
	}

	// Vanity
	if (targetVanity && RE::PlayerCamera::GetSingleton()) {
		RE::PlayerCamera::GetSingleton()->GetRuntimeData2().idleTimer = 0.0f;
	}

	// Scaleform Movie Pausing (to prevent background hover detection)
	bool blockInput = IsInputBlocked();
	if (auto ui = RE::UI::GetSingleton()) {
		if (blockInput) {
			for (auto& [name, entry] : ui->menuMap) {
				if (entry.menu && entry.menu->OnStack() && entry.menu->uiMovie) {
					if (name == RE::CursorMenu ::MENU_NAME ||
						name == RE::Console ::MENU_NAME ||
						name == RE::LoadingMenu::MENU_NAME ||
						name == RE::HUDMenu ::MENU_NAME ||
						name == RE::FaderMenu ::MENU_NAME) {
						continue;
					}
					if (_pausedMenus.find(name.c_str()) == _pausedMenus.end()) {
						entry.menu->uiMovie->SetPause(true);
						_pausedMenus.insert(name.c_str());
					}
				}
			}
		} else if (!_pausedMenus.empty()) {
			for (const auto& name : _pausedMenus) {
				auto menu = ui->GetMenu(name);
				if (menu && menu->uiMovie) {
					menu->uiMovie->SetPause(false);
				}
			}
			_pausedMenus.clear();
		}
	}
}

// ==================================================
// Input Processing
// ==================================================

bool FUCKMan::ProcessAsyncInput(const RE::InputEvent* const* a_event)
{
	FlushPendingRegistrations();
	bool consumed   = false;
	bool wasBinding = MANAGER(Input)->IsBinding();

	// Active Tool Input (Priority)
	if (_activeTool && _activeTool->OnAsyncInput(a_event)) {
		consumed = true;
	}

	// Make sure we skip the global Escape-to-Close override if rebinding
	if (!consumed && !wasBinding && (_isOpen || IsInputBlocked())) {
		// ESC / Close Logic (Priority over Global Hotkeys)
		if (MANAGER(Input)->IsInputReleased(a_event, Hotkeys::Manager::EscapeKey())) {
			bool handled = false;

			// A. Close Child Windows with kCloseOnEsc flag
			for (auto* win : _windows) {
				if (!IsWindowSuppressed(win) && (win->GetFlags() & FUCK::WindowFlags::kCloseOnEsc)) {
					win->SetOpen(false);
					handled = true;
				}
			}

			// B. Close Main Menu
			if (!handled && _isOpen) {
				Close();
				handled = true;
			}

			if (handled) {
				consumed = true;
			}
		}
	}

	if (!consumed) {
		// Framework Global Hotkeys
		if (MANAGER(Hotkeys)->ProcessInput(a_event)) {
			consumed = true;
		}
	}

	if (!consumed) {
		// Background Tool Input
		for (auto* tool : _tools) {
			if (tool != _activeTool && tool->OnAsyncInput(a_event)) {
				consumed = true;
				break;
			}
		}
	}

	if (!consumed) {
		// Window input (skip suppressed windows).
		for (auto* win : _windows) {
			if (!IsWindowSuppressed(win) && win->OnAsyncInput(a_event)) {
				consumed = true;
				break;
			}
		}
	}

	UpdateGameState();

	// Block Game Input if Menu/Windows are blocking
	return consumed || IsInputBlocked();
}

// ==================================================
// Settings & State Management
// ==================================================

void FUCKMan::ResetSettings()
{
	_cfg = _def;

	auto hotkeys = MANAGER(Hotkeys);
	hotkeys->GetToggleHotkey().Clear();

	Settings::Core.LoadKeybindsDefaults([](CSimpleIniA& ini) {
		MANAGER(Hotkeys)->LoadHotKeys(ini);
	});

	SetCurrentFont(_cfg.currentFont);

	if (_isOpen) {
		ClampWindowToScreen(_cfg.windowPos, _cfg.windowSize);
		_pendingWindowRestore = true;
		ImGui::Styles::GetSingleton()->RefreshStyle();
	}

	_lastSavedPos  = _cfg.windowPos;
	_lastSavedSize = _cfg.windowSize;

	Save();
	SaveWorkspace();
	SaveKeybinds();

	Settings::GetSingleton()->Save(FileType::kStyle, [](CSimpleIniA& ini) {
		ini.Delete("Style", "sFont", true);
	});
}

void FUCKMan::LoadSettings(const CSimpleIniA& a_ini)
{
	float loadedScale = FUCK::INI::LoadFloat(a_ini, "Settings", "fUserScale", _def.userScale);
	_cfg.userScale    = std::clamp(loadedScale, 0.5f, 2.0f);

	_cfg.globalPauseType = FUCK::INI::LoadInt(a_ini, "Settings", "iGlobalPauseType", _def.globalPauseType);

	_cfg.injectSystemMenu = static_cast<InjectSystemMenuType>(FUCK::INI::LoadInt(a_ini, "Settings", "iInjectSystemMenu", static_cast<int>(_def.injectSystemMenu)));

	_cfg.sidebarOnRight        = FUCK::INI::LoadBool(a_ini, "Settings", "bSidebarOnRight",        _def.sidebarOnRight);
	_cfg.replaceHelpMenu       = FUCK::INI::LoadBool(a_ini, "Settings", "bReplaceHelpMenu",       _def.replaceHelpMenu);
	_cfg.injectSettingsSubmenu = FUCK::INI::LoadBool(a_ini, "Settings", "bInjectSettingsSubmenu", _def.injectSettingsSubmenu);
	_cfg.showSidebarFilter     = FUCK::INI::LoadBool(a_ini, "Settings", "bShowSidebarFilter",     _def.showSidebarFilter);
	_cfg.showSidebarFavourites = FUCK::INI::LoadBool(a_ini, "Settings", "bShowSidebarFavourites", _def.showSidebarFavourites);
	_cfg.groupFavourites       = FUCK::INI::LoadBool(a_ini, "Settings", "bGroupFavourites",       _def.groupFavourites);
	_cfg.muteAudio             = FUCK::INI::LoadBool(a_ini, "Settings", "bMuteAudio",             _def.muteAudio);

	_cfg.customSystemMenuName = a_ini.GetValue("Settings", "sSystemMenuName", _def.customSystemMenuName.c_str());
}

void FUCKMan::SaveSettings(CSimpleIniA& a_ini)
{
	FUCK::INI::SaveInt(a_ini, "Settings", "iGlobalPauseType", static_cast<int>(_cfg.globalPauseType), static_cast<int>(_def.globalPauseType));

	FUCK::INI::SaveInt(a_ini, "Settings", "iInjectSystemMenu", static_cast<int>(_cfg.injectSystemMenu), static_cast<int>(_def.injectSystemMenu));

	FUCK::INI::SaveDouble(a_ini, "Settings", "fUserScale", _cfg.userScale, _def.userScale);

	FUCK::INI::SaveBool(a_ini, "Settings", "bSidebarOnRight",        _cfg.sidebarOnRight,        _def.sidebarOnRight);
	FUCK::INI::SaveBool(a_ini, "Settings", "bReplaceHelpMenu",       _cfg.replaceHelpMenu,       _def.replaceHelpMenu);
	FUCK::INI::SaveBool(a_ini, "Settings", "bInjectSettingsSubmenu", _cfg.injectSettingsSubmenu, _def.injectSettingsSubmenu);
	FUCK::INI::SaveBool(a_ini, "Settings", "bShowSidebarFilter",     _cfg.showSidebarFilter,     _def.showSidebarFilter);
	FUCK::INI::SaveBool(a_ini, "Settings", "bShowSidebarFavourites", _cfg.showSidebarFavourites, _def.showSidebarFavourites);
	FUCK::INI::SaveBool(a_ini, "Settings", "bGroupFavourites",       _cfg.groupFavourites,       _def.groupFavourites);
	FUCK::INI::SaveBool(a_ini, "Settings", "bMuteAudio",             _cfg.muteAudio,             _def.muteAudio);

	FUCK::INI::SaveString(a_ini, "Settings", "sSystemMenuName", _cfg.customSystemMenuName.c_str(), _def.customSystemMenuName.c_str());
}

void FUCKMan::Save()
{
	Settings::Core.Save([this](CSimpleIniA& ini) { SaveSettings(ini); });
}

void FUCKMan::SaveKeybinds()
{
	Settings::Core.SaveKeybinds([](CSimpleIniA& ini) { MANAGER(Hotkeys)->SaveHotKeys(ini); });
}

void FUCKMan::SetVanityBlocked(bool blocked) { _isVanityBlocked = blocked; }
void FUCKMan::SetManualHardPause(bool paused) { _apiHardPause = paused; }
void FUCKMan::SetManualSoftPause(bool paused) { _apiSoftPause = paused; }
void FUCKMan::SetForceCursor(bool force) { _forceCursor = force; }
void FUCKMan::SuspendRendering(bool suspend) { _suspendRendering = suspend; }

void FUCKMan::SetCurrentFont(const std::string& a_font)
{
	_cfg.currentFont = a_font;
	IconFont::Manager::GetSingleton()->SetFontName(a_font);
}

// ==================================================
// Accessors & Queries
// ==================================================

bool FUCKMan::ShouldRender() const
{
	if (_suspendRendering)
		return false;

	// Keep rendering if the main menu is open.
	if (_isOpen)
		return true;

	// Keep render loop active for open windows to maintain ImGui context across load screens.
	for (const auto* win : _windows) {
		if (win->IsOpen())
			return true;
	}

	return false;
}

bool FUCKMan::ShouldRenderExclusive() const
{
	if (_suspendRendering)
		return false;

	if (_isOpen)
		return true;

	for (const auto* win : _windows) {
		if (win->IsOpen() && !(win->GetFlags() & FUCK::WindowFlags::kPassInputToGame))
			return true;
	}

	return false;
}

bool FUCKMan::ShouldRenderPassthrough() const
{
	// Independent of _isOpen: these live on their own always-on HUD panel in
	// VR, so they render whether or not the main menu happens to be open too.
	if (_suspendRendering)
		return false;

	for (const auto* win : _windows) {
		if (win->IsOpen() && (win->GetFlags() & FUCK::WindowFlags::kPassInputToGame))
			return true;
	}

	return false;
}

bool FUCKMan::IsInputBlocked() const
{
	if (_isOpen)
		return true;

	ImVec2 mouse = ImGui::GetMousePos();

	for (const auto* win : _windows) {
		if (IsWindowSuppressed(win)) {
			continue;
		}

		std::string key = std::format("{}|{}", win->PluginName(), win->Id());
		auto        it  = s_windowStates.find(key);
		if (it != s_windowStates.end()) {
			const auto& winState = it->second;

			// Protect active interactions (dragging window or using widgets)
			if (winState.isFocused && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				return true;
			}

			// Protect Host Chrome
			if (winState.chromePos.x != -1.0f) {
				if (mouse.x >= winState.chromePos.x && mouse.x <= winState.chromePos.x + winState.chromeSize.x &&
					mouse.y >= winState.chromePos.y && mouse.y <= winState.chromePos.y + winState.chromeSize.y) {
					return true;
				}
			}
		}

		// Trust the plugin's requested flags for the content area
		if (!(win->GetFlags() & FUCK::WindowFlags::kPassInputToGame)) {
			return true;
		}
	}
	return false;
}

bool FUCKMan::IsCursorForced() const
{
	return _forceCursor;
}

bool FUCKMan::HasWindowWithFlag(FUCK::WindowFlags a_flag) const
{
	for (const auto* win : _windows) {
		// Deliberately skips IsWindowSuppressed. Used internally to check intended window behaviors.
		if (win->IsOpen() && (win->GetFlags() & a_flag)) {
			return true;
		}
	}
	return false;
}

// ==================================================
// Open / Close Logic
// ==================================================

void FUCKMan::Open()
{
	if (_isOpen)
		return;

	_isOpen = true;

	_isCollapsed = false;

	_forceCursor = false;

	ImGui::ClearNavState();

	ImGui::Styles::GetSingleton()->OnStyleRefresh();

	if (_activeTool)
		_activeTool->OnOpen();

	_pendingWindowRestore = true;

	ImGui::PlayAudio(ImGui::Audio::kOk);
}

void FUCKMan::Close()
{
	FUCK::AbortBinding();

	if (!_isOpen)
		return;
	_isOpen = false;

	if (_activeTool)
		_activeTool->OnClose();

	_forceCursor = false;

	if (!IsInputBlocked()) {
		MANAGER(Input)->ClearState();
	}

	UpdateGameState();

	ImGui::ClearNavState();
	ImGui::PlayAudio(ImGui::Audio::kCancel);
}

void FUCKMan::Toggle()
{
	_isOpen ? Close() : Open();
}

// Vanilla DirectionHandler repeat timings. Some weird interactions have led to keys spamming. A lot of logging and testing of input code
// has led seemingly to the conclusion that the DirectionHandler is being zeroed out by some other code, and this is a workaround to restore it when it's caught.
namespace DirectionRepeat
{
	static constexpr std::ptrdiff_t kDelayOffset    = 0x14;
	static constexpr std::ptrdiff_t kIntervalOffset = 0x18;

	static float s_delay    = 0.0f;
	static float s_interval = 0.0f;

	static RE::MenuEventHandler* GetHandler()
	{
		// Layout only verified on flat.
		if (REL::Module::IsVR())
			return nullptr;

		auto mc = RE::MenuControls::GetSingleton();
		if (!mc)
			return nullptr;

		const auto vtbl = RE::VTABLE_DirectionHandler[0].address();
		for (auto* h : mc->handlers) {
			if (h && *reinterpret_cast<const std::uintptr_t*>(h) == vtbl)
				return h;
		}
		return nullptr;
	}

	static void Sync()
	{
		auto handler = GetHandler();
		if (!handler)
			return;

		auto  base     = reinterpret_cast<std::uint8_t*>(handler);
		auto& delay    = *reinterpret_cast<float*>(base + kDelayOffset);
		auto& interval = *reinterpret_cast<float*>(base + kIntervalOffset);

		if (delay > 0.0f && interval > 0.0f) {
			s_delay    = delay;
			s_interval = interval;
		} else if (s_delay > 0.0f && s_interval > 0.0f) {
			delay    = s_delay;
			interval = s_interval;
			logger::info("FUCK: Restored menu direction repeat ({}, {})", s_delay, s_interval);
		}
	}
}

EventResult FUCKMan::ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
{
	if (!a_event)
		return EventResult::kContinue;

	DirectionRepeat::Sync();

	DispatchMenuEvent(a_event->menuName.c_str(), a_event->opening);

	if (a_event->opening) {
		if (a_event->menuName == RE::MainMenu::MENU_NAME) {
			ImGui::ClearFormCaches();
		}

		// Close automatically behind dominant fullscreen game menus
		if (std::ranges::find(s_closeOnOpen, a_event->menuName.data()) != s_closeOnOpen.end()) {
			bool closedSomething = false;

			if (_isOpen && a_event->menuName != RE::Console::MENU_NAME) {
				Close();
				closedSomething = true;
			}

			if (closedSomething) {
				UpdateGameState();
				ImGui::ClearNavState();
			}
		}
	}

	return RE::BSEventNotifyControl::kContinue;
}

// ==================================================
// CORE RENDER LOOP
// ==================================================

void FUCKMan::Draw(bool a_passthroughOnly)
{
	FlushPendingRegistrations();

	if (!_workspaceLoaded) {
		LoadWorkspace();
		_workspaceLoaded = true;
	}

	if (auto ui = RE::UI::GetSingleton(); ui && ui->closingAllMenus) {
		if (_isOpen) {
			Close();
			ImGui::ClearNavState();
		}
		return;
	}

	UpdateGameState();

	// Before any window is drawn, so it decides who gets the mouse this frame
	if (_isOpen)
		ClaimEditHandleHover();

	// If the user hides menus via 'tm', the Skyrim Scaleform cursor becomes invisible.
	// We force ImGui to draw its own cursor so KBM users aren't clicking blind. Flatrim only.
	bool menusHidden = false;
	if (!REL::Module::IsVR()) {
		if (auto ui = RE::UI::GetSingleton()) {
			menusHidden = !ui->IsShowingMenus();
		}
	}

	if (menusHidden && IsInputBlocked() && Input::Manager::GetSingleton()->IsInputKBM()) {
		ImGui::GetIO().MouseDrawCursor = true;
	} else {
		ImGui::GetIO().MouseDrawCursor = false;
	}

	// Global mouse click detection for auto-closing flyout windows
	bool        isAnyMouseDown    = MANAGER(Input)->IsInputDown(Input::Keymap::kMBBase) || MANAGER(Input)->IsInputDown(Input::Keymap::kMBBase + 1);
	static bool s_wasAnyMouseDown = false;
	bool        globalMouseClick  = isAnyMouseDown && !s_wasAnyMouseDown;
	s_wasAnyMouseDown             = isAnyMouseDown;

	// ==================================================
	// LAYOUT METRICS SETUP (Chrome / Unscaled)
	// ==================================================
	auto iconArrow = MANAGER(IconFont)->GetStepperRight();

	struct LayoutMetrics
	{
		float uiScale;
		float textH;
		float padBase;

		float iconAspect;
		float chromeIconBaseH;  // = 16.0f * uiScale  (userScale intentionally excluded for chrome)

		// Titlebar
		float titleH;
		float titleFontSize;
		float titleIconPadX;
		float titleIconNudgeY;
		float titleTextOffsetY;

		// Sidebar
		float sidebarWidth;
		float sidebarItemH;
		float sidebarFontSize;
		float sidebarIndent;
	} m;

	m.uiScale = FUCK::GetResolutionScale();
	m.textH   = FUCK::GetTextLineHeight();
	m.padBase = 15.0f * m.uiScale;

	m.iconAspect = (iconArrow && iconArrow->imageSize.y > 0.0f) ? (iconArrow->imageSize.x / iconArrow->imageSize.y) : 1.0f;

	m.chromeIconBaseH = 20.0f * m.uiScale;

	float headerPadding = 3.0f * m.uiScale;

	const float kChromeFontSize = 22.0f * 0.9f * m.uiScale;

	m.titleH           = m.textH + (headerPadding * 2.0f);
	m.titleFontSize    = kChromeFontSize;
	m.titleH           = m.titleFontSize + (headerPadding * 4.0f);
	m.titleIconPadX    = 8.0f * m.uiScale;
	m.titleIconNudgeY  = 1.0f * m.uiScale;  // positive = down, negative = up
	m.titleTextOffsetY = (m.titleH - m.titleFontSize) * 0.5f + (1.0f * m.uiScale);

	m.sidebarWidth    = 250.0f * m.uiScale;
	m.sidebarItemH    = 30.0f * m.uiScale;
	m.sidebarFontSize = kChromeFontSize;
	m.sidebarIndent   = 15.0f * m.uiScale;

	auto chromeArrow = [&](bool pointsDown, float rowH) {
		return ImGui::CalcArrowIconParams(m.iconAspect, pointsDown, rowH, m.chromeIconBaseH);
	};

	// Content scale helper — respects kIgnoreUserScale flag
	auto pushContentScale = [&](bool ignoreUserScale = false) {
		_isIgnoringUserScale = ignoreUserScale;
		_activeScale         = ignoreUserScale ? 1.0f : _cfg.userScale;

		float targetFontSize = ImGui::GetStyle().FontSizeBase * _activeScale * m.uiScale;
		ImGui::PushFont(nullptr, targetFontSize);

		auto styleUser = ImGui::Styles::GetSingleton()->user;

		float spaceX = styleUser.itemSpacing.x * m.uiScale * _activeScale;
		float spaceY = styleUser.itemSpacing.y * m.uiScale * _activeScale;

		float innerSpaceX = 4.0f * m.uiScale * _activeScale;
		float innerSpaceY = 4.0f * m.uiScale * _activeScale;

		float indentSpacing = styleUser.indentSpacing;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f * m.uiScale * _activeScale, 3.0f * m.uiScale * _activeScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spaceX, spaceY));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(innerSpaceX, innerSpaceY));
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indentSpacing * m.uiScale * _activeScale);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12.0f * m.uiScale);
		ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 10.0f * m.uiScale);
	};

	auto popContentScale = [&]() {
		ImGui::PopFont();
		ImGui::PopStyleVar(6);
		_activeScale         = _cfg.userScale;
		_isIgnoringUserScale = false;
	};

	// ==================================================
	// OVERLAY RENDER PASS (Fullscreen Transparent Layer)
	// ==================================================
	// Belongs on the exclusive-focus panel only.
	if (!a_passthroughOnly && (_activeTool || ShouldRender())) {
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
		ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
		                         ImGuiWindowFlags_NoInputs |
		                         ImGuiWindowFlags_NoBackground |
		                         ImGuiWindowFlags_NoNav |
		                         ImGuiWindowFlags_NoBringToFrontOnFocus |
		                         ImGuiWindowFlags_NoFocusOnAppearing;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (ImGui::Begin("##ToolOverlayLayer", nullptr, flags)) {
			pushContentScale(false);

			if (_activeTool && !menusHidden) {
				_currentRenderingPlugin = _activeTool->PluginName();
				_activeTool->RenderOverlay();
			}

			for (auto* tool : _tools) {
				if (tool != _activeTool && !menusHidden) {
					_currentRenderingPlugin = tool->PluginName();
					tool->RenderOverlay();
				}
			}

			for (auto* win : _windows) {
				// Skip overlays for suppressed windows.
				if (!IsWindowSuppressed(win)) {
					_currentRenderingPlugin = win->PluginName();
					win->RenderOverlay();
				}
			}

			_currentRenderingPlugin.clear();

			popContentScale();
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	// ==================================================
	// EXTERNAL WINDOWS RENDER PASS
	// ==================================================
	for (auto* win : _windows) {

		// --- Setup & State ---
		std::string key      = std::format("{}|{}", win->PluginName(), win->Id());
		auto&       winState = s_windowStates[key];

		// --- Trigger Event Listeners automatically if state changed! ---
		bool currentOpen = win->IsOpen();
		bool wasOpen     = winState.wasOpen;
		if (currentOpen != winState.wasOpen) {
			winState.wasOpen = currentOpen;
			DispatchWindowEvent(win->PluginName(), win->Id(), currentOpen);
		}

		// Skip rendering if the game state suppresses the window.
		if (IsWindowSuppressed(win)) {
			continue;
		}

		// In VR, kPassInputToGame windows always render on the separate,
		// always-on HUD pass instead (see ImGui::Renderer::DrawHud) — split
		// by which pass this call is for. Flat has no separate pass, so they
		// stay in the single normal draw there (a_passthroughOnly is always
		// false on flat; see ImGui::Renderer::Draw).
		bool isPassthroughWindow = (win->GetFlags() & FUCK::WindowFlags::kPassInputToGame) != 0;
		if (a_passthroughOnly ? !isPassthroughWindow : (isPassthroughWindow && REL::Module::IsVR())) {
			continue;
		}

		const char*       title     = win->Title();
		FUCK::WindowFlags userFlags = win->GetFlags();

		ImGuiWindowFlags flags = ImGuiWindowFlags_None;

		// --- Flags Setup ---
		bool noDecoration    = (userFlags & FUCK::WindowFlags::kNoDecoration);
		bool ignoreUserScale = (userFlags & FUCK::WindowFlags::kIgnoreUserScale);
		bool noResize        = (userFlags & FUCK::WindowFlags::kNoResize);
		bool autoResize      = (userFlags & FUCK::WindowFlags::kAutoResize);
		bool noMove          = (userFlags & FUCK::WindowFlags::kNoMove);

		bool editable    = (userFlags & FUCK::WindowFlags::kEditable);
		bool scalable    = editable && (userFlags & FUCK::WindowFlags::kScalable);
		bool rotatable   = editable && (userFlags & FUCK::WindowFlags::kRotatable);
		bool anchorRight = editable && (userFlags & FUCK::WindowFlags::kAnchorRight);
		bool isEditing   = editable && _isOpen;

		winState.anchorRight = anchorRight;

		// While editing, the margin grows the window past the outline so its handles sit over it
		ImVec2 editMargin = { 0.0f, 0.0f };
		if (isEditing) {
			float margin = kEditMargin * m.uiScale;
			editMargin   = { margin, margin };

			// Room for the handles at any angle, so turning never resizes or moves the window.
			// A fixed-size window's body is its default size, as its margin is added on top below
			if (rotatable) {
				ImVec2 body  = autoResize ? winState.bodySize : win->GetDefaultSize();
				ImVec2 half  = { body.x * 0.5f, body.y * 0.5f };
				float  reach = std::max(std::sqrt(half.x * half.x + half.y * half.y) + margin, half.y + kRotateReach * m.uiScale);
				editMargin   = { std::max(margin, reach - half.x), std::max(margin, reach - half.y) };
			}

			// Room above the outline for the reset icon, kept even while it's hidden so the window doesn't jump
			editMargin.y = std::max(editMargin.y, margin + ImGui::GetFontSize());

			// Whole pixels. ImGui snaps the window to a pixel, so a fractional margin shifts the body by a sub-pixel
			// amount that can change its measured size, which changes the margin again next frame and jitters
			editMargin = { std::ceil(editMargin.x), std::ceil(editMargin.y) };
		}

		flags |= ImGuiWindowFlags_NoTitleBar;
		if (noDecoration) {
			winState.isCollapsed  = false;
			winState.wasCollapsed = false;
			flags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
		} else {
			flags |= ImGuiWindowFlags_NoScrollbar;
		}

		if (noResize)
			flags |= ImGuiWindowFlags_NoResize;
		if (autoResize)
			flags |= ImGuiWindowFlags_AlwaysAutoResize;
		// Editable windows are moved by the Host's own drag below, not ImGui's
		if (noMove || editable)
			flags |= ImGuiWindowFlags_NoMove;

		bool poppedInvisibleBg = false;
		if (userFlags & FUCK::WindowFlags::kNoBackground) {
			if (!IsInputBlocked()) {
				flags |= ImGuiWindowFlags_NoBackground;
			} else {
				// Give kNoBackground windows a 0.1% invisible background so they can be easily dragged.
				// We also suppress the Border, otherwise ImGui draws a 1px grey frame.
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.001f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
				poppedInvisibleBg = true;
			}
		}

		// --- Collapse Logic ---
		bool isCollapsed      = winState.isCollapsed;
		bool wasCollapsed     = winState.wasCollapsed;
		winState.wasCollapsed = isCollapsed;

		// Get Metrics from Interface
		ImVec2 targetSize = win->GetDefaultSize();

		// Handle collapse override
		if (isCollapsed) {
			float targetW = (winState.size.x > 0.0f) ? winState.size.x : (winState.preCollapseSize.x > 0.0f) ? winState.preCollapseSize.x :
			                                                                                                   targetSize.x;
			targetSize.x  = targetW;
			flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize;
		} else if (wasCollapsed) {
			// Prefer the in-session captured pre-collapse size; fall back to persisted size
			if (winState.preCollapseSize.x > 0.0f)
				targetSize = winState.preCollapseSize;
			else if (winState.size.x > 0.0f)
				targetSize = winState.size;
		}

		// --- Position Logic ---
		bool isCustomPos = (userFlags & FUCK::WindowFlags::kCustomPosition);

		if (editable) {
			// Follow the default until the user moves it, so a consumer's default can change with its size
			if (!winState.hasLoadedPos)
				winState.pos = win->GetDefaultPos();

			// Last frame's width places a right-anchored window. The margin keeps the content where it was
			float  left   = anchorRight ? winState.pos.x - winState.bodySize.x : winState.pos.x;
			ImVec2 winPos = { left - editMargin.x, winState.pos.y - editMargin.y };
			FUCK::SetNextWindowPos(winPos, ImGuiCond_Always);
		} else if (!isCustomPos) {
			if (noMove) {
				// Rigidly enforce the default position for immovable windows
				ImVec2 defPos = win->GetDefaultPos();
				ClampWindowToScreen(defPos, targetSize);
				FUCK::SetNextWindowPos(defPos, ImGuiCond_Always);
			} else if (winState.hasLoadedPos) {
				ClampWindowToScreen(winState.pos, targetSize);
				FUCK::SetNextWindowPos(winState.pos, ImGuiCond_FirstUseEver);  // Applies at game launch
			} else {
				// Default position for new windows
				ImVec2 defPos = win->GetDefaultPos();
				ClampWindowToScreen(defPos, targetSize);
				FUCK::SetNextWindowPos(defPos, ImGuiCond_FirstUseEver);
			}
		}

		// If AutoResize is active, ImGui shrinks the window dynamically. We bypass hard size forcing.
		if (editable && !autoResize && !isCollapsed) {
			// The edit margin is added around the body rather than taken out of it, so the body keeps its size
			// whether or not it's being edited. Editable windows are resized by their scale handles, not their edges
			flags |= ImGuiWindowFlags_NoResize;
			FUCK::SetNextWindowSize({ targetSize.x + editMargin.x * 2.0f, targetSize.y + editMargin.y * 2.0f }, ImGuiCond_Always);
		} else if (!autoResize) {
			if (isCollapsed) {
				ImGui::SetNextWindowSizeConstraints(ImVec2(targetSize.x, 0.0f), ImVec2(targetSize.x, FLT_MAX));
			} else {
				ImVec2    sz       = ((winState.size.x != -1.0f) ? winState.size : targetSize);
				ImGuiCond sizeCond = (wasCollapsed || noResize) ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
				FUCK::SetNextWindowSize(sz, sizeCond);
			}
		}

		// --- Host Chrome Interaction Protection ---
		// Works from the live position, so custom-positioned windows are covered too
		bool mouseOverChrome = false;
		if (!noDecoration) {
			ImVec2 mouse      = ImGui::GetMousePos();
			ImVec2 checkPos   = (winState.livePos.x != -1.0f) ? winState.livePos : (winState.hasLoadedPos ? winState.pos : win->GetDefaultPos());
			float  checkWidth = (winState.liveSize.x != -1.0f) ? winState.liveSize.x : targetSize.x;

			float paddingY      = (userFlags & FUCK::WindowFlags::kNoBackground) ? 0.0f : m.padBase;
			float chromeBottomY = checkPos.y + paddingY + m.titleH + 1.0f;

			// When collapsed, the entire window is basically just the chrome + padding.
			if (winState.isCollapsed) {
				float checkHeight = (winState.liveSize.y != -1.0f) ? winState.liveSize.y : (paddingY * 2.0f + m.titleH + 1.0f);
				chromeBottomY     = checkPos.y + checkHeight;
			}

			winState.chromePos  = checkPos;
			winState.chromeSize = ImVec2(checkWidth, chromeBottomY - checkPos.y);

			if (mouse.x >= checkPos.x && mouse.x <= checkPos.x + checkWidth && mouse.y >= checkPos.y && mouse.y <= chromeBottomY) {
				mouseOverChrome = true;
			}
		} else {
			winState.chromePos = ImVec2(-1.0f, -1.0f);
		}

		bool isDraggingWindow = winState.isFocused && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		if ((userFlags & FUCK::WindowFlags::kPassInputToGame) && !IsInputBlocked() && !mouseOverChrome && !isDraggingWindow) {
			flags |= ImGuiWindowFlags_NoInputs;
		}

		bool open = true;

		// While editing, the margin grows the window past the outline so the corner handles sit over it
		if (!noDecoration || (userFlags & FUCK::WindowFlags::kNoBackground)) {
			// Push 0 padding for decorated windows and background-less overlays
			FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, editMargin);
		} else {
			// Give standard padding to frameless windows that still have a solid background panel
			FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m.padBase + editMargin.x, m.padBase + editMargin.y));
		}

		// Combine the display title with the Host's unique tracking key
		std::string windowLabel = std::format("{}###{}", title, key);

		// Use the formatted string instead of raw title
		if (FUCK::BeginWindow(windowLabel.c_str(), &open, flags)) {
			if (poppedInvisibleBg) {
				ImGui::PopStyleColor(2);  // Clear WindowBg and Border
			}

			ImVec2 curPos  = FUCK::GetWindowPos();
			ImVec2 curSize = FUCK::GetWindowSize();

			// Update live state for chrome protection
			winState.livePos   = curPos;
			winState.liveSize  = curSize;
			winState.isFocused = ImGui::IsWindowFocused();

			// An auto-sized window is capped to the screen, so a big enough widget would read back smaller than it is.
			// Work its size out from the content instead, which isn't capped. It's the same sum ImGui uses to fit the window
			ImVec2 fullSize = curSize;
			if (editable && autoResize) {
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				fullSize            = { window->ContentSizeIdeal.x + window->WindowPadding.x * 2.0f + window->DecoOuterSizeX1 + window->DecoOuterSizeX2,
							   window->ContentSizeIdeal.y + window->WindowPadding.y * 2.0f + window->DecoOuterSizeY1 + window->DecoOuterSizeY2 };
			}

			if (editable) {
				winState.bodySize = { fullSize.x - editMargin.x * 2.0f, fullSize.y - editMargin.y * 2.0f };
				s_windowScale     = scalable ? winState.scale : 1.0f;
				s_windowRotation  = rotatable ? winState.rotation : 0.0f;
			}

			// Auto-Close on Click Outside
			if (open && wasOpen && (userFlags & FUCK::WindowFlags::kCloseOnClickOutside)) {
				if (globalMouseClick && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
					if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows)) {
						open = false;
					}
				}
			}

			// Auto-save settings on move/resize end preventing overwrite during collapse.
			// Editable windows save their own position, which isn't the window's corner
			if (!editable && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				bool posChanged  = !isCustomPos && !noMove && (std::abs(curPos.x - winState.pos.x) > 1.0f || std::abs(curPos.y - winState.pos.y) > 1.0f);
				bool sizeChanged = !isCollapsed && (std::abs(curSize.x - winState.size.x) > 1.0f || std::abs(curSize.y - winState.size.y) > 1.0f);

				if (posChanged || sizeChanged) {
					if (posChanged)
						winState.pos = curPos;
					if (sizeChanged)
						winState.size = curSize;
					if (posChanged)
						winState.hasLoadedPos = true;
					SaveWorkspace();
				}
			}

			if (win->GetFlags() & FUCK::WindowFlags::kExtendBorder)
				FUCK::ExtendWindowPastBorder();

			if (!noDecoration) {
				// --- Window Chrome Decoration (unscaled) ---
				float winWidth = FUCK::GetWindowSize().x;

				ImVec2 headerStartCursor = FUCK::GetCursorPos();
				ImVec2 cursorScreen      = FUCK::GetCursorScreenPos();

				FUCK::BeginGroup();

				// 1. Collapse Icon
				float iconW = 0.0f;

				if (iconArrow) {
					// Calculate exact physical dimensions of the arrow first
					bool pointsDown = !isCollapsed;
					auto ap         = chromeArrow(pointsDown, m.titleH);

					// Lock horizontal container width to its maximum dimension to prevent shifting on rotation
					float maxIconDim = std::max(ap.drawSize.x, ap.drawSize.y);
					float btnWidth   = (m.titleIconPadX * 2.0f) + maxIconDim;

					// Tightly wrap the invisible button around the graphic
					if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(btnWidth, m.titleH))) {
						winState.isCollapsed = !isCollapsed;
						if (!isCollapsed) {
							// Capture the true live size before we force it to titleH next frame
							winState.preCollapseSize = FUCK::GetWindowSize();
							// Also persist it as the canonical size so cross-session restore works
							winState.size = winState.preCollapseSize;
						}
						SaveWorkspace();
					}
					bool  isHovered = ImGui::IsItemHovered();
					ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

					float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

					ImVec2 drawPos = cursorScreen;
					drawPos.x += m.titleIconPadX + iconOffsetX;
					drawPos.y += ap.offsetY + m.titleIconNudgeY;

					ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
						pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);

					// Give the title text slightly more breathing room past the exact button width
					iconW = btnWidth + m.titleIconPadX;
				}

				// 2. Title Text
				ImFont* baseFont = ImGui::GetFont();

				FUCK::SetCursorPos({ iconW, m.titleTextOffsetY });
				ImGui::GetWindowDrawList()->AddText(baseFont, m.titleFontSize,
					FUCK::GetCursorScreenPos(), ImGui::GetColorU32(ImGuiCol_Text), title);

				// 3. Close Button
				const float btnSize = m.titleH;
				const float btnX    = winWidth - btnSize - headerPadding;

				FUCK::SetCursorPos({ btnX, 0 });
				if (ImGui::InvisibleButton("##WinClose", ImVec2(btnSize, btnSize))) {
					open = false;
				}

				{
					const char* xIcon      = ICON_FA_XMARK;
					float       uiFontSize = ImGui::GetStyle().FontSizeBase * m.uiScale;

					ImGui::PushFont(nullptr, uiFontSize);
					ImVec2 textSize = ImGui::CalcTextSize(xIcon);
					ImGui::PopFont();

					ImVec2 btnScreenPos = ImGui::GetItemRectMin();
					ImVec2 textPos      = {
                        btnScreenPos.x + (btnSize - textSize.x) * 0.5f,
                        btnScreenPos.y + (btnSize - textSize.y) * 0.5f + (1.0f * m.uiScale)
					};

					ImU32 xColor = ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

					ImGui::GetWindowDrawList()->AddText(
						baseFont,
						uiFontSize,
						textPos,
						xColor,
						xIcon);
				}

				FUCK::EndGroup();

				// Double Click Header Interaction
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
						winState.isCollapsed = !isCollapsed;
						if (!isCollapsed) {
							winState.preCollapseSize = FUCK::GetWindowSize();
							winState.size            = winState.preCollapseSize;
						}
						SaveWorkspace();
					}
				}

				// 4. Separator
				FUCK::SetCursorPos({ headerStartCursor.x, m.titleH });
				FUCK::SeparatorThick();

				// 5. Content Child (scaled)
				if (!winState.isCollapsed && !isCollapsed) {
					FUCK::SetCursorPosX(0.0f);

					FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m.padBase, m.padBase));

					ImGuiChildFlags  childFlags  = ImGuiChildFlags_AlwaysUseWindowPadding;
					ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground;

					if (ImGui::BeginChild("##Content", ImVec2(0, 0), childFlags, windowFlags)) {
						ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
						pushContentScale(ignoreUserScale);
						_currentRenderingPlugin = win->PluginName();

						// So SetWindowPos/Size from Draw() apply to the window, not this child
						FUCK::Host::SetContentChild(ImGui::GetCurrentWindow());
						win->Draw();
						FUCK::Host::SetContentChild(nullptr);

						_currentRenderingPlugin.clear();
						popContentScale();
						ImGui::PopItemWidth();
					}
					ImGui::EndChild();

					FUCK::PopStyleVar();
				}
			} else {
				// --- No Chrome Decoration ---
				// Rotated content can reach past the window's corners, so don't clip it to the window
				if (rotatable)
					ImGui::PushClipRect({ 0.0f, 0.0f }, ImGui::GetIO().DisplaySize, false);

				pushContentScale(ignoreUserScale);
				_currentRenderingPlugin = win->PluginName();
				win->Draw();
				_currentRenderingPlugin.clear();
				popContentScale();

				if (rotatable)
					ImGui::PopClipRect();
			}

			if (isEditing) {
				UpdateEditableWindow(winState, curPos, fullSize, editMargin, scalable, rotatable);
			}
		}
		FUCK::EndWindow();

		s_windowScale    = 1.0f;
		s_windowRotation = 0.0f;

		FUCK::PopStyleVar();

		if (!open) {
			win->SetOpen(false);
			UpdateGameState();

			if (!IsInputBlocked()) {
				MANAGER(Input)->ClearState();
			}
		}
	}

	// ==================================================
	// MAIN FUCK WORKSPACE MENU RENDER PASS
	// ==================================================

	if (a_passthroughOnly || !_isOpen || menusHidden)
		return;

	ImGui::GetIO().MouseDrawCursor = false;

	const float collapsedH = m.titleH * 1.02f;

	if (_pendingWindowRestore) {
		ClampWindowToScreen(_cfg.windowPos, _cfg.windowSize);
		ImGui::SetNextWindowPos(_cfg.windowPos, ImGuiCond_Always);
		if (!_isCollapsed) {
			ImGui::SetNextWindowSize(_cfg.windowSize, ImGuiCond_Always);
		} else {
			ImGui::SetNextWindowSize(ImVec2(_cfg.windowSize.x, collapsedH), ImGuiCond_Always);
		}
		_pendingWindowRestore = false;
	} else {
		if (_isCollapsed) {
			ImGui::SetNextWindowSize(ImVec2(_cfg.windowSize.x, collapsedH));
		} else if (_wasCollapsed && !_isCollapsed) {
			ImGui::SetNextWindowSize(_cfg.windowSize);
		} else {
			ImGui::SetNextWindowSize(_cfg.windowSize, ImGuiCond_FirstUseEver);
		}
	}

	_wasCollapsed = _isCollapsed;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	std::string windowTitle = std::format("##{}", "$FUCK_Title"_T);
	bool        wantsOpen   = true;

	ImGuiWindowFlags winFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
	if (_isCollapsed)
		winFlags |= ImGuiWindowFlags_NoResize;

	if (ImGui::Begin(windowTitle.c_str(), &wantsOpen, winFlags)) {
		ImGui::ExtendWindowPastBorder();

		if (!_isCollapsed) {
			_cfg.windowPos  = ImGui::GetWindowPos();
			_cfg.windowSize = ImGui::GetWindowSize();

			// Auto-save settings on move/resize end
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				if (_cfg.windowPos.x != _lastSavedPos.x || _cfg.windowPos.y != _lastSavedPos.y ||
					_cfg.windowSize.x != _lastSavedSize.x || _cfg.windowSize.y != _lastSavedSize.y) {
					_lastSavedPos  = _cfg.windowPos;
					_lastSavedSize = _cfg.windowSize;
					SaveWorkspace();
				}
			}
		}

		// -- Custom Title Bar (unscaled) --
		{
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
			ImGui::BeginGroup();

			float winWidth = ImGui::GetWindowSize().x;

			ImVec2 cursorScreen = ImGui::GetCursorScreenPos();

			// 1. Collapse Icon
			if (iconArrow) {
				// Calculate exact physical dimensions of the arrow first
				bool pointsDown = !_isCollapsed;
				auto ap         = chromeArrow(pointsDown, m.titleH);

				float maxIconDim = std::max(ap.drawSize.x, ap.drawSize.y);
				float btnWidth   = (m.titleIconPadX * 2.0f) + maxIconDim;

				ImGui::SetCursorPos({ 0, 0 });
				// Tightly wrap the invisible button around the graphic
				if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(btnWidth, m.titleH))) {
					_isCollapsed = !_isCollapsed;
				}

				bool  isHovered = ImGui::IsItemHovered();
				ImU32 iconColor = isHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				float iconOffsetX = (maxIconDim - ap.drawSize.x) * 0.5f;

				ImVec2 drawPos = cursorScreen;
				drawPos.x += m.titleIconPadX + iconOffsetX;
				drawPos.y += ap.offsetY + m.titleIconNudgeY;

				ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
					pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
			}

			// 2. Close Button
			float btnSize = m.titleH;
			float xPos    = winWidth - btnSize - headerPadding;

			ImGui::SetCursorPos({ xPos, 0.0f });
			ImVec2 btnCursor = ImGui::GetCursorScreenPos();

			if (ImGui::InvisibleButton("##CloseBtn", ImVec2(btnSize, btnSize))) {
				wantsOpen = false;
			}

			{
				const char* xIcon      = ICON_FA_XMARK;
				float       uiFontSize = ImGui::GetStyle().FontSizeBase * m.uiScale;

				ImGui::PushFont(nullptr, uiFontSize);
				ImVec2 textSize = ImGui::CalcTextSize(xIcon);
				ImGui::PopFont();

				ImVec2 textPos = {
					btnCursor.x + (btnSize - textSize.x) * 0.5f,
					btnCursor.y + (btnSize - textSize.y) * 0.5f + (1.0f * m.uiScale)
				};

				ImU32 xColor = ImGui::IsItemHovered() ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);

				ImGui::GetWindowDrawList()->AddText(
					ImGui::GetFont(),
					uiFontSize,
					textPos,
					xColor,
					xIcon);
			}

			ImGui::EndGroup();
			ImGui::PopStyleVar();

			// Double Click Header
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
				ImVec2 mousePos = ImGui::GetMousePos();
				ImVec2 wP       = ImGui::GetWindowPos();
				if (mousePos.x >= wP.x && mousePos.x <= wP.x + winWidth &&
					mousePos.y >= wP.y && mousePos.y <= wP.y + m.titleH) {
					_isCollapsed = !_isCollapsed;
				}
			}
		}

		// -- Content --
		if (!_isCollapsed && !_wasCollapsed) {
			float contentY = m.titleH;
			ImGui::SetCursorPos({ 0, contentY });

			float availHeight = ImGui::GetContentRegionAvail().y;

			// -- Sidebar (unscaled) --
			auto renderSidebar = [&]() {
				const float topPadding       = 2.0f * m.uiScale;
				const float bottomPadding    = 2.0f * m.uiScale;
				const float textVisualOffset = 1.0f * m.uiScale;

				ImFont* regularFont = MANAGER(IconFont)->GetRegularFont();

				auto GetOverrides = [&](FUCK::ITool* t) -> ToolOverrideState& {
					return _toolOverrides[std::format("{}|{}", t->PluginName(), t->Name())];
				};

				std::vector<FUCK::ITool*>            favTools;
				std::vector<FUCK::ITool*>            looseTools;
				StringMap<std::vector<FUCK::ITool*>> toolGroups;

				ImGui::BeginChild("Sidebar", ImVec2(m.sidebarWidth, availHeight), true, ImGuiWindowFlags_None);
				{
					ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
					ImVec2 headerStart = ImGui::GetCursorPos();
					headerStart.y += topPadding;

					// --- HEADER: TOOLS (Centred) ---
					ImGui::SetCursorPos(headerStart);

					// Bypassing ImGui::PushFont scaling
					ImGui::PushFont(regularFont, m.sidebarFontSize);
					float       textHeightCalc = ImGui::GetTextLineHeight();
					float       sidebarAvailW  = ImGui::GetContentRegionAvail().x;
					const char* headerText     = "$FUCK_Tools"_T;
					float       headerW        = ImGui::CalcTextSize(headerText).x;

					ImGui::SetCursorPosX(headerStart.x + (sidebarAvailW - headerW) * 0.5f);
					ImGui::SetCursorPosY(headerStart.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset);
					ImGui::TextUnformatted(headerText);
					ImGui::PopFont();

					ImGui::SetCursorPos(ImVec2(headerStart.x, headerStart.y + m.sidebarItemH));
					ImGui::SeparatorThick();

					// --- FILTER BOX ---
					static char filterBuf[128] = "";
					if (_cfg.showSidebarFilter) {
						ImGui::PushFont(regularFont, m.sidebarFontSize * 0.8f);
						ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f * m.uiScale, 6.0f * m.uiScale));
						ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f * m.uiScale);
						ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f * m.uiScale);

						// Sync background with the active theme's input box styling
						ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetUserStyleColorVec4(ImGui::USER_STYLE::kComboBoxTextBox));

						ImGui::SetNextItemWidth(-1.0f);

						// CTRL+F to focus the sidebar filter
						if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
							ImGui::SetKeyboardFocusHere();
						}

						ImGui::InputTextWithHint("##SidebarFilter", "$FUCK_Sidebar_FilterHint"_T, filterBuf, sizeof(filterBuf));

						ImGui::PopStyleColor(1);
						ImGui::PopStyleVar(3);
						ImGui::PopFont();

						ImGui::Dummy(ImVec2(0.0f, 4.0f * m.uiScale));
					}

					bool        filtering = filterBuf[0] != '\0';
					std::string lowerFilter;
					if (filtering) {
						lowerFilter = clib_util::string::tolower(filterBuf);
					}

					static std::string lowerNameBuffer;

					// Evaluate and distribute tools into collections
					for (auto* tool : _tools) {
						if (!tool->ShowInSidebar())
							continue;

						auto& over = GetOverrides(tool);
						if (over.isHidden)
							continue;

						std::string resolvedName = over.customName.empty() ? tool->Name() : over.customName;

						if (filtering) {
							// Copy to buffer and lower-case in-place to avoid allocations
							lowerNameBuffer = resolvedName;
							std::transform(lowerNameBuffer.begin(), lowerNameBuffer.end(), lowerNameBuffer.begin(),
								[](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });

							auto score = rapidfuzz::fuzz::partial_token_ratio(lowerFilter, lowerNameBuffer);
							if (score < 65.0)
								continue;
						}

						if (_cfg.showSidebarFavourites && over.isFavourited) {
							favTools.push_back(tool);
						} else {
							// Use the exact same resolution logic as the Settings Table
							std::string resolvedGrp = over.customGroup;
							if (resolvedGrp.empty())
								resolvedGrp = tool->Group() ? tool->Group() : "";
							if (resolvedGrp == "##ROOT")
								resolvedGrp = "";

							if (resolvedGrp.empty())
								looseTools.push_back(tool);
							else
								toolGroups[resolvedGrp].push_back(tool);
						}
					}

					auto sortToolsAlphabetically = [&](FUCK::ITool* a, FUCK::ITool* b) {
						auto& oa = GetOverrides(a);
						auto& ob = GetOverrides(b);
						if (oa.sortOrder != ob.sortOrder)
							return oa.sortOrder < ob.sortOrder;
						std::string na = oa.customName.empty() ? a->Name() : oa.customName;
						std::string nb = ob.customName.empty() ? b->Name() : ob.customName;
						return _stricmp(na.c_str(), nb.c_str()) < 0;
					};

					auto sortToolsByRegOrder = [&](FUCK::ITool* a, FUCK::ITool* b) {
						return GetOverrides(a).sortOrder < GetOverrides(b).sortOrder;
					};

					struct SidebarEntry
					{
						const char*                label;
						const char*                origGroup;
						bool                       isGroup   = false;
						FUCK::ITool*               tool      = nullptr;
						std::vector<FUCK::ITool*>* tools     = nullptr;
						int                        sortOrder = 0;
					};

					std::vector<SidebarEntry> entries;

					if (!favTools.empty()) {
						std::sort(favTools.begin(), favTools.end(), sortToolsAlphabetically);
						if (_cfg.groupFavourites) {
							entries.push_back({ "$FUCK_Sidebar_FavouritesGroup"_T, "", true, nullptr, &favTools, -1000000 });
						} else {
							for (auto* t : favTools) {
								auto&       over = GetOverrides(t);
								const char* name = over.customName.empty() ? t->Name() : over.customName.c_str();
								entries.push_back({ name, "", false, t, nullptr, -1000000 });
							}
						}
					}

					for (auto* t : looseTools) {
						auto&       over = GetOverrides(t);
						const char* name = over.customName.empty() ? t->Name() : over.customName.c_str();
						entries.push_back({ name, "", false, t, nullptr, over.sortOrder });
					}

					for (auto& [name, tools] : toolGroups) {
						std::stable_sort(tools.begin(), tools.end(), sortToolsByRegOrder);
						auto&       gOver      = _groupOverrides[name];
						const char* dispName   = gOver.customName.empty() ? name.c_str() : gOver.customName.c_str();
						int         groupOrder = (_cfg.showSidebarFavourites && gOver.isFavourited) ? -999999 : gOver.sortOrder;
						entries.push_back({ dispName, name.c_str(), true, nullptr, &tools, groupOrder });
					}

					std::sort(entries.begin(), entries.end(), [](const SidebarEntry& a, const SidebarEntry& b) {
						if (a.sortOrder != b.sortOrder)
							return a.sortOrder < b.sortOrder;
						return _stricmp(a.label, b.label) < 0;
					});

					auto  apRight           = chromeArrow(false, m.sidebarItemH);
					float alignedTextOffset = (m.sidebarIndent * 0.5f) + apRight.drawSize.x + (10.0f * m.uiScale);

					auto RenderSidebarItem = [&](FUCK::ITool* tool, const char* label, float extraIndent = 0.0f) {
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						if (window->SkipItems)
							return;  // FIX: Prevents EXCEPTION_ACCESS_VIOLATION during collapse

						ImGui::PushID(tool);

						bool  isSelected = (_activeTool == tool);
						auto& over       = GetOverrides(tool);

						// Cache both Screen and Window positions before the Selectable moves the cursor!
						const auto startScreenPos = ImGui::GetCursorScreenPos();
						const auto startPos       = ImGui::GetCursorPos();

						// Draw Selectable (Full width background & highlight)
						if (ImGui::Selectable("##SidebarItem", isSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, m.sidebarItemH))) {
							if (_activeTool != tool) {
								if (_activeTool) {
									Input::Manager::GetSingleton()->AbortBinding();
									_activeTool->OnClose();
								}
								ImGui::PlayAudio(ImGui::Audio::kOk);
								_activeTool = tool;
								_activeTool->OnOpen();
							}
						}

						bool itemHovered = ImGui::IsItemHovered(0);
						bool itemFocused = ImGui::IsItemFocused();

						// Add Gamepad 'Y' shortcut to favourite tools on the sidebar
						if ((itemHovered || itemFocused) && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) {
							over.isFavourited = !over.isFavourited;
							SaveWorkspace();
						}

						ImVec2 endPos = ImGui::GetCursorPos();  // Save position after selectable

						// Bypassing ImGui::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);
						float textY = startPos.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset;

						ImGui::SetCursorPos({ startPos.x + alignedTextOffset + extraIndent, textY });
						ImGui::TextUnformatted(label);

						// Draw Star
						if (_cfg.showSidebarFavourites && (over.isFavourited || itemHovered)) {
							const char* starIcon     = ICON_FA_STAR;
							float       starScale    = 0.85f;
							float       starFontSize = m.sidebarFontSize * starScale;

							ImGui::PushFont(regularFont, starFontSize);
							ImVec2 starSize = ImGui::CalcTextSize(starIcon);
							ImGui::PopFont();

							float starX = startScreenPos.x + sidebarAvailW - starSize.x - (15.0f * m.uiScale);
							float starY = startScreenPos.y + (m.sidebarItemH - starFontSize) * 0.5f - (1.0f * m.uiScale);

							// Hit-testing logic bypassing ImGui Item overlap clashing
							ImRect starBB(
								ImVec2(starX - (5.0f * m.uiScale), startScreenPos.y),
								ImVec2(starX + starSize.x + (5.0f * m.uiScale), startScreenPos.y + m.sidebarItemH));
							bool starHovered = ImGui::IsMouseHoveringRect(starBB.Min, starBB.Max);

							if (starHovered && ImGui::IsMouseClicked(0)) {
								over.isFavourited = !over.isFavourited;
								SaveWorkspace();
							}

							ImU32 starCol = starHovered ? IM_COL32(255, 255, 100, 255) : (over.isFavourited ? IM_COL32(255, 215, 0, 255) : IM_COL32(150, 150, 150, 150));

							ImGui::GetWindowDrawList()->AddText(regularFont, starFontSize,
								ImVec2(starX, starY),
								starCol, starIcon);
						}

						ImGui::PopFont();
						ImGui::SetCursorPos(endPos);  // Restore cursor to next line
						ImGui::PopID();
					};

					auto RenderSidebarGroup = [&](const char* groupName, const char* origGroup, std::vector<FUCK::ITool*>& tools) {
						ImGuiWindow* window = ImGui::GetCurrentWindow();
						if (window->SkipItems)
							return;

						// Bypassing ImGui::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);

						// Custom TreeNode rendering
						ImGui::PushID(groupName);
						ImGuiID id     = window->GetID(groupName);
						bool    isOpen = window->DC.StateStorage->GetInt(id, 0);

						ImVec2 pos = window->DC.CursorPos;
						ImRect bb(pos, pos + ImVec2(ImGui::GetContentRegionAvail().x, m.sidebarItemH));

						ImGui::ItemSize(bb);
						if (ImGui::ItemAdd(bb, id)) {
							bool hovered, held;
							if (ImGui::ButtonBehavior(bb, id, &hovered, &held, 0)) {
								isOpen = !isOpen;
								window->DC.StateStorage->SetInt(id, isOpen);
								ImGui::PlayAudio(isOpen ? ImGui::Audio::kFocus : ImGui::Audio::kCancel);
							}
							if (hovered)
								ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_HeaderHovered), false);

							bool isFocused = ImGui::IsItemFocused();

							// Add Gamepad 'Y' shortcut to favourite groups on the sidebar
							if ((hovered || isFocused) && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) {
								if (origGroup && origGroup[0] != '\0') {
									auto& gOver        = _groupOverrides[origGroup];
									gOver.isFavourited = !gOver.isFavourited;
									SaveWorkspace();
								}
							}

							// Draw chevron
							if (iconArrow) {
								ImU32 col   = ImGui::GetDynamicTextColor(hovered);
								bool  pDown = isOpen;
								auto  ap    = chromeArrow(pDown, m.sidebarItemH);

								ImVec2 drawPos = {
									pos.x + (m.sidebarIndent * 0.5f),
									pos.y + ap.offsetY + textVisualOffset
								};
								ImGui::DrawArrowIcon(window->DrawList, drawPos, ap.drawSize, col,
									pDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
							}

							float textY = bb.Min.y + (m.sidebarItemH - ImGui::CalcTextSize(groupName).y) * 0.5f + textVisualOffset;
							ImGui::RenderText({ pos.x + alignedTextOffset, textY }, groupName);

							// Draw Star
							if (origGroup && origGroup[0] != '\0') {
								bool isFav = false;

								// Transparent lookup
								auto it = _groupOverrides.find(origGroup);
								if (it != _groupOverrides.end()) {
									isFav = it->second.isFavourited;
								}

								if (_cfg.showSidebarFavourites && (isFav || hovered)) {
									const char* starIcon     = ICON_FA_STAR;
									float       starScale    = 0.85f;
									float       starFontSize = m.sidebarFontSize * starScale;

									ImGui::PushFont(regularFont, starFontSize);
									ImVec2 starSize = ImGui::CalcTextSize(starIcon);
									ImGui::PopFont();

									float starX = bb.Min.x + sidebarAvailW - starSize.x - (15.0f * m.uiScale);
									float starY = bb.Min.y + (m.sidebarItemH - starFontSize) * 0.5f - (1.0f * m.uiScale);

									// Hit-testing logic bypassing ImGui Item overlap clashing
									ImRect starBB(
										ImVec2(starX - (5.0f * m.uiScale), bb.Min.y),
										ImVec2(starX + starSize.x + (5.0f * m.uiScale), bb.Min.y + m.sidebarItemH));
									bool starHovered = ImGui::IsMouseHoveringRect(starBB.Min, starBB.Max);

									if (starHovered && ImGui::IsMouseClicked(0)) {
										isFav                                   = !isFav;
										_groupOverrides[origGroup].isFavourited = isFav;
										SaveWorkspace();
									}

									ImU32 starCol = starHovered ? IM_COL32(255, 255, 100, 255) : (isFav ? IM_COL32(255, 215, 0, 255) : IM_COL32(150, 150, 150, 150));

									ImGui::GetWindowDrawList()->AddText(regularFont, starFontSize,
										ImVec2(starX, starY),
										starCol, starIcon);
								}
							}
						}

						if (isOpen) {
							for (auto* tool : tools) {
								auto&       over         = GetOverrides(tool);
								const char* resolvedName = over.customName.empty() ? tool->Name() : over.customName.c_str();
								RenderSidebarItem(tool, resolvedName, m.sidebarIndent);
							}
						}

						ImGui::PopID();
						ImGui::PopFont();
					};

					for (auto& entry : entries) {
						if (entry.isGroup) {
							RenderSidebarGroup(entry.label, entry.origGroup, *entry.tools);
						} else {
							RenderSidebarItem(entry.tool, entry.label);
						}
					}

					// --- FOOTER: SETTINGS (Centred) ---
					float childHeight     = ImGui::GetWindowSize().y;
					float separatorHeight = 1.0f;
					float settingsY       = childHeight - m.sidebarItemH - bottomPadding;

					float minSettingY = ImGui::GetCursorPos().y + separatorHeight;
					if (settingsY < minSettingY)
						settingsY = minSettingY;

					ImGui::SetCursorPosY(settingsY - separatorHeight);
					ImGui::SeparatorThick();

					ImGui::SetCursorPosY(settingsY);
					{
						auto*      settingsTool = &_settingsTool;
						bool       isSelected   = (_activeTool == settingsTool);
						const auto cursorPos    = ImGui::GetCursorPos();

						if (ImGui::Selectable("##SETTINGS", isSelected, 0, ImVec2(0, m.sidebarItemH))) {
							if (_activeTool != settingsTool) {
								if (_activeTool) {
									Input::Manager::GetSingleton()->AbortBinding();
									_activeTool->OnClose();
								}
								ImGui::PlayAudio(ImGui::Audio::kOk);
								_activeTool = settingsTool;
								_activeTool->OnOpen();
							}
						}

						ImVec2 endPos = ImGui::GetCursorPos();
						ImGui::SetCursorPos(cursorPos);

						// Bypassing ImGui::PushFont scaling
						ImGui::PushFont(regularFont, m.sidebarFontSize);

						const char* settingText = "$FUCK_Settings"_T;
						float       setW        = ImGui::CalcTextSize(settingText).x;

						ImGui::SetCursorPosX(cursorPos.x + (sidebarAvailW - setW) * 0.5f);
						ImGui::SetCursorPosY(cursorPos.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset);
						ImGui::TextUnformatted(settingText);

						ImGui::PopFont();
						ImGui::SetCursorPos(endPos);
					}
					ImGui::PopStyleVar();
					ImGui::Dummy(ImVec2(0, bottomPadding));
				}
				ImGui::EndChild();
			};

			// -- Content (scaled) --
			auto renderContent = [&](float width) {
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(m.padBase, m.padBase));
				ImGui::BeginChild("Content", ImVec2(width, availHeight), true, ImGuiChildFlags_AlwaysUseWindowPadding);
				{
					pushContentScale(false);
					if (_activeTool)
						_activeTool->Draw();
					else
						ImGui::CenteredText("$FUCK_NoToolSelected"_T, true);

					popContentScale();
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
			};

			if (_cfg.sidebarOnRight) {
				float contentWidth = ImGui::GetContentRegionAvail().x - m.sidebarWidth - ImGui::GetStyle().ItemSpacing.x;
				renderContent(contentWidth);
				ImGui::SameLine();
				renderSidebar();
			} else {
				renderSidebar();
				ImGui::SameLine();
				renderContent(0.0f);
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	if (!wantsOpen)
		Close();
}
