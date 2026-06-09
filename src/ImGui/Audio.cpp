#include "FUCK-Man.h"

#include "ImGui/Audio.h"
#include "ImGui/Styles.h"

namespace ImGui
{
	void PlayAudio(Audio a_action)
	{
		if (FUCKMan::GetSingleton()->GetMuteAudio())
			return;

		auto        style = Styles::GetSingleton();
		const char* sound = nullptr;

		switch (a_action) {
		case Audio::kOk:
			sound = style->user.soundOk.c_str();
			break;
		case Audio::kCancel:
			sound = style->user.soundCancel.c_str();
			break;
		case Audio::kFocus:
			sound = style->user.soundFocus.c_str();
			break;
		case Audio::kPrevNext:
			sound = style->user.soundPrevNext.c_str();
			break;
		case Audio::kTab:
			sound = style->user.soundTab.c_str();
			break;
		}

		if (sound && sound[0] != '\0') {
			RE::PlaySound(sound);
		}
	}
}
