// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoardUnitBase.h"
#include "EnemyBase.generated.h"

class UEnemyData;
class UHexGrid;
class AEnemyBase;

/** Broadcast once when an enemy reaches the back row, just before it despawns. WaveManager counts leaks off it. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEnemyExited, AEnemyBase*);

/**
 * Enemy board unit (PLAN.md 1.2). Derives from BoardUnitBase like AChampionBase and is spawned
 * generically, then initialized from a UEnemyData - never a Blueprint per enemy.
 *
 * Walks one hex at a time and stores no path (CLAUDE.md): on each arrival it asks FHexPathfinder
 * for the neighbour with the lowest path distance to the exit, then claims that hex with
 * UHexGrid::TryOccupy the moment the step begins, releasing the hex it left at the same moment. If
 * the claim fails it holds and re-decides next tick. The pathfinder prices an occupied hex; this
 * claim is where occupancy is actually enforced, so the enemy never enters one.
 *
 * GetCurrentCoord() is the hex this enemy has claimed, which mid-step is the hex it is walking
 * *into* - the grid and the coord agree at every moment, even while the actor is visually between
 * two hexes.
 *
 * Walking only: no targeting, no attacks, no HP until Phase 2/3.
 */
UCLASS()
class TERRABOUND_API AEnemyBase : public ABoardUnitBase
{
	GENERATED_BODY()

public:
	AEnemyBase();

	/**
	 * Applies Data's skeletal mesh and anim blueprint to the inherited Mesh and stores Data so later
	 * systems read stats from one place. Does not touch HexGrid - call EnterBoard to place and start it.
	 */
	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void InitializeFromEnemyData(UEnemyData* Data);

	UFUNCTION(BlueprintPure, Category = "Enemy")
	UEnemyData* GetEnemyData() const { return EnemyData; }

	/**
	 * Claims StartCoord, snaps to it and starts walking to the exit. Returns false, and does not
	 * walk, if the hex is invalid or already held (the caller decides whether to retry later), or if
	 * the enemy has no valid data to walk with.
	 */
	bool EnterBoard(const FHexCoord& StartCoord);

	/**
	 * Advances walking by DeltaSeconds. Tick calls this; it is public so a headless test can drive an
	 * enemy in a world that never ticks. Time left over after an arrival carries into the next step,
	 * so crossing time doesn't depend on frame rate.
	 */
	void AdvanceMovement(float DeltaSeconds);

	FOnEnemyExited OnEnemyExited;

	virtual void Tick(float DeltaSeconds) override;

	/** Releases the claimed hex. Destroyed(), not EndPlay: it fires on every explicit destroy but not on world teardown. */
	virtual void Destroyed() override;

private:
	/** Picks the next hex and claims it. False means hold this tick (no route, or the hex is taken). */
	bool TryBeginNextStep(UHexGrid& Grid);

	/** Logs a hold once per hold, not once per tick. */
	void LogHoldOnce(const FString& Reason);

	void ExitBoard();

	UPROPERTY()
	TObjectPtr<UEnemyData> EnemyData;

	bool bWalking = false;
	bool bMidStep = false;
	bool bHoldLogged = false;

	FVector StepStart = FVector::ZeroVector;
	FVector StepEnd = FVector::ZeroVector;
	float StepProgress = 0.f;

	// Crossing-time bookkeeping, logged on exit.
	float ElapsedSeconds = 0.f;
	float HeldSeconds = 0.f;
	int32 StepsTaken = 0;
};
