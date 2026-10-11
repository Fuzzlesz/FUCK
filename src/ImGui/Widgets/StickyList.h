#pragma once

namespace ImGui
{
	// Sticky List
	void BeginStickyList(const char* str_id, const ImVec2& size = ImVec2(0, 0), bool border = true);
	bool StickyHeader(const char* label, bool* open = nullptr, int count = -1, int flags = 0);  // Draw the rows when true
	void EndStickyList();
}
