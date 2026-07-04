#include "imgui.h"
#ifndef IMGUI_DISABLE

#	include "imgui_impl_skyrim.h"

#	ifdef SKYRIMVR
#		include "ImGui/Renderer.h"
#	endif

namespace SKSE
{
	bool ImGui_ImplSkyrim_Init()
	{
		ImGuiIO& io = ImGui::GetIO();

		io.BackendPlatformUserData = nullptr;
		io.BackendPlatformName     = "imgui_impl_skyrim";
		io.WantCaptureMouse        = false;
		io.WantCaptureKeyboard     = false;
		io.WantTextInput           = false;
		io.WantSetMousePos         = false;

		return true;
	}

	void ImGui_ImplSkyrim_NewFrame()
	{
		ImGuiIO& io = ImGui::GetIO();

		const auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();
		io.DisplaySize        = ImVec2(static_cast<float>(screenSize.width), static_cast<float>(screenSize.height));

		float deltaTime = 1.0f / 60.0f;  // In case there is no timer for some reason, default to 60fps

		if (const auto* timer = RE::BSTimer::GetSingleton(); timer && timer->realTimeDelta > 0.0f) {
			deltaTime = timer->realTimeDelta;
		}
		io.DeltaTime = deltaTime;

		// The wand (pumped into ImGui's IO by ImGuiVRHelper before NewFrame) owns
		// the cursor; MenuCursor doesn't track it and would snap the position back.
#	ifdef SKYRIMVR
		if (!ImGui::Renderer::IsVRHelperConnected())
#	endif
		{
			if (const auto* menuCursor = RE::MenuCursor::GetSingleton()) {
				io.AddMousePosEvent(menuCursor->cursorPosX, menuCursor->cursorPosY);
			}
		}
	}
}

#endif
