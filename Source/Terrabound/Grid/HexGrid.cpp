// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexGrid.h"
#include "../TerraboundSettings.h"
#include "../Data/BoardConfig.h"
#include "../Units/BoardUnitBase.h"

void UHexGrid::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	GenerateGrid();
}

void UHexGrid::GenerateGrid()
{
	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	const UBoardConfig* Config = Settings ? Settings->BoardConfig.LoadSynchronous() : nullptr;
	if (!Config)
	{
		UE_LOG(LogTemp, Error, TEXT("UHexGrid: no BoardConfig set in Project Settings > Terrabound. Grid not generated."));
		return;
	}

	BoardWidth = Config->BoardWidth;
	BoardDepth = Config->BoardDepth;
	PlaceableRowCount = Config->PlaceableRowCount;
	HexRadius = Config->HexRadius;
	OccupiedTileCost = Config->OccupiedTileCost;

	Tiles.Empty(BoardWidth * BoardDepth);
	const int32 EnemyRowCount = BoardDepth - PlaceableRowCount;

	for (int32 Row = 0; Row < BoardDepth; ++Row)
	{
		for (int32 Col = 0; Col < BoardWidth; ++Col)
		{
			FHexTile Tile;
			Tile.Coord = UHexCoordinateLibrary::OffsetToAxial(Col, Row);
			Tile.bIsPlaceable = Row >= EnemyRowCount;
			Tiles.Add(Tile);
		}
	}

	int32 PlaceableCount = 0;
	for (const FHexTile& Tile : Tiles)
	{
		PlaceableCount += Tile.bIsPlaceable ? 1 : 0;
	}
	UE_LOG(LogTemp, Display, TEXT("HexGrid: generated %d tiles, %d placeable"), Tiles.Num(), PlaceableCount);
}

bool UHexGrid::IsValidCoord(const FHexCoord& Coord) const
{
	const FIntPoint Offset = UHexCoordinateLibrary::AxialToOffset(Coord);
	return Offset.X >= 0 && Offset.X < BoardWidth && Offset.Y >= 0 && Offset.Y < BoardDepth;
}

const FHexTile* UHexGrid::GetTile(const FHexCoord& Coord) const
{
	if (!IsValidCoord(Coord))
	{
		return nullptr;
	}
	const FIntPoint Offset = UHexCoordinateLibrary::AxialToOffset(Coord);
	return &Tiles[Offset.Y * BoardWidth + Offset.X];
}

FHexTile* UHexGrid::GetMutableTile(const FHexCoord& Coord)
{
	if (!IsValidCoord(Coord))
	{
		return nullptr;
	}
	const FIntPoint Offset = UHexCoordinateLibrary::AxialToOffset(Coord);
	return &Tiles[Offset.Y * BoardWidth + Offset.X];
}

bool UHexGrid::SetOccupant(const FHexCoord& Coord, ABoardUnitBase* Unit)
{
	FHexTile* Tile = GetMutableTile(Coord);
	if (!Tile)
	{
		return false;
	}
	Tile->Occupant = Unit;
	OnOccupancyChanged.Broadcast();
	return true;
}

bool UHexGrid::ClearOccupant(const FHexCoord& Coord)
{
	FHexTile* Tile = GetMutableTile(Coord);
	if (!Tile)
	{
		return false;
	}
	Tile->Occupant = nullptr;
	OnOccupancyChanged.Broadcast();
	return true;
}

bool UHexGrid::SetSpawnFlag(const FHexCoord& Coord, bool bEnabled)
{
	FHexTile* Tile = GetMutableTile(Coord);
	if (!Tile)
	{
		return false;
	}
	Tile->bIsSpawn = bEnabled;
	return true;
}

bool UHexGrid::CanPlaceAt(const FHexCoord& Coord) const
{
	const FHexTile* Tile = GetTile(Coord);
	return Tile && CanPlaceOrSwapAt(Coord) && !Tile->Occupant.IsValid();
}

bool UHexGrid::CanPlaceOrSwapAt(const FHexCoord& Coord) const
{
	const FHexTile* Tile = GetTile(Coord);
	return Tile && Tile->bIsPlaceable && !Tile->bIsSpawn;
}

TArray<FHexCoord> UHexGrid::GetPlayerZoneTiles() const
{
	TArray<FHexCoord> Result;
	for (const FHexTile& Tile : Tiles)
	{
		if (Tile.bIsPlaceable)
		{
			Result.Add(Tile.Coord);
		}
	}
	return Result;
}

TArray<FHexCoord> UHexGrid::GetAllTileCoords() const
{
	TArray<FHexCoord> Result;
	Result.Reserve(Tiles.Num());
	for (const FHexTile& Tile : Tiles)
	{
		Result.Add(Tile.Coord);
	}
	return Result;
}

TArray<FHexCoord> UHexGrid::GetBackRowCoords() const
{
	TArray<FHexCoord> Result;
	if (BoardDepth <= 0)
	{
		return Result;
	}

	Result.Reserve(BoardWidth);
	for (int32 Col = 0; Col < BoardWidth; ++Col)
	{
		Result.Add(UHexCoordinateLibrary::OffsetToAxial(Col, BoardDepth - 1));
	}
	return Result;
}
