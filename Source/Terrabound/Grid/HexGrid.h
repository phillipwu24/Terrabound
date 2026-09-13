// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HexCoordinates.h"
#include "HexTile.h"
#include "HexGrid.generated.h"

/**
 * Owns the board's flat tile array — the only place tile state lives. Generates from
 * DA_BoardConfig (via UTerraboundSettings) on world init. Nothing outside this class may write
 * a tile; Blueprints go through the BlueprintCallable functions here instead of touching
 * FHexTile directly (see HexTile.h).
 */
UCLASS()
class TERRABOUND_API UHexGrid : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** True if Coord falls within the generated board. */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	bool IsValidCoord(const FHexCoord& Coord) const;

	/**
	 * Not a UFUNCTION: FHexTile is not BlueprintType (see HexTile.h), so this stays a
	 * C++-only accessor. Returns nullptr for an invalid coord.
	 */
	const FHexTile* GetTile(const FHexCoord& Coord) const;

	// SetOccupant deliberately isn't here yet. ABoardUnitBase is only forward-declared until
	// task 4.2, and assigning a real ABoardUnitBase* into TWeakObjectPtr<ABoardUnitBase>
	// requires the compiler to know it derives from UObject — a forward declaration isn't
	// enough for that assignment to compile. SetOccupant is added in 4.2 alongside the class
	// it depends on.

	/** Clears whatever occupies Coord, if anything. Returns false for an invalid coord. */
	UFUNCTION(BlueprintCallable, Category = "Hex Grid")
	bool ClearOccupant(const FHexCoord& Coord);

	/** Coordinates of every placeable (player-zone) tile. */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	TArray<FHexCoord> GetPlayerZoneTiles() const;

private:
	void GenerateGrid();
	FHexTile* GetMutableTile(const FHexCoord& Coord);

	// Plain member, not UPROPERTY: FHexTile holds only TWeakObjectPtr fields, which don't need
	// reflection for GC safety, and the array is never touched outside this class.
	TArray<FHexTile> Tiles;

	int32 BoardWidth = 0;
	int32 BoardDepth = 0;
	int32 PlaceableRowCount = 0;
};
