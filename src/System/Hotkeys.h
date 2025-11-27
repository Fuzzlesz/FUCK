#pragma once

#include "FUCK_API.h"

namespace IconFont
{
	struct IconTexture;
}

namespace Hotkeys
{
	class Manager : public REX::Singleton<Manager>
	{
	public:
		void	LoadHotKeys(const CSimpleIniA& a_ini);
		void	SaveHotKeys(CSimpleIniA& a_ini);
		void	ProcessInput(const RE::InputEvent* const* a_event);
		void	Enable(bool a_enable) { enabled = a_enable; }
		bool	IsEnabled() const { return enabled; }

		static std::uint32_t			EscapeKey();
		const IconFont::IconTexture*	EscapeIcon() const;
		const IconFont::IconTexture*	ToggleIcon() const;

		FUCK::ManagedHotkey& GetToggleHotkey() { return _toggleHotkey; }

	private:
		bool enabled = false;
		FUCK::ManagedHotkey _toggleHotkey;
	};
}
