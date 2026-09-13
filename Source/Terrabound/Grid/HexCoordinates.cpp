// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexCoordinates.h"

FIntVector UHexCoordinateLibrary::AxialToCube(const FHexCoord& Coord)
{
	const int32 X = Coord.Q;
	const int32 Z = Coord.R;
	const int32 Y = -X - Z;
	return FIntVector(X, Y, Z);
}

int32 UHexCoordinateLibrary::AxialDistance(const FHexCoord& A, const FHexCoord& B)
{
	const int32 DQ = A.Q - B.Q;
	const int32 DR = A.R - B.R;
	return (FMath::Abs(DQ) + FMath::Abs(DQ + DR) + FMath::Abs(DR)) / 2;
}

TArray<FHexCoord> UHexCoordinateLibrary::GetNeighbours(const FHexCoord& Coord)
{
	static const FHexCoord Deltas[6] =
	{
		FHexCoord(1, 0),
		FHexCoord(1, -1),
		FHexCoord(0, -1),
		FHexCoord(-1, 0),
		FHexCoord(-1, 1),
		FHexCoord(0, 1),
	};

	TArray<FHexCoord> Neighbours;
	Neighbours.Reserve(6);
	for (const FHexCoord& Delta : Deltas)
	{
		Neighbours.Add(FHexCoord(Coord.Q + Delta.Q, Coord.R + Delta.R));
	}
	return Neighbours;
}

FHexCoord UHexCoordinateLibrary::OffsetToAxial(int32 Col, int32 Row)
{
	const int32 Q = Col - (Row - (Row & 1)) / 2;
	return FHexCoord(Q, Row);
}

FIntPoint UHexCoordinateLibrary::AxialToOffset(const FHexCoord& Coord)
{
	const int32 Row = Coord.R;
	const int32 Col = Coord.Q + (Row - (Row & 1)) / 2;
	return FIntPoint(Col, Row);
}

FVector2D UHexCoordinateLibrary::AxialToWorld2D(const FHexCoord& Coord, float HexRadius)
{
	const float Sqrt3 = FMath::Sqrt(3.f);
	const float X = HexRadius * (Sqrt3 * Coord.Q + (Sqrt3 / 2.f) * Coord.R);
	const float Y = HexRadius * (1.5f * Coord.R);
	return FVector2D(X, Y);
}

FHexCoord UHexCoordinateLibrary::World2DToAxial(const FVector2D& Position, float HexRadius)
{
	const float Sqrt3 = FMath::Sqrt(3.f);

	// Inverse of AxialToWorld2D: fractional axial coordinates.
	const float FracQ = ((Sqrt3 / 3.f) * Position.X - (1.f / 3.f) * Position.Y) / HexRadius;
	const float FracR = (2.f / 3.f) * Position.Y / HexRadius;

	// Fractional cube coordinates, then cube rounding: round each component, then
	// recompute the one with the largest rounding error from the other two so
	// X + Y + Z stays 0.
	const float X = FracQ;
	const float Z = FracR;
	const float Y = -X - Z;

	float RoundX = FMath::RoundToFloat(X);
	float RoundY = FMath::RoundToFloat(Y);
	float RoundZ = FMath::RoundToFloat(Z);

	const float XDiff = FMath::Abs(RoundX - X);
	const float YDiff = FMath::Abs(RoundY - Y);
	const float ZDiff = FMath::Abs(RoundZ - Z);

	if (XDiff > YDiff && XDiff > ZDiff)
	{
		RoundX = -RoundY - RoundZ;
	}
	else if (YDiff > ZDiff)
	{
		RoundY = -RoundX - RoundZ;
	}
	else
	{
		RoundZ = -RoundX - RoundY;
	}

	return FHexCoord(FMath::RoundToInt(RoundX), FMath::RoundToInt(RoundZ));
}
