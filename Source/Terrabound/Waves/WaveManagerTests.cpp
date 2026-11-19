// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveManager.h"
#include "RoundManager.h"
#include "SpawnHexConfig.h"
#include "WaveDefinition.h"
#include "../Data/EnemyData.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexGridTestWorld.h"
#include "../Grid/HexCoordinates.h"
#include "../Units/EnemyBase.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "EngineUtils.h"

// The test world never ticks, so these drive UWaveManager::AdvanceSpawning (and, where enemies must
// walk, AEnemyBase::AdvanceMovement) on a fixed step. Each test spawns its own ASpawnHexConfig with
// Min == Max so spawn timing is exact, flags spawn hexes directly on the grid, and calls StartWave
// itself. RoundManager stays in Prep, so the wave-resolved report is logged and ignored; leaks
// still count. A "death" is simulated by destroying the enemy, which is what resolves it.
namespace WaveManagerTests
{
	using namespace HexGridTests;

	constexpr float Dt = 1.f / 60.f;

	static FHexCoord AtOffset(int32 Col, int32 Row)
	{
		return UHexCoordinateLibrary::OffsetToAxial(Col, Row);
	}

	static UEnemyData* MakeEnemyData()
	{
		UEnemyData* Data = NewObject<UEnemyData>(GetTransientPackage());
		Data->MoveSpeedHexesPerSecond = 2.f;
		Data->SpawnHoldSeconds = 0.f;
		return Data;
	}

	static FWaveRosterEntry MakeEntry(UEnemyData* Data, int32 Count, ESpawnWindow Window)
	{
		FWaveRosterEntry Entry;
		Entry.Enemy = Data;
		Entry.Count = Count;
		Entry.SpawnWindow = Window;
		return Entry;
	}

	/** Flags SpawnHexes on the grid and places a config with a fixed spawn delay and wave gap. Returns the WaveManager. */
	static UWaveManager* SetUp(FAutomationTestBase& Test, const FScopedGridWorld& TestWorld, const TArray<FHexCoord>& SpawnHexes, float Interval, float WaveGap = 0.f)
	{
		UHexGrid* Grid = TestWorld.GetGrid();
		UWaveManager* Waves = TestWorld.GetWorld()->GetSubsystem<UWaveManager>();
		if (!Test.TestNotNull(TEXT("HexGrid subsystem exists"), Grid) || !Test.TestTrue(TEXT("Grid generated tiles"), Grid->GetTileCount() > 0)
			|| !Test.TestNotNull(TEXT("WaveManager subsystem exists"), Waves))
		{
			return nullptr;
		}
		for (const FHexCoord& Coord : SpawnHexes)
		{
			Grid->SetSpawnFlag(Coord, true);
		}
		ASpawnHexConfig* Config = TestWorld.GetWorld()->SpawnActor<ASpawnHexConfig>();
		Config->MinSpawnInterval = Interval;
		Config->MaxSpawnInterval = Interval;
		Config->WaveGapSeconds = WaveGap;
		return Waves;
	}

	static TArray<AEnemyBase*> GetEnemies(UWorld* World)
	{
		TArray<AEnemyBase*> Enemies;
		for (TActorIterator<AEnemyBase> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Enemies.Add(*It);
			}
		}
		return Enemies;
	}

	/** Advances spawning for Seconds; with bWalk, every live enemy walks too. */
	static void Run(UWaveManager& Waves, UWorld* World, float Seconds, bool bWalk)
	{
		for (float Elapsed = 0.f; Elapsed < Seconds; Elapsed += Dt)
		{
			Waves.AdvanceSpawning(Dt);
			if (bWalk)
			{
				for (AEnemyBase* Enemy : GetEnemies(World))
				{
					Enemy->AdvanceMovement(Dt);
				}
			}
		}
	}
}

