// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WaveDefinition.generated.h"

class UEnemyData;

/**
 * When in a wave a roster entry may spawn, gated on wave progress (PLAN.md D20). The thresholds and
 * the gating are WaveManager's (1.4b); this is only the authored label.
 */
UENUM(BlueprintType)
enum class ESpawnWindow : uint8
{
	Early,
	Any,
	Late
};

/** One line of a wave's roster: Count copies of one enemy, all sharing a spawn window (D13). */
USTRUCT(BlueprintType)
struct FWaveRosterEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Wave")
	TSoftObjectPtr<UEnemyData> Enemy;

	UPROPERTY(EditAnywhere, Category = "Wave", meta = (ClampMin = "1"))
	int32 Count = 1;

	UPROPERTY(EditAnywhere, Category = "Wave")
	ESpawnWindow SpawnWindow = ESpawnWindow::Any;
};

/** One wave: a fixed roster. Spawn timing and spawn hexes are not authored (CLAUDE.md). */
USTRUCT(BlueprintType)
struct FWaveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Wave")
	TArray<FWaveRosterEntry> Roster;
};

/**
 * One row of DT_Rounds: a round's waves, in order (D13). The wave count is per row, not a constant.
 * Rows are read in table order; their names are ignored. Rounds past the last row repeat it.
 */
USTRUCT(BlueprintType)
struct FRoundRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Round")
	TArray<FWaveDefinition> Waves;
};
