#include "FUCK-Man.h"

#include "Hooks.h"
#include "Input.h"

namespace Hooks
{
	static bool   s_fuckButtonInjected = false;
	static double s_fuckButtonIndex    = -1.0;

	constexpr const char* kSystemPagePath = "_root.QuestJournalFader.Menu_mc.SystemFader.Page_mc";

	static void TryInjectFUCKButton(RE::GFxMovieView* a_movieView)
	{
		if (!a_movieView || s_fuckButtonInjected)
			return;

		auto* manager = FUCKMan::GetSingleton();
		if (!manager->GetInjectSystemMenu())
			return;

		RE::GFxValue page, cat, listObj, entryList;
		if (!a_movieView->GetVariable(&page, kSystemPagePath) ||
			!page.GetMember("CategoryList_mc", &cat) || !cat.GetMember("List_mc", &listObj) ||
			!listObj.GetMember("entryList", &entryList) || !entryList.IsArray()) {
			return;
		}

		const std::uint32_t arraySize = entryList.GetArraySize();
		if (arraySize == 0)
			return;

		const std::string menuName = TRANSLATE_S("$FUCK_Title");

		if (manager->GetReplaceHelpMenu()) {
			for (std::uint32_t i = 0; i < arraySize; ++i) {
				RE::GFxValue element, textVal;
				if (entryList.GetElement(i, &element) && element.GetMember("text", &textVal) &&
					textVal.IsString() && std::string_view(textVal.GetString()) == "$HELP") {
					element.SetMember("text", RE::GFxValue(menuName.c_str()));
					entryList.SetElement(i, element);
					s_fuckButtonIndex = static_cast<double>(i);
					listObj.Invoke("InvalidateData", nullptr, nullptr, 0);
					s_fuckButtonInjected = true;
					return;
				}
			}
		}

		RE::GFxValue newEntry;
		a_movieView->CreateObject(&newEntry);
		newEntry.SetMember("text", RE::GFxValue(menuName.c_str()));
		entryList.PushBack(newEntry);
		listObj.Invoke("InvalidateData", nullptr, nullptr, 0);

		s_fuckButtonIndex    = static_cast<double>(arraySize);
		s_fuckButtonInjected = true;
	}

	// Returns true if the injected entry is selected and an accept input is detected.
	[[nodiscard]] static bool CheckForJournalAccept(RE::InputEvent* const* a_events)
	{
		if (!FUCKMan::GetSingleton()->GetInjectSystemMenu() || !s_fuckButtonInjected)
			return false;

		auto* journal = RE::UI::GetSingleton()->GetMenu<RE::JournalMenu>().get();
		if (!journal || !journal->uiMovie)
			return false;

		RE::GFxValue page, cat, listObj, selIdx;
		if (!journal->uiMovie->GetVariable(&page, kSystemPagePath) ||
			!page.GetMember("CategoryList_mc", &cat) || !cat.GetMember("List_mc", &listObj) ||
			!listObj.GetMember("selectedIndex", &selIdx) || !selIdx.IsNumber() ||
			selIdx.GetNumber() != s_fuckButtonIndex) {
			return false;
		}

		auto* input = MANAGER(Input);

		constexpr std::uint32_t kKeyEnter  = Input::Keymap::AsKey(KEY::kEnter);
		constexpr std::uint32_t kMouseLeft = Input::Keymap::kMBBase;
		constexpr std::uint32_t kGamepadA  = Input::Keymap::kGPBase + SKSE::InputMap::kGamepadButtonOffset_A;

		if (input->IsInputPressed(a_events, kKeyEnter) ||
			input->IsInputPressed(a_events, kMouseLeft) ||
			input->IsInputPressed(a_events, kGamepadA)) {
			if (auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			}

			SKSE::GetTaskInterface()->AddTask([]() {
				FUCKMan::GetSingleton()->Open();
			});

			return true;
		}

		return false;
	}

	// ==================================================
	// EVENT HOOKS
	// ==================================================

	// Hoping this doesn't bite me in the ass. I attempted to flag input to kNone instead
	// of messing with the linked list, but it utterly failed.
	
	struct ProcessInputQueue
	{
		// Tracks key-down events that were blocked, so their corresponding key-up
		// events can be suppressed to prevent orphaned releases reaching the game.
		static inline Set<std::uint32_t> s_blockedKeys;

		static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
		{
			if (!a_events || !*a_events) {
				func(a_dispatcher, a_events);
				return;
			}

			if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
				func(a_dispatcher, a_events);
				return;
			}

			MANAGER(Input)->ProcessInputEvents(a_events);

			auto* manager = FUCKMan::GetSingleton();

			// State before processing (did the user hit Esc while the menu was open?)
			const bool wasBlocking = manager->IsInputBlocked() || manager->IsOpen();

