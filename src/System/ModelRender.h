#pragma once

#include "API/FUCK_ModelRender.h"

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
		bool                   frameFace = false;  // framed on the face alone, so hair or a beard doesn't change its size
		std::uint32_t          supersample = 2;    // drawn this many times the size a side, then scaled down
	};

	void Queue(Request a_request);

	void OnFrame(ID3D11Device* a_device, ID3D11DeviceContext* a_context);

	// A caller's own meshes drawn at once with the same look, for FUCK::DrawMeshes(). On the render thread only, and refused off it once
	// FUCK has drawn a frame. The meshes are stepped through by the first one's size
	bool DrawMeshes(const FUCK::MeshDrawRequest& a_request);

	// A shape's material as these renders draw it, into a MeshDraw, for FUCK::ReadMeshMaterial(). Its whole look replaced, its size, buffers and
	// normalTurn kept. False with neither a lighting nor an effect shader, leaving it as it was
	bool ReadMeshMaterial(RE::NiProperty* a_shader, RE::NiProperty* a_alpha, FUCK::MeshDraw& a_out);
}
