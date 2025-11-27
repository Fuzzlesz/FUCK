#include "Compat.h"

	namespace Compat
{
	namespace ImmersiveHUD
	{
		static RE::TESGlobal* disableGlobal = nullptr;

		void Initialize()
		{
			if (const auto dataHandler = RE::TESDataHandler::GetSingleton()) {
				disableGlobal = dataHandler->LookupForm<RE::TESGlobal>(0xDDD, "ImmersiveHUD.esp");

				if (disableGlobal) {
					logger::info("Compat: Obtained ImmersiveHUD Control Global (0xDDD)");
				} else {
					logger::info("Compat: ImmersiveHUD not installed or global not found.");
				}
			}
		}

		void SetDisabled(bool a_disabled)
		{
			if (disableGlobal) {
				float targetValue = a_disabled ? 1.0f : 0.0f;

				if (disableGlobal->value != targetValue) {
					disableGlobal->value = targetValue;
				}
			}
		}
	}
}
