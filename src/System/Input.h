#pragma once

#include "FUCK_API.h"
#include "InputMap.h"

#include <shared_mutex>

namespace Input
{
	enum class DEVICE
	{
		kNone,
		kKeyboard,
		kMouse,
		kGamepadDirectX,
		kGamepadOrbis
	};

	struct Context
	{
		std::string name;
		int priority{ 0 };
		bool blocksLower{ true };
	};

	struct RebindContext
	{
		bool active{ false };
		float timer{ 0.0f };
		std::uint32_t originalKey{ 0 };
		std::int32_t originalMod1{ -1 };
		std::int32_t originalMod2{ -1 };

		void Reset()
		{
			active = false;
			timer = 0.0f;
		}
	};

	class Manager : public REX::Singleton<Manager>
	{
	public:
		static void Register();

		void LoadSettings(const CSimpleIniA& a_ini);

		void ClearState(); 

		DEVICE	GetInputDevice() const;
		bool	IsInputKBM() const;
		bool	IsInputGamepad() const;
		bool	CanNavigateWithMouse() const;
		bool	DoNavigateWithMouse() const { return navigateWithMouse; }

		static void	ToggleCursor(bool a_enable);
		void		ResetCursorState();

		void ProcessInputEvents(RE::InputEvent* const* a_events);

		bool IsInputPressed(const RE::InputEvent* const* a_event, std::uint32_t a_unifiedKey);

		// --- Rebinding API ---
		void		StartBinding(std::uint32_t a_currentKey, std::int32_t a_currentMod1, std::int32_t a_currentMod2);
		BindResult	UpdateBinding(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2);
		bool		IsBinding() const { return _rebindCtx.active; }
		void		AbortBinding() { _rebindCtx.Reset(); }
		BindResult	GetInputBind(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2);

		bool	IsInputDown(std::uint32_t a_unifiedKey) const;
		float	GetAnalogInput(std::uint32_t a_unifiedKey) const;
		bool	IsModifierPressed(Modifier a_modifier) const;

		void PushContext(Context a_ctx);
		void PopContext(std::string_view a_name);
		bool IsContextActive(std::string_view a_name) const;
		bool ShouldBlockLowerContexts() const;

		const char* GetKeyName(std::uint32_t a_key) const;

	private:
		void UpdateInputDevice(RE::INPUT_DEVICE a_device);
		void CacheInputState(const RE::InputEvent* const* a_events);

		DEVICE inputDevice{ DEVICE::kNone };
		DEVICE lastInputDevice{ DEVICE::kNone };

		RebindContext _rebindCtx;

		bool navigateWithMouse{ true };

		std::optional<bool> cursorInit{ std::nullopt };

		mutable std::shared_mutex _dataLock;
		Map<std::uint32_t, float> keyStateCache;

		std::vector<Context> contexts;
	};
}
