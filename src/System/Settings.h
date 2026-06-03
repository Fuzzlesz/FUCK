#pragma once

enum class FileType
{
	kStyle,
	kDisplayTweaks,
};

class Settings : public REX::Singleton<Settings>
{
public:
	using INIFunc = std::function<void(CSimpleIniA&)>;

	std::set<std::string> trackedINIs;
	std::mutex            trackingMutex;

	// Centralised instance for the framework's own settings
	static inline const FUCK::PluginSettings Core{ "FUCK" };

	void Load(FileType type, INIFunc a_func) const;
	void Save(FileType type, INIFunc a_func) const;

	// Dynamically build FUCK's own resource directories using the API
	std::string GetStylesPath() const       { return Core.GetConfigDirectory() + "styles"; }
	std::string GetUserFontsPath() const    { return Core.GetConfigDirectory() + "fonts"; }
	std::string GetDefaultStylePath() const { return Core.GetConfigDirectory() + "defaultstyle.ini"; }
	std::string GetToolsPath() const        { return Core.GetConfigDirectory() + "tools"; }
	std::string GetWorkspacePath() const    { return Core.GetConfigDirectory() + "workspace.json"; }

	const char* GetLegacyFontsPath() const  { return legacyFontsPath; }
	const char* GetCSFontsPath() const      { return csFontsPath; }

	static void LoadINI(const char* a_path, INIFunc a_func, bool a_generate = false);
	static void LoadINI(const char* a_defaultPath, const char* a_userPath, INIFunc a_func);

private:
	const char* legacyFontsPath          { R"(Data\Interface\ImGuiIcons\Fonts)" };
	const char* csFontsPath              { R"(Data\Interface\CommunityShaders\Fonts)" };

	const char* defaultDisplayTweaksPath { R"(Data\SKSE\Plugins\SSEDisplayTweaks.ini)" };
	const char* userDisplayTweaksPath    { R"(Data\SKSE\Plugins\SSEDisplayTweaks_Custom.ini)" };
};
