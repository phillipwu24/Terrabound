// Copyright Epic Games, Inc. All Rights Reserved.

#include "EconomyState.h"
#include "../TerraboundSettings.h"
#include "../Data/EconomyConfig.h"

void UEconomyState::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	const UEconomyConfig* Config = Settings ? Settings->EconomyConfig.LoadSynchronous() : nullptr;
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("UEconomyState: no EconomyConfig set in Project Settings > Terrabound. Gold not initialized."));
		return;
	}

	Gold = Config->StartingGold;
}

bool UEconomyState::Spend(int32 Amount)
{
	if (!CanAfford(Amount))
	{
		return false;
	}

	Gold -= Amount;
	OnGoldChanged.Broadcast(Gold);
	return true;
}

void UEconomyState::Add(int32 Amount)
{
	if (Amount == 0)
	{
		return;
	}

	Gold += Amount;
	OnGoldChanged.Broadcast(Gold);
}
