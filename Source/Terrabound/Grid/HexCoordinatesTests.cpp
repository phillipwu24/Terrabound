// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexCoordinates.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace HexCoordinatesTests
{
	// Board dimensions per DA_BoardConfig; not read from the asset since these are pure-math
	// tests that must hold regardless of the content asset's tuned values.
	constexpr int32 BoardWidth = 7;
	constexpr int32 BoardDepth = 8;

	// Arbitrary positive test radius. The math must round-trip correctly for any radius,
	// so this deliberately isn't DA_BoardConfig's real HexRadius.
	constexpr float TestHexRadius = 100.f;
}

/**
 * World -> axial -> world (and offset -> axial -> offset) round-trips for every board tile,
 * including points near a hex's edge, not just its center. Near-edge coverage matters: a
 * naive implementation that rounds q and r independently instead of doing proper cube
 * rounding can still pass for points near a hex's center and only fail near its boundary.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordRoundTripTest, "Terrabound.Grid.HexCoordinates.RoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexCoordRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace HexCoordinatesTests;

	FRandomStream Rng(12345);

	// Inscribed-circle radius: any point within this distance of a hex's center, in any
	// direction, is guaranteed to lie inside that hex.
	const float Apothem = TestHexRadius * FMath::Sqrt(3.f) / 2.f;

	for (int32 Row = 0; Row < BoardDepth; ++Row)
	{
		for (int32 Col = 0; Col < BoardWidth; ++Col)
		{
			const FHexCoord Axial = UHexCoordinateLibrary::OffsetToAxial(Col, Row);

			const FIntPoint BackToOffset = UHexCoordinateLibrary::AxialToOffset(Axial);
			TestEqual(TEXT("Offset round-trip Col"), BackToOffset.X, Col);
			TestEqual(TEXT("Offset round-trip Row"), BackToOffset.Y, Row);

			const FVector2D Center = UHexCoordinateLibrary::AxialToWorld2D(Axial, TestHexRadius);

			const FHexCoord FromCenter = UHexCoordinateLibrary::World2DToAxial(Center, TestHexRadius);
			TestTrue(FString::Printf(TEXT("Center round-trip at %s"), *Axial.ToString()), FromCenter == Axial);

			// Sample near the center and near the edge (up to 95% of the apothem, safely
			// short of crossing into a neighbour) in a ring of directions.
			const float SampleDistances[] = { Apothem * 0.1f, Apothem * 0.5f, Apothem * 0.95f };
			for (float Distance : SampleDistances)
			{
				for (int32 Sample = 0; Sample < 6; ++Sample)
				{
					const float Angle = Rng.FRandRange(0.f, 2.f * PI);
					const FVector2D Offset(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance);
					const FHexCoord FromSample = UHexCoordinateLibrary::World2DToAxial(Center + Offset, TestHexRadius);
					TestTrue(
						FString::Printf(TEXT("Interior sample round-trip at %s, distance %f, angle %f"), *Axial.ToString(), Distance, Angle),
						FromSample == Axial);
				}
			}
		}
	}

	return true;
}

/**
 * If B is a neighbour of A, A must be a neighbour of B, for every tile on the board, both
 * row parities. Also checks each tile has exactly 6 unique neighbours.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordNeighbourSymmetryTest, "Terrabound.Grid.HexCoordinates.NeighbourSymmetry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexCoordNeighbourSymmetryTest::RunTest(const FString& Parameters)
{
	using namespace HexCoordinatesTests;

	for (int32 Row = 0; Row < BoardDepth; ++Row)
	{
		for (int32 Col = 0; Col < BoardWidth; ++Col)
		{
			const FHexCoord Coord = UHexCoordinateLibrary::OffsetToAxial(Col, Row);
			const TArray<FHexCoord> Neighbours = UHexCoordinateLibrary::GetNeighbours(Coord);

			TestEqual(FString::Printf(TEXT("Six neighbours at %s"), *Coord.ToString()), Neighbours.Num(), 6);

			TSet<FHexCoord> UniqueNeighbours(Neighbours);
			TestEqual(FString::Printf(TEXT("Neighbours are unique at %s"), *Coord.ToString()), UniqueNeighbours.Num(), Neighbours.Num());

			for (const FHexCoord& Neighbour : Neighbours)
			{
				const TArray<FHexCoord> BackNeighbours = UHexCoordinateLibrary::GetNeighbours(Neighbour);
				const bool bSymmetric = BackNeighbours.ContainsByPredicate([&Coord](const FHexCoord& Candidate)
				{
					return Candidate == Coord;
				});
				TestTrue(
					FString::Printf(TEXT("%s is a neighbour of its neighbour %s"), *Coord.ToString(), *Neighbour.ToString()),
					bSymmetric);
			}
		}
	}

	return true;
}

/**
 * Pins the orientation that symmetry alone can't catch: the exact 6 neighbour deltas from
 * the hex math reference (not just an internally-consistent set), and that odd rows are
 * shifted in the +X direction relative to even rows, per CLAUDE.md's locked convention.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordOrientationTest, "Terrabound.Grid.HexCoordinates.Orientation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexCoordOrientationTest::RunTest(const FString& Parameters)
{
	using namespace HexCoordinatesTests;

	// Exact expected delta set, including the two forward-toward-exit moves (0,+1) and (-1,+1).
	const FHexCoord ExpectedDeltas[6] =
	{
		FHexCoord(1, 0),
		FHexCoord(1, -1),
		FHexCoord(0, -1),
		FHexCoord(-1, 0),
		FHexCoord(-1, 1),
		FHexCoord(0, 1),
	};

	const TArray<FHexCoord> Neighbours = UHexCoordinateLibrary::GetNeighbours(FHexCoord(0, 0));
	TestEqual(TEXT("Origin has 6 neighbours"), Neighbours.Num(), 6);
	for (const FHexCoord& Expected : ExpectedDeltas)
	{
		TestTrue(
			FString::Printf(TEXT("Origin neighbours include delta %s"), *Expected.ToString()),
			Neighbours.ContainsByPredicate([&Expected](const FHexCoord& Candidate) { return Candidate == Expected; }));
	}

	// Odd rows shift right (+X): at any column, an odd row's world X is exactly
	// HexRadius * sqrt(3)/2 to the right of the same column on an even row.
	const float ExpectedShift = TestHexRadius * FMath::Sqrt(3.f) / 2.f;
	for (int32 Col = 0; Col < BoardWidth; ++Col)
	{
		for (int32 EvenRow = 0; EvenRow < BoardDepth; EvenRow += 2)
		{
			for (int32 OddRow = 1; OddRow < BoardDepth; OddRow += 2)
			{
				const FVector2D EvenPos = UHexCoordinateLibrary::AxialToWorld2D(UHexCoordinateLibrary::OffsetToAxial(Col, EvenRow), TestHexRadius);
				const FVector2D OddPos = UHexCoordinateLibrary::AxialToWorld2D(UHexCoordinateLibrary::OffsetToAxial(Col, OddRow), TestHexRadius);
				TestEqual(
					FString::Printf(TEXT("Odd row %d shifts +X relative to even row %d at col %d"), OddRow, EvenRow, Col),
					static_cast<float>(OddPos.X - EvenPos.X), ExpectedShift, 0.01f);
			}
		}
	}

	return true;
}

/** AxialDistance correctness: zero self-distance, symmetry, unit distance to neighbours, and a hand-worked case. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordDistanceTest, "Terrabound.Grid.HexCoordinates.Distance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexCoordDistanceTest::RunTest(const FString& Parameters)
{
	using namespace HexCoordinatesTests;

	FRandomStream Rng(6789);

	for (int32 Row = 0; Row < BoardDepth; ++Row)
	{
		for (int32 Col = 0; Col < BoardWidth; ++Col)
		{
			const FHexCoord Coord = UHexCoordinateLibrary::OffsetToAxial(Col, Row);

			TestEqual(FString::Printf(TEXT("Self-distance zero at %s"), *Coord.ToString()), UHexCoordinateLibrary::AxialDistance(Coord, Coord), 0);

			for (const FHexCoord& Neighbour : UHexCoordinateLibrary::GetNeighbours(Coord))
			{
				TestEqual(
					FString::Printf(TEXT("Distance to neighbour %s from %s is 1"), *Neighbour.ToString(), *Coord.ToString()),
					UHexCoordinateLibrary::AxialDistance(Coord, Neighbour), 1);
			}
		}
	}

	// Symmetry, random pairs.
	for (int32 Sample = 0; Sample < 20; ++Sample)
	{
		const FHexCoord A(Rng.RandRange(-10, 10), Rng.RandRange(-10, 10));
		const FHexCoord B(Rng.RandRange(-10, 10), Rng.RandRange(-10, 10));
		TestEqual(TEXT("Distance is symmetric"), UHexCoordinateLibrary::AxialDistance(A, B), UHexCoordinateLibrary::AxialDistance(B, A));
	}

	// Hand-worked case, verified independently via cube distance (max of |dx|,|dy|,|dz|):
	// (0,0) to (2,3) -> cube delta (2, -5, 3) -> max(2,5,3) = 5.
	TestEqual(TEXT("Hand-worked distance (0,0)->(2,3)"), UHexCoordinateLibrary::AxialDistance(FHexCoord(0, 0), FHexCoord(2, 3)), 5);

	return true;
}

/** FHexCoord must work as a TMap/TSet key: equal-but-distinct instances hash and compare consistently. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexCoordMapKeyTest, "Terrabound.Grid.HexCoordinates.MapKey", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHexCoordMapKeyTest::RunTest(const FString& Parameters)
{
	using namespace HexCoordinatesTests;

	TMap<FHexCoord, int32> TileIndex;
	int32 NextIndex = 0;
	for (int32 Row = 0; Row < BoardDepth; ++Row)
	{
		for (int32 Col = 0; Col < BoardWidth; ++Col)
		{
			TileIndex.Add(UHexCoordinateLibrary::OffsetToAxial(Col, Row), NextIndex++);
		}
	}

	TestEqual(TEXT("No key collisions across the board"), TileIndex.Num(), BoardWidth * BoardDepth);

	for (int32 Row = 0; Row < BoardDepth; ++Row)
	{
		for (int32 Col = 0; Col < BoardWidth; ++Col)
		{
			// Freshly constructed coordinate, not the same instance used to insert.
			const FHexCoord LookupCoord = UHexCoordinateLibrary::OffsetToAxial(Col, Row);
			const int32 ExpectedIndex = Row * BoardWidth + Col;
			const int32* Found = TileIndex.Find(LookupCoord);
			TestNotNull(FString::Printf(TEXT("Lookup finds %s"), *LookupCoord.ToString()), Found);
			if (Found)
			{
				TestEqual(FString::Printf(TEXT("Lookup returns correct value at %s"), *LookupCoord.ToString()), *Found, ExpectedIndex);
			}
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
