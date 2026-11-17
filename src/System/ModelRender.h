#pragma once

// Draws a loaded model off screen and saves it to a .png, for FUCK::RenderModel(). Taken at the start of FUCK's draw, as the model's
// buffers are read on the render thread, and written out on a background thread
namespace ModelRender
{
	struct Request
	{
		RE::NiPointer<RE::NiAVObject> model;
		std::wstring                  path;
		std::string                   pathUtf8;  // handed back to the callback
		std::uint32_t                 pixels      = 256;
		float                         yaw         = 0.0f;  // degrees, turning the model about its up axis
		float                         pitch       = 0.0f;  // degrees, the camera looking down on it
		float                         margin      = 1.05f;
		bool                          transparent = false;
		void*                         userdata    = nullptr;
		void (*callback)(const char*, bool, void*) = nullptr;

		struct TintLayer
		{
			std::string          mask;
			std::array<float, 4> colour{};
		};
		std::vector<TintLayer> faceTint;
	};

	void Queue(Request a_request);

	void OnFrame(ID3D11Device* a_device, ID3D11DeviceContext* a_context);

	// A caller's own meshes drawn at once with the same look, for FUCK::DrawMeshes(). On the render thread only
	bool DrawMeshes(const FUCK::MeshDrawRequest& a_request);
}
