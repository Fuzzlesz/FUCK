#pragma once

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

	struct RebindContext
	{
		bool   active{ false };
		bool   disallowModifiers{ false };
		double startTime{ 0.0 };

		std::uint32_t originalKey{ 0 };
		std::int32_t  originalMod1{ -1 };
		std::int32_t  originalMod2{ -1 };

		std::set<std::uint32_t> ignoredKeys;

		void Reset()
		{
			active            = false;
			disallowModifiers = false;
			startTime         = 0.0;
			originalKey       = 0;
			originalMod1      = -1;
			originalMod2      = -1;
			ignoredKeys.clear();
		}
	};

	class Manager : public REX::Singleton<Manager>
	{
	public:
		void ClearState();

		DEVICE GetInputDevice() const;
		bool   IsInputKBM() const;
		bool   IsInputGamepad() const;
		bool   CanNavigateWithMouse() const;

		static void ToggleCursor(bool a_enable);

		void ProcessInputEvents(RE::InputEvent* const* a_events);

		bool IsInputPressed(const RE::InputEvent* const* a_event, std::uint32_t a_unifiedKey);

		// --- Rebinding API ---
		void             StartBinding(std::uint32_t a_currentKey, std::int32_t a_currentMod1, std::int32_t a_currentMod2, bool a_disallowModifiers = false);
		FUCK::BindResult UpdateBinding(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2);
		bool             IsBinding() const { return _rebindCtx.active; }
		void             AbortBinding() { _rebindCtx.Reset(); }
		FUCK::BindResult GetInputBind(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2);

		bool UpdateManagedHotkey(const RE::InputEvent* const* a_event, FUCK::ManagedHotkey& h);
		bool ProcessManagedHotkey(const RE::InputEvent* const* a_event, FUCK::ManagedHotkey& h);
		bool IsManagedHotkeyDown(FUCK::ManagedHotkey& h);

		bool        IsInputDown(std::uint32_t a_unifiedKey) const;
		float       GetAnalogInput(std::uint32_t a_unifiedKey) const;
		bool        IsModifierPressed(FUCK::Modifier a_modifier) const;
		static bool IsUnifiedModifier(std::uint32_t a_unifiedKey);
		bool        IsCursorMovedByJoystick() const { return _cursorMovedByJoystick; }

	private:
		void UpdateInputDevice(RE::INPUT_DEVICE a_device);
		void CacheInputState(const RE::InputEvent* const* a_events);
		void UpdateCursorVisibility();
		void ForwardEventsToImGui(RE::InputEvent* const* a_events);

		bool CheckModifiersStrict(const std::uint32_t* mods, size_t count, std::int32_t req1, std::int32_t req2, std::uint32_t primaryKey) const;

		DEVICE _inputDevice{ DEVICE::kNone };
		DEVICE _lastInputDevice{ DEVICE::kNone };

		RebindContext _rebindCtx;

		bool _cursorShownByUs{ false };
		bool _cursorMovedByJoystick{ false };
		bool _expectingSyntheticMouseMove{ false };

		mutable std::shared_mutex _dataLock;
		Map<std::uint32_t, float> _keyStateCache;
	};
}
