// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoardConfig.generated.h"

/**
 * Board dimensions and hex sizing, tuned from a content asset rather than compiled in.
 * Spawn hexes deliberately do not live here — those are a per-level array that arrives
 * with the enemy checkpoint (see PLAN.md 0.2).
 */
UCLASS()
class TERRABOUND_API UBoardConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Hexes across the board. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	int32 BoardWidth = 7;

	/** Rows deep, front to back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	int32 BoardDepth = 8;

	/** Rows nearest the exit (highest row indices) that the player may place on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	int32 PlaceableRowCount = 5;

	/** Hex circumradius in world units. Placeholder until task 0.6 sets it against a real character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Board")
	float HexRadius = 100.f;

	/**
	 * PLACEHOLDER. Flat extra cost, added to a tile's PathCost, for the pathfinder to route through
	 * an occupied tile (champion or enemy alike - never varies by unit). Must exceed the longest
	 * possible detour (roughly the tile count) so a gap is always preferred over going through a
	 * unit. See CLAUDE.md's "Terrain blocks pathing outright. Units block it at a flat cost."
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pathfinding")
	float OccupiedTileCost = 100.f;
};
