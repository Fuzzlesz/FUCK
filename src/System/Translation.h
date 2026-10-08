#pragma once

#include <shared_mutex>

namespace Translation
{
	class Manager final : public REX::Singleton<Manager>
	{
	public:
		static std::string GetGameLanguage();

		// Loads the main "FUCK" translations
		void BuildTranslationMap();

		// Generic loader for plugins (e.g. "FUCK-HUD")
		void LoadCustomTranslation(std::string_view a_name);

		const char* GetTranslation(const char* a_key)
		{
			if (!a_key)
				return "";

			std::shared_lock lock(_lock);
			if (const auto it = _translationMap.find(std::string_view(a_key)); it != _translationMap.end()) {
				return it->second.c_str();
			}

			return a_key;
		}

	private:
		bool ParseFile(const fs::path& a_path);

		StableStringMap<std::string> _translationMap{};
		std::shared_mutex            _lock;
	};
}

#define TRANSLATE(STR)   Translation::Manager::GetSingleton()->GetTranslation(STR)
#define TRANSLATE_S(STR) std::string(Translation::Manager::GetSingleton()->GetTranslation(STR))
