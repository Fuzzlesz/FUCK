#include "FUCK-Man.h"

#include "Input.h"
#include "Journal.h"

namespace Hooks::Journal
{
	constexpr const char* kSystemPagePath = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc";

	// --- The Entry ---
	// The System page's lists and the entry FUCK adds to them
	static std::string s_injectedMenuName;

	// The main list, or the Settings list with a_injectSettings, wherever the page keeps it
	static bool GetTargetList(RE::GFxValue& a_page, bool a_injectSettings, RE::GFxValue& a_listObj, RE::GFxValue& a_entryList)
	{
		const char* targetList = a_injectSettings ? "SettingsList" : "CategoryList";
		if (a_page.GetMember(targetList, &a_listObj) && a_listObj.IsObject() &&
			a_listObj.GetMember("entryList", &a_entryList) && a_entryList.IsArray()) {
			return true;
		}

		// Fallback for wrapped lists
		const char*  fallbackTarget = a_injectSettings ? "SettingsList_mc" : "CategoryList_mc";
		RE::GFxValue wrapper;
		if (a_page.GetMember(fallbackTarget, &wrapper) && wrapper.IsObject() &&
			wrapper.GetMember("List_mc", &a_listObj) && a_listObj.IsObject() &&
			a_listObj.GetMember("entryList", &a_entryList) && a_entryList.IsArray()) {
			return true;
		}

		return false;
	}

	// The name the entry was added under, which stays the same while the Journal is open even if the setting changes
	static const char* InjectedName()
	{
		return s_injectedMenuName.empty() ? FUCK::Translate(FUCKMan::GetSingleton()->GetSystemMenuName()) : s_injectedMenuName.c_str();
	}

	// Where the entry with the given text sits in the list, or -1 where it isn't there
	static std::int32_t FindEntry(RE::GFxValue& a_entryList, std::string_view a_text)
	{
		const std::uint32_t size = a_entryList.GetArraySize();
		for (std::uint32_t i = 0; i < size; ++i) {
			RE::GFxValue element, textVal;
			if (a_entryList.GetElement(i, &element) && element.IsObject() && element.GetMember("text", &textVal) && textVal.IsString() &&
				a_text == textVal.GetString())
				return static_cast<std::int32_t>(i);
		}
		return -1;
	}

	// Inserts an entry at the index, or removes the one there when given none, with the array's own splice so the rest keep their order
	static void SpliceEntry(RE::GFxValue& a_entryList, std::uint32_t a_index, const RE::GFxValue* a_insert)
	{
		if (a_insert) {
			const RE::GFxValue args[3] = { RE::GFxValue(static_cast<double>(a_index)), RE::GFxValue(0.0), *a_insert };
			a_entryList.Invoke("splice", nullptr, args, 3);
		} else {
			const RE::GFxValue args[2] = { RE::GFxValue(static_cast<double>(a_index)), RE::GFxValue(1.0) };
			a_entryList.Invoke("splice", nullptr, args, 2);
		}
	}

	// Where the entry sits when it was inserted into the main list, moving the buttons below it, or -1 where it isn't there or took
	// $HELP's place
	static std::int32_t FindInsertedEntry(RE::GFxValue& a_entryList)
	{
		const std::int32_t index = FindEntry(a_entryList, InjectedName());
		RE::GFxValue       entry;
		return index >= 0 && a_entryList.GetElement(static_cast<std::uint32_t>(index), &entry) && entry.HasMember("fuck_inserted") ? index : -1;
	}

	// Where the given entry object sits in the list, or -1. Found by marking it and looking for the mark, as two values can't be compared
	// as the same object
	static std::int32_t FindEntryObject(RE::GFxValue& a_entryList, RE::GFxValue& a_entry)
	{
		if (!a_entry.IsObject())
			return -1;

		std::int32_t found = -1;
		a_entry.SetMember("fuck_probe", RE::GFxValue(true));
		const std::uint32_t size = a_entryList.GetArraySize();
		for (std::uint32_t i = 0; i < size && found < 0; ++i) {
			RE::GFxValue element;
			if (a_entryList.GetElement(i, &element) && element.IsObject() && element.HasMember("fuck_probe"))
				found = static_cast<std::int32_t>(i);
		}
		a_entry.DeleteMember("fuck_probe");
		return found;
	}

	static void SetInjectedName(const char* a_name)
	{
		s_injectedMenuName = a_name ? a_name : "";
	}

