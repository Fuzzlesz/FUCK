#include "Input.h"

#include "FUCKMan.h"
#include "Hotkeys.h"

namespace Input
{
	// Custom offsets for Stick Axis caching
	constexpr uint32_t CUSTOM_LEFT_STICK_X = 32;
	constexpr uint32_t CUSTOM_LEFT_STICK_Y = 33;
	constexpr uint32_t CUSTOM_RIGHT_STICK_X = 34;
	constexpr uint32_t CUSTOM_RIGHT_STICK_Y = 35;

	void Manager::Register()
	{
		logger::info("Input Manager Initialized");
	}

	void Manager::ClearState()
	{
		std::unique_lock lock(_dataLock);
		keyStateCache.clear();
		_rebindCtx.Reset();
	}

	DEVICE Manager::GetInputDevice() const
	{
		return inputDevice;
	}

	bool Manager::IsInputKBM() const
	{
		return inputDevice == DEVICE::kKeyboard || inputDevice == DEVICE::kMouse;
	}

	bool Manager::IsInputGamepad() const
	{
		return inputDevice == DEVICE::kGamepadDirectX || inputDevice == DEVICE::kGamepadOrbis;
	}

	bool Manager::CanNavigateWithMouse() const
	{
		return IsInputKBM();
	}

	void Manager::ToggleCursor(bool a_enable)
	{
		SKSE::GetTaskInterface()->AddUITask([a_enable]() {
			RE::UIMessageQueue::GetSingleton()->AddMessage(RE::CursorMenu::MENU_NAME,
				a_enable ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide,
				nullptr);
		});
	}

	void Manager::ResetCursorState()
	{
		cursorInit = std::nullopt;
	}

	bool Manager::IsInputPressed(const RE::InputEvent* const* a_event, std::uint32_t a_unifiedKey)
	{
		if (!a_event) {
			return false;
		}

		for (auto event = *a_event; event; event = event->next) {
			const auto button = event->AsButtonEvent();
			if (!button || !button->HasIDCode() || !button->IsPressed())
				continue;

			auto key = button->GetIDCode();

			switch (button->GetDevice()) {
			case RE::INPUT_DEVICE::kMouse:
				key += SKSE::InputMap::kMacro_MouseButtonOffset;
				break;
			case RE::INPUT_DEVICE::kGamepad:
				key = SKSE::InputMap::GamepadMaskToKeycode(key);
				key += SKSE::InputMap::kMacro_GamepadOffset;
				break;
			default:
				break;
			}

			if (key == a_unifiedKey)
				return true;
		}
		return false;
	}

	bool Manager::IsInputDown(std::uint32_t a_unifiedKey) const
	{
		std::shared_lock lock(_dataLock);
		auto it = keyStateCache.find(a_unifiedKey);
		return it != keyStateCache.end() && it->second > 0.0f;
	}

	void Manager::StartBinding(std::uint32_t k, std::int32_t m1, std::int32_t m2)
	{
		_rebindCtx.active = true;
		_rebindCtx.timer = 0.0f;
		_rebindCtx.originalKey = k;
		_rebindCtx.originalMod1 = m1;
		_rebindCtx.originalMod2 = m2;
	}

	BindResult Manager::UpdateBinding(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2)
	{
		if (!_rebindCtx.active)
			return BindResult::kNone;

		// 1. Update debounce timer (using ImGui's DeltaTime is easiest here)
		_rebindCtx.timer += ImGui::GetIO().DeltaTime;

		// 2. Poll the raw input
		std::uint32_t newKey = 0;
		std::int32_t newM1 = -1;
		std::int32_t newM2 = -1;

		auto result = GetInputBind(a_event, &newKey, &newM1, &newM2);

		// 3. Prevent accidental "double-click" capture
		if (result == BindResult::kBound && _rebindCtx.timer < 0.2f) {
			return BindResult::kNone;
		}

		// 4. Handle Results
		if (result == BindResult::kCancelled) {
			// AUTOMATIC RESTORE
			if (outKey)
				*outKey = _rebindCtx.originalKey;
			if (outMod1)
				*outMod1 = _rebindCtx.originalMod1;
			if (outMod2)
				*outMod2 = _rebindCtx.originalMod2;
			_rebindCtx.Reset();
			return BindResult::kCancelled;
		}

		if (result == BindResult::kBound) {
			if (outKey)
				*outKey = newKey;
			if (outMod1)
				*outMod1 = newM1;
			if (outMod2)
				*outMod2 = newM2;
			_rebindCtx.Reset();
			return BindResult::kBound;
		}

		return BindResult::kNone;
	}

