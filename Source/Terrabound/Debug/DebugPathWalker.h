// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Grid/HexCoordinates.h"
#include "DebugPathWalker.generated.h"

class UStaticMeshComponent;
class UHexGrid;

/**
 * Debug-only stand-in for an enemy (PLAN.md 7.1), to measure board crossing time and exercise the
 * pathfinder. Walks from a spawn hex to the back row one hex at a time: on each arrival it asks
 * FHexPathfinder for its next hex, moves centre to centre, and despawns on reaching the back row.
 * Keeps no path.
 *
 * Deliberately not an ABoardUnitBase and takes no occupancy - there is no EnemyBase yet, and no
 * combat, AI, or targeting. It never enters an occupied hex: if the pathfinder's best next hex is
 * occupied it holds and retries every tick.
 *
 * Speed is in hexes per second, not world units, for the same reason range is stored in hexes: a
 * later HexRadius change must not silently retune how the board feels.
 */
UCLASS()
class TERRABOUND_API ADebugPathWalker : public AActor
{
	GENERATED_BODY()

public:
	ADebugPathWalker();

	/** Places the walker on StartCoord and starts it walking at HexesPerSecond (must be > 0). */
	void StartWalking(const FHexCoord& StartCoord, float InHexesPerSecond);

	virtual void Tick(float DeltaSeconds) override;

private:
	/**
	 * Picks the next hex and starts the step toward it. Returns false, leaving the walker where it
	 * is, if there is no route or the best next hex is occupied.
	 */
	bool TryBeginNextStep(const UHexGrid& Grid);

	void LogHoldOnce(const FString& Reason);
	void LogCrossing() const;

	FVector HexToWorld(const UHexGrid& Grid, const FHexCoord& Coord) const;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	FHexCoord CurrentCoord;
	FHexCoord StepTarget;
	FVector StepStart = FVector::ZeroVector;
	FVector StepEnd = FVector::ZeroVector;

	// 0 at the start of a step, 1 on arrival. A step is always exactly one hex, so progress
	// advances by HexesPerSecond * time with no world-unit conversion.
	float StepProgress = 0.f;

	float HexesPerSecond = 0.f;
	float ElapsedSeconds = 0.f;
	float HeldSeconds = 0.f;
	int32 StepsTaken = 0;

	bool bWalking = false;
	bool bMidStep = false;
	bool bHoldLogged = false;
};
