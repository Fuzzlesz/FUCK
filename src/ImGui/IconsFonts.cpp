#include "IconsFonts.h"

#include "FUCKMan.h"
#include "IconsFontAwesome6.h"
#include "Renderer.h"
#include "Styles.h"
#include "System/Input.h"
#include "System/Settings.h"
#include "System/Utils.h"
#include "Util.h"

#include "ImGui/Backend/imgui_default.h"

namespace IconFont
{
	IconTexture::IconTexture(std::wstring_view a_iconName) :
		ImGui::Texture(LR"(Data/Interface/ImGuiIcons/Icons/)", a_iconName)
	{}

	bool IconTexture::Load(bool a_resizeToScreenRes)
	{
		const bool result = ImGui::Texture::Load(a_resizeToScreenRes);
		if (result) {
			imageSize = size;
			if (image)
				image.reset();
		}
		return result;
	}

	std::string Manager::ResolveFontPath(const std::string& a_filename) const
	{
		auto settings = Settings::GetSingleton();

		// Check User Fonts (Data/FUCKs/FUCK/fonts)
		if (auto p = Utils::FindFileRecursive(settings->GetUserFontsPath(), a_filename))
			return *p;
		// Check Legacy Fonts (Data/Interface/ImGuiIcons/Fonts)
		if (auto p = Utils::FindFileRecursive(settings->GetLegacyFontsPath(), a_filename))
			return *p;
		// Check Community Shaders Fonts (Data/Interface/CommunityShaders/Fonts)
		if (auto p = Utils::FindFileRecursive(settings->GetCSFontsPath(), a_filename))
			return *p;

		// Fallback
		return a_filename;
	}

	void Manager::SetFontName(const std::string& a_fontName)
	{
		if (_fontName != a_fontName && !a_fontName.empty()) {
			_fontName = a_fontName;
			ReloadFonts();
		}
	}

	void Manager::ReloadFonts()
	{
		_pendingReload = true;
	}

	bool Manager::ProcessPendingReload()
	{
		if (!_pendingReload)
			return false;

		RebuildFontAtlas();
		_pendingReload = false;
		return true;
	}

	void Manager::RebuildFontAtlas()
	{
		_loadedFonts = true;
		auto& io    = ImGui::GetIO();
		io.Fonts->Clear();

		ImVector<ImWchar>        ranges;
		ImFontGlyphRangesBuilder builder;
		if (auto scaleform = RE::BSScaleformManager::GetSingleton()) {
			builder.AddText(scaleform->validNameChars.c_str());
		} else {
			builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
		}
		static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
		builder.AddRanges(icons_ranges);
		builder.BuildRanges(&ranges);

		_regularFont = LoadFontIconSet(_baseFontSize, _baseIconSize, ranges);
		_largeFont   = LoadFontIconSet(_baseLargeFontSize, _baseLargeIconSize, ranges);

		io.FontDefault                 = _regularFont;
		ImGui::GetStyle().FontSizeBase = _regularFont->LegacySize;

		io.Fonts->SetFontLoader(ImGuiFreeType::GetFontLoader());
		io.Fonts->FontLoaderFlags = ImGuiFreeTypeLoaderFlags_NoHinting;

		io.Fonts->Build();

		ImGui_ImplDX11_InvalidateDeviceObjects();
		ImGui_ImplDX11_CreateDeviceObjects();
	}

	ImFont* Manager::LoadFontIconSet(float a_fontSize, float a_iconSize, const ImVector<ImWchar>& a_ranges) const
	{
		const auto& io = ImGui::GetIO();

		ImFont* font = nullptr;

		if (_fontName == "Default") {
			ImFontConfig default_cfg;
			default_cfg.FontDataOwnedByAtlas = false;
			font                             = io.Fonts->AddFontFromMemoryCompressedBase85TTF(
                GetDefaultFontData(), a_fontSize, &default_cfg, a_ranges.Data);
		} else {
			std::string fullPath = ResolveFontPath(_fontName);
			font                 = io.Fonts->AddFontFromFileTTF(fullPath.c_str(), a_fontSize, nullptr, a_ranges.Data);
		}

		if (!font)
			font = io.Fonts->AddFontDefault();

		ImFontConfig icon_config;
		icon_config.MergeMode   = true;
		icon_config.PixelSnapH  = true;
		icon_config.OversampleH = icon_config.OversampleV = 1;

		std::string faPath = ResolveFontPath(FONT_ICON_FILE_NAME_FAS);
		io.Fonts->AddFontFromFileTTF(faPath.c_str(), a_iconSize, &icon_config, a_ranges.Data);
		return font;
	}

