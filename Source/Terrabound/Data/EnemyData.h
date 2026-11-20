// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EnemyData.generated.h"

class USkeletalMesh;
class UAnimInstance;
class UAnimSequence;

/**
 * Per-enemy tuning and identity, authored as a data asset (PLAN.md 1.2) - one instance per enemy
 * type, in Content/Terrabound/Data/Enemies/. Enemies are spawned generically as AEnemyBase and
 * initialized from one of these; there is no per-enemy Blueprint.
 *
 * Every number here is a PLACEHOLDER. MaxHealth, AttackDamage and AttackSpeed are copied into the
 * enemy's UCombatAttributeSet once, by InitializeFromEnemyData (PLAN.md 2.1), same arrangement as
 * UChampionData. RangeInHexes and GoldOnKill are read from here directly.
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

	/**
	 * The pack's stock ABP, unedited. It plays idle and the LevelStart spawn montage on its own; the
	 * walk is played from C++ on top of it (PLAN.md D15).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TSubclassOf<UAnimInstance> AnimBlueprint;

	/**
	 * In-place walk clip (the pack's Jog_Fwd), looped as a montage on the settings' AnimMontageSlotName
	 * while the enemy steps between hexes. None = the enemy glides in idle.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Animation")
	TObjectPtr<UAnimSequence> WalkAnimation;

	/** Play rate of WalkAnimation, tuned by eye so the feet don't slide at MoveSpeedHexesPerSecond. Placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Animation", meta = (ClampMin = "0.01"))
	float WalkPlayRate = 1.f;

	/**
	 * Walking speed in hexes per second, not cm/s, so a HexRadius change never retunes it. 0.5 puts
	 * an empty-board crossing (7 hexes) at 14 s (PLAN.md 1.7).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0.1"))
	float MoveSpeedHexesPerSecond = 0.5f;

	/**
	 * Seconds a freshly spawned enemy stands on its spawn hex before its first step, while the ABP's
	 * LevelStart montage plays, re-rated to end with the hold. It can be targeted during the hold.
	 * 0 = walk at once. Placeholder (PLAN.md 1.8): a hold longer than the spawn interval throttles
	 * spawning when spawn hexes are few, since the holding enemy keeps its hex.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement", meta = (ClampMin = "0"))
	float SpawnHoldSeconds = 2.f;

	// --- Starting stats; see class comment. ---

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
