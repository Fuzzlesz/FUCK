#include "Hotkeys.h"

#include "FUCKMan.h"
#include "ImGui/IconsFonts.h"
#include "Input.h"

namespace Hotkeys
{
	void Manager::LoadHotKeys(const CSimpleIniA& a_ini)
	{
		_toggleHotkey.kKey = static_cast<std::uint32_t>(a_ini.GetLongValue("Hotkeys", "iToggleFUCK_Key", _defToggle.kKey));
		_toggleHotkey.kMod1 = static_cast<std::int32_t>(a_ini.GetLongValue("Hotkeys", "iToggleFUCK_Mod1", _defToggle.kMod1));
		_toggleHotkey.kMod2 = static_cast<std::int32_t>(a_ini.GetLongValue("Hotkeys", "iToggleFUCK_Mod2", _defToggle.kMod2));
		_toggleHotkey.gKey = static_cast<std::uint32_t>(a_ini.GetLongValue("Hotkeys", "iToggleFUCK_GPKey", _defToggle.gKey));
		_toggleHotkey.gMod1 = static_cast<std::int32_t>(a_ini.GetLongValue("Hotkeys", "iToggleFUCK_GPMod1", _defToggle.gMod1));
		_toggleHotkey.gMod2 = static_cast<std::int32_t>(a_ini.GetLongValue("Hotkeys", "iToggleFUCK_GPMod2", _defToggle.gMod2));
	}

	void Manager::SaveHotKeys(CSimpleIniA& a_ini)
	{
		a_ini.SetLongValue("Hotkeys", "iToggleFUCK_Key", _toggleHotkey.kKey);
		a_ini.SetLongValue("Hotkeys", "iToggleFUCK_Mod1", _toggleHotkey.kMod1);
		a_ini.SetLongValue("Hotkeys", "iToggleFUCK_Mod2", _toggleHotkey.kMod2);
		a_ini.SetLongValue("Hotkeys", "iToggleFUCK_GPKey", _toggleHotkey.gKey);
		a_ini.SetLongValue("Hotkeys", "iToggleFUCK_GPMod1", _toggleHotkey.gMod1);
		a_ini.SetLongValue("Hotkeys", "iToggleFUCK_GPMod2", _toggleHotkey.gMod2);
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
		bool isGP = (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad);
		std::uint32_t key = isGP ? _toggleHotkey.gKey : _toggleHotkey.kKey;
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
