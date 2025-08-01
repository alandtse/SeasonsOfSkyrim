#include "SeasonManager.h"
#include "Papyrus.h"

Season* SeasonManager::GetSeasonImpl(SEASON a_season)
{
	switch (a_season) {
	case SEASON::kWinter:
		return &winter;
	case SEASON::kSpring:
		return &spring;
	case SEASON::kSummer:
		return &summer;
	case SEASON::kAutumn:
		return &autumn;
	default:
		return nullptr;
	}
}

Season* SeasonManager::GetCurrentSeason(bool a_ignoreOverride)
{
	if (!a_ignoreOverride && seasonOverride != SEASON::kNone) {
		return GetSeasonImpl(seasonOverride);
	}

	switch (seasonType) {
	case SEASON_TYPE::kPermanentWinter:
		return &winter;
	case SEASON_TYPE::kPermanentSpring:
		return &spring;
	case SEASON_TYPE::kPermanentSummer:
		return &summer;
	case SEASON_TYPE::kPermanentAutumn:
		return &autumn;
	case SEASON_TYPE::kSeasonal:
		{
			const auto calendar = RE::Calendar::GetSingleton();
			const auto month = calendar ? calendar->GetMonth() : 7;

			if (const auto it = monthToSeasons.find(static_cast<MONTH>(month)); it != monthToSeasons.end()) {
				return GetSeasonImpl(it->second);
			}

			return nullptr;
		}
	default:
		return nullptr;
	}
}

bool SeasonManager::UpdateSeason()
{
	bool shouldUpdate = false;

	if (loadedFromSave) {
		shouldUpdate = true;
	}

	if (seasonOverride != SEASON::kNone) {
		const auto tempLastSeason = lastSeason;
		lastSeason = seasonOverride;

		if (!shouldUpdate) {
			shouldUpdate = seasonOverride != tempLastSeason;
		}
		if (!loadedFromSave && shouldUpdate) {
			Papyrus::Events::Manager::GetSingleton()->seasonChange.QueueEvent(std::to_underlying(tempLastSeason), std::to_underlying(seasonOverride), true);
		}

	} else {
		lastSeason = currentSeason;

		if (!shouldUpdate) {
			const auto season = GetCurrentSeason();
			currentSeason = season ? season->GetType() : SEASON::kNone;

			shouldUpdate = currentSeason != lastSeason;
		}
		if (!loadedFromSave && shouldUpdate) {
			Papyrus::Events::Manager::GetSingleton()->seasonChange.QueueEvent(std::to_underlying(lastSeason), std::to_underlying(currentSeason), false);
		}
	}

	if (loadedFromSave) {
		loadedFromSave = false;
	}

	return shouldUpdate;
}

Season* SeasonManager::GetSeason()
{
	if (!GetExterior()) {
		return nullptr;
	}

	if (seasonOverride != SEASON::kNone) {
		return GetSeasonImpl(seasonOverride);
	} else {
		if (currentSeason == SEASON::kNone) {
			UpdateSeason();
		}
		return GetSeasonImpl(currentSeason);
	}
}

void SeasonManager::LoadMonthToSeasonMap(CSimpleIniA& a_ini)
{
	for (const auto& [month, monthName] : monthNames) {
		auto& [tes, irl] = monthName;
		ini::get_value(a_ini, monthToSeasons.at(month), "Settings", tes.data(),
			month == MONTH::kMorningStar ? ";0 - none\n;1 - winter\n;2 - spring\n;3 - summer\n;4 - autumn\n\n;January" : irl.data());
	}
}

