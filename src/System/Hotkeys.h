#pragma once

namespace IconFont
{
	struct IconTexture;
}

namespace Hotkeys
{
	struct HotkeyState
	{
		std::uint32_t kKey  = 65;  // F7
		std::int32_t  kMod1 = -1;
		std::int32_t  kMod2 = -1;
		std::uint32_t gKey  = 0;
		std::int32_t  gMod1 = -1;
		std::int32_t  gMod2 = -1;
	};

	class Manager : public REX::Singleton<Manager>
	{
	public:
		void LoadHotKeys(const CSimpleIniA& a_ini);
		void SaveHotKeys(CSimpleIniA& a_ini);
		bool ProcessInput(const RE::InputEvent* const* a_event);
		void Enable(bool a_enable) { enabled = a_enable; }
		bool IsEnabled() const { return enabled; }

		static std::uint32_t         EscapeKey();
		const IconFont::IconTexture* EscapeIcon() const;
		const IconFont::IconTexture* ToggleIcon() const;

		FUCK::ManagedHotkey& GetToggleHotkey() { return _toggleHotkey; }

		const HotkeyState _defToggle;

	private:
		bool                enabled = false;
		FUCK::ManagedHotkey _toggleHotkey;
	};
}