	BindResult Manager::GetInputBind(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2)
	{
		if (!a_event)
			return BindResult::kNone;

		const auto gpOffset = static_cast<std::uint32_t>(SKSE::InputMap::kMacro_GamepadOffset);
		const auto msOffset = static_cast<std::uint32_t>(SKSE::InputMap::kMacro_MouseButtonOffset);

		const uint32_t lb = gpOffset + static_cast<uint32_t>(SKSE::InputMap::kGamepadButtonOffset_LEFT_SHOULDER);
		const uint32_t rb = gpOffset + static_cast<uint32_t>(SKSE::InputMap::kGamepadButtonOffset_RIGHT_SHOULDER);
		const uint32_t lt = gpOffset + static_cast<uint32_t>(SKSE::InputMap::kGamepadButtonOffset_LT);
		const uint32_t rt = gpOffset + static_cast<uint32_t>(SKSE::InputMap::kGamepadButtonOffset_RT);

		for (auto event = *a_event; event; event = event->next) {
			auto button = event->AsButtonEvent();
			if (!button || !button->HasIDCode() || button->Value() <= 0.0f)
				continue;

			auto key = button->GetIDCode();
			auto device = button->GetDevice();
			uint32_t unifiedKey = key;

			if (device == RE::INPUT_DEVICE::kMouse) {
				unifiedKey += msOffset;
			} else if (device == RE::INPUT_DEVICE::kGamepad) {
				unifiedKey = SKSE::InputMap::GamepadMaskToKeycode(key) + gpOffset;
			}

			// BLOCKERS
			if (device == RE::INPUT_DEVICE::kMouse && (key == 0 || key == 1))
				return BindResult::kNone;
			if (device == RE::INPUT_DEVICE::kKeyboard && (key == KEY::kLeftWin || key == KEY::kRightWin))
				return BindResult::kError;

			if (unifiedKey == Hotkeys::Manager::EscapeKey()) {
				RE::PlaySound("UIMenuCancel");
				return BindResult::kCancelled;
			}

			// MODIFIER COLLEC
			std::vector<int32_t> pressedMods;
			if (device == RE::INPUT_DEVICE::kKeyboard) {
				// Check specific sides
				if (IsInputDown(static_cast<uint32_t>(KEY::kLeftShift)))
					pressedMods.push_back(static_cast<int32_t>(KEY::kLeftShift));
				if (IsInputDown(static_cast<uint32_t>(KEY::kRightShift)))
					pressedMods.push_back(static_cast<int32_t>(KEY::kRightShift));

				if (IsInputDown(static_cast<uint32_t>(KEY::kLeftControl)))
					pressedMods.push_back(static_cast<int32_t>(KEY::kLeftControl));
				if (IsInputDown(static_cast<uint32_t>(KEY::kRightControl)))
					pressedMods.push_back(static_cast<int32_t>(KEY::kRightControl));

				if (IsInputDown(static_cast<uint32_t>(KEY::kLeftAlt)))
					pressedMods.push_back(static_cast<int32_t>(KEY::kLeftAlt));
				if (IsInputDown(static_cast<uint32_t>(KEY::kRightAlt)))
					pressedMods.push_back(static_cast<int32_t>(KEY::kRightAlt));
			} else if (device == RE::INPUT_DEVICE::kGamepad) {
				if (IsInputDown(lb))
					pressedMods.push_back(static_cast<int32_t>(lb));
				if (IsInputDown(rb))
					pressedMods.push_back(static_cast<int32_t>(rb));
				if (GetAnalogInput(lt) > 0.15f)
					pressedMods.push_back(static_cast<int32_t>(lt));
				if (GetAnalogInput(rt) > 0.15f)
					pressedMods.push_back(static_cast<int32_t>(rt));
			}

			// MODIFIER PROTEC
			bool isModCandidate = (unifiedKey == lb || unifiedKey == rb || unifiedKey == lt || unifiedKey == rt ||
								   unifiedKey == (uint32_t)KEY::kLeftShift || unifiedKey == (uint32_t)KEY::kRightShift ||
								   unifiedKey == (uint32_t)KEY::kLeftControl || unifiedKey == (uint32_t)KEY::kRightControl ||
								   unifiedKey == (uint32_t)KEY::kLeftAlt || unifiedKey == (uint32_t)KEY::kRightAlt);

			if (isModCandidate) {
				if (button->Value() < 1.0f || !pressedMods.empty())
					return BindResult::kNone;
			}

			// PREVENT SELF-BIND
			for (auto m : pressedMods) {
				uint32_t uM = static_cast<uint32_t>(m);
				bool match = (unifiedKey == uM);
				if (!match && device == RE::INPUT_DEVICE::kKeyboard) {
					// Check Right-sided pairs
					if (uM == (uint32_t)KEY::kLeftShift && unifiedKey == (uint32_t)KEY::kRightShift)
						match = true;
					if (uM == (uint32_t)KEY::kLeftControl && unifiedKey == (uint32_t)KEY::kRightControl)
						match = true;
					if (uM == (uint32_t)KEY::kLeftAlt && unifiedKey == (uint32_t)KEY::kRightAlt)
						match = true;
				}
				if (match)
					return BindResult::kNone;
			}

			// OUT
			if (outKey)
				*outKey = unifiedKey;
			if (outMod1)
				*outMod1 = (pressedMods.size() > 0) ? pressedMods[0] : -1;
			if (outMod2)
				*outMod2 = (pressedMods.size() > 1) ? pressedMods[1] : -1;

			return BindResult::kBound;
		}
		return BindResult::kNone;
	}

