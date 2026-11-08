#pragma once

// Keeps the item menus' 3D preview and item cards out of the way while any plugin asks, e.g. while the item's shown on the player instead
namespace ItemPreview
{
	// Each plugin's ask is kept apart, so letting go only lets go of its own. Hiding clears what's shown now, and once let go both come
	// back with the next item loaded
	void SetHidden(const char* a_plugin, bool a_preview, bool a_card);
}
