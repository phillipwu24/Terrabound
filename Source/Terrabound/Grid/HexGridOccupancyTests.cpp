// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexGrid.h"
#include "HexGridTestWorld.h"
#include "HexCoordinates.h"
#include "../Units/BoardUnitBase.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"

/**
 * TryOccupy (PLAN.md 1.1): the claim only succeeds when no other valid unit holds the tile, so
 * two claimants can never both end up on it. Run in both orders so the test can't pass just
 * because the first claimant happened to be A.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridTryOccupyContestedTest, "Terrabound.Grid.HexGrid.TryOccupy.Contested", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexGridTryOccupyContestedTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid))
	{
		return false;
	}

	ABoardUnitBase* A = TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>();
	ABoardUnitBase* B = TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>();

	const FHexCoord First = UHexCoordinateLibrary::OffsetToAxial(3, 2);
	TestTrue(TEXT("A claims an empty tile"), Grid->TryOccupy(First, A));
	TestFalse(TEXT("B cannot claim a tile A holds"), Grid->TryOccupy(First, B));
	TestTrue(TEXT("Tile still holds A after B's failed claim"), Grid->GetTile(First)->Occupant.Get() == A);

	// Same contest with the claim order reversed, on a different tile.
	const FHexCoord Second = UHexCoordinateLibrary::OffsetToAxial(4, 2);
	TestTrue(TEXT("B claims an empty tile"), Grid->TryOccupy(Second, B));
	TestFalse(TEXT("A cannot claim a tile B holds"), Grid->TryOccupy(Second, A));
	TestTrue(TEXT("Tile still holds B after A's failed claim"), Grid->GetTile(Second)->Occupant.Get() == B);

	// A unit re-claiming its own tile is idempotent, not a self-lockout.
	TestTrue(TEXT("A re-claiming its own tile succeeds"), Grid->TryOccupy(First, A));
	TestTrue(TEXT("Tile still holds A after the re-claim"), Grid->GetTile(First)->Occupant.Get() == A);

	// Releasing the tile is what lets the loser in.
	Grid->ClearOccupant(First);
	TestTrue(TEXT("B claims the tile once A has released it"), Grid->TryOccupy(First, B));
	TestTrue(TEXT("Tile now holds B"), Grid->GetTile(First)->Occupant.Get() == B);

	return true;
}

/**
 * The remaining rules: a destroyed occupant frees its tile, bad input is refused, and the claim
 * checks occupancy only - an enemy-zone tile (not placeable) is claimable like any other.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridTryOccupyEdgeCasesTest, "Terrabound.Grid.HexGrid.TryOccupy.EdgeCases", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexGridTryOccupyEdgeCasesTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid))
	{
		return false;
	}

	ABoardUnitBase* A = TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>();
	ABoardUnitBase* B = TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>();

	// Stale occupant: A dies while standing on the tile, so B may take it.
	const FHexCoord Stale = UHexCoordinateLibrary::OffsetToAxial(3, 3);
	Grid->TryOccupy(Stale, A);
	A->Destroy();
	TestTrue(TEXT("A tile held by a destroyed unit is claimable"), Grid->TryOccupy(Stale, B));
	TestTrue(TEXT("Tile now holds B"), Grid->GetTile(Stale)->Occupant.Get() == B);

	const FHexCoord Empty = UHexCoordinateLibrary::OffsetToAxial(2, 3);
	TestFalse(TEXT("Null unit is refused"), Grid->TryOccupy(Empty, nullptr));
	TestFalse(TEXT("Refused claim leaves the tile empty"), Grid->GetTile(Empty)->Occupant.IsValid());
	TestFalse(TEXT("Out-of-bounds coord is refused"), Grid->TryOccupy(FHexCoord(1000, 1000), B));

	// Row 0 is enemy-zone and not placeable; TryOccupy must not apply CanPlaceAt's zone rule.
	const FHexCoord EnemyZone = UHexCoordinateLibrary::OffsetToAxial(3, 0);
	TestFalse(TEXT("Fixture: enemy-zone tile is not placeable"), Grid->CanPlaceAt(EnemyZone));
	TestTrue(TEXT("Non-placeable tile is claimable"), Grid->TryOccupy(EnemyZone, B));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
