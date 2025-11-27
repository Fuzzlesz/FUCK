#include "FUCKHost.h"
#include "FUCKMan.h"
#include "System/Console.h"
#include "System/Compat.h"
#include "System/Hooks.h"
#include "System/Hotkeys.h"
#include "System/Input.h"
#include "System/Papyrus.h"
#include "System/Settings.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"


// ==========================================
// Exports
// ==========================================

extern "C" DLLEXPORT void* RequestFUCK()
{
	return FUCK::Host::CreateInterface();
}

// ==========================================
// Initialization
// ==========================================

void OnInit(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		if (auto ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(FUCKMan::GetSingleton());
		}
		Translation::Manager::GetSingleton()->BuildTranslationMap();
		Console::Install();
		Compat::ImmersiveHUD::Initialize(); 
		ImGui::Styles::GetSingleton()->OnStyleRefresh();
		logger::info("FUCK Data Loaded");
		break;

	case SKSE::MessagingInterface::kPostLoadGame:
	case SKSE::MessagingInterface::kNewGame:
		MANAGER(Hotkeys)->Enable(true);
		logger::info("FUCK Hotkeys Enabled");
		break;
	}
}

#ifdef SKYRIM_AE
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("FUCK");
	v.AuthorName("Fuzzles");
	v.UsesAddressLibrary();
	v.UsesUpdatedStructs();
	v.CompatibleVersions({ SKSE::RUNTIME_SSE_LATEST });

	return v;
}();
#else
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
	a_info->infoVersion = SKSE::PluginInfo::kVersion;
	a_info->name = "FUCK";
	a_info->version = Version::MAJOR;

	if (a_skse->IsEditor()) {
		logger::critical("Loaded in editor, marking as incompatible"sv);
		return false;
	}

	const auto ver = a_skse->RuntimeVersion();
	if (ver < SKSE::RUNTIME_SSE_1_5_39) {
		logger::critical(FMT_STRING("Unsupported runtime version {}"), ver.string());
		return false;
	}

	return true;
}
#endif

void InitializeLog()
{
	auto path = logger::log_directory();
	if (!path) {
		stl::report_and_fail("Failed to find standard logging directory"sv);
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::info);

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("[%H:%M:%S] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();
	SKSE::Init(a_skse, false);

	auto settings = Settings::GetSingleton();

	// Load Display Tweaks settings for scaling
	settings->Load(FileType::kDisplayTweaks, [](auto& ini) {
		ImGui::Renderer::LoadSettings(ini);
	});

	settings->Load(FileType::kSettings, [](auto& ini) {
		MANAGER(Hotkeys)->LoadHotKeys(ini);
		FUCKMan::GetSingleton()->LoadSettings(ini);
		MANAGER(Input)->LoadSettings(ini);
	});

	ImGui::Styles::GetSingleton()->LoadStyles();

	SKSE::AllocTrampoline(14 * 3);
	Hooks::Install();
	ImGui::Renderer::Install();

	SKSE::GetPapyrusInterface()->Register(Papyrus::Register);

	const auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener(OnInit))
		return false;

	return true;
}
