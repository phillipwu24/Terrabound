// Copyright Epic Games, Inc. All Rights Reserved.

#include "HexGridVisualizer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HexGrid.h"
#include "HexCoordinates.h"

AHexGridVisualizer::AHexGridVisualizer()
{
	PrimaryActorTick.bCanEverTick = false;

	TileInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TileInstances"));
	SetRootComponent(TileInstances);
}

void AHexGridVisualizer::BeginPlay()
{
	Super::BeginPlay();
	BuildTileInstances();
}

void AHexGridVisualizer::BuildTileInstances()
{
	TileInstances->ClearInstances();

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
		TileInstances->AddInstance(InstanceTransform);
	}
}
