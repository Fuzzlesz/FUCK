#include "FUCK-Host.h"
#include "FUCK-Man.h"
#include "FUCK-Windows.h"

#include "ImGui/Audio.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

#include "System/Hotkeys.h"
#include "System/Input.h"
#include "System/Settings.h"
#include "System/Utils.h"

#include "API/IconsFontAwesome6.h"

// Set while a window is drawn, for GetWindowScale() and GetWindowRotation()
static float s_windowScale    = 1.0f;
static float s_windowRotation = 0.0f;

// Helper to keep windows within the visible viewport
void ClampWindowToScreen(ImVec2& pos, const ImVec2& size)
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

// Fades everything a window and its child windows drew this frame, for a HUD manager's alpha override.
// Scaling the vertices catches every colour, including those drawn without the style's alpha (images, rotated quads, raw colours)
static void ScaleWindowAlpha(ImGuiWindow* a_window, float a_alpha)
{
	if (!a_window || !a_window->DrawList)
		return;

	for (auto& vtx : a_window->DrawList->VtxBuffer) {
		const ImU32 alpha = (vtx.col >> IM_COL32_A_SHIFT) & 0xFF;
		vtx.col           = (vtx.col & ~IM_COL32_A_MASK) | (static_cast<ImU32>(alpha * a_alpha + 0.5f) << IM_COL32_A_SHIFT);
	}

	// Only children drawn this frame. One that wasn't keeps last frame's list, which isn't drawn
	const int frame = ImGui::GetFrameCount();
	for (auto* child : a_window->DC.ChildWindows) {
		if (child && child->LastFrameActive == frame)
			ScaleWindowAlpha(child, a_alpha);
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
void ClaimEditHandleHover()
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
// Plugin Windows
// ==================================================

void FUCKMan::DrawPluginWindows(const LayoutMetrics& a_m, bool a_passthroughOnly, bool a_globalMouseClick)
{
	auto iconArrow   = MANAGER(IconFont)->GetStepperRight();
	auto chromeArrow = [&](bool pointsDown, float rowH) {
		return ImGui::CalcArrowIconParams(a_m.iconAspect, pointsDown, rowH, a_m.chromeIconBaseH);
	};

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

		// A HUD manager's fade, for kHideWithHUD windows. 1 when there's none
		const float hudAlpha = GetHUDWidgetAlpha(win, userFlags);

		// While editing, the margin grows the window past the outline so its handles sit over it
		ImVec2 editMargin = { 0.0f, 0.0f };
		if (isEditing) {
			float margin = kEditMargin * a_m.uiScale;
			editMargin   = { margin, margin };

			// Room for the handles at any angle, so turning never resizes or moves the window.
			// A fixed-size window's body is its default size, as its margin is added on top below
			if (rotatable) {
				ImVec2 body  = autoResize ? winState.bodySize : win->GetDefaultSize();
				ImVec2 half  = { body.x * 0.5f, body.y * 0.5f };
				float  reach = std::max(std::sqrt(half.x * half.x + half.y * half.y) + margin, half.y + kRotateReach * a_m.uiScale);
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

			float paddingY      = (userFlags & FUCK::WindowFlags::kNoBackground) ? 0.0f : a_m.pad.y;
			float chromeBottomY = checkPos.y + paddingY + a_m.titleH + 1.0f;

			// When collapsed, the entire window is basically just the chrome + padding.
			if (winState.isCollapsed) {
				float checkHeight = (winState.liveSize.y != -1.0f) ? winState.liveSize.y : (paddingY * 2.0f + a_m.titleH + 1.0f);
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
			// Give the theme's padding to frameless windows that still have a solid background panel
			FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(a_m.pad.x + editMargin.x, a_m.pad.y + editMargin.y));
		}

		// Combine the display title with the Host's unique tracking key
		std::string windowLabel = std::format("{}###{}", title, key);

		// FUCK::BeginWindow() draws the background and border itself when corners are inverted, as ImGui only rounds
		// them outwards. Whether it does, for the title bar below
		const bool themedBg = !(userFlags & FUCK::WindowFlags::kNoBackground) && ImGui::UseInvertedCorners() && ImGui::GetStyle().WindowRounding > 0.0f;

		// Use the formatted string instead of raw title
		const bool visible = FUCK::BeginWindow(windowLabel.c_str(), &open, flags);

		if (visible) {
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
				if (a_globalMouseClick && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
					if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup | ImGuiHoveredFlags_AllowWhenBlockedByActiveItem | ImGuiHoveredFlags_ChildWindows)) {
						open = false;
					}
				}
			}

			// Auto-save settings on move/resize end preventing overwrite during collapse.
			// Editable windows save their own position, which isn't the window's corner.
			// Only what changed while the mouse was held counts, so a click anywhere doesn't save a window the user
			// never touched, and a window that sizes itself to its content doesn't save its size at all
			if (!editable && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				winState.pressPos  = curPos;
				winState.pressSize = curSize;
			}

			if (!editable && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && winState.pressPos.x != -FLT_MAX) {
				bool movedNow    = std::abs(curPos.x - winState.pressPos.x) > 1.0f || std::abs(curPos.y - winState.pressPos.y) > 1.0f;
				bool resizedNow  = std::abs(curSize.x - winState.pressSize.x) > 1.0f || std::abs(curSize.y - winState.pressSize.y) > 1.0f;
				bool posChanged  = movedNow && !isCustomPos && !noMove && (std::abs(curPos.x - winState.pos.x) > 1.0f || std::abs(curPos.y - winState.pos.y) > 1.0f);
				bool sizeChanged = resizedNow && !isCollapsed && !autoResize && !noResize && (std::abs(curSize.x - winState.size.x) > 1.0f || std::abs(curSize.y - winState.size.y) > 1.0f);

				winState.pressPos = { -FLT_MAX, -FLT_MAX };

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

				// Inverted corners cut into the ends of the title bar, so its icons move in clear of them
				const float cornerInset = themedBg ? ImGui::GetWindowCornerInset() : 0.0f;

				ImVec2 headerStartCursor = FUCK::GetCursorPos();
				ImVec2 cursorScreen      = FUCK::GetCursorScreenPos();
				cursorScreen.x += cornerInset;

				FUCK::SetCursorPosX(headerStartCursor.x + cornerInset);
				FUCK::BeginGroup();

				// 1. Collapse Icon
				float iconW = 0.0f;

				if (iconArrow) {
					// Calculate exact physical dimensions of the arrow first
					bool pointsDown = !isCollapsed;
					auto ap         = chromeArrow(pointsDown, a_m.titleH);

					// Lock horizontal container width to its maximum dimension to prevent shifting on rotation
					float maxIconDim = std::max(ap.drawSize.x, ap.drawSize.y);
					float btnWidth   = (a_m.titleIconPadX * 2.0f) + maxIconDim;

					// Tightly wrap the invisible button around the graphic
					if (ImGui::InvisibleButton("##CollapseToggle", ImVec2(btnWidth, a_m.titleH))) {
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
					drawPos.x += a_m.titleIconPadX + iconOffsetX;
					drawPos.y += ap.offsetY + a_m.titleIconNudgeY;

					ImGui::DrawArrowIcon(ImGui::GetWindowDrawList(), drawPos, ap.drawSize, iconColor,
						pointsDown ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);

					// Give the title text slightly more breathing room past the exact button width
					iconW = btnWidth + a_m.titleIconPadX;
				}

				// 2. Title Text
				ImFont* baseFont = ImGui::GetFont();

				FUCK::SetCursorPos({ cornerInset + iconW, a_m.titleTextOffsetY });
				ImGui::GetWindowDrawList()->AddText(baseFont, a_m.titleFontSize,
					FUCK::GetCursorScreenPos(), ImGui::GetColorU32(ImGuiCol_Text), title);

				// 3. Close Button
				const float btnSize = a_m.titleH;
				const float btnX    = winWidth - btnSize - a_m.headerPadding - cornerInset;

				FUCK::SetCursorPos({ btnX, 0 });
				if (ImGui::InvisibleButton("##WinClose", ImVec2(btnSize, btnSize))) {
					open = false;
				}

				{
					const char* xIcon      = ICON_FA_XMARK;
					float       uiFontSize = ImGui::GetStyle().FontSizeBase * a_m.uiScale;

					ImGui::PushFont(nullptr, uiFontSize);
					ImVec2 textSize = ImGui::CalcTextSize(xIcon);
					ImGui::PopFont();

					ImVec2 btnScreenPos = ImGui::GetItemRectMin();
					ImVec2 textPos      = {
                        btnScreenPos.x + (btnSize - textSize.x) * 0.5f,
                        btnScreenPos.y + (btnSize - textSize.y) * 0.5f + (1.0f * a_m.uiScale)
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
				// ImGui's own, not FUCK::'s, which pads consumers' separators and would push the chrome down
				FUCK::SetCursorPos({ headerStartCursor.x, a_m.titleH });
				ImGui::SeparatorThick();

				// 5. Content Child (scaled)
				if (!winState.isCollapsed && !isCollapsed) {
					FUCK::SetCursorPosX(0.0f);

					FUCK::PushStyleVar(ImGuiStyleVar_WindowPadding, a_m.pad);

					ImGuiChildFlags  childFlags  = ImGuiChildFlags_AlwaysUseWindowPadding;
					ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoBackground;

					// Themed for its scrollbar, which would otherwise run into the window's bottom corner
					if (ImGui::BeginChildThemed("##Content", ImVec2(0, 0), childFlags, windowFlags)) {
						ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
						PushContentScale(a_m, ignoreUserScale);
						_currentRenderingPlugin = win->PluginName();

						// So SetWindowPos/Size from Draw() apply to the window, not this child
						FUCK::Host::SetContentChild(ImGui::GetCurrentWindow());
						win->Draw();
						FUCK::Host::SetContentChild(nullptr);

						_currentRenderingPlugin.clear();
						PopContentScale();
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

				PushContentScale(a_m, ignoreUserScale);
				_currentRenderingPlugin = win->PluginName();
				win->Draw();
				_currentRenderingPlugin.clear();
				PopContentScale();

				if (rotatable)
					ImGui::PopClipRect();
			}

			if (isEditing) {
				UpdateEditableWindow(winState, curPos, fullSize, editMargin, scalable, rotatable);
			}

			// Fade everything it drew this frame, chrome and content alike
			if (hudAlpha < 1.0f) {
				ScaleWindowAlpha(ImGui::GetCurrentWindow(), hudAlpha);
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
}
