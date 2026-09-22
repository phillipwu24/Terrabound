// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HexCoordinates.h"
#include "HexTile.h"
#include "HexGrid.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBoardOccupancyChanged);

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

	/**
	 * Sets Coord's occupant, overwriting whoever is there - placement, swap and debug spawns check
	 * CanPlaceAt/CanPlaceOrSwapAt first. A unit that walks into a hex uses TryOccupy instead.
	 * Returns false for an invalid coord.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hex Grid")
	bool SetOccupant(const FHexCoord& Coord, ABoardUnitBase* Unit);

	/** Clears whatever occupies Coord, if anything. Returns false for an invalid coord. */
	UFUNCTION(BlueprintCallable, Category = "Hex Grid")
	bool ClearOccupant(const FHexCoord& Coord);

	/**
	 * Claims Coord for Unit only if no other valid unit is standing there - the enforcement half of
	 * "one unit per tile" for anything that moves (enemies claim the next hex as they begin a step).
	 * Unlike SetOccupant it never overwrites. Returns false for an invalid coord, a null Unit, or a
	 * tile held by a different unit; a stale (destroyed) occupant counts as empty. Re-claiming a tile
	 * Unit already holds returns true without a broadcast. Checks occupancy only: walkable, spawn and
	 * placeable are the pathfinder's and placement's concern, not "can a unit stand here".
	 */
	UFUNCTION(BlueprintCallable, Category = "Hex Grid")
	bool TryOccupy(const FHexCoord& Coord, ABoardUnitBase* Unit);

	/**
	 * Sets whether Coord is a spawn tile. A function, not exposed state: FHexTile.bIsSpawn stays
	 * non-UPROPERTY, so a level Blueprint asks the grid to flip the flag rather than holding a
	 * copy of it. Returns false for an invalid coord.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hex Grid")
	bool SetSpawnFlag(const FHexCoord& Coord, bool bEnabled);

	/** Coordinates of every placeable (player-zone) tile. */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	TArray<FHexCoord> GetPlayerZoneTiles() const;

	/**
	 * True only when Coord is on the board, in the placeable (player) zone, not a spawn tile,
	 * and unoccupied. This is a pure grid-level check with no pathfinding — the terrain
	 * checkpoint's PlacementValidator (Terrain/) additionally asks whether placement seals the
	 * board, which needs A*; the two are not unified. See PLAN.md 5.1.
	 */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	bool CanPlaceAt(const FHexCoord& Coord) const;

	/**
	 * As CanPlaceAt, but ignoring occupancy - true for any tile a carried unit may legally be
	 * dropped onto, whether that lands as a placement (empty) or a swap (occupied by another
	 * unit). Drag/drop logic decides which of those two a drop onto a true result actually is;
	 * this only answers "is the tile itself legal." See PLAN.md 5.4.
	 */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	bool CanPlaceOrSwapAt(const FHexCoord& Coord) const;

	/** Total generated tile count. Derived read, not new state. */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	int32 GetTileCount() const { return Tiles.Num(); }

	/** Coordinates of every generated tile, placeable or not. Derived read, not new state. */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	TArray<FHexCoord> GetAllTileCoords() const;

	/** Hex circumradius in world units, as read from BoardConfig at generation. Derived read. */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	float GetHexRadius() const { return HexRadius; }

	/** Flat extra path cost of an occupied tile, as read from BoardConfig at generation. Derived read. */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	float GetOccupiedTileCost() const { return OccupiedTileCost; }

	/**
	 * Coordinates of every back-row tile - the highest row index, adjacent to the exit. These are
	 * the sources the pathfinder seeds its virtual goal node from. Derived read, not new state.
	 */
	UFUNCTION(BlueprintPure, Category = "Hex Grid")
	TArray<FHexCoord> GetBackRowCoords() const;

	/**
	 * Fires whenever SetOccupant/ClearOccupant/TryOccupy successfully mutates a tile - every board
	 * place/remove/swap/sell/step path routes through one of the three (PLAN.md 6.7), so this is
	 * the single correct hook for anything that needs to know "the board changed," without that
	 * thing needing to know why. No payload - listeners re-query whatever they care about.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Hex Grid")
	FOnBoardOccupancyChanged OnOccupancyChanged;

private:
	void GenerateGrid();
	FHexTile* GetMutableTile(const FHexCoord& Coord);

	// Plain member, not UPROPERTY: FHexTile holds only TWeakObjectPtr fields, which don't need
	// reflection for GC safety, and the array is never touched outside this class.
	TArray<FHexTile> Tiles;

	int32 BoardWidth = 0;
	int32 BoardDepth = 0;
	int32 PlaceableRowCount = 0;
	float HexRadius = 0.f;
	float OccupiedTileCost = 0.f;
};
