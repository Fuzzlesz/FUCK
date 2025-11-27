#pragma once

#include "Graphics.h"

namespace IconFont
{
	struct IconTexture final : ImGui::Texture
	{
		IconTexture() = delete;
		IconTexture(std::wstring_view a_iconName);

		~IconTexture() override = default;

		bool Load(bool a_resizeToScreenRes = false) override;
		void Resize(float a_scale);

		// members
		ImVec2 imageSize{};
	};

	class Manager final : public REX::Singleton<Manager>
	{
	public:
		struct GamepadIcon
		{
			IconTexture xbox;
			IconTexture ps4;
		};

		void LoadSettings(CSimpleIniA& a_ini);

		void LoadIcons();

		// Called internally, but now just sets a flag
		void ReloadFonts();
		// Actually performs the heavy lifting, called at start of frame
		bool ProcessPendingReload();

		void ResizeIcons();

		// New API for Styles integration
		void SetFontName(const std::string& a_fontName);
		std::string GetFontName() const { return fontName; }

		ImFont* GetLargeFont() const;
		ImFont* GetRegularFont() const;

		const IconTexture* GetStepperLeft() const;
		const IconTexture* GetStepperRight() const;
		const IconTexture* GetCheckbox() const;
		const IconTexture* GetCheckboxFilled() const;

		const IconTexture* GetIcon(std::uint32_t key);
		std::set<const IconTexture*> GetIcons(const std::set<std::uint32_t>& keys);

		const IconTexture* GetGamePadIcon(const GamepadIcon& a_icons) const;

	private:
		enum class BUTTON_SCHEME
		{
			kAutoDetect,
			kXbox,
			kPS4
		};

		void LoadFontSettings(CSimpleIniA& a_ini);
		ImFont* LoadFontIconSet(float a_fontSize, float a_iconSize, const ImVector<ImWchar>& a_ranges) const;
		// Internal helper that actually clears the atlas
		void RebuildFontAtlas();

		std::string ResolveFontPath(const std::string& a_filename) const;

		// members
		bool loadedFonts{ false };
		bool _pendingReload{ false };  // Flag for deferred reload

		BUTTON_SCHEME buttonScheme{ BUTTON_SCHEME::kAutoDetect };

		std::string fontName{ "Jost-Regular.ttf" };

		float baseFontSize{ 24.0f };
		float baseIconSize{ 20.0f };
		float baseLargeFontSize{ 26.0f };
		float baseLargeIconSize{ 24.0f };

		ImFont* largeFont{ nullptr };
		ImFont* regularFont{ nullptr };

		IconTexture stepperLeft{ L"StepperLeft"sv };
		IconTexture stepperRight{ L"StepperRight"sv };
		IconTexture checkbox{ L"Checkbox"sv };
		IconTexture checkboxFilled{ L"Checkbox-Filled"sv };

		IconTexture unknownKey{ L"UnknownKey"sv };

		IconTexture leftKey{ L"Left"sv };
		IconTexture rightKey{ L"Right"sv };
		IconTexture upKey{ L"Up"sv };
		IconTexture downKey{ L"Down"sv };

		Map<KEY, IconTexture> keyboard;
		Map<std::uint32_t, GamepadIcon> gamePad;
		Map<std::uint32_t, IconTexture> mouse;
	};
}
