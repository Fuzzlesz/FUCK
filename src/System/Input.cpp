#include "FUCK-Man.h"

#include "ImGui/Audio.h"
#include "ImGui/Renderer.h"
#include "ImGui/Util.h"

#include "Hotkeys.h"
#include "Input.h"

namespace Input
{
	using namespace Input::Keymap;

	// Custom offsets for Stick Axis caching
	constexpr uint32_t CUSTOM_LEFT_STICK_X  = 32;
	constexpr uint32_t CUSTOM_LEFT_STICK_Y  = 33;
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

	void Manager::Register()
	{
		logger::info("Input Manager Initialized");
	}

	void Manager::ClearState()
	{
		std::unique_lock lock(_dataLock);
		_keyStateCache.clear();
		_rebindCtx.Reset();
	}

	DEVICE Manager::GetInputDevice() const
	{
		return _inputDevice;
	}

	bool Manager::IsInputKBM() const
	{
		return _inputDevice == DEVICE::kKeyboard || _inputDevice == DEVICE::kMouse;
	}

	bool Manager::IsInputGamepad() const
	{
		return _inputDevice == DEVICE::kGamepadDirectX || _inputDevice == DEVICE::kGamepadOrbis;
	}

	bool Manager::CanNavigateWithMouse() const
	{
		// The VR wand is pumped straight into ImGui's IO as a mouse, without ever
		// appearing as a keyboard/mouse RE::InputEvent or opening the CursorMenu
		if (ImGui::Renderer::IsVRHelperConnected()) {
			return true;
		}
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
		_cursorInit = std::nullopt;
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

			auto     key        = button->GetIDCode();
			uint32_t unifiedKey = Keymap::GetUnifiedKey(button->GetDevice(), key);

			if (unifiedKey == a_unifiedKey)
				return true;
		}
		return false;
	}

	bool Manager::IsInputReleased(const RE::InputEvent* const* a_event, std::uint32_t a_unifiedKey)
	{
		if (!a_event) {
			return false;
		}

		for (auto event = *a_event; event; event = event->next) {
			const auto button = event->AsButtonEvent();

			if (!button || !button->HasIDCode() || !button->IsUp())
				continue;

			auto     key        = button->GetIDCode();
			uint32_t unifiedKey = Keymap::GetUnifiedKey(button->GetDevice(), key);

			if (unifiedKey == a_unifiedKey)
				return true;
		}
		return false;
	}

	bool Manager::IsInputDown(std::uint32_t a_unifiedKey) const
	{
		std::shared_lock lock(_dataLock);
		auto             it = _keyStateCache.find(a_unifiedKey);
		return it != _keyStateCache.end() && it->second > 0.0f;
	}

	// --- Rebinding API ---
	void Manager::StartBinding(std::uint32_t k, std::int32_t m1, std::int32_t m2, bool disallowModifiers)
	{
		_rebindCtx.active            = true;
		_rebindCtx.disallowModifiers = disallowModifiers;
		_rebindCtx.startTime         = ImGui::GetTime();
		_rebindCtx.originalKey       = k;
		_rebindCtx.originalMod1      = m1;
		_rebindCtx.originalMod2      = m2;
		_rebindCtx.ignoredKeys.clear();

		// Snapshot any keys currently held down so they don't instantly register as the new keybind
		std::shared_lock lock(_dataLock);
		for (const auto& [key, value] : _keyStateCache) {
			if (value > 0.0f) {
				_rebindCtx.ignoredKeys.insert(key);
			}
		}
	}

	FUCK::BindResult Manager::UpdateBinding(const RE::InputEvent* const* a_event, std::uint32_t* outKey, std::int32_t* outMod1, std::int32_t* outMod2)
	{
		if (!_rebindCtx.active)
			return FUCK::BindResult::kNone;

		// Poll the raw input
		std::uint32_t newKey = 0;
		std::int32_t  newM1  = -1;
		std::int32_t  newM2  = -1;

		auto result = GetInputBind(a_event, &newKey, &newM1, &newM2);

		// Prevent accidental double-click capture. (de-bounce)
		if (result == FUCK::BindResult::kBound && (ImGui::GetTime() - _rebindCtx.startTime) < 0.2) {
			return FUCK::BindResult::kNone;
		}

		// Handle Results
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
			if (!button || !button->HasIDCode())
				continue;

			auto     key        = button->GetIDCode();
			auto     device     = button->GetDevice();
			uint32_t unifiedKey = Keymap::GetUnifiedKey(device, key);

			// If a key is released, it is no longer ignored and can be used in future bindings
			if (button->Value() <= 0.0f) {
				_rebindCtx.ignoredKeys.erase(unifiedKey);
				continue;
			}

			// If the key was held down *before* we started binding, completely ignore it
			if (_rebindCtx.ignoredKeys.contains(unifiedKey)) {
				continue;
			}

			// BLOCKERS
			if (device == RE::INPUT_DEVICE::kMouse && (key == static_cast<uint32_t>(MOUSE::kLeftButton) || key == static_cast<uint32_t>(MOUSE::kRightButton)))
				continue;
			if (device == RE::INPUT_DEVICE::kKeyboard && (key == static_cast<uint32_t>(KEY::kLeftWin) || key == static_cast<uint32_t>(KEY::kRightWin)))
				continue;

			// MODIFIER COLLEC
			std::vector<int32_t> pressedMods;
			if (!_rebindCtx.disallowModifiers) {
				if (device == RE::INPUT_DEVICE::kKeyboard) {
					for (auto m : KB_MODS) {
						// Don't register a modifier if it is currently ignored
						if (!_rebindCtx.ignoredKeys.contains(m) && IsInputDown(m))
							pressedMods.push_back(m);
					}
				} else if (device == RE::INPUT_DEVICE::kGamepad) {
					for (auto m : GP_MODS) {
						if (!_rebindCtx.ignoredKeys.contains(m)) {
							if (m == kGP_LT || m == kGP_RT) {
								if (GetAnalogInput(m) > 0.15f)
									pressedMods.push_back(m);
							} else {
								if (IsInputDown(m))
									pressedMods.push_back(m);
							}
						}
					}
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

				std::erase_if(pressedMods, [&](int32_t m) { return IsSameModPair(m, unifiedKey); });
			}

			// CANCEL BIND
			if (unifiedKey == Hotkeys::Manager::EscapeKey() && pressedMods.empty()) {
				ImGui::PlayAudio(ImGui::Audio::kCancel);
				return FUCK::BindResult::kCancelled;
			}

			// MODIFIER PROTEC
			if (!_rebindCtx.disallowModifiers && IsUnifiedModifier(unifiedKey)) {
				continue;
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

		std::uint32_t    k;
		std::int32_t     m1, m2;
		FUCK::BindResult res = UpdateBinding(a_event, &k, &m1, &m2);

		if (res == FUCK::BindResult::kBound) {
			if (k >= Keymap::kGPBase) {
				h.gKey  = k;
				h.gMod1 = m1;
				h.gMod2 = m2;
			} else {
				h.kKey  = k;
				h.kMod1 = m1;
				h.kMod2 = m2;
			}
			h.isBinding      = false;
			h.wasTriggered   = false;  // Do not trigger immediately upon binding
			h.waitForRelease = true;   // Flag for debounce
			return true;
		} else if (res == FUCK::BindResult::kCancelled) {
			h.isBinding = false;
			return true;
		}
		return false;
	}

	bool Manager::CheckModifiersStrict(const std::uint32_t* mods, size_t count, std::int32_t req1, std::int32_t req2, std::uint32_t primaryKey) const
	{
		for (size_t i = 0; i < count; ++i) {
			std::int32_t currentMod = static_cast<std::int32_t>(mods[i]);

			// Ignore the primary key itself from the strictness check!
			if (static_cast<std::uint32_t>(currentMod) == primaryKey)
				continue;

			bool isModDown = false;
			if (currentMod > 0) {
				std::uint32_t umod = static_cast<std::uint32_t>(currentMod);
				if (umod == kGP_LT || umod == kGP_RT) {
					isModDown = GetAnalogInput(umod) > 0.15f;
				} else {
					isModDown = IsInputDown(umod);
				}
			}

			if (isModDown != (currentMod == req1 || currentMod == req2))
				return false;
		}
		return true;
	}

	bool Manager::IsManagedHotkeyDown(FUCK::ManagedHotkey& h)
	{
		if (h.isBinding)
			return false;

		bool isDown = false;

		if (h.kKey != 0 && IsInputDown(h.kKey)) {
			if (CheckModifiersStrict(KB_MODS, std::size(KB_MODS), h.kMod1, h.kMod2, h.kKey))
				isDown = true;
		}

		if (!isDown && h.gKey != 0 && IsInputDown(h.gKey)) {
			if (CheckModifiersStrict(GP_MODS, std::size(GP_MODS), h.gMod1, h.gMod2, h.gKey))
				isDown = true;
		}

		if (h.waitForRelease) {
			if (!isDown) {
				h.waitForRelease = false;
			} else {
				return false;
			}
		}
		return isDown;
	}

	bool Manager::ProcessManagedHotkey(const RE::InputEvent* const* a_event, FUCK::ManagedHotkey& h)
	{
		if (h.isBinding)
			return false;

		if (h.waitForRelease) {
			bool currentlyHeld = false;
			if (h.kKey != 0 && IsInputDown(h.kKey))
				currentlyHeld = true;
			if (h.gKey != 0 && IsInputDown(h.gKey))
				currentlyHeld = true;

			if (!currentlyHeld) {
				h.waitForRelease = false;
			} else {
				return false;
			}
		}

		bool pressed = false;

		if (h.kKey != 0 && IsInputPressed(a_event, h.kKey)) {
			if (CheckModifiersStrict(KB_MODS, std::size(KB_MODS), h.kMod1, h.kMod2, h.kKey))
				pressed = true;
		}

		if (!pressed && h.gKey != 0 && IsInputPressed(a_event, h.gKey)) {
			if (CheckModifiersStrict(GP_MODS, std::size(GP_MODS), h.gMod1, h.gMod2, h.gKey))
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
		auto             it = _keyStateCache.find(a_unifiedKey);
		return it != _keyStateCache.end() ? it->second : 0.0f;
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

	// ==================================================
	// Key Names
	// ==================================================

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

	// ==================================================
	// Processing
	// ==================================================

	void Manager::CacheInputState(const RE::InputEvent* const* a_events)
	{
		std::unique_lock lock(_dataLock);

		for (auto event = *a_events; event; event = event->next) {
			// Only update input device for meaningful events, ignoring micro sensor drift
			bool meaningful = false;
			if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
				// Any press or release solidifies device mode to keep UI responsive
				meaningful = true;
			} else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
				auto stick = event->AsThumbstickEvent();
				if (std::abs(stick->xValue) > 0.15f || std::abs(stick->yValue) > 0.15f) {
					meaningful = true;
				}
			} else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove) {
				// Ignore mouse events entirely if we just issued a synthetic OS cursor snap
				if (!_expectingSyntheticMouseMove) {
					auto mouse = event->AsMouseMoveEvent();
					if (std::abs(mouse->mouseInputX) > 2.0f || std::abs(mouse->mouseInputY) > 2.0f) {
						meaningful = true;
					}
				}
			}

			if (meaningful) {
				UpdateInputDevice(event->GetDevice());
			}

			if (auto button = event->AsButtonEvent()) {
				if (button->HasIDCode()) {
					auto     key        = button->GetIDCode();
					uint32_t unifiedKey = Keymap::GetUnifiedKey(button->GetDevice(), key);

					if (button->Value() > 0.0f) {
						_keyStateCache[unifiedKey] = button->Value();
					} else {
						_keyStateCache.erase(unifiedKey);
					}
				}
			} else if (auto stick = event->AsThumbstickEvent()) {
				uint32_t id         = stick->GetIDCode();
				uint32_t baseOffset = SKSE::InputMap::kMacro_GamepadOffset;

				if (id == 0x0B) {  // Left Stick
					_keyStateCache[baseOffset + CUSTOM_LEFT_STICK_X] = stick->xValue;
					_keyStateCache[baseOffset + CUSTOM_LEFT_STICK_Y] = stick->yValue;
				} else if (id == 0x0C) {  // Right Stick
					_keyStateCache[baseOffset + CUSTOM_RIGHT_STICK_X] = stick->xValue;
					_keyStateCache[baseOffset + CUSTOM_RIGHT_STICK_Y] = stick->yValue;
				}
			}
		}
	}

	void Manager::UpdateInputDevice(RE::INPUT_DEVICE a_device)
	{
		_lastInputDevice = _inputDevice;

		switch (a_device) {
		case RE::INPUT_DEVICE::kKeyboard:
			_inputDevice = DEVICE::kKeyboard;
			break;
		case RE::INPUT_DEVICE::kMouse:
			_inputDevice = DEVICE::kMouse;
			break;
		case RE::INPUT_DEVICE::kGamepad:
			if (RE::ControlMap::GetSingleton()->GetGamePadType() == RE::PC_GAMEPAD_TYPE::kOrbis)
				_inputDevice = DEVICE::kGamepadOrbis;
			else
				_inputDevice = DEVICE::kGamepadDirectX;
			break;
		default:
			break;
		}

		bool wasGamepad = (_lastInputDevice == DEVICE::kGamepadDirectX || _lastInputDevice == DEVICE::kGamepadOrbis);
		bool isGamepad  = (_inputDevice     == DEVICE::kGamepadDirectX || _inputDevice     == DEVICE::kGamepadOrbis);

		// Only reset navigation state when crossing the boundary between KBM and Gamepad
		if (wasGamepad != isGamepad || _lastInputDevice == DEVICE::kNone) {
			auto& io = ImGui::GetIO();

			io.ConfigFlags  &= ~ImGuiConfigFlags_NavEnableGamepad;
			io.ConfigFlags  &= ~ImGuiConfigFlags_NavEnableKeyboard;
			io.ConfigFlags  &= ~ImGuiConfigFlags_IsTouchScreen;
			io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;

			if (isGamepad) {
				io.ConfigFlags  |= ImGuiConfigFlags_NavEnableGamepad;
				io.ConfigFlags  |= ImGuiConfigFlags_IsTouchScreen;  // unused flag to force ImGui to update gamepad input from backend
				io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
			} else {
				io.ConfigFlags  |= ImGuiConfigFlags_NavEnableKeyboard;
				if (_lastInputDevice != DEVICE::kNone) {
					ImGui::ClearNavState();
				}
			}
		}
	}

	void Manager::ProcessInputEvents(RE::InputEvent* const* a_events)
	{
		if (a_events && *a_events) {
			CacheInputState(a_events);
		}

		// Prevent forwarding keys to ImGui if the console is open
		if (auto ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::Console::MENU_NAME)) {
			return;
		}

		// Cursor Visibility
		const auto fuck         = FUCKMan::GetSingleton();
		const bool blockInput   = fuck->IsInputBlocked();
		const bool forceCursor  = fuck->IsCursorForced();
		const bool menuOpen     = fuck->IsOpen();
		const bool shouldRender = fuck->ShouldRender();

		bool shouldShowCursor = false;

		// Always show cursor if the menu is open or forced
		// otherwise restrict blockinput cursor to kbm
		if (menuOpen || forceCursor) {
			shouldShowCursor = true;
		} else if (blockInput && IsInputKBM()) {
			shouldShowCursor = true;
		}

		bool cursorCurrentlyOpen = false;
		if (auto ui = RE::UI::GetSingleton()) {
			cursorCurrentlyOpen = ui->IsMenuOpen(RE::CursorMenu::MENU_NAME);
		}

		static double lastCursorToggleTime = 0.0;
		double        currentTime          = ImGui::GetTime();

		if (shouldShowCursor) {
			if (!_cursorInit.has_value() || (*_cursorInit == false && !cursorCurrentlyOpen)) {
				if (!cursorCurrentlyOpen) {
					ToggleCursor(true);
					_cursorInit          = true;
					lastCursorToggleTime = currentTime;
				} else {
					_cursorInit = false;
				}
			} else if (*_cursorInit == true && !cursorCurrentlyOpen) {
				// Recover the cursor if it was unexpectedly closed by the game engine
				if (currentTime - lastCursorToggleTime > 0.1) {
					ToggleCursor(true);
					lastCursorToggleTime = currentTime;
				}
			}
		} else {
			if (_cursorInit.has_value()) {
				if (*_cursorInit == true) {
					ToggleCursor(false);
				}
				_cursorInit = std::nullopt;
			}
		}

		// Hide visual cursor when playing with a gamepad
		if (auto ui = RE::UI::GetSingleton()) {
			if (auto cursorMenu = ui->GetMenu<RE::CursorMenu>()) {
				if (cursorMenu->uiMovie) {
					RE::GFxValue root;
					if (cursorMenu->uiMovie->GetVariable(&root, "_root")) {
						bool hideVisual = IsInputGamepad() && !menuOpen;

						// Exception: The Map Menu uses the cursor
						if (hideVisual && ui->IsMenuOpen(RE::MapMenu::MENU_NAME)) {
							hideVisual = false;
						}

						root.SetMember("_alpha", RE::GFxValue(hideVisual ? 0.0 : 100.0));
					}
				}
			}
		}

	// Event Forwarding
		auto&      io             = ImGui::GetIO();
		const bool cursorMenuOpen = RE::UI::GetSingleton()->IsMenuOpen(RE::CursorMenu::MENU_NAME);

		const bool isBinding              = IsBinding();
		const bool passKeyboardAndGamepad = shouldRender && blockInput && !isBinding;
		const bool passMouse              = shouldRender && (blockInput || forceCursor);

		if (a_events && *a_events) {
			for (auto event = *a_events; event; event = event->next) {
				if (const auto charEvent = event->AsCharEvent()) {
					if (passKeyboardAndGamepad) {
						io.AddInputCharacter(charEvent->keyCode);
					}
				} else if (const auto buttonEvent = event->AsButtonEvent()) {
					const auto  key    = buttonEvent->GetIDCode();
					const float value  = buttonEvent->Value();
					const bool  isDown = value > 0.0f;

					std::uint32_t unifiedKey = Keymap::GetUnifiedKey(event->GetDevice(), key);

					switch (event->GetDevice()) {
					case RE::INPUT_DEVICE::kKeyboard:
						// Always forward KeyUp events (!isDown) to prevent stuck keys in ImGui
						if (passKeyboardAndGamepad || !isDown)
							io.AddKeyEvent(Keymap::ToImGuiKey(static_cast<KEY>(key)), isDown);
						break;
					case RE::INPUT_DEVICE::kMouse:
						if (passMouse || !isDown) {
							switch (auto mouseKey = static_cast<MOUSE>(key)) {
							case MOUSE::kWheelUp:
								if (passMouse)
									io.AddMouseWheelEvent(0, value);
								break;
							case MOUSE::kWheelDown:
								if (passMouse)
									io.AddMouseWheelEvent(0, value * -1);
								break;
							default:
								io.AddMouseButtonEvent(key, isDown);
								break;
							}
						}
						break;
					case RE::INPUT_DEVICE::kGamepad:
						if (passKeyboardAndGamepad || !isDown) {
							bool isDpad = (unifiedKey == kGP_Up || unifiedKey == kGP_Down || unifiedKey == kGP_Left || unifiedKey == kGP_Right);
							if (isDpad && isDown) {
								_cursorMovedByJoystick = false;  // Revert to standard D-Pad Nav
							}

							bool isMouseClickOverride = (unifiedKey == kGP_L3) || (unifiedKey == kGP_R3) || (unifiedKey == kGP_A && _cursorMovedByJoystick);

							if (isMouseClickOverride) {
								// Inject mouse click AND suppress native gamepad button
								io.AddMouseButtonEvent(0, isDown);
							} else {
								// Pass native gamepad button to ImGui
								if (RE::ControlMap::GetSingleton()->GetGamePadType() == RE::PC_GAMEPAD_TYPE::kOrbis) {
									auto mapped = Keymap::ToImGuiKey(static_cast<GAMEPAD_ORBIS>(key));
									if (mapped.second) {
										if (passKeyboardAndGamepad)
											io.AddKeyAnalogEvent(mapped.first, isDown, value);
									} else {
										io.AddKeyEvent(mapped.first, isDown);
									}
								} else {
									auto mapped = Keymap::ToImGuiKey(static_cast<GAMEPAD_DIRECTX>(key));
									if (mapped.second) {
										if (passKeyboardAndGamepad)
											io.AddKeyAnalogEvent(mapped.first, isDown, value);
									} else {
										io.AddKeyEvent(mapped.first, isDown);
									}
								}
							}
						}
						break;
					default:
						break;
					}
				} else if (passMouse) {
					if (auto mouseEvent = event->AsMouseMoveEvent()) {
						// Drop the "Echo" from SetCursorPos
						if (_expectingSyntheticMouseMove) {
							_expectingSyntheticMouseMove = false;
							continue;
						}
						if (std::abs(mouseEvent->mouseInputX) > 2.0f || std::abs(mouseEvent->mouseInputY) > 2.0f) {
							_cursorMovedByJoystick = false;
						}
						if (cursorMenuOpen) {
							if (auto cursorMenu = RE::UI::GetSingleton()->GetMenu<RE::CursorMenu>()) {
								cursorMenu->AsMenuEventHandler()->ProcessMouseMove(mouseEvent);
							}
						}
					} else if (const auto thumbstickEvent = event->AsThumbstickEvent()) {
						if (std::abs(thumbstickEvent->xValue) > 0.05f || std::abs(thumbstickEvent->yValue) > 0.05f) {
							_cursorMovedByJoystick = true;
						}
						if (cursorMenuOpen && (fuck->IsOpen() || forceCursor)) {
							if (auto cursorMenu = RE::UI::GetSingleton()->GetMenu<RE::CursorMenu>()) {
								cursorMenu->AsMenuEventHandler()->ProcessThumbstick(thumbstickEvent);
							}
						}
					}
				}
			}

			// Sync modifiers to ImGui
			if (passKeyboardAndGamepad) {
				io.AddKeyEvent(ImGuiMod_Ctrl,  IsModifierPressed(FUCK::Modifier::kCtrl));
				io.AddKeyEvent(ImGuiMod_Shift, IsModifierPressed(FUCK::Modifier::kShift));
				io.AddKeyEvent(ImGuiMod_Alt,   IsModifierPressed(FUCK::Modifier::kAlt));

				bool superDown = IsInputDown(AsKey(KEY::kLeftWin)) ||
				                 IsInputDown(AsKey(KEY::kRightWin));
				io.AddKeyEvent(ImGuiMod_Super, superDown);
			}

			// Sync OS hardware cursor to the virtual joystick cursor. Flat only:
			// there's no meaningful OS cursor in a headset, and warping it here
			// would fight the wand's own AddMousePosEvent pump.
			if (!ImGui::Renderer::IsVRHelperConnected() && _cursorMovedByJoystick && cursorMenuOpen && passMouse) {
				if (auto mc = RE::MenuCursor::GetSingleton()) {
					ImVec2 clientPos = ImGui::TranslateScaleformToScreen(mc->GetRuntimeData().cursorPosX, mc->GetRuntimeData().cursorPosY);

					static float s_lastClientX = -1.0f;
					static float s_lastClientY = -1.0f;

					if (clientPos.x != s_lastClientX || clientPos.y != s_lastClientY) {
						s_lastClientX = clientPos.x;
						s_lastClientY = clientPos.y;

						if (HWND hwnd = ::GetActiveWindow()) {
							POINT pt = { static_cast<LONG>(clientPos.x), static_cast<LONG>(clientPos.y) };
							::ClientToScreen(hwnd, &pt);
							::SetCursorPos(pt.x, pt.y);
							_expectingSyntheticMouseMove = true;
						}
					}
				}
			}
		}
	}
}
