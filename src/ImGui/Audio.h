#pragma once

namespace ImGui
{
	enum class Audio
	{
		kOk,
		kCancel,
		kFocus,
		kPrevNext,
		kTab
	};

	void PlayAudio(Audio a_action);
}
