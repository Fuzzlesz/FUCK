#include "Settings.h"

void Settings::LoadINI(const char* a_path, const INIFunc a_func, bool a_generate)
{
	if (!a_path || a_path[0] == '\0')
		return;

	CSimpleIniA ini;
	ini.SetUnicode();

	std::filesystem::path p(a_path);
	p.make_preferred();
	auto pathStr = p.string();

	if (a_generate) {
		std::filesystem::create_directories(p.parent_path());
	}

	SI_Error rc = ini.LoadFile(pathStr.c_str());

	if (rc >= SI_OK || a_generate) {
		{
			std::lock_guard<std::mutex> lock(GetSingleton()->trackingMutex);
			GetSingleton()->trackedINIs.insert(pathStr);
		}

		if (rc >= SI_OK && !a_generate) {
			logger::info("Loaded INI from {}", pathStr);
		}

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
	case FileType::kStyle:
		LoadINI(GetDefaultStylePath().c_str(), a_func);
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
	case FileType::kStyle:
		LoadINI(GetDefaultStylePath().c_str(), a_func, true);
		break;
	default:
		break;
	}
}
