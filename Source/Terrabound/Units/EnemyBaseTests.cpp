// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyBase.h"
#include "../Data/EnemyData.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexGridTestWorld.h"
#include "../Grid/HexCoordinates.h"
#include "../Grid/HexTile.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"

// The test world never ticks, so these drive AEnemyBase::AdvanceMovement directly on a fixed step. They
// run against whatever DA_BoardConfig Project Settings points at (default 7x8, PathCost 1, occupied
// cost 100) and derive every row from the back row rather than hardcoding depth.
namespace EnemyBaseTests
{
	constexpr float Dt = 1.f / 60.f;
	constexpr float Speed = 2.f;

	static int32 RowOf(const FHexCoord& Coord)
	{
		return UHexCoordinateLibrary::AxialToOffset(Coord).Y;
	}

	static FHexCoord AtOffset(int32 Col, int32 Row)
	{
		return UHexCoordinateLibrary::OffsetToAxial(Col, Row);
	}

	static AEnemyBase* SpawnEnemy(UWorld* World)
	{
		UEnemyData* Data = NewObject<UEnemyData>(GetTransientPackage());
		Data->DisplayName = FText::FromString(TEXT("TestEnemy"));
		Data->MoveSpeedHexesPerSecond = Speed;

		AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>();
		Enemy->InitializeFromEnemyData(Data);
		return Enemy;
	}

	static ABoardUnitBase* SpawnBlocker(UWorld* World, UHexGrid& Grid, const FHexCoord& Coord)
	{
		ABoardUnitBase* Blocker = World->SpawnActor<ABoardUnitBase>();
		Grid.SetOccupant(Coord, Blocker);
		return Blocker;
	}

	/** How many tiles hold Unit. An enemy must hold exactly one at every moment once it has entered. */
	static int32 CountTilesHeldBy(const UHexGrid& Grid, const ABoardUnitBase* Unit)
	{
		int32 Count = 0;
		for (const FHexCoord& Coord : Grid.GetAllTileCoords())
		{
			if (Grid.GetTile(Coord)->Occupant.Get() == Unit)
			{
				++Count;
			}
		}
		return Count;
	}
}

