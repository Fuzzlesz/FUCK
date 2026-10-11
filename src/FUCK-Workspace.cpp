#include "FUCK-Man.h"
#include "FUCK-Host.h"
#include "FUCK-Windows.h"

#include "ImGui/Audio.h"
#include "ImGui/IconsFontAwesome6.h"
#include "ImGui/IconsFonts.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/Util.h"
#include "ImGui/Widgets.h"

#include "System/Compat.h"
#include "System/Hotkeys.h"
#include "System/Input.h"
#include "System/Settings.h"
#include "System/Utils.h"

// ==================================================
// Glaze JSON Serialization Structs
// ==================================================
struct WindowSaveData
{
	float x         = -1.0f;
	float y         = -1.0f;
	float w         = -1.0f;
	float h         = -1.0f;
	bool  collapsed = false;
	float scale     = 1.0f;
	float rotation  = 0.0f;
};

struct ToolSaveData
{
	bool        bFavourited = false;
	bool        bHidden     = false;
	std::string sName       = "";
	std::string sGroup      = "";
};

struct PluginSaveData
{
	StringMap<ToolSaveData>   tools;
	StringMap<WindowSaveData> windows;
};

struct GroupSaveData
{
	bool        bFavourited = false;
	std::string sName       = "";
	int         iOrder      = 0;
};

struct WorkspaceSaveData
{
	WindowSaveData           mainWindow;
	StringMap<int>           toolOrder;
	StringMap<GroupSaveData> groups;
	StringMap<std::string>   groupMoves;  // groups the user moved: the path the plugins give, and where it is now
};

// ==================================================
// Workspace
// ==================================================

// What each workspace file last held, as written or read this session, so a save only writes the files that changed
static StringMap<std::string> s_workspaceFiles;

// Plugin files that couldn't be read. Left alone, so a typo made by hand isn't wiped out by the next save
static std::set<std::string> s_unreadablePluginFiles;

// Likewise for the workspace file itself
static bool s_workspaceUnreadable = false;

// One spelling per file, whichever way its path was put together, so it's tracked under one key
static std::string NormalisePath(const fs::path& a_path)
{
	return a_path.lexically_normal().string();
}

template <class T>
static void WriteJsonIfChanged(const T& a_data, const std::string& a_path)
{
	std::string buffer;
	if (auto err = glz::write_json(a_data, buffer); err) {
		logger::warn("Failed to serialise {}", a_path);
		return;
	}

	// Not read or written yet this session, so compare against what's on disk
	if (s_workspaceFiles.find(a_path) == s_workspaceFiles.end()) {
		std::ifstream in(a_path, std::ios::binary);
		if (in) {
			s_workspaceFiles[a_path] = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}
	}

	// Unchanged, and still there (it may have been deleted by hand)
	if (auto it = s_workspaceFiles.find(a_path); it != s_workspaceFiles.end() && it->second == buffer && fs::exists(a_path))
		return;

	std::ofstream out(a_path, std::ios::binary | std::ios::trunc);
	if (!out) {
		logger::warn("Failed to write to {}", a_path);
		return;
	}
	out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
	s_workspaceFiles[a_path] = std::move(buffer);
}

