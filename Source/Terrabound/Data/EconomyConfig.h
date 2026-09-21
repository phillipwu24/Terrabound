// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EconomyConfig.generated.h"

/**
 * Every economy number lives here, and nowhere else in the project. All values below are
 * placeholders per PLAN.md 0.4 — none of them are tuned, they exist so there is something
 * to play against and adjust. StartingGold and RerollCost have no anchor in DESIGN.md at
 * all and are flagged individually below; the rest at least match DESIGN.md's own rough
 * starting points.
 *
 * The champion pool's tier-to-size and tier-to-odds tables are NOT here — those are a
 * distribution rather than a scalar and live in their own data table (PLAN.md 6.2).
 */
UCLASS()
class TERRABOUND_API UEconomyConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Champion cost by tier (1-3). Matches DESIGN.md's own starting point of cost == tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	TMap<int32, int32> TierCostTable = { {1, 1}, {2, 2}, {3, 3} };

	/** Gold the player starts a run with. Not specified anywhere in DESIGN.md — unresolved placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 StartingGold = 30;

	/** Gold cost to reroll the shop. Not specified anywhere in DESIGN.md — unresolved placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 RerollCost = 2;

	/** Number of shop slots. From DESIGN.md's rough starting points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 ShopSlotCount = 5;

	/** Number of bench slots. From DESIGN.md's rough starting points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	int32 BenchSlotCount = 6;

	/**
	 * Fraction of a champion's tier cost refunded on sell. DESIGN.md currently calls for full
	 * purchase price (1.0) — stored as a ratio rather than hardcoded in sell logic so a future
	 * change to partial refunds is a data edit, not a code change.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Economy")
	float SellRefundPercentage = 1.f;

	/** Highest star level a champion can reach. Copies at this level never merge further. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Level")
	int32 MaxStarLevel = 3;

	/** Copies of the same champion at the same star level that merge into one at the next level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Star Level")
	int32 CopiesPerStarUp = 3;

	/** Looks up TierCostTable for the given tier. Returns 0 if the tier isn't in the table. */
	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetCostForTier(int32 Tier) const { return TierCostTable.FindRef(Tier); }

	/**
	 * How many single copies a champion at StarLevel is worth: CopiesPerStarUp^(StarLevel-1), so
	 * 1 for a 1-star and 3 for a 2-star at the default. Drives sell refund and how many copies go
	 * back to the pool - a merge consumes copies from the pool, a sell returns all of them.
	 */
	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetCopiesInStar(int32 StarLevel) const
	{
		int32 Copies = 1;
		for (int32 Level = 1; Level < StarLevel; ++Level)
		{
			Copies *= CopiesPerStarUp;
		}
		return Copies;
	}
};
