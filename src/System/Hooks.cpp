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
		// Tracks keys swallowed while blocking, held until physically released
		static inline Set<uint32_t> s_blockedKeys;

		static bool IsWhitelisted(RE::ButtonEvent* a_btn, bool a_allowGameMenus)
		{
			auto userEvents = RE::UserEvents::GetSingleton();
			auto cmd        = a_btn->QUserEvent();
			if (!userEvents || cmd.empty())
				return false;

			// Always allow Screenshot and Console through the block
			if (cmd == userEvents->screenshot || cmd == userEvents->console)
				return true;

			// Game menu passthrough — only relevant when a kCloseOnGameMenu window is open
			if (a_allowGameMenus) {
				return cmd == userEvents->tweenMenu  || cmd == userEvents->journal        ||
				       cmd == userEvents->map        || cmd == userEvents->quickMap       ||
				       cmd == userEvents->inventory  || cmd == userEvents->quickInventory ||
				       cmd == userEvents->quickMagic || cmd == userEvents->stats          ||
				       cmd == userEvents->quickStats || cmd == userEvents->favorites      ;
			}
			return false;
		}

		static void FilterButton(RE::ButtonEvent* a_btn, bool a_isBlocking, bool a_justBlocked, bool a_justUnblocked, bool a_allowGameMenus)
		{
			auto&          rd     = a_btn->GetRuntimeData();
			const uint32_t device = static_cast<uint32_t>(a_btn->GetDevice());
			const uint32_t id     = a_btn->GetIDCode();
			const uint32_t hash   = (device << 16) | id;

			// Drop orphaned releases for keys the game never saw go down
			if (rd.value == 0.0f) {
				if (s_blockedKeys.contains(hash)) {
					s_blockedKeys.erase(hash);
					rd.heldDownSecs = 0.0f;
				}
				return;
			}

			if (a_isBlocking) {
				if (IsWhitelisted(a_btn, a_allowGameMenus))
					return;

				if (a_justBlocked && rd.heldDownSecs > 0.0f) {
					// Game already saw this key go down, send a release so it doesn't get stuck
					rd.value = 0.0f;
				} else {
					// Zero-out the event
					rd.value        = 0.0f;
					rd.heldDownSecs = 0.0f;
				}
				s_blockedKeys.insert(hash);
				return;
			}

			// Keys held across the block boundary stay swallowed until physically released
			if (a_justUnblocked)
				s_blockedKeys.insert(hash);

			if (s_blockedKeys.contains(hash)) {
				rd.value        = 0.0f;
				rd.heldDownSecs = 0.0f;
			}
		}

		static void NeutraliseEvent(RE::InputEvent* a_event)
		{
			// Zero-out the event
			if (auto idEvent = a_event->AsIDEvent()) {
				if (auto thumb = idEvent->AsThumbstickEvent()) {
					thumb->xValue = 0.0f;
					thumb->yValue = 0.0f;
				} else if (auto mouse = idEvent->AsMouseMoveEvent()) {
					mouse->mouseInputX = 0;
					mouse->mouseInputY = 0;
				}
			} else if (auto charEvent = a_event->AsCharEvent()) {
				charEvent->keyCode = 0;
			}
		}

		static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
		{
			// Update keystate tracking
			MANAGER(Input)->ProcessInputEvents(a_events);

			auto* manager = FUCKMan::GetSingleton();
			auto* ui      = RE::UI::GetSingleton();

			// Do not interfere if console is open
			const bool consoleOpen = ui && ui->IsMenuOpen(RE::Console::MENU_NAME);

			// Process menu input (may change the open/blocked state mid-frame)
			const bool consumed = manager->ProcessAsyncInput(a_events);

			// State after processing
			const bool isBlocking = !consoleOpen && (manager->IsInputBlocked() || manager->IsOpen() || consumed);

			// Detect the exact frame we transition into or out of a blocked state
			const bool justBlocked   = isBlocking && !s_wasBlocking;
			const bool justUnblocked = !isBlocking && s_wasBlocking;
			s_wasBlocking            = isBlocking;

			if (a_events && *a_events) {
				const bool allowGameMenus = !manager->IsOpen() && manager->HasWindowWithFlag(FUCK::WindowFlags::kCloseOnGameMenu) && !ImGui::GetIO().WantTextInput;

				for (auto iter = *a_events; iter; iter = iter->next) {
					if (auto btn = iter->AsButtonEvent()) {
						FilterButton(btn, isBlocking, justBlocked, justUnblocked, allowGameMenus);
					} else if (isBlocking) {
						NeutraliseEvent(iter);
					}
				}
			}

			// Temporarily unpause the game if a paused menu opens, to ensure our filtered events are processed and don't get "stuck"
			std::uint32_t savedPauses = 0;
			bool          savedFreeze = false;
			auto*         main        = RE::Main::GetSingleton();

			if (justBlocked) {
				if (ui && ui->numPausesGame > 0) {
					savedPauses       = ui->numPausesGame;
					ui->numPausesGame = 0;
				}
				if (main && main->GetRuntimeData().freezeTime) {
					savedFreeze                       = true;
					main->GetRuntimeData().freezeTime = false;
				}
			}

			// Dispatch the filtered/zeroed events
			func(a_dispatcher, a_events);

			// Restore the pauses immediately after dispatch
			if (justBlocked) {
				if (main && savedFreeze)
					main->GetRuntimeData().freezeTime = true;
				if (ui && savedPauses > 0)
					ui->numPausesGame += savedPauses;
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		// Same address-library ID as SE, but VR's call site sits at 0x81
		REL::Relocation<std::uintptr_t> inputUnk(RELOCATION_ID(67315, 68617), REL::Relocate(0x7B, 0x7B, 0x81));
		stl::write_thunk_call<ProcessInputQueue>(inputUnk.address());

		Journal::Install();

		logger::info("Installed Input Hooks");
	}
}
