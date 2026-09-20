// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "TraitCounter.generated.h"

/**
 * Answers "how many distinct board champions carry Tag" (PLAN.md 6.7) - a UWorldSubsystem so the future
 * trait panel widget can reach it the same way it reaches HexGrid/ShopSystem/EconomyState.
 *
 * Recomputes from HexGrid on every call rather than maintaining a live tally - the board is only
 * 56 tiles, and this project already accepts recompute-over-cache at that scale (PLAN.md 7.1
 * makes the same call for pathfinding). No cached state, no delegate of its own: callers refresh
 * off HexGrid::OnOccupancyChanged directly.
 *
 * Holds no knowledge of trait display info (name/icon) - that's FTraitDefinitionRow's job
 * (Data/TraitDefinition.h), read only by the widget. This class only counts tags.
 */
UCLASS()
class TERRABOUND_API UTraitCounter : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Distinct champions (not bench, not carried) whose ChampionData::Traits contains Tag. Two
	 * copies of the same champion - the same ChampionData asset - count once, as in TFT.
	 */
	UFUNCTION(BlueprintPure, Category = "Traits")
	int32 GetTraitCount(FGameplayTag Tag) const;
};
