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

		// If configured, overwrite the vanilla "Help" menu option.
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

		// Otherwise, append our button to the bottom of the list.
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
		constexpr std::uint32_t kGamepadA  = SKSE::InputMap::kGamepadButtonOffset_A;

		if (input->IsInputPressed(a_events, kKeyEnter) ||
			input->IsInputPressed(a_events, kMouseLeft) ||
			input->IsInputPressed(a_events, kGamepadA)) {
			// Close the Journal menu natively so the game state clears
			if (auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(RE::JournalMenu::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			}

			// Defer opening our menu to the next frame to prevent input collisions
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

	struct ProcessInputQueue
	{
		// Tracks the blocking state of the previous frame
		static inline bool s_wasBlocking = false;

		static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
		{
			// Do not interfere if console is open
			if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
				func(a_dispatcher, a_events);
				return;
			}

			// FUCKMan needs to run state checks every frame, even if a_events is empty
			MANAGER(Input)->ProcessInputEvents(a_events);

			auto* manager = FUCKMan::GetSingleton();

			// Process menu input (may change the open/blocked state mid-frame)
			const bool consumed = manager->ProcessAsyncInput(a_events);

			// State after processing
			const bool isBlocking = manager->IsInputBlocked() || manager->IsOpen() || consumed;

			// Detect the exact frame we transition into a blocked state
			const bool justBlocked = isBlocking && !s_wasBlocking;
			s_wasBlocking          = isBlocking;

			// Check if the user just clicked our injected System Menu button
			if (CheckForJournalAccept(a_events)) {
				// Swallow the input
				constexpr RE::InputEvent* const empty_events[] = { nullptr };
				func(a_dispatcher, empty_events);
				return;
			}

			if (isBlocking) {
				auto*      userEvents     = RE::UserEvents::GetSingleton();
				const bool allowGameMenus = !manager->IsOpen() && manager->HasWindowWithFlag(FUCK::WindowFlags::kCloseOnGameMenu) && !ImGui::GetIO().WantTextInput;

				RE::InputEvent* newHead = nullptr;
				RE::InputEvent* newTail = nullptr;

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
								btn->userEvent == userEvents->favorites)     {
								keep = true;
							}
						}

						// Zero-out on transition to blocked state to prevent "stuck" inputs
						if (!keep && justBlocked) {
							btn->value        = 0.0f;
							btn->heldDownSecs = 0.0f;
							keep              = true;
						}
					} else if (auto idEvent = iter->AsIDEvent()) {
						if (justBlocked) {
							if (auto thumb = idEvent->AsThumbstickEvent()) {
								thumb->xValue = 0.0f;
								thumb->yValue = 0.0f;
							}
							keep = true;
						}
					}

					// Rebuild the linked list with only kept (or zeroed) events
					if (keep) {
						if (!newHead) {
							newHead = iter;
							} else {
								newTail->next = iter;
							}
							newTail = iter;
						}
					}
				}

				// Terminate the chain
				if (newTail) {
					newTail->next = nullptr;
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
				if (newHead) {
					RE::InputEvent* const filtered[] = { newHead };
					func(a_dispatcher, filtered);
				} else {
					constexpr RE::InputEvent* const dummy[] = { nullptr };
					func(a_dispatcher, dummy);
				}

				// Restore the pauses immediately after dispatch
				if (justBlocked) {
					if (main && savedFreeze) {
						main->freezeTime = savedFreeze;
					}
					if (ui && savedPauses > 0) {
						ui->numPausesGame = savedPauses;
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
		static RE::UI_MESSAGE_RESULTS thunk(RE::JournalMenu* a_this, RE::UIMessage& a_message)
		{
			// Reset tracking when the journal menu closes
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
