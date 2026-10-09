#include "FUCK-Man.h"
#include "FUCK-Sidebar.h"

#include "ImGui/Audio.h"
#include "ImGui/IconsFontAwesome6.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

#include "System/Input.h"
#include "System/Utils.h"

// ==================================================
// Group Paths
// ==================================================

namespace Sidebar
{
	int LevelBaselines::Of(const Entry& a_entry) const
	{
		if (a_entry.tool) {
			auto it = tools.find(a_entry.tool);
			return it != tools.end() ? it->second : 0;
		}
		auto it = groups.find(a_entry.group);
		return it != groups.end() ? it->second : 0;
	}

	namespace
	{
		std::vector<std::size_t> FindSeparators(std::string_view a_path)
		{
			std::vector<std::size_t> out;
			for (std::size_t at = a_path.find(kGroupSeparator); at != std::string_view::npos; at = a_path.find(kGroupSeparator, at + kGroupSeparator.size()))
				out.push_back(at);
			return out;
		}

		std::string_view Trim(std::string_view a_text)
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front())))
				a_text.remove_prefix(1);
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back())))
				a_text.remove_suffix(1);
			return a_text;
		}
	}

	std::vector<std::string> SplitGroupPath(std::string_view a_path)
	{
		std::vector<std::string> levels;
		std::size_t              start = 0;
		auto                     seps  = FindSeparators(a_path);
		seps.push_back(a_path.size());

		for (std::size_t sep : seps) {
			// Stray colons at a level's ends go too, as in "A:::B" there's no telling which level the third one belongs to
			std::string_view level = Trim(a_path.substr(start, sep - start));
			while (!level.empty() && (level.front() == ':' || std::isspace(static_cast<unsigned char>(level.front()))))
				level.remove_prefix(1);
			while (!level.empty() && (level.back() == ':' || std::isspace(static_cast<unsigned char>(level.back()))))
				level.remove_suffix(1);
			if (!level.empty())
				levels.emplace_back(level);
			start = sep + kGroupSeparator.size();
		}
		return levels;
	}

	std::string JoinGroupPath(const std::vector<std::string>& a_levels)
	{
		std::string out;
		for (const auto& level : a_levels) {
			if (!out.empty())
				out += kGroupSeparator;
			out += level;
		}
		return out;
	}

	// Trims each level and drops empty ones, so "A :: B::" and "A::B" are the same group.
	// Levels past kMaxGroupDepth are folded into the last one, keeping their names: "A::B::C::D::E" ends in a group "D / E"
	std::string NormaliseGroupPath(std::string_view a_path)
	{
		auto levels = SplitGroupPath(a_path);
		if (levels.size() > static_cast<std::size_t>(kMaxGroupDepth)) {
			std::string folded = levels[kMaxGroupDepth - 1];
			for (std::size_t i = kMaxGroupDepth; i < levels.size(); ++i) {
				folded += " / ";
				folded += levels[i];
			}
			levels.resize(kMaxGroupDepth);
			levels.back() = std::move(folded);
		}
		return JoinGroupPath(levels);
	}

	std::string GetGroupParent(std::string_view a_path)
	{
		const auto seps = FindSeparators(a_path);
		return seps.empty() ? std::string() : std::string(a_path.substr(0, seps.back()));
	}

	std::string GetGroupLeaf(std::string_view a_path)
	{
		const auto seps = FindSeparators(a_path);
		return std::string(seps.empty() ? a_path : a_path.substr(seps.back() + kGroupSeparator.size()));
	}

	int GetGroupDepth(std::string_view a_path)
	{
		return static_cast<int>(FindSeparators(a_path).size());
	}

	std::vector<std::string> GetGroupPrefixes(std::string_view a_path)
	{
		std::vector<std::string> out;
		if (a_path.empty())
			return out;
		for (std::size_t sep : FindSeparators(a_path))
			out.emplace_back(a_path.substr(0, sep));
		out.emplace_back(a_path);
		return out;
	}

	bool IsInGroup(std::string_view a_path, std::string_view a_group)
	{
		if (a_group.empty() || a_path.size() < a_group.size() || a_path.substr(0, a_group.size()) != a_group)
			return false;
		return a_path.size() == a_group.size() || a_path.substr(a_group.size(), kGroupSeparator.size()) == kGroupSeparator;
	}

	// Looks each level up again rather than holding references, as inserting can move the map's entries
	void AddToGroupTree(GroupTree& a_tree, const std::string& a_path, FUCK::ITool* a_tool)
	{
		if (a_tree.find(std::string()) == a_tree.end())
			a_tree[std::string()];

		std::string parent;
		for (auto& level : GetGroupPrefixes(a_path)) {
			if (a_tree.find(level) == a_tree.end()) {
				a_tree[level];
				a_tree[parent].groups.push_back(level);
			}
			parent = std::move(level);
		}

		a_tree[a_path].tools.push_back(a_tool);
	}
}

namespace
{
	// An order between two neighbours, or none when there's no room left between them
	std::optional<int> OrderBetween(std::optional<int> a_prev, std::optional<int> a_next)
	{
		long long between = 0;
		if (!a_prev && !a_next)
			between = Sidebar::kOrderStep;
		else if (!a_prev)
			between = static_cast<long long>(*a_next) - Sidebar::kOrderStep;
		else if (!a_next)
			between = static_cast<long long>(*a_prev) + Sidebar::kOrderStep;
		else if (static_cast<long long>(*a_next) - *a_prev >= 2)
			between = *a_prev + (static_cast<long long>(*a_next) - *a_prev) / 2;
		else
			return std::nullopt;

		// 0 means no order of its own
		if (between == 0 || between < std::numeric_limits<int>::min() / 2 || between > std::numeric_limits<int>::max() / 2)
			return std::nullopt;
		return static_cast<int>(between);
	}
}

// ==================================================
// Tool Groups
// ==================================================

std::string FUCKMan::GetToolGroup(FUCK::ITool* a_tool)
{
	// Looked up directly, not through GetToolKey(), which would rebuild the layout this is called from
	auto it = _toolOverrides.find(std::format("{}|{}", a_tool->PluginName(), a_tool->Name()));
	if (it != _toolOverrides.end() && !it->second.customGroup.empty()) {
		if (it->second.customGroup == "##ROOT")
			return {};
		return Sidebar::NormaliseGroupPath(it->second.customGroup);
	}
	return GetNativeGroup(a_tool);
}

// The group the tool asks for, followed to wherever the user has since moved it
std::string FUCKMan::GetNativeGroup(FUCK::ITool* a_tool)
{
	return ApplyGroupMoves(Sidebar::NormaliseGroupPath(a_tool->Group() ? a_tool->Group() : ""));
}