	// --- Row Interaction Handlers ---
	class EntryPressHandler : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (!a_params.thisPtr)
				return;
			RE::GFxValue itemIndex, parent;
			if (!a_params.thisPtr->GetMember("itemIndex", &itemIndex) || itemIndex.IsUndefined())
				return;
			if (!a_params.thisPtr->GetMember("_parent", &parent))
				return;

			RE::GFxValue kbOrMouse{ 0.0 };
			if (a_params.argCount >= 2 && a_params.args)
				kbOrMouse = a_params.args[1];

			parent.Invoke("onItemPress", nullptr, &kbOrMouse, 1);
		}
	};
	static EntryPressHandler g_entryPressHandler;

	class EntryRollOverHandler : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (!a_params.thisPtr)
				return;
			RE::GFxValue itemIndex, parent;
			if (!a_params.thisPtr->GetMember("itemIndex", &itemIndex) || itemIndex.IsUndefined())
				return;
			if (!a_params.thisPtr->GetMember("_parent", &parent))
				return;

			RE::GFxValue anim, dis;
			const bool   animating = parent.GetMember("listAnimating", &anim) && anim.IsBool() && anim.GetBool();
			const bool   disabled  = parent.GetMember("bDisableInput", &dis) && dis.IsBool() && dis.GetBool();
			if (animating || disabled)
				return;

			const RE::GFxValue args2[2] = { itemIndex, RE::GFxValue(0.0) };
			parent.Invoke("doSetSelectedIndex", nullptr, args2, 2);
			parent.SetMember("bMouseDrivenNav", RE::GFxValue(true));
		}
	};
	static EntryRollOverHandler g_entryRollOverHandler;

	// Opens FUCK once the Journal has closed. Opening while it's still fading out leaves it on the stack,
	// where FUCK pauses it mid-close and it keeps the game's input
	static void OpenWhenJournalCloses()
	{
		static bool s_waiting = false;
		if (s_waiting)
			return;
		s_waiting = true;

		FUCKMan::GetSingleton()->AddMenuListener(&s_waiting, [](const char* a_menu, bool a_opening, void* a_userdata) {
			if (a_opening || RE::JournalMenu::MENU_NAME != a_menu)
				return;

			FUCKMan::GetSingleton()->RemoveMenuListener(a_userdata);
			*static_cast<bool*>(a_userdata) = false;
			SKSE::GetTaskInterface()->AddTask([]() { FUCKMan::GetSingleton()->Open(); });
		});
	}

	// Whether the press is on the entry, which closes the Journal and opens FUCK
	static bool OpenIfEntryPressed(RE::GFxFunctionHandler::Params& a_params)
	{
		RE::GFxValue entry, textVal;
		if (a_params.argCount < 1 || !a_params.args || !a_params.args[0].GetMember("entry", &entry) || !entry.GetMember("text", &textVal) ||
			!textVal.IsString() || std::string_view(textVal.GetString()) != InjectedName())
			return false;

		// Close the Journal menu natively so the game state clears
		if (auto queue = RE::UIMessageQueue::GetSingleton())
			queue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
		OpenWhenJournalCloses();
		return true;
	}

	// --- Duplicates the visual MovieClips and expands the mask boundary
	static void UpdateListVisuals(RE::GFxMovieView* a_movieView, RE::GFxValue& a_listObj, std::uint32_t a_numItems, bool a_updateHeight)
	{
		RE::GFxValue maxShownV;
		if (a_listObj.GetMember("iMaxItemsShown", &maxShownV) && maxShownV.IsNumber()) {
			std::uint32_t curClips = static_cast<std::uint32_t>(maxShownV.GetNumber());

			double       entryH = 0.0;
			RE::GFxValue e0, h;
			if (a_listObj.GetMember("Entry0", &e0) && e0.GetMember("_height", &h) && h.IsNumber()) {
				entryH = h.GetNumber();
			}

			if (curClips > 0 && a_numItems > curClips) {
				for (std::uint32_t i = curClips; i < a_numItems; ++i) {
					const std::string srcName = "Entry" + std::to_string(i - 1);
					RE::GFxValue      src;
					if (!a_listObj.GetMember(srcName.c_str(), &src))
						break;

					const std::string  newName    = "Entry" + std::to_string(i);
					const RE::GFxValue dupArgs[2] = { RE::GFxValue(newName.c_str()), RE::GFxValue(static_cast<double>(20000 + i)) };
					src.Invoke("duplicateMovieClip", nullptr, dupArgs, 2);

					RE::GFxValue nc;
					if (a_listObj.GetMember(newName.c_str(), &nc)) {
						nc.SetMember("clipIndex", RE::GFxValue(static_cast<double>(i)));

						// Attach interactivity handlers to new rows
						RE::GFxValue fnPress, fnRoll;
						a_movieView->CreateFunction(&fnPress, &g_entryPressHandler);
						a_movieView->CreateFunction(&fnRoll, &g_entryRollOverHandler);
						nc.SetMember("onPress", fnPress);
						nc.SetMember("onRollOver", fnRoll);
					}
				}
				a_listObj.SetMember("iMaxItemsShown", RE::GFxValue(static_cast<double>(a_numItems)));
			}

			// Expand the physical clipping mask for the Settings List
			if (a_updateHeight && entryH > 0.0) {
				RE::GFxValue border;
				if (a_listObj.GetMember("border", &border)) {
					border.SetMember("_height", RE::GFxValue(entryH * static_cast<double>(a_numItems) + 4.0));
				}
			}
		}
	}

	// --- Compatibility ---
	// Keeping the entry in step with SkyUI 5's System page, which acts on its buttons by their places in the list, and with other mods
	// that change the page's lists
	static bool s_settingsListTaken = false;  // The entry listens to the Settings list itself, while the Journal is open

	// Mods that rebuild the list, as Native System Menu Framework and STB System Page do, stamp each entry with its place when they find
	// it and hand the page that place on a press. Native System Menu Framework also keeps the list as it found it, and shows a copy
	constexpr std::array<const char*, 2> kPlaceStamps{ "__nsmf_vidx", "__stb_vidx" };
	constexpr const char*                kKeptListMember = "__nsmf_vanillaEntries";

	// The list a stamp's places were counted in
	static RE::GFxValue GetStampedList(RE::GFxValue& a_listObj, RE::GFxValue& a_entryList, const char* a_stamp)
	{
		RE::GFxValue kept;
		if (std::string_view(a_stamp) == kPlaceStamps[0] && a_listObj.GetMember(kKeptListMember, &kept) && kept.IsArray())
			return kept;
		return a_entryList;
	}

	// On the older page, where the entry was in the list when a mod stamped it, the stamps below it count it. They're lowered by one and
	// its own stamp taken away, so every stamp is the page's own place
	static void CorrectPlaceStamps(RE::GFxValue& a_listObj, RE::GFxValue& a_entryList)
	{
		const std::int32_t ours = FindInsertedEntry(a_entryList);
		RE::GFxValue       entry;
		if (ours < 0 || !a_entryList.GetElement(static_cast<std::uint32_t>(ours), &entry))
			return;

		for (const char* stamp : kPlaceStamps) {
			RE::GFxValue oursAt;
			if (!entry.GetMember(stamp, &oursAt) || !oursAt.IsNumber())
				continue;
			entry.DeleteMember(stamp);

			RE::GFxValue        stamped = GetStampedList(a_listObj, a_entryList, stamp);
			const std::uint32_t size    = stamped.GetArraySize();
			for (std::uint32_t i = 0; i < size; ++i) {
				RE::GFxValue element, place;
				if (stamped.GetElement(i, &element) && element.IsObject() && element.GetMember(stamp, &place) && place.IsNumber() &&
					place.GetNumber() > oursAt.GetNumber())
					element.SetMember(stamp, RE::GFxValue(place.GetNumber() - 1.0));
			}
		}
	}

	// Where Native System Menu Framework shows a copy of the list, it hands the page places in the list it kept, so they're worked out
	// in that one
	static void RefreshIndices(RE::GFxValue& a_page, RE::GFxValue& a_listObj)
	{
		RE::GFxValue kept, shown;
		const bool   swap = a_listObj.GetMember(kKeptListMember, &kept) && kept.IsArray() && a_listObj.GetMember("entryList", &shown);
		if (swap)
			a_listObj.SetMember("entryList", kept);
		a_page.Invoke("UpdateIndices", nullptr, nullptr, 0);
		if (swap)
			a_listObj.SetMember("entryList", shown);
	}

	// A press on the main list, passed on with the place the page expects for the button pressed
	static void AdjustMainPress(RE::GFxValue& a_page, RE::GFxValue& a_event)
	{
		RE::GFxValue listObj, entryList, entry, index;
		if (!GetTargetList(a_page, false, listObj, entryList))
			return;
		a_event.GetMember("entry", &entry);
		if (!a_event.GetMember("index", &index) || !index.IsNumber())
			return;

		if (a_page.HasMember("UpdateIndices")) {
			// The newer page matches the index against places it worked out, which another mod may have worked out again since, so
			// they're worked out once more now
			RefreshIndices(a_page, listObj);

			// Those are where the buttons sit now, so the index is too. A mod written for the older page may have lowered it for its own
			// entry above (Bestiary UI), which would then miss by one. Not where Native System Menu Framework shows a copy, whose places
			// are in the list it kept
			RE::GFxValue kept;
			if (!listObj.GetMember(kKeptListMember, &kept) || !kept.IsArray()) {
				if (const std::int32_t clicked = entry.IsObject() ? FindEntryObject(entryList, entry) : -1; clicked >= 0)
					a_event.SetMember("index", RE::GFxValue(static_cast<double>(clicked)));
			}
			return;
		}

		// The older page acts on a button by its place in the list. A button another mod stamped is passed on with its stamp, corrected
		// for the entry, which is what that mod passes on too, so either can run first
		CorrectPlaceStamps(listObj, entryList);
		for (const char* stamp : kPlaceStamps) {
			RE::GFxValue place;
			if (entry.IsObject() && entry.GetMember(stamp, &place) && place.IsNumber()) {
				a_event.SetMember("index", place);
				return;
			}
		}

		// Otherwise a button below the entry is passed on with one place fewer. Whether it's below is judged by where the button sits
		// rather than by the index, which another mod's entry may already have lowered the same way, so each mod takes off only its own
		// place, in whichever order they're called
		const std::int32_t ours    = FindInsertedEntry(entryList);
		const std::int32_t clicked = ours >= 0 ? FindEntryObject(entryList, entry) : -1;
		if (ours >= 0 && clicked > ours)
			a_event.SetMember("index", RE::GFxValue(index.GetNumber() - 1.0));
	}

	// The older System page hands the game its buttons by their places in the list, for it to disable Save and the like when they can't
	// be used, from both RefreshSystemButtons and UpdatePermissions. So the entry is taken out while either runs, leaving the list as it
	// was before it went in. Only this entry: the game reads every place it's handed, and taking out others, whose own buttons may be
	// missing or renamed, can leave a place past the end
	class ButtonPlacesHandler : public RE::GFxFunctionHandler
	{
	public:
		ButtonPlacesHandler(const char* a_function, const char* a_original) :
			function(a_function), original(a_original) {}

		void Call(Params& a_params) override
		{
			if (!a_params.thisPtr)
				return;

			RE::GFxValue       listObj, entryList, entry;
			const std::int32_t ours = GetTargetList(*a_params.thisPtr, false, listObj, entryList) ? FindInsertedEntry(entryList) : -1;
			if (ours >= 0 && entryList.GetElement(static_cast<std::uint32_t>(ours), &entry))
				SpliceEntry(entryList, static_cast<std::uint32_t>(ours), nullptr);

			a_params.thisPtr->Invoke(original, a_params.retVal, a_params.args, a_params.argCount);

			if (ours >= 0 && entry.IsObject()) {
				SpliceEntry(entryList, static_cast<std::uint32_t>(ours), &entry);
				listObj.Invoke("InvalidateData", nullptr, nullptr, 0);
			}
		}

		const char* function;  // The page's function
		const char* original;  // Where the page's own is kept
	};
	static ButtonPlacesHandler g_buttonPlacesHandlers[] = {
		{ "RefreshSystemButtons", "fuck_orig_RefreshSystemButtons" },
		{ "UpdatePermissions", "fuck_orig_UpdatePermissions" }
	};

	// The game greys out Save and the like on the buttons the System page hands it, and crashes on one that isn't there, as when another
	// mod has hidden an entry and the page reads past the end of its list. So an empty object stands in for any that's missing, on either
	// version of the page and whichever script hands them over
	static RE::FxDelegateHandler::CallbackFn* s_setSaveDisabled = nullptr;

	static void SetSaveDisabled(const RE::FxDelegateArgs& a_params)
	{
		// Should another mod's hook lead back here, the call stops rather than going round again
		static bool running = false;
		if (running || !s_setSaveDisabled)
			return;
		running = true;

		const auto count    = a_params.GetArgCount();
		bool       complete = true;
		for (std::uint32_t i = 0; i < count && complete; ++i)
			complete = !a_params[i].IsUndefined() && !a_params[i].IsNull();

		if (complete) {
			s_setSaveDisabled(a_params);
		} else if (auto movie = a_params.GetMovie()) {
			std::vector<RE::GFxValue> args(count);
			for (std::uint32_t i = 0; i < count; ++i) {
				args[i] = a_params[i];
				if (args[i].IsUndefined() || args[i].IsNull())
					movie->CreateObject(&args[i]);
			}
			const RE::FxDelegateArgs filled(RE::GFxValue(), a_params.GetHandler(), movie, args.data(), count);
			s_setSaveDisabled(filled);
		}
		running = false;
	}

	// Each time the Journal opens, as it's made afresh with the game's own callback
	static void GuardSaveButtons(RE::FxDelegate* a_delegate)
	{
		auto defn = a_delegate ? a_delegate->callbacks.Get(RE::GString("SetSaveDisabled")) : nullptr;
		if (defn && defn->callback && defn->callback != &SetSaveDisabled) {
			s_setSaveDisabled = defn->callback;
			defn->callback    = &SetSaveDisabled;
		}
	}

	// The older page's two functions that hand the game its buttons by place, wrapped to take the entry out while they run
	static void HookButtonPlaces(RE::GFxMovieView* a_movieView, RE::GFxValue& a_page)
	{
		for (auto& handler : g_buttonPlacesHandlers) {
			RE::GFxValue original;
			if (a_page.GetMember(handler.function, &original) && !original.IsUndefined()) {
				a_page.SetMember(handler.original, original);
				RE::GFxValue hook;
				a_movieView->CreateFunction(&hook, &handler);
				a_page.SetMember(handler.function, hook);
			}
		}
	}

	static void ResetCompat()
	{
		s_settingsListTaken = false;
	}

	// Presses on the Settings list, for when another mod has replaced the page's own listener there with one of its own
	class SettingsListPressHandler : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override { OpenIfEntryPressed(a_params); }
	};
	static SettingsListPressHandler g_settingsListPressHandler;

	// The Settings list holds the game's three tabs, so a fourth that isn't the entry is another mod's. Community Shaders finds its tab
	// by its place, the fourth, so the entry is kept after any such tab rather than taking it. That mod also replaces the page's listener
	// on the list with its own, which ignores the entry, so the entry listens for itself
	constexpr std::uint32_t kGameSettingsTabs = 3;

	static void KeepEntryAfterOtherTabs(RE::GFxMovieView* a_movieView, RE::GFxValue& a_listObj, RE::GFxValue& a_entryList)
	{
		const std::int32_t ours = FindEntry(a_entryList, InjectedName());
		const std::uint32_t size = a_entryList.GetArraySize();
		if (ours < 0 || size - 1 <= kGameSettingsTabs)
			return;

		if (static_cast<std::uint32_t>(ours) != size - 1) {
			RE::GFxValue entry;
			a_entryList.GetElement(static_cast<std::uint32_t>(ours), &entry);
			SpliceEntry(a_entryList, static_cast<std::uint32_t>(ours), nullptr);
			a_entryList.PushBack(entry);
			a_listObj.Invoke("InvalidateData", nullptr, nullptr, 0);
		}

		if (!s_settingsListTaken) {
			s_settingsListTaken = true;
			RE::GFxValue scope, fn;
			a_movieView->CreateObject(&scope);
			a_movieView->CreateFunction(&fn, &g_settingsListPressHandler);
			scope.SetMember("onFUCKSettingsPress", fn);
			const RE::GFxValue add[3] = { RE::GFxValue("itemPress"), scope, RE::GFxValue("onFUCKSettingsPress") };
			a_listObj.Invoke("addEventListener", nullptr, add, 3);
		}
	}

	static bool s_fuckButtonInjected = false;

	// --- Menu Action Interceptors ---
	// Anything that isn't the entry goes on to the page's own handler as it came, arguments or none, as other mods call these to have the
	// page do its part, such as Native System Menu Framework running the settings press with nothing before filling its own tabs
	class FUCKMainPressHandler : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (!a_params.thisPtr || OpenIfEntryPressed(a_params))
				return;

			if (a_params.argCount >= 1 && a_params.args)
				AdjustMainPress(*a_params.thisPtr, a_params.args[0]);
			a_params.thisPtr->Invoke("fuck_orig_onCategoryButtonPress", a_params.retVal, a_params.args, a_params.argCount);
		}
	};
	static FUCKMainPressHandler g_fuckMainPressHandler;

	class FUCKSettingsPressHandler : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (!a_params.thisPtr || OpenIfEntryPressed(a_params))
				return;
			a_params.thisPtr->Invoke("fuck_orig_onSettingsCategoryPress", a_params.retVal, a_params.args, a_params.argCount);
		}
	};
	static FUCKSettingsPressHandler g_fuckSettingsPressHandler;

	static void TryInjectFUCKButton(RE::GFxMovieView* a_movieView)
	{
		if (!a_movieView)
			return;

		auto* manager = FUCKMan::GetSingleton();

		RE::GFxValue page;
		if (!a_movieView->GetVariable(&page, kSystemPagePath) || !page.IsObject()) {
			return;
		}

		const bool injectSettings = manager->GetInjectSettingsSubmenu();

		// 1. FETCH LIST
		RE::GFxValue listObj, entryList;
		if (!GetTargetList(page, injectSettings, listObj, entryList)) {
			return;
		}

		if (injectSettings)
			KeepEntryAfterOtherTabs(a_movieView, listObj, entryList);

		const std::uint32_t arraySize = entryList.GetArraySize();
		if (arraySize == 0)
			return;

		const char* menuName = FUCK::Translate(manager->GetSystemMenuName());
		SetInjectedName(menuName);

		// Determine menu version
		bool isSafeMenu = page.HasMember("UpdateIndices");
		manager->SetJournalMenuType(isSafeMenu ? FUCKMan::JournalMenuType::kSafe : FUCKMan::JournalMenuType::kSkyUIv5);

		// Quit is found by its text, or taken as the last entry where a mod has renamed it
		bool          replaced = false;
		std::uint32_t quitIdx  = arraySize - 1;

		// 2. SCAN LIST - Check for duplicates, locate $QUIT, and optionally replace $HELP
		for (std::uint32_t i = 0; i < arraySize; ++i) {
			RE::GFxValue element, textVal;
			if (entryList.GetElement(i, &element) && element.IsObject() && element.GetMember("text", &textVal) && textVal.IsString()) {
				std::string_view textStr(textVal.GetString());

				if (textStr == menuName) {
					s_fuckButtonInjected = true;
					return;
				}

				if (textStr == "$QUIT") {
					quitIdx = i;
				}

				// Only replace $HELP if enabled, targeting main menu, and on SkyUI v5 or Vanilla
				if (!isSafeMenu && !injectSettings && manager->GetReplaceHelpMenu() && textStr == "$HELP") {
					RE::GFxValue newEntry;
					a_movieView->CreateObject(&newEntry);
					newEntry.SetMember("text", menuName);
					entryList.SetElement(i, newEntry);
					replaced = true;
				}
			}
		}

		// 3. INJECT ENTRY - If we didn't replace $HELP, inject our new entry
		if (!replaced) {
			RE::GFxValue newEntry;
			a_movieView->CreateObject(&newEntry);
			newEntry.SetMember("text", menuName);

			if (!injectSettings) {
				// Above $QUIT. The newer page works out its buttons' places again, and the older one has its button presses and the
				// game's button refresh adjusted by the page hooks below
				newEntry.SetMember("fuck_inserted", RE::GFxValue(true));
				SpliceEntry(entryList, quitIdx, &newEntry);
				if (isSafeMenu)
					RefreshIndices(page, listObj);
			} else {
				entryList.PushBack(newEntry);
			}
		}

		// 4. ATTACH FLASH PAGE HOOKS
		if (!page.HasMember("fuck_hooked")) {
			RE::GFxValue origMain, origSet;
			if (page.GetMember("onCategoryButtonPress", &origMain)) {
				page.SetMember("fuck_orig_onCategoryButtonPress", origMain);
				RE::GFxValue hookMain;
				a_movieView->CreateFunction(&hookMain, &g_fuckMainPressHandler);
				page.SetMember("onCategoryButtonPress", hookMain);
			}
			if (page.GetMember("onSettingsCategoryPress", &origSet)) {
				page.SetMember("fuck_orig_onSettingsCategoryPress", origSet);
				RE::GFxValue hookSet;
				a_movieView->CreateFunction(&hookSet, &g_fuckSettingsPressHandler);
				page.SetMember("onSettingsCategoryPress", hookSet);
			}
			if (!isSafeMenu)
				HookButtonPlaces(a_movieView, page);
			page.SetMember("fuck_hooked", RE::GFxValue(true));
		}

		// 5. UPDATE UI GRAPHICS & BOUNDS
		UpdateListVisuals(a_movieView, listObj, entryList.GetArraySize(), injectSettings);

		// Invalidate lets us actually select the new entry
		listObj.Invoke("InvalidateData", nullptr, nullptr, 0);

		s_fuckButtonInjected = true;
	}

	static void TryRemoveFUCKButton(RE::GFxMovieView* a_movieView)
	{
		if (!a_movieView || !s_fuckButtonInjected)
			return;

		auto*        manager = FUCKMan::GetSingleton();
		RE::GFxValue page;
		if (!a_movieView->GetVariable(&page, kSystemPagePath) || !page.IsObject()) {
			return;
		}

		const bool injectSettings = manager->GetInjectSettingsSubmenu();

		// 1. FETCH LIST
		RE::GFxValue listObj, entryList;
		if (!GetTargetList(page, injectSettings, listObj, entryList)) {
			return;
		}

		const std::uint32_t arraySize = entryList.GetArraySize();
		if (arraySize == 0)
			return;

		const char* menuName   = InjectedName();
		bool        isSafeMenu = page.HasMember("UpdateIndices");
		bool        removed    = false;

		// 2. REMOVE ENTRY
		for (std::uint32_t i = 0; i < arraySize; ++i) {
			RE::GFxValue element, textVal;
			if (entryList.GetElement(i, &element) && element.IsObject() && element.GetMember("text", &textVal) && textVal.IsString()) {
				if (std::string_view(textVal.GetString()) == menuName) {
					if (!isSafeMenu && !injectSettings && manager->GetReplaceHelpMenu()) {
						element.SetMember("text", "$HELP");
						entryList.SetElement(i, element);
					} else {
						// Shift up to overwrite the injected entry
						for (std::uint32_t j = i; j < arraySize - 1; ++j) {
							RE::GFxValue temp;
							entryList.GetElement(j + 1, &temp);
							entryList.SetElement(j, temp);
						}
						entryList.SetArraySize(arraySize - 1);

						if (isSafeMenu && !injectSettings) {
							RefreshIndices(page, listObj);
						}
					}
					removed = true;
					break;
				}
			}
		}

		if (removed) {
			UpdateListVisuals(a_movieView, listObj, arraySize - 1, injectSettings);
			listObj.Invoke("InvalidateData", nullptr, nullptr, 0);
		}
		s_fuckButtonInjected = false;
	}

	struct JournalMenu_ProcessMessage
	{
		static inline bool s_journalMenuOpen = false;
		static inline bool s_showEntry       = false;

		static RE::UI_MESSAGE_RESULTS thunk(RE::JournalMenu* a_this, RE::UIMessage& a_message)
		{
			if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
				s_fuckButtonInjected = false;
				ResetCompat();
				s_journalMenuOpen    = false;
				return func(a_this, a_message);
			}

			auto result = func(a_this, a_message);

			if (a_message.type == RE::UI_MESSAGE_TYPE::kUpdate && a_this->uiMovie) {
				if (!s_journalMenuOpen) {
					s_journalMenuOpen = true;
					GuardSaveButtons(a_this->fxDelegate.get());

					const auto injectType     = FUCKMan::GetSingleton()->GetInjectSystemMenu();
					const bool controllerOnly = injectType == FUCKMan::InjectSystemMenuType::kControllerOnly;
					s_showEntry               = injectType == FUCKMan::InjectSystemMenuType::kYes || (controllerOnly && MANAGER(Input)->IsInputGamepad());
				}

				if (s_showEntry) {
					TryInjectFUCKButton(a_this->uiMovie.get());
				} else {
					TryRemoveFUCKButton(a_this->uiMovie.get());
				}
			}

			return result;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> journalVtbl(RE::VTABLE_JournalMenu[0]);
		JournalMenu_ProcessMessage::func = journalVtbl.write_vfunc(0x4, &JournalMenu_ProcessMessage::thunk);

		logger::info("Installed Journal Menu Hooks");
	}
}
