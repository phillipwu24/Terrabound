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
 * bIsWalkable and Occupant are independent: bIsWalkable says a path may cross the tile,
 * Occupant == nullptr says a unit may stand on it. Never derive one from the other.
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

	// Placeholder uniform cost; nothing tunes this yet.
	float PathCost = 1.f;
};
