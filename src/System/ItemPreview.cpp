#include "System/ItemPreview.h"

namespace
{
	using Plugins = std::set<std::string, std::less<>>;

	Plugins previewHiders;
	Plugins cardHiders;

	void SetAsk(Plugins& a_plugins, const char* a_plugin, bool a_hidden)
	{
		if (a_hidden) {
			a_plugins.emplace(a_plugin);
		} else if (auto it = a_plugins.find(a_plugin); it != a_plugins.end()) {
			a_plugins.erase(it);
		}
	}

	bool IsItemLoaded()
	{
		auto models = RE::Inventory3DManager::GetSingleton();
		return models && !models->GetRuntimeData().loadedModels.empty();
	}

	// Replaces the menus' item load, so nothing loads while hidden. Installed on first use, leaving the game's own load till then.
	// Thank you myztikrice, from Show Player in Menus
	struct LoadItem
	{
		static void thunk(RE::Inventory3DManager* a_this, RE::InventoryEntryData* a_entry)
		{
			auto object = a_entry ? a_entry->object : nullptr;  // Not GetObject(), which windows.h renames
			if (!object || !previewHiders.empty()) {
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

	// SkyUI's card or the game's, and moreHUD's beside it. Hidden by their visibility, which the menus leave alone, and faded out too, so
	// the next row highlighted fades it back in as it would
	struct Cards
	{
		RE::GFxMovieView* view          = nullptr;
		bool              cardHidden    = false;
		bool              moreHUDHidden = false;

		void Hold(RE::GFxMovieView* a_view)
		{
			if (a_view != view) {
				view          = a_view;
				cardHidden    = false;
				moreHUDHidden = false;
			}

			const bool hide = !cardHiders.empty();
			if (!view || (!hide && ((!cardHidden && !moreHUDHidden) || !IsItemLoaded())))
				return;

			const auto setVisible = [hide](RE::GFxValue& a_clip, bool& a_hidden) {
				RE::GFxValue::DisplayInfo info;
				a_clip.GetDisplayInfo(&info);
				if (hide && info.GetVisible()) {
					info.SetVisible(false);
					a_clip.SetDisplayInfo(info);
					a_hidden = true;
				} else if (!hide && a_hidden) {
					info.SetVisible(true);
					a_clip.SetDisplayInfo(info);
					a_hidden = false;
				}
			};

			RE::GFxValue card;
			RE::GFxValue holder;
			if (view->GetVariable(&card, "_root.Menu_mc.itemCard") && card.IsDisplayObject() && card.GetMember("_parent", &holder) && holder.IsDisplayObject()) {
				setVisible(holder, cardHidden);
			} else if (view->GetVariable(&card, "_root.Menu_mc.ItemCard_mc") && card.IsDisplayObject()) {
				setVisible(card, cardHidden);
			}
			if (hide && card.IsDisplayObject()) {
				card.Invoke("FadeOutCard");
				RE::GFxValue faded{ false };
				view->SetVariable("_root.Menu_mc._bItemCardFadedIn", faded, RE::GFxMovie::SetVarType::kNormal);
			}

			RE::GFxValue moreHUD;
			if (view->GetVariable(&moreHUD, "_root.AHZItemCardContainer") && moreHUD.IsDisplayObject())
				setVisible(moreHUD, moreHUDHidden);
		}
	};
	Cards cards;

	// After the menu's own update, as it fades the card back in as the selection changes
	template <class T>
	struct AdvanceMovie
	{
		static void thunk(T* a_this, float a_interval, std::uint32_t a_currentTime)
		{
			func(a_this, a_interval, a_currentTime);
			cards.Hold(a_this->uiMovie.get());
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline constexpr std::size_t            idx{ 0x05 };
	};

	void InstallCardHooks()
	{
		static bool installed = false;
		if (std::exchange(installed, true))
			return;

		stl::write_vfunc<RE::InventoryMenu, AdvanceMovie<RE::InventoryMenu>>();
		stl::write_vfunc<RE::BarterMenu, AdvanceMovie<RE::BarterMenu>>();
		stl::write_vfunc<RE::ContainerMenu, AdvanceMovie<RE::ContainerMenu>>();
		stl::write_vfunc<RE::GiftMenu, AdvanceMovie<RE::GiftMenu>>();
		logger::info("Installed item card hooks");
	}
}

namespace ItemPreview
{
	void SetHidden(const char* a_plugin, bool a_preview, bool a_card)
	{
		if (!a_plugin)
			return;

		if (a_preview) {
			LoadItem::Install();
			if (previewHiders.empty()) {
				if (auto models = RE::Inventory3DManager::GetSingleton())
					models->UnloadInventoryItem();
			}
		}
		if (a_card)
			InstallCardHooks();

		SetAsk(previewHiders, a_plugin, a_preview);
		SetAsk(cardHiders, a_plugin, a_card);
	}
}
