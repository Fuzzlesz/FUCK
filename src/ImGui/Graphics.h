#pragma once

namespace ImGui
{
	struct Texture
	{
		Texture() = delete;
		Texture(std::wstring_view a_folder, std::wstring_view a_textureName);
		Texture(std::wstring_view a_path);

		virtual ~Texture();

		virtual bool Load(bool a_resizeToScreenRes = false);

		// Getters
		[[nodiscard]] const std::wstring& GetPath() const { return path; }
		[[nodiscard]] ID3D11ShaderResourceView* GetSRView() const { return srView.Get(); }
		[[nodiscard]] const ImVec2& GetSize() const { return size; }
		[[nodiscard]] bool IsLoaded() const { return srView != nullptr; }

		// members
		std::wstring                           path{};
		ComPtr<ID3D11ShaderResourceView>       srView{ nullptr };
		std::shared_ptr<DirectX::ScratchImage> image{ nullptr };
		ImVec2                                 size{};
	};
}
