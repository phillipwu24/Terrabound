// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraitCounter.h"
#include "HexGrid.h"
#include "HexTile.h"
#include "../Units/ChampionBase.h"
#include "../Data/ChampionData.h"

int32 UTraitCounter::GetTraitCount(FGameplayTag Tag) const
{
	const UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid)
	{
		return 0;
	}

	// Distinct champions, not bodies: a champion's identity is its data asset, so a second copy of
	// the same champion adds nothing, as in TFT.
	TSet<const UChampionData*> DistinctChampions;
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		const FHexTile* Tile = Grid->GetTile(Coord);
		const AChampionBase* Champion = Tile ? Cast<AChampionBase>(Tile->Occupant.Get()) : nullptr;
		const UChampionData* Data = Champion ? Champion->GetChampionData() : nullptr;
		if (Data && Data->Traits.HasTag(Tag))
		{
			DistinctChampions.Add(Data);
		}
	}
	return DistinctChampions.Num();
}
