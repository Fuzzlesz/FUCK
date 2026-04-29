#include "Input.h"

#include "FUCKMan.h"
#include "Hotkeys.h"

namespace Input
{
	using namespace Input::Keymap;

	// Custom offsets for Stick Axis caching
	constexpr uint32_t CUSTOM_LEFT_STICK_X = 32;
	constexpr uint32_t CUSTOM_LEFT_STICK_Y = 33;
	constexpr uint32_t CUSTOM_RIGHT_STICK_X = 34;
	constexpr uint32_t CUSTOM_RIGHT_STICK_Y = 35;

	// Global Modifier Definitions for unified checks
	static const std::uint32_t KB_MODS[] = {
		AsKey(KEY::kLeftShift),   AsKey(KEY::kRightShift),
		AsKey(KEY::kLeftControl), AsKey(KEY::kRightControl),
		AsKey(KEY::kLeftAlt),     AsKey(KEY::kRightAlt)
	};
	static const std::uint32_t GP_MODS[] = {
		kGPBase + SKSE::InputMap::kGamepadButtonOffset_LEFT_SHOULDER,
		kGPBase + SKSE::InputMap::kGamepadButtonOffset_RIGHT_SHOULDER,
		kGPBase + SKSE::InputMap::kGamepadButtonOffset_LT,
		kGPBase + SKSE::InputMap::kGamepadButtonOffset_RT
	};

	static constexpr std::uint32_t kGP_LT = kGPBase + SKSE::InputMap::kGamepadButtonOffset_LT;
	static constexpr std::uint32_t kGP_RT = kGPBase + SKSE::InputMap::kGamepadButtonOffset_RT;

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
		return IsInputKBM() || RE::UI::GetSingleton()->IsMenuOpen(RE::CursorMenu::MENU_NAME);
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
			uint32_t unifiedKey = Keymap::GetUnifiedKey(button->GetDevice(), key);

			if (unifiedKey == a_unifiedKey)
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

	FUCK::BindResult Manager::UpdateBinding(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2)
	{
		if (!_rebindCtx.active)
			return FUCK::BindResult::kNone;

		// 1. Update debounce timer (using ImGui's DeltaTime is easiest here)
		_rebindCtx.timer += ImGui::GetIO().DeltaTime;

		// 2. Poll the raw input
		std::uint32_t newKey = 0;
		std::int32_t newM1 = -1;
		std::int32_t newM2 = -1;

		auto result = GetInputBind(a_event, &newKey, &newM1, &newM2);

		// 3. Prevent accidental "double-click" capture
		if (result == FUCK::BindResult::kBound && _rebindCtx.timer < 0.2f) {
			return FUCK::BindResult::kNone;
		}

		// 4. Handle Results
		if (result == FUCK::BindResult::kCancelled) {
			// AUTOMATIC RESTORE
			if (outKey)
				*outKey = _rebindCtx.originalKey;
			if (outMod1)
				*outMod1 = _rebindCtx.originalMod1;
			if (outMod2)
				*outMod2 = _rebindCtx.originalMod2;
			_rebindCtx.Reset();
			return FUCK::BindResult::kCancelled;
		}

		if (result == FUCK::BindResult::kBound) {
			if (outKey)
				*outKey = newKey;
			if (outMod1)
				*outMod1 = newM1;
			if (outMod2)
				*outMod2 = newM2;
			_rebindCtx.Reset();
			return FUCK::BindResult::kBound;
		}

		return FUCK::BindResult::kNone;
	}