/** Spawn timer (PLAN.md 1.4b, D14): the first enemy comes at once, the next only after the delay. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveManagerTimerTest, "Terrabound.Waves.WaveManager.Timer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWaveManagerTimerTest::RunTest(const FString& Parameters)
{
	using namespace WaveManagerTests;

	FScopedGridWorld TestWorld;
	UWaveManager* Waves = SetUp(*this, TestWorld, { AtOffset(1, 0), AtOffset(5, 0) }, 1.f);
	if (!Waves)
	{
		return false;
	}

	FWaveDefinition Wave;
	Wave.Roster.Add(MakeEntry(MakeEnemyData(), 2, ESpawnWindow::Any));
	Waves->StartWave(Wave, true);
	TestEqual(TEXT("Both enemies start pending"), Waves->GetPendingCount(), 2);

	Waves->AdvanceSpawning(Dt);
	TestEqual(TEXT("The first enemy spawns at once"), Waves->GetAliveCount(), 1);

	Run(*Waves, TestWorld.GetWorld(), 0.9f, false);
	TestEqual(TEXT("No second spawn before the delay"), Waves->GetAliveCount(), 1);

	Run(*Waves, TestWorld.GetWorld(), 0.2f, false);
	TestEqual(TEXT("The second enemy spawns once the delay has passed"), Waves->GetAliveCount(), 2);
	TestEqual(TEXT("Nothing left pending"), Waves->GetPendingCount(), 0);

	return true;
}

/**
 * Wave gap (D0): a later wave of the round holds its first spawn for WaveGapSeconds; the round's
 * first wave still spawns at once.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveManagerGapTest, "Terrabound.Waves.WaveManager.Gap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWaveManagerGapTest::RunTest(const FString& Parameters)
{
	using namespace WaveManagerTests;

	FScopedGridWorld TestWorld;
	UWaveManager* Waves = SetUp(*this, TestWorld, { AtOffset(3, 0) }, 1.f, 3.f);
	if (!Waves)
	{
		return false;
	}

	FWaveDefinition Wave;
	Wave.Roster.Add(MakeEntry(MakeEnemyData(), 1, ESpawnWindow::Any));

	Waves->StartWave(Wave, false);
	Run(*Waves, TestWorld.GetWorld(), 2.9f, false);
	TestEqual(TEXT("A later wave spawns nothing during the gap"), Waves->GetAliveCount(), 0);
	Run(*Waves, TestWorld.GetWorld(), 0.2f, false);
	const TArray<AEnemyBase*> Enemies = GetEnemies(TestWorld.GetWorld());
	if (!TestEqual(TEXT("A later wave spawns once the gap has passed"), Enemies.Num(), 1))
	{
		return false;
	}
	Enemies[0]->Destroy();
	TestFalse(TEXT("That wave resolves"), Waves->IsWaveActive());

	Waves->StartWave(Wave, true);
	Waves->AdvanceSpawning(Dt);
	TestEqual(TEXT("The round's first wave spawns at once despite the gap"), Waves->GetAliveCount(), 1);

	return true;
}

/**
 * Occupancy only delays (PLAN.md 1.4b, D14): with the only spawn hex held, nothing spawns and the
 * timer waits at zero; the tick after the hex clears, the enemy spawns.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveManagerOccupancyTest, "Terrabound.Waves.WaveManager.Occupancy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWaveManagerOccupancyTest::RunTest(const FString& Parameters)
{
	using namespace WaveManagerTests;

	FScopedGridWorld TestWorld;
	const FHexCoord SpawnHex = AtOffset(3, 0);
	UWaveManager* Waves = SetUp(*this, TestWorld, { SpawnHex }, 0.5f);
	if (!Waves)
	{
		return false;
	}

	UHexGrid* Grid = TestWorld.GetGrid();
	ABoardUnitBase* Blocker = TestWorld.GetWorld()->SpawnActor<ABoardUnitBase>();
	Grid->SetOccupant(SpawnHex, Blocker);

	FWaveDefinition Wave;
	Wave.Roster.Add(MakeEntry(MakeEnemyData(), 1, ESpawnWindow::Any));
	Waves->StartWave(Wave, true);

	Run(*Waves, TestWorld.GetWorld(), 2.f, false);
	TestEqual(TEXT("Nothing spawns onto a held spawn hex"), Waves->GetAliveCount(), 0);
	TestEqual(TEXT("The enemy is still pending"), Waves->GetPendingCount(), 1);
	TestEqual(TEXT("The timer waits at zero"), Waves->GetTimeUntilNextSpawn(), 0.f);
	TestTrue(TEXT("The blocker keeps its hex"), Grid->GetTile(SpawnHex)->Occupant.Get() == Blocker);

	Grid->ClearOccupant(SpawnHex);
	Blocker->Destroy();
	Waves->AdvanceSpawning(Dt);
	TestEqual(TEXT("The enemy spawns the tick the hex clears"), Waves->GetAliveCount(), 1);

	return true;
}

/**
 * Exits (PLAN.md 1.4b, D20): an exit is reported to RoundManager as a leak, progress counts spawns
 * rather than exits, and the wave resolves once every enemy has exited.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveManagerExitsTest, "Terrabound.Waves.WaveManager.Exits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWaveManagerExitsTest::RunTest(const FString& Parameters)
{
	using namespace WaveManagerTests;

	FScopedGridWorld TestWorld;
	UWaveManager* Waves = SetUp(*this, TestWorld, { AtOffset(3, 0) }, 0.5f);
	URoundManager* Rounds = TestWorld.GetWorld()->GetSubsystem<URoundManager>();
	if (!Waves || !TestNotNull(TEXT("RoundManager subsystem exists"), Rounds))
	{
		return false;
	}

	FWaveDefinition Wave;
	Wave.Roster.Add(MakeEntry(MakeEnemyData(), 2, ESpawnWindow::Any));
	Waves->StartWave(Wave, true);
	Waves->AdvanceSpawning(Dt);
	TestEqual(TEXT("One of two spawned makes progress one half"), Waves->GetProgress(), 0.5f);

	float ProgressAtFirstLeak = -1.f;
	for (int32 Tick = 0; Tick < 900 && Waves->IsWaveActive(); ++Tick)
	{
		Run(*Waves, TestWorld.GetWorld(), Dt, true);
		if (ProgressAtFirstLeak < 0.f && Rounds->GetLeaksTotal() == 1)
		{
			ProgressAtFirstLeak = Waves->GetProgress();
		}
	}

	// Both have spawned long before the first crosses, so the exit itself adds nothing.
	TestEqual(TEXT("Progress is 1 at the first exit, from spawns alone"), ProgressAtFirstLeak, 1.f);
	TestEqual(TEXT("Both exits are reported as leaks"), Rounds->GetLeaksTotal(), 2);
	TestFalse(TEXT("The wave resolves once every enemy has exited"), Waves->IsWaveActive());

	return true;
}

/**
 * Early window (PLAN.md 1.4b, D20): an Early entry is first in line while its window is open, and
 * one that misses the window becomes Any, so it still spawns and the wave still resolves.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveManagerEarlyTest, "Terrabound.Waves.WaveManager.Early", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWaveManagerEarlyTest::RunTest(const FString& Parameters)
{
	using namespace WaveManagerTests;

	FScopedGridWorld TestWorld;
	UWaveManager* Waves = SetUp(*this, TestWorld, { AtOffset(3, 0) }, 0.1f);
	if (!Waves)
	{
		return false;
	}

	// First in line: ten fresh waves of three Any and one Early, and the Early one always spawns first.
	UEnemyData* AnyData = MakeEnemyData();
	UEnemyData* EarlyData = MakeEnemyData();
	FWaveDefinition Mixed;
	Mixed.Roster.Add(MakeEntry(AnyData, 3, ESpawnWindow::Any));
	Mixed.Roster.Add(MakeEntry(EarlyData, 1, ESpawnWindow::Early));
	bool bEarlyAlwaysFirst = true;
	for (int32 Attempt = 0; Attempt < 10; ++Attempt)
	{
		Waves->StartWave(Mixed, true);
		Waves->AdvanceSpawning(Dt);
		const TArray<AEnemyBase*> Enemies = GetEnemies(TestWorld.GetWorld());
		if (!TestEqual(TEXT("One enemy spawned"), Enemies.Num(), 1))
		{
			return false;
		}
		bEarlyAlwaysFirst &= Enemies[0]->GetEnemyData() == EarlyData;
		Enemies[0]->Destroy();
	}
	TestTrue(TEXT("The Early entry spawns first while its window is open"), bEarlyAlwaysFirst);

	// Missed window: two Early; once the first has spawned, progress is past the window. The first
	// is then destroyed only to free the one spawn hex.
	FWaveDefinition AllEarly;
	AllEarly.Roster.Add(MakeEntry(EarlyData, 2, ESpawnWindow::Early));
	Waves->StartWave(AllEarly, true);
	Waves->AdvanceSpawning(Dt);
	TArray<AEnemyBase*> Enemies = GetEnemies(TestWorld.GetWorld());
	if (!TestEqual(TEXT("The first Early enemy spawns"), Enemies.Num(), 1))
	{
		return false;
	}
	TestTrue(TEXT("Progress is past the Early window on the spawn alone"), Waves->GetProgress() >= 0.5f);
	Enemies[0]->Destroy();

	Run(*Waves, TestWorld.GetWorld(), 0.5f, false);
	Enemies = GetEnemies(TestWorld.GetWorld());
	if (!TestEqual(TEXT("The Early enemy that missed its window still spawns"), Enemies.Num(), 1))
	{
		return false;
	}
	Enemies[0]->Destroy();
	TestFalse(TEXT("The wave resolves"), Waves->IsWaveActive());

	return true;
}

/**
 * Late window (PLAN.md 1.4b, D20): a Late entry waits while any non-Late enemy is still to spawn,
 * and opens once they all have, though they are still alive and progress is below LateWindowStart.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWaveManagerLateTest, "Terrabound.Waves.WaveManager.Late", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FWaveManagerLateTest::RunTest(const FString& Parameters)
{
	using namespace WaveManagerTests;

	FScopedGridWorld TestWorld;
	UWaveManager* Waves = SetUp(*this, TestWorld, { AtOffset(1, 0), AtOffset(3, 0), AtOffset(5, 0) }, 0.1f);
	if (!Waves)
	{
		return false;
	}

	UEnemyData* AnyData = MakeEnemyData();
	UEnemyData* LateData = MakeEnemyData();
	FWaveDefinition Wave;
	Wave.Roster.Add(MakeEntry(AnyData, 2, ESpawnWindow::Any));
	Wave.Roster.Add(MakeEntry(LateData, 1, ESpawnWindow::Late));
	Waves->StartWave(Wave, true);

	// Spawns at 0 s and 0.1 s: both must be the Any enemies, with the Late one held back.
	Run(*Waves, TestWorld.GetWorld(), 0.15f, false);
	TArray<AEnemyBase*> Enemies = GetEnemies(TestWorld.GetWorld());
	if (!TestEqual(TEXT("Two enemies spawned"), Enemies.Num(), 2))
	{
		return false;
	}
	TestTrue(TEXT("Both are the Any enemies"), Enemies[0]->GetEnemyData() == AnyData && Enemies[1]->GetEnemyData() == AnyData);
	TestEqual(TEXT("The Late enemy waited"), Waves->GetPendingCountInWindow(ESpawnWindow::Late), 1);
	TestTrue(TEXT("Progress is below LateWindowStart"), Waves->GetProgress() < 0.95f);

	// Nothing is destroyed: the Late enemy needs only the others to have spawned.
	Run(*Waves, TestWorld.GetWorld(), 0.2f, false);
	Enemies = GetEnemies(TestWorld.GetWorld());
	if (!TestEqual(TEXT("The Late enemy spawns while the others are still alive"), Enemies.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("Nothing left pending"), Waves->GetPendingCount(), 0);
	for (AEnemyBase* Enemy : Enemies)
	{
		Enemy->Destroy();
	}
	TestFalse(TEXT("The wave resolves"), Waves->IsWaveActive());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