// Where a group is now, after the user's moves. The longest moved path it's in decides
std::string FUCKMan::ApplyGroupMoves(const std::string& a_path) const
{
	const std::string* bestFrom = nullptr;
	const std::string* bestTo   = nullptr;
	for (const auto& [from, to] : _groupMoves) {
		if (Sidebar::IsInGroup(a_path, from) && (!bestFrom || from.size() > bestFrom->size())) {
			bestFrom = &from;
			bestTo   = &to;
		}
	}
	// Normalised, as a plugin may have added levels below a group that was moved deep, which would pass the depth limit
	return bestFrom ? Sidebar::NormaliseGroupPath(*bestTo + a_path.substr(bestFrom->size())) : a_path;
}

std::string FUCKMan::GetGroupDisplayName(const std::string& a_path)
{
	auto it = _groupOverrides.find(a_path);
	if (it != _groupOverrides.end() && !it->second.customName.empty())
		return it->second.customName;
	return Sidebar::GetGroupLeaf(a_path);
}

// ==================================================
// Tool Layout
// ==================================================

const Sidebar::ToolLayout& FUCKMan::GetToolLayout()
{
	if (_layoutDirty)
		RebuildToolLayout();
	return _layout;
}

const std::string& FUCKMan::GetToolKey(FUCK::ITool* a_tool)
{
	const auto& layout = GetToolLayout();
	if (auto it = layout.keys.find(a_tool); it != layout.keys.end())
		return it->second;

	// Not in the layout, as its ShowInSidebar() is false. The reference is only good until the next call
	static std::string s_key;
	s_key = std::format("{}|{}", a_tool->PluginName(), a_tool->Name());
	return s_key;
}

const std::string& FUCKMan::GetToolLabel(FUCK::ITool* a_tool)
{
	const auto& layout = GetToolLayout();
	if (auto it = layout.labels.find(a_tool); it != layout.labels.end())
		return it->second;

	// As above
	static std::string s_label;
	s_label = a_tool->Name();
	return s_label;
}

// Reads the layout as it stands, without rebuilding, as the rebuild itself sorts with it
int FUCKMan::GetEntryOrder(const Sidebar::Entry& a_entry)
{
	if (a_entry.tool) {
		if (auto key = _layout.keys.find(a_entry.tool); key != _layout.keys.end()) {
			if (auto it = _toolOverrides.find(key->second); it != _toolOverrides.end() && it->second.sortOrder != 0)
				return it->second.sortOrder;
		}
		auto base = _layout.toolBaseline.find(a_entry.tool);
		return base != _layout.toolBaseline.end() ? base->second : 0;
	}

	if (auto it = _groupOverrides.find(a_entry.group); it != _groupOverrides.end() && it->second.sortOrder != 0)
		return it->second.sortOrder;
	auto base = _layout.groupBaseline.find(a_entry.group);
	return base != _layout.groupBaseline.end() ? base->second : 0;
}

// Where each entry sits with no order of its own. At the top level that's alphabetical, groups and tools together.
// Inside a group it's registration order, a nested group counting from its earliest tool
Sidebar::LevelBaselines FUCKMan::ComputeLevelBaselines(const std::string& a_level, const std::vector<Sidebar::Entry>& a_entries)
{
	struct Item
	{
		const Sidebar::Entry* entry;
		std::string           name;
		std::size_t           registration;
	};

	std::vector<Item> items;
	items.reserve(a_entries.size());
	for (const auto& e : a_entries) {
		Item item{ &e, std::string(), SIZE_MAX };
		if (e.tool) {
			auto label        = _layout.labels.find(e.tool);
			item.name         = label != _layout.labels.end() ? label->second : std::string(e.tool->Name());
			auto reg          = _layout.registration.find(e.tool);
			item.registration = reg != _layout.registration.end() ? reg->second : SIZE_MAX;
		} else {
			item.name         = GetGroupDisplayName(e.group);
			auto reg          = _layout.groupRegistration.find(e.group);
			item.registration = reg != _layout.groupRegistration.end() ? reg->second : SIZE_MAX;
		}
		items.push_back(std::move(item));
	}

	if (a_level.empty()) {
		std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
			if (int cmp = _stricmp(a.name.c_str(), b.name.c_str()); cmp != 0)
				return cmp < 0;
			return a.registration < b.registration;  // same name, so whichever registered first
		});
	} else {
		std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
			if (a.registration != b.registration)
				return a.registration < b.registration;
			return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
		});
	}

	Sidebar::LevelBaselines out;
	for (std::size_t i = 0; i < items.size(); ++i) {
		const int baseline = static_cast<int>((i + 1) * Sidebar::kOrderStep);
		if (items[i].entry->tool)
			out.tools[items[i].entry->tool] = baseline;
		else
			out.groups[items[i].entry->group] = baseline;
	}
	return out;
}

void FUCKMan::RebuildToolLayout()
{
	_layoutDirty = false;

	Sidebar::ToolLayout layout;
	layout.version = _layout.version + 1;
	layout.tree[std::string()];

	for (std::size_t i = 0; i < _tools.size(); ++i) {
		auto* t = _tools[i];
		if (!t->ShowInSidebar())
			continue;

		std::string key = std::format("{}|{}", t->PluginName(), t->Name());
		auto        it  = _toolOverrides.find(key);

		layout.labels[t]       = (it != _toolOverrides.end() && !it->second.customName.empty()) ? it->second.customName : std::string(t->Name());
		layout.keys[t]         = std::move(key);
		layout.registration[t] = i;

		std::string group = GetToolGroup(t);
		Sidebar::AddToGroupTree(layout.tree, group, t);

		// Each group the tool sits in, nested or not, counts from its earliest tool
		for (const auto& level : Sidebar::GetGroupPrefixes(group)) {
			if (auto reg = layout.groupRegistration.find(level); reg == layout.groupRegistration.end())
				layout.groupRegistration[level] = i;
			else
				reg->second = std::min(reg->second, i);
		}

		layout.groups[t] = std::move(group);
	}

	_layout = std::move(layout);

	// Each level's groups and tools together, and where each sits by default
	for (auto& [path, node] : _layout.tree) {
		node.entries.clear();
		for (const auto& g : node.groups)
			node.entries.push_back({ nullptr, g });
		for (auto* t : node.tools)
			node.entries.push_back({ t, std::string() });

		const auto baselines = ComputeLevelBaselines(path, node.entries);
		for (const auto& [t, order] : baselines.tools)
			_layout.toolBaseline[t] = order;
		for (const auto& [g, order] : baselines.groups)
			_layout.groupBaseline[g] = order;
	}

	// Then each level sorted by effective order. Baselines are unique within a level, so they settle any tie
	for (auto& [path, node] : _layout.tree) {
		auto baselineOf = [&](const Sidebar::Entry& e) {
			if (e.tool) {
				auto it = _layout.toolBaseline.find(e.tool);
				return it != _layout.toolBaseline.end() ? it->second : 0;
			}
			auto it = _layout.groupBaseline.find(e.group);
			return it != _layout.groupBaseline.end() ? it->second : 0;
		};

		std::stable_sort(node.entries.begin(), node.entries.end(), [&](const Sidebar::Entry& a, const Sidebar::Entry& b) {
			const int oa = GetEntryOrder(a);
			const int ob = GetEntryOrder(b);
			if (oa != ob)
				return oa < ob;
			return baselineOf(a) < baselineOf(b);
		});

		node.groups.clear();
		node.tools.clear();
		for (const auto& e : node.entries) {
			if (e.tool)
				node.tools.push_back(e.tool);
			else
				node.groups.push_back(e.group);
		}
	}
}