/**
 * Empty board (PLAN.md 1.2): an enemy from the far edge crosses in rows / speed seconds, exits once,
 * and releases its tile on the way out.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseEmptyBoardTest, "Terrabound.Units.EnemyBase.EmptyBoard", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEnemyBaseEmptyBoardTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace EnemyBaseTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid) || !TestTrue(TEXT("Grid generated tiles"), Grid->GetTileCount() > 0))
	{
		return false;
	}

	const int32 BackRow = RowOf(Grid->GetBackRowCoords()[0]);
	AEnemyBase* Enemy = SpawnEnemy(TestWorld.GetWorld());
	int32 ExitCount = 0;
	FHexCoord ExitCoord;
	Enemy->OnEnemyExited.AddLambda([&](AEnemyBase* Exited)
	{
		++ExitCount;
		ExitCoord = Exited->GetCurrentCoord();
	});

	if (!TestTrue(TEXT("Enemy enters an empty far-edge tile"), Enemy->EnterBoard(AtOffset(3, 0))))
	{
		return false;
	}
	TestEqual(TEXT("Enemy holds exactly one tile after entering"), CountTilesHeldBy(*Grid, Enemy), 1);

	float Elapsed = 0.f;
	for (int32 Tick = 0; Tick < 1200 && ExitCount == 0; ++Tick)
	{
		Enemy->AdvanceMovement(Dt);
		Elapsed += Dt;
	}

	TestEqual(TEXT("Exit event fires exactly once"), ExitCount, 1);
	TestEqual(TEXT("Enemy exits on the back row"), RowOf(ExitCoord), BackRow);
	TestTrue(FString::Printf(TEXT("Crossing time %.2fs is %d hexes at %.1f hexes/s"), Elapsed, BackRow, Speed),
		FMath::IsNearlyEqual(Elapsed, BackRow / Speed, 0.1f));
	// IsValid() would pass for a destroyed enemy's stale pointer whether or not the release ran;
	// only an explicitly cleared tile is null, and the clear is what tells OnOccupancyChanged listeners.
	TestTrue(TEXT("Exit tile is explicitly released, not left stale"), Grid->GetTile(ExitCoord)->Occupant.IsExplicitlyNull());

	return true;
}

/**
 * A lone blocker in the enemy's lane: from one row above the back row there are two ways out. Block
 * either in turn and the enemy must take the other, without ever overwriting the blocker.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseLoneBlockerTest, "Terrabound.Units.EnemyBase.LoneBlocker", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEnemyBaseLoneBlockerTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace EnemyBaseTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid) || !TestTrue(TEXT("Grid generated tiles"), Grid->GetTileCount() > 0))
	{
		return false;
	}

	const int32 BackRow = RowOf(Grid->GetBackRowCoords()[0]);
	const FHexCoord Start = AtOffset(3, BackRow - 1);
	TArray<FHexCoord> Exits;
	for (const FHexCoord& Neighbour : UHexCoordinateLibrary::GetNeighbours(Start))
	{
		if (Grid->IsValidCoord(Neighbour) && RowOf(Neighbour) == BackRow)
		{
			Exits.Add(Neighbour);
		}
	}
	if (!TestEqual(TEXT("Fixture: start has two back-row neighbours"), Exits.Num(), 2))
	{
		return false;
	}

	for (int32 BlockedIndex = 0; BlockedIndex < 2; ++BlockedIndex)
	{
		const FHexCoord Blocked = Exits[BlockedIndex];
		const FHexCoord Free = Exits[1 - BlockedIndex];
		ABoardUnitBase* Blocker = SpawnBlocker(TestWorld.GetWorld(), *Grid, Blocked);

		AEnemyBase* Enemy = SpawnEnemy(TestWorld.GetWorld());
		int32 ExitCount = 0;
		FHexCoord ExitCoord;
		Enemy->OnEnemyExited.AddLambda([&](AEnemyBase* Exited)
		{
			++ExitCount;
			ExitCoord = Exited->GetCurrentCoord();
		});
		if (!TestTrue(TEXT("Enemy enters the start tile"), Enemy->EnterBoard(Start)))
		{
			return false;
		}

		for (int32 Tick = 0; Tick < 600 && ExitCount == 0; ++Tick)
		{
			Enemy->AdvanceMovement(Dt);
			TestTrue(TEXT("Blocker still holds its tile"), Grid->GetTile(Blocked)->Occupant.Get() == Blocker);
		}

		TestEqual(TEXT("Enemy exits"), ExitCount, 1);
		TestTrue(FString::Printf(TEXT("Enemy exits through the free hex when %s is blocked"), *Blocked.ToString()), ExitCoord == Free);

		// Reset for the second pass: the blocker is the only thing left on the grid.
		Grid->ClearOccupant(Blocked);
		Blocker->Destroy();
	}

	return true;
}

/**
 * A wall with no gap: the search routes through it at a cost, but the step refuses to enter, so the
 * enemy walks up and holds. It never overwrites a wall unit and never holds two tiles. Opening a gap
 * lets it through - occupancy only ever delays.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseSealedRowTest, "Terrabound.Units.EnemyBase.SealedRow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEnemyBaseSealedRowTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace EnemyBaseTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid) || !TestTrue(TEXT("Grid generated tiles"), Grid->GetTileCount() > 0))
	{
		return false;
	}

	const int32 BackRow = RowOf(Grid->GetBackRowCoords()[0]);
	const int32 WallRow = BackRow - 3;

	TArray<FHexCoord> WallCoords;
	TArray<ABoardUnitBase*> WallUnits;
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		if (RowOf(Coord) == WallRow)
		{
			WallCoords.Add(Coord);
			WallUnits.Add(SpawnBlocker(TestWorld.GetWorld(), *Grid, Coord));
		}
	}
	if (!TestTrue(TEXT("Fixture: wall row is populated"), WallCoords.Num() > 0))
	{
		return false;
	}

	AEnemyBase* Enemy = SpawnEnemy(TestWorld.GetWorld());
	int32 ExitCount = 0;
	Enemy->OnEnemyExited.AddLambda([&](AEnemyBase*) { ++ExitCount; });
	if (!TestTrue(TEXT("Enemy enters an empty far-edge tile"), Enemy->EnterBoard(AtOffset(3, 0))))
	{
		return false;
	}

	bool bWallIntact = true;
	bool bHeldOneTile = true;
	for (int32 Tick = 0; Tick < 600; ++Tick)
	{
		Enemy->AdvanceMovement(Dt);
		for (int32 Index = 0; Index < WallCoords.Num(); ++Index)
		{
			bWallIntact &= Grid->GetTile(WallCoords[Index])->Occupant.Get() == WallUnits[Index];
		}
		bHeldOneTile &= CountTilesHeldBy(*Grid, Enemy) == 1;
	}

	TestTrue(TEXT("Enemy never overwrites a wall unit"), bWallIntact);
	TestTrue(TEXT("Enemy holds exactly one tile at every tick"), bHeldOneTile);
	TestEqual(TEXT("Enemy never exits through a sealed row"), ExitCount, 0);
	TestEqual(TEXT("Enemy walked up to the wall and held"), RowOf(Enemy->GetCurrentCoord()), WallRow - 1);

	// Open a gap: the delay ends and the enemy gets through.
	Grid->ClearOccupant(WallCoords[WallCoords.Num() / 2]);
	for (int32 Tick = 0; Tick < 1200 && ExitCount == 0; ++Tick)
	{
		Enemy->AdvanceMovement(Dt);
	}
	TestEqual(TEXT("Enemy exits once a gap opens"), ExitCount, 1);

	return true;
}

/** A start hex someone already holds is refused, and the holder keeps it. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyBaseOccupiedStartTest, "Terrabound.Units.EnemyBase.OccupiedStart", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FEnemyBaseOccupiedStartTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace EnemyBaseTests;

	FScopedGridWorld TestWorld;
	UHexGrid* Grid = TestWorld.GetGrid();
	if (!TestNotNull(TEXT("HexGrid subsystem exists"), Grid) || !TestTrue(TEXT("Grid generated tiles"), Grid->GetTileCount() > 0))
	{
		return false;
	}

	const FHexCoord Start = AtOffset(3, 0);
	ABoardUnitBase* Holder = SpawnBlocker(TestWorld.GetWorld(), *Grid, Start);
	AEnemyBase* Enemy = SpawnEnemy(TestWorld.GetWorld());

	TestFalse(TEXT("Enemy cannot enter an occupied hex"), Enemy->EnterBoard(Start));
	TestTrue(TEXT("Holder keeps the tile"), Grid->GetTile(Start)->Occupant.Get() == Holder);
	TestEqual(TEXT("Refused enemy holds no tile"), CountTilesHeldBy(*Grid, Enemy), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
