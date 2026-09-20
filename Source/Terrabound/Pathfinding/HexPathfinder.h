// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Optional.h"
#include "../Grid/HexCoordinates.h"

class UHexGrid;

/**
 * Grid pathfinding (PLAN.md 7.1), run as a distance query over UHexGrid's tile array and consulted
 * one step at a time - units store no path, so there is nothing to cache or invalidate. Stateless:
 * every function is static and reads only FHexTile data, never an actor.
 *
 * Cost model, per CLAUDE.md ("Terrain blocks pathing outright. Units block it at a flat cost."):
 * a tile with bIsWalkable == false cannot be entered; an occupied tile can, at its PathCost plus
 * one flat OccupiedTileCost that never looks at who the occupant is. The cost only steers route
 * choice - it does not let a unit stand on an occupied tile, which the caller's step enforces.
 */
struct TERRABOUND_API FHexPathfinder
{
	/**
	 * Cost for a unit to step into Coord. Returns false if it cannot be entered at all (off the
	 * board, or not walkable).
	 */
	static bool TryGetEnterCost(const UHexGrid& Grid, const FHexCoord& Coord, float& OutCost);

	/**
	 * Path distance from every reachable tile to the nearest of Sources, which sit at distance 0.
	 * Seeding several sources at 0 is the virtual goal node with zero-cost edges from each of them
	 * (CLAUDE.md), without adding a fake tile to the array. A tile terrain cuts off from every
	 * source is absent from the result. Distance is the cost of the steps still to take, so it
	 * does not include the cost of entering the tile itself.
	 */
	static TMap<FHexCoord, float> ComputeDistanceField(const UHexGrid& Grid, const TArray<FHexCoord>& Sources);

	/**
	 * The neighbour of From that is cheapest to step into and continue from, per Field. Ties are
	 * broken at random rather than by neighbour order, so a wave doesn't all drift to one side.
	 * Unset if no neighbour is enterable and connected to a source.
	 */
	static TOptional<FHexCoord> ChooseNextStep(const UHexGrid& Grid, const TMap<FHexCoord, float>& Field, const FHexCoord& From);
};
