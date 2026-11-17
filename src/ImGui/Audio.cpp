#include "ImGui/Audio.h"

namespace ImGui
{
	void PlayAudio(Audio a_action)
	{
		// The game's own menu sounds, so a UI sound replacer covers FUCK as it does every other menu
		const char* sound = nullptr;
		switch (a_action) {
		case Audio::kOk:       sound = "UIMenuOK";        break;
		case Audio::kCancel:   sound = "UIMenuCancel";    break;
		case Audio::kFocus:    sound = "UIMenuFocus";     break;
		case Audio::kPrevNext: sound = "UIMenuPrevNext";  break;
		case Audio::kTab:      sound = "UIJournalTabsSD"; break;
		}
		if (sound) {
			RE::PlaySound(sound);
		}
	}
}
