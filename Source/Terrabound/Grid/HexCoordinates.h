// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HexCoordinates.generated.h"

/**
 * Axial hex coordinate (q, r). The only coordinate representation tiles are stored in;
 * never store offset coordinates. See CLAUDE.md's "Axial coordinates" invariant.
 */
USTRUCT(BlueprintType)
struct TERRABOUND_API FHexCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex")
	int32 Q = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hex")
	int32 R = 0;

	FHexCoord() = default;
	FHexCoord(int32 InQ, int32 InR) : Q(InQ), R(InR) {}

	bool operator==(const FHexCoord& Other) const
	{
		return Q == Other.Q && R == Other.R;
	}

	FString ToString() const
	{
		return FString::Printf(TEXT("(%d, %d)"), Q, R);
	}
};

FORCEINLINE uint32 GetTypeHash(const FHexCoord& Coord)
{
	return HashCombine(::GetTypeHash(Coord.Q), ::GetTypeHash(Coord.R));
}

/**
 * Pure hex coordinate math: axial/cube/offset/world conversions for a pointy-top,
 * odd-r offset grid (odd rows shift right). No world access, no side effects — see
 * CLAUDE.md's "Coordinate math stays pure" note. HexRadius is always a parameter,
 * never read from a subsystem, so this library stays stateless.
 */
UCLASS()
class TERRABOUND_API UHexCoordinateLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Axial to cube coordinates: X = Q, Z = R, Y = -X - Z. */
	UFUNCTION(BlueprintPure, Category = "Hex")
	static FIntVector AxialToCube(const FHexCoord& Coord);

	/** Hex distance between two axial coordinates. */
	UFUNCTION(BlueprintPure, Category = "Hex")
	static int32 AxialDistance(const FHexCoord& A, const FHexCoord& B);

	/** The 6 axial neighbours, no bounds checking. */
	UFUNCTION(BlueprintPure, Category = "Hex")
	static TArray<FHexCoord> GetNeighbours(const FHexCoord& Coord);

	/** Odd-r offset (col, row) to axial. */
	UFUNCTION(BlueprintPure, Category = "Hex")
	static FHexCoord OffsetToAxial(int32 Col, int32 Row);

	/** Axial to odd-r offset (col, row), returned as (X = Col, Y = Row). */
	UFUNCTION(BlueprintPure, Category = "Hex")
	static FIntPoint AxialToOffset(const FHexCoord& Coord);

	/** Axial to 2D world position (pointy-top, odd-r shift), before projecting onto the board plane. */
	UFUNCTION(BlueprintPure, Category = "Hex")
	static FVector2D AxialToWorld2D(const FHexCoord& Coord, float HexRadius);

	/** 2D world position to axial, via fractional axial then cube rounding. */
	UFUNCTION(BlueprintPure, Category = "Hex")
	static FHexCoord World2DToAxial(const FVector2D& Position, float HexRadius);
};
