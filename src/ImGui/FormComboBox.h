#pragma once

#include "FUCK-Man.h"
#include "Widgets.h"

#include "System/Favourites.h"
#include "System/Hooks.h"

namespace ImGui
{
	constexpr auto allMods  = "$FUCK_ALL"sv;
	constexpr auto favForms = "$FUCK_Favourites"sv;
	constexpr auto ffForms  = "$FUCK_FF_Forms"sv;

	template <class T>
	class FormComboBox
	{
	public:
		T* GetForm(const std::string& a_edid) const
		{
			const auto it = edidForms.find(a_edid);
			return it != edidForms.end() ? it->second : nullptr;
		}

		void AddForm(const std::string& a_edid, T* a_form)
		{
			if (edidForms.emplace(a_edid, a_form).second) {
				edids.push_back(a_edid);
			}
		}

		void RemoveForm(const std::string& a_edid)
		{
			if (edidForms.erase(a_edid)) {
				if (const auto it = std::ranges::find(edids, a_edid); it != edids.end()) {
					edids.erase(it);
				}
				ClampIndex();
			}
		}

		void SortForms()
		{
			std::ranges::sort(edids, [](const std::string& a, const std::string& b) {
				return _stricmp(a.c_str(), b.c_str()) < 0;
			});
		}

		void UpdateValidForms(RE::Actor* a_actor = nullptr)
		{
			if (valid) {
				return;
			}

			SetValid(true);

			if constexpr (std::is_same_v<T, RE::TESIdleForm>) {
				if (!a_actor) {
					a_actor = RE::PlayerCharacter::GetSingleton();
				}
				edids.clear();
				for (auto& [edid, idle] : edidForms) {
					if (a_actor->CanUseIdle(idle) && idle->CheckConditions(a_actor, nullptr, false)) {
						edids.push_back(edid);
					}
				}
				SortForms();
				ClampIndex();
			}
		}

		std::int32_t GetIndex() const
		{
			return index;
		}

		void SetIndex(std::int32_t a_index)
		{
			index = a_index;
			ClampIndex();
		}

		void ResetIndex()
		{
			index = 0;

			if constexpr (std::is_same_v<T, RE::TESWeather>) {
				if (auto currentWeather = RE::Sky::GetSingleton()->currentWeather) {
					if (const auto it = std::ranges::find(edids, clib_util::editorID::get_editorID(currentWeather)); it != edids.end()) {
						index = static_cast<std::int32_t>(std::distance(edids.begin(), it));
					}
				}
			}
		}

		void ClampIndex()
		{
			index = edids.empty() ? 0 : std::clamp(index, 0, static_cast<std::int32_t>(edids.size()) - 1);
		}

		void SetValid(bool a_valid)
		{
			valid = a_valid;
		}

		bool Sync(RE::FormID a_formID)
		{
			for (size_t i = 0; i < edids.size(); ++i) {
				if (edidForms[edids[i]]->GetFormID() == a_formID) {
					index = static_cast<std::int32_t>(i);
					return true;
				}
			}
			return false;
		}

		// OVERLOAD: Sync by EditorID string (Case-Insensitive)
		bool Sync(const std::string& a_edid)
		{
			for (size_t i = 0; i < edids.size(); ++i) {
				if (_stricmp(edids[i].c_str(), a_edid.c_str()) == 0) {
					index = static_cast<std::int32_t>(i);
					return true;
				}
			}
			return false;
		}

		T* GetComboWithFilterResult(RE::Actor* a_actor = nullptr, const Set<std::string>* a_favourites = nullptr, std::string* a_favToggled = nullptr)
		{
			UpdateValidForms(a_actor);
			if (ComboWithFilter("##forms", &index, edids, -1, a_favourites, a_favToggled)) {
				// avoid losing focus
				SetKeyboardFocusHere(-1);
				return edidForms.find(edids[index])->second;
			}
			return nullptr;
		}

	private:
		// members
		StringMap<T*>            edidForms{};
		std::vector<std::string> edids{};
		std::int32_t             index{ 0 };
		bool                     valid{ false };
	};

	// modName, forms
	template <class T>
	class FormComboBoxFiltered
	{
	public:
		FormComboBoxFiltered(std::string a_name) :
			name(a_name),
			rawName(std::move(a_name))
		{}

		void AddForm(const std::string& a_edid, T* a_form)
		{
			std::string modName;
			if (auto file = a_form->GetFile(0)) {
				modName = file->fileName;
			} else {
				modName = ffForms;
			}

			modNameForms[std::string(allMods)].AddForm(a_edid, a_form);
			modNameForms[modName].AddForm(a_edid, a_form);
		}

		void InitForms(RE::FormType a_type = RE::FormType::None)
		{
			if (!modNameForms.empty())
				return;

			auto dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler)
				return;

			const auto& formArray = dataHandler->GetFormArray(a_type);
			for (auto* form : formArray) {
				if (auto* typedForm = form->As<T>()) {
					std::string edid = editorID::get_editorID(form);
					if (edid.empty())
						edid = std::format("0x{:X}", form->GetFormID());
					AddForm(edid, typedForm);
				}
			}

			// Populate Favourites
			auto&       favModForms = modNameForms[std::string(favForms)];
			const auto& allModForms = modNameForms[std::string(allMods)];
			auto        favManager  = FUCK::FavouritesManager::GetSingleton();

			std::string pluginName = FUCKMan::GetSingleton()->GetCurrentRenderingPlugin();
			if (pluginName.empty())
				pluginName = "FUCK";

			for (const auto& edid : favManager->GetFavourites(pluginName, rawName)) {
				if (const auto form = allModForms.GetForm(edid)) {
					favModForms.AddForm(edid, form);
				}
			}