void FUCKMan::SetEntryOrder(const Sidebar::Entry& a_entry, int a_order)
{
	if (a_entry.tool) {
		// Looked up without rebuilding the layout, which may be marked out of date partway through a move
		auto key = _layout.keys.find(a_entry.tool);
		_toolOverrides[key != _layout.keys.end() ? key->second : std::format("{}|{}", a_entry.tool->PluginName(), a_entry.tool->Name())].sortOrder = a_order;
	} else {
		_groupOverrides[a_entry.group].sortOrder = a_order;
	}
}

// Gives a level its order, step by step. An entry landing where it would sit anyway keeps no order of its own,
// so only the ones that actually moved are written
void FUCKMan::RenumberLevel(const std::vector<Sidebar::Entry>& a_entries, const Sidebar::LevelBaselines& a_baselines)
{
	for (std::size_t i = 0; i < a_entries.size(); ++i) {
		const int step = static_cast<int>((i + 1) * Sidebar::kOrderStep);
		SetEntryOrder(a_entries[i], a_baselines.Of(a_entries[i]) == step ? 0 : step);
	}
}

// Moves an entry within its own level. Usually only the moved entry takes an order, between its new neighbours
void FUCKMan::PlaceInLevel(const std::string& a_level, const Sidebar::Entry& a_entry, std::size_t a_index)
{
	const auto& layout = GetToolLayout();

	std::vector<Sidebar::Entry> list;
	if (auto it = layout.tree.find(a_level); it != layout.tree.end())
		list = it->second.entries;
	std::erase(list, a_entry);
	a_index = std::min(a_index, list.size());

	std::optional<int> prev = a_index > 0 ? std::optional<int>(GetEntryOrder(list[a_index - 1])) : std::nullopt;
	std::optional<int> next = a_index < list.size() ? std::optional<int>(GetEntryOrder(list[a_index])) : std::nullopt;

	if (auto between = OrderBetween(prev, next)) {
		int baseline = 0;
		if (a_entry.tool) {
			if (auto it = layout.toolBaseline.find(a_entry.tool); it != layout.toolBaseline.end())
				baseline = it->second;
		} else if (auto it = layout.groupBaseline.find(a_entry.group); it != layout.groupBaseline.end()) {
			baseline = it->second;
		}
		SetEntryOrder(a_entry, *between == baseline ? 0 : *between);
	} else {
		list.insert(list.begin() + static_cast<std::ptrdiff_t>(a_index), a_entry);
		RenumberLevel(list, ComputeLevelBaselines(a_level, list));
	}
}

void FUCKMan::SetToolGroup(FUCK::ITool* a_tool, const std::string& a_group)
{
	const std::string target = Sidebar::NormaliseGroupPath(a_group);
	const std::string native = GetNativeGroup(a_tool);
	auto&             over   = _toolOverrides[GetToolKey(a_tool)];

	if (target == GetToolGroup(a_tool))
		return;

	if (target == native) {
		over.customGroup = "";
	} else if (target.empty()) {
		over.customGroup = "##ROOT";
	} else {
		over.customGroup = target;
	}

	// Its order belonged to the old group, so it starts at its usual place in the new one
	over.sortOrder = 0;
	OnOverridesChanged();
}

void FUCKMan::MoveTool(FUCK::ITool* a_tool, const std::string& a_group, std::size_t a_index)
{
	const auto& layout = GetToolLayout();

	const std::string    target = Sidebar::NormaliseGroupPath(a_group);
	const std::string    from   = GetToolGroup(a_tool);
	const Sidebar::Entry me{ a_tool, std::string() };

	if (from == target) {
		PlaceInLevel(target, me, a_index);
		OnOverridesChanged();
		return;
	}

	// Changing group can shift where things sit by default well past the two groups: a nested group sits where its
	// earliest tool was registered, a group can appear or empty out, and the top level is alphabetical. So the order
	// every level shows now is noted, the move is made, and any level that would change is given back the order it
	// showed, with the tool in its new place
	StringMap<std::vector<Sidebar::Entry>> shown;
	for (const auto& [path, node] : layout.tree)
		shown[path] = node.entries;

	if (auto it = shown.find(from); it != shown.end())
		std::erase(it->second, me);

	auto& list = shown[target];
	a_index    = std::min(a_index, list.size());
	list.insert(list.begin() + static_cast<std::ptrdiff_t>(a_index), me);

	SetToolGroup(a_tool, target);
	RebuildToolLayout();
	KeepShownOrder(shown);

	OnOverridesChanged();
}

// After a change that rebuilt the layout, gives back each level the order it showed before (a_shown, already updated
// for the change), where the new defaults would otherwise shuffle it. A level that's new takes its default order
void FUCKMan::KeepShownOrder(const StringMap<std::vector<Sidebar::Entry>>& a_shown)
{
	for (const auto& [path, node] : _layout.tree) {
		auto it = a_shown.find(path);
		if (it == a_shown.end())
			continue;

		// The order it showed, less anything no longer here, plus anything new here (a group the change made) where the rebuild put it
		std::vector<Sidebar::Entry> wanted;
		for (const auto& e : it->second) {
			const bool here  = std::find(node.entries.begin(), node.entries.end(), e) != node.entries.end();
			const bool twice = std::find(wanted.begin(), wanted.end(), e) != wanted.end();
			if (here && !twice)
				wanted.push_back(e);
		}
		for (std::size_t i = 0; i < node.entries.size(); ++i) {
			if (std::find(wanted.begin(), wanted.end(), node.entries[i]) == wanted.end())
				wanted.insert(wanted.begin() + static_cast<std::ptrdiff_t>(std::min(i, wanted.size())), node.entries[i]);
		}

		if (wanted != node.entries)
			RenumberLevel(wanted, ComputeLevelBaselines(path, wanted));
	}
}