	void Manager::LoadIcons()
	{
		_stepperRight.Load();
		_checkbox.Load();
		_checkboxFilled.Load();

		_unknownKey.Load();
		_upKey.Load();
		_downKey.Load();
		_leftKey.Load();
		_rightKey.Load();

		_keyboard.clear();
		_gamePad.clear();
		_mouse.clear();

		// Keyboard
		_keyboard.emplace(KEY::kTab, IconTexture(L"Tab"sv));
		_keyboard.emplace(KEY::kPageUp, IconTexture(L"PgUp"sv));
		_keyboard.emplace(KEY::kPageDown, IconTexture(L"PgDn"sv));
		_keyboard.emplace(KEY::kHome, IconTexture(L"Home"sv));
		_keyboard.emplace(KEY::kEnd, IconTexture(L"End"sv));
		_keyboard.emplace(KEY::kInsert, IconTexture(L"Insert"sv));
		_keyboard.emplace(KEY::kDelete, IconTexture(L"Delete"sv));
		_keyboard.emplace(KEY::kBackspace, IconTexture(L"Backspace"sv));
		_keyboard.emplace(KEY::kSpacebar, IconTexture(L"Space"sv));
		_keyboard.emplace(KEY::kEnter, IconTexture(L"Enter"sv));
		_keyboard.emplace(KEY::kEscape, IconTexture(L"Esc"sv));
		_keyboard.emplace(KEY::kLeftControl, IconTexture(L"L-Ctrl"sv));
		_keyboard.emplace(KEY::kLeftShift, IconTexture(L"L-Shift"sv));
		_keyboard.emplace(KEY::kLeftAlt, IconTexture(L"L-Alt"sv));
		_keyboard.emplace(KEY::kRightControl, IconTexture(L"R-Ctrl"sv));
		_keyboard.emplace(KEY::kRightShift, IconTexture(L"R-Shift"sv));
		_keyboard.emplace(KEY::kRightAlt, IconTexture(L"R-Alt"sv));
		_keyboard.emplace(KEY::kNum0, IconTexture(L"0"sv));
		_keyboard.emplace(KEY::kNum1, IconTexture(L"1"sv));
		_keyboard.emplace(KEY::kNum2, IconTexture(L"2"sv));
		_keyboard.emplace(KEY::kNum3, IconTexture(L"3"sv));
		_keyboard.emplace(KEY::kNum4, IconTexture(L"4"sv));
		_keyboard.emplace(KEY::kNum5, IconTexture(L"5"sv));
		_keyboard.emplace(KEY::kNum6, IconTexture(L"6"sv));
		_keyboard.emplace(KEY::kNum7, IconTexture(L"7"sv));
		_keyboard.emplace(KEY::kNum8, IconTexture(L"8"sv));
		_keyboard.emplace(KEY::kNum9, IconTexture(L"9"sv));
		_keyboard.emplace(KEY::kA, IconTexture(L"A"sv));
		_keyboard.emplace(KEY::kB, IconTexture(L"B"sv));
		_keyboard.emplace(KEY::kC, IconTexture(L"C"sv));
		_keyboard.emplace(KEY::kD, IconTexture(L"D"sv));
		_keyboard.emplace(KEY::kE, IconTexture(L"E"sv));
		_keyboard.emplace(KEY::kF, IconTexture(L"F"sv));
		_keyboard.emplace(KEY::kG, IconTexture(L"G"sv));
		_keyboard.emplace(KEY::kH, IconTexture(L"H"sv));
		_keyboard.emplace(KEY::kI, IconTexture(L"I"sv));
		_keyboard.emplace(KEY::kJ, IconTexture(L"J"sv));
		_keyboard.emplace(KEY::kK, IconTexture(L"K"sv));
		_keyboard.emplace(KEY::kL, IconTexture(L"L"sv));
		_keyboard.emplace(KEY::kM, IconTexture(L"M"sv));
		_keyboard.emplace(KEY::kN, IconTexture(L"N"sv));
		_keyboard.emplace(KEY::kO, IconTexture(L"O"sv));
		_keyboard.emplace(KEY::kP, IconTexture(L"P"sv));
		_keyboard.emplace(KEY::kQ, IconTexture(L"Q"sv));
		_keyboard.emplace(KEY::kR, IconTexture(L"R"sv));
		_keyboard.emplace(KEY::kS, IconTexture(L"S"sv));
		_keyboard.emplace(KEY::kT, IconTexture(L"T"sv));
		_keyboard.emplace(KEY::kU, IconTexture(L"U"sv));
		_keyboard.emplace(KEY::kV, IconTexture(L"V"sv));
		_keyboard.emplace(KEY::kW, IconTexture(L"W"sv));
		_keyboard.emplace(KEY::kX, IconTexture(L"X"sv));
		_keyboard.emplace(KEY::kY, IconTexture(L"Y"sv));
		_keyboard.emplace(KEY::kZ, IconTexture(L"Z"sv));
		_keyboard.emplace(KEY::kF1, IconTexture(L"F1"sv));
		_keyboard.emplace(KEY::kF2, IconTexture(L"F2"sv));
		_keyboard.emplace(KEY::kF3, IconTexture(L"F3"sv));
		_keyboard.emplace(KEY::kF4, IconTexture(L"F4"sv));
		_keyboard.emplace(KEY::kF5, IconTexture(L"F5"sv));
		_keyboard.emplace(KEY::kF6, IconTexture(L"F6"sv));
		_keyboard.emplace(KEY::kF7, IconTexture(L"F7"sv));
		_keyboard.emplace(KEY::kF8, IconTexture(L"F8"sv));
		_keyboard.emplace(KEY::kF9, IconTexture(L"F9"sv));
		_keyboard.emplace(KEY::kF10, IconTexture(L"F10"sv));
		_keyboard.emplace(KEY::kF11, IconTexture(L"F11"sv));
		_keyboard.emplace(KEY::kF12, IconTexture(L"F12"sv));
		_keyboard.emplace(KEY::kApostrophe, IconTexture(L"Quotesingle"sv));
		_keyboard.emplace(KEY::kComma, IconTexture(L"Comma"sv));
		_keyboard.emplace(KEY::kMinus, IconTexture(L"Hyphen"sv));
		_keyboard.emplace(KEY::kPeriod, IconTexture(L"Period"sv));
		_keyboard.emplace(KEY::kSlash, IconTexture(L"Slash"sv));
		_keyboard.emplace(KEY::kSemicolon, IconTexture(L"Semicolon"sv));
		_keyboard.emplace(KEY::kEquals, IconTexture(L"Equal"sv));
		_keyboard.emplace(KEY::kBracketLeft, IconTexture(L"Bracketleft"sv));
		_keyboard.emplace(KEY::kBackslash, IconTexture(L"Backslash"sv));
		_keyboard.emplace(KEY::kBracketRight, IconTexture(L"Bracketright"sv));
		_keyboard.emplace(KEY::kTilde, IconTexture(L"Tilde"sv));
		_keyboard.emplace(KEY::kCapsLock, IconTexture(L"CapsLock"sv));
		_keyboard.emplace(KEY::kScrollLock, IconTexture(L"ScrollLock"sv));
		_keyboard.emplace(KEY::kNumLock, IconTexture(L"NumLock"sv));
		_keyboard.emplace(KEY::kPrintScreen, IconTexture(L"PrintScreen"sv));
		_keyboard.emplace(KEY::kPause, IconTexture(L"Pause"sv));
		_keyboard.emplace(KEY::kKP_0, IconTexture(L"NumPad0"sv));
		_keyboard.emplace(KEY::kKP_1, IconTexture(L"Keypad1"sv));
		_keyboard.emplace(KEY::kKP_2, IconTexture(L"Keypad2"sv));
		_keyboard.emplace(KEY::kKP_3, IconTexture(L"Keypad3"sv));
		_keyboard.emplace(KEY::kKP_4, IconTexture(L"Keypad4"sv));
		_keyboard.emplace(KEY::kKP_5, IconTexture(L"Keypad5"sv));
		_keyboard.emplace(KEY::kKP_6, IconTexture(L"Keypad6"sv));
		_keyboard.emplace(KEY::kKP_7, IconTexture(L"Keypad7"sv));
		_keyboard.emplace(KEY::kKP_8, IconTexture(L"Keypad8"sv));
		_keyboard.emplace(KEY::kKP_9, IconTexture(L"NumPad9"sv));
		_keyboard.emplace(KEY::kKP_Decimal, IconTexture(L"NumPadDec"sv));
		_keyboard.emplace(KEY::kKP_Divide, IconTexture(L"NumPadDivide"sv));
		_keyboard.emplace(KEY::kKP_Multiply, IconTexture(L"NumPadMult"sv));
		_keyboard.emplace(KEY::kKP_Subtract, IconTexture(L"NumPadMinus"sv));
		_keyboard.emplace(KEY::kKP_Plus, IconTexture(L"NumPadPlus"sv));
		_keyboard.emplace(KEY::kKP_Enter, IconTexture(L"KeypadEnter"sv));

		// Gamepad
		constexpr uint32_t gpOffset = SKSE::InputMap::kMacro_GamepadOffset;
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_START, GamepadIcon{ IconTexture(L"360_Start"sv), IconTexture(L"PS3_Start"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_BACK, GamepadIcon{ IconTexture(L"360_Back"sv), IconTexture(L"PS3_Back"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_LEFT_THUMB, GamepadIcon{ IconTexture(L"360_LS"sv), IconTexture(L"PS3_L3"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_RIGHT_THUMB, GamepadIcon{ IconTexture(L"360_RS"sv), IconTexture(L"PS3_R3"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_LEFT_SHOULDER, GamepadIcon{ IconTexture(L"360_LB"sv), IconTexture(L"PS3_LB"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_RIGHT_SHOULDER, GamepadIcon{ IconTexture(L"360_RB"sv), IconTexture(L"PS3_RB"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_A, GamepadIcon{ IconTexture(L"360_A"sv), IconTexture(L"PS3_A"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_B, GamepadIcon{ IconTexture(L"360_B"sv), IconTexture(L"PS3_B"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_X, GamepadIcon{ IconTexture(L"360_X"sv), IconTexture(L"PS3_X"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_Y, GamepadIcon{ IconTexture(L"360_Y"sv), IconTexture(L"PS3_Y"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_LT, GamepadIcon{ IconTexture(L"360_LT"sv), IconTexture(L"PS3_LT"sv) });
		_gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_RT, GamepadIcon{ IconTexture(L"360_RT"sv), IconTexture(L"PS3_RT"sv) });

		// Mouse
		_mouse.emplace(256 + MOUSE::kLeftButton, IconTexture(L"Mouse1"sv));
		_mouse.emplace(256 + MOUSE::kRightButton, IconTexture(L"Mouse2"sv));
		_mouse.emplace(256 + MOUSE::kMiddleButton, IconTexture(L"Mouse3"sv));
		_mouse.emplace(256 + MOUSE::kButton3, IconTexture(L"Mouse4"sv));
		_mouse.emplace(256 + MOUSE::kButton4, IconTexture(L"Mouse5"sv));
		_mouse.emplace(256 + MOUSE::kButton5, IconTexture(L"Mouse6"sv));
		_mouse.emplace(256 + MOUSE::kButton6, IconTexture(L"Mouse7"sv));
		_mouse.emplace(256 + MOUSE::kButton7, IconTexture(L"Mouse8"sv));

		// Batch load
		std::for_each(_keyboard.begin(), _keyboard.end(), [](auto& Icon) { Icon.second.Load(); });
		std::for_each(_gamePad.begin(), _gamePad.end(), [](auto& Icon) {
			auto& [xbox, ps4] = Icon.second;
			xbox.Load();
			ps4.Load();
		});
		std::for_each(_mouse.begin(), _mouse.end(), [](auto& Icon) { Icon.second.Load(); });
	}

	ImFont*            Manager::GetLargeFont() const { return _largeFont; }
	ImFont*            Manager::GetRegularFont() const { return _regularFont; }
	const IconTexture* Manager::GetStepperRight() const { return &_stepperRight; }
	const IconTexture* Manager::GetCheckbox() const { return &_checkbox; }
	const IconTexture* Manager::GetCheckboxFilled() const { return &_checkboxFilled; }

	const IconTexture* Manager::GetIcon(std::uint32_t key)
	{
		constexpr uint32_t gpOffset = static_cast<uint32_t>(SKSE::InputMap::kMacro_GamepadOffset);
		constexpr uint32_t msOffset = static_cast<uint32_t>(SKSE::InputMap::kMacro_MouseButtonOffset);

		switch (key) {
		case KEY::kUp:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_UP:
			return &_upKey;
		case KEY::kDown:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_DOWN:
			return &_downKey;
		case KEY::kLeft:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_LEFT:
			return &_leftKey;
		case KEY::kRight:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_RIGHT:
			return &_rightKey;
		}

		if (key >= gpOffset) {
			if (const auto it = _gamePad.find(key); it != _gamePad.end()) {
				return GetGamePadIcon(it->second);
			}
		} else if (key >= msOffset) {
			if (const auto it = _mouse.find(key); it != _mouse.end()) {
				return &it->second;
			}
		} else {
			if (const auto it = _keyboard.find(static_cast<KEY>(key)); it != _keyboard.end()) {
				return &it->second;
			}
		}

		return &_unknownKey;
	}

	std::set<const IconTexture*> Manager::GetIcons(const std::set<std::uint32_t>& keys)
	{
		std::set<const IconTexture*> icons{};
		if (keys.empty()) {
			icons.insert(&_unknownKey);
		} else {
			for (auto& key : keys) {
				icons.insert(GetIcon(key));
			}
		}
		return icons;
	}

	const IconTexture* Manager::GetGamePadIcon(const GamepadIcon& a_icons) const
	{
		switch (_buttonScheme) {
		case BUTTON_SCHEME::kAutoDetect:
			return MANAGER(Input)->GetInputDevice() == Input::DEVICE::kGamepadOrbis ? &a_icons.ps4 : &a_icons.xbox;
		case BUTTON_SCHEME::kXbox:
			return &a_icons.xbox;
		case BUTTON_SCHEME::kPS4:
			return &a_icons.ps4;
		default:
			return &a_icons.xbox;
		}
	}
}
