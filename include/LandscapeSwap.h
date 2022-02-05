#pragma once

#include "SeasonManager.h"

namespace LandscapeSwap
{
	namespace Texture
	{
		struct GetAsShaderTextureSet
		{
			static RE::BSTextureSet* thunk(RE::BGSTextureSet* a_textureSet)
			{
				if (const auto seasonManager = SeasonManager::GetSingleton(); seasonManager->IsLandscapeSwapAllowed()) {
					const auto newLandTexture = seasonManager->GetSwapLandTextureFromTextureSet(a_textureSet);
					return newLandTexture ? newLandTexture->textureSet : a_textureSet;
				}
				return a_textureSet;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	namespace Grass
	{
		struct detail
		{
			static bool is_underwater_grass(RE::BSSimpleList<RE::TESGrass*>& a_grassList)
			{
				return std::ranges::any_of(a_grassList, [](const auto& grass) {
					return grass && grass->GetUnderwaterState() == RE::TESGrass::GRASS_WATER_STATE::kBelowOnlyAtLeast;
				});
			}
		};

		struct GetGrassList
		{
			static RE::BSSimpleList<RE::TESGrass*>& func(RE::TESLandTexture* a_landTexture)
			{
				if (const auto seasonManager = SeasonManager::GetSingleton(); seasonManager->CanSwapGrass()) {
					const auto newLandTexture = seasonManager->GetSwapLandTexture(a_landTexture);

				    return newLandTexture && !detail::is_underwater_grass(a_landTexture->textureGrassList) ? newLandTexture->textureGrassList : a_landTexture->textureGrassList;
				}
				return a_landTexture->textureGrassList;
			}

			static inline size_t size = 0x5;
			static inline std::uint64_t id = 18845;
		};
	}

	namespace Material
	{
		struct GetHavokMaterialType
		{
			static RE::MATERIAL_ID func(const RE::TESLandTexture* a_landTexture)
			{
				if (const auto seasonManager = SeasonManager::GetSingleton(); seasonManager->IsLandscapeSwapAllowed()) {
					const auto newLandTexture = seasonManager->GetSwapLandTexture(a_landTexture);
					const auto materialType = newLandTexture ? newLandTexture->materialType : a_landTexture->materialType;

					return materialType ? materialType->materialID : RE::MATERIAL_ID::kNone;
				}
				return a_landTexture->materialType ? a_landTexture->materialType->materialID : RE::MATERIAL_ID::kNone;
			}

			static inline size_t size = 0xE;
			static inline std::uint64_t id = 18849;
		};
	}

	inline void Install()
	{
		REL::Relocation<std::uintptr_t> create_land_geometry{ REL::ID(18791) };
		stl::write_thunk_call<Texture::GetAsShaderTextureSet>(create_land_geometry.address() + 0x174);
		stl::write_thunk_call<Texture::GetAsShaderTextureSet>(create_land_geometry.address() + 0x18D);
		stl::write_thunk_call<Texture::GetAsShaderTextureSet>(create_land_geometry.address() + 0x1E6);

	    stl::asm_replace<Grass::GetGrassList>();
		stl::asm_replace<Material::GetHavokMaterialType>();
	}
}