	float Manager::GetAnalogInput(std::uint32_t a_unifiedKey) const
	{
		std::shared_lock lock(_dataLock);
		auto it = keyStateCache.find(a_unifiedKey);
		return it != keyStateCache.end() ? it->second : 0.0f;
	}

	bool Manager::IsModifierPressed(Modifier a_modifier) const
	{
		switch (a_modifier) {
		case Modifier::kShift:
			return IsInputDown(KEY::kLeftShift) || IsInputDown(KEY::kRightShift);
		case Modifier::kCtrl:
			return IsInputDown(KEY::kLeftControl) || IsInputDown(KEY::kRightControl);
		case Modifier::kAlt:
			return IsInputDown(KEY::kLeftAlt) || IsInputDown(KEY::kRightAlt);
		default:
			return false;
		}
	}

	// ==========================================
	// Context Management
	// ==========================================

	void Manager::PushContext(Context a_ctx)
	{
		contexts.push_back(std::move(a_ctx));
		std::sort(contexts.begin(), contexts.end(),
			[](const Context& a, const Context& b) { return a.priority > b.priority; });
	}

	void Manager::PopContext(std::string_view a_name)
	{
		std::erase_if(contexts, [&](const Context& c) { return c.name == a_name; });
	}

	bool Manager::IsContextActive(std::string_view a_name) const
	{
		return std::ranges::any_of(contexts, [&](const Context& c) { return c.name == a_name; });
	}

	bool Manager::ShouldBlockLowerContexts() const
	{
		return !contexts.empty() && contexts.front().blocksLower;
	}

	// ==========================================
	// Key Names
	// ==========================================

