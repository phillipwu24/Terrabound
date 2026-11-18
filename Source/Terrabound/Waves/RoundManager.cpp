// Copyright Epic Games, Inc. All Rights Reserved.

#include "RoundManager.h"
#include "WaveDefinition.h"
#include "WaveManager.h"
#include "../TerraboundSettings.h"
#include "../Input/BoardPlayerController.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

void URoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	RoundsTable = Settings ? Settings->RoundsTable.LoadSynchronous() : nullptr;
	if (!RoundsTable)
	{
		// Not an error until someone presses Start: StartRound refuses and says why.
		UE_LOG(LogTemp, Warning, TEXT("URoundManager: no RoundsTable set in Project Settings > Terrabound."));
	}
}

bool URoundManager::StartRound()
{
	if (Phase != ERoundPhase::Prep)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoundManager: StartRound refused - round %d is already in combat."), RoundIndex + 1);
		return false;
	}

	const UWorld* World = GetWorld();
	const ABoardPlayerController* Controller = World ? Cast<ABoardPlayerController>(World->GetFirstPlayerController()) : nullptr;
	if (Controller && Controller->IsCarryingUnit())
	{
		UE_LOG(LogTemp, Warning, TEXT("RoundManager: StartRound refused - a unit is being carried. Drop it first."));
		return false;
	}

	if (GetWaveCount() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("RoundManager: StartRound refused - no round data (set RoundsTable in Project Settings > Terrabound to a DT_Rounds with FRoundRow rows, each with at least one wave)."));
		return false;
	}

	LeaksThisRound = 0;
	UE_LOG(LogTemp, Display, TEXT("RoundManager: round %d starts, %d waves."), RoundIndex + 1, GetWaveCount());
	SetPhase(ERoundPhase::Combat);
	StartWave(0);
	return true;
}

void URoundManager::NotifyWaveResolved()
{
	if (Phase != ERoundPhase::Combat)
	{
		UE_LOG(LogTemp, Warning, TEXT("RoundManager: wave resolved outside combat - ignored."));
		return;
	}

	UE_LOG(LogTemp, Display, TEXT("RoundManager: round %d wave %d resolved."), RoundIndex + 1, WaveIndex + 1);
	if (WaveIndex + 1 < GetWaveCount())
	{
		// WaveManager holds the next wave's first spawn for the wave gap (D0); still Combat throughout.
		StartWave(WaveIndex + 1);
	}
	else
	{
		EndRound();
	}
}

void URoundManager::NotifyEnemyExited()
{
	++LeaksThisRound;
	++LeaksTotal;
	UE_LOG(LogTemp, Display, TEXT("RoundManager: leak - %d this round, %d total."), LeaksThisRound, LeaksTotal);
}

int32 URoundManager::GetWaveCount() const
{
	const FRoundRow* Row = GetCurrentRoundRow();
	return Row ? Row->Waves.Num() : 0;
}

const FRoundRow* URoundManager::GetCurrentRoundRow() const
{
	if (!RoundsTable || RoundsTable->GetRowStruct() != FRoundRow::StaticStruct())
	{
		return nullptr;
	}

	const TArray<FName> RowNames = RoundsTable->GetRowNames();
	if (RowNames.Num() == 0)
	{
		return nullptr;
	}

	// Rounds past the table repeat the last row (D13); the endless scaling formula is out of scope.
	const int32 RowIndex = FMath::Min(RoundIndex, RowNames.Num() - 1);
	return RoundsTable->FindRow<FRoundRow>(RowNames[RowIndex], TEXT("URoundManager"));
}

void URoundManager::StartWave(int32 Index)
{
	// WaveIndex is set before the hand-off: an empty wave resolves inside StartWave, which calls
	// back into NotifyWaveResolved.
	WaveIndex = Index;
	UE_LOG(LogTemp, Display, TEXT("RoundManager: round %d wave %d of %d starts."), RoundIndex + 1, WaveIndex + 1, GetWaveCount());

	const FRoundRow* Row = GetCurrentRoundRow();
	UWaveManager* Waves = GetWorld() ? GetWorld()->GetSubsystem<UWaveManager>() : nullptr;
	if (!Row || !Row->Waves.IsValidIndex(Index) || !Waves)
	{
		UE_LOG(LogTemp, Error, TEXT("RoundManager: couldn't hand wave %d to WaveManager."), Index + 1);
		return;
	}
	Waves->StartWave(Row->Waves[Index], Index == 0);
}

void URoundManager::EndRound()
{
	UE_LOG(LogTemp, Display, TEXT("RoundManager: round %d ends - %d leaks this round, %d total."), RoundIndex + 1, LeaksThisRound, LeaksTotal);
	++RoundIndex;
	WaveIndex = 0;
	// The prep heal (D17) is applied here, on entering Prep, in PLAN.md 2.7.
	SetPhase(ERoundPhase::Prep);
}

void URoundManager::SetPhase(ERoundPhase NewPhase)
{
	if (Phase == NewPhase)
	{
		return;
	}

	Phase = NewPhase;
	OnPhaseChanged.Broadcast(Phase);
}
