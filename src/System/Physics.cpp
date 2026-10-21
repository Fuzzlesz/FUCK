#include "System/Physics.h"

#include "Camera/Manager.h"

namespace Physics
{
	namespace
	{
		// The call FSMP hooks to step its physics each frame (its MainHooks::UpdateHook1)
		std::uintptr_t CallSite()
		{
			static const REL::Relocation<std::uintptr_t> site{ REL::VariantID(35551, 36544, 0x5B6D70), REL::VariantOffset(0x11F, 0x160, 0x11F) };
			return site.address();
		}

		// Where the call goes now, which changes with each hook on it
		std::uintptr_t CallTarget()
		{
			const auto site = CallSite();
			return site + 5 + *reinterpret_cast<const std::int32_t*>(site + 1);
		}

		// What FSMP reads to decide whether the game is paused, and the frame time it steps by, as the game had them
		struct Saved
		{
			std::uint32_t pauses     = 0;
			bool          freezeTime = false;
			float         delta      = 0.0f;
		};
		std::optional<Saved> g_saved;

		std::uintptr_t g_innerTarget = 0;
		bool           g_active      = false;  // both hooks are on, with FSMP's between them

		struct Inner
		{
			static void thunk(RE::Main* a_main)
			{
				func(a_main);

				auto ui    = RE::UI::GetSingleton();
				auto timer = RE::BSTimer::GetSingleton();
				if (!g_active || !ui || !timer || !MANAGER(Camera)->RunsPhysics())
					return;

				auto& runtime = a_main->GetRuntimeData();
				if (ui->numPausesGame == 0 && !runtime.freezeTime)
					return;

				g_saved            = Saved{ ui->numPausesGame, runtime.freezeTime, timer->delta };
				ui->numPausesGame  = 0;
				runtime.freezeTime = false;
				// A paused game's frame time, so FSMP steps by the time that really passed
				timer->delta = timer->realTimeDelta;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Outer
		{
			static void thunk(RE::Main* a_main)
			{
				func(a_main);

				if (!g_saved)
					return;

				if (auto ui = RE::UI::GetSingleton())
					ui->numPausesGame = g_saved->pauses;
				if (auto timer = RE::BSTimer::GetSingleton())
					timer->delta = g_saved->delta;
				a_main->GetRuntimeData().freezeTime = g_saved->freezeTime;
				g_saved.reset();
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void InstallInner()
	{
		if (!GetModuleHandleW(L"hdtSMP64.dll"))
			return;

		stl::write_thunk_call<Inner>(CallSite());
		g_innerTarget = CallTarget();
	}

	void InstallOuter()
	{
		if (!g_innerTarget)
			return;

		// Nothing hooked the call after the inner hook, so FSMP isn't between them and there's nothing to do
		if (CallTarget() == g_innerTarget) {
			logger::info("FSMP didn't hook its frame call after FUCK, so its physics stay paused with the game");
			return;
		}

		stl::write_thunk_call<Outer>(CallSite());
		g_active = true;
		logger::info("FSMP found, so its physics run in paused camera sessions that ask for them");
	}
}
