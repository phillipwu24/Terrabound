// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexCoordinates.h"
#include "HexGridVisualizer.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Per-tile visual state, driven into the ISM's per-instance custom data (index 0) and read by
 * M_HexTile (PLAN.md 2.3). The numeric order here IS the contract with the material's If-chain —
 * changing it means updating M_HexTile's thresholds too.
 */
enum class EHexTileVisualState : uint8
{
	Default,
	PlayerZone,
	EnemyZone,
	Hovered,
	ValidPlacement,
	InvalidPlacement,
	Occupied
};

/**
 * Renders the board as one actor with an instanced static mesh component — not 56 actors.
 * Reads tile coordinates and HexRadius from UHexGrid; never writes to it. Placed once in the
 * level with SM_HexTile assigned (PLAN.md 2.1/2.2).
 */
UCLASS()
class TERRABOUND_API AHexGridVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AHexGridVisualizer();

	/**
	 * Sets Coord's visual state (custom data driving M_HexTile). No-op if Coord isn't a built
	 * tile. Not a UFUNCTION: EHexTileVisualState is a plain enum, not a UENUM, since nothing
	 * outside C++ needs to call this yet (Phase 3's hover/placement-preview work will).
	 */
	void SetTileVisualState(const FHexCoord& Coord, EHexTileVisualState State);

	/**
	 * Coord's resting state (PlayerZone/EnemyZone) with no hover/placement/occupancy overlay -
	 * what a tile reverts to once a transient state like Hovered is no longer relevant. Defaults
	 * to EnemyZone if Coord isn't a valid tile (matches BuildTileInstances' own fallback).
	 */
	EHexTileVisualState GetBaseVisualState(const FHexCoord& Coord) const;

	/** Toggles the debug (q, r) coordinate text overlay, redrawn every tick while enabled. */
	void ToggleCoordOverlay() { bShowCoordOverlay = !bShowCoordOverlay; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void BuildTileInstances();
	void DrawCoordOverlay() const;

	// Its native Static Mesh field (assigned in the editor to SM_HexTile from task 2.1) is
	// what BuildTileInstances reads — no separate wrapper property, so there's one place to
	// set the mesh, not two.
	UPROPERTY(VisibleAnywhere, Category = "Hex Grid Visualizer")
	TObjectPtr<UInstancedStaticMeshComponent> TileInstances;

	// Coord -> ISM instance index, built alongside instance creation so SetTileVisualState can
	// address a single tile without a linear search.
	TMap<FHexCoord, int32> InstanceIndexByCoord;

	// Debug-only (PLAN.md 2.4): off by default, toggled via the DebugCoordOverlay console command.
	bool bShowCoordOverlay = false;
};