	FUCK::BindResult Manager::GetInputBind(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2)
	{
		if (!a_event)
			return FUCK::BindResult::kNone;

		for (auto event = *a_event; event; event = event->next) {
			auto button = event->AsButtonEvent();
			if (!button || !button->HasIDCode() || button->Value() <= 0.0f)
				continue;

			auto key = button->GetIDCode();
			auto device = button->GetDevice();
			uint32_t unifiedKey = Keymap::GetUnifiedKey(device, key);

			// BLOCKERS
			if (device == RE::INPUT_DEVICE::kMouse && (key == static_cast<uint32_t>(MOUSE::kLeftButton) || key == static_cast<uint32_t>(MOUSE::kRightButton)))
				return FUCK::BindResult::kNone;
			if (device == RE::INPUT_DEVICE::kKeyboard && (key == KEY::kLeftWin || key == KEY::kRightWin))
				return FUCK::BindResult::kNone;

			if (unifiedKey == Hotkeys::Manager::EscapeKey()) {
				RE::PlaySound("UIMenuCancel");
				return FUCK::BindResult::kCancelled;
			}

			// MODIFIER COLLEC
			std::vector<int32_t> pressedMods;
			if (device == RE::INPUT_DEVICE::kKeyboard) {
				for (auto m : KB_MODS) {
					if (IsInputDown(m))
						pressedMods.push_back(m);
				}
			} else if (device == RE::INPUT_DEVICE::kGamepad) {
				for (auto m : GP_MODS) {
					// LT and RT are analog — check threshold rather than digital state
					if (m == kGP_LT || m == kGP_RT) {
						if (GetAnalogInput(m) > 0.15f)
							pressedMods.push_back(m);
					} else {
						if (IsInputDown(m))
							pressedMods.push_back(m);
					}
				}
			}

			// MODIFIER PROTEC
			if (IsUnifiedModifier(unifiedKey)) {
				if (button->Value() < 1.0f || !pressedMods.empty())
					return FUCK::BindResult::kNone;
			}

			// PREVENT SELF-BIND
			auto IsSameModPair = [](uint32_t a, uint32_t b) {
				if (a > b)
					std::swap(a, b);
				return a == b ||
				       (a == AsKey(KEY::kLeftShift) && b ==		AsKey(KEY::kRightShift)) ||
				       (a == AsKey(KEY::kLeftControl) && b ==	AsKey(KEY::kRightControl)) ||
				       (a == AsKey(KEY::kLeftAlt) && b ==		AsKey(KEY::kRightAlt));
			};

			if (std::ranges::any_of(pressedMods, [&](int32_t m) { return IsSameModPair(m, unifiedKey); })) {
				return FUCK::BindResult::kNone;
			}

			// OUT
			if (outKey)
				*outKey = unifiedKey;
			if (outMod1)
				*outMod1 = (pressedMods.size() > 0) ? pressedMods[0] : -1;
			if (outMod2)
				*outMod2 = (pressedMods.size() > 1) ? pressedMods[1] : -1;

			return FUCK::BindResult::kBound;
		}
		return FUCK::BindResult::kNone;
	}

	bool Manager::UpdateManagedHotkey(const RE::InputEvent* const* a_event, FUCK::ManagedHotkey& h)
	{
		if (!h.isBinding)
			return false;
		if (!IsBinding()) {
			h.isBinding = false;
			return false;
		}

		std::uint32_t k;
		std::int32_t m1, m2;
		FUCK::BindResult res = UpdateBinding(a_event, &k, &m1, &m2);

		if (res == FUCK::BindResult::kBound) {
			if (k >= Keymap::kGPBase) {
				h.gKey = k;
				h.gMod1 = m1;
				h.gMod2 = m2;
			} else {
				h.kKey = k;
				h.kMod1 = m1;
				h.kMod2 = m2;
			}
			h.isBinding = false;
			h.wasTriggered = true;
			return true;
		} else if (res == FUCK::BindResult::kCancelled) {
			h.isBinding = false;
			return true;
		}
		return true;
	}

