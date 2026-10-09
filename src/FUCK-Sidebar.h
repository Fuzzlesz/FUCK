#pragma once

// ==================================================
// Sidebar
// ==================================================
// The main window's tool list: how tools are grouped, the order they're in, and drawing the list.
// FUCKMan owns the state (the user's overrides), so the functions that change it are FUCKMan's,
// and are defined in FUCK-Sidebar.cpp alongside the drawing

namespace Sidebar
{
	// Orders are spaced this far apart, so a moved entry can usually take one between its neighbours
	inline constexpr int kOrderStep = 10000;

	// Groups nest at most this many levels deep. A deeper path's extra levels are folded into the last one
	inline constexpr int kMaxGroupDepth = 4;

	// One thing in a group's list: a tool, or a group nested in it
	struct Entry
	{
		FUCK::ITool* tool = nullptr;  // null for a group
		std::string  group;           // the nested group's full path

		bool IsGroup() const { return tool == nullptr; }
		bool operator==(const Entry&) const = default;
	};

	// A group's contents. Groups and tools share one order, so a nested group can sit between tools
	struct GroupNode
	{
		std::vector<Entry>        entries;  // groups and tools together, in display order
		std::vector<std::string>  groups;   // just the groups, in the same order
		std::vector<FUCK::ITool*> tools;    // just the tools, in the same order
	};

	// Keyed by group path, "Camera::Presets" for a group nested in another, with "" as the top level
	using GroupTree = StringMap<GroupNode>;

	// Every sidebar tool sorted into its groups, including ones the user has hidden but not ones whose ShowInSidebar()
	// is false. Shared by the sidebar and the Settings
	// table, so both show the same order, and built only when something changes rather than every frame.
	//
	// Each level holds its groups and tools together, sorted by their effective order: the user's own, if they've
	// moved it, or else its baseline. Baselines are alphabetical at the top level. Inside a group they follow
	// registration order, and a nested group sits where its first tool was registered
	struct ToolLayout
	{
		GroupTree                      tree;
		Map<FUCK::ITool*, std::string> keys;    // "Plugin|Name", the override key
		Map<FUCK::ITool*, std::string> labels;  // the user's name, or the tool's own
		Map<FUCK::ITool*, std::string> groups;  // the group it's in, resolved once
		Map<FUCK::ITool*, int>         toolBaseline;
		Map<std::string, int>          groupBaseline;
		Map<FUCK::ITool*, std::size_t> registration;       // index in the registered tools
		Map<std::string, std::size_t>  groupRegistration;  // its earliest tool's, nested ones included
		std::uint32_t                  version = 0;        // goes up with every rebuild
	};

	// Where each entry at one level sits with no order of its own
	struct LevelBaselines
	{
		Map<FUCK::ITool*, int> tools;
		Map<std::string, int>  groups;

		int Of(const Entry& a_entry) const;
	};

	// --- Group paths ---
	// A path is its levels joined by "::", e.g. "Camera::Presets" for a group Presets inside Camera. Anything else is
	// part of a level's name. Paths are kept normalised so they can be compared as strings
	inline constexpr std::string_view kGroupSeparator = "::";

	std::string              NormaliseGroupPath(std::string_view a_path);                   // trims each level, drops empty ones, folds extra depth
	std::string              GetGroupParent(std::string_view a_path);                       // "" for a top-level group
	std::string              GetGroupLeaf(std::string_view a_path);                         // the last level's name
	int                      GetGroupDepth(std::string_view a_path);                        // 0 for a top-level group
	std::vector<std::string> GetGroupPrefixes(std::string_view a_path);                     // "A", "A::B", "A::B::C" for "A::B::C"
	bool                     IsInGroup(std::string_view a_path, std::string_view a_group);  // the group itself, or nested in it
	std::vector<std::string> SplitGroupPath(std::string_view a_path);                       // each level's name
	std::string              JoinGroupPath(const std::vector<std::string>& a_levels);

	// Adds a tool under its group, creating each level of the path on the way
	void AddToGroupTree(GroupTree& a_tree, const std::string& a_path, FUCK::ITool* a_tool);
}

// The main window's measurements the sidebar is drawn with, at resolution scale
struct SidebarMetrics
{
	float uiScale;
	float iconAspect;
	float chromeIconBaseH;
	float sidebarWidth;
	float sidebarItemH;
	float sidebarFontSize;
	float sidebarIndent;
};
