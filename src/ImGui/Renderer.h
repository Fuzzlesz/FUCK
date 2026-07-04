#pragma once

namespace ImGui::Renderer
{
	inline std::atomic initialized{ false };

	namespace DisplayTweaks
	{
		inline float resolutionScale{ 1.0f };
		inline bool  borderlessUpscale{ false };
	}

	float GetResolutionScale();

	void LoadSettings(const CSimpleIniA& a_ini);
	void Install();

#ifdef SKYRIMVR
	// ImGuiVRHelper (VR overlay) client. Connect at kPostPostLoad, by which point
	// the helper has registered its handshake listener regardless of load order.
	void ConnectVRHelper();
	bool IsVRHelperConnected();
#endif
}
