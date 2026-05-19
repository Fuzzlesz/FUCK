#include "Hotkeys.h"

#include "FUCKMan.h"
#include "ImGui/IconsFonts.h"
#include "Input.h"

namespace Hotkeys
{
	void Manager::LoadHotKeys(const CSimpleIniA& a_ini)
	{
		_toggleHotkey.kKey  = FUCK::INI::LoadInt(a_ini, "Hotkeys", "iToggleFUCK_Key", _defToggle.kKey);
		_toggleHotkey.kMod1 = FUCK::INI::LoadInt(a_ini, "Hotkeys", "iToggleFUCK_Mod1", _defToggle.kMod1);
		_toggleHotkey.kMod2 = FUCK::INI::LoadInt(a_ini, "Hotkeys", "iToggleFUCK_Mod2", _defToggle.kMod2);
		_toggleHotkey.gKey  = FUCK::INI::LoadInt(a_ini, "Hotkeys", "iToggleFUCK_GPKey", _defToggle.gKey);
		_toggleHotkey.gMod1 = FUCK::INI::LoadInt(a_ini, "Hotkeys", "iToggleFUCK_GPMod1", _defToggle.gMod1);
		_toggleHotkey.gMod2 = FUCK::INI::LoadInt(a_ini, "Hotkeys", "iToggleFUCK_GPMod2", _defToggle.gMod2);
	}

	void Manager::SaveHotKeys(CSimpleIniA& a_ini)
	{
		FUCK::INI::SaveInt(a_ini, "Hotkeys", "iToggleFUCK_Key", _toggleHotkey.kKey, _defToggle.kKey);
		FUCK::INI::SaveInt(a_ini, "Hotkeys", "iToggleFUCK_Mod1", _toggleHotkey.kMod1, _defToggle.kMod1);
		FUCK::INI::SaveInt(a_ini, "Hotkeys", "iToggleFUCK_Mod2", _toggleHotkey.kMod2, _defToggle.kMod2);
		FUCK::INI::SaveInt(a_ini, "Hotkeys", "iToggleFUCK_GPKey", _toggleHotkey.gKey, _defToggle.gKey);
		FUCK::INI::SaveInt(a_ini, "Hotkeys", "iToggleFUCK_GPMod1", _toggleHotkey.gMod1, _defToggle.gMod1);
		FUCK::INI::SaveInt(a_ini, "Hotkeys", "iToggleFUCK_GPMod2", _toggleHotkey.gMod2, _defToggle.gMod2);
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
