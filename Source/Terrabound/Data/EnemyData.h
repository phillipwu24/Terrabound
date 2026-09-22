// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyData.generated.h"

class USkeletalMesh;
class UAnimInstance;

/**
 * Per-enemy tuning and identity, authored as a data asset (PLAN.md 1.2) - one instance per enemy
 * type, in Content/Terrabound/Data/Enemies/. Enemies are spawned generically as AEnemyBase and
 * initialized from one of these; there is no per-enemy Blueprint.
 *
 * Every number here is a PLACEHOLDER. Only MoveSpeedHexesPerSecond is read before Phase 2 - the
 * rest is AttributeSet initialization data (stored, read by nothing until GAS exists), same
 * arrangement as UChampionData.
 */
UCLASS()
class TERRABOUND_API UEnemyData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	/** Idle only for now (PLAN.md D15); the walk clip arrives in 1.8. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TSubclassOf<UAnimInstance> AnimBlueprint;

	/**
	 * Walking speed in hexes per second, not cm/s, so a HexRadius change never retunes it. 2.0 puts
	 * an empty-board crossing (7 hexes) at 3.5 s - the figure Checkpoint 1's walker settled on.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0.1"))
	float MoveSpeedHexesPerSecond = 2.f;

	// --- AttributeSet initialization data. Stored, read by nothing until Phase 2. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
	float AttackSpeed = 1.f;

	/**
	 * Aggro/attack range, in hexes. 1 (melee, adjacent) or 2 - never 0 (impossible under one unit per
	 * tile) and never scaled with wave number. Stored as a tile count, converted to world units at
	 * query time.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats", meta = (ClampMin = "1", ClampMax = "2"))
	int32 RangeInHexes = 1;

	/** Gold granted the moment this enemy dies (PLAN.md D3). Placeholder. A leaking enemy drops none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Economy")
	int32 GoldOnKill = 1;
};
