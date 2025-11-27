#include "IconsFonts.h"

#include "FUCKMan.h"
#include "System/Input.h"
#include "System/Settings.h"
#include "IconsFontAwesome6.h"
#include "Renderer.h"
#include "Styles.h"
#include "Util.h"

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

	void IconTexture::Resize(float a_scale)
	{
		float userScale = FUCKMan::GetSingleton()->GetUserScale();
		auto height = RE::BSGraphics::Renderer::GetScreenSize().height;
		float relativeHeight = height / 1080.0f;
		float finalScale = a_scale * relativeHeight * userScale;
		size = imageSize * finalScale;
	}

	std::string Manager::ResolveFontPath(const std::string& a_filename) const
	{
		// 1. Check User Fonts (Data/Interface/FUCK/Fonts)
		std::filesystem::path userPath(Settings::GetSingleton()->GetUserFontsPath());
		userPath /= a_filename;
		if (std::filesystem::exists(userPath))
			return userPath.string();

		// 2. Check Legacy Fonts (Data/Interface/ImGuiIcons/Fonts)
		std::filesystem::path legacyPath(Settings::GetSingleton()->GetLegacyFontsPath());
		legacyPath /= a_filename;
		if (std::filesystem::exists(legacyPath))
			return legacyPath.string();

		// 3. Fallback
		return a_filename;
	}

	void Manager::SetFontName(const std::string& a_fontName)
	{
		// If font changed, update name and request reload
		if (fontName != a_fontName && !a_fontName.empty()) {
			fontName = a_fontName;
			ReloadFonts();
		}
	}

	void Manager::LoadFontSettings(CSimpleIniA& a_ini)
	{
		const char* iniFont = a_ini.GetValue("Fonts", "sFont");
		if (iniFont && *iniFont && fontName.empty()) {
			fontName = iniFont;
		}

		baseFontSize = static_cast<float>(a_ini.GetDoubleValue("Fonts", "iFontSize", baseFontSize));
		baseLargeFontSize = static_cast<float>(a_ini.GetDoubleValue("Fonts", "iLargeFontSize", baseLargeFontSize));
		baseIconSize = static_cast<float>(a_ini.GetDoubleValue("Fonts", "iIconSize", baseIconSize));
		baseLargeIconSize = static_cast<float>(a_ini.GetDoubleValue("Fonts", "iLargeIconSize", baseLargeIconSize));
	}

	void Manager::LoadSettings(CSimpleIniA& a_ini)
	{
		LoadFontSettings(a_ini);
		buttonScheme = static_cast<BUTTON_SCHEME>(a_ini.GetLongValue("Controls", "iButtonScheme", std::to_underlying(buttonScheme)));
	}

	void Manager::ReloadFonts()
	{
		// Just set the flag.
		// DO NOT rebuild the atlas here, or we crash if called mid-frame.
		_pendingReload = true;
	}

	bool Manager::ProcessPendingReload()
	{
		if (!_pendingReload)
			return false;

		RebuildFontAtlas();
		_pendingReload = false;
		return true;  // Return true to indicate we did work (caller should skip frame)
	}

	void Manager::RebuildFontAtlas()
	{
		loadedFonts = true;
		auto& io = ImGui::GetIO();
		io.Fonts->Clear();

		ImVector<ImWchar> ranges;
		ImFontGlyphRangesBuilder builder;
		if (auto scaleform = RE::BSScaleformManager::GetSingleton()) {
			builder.AddText(scaleform->validNameChars.c_str());
		} else {
			builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
		}
		static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
		builder.AddRanges(icons_ranges);
		builder.BuildRanges(&ranges);

		float resScale = ImGui::Renderer::GetResolutionScale();
		float userScale = FUCKMan::GetSingleton()->GetUserScale();
		float totalScale = std::max(resScale * userScale, 0.5f);

		regularFont = LoadFontIconSet(baseFontSize * totalScale, baseIconSize * totalScale, ranges);
		largeFont = LoadFontIconSet(baseLargeFontSize * totalScale, baseLargeIconSize * totalScale, ranges);

		io.FontDefault = regularFont;
		io.FontGlobalScale = 1.0f;
		io.Fonts->Build();

		ImGui_ImplDX11_InvalidateDeviceObjects();
		ImGui_ImplDX11_CreateDeviceObjects();
	}

	ImFont* Manager::LoadFontIconSet(float a_fontSize, float a_iconSize, const ImVector<ImWchar>& a_ranges) const
	{
		const auto& io = ImGui::GetIO();
		std::string fullPath = ResolveFontPath(fontName);

		auto font = io.Fonts->AddFontFromFileTTF(fullPath.c_str(), a_fontSize, nullptr, a_ranges.Data);
		if (!font)
			font = io.Fonts->AddFontDefault();

		ImFontConfig icon_config;
		icon_config.MergeMode = true;
		icon_config.PixelSnapH = true;
		icon_config.OversampleH = icon_config.OversampleV = 1;

		std::string faPath = ResolveFontPath(FONT_ICON_FILE_NAME_FAS);
		io.Fonts->AddFontFromFileTTF(faPath.c_str(), a_iconSize, &icon_config, a_ranges.Data);
		return font;
	}

	void Manager::LoadIcons()
	{
		stepperLeft.Load();
		stepperRight.Load();
		checkbox.Load();
		checkboxFilled.Load();

		unknownKey.Load();
		upKey.Load();
		downKey.Load();
		leftKey.Load();
		rightKey.Load();

		keyboard.clear();
		gamePad.clear();
		mouse.clear();

		// Keyboard
		keyboard.emplace(KEY::kTab, IconTexture(L"Tab"sv));
		keyboard.emplace(KEY::kPageUp, IconTexture(L"PgUp"sv));
		keyboard.emplace(KEY::kPageDown, IconTexture(L"PgDn"sv));
		keyboard.emplace(KEY::kHome, IconTexture(L"Home"sv));
		keyboard.emplace(KEY::kEnd, IconTexture(L"End"sv));
		keyboard.emplace(KEY::kInsert, IconTexture(L"Insert"sv));
		keyboard.emplace(KEY::kDelete, IconTexture(L"Delete"sv));
		keyboard.emplace(KEY::kBackspace, IconTexture(L"Backspace"sv));
		keyboard.emplace(KEY::kSpacebar, IconTexture(L"Space"sv));
		keyboard.emplace(KEY::kEnter, IconTexture(L"Enter"sv));
		keyboard.emplace(KEY::kEscape, IconTexture(L"Esc"sv));
		keyboard.emplace(KEY::kLeftControl, IconTexture(L"L-Ctrl"sv));
		keyboard.emplace(KEY::kLeftShift, IconTexture(L"L-Shift"sv));
		keyboard.emplace(KEY::kLeftAlt, IconTexture(L"L-Alt"sv));
		keyboard.emplace(KEY::kRightControl, IconTexture(L"R-Ctrl"sv));
		keyboard.emplace(KEY::kRightShift, IconTexture(L"R-Shift"sv));
		keyboard.emplace(KEY::kRightAlt, IconTexture(L"R-Alt"sv));
		keyboard.emplace(KEY::kNum0, IconTexture(L"0"sv));
		keyboard.emplace(KEY::kNum1, IconTexture(L"1"sv));
		keyboard.emplace(KEY::kNum2, IconTexture(L"2"sv));
		keyboard.emplace(KEY::kNum3, IconTexture(L"3"sv));
		keyboard.emplace(KEY::kNum4, IconTexture(L"4"sv));
		keyboard.emplace(KEY::kNum5, IconTexture(L"5"sv));
		keyboard.emplace(KEY::kNum6, IconTexture(L"6"sv));
		keyboard.emplace(KEY::kNum7, IconTexture(L"7"sv));
		keyboard.emplace(KEY::kNum8, IconTexture(L"8"sv));
		keyboard.emplace(KEY::kNum9, IconTexture(L"9"sv));
		keyboard.emplace(KEY::kA, IconTexture(L"A"sv));
		keyboard.emplace(KEY::kB, IconTexture(L"B"sv));
		keyboard.emplace(KEY::kC, IconTexture(L"C"sv));
		keyboard.emplace(KEY::kD, IconTexture(L"D"sv));
		keyboard.emplace(KEY::kE, IconTexture(L"E"sv));
		keyboard.emplace(KEY::kF, IconTexture(L"F"sv));
		keyboard.emplace(KEY::kG, IconTexture(L"G"sv));
		keyboard.emplace(KEY::kH, IconTexture(L"H"sv));
		keyboard.emplace(KEY::kI, IconTexture(L"I"sv));
		keyboard.emplace(KEY::kJ, IconTexture(L"J"sv));
		keyboard.emplace(KEY::kK, IconTexture(L"K"sv));
		keyboard.emplace(KEY::kL, IconTexture(L"L"sv));
		keyboard.emplace(KEY::kM, IconTexture(L"M"sv));
		keyboard.emplace(KEY::kN, IconTexture(L"N"sv));
		keyboard.emplace(KEY::kO, IconTexture(L"O"sv));
		keyboard.emplace(KEY::kP, IconTexture(L"P"sv));
		keyboard.emplace(KEY::kQ, IconTexture(L"Q"sv));
		keyboard.emplace(KEY::kR, IconTexture(L"R"sv));
		keyboard.emplace(KEY::kS, IconTexture(L"S"sv));
		keyboard.emplace(KEY::kT, IconTexture(L"T"sv));
		keyboard.emplace(KEY::kU, IconTexture(L"U"sv));
		keyboard.emplace(KEY::kV, IconTexture(L"V"sv));
		keyboard.emplace(KEY::kW, IconTexture(L"W"sv));
		keyboard.emplace(KEY::kX, IconTexture(L"X"sv));
		keyboard.emplace(KEY::kY, IconTexture(L"Y"sv));
		keyboard.emplace(KEY::kZ, IconTexture(L"Z"sv));
		keyboard.emplace(KEY::kF1, IconTexture(L"F1"sv));
		keyboard.emplace(KEY::kF2, IconTexture(L"F2"sv));
		keyboard.emplace(KEY::kF3, IconTexture(L"F3"sv));
		keyboard.emplace(KEY::kF4, IconTexture(L"F4"sv));
		keyboard.emplace(KEY::kF5, IconTexture(L"F5"sv));
		keyboard.emplace(KEY::kF6, IconTexture(L"F6"sv));
		keyboard.emplace(KEY::kF7, IconTexture(L"F7"sv));
		keyboard.emplace(KEY::kF8, IconTexture(L"F8"sv));
		keyboard.emplace(KEY::kF9, IconTexture(L"F9"sv));
		keyboard.emplace(KEY::kF10, IconTexture(L"F10"sv));
		keyboard.emplace(KEY::kF11, IconTexture(L"F11"sv));
		keyboard.emplace(KEY::kF12, IconTexture(L"F12"sv));
		keyboard.emplace(KEY::kApostrophe, IconTexture(L"Quotesingle"sv));
		keyboard.emplace(KEY::kComma, IconTexture(L"Comma"sv));
		keyboard.emplace(KEY::kMinus, IconTexture(L"Hyphen"sv));
		keyboard.emplace(KEY::kPeriod, IconTexture(L"Period"sv));
		keyboard.emplace(KEY::kSlash, IconTexture(L"Slash"sv));
		keyboard.emplace(KEY::kSemicolon, IconTexture(L"Semicolon"sv));
		keyboard.emplace(KEY::kEquals, IconTexture(L"Equal"sv));
		keyboard.emplace(KEY::kBracketLeft, IconTexture(L"Bracketleft"sv));
		keyboard.emplace(KEY::kBackslash, IconTexture(L"Backslash"sv));
		keyboard.emplace(KEY::kBracketRight, IconTexture(L"Bracketright"sv));
		keyboard.emplace(KEY::kTilde, IconTexture(L"Tilde"sv));
		keyboard.emplace(KEY::kCapsLock, IconTexture(L"CapsLock"sv));
		keyboard.emplace(KEY::kScrollLock, IconTexture(L"ScrollLock"sv));
		keyboard.emplace(KEY::kNumLock, IconTexture(L"NumLock"sv));
		keyboard.emplace(KEY::kPrintScreen, IconTexture(L"PrintScreen"sv));
		keyboard.emplace(KEY::kPause, IconTexture(L"Pause"sv));
		keyboard.emplace(KEY::kKP_0, IconTexture(L"NumPad0"sv));
		keyboard.emplace(KEY::kKP_1, IconTexture(L"Keypad1"sv));
		keyboard.emplace(KEY::kKP_2, IconTexture(L"Keypad2"sv));
		keyboard.emplace(KEY::kKP_3, IconTexture(L"Keypad3"sv));
		keyboard.emplace(KEY::kKP_4, IconTexture(L"Keypad4"sv));
		keyboard.emplace(KEY::kKP_5, IconTexture(L"Keypad5"sv));
		keyboard.emplace(KEY::kKP_6, IconTexture(L"Keypad6"sv));
		keyboard.emplace(KEY::kKP_7, IconTexture(L"Keypad7"sv));
		keyboard.emplace(KEY::kKP_8, IconTexture(L"Keypad8"sv));
		keyboard.emplace(KEY::kKP_9, IconTexture(L"NumPad9"sv));
		keyboard.emplace(KEY::kKP_Decimal, IconTexture(L"NumPadDec"sv));
		keyboard.emplace(KEY::kKP_Divide, IconTexture(L"NumPadDivide"sv));
		keyboard.emplace(KEY::kKP_Multiply, IconTexture(L"NumPadMult"sv));
		keyboard.emplace(KEY::kKP_Subtract, IconTexture(L"NumPadMinus"sv));
		keyboard.emplace(KEY::kKP_Plus, IconTexture(L"NumPadPlus"sv));
		keyboard.emplace(KEY::kKP_Enter, IconTexture(L"KeypadEnter"sv));

		// Gamepad
		constexpr uint32_t gpOffset = SKSE::InputMap::kMacro_GamepadOffset;
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_START, GamepadIcon{ IconTexture(L"360_Start"sv), IconTexture(L"PS3_Start"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_BACK, GamepadIcon{ IconTexture(L"360_Back"sv), IconTexture(L"PS3_Back"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_LEFT_THUMB, GamepadIcon{ IconTexture(L"360_LS"sv), IconTexture(L"PS3_L3"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_RIGHT_THUMB, GamepadIcon{ IconTexture(L"360_RS"sv), IconTexture(L"PS3_R3"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_LEFT_SHOULDER, GamepadIcon{ IconTexture(L"360_LB"sv), IconTexture(L"PS3_LB"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_RIGHT_SHOULDER, GamepadIcon{ IconTexture(L"360_RB"sv), IconTexture(L"PS3_RB"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_A, GamepadIcon{ IconTexture(L"360_A"sv), IconTexture(L"PS3_A"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_B, GamepadIcon{ IconTexture(L"360_B"sv), IconTexture(L"PS3_B"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_X, GamepadIcon{ IconTexture(L"360_X"sv), IconTexture(L"PS3_X"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_Y, GamepadIcon{ IconTexture(L"360_Y"sv), IconTexture(L"PS3_Y"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_LT, GamepadIcon{ IconTexture(L"360_LT"sv), IconTexture(L"PS3_LT"sv) });
		gamePad.emplace(gpOffset + SKSE::InputMap::kGamepadButtonOffset_RT, GamepadIcon{ IconTexture(L"360_RT"sv), IconTexture(L"PS3_RT"sv) });

		// Mouse
		mouse.emplace(256 + MOUSE::kLeftButton, IconTexture(L"Mouse1"sv));
		mouse.emplace(256 + MOUSE::kRightButton, IconTexture(L"Mouse2"sv));
		mouse.emplace(256 + MOUSE::kMiddleButton, IconTexture(L"Mouse3"sv));
		mouse.emplace(256 + MOUSE::kButton3, IconTexture(L"Mouse4"sv));
		mouse.emplace(256 + MOUSE::kButton4, IconTexture(L"Mouse5"sv));
		mouse.emplace(256 + MOUSE::kButton5, IconTexture(L"Mouse6"sv));
		mouse.emplace(256 + MOUSE::kButton6, IconTexture(L"Mouse7"sv));
		mouse.emplace(256 + MOUSE::kButton7, IconTexture(L"Mouse8"sv));

		// Batch load
		std::for_each(keyboard.begin(), keyboard.end(), [](auto& Icon) { Icon.second.Load(); });
		std::for_each(gamePad.begin(), gamePad.end(), [](auto& Icon) {
			auto& [xbox, ps4] = Icon.second;
			xbox.Load();
			ps4.Load();
		});
		std::for_each(mouse.begin(), mouse.end(), [](auto& Icon) { Icon.second.Load(); });
	}

	void Manager::ResizeIcons()
	{
		// (Same as before)
		float buttonScale = ImGui::GetUserStyleVar(ImGui::USER_STYLE::kButtons);
		float checkboxScale = ImGui::GetUserStyleVar(ImGui::USER_STYLE::kCheckbox);
		float stepperScale = ImGui::GetUserStyleVar(ImGui::USER_STYLE::kStepper);

		unknownKey.Resize(buttonScale);
		upKey.Resize(buttonScale);
		downKey.Resize(buttonScale);
		leftKey.Resize(buttonScale);
		rightKey.Resize(buttonScale);

		std::for_each(keyboard.begin(), keyboard.end(), [&](auto& Icon) { Icon.second.Resize(buttonScale); });
		std::for_each(gamePad.begin(), gamePad.end(), [&](auto& Icon) {
			auto& [xbox, ps4] = Icon.second;
			xbox.Resize(buttonScale);
			ps4.Resize(buttonScale);
		});
		std::for_each(mouse.begin(), mouse.end(), [&](auto& Icon) { Icon.second.Resize(buttonScale); });

		stepperLeft.Resize(stepperScale);
		stepperRight.Resize(stepperScale);
		checkbox.Resize(checkboxScale);
		checkboxFilled.Resize(checkboxScale);
	}

	ImFont* Manager::GetLargeFont() const { return largeFont; }
	ImFont* Manager::GetRegularFont() const { return regularFont; }
	const IconTexture* Manager::GetStepperLeft() const { return &stepperLeft; }
	const IconTexture* Manager::GetStepperRight() const { return &stepperRight; }
	const IconTexture* Manager::GetCheckbox() const { return &checkbox; }
	const IconTexture* Manager::GetCheckboxFilled() const { return &checkboxFilled; }

	const IconTexture* Manager::GetIcon(std::uint32_t key)
	{
		constexpr uint32_t gpOffset = static_cast<uint32_t>(SKSE::InputMap::kMacro_GamepadOffset);
		constexpr uint32_t msOffset = static_cast<uint32_t>(SKSE::InputMap::kMacro_MouseButtonOffset);

		switch (key) {
		case KEY::kUp:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_UP:
			return &upKey;
		case KEY::kDown:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_DOWN:
			return &downKey;
		case KEY::kLeft:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_LEFT:
			return &leftKey;
		case KEY::kRight:
		case gpOffset + SKSE::InputMap::kGamepadButtonOffset_DPAD_RIGHT:
			return &rightKey;
		}

		if (key >= gpOffset) {
			if (const auto it = gamePad.find(key); it != gamePad.end()) {
				return GetGamePadIcon(it->second);
			}
		} else if (key >= msOffset) {
			if (const auto it = mouse.find(key); it != mouse.end()) {
				return &it->second;
			}
		} else {
			if (const auto it = keyboard.find(static_cast<KEY>(key)); it != keyboard.end()) {
				return &it->second;
			}
		}

		return &unknownKey;
	}

	std::set<const IconTexture*> Manager::GetIcons(const std::set<std::uint32_t>& keys)
	{
		std::set<const IconTexture*> icons{};
		if (keys.empty()) {
			icons.insert(&unknownKey);
		} else {
			for (auto& key : keys) {
				icons.insert(GetIcon(key));
			}
		}
		return icons;
	}

	const IconTexture* Manager::GetGamePadIcon(const GamepadIcon& a_icons) const
	{
		switch (buttonScheme) {
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
