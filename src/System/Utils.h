#pragma once

namespace Utils
{
	inline void SanitizePath(char* dest, const char* source, std::size_t destSize)
	{
		if (!dest || !source || destSize == 0)
			return;

		std::string path(source);

		path = clib_util::string::tolower(path);

		path = srell::regex_replace(path, srell::regex("/+|\\\\+"), "\\");
		path = srell::regex_replace(path, srell::regex("^\\\\+"), "");
		path = srell::regex_replace(path, srell::regex(R"(.*?[^\s]textures\\|^textures\\|.*?[^\s]meshes\\|^meshes\\)", srell::regex::icase), "");

		strncpy_s(dest, destSize, path.c_str(), _TRUNCATE);
	}

	inline std::vector<std::string> GetDirectoryFiles(const std::filesystem::path& a_path, const std::string& a_ext = ".ini", bool a_recursive = false)
	{
		std::vector<std::string> files;

		if (!std::filesystem::exists(a_path)) {
			try {
				std::filesystem::create_directories(a_path);
			} catch (...) {
				return files;
			}
		}

		auto options = std::filesystem::directory_options::skip_permission_denied;

		auto process_entry = [&](const std::filesystem::directory_entry& entry) {
			if (entry.is_regular_file()) {
				auto path = entry.path();
				if (a_ext.empty() || path.extension().string() == a_ext) {
					files.push_back(path.filename().string());
				}
			}
		};

		if (a_recursive) {
			for (const auto& entry : std::filesystem::recursive_directory_iterator(a_path, options)) {
				process_entry(entry);
			}
		} else {
			for (const auto& entry : std::filesystem::directory_iterator(a_path, options)) {
				process_entry(entry);
			}
		}

		std::sort(files.begin(), files.end());
		// Erase potential duplicates
		files.erase(std::unique(files.begin(), files.end()), files.end());

		return files;
	}

	inline std::optional<std::string> FindFileRecursive(const std::filesystem::path& a_dir, const std::string& a_file)
	{
		if (!std::filesystem::exists(a_dir))
			return std::nullopt;
		auto options = std::filesystem::directory_options::skip_permission_denied;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(a_dir, options)) {
			if (entry.is_regular_file() && entry.path().filename().string() == a_file) {
				return entry.path().string();
			}
		}
		return std::nullopt;
	}
}