void SeasonManager::LoadSettings()
{
	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(settings);

	logger::info("{:*^30}", "SETTINGS");

	//delete and recreate settings if new settings are not found.
	const auto        month = ini.GetLongValue("Settings", "Morning Star", -1);
	const std::string flora = ini.GetValue("Winter", "Flora", "-1");

	if (month == -1 || flora == "-1") {
		ini.Delete("Settings", nullptr);

		ini.Delete("Settings", nullptr);
		ini.Delete("Winter", nullptr);
		ini.Delete("Spring", nullptr);
		ini.Delete("Summer", nullptr);
		ini.Delete("Autumn", nullptr);
	}

	ini::get_value(ini, seasonType, "Settings", "Season Type", ";0 - disabled\n;1 - permanent winter\n;2 - permanent spring\n;3 - permanent summer\n;4 - permanent autumn\n;5 - seasonal");

	ini::get_value(ini, mainWINSwap.skip, "Winter", "Ignore auto generated WIN formswap", ";Autogenerated winter formswap config will not be applied.");
	ini::get_value(ini, mainWINSwap.skipLT, "Winter", "Skip Land Textures", ";Skip loading these form types from autogenerated winter formswap.");
	ini::get_value(ini, mainWINSwap.skipActi, "Winter", "Skip Activator", nullptr);
	ini::get_value(ini, mainWINSwap.skipFurn, "Winter", "Skip Furniture", nullptr);
	ini::get_value(ini, mainWINSwap.skipMovStat, "Winter", "Skip Movable Statics", nullptr);
	ini::get_value(ini, mainWINSwap.skipStat, "Winter", "Skip Statics", nullptr);
	ini::get_value(ini, mainWINSwap.skipTree, "Winter", "Skip Tree", nullptr);

	logger::info("Season type is {}", std::to_underlying(seasonType));

	ini::get_value(ini, preferMultipass, "Settings", "Prefer Multipass", ";If true, multipass materials will be used where supported.\n;If false, single pass will be used instead.");

	LoadMonthToSeasonMap(ini);

	winter.LoadSettings(ini, true);
	spring.LoadSettings(ini);
	summer.LoadSettings(ini);
	autumn.LoadSettings(ini);

	(void)ini.SaveFile(settings);
}

bool SeasonManager::ShouldRegenerateWinterFormSwap() const
{
	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(serializedSeasonList);

#ifndef SKYRIMVR
	const auto&  mods = RE::TESDataHandler::GetSingleton()->compiledFileCollection;
	const size_t actualModCount = mods.files.size() + mods.smallFiles.size();
#else
	auto&  mods = RE::TESDataHandler::GetSingleton()->VRcompiledFileCollection;
	size_t actualModCount = 0;
	if (mods) {
		actualModCount = mods->files.size() + mods->smallFiles.size();
	} else {
		actualModCount = RE::TESDataHandler::GetSingleton()->loadedModCount;
	}
#endif
	//1.6.0 - delete old serialized value to force regeneration
	ini.DeleteValue("Game", "Mod Count", nullptr);

	const auto expectedModCount = string::to_num<size_t>(ini.GetValue("Game", "Total Mod Count", "0"));

	const auto shouldRegenerate = actualModCount != expectedModCount;

	if (shouldRegenerate) {
		ini.SetValue("Game", "Total Mod Count", std::to_string(actualModCount).c_str(), nullptr);
		if (expectedModCount != 0) {
			logger::info("Mod count has changed since last run ({} -> {}), regenerating main WIN formswap", expectedModCount, actualModCount);
		} else {
			logger::info("Regenerating main WIN formswap since last update");
		}
		(void)ini.SaveFile(serializedSeasonList);
	}

	return shouldRegenerate;
}

void SeasonManager::LoadOrGenerateWinterFormSwap()
{
	if (mainWINSwap.skip) {
		logger::info("Main WIN formswap loading disabled in config");
		return;
	}

	constexpr auto path = L"Data/Seasons/MainFormSwap_WIN.ini";

	logger::info("Loading main WIN formswap settings");

	CSimpleIniA ini;
	ini.SetUnicode();
	ini.SetMultiKey();
	ini.SetAllowKeyOnly();

	ini.LoadFile(path);

	auto& winFormSwapMap = winter.GetFormSwapMap();

	if (winFormSwapMap.GenerateFormSwaps(ini, ShouldRegenerateWinterFormSwap())) {
		(void)ini.SaveFile(path);
	} else {
		for (auto& type : FormSwapMap::standardTypes) {
			switch (string::const_hash(type)) {
			case string::const_hash("LandTextures"sv):
				{
					if (mainWINSwap.skipLT) {
						logger::info("\t[{}] skipping...", type);
						continue;
					}
				}
				break;
			case string::const_hash("Activators"sv):
				{
					if (mainWINSwap.skipActi) {
						logger::info("\t[{}] skipping...", type);
						continue;
					}
				}
				break;
			case string::const_hash("Furniture"sv):
				{
					if (mainWINSwap.skipFurn) {
						logger::info("\t[{}] skipping...", type);
						continue;
					}
				}
				break;
			case string::const_hash("MovableStatics"sv):
				{
					if (mainWINSwap.skipMovStat) {
						logger::info("\t[{}] skipping...", type);
						continue;
					}
				}
				break;
			case string::const_hash("Statics"sv):
				{
					if (mainWINSwap.skipStat) {
						logger::info("\t[{}] skipping...", type);
						continue;
					}
				}
				break;
			case string::const_hash("Trees"sv):
				{
					if (mainWINSwap.skipTree) {
						logger::info("\t[{}] skipping...", type);
						continue;
					}
				}
				break;
			default:
				break;
			}

			CSimpleIniA::TNamesDepend values;
			ini.GetAllKeys(type.c_str(), values);
			values.sort(CSimpleIniA::Entry::LoadOrder());

			if (!values.empty()) {
				logger::info("\t[{}] read {} variants", type, values.size());

				std::vector<std::string> vec;
				std::ranges::transform(values, std::back_inserter(vec), [&](const auto& val) { return val.pItem; });

				winFormSwapMap.LoadFormSwaps(type, vec);
			}
		}
	}
}

