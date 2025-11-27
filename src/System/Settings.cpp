#include "Settings.h"

std::vector<std::string> Settings::GetConfigs(const std::filesystem::path& a_path, const std::string& a_ext)
{
	std::vector<std::string> configs;

	if (!std::filesystem::exists(a_path)) {
		try {
			std::filesystem::create_directories(a_path);
		} catch (...) {
			return configs;
		}
	}

	for (const auto& entry : std::filesystem::directory_iterator(a_path)) {
		if (entry.is_regular_file()) {
			auto path = entry.path();
			std::string ext = path.extension().string();

			if (ext == a_ext) {
				configs.push_back(path.filename().string());
			}
		}
	}

	std::sort(configs.begin(), configs.end());
	return configs;
}

void Settings::LoadINI(const wchar_t* a_path, const INIFunc a_func, bool a_generate)
{
	CSimpleIniA ini;
	ini.SetUnicode();

	std::filesystem::path p(a_path);
	auto pathStr = p.string();

	if (a_generate) {
		std::filesystem::create_directories(p.parent_path());
	}

	if (ini.LoadFile(pathStr.c_str()) >= SI_OK || a_generate) {
		a_func(ini);

		if (a_generate) {
			(void)ini.SaveFile(pathStr.c_str());
		}
	}
}

void Settings::LoadINI(const wchar_t* a_defaultPath, const wchar_t* a_userPath, INIFunc a_func)
{
	LoadINI(a_defaultPath, a_func);
	LoadINI(a_userPath, a_func);
}

void Settings::Load(FileType type, INIFunc a_func, bool) const
{
	if (!a_func)
		return;

	switch (type) {
	case FileType::kSettings:
		LoadINI(defaultSettingsPath, userSettingsPath, a_func);
		break;
	case FileType::kDisplayTweaks:
		LoadINI(defaultDisplayTweaksPath, userDisplayTweaksPath, a_func);
		break;
	default:
		break;
	}
}

void Settings::Save(FileType type, INIFunc a_func, bool) const
{
	if (!a_func)
		return;

	const bool forceGenerate = true;

	switch (type) {
	case FileType::kSettings:
		LoadINI(userSettingsPath, a_func, forceGenerate);
		break;
	default:
		break;
	}
}
