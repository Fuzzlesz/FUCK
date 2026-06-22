#pragma once

#include "imgui.h"

namespace SKSE
{
#ifndef IMGUI_DISABLE

	IMGUI_IMPL_API bool ImGui_ImplSkyrim_Init();
	IMGUI_IMPL_API void ImGui_ImplSkyrim_NewFrame();

#endif
}
