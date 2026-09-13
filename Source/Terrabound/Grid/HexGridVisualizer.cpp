// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexGridVisualizer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HexGrid.h"
#include "HexCoordinates.h"
#include "DrawDebugHelpers.h"

AHexGridVisualizer::AHexGridVisualizer()
{
	// Ticks unconditionally so the debug coord overlay can be toggled at runtime; DrawCoordOverlay
	// early-returns when it's off, so this costs one bool check per frame otherwise.
	PrimaryActorTick.bCanEverTick = true;

	TileInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TileInstances"));
	SetRootComponent(TileInstances);
	TileInstances->NumCustomDataFloats = 1;
}

void AHexGridVisualizer::SetTileVisualState(const FHexCoord& Coord, EHexTileVisualState State)
{
	if (const int32* Index = InstanceIndexByCoord.Find(Coord))
	{
		TileInstances->SetCustomDataValue(*Index, 0, static_cast<float>(State), /*bMarkRenderStateDirty=*/true);
	}
}

void AHexGridVisualizer::BeginPlay()
{
	Super::BeginPlay();
	BuildTileInstances();
}

void AHexGridVisualizer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bShowCoordOverlay)
	{
		DrawCoordOverlay();
	}
}

void AHexGridVisualizer::DrawCoordOverlay() const
{
	const UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid)
	{
		return;
	}

	const float HexRadius = Grid->GetHexRadius();
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		const FVector2D WorldPos2D = UHexCoordinateLibrary::AxialToWorld2D(Coord, HexRadius);
		const FVector TextLocation(WorldPos2D.X, WorldPos2D.Y, 10.f);
		// Duration 0: drawn fresh every tick while the overlay is on, gone the instant it's off.
		DrawDebugString(GetWorld(), TextLocation, Coord.ToString(), nullptr, FColor::White, 0.f, false, 1.5f);
	}
}

void AHexGridVisualizer::BuildTileInstances()
{
	TileInstances->ClearInstances();
	InstanceIndexByCoord.Reset();

	if (!TileInstances->GetStaticMesh())
	{
		UE_LOG(LogTemp, Error, TEXT("HexGridVisualizer: no static mesh assigned to TileInstances."));
		return;
	}

	const UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid)
	{
		UE_LOG(LogTemp, Error, TEXT("HexGridVisualizer: no HexGrid subsystem for this world."));
		return;
	}

	const float HexRadius = Grid->GetHexRadius();
	const FVector ActorOrigin = GetActorLocation();
	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		const FVector2D WorldPos2D = UHexCoordinateLibrary::AxialToWorld2D(Coord, HexRadius);
		const FVector InstanceLocation(WorldPos2D.X, WorldPos2D.Y, 0.f);
		const FTransform InstanceTransform(FRotator::ZeroRotator, InstanceLocation - ActorOrigin, FVector::OneVector);
		const int32 InstanceIndex = TileInstances->AddInstance(InstanceTransform);
		InstanceIndexByCoord.Add(Coord, InstanceIndex);

		const FHexTile* Tile = Grid->GetTile(Coord);
		const EHexTileVisualState State = (Tile && Tile->bIsPlaceable) ? EHexTileVisualState::PlayerZone : EHexTileVisualState::EnemyZone;
		TileInstances->SetCustomDataValue(InstanceIndex, 0, static_cast<float>(State));
	}
}
