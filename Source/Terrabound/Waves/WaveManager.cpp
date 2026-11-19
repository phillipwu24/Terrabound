// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveManager.h"
#include "RoundManager.h"
#include "SpawnHexConfig.h"
#include "../TerraboundSettings.h"
#include "../Data/EnemyData.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexTile.h"
#include "../Units/EnemyBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void UWaveManager::StartWave(const FWaveDefinition& Wave, bool bFirstWaveOfRound)
{
	Pending.Reset();
	Alive.Reset();
	RosterSize = 0;

	for (const FWaveRosterEntry& Entry : Wave.Roster)
	{
		if (Entry.Count <= 0)
		{
			continue;
		}
		RosterSize += Entry.Count;
		if (Entry.Enemy.IsNull())
		{
			// Never pending, so it counts as spawned: a data mistake can't hang the wave or a window.
			UE_LOG(LogTemp, Error, TEXT("WaveManager: a roster entry has no enemy set - its %d enemies are skipped."), Entry.Count);
			continue;
		}
		for (int32 Copy = 0; Copy < Entry.Count; ++Copy)
		{
			Pending.Add({ Entry.Enemy, Entry.SpawnWindow });
		}
	}

	ReadSpawnConfig();
	// The round's first wave starts at once (D14); later waves wait out the gap on the same timer (D0).
	TimeUntilNextSpawn = bFirstWaveOfRound ? 0.f : WaveGapSeconds;
	bWaveActive = true;
	UE_LOG(LogTemp, Display, TEXT("WaveManager: wave starts - %d enemies, first spawn in %.2f s, then every %.2f-%.2f s."),
		RosterSize, TimeUntilNextSpawn, MinSpawnInterval, MaxSpawnInterval);

	CheckResolved();
}

void UWaveManager::AdvanceSpawning(float DeltaSeconds)
{
	if (!bWaveActive)
	{
		return;
	}

	TimeUntilNextSpawn = FMath::Max(0.f, TimeUntilNextSpawn - DeltaSeconds);
	if (TimeUntilNextSpawn <= 0.f && Pending.Num() > 0 && TrySpawn())
	{
		TimeUntilNextSpawn = FMath::FRandRange(MinSpawnInterval, MaxSpawnInterval);
	}

	CheckResolved();
}

void UWaveManager::Tick(float DeltaTime)
{
	AdvanceSpawning(DeltaTime);
}

TStatId UWaveManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWaveManager, STATGROUP_Tickables);
}

float UWaveManager::GetProgress() const
{
	return RosterSize > 0 ? static_cast<float>(GetSpawnedCount()) / RosterSize : 1.f;
}

int32 UWaveManager::GetPendingCountInWindow(ESpawnWindow Window) const
{
	int32 Count = 0;
	for (const FPendingSpawn& Spawn : Pending)
	{
		Count += Spawn.Window == Window ? 1 : 0;
	}
	return Count;
}

void UWaveManager::ReadSpawnConfig()
{
	const ASpawnHexConfig* Config = nullptr;
	if (UWorld* World = GetWorld())
	{
		TActorIterator<ASpawnHexConfig> It(World);
		Config = It ? *It : nullptr;
	}
	if (!Config)
	{
		UE_LOG(LogTemp, Warning, TEXT("WaveManager: no SpawnHexConfig in the level - using its default spawn delay range and wave gap."));
		Config = GetDefault<ASpawnHexConfig>();
	}

	WaveGapSeconds = Config->WaveGapSeconds;
	MinSpawnInterval = Config->MinSpawnInterval;
	MaxSpawnInterval = Config->MaxSpawnInterval;
	if (MinSpawnInterval > MaxSpawnInterval)
	{
		UE_LOG(LogTemp, Warning, TEXT("WaveManager: MinSpawnInterval %.2f is above MaxSpawnInterval %.2f - swapped."), MinSpawnInterval, MaxSpawnInterval);
		Swap(MinSpawnInterval, MaxSpawnInterval);
	}
}

