#include "FUCK-Man.h"

#include "ImGui/IconsFonts.h"

#include "Hotkeys.h"
#include "Input.h"

namespace Hotkeys
{
	namespace
	{
		using Hotkey = FUCK::ManagedHotkey;

		const FUCK::INI::Fields<Hotkey> kToggleFields{ "Hotkeys", {
			{ "iToggleFUCK_Key",    &Hotkey::kKey  },
			{ "iToggleFUCK_Mod1",   &Hotkey::kMod1 },
			{ "iToggleFUCK_Mod2",   &Hotkey::kMod2 },
			{ "iToggleFUCK_GPKey",  &Hotkey::gKey  },
			{ "iToggleFUCK_GPMod1", &Hotkey::gMod1 },
			{ "iToggleFUCK_GPMod2", &Hotkey::gMod2 },
		} };
	}

	void Manager::LoadHotKeys(const CSimpleIniA& a_ini)
	{
		kToggleFields.Load(a_ini, _toggleHotkey, _defToggle);
	}

	void Manager::SaveHotKeys(CSimpleIniA& a_ini)
	{
		kToggleFields.Save(a_ini, _toggleHotkey, _defToggle);
	}

	bool Manager::ProcessInput(const RE::InputEvent* const* a_event)
	{
		if (!enabled)
			return false;

		// Block global toggle from triggering if ANY widget is actively binding
		if (Input::Manager::GetSingleton()->IsBinding())
			return false;

		// Prevent toggle while typing in ImGui widgets
		if (FUCKMan::GetSingleton()->IsOpen()) {
			if (ImGui::GetCurrentContext()) {
				if (ImGui::GetIO().WantTextInput) {
					return false;
				}
			}
		}

		if (auto ui = RE::UI::GetSingleton()) {
			// Don't open if Console is open
			if (ui->IsMenuOpen(RE::Console::MENU_NAME))
				return false;
			// Don't open during Load Screens
			if (ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME))
				return false;
		}

		if (FUCK::ProcessManagedHotkey(a_event, _toggleHotkey)) {
			FUCKMan::GetSingleton()->Toggle();
			return true;
		}
		return false;
	}

	const IconFont::IconTexture* Manager::ToggleIcon() const
	{
		bool          isGP = (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad);
		std::uint32_t key  = isGP ? _toggleHotkey.gKey : _toggleHotkey.kKey;
		return (key != 0) ? MANAGER(IconFont)->GetIcon(key) : nullptr;
	}

	std::uint32_t Manager::EscapeKey()
	{
		return (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad)
		? Input::Keymap::kGPBase + SKSE::InputMap::kGamepadButtonOffset_B
		: Input::Keymap::AsKey(KEY::kEscape);
	}

	const IconFont::IconTexture* Manager::EscapeIcon() const
	{
		return MANAGER(IconFont)->GetIcon(EscapeKey());
	}
}
