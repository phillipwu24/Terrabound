// Copyright Epic Games, Inc. All Rights Reserved.

#include "ShopSystem.h"
#include "ChampionPool.h"
#include "Bench.h"
#include "BenchVisualizer.h"
#include "ChampionMerger.h"
#include "EconomyState.h"
#include "../Input/BoardPlayerController.h"
#include "../TerraboundSettings.h"
#include "../Data/EconomyConfig.h"
#include "../Data/ChampionData.h"
#include "../Units/ChampionBase.h"
#include "EngineUtils.h"

void UShopSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	Config = Settings ? Settings->EconomyConfig.LoadSynchronous() : nullptr;
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("UShopSystem: no EconomyConfig set in Project Settings > Terrabound. Shop not initialized."));
		return;
	}

	UDataTable* PoolTable = Settings->ChampionPoolTable.LoadSynchronous();
	UDataTable* OddsTable = Settings->ChampionTierOddsTable.LoadSynchronous();
	if (!PoolTable || !OddsTable)
	{
		UE_LOG(LogTemp, Error, TEXT("UShopSystem: ChampionPoolTable and/or ChampionTierOddsTable not set in Project Settings > Terrabound. Shop not initialized."));
		return;
	}

	Pool = NewObject<UChampionPool>(this);
	Pool->Initialize(PoolTable, OddsTable);

	// Free initial fill - not a Reroll, costs nothing, so there is something to buy from the
	// first frame without spending StartingGold on a reroll first.
	Slots.Init(nullptr, Config->ShopSlotCount);
	FillAllSlots();
}

UChampionData* UShopSystem::GetChampionAt(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : nullptr;
}

int32 UShopSystem::GetCostAt(int32 SlotIndex) const
{
	const UChampionData* Data = Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : nullptr;
	return (Config && Data) ? Config->GetCostForTier(Data->Tier) : 0;
}

int32 UShopSystem::GetRerollCost() const
{
	return Config ? Config->RerollCost : 0;
}

bool UShopSystem::CanBuy(int32 SlotIndex) const
{
	const UChampionData* Data = Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : nullptr;
	if (!Config || !Data)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const UEconomyState* Economy = World ? World->GetSubsystem<UEconomyState>() : nullptr;
	const UBench* Bench = World ? World->GetSubsystem<UBench>() : nullptr;
	if (!Economy || !Bench || !Economy->CanAfford(Config->GetCostForTier(Data->Tier)))
	{
		return false;
	}

	const ABoardPlayerController* Controller = Cast<ABoardPlayerController>(World->GetFirstPlayerController());
	if (Controller && Controller->IsCarryingUnit())
	{
		return false;
	}

	// A full bench still allows a copy that completes a merge: it is consumed, never benched.
	const UChampionMerger* Merger = World->GetSubsystem<UChampionMerger>();
	return Bench->HasFreeSlot() || (Merger && Merger->WouldCompleteMerge(Data));
}

void UShopSystem::NotifyAvailabilityChanged()
{
	OnShopChanged.Broadcast();
}

void UShopSystem::FillAllSlots()
{
	for (TObjectPtr<UChampionData>& Slot : Slots)
	{
		Slot = Pool ? Pool->DrawRandomChampion() : nullptr;
	}
}

bool UShopSystem::Reroll()
{
	UEconomyState* Economy = GetWorld() ? GetWorld()->GetSubsystem<UEconomyState>() : nullptr;
	if (!Economy || !Config || !Pool || !Economy->CanAfford(Config->RerollCost))
	{
		return false;
	}

	Economy->Spend(Config->RerollCost);

	for (const TObjectPtr<UChampionData>& Slot : Slots)
	{
		if (Slot)
		{
			Pool->ReturnChampion(Slot);
		}
	}
	FillAllSlots();

	OnShopChanged.Broadcast();
	return true;
}

bool UShopSystem::Buy(int32 SlotIndex)
{
	if (!CanBuy(SlotIndex))
	{
		return false;
	}

	UWorld* World = GetWorld();
	UChampionData* Data = Slots[SlotIndex];
	UEconomyState* Economy = World->GetSubsystem<UEconomyState>();
	UBench* Bench = World->GetSubsystem<UBench>();
	const int32 Cost = Config->GetCostForTier(Data->Tier);

	// Spawn before spending: if the spawn fails, nothing has been charged or changed.
	AChampionBase* Champion = World->SpawnActor<AChampionBase>();
	if (!Champion)
	{
		return false;
	}

	Champion->InitializeFromChampionData(Data);

	// A copy that completes a merge is consumed by it and never benched. Otherwise CanBuy has
	// guaranteed a free bench slot: a full bench is only allowed through for a completing copy.
	UChampionMerger* Merger = World->GetSubsystem<UChampionMerger>();
	if (!(Merger && Merger->TryMerge(Champion)))
	{
		Bench->AddChampion(Champion);

		if (TActorIterator<ABenchVisualizer> It(World); It)
		{
			const int32 BenchSlotIndex = Bench->FindSlotIndex(Champion);
			Champion->SetActorLocation(It->GetSlotTransform(BenchSlotIndex).GetLocation());
		}
	}
	Economy->Spend(Cost);

	Slots[SlotIndex] = nullptr;
	OnShopChanged.Broadcast();
	return true;
}

bool UShopSystem::Sell(ABoardUnitBase* Unit)
{
	const AChampionBase* Champion = Cast<AChampionBase>(Unit);
	UChampionData* Data = Champion ? Champion->GetChampionData() : nullptr;
	if (!Data || !Config || !Pool)
	{
		return false;
	}

	UEconomyState* Economy = GetWorld() ? GetWorld()->GetSubsystem<UEconomyState>() : nullptr;
	if (!Economy)
	{
		return false;
	}

	// A star-up consumed copies from the pool for good; selling the merged unit returns all of them.
	const int32 Copies = Config->GetCopiesInStar(Champion->GetStarLevel());
	const int32 Refund = FMath::RoundToInt(Config->GetCostForTier(Data->Tier) * Copies * Config->SellRefundPercentage);
	Economy->Add(Refund);
	for (int32 Copy = 0; Copy < Copies; ++Copy)
	{
		Pool->ReturnChampion(Data);
	}
	Unit->Destroy();
	return true;
}