	bool Manager::ProcessManagedHotkey(const RE::InputEvent* const* a_event, FUCK::ManagedHotkey& h)
	{
		if (h.isBinding)
			return false;

		auto checkMod = [this](std::int32_t mod) -> bool {
			if (mod <= 0)
				return false;

			std::uint32_t umod = static_cast<std::uint32_t>(mod);
			if (umod == kGP_LT || umod == kGP_RT)
				return GetAnalogInput(umod) > 0.15f;

			return IsInputDown(umod);
		};

		auto checkStrict = [&](const std::uint32_t* mods, size_t count, std::int32_t req1, std::int32_t req2) {
			for (size_t i = 0; i < count; ++i) {
				std::int32_t currentMod = static_cast<std::int32_t>(mods[i]);
				if (checkMod(currentMod) != (currentMod == req1 || currentMod == req2))
					return false;
			}
			return true;
		};

		bool pressed = false;

		if (h.kKey != 0 && IsInputPressed(a_event, h.kKey)) {
			if (checkStrict(KB_MODS, std::size(KB_MODS), h.kMod1, h.kMod2))
				pressed = true;
		}

		if (!pressed && h.gKey != 0 && IsInputPressed(a_event, h.gKey)) {
			if (checkStrict(GP_MODS, std::size(GP_MODS), h.gMod1, h.gMod2))
				pressed = true;
		}

		if (pressed) {
			if (!h.wasTriggered) {
				h.wasTriggered = true;
				return true;
			}
		} else {
			h.wasTriggered = false;
		}
		return false;
	}

	float Manager::GetAnalogInput(std::uint32_t a_unifiedKey) const
	{
		std::shared_lock lock(_dataLock);
		auto it = keyStateCache.find(a_unifiedKey);
		return it != keyStateCache.end() ? it->second : 0.0f;
	}

	bool Manager::IsModifierPressed(FUCK::Modifier a_modifier) const
	{
		switch (a_modifier) {
			case FUCK::Modifier::kShift:
				return IsInputDown(AsKey(KEY::kLeftShift))
					|| IsInputDown(AsKey(KEY::kRightShift));
			case FUCK::Modifier::kCtrl:
				return IsInputDown(AsKey(KEY::kLeftControl))
					|| IsInputDown(AsKey(KEY::kRightControl));
			case FUCK::Modifier::kAlt:
				return IsInputDown(AsKey(KEY::kLeftAlt))
					|| IsInputDown(AsKey(KEY::kRightAlt));
			default:
				return false;
		}
	}

