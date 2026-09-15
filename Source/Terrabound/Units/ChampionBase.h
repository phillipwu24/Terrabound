// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoardUnitBase.h"
#include "ChampionBase.generated.h"

class UChampionData;

/**
 * Player-side board unit (PLAN.md 4.4). Derives from BoardUnitBase - nothing champion-specific
 * lives on the base class, since it's shared with the future EnemyBase.
 *
 * Idle animation only for now: no combat, no targeting, no attack animation - that's later
 * checkpoints. Always Player team (set in the constructor) and always faces the enemy side
 * (enforced in BeginPlay, not the constructor - a level-placed instance's own transform is
 * applied after construction and would otherwise override a constructor-set rotation). Neither
 * depends on which champion this is.
 *
 * Individual champions are Blueprints deriving from this class, in
 * Content/Terrabound/Blueprints/Champions/ - never a C++ class per champion (CLAUDE.md's split).
 * Each Blueprint sets its own ChampionData default; BeginPlay auto-initializes from it, so
 * dropping a champion Blueprint into a level and pressing Play is enough to see it idle with the
 * right mesh, no console command required.
 */
UCLASS()
class TERRABOUND_API AChampionBase : public ABoardUnitBase
{
	GENERATED_BODY()

public:
	AChampionBase();

	/**
	 * Applies Data's skeletal mesh and anim blueprint to the inherited Mesh component and stores
	 * Data itself as this champion's ChampionData, so later systems (trait counting, GAS
	 * attribute init at Step 2) can read tier/stats/traits from one place instead of duplicating
	 * fields here. Does not touch HexGrid or CurrentCoord - call SnapToHex separately to place
	 * this champion.
	 */
	UFUNCTION(BlueprintCallable, Category = "Champion")
	void InitializeFromChampionData(UChampionData* Data);

	UFUNCTION(BlueprintPure, Category = "Champion")
	UChampionData* GetChampionData() const { return ChampionData; }

protected:
	virtual void BeginPlay() override;

	/**
	 * Which champion this Blueprint represents - set per-Blueprint (e.g. BP_Grux defaults this
	 * to DA_Champion_Grux). No other source of truth mirrors this, unlike CurrentCoord/Team, so
	 * it's a plain editable default rather than a function-only setter.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Champion")
	TObjectPtr<UChampionData> ChampionData;
};
