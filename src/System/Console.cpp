#include "Console.h"
#include "FUCKMan.h"

namespace Console
{
	struct StartFUCK
	{
		constexpr static auto OG_COMMAND = "ResetGrassFade"sv;

		constexpr static auto LONG_NAME = "FUCKMenu"sv;
		constexpr static auto SHORT_NAME = "FUCK"sv;
		constexpr static auto HELP = "Opens the F.U.C.K. menu interface.\n"sv;

		static bool Execute(const RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&, std::uint32_t&)
		{
			auto manager = FUCKMan::GetSingleton();

			if (auto queue = RE::UIMessageQueue::GetSingleton()) {
				queue->AddMessage(RE::Console::MENU_NAME, RE::UI_MESSAGE_TYPE::kHide, nullptr);
			}

			if (!manager->IsOpen()) {
				manager->Toggle();
				RE::ConsoleLog::GetSingleton()->Print("$FUCK_Console_Opened"_T);
			} else {
				manager->Toggle();
				RE::ConsoleLog::GetSingleton()->Print("$FUCK_Console_Closed"_T);
			}

			return true;
		}
	};

	void Install()
	{
		logger::info("{:*^30}", "CONSOLE COMMANDS");
		ConsoleCommandHandler<StartFUCK>::Install();
	}
}
