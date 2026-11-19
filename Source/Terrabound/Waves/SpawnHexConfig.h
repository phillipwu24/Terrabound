// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Grid/HexCoordinates.h"
#include "SpawnHexConfig.generated.h"

/**
 * Per-level spawn hex configuration (PLAN.md 1.3, D12). Placed once in a level; the designer lists
 * that level's spawn hexes on the placed instance. At BeginPlay it asks UHexGrid to flag each one,
 * and nothing reads the array afterwards - it is an input read once, not a mirror of tile state, so
 * the grid's bIsSpawn flags are the only live copy. It also holds the level's spawn delay range
 * (PLAN.md 1.4b, D14) and the gap between waves (D0), which UWaveManager reads at the start of
 * each wave and never writes back.
 */
UCLASS()
class TERRABOUND_API ASpawnHexConfig : public AActor
{
	GENERATED_BODY()

public:
	ASpawnHexConfig();

	/**
	 * This level's spawn hexes, as axial (q, r) - the DebugCoordOverlay command shows each hex's
	 * value. Meant for the enemy side, usually the far edge (row 0). A player-zone entry is an
	 * error: logged, and the hex is kept a normal player hex. BlueprintReadOnly, not ReadWrite: it
	 * is read once at BeginPlay, so a later Blueprint write would silently do nothing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	TArray<FHexCoord> SpawnHexes;

	/**
	 * PLACEHOLDER (D14), tuned in PLAN.md 1.7 against the 14 s crossing. After each spawn the next
	 * delay is drawn uniformly from [MinSpawnInterval, MaxSpawnInterval] seconds. Read at the start
	 * of every wave. Min above Max is logged and swapped.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0", Units = "s"))
	float MinSpawnInterval = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0", Units = "s"))
	float MaxSpawnInterval = 4.f;

	/**
	 * PLACEHOLDER (D0). Seconds between a wave resolving and the next wave's first spawn. The first
	 * wave of a round still starts at once. The gap is still Combat: no input, healing or
	 * repositioning. Read at the start of every wave.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn", meta = (ClampMin = "0", Units = "s"))
	float WaveGapSeconds = 5.f;

	/**
	 * Flags every SpawnHexes entry as a spawn tile on the grid. An off-board entry is logged and
	 * skipped; a player-zone entry is logged and has its spawn flag explicitly cleared. Called from
	 * BeginPlay; public so headless tests (whose worlds never BeginPlay actors) can call it.
	 */
	void ApplySpawnHexes();

protected:
	virtual void BeginPlay() override;
};
