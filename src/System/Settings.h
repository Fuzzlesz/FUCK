#pragma once

enum class FileType
{
	kSettings,
	kDisplayTweaks,
};

class Settings
{
public:
	using INIFunc = std::function<void(CSimpleIniA&)>;

	static Settings* GetSingleton()
	{
		static Settings instance;
		return &instance;
	}

	void Load(FileType type, INIFunc a_func, bool a_generate = false) const;
	void Save(FileType type, INIFunc a_func, bool a_generate = false) const;

	static std::vector<std::string> GetConfigs(const std::filesystem::path& a_path, const std::string& a_ext = ".ini");

	const wchar_t* GetPresetsPath() const { return presetsRoot; }
	const wchar_t* GetUserFontsPath() const { return userFontsPath; }
	const wchar_t* GetLegacyFontsPath() const { return legacyFontsPath; }

private:
	static void LoadINI(const wchar_t* a_path, INIFunc a_func, bool a_generate = false);
	static void LoadINI(const wchar_t* a_defaultPath, const wchar_t* a_userPath, INIFunc a_func);

	const wchar_t* defaultSettingsPath{ L"Data/SKSE/Plugins/FUCK.ini" };
	const wchar_t* userSettingsPath{ L"Data/SKSE/Plugins/FUCK_Custom.ini" };

	const wchar_t* defaultDisplayTweaksPath{ L"Data/SKSE/Plugins/SSEDisplayTweaks.ini" };
	const wchar_t* userDisplayTweaksPath{ L"Data/SKSE/Plugins/SSEDisplayTweaks_Custom.ini" };

	const wchar_t* presetsRoot{ L"Data/Interface/FUCK/Presets" };
	const wchar_t* userFontsPath{ L"Data/Interface/FUCK/Fonts" };
	const wchar_t* legacyFontsPath{ L"Data/Interface/ImGuiIcons/Fonts" };
};
