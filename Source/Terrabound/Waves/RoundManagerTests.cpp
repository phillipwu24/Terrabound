// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoundManager.h"
#include "WaveDefinition.h"
#include "../Grid/HexGridTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

// These drive NotifyWaveResolved by hand, standing in for WaveManager (1.4b). The rounds table is
// built in code, so the tests don't depend on DT_Rounds. The test world has no player controller,
// so the carried-unit refusal is checked in PIE instead.
namespace RoundManagerTests
{
	/** A transient FRoundRow table with one row per entry of WaveCounts, each wave an empty roster. */
	static UDataTable* MakeRoundsTable(const TArray<int32>& WaveCounts)
	{
		UDataTable* Table = NewObject<UDataTable>(GetTransientPackage());
		Table->RowStruct = FRoundRow::StaticStruct();
		for (int32 Index = 0; Index < WaveCounts.Num(); ++Index)
		{
			FRoundRow Row;
			Row.Waves.SetNum(WaveCounts[Index]);
			Table->AddRow(FName(*FString::Printf(TEXT("Round%d"), Index + 1)), Row);
		}
		return Table;
	}

	static URoundManager* GetRoundManager(FAutomationTestBase& Test, const HexGridTests::FScopedGridWorld& TestWorld)
	{
		URoundManager* Rounds = TestWorld.GetWorld()->GetSubsystem<URoundManager>();
		Test.TestNotNull(TEXT("RoundManager subsystem exists"), Rounds);
		return Rounds;
	}
}

