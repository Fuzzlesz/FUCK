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

		void LoadIcons();

		// Called internally, but now just sets a flag
		void ReloadFonts();
		// Actually performs the heavy lifting, called at start of frame
		bool ProcessPendingReload();

		void        SetFontName(const std::string& a_fontName);
		std::string GetFontName() const { return _fontName; }

		ImFont* GetLargeFont() const;
		ImFont* GetRegularFont() const;

		const IconTexture* GetStepperRight() const;
		const IconTexture* GetCheckbox() const;
		const IconTexture* GetCheckboxFilled() const;

		const IconTexture*           GetIcon(std::uint32_t key);
		std::set<const IconTexture*> GetIcons(const std::set<std::uint32_t>& keys);

		const IconTexture* GetGamePadIcon(const GamepadIcon& a_icons) const;

	private:
		ImFont* LoadFontIconSet(float a_fontSize, float a_iconSize, const ImVector<ImWchar>& a_ranges) const;
		// Internal helper that actually clears the atlas
		void RebuildFontAtlas();

		std::string ResolveFontPath(const std::string& a_filename) const;

		// members
		bool _loadedFonts{ false };
		bool _pendingReload{ false };  // Flag for deferred reload

		std::string _fontName{ "Default" };

		float _baseFontSize{ 30.0f };       // was 24.0f
		float _baseIconSize{ 26.0f };       // was 20.0f
		float _baseLargeFontSize{ 34.0f };  // was 26.0f
		float _baseLargeIconSize{ 31.0f };  // was 24.0f

		ImFont* _largeFont{ nullptr };
		ImFont* _regularFont{ nullptr };

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
