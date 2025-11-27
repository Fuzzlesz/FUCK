#include "Papyrus.h"
#include "FUCKMan.h"

namespace Papyrus
{
	constexpr std::string_view ScriptName = "FUCK_API";

	bool ToggleMenu(RE::StaticFunctionTag*)
	{
		FUCKMan::GetSingleton()->Toggle();
		return true;
	}

	bool OpenMenu(RE::StaticFunctionTag*)
	{
		auto* man = FUCKMan::GetSingleton();
		if (!man->IsOpen()) {
			man->Toggle();
		}
		return true;
	}

	bool CloseMenu(RE::StaticFunctionTag*)
	{
		auto* man = FUCKMan::GetSingleton();
		if (man->IsOpen()) {
			man->Toggle();
		}
		return true;
	}

	bool IsMenuOpen(RE::StaticFunctionTag*)
	{
		return FUCKMan::GetSingleton()->IsOpen();
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		a_vm->RegisterFunction("ToggleMenu", ScriptName, ToggleMenu);
		a_vm->RegisterFunction("OpenMenu", ScriptName, OpenMenu);
		a_vm->RegisterFunction("CloseMenu", ScriptName, CloseMenu);
		a_vm->RegisterFunction("IsMenuOpen", ScriptName, IsMenuOpen);

		logger::info("Registered Papyrus Bindings for {}", ScriptName);
		return true;
	}
}