void SeasonManager::LoadSeasonData(Season& a_season, CSimpleIniA& a_settings)
{
	std::vector<std::string> configs;

	const auto& [type, suffix] = a_season.GetID();

	logger::info("{}", type);

	for (constexpr auto folder = R"(Data\Seasons)"; const auto& entry : std::filesystem::directory_iterator(folder)) {
		if (entry.exists() && !entry.path().empty() && entry.path().extension() == ".ini"sv) {
			if (const auto path = entry.path().string(); path.contains(suffix) && !path.contains("MainFormSwap"sv)) {
				configs.push_back(path);
			}
		}
	}

	if (configs.empty()) {
		logger::warn("\tNo .ini files with _{} suffix were found in Data/Seasons folder, skipping {} formswaps", suffix, suffix == "WIN" ? "secondary" : "all");
		return;
	}

	logger::info("\t{} matching inis found", configs.size());

	std::ranges::sort(configs);

	for (auto& path : configs) {
		logger::info("\tINI : {}", path);

		CSimpleIniA ini;
		ini.SetUnicode();
		ini.SetMultiKey();
		ini.SetAllowKeyOnly();

		if (const auto rc = ini.LoadFile(path.c_str()); rc < 0) {
			logger::error("\t\tcouldn't read INI");
			continue;
		}

		a_season.LoadData(ini);
	}

	//save worldspaces to settings so DynDOLOD can read them
	a_season.SaveData(a_settings);
}

void SeasonManager::LoadSeasonData()
{
	CSimpleIniA settingsINI;
	settingsINI.SetUnicode();

	settingsINI.LoadFile(settings);

	LoadSeasonData(winter, settingsINI);
	LoadSeasonData(spring, settingsINI);
	LoadSeasonData(summer, settingsINI);
	LoadSeasonData(autumn, settingsINI);

	(void)settingsINI.SaveFile(settings);
}

void SeasonManager::CheckLODExists()
{
	logger::info("{:*^30}", "LOD");

	winter.CheckLODExists();
	spring.CheckLODExists();
	summer.CheckLODExists();
	autumn.CheckLODExists();
}

void SeasonManager::SaveSeason(std::string_view a_savePath)
{
	if (const auto player = RE::PlayerCharacter::GetSingleton(); !player->parentCell || !player->parentCell->IsExteriorCell()) {
		return;
	}

	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(serializedSeasonList);

	const auto season = GetCurrentSeason(true);
	currentSeason = season ? season->GetType() : SEASON::kNone;

	const auto seasonData = std::format("{}|{}", std::to_underlying(currentSeason), std::to_underlying(seasonOverride));
	ini.SetValue("Saves", a_savePath.data(), seasonData.c_str(), nullptr);

	(void)ini.SaveFile(serializedSeasonList);
}

void SeasonManager::LoadSeason(const std::string& a_savePath)
{
	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(serializedSeasonList);

	const auto seasonData = string::split(ini.GetValue("Saves", a_savePath.c_str(), "3"), "|");
	if (seasonData.size() == 2) {
		currentSeason = string::to_num<SEASON>(seasonData[0]);
		seasonOverride = string::to_num<SEASON>(seasonData[1]);
	} else {
		currentSeason = string::to_num<SEASON>(seasonData[0]);
		seasonOverride = SEASON::kNone;
	}

	loadedFromSave = true;

	(void)ini.SaveFile(serializedSeasonList);
}

void SeasonManager::ClearSeason(std::string_view a_savePath) const
{
	CSimpleIniA ini;
	ini.SetUnicode();

	ini.LoadFile(serializedSeasonList);

	ini.DeleteValue("Saves", a_savePath.data(), nullptr);

	(void)ini.SaveFile(serializedSeasonList);
}

