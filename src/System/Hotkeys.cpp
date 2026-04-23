#include "Hotkeys.h"

#include "FUCKMan.h"
#include "ImGui/IconsFonts.h"
#include "Input.h"

namespace Hotkeys
{
	void Manager::LoadHotKeys(const CSimpleIniA& a_ini)
	{
		_toggleHotkey.kKey =  static_cast<std::uint32_t>(a_ini.GetLongValue("Controls", "iToggleEditorKey", 65));
		_toggleHotkey.kMod1 = static_cast<std::int32_t>(a_ini.GetLongValue("Controls", "iToggleEditorKeyMod1", -1));
		_toggleHotkey.kMod2 = static_cast<std::int32_t>(a_ini.GetLongValue("Controls", "iToggleEditorKeyMod2", -1));

		_toggleHotkey.gKey =  static_cast<std::uint32_t>(a_ini.GetLongValue("Controls", "iToggleEditorGamePad", 0));
		_toggleHotkey.gMod1 = static_cast<std::int32_t>(a_ini.GetLongValue("Controls", "iToggleEditorGamePadMod1", -1));
		_toggleHotkey.gMod2 = static_cast<std::int32_t>(a_ini.GetLongValue("Controls", "iToggleEditorGamePadMod2", -1));
	}

	void Manager::SaveHotKeys(CSimpleIniA& a_ini)
	{
		a_ini.SetLongValue("Controls", "iToggleEditorKey",     static_cast<long>(_toggleHotkey.kKey));
		a_ini.SetLongValue("Controls", "iToggleEditorKeyMod1", static_cast<long>(_toggleHotkey.kMod1));
		a_ini.SetLongValue("Controls", "iToggleEditorKeyMod2", static_cast<long>(_toggleHotkey.kMod2));

		a_ini.SetLongValue("Controls", "iToggleEditorGamePad",     static_cast<long>(_toggleHotkey.gKey));
		a_ini.SetLongValue("Controls", "iToggleEditorGamePadMod1", static_cast<long>(_toggleHotkey.gMod1));
		a_ini.SetLongValue("Controls", "iToggleEditorGamePadMod2", static_cast<long>(_toggleHotkey.gMod2));
	}

	bool Manager::ProcessInput(const RE::InputEvent* const* a_event)
	{
		if (!enabled)
			return false;

		if (_toggleHotkey.isBinding)
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
		uint32_t key = isGP ? _toggleHotkey.gKey : _toggleHotkey.kKey;
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
