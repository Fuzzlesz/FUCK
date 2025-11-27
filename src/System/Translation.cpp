#include "Translation.h"

namespace Translation
{
	std::string Manager::GetGameLanguage()
	{
		const auto setting = RE::GetINISetting("sLanguage:General");
		return (setting && setting->GetType() == RE::Setting::Type::kString) ? clib_util::string::toupper(setting->GetString()) : "ENGLISH"s;
	}

	void Manager::BuildTranslationMap()
	{
		LoadCustomTranslation("FUCK");
	}

	void Manager::LoadCustomTranslation(std::string_view a_name)
	{
		// Construct path: Data\Interface\Translations\{NAME}_{LANG}.txt
		std::filesystem::path path{ std::format(R"(Data\Interface\Translations\{}_{}.txt)", a_name, GetGameLanguage()) };

		if (!ParseFile(path)) {
			// Fallback to ENGLISH if specific language fails
			std::filesystem::path fallback{ std::format(R"(Data\Interface\Translations\{}_ENGLISH.txt)", a_name) };
			if (std::filesystem::exists(fallback)) {
				logger::info("Failed to load translation file in {}, loading default ENGLISH file...", path.string());
				ParseFile(fallback);
			} else {
				// If neither exists, that's okay, maybe the user wants to use shared strings
				logger::warn("No translation file found for {} (Expected: {})", a_name, path.string());
			}
		}
	}

	bool Manager::ParseFile(const std::filesystem::path& a_path)
	{
		if (!std::filesystem::exists(a_path)) {
			return false;
		}

		// Read file as binary
		std::ifstream file(a_path, std::ios::binary | std::ios::ate);
		if (!file.good()) {
			return false;
		}

		logger::info("Reading translations from {}...", a_path.string());

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		if (size < 2)
			return false;  // Too small for BOM

		std::vector<char> buffer(size);
		if (!file.read(buffer.data(), size))
			return false;

		// Check BOM (UTF-16 LE = FF FE)
		if (static_cast<unsigned char>(buffer[0]) != 0xFF || static_cast<unsigned char>(buffer[1]) != 0xFE) {
			logger::error("\tBOM Error in {}, file must be encoded in UCS-2 LE / UTF-16 LE.", a_path.string());
			return false;
		}

		// Skip BOM
		const wchar_t* wData = reinterpret_cast<const wchar_t*>(buffer.data() + 2);
		int wLength = static_cast<int>((size - 2) / sizeof(wchar_t));

		if (wLength <= 0)
			return true;  // Empty file

		// Convert to UTF-8
		int utf8Length = ::WideCharToMultiByte(CP_UTF8, 0, wData, wLength, nullptr, 0, nullptr, nullptr);
		if (utf8Length <= 0)
			return false;

		std::string utf8Content(utf8Length, '\0');
		::WideCharToMultiByte(CP_UTF8, 0, wData, wLength, &utf8Content[0], utf8Length, nullptr, nullptr);

		// Parse Line by Line
		std::stringstream ss(utf8Content);
		std::string line;
		size_t count = 0;

		while (std::getline(ss, line)) {
			// Remove CR if present (Windows line ending)
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}

			if (line.empty())
				continue;

			std::stringstream ls(line);
			std::string key, value;

			ls >> key;

			if (key.empty())
				continue;

			// Get remainder as value, trimming leading whitespace
			std::getline(ls >> std::ws, value);

			if (!key.empty() && !value.empty()) {
				translationMap.emplace(key, value);
				count++;
			}
		}

		logger::info("\tLoaded {} entries.", count);
		return true;
	}
}
