#include "System/ItemPreview.h"

namespace
{
	std::set<std::string, std::less<>> hiders;

	// Replaces the menus' item load, so nothing loads while hidden. Installed on first use, leaving the game's own load till then.
	// Thank you myztikrice, from Show Player in Menus
	struct LoadItem
	{
		static void thunk(RE::Inventory3DManager* a_this, RE::InventoryEntryData* a_entry)
		{
			auto object = a_entry ? a_entry->object : nullptr;  // Not GetObject(), which windows.h renames
			if (!object || !hiders.empty()) {
				a_this->UnloadInventoryItem();
				return;
			}

			auto extra = a_entry->extraLists && !a_entry->extraLists->empty() ? a_entry->extraLists->front() : nullptr;
			a_this->LoadInventoryItem(object, extra);
		}

		static void Install()
		{
			static bool installed = false;
			if (std::exchange(installed, true))
				return;

			REL::Relocation<std::uintptr_t> target{ REL::VariantID(50884, 51757, 50884) };
			SKSE::GetTrampoline().write_branch<5>(target.address(), thunk);
			logger::info("Installed item preview hook");
		}
	};
}

namespace ItemPreview
{
	void SetHidden(const char* a_plugin, bool a_hidden)
	{
		if (!a_plugin)
			return;

		if (!a_hidden) {
			if (auto it = hiders.find(a_plugin); it != hiders.end())
				hiders.erase(it);
			return;
		}

		LoadItem::Install();
		if (hiders.emplace(a_plugin).second && hiders.size() == 1) {
			if (auto models = RE::Inventory3DManager::GetSingleton())
				models->UnloadInventoryItem();
		}
	}
}
