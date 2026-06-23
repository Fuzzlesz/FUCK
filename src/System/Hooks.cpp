#include "FUCK-Man.h"

#include "Hooks.h"
#include "Input.h"
#include "Journal.h"

namespace Hooks
{
	template <class T>
	struct InputHandler_CanProcess
	{
		static bool thunk(T* a_this, RE::InputEvent* a_event)
		{
			if (FUCKMan::GetSingleton()->IsInputBlocked()) {
				return false;
			}
			return func(a_this, a_event);
		}
		static inline REL::Relocation<decltype(thunk)> func;

		static void Install(std::uint64_t a_offset = 0x1)
		{
			REL::Relocation<std::uintptr_t> vtbl{ T::VTABLE[0] };
			func = vtbl.write_vfunc(a_offset, &thunk);
		}
	};

	struct MenuOpenHandler_CanProcess
	{
		static bool thunk(RE::MenuOpenHandler* a_this, RE::InputEvent* a_event)
		{
			auto* manager = FUCKMan::GetSingleton();
			if (manager->IsInputBlocked()) {
				const bool allowGameMenus = !manager->IsOpen() && manager->HasWindowWithFlag(FUCK::WindowFlags::kCloseOnGameMenu) && !ImGui::GetIO().WantTextInput;
				if (!allowGameMenus) {
					return false;
				}
			}
			return func(a_this, a_event);
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		// Gameplay input handlers - silenced while FUCK is capturing input
		InputHandler_CanProcess<RE::MovementHandler>::Install();
		InputHandler_CanProcess<RE::LookHandler>::Install();
		InputHandler_CanProcess<RE::SprintHandler>::Install();
		InputHandler_CanProcess<RE::ReadyWeaponHandler>::Install();
		InputHandler_CanProcess<RE::AutoMoveHandler>::Install();
		InputHandler_CanProcess<RE::ToggleRunHandler>::Install();
		InputHandler_CanProcess<RE::ActivateHandler>::Install();
		InputHandler_CanProcess<RE::JumpHandler>::Install();
		InputHandler_CanProcess<RE::ShoutHandler>::Install();
		InputHandler_CanProcess<RE::AttackBlockHandler>::Install();
		InputHandler_CanProcess<RE::RunHandler>::Install();
		InputHandler_CanProcess<RE::SneakHandler>::Install();
		InputHandler_CanProcess<RE::TogglePOVHandler>::Install();

		InputHandler_CanProcess<RE::FirstPersonState>::Install(0xB);
		InputHandler_CanProcess<RE::ThirdPersonState>::Install(0x12);

		// Menu-open handler - blocks opening game menus while capturing input
		REL::Relocation<std::uintptr_t> menuOpenVtbl{ RE::MenuOpenHandler::VTABLE[0] };
		MenuOpenHandler_CanProcess::func = menuOpenVtbl.write_vfunc(0x1, &MenuOpenHandler_CanProcess::thunk);

		Journal::Install();

		logger::info("Installed Hooks");
	}
}
