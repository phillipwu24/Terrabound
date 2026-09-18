// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChampionPool.h"
#include "../Data/ChampionData.h"

void UChampionPool::Initialize(UDataTable* PoolTable, UDataTable* OddsTable)
{
	Entries.Reset();
	TierRollOdds.Reset();

	if (!PoolTable || !OddsTable)
	{
		UE_LOG(LogTemp, Error, TEXT("UChampionPool::Initialize: missing pool table or odds table."));
		return;
	}

	TArray<FChampionTierOddsRow*> OddsRows;
	OddsTable->GetAllRows<FChampionTierOddsRow>(TEXT("UChampionPool::Initialize"), OddsRows);
	for (const FChampionTierOddsRow* Row : OddsRows)
	{
		TierRollOdds.Add(Row->Tier, Row->RollOdds);
	}

	TArray<FChampionPoolRow*> PoolRows;
	PoolTable->GetAllRows<FChampionPoolRow>(TEXT("UChampionPool::Initialize"), PoolRows);
	for (const FChampionPoolRow* Row : PoolRows)
	{
		UChampionData* Data = Row->ChampionData.LoadSynchronous();
		if (!Data)
		{
			UE_LOG(LogTemp, Warning, TEXT("UChampionPool::Initialize: row with no ChampionData set, skipping."));
			continue;
		}

		FPoolEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.ChampionData = Data;
		Entry.Tier = Data->Tier;
		Entry.RemainingCopies = Row->PoolSize;
	}
}

UChampionData* UChampionPool::DrawRandomChampion()
{
	// One weight per tier that still has at least one copy left.
	TMap<int32, float> AvailableTierWeights;
	for (const FPoolEntry& Entry : Entries)
	{
		if (Entry.RemainingCopies > 0)
		{
			AvailableTierWeights.FindOrAdd(Entry.Tier) = TierRollOdds.FindRef(Entry.Tier);
		}
	}

	float TotalWeight = 0.f;
	for (const TPair<int32, float>& Pair : AvailableTierWeights)
	{
		TotalWeight += Pair.Value;
	}

	if (TotalWeight <= 0.f)
	{
		return nullptr;
	}

	// Tracks the last-examined tier as a fallback: floating-point error can leave Roll just above
	// zero after subtracting every weight, so relying solely on "Roll <= 0.f" to assign ChosenTier
	// can finish the loop without ever assigning it.
	int32 ChosenTier = 0;
	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (const TPair<int32, float>& Pair : AvailableTierWeights)
	{
		ChosenTier = Pair.Key;
		Roll -= Pair.Value;
		if (Roll <= 0.f)
		{
			break;
		}
	}

	int32 TotalCopiesInTier = 0;
	for (const FPoolEntry& Entry : Entries)
	{
		if (Entry.Tier == ChosenTier)
		{
			TotalCopiesInTier += Entry.RemainingCopies;
		}
	}

	int32 Pick = FMath::RandRange(1, TotalCopiesInTier);
	for (FPoolEntry& Entry : Entries)
	{
		if (Entry.Tier != ChosenTier)
		{
			continue;
		}
		Pick -= Entry.RemainingCopies;
		if (Pick <= 0)
		{
			--Entry.RemainingCopies;
			return Entry.ChampionData;
		}
	}

	return nullptr;
}

void UChampionPool::ReturnChampion(UChampionData* ChampionData)
{
	for (FPoolEntry& Entry : Entries)
	{
		if (Entry.ChampionData == ChampionData)
		{
			++Entry.RemainingCopies;
			return;
		}
	}
}
