#pragma once

namespace ImGui
{
	struct Texture
	{
		// Background loads report their progress here. Anything else is drawn once loaded
		enum class State : std::uint8_t
		{
			kLoading,
			kReady,
			kFailed
		};

		// Atomic, as a background thread sets it while the frame reads it. Copyable, as icon textures are copied into their maps
		struct AtomicState
		{
			AtomicState() = default;
			AtomicState(const AtomicState& a_other) :
				value(a_other.load()) {}
			AtomicState& operator=(const AtomicState& a_other)
			{
				store(a_other.load());
				return *this;
			}

			State load() const { return value.load(std::memory_order_acquire); }
			void  store(State a_state) { value.store(a_state, std::memory_order_release); }

			std::atomic<State> value{ State::kReady };
		};

		Texture() = delete;
		Texture(std::wstring_view a_folder, std::wstring_view a_textureName);
		Texture(std::wstring_view a_path);

		virtual ~Texture();

		virtual bool Load(bool a_resizeToScreenRes = false);

		// Loads at a scale, e.g. 0.25 for a thumbnail. With a cache path, a copy saved there earlier is loaded instead, and a new one is saved after loading
		bool LoadScaled(float a_scale, std::wstring_view a_cachePath = {});

		// Getters
		[[nodiscard]] const std::wstring&       GetPath() const { return path; }
		[[nodiscard]] ID3D11ShaderResourceView* GetSRView() const { return state.load() == State::kReady ? srView.Get() : nullptr; }
		[[nodiscard]] const ImVec2&             GetSize() const { return size; }
		[[nodiscard]] bool                      IsLoaded() const { return srView != nullptr; }

		// members
		std::wstring                           path{};
		ComPtr<ID3D11ShaderResourceView>       srView{ nullptr };
		std::shared_ptr<DirectX::ScratchImage> image{ nullptr };
		ImVec2                                 size{};
		AtomicState                            state{};

	private:
		bool LoadFile(const std::wstring& a_file, bool a_resizeToScreenRes, float a_scale);
		bool CreateView();
	};

	// Loads textures on background threads, so a gallery can fill in without stalling the frame
	namespace ImageLoader
	{
		struct Request
		{
			std::shared_ptr<Texture> texture;
			float                    scale = 1.0f;
			std::wstring             cachePath;
			bool                     urgent = false;
		};

		// Queues the load and returns at once. The texture's state is kLoading until it's ready or has failed
		void Queue(Request a_request);

		// Drops a load that hasn't started. One already loading finishes and is freed with its last owner
		void Cancel(const Texture* a_texture);
	}
}
