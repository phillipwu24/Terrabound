// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexGrid.h"
#include "HexCoordinates.h"
#include "../Units/BoardUnitBase.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace HexGridPlacementTests
{
	/**
	 * Scoped throwaway UWorld so a test can get a live, populated UHexGrid subsystem - a
	 * WorldSubsystem only exists alongside a UWorld, and CanPlaceAt (task 5.1) needs a real
	 * generated grid rather than pure coordinate math. Reusable by any later test that needs the
	 * same thing (e.g. Phase 5.3/5.4's commit and swap tests) - extract to a shared header if a
	 * second test file ends up needing it.
	 */
	class FScopedGridWorld
	{
	public:
		FScopedGridWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			WorldContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext->SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FScopedGridWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		UHexGrid* GetGrid() const { return World->GetSubsystem<UHexGrid>(); }
		UWorld* GetWorld() const { return World; }

	private:
		UWorld* World = nullptr;
		FWorldContext* WorldContext = nullptr;
	};
}

/**
 * CanPlaceAt (PLAN.md 5.1): true only for a coord that is on the board, in the placeable zone,
 * not spawn-flagged, and unoccupied. Each fixture below isolates one of those four reasons so the
 * test fails if the corresponding guard is removed, rather than passing for the wrong reason.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridCanPlaceAtTest, "Terrabound.Grid.HexGrid.CanPlaceAt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexGridCanPlaceAtTest::RunTest(const FString& Parameters)
{
	using namespace HexGridPlacementTests;

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

	// Valid interior: an untouched tile in the placeable zone (rows 3-7 per the current config).
	const FHexCoord Interior = UHexCoordinateLibrary::OffsetToAxial(3, 5);
	TestTrue(TEXT("Empty placeable-zone tile is placeable"), Grid->CanPlaceAt(Interior));

	// Occupied: a different placeable-zone tile, with an occupant set. Zone and spawn checks both
	// pass here, so occupancy is the only possible reason for the rejection.
	const FHexCoord Occupied = UHexCoordinateLibrary::OffsetToAxial(2, 4);
	ABoardUnitBase* Dummy = TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>();
	Grid->SetOccupant(Occupied, Dummy);
	TestFalse(TEXT("Occupied tile is not placeable"), Grid->CanPlaceAt(Occupied));

	// Enemy zone: row 1 is outside the placeable zone (rows 0-2), regardless of any other flag.
	const FHexCoord EnemyZone = UHexCoordinateLibrary::OffsetToAxial(3, 1);
	TestFalse(TEXT("Enemy-zone tile is not placeable"), Grid->CanPlaceAt(EnemyZone));

	// Spawn: flagged inside the placeable zone, not on the (already non-placeable) far edge, so
	// bIsSpawn is the only possible reason for the rejection. See PLAN.md 5.1's note on this
	// fixture - a spawn flag on row 0 would pass for the wrong reason (the zone check).
	const FHexCoord SpawnInZone = UHexCoordinateLibrary::OffsetToAxial(5, 5);
	Grid->SetSpawnFlag(SpawnInZone, true);
	TestFalse(TEXT("Spawn-flagged placeable-zone tile is not placeable"), Grid->CanPlaceAt(SpawnInZone));

	// Out of bounds: far outside any valid offset, regardless of the conversion's exact bounds.
	const FHexCoord OutOfBounds(1000, 1000);
	TestFalse(TEXT("Out-of-bounds coord is not placeable"), Grid->CanPlaceAt(OutOfBounds));

	return true;
}

/**
 * CanPlaceOrSwapAt (PLAN.md 5.4): same as CanPlaceAt but ignoring occupancy - true for the
 * occupied fixture above (a legal swap target), false for the reasons occupancy can't override
 * (enemy zone, spawn, out of bounds).
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexGridCanPlaceOrSwapAtTest, "Terrabound.Grid.HexGrid.CanPlaceOrSwapAt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexGridCanPlaceOrSwapAtTest::RunTest(const FString& Parameters)
{
	using namespace HexGridPlacementTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid))
	{
		return false;
	}

	// Occupied: unlike CanPlaceAt, this is a legal swap target, not a rejection.
	const FHexCoord Occupied = UHexCoordinateLibrary::OffsetToAxial(2, 4);
	ABoardUnitBase* Dummy = TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>();
	Grid->SetOccupant(Occupied, Dummy);
	TestTrue(TEXT("Occupied placeable-zone tile is a legal swap target"), Grid->CanPlaceOrSwapAt(Occupied));

	// Enemy zone and spawn-flagged tiles are still rejected - occupancy isn't the only guard.
	const FHexCoord EnemyZone = UHexCoordinateLibrary::OffsetToAxial(3, 1);
	TestFalse(TEXT("Enemy-zone tile is not a legal swap target"), Grid->CanPlaceOrSwapAt(EnemyZone));

	const FHexCoord SpawnInZone = UHexCoordinateLibrary::OffsetToAxial(5, 5);
	Grid->SetSpawnFlag(SpawnInZone, true);
	TestFalse(TEXT("Spawn-flagged tile is not a legal swap target"), Grid->CanPlaceOrSwapAt(SpawnInZone));

	const FHexCoord OutOfBounds(1000, 1000);
	TestFalse(TEXT("Out-of-bounds coord is not a legal swap target"), Grid->CanPlaceOrSwapAt(OutOfBounds));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
