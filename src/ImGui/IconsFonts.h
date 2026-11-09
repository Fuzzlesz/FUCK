#pragma once

#include "GameFonts.h"
#include "Graphics.h"

namespace IconFont
{
	struct IconTexture final : ImGui::Texture
	{
		IconTexture() = delete;
		IconTexture(std::wstring_view a_iconName);

		~IconTexture() override = default;

		bool Load(bool a_resizeToScreenRes = false) override;

		// members
		ImVec2 imageSize{};
	};

	class Manager final : public REX::Singleton<Manager>
	{
	public:
		enum class IconSet
		{
			kFontAwesome,
			kGameIcons
		};

		struct GamepadIcon
		{
			IconTexture xbox;
			IconTexture ps4;
		};

		void LoadIcons();

		// Called internally, but now just sets a flag
		void ReloadFonts();
		// Actually performs the heavy lifting, called at start of frame
		bool ProcessPendingReload();

		void        SetFontName(const std::string& a_fontName);
		std::string GetFontName() const { return _fontName; }

		// Uses the game's own font (GameFonts) in place of the chosen typeface, whenever it can be read
		void SetGameFont(bool a_use);
		bool GetGameFont() const { return _gameFont; }

		// How much wider the regular font's text runs than the built-in typeface's, from 1 to 1.5, so the main window can open wider for
		// a wide font. 1 for a typeface chosen from the list, as it can't be measured
		float GetTextWidthScale() const { return _textWidthScale; }

		ImFont* GetLargeFont() const;
		ImFont* GetRegularFont() const;

		// The game font's light and bold faces. Where the typeface has no separate face, as with the built-in one or a theme with a single
		// font, these return the regular font
		ImFont* GetLightFont() const { return _lightFont ? _lightFont : _regularFont; }
		ImFont* GetBoldFont() const { return _boldFont ? _boldFont : _regularFont; }

		// The regular font with the game-icons.net icons in place of Font Awesome's, as the two share codes. The regular font when
		// game-icons.ttf isn't installed
		ImFont* GetGameIconsFont() const { return _gameIconsFont ? _gameIconsFont : _regularFont; }

		const IconTexture* GetStepperRight() const;
		const IconTexture* GetCheckbox() const;
		const IconTexture* GetCheckboxFilled() const;

		const IconTexture*           GetIcon(std::uint32_t key);
		std::set<const IconTexture*> GetIcons(const std::set<std::uint32_t>& keys);

		const IconTexture* GetGamePadIcon(const GamepadIcon& a_icons) const;

	private:
		ImFont* LoadFontIconSet(float a_fontSize, float a_iconSize, const ImVector<ImWchar>& a_ranges, GameFonts::Face a_face = GameFonts::Face::kMedium, IconSet a_icons = IconSet::kFontAwesome) const;
		// Internal helper that actually clears the atlas
		void RebuildFontAtlas();

		std::string ResolveFontPath(const std::string& a_filename) const;

		// members
		bool _loadedFonts{ false };
		bool _pendingReload{ false };  // Flag for deferred reload

		std::string _fontName{ "Default" };
		bool        _gameFont{ true };
		float       _textWidthScale{ 1.0f };

		float _baseFontSize{ 30.0f };       // was 24.0f
		float _baseIconSize{ 24.0f };       // was 26.0f
		float _baseLargeFontSize{ 34.0f };  // was 26.0f
		float _baseLargeIconSize{ 29.0f };  // was 31.0f

		ImFont* _largeFont{ nullptr };
		ImFont* _regularFont{ nullptr };
		ImFont* _lightFont{ nullptr };
		ImFont* _boldFont{ nullptr };
		ImFont* _gameIconsFont{ nullptr };

		IconTexture _stepperRight{ L"StepperRight"sv };
		IconTexture _checkbox{ L"Checkbox"sv };
		IconTexture _checkboxFilled{ L"Checkbox-Filled"sv };

		IconTexture _unknownKey{ L"UnknownKey"sv };

		IconTexture _leftKey{ L"Left"sv };
		IconTexture _rightKey{ L"Right"sv };
		IconTexture _upKey{ L"Up"sv };
		IconTexture _downKey{ L"Down"sv };

		Map<KEY, IconTexture>           _keyboard;
		Map<std::uint32_t, GamepadIcon> _gamePad;
		Map<std::uint32_t, IconTexture> _mouse;
	};
}
