// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexPathfinder.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexGridTestWorld.h"
#include "../Grid/HexCoordinates.h"
#include "../Units/BoardUnitBase.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"

// These tests run against whatever DA_BoardConfig Project Settings points at, like the placement
// tests: they assume the default 7x8 board with a PathCost of 1 per tile, and derive every row from
// the back row rather than hardcoding depth.
namespace HexPathfinderTests
{
	static int32 RowOf(const FHexCoord& Coord)
	{
		return UHexCoordinateLibrary::AxialToOffset(Coord).Y;
	}

	static FHexCoord AtOffset(int32 Col, int32 Row)
	{
		return UHexCoordinateLibrary::OffsetToAxial(Col, Row);
	}

	static int32 GetBackRow(const UHexGrid& Grid)
	{
		return RowOf(Grid.GetBackRowCoords()[0]);
	}
}

/**
 * Empty board (PLAN.md 7.1): with nothing occupied, every tile is exactly as many steps from the
 * exit as rows remain, and a step from the far edge always goes down one row.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPathfinderEmptyBoardTest, "Terrabound.Pathfinding.HexPathfinder.EmptyBoard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexPathfinderEmptyBoardTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace HexPathfinderTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid))
	{
		return false;
	}
	if (!TestTrue(TEXT("Grid generated tiles (DA_BoardConfig must be set in Project Settings > Terrabound)"), Grid->GetTileCount() > 0))
	{
		return false;
	}

	const int32 BackRow = GetBackRow(*Grid);
	const TMap<FHexCoord, float> Field = FHexPathfinder::ComputeDistanceField(*Grid, Grid->GetBackRowCoords());

	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		const float* Distance = Field.Find(Coord);
		if (!TestNotNull(*FString::Printf(TEXT("%s is reachable"), *Coord.ToString()), Distance))
		{
			return false;
		}
		TestEqual(*FString::Printf(TEXT("%s distance to the exit"), *Coord.ToString()), *Distance, static_cast<float>(BackRow - RowOf(Coord)));
	}

	// Ties between the two down-neighbours are broken at random, so repeat: every pick must still
	// land one row down.
	const FHexCoord Start = AtOffset(3, 0);
	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		const TOptional<FHexCoord> Next = FHexPathfinder::ChooseNextStep(*Grid, Field, Start);
		if (!TestTrue(TEXT("A next step exists from the far edge"), Next.IsSet()))
		{
			return false;
		}
		TestEqual(TEXT("Next step is one row down"), RowOf(Next.GetValue()), 1);
	}

	return true;
}

/**
 * One occupied hex (PLAN.md 7.1): a gap exists, so the flat occupied cost makes the step avoid the
 * occupied hex without lengthening the route. Fifty attempts, because a cost that were ignored would
 * still pass any single one of them half the time.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPathfinderOneOccupiedHexTest, "Terrabound.Pathfinding.HexPathfinder.OneOccupiedHex", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexPathfinderOneOccupiedHexTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace HexPathfinderTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid))
	{
		return false;
	}
	if (!TestTrue(TEXT("Grid generated tiles (DA_BoardConfig must be set in Project Settings > Terrabound)"), Grid->GetTileCount() > 0))
	{
		return false;
	}

	const FHexCoord From = AtOffset(3, 2);
	const FHexCoord Blocked = AtOffset(3, 3);
	if (!TestTrue(TEXT("Fixture: the occupied hex is a neighbour of the start"), UHexCoordinateLibrary::GetNeighbours(From).Contains(Blocked)))
	{
		return false;
	}

	Grid->SetOccupant(Blocked, TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>());

	const int32 BackRow = GetBackRow(*Grid);
	const TMap<FHexCoord, float> Field = FHexPathfinder::ComputeDistanceField(*Grid, Grid->GetBackRowCoords());

	const float* FromDistance = Field.Find(From);
	if (!TestNotNull(TEXT("The start is reachable"), FromDistance))
	{
		return false;
	}
	TestEqual(TEXT("A gap exists, so the occupied hex adds nothing to the distance"), *FromDistance, static_cast<float>(BackRow - RowOf(From)));

	for (int32 Attempt = 0; Attempt < 50; ++Attempt)
	{
		const TOptional<FHexCoord> Next = FHexPathfinder::ChooseNextStep(*Grid, Field, From);
		if (!TestTrue(TEXT("A next step exists"), Next.IsSet()))
		{
			return false;
		}
		TestFalse(TEXT("The step avoids the occupied hex"), Next.GetValue() == Blocked);
		TestEqual(TEXT("The step still goes one row down"), RowOf(Next.GetValue()), RowOf(From) + 1);
	}

	return true;
}

/**
 * Full occupied row (PLAN.md 7.1): with no gap, the search routes through the wall rather than
 * failing - the step enters the wall row, and the distance rises by exactly one OccupiedTileCost,
 * from directly above the wall or from the far edge.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexPathfinderFullOccupiedRowTest, "Terrabound.Pathfinding.HexPathfinder.FullOccupiedRow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexPathfinderFullOccupiedRowTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace HexPathfinderTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid))
	{
		return false;
	}
	if (!TestTrue(TEXT("Grid generated tiles (DA_BoardConfig must be set in Project Settings > Terrabound)"), Grid->GetTileCount() > 0))
	{
		return false;
	}

	const int32 BackRow = GetBackRow(*Grid);
	const int32 WallRow = BackRow - 2;
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		if (RowOf(Coord) == WallRow)
		{
			Grid->SetOccupant(Coord, TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>());
		}
	}

	const TMap<FHexCoord, float> Field = FHexPathfinder::ComputeDistanceField(*Grid, Grid->GetBackRowCoords());
	const float OccupiedCost = Grid->GetOccupiedTileCost();

	const FHexCoord AboveWall = AtOffset(3, WallRow - 1);
	const float* AboveWallDistance = Field.Find(AboveWall);
	if (!TestNotNull(TEXT("A tile above a sealed wall is still reachable"), AboveWallDistance))
	{
		return false;
	}
	TestEqual(TEXT("Distance from above the wall gains one occupied cost"), *AboveWallDistance, static_cast<float>(BackRow - RowOf(AboveWall)) + OccupiedCost);

	const FHexCoord FarEdge = AtOffset(3, 0);
	const float* FarEdgeDistance = Field.Find(FarEdge);
	if (!TestNotNull(TEXT("The far edge is still reachable"), FarEdgeDistance))
	{
		return false;
	}
	TestEqual(TEXT("Distance from the far edge gains one occupied cost"), *FarEdgeDistance, static_cast<float>(BackRow - RowOf(FarEdge)) + OccupiedCost);

	const TOptional<FHexCoord> Next = FHexPathfinder::ChooseNextStep(*Grid, Field, AboveWall);
	if (!TestTrue(TEXT("A next step exists even though the wall is sealed"), Next.IsSet()))
	{
		return false;
	}
	TestEqual(TEXT("The step goes into the wall row"), RowOf(Next.GetValue()), WallRow);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