			for (auto& [modName, formData] : modNameForms) {
				formData.SortForms();
			}

			if (modNames.empty()) {
				modNames.reserve(modNameForms.size());
				modNames.emplace_back(TRANSLATE_S(allMods.data()));
				modNames.emplace_back(TRANSLATE_S(favForms.data()));

				for (const auto& file : dataHandler->files) {
					if (modNameForms.contains(file->fileName)) {
						modNames.emplace_back(file->fileName);
					}
				}
				if (modNameForms.contains(ffForms)) {
					containsFF = true;
					modNames.emplace_back(TRANSLATE_S(ffForms.data()));
				}
			}

			Reset();
		}

		void Reset()
		{
			curMod = std::string(allMods);
			index  = 0;
			for (auto& [modName, formData] : modNameForms) {
				formData.ResetIndex();
				formData.SetValid(false);
			}
		}

		void Sync(RE::FormID a_formID)
		{
			if (a_formID == 0)
				return;

			// Check the currently active mod list first (fast path)
			if (modNameForms[curMod].Sync(a_formID))
				return;

			// Otherwise, search all mod lists
			for (size_t i = 0; i < modNames.size(); ++i) {
				const auto& mod = modNames[i];
				if (modNameForms[mod].Sync(a_formID)) {
					index  = static_cast<std::int32_t>(i);
					curMod = mod;
					return;
				}
			}
		}

		// OVERLOAD: Sync by EditorID string (Case-Insensitive)
		void Sync(const std::string& a_edid)
		{
			if (a_edid.empty())
				return;

			// Check the currently active mod list first
			if (modNameForms[curMod].Sync(a_edid))
				return;

			// Otherwise, search all mod lists
			for (size_t i = 0; i < modNames.size(); ++i) {
				const auto& mod = modNames[i];
				if (modNameForms[mod].Sync(a_edid)) {
					index  = static_cast<std::int32_t>(i);
					curMod = mod;
					return;
				}
			}
		}

		void GetFormResultFromCombo(std::function<void(T*)> a_func, RE::Actor* a_actor = nullptr)
		{
			T* formResult = nullptr;
			if (!translated) {
				name       = TRANSLATE_S(name.c_str());
				translated = true;
			}

			BeginGroup();
			{
				SeparatorText(name.c_str());
				PushStyleColor(ImGuiCol_NavCursor, GetUserStyleColorVec4(USER_STYLE::kComboBoxText));
				PushID(name.c_str());
				PushMultiItemsWidths(2, GetContentRegionAvail().x);

				if (ComboWithFilter("##mods", &index, modNames)) {
					if (index == 0) {
						curMod = std::string(allMods);
					} else if (index == 1) {
						curMod = std::string(favForms);
					} else if (containsFF && index == modNames.size() - 1) {
						curMod = std::string(ffForms);
					} else {
						curMod = modNames[index];
					}
				}

				PopItemWidth();
				SameLine(0, GetStyle().ItemInnerSpacing.x);

				std::string favToggled;
				auto        favManager = FUCK::FavouritesManager::GetSingleton();

				std::string pluginName = FUCKMan::GetSingleton()->GetCurrentRenderingPlugin();
				if (pluginName.empty())
					pluginName = "FUCK";

				formResult = modNameForms[curMod].GetComboWithFilterResult(a_actor, &favManager->GetFavourites(pluginName, rawName), &favToggled);

				if (!favToggled.empty()) {
					ToggleFavourite(pluginName, favToggled);
				}

				PopItemWidth();
				PopID();
				PopStyleColor(1);
			}
			EndGroup();

			if (formResult) {
				a_func(formResult);
			}
		}

	private:
		void ToggleFavourite(const std::string& a_pluginName, const std::string& a_edid)
		{
			FUCK::FavouritesManager::GetSingleton()->ToggleFavourite(a_pluginName, rawName, a_edid);

			auto& favModForms = modNameForms[std::string(favForms)];
			if (favModForms.GetForm(a_edid)) {
				favModForms.RemoveForm(a_edid);
			} else if (const auto form = modNameForms[std::string(allMods)].GetForm(a_edid)) {
				favModForms.AddForm(a_edid, form);
				favModForms.SortForms();
				favModForms.SetValid(false);
			}
		}

		// members
		std::string name;
		std::string rawName;
		bool        translated{ false };

		StringMap<FormComboBox<T>> modNameForms{};
		std::vector<std::string>   modNames{};
		std::int32_t               index{ 0 };
		bool                       containsFF{ false };

		std::string curMod{ std::string(allMods) };
	};

	struct FormComboBoxState
	{
		template <class T>
		void SaveState(const FormComboBoxFiltered<T>& a_formCombo)
		{
			modIndex = a_formCombo.index;
			curMod = a_formCombo.curMod;
			for (const auto& [modName, formData] : a_formCombo.modNameForms) {
				modFormIndices.insert_or_assign(modName, formData.GetIndex());
			}
		}

		template <class T>
		void RestoreState(FormComboBoxFiltered<T>& a_formCombo)
		{
			a_formCombo.index = modIndex;
			a_formCombo.curMod = curMod;
			for (auto& [modName, formData] : a_formCombo.modNameForms) {
				if (const auto it = modFormIndices.find(modName); it != modFormIndices.end()) {
					formData.SetIndex(it->second);
				} else {
					formData.SetIndex(0);
				}
				formData.SetValid(false);
			}
		}

		// members
		std::int32_t            modIndex{ 0 };
		std::string             curMod{ std::string(allMods) };
		StringMap<std::int32_t> modFormIndices{};
	};
}
