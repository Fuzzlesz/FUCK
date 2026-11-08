#pragma once

// Keeps the item menus' 3D preview empty while any plugin asks, e.g. while the item's shown on the player instead
namespace ItemPreview
{
	// Each plugin's ask is kept apart, so letting go only lets go of its own. Hiding unloads the item shown now, and once let go the next
	// item selected shows
	void SetHidden(const char* a_plugin, bool a_hidden);
}
