#pragma once

#include "FUCK-Man.h"

namespace FUCK
{
	class FavouritesManager : public REX::Singleton<FavouritesManager>
	{
	public:
		void LoadFavourites(const std::string& a_pluginName);
		void SaveFavourites(const std::string& a_pluginName) const;

		const Set<std::string>& GetFavourites(const std::string& a_pluginName, const std::string& a_comboLabel);
		bool                    IsFavourited(const std::string& a_pluginName, const std::string& a_comboLabel, const std::string& a_edid);
		void                    ToggleFavourite(const std::string& a_pluginName, const std::string& a_comboLabel, const std::string& a_edid);

	private:
		std::string GetPath(const std::string& a_pluginName) const;

		// Map of Plugin Name -> Combo Label -> Set of EditorIDs
		StringMap<StringMap<Set<std::string>>> _favourites;
		Set<std::string>                       _loadedPlugins;
	};
}
