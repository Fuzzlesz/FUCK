#pragma once
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace Utils
{
	inline bool IContains(std::string_view haystack, std::string_view needle)
	{
		if (needle.empty())
			return true;
		auto it = std::search(
			haystack.begin(), haystack.end(),
			needle.begin(), needle.end(),
			[](char ch1, char ch2) {
				return std::toupper(static_cast<unsigned char>(ch1)) ==
			           std::toupper(static_cast<unsigned char>(ch2));
			});
		return it != haystack.end();
	}

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
}