void FUCKMan::LoadWorkspace()
{
	float scale            = ImGui::Renderer::GetResolutionScale();
	bool  mainWindowLoaded = false;

	// Load global workspace settings. (Main Window, Groups & Tool Order)
	std::string workspacePath = NormalisePath(Settings::GetSingleton()->GetWorkspacePath());
	if (fs::exists(workspacePath)) {
		WorkspaceSaveData sd;
		std::string       wBuffer;
		if (auto err = glz::read_file_json(sd, workspacePath, wBuffer); !err) {
			s_workspaceFiles[workspacePath] = wBuffer;

			// Apply Main Window state
			if (sd.mainWindow.x != -1.0f && sd.mainWindow.y != -1.0f) {
				_cfg.windowPos  = { sd.mainWindow.x * scale, sd.mainWindow.y * scale };
				_cfg.windowSize = { sd.mainWindow.w * scale, sd.mainWindow.h * scale };
				_isCollapsed    = sd.mainWindow.collapsed;

				_lastSavedPos         = _cfg.windowPos;
				_lastSavedSize        = _cfg.windowSize;
				_pendingWindowRestore = true;
				mainWindowLoaded      = true;
			}

			for (const auto& [k, v] : sd.toolOrder) {
				_toolOverrides[k].sortOrder = v;
			}
			for (const auto& [k, v] : sd.groups) {
				// Saved before groups could nest, or typed by hand, so tidied to match the paths they're looked up by
				auto& over        = _groupOverrides[Sidebar::NormaliseGroupPath(k)];
				over.isFavourited = v.bFavourited;
				over.customName   = v.sName;
				over.sortOrder    = v.iOrder;
			}
			for (const auto& [from, to] : sd.groupMoves) {
				const std::string fromPath = Sidebar::NormaliseGroupPath(from);
				const std::string toPath   = Sidebar::NormaliseGroupPath(to);
				if (!fromPath.empty() && !toPath.empty() && fromPath != toPath)
					_groupMoves[fromPath] = toPath;
			}
		} else {
			s_workspaceUnreadable = true;
			logger::warn("FUCK: Couldn't read {}, so it'll be left as it is", workspacePath);
		}
	}

	if (!mainWindowLoaded) {
		_cfg.windowPos  = { _def.windowPos.x * scale, _def.windowPos.y * scale };
		_cfg.windowSize = { _def.windowSize.x * scale, _def.windowSize.y * scale };
		_lastSavedPos   = _cfg.windowPos;
		_lastSavedSize  = _cfg.windowSize;
	}

	// Load per-plugin tool states and window geometries.
	std::string toolsPath = Settings::GetSingleton()->GetToolsPath();
	if (fs::exists(toolsPath)) {
		for (const auto& entry : fs::directory_iterator(toolsPath)) {
			if (entry.path().extension() == ".json") {
				std::string pluginName = entry.path().stem().string();
				std::string path       = NormalisePath(entry.path());

				PluginSaveData pd;
				std::string    pBuffer;
				if (auto err = glz::read_file_json(pd, path, pBuffer); !err) {
					s_workspaceFiles[path] = pBuffer;

					for (const auto& [toolName, toolData] : pd.tools) {
						std::string key                  = std::format("{}|{}", pluginName, toolName);
						_toolOverrides[key].isFavourited = toolData.bFavourited;
						_toolOverrides[key].isHidden     = toolData.bHidden;
						_toolOverrides[key].customName   = toolData.sName;
						_toolOverrides[key].customGroup  = toolData.sGroup;
					}

					for (const auto& [winId, winData] : pd.windows) {
						std::string key = std::format("{}|{}", pluginName, winId);
						auto&       st  = s_windowStates[key];

						if (winData.x >= 0.0f && winData.y >= 0.0f) {
							st.pos          = { winData.x * scale, winData.y * scale };
							st.hasLoadedPos = true;
						}
						if (winData.w >= 0.0f && winData.h >= 0.0f) {
							st.size = { winData.w * scale, winData.h * scale };
						}
						st.isCollapsed = winData.collapsed;
						st.scale       = std::clamp(winData.scale, kMinEditScale, kMaxEditScale);
						st.rotation    = winData.rotation;
					}
				} else {
					s_unreadablePluginFiles.insert(pluginName);
					logger::warn("FUCK: Couldn't read {}, so it'll be left as it is", path);
				}
			}
		}
	}

	MarkLayoutDirty();
}

void FUCKMan::SaveWorkspace()
{
	_workspaceDirty   = true;
	_workspaceDirtyAt = std::chrono::steady_clock::now();
}

void FUCKMan::FlushWorkspace(bool a_force)
{
	// Never before it's loaded, or the defaults would be written over what's saved
	if (!_workspaceDirty || !_workspaceLoaded)
		return;

	if (!a_force && std::chrono::steady_clock::now() - _workspaceDirtyAt < std::chrono::milliseconds(500))
		return;

	_workspaceDirty = false;
	WriteWorkspace();
}

