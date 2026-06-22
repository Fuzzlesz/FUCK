#include "FUCK-Man.h"

#include "IconsFonts.h"
#include "Renderer.h"
#include "Styles.h"

#include "System\Input.h"

namespace ImGui::Renderer
{
	float GetResolutionScale()
	{
		const auto height = RE::BSGraphics::Renderer::GetScreenSize().height;
		return DisplayTweaks::borderlessUpscale ? DisplayTweaks::resolutionScale : static_cast<float>(height) / 1080.0f;
	}

	void LoadSettings(const CSimpleIniA& a_ini)
	{
		DisplayTweaks::resolutionScale   = static_cast<float>(a_ini.GetDoubleValue("Render", "ResolutionScale", static_cast<double>(DisplayTweaks::resolutionScale)));
		DisplayTweaks::borderlessUpscale = a_ini.GetBoolValue("Render", "BorderlessUpscale", DisplayTweaks::borderlessUpscale);
	}

	// ==================================================
	// HELPER
	// ==================================================

	void Draw()
	{
		if (!initialized.load()) {
			return;
		}

		Styles::GetSingleton()->OnStyleRefresh();
		IconFont::Manager::GetSingleton()->ProcessPendingReload();

		const auto manager = FUCKMan::GetSingleton();

		if (!manager->ShouldRender()) {
			return;
		}

		ImGui_ImplDX11_NewFrame();
		SKSE::ImGui_ImplSkyrim_NewFrame();
		NewFrame();
		{
			// disable windowing
			GImGui->NavWindowingTarget = nullptr;

			manager->Draw();
		}
		EndFrame();
		Render();
		ImGui_ImplDX11_RenderDrawData(GetDrawData());
	}

	// ==================================================
	// HOOKS
	// ==================================================

	struct WndProc
	{
		static LRESULT thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			// Handle focus loss
			if (uMsg == WM_KILLFOCUS) {
				// Clear ImGui's internal key buffer
				auto& io = GetIO();
				io.ClearInputKeys();

				// Clear our Input Manager's stuck keys
				if (initialized.load()) {
					Input::Manager::GetSingleton()->ClearState();
				}
			}

			// Defensively call the original/next WndProc in the chain
			if (func) {
				MEMORY_BASIC_INFORMATION mbi;
				// Query the memory region of the function pointer
				if (VirtualQuery(reinterpret_cast<LPCVOID>(func), &mbi, sizeof(mbi))) {
					// Check if the memory is committed and holds executable code
					if (mbi.State == MEM_COMMIT &&
						(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) {
						// Safe to call. Use CallWindowProc for proper subclass chaining.
						return CallWindowProc(func, hWnd, uMsg, wParam, lParam);
					}
				}

				static bool s_LoggedCorruption = false;
				if (!s_LoggedCorruption) {
					logger::critical("WndProc chain corruption detected! Falling back to DefWindowProc.");
					s_LoggedCorruption = true;
				}
			}

			// If func is null or points to dead memory, fallback to OS default
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
		static inline WNDPROC func;
	};

	struct CreateD3DAndSwapChain
	{
		static void thunk()
		{
			func();

			if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
				const auto swapChain = reinterpret_cast<IDXGISwapChain*>(renderer->data.renderWindows[0].swapChain);
				if (!swapChain) {
					logger::error("couldn't find swapChain");
					return;
				}

				DXGI_SWAP_CHAIN_DESC desc{};
				if (FAILED(swapChain->GetDesc(std::addressof(desc)))) {
					logger::error("IDXGISwapChain::GetDesc failed.");
					return;
				}

				const auto device  = reinterpret_cast<ID3D11Device*>(renderer->data.forwarder);
				const auto context = reinterpret_cast<ID3D11DeviceContext*>(renderer->data.context);

				logger::info("Initializing ImGui..."sv);

				CreateContext();

				auto& io       = GetIO();
				io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NoMouseCursorChange;
				io.IniFilename = nullptr;

				if (!SKSE::ImGui_ImplSkyrim_Init()) {
					logger::error("ImGui initialization failed (Skyrim platform)");
					return;
				}
				if (!ImGui_ImplDX11_Init(device, context)) {
					logger::error("ImGui initialization failed (DX11)"sv);
					return;
				}

				MANAGER(IconFont)->LoadIcons();
				MANAGER(IconFont)->ReloadFonts();

				auto styles = Styles::GetSingleton();
				styles->LoadStyles();

				logger::info("ImGui initialized.");

				initialized.store(true);

				WndProc::func = reinterpret_cast<WNDPROC>(
					SetWindowLongPtrA(
						desc.OutputWindow,
						GWLP_WNDPROC,
						reinterpret_cast<LONG_PTR>(WndProc::thunk)));
				if (!WndProc::func) {
					logger::error("SetWindowLongPtrA failed!");
				}
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// IMenu::PostDisplay (HUDMenu)
	struct HUDMenu_PostDisplay
	{
		static void thunk(RE::IMenu* a_menu)
		{
			auto ui = RE::UI::GetSingleton();
			// Only draw ImGui on the HUDMenu if the CursorMenu isn't going to do it for us
			if (!ui || !ui->IsMenuOpen(RE::CursorMenu::MENU_NAME)) {
				Draw();
			}
			return func(a_menu);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::size_t                      idx{ 0x6 };
	};

	// IMenu::PostDisplay (CursorMenu)
	struct CursorMenu_PostDisplay
	{
		static void thunk(RE::IMenu* a_menu)
		{
			Draw();
			return func(a_menu);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::size_t                      idx{ 0x6 };
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(75595, 77226), OFFSET(0x9, 0x275) };
		stl::write_thunk_call<CreateD3DAndSwapChain>(target.address());

		stl::write_vfunc<RE::HUDMenu, HUDMenu_PostDisplay>();
		stl::write_vfunc<RE::CursorMenu, CursorMenu_PostDisplay>();
	}
}