int32 UWaveManager::PickPendingIndex() const
{
	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	const bool bEarlyWindowOpen = GetProgress() < Settings->EarlyWindowEnd;
	const bool bLateOpen = IsLateOpen();

	// Early entries are first in line while their window is open. Once it closes they are plain
	// Any, so an Early entry that missed its window still spawns (D20).
	TArray<int32> Candidates;
	if (bEarlyWindowOpen)
	{
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (Pending[Index].Window == ESpawnWindow::Early)
			{
				Candidates.Add(Index);
			}
		}
	}
	if (Candidates.Num() == 0)
	{
		for (int32 Index = 0; Index < Pending.Num(); ++Index)
		{
			if (Pending[Index].Window != ESpawnWindow::Late || bLateOpen)
			{
				Candidates.Add(Index);
			}
		}
	}

	// One candidate per pending enemy, so an entry with Count 3 is three times as likely.
	return Candidates.Num() > 0 ? Candidates[FMath::RandRange(0, Candidates.Num() - 1)] : INDEX_NONE;
}

bool UWaveManager::IsLateOpen() const
{
	if (GetProgress() >= GetDefault<UTerraboundSettings>()->LateWindowStart)
	{
		return true;
	}

	// Or once every non-Late enemy has spawned: with Late entries making up more than the share
	// above the threshold, progress could otherwise never reach it and the wave would hang.
	for (const FPendingSpawn& Spawn : Pending)
	{
		if (Spawn.Window != ESpawnWindow::Late)
		{
			return false;
		}
	}
	return true;
}

TArray<FHexCoord> UWaveManager::GetFreeSpawnHexes() const
{
	TArray<FHexCoord> Free;
	const UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid)
	{
		return Free;
	}
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		const FHexTile* Tile = Grid->GetTile(Coord);
		if (Tile && Tile->bIsSpawn && !Tile->Occupant.IsValid())
		{
			Free.Add(Coord);
		}
	}
	return Free;
}

bool UWaveManager::TrySpawn()
{
	UWorld* World = GetWorld();
	const TArray<FHexCoord> FreeHexes = GetFreeSpawnHexes();
	if (!World || FreeHexes.Num() == 0)
	{
		return false;
	}

	const int32 PendingIndex = PickPendingIndex();
	if (PendingIndex == INDEX_NONE)
	{
		return false;
	}

	const FPendingSpawn Spawn = Pending[PendingIndex];
	UEnemyData* Data = Spawn.Enemy.LoadSynchronous();
	if (!Data)
	{
		UE_LOG(LogTemp, Error, TEXT("WaveManager: couldn't load enemy %s - skipped."), *Spawn.Enemy.ToString());
		Pending.RemoveAtSwap(PendingIndex);
		return false;
	}

	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>();
	if (!Enemy)
	{
		UE_LOG(LogTemp, Error, TEXT("WaveManager: failed to spawn AEnemyBase."));
		return false;
	}
	Enemy->InitializeFromEnemyData(Data);

	const FHexCoord Hex = FreeHexes[FMath::RandRange(0, FreeHexes.Num() - 1)];
	if (!Enemy->EnterBoard(Hex))
	{
		// Destroyed before its handlers are bound, so it never counts as resolved; retried next tick.
		Enemy->Destroy();
		return false;
	}

	Enemy->OnEnemyExited.AddUObject(this, &UWaveManager::HandleEnemyExited);
	Enemy->OnDestroyed.AddDynamic(this, &UWaveManager::HandleEnemyDestroyed);
	Alive.Add(TObjectKey<AActor>(Enemy));
	Pending.RemoveAtSwap(PendingIndex);

	UE_LOG(LogTemp, Display, TEXT("WaveManager: spawned %s at %s - %d pending, %d alive."), *Data->GetName(), *Hex.ToString(), Pending.Num(), Alive.Num());
	return true;
}

void UWaveManager::CheckResolved()
{
	if (!bWaveActive || Pending.Num() > 0 || Alive.Num() > 0)
	{
		return;
	}

	bWaveActive = false;
	UE_LOG(LogTemp, Display, TEXT("WaveManager: wave resolved - %d enemies."), RosterSize);
	if (URoundManager* Rounds = GetWorld() ? GetWorld()->GetSubsystem<URoundManager>() : nullptr)
	{
		Rounds->NotifyWaveResolved();
	}
}

void UWaveManager::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	if (Alive.Remove(TObjectKey<AActor>(DestroyedActor)) > 0)
	{
		CheckResolved();
	}
}

void UWaveManager::HandleEnemyExited(AEnemyBase* Enemy)
{
	// The enemy destroys itself right after this, which is what counts it as resolved.
	if (URoundManager* Rounds = GetWorld() ? GetWorld()->GetSubsystem<URoundManager>() : nullptr)
	{
		Rounds->NotifyEnemyExited();
	}
}