void FUCKMan::WriteWorkspace()
{
	WorkspaceSaveData         sd;
	StringMap<PluginSaveData> pdMap;

	// Distribute Tool Overrides. Only what differs from the defaults is kept
	for (const auto& [key, over] : _toolOverrides) {
		auto pipe = key.find('|');
		if (pipe != std::string::npos) {
			std::string plugin   = key.substr(0, pipe);
			std::string toolName = key.substr(pipe + 1);

			if (over.isFavourited || over.isHidden || !over.customName.empty() || !over.customGroup.empty()) {
				ToolSaveData td;
				td.bFavourited = over.isFavourited;
				td.bHidden     = over.isHidden;
				td.sName       = over.customName;
				td.sGroup      = over.customGroup;

				pdMap[plugin].tools[toolName] = td;
			}

			if (over.sortOrder != 0) {
				sd.toolOrder[key] = over.sortOrder;
			}
		}
	}

	// Distribute Group Overrides
	for (const auto& [grp, over] : _groupOverrides) {
		if (over.isFavourited || !over.customName.empty() || over.sortOrder != 0) {
			GroupSaveData gd;
			gd.bFavourited = over.isFavourited;
			gd.sName       = over.customName;
			gd.iOrder      = over.sortOrder;

			sd.groups[grp] = gd;
		}
	}

	for (const auto& [from, to] : _groupMoves) {
		sd.groupMoves[from] = to;
	}

	// Distribute Main Window Geometry
	float scale = ImGui::Renderer::GetResolutionScale();
	if (scale < 0.1f)
		scale = 1.0f;  // Safety clamp

	sd.mainWindow.x         = _cfg.windowPos.x / scale;
	sd.mainWindow.y         = _cfg.windowPos.y / scale;
	sd.mainWindow.w         = _cfg.windowSize.x / scale;
	sd.mainWindow.h         = _cfg.windowSize.y / scale;
	sd.mainWindow.collapsed = _isCollapsed;

	// Distribute Window Geometries (External plugins)
	for (const auto& [key, st] : s_windowStates) {
		auto pipe = key.find('|');
		if (pipe == std::string::npos)
			continue;

		std::string plugin = key.substr(0, pipe);
		std::string winId  = key.substr(pipe + 1);

		const FUCK::IWindow* registered = nullptr;
		for (auto* w : _windows) {
			if (w->PluginName() == plugin && w->Id() == winId) {
				registered = w;
				break;
			}
		}

		// Nothing the user changed, so nothing to keep. The window follows its defaults
		const bool hasPos  = st.hasLoadedPos;
		const bool hasSize = st.size.x >= 0.0f && st.size.y >= 0.0f;
		const bool hasEdit = std::abs(st.scale - 1.0f) > 0.001f || std::abs(st.rotation) > 0.001f;
		if (!hasPos && !hasSize && !st.isCollapsed && !hasEdit)
			continue;

		// A window from a plugin that isn't loaded this session keeps what it had, untouched
		if (!registered) {
			WindowSaveData wd;
			wd.x         = hasPos ? st.pos.x / scale : -1.0f;
			wd.y         = hasPos ? st.pos.y / scale : -1.0f;
			wd.w         = hasSize ? st.size.x / scale : -1.0f;
			wd.h         = hasSize ? st.size.y / scale : -1.0f;
			wd.collapsed = st.isCollapsed;
			wd.scale     = st.scale;
			wd.rotation  = st.rotation;

			pdMap[plugin].windows[winId] = wd;
			continue;
		}

		const auto flags         = registered->GetFlags();
		const bool isEditable    = flags & FUCK::WindowFlags::kEditable;
		const bool isCustomPos   = flags & FUCK::WindowFlags::kCustomPosition;
		const bool isUndecorated = flags & FUCK::WindowFlags::kNoDecoration;
		const bool isNoMove      = flags & FUCK::WindowFlags::kNoMove;
		const bool isFixedSize   = (flags & FUCK::WindowFlags::kNoResize) || (flags & FUCK::WindowFlags::kAutoResize);

		// Editable widgets only save what the user changed, so an untouched one keeps following its default
		if (isEditable) {
			if (!st.IsLayoutModified())
				continue;

			WindowSaveData wd;
			wd.x        = st.hasLoadedPos ? st.pos.x / scale : -1.0f;
			wd.y        = st.hasLoadedPos ? st.pos.y / scale : -1.0f;
			wd.scale    = st.scale;
			wd.rotation = st.rotation;

			pdMap[plugin].windows[winId] = wd;
			continue;
		}

		// Skip saving to JSON entirely for custom-positioned or unmovable widgets
		if (isCustomPos || isNoMove) {
			continue;
		}

		WindowSaveData wd;
		wd.x         = hasPos ? st.pos.x / scale : -1.0f;
		wd.y         = hasPos ? st.pos.y / scale : -1.0f;
		wd.w         = (hasSize && !isFixedSize) ? st.size.x / scale : -1.0f;
		wd.h         = (hasSize && !isFixedSize) ? st.size.y / scale : -1.0f;
		wd.collapsed = !isUndecorated && st.isCollapsed;

		// Its size alone doesn't count when the window sizes itself
		if (!hasPos && wd.w < 0.0f && !wd.collapsed)
			continue;

		pdMap[plugin].windows[winId] = wd;
	}

	// Commit Workspace Settings
	std::string workspacePath = NormalisePath(Settings::GetSingleton()->GetWorkspacePath());
	fs::create_directories(Settings::Core.GetConfigDirectory());
	if (!s_workspaceUnreadable)
		WriteJsonIfChanged(sd, workspacePath);

	// Commit Per-Plugin Settings
	std::string toolsPath = Settings::GetSingleton()->GetToolsPath();
	fs::create_directories(toolsPath);

	for (const auto& [plugin, pd] : pdMap) {
		if (s_unreadablePluginFiles.contains(plugin))
			continue;
		WriteJsonIfChanged(pd, NormalisePath(fs::path(toolsPath) / (plugin + ".json")));
	}

	// A plugin whose settings are all back to default has nothing left to keep, so its file goes.
	// Otherwise a reset name or group would come back from the old file next session
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(toolsPath, ec)) {
		if (entry.path().extension() != ".json")
			continue;

		std::string pluginName = entry.path().stem().string();
		if (pdMap.find(pluginName) != pdMap.end() || s_unreadablePluginFiles.contains(pluginName))
			continue;

		std::error_code removeEc;
		fs::remove(entry.path(), removeEc);
		s_workspaceFiles.erase(NormalisePath(entry.path()));
	}
}