	const char* Manager::GetKeyName(std::uint32_t a_key) const
	{
		// Basic lookup for common keys
		static std::unordered_map<uint32_t, const char*> keyNames = {
			{ KEY::kEscape, "Esc" }, { KEY::kEnter, "Enter" }, { KEY::kSpacebar, "Space" },
			{ KEY::kLeftShift, "LShift" }, { KEY::kRightShift, "RShift" },
			{ KEY::kLeftControl, "LCtrl" }, { KEY::kRightControl, "RCtrl" },
			{ KEY::kLeftAlt, "LAlt" }, { KEY::kRightAlt, "RAlt" },
			{ KEY::kTab, "Tab" }, { KEY::kCapsLock, "Caps" },
			{ KEY::kLeft, "Left" }, { KEY::kRight, "Right" }, { KEY::kUp, "Up" }, { KEY::kDown, "Down" },
			{ KEY::kBackspace, "Backspace" },
			{ KEY::kF5, "F5" }, { KEY::kF9, "F9" }, { KEY::kF12, "F12" }
		};

		// Mouse
		if (a_key >= SKSE::InputMap::kMacro_MouseButtonOffset && a_key < SKSE::InputMap::kMacro_MouseButtonOffset + 10) {
			static const char* mouseNames[] = { "Mouse1", "Mouse2", "Mouse3", "Mouse4", "Mouse5", "Mouse6", "Mouse7", "Mouse8" };
			uint32_t idx = a_key - SKSE::InputMap::kMacro_MouseButtonOffset;
			if (idx < 8)
				return mouseNames[idx];
		}

		// Gamepad
		if (a_key >= SKSE::InputMap::kMacro_GamepadOffset) {
			return "Gamepad";
		}

		auto it = keyNames.find(a_key);
		if (it != keyNames.end())
			return it->second;

		// Simple fallback
		return "Key";
	}

	// ==========================================
	// Processing
	// ==========================================

	void Manager::CacheInputState(const RE::InputEvent* const* a_events)
	{
		std::unique_lock lock(_dataLock);

		for (auto event = *a_events; event; event = event->next) {
			if (auto button = event->AsButtonEvent()) {
				if (button->HasIDCode()) {
					auto key = button->GetIDCode();
					switch (button->GetDevice()) {
					case RE::INPUT_DEVICE::kMouse:
						key += static_cast<std::uint32_t>(SKSE::InputMap::kMacro_MouseButtonOffset);
						break;
					case RE::INPUT_DEVICE::kGamepad:
						key = SKSE::InputMap::GamepadMaskToKeycode(key);
						key += static_cast<std::uint32_t>(SKSE::InputMap::kMacro_GamepadOffset);
						break;
					default:
						break;
					}

					if (button->IsHeld() || button->IsPressed()) {
						keyStateCache[key] = button->Value();
					} else if (button->IsUp()) {
						keyStateCache.erase(key);
					}
				}
			} else if (auto stick = event->AsThumbstickEvent()) {
				uint32_t id = stick->GetIDCode();
				uint32_t baseOffset = SKSE::InputMap::kMacro_GamepadOffset;

				if (id == 0x0B) {  // Left Stick
					keyStateCache[baseOffset + CUSTOM_LEFT_STICK_X] = stick->xValue;
					keyStateCache[baseOffset + CUSTOM_LEFT_STICK_Y] = stick->yValue;
				} else if (id == 0x0C) {  // Right Stick
					keyStateCache[baseOffset + CUSTOM_RIGHT_STICK_X] = stick->xValue;
					keyStateCache[baseOffset + CUSTOM_RIGHT_STICK_Y] = stick->yValue;
				}
			}
		}
	}

	void Manager::UpdateInputDevice(RE::INPUT_DEVICE a_device)
	{
		lastInputDevice = inputDevice;

		switch (a_device) {
		case RE::INPUT_DEVICE::kKeyboard:
			inputDevice = DEVICE::kKeyboard;
			break;
		case RE::INPUT_DEVICE::kMouse:
			inputDevice = DEVICE::kMouse;
			break;
		case RE::INPUT_DEVICE::kGamepad:
			if (RE::ControlMap::GetSingleton()->GetGamePadType() == RE::PC_GAMEPAD_TYPE::kOrbis)
				inputDevice = DEVICE::kGamepadOrbis;
			else
				inputDevice = DEVICE::kGamepadDirectX;
			break;
		default:
			break;
		}

		if (lastInputDevice == DEVICE::kNone || inputDevice == DEVICE::kNone || lastInputDevice != inputDevice) {
			auto& io = ImGui::GetIO();
			io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
			io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;

			if (IsInputGamepad()) {
				io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
				io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;  // unused flag to force ImGui to update gamepad input from backend
			} else {
				if (IsInputKBM()) {
					io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
				}
			}
		}
	}

