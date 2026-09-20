// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HexCoordinates.h"
#include "HexTile.generated.h"

class ABoardUnitBase;

/**
 * A single tile's state. Owned exclusively by HexGrid's flat array — never held or mirrored by
 * a Blueprint. Blueprints change live tile state through BlueprintCallable functions on the grid
 * subsystem (e.g. SetSpawnFlag, task 1.6), not by touching this struct directly, which is why
 * nothing here is EditAnywhere or BlueprintReadWrite.
 *
 * bIsWalkable and Occupant are independent: bIsWalkable says terrain permits a path across the
 * tile, Occupant == nullptr says a unit may stand on it. The pathfinder reads both - an occupied
 * tile is passable to the search at a flat extra cost. Never derive one from the other.
 */
USTRUCT()
struct TERRABOUND_API FHexTile
{
	GENERATED_BODY()

	FHexCoord Coord;

	bool bIsSpawn = false;
	bool bIsPlaceable = false;
	bool bIsWalkable = true;

	// Single reference: one unit per tile, champion or enemy, no stacking.
	TWeakObjectPtr<ABoardUnitBase> Occupant;

	// Declared now per the layout in Grid/; stays null all of Checkpoint 1.
	TWeakObjectPtr<AActor> Terrain;

	// Base cost to enter this tile, read by HexPathfinder. Uniform for now; nothing varies it yet.
	float PathCost = 1.f;
};
