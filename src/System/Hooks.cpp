#include "FUCK-Man.h"

#include "Hooks.h"
#include "Input.h"
#include "Journal.h"

namespace Hooks
{
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

	void Install()
	{
		REL::Relocation<std::uintptr_t> inputUnk(RELOCATION_ID(67315, 68617), 0x7B);
		stl::write_thunk_call<ProcessInputQueue>(inputUnk.address());

		Journal::Install();

		logger::info("Installed Input Hooks");
	}
}