void FUCKMan::MoveGroup(const std::string& a_path, std::size_t a_index)
{
	PlaceInLevel(Sidebar::GetGroupParent(a_path), Sidebar::Entry{ nullptr, a_path }, a_index);
	OnOverridesChanged();
}

bool FUCKMan::CanMoveGroupInto(const std::string& a_path, const std::string& a_parent)
{
	const auto&       layout = GetToolLayout();
	const std::string parent = Sidebar::NormaliseGroupPath(a_parent);

	if (a_path.empty() || parent == Sidebar::GetGroupParent(a_path) || Sidebar::IsInGroup(parent, a_path))
		return false;

	// How many levels the group's contents reach below it
	int reach = 0;
	for (const auto& [path, node] : layout.tree) {
		if (Sidebar::IsInGroup(path, a_path))
			reach = std::max(reach, Sidebar::GetGroupDepth(path) - Sidebar::GetGroupDepth(a_path));
	}

	const int newDepth = parent.empty() ? 0 : Sidebar::GetGroupDepth(parent) + 1;
	return newDepth + reach < Sidebar::kMaxGroupDepth;
}

bool FUCKMan::MoveGroupInto(const std::string& a_path, const std::string& a_parent, std::size_t a_index)
{
	if (!CanMoveGroupInto(a_path, a_parent))
		return false;

	const auto&       layout  = GetToolLayout();
	const std::string parent  = Sidebar::NormaliseGroupPath(a_parent);
	const std::string newPath = parent.empty() ? Sidebar::GetGroupLeaf(a_path) : parent + std::string(Sidebar::kGroupSeparator) + Sidebar::GetGroupLeaf(a_path);

	// A path in the moved group, as it'll be once it's moved. Anything else is left as it is
	auto moved = [&](const std::string& a_old) {
		return Sidebar::IsInGroup(a_old, a_path) ? newPath + a_old.substr(a_path.size()) : a_old;
	};

	// The order every level shows now, under the paths they'll have. If a group of the same name is already there, the two merge
	StringMap<std::vector<Sidebar::Entry>> shown;
	for (const auto& [path, node] : layout.tree) {
		auto& list = shown[moved(path)];
		for (const auto& e : node.entries) {
			Sidebar::Entry renamed = e.tool ? e : Sidebar::Entry{ nullptr, moved(e.group) };
			if (std::find(list.begin(), list.end(), renamed) == list.end())
				list.push_back(std::move(renamed));
		}
	}

	const Sidebar::Entry me{ nullptr, newPath };
	std::erase(shown[Sidebar::GetGroupParent(a_path)], me);
	// a_index counts a same-named group already there, so if that's before the drop point, the index drops by one
	auto& dest = shown[parent];
	if (auto it = std::find(dest.begin(), dest.end(), me); it != dest.end()) {
		if (static_cast<std::size_t>(it - dest.begin()) < a_index)
			--a_index;
		dest.erase(it);
	}
	a_index = std::min(a_index, dest.size());
	dest.insert(dest.begin() + static_cast<std::ptrdiff_t>(a_index), me);

	// Remembered, so tools a plugin adds later, or that come back with a plugin that wasn't loaded, follow the group.
	// Moves are keyed by the paths plugins give, so collect every one that leads here now: a tool's group or one above
	// it, an earlier move's own path, or a path under an earlier move. A group made only by hand has none
	std::vector<std::string> sources;
	auto                     addSource = [&](const std::string& a_source) {
        if (!a_source.empty() && ApplyGroupMoves(a_source) == a_path && std::find(sources.begin(), sources.end(), a_source) == sources.end())
            sources.push_back(a_source);
	};
	for (const auto& [t, key] : layout.keys) {
		for (const auto& level : Sidebar::GetGroupPrefixes(Sidebar::NormaliseGroupPath(t->Group() ? t->Group() : "")))
			addSource(level);
	}
	for (const auto& [from, to] : _groupMoves) {
		addSource(from);
		if (Sidebar::IsInGroup(a_path, to) && a_path != to)
			addSource(from + a_path.substr(to.size()));
	}

	// Earlier moves that ended inside this group follow it, so one lookup always gives where a group is now
	for (auto& [from, to] : _groupMoves) {
		if (Sidebar::IsInGroup(to, a_path))
			to = moved(to);
	}
	for (const auto& source : sources)
		_groupMoves[source] = newPath;

	std::vector<std::string> undone;  // moved back to where they started
	for (const auto& [from, to] : _groupMoves) {
		if (from == to)
			undone.push_back(from);
	}
	for (const auto& from : undone)
		_groupMoves.erase(from);

	// Tools put in the group by hand, with their own group set in Settings, take its new path too. Every tool's,
	// including ones that aren't in the sidebar this session. Only values change here, so nothing moves in the map
	for (auto& [key, over] : _toolOverrides) {
		if (over.customGroup.empty() || over.customGroup == "##ROOT")
			continue;
		const std::string group = Sidebar::NormaliseGroupPath(over.customGroup);
		if (Sidebar::IsInGroup(group, a_path))
			over.customGroup = moved(group);
	}

	// A tool that now sits where its own group leads goes back to following it, so it keeps up with later moves
	for (const auto& [t, key] : layout.keys) {
		auto it = _toolOverrides.find(key);
		if (it != _toolOverrides.end() && !it->second.customGroup.empty() && it->second.customGroup != "##ROOT" &&
			it->second.customGroup == GetNativeGroup(t))
			it->second.customGroup.clear();
	}

	// The sidebar keeps open groups open under their new paths
	_movedGroups.emplace_back(a_path, newPath);

	// The groups' own names, favourites and orders go with them, kept where a merged group already had its own
	std::vector<std::pair<std::string, GroupOverrideState>> groups;
	for (const auto& [path, over] : _groupOverrides) {
		if (Sidebar::IsInGroup(path, a_path))
			groups.emplace_back(path, over);
	}
	for (const auto& [path, over] : groups)
		_groupOverrides.erase(path);
	for (const auto& [path, over] : groups) {
		auto& dst = _groupOverrides[moved(path)];
		if (dst.customName.empty())
			dst.customName = over.customName;
		dst.isFavourited = dst.isFavourited || over.isFavourited;
		if (dst.sortOrder == 0)
			dst.sortOrder = over.sortOrder;
	}

	RebuildToolLayout();
	KeepShownOrder(shown);

	OnOverridesChanged();
	return true;
}

// ==================================================
// Drawing
// ==================================================

