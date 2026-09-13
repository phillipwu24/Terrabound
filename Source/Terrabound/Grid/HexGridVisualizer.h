// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HexGridVisualizer.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Renders the board as one actor with an instanced static mesh component — not 56 actors.
 * Reads tile coordinates and HexRadius from UHexGrid; never writes to it. Placed once in the
 * level with SM_HexTile assigned (PLAN.md 2.1/2.2).
 */
UCLASS()
class TERRABOUND_API AHexGridVisualizer : public AActor
{
	GENERATED_BODY()

public:
	AHexGridVisualizer();

protected:
	virtual void BeginPlay() override;

private:
	void BuildTileInstances();

	// Its native Static Mesh field (assigned in the editor to SM_HexTile from task 2.1) is
	// what BuildTileInstances reads — no separate wrapper property, so there's one place to
	// set the mesh, not two.
	UPROPERTY(VisibleAnywhere, Category = "Hex Grid Visualizer")
	TObjectPtr<UInstancedStaticMeshComponent> TileInstances;
};
