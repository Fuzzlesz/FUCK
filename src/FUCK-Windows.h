#pragma once

#include "FUCK-Man.h"

// Plugin windows' state, shared by FUCK-Man.cpp, FUCK-Windows.cpp and FUCK-Workspace.cpp

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

	// Where the window was when the mouse went down, so a release can tell whether the user moved or resized it
	ImVec2 pressPos{ -FLT_MAX, -FLT_MAX };
	ImVec2 pressSize{};

	bool IsLayoutModified() const { return hasLoadedPos || std::abs(scale - 1.0f) > 0.001f || std::abs(rotation) > 0.001f; }
};
// Maps using "PluginName|WindowId". Shared by FUCK-Man, FUCK-Windows and FUCK-Workspace
inline StringMap<WindowState> s_windowStates;

// kEditable windows, sizes at 1080p
inline constexpr float kEditMargin   = 12.0f;  // room past the outline for the corner handles
inline constexpr float kEditOutline  = 4.0f;   // outline distance from the window body
inline constexpr float kRotateReach  = 36.0f;  // rotation handle's reach past the body's top edge
inline constexpr float kMinEditScale = 0.5f;
inline constexpr float kMaxEditScale = 10.0f;

// Keeps a window within the visible viewport
void ClampWindowToScreen(ImVec2& pos, const ImVec2& size);

// Before any window is drawn, gives the mouse to an edited window whose handle it's over
void ClaimEditHandleHover();