void FUCKMan::DrawSidebar(const SidebarMetrics& a_metrics, float a_height)
{
	const auto& m           = a_metrics;
	const float availHeight = a_height;

	auto iconArrow   = MANAGER(IconFont)->GetStepperRight();
	auto chromeArrow = [&](bool pointsDown, float rowH) {
		return ImGui::CalcArrowIconParams(m.iconAspect, pointsDown, rowH, m.chromeIconBaseH);
	};

	const float topPadding       = 2.0f * m.uiScale;
	const float bottomPadding    = 2.0f * m.uiScale;
	const float textVisualOffset = 1.0f * m.uiScale;

	// The favourites gather in a group of their own, under a key no group path can be
	static constexpr const char* kFavouritesPath = "##FAVOURITES";

	ImFont* regularFont = MANAGER(IconFont)->GetRegularFont();

	auto GetOverrides = [&](FUCK::ITool* t) -> ToolOverrideState& {
		return _toolOverrides[GetToolKey(t)];
	};

	// The list's rows, flattened from the tool layout. Rebuilt only when something changes,
	// so a long list costs no more per frame than the rows on screen
	struct SidebarRow
	{
		FUCK::ITool* tool = nullptr;  // null for a group
		std::string  path;            // the group's
		std::string  label;
		int          depth      = 0;
		ImGuiID      stateId    = 0;   // where a group keeps its open state
		int          parent     = -1;  // the group row this row sits in, for the stuck headers
		int          sectionEnd = 0;   // on a group row, one past its last row
	};
	static std::vector<SidebarRow> s_rows;
	static std::uint32_t           s_rowsVersion = 0;
	static bool                    s_rowsDirty   = true;
	static std::string             s_rowsFilter;
	static int                     s_rowsConfig = -1;

	// The header and footer stay put while the list between them scrolls, so a long list can't push Settings out of view
	ImGui::BeginChild("Sidebar", ImVec2(m.sidebarWidth, availHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		ImVec2 headerStart = ImGui::GetCursorPos();
		headerStart.y += topPadding;

		// --- HEADER: TOOLS (Centred) ---
		ImGui::SetCursorPos(headerStart);

		// At the chrome's own size, unaffected by the user's UI scale
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
			ImGui::InputTextWithHint("##SidebarFilter", "$FUCK_Sidebar_FilterHint"_T, filterBuf, sizeof(filterBuf));

			ImGui::PopStyleColor(1);
			ImGui::PopStyleVar(3);
			ImGui::PopFont();

			ImGui::Dummy(ImVec2(0.0f, 4.0f * m.uiScale));
		} else {
			filterBuf[0] = '\0';
		}

		auto  apRight           = chromeArrow(false, m.sidebarItemH);
		float alignedTextOffset = (m.sidebarIndent * 0.5f) + apRight.drawSize.x + (10.0f * m.uiScale);

		// Room the favourite star takes on the right of a row: 15px from the edge, plus a gap before it. Kept free
		// whether the star shows or not, so a name doesn't change length when the row is hovered
		float starReserve = 8.0f * m.uiScale;
		float starWidth   = 0.0f;
		if (_cfg.showSidebarFavourites) {
			ImGui::PushFont(regularFont, m.sidebarFontSize * 0.85f);
			starWidth   = ImGui::CalcTextSize(ICON_FA_STAR).x;
			starReserve = starWidth + 23.0f * m.uiScale;
			ImGui::PopFont();
		}

		// Whether the mouse is on a row's star, matching where the star takes its click
		auto IsOverStar = [&](float a_rowRight, float a_rowTop) {
			return _cfg.showSidebarFavourites &&
			       ImGui::IsMouseHoveringRect(ImVec2(a_rowRight - starWidth - 20.0f * m.uiScale, a_rowTop), ImVec2(a_rowRight - 10.0f * m.uiScale, a_rowTop + m.sidebarItemH));
		};

		// Draws a name in the current font, cut short with "..." when it's wider than a_maxW. Returns whether it was cut
		auto DrawFittedLabel = [&](ImDrawList* a_drawList, const ImVec2& a_pos, float a_maxW, const char* a_label) -> bool {
			ImFont*     font     = ImGui::GetFont();
			const float fontSize = ImGui::GetFontSize();
			const ImU32 col      = ImGui::GetColorU32(ImGuiCol_Text);
			const char* end      = a_label + std::strlen(a_label);

			if (ImGui::CalcTextSize(a_label, end).x <= a_maxW) {
				a_drawList->AddText(font, fontSize, a_pos, col, a_label, end);
				return false;
			}

			static constexpr const char* kEllipsis = "...";
			const float                  avail     = a_maxW - ImGui::CalcTextSize(kEllipsis).x;

			// The longest run of whole characters that fits, found by halving
			std::vector<const char*> bounds;
			for (const char* p = a_label; p < end;) {
				bounds.push_back(p);
				unsigned int c = 0;
				p += std::max(1, ImTextCharFromUtf8(&c, p, end));
			}
			bounds.push_back(end);

			std::size_t lo = 0;
			std::size_t hi = bounds.size() - 1;
			while (lo < hi) {
				const std::size_t mid = (lo + hi + 1) / 2;
				if (ImGui::CalcTextSize(a_label, bounds[mid]).x <= avail)
					lo = mid;
				else
					hi = mid - 1;
			}

			std::string fitted(a_label, bounds[lo]);
			while (!fitted.empty() && fitted.back() == ' ')
				fitted.pop_back();
			fitted += kEllipsis;

			a_drawList->AddText(font, fontSize, a_pos, col, fitted.c_str());
			return true;
		};

		auto RenderSidebarItem = [&](FUCK::ITool* tool, const char* label, float extraIndent = 0.0f) {
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return;  // nothing to draw while the window's collapsed, and drawing then would crash

			ImGui::PushID(tool);

			bool  isSelected = (_activeTool == tool);
			auto& over       = GetOverrides(tool);

			// Cache both Screen and Window positions before the Selectable moves the cursor!
			const auto  startScreenPos = ImGui::GetCursorScreenPos();
			const auto  startPos       = ImGui::GetCursorPos();
			const float rowW           = ImGui::GetContentRegionAvail().x;  // narrower when the list's scrollbar shows

			// A click on the star favourites the tool without opening it
			const bool onStar = IsOverStar(startScreenPos.x + rowW, startScreenPos.y);
			if (ImGui::Selectable("##SidebarItem", isSelected, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, m.sidebarItemH)) && !onStar) {
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
				OnOverridesChanged();
			}

			ImVec2 endPos = ImGui::GetCursorPos();

			ImGui::PushFont(regularFont, m.sidebarFontSize);
			float textY = startPos.y + (m.sidebarItemH - textHeightCalc) * 0.5f + textVisualOffset;

			// Cut short before the star, with the whole name in a tooltip on hover or gamepad focus
			const ImVec2 textPos = { startScreenPos.x + alignedTextOffset + extraIndent, startScreenPos.y + (textY - startPos.y) };
			const bool   cut     = DrawFittedLabel(window->DrawList, textPos, rowW - alignedTextOffset - extraIndent - starReserve, label);
			if (cut)
				ImGui::SetTooltipEx(label);

			if (_cfg.showSidebarFavourites && (over.isFavourited || itemHovered)) {
				const char* starIcon     = ICON_FA_STAR;
				float       starScale    = 0.85f;
				float       starFontSize = m.sidebarFontSize * starScale;

				ImGui::PushFont(regularFont, starFontSize);
				ImVec2 starSize = ImGui::CalcTextSize(starIcon);
				ImGui::PopFont();

				float starX = startScreenPos.x + rowW - starSize.x - (15.0f * m.uiScale);
				float starY = startScreenPos.y + (m.sidebarItemH - starFontSize) * 0.5f - (1.0f * m.uiScale);

				// Hit-tested by hand, as the star sits on top of the row's own item
				ImRect starBB(
					ImVec2(starX - (5.0f * m.uiScale), startScreenPos.y),
					ImVec2(starX + starSize.x + (5.0f * m.uiScale), startScreenPos.y + m.sidebarItemH));
				bool starHovered = ImGui::IsMouseHoveringRect(starBB.Min, starBB.Max);

				if (starHovered && ImGui::IsMouseClicked(0)) {
					over.isFavourited = !over.isFavourited;
					OnOverridesChanged();
				}

				ImU32 starCol = starHovered ? IM_COL32(255, 255, 100, 255) : (over.isFavourited ? IM_COL32(255, 215, 0, 255) : IM_COL32(150, 150, 150, 150));

				ImGui::GetWindowDrawList()->AddText(regularFont, starFontSize,
					ImVec2(starX, starY),
					starCol, starIcon);
			}

			ImGui::PopFont();
			ImGui::SetCursorPos(endPos);
			ImGui::PopID();
		};

		// One group's row: chevron, name and star, indented by its depth. Returns whether it's open, after any click.
		// stateId holds the open state and itemId is the row's own, so a pinned copy of the row can share the state.
		// While the list is filtered every group shows open, so its matches can be seen
		auto DrawGroupRow = [&](const std::string& path, const char* label, int depth, ImGuiID stateId, ImGuiID itemId, bool forceOpen) -> bool {
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return false;

			ImGuiStorage* storage = window->DC.StateStorage;
			bool          isOpen  = forceOpen || storage->GetInt(stateId, 0) != 0;
			const bool    canFav  = path != kFavouritesPath;
			const float   indent  = depth * m.sidebarIndent;

			ImGui::PushFont(regularFont, m.sidebarFontSize);

			ImVec2 pos = window->DC.CursorPos;
			ImRect bb(pos, pos + ImVec2(ImGui::GetContentRegionAvail().x, m.sidebarItemH));

			ImGui::ItemSize(bb);
			if (ImGui::ItemAdd(bb, itemId)) {
				bool hovered, held;
				// A click on the star favourites the group without opening or closing it
				const bool onStar = canFav && IsOverStar(bb.Max.x, bb.Min.y);
				if (ImGui::ButtonBehavior(bb, itemId, &hovered, &held, 0) && !forceOpen && !onStar) {
					isOpen = !isOpen;
					storage->SetInt(stateId, isOpen);
					s_rowsDirty = true;
					ImGui::PlayAudio(isOpen ? ImGui::Audio::kFocus : ImGui::Audio::kCancel);
				}
				if (hovered)
					ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_HeaderHovered), false);

				bool isFocused = ImGui::IsItemFocused();

				// Add Gamepad 'Y' shortcut to favourite groups on the sidebar
				if (canFav && (hovered || isFocused) && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceUp, false)) {
					auto& gOver        = _groupOverrides[path];
					gOver.isFavourited = !gOver.isFavourited;
					OnOverridesChanged();
				}

				if (iconArrow) {
					ImU32 col = ImGui::GetDynamicTextColor(hovered);
					auto  ap  = chromeArrow(isOpen, m.sidebarItemH);

					ImVec2 drawPos = {
						pos.x + indent + (m.sidebarIndent * 0.5f),
						pos.y + ap.offsetY + textVisualOffset
					};
					ImGui::DrawArrowIcon(window->DrawList, drawPos, ap.drawSize, col,
						isOpen ? ImGui::IconDirection::kDown : ImGui::IconDirection::kRight);
				}

				// Cut short before the star, with the whole name in a tooltip on hover or gamepad focus
				const float textX = pos.x + indent + alignedTextOffset;
				const float textY = bb.Min.y + (m.sidebarItemH - ImGui::GetFontSize()) * 0.5f + textVisualOffset;
				const bool  cut   = DrawFittedLabel(window->DrawList, { textX, textY }, bb.Max.x - textX - (canFav ? starReserve : 8.0f * m.uiScale), label);
				if (cut)
					ImGui::SetTooltipEx(label);

				if (canFav) {
					auto it    = _groupOverrides.find(path);
					bool isFav = it != _groupOverrides.end() && it->second.isFavourited;

					if (_cfg.showSidebarFavourites && (isFav || hovered)) {
						const char* starIcon     = ICON_FA_STAR;
						float       starScale    = 0.85f;
						float       starFontSize = m.sidebarFontSize * starScale;

						ImGui::PushFont(regularFont, starFontSize);
						ImVec2 starSize = ImGui::CalcTextSize(starIcon);
						ImGui::PopFont();

						float starX = bb.Max.x - starSize.x - (15.0f * m.uiScale);
						float starY = bb.Min.y + (m.sidebarItemH - starFontSize) * 0.5f - (1.0f * m.uiScale);

						// Hit-tested by hand, as the star sits on top of the row's own item
						ImRect starBB(
							ImVec2(starX - (5.0f * m.uiScale), bb.Min.y),
							ImVec2(starX + starSize.x + (5.0f * m.uiScale), bb.Min.y + m.sidebarItemH));
						bool starHovered = ImGui::IsMouseHoveringRect(starBB.Min, starBB.Max);

						if (starHovered && ImGui::IsMouseClicked(0)) {
							isFav                              = !isFav;
							_groupOverrides[path].isFavourited = isFav;
							OnOverridesChanged();
						}

						ImU32 starCol = starHovered ? IM_COL32(255, 255, 100, 255) : (isFav ? IM_COL32(255, 215, 0, 255) : IM_COL32(150, 150, 150, 150));

						ImGui::GetWindowDrawList()->AddText(regularFont, starFontSize,
							ImVec2(starX, starY),
							starCol, starIcon);
					}
				}
			}

			ImGui::PopFont();
			return isOpen;
		};

		// --- LIST (scrolls between the header and the footer) ---
		const float separatorThickness = std::max(1.0f, std::round(ImGui::GetStyle().WindowBorderSize));
		const float footerH            = separatorThickness + m.sidebarItemH + bottomPadding;
		const float listH              = std::max(m.sidebarItemH, ImGui::GetContentRegionAvail().y - footerH);

		ImGui::BeginChild("##SidebarList", ImVec2(0.0f, listH), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
		{
			ImGuiWindow*  listWindow = ImGui::GetCurrentWindow();
			ImGuiStorage* storage    = listWindow->DC.StateStorage;

			// Groups moved since the last frame keep their open state, and so does everything in them
			if (!_movedGroups.empty()) {
				const auto& moveLayout = GetToolLayout();
				for (const auto& [from, to] : _movedGroups) {
					for (const auto& [path, node] : moveLayout.tree) {
						if (!Sidebar::IsInGroup(path, to))
							continue;
						const std::string old = from + path.substr(to.size());
						storage->SetInt(ImGui::GetID(("##grp|" + path).c_str()), storage->GetInt(ImGui::GetID(("##grp|" + old).c_str()), 0));
					}
				}
				_movedGroups.clear();
				s_rowsDirty = true;
			}
			const float viewTop   = listWindow->ClipRect.Min.y;
			const float itemH     = m.sidebarItemH;
			const bool  filtering = filterBuf[0] != '\0';

			// --- Rebuild the rows, only when something they show has changed ---
			const auto& layout       = GetToolLayout();
			const int   configBits   = (_cfg.showSidebarFavourites ? 1 : 0) | (_cfg.groupFavourites ? 2 : 0);
			const bool  filterChange = s_rowsFilter != filterBuf;

			if (s_rowsDirty || filterChange || layout.version != s_rowsVersion || configBits != s_rowsConfig) {
				s_rowsDirty   = false;
				s_rowsVersion = layout.version;
				s_rowsConfig  = configBits;
				s_rowsFilter  = filterBuf;
				s_rows.clear();

				std::string lowerFilter = filtering ? clib_util::string::tolower(std::string(filterBuf)) : std::string();
				std::string lowerName;

				auto isFavourite = [&](FUCK::ITool* t) {
					auto it = _toolOverrides.find(GetToolKey(t));
					return _cfg.showSidebarFavourites && it != _toolOverrides.end() && it->second.isFavourited;
				};

				auto isShown = [&](FUCK::ITool* t) {
					auto it = _toolOverrides.find(GetToolKey(t));
					if (it != _toolOverrides.end() && it->second.isHidden)
						return false;
					if (!filtering)
						return true;

					// Into one reused buffer, so filtering doesn't allocate per row
					lowerName = GetToolLabel(t);
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
						[](unsigned char c) { return static_cast<unsigned char>(std::tolower(c)); });
					return rapidfuzz::fuzz::partial_token_ratio(lowerFilter, lowerName) >= 65.0;
				};

				auto isGroupOpen = [&](ImGuiID id) {
					return filtering || storage->GetInt(id, 0) != 0;
				};

				auto groupStateId = [&](const std::string& path) {
					return ImGui::GetID(("##grp|" + path).c_str());
				};

				// A favourited group shows up top with the favourites, from whatever level it's at
				auto isFavouriteGroup = [&](const std::string& g) {
					if (!_cfg.showSidebarFavourites)
						return false;
					auto gIt = _groupOverrides.find(g);
					return gIt != _groupOverrides.end() && gIt->second.isFavourited;
				};

				// Whether a group has anything to show, for a closed group whose rows aren't added.
				// Favourited tools and groups are shown with the favourites, so they don't count
				std::function<bool(const std::string&)> hasShownTool = [&](const std::string& path) -> bool {
					auto it = layout.tree.find(path);
					if (it == layout.tree.end())
						return false;
					for (const auto& e : it->second.entries) {
						if (e.tool) {
							if (isShown(e.tool) && !isFavourite(e.tool))
								return true;
						} else if (!isFavouriteGroup(e.group) && hasShownTool(e.group)) {
							return true;
						}
					}
					return false;
				};

				std::function<bool(const std::string&, int)> addLevel;

				// Adds a group's row, and its contents when it's open. A group with nothing to show is left out
				auto addGroup = [&](const std::string& g, int depth) -> bool {
					const std::size_t at = s_rows.size();
					const ImGuiID     id = groupStateId(g);
					s_rows.push_back({ nullptr, g, GetGroupDisplayName(g), depth, id });

					const bool hasContent = isGroupOpen(id) ? addLevel(g, depth + 1) : hasShownTool(g);
					if (!hasContent) {
						s_rows.resize(at);
						return false;
					}
					return true;
				};

				// Adds a level's groups and tools at depth, in the layout's order. Returns whether anything was added
				addLevel = [&](const std::string& path, int depth) -> bool {
					auto it = layout.tree.find(path);
					if (it == layout.tree.end())
						return false;

					bool added = false;
					for (const auto& e : it->second.entries) {
						if (e.tool) {
							if (!isShown(e.tool) || isFavourite(e.tool))
								continue;
							s_rows.push_back({ e.tool, std::string(), GetToolLabel(e.tool), depth });
							added = true;
						} else if (!isFavouriteGroup(e.group)) {
							if (addGroup(e.group, depth))
								added = true;
						}
					}
					return added;
				};

				// Favourites first: favourited tools and groups together, alphabetically, under a header of their own or as
				// loose rows. A favourited group brings everything in it along, from whatever level it's at
				struct Favourite
				{
					std::string  name;
					std::string  tieBreak;  // a fixed order between the same name
					FUCK::ITool* tool = nullptr;
					std::string  group;
				};
				std::vector<Favourite> favourites;
				for (const auto& [path, node] : layout.tree) {
					for (auto* t : node.tools) {
						if (isFavourite(t) && isShown(t))
							favourites.push_back({ GetToolLabel(t), GetToolKey(t), t, std::string() });
					}
					for (const auto& g : node.groups) {
						if (isFavouriteGroup(g))
							favourites.push_back({ GetGroupDisplayName(g), g, nullptr, g });
					}
				}
				std::sort(favourites.begin(), favourites.end(), [](const Favourite& a, const Favourite& b) {
					if (int cmp = _stricmp(a.name.c_str(), b.name.c_str()); cmp != 0)
						return cmp < 0;
					return a.tieBreak < b.tieBreak;
				});

				// Adds one favourite at depth. Returns false for a group with nothing to show, which is left out
				auto addFavourite = [&](const Favourite& f, int depth) -> bool {
					if (f.tool) {
						s_rows.push_back({ f.tool, std::string(), GetToolLabel(f.tool), depth });
						return true;
					}
					return addGroup(f.group, depth);
				};

				if (!favourites.empty()) {
					if (_cfg.groupFavourites) {
						const std::size_t at = s_rows.size();
						const ImGuiID     id = groupStateId(kFavouritesPath);
						s_rows.push_back({ nullptr, kFavouritesPath, "$FUCK_Sidebar_FavouritesGroup"_T, 0, id });

						// Open, its rows are added. Closed, it stays as long as one of them would show
						bool hasContent = false;
						for (const auto& f : favourites) {
							if (isGroupOpen(id)) {
								if (addFavourite(f, 1))
									hasContent = true;
							} else if (f.tool || hasShownTool(f.group)) {
								hasContent = true;
								break;
							}
						}
						if (!hasContent)
							s_rows.resize(at);
					} else {
						for (const auto& f : favourites)
							addFavourite(f, 0);
					}
				}

				// Then everything else, groups and tools together in the layout's order
				addLevel(std::string(), 0);

				// Each group's section and each row's group, for the stuck headers
				std::vector<int> enclosing;
				for (int i = 0; i < static_cast<int>(s_rows.size()); ++i) {
					auto& row = s_rows[i];
					while (!enclosing.empty() && s_rows[enclosing.back()].depth >= row.depth) {
						s_rows[enclosing.back()].sectionEnd = i;
						enclosing.pop_back();
					}
					row.parent = enclosing.empty() ? -1 : enclosing.back();
					if (!row.tool)
						enclosing.push_back(i);
				}
				for (int g : enclosing)
					s_rows[g].sectionEnd = static_cast<int>(s_rows.size());
			}

			// --- Stuck headers: the open group at each level whose header has scrolled away ---
			// Copies of their headers stack at the top of the list, outermost first. Each stays until its section's last
			// row reaches it, then is pushed up, sliding behind the one above
			struct StuckHeader
			{
				int   row;
				float y;     // where it's drawn, pushed up as its section ends
				float slot;  // where it sits when it isn't being pushed
			};
			std::vector<StuckHeader> stuck;

			const float listTop = listWindow->DC.CursorPos.y;
			if (!s_rows.empty() && listTop < viewTop) {
				float slot = viewTop;
				for (int level = 0; level <= Sidebar::kMaxGroupDepth; ++level) {
					// The row showing under this slot, and the group at this level that holds it
					const int under = std::clamp(static_cast<int>((slot - listTop) / itemH), 0, static_cast<int>(s_rows.size()) - 1);
					int       group = -1;
					for (int r = under; r >= 0; r = s_rows[r].parent) {
						if (!s_rows[r].tool && s_rows[r].depth == level) {
							group = r;
							break;
						}
					}

					// Nothing held here, a closed or empty group, or its own header is still in view below the slot
					if (group < 0 || s_rows[group].sectionEnd <= group + 1 || listTop + group * itemH >= slot)
						break;

					const float y = std::min(slot, listTop + s_rows[group].sectionEnd * itemH - itemH);
					stuck.push_back({ group, y, slot });
					slot = y + itemH;
				}
			}

			float stackBottom = viewTop;
			for (const auto& h : stuck)
				stackBottom = std::max(stackBottom, h.y + itemH);

			// --- Rows, only those on screen ---
			// Clipped below the stuck headers, so rows can't show through them or take their clicks
			if (!stuck.empty())
				ImGui::PushClipRect(ImVec2(listWindow->ClipRect.Min.x, stackBottom), listWindow->ClipRect.Max, true);

			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(s_rows.size()), itemH);
			while (clipper.Step()) {
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
					const SidebarRow& row = s_rows[i];
					if (row.tool) {
						RenderSidebarItem(row.tool, row.label.c_str(), row.depth * m.sidebarIndent);
					} else {
						DrawGroupRow(row.path, row.label.c_str(), row.depth, row.stateId, row.stateId, filtering);
					}

					// Navigating up can land on a row hidden behind the stuck headers, which ImGui won't scroll to, as
					// it's inside the window. So it's scrolled out from under them
					if (!stuck.empty() && ImGui::IsItemFocused()) {
						const float rowTop = listTop + static_cast<float>(i) * itemH;
						if (rowTop < stackBottom)
							ImGui::SetScrollY(listWindow, listWindow->Scroll.y - (stackBottom - rowTop));
					}
				}
			}
			clipper.End();

			if (!stuck.empty())
				ImGui::PopClipRect();

			// Drawn last so they're on top and take the clicks, innermost first so the outer ones cover it as it slides
			// up behind them. Then the layout is put back as if they were never there
			if (!stuck.empty() && !listWindow->SkipItems) {
				auto&        dc           = listWindow->DC;
				const ImVec2 cursor       = dc.CursorPos;
				const ImVec2 cursorMax    = dc.CursorMaxPos;
				const ImVec2 prevLinePos  = dc.CursorPosPrevLine;
				const ImVec2 prevLineSize = dc.PrevLineSize;
				const ImVec2 currLineSize = dc.CurrLineSize;
				const float  prevBase     = dc.PrevLineTextBaseOffset;
				const float  currBase     = dc.CurrLineTextBaseOffset;
				const bool   isSameLine   = dc.IsSameLine;

				// Copied, as a click below can rebuild the rows
				std::vector<std::pair<SidebarRow, StuckHeader>> headers;
				for (const auto& h : stuck)
					headers.emplace_back(s_rows[h.row], h);

				ImGui::PushID("StuckHeader");
				for (int d = static_cast<int>(headers.size()) - 1; d >= 0; --d) {
					const auto& [row, h] = headers[d];

					// Kept below the header above it
					const float top = d > 0 ? headers[d - 1].second.y + itemH : viewTop;
					ImGui::PushClipRect(ImVec2(listWindow->ClipRect.Min.x, top), listWindow->ClipRect.Max, true);

					dc.CursorPos = ImVec2(cursor.x, h.y);

					// Under its own ID, but sharing the group's open state, so closing the copy closes the group
					const ImGuiID itemId    = ImGui::GetID(row.path.c_str());
					const bool    stillOpen = DrawGroupRow(row.path, row.label.c_str(), row.depth, row.stateId, itemId, filtering);

					ImGui::PopClipRect();

					// Collapsing from here would leave the view where its rows were, so scroll back to put its header in its slot
					if (!stillOpen)
						ImGui::SetScrollY(listWindow, listWindow->Scroll.y - (h.slot - (listTop + h.row * itemH)));
				}
				ImGui::PopID();

				dc.CursorPos              = cursor;
				dc.CursorMaxPos           = cursorMax;
				dc.CursorPosPrevLine      = prevLinePos;
				dc.PrevLineSize           = prevLineSize;
				dc.CurrLineSize           = currLineSize;
				dc.PrevLineTextBaseOffset = prevBase;
				dc.CurrLineTextBaseOffset = currBase;
				dc.IsSameLine             = isSameLine;
			}
		}
		ImGui::EndChild();

		// --- FOOTER: SETTINGS (Centred) ---
		ImGui::SeparatorThick();
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
	}
	ImGui::EndChild();
}
