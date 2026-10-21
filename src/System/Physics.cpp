#include "System/Physics.h"

#include "Camera/Manager.h"

namespace Physics
{
	namespace
	{
		// Whether the camera session wants physics, and the game is paused so they'd otherwise stop
		bool IsWanted(RE::UI* a_ui, RE::Main* a_main)
		{
			return MANAGER(Camera)->RunsPhysics() && (a_ui->GameIsPaused() || a_main->GetRuntimeData().freezeTime);
		}

		// ==================================================
		// FSMP
		// ==================================================

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
		bool           g_fsmpActive  = false;  // both hooks are on, with FSMP's between them

		struct Inner
		{
			static void thunk(RE::Main* a_main)
			{
				func(a_main);

				auto ui    = RE::UI::GetSingleton();
				auto timer = RE::BSTimer::GetSingleton();
				if (!g_fsmpActive || !ui || !timer || !IsWanted(ui, a_main))
					return;

				auto& runtime      = a_main->GetRuntimeData();
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

		// ==================================================
		// CBPC
		// ==================================================

		using MenuSource = RE::BSTEventSource<RE::MenuOpenCloseEvent>;
		using MenuSink   = RE::BSTEventSink<RE::MenuOpenCloseEvent>;

		std::uintptr_t g_cbpcBase    = 0;
		std::size_t    g_cbpcSize    = 0;
		bool           g_cbpcRunning = false;  // CBPC has been told RaceMenu is open

		// CBPC's sink is the one whose code lives in its DLL
		MenuSink* FindCbpcSink(MenuSource* a_source)
		{
			RE::BSSpinLockGuard locker(a_source->lock);
			for (auto sink : a_source->sinks) {
				const auto vtable = sink ? *reinterpret_cast<std::uintptr_t*>(sink) : 0;
				if (vtable >= g_cbpcBase && vtable < g_cbpcBase + g_cbpcSize)
					return sink;
			}
			return nullptr;
		}

		void TellCbpc(RE::UI* a_ui, bool a_raceMenuOpen)
		{
			auto source = static_cast<MenuSource*>(a_ui);
			auto sink   = FindCbpcSink(source);
			if (!sink)
				return;

			RE::MenuOpenCloseEvent event{};
			event.menuName = RE::RaceSexMenu::MENU_NAME;
			event.opening  = a_raceMenuOpen;
			sink->ProcessEvent(&event, source);

			g_cbpcRunning = a_raceMenuOpen;
		}
	}

	void OnPostLoad()
	{
		if (GetModuleHandleW(L"hdtSMP64.dll")) {
			stl::write_thunk_call<Inner>(CallSite());
			g_innerTarget = CallTarget();
		}

		if (const auto module = GetModuleHandleW(L"cbp.dll")) {
			const auto base = reinterpret_cast<std::uintptr_t>(module);
			const auto dos  = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			const auto nt   = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
			g_cbpcBase      = base;
			g_cbpcSize      = nt->OptionalHeader.SizeOfImage;
			logger::info("CBPC found, so its physics run in paused camera sessions that ask for them");
		}
	}

	void OnDataLoaded()
	{
		if (!g_innerTarget)
			return;

		// Nothing hooked the call after the inner hook, so FSMP isn't between them and there's nothing to do
		if (CallTarget() == g_innerTarget) {
			logger::info("FSMP didn't hook its frame call after FUCK, so its physics stay paused with the game");
			return;
		}

		stl::write_thunk_call<Outer>(CallSite());
		g_fsmpActive = true;
		logger::info("FSMP found, so its physics run in paused camera sessions that ask for them");
	}

	void OnFrame()
	{
		auto ui   = RE::UI::GetSingleton();
		auto main = RE::Main::GetSingleton();
		if (!g_cbpcBase || !ui || !main)
			return;

		// The real RaceMenu runs CBPC already, and its own close tells CBPC it's gone
		if (ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME))
			return;

		const bool wanted = IsWanted(ui, main);
		if (wanted != g_cbpcRunning)
			TellCbpc(ui, wanted);
	}
}
