// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TerraboundSettings.generated.h"

class UBoardConfig;
class UEconomyConfig;
class UMaterialInterface;
class UDataTable;

/**
 * Project-wide settings, editable from Project Settings without touching a level.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Terrabound"))
class TERRABOUND_API UTerraboundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// HexGrid finds DA_BoardConfig through this at generation time (1.4).
	UPROPERTY(EditAnywhere, config, Category = "Board")
	TSoftObjectPtr<UBoardConfig> BoardConfig;

	// EconomyState finds DA_EconomyConfig through this at Initialize time (6.1). Later economy
	// systems (ShopSystem, Bench) resolve the same reference independently rather than going
	// through EconomyState, same as HexGrid doesn't proxy other subsystems' config reads.
	UPROPERTY(EditAnywhere, config, Category = "Economy")
	TSoftObjectPtr<UEconomyConfig> EconomyConfig;

	// UShopSystem's UChampionPool (6.3) resolves these two through here rather than a hardcoded
	// content path, same reasoning as BoardConfig/EconomyConfig above.
	UPROPERTY(EditAnywhere, config, Category = "Economy")
	TSoftObjectPtr<UDataTable> ChampionPoolTable;

	UPROPERTY(EditAnywhere, config, Category = "Economy")
	TSoftObjectPtr<UDataTable> ChampionTierOddsTable;

	/**
	 * BoardUnitBase's team tint hook (4.6) reads these three - one shared source rather than a
	 * value repeated on every champion/enemy Blueprint. Must expose a Vector Parameter named
	 * "TintColor" for ApplyTeamTint to drive.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Team Tint")
	TSoftObjectPtr<UMaterialInterface> TeamTintOverlayMaterial;

	UPROPERTY(EditAnywhere, config, Category = "Team Tint")
	FLinearColor PlayerTintColor = FLinearColor(0.1f, 0.4f, 1.f, 0.25f);

	UPROPERTY(EditAnywhere, config, Category = "Team Tint")
	FLinearColor EnemyTintColor = FLinearColor(1.f, 0.1f, 0.1f, 0.25f);

	/**
	 * Champion mesh scale multiplier by star level: index 0 is 1-star, index 1 is 2-star, and so
	 * on. A level past the end of the array uses the last entry. Applied to the mesh only, never
	 * the actor, so the click/drag HitBox stays uniform at every star level.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Star Level")
	TArray<float> StarMeshScaleMultipliers = { 0.8f, 1.f, 1.25f };
};
