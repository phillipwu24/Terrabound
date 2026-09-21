// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChampionMerger.h"
#include "Bench.h"
#include "../Grid/HexGrid.h"
#include "../TerraboundSettings.h"
#include "../Data/EconomyConfig.h"
#include "../Data/ChampionData.h"
#include "../Units/ChampionBase.h"

void UChampionMerger::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	Config = Settings ? Settings->EconomyConfig.LoadSynchronous() : nullptr;
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("UChampionMerger: no EconomyConfig set in Project Settings > Terrabound. Merging disabled."));
	}
}

bool UChampionMerger::SelectMergeGroup(const TArray<FMergeCandidate>& Candidates, int32 CopiesPerStarUp, int32 MaxStarLevel,
	int32& OutSurvivorIndex, TArray<int32>& OutConsumedIndices)
{
	OutConsumedIndices.Reset();
	if (CopiesPerStarUp < 2)
	{
		return false;
	}

	// Lowest star first, so which trio merges is well-defined when several exist at once.
	for (int32 StarLevel = 1; StarLevel < MaxStarLevel; ++StarLevel)
	{
		TMap<const UChampionData*, TArray<int32>> IndicesByChampion;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Candidates[Index].StarLevel == StarLevel && Candidates[Index].Data)
			{
				IndicesByChampion.FindOrAdd(Candidates[Index].Data).Add(Index);
			}
		}

		for (TPair<const UChampionData*, TArray<int32>>& Pair : IndicesByChampion)
		{
			TArray<int32>& Indices = Pair.Value;
			if (Indices.Num() < CopiesPerStarUp)
			{
				continue;
			}

			// Best survivor candidate first: a board copy over a bench copy over the pending one.
			Indices.StableSort([&Candidates](int32 A, int32 B)
			{
				return Candidates[A].SurvivorPriority < Candidates[B].SurvivorPriority;
			});
			OutSurvivorIndex = Indices[0];
			for (int32 Slot = 1; Slot < CopiesPerStarUp; ++Slot)
			{
				OutConsumedIndices.Add(Indices[Slot]);
			}
			return true;
		}
	}
	return false;
}

void UChampionMerger::GatherCandidates(AChampionBase* Pending, TArray<FMergeCandidate>& OutCandidates, TArray<AChampionBase*>& OutUnits) const
{
	const auto AddUnit = [&](AChampionBase* Unit, int32 SurvivorPriority)
	{
		if (Unit && Unit->GetChampionData())
		{
			FMergeCandidate Candidate;
			Candidate.Data = Unit->GetChampionData();
			Candidate.StarLevel = Unit->GetStarLevel();
			Candidate.SurvivorPriority = SurvivorPriority;
			OutCandidates.Add(Candidate);
			OutUnits.Add(Unit);
		}
	};

	if (const UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr)
	{
		for (const FHexCoord& Coord : Grid->GetAllTileCoords())
		{
			const FHexTile* Tile = Grid->GetTile(Coord);
			AddUnit(Tile ? Cast<AChampionBase>(Tile->Occupant.Get()) : nullptr, FMergeCandidate::SurvivorPriorityBoard);
		}
	}

	if (const UBench* Bench = GetWorld() ? GetWorld()->GetSubsystem<UBench>() : nullptr)
	{
		for (int32 SlotIndex = 0; SlotIndex < Bench->GetSlotCount(); ++SlotIndex)
		{
			AddUnit(Cast<AChampionBase>(Bench->GetChampionAt(SlotIndex)), FMergeCandidate::SurvivorPriorityBench);
		}
	}

	AddUnit(Pending, FMergeCandidate::SurvivorPriorityPending);
}

bool UChampionMerger::WouldCompleteMerge(const UChampionData* Data) const
{
	if (!Config || !Data)
	{
		return false;
	}

	TArray<FMergeCandidate> Candidates;
	TArray<AChampionBase*> Units;
	GatherCandidates(nullptr, Candidates, Units);

	FMergeCandidate Hypothetical;
	Hypothetical.Data = Data;
	Hypothetical.StarLevel = 1;
	Hypothetical.SurvivorPriority = FMergeCandidate::SurvivorPriorityPending;
	Candidates.Add(Hypothetical);

	int32 SurvivorIndex = INDEX_NONE;
	TArray<int32> ConsumedIndices;
	return SelectMergeGroup(Candidates, Config->CopiesPerStarUp, Config->MaxStarLevel, SurvivorIndex, ConsumedIndices);
}

bool UChampionMerger::TryMerge(AChampionBase* NewCopy)
{
	UWorld* World = GetWorld();
	UBench* Bench = World ? World->GetSubsystem<UBench>() : nullptr;
	UHexGrid* Grid = World ? World->GetSubsystem<UHexGrid>() : nullptr;
	if (!Config || !Bench || !Grid)
	{
		return false;
	}

	AChampionBase* Pending = NewCopy;
	bool bPendingConsumed = false;

	// Rebuilt every pass from live state: a merge changes both which copies exist and their star
	// levels, and the lists are tiny (bench slots + board tiles). Each pass destroys at least two
	// units, so the loop ends.
	while (true)
	{
		TArray<FMergeCandidate> Candidates;
		TArray<AChampionBase*> Units;
		GatherCandidates(Pending, Candidates, Units);

		int32 SurvivorIndex = INDEX_NONE;
		TArray<int32> ConsumedIndices;
		if (!SelectMergeGroup(Candidates, Config->CopiesPerStarUp, Config->MaxStarLevel, SurvivorIndex, ConsumedIndices))
		{
			break;
		}

		AChampionBase* Survivor = Units[SurvivorIndex];
		Survivor->SetStarLevel(Survivor->GetStarLevel() + 1);
		UE_LOG(LogTemp, Display, TEXT("UChampionMerger: %s merged to star %d."),
			*Survivor->GetChampionData()->DisplayName.ToString(), Survivor->GetStarLevel());

		for (const int32 ConsumedIndex : ConsumedIndices)
		{
			AChampionBase* Consumed = Units[ConsumedIndex];
			if (Consumed == Pending)
			{
				// Never placed anywhere, so there's no slot or tile to clear.
				bPendingConsumed = true;
				Pending = nullptr;
			}
			else if (Bench->FindSlotIndex(Consumed) != INDEX_NONE)
			{
				Bench->RemoveChampion(Consumed);
			}
			else
			{
				const FHexCoord Coord = Consumed->GetCurrentCoord();
				const FHexTile* Tile = Grid->GetTile(Coord);
				if (Tile && Tile->Occupant.Get() == Consumed)
				{
					Grid->ClearOccupant(Coord);
				}
			}
			Consumed->Destroy();
		}
	}

	return bPendingConsumed;
}
