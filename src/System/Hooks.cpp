#include "FUCK-Man.h"

#include "ImGui/Audio.h"

#include "Hooks.h"
#include "Input.h"

namespace Hooks
{
	static bool        s_fuckButtonInjected = false;
	static std::string s_injectedMenuName   = "";

	constexpr const char* kSystemPagePath = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc";

	// --- Helper: Safely locates the target list whether we are in Vanilla, SkyUI, or heavily modded setups
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

	// --- Menu Action Interceptors ---
	class FUCKMainPressHandler : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (a_params.argCount < 1 || !a_params.args || !a_params.thisPtr)
				return;
			RE::GFxValue& eventObj = a_params.args[0];

			RE::GFxValue entry, textVal;
			if (eventObj.GetMember("entry", &entry) && entry.GetMember("text", &textVal) && textVal.IsString()) {
				std::string_view textStr(textVal.GetString());
				const char*      menuName = s_injectedMenuName.empty() ? FUCK::Translate(FUCKMan::GetSingleton()->GetSystemMenuName()) : s_injectedMenuName.c_str();

				if (textStr == menuName) {
					// Close the Journal menu natively so the game state clears
					if (auto queue = RE::UIMessageQueue::GetSingleton()) {
						queue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
					}
					// Defer opening our menu to the next frame to prevent input collisions
					SKSE::GetTaskInterface()->AddTask([]() { FUCKMan::GetSingleton()->Open(); });
					return;  // Safe early out! Do not forward to vanilla
				}
			}
			a_params.thisPtr->Invoke("fuck_orig_onCategoryButtonPress", nullptr, a_params.args, 1);
		}
	};
	static FUCKMainPressHandler g_fuckMainPressHandler;

	class FUCKSettingsPressHandler : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			if (a_params.argCount < 1 || !a_params.args || !a_params.thisPtr)
				return;
			RE::GFxValue& eventObj = a_params.args[0];

			RE::GFxValue entry, textVal;
			if (eventObj.GetMember("entry", &entry) && entry.GetMember("text", &textVal) && textVal.IsString()) {
				std::string_view textStr(textVal.GetString());
				const char*      menuName = s_injectedMenuName.empty() ? FUCK::Translate(FUCKMan::GetSingleton()->GetSystemMenuName()) : s_injectedMenuName.c_str();

				if (textStr == menuName) {
					// Close the Journal menu natively so the game state clears
					if (auto queue = RE::UIMessageQueue::GetSingleton()) {
						queue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
					}
					// Defer opening our menu to the next frame to prevent input collisions
					SKSE::GetTaskInterface()->AddTask([]() { FUCKMan::GetSingleton()->Open(); });
					return;  // Safe early out! Do not forward to vanilla
				}
			}
			a_params.thisPtr->Invoke("fuck_orig_onSettingsCategoryPress", nullptr, a_params.args, 1);
		}
	};
	static FUCKSettingsPressHandler g_fuckSettingsPressHandler;

	// --- Helper: Duplicates the visual MovieClips and expands the mask boundary
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

		const std::uint32_t arraySize = entryList.GetArraySize();
		if (arraySize == 0)
			return;

		const char* menuName = FUCK::Translate(manager->GetSystemMenuName());
		s_injectedMenuName   = menuName;

		// Determine menu version
		bool isSafeMenu = page.HasMember("UpdateIndices");
		manager->SetJournalMenuType(isSafeMenu ? FUCKMan::JournalMenuType::kSafe : FUCKMan::JournalMenuType::kSkyUIv5);

		bool          replaced = false;
		std::uint32_t quitIdx  = arraySize;

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

			if (!injectSettings && isSafeMenu && quitIdx < arraySize) {
				// Push the last element back to safely grow the array size by 1 in GFx
				RE::GFxValue lastTemp;
				entryList.GetElement(arraySize - 1, &lastTemp);
				entryList.PushBack(lastTemp);

				// Shift elements down by 1 to make room at quitIdx
				for (std::uint32_t i = arraySize - 1; i > quitIdx; --i) {
					RE::GFxValue temp;
					entryList.GetElement(i - 1, &temp);
					entryList.SetElement(i, temp);
				}
				// Insert our new entry above $QUIT
				entryList.SetElement(quitIdx, newEntry);
				
				// Re-align internal indices for SkyUI v6+
				page.Invoke("UpdateIndices", nullptr, nullptr, 0);
			} else {
				// Fallback: append to the very bottom for SkyUI v5 so we don't shift hardcoded indices
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

		const char* menuName   = s_injectedMenuName.empty() ? FUCK::Translate(manager->GetSystemMenuName()) : s_injectedMenuName.c_str();
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
							page.Invoke("UpdateIndices", nullptr, nullptr, 0);
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

	// ==================================================
	// EVENT HOOKS
	// ==================================================

	struct ProcessInputQueue
	{
		// Tracks the blocking state of the previous frame
		static inline bool s_wasBlocking = false;

		static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
		{
			// Update keystate tracking
			MANAGER(Input)->ProcessInputEvents(a_events);

			// Do not interfere if console is open
			if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
				func(a_dispatcher, a_events);
				return;
			}

			auto* manager = FUCKMan::GetSingleton();

			// Process menu input (may change the open/blocked state mid-frame)
			const bool consumed = manager->ProcessAsyncInput(a_events);

			// State after processing
			const bool isBlocking = manager->IsInputBlocked() || manager->IsOpen() || consumed;
			
			// Detect the exact frame we transition into a blocked state
			const bool justBlocked = isBlocking && !s_wasBlocking;
			s_wasBlocking          = isBlocking;

			if (isBlocking) {
				auto*      userEvents     = RE::UserEvents::GetSingleton();
				const bool allowGameMenus = !manager->IsOpen() && manager->HasWindowWithFlag(FUCK::WindowFlags::kCloseOnGameMenu) && !ImGui::GetIO().WantTextInput;

				if (a_events && *a_events) {
					for (auto iter = *a_events; iter; iter = iter->next) {
						bool keep = false;

						if (auto btn = iter->AsButtonEvent()) {
							// Always allow Screenshot and Console through the block
							if (userEvents && (btn->userEvent == userEvents->screenshot || btn->userEvent == userEvents->console)) {
								keep = true;
							}
							// Game menu passthrough — only relevant when a kCloseOnGameMenu window is open
							else if (allowGameMenus && userEvents) {
								if (btn->userEvent == userEvents->tweenMenu      ||
									btn->userEvent == userEvents->journal        ||
									btn->userEvent == userEvents->map            ||
									btn->userEvent == userEvents->quickMap       ||
									btn->userEvent == userEvents->inventory      ||
									btn->userEvent == userEvents->quickInventory ||
									btn->userEvent == userEvents->quickMagic     ||
									btn->userEvent == userEvents->stats          ||
									btn->userEvent == userEvents->quickStats     ||
									btn->userEvent == userEvents->favorites)      {
									keep = true;
								}
							}
						}

						// Zero-out the event
						if (!keep) {
							if (auto btn = iter->AsButtonEvent()) {
								btn->value        = 0.0f;
								btn->heldDownSecs = 0.0f;
							} else if (auto idEvent = iter->AsIDEvent()) {
								if (auto thumb = idEvent->AsThumbstickEvent()) {
									thumb->xValue = 0.0f;
									thumb->yValue = 0.0f;
								} else if (auto mouse = idEvent->AsMouseMoveEvent()) {
									mouse->mouseInputX = 0;
									mouse->mouseInputY = 0;
								}
							} else if (auto charEvent = iter->AsCharEvent()) {
								charEvent->keyCode = 0;
							}
						}
					}
				}

				// Temporarily unpause the game if a paused menu opens, to ensure our filtered events are processed and don't get "stuck"
				std::uint32_t savedPauses = 0;
				bool          savedFreeze = false;
				auto          ui          = RE::UI::GetSingleton();
				auto          main        = RE::Main::GetSingleton();

				if (justBlocked) {
					if (ui && ui->numPausesGame > 0) {
						savedPauses       = ui->numPausesGame;
						ui->numPausesGame = 0;
					}
					if (main && main->freezeTime) {
						savedFreeze      = main->freezeTime;
						main->freezeTime = false;
					}
				}

				// Dispatch the filtered/zeroed events
				func(a_dispatcher, a_events);

				// Restore the pauses immediately after dispatch
				if (justBlocked) {
					if (main && savedFreeze) {
						main->freezeTime = savedFreeze;
					}
					if (ui && savedPauses > 0) {
						ui->numPausesGame += savedPauses;
					}
				}

				return;
			}

			// Not blocking, normal pass-through
			func(a_dispatcher, a_events);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct JournalMenu_ProcessMessage
	{
		static inline bool   s_journalMenuOpen    = false;
		static inline bool   s_lockedGamepadState = false;
		static inline int    s_deviceChangeTicks  = 0;
		static constexpr int kHysteresisThreshold = 10;  // Frames required to lock-in a device change

		static RE::UI_MESSAGE_RESULTS thunk(RE::JournalMenu* a_this, RE::UIMessage& a_message)
		{
			if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
				s_fuckButtonInjected = false;
				s_journalMenuOpen    = false;
				s_deviceChangeTicks  = 0;
				return func(a_this, a_message);
			}

			auto result = func(a_this, a_message);

			if (a_message.type == RE::UI_MESSAGE_TYPE::kUpdate && a_this->uiMovie) {
				auto* manager = FUCKMan::GetSingleton();

				if (!manager->GetInjectSystemMenu()) {
					TryRemoveFUCKButton(a_this->uiMovie.get());
					return result;
				}

				bool currentIsGamepad = MANAGER(Input)->IsInputGamepad();

				if (!s_journalMenuOpen) {
					s_journalMenuOpen    = true;
					s_lockedGamepadState = currentIsGamepad;
					s_deviceChangeTicks  = 0;
				} else {
					if (currentIsGamepad != s_lockedGamepadState) {
						s_deviceChangeTicks++;
						if (s_deviceChangeTicks >= kHysteresisThreshold) {
							s_lockedGamepadState = currentIsGamepad;
							s_deviceChangeTicks  = 0;
						}
					} else {
						// State returned to normal before threshold was met, drop the buffer
						s_deviceChangeTicks = 0;
					}
				}

				if (s_lockedGamepadState) {
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
		REL::Relocation<std::uintptr_t> inputUnk(RELOCATION_ID(67315, 68617), 0x7B);
		stl::write_thunk_call<ProcessInputQueue>(inputUnk.address());

		REL::Relocation<std::uintptr_t> journalVtbl(RE::VTABLE_JournalMenu[0]);
		JournalMenu_ProcessMessage::func = journalVtbl.write_vfunc(0x4, &JournalMenu_ProcessMessage::thunk);

		logger::info("Installed Input and Journal Menu Hooks");
	}
}
