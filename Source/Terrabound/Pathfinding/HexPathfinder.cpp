// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexPathfinder.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexTile.h"

bool FHexPathfinder::TryGetEnterCost(const UHexGrid& Grid, const FHexCoord& Coord, float& OutCost)
{
	const FHexTile* Tile = Grid.GetTile(Coord);
	if (!Tile || !Tile->bIsWalkable)
	{
		return false;
	}

	// Flat: any occupant costs the same, whoever it is. Only whether the tile is occupied is read.
	OutCost = Tile->PathCost + (Tile->Occupant.IsValid() ? Grid.GetOccupiedTileCost() : 0.f);
	return true;
}

TMap<FHexCoord, float> FHexPathfinder::ComputeDistanceField(const UHexGrid& Grid, const TArray<FHexCoord>& Sources)
{
	TMap<FHexCoord, float> Distance;
	TArray<FHexCoord> Open;

	for (const FHexCoord& Source : Sources)
	{
		if (Grid.IsValidCoord(Source))
		{
			Distance.Add(Source, 0.f);
			Open.Add(Source);
		}
	}

	while (Open.Num() > 0)
	{
		// The board is a few dozen tiles, so a linear scan for the nearest open tile is plenty and
		// reads more plainly than a heap.
		int32 BestIndex = 0;
		for (int32 Index = 1; Index < Open.Num(); ++Index)
		{
			if (Distance.FindChecked(Open[Index]) < Distance.FindChecked(Open[BestIndex]))
			{
				BestIndex = Index;
			}
		}
		const FHexCoord Current = Open[BestIndex];
		Open.RemoveAtSwap(BestIndex);

		// A tile that can't be entered can't be routed through, so nothing propagates past it.
		float EnterCost = 0.f;
		if (!TryGetEnterCost(Grid, Current, EnterCost))
		{
			continue;
		}

		// A neighbour reaches Current by stepping into it, which costs Current's enter cost.
		const float Candidate = Distance.FindChecked(Current) + EnterCost;
		for (const FHexCoord& Neighbour : UHexCoordinateLibrary::GetNeighbours(Current))
		{
			if (!Grid.IsValidCoord(Neighbour))
			{
				continue;
			}

			float* Existing = Distance.Find(Neighbour);
			if (!Existing)
			{
				Distance.Add(Neighbour, Candidate);
				Open.Add(Neighbour);
			}
			else if (Candidate < *Existing)
			{
				*Existing = Candidate;
			}
		}
	}

	return Distance;
}

TOptional<FHexCoord> FHexPathfinder::ChooseNextStep(const UHexGrid& Grid, const TMap<FHexCoord, float>& Field, const FHexCoord& From)
{
	float BestScore = TNumericLimits<float>::Max();
	TArray<FHexCoord> Best;

	for (const FHexCoord& Neighbour : UHexCoordinateLibrary::GetNeighbours(From))
	{
		const float* Distance = Field.Find(Neighbour);
		float EnterCost = 0.f;
		if (!Distance || !TryGetEnterCost(Grid, Neighbour, EnterCost))
		{
			continue;
		}

		const float Score = EnterCost + *Distance;
		if (FMath::IsNearlyEqual(Score, BestScore))
		{
			Best.Add(Neighbour);
		}
		else if (Score < BestScore)
		{
			BestScore = Score;
			Best.Reset();
			Best.Add(Neighbour);
		}
	}

	if (Best.IsEmpty())
	{
		return TOptional<FHexCoord>();
	}
	return Best[FMath::RandRange(0, Best.Num() - 1)];
}
