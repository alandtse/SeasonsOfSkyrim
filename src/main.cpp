#include "FormSwap.h"
#include "LODSwap.h"
#include "LandscapeSwap.h"
#include "SeasonManager.h"

void MessageHandler(SKSE::MessagingInterface::Message* a_message)
{
	switch (a_message->type) {
	case SKSE::MessagingInterface::kPostLoad:
		{
			SeasonManager::GetSingleton()->LoadSettings();

	        logger::info("{:*^30}", "HOOKS");

			SeasonManager::InstallHooks();

	        FormSwap::Install();
			LandscapeSwap::Install();
			LODSwap::Install();
		}
		break;
	case SKSE::MessagingInterface::kDataLoaded:
		{
			Cache::DataHolder::GetSingleton()->GetData();

			const auto manager = SeasonManager::GetSingleton();
			manager->LoadOrGenerateWinterFormSwap();
			manager->LoadFormSwaps();
			manager->RegisterEvents();
			manager->CleanupSerializedSeasonList();
		}
		break;
	case SKSE::MessagingInterface::kSaveGame:
		{
			std::string_view savePath = { static_cast<char*>(a_message->data), a_message->dataLen };
			SeasonManager::GetSingleton()->SaveSeason(savePath);
		}
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		{
			std::string savePath({ static_cast<char*>(a_message->data), a_message->dataLen });
			string::replace_last_instance(savePath, ".ess", "");

			SeasonManager::GetSingleton()->LoadSeason(savePath);
		}
		break;
	case SKSE::MessagingInterface::kDeleteGame:
		{
			std::string_view savePath = { static_cast<char*>(a_message->data), a_message->dataLen };
			SeasonManager::GetSingleton()->ClearSeason(savePath);
		}
		break;
	default:
		break;
	}
}

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = []() {
	SKSE::PluginVersionData v;
	v.PluginVersion(Version::MAJOR);
	v.PluginName("Seasons Of Skyrim");
	v.AuthorName("powerofthree");
	v.UsesAddressLibrary(true);
	v.CompatibleVersions({ SKSE::RUNTIME_LATEST });

	return v;
}();

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
	spdlog::set_pattern("[%H:%M:%S] [%l] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);
}

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Load(const SKSE::LoadInterface* a_skse)
{
	InitializeLog();

	logger::info("loaded");

    SKSE::Init(a_skse);

	const auto messaging = SKSE::GetMessagingInterface();
	messaging->RegisterListener(MessageHandler);

	return true;
}
