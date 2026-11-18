// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpawnHexConfig.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexGridTestWorld.h"
#include "../Grid/HexCoordinates.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"

/**
 * ASpawnHexConfig::ApplySpawnHexes (PLAN.md 1.3, D12): enemy-side entries become spawn tiles; a
 * player-zone entry ends up unflagged and placeable even if it was flagged beforehand; an off-board
 * entry is skipped without disturbing the rest. Calls ApplySpawnHexes directly because actors in
 * FScopedGridWorld never BeginPlay.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSpawnHexConfigApplyTest, "Terrabound.Waves.SpawnHexConfig.Apply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSpawnHexConfigApplyTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;

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

	// Far edge (row 0) is enemy side under any PlaceableRowCount below BoardDepth.
	const FHexCoord FarEdgeA = UHexCoordinateLibrary::OffsetToAxial(1, 0);
	const FHexCoord FarEdgeB = UHexCoordinateLibrary::OffsetToAxial(5, 0);
	// Back row is always player zone. Pre-flagged, so the test fails if the config only skips it
	// instead of clearing the flag.
	const TArray<FHexCoord> BackRow = Grid->GetBackRowCoords();
	if (!TestTrue(TEXT("Back row exists"), BackRow.Num() > 0))
	{
		return false;
	}
	const FHexCoord PlayerZone = BackRow[BackRow.Num() / 2];
	Grid->SetSpawnFlag(PlayerZone, true);
	const FHexCoord OffBoard(1000, 1000);

	ASpawnHexConfig* Config = TestWorld.GetWorld()->SpawnActor<ASpawnHexConfig>();
	if (!TestNotNull(TEXT("SpawnHexConfig spawned"), Config))
	{
		return false;
	}
	// Off-board sits between the two valid entries so a bail-out on it would leave FarEdgeB unflagged.
	Config->SpawnHexes = { FarEdgeA, PlayerZone, OffBoard, FarEdgeB };

	AddExpectedError(TEXT("is in the player zone"), EAutomationExpectedErrorFlags::Contains, 1, false);
	AddExpectedError(TEXT("is not on the board"), EAutomationExpectedErrorFlags::Contains, 1, false);
	Config->ApplySpawnHexes();

	const FHexTile* TileA = Grid->GetTile(FarEdgeA);
	const FHexTile* TileB = Grid->GetTile(FarEdgeB);
	const FHexTile* TileP = Grid->GetTile(PlayerZone);
	if (!TestTrue(TEXT("Fixture tiles exist"), TileA && TileB && TileP))
	{
		return false;
	}

	TestTrue(TEXT("First far-edge entry is a spawn tile"), TileA->bIsSpawn);
	TestTrue(TEXT("Far-edge entry after the off-board one is still a spawn tile"), TileB->bIsSpawn);
	TestFalse(TEXT("Player-zone entry has its spawn flag cleared"), TileP->bIsSpawn);
	TestTrue(TEXT("Player-zone entry is placeable again"), Grid->CanPlaceAt(PlayerZone));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
