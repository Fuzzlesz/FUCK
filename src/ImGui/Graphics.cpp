#include "Graphics.h"
#include "Renderer.h"

namespace ImGui
{
	Texture::Texture(std::wstring_view a_path) :
		path(a_path)
	{}

	Texture::Texture(std::wstring_view a_folder, std::wstring_view a_textureName)
	{
		path.append(a_folder).append(a_textureName).append(L".png");
	}

	Texture::~Texture()
	{
		if (srView) {
			srView.Reset();
		}
		if (image) {
			image.reset();
		}
	}

	bool Texture::Load(bool a_resizeToScreenRes)
	{
		return LoadFile(path, a_resizeToScreenRes, 1.0f) && CreateView();
	}

	bool Texture::LoadScaled(float a_scale, std::wstring_view a_cachePath)
	{
		std::error_code ec;
		const std::filesystem::path cache{ a_cachePath };

		// A copy saved earlier is already at its scale. One that won't load is removed and made again
		if (!cache.empty() && std::filesystem::exists(cache, ec)) {
			if (LoadFile(cache.wstring(), false, 1.0f) && CreateView()) {
				image.reset();
				return true;
			}
			std::filesystem::remove(cache, ec);
		}

		if (!LoadFile(path, false, a_scale)) {
			return false;
		}

		if (!cache.empty()) {
			std::filesystem::create_directories(cache.parent_path(), ec);
			if (FAILED(DirectX::SaveToWICFile(*image->GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), cache.c_str()))) {
				std::filesystem::remove(cache, ec);
			}
		}

		// The pixels are only needed until the texture is made, and a full-size photo's are large
		const bool result = CreateView();
		image.reset();
		return result;
	}

	bool Texture::LoadFile(const std::wstring& a_file, bool a_resizeToScreenRes, float a_scale)
	{
		// RGB byte order, as Photo Mode loads, so pixels match what its screenshot code works with
		image = std::make_shared<DirectX::ScratchImage>();
		if (FAILED(DirectX::LoadFromWICFile(a_file.c_str(), DirectX::WIC_FLAGS_FORCE_RGB | DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, *image))) {
			image.reset();
			return false;
		}

		const auto& meta = image->GetMetadata();
		std::size_t width  = 0;
		std::size_t height = 0;

		if (a_resizeToScreenRes) {
			// The helper panel is a fixed logical canvas, unrelated to the
			// HMD's native per-eye render resolution (see GetResolutionScale).
			auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();
			if (ImGui::Renderer::IsVRHelperConnected()) {
				const auto& displaySize = ImGui::GetIO().DisplaySize;
				screenSize.width         = static_cast<std::uint32_t>(displaySize.x);
				screenSize.height        = static_cast<std::uint32_t>(displaySize.y);
			}
			if (screenSize.height != meta.height && screenSize.width != meta.width) {
				width  = screenSize.width;
				height = screenSize.height;
			}
		} else if (a_scale > 0.0f && a_scale != 1.0f) {
			width  = std::max<std::size_t>(1, static_cast<std::size_t>(meta.width * a_scale));
			height = std::max<std::size_t>(1, static_cast<std::size_t>(meta.height * a_scale));
		}

		if (width > 0 && height > 0) {
			// Fant for shrinking, as it averages every pixel rather than skipping some
			const auto filter = a_resizeToScreenRes ? DirectX::TEX_FILTER_CUBIC : DirectX::TEX_FILTER_FANT;
			auto       resized = std::make_shared<DirectX::ScratchImage>();
			if (SUCCEEDED(DirectX::Resize(*image->GetImage(0, 0, 0), width, height, filter, *resized))) {
				image = std::move(resized);
			}
		}

		return true;
	}

	bool Texture::CreateView()
	{
		auto renderer = RE::BSGraphics::Renderer::GetSingleton();
		if (!renderer || !image) {
			return false;
		}

		// The texture and its view in one call, as Photo Mode makes them
		const auto device = reinterpret_cast<ID3D11Device*>(renderer->GetRuntimeData().forwarder);
		const bool result = SUCCEEDED(DirectX::CreateShaderResourceView(device, image->GetImages(), 1, image->GetMetadata(), srView.ReleaseAndGetAddressOf()));

		size.x = static_cast<float>(image->GetMetadata().width);
		size.y = static_cast<float>(image->GetMetadata().height);
		return result;
	}

	namespace ImageLoader
	{
		namespace
		{
			// Started with the first request and kept for the session, waiting while there's nothing to load
			class Workers
			{
			public:
				static Workers& Get()
				{
					static Workers workers;
					return workers;
				}

				void Queue(Request&& a_request)
				{
					Start();
					{
						std::scoped_lock lock(_lock);
						// What's being looked at goes ahead of what's being fetched in advance
						if (a_request.urgent) {
							_queue.push_front(std::move(a_request));
						} else {
							_queue.push_back(std::move(a_request));
						}
					}
					_wake.notify_one();
				}

				void Cancel(const Texture* a_texture)
				{
					std::scoped_lock lock(_lock);
					std::erase_if(_queue, [&](const Request& a_request) { return a_request.texture.get() == a_texture; });
				}

			private:
				void Start()
				{
					std::call_once(_started, [this] {
						// Below normal priority and a few at most, so loading never competes with the game
						const auto count = std::max(2u, std::thread::hardware_concurrency() / 4);
						for (std::uint32_t i = 0; i < count; ++i) {
							_threads.emplace_back([this](std::stop_token a_stop) { Run(a_stop); });
						}
					});
				}

				void Run(std::stop_token a_stop)
				{
					::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

					// WIC decodes the files, and needs COM on every thread that uses it
					const bool com = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));

					while (!a_stop.stop_requested()) {
						Request request;
						{
							std::unique_lock lock(_lock);
							if (!_wake.wait(lock, a_stop, [this] { return !_queue.empty(); })) {
								break;
							}
							request = std::move(_queue.front());
							_queue.pop_front();
						}

						auto& texture = *request.texture;
						const bool loaded = texture.LoadScaled(request.scale, request.cachePath);
						texture.state.store(loaded ? Texture::State::kReady : Texture::State::kFailed);
					}

					if (com) {
						::CoUninitialize();
					}
				}

				std::mutex                  _lock;
				std::condition_variable_any _wake;
				std::deque<Request>         _queue;
				std::once_flag              _started;
				std::vector<std::jthread>   _threads;
			};
		}

		void Queue(Request a_request)
		{
			if (!a_request.texture) {
				return;
			}

			a_request.texture->state.store(Texture::State::kLoading);
			Workers::Get().Queue(std::move(a_request));
		}

		void Cancel(const Texture* a_texture)
		{
			Workers::Get().Cancel(a_texture);
		}
	}
}
