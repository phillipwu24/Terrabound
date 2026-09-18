// Copyright Epic Games, Inc. All Rights Reserved.

#include "Bench.h"
#include "../TerraboundSettings.h"
#include "../Data/EconomyConfig.h"
#include "../Units/BoardUnitBase.h"

void UBench::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	const UEconomyConfig* Config = Settings ? Settings->EconomyConfig.LoadSynchronous() : nullptr;
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("UBench: no EconomyConfig set in Project Settings > Terrabound. Bench not sized."));
		return;
	}

	Slots.Init(nullptr, Config->BenchSlotCount);
}

bool UBench::HasFreeSlot() const
{
	for (const TObjectPtr<ABoardUnitBase>& Slot : Slots)
	{
		if (!Slot)
		{
			return true;
		}
	}
	return false;
}

ABoardUnitBase* UBench::GetChampionAt(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : nullptr;
}

int32 UBench::FindSlotIndex(ABoardUnitBase* Unit) const
{
	return Unit ? Slots.IndexOfByKey(Unit) : INDEX_NONE;
}

bool UBench::AddChampion(ABoardUnitBase* Unit)
{
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		if (!Slots[SlotIndex])
		{
			return SetSlot(SlotIndex, Unit);
		}
	}
	return false;
}

bool UBench::RemoveChampion(ABoardUnitBase* Unit)
{
	const int32 SlotIndex = FindSlotIndex(Unit);
	return SlotIndex != INDEX_NONE && SetSlot(SlotIndex, nullptr);
}

bool UBench::SetSlot(int32 SlotIndex, ABoardUnitBase* Unit)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return false;
	}

	Slots[SlotIndex] = Unit;
	OnBenchChanged.Broadcast();
	return true;
}
