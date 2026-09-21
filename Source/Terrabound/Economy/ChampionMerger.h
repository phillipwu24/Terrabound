// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ChampionMerger.generated.h"

class UChampionData;
class UEconomyConfig;
class AChampionBase;

/**
 * One champion copy as SelectMergeGroup sees it - plain data, no actor, so the merge rule is
 * testable without a world. Identity is the ChampionData asset (as in trait counting): two copies
 * are "the same champion" when they share it.
 */
struct FMergeCandidate
{
	const UChampionData* Data = nullptr;
	int32 StarLevel = 1;

	/** Lower wins the survivor slot: SurvivorPriorityBoard, then Bench, then Pending. */
	int32 SurvivorPriority = 0;

	static constexpr int32 SurvivorPriorityBoard = 0;
	static constexpr int32 SurvivorPriorityBench = 1;
	static constexpr int32 SurvivorPriorityPending = 2;
};

/**
 * TFT-style star-up: CopiesPerStarUp copies of the same champion at the same star level merge into
 * one copy a level higher. Runs only when a copy arrives (ShopSystem::Buy), never on drag - moving
 * a unit doesn't change what the player owns.
 *
 * Owns the merge, not ShopSystem, because a merge touches both UBench and UHexGrid (clearing the
 * consumed copies' slots/tiles) and ShopSystem deliberately touches neither. The survivor is
 * upgraded in place rather than recreated: it keeps its position, and once GAS exists its
 * attributes (CLAUDE.md: units persist). Merging destroys the other copies, an intentional removal
 * like selling.
 */
UCLASS()
class TERRABOUND_API UChampionMerger : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * The merge rule, pure. Finds the lowest star level (below MaxStarLevel) where some champion has
	 * at least CopiesPerStarUp copies. The survivor is the lowest-SurvivorPriority copy of that
	 * champion at that level and the next CopiesPerStarUp-1 by priority are consumed; ties are
	 * arbitrary, per CLAUDE.md's accepted drift. Returns false if nothing merges. Indices refer to
	 * Candidates.
	 */
	static bool SelectMergeGroup(const TArray<FMergeCandidate>& Candidates, int32 CopiesPerStarUp, int32 MaxStarLevel,
		int32& OutSurvivorIndex, TArray<int32>& OutConsumedIndices);

	/**
	 * True if a newly bought 1-star copy of Data would merge with what is already on the bench and
	 * board. ShopSystem uses this to let a completing copy be bought onto a full bench.
	 */
	bool WouldCompleteMerge(const UChampionData* Data) const;

	/**
	 * Merges every trio on the bench and board, cascading (a new 2-star can complete a trio of
	 * 2-stars). NewCopy is a just-bought champion not yet on the bench; it counts as a copy but is
	 * never the survivor. Returns true if NewCopy was consumed and destroyed - the caller must then
	 * not place it. Otherwise the caller places it as usual.
	 */
	bool TryMerge(AChampionBase* NewCopy);

private:
	/** Fills OutCandidates and a parallel OutUnits array (same indices) from bench, board, Pending. */
	void GatherCandidates(AChampionBase* Pending, TArray<FMergeCandidate>& OutCandidates, TArray<AChampionBase*>& OutUnits) const;

	UPROPERTY()
	TObjectPtr<UEconomyConfig> Config;
};
