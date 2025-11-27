#include "Hooks.h"
#include "FUCKMan.h"
#include "Input.h"

namespace Hooks
{
	struct ProcessInputQueue
	{
		static void thunk(RE::BSTEventSource<RE::InputEvent*>* a_dispatcher, RE::InputEvent* const* a_events)
		{
			if (!a_events || !*a_events) {
				func(a_dispatcher, a_events);
				return;
			}

			// Process API events
			MANAGER(Input)->ProcessInputEvents(a_events);
			const bool consumed = FUCKMan::GetSingleton()->ProcessAsyncInput(a_events);

			// Filter input if blocked by UI
			if (consumed || FUCKMan::GetSingleton()->IsInputBlocked()) {
				auto* controlMap = RE::ControlMap::GetSingleton();
				auto* userEvents = RE::UserEvents::GetSingleton();

				RE::InputEvent* head = nullptr;
				RE::InputEvent* tail = nullptr;

				for (auto iter = *a_events; iter;) {
					auto next = iter->next;
					bool keep = false;

					if (auto button = iter->AsButtonEvent()) {
						if (controlMap && userEvents) {
							const auto id = button->GetIDCode();
							const auto device = button->GetDevice();
							const auto keyScreenshot = controlMap->GetMappedKey(userEvents->screenshot, device);
							const auto keyConsole = controlMap->GetMappedKey(userEvents->console, device);

							// Only pass mapped Screenshot or Console keys
							if (id != 0xFF && (id == keyScreenshot || id == keyConsole)) {
								keep = true;
							}
						}
					}

					if (keep) {
						if (!head)
							head = iter;
						else
							tail->next = iter;
						tail = iter;
						tail->next = nullptr;
					}
					iter = next;
				}

				if (head) {
					RE::InputEvent* const filtered[] = { head };
					func(a_dispatcher, filtered);
				} else {
					constexpr RE::InputEvent* const dummy[] = { nullptr };
					func(a_dispatcher, dummy);
				}
				return;
			}

			func(a_dispatcher, a_events);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> inputUnk(RELOCATION_ID(67315, 68617), 0x7B);
		stl::write_thunk_call<ProcessInputQueue>(inputUnk.address());

		logger::info("Installed Input Hooks");
	}
}