	bool Manager::IsUnifiedModifier(std::uint32_t a_unifiedKey)
	{
		return std::ranges::contains(KB_MODS, a_unifiedKey) || std::ranges::contains(GP_MODS, a_unifiedKey);
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
		// Letters — QWERTY row ranges (DirectInput scancode order, not alphabetical)
		if (a_key >= AsKey(KEY::kQ) && a_key <= AsKey(KEY::kP)) {
			static const char* row1[] = { "Q","W","E","R","T","Y","U","I","O","P" };
			return row1[a_key - AsKey(KEY::kQ)];
		}
		if (a_key >= AsKey(KEY::kA) && a_key <= AsKey(KEY::kL)) {
			static const char* row2[] = { "A","S","D","F","G","H","J","K","L" };
			return row2[a_key - AsKey(KEY::kA)];
		}
		if (a_key >= AsKey(KEY::kZ) && a_key <= AsKey(KEY::kM)) {
			static const char* row3[] = { "Z","X","C","V","B","N","M" };
			return row3[a_key - AsKey(KEY::kZ)];
		}

		// Numbers — scancode order is 1-9 then 0 (0x02-0x0B)
		if (a_key >= AsKey(KEY::kNum1) && a_key <= AsKey(KEY::kNum0)) {
			static const char* nums[] = { "1","2","3","4","5","6","7","8","9","0" };
			return nums[a_key - AsKey(KEY::kNum1)];
		}

		// F-keys — F11/F12 (0x57-0x58) are not contiguous with F1-F10 (0x3B-0x44)
		if (a_key >= AsKey(KEY::kF1) && a_key <= AsKey(KEY::kF10)) {
			static const char* fkeys[] = { "F1","F2","F3","F4","F5","F6","F7","F8","F9","F10" };
			return fkeys[a_key - AsKey(KEY::kF1)];
		}
		if (a_key >= AsKey(KEY::kF11) && a_key <= AsKey(KEY::kF12)) {
			static const char* fkeys[] = { "F11","F12" };
			return fkeys[a_key - AsKey(KEY::kF11)];
		}

		// Mouse
		if (a_key >= kMBBase && a_key < kMBBase + 8) {
			static const char* mouse[] = {
				"Mouse1","Mouse2","Mouse3","Mouse4","Mouse5","Mouse6","Mouse7","Mouse8"
			};
			return mouse[a_key - kMBBase];
		}

		// Gamepad
		if (a_key >= kGPBase) {
			static const Map<uint32_t, const char*> gp = {
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_A,              "A"      },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_B,              "B"      },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_X,              "X"      },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_Y,              "Y"      },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_LEFT_SHOULDER,  "LB"     },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_RIGHT_SHOULDER, "RB"     },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_LT,             "LT"     },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_RT,             "RT"     },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_LEFT_THUMB,     "LS"     },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_RIGHT_THUMB,    "RS"     },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_START,          "Start"  },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_BACK,           "Back"   },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_UP,        "DUp"    },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_DOWN,      "DDown"  },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_LEFT,      "DLeft"  },
				{ kGPBase + SKSE::InputMap::kGamepadButtonOffset_DPAD_RIGHT,     "DRight" },
			};
			auto it = gp.find(a_key);
			return it != gp.end() ? it->second : "GP?";
		}

		// Everything else — irregular keyboard keys + all KP digits (non-contiguous)
		static const Map<uint32_t, const char*> kb = {
			{ AsKey(KEY::kEscape),      "Esc"       }, { AsKey(KEY::kEnter),        "Enter"    },
			{ AsKey(KEY::kSpacebar),    "Space"     }, { AsKey(KEY::kTab),          "Tab"      },
			{ AsKey(KEY::kBackspace),   "Backspace" }, { AsKey(KEY::kCapsLock),     "Caps"     },
			{ AsKey(KEY::kLeftShift),   "LShift"    }, { AsKey(KEY::kRightShift),   "RShift"   },
			{ AsKey(KEY::kLeftControl), "LCtrl"     }, { AsKey(KEY::kRightControl), "RCtrl"    },
			{ AsKey(KEY::kLeftAlt),     "LAlt"      }, { AsKey(KEY::kRightAlt),     "RAlt"     },
			{ AsKey(KEY::kLeft),        "Left"      }, { AsKey(KEY::kRight),        "Right"    },
			{ AsKey(KEY::kUp),          "Up"        }, { AsKey(KEY::kDown),         "Down"     },
			{ AsKey(KEY::kHome),        "Home"      }, { AsKey(KEY::kEnd),          "End"      },
			{ AsKey(KEY::kPageUp),      "PgUp"      }, { AsKey(KEY::kPageDown),     "PgDn"     },
			{ AsKey(KEY::kInsert),      "Insert"    }, { AsKey(KEY::kDelete),       "Delete"   },
			{ AsKey(KEY::kPrintScreen), "PrtSc"     }, { AsKey(KEY::kScrollLock),   "ScrLk"    },
			{ AsKey(KEY::kPause),       "Pause"     }, { AsKey(KEY::kNumLock),      "NumLk"    },
			{ AsKey(KEY::kMinus),       "-"         }, { AsKey(KEY::kEquals),       "="        },
			{ AsKey(KEY::kBracketLeft), "["         }, { AsKey(KEY::kBracketRight), "]"        },
			{ AsKey(KEY::kBackslash),   "\\"        }, { AsKey(KEY::kSemicolon),    ";"        },
			{ AsKey(KEY::kApostrophe),  "'"         }, { AsKey(KEY::kComma),        ","        },
			{ AsKey(KEY::kPeriod),      "."         }, { AsKey(KEY::kSlash),        "/"        },
			{ AsKey(KEY::kTilde),       "`"         },
			{ AsKey(KEY::kKP_0),        "KP0"       }, { AsKey(KEY::kKP_1),        "KP1"      },
			{ AsKey(KEY::kKP_2),        "KP2"       }, { AsKey(KEY::kKP_3),        "KP3"      },
			{ AsKey(KEY::kKP_4),        "KP4"       }, { AsKey(KEY::kKP_5),        "KP5"      },
			{ AsKey(KEY::kKP_6),        "KP6"       }, { AsKey(KEY::kKP_7),        "KP7"      },
			{ AsKey(KEY::kKP_8),        "KP8"       }, { AsKey(KEY::kKP_9),        "KP9"      },
			{ AsKey(KEY::kKP_Decimal),  "KP."       }, { AsKey(KEY::kKP_Divide),   "KP/"      },
			{ AsKey(KEY::kKP_Multiply), "KP*"       }, { AsKey(KEY::kKP_Subtract), "KP-"      },
			{ AsKey(KEY::kKP_Plus),     "KP+"       }, { AsKey(KEY::kKP_Enter),    "KPEnter"  },
		};
		auto it = kb.find(a_key);
		return it != kb.end() ? it->second : "?";
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
					uint32_t unifiedKey = Keymap::GetUnifiedKey(button->GetDevice(), key);

					if (button->IsHeld() || button->IsPressed()) {
						keyStateCache[unifiedKey] = button->Value();
					} else if (button->IsUp()) {
						keyStateCache.erase(unifiedKey);
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

		// Determine exactly what input ImGui is allowed to see right now
		const bool shouldRender = fuck->ShouldRender();
		const bool passKeyboardAndGamepad = shouldRender && blockInput;
		const bool passMouse = shouldRender && (blockInput || forceCursor);

		for (auto event = *a_events; event; event = event->next) {
			if (const auto charEvent = event->AsCharEvent()) {
				if (passKeyboardAndGamepad) {
					io.AddInputCharacter(charEvent->keyCode);
				}
			} else if (const auto buttonEvent = event->AsButtonEvent()) {
				const auto key = buttonEvent->GetIDCode();
				const float value = buttonEvent->Value();
				const bool isDown = value > 0.0f;

				switch (inputDevice) {
				case DEVICE::kKeyboard:
					if (passKeyboardAndGamepad)
						io.AddKeyEvent(Keymap::ToImGuiKey(static_cast<KEY>(key)), isDown);
					break;
				case DEVICE::kMouse:
					if (passMouse) {
						switch (auto mouseKey = static_cast<MOUSE>(key)) {
						case MOUSE::kWheelUp:
							io.AddMouseWheelEvent(0, value);
							break;
						case MOUSE::kWheelDown:
							io.AddMouseWheelEvent(0, value * -1);
							break;
						default:
							io.AddMouseButtonEvent(key, isDown);
							break;
						}
					}
					break;
				case DEVICE::kGamepadDirectX:
					if (passKeyboardAndGamepad) {
						auto [imKey, analog] = Keymap::ToImGuiKey(static_cast<GAMEPAD_DIRECTX>(key));
						if (analog)
							io.AddKeyAnalogEvent(imKey, isDown, value);
						else
							io.AddKeyEvent(imKey, isDown);

						if (key == AsKey(GAMEPAD_DIRECTX::kLeftThumb) ||
							key == AsKey(GAMEPAD_DIRECTX::kRightThumb)) {
							io.AddMouseButtonEvent(0, isDown);
						}
					}
					break;
				case DEVICE::kGamepadOrbis:
					if (passKeyboardAndGamepad) {
						auto [imKey, analog] = Keymap::ToImGuiKey(static_cast<GAMEPAD_ORBIS>(key));
						if (analog)
							io.AddKeyAnalogEvent(imKey, isDown, value);
						else
							io.AddKeyEvent(imKey, isDown);

						if (key == AsKey(GAMEPAD_ORBIS::kPS3_L3) ||
							key == AsKey(GAMEPAD_ORBIS::kPS3_R3)) {
							io.AddMouseButtonEvent(0, isDown);
						}
					}
					break;
				default:
					break;
				}
			} else if (passMouse) {
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

		// Explicitly sync modifiers to ImGui
		if (passKeyboardAndGamepad) {
			io.AddKeyEvent(ImGuiMod_Ctrl,	IsModifierPressed(FUCK::Modifier::kCtrl));
			io.AddKeyEvent(ImGuiMod_Shift,	IsModifierPressed(FUCK::Modifier::kShift));
			io.AddKeyEvent(ImGuiMod_Alt,	IsModifierPressed(FUCK::Modifier::kAlt));

			bool superDown = IsInputDown(AsKey(KEY::kLeftWin)) ||
			                 IsInputDown(AsKey(KEY::kRightWin));
			io.AddKeyEvent(ImGuiMod_Super, superDown);
		}
	}
}
