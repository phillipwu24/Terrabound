// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChampionMerger.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Data/ChampionData.h"

namespace ChampionMergerTests
{
	constexpr int32 Copies = 3;
	constexpr int32 MaxStar = 3;

	FMergeCandidate Make(const UChampionData* Data, int32 Star, int32 Priority)
	{
		FMergeCandidate Candidate;
		Candidate.Data = Data;
		Candidate.StarLevel = Star;
		Candidate.SurvivorPriority = Priority;
		return Candidate;
	}

	bool Select(const TArray<FMergeCandidate>& Candidates, int32& OutSurvivor, TArray<int32>& OutConsumed)
	{
		return UChampionMerger::SelectMergeGroup(Candidates, Copies, MaxStar, OutSurvivor, OutConsumed);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChampionMergeTrioTest, "Terrabound.Economy.ChampionMerge.Trio", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FChampionMergeTrioTest::RunTest(const FString& Parameters)
{
	using namespace ChampionMergerTests;

	const UChampionData* Grux = NewObject<UChampionData>(GetTransientPackage());
	int32 Survivor = INDEX_NONE;
	TArray<int32> Consumed;

	TArray<FMergeCandidate> Two = { Make(Grux, 1, 1), Make(Grux, 1, 1) };
	TestFalse(TEXT("Two copies don't merge"), Select(Two, Survivor, Consumed));

	TArray<FMergeCandidate> Three = { Make(Grux, 1, 1), Make(Grux, 1, 1), Make(Grux, 1, 1) };
	TestTrue(TEXT("Three copies merge"), Select(Three, Survivor, Consumed));
	TestEqual(TEXT("Two copies consumed"), Consumed.Num(), 2);
	TestFalse(TEXT("Survivor isn't also consumed"), Consumed.Contains(Survivor));

	// Copies below the merge threshold of 2 would merge everything into nothing - refused outright.
	TestFalse(TEXT("CopiesPerStarUp of 1 never merges"), UChampionMerger::SelectMergeGroup(Three, 1, MaxStar, Survivor, Consumed));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChampionMergeIdentityTest, "Terrabound.Economy.ChampionMerge.Identity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FChampionMergeIdentityTest::RunTest(const FString& Parameters)
{
	using namespace ChampionMergerTests;

	const UChampionData* Grux = NewObject<UChampionData>(GetTransientPackage());
	const UChampionData* Kwang = NewObject<UChampionData>(GetTransientPackage());
	int32 Survivor = INDEX_NONE;
	TArray<int32> Consumed;

	// Two of one champion and one of another: three bodies, but no trio of anyone.
	TArray<FMergeCandidate> DifferentChampions = { Make(Grux, 1, 1), Make(Grux, 1, 1), Make(Kwang, 1, 1) };
	TestFalse(TEXT("Different champions don't pool"), Select(DifferentChampions, Survivor, Consumed));

	// Star levels don't pool either: two 1-stars and a 2-star is not a trio.
	TArray<FMergeCandidate> DifferentStars = { Make(Grux, 1, 1), Make(Grux, 1, 1), Make(Grux, 2, 1) };
	TestFalse(TEXT("Different star levels don't pool"), Select(DifferentStars, Survivor, Consumed));

	// A trio surrounded by other units merges, and only the trio's members are touched.
	TArray<FMergeCandidate> Mixed = { Make(Kwang, 1, 0), Make(Grux, 1, 1), Make(Grux, 2, 1), Make(Grux, 1, 1), Make(Grux, 1, 1) };
	TestTrue(TEXT("Trio found among other units"), Select(Mixed, Survivor, Consumed));
	TArray<int32> Touched = Consumed;
	Touched.Add(Survivor);
	Touched.Sort();
	TestTrue(TEXT("Only the three 1-star Gruxes merge"), Touched == TArray<int32>({ 1, 3, 4 }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChampionMergeSurvivorTest, "Terrabound.Economy.ChampionMerge.Survivor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FChampionMergeSurvivorTest::RunTest(const FString& Parameters)
{
	using namespace ChampionMergerTests;

	const UChampionData* Grux = NewObject<UChampionData>(GetTransientPackage());
	int32 Survivor = INDEX_NONE;
	TArray<int32> Consumed;

	// Board copy survives over bench and pending, wherever it sits in the list.
	TArray<FMergeCandidate> Candidates = {
		Make(Grux, 1, FMergeCandidate::SurvivorPriorityPending),
		Make(Grux, 1, FMergeCandidate::SurvivorPriorityBench),
		Make(Grux, 1, FMergeCandidate::SurvivorPriorityBoard) };
	TestTrue(TEXT("Trio merges"), Select(Candidates, Survivor, Consumed));
	TestEqual(TEXT("Board copy is the survivor"), Survivor, 2);

	// With no board copy, a bench copy survives - the pending one never does.
	Candidates = {
		Make(Grux, 1, FMergeCandidate::SurvivorPriorityPending),
		Make(Grux, 1, FMergeCandidate::SurvivorPriorityBench),
		Make(Grux, 1, FMergeCandidate::SurvivorPriorityBench) };
	TestTrue(TEXT("Trio merges"), Select(Candidates, Survivor, Consumed));
	TestTrue(TEXT("Bench copy is the survivor"), Survivor == 1 || Survivor == 2);
	TestTrue(TEXT("Pending copy is consumed"), Consumed.Contains(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FChampionMergeStarLimitTest, "Terrabound.Economy.ChampionMerge.StarLimit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FChampionMergeStarLimitTest::RunTest(const FString& Parameters)
{
	using namespace ChampionMergerTests;

	const UChampionData* Grux = NewObject<UChampionData>(GetTransientPackage());
	int32 Survivor = INDEX_NONE;
	TArray<int32> Consumed;

	TArray<FMergeCandidate> MaxedOut = { Make(Grux, MaxStar, 1), Make(Grux, MaxStar, 1), Make(Grux, MaxStar, 1) };
	TestFalse(TEXT("Copies at MaxStarLevel never merge"), Select(MaxedOut, Survivor, Consumed));

	TArray<FMergeCandidate> OneBelowMax = { Make(Grux, MaxStar - 1, 1), Make(Grux, MaxStar - 1, 1), Make(Grux, MaxStar - 1, 1) };
	TestTrue(TEXT("Copies one below MaxStarLevel do merge"), Select(OneBelowMax, Survivor, Consumed));

	// Two trios at once: the lower star level goes first.
	TArray<FMergeCandidate> TwoTrios = {
		Make(Grux, 2, 1), Make(Grux, 2, 1), Make(Grux, 2, 1),
		Make(Grux, 1, 1), Make(Grux, 1, 1), Make(Grux, 1, 1) };
	TestTrue(TEXT("Trio found"), Select(TwoTrios, Survivor, Consumed));
	TestEqual(TEXT("Lowest star level merges first"), TwoTrios[Survivor].StarLevel, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
