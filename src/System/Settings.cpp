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

void Settings::LoadINI(const char* a_path, const INIFunc a_func, bool a_generate)
{
	if (!a_path || a_path[0] == '\0')
		return;

	CSimpleIniA ini;
	ini.SetUnicode();

	std::filesystem::path p(a_path);

	p.make_preferred();
	auto pathStr = p.string();

	{
		std::lock_guard<std::mutex> lock(GetSingleton()->trackingMutex);
		GetSingleton()->trackedINIs.insert(pathStr);
	}

	if (a_generate) {
		std::filesystem::create_directories(p.parent_path());
	}

	SI_Error rc = ini.LoadFile(pathStr.c_str());

	if (rc >= SI_OK && !a_generate) {
		logger::info("Loaded INI from {}", pathStr);
	}

	if (rc >= SI_OK || a_generate) {
		a_func(ini);

		if (a_generate) {
			(void)ini.SaveFile(pathStr.c_str());
		}
	}
}

void Settings::LoadINI(const char* a_defaultPath, const char* a_userPath, INIFunc a_func)
{
	LoadINI(a_defaultPath, a_func);
	LoadINI(a_userPath, a_func);
}

void Settings::Load(FileType type, INIFunc a_func) const
{
	if (!a_func)
		return;

	switch (type) {
	case FileType::kSettings:
		LoadINI(settingsDefaultPath, settingsUserPath, a_func);
		break;
	case FileType::kStyle:
		LoadINI(stylePath, a_func);
		break;
	case FileType::kDisplayTweaks:
		LoadINI(defaultDisplayTweaksPath, userDisplayTweaksPath, a_func);
		break;
	default:
		break;
	}
}

void Settings::Save(FileType type, INIFunc a_func) const
{
	if (!a_func)
		return;

	switch (type) {
	case FileType::kSettings:
		LoadINI(settingsUserPath, a_func, true);
		break;
	case FileType::kStyle:
		LoadINI(stylePath, a_func, true);
		break;
	default:
		break;
	}
}