			// Process menu input (may change the open/blocked state mid-frame)
			const bool consumed = manager->ProcessAsyncInput(a_events);

			// State after processing
			const bool isBlocking  = manager->IsInputBlocked() || manager->IsOpen();
			const bool shouldBlock = wasBlocking || isBlocking || consumed;

			if (CheckForJournalAccept(a_events)) {
				constexpr RE::InputEvent* const empty_events[] = { nullptr };
				func(a_dispatcher, empty_events);
				return;
			}

			// Only filter if there's an active block or lingering orphaned up-events.
			if (shouldBlock || !s_blockedKeys.empty()) {
				auto*      userEvents     = RE::UserEvents::GetSingleton();
				const bool allowGameMenus = !manager->IsOpen() && manager->HasWindowWithFlag(FUCK::WindowFlags::kCloseOnGameMenu) && !ImGui::GetIO().WantTextInput;

				RE::InputEvent* newHead = nullptr;
				RE::InputEvent* newTail = nullptr;

				for (auto iter = *a_events; iter; iter = iter->next) {
					bool keep = true;

					if (auto btn = iter->AsButtonEvent()) {
						uint32_t key = btn->HasIDCode() ? Input::Keymap::GetUnifiedKey(btn->GetDevice(), btn->GetIDCode()) : 0;

						if (shouldBlock) {
							keep = false;

							// Always allow Screenshot and Console
							if (userEvents && (btn->userEvent == userEvents->screenshot || btn->userEvent == userEvents->console)) {
								keep = true;
							} else if (Input::Manager::IsUnifiedModifier(key)) {
								keep = true;
							}
							// Game menu passthrough — only relevant when a kCloseOnGameMenu window is open.
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
									btn->userEvent == userEvents->favorites)     {
									keep = true;
								}
							}

							// If the key is being blocked on the way down, track it for orphan suppression.
							if (!keep && btn->Value() > 0.0f) {
								s_blockedKeys.insert(key);
							}
						} else {
							// Menu is closed, but this key was pressed while the menu was open — suppress the orphaned up-event.
							if (s_blockedKeys.contains(key)) {
								keep = false;
							}
						}

						// Explicitly block mouse wheel while blocking (wheel events are discrete, they don't hold).
						if (btn->GetDevice() == RE::INPUT_DEVICE::kMouse && (btn->GetIDCode() == 8 || btn->GetIDCode() == 9)) {
							if (shouldBlock)
								keep = false;
						}

						// Always clean up tracking on release.
						if (btn->Value() <= 0.0f) {
							s_blockedKeys.erase(key);
						}

					} else if (auto idEvent = iter->AsIDEvent()) {
						uint32_t key = idEvent->HasIDCode() ? Input::Keymap::GetUnifiedKey(idEvent->GetDevice(), idEvent->GetIDCode()) : 0;
						if (s_blockedKeys.contains(key)) {
							keep = false;
						}
					}

					// Rebuild the linked list with only kept events; blocked events are dropped.
					if (keep) {
						if (!newHead) {
							newHead = iter;
						} else {
							newTail->next = iter;
						}
						newTail = iter;
					}
				}

				if (newTail) {
					newTail->next = nullptr;
				}

				// Temporarily lift the hard pause so filtered events can still reach
				// active menus (e.g. game-menu passthrough). Restored after dispatch.
				auto          ui          = RE::UI::GetSingleton();
				std::uint32_t savedPauses = 0;
				if (ui && ui->numPausesGame > 0) {
					savedPauses       = ui->numPausesGame;
					ui->numPausesGame = 0;
				}

				// Dispatch the events
				if (newHead) {
					RE::InputEvent* const filtered[] = { newHead };
					func(a_dispatcher, filtered);
				} else {
					constexpr RE::InputEvent* const dummy[] = { nullptr };
					func(a_dispatcher, dummy);
				}

				if (ui && savedPauses > 0) {
					ui->numPausesGame = savedPauses;
				}
				return;
			}

			func(a_dispatcher, a_events);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	struct JournalMenu_ProcessMessage
	{
		static RE::UI_MESSAGE_RESULTS thunk(RE::JournalMenu* a_this, RE::UIMessage& a_message)
		{
			if (a_message.type == RE::UI_MESSAGE_TYPE::kHide) {
				s_fuckButtonInjected = false;
				s_fuckButtonIndex    = -1.0;
				return func(a_this, a_message);
			}

			auto result = func(a_this, a_message);

			if (a_message.type == RE::UI_MESSAGE_TYPE::kUpdate && !s_fuckButtonInjected && a_this->uiMovie) {
				TryInjectFUCKButton(a_this->uiMovie.get());
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