	void Manager::ProcessInputEvents(RE::InputEvent* const* a_events)
	{
		CacheInputState(a_events);

		for (auto event = *a_events; event; event = event->next) {
			UpdateInputDevice(event->GetDevice());
		}

		// -- Cursor Visibility Logic --
		const auto fuck = FUCKMan::GetSingleton();
		const bool blockInput = fuck->IsInputBlocked();
		const bool forceCursor = fuck->IsCursorForced();
		const bool menuOpen = fuck->IsOpen();

		bool shouldShowCursor = forceCursor || menuOpen;
		if (!shouldShowCursor && blockInput) {
			if (CanNavigateWithMouse() || IsInputGamepad()) {
				shouldShowCursor = true;
			}
		}

		if (shouldShowCursor) {
			if (auto ui = RE::UI::GetSingleton(); !ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
				cursorInit = std::nullopt;
			}
		}

		if (!cursorInit.has_value() || shouldShowCursor != cursorInit.value()) {
			ToggleCursor(shouldShowCursor);
			cursorInit = shouldShowCursor;
		}

		// -- Event Forwarding --
		auto& io = ImGui::GetIO();
		const bool cursorMenuOpen = RE::UI::GetSingleton()->IsMenuOpen(RE::CursorMenu::MENU_NAME);
		const bool shouldPassToImGui = fuck->ShouldRender();

		for (auto event = *a_events; event; event = event->next) {
			if (const auto charEvent = event->AsCharEvent()) {
				if (shouldPassToImGui) {
					io.AddInputCharacter(charEvent->keyCode);
				}
			} else if (const auto buttonEvent = event->AsButtonEvent()) {
				const auto key = buttonEvent->GetIDCode();
				const bool isPressed = buttonEvent->IsPressed();
				const float value = buttonEvent->Value();

				if (shouldPassToImGui) {
					switch (inputDevice) {
					case DEVICE::kKeyboard:
						io.AddKeyEvent(Keymap::ToImGuiKey(static_cast<KEY>(key)), isPressed);
						break;
					case DEVICE::kMouse:
						switch (auto mouseKey = static_cast<MOUSE>(key)) {
						case MOUSE::kWheelUp:
							io.AddMouseWheelEvent(0, value);
							break;
						case MOUSE::kWheelDown:
							io.AddMouseWheelEvent(0, value * -1);
							break;
						default:
							io.AddMouseButtonEvent(key, isPressed);
							break;
						}
						break;
					case DEVICE::kGamepadDirectX:
						{
							auto [imKey, analog] = Keymap::ToImGuiKey(static_cast<GAMEPAD_DIRECTX>(key));
							if (analog)
								io.AddKeyAnalogEvent(imKey, isPressed, value);
							else
								io.AddKeyEvent(imKey, isPressed);
						}
						break;
					case DEVICE::kGamepadOrbis:
						{
							auto [imKey, analog] = Keymap::ToImGuiKey(static_cast<GAMEPAD_ORBIS>(key));
							if (analog)
								io.AddKeyAnalogEvent(imKey, isPressed, value);
							else
								io.AddKeyEvent(imKey, isPressed);
						}
						break;
					default:
						break;
					}
				}
			}
			else if (blockInput || shouldPassToImGui) {
				if (auto mouseEvent = event->AsMouseMoveEvent()) {
					if (auto cursorMenu = RE::UI::GetSingleton()->GetMenu<RE::CursorMenu>()) {
						cursorMenu->ProcessMouseMove(mouseEvent);
					}
				} else if (const auto thumbstickEvent = event->AsThumbstickEvent()) {
					if (cursorMenuOpen) {
						if (auto cursorMenu = RE::UI::GetSingleton()->GetMenu<RE::CursorMenu>()) {
							cursorMenu->ProcessThumbstick(thumbstickEvent);
						}
					}
				}
			}
		}
	}
}
