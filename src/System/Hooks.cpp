#include "Hooks.h"
#include "FUCKMan.h"
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

	// Filters the input event list. Allows Screenshot/Console, and conditionally Game Menus.
	static RE::InputEvent* FilterInputEvents(RE::InputEvent* const* a_events)
	{
		auto* userEvents = RE::UserEvents::GetSingleton();
		auto* manager    = FUCKMan::GetSingleton();

		const bool allowGameMenus = !manager->IsOpen() &&
		                            manager->HasWindowWithFlag(FUCK::WindowFlags::kCloseOnGameMenu) &&
		                            !ImGui::GetIO().WantTextInput;

		RE::InputEvent* head = nullptr;
		RE::InputEvent* tail = nullptr;

		for (auto iter = *a_events; iter;) {
			auto next = iter->next;
			bool keep = false;

			if (auto button = iter->AsButtonEvent()) {
				const auto& eventName = button->userEvent;

				// Always let Key-Up events through so the game doesn't get stuck inputs
				if (button->IsUp()) {
					keep = true;
				}

				// Always allow Screenshot and Console
				else if (userEvents && (eventName == userEvents->screenshot || eventName == userEvents->console)) {
					keep = true;
				}
				// Pass menu inputs to the game so MenuOpenCloseEvent can trigger
				else if (button->HasIDCode()) {
					const auto rawKey     = button->GetIDCode();
					const auto device     = button->GetDevice();
					const auto unifiedKey = Input::Keymap::GetUnifiedKey(device, rawKey);

					if (Input::Manager::IsUnifiedModifier(unifiedKey)) {
						keep = true;
					}
					// Game menu passthrough — only relevant when a kCloseOnGameMenu window is open.
					else if (allowGameMenus && userEvents) {
						if (eventName == userEvents->tweenMenu ||
							eventName == userEvents->journal ||
							eventName == userEvents->map ||
							eventName == userEvents->quickMap ||
							eventName == userEvents->inventory ||
							eventName == userEvents->quickInventory ||
							eventName == userEvents->quickMagic ||
							eventName == userEvents->stats ||
							eventName == userEvents->quickStats ||
							eventName == userEvents->favorites) {
							keep = true;
						}
					}
				}
			}
			if (keep) {
				if (!head)
					head = iter;
				else
					tail->next = iter;
				tail       = iter;
				tail->next = nullptr;
			}
			iter = next;
		}

		return head;
	}

	// ========================================================================
	// EVENT HOOKS
	// ========================================================================

	struct ProcessInputQueue
	{
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

			const bool wasBlocked = FUCKMan::GetSingleton()->IsInputBlocked();
			const bool consumed   = FUCKMan::GetSingleton()->ProcessAsyncInput(a_events);

			if (CheckForJournalAccept(a_events)) {
				constexpr RE::InputEvent* const empty_events[] = { nullptr };
				func(a_dispatcher, empty_events);
				return;
			}

			if (consumed || wasBlocked) {
				// Get the new filtered list head
				RE::InputEvent* filteredHead = FilterInputEvents(a_events);

				auto          ui          = RE::UI::GetSingleton();
				std::uint32_t savedPauses = 0;

				// Temporarily lift the hard pause
				if (ui && ui->numPausesGame > 0) {
					savedPauses       = ui->numPausesGame;
					ui->numPausesGame = 0;
				}

				// Dispatch the events
				if (filteredHead) {
					RE::InputEvent* const filtered[] = { filteredHead };
					func(a_dispatcher, filtered);
				} else {
					constexpr RE::InputEvent* const dummy[] = { nullptr };
					func(a_dispatcher, dummy);
				}

				// Restore the hard pause
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
