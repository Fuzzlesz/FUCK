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

	namespace Capture
	{
		namespace
		{
			std::mutex           g_lock;
			std::vector<Request> g_pending;

			void Finish(const Request& a_request, bool a_success)
			{
				if (!a_success)
					logger::warn("CaptureFrame: couldn't save {}", a_request.pathUtf8);
				if (a_request.callback)
					a_request.callback(a_request.pathUtf8.c_str(), a_success, a_request.userdata);
			}

			// Scaled, made opaque and saved off the render thread, as encoding a .png takes a while
			void Write(Request a_request, std::shared_ptr<DirectX::ScratchImage> a_pixels)
			{
				std::thread([request = std::move(a_request), pixels = std::move(a_pixels)]() mutable {
					const bool com = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_MULTITHREADED));

					bool ok = true;
					if (pixels->GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM) {
						auto converted = std::make_shared<DirectX::ScratchImage>();
						ok             = SUCCEEDED(DirectX::Convert(*pixels->GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, *converted));
						pixels         = std::move(converted);
					}

					// The frame's alpha is whatever the game left in it, so it's dropped
					if (ok) {
						const auto* image = pixels->GetImage(0, 0, 0);
						for (std::size_t y = 0; y < image->height; ++y) {
							auto* row = image->pixels + y * image->rowPitch;
							for (std::size_t x = 0; x < image->width; ++x)
								row[x * 4 + 3] = 0xFF;
						}
					}

					const auto outW = static_cast<std::size_t>(request.outSize.x);
					const auto outH = static_cast<std::size_t>(request.outSize.y);
					if (ok && outW > 0 && outH > 0 && (outW != pixels->GetMetadata().width || outH != pixels->GetMetadata().height)) {
						auto resized = std::make_shared<DirectX::ScratchImage>();
						ok           = SUCCEEDED(DirectX::Resize(*pixels->GetImage(0, 0, 0), outW, outH, DirectX::TEX_FILTER_FANT, *resized));
						pixels       = std::move(resized);
					}

					if (ok) {
						std::error_code             ec;
						const std::filesystem::path path{ request.path };
						std::filesystem::create_directories(path.parent_path(), ec);
						ok = SUCCEEDED(DirectX::SaveToWICFile(*pixels->GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path.c_str()));
					}

					Finish(request, ok);
					if (com)
						::CoUninitialize();
				}).detach();
			}
		}

		void Queue(Request a_request)
		{
			std::scoped_lock lock(g_lock);
			g_pending.push_back(std::move(a_request));
		}

		void OnFrame(ID3D11Device* a_device, ID3D11DeviceContext* a_context)
		{
			std::vector<Request> requests;
			{
				std::scoped_lock lock(g_lock);
				if (g_pending.empty())
					return;
				requests.swap(g_pending);
			}

			// What FUCK's UI is about to be drawn onto, so it's the frame as shown, less the UI
			ComPtr<ID3D11Texture2D> target;
			{
				ID3D11RenderTargetView* rtv = nullptr;
				a_context->OMGetRenderTargets(1, &rtv, nullptr);
				if (rtv) {
					ComPtr<ID3D11Resource> resource;
					rtv->GetResource(resource.GetAddressOf());
					resource.As(&target);
					rtv->Release();
				}
			}

			D3D11_TEXTURE2D_DESC desc{};
			if (target)
				target->GetDesc(&desc);
			if (!target || desc.SampleDesc.Count > 1) {
				for (const auto& request : requests)
					Finish(request, false);
				return;
			}

			// The display's pixels to the target's, as the UI can be drawn at another size than the frame
			const ImVec2 display = GetIO().DisplaySize;
			const float  sx      = display.x > 0.0f ? static_cast<float>(desc.Width) / display.x : 1.0f;
			const float  sy      = display.y > 0.0f ? static_cast<float>(desc.Height) / display.y : 1.0f;

			// A typeless or sRGB target is read as its plain format, as the bytes are what's shown. Anything wider is converted later
			DXGI_FORMAT format = desc.Format;
			if (DirectX::IsTypeless(format))
				format = DirectX::MakeTypelessUNORM(format);
			else if (DirectX::IsSRGB(format))
				format = DirectX::MakeLinear(format);

			for (auto& request : requests) {
				const bool whole = request.max.x <= request.min.x || request.max.y <= request.min.y;
				const auto x0    = whole ? 0u : static_cast<UINT>(std::clamp(request.min.x * sx, 0.0f, static_cast<float>(desc.Width)));
				const auto y0    = whole ? 0u : static_cast<UINT>(std::clamp(request.min.y * sy, 0.0f, static_cast<float>(desc.Height)));
				const auto x1    = whole ? desc.Width : static_cast<UINT>(std::clamp(request.max.x * sx, 0.0f, static_cast<float>(desc.Width)));
				const auto y1    = whole ? desc.Height : static_cast<UINT>(std::clamp(request.max.y * sy, 0.0f, static_cast<float>(desc.Height)));
				if (x1 <= x0 || y1 <= y0) {
					Finish(request, false);
					continue;
				}

				D3D11_TEXTURE2D_DESC stagingDesc{};
				stagingDesc.Width            = x1 - x0;
				stagingDesc.Height           = y1 - y0;
				stagingDesc.MipLevels        = 1;
				stagingDesc.ArraySize        = 1;
				stagingDesc.Format           = desc.Format;
				stagingDesc.SampleDesc.Count = 1;
				stagingDesc.Usage            = D3D11_USAGE_STAGING;
				stagingDesc.CPUAccessFlags   = D3D11_CPU_ACCESS_READ;

				ComPtr<ID3D11Texture2D> staging;
				if (FAILED(a_device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf()))) {
					Finish(request, false);
					continue;
				}

				const D3D11_BOX box{ x0, y0, 0, x1, y1, 1 };
				a_context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, target.Get(), 0, &box);

				// Read back at once, which waits on the GPU. Fine for the odd capture
				auto                     pixels = std::make_shared<DirectX::ScratchImage>();
				D3D11_MAPPED_SUBRESOURCE mapped{};
				bool                     ok     = SUCCEEDED(pixels->Initialize2D(format, stagingDesc.Width, stagingDesc.Height, 1, 1)) &&
				                                  SUCCEEDED(a_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
				if (ok) {
					const auto* image = pixels->GetImage(0, 0, 0);
					const auto  bytes = std::min<std::size_t>(image->rowPitch, mapped.RowPitch);
					for (UINT y = 0; y < stagingDesc.Height; ++y)
						std::memcpy(image->pixels + y * image->rowPitch, static_cast<const std::uint8_t*>(mapped.pData) + y * mapped.RowPitch, bytes);
					a_context->Unmap(staging.Get(), 0);
				}

				if (!ok) {
					Finish(request, false);
					continue;
				}
				Write(std::move(request), std::move(pixels));
			}
		}
	}
}
