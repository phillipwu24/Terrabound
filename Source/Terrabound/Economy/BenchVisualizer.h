// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BenchVisualizer.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Renders the bench as a row of world-space slot markers, TFT-style - reuses the same
 * ground-plane cursor math and drag state machine the hex board uses (PLAN.md 6.4) instead of
 * requiring a UMG widget first. Reads UBench for slot count only; never writes to it, same
 * relationship AHexGridVisualizer has to UHexGrid. Placed once in the level with a placeholder
 * mesh assigned - deliberately cheap, same spirit as SM_HexTile (PLAN.md 2.1).
 */
UCLASS()
class TERRABOUND_API ABenchVisualizer : public AActor
{
	GENERATED_BODY()

public:
	ABenchVisualizer();

	/** World transform of SlotIndex's placement anchor. Falls back to the actor's own transform
	 * if SlotIndex is out of range. */
	FTransform GetSlotTransform(int32 SlotIndex) const;

	/** True and fills OutSlotIndex with the closest slot if Point (board-plane XY) is within
	 * PickRadius of one. */
	bool FindNearestSlot(const FVector2D& Point, int32& OutSlotIndex) const;

protected:
	virtual void BeginPlay() override;

private:
	void BuildSlotInstances();

	// Its native Static Mesh field (assigned in the editor to a placeholder mesh) is what
	// BuildSlotInstances reads - no separate wrapper property, matching AHexGridVisualizer.
	UPROPERTY(VisibleAnywhere, Category = "Bench Visualizer")
	TObjectPtr<UInstancedStaticMeshComponent> SlotInstances;

	/** World-unit offset between adjacent slots, in the actor's local space. */
	UPROPERTY(EditAnywhere, Category = "Bench Visualizer")
	FVector SlotSpacing = FVector(150.f, 0.f, 0.f);

	/** How close the cursor needs to land to a slot's anchor to count as hovering it. */
	UPROPERTY(EditAnywhere, Category = "Bench Visualizer")
	float PickRadius = 75.f;

	// World-space, computed once in BeginPlay from the actor's own (level-placed) transform -
	// the bench never moves after that, same assumption AHexGridVisualizer makes about tiles.
	TArray<FTransform> SlotTransforms;
};