/**
 * Round flow (PLAN.md 1.4a, D0): Start moves Prep -> Combat; the round stays in Combat until its last
 * wave resolves, then returns to Prep with the round index advanced. Start refuses in Combat. A
 * round past the table repeats the last row.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundManagerFlowTest, "Terrabound.Waves.RoundManager.Flow", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundManagerFlowTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace RoundManagerTests;

	FScopedGridWorld TestWorld;
	URoundManager* Rounds = GetRoundManager(*this, TestWorld);
	if (!Rounds)
	{
		return false;
	}
	Rounds->SetRoundsTable(MakeRoundsTable({ 3, 1 }));

	TArray<ERoundPhase> PhaseEvents;
	Rounds->OnPhaseChanged.AddLambda([&](ERoundPhase NewPhase) { PhaseEvents.Add(NewPhase); });

	TestTrue(TEXT("A new run starts in Prep"), Rounds->GetPhase() == ERoundPhase::Prep);
	TestEqual(TEXT("A new run starts on round index 0"), Rounds->GetRoundIndex(), 0);

	if (!TestTrue(TEXT("StartRound succeeds in Prep"), Rounds->StartRound()))
	{
		return false;
	}
	TestTrue(TEXT("Start enters Combat"), Rounds->GetPhase() == ERoundPhase::Combat);
	TestEqual(TEXT("Round 1 has three waves"), Rounds->GetWaveCount(), 3);
	TestEqual(TEXT("Wave 1 is in progress"), Rounds->GetWaveIndex(), 0);
	TestFalse(TEXT("StartRound refuses in Combat"), Rounds->StartRound());
	TestEqual(TEXT("The refused Start changed nothing"), Rounds->GetWaveIndex(), 0);

	Rounds->NotifyWaveResolved();
	TestTrue(TEXT("Still Combat after wave 1 resolves"), Rounds->GetPhase() == ERoundPhase::Combat);
	TestEqual(TEXT("Wave 2 starts at once"), Rounds->GetWaveIndex(), 1);

	Rounds->NotifyWaveResolved();
	TestTrue(TEXT("Still Combat after wave 2 resolves"), Rounds->GetPhase() == ERoundPhase::Combat);
	TestEqual(TEXT("Wave 3 starts at once"), Rounds->GetWaveIndex(), 2);

	Rounds->NotifyWaveResolved();
	TestTrue(TEXT("Back to Prep after the last wave"), Rounds->GetPhase() == ERoundPhase::Prep);
	TestEqual(TEXT("Round index advances"), Rounds->GetRoundIndex(), 1);

	if (TestEqual(TEXT("Exactly two phase changes in round 1"), PhaseEvents.Num(), 2))
	{
		TestTrue(TEXT("First change is to Combat"), PhaseEvents[0] == ERoundPhase::Combat);
		TestTrue(TEXT("Second change is to Prep"), PhaseEvents[1] == ERoundPhase::Prep);
	}

	// Round 2 is the table's last row, with one wave; round 3 is past the table and repeats it.
	for (int32 ExpectedIndex = 1; ExpectedIndex <= 2; ++ExpectedIndex)
	{
		if (!TestTrue(FString::Printf(TEXT("StartRound succeeds for round index %d"), ExpectedIndex), Rounds->StartRound()))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("Round index %d uses the one-wave last row"), ExpectedIndex), Rounds->GetWaveCount(), 1);
		Rounds->NotifyWaveResolved();
		TestTrue(TEXT("One resolved wave ends a one-wave round"), Rounds->GetPhase() == ERoundPhase::Prep);
		TestEqual(TEXT("Round index advances"), Rounds->GetRoundIndex(), ExpectedIndex + 1);
	}

	return true;
}

/** Leaks (PLAN.md 1.4a): both counters count; the per-round one resets at the next Start. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundManagerLeaksTest, "Terrabound.Waves.RoundManager.Leaks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundManagerLeaksTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace RoundManagerTests;

	FScopedGridWorld TestWorld;
	URoundManager* Rounds = GetRoundManager(*this, TestWorld);
	if (!Rounds)
	{
		return false;
	}
	Rounds->SetRoundsTable(MakeRoundsTable({ 1 }));

	if (!TestTrue(TEXT("Round 1 starts"), Rounds->StartRound()))
	{
		return false;
	}
	Rounds->NotifyEnemyExited();
	Rounds->NotifyEnemyExited();
	TestEqual(TEXT("Two leaks this round"), Rounds->GetLeaksThisRound(), 2);
	TestEqual(TEXT("Two leaks total"), Rounds->GetLeaksTotal(), 2);

	Rounds->NotifyWaveResolved();
	TestEqual(TEXT("The round's leaks stay readable in Prep"), Rounds->GetLeaksThisRound(), 2);

	if (!TestTrue(TEXT("Round 2 starts"), Rounds->StartRound()))
	{
		return false;
	}
	TestEqual(TEXT("Leaks this round reset at Start"), Rounds->GetLeaksThisRound(), 0);
	Rounds->NotifyEnemyExited();
	TestEqual(TEXT("One leak this round"), Rounds->GetLeaksThisRound(), 1);
	TestEqual(TEXT("Total keeps counting across rounds"), Rounds->GetLeaksTotal(), 3);

	return true;
}

/**
 * Refusals (PLAN.md 1.4a): with no round data, or a round with no waves, Start refuses and the run
 * stays in Prep. A wave resolving outside Combat is ignored.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoundManagerRefusalsTest, "Terrabound.Waves.RoundManager.Refusals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRoundManagerRefusalsTest::RunTest(const FString& Parameters)
{
	using namespace HexGridTests;
	using namespace RoundManagerTests;

	FScopedGridWorld TestWorld;
	URoundManager* Rounds = GetRoundManager(*this, TestWorld);
	if (!Rounds)
	{
		return false;
	}

	// StartRound logs an error on each of these refusals by design; don't fail the test for it.
	AddExpectedError(TEXT("StartRound refused - no round data"), EAutomationExpectedErrorFlags::Contains, 3);

	Rounds->SetRoundsTable(nullptr);
	TestFalse(TEXT("No table: Start refuses"), Rounds->StartRound());

	Rounds->SetRoundsTable(MakeRoundsTable({}));
	TestFalse(TEXT("Empty table: Start refuses"), Rounds->StartRound());

	Rounds->SetRoundsTable(MakeRoundsTable({ 0 }));
	TestFalse(TEXT("A round with no waves: Start refuses"), Rounds->StartRound());

	TestTrue(TEXT("Still in Prep after every refusal"), Rounds->GetPhase() == ERoundPhase::Prep);
	TestEqual(TEXT("Round index unchanged"), Rounds->GetRoundIndex(), 0);

	Rounds->NotifyWaveResolved();
	TestTrue(TEXT("A wave resolving in Prep is ignored"), Rounds->GetPhase() == ERoundPhase::Prep);
	TestEqual(TEXT("Round index still unchanged"), Rounds->GetRoundIndex(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
