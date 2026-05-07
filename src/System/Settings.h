#pragma once

enum class FileType {
	kSettings,
	kStyle,
	kDisplayTweaks,
};

class Settings
{
public:
	using INIFunc = std::function<void(CSimpleIniA&)>;

	std::set<std::string> trackedINIs;
	std::mutex trackingMutex;

	static Settings* GetSingleton()
	{
		static Settings instance;
		return &instance;
	}

	void Load(FileType type, INIFunc a_func) const;
	void Save(FileType type, INIFunc a_func) const;

	static std::vector<std::string> GetConfigs(const std::filesystem::path& a_path, const std::string& a_ext = ".ini");

	const char* GetPresetsPath() const { return presetsRoot; }
	const char* GetUserFontsPath() const { return userFontsPath; }
	const char* GetLegacyFontsPath() const { return legacyFontsPath; }

	static void LoadINI(const char* a_path, INIFunc a_func, bool a_generate = false);
	static void LoadINI(const char* a_defaultPath, const char* a_userPath, INIFunc a_func);

private:
	const char* settingsDefaultPath{ R"(Data\SKSE\Plugins\FUCK\settings\FUCK.ini)" };
	const char* settingsUserPath{ R"(Data\SKSE\Plugins\FUCK\settings-user\FUCK_user.ini)" };

	const char* stylePath{ R"(Data\SKSE\Plugins\FUCK\FUCK_Style.ini)" };

	const char* presetsRoot{ R"(Data\SKSE\Plugins\FUCK\styles)" };

	const char* userFontsPath{ R"(Data\SKSE\Plugins\FUCK\fonts)" };

	const char* legacyFontsPath			{ R"(Data\Interface\ImGuiIcons\Fonts)" };
	
	const char* defaultDisplayTweaksPath{ R"(Data\SKSE\Plugins\SSEDisplayTweaks.ini)" };
	const char* userDisplayTweaksPath	{ R"(Data\SKSE\Plugins\SSEDisplayTweaks_Custom.ini)" };
};
