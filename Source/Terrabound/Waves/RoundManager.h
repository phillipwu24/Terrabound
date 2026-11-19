// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RoundManager.generated.h"

class UDataTable;
struct FRoundRow;

UENUM(BlueprintType)
enum class ERoundPhase : uint8
{
	Prep,
	Combat
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnRoundPhaseChanged, ERoundPhase /*NewPhase*/);

/**
 * Owns the run's flow (PLAN.md 1.4a, D19): the Prep/Combat phase, Start, the round index, and the
 * leak counters. The current wave's roster, progress and spawn timer belong to WaveManager (1.4b),
 * which reports back through NotifyWaveResolved and NotifyEnemyExited.
 *
 * A round is Prep, then Combat across every wave of the round's DT_Rounds row in turn, then Prep
 * again (D0). Nothing here is a timer: Start is the player's, each wave ends when WaveManager says it
 * has resolved, and the gap before the next wave is WaveManager's spawn timer.
 */
UCLASS()
class TERRABOUND_API URoundManager : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * Starts the current round: Prep -> Combat, wave 1. Refused (logged, nothing changed) outside
	 * Prep, while a unit is carried (a carried unit is on neither bench nor board), or when there is
	 * no round data. Returns whether the round started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Round")
	bool StartRound();

	/** Whether StartRound would succeed right now: the same checks, without the log. Drives the Start button. */
	UFUNCTION(BlueprintPure, Category = "Round")
	bool CanStartRound() const { return GetStartRefusal() == EStartRefusal::None; }

	/** The round HUD label: "Round N · Wave X/Y" in Combat, "Round N · Prep" in Prep (N being the round about to start). */
	UFUNCTION(BlueprintPure, Category = "Round")
	FText GetRoundStatusText() const;

	/**
	 * WaveManager's hook: the current wave is resolved (every enemy dead or exited). Hands over the
	 * next wave, or ends the round after the last. Ignored outside Combat.
	 */
	void NotifyWaveResolved();

	/** WaveManager's hook: an enemy reached the exit. Counts a leak; nothing reads the count yet. */
	void NotifyEnemyExited();

	/** Replaces the rounds table read from settings. For tests; DT_Rounds is the normal source. */
	void SetRoundsTable(UDataTable* Table) { RoundsTable = Table; }

	UFUNCTION(BlueprintPure, Category = "Round")
	ERoundPhase GetPhase() const { return Phase; }

	/** True in Combat: no champion is placed on, moved on, sold from or merged on the board (D1). */
	UFUNCTION(BlueprintPure, Category = "Round")
	bool IsBoardLocked() const { return Phase == ERoundPhase::Combat; }

	/** 0-based; logs show it as "Round N" with N = index + 1. Advances when a round ends. */
	UFUNCTION(BlueprintPure, Category = "Round")
	int32 GetRoundIndex() const { return RoundIndex; }

	/** 0-based index of the wave in progress. Meaningful only in Combat. */
	UFUNCTION(BlueprintPure, Category = "Round")
	int32 GetWaveIndex() const { return WaveIndex; }

	/** Waves in the current round's row (0 when there is no round data). */
	UFUNCTION(BlueprintPure, Category = "Round")
	int32 GetWaveCount() const;

	/** Leaks since the current or most recent round started. */
	UFUNCTION(BlueprintPure, Category = "Round")
	int32 GetLeaksThisRound() const { return LeaksThisRound; }

	UFUNCTION(BlueprintPure, Category = "Round")
	int32 GetLeaksTotal() const { return LeaksTotal; }

	/** Fires on every phase change. Native: no widget listens yet. */
	FOnRoundPhaseChanged OnPhaseChanged;

private:
	enum class EStartRefusal : uint8
	{
		None,
		NotInPrep,
		CarryingUnit,
		NoRoundData
	};

	/** Why StartRound would refuse now, shared by StartRound (which logs it) and CanStartRound. */
	EStartRefusal GetStartRefusal() const;

	/** The current round's row: RoundIndex, or the last row once past the end. Null without data. */
	const FRoundRow* GetCurrentRoundRow() const;

	void StartWave(int32 Index);
	void EndRound();
	void SetPhase(ERoundPhase NewPhase);

	UPROPERTY()
	TObjectPtr<UDataTable> RoundsTable;

	ERoundPhase Phase = ERoundPhase::Prep;
	int32 RoundIndex = 0;
	int32 WaveIndex = 0;
	int32 LeaksThisRound = 0;
	int32 LeaksTotal = 0;
};
