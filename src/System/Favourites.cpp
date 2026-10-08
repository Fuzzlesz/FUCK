#include "Favourites.h"
#include "Settings.h"

namespace FUCK
{
	std::string FavouritesManager::GetPath(const std::string& a_pluginName) const
	{
		return std::format(R"(Data\FUCKs\{}\favourites.json)", a_pluginName);
	}

	void FavouritesManager::LoadFavourites(const std::string& a_pluginName)
	{
		if (_loadedPlugins.contains(a_pluginName))
			return;
		_loadedPlugins.insert(a_pluginName);

		std::string path = GetPath(a_pluginName);
		if (!fs::exists(path)) {
			return;
		}

		std::string buffer;
		if (auto ec = glz::read_file_json(_favourites[a_pluginName], path, buffer)) {
			logger::warn("FUCK: Failed to load favourites from {} ({})", path, glz::format_error(ec, buffer));
		}
	}

	// Only combos that have favourites are written, and a plugin with none left has its file removed
	void FavouritesManager::SaveFavourites(const std::string& a_pluginName) const
	{
		std::string path = GetPath(a_pluginName);

		StringMap<Set<std::string>> toSave;
		if (auto it = _favourites.find(a_pluginName); it != _favourites.end()) {
			for (const auto& [label, edids] : it->second) {
				if (!edids.empty())
					toSave[label] = edids;
			}
		}

		if (toSave.empty()) {
			std::error_code ec;
			fs::remove(path, ec);
			return;
		}

		fs::create_directories(fs::path(path).parent_path());

		std::string buffer;
		if (auto ec = glz::write_file_json(toSave, path, buffer)) {
			logger::warn("FUCK: Failed to save favourites to {} ({})", path, glz::format_error(ec, buffer));
		}
	}

	// Called every frame by each filtered combo, so it only looks, never adding an empty entry
	const Set<std::string>& FavouritesManager::GetFavourites(const std::string& a_pluginName, const std::string& a_comboLabel)
	{
		static const Set<std::string> s_empty;

		LoadFavourites(a_pluginName);

		auto itPlugin = _favourites.find(a_pluginName);
		if (itPlugin == _favourites.end())
			return s_empty;

		auto itCombo = itPlugin->second.find(a_comboLabel);
		return itCombo != itPlugin->second.end() ? itCombo->second : s_empty;
	}

	bool FavouritesManager::IsFavourited(const std::string& a_pluginName, const std::string& a_comboLabel, const std::string& a_edid)
	{
		LoadFavourites(a_pluginName);
		auto itPlugin = _favourites.find(a_pluginName);
		if (itPlugin != _favourites.end()) {
			auto itCombo = itPlugin->second.find(a_comboLabel);
			return itCombo != itPlugin->second.end() && itCombo->second.contains(a_edid);
		}
		return false;
	}

	void FavouritesManager::ToggleFavourite(const std::string& a_pluginName, const std::string& a_comboLabel, const std::string& a_edid)
	{
		LoadFavourites(a_pluginName);
		auto& edids = _favourites[a_pluginName][a_comboLabel];

		if (!edids.emplace(a_edid).second) {
			edids.erase(a_edid);
		}

		// An emptied combo stays in memory, as a caller may still hold the set from GetFavourites.
		// SaveFavourites leaves it out of the file

		SaveFavourites(a_pluginName);
	}
}
