#pragma once

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

			if (const auto it = translationMap.find(a_key); it != translationMap.end()) {
				return it->second.c_str();
			}

			return a_key;
		}

	private:
		bool ParseFile(const std::filesystem::path& a_path);

		StringMap<std::string> translationMap{};
	};
}

#define TRANSLATE(STR) Translation::Manager::GetSingleton()->GetTranslation(STR)
#define TRANSLATE_S(STR) std::string(Translation::Manager::GetSingleton()->GetTranslation(STR))
