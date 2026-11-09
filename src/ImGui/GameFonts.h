#pragma once

// Reads the fonts the game's menus use, as fontconfig.txt maps them, and turns them into TrueType fonts ImGui can load, so FUCK's text
// matches the game's, whatever UI mod or language sets it
namespace GameFonts
{
	enum class Face
	{
		kLight,   // $EverywhereFont
		kMedium,  // $EverywhereMediumFont
		kBold     // $EverywhereBoldFont
	};

	struct Font
	{
		std::string                name;    // The font's name in its library
		std::vector<std::uint8_t>  ttf;     // Empty if the font couldn't be found
		std::vector<ImWchar>       ranges;  // Every character the font has, for ImGui to load
	};

	// Gives every game font the built-in typeface's ascent to descent per em. ImGui sizes a font by that span while the game sizes it by
	// the em, so this puts each game font at the size the game draws it, relative to FUCK's layout. Must be called before the first Get()
	void SetLineHeight(float a_perEm);

	// Reads all three faces the first time any is asked for, and keeps them
	const Font& Get(Face a_face);
}
