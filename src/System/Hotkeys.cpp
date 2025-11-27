#include "Hotkeys.h"

#include "FUCKMan.h"
#include "ImGui/IconsFonts.h"
#include "Input.h"

namespace Hotkeys
{
	void Manager::LoadHotKeys(const CSimpleIniA& a_ini)
	{
		_toggleHotkey.kKey = (uint32_t)a_ini.GetLongValue("Controls", "iToggleEditorKey", 65);
		_toggleHotkey.kMod1 = (int32_t)a_ini.GetLongValue("Controls", "iToggleEditorKeyMod1", -1);
		_toggleHotkey.kMod2 = (int32_t)a_ini.GetLongValue("Controls", "iToggleEditorKeyMod2", -1);

		_toggleHotkey.gKey = (uint32_t)a_ini.GetLongValue("Controls", "iToggleEditorGamePad", 0);
		_toggleHotkey.gMod1 = (int32_t)a_ini.GetLongValue("Controls", "iToggleEditorGamePadMod1", -1);
		_toggleHotkey.gMod2 = (int32_t)a_ini.GetLongValue("Controls", "iToggleEditorGamePadMod2", -1);
	}

	void Manager::SaveHotKeys(CSimpleIniA& a_ini)
	{
		a_ini.SetLongValue("Controls", "iToggleEditorKey", _toggleHotkey.kKey);
		a_ini.SetLongValue("Controls", "iToggleEditorKeyMod1", _toggleHotkey.kMod1);
		a_ini.SetLongValue("Controls", "iToggleEditorKeyMod2", _toggleHotkey.kMod2);

		a_ini.SetLongValue("Controls", "iToggleEditorGamePad", _toggleHotkey.gKey);
		a_ini.SetLongValue("Controls", "iToggleEditorGamePadMod1", _toggleHotkey.gMod1);
		a_ini.SetLongValue("Controls", "iToggleEditorGamePadMod2", _toggleHotkey.gMod2);
	}

	void Manager::ProcessInput(const RE::InputEvent* const* a_event)
	{
		if (!enabled)
			return;

		if (_toggleHotkey.isBinding)
			return;

		if (FUCKMan::GetSingleton()->IsOpen()) {
			if (ImGui::GetCurrentContext()) {
				if (ImGui::GetIO().WantTextInput) {
						return;
					}
				}
		}
		if (auto ui = RE::UI::GetSingleton()) {
			if (ui->IsMenuOpen(RE::Console::MENU_NAME)) {
				return;
			}

			if (ui->GameIsPaused() && !FUCKMan::GetSingleton()->IsOpen()) {
            return;
			}
		}

		if (_toggleHotkey.isBinding)
			return;

		if (FUCK::ProcessManagedHotkey(a_event, _toggleHotkey)) {
			FUCKMan::GetSingleton()->Toggle();
		}
	}

	const IconFont::IconTexture* Manager::ToggleIcon() const
	{
		bool isGP = (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad);
		uint32_t key = isGP ? _toggleHotkey.gKey : _toggleHotkey.kKey;
		return (key != 0) ? MANAGER(IconFont)->GetIcon(key) : nullptr;
	}

	std::uint32_t Manager::EscapeKey()
	{
		return (FUCK::GetInputDevice() == FUCK::InputDevice::kGamepad) ?
		           static_cast<uint32_t>(SKSE::InputMap::kMacro_GamepadOffset) + SKSE::InputMap::kGamepadButtonOffset_B :
		           static_cast<uint32_t>(KEY::kEscape);
	}

	const IconFont::IconTexture* Manager::EscapeIcon() const
	{
		return MANAGER(IconFont)->GetIcon(EscapeKey());
	}
}