void SeasonManager::CleanupSerializedSeasonList() const
{
	constexpr auto get_save_directory = []() -> std::optional<std::filesystem::path> {
		if (auto path = logger::log_directory()) {
			path->remove_filename();  // remove "/SKSE"
			path->append(RE::INISettingCollection::GetSingleton()->GetSetting("sLocalSavePath:General")->GetString());
			return path;
		}
		return std::nullopt;
	};

	const auto directory = get_save_directory();
	if (!directory) {
		return;
	}

	logger::info("{:*^30}", "SAVES");

	logger::info("Save directory is {}", directory->string());

	CSimpleIniA ini;
	ini.SetUnicode();

	if (const auto rc = ini.LoadFile(serializedSeasonList); rc < 0) {
		return;
	}

	CSimpleIniA::TNamesDepend values;
	ini.GetAllKeys("Saves", values);
	values.sort(CSimpleIniA::Entry::LoadOrder());

	if (!values.empty()) {
		std::vector<std::string> badSaves;
		badSaves.reserve(values.size());
		for (const auto& key : values) {
			if (auto save = std::format("{}{}.ess", directory->string(), key.pItem); !std::filesystem::exists(save)) {
				badSaves.emplace_back(key.pItem);
			}
		}
		for (auto& badSave : badSaves) {
			ini.DeleteValue("Saves", badSave.c_str(), nullptr);
		}
	}

	(void)ini.SaveFile(serializedSeasonList);
}

SEASON SeasonManager::GetCurrentSeasonType()
{
	const auto season = GetCurrentSeason();
	return season ? season->GetType() : SEASON::kNone;
}

SEASON SeasonManager::GetSeasonType()
{
	const auto season = GetSeason();
	return season ? season->GetType() : SEASON::kNone;
}

bool SeasonManager::CanApplySnowShader()
{
	const auto season = GetSeason();
	return season ? season->CanApplySnowShader() : false;
}

std::pair<bool, std::string> SeasonManager::CanSwapLOD(LOD_TYPE a_type)
{
	const auto season = GetSeason();
	return season ? std::make_pair(season->CanSwapLOD(a_type), season->GetID().suffix) : std::make_pair(false, "");
}

bool SeasonManager::CanSwapLandscape()
{
	const auto season = GetSeason();
	return season ? season->CanSwapLandscape() : false;
}

bool SeasonManager::CanSwapForm(RE::FormType a_formType)
{
	const auto season = GetSeason();
	return season ? season->CanSwapForm(a_formType) : false;
}

bool SeasonManager::CanSwapGrass()
{
	const auto season = GetSeason();
	return season ? season->CanSwapForm(RE::FormType::Grass) : false;
}

RE::TESBoundObject* SeasonManager::GetSwapForm(const RE::TESForm* a_form)
{
	const auto season = GetSeason();
	return season ? season->GetFormSwapMap().GetSwapForm(a_form) : nullptr;
}

RE::TESLandTexture* SeasonManager::GetSwapLandTexture(const RE::TESLandTexture* a_landTxst)
{
	const auto season = GetSeason();
	return season ? season->GetFormSwapMap().GetSwapLandTexture(a_landTxst) : nullptr;
}

RE::TESLandTexture* SeasonManager::GetSwapLandTexture(const RE::BGSTextureSet* a_txst)
{
	const auto season = GetSeason();
	return season ? season->GetFormSwapMap().GetSwapLandTexture(a_txst) : nullptr;
}

bool SeasonManager::GetExterior()
{
	return isExterior;
}

void SeasonManager::SetExterior(bool a_isExterior)
{
	isExterior = a_isExterior;
}

SEASON SeasonManager::GetSeasonOverride() const
{
	return seasonOverride;
}

bool SeasonManager::PreferMultipass() const
{
	return preferMultipass;
}

void SeasonManager::SetSeasonOverride(SEASON a_season)
{
	seasonOverride = a_season;
}

SeasonManager::EventResult SeasonManager::ProcessEvent(const RE::TESActivateEvent* a_event, RE::BSTEventSource<RE::TESActivateEvent>*)
{
	if (!a_event || GetExterior()) {
		return EventResult::kContinue;
	}

	constexpr auto is_teleport_door = [](auto&& a_ref, auto&& a_object) {
		return a_ref && a_ref->IsPlayerRef() && a_object && a_object->extraList.HasType(RE::ExtraDataType::kTeleport);
	};

	if (!is_teleport_door(a_event->actionRef, a_event->objectActivated)) {
		return EventResult::kContinue;
	}

	if (const auto tes = RE::TES::GetSingleton(); UpdateSeason()) {
		tes->PurgeBufferedCells();
	}

	return EventResult::kContinue;
}
