#pragma once

namespace FUCK::Host
{
	FUCK_Interface* CreateInterface();

	// The child a decorated window's Draw() runs in, so SetWindowPos/Size can apply to the window itself
	void SetContentChild(ImGuiWindow* a_child);
}
