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
 * The stat fields below are a fresh 1-star unit's starting values. InitializeFromChampionData
 * copies MaxHealth, AttackDamage and AttackSpeed into the unit's UCombatAttributeSet once, at
 * creation (PLAN.md 2.1); after that GAS owns the live values and nothing writes back here.
 * RangeInHexes is not an attribute (D6) and is read from here directly. MaxMana is read by nothing
 * until mana arrives with the first cast ability (D6).
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

	// --- Starting stats; see class comment. ---

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
