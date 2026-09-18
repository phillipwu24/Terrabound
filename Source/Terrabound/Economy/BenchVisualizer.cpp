// Copyright Epic Games, Inc. All Rights Reserved.

#include "BenchVisualizer.h"
#include "Bench.h"
#include "Components/InstancedStaticMeshComponent.h"

ABenchVisualizer::ABenchVisualizer()
{
	PrimaryActorTick.bCanEverTick = false;

	SlotInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SlotInstances"));
	RootComponent = SlotInstances;
}

void ABenchVisualizer::BeginPlay()
{
	Super::BeginPlay();
	BuildSlotInstances();
}

void ABenchVisualizer::BuildSlotInstances()
{
	SlotInstances->ClearInstances();
	SlotTransforms.Reset();

	const UBench* Bench = GetWorld() ? GetWorld()->GetSubsystem<UBench>() : nullptr;
	const int32 SlotCount = Bench ? Bench->GetSlotCount() : 0;

	for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
	{
		const FTransform LocalTransform(SlotSpacing * static_cast<float>(SlotIndex));
		SlotInstances->AddInstance(LocalTransform);
		SlotTransforms.Add(LocalTransform * GetActorTransform());
	}
}

FTransform ABenchVisualizer::GetSlotTransform(int32 SlotIndex) const
{
	return SlotTransforms.IsValidIndex(SlotIndex) ? SlotTransforms[SlotIndex] : GetActorTransform();
}

bool ABenchVisualizer::FindNearestSlot(const FVector2D& Point, int32& OutSlotIndex) const
{
	int32 BestIndex = INDEX_NONE;
	float BestDistSq = PickRadius * PickRadius;

	for (int32 SlotIndex = 0; SlotIndex < SlotTransforms.Num(); ++SlotIndex)
	{
		const FVector2D SlotLocation2D(SlotTransforms[SlotIndex].GetLocation());
		const float DistSq = FVector2D::DistSquared(Point, SlotLocation2D);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			BestIndex = SlotIndex;
		}
	}

	OutSlotIndex = BestIndex;
	return BestIndex != INDEX_NONE;
}
