// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ChampionPool.generated.h"

class UChampionData;

/**
 * One row per champion (PLAN.md 6.2): which pool a champion belongs to and how many copies.
 * No Tier field here - ChampionData::Tier (set in task 4.3) is already the source of truth for
 * a champion's tier, and a second copy here could silently disagree with it.
 */
USTRUCT(BlueprintType)
struct FChampionPoolRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Champion Pool")
	TSoftObjectPtr<UChampionData> ChampionData;

	/** Copies of this specific champion available in the shared pool. */
	UPROPERTY(EditAnywhere, Category = "Champion Pool")
	int32 PoolSize = 1;
};

/**
 * One row per tier (PLAN.md 6.2): weight for rolling that tier. Kept separate from
 * FChampionPoolRow so retuning a tier's odds is one row, not one edit per champion in it.
 */
USTRUCT(BlueprintType)
struct FChampionTierOddsRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Champion Pool")
	int32 Tier = 1;

	UPROPERTY(EditAnywhere, Category = "Champion Pool")
	float RollOdds = 1.f;
};

/**
 * TFT-style shared champion pool: a roll picks a tier weighted by RollOdds (skipping any tier
 * with no copies left), then a champion uniformly weighted by remaining copies within that tier.
 * Bought champions are drawn out; sold champions are returned.
 *
 * Owned by ShopSystem (PLAN.md 6.3), not a subsystem - nothing outside the shop needs to query
 * pool state this checkpoint.
 */
UCLASS()
class TERRABOUND_API UChampionPool : public UObject
{
	GENERATED_BODY()

public:
	/** Loads PoolTable and OddsTable and resets each champion to its full PoolSize copies. */
	void Initialize(UDataTable* PoolTable, UDataTable* OddsTable);

	/** Returns nullptr only if every champion's copies are exhausted. */
	UChampionData* DrawRandomChampion();

	/** Returns one copy of ChampionData to the pool. No-ops if ChampionData isn't in the pool. */
	void ReturnChampion(UChampionData* ChampionData);

private:
	struct FPoolEntry
	{
		TObjectPtr<UChampionData> ChampionData = nullptr;
		int32 Tier = 1;
		int32 RemainingCopies = 0;
	};

	TArray<FPoolEntry> Entries;
	TMap<int32, float> TierRollOdds;
};
