#include "Hooks.h"
#include "FUCKMan.h"
#include "Input.h"

namespace Hooks
{
	// Filters the input event list to only pass through Screenshot and Console keys.
	[[nodiscard]] static RE::InputEvent* FilterInputEvents(RE::InputEvent* const* a_events)
	{
		auto* controlMap = RE::ControlMap::GetSingleton();
		auto* userEvents = RE::UserEvents::GetSingleton();

		RE::InputEvent* head = nullptr;
		RE::InputEvent* tail = nullptr;

		for (auto iter = *a_events; iter;) {
			auto next = iter->next;
			bool keep = false;

			if (auto button = iter->AsButtonEvent()) {
				const auto id = button->GetIDCode();
				const auto device = button->GetDevice();
				const auto keyScreenshot = controlMap->GetMappedKey(userEvents->screenshot, device);
				const auto keyConsole = controlMap->GetMappedKey(userEvents->console, device);

				if (id != 0xFF && (id == keyScreenshot || id == keyConsole)) {
					keep = true;
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

			// Process API events
			MANAGER(Input)->ProcessInputEvents(a_events);
			const bool consumed = FUCKMan::GetSingleton()->ProcessAsyncInput(a_events);

			// Filter input if blocked by UI
			if (consumed || FUCKMan::GetSingleton()->IsInputBlocked()) {
				RE::InputEvent* filteredHead = FilterInputEvents(a_events);

				if (filteredHead) {
					RE::InputEvent* const filtered[] = { filteredHead };
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
