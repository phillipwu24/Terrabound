// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/ObjectKey.h"
#include "../Grid/HexCoordinates.h"
#include "WaveDefinition.h"
#include "WaveManager.generated.h"

class AEnemyBase;
class UEnemyData;

/**
 * Owns the current wave (PLAN.md 1.4b, D19): its roster, progress, spawn windows and the one spawn
 * timer (D14). URoundManager hands it each wave through StartWave; it reports leaks through
 * URoundManager::NotifyEnemyExited and the wave's end through URoundManager::NotifyWaveResolved.
 *
 * Spawning: the first enemy of a round's first wave comes at once, and of every later wave after
 * the level's wave gap (D0); after each spawn the next delay is drawn from the level's
 * ASpawnHexConfig range. Once the delay has elapsed, a spawn needs a free spawn hex
 * (read from the grid's bIsSpawn flags) and an eligible roster entry. Missing either, the timer
 * stays elapsed and retries every tick, so occupancy only ever delays a spawn.
 *
 * Progress is spawned / roster size (D20, revised 2026-11-19). Windows: Early entries are first in
 * line while progress is under EarlyWindowEnd and plain Any after; Late entries wait for
 * LateWindowStart, or until every non-Late enemy has spawned, so no entry can be stranded. Otherwise
 * the next enemy is picked at random among the pending ones. No seed: execution drift is accepted
 * (CLAUDE.md). The wave itself resolves only once nothing is pending or alive, where an enemy stops
 * being alive when its actor is destroyed: by exiting today, by dying from Phase 2.
 */
UCLASS()
class TERRABOUND_API UWaveManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Starts Wave, replacing any wave in progress. Reads the spawn delay range and the wave gap from
	 * the level's ASpawnHexConfig. The first wave of a round spawns its first enemy at once; any
	 * later wave waits WaveGapSeconds first, on the same timer. A wave with nothing to spawn
	 * resolves at once.
	 */
	void StartWave(const FWaveDefinition& Wave, bool bFirstWaveOfRound);

	/** Advances the spawn timer and spawns if due. Tick calls this; public so headless tests can drive it. */
	void AdvanceSpawning(float DeltaSeconds);

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	UFUNCTION(BlueprintPure, Category = "Wave")
	bool IsWaveActive() const { return bWaveActive; }

	/** Share of the roster already spawned, 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "Wave")
	float GetProgress() const;

	UFUNCTION(BlueprintPure, Category = "Wave")
	int32 GetRosterSize() const { return RosterSize; }

	/** Enemies of this wave spawned so far, counting entries skipped as data mistakes. */
	UFUNCTION(BlueprintPure, Category = "Wave")
	int32 GetSpawnedCount() const { return RosterSize - Pending.Num(); }

	/** Enemies of this wave not yet spawned. */
	UFUNCTION(BlueprintPure, Category = "Wave")
	int32 GetPendingCount() const { return Pending.Num(); }

	/** Enemies of this wave not yet spawned, from entries with the given window. */
	UFUNCTION(BlueprintPure, Category = "Wave")
	int32 GetPendingCountInWindow(ESpawnWindow Window) const;

	/** Enemies of this wave on the board. */
	UFUNCTION(BlueprintPure, Category = "Wave")
	int32 GetAliveCount() const { return Alive.Num(); }

	/** Seconds until the next spawn is due; 0 means due now, waiting on a free hex or an eligible entry. */
	UFUNCTION(BlueprintPure, Category = "Wave")
	float GetTimeUntilNextSpawn() const { return TimeUntilNextSpawn; }

private:
	struct FPendingSpawn
	{
		TSoftObjectPtr<UEnemyData> Enemy;
		ESpawnWindow Window = ESpawnWindow::Any;
	};

	/** Reads the delay range and wave gap from the level's ASpawnHexConfig (or its defaults). */
	void ReadSpawnConfig();

	/** Index into Pending of the next enemy to spawn, or INDEX_NONE if none is eligible. */
	int32 PickPendingIndex() const;

	bool IsLateOpen() const;

	TArray<FHexCoord> GetFreeSpawnHexes() const;

	/** Spawns one enemy if a free spawn hex and an eligible entry exist. True if one entered the board. */
	bool TrySpawn();

	/** If nothing is pending or alive, ends the wave and tells RoundManager. Call last: it can start the next wave. */
	void CheckResolved();

	UFUNCTION()
	void HandleEnemyDestroyed(AActor* DestroyedActor);

	void HandleEnemyExited(AEnemyBase* Enemy);

	TArray<FPendingSpawn> Pending;
	// Keys, not weak pointers: they still match while the actor is being destroyed.
	TArray<TObjectKey<AActor>> Alive;

	int32 RosterSize = 0;
	float TimeUntilNextSpawn = 0.f;
	float MinSpawnInterval = 0.f;
	float MaxSpawnInterval = 0.f;
	float WaveGapSeconds = 0.f;
	bool bWaveActive = false;
};
