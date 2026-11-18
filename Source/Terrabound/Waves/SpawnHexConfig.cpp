// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpawnHexConfig.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexTile.h"

ASpawnHexConfig::ASpawnHexConfig()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ASpawnHexConfig::BeginPlay()
{
	Super::BeginPlay();
	ApplySpawnHexes();
}

void ASpawnHexConfig::ApplySpawnHexes()
{
	UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnHexConfig: no HexGrid subsystem for this world."));
		return;
	}

	int32 FlaggedCount = 0;
	for (const FHexCoord& Coord : SpawnHexes)
	{
		const FHexTile* Tile = Grid->GetTile(Coord);
		if (!Tile)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnHexConfig: %s is not on the board - ignored."), *Coord.ToString());
			continue;
		}

		// A spawn hex must never sit in the player zone (D12). Clear the flag rather than just
		// skipping, so a flag set earlier (e.g. by the SetSpawnFlag debug command) can't survive.
		if (Tile->bIsPlaceable)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnHexConfig: %s is in the player zone - ignored, kept as a normal player hex."), *Coord.ToString());
			Grid->SetSpawnFlag(Coord, false);
			continue;
		}

		Grid->SetSpawnFlag(Coord, true);
		++FlaggedCount;
	}

	UE_LOG(LogTemp, Display, TEXT("SpawnHexConfig: flagged %d of %d spawn hexes."), FlaggedCount, SpawnHexes.Num());
}
