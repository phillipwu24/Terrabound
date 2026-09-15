// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ChampionData.generated.h"

class USkeletalMesh;
class UAnimInstance;

/**
 * Per-champion tuning and identity, authored as a data asset (PLAN.md 4.3) - one instance per
 * champion, in Content/Terrabound/Data/Champions/.
 *
 * No Cost field. Cost is derived from Tier via UEconomyConfig::TierCostTable, TFT-style - storing
 * it here would put an economy number outside the one config file (PLAN.md 0.4) and would let a
 * champion's cost silently disagree with its tier.
 *
 * The stat fields below are AttributeSet initialization data for a later checkpoint: stored, read
 * by nothing. No current-HP tracking, damage application, mana gain/spend, or any runtime stat
 * mutation belongs here - that is GAS's job starting at Step 2. MaxMana is included because
 * DESIGN.md's GAS section treats mana as universal (every champion gains it on attack and on
 * damage taken, then casts at a threshold) - but the ability it casts is deliberately not
 * referenced here: a TSubclassOf<UGameplayAbility> field needs the GameplayAbilities module and an
 * actual ability class, neither of which exist before GAS arrives.
 */
UCLASS()
class TERRABOUND_API UChampionData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion")
	FText DisplayName;

	/** 1-3. Looked up in UEconomyConfig::TierCostTable for cost - never stored here. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion")
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion")
	TObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion")
	TSubclassOf<UAnimInstance> AnimBlueprint;

	/** Populated from Trait.Woodland / Trait.Bruiser (DT_TraitTags). One or both per champion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion")
	FGameplayTagContainer Traits;

	// --- AttributeSet initialization data. Stored, read by nothing until GAS exists. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion|Stats")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion|Stats")
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion|Stats")
	float AttackSpeed = 1.f;

	/** Aggro/attack range, in hexes - never centimeters. Converted to world units at query time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion|Stats")
	int32 RangeInHexes = 1;

	/** Mana threshold at which the champion's ability casts. See class comment: the ability
	 * itself is not referenced here, only the threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Champion|Stats")
	float MaxMana = 100.f;
};
