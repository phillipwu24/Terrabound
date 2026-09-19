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

	int32 Count = 0;
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		const FHexTile* Tile = Grid->GetTile(Coord);
		const AChampionBase* Champion = Tile ? Cast<AChampionBase>(Tile->Occupant.Get()) : nullptr;
		const UChampionData* Data = Champion ? Champion->GetChampionData() : nullptr;
		if (Data && Data->Traits.HasTag(Tag))
		{
			++Count;
		}
	}
	return Count;
}
