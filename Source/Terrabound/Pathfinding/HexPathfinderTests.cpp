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

	/** Every tile in Before has the same distance in After, and neither has extra tiles. */
	static bool FieldsMatch(const TMap<FHexCoord, float>& Before, const TMap<FHexCoord, float>& After)
	{
		if (Before.Num() != After.Num())
		{
			return false;
		}
		for (const TPair<FHexCoord, float>& Entry : Before)
		{
			const float* Other = After.Find(Entry.Key);
			if (!Other || !FMath::IsNearlyEqual(*Other, Entry.Value))
			{
				return false;
			}
		}
		return true;
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
 * One occupied hex (PLAN.md 1.2b): units are invisible to route planning, so the distance field is
 * identical with the unit and without it, and the step still picks the occupied hex as often as its
 * equally short neighbour - refusing to enter is the caller's occupancy claim, not the search. Fifty
 * attempts: a search that avoided the unit would never pick it, and a fair tie misses it fifty times
 * in a row with odds of 2^-50.
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

	const TMap<FHexCoord, float> EmptyField = FHexPathfinder::ComputeDistanceField(*Grid, Grid->GetBackRowCoords());
	Grid->SetOccupant(Blocked, TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>());
	const TMap<FHexCoord, float> Field = FHexPathfinder::ComputeDistanceField(*Grid, Grid->GetBackRowCoords());

	TestTrue(TEXT("The distance field is unchanged by the occupant"), FieldsMatch(EmptyField, Field));

	bool bPickedOccupied = false;
	for (int32 Attempt = 0; Attempt < 50; ++Attempt)
	{
		const TOptional<FHexCoord> Next = FHexPathfinder::ChooseNextStep(*Grid, Field, From);
		if (!TestTrue(TEXT("A next step exists"), Next.IsSet()))
		{
			return false;
		}
		TestEqual(TEXT("The step goes one row down"), RowOf(Next.GetValue()), RowOf(From) + 1);
		bPickedOccupied |= Next.GetValue() == Blocked;
	}
	TestTrue(TEXT("The search does not avoid the occupied hex"), bPickedOccupied);

	return true;
}

/**
 * Full occupied row (PLAN.md 1.2b): a wall of units is not a seal - the field is identical to the
 * empty board's, every tile stays reachable, and the step from above the wall still goes into it.
 * Holding in front of the wall is the enemy's occupancy claim (EnemyBase.SealedRow).
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
	const TMap<FHexCoord, float> EmptyField = FHexPathfinder::ComputeDistanceField(*Grid, Grid->GetBackRowCoords());
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		if (RowOf(Coord) == WallRow)
		{
			Grid->SetOccupant(Coord, TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>());
		}
	}

	const TMap<FHexCoord, float> Field = FHexPathfinder::ComputeDistanceField(*Grid, Grid->GetBackRowCoords());
	TestTrue(TEXT("The distance field is unchanged by a full row of units"), FieldsMatch(EmptyField, Field));
	TestEqual(TEXT("Every tile is still reachable"), Field.Num(), Grid->GetTileCount());

	const FHexCoord AboveWall = AtOffset(3, WallRow - 1);
	const TOptional<FHexCoord> Next = FHexPathfinder::ChooseNextStep(*Grid, Field, AboveWall);
	if (!TestTrue(TEXT("A next step exists even though the wall is sealed"), Next.IsSet()))
	{
		return false;
	}
	TestEqual(TEXT("The step goes into the wall row"), RowOf(Next.GetValue()), WallRow);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
