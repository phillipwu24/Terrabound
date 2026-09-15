// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChampionBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "../Data/ChampionData.h"

AChampionBase::AChampionBase()
{
	SetTeam(EBoardUnitTeam::Player);
}

void AChampionBase::BeginPlay()
{
	Super::BeginPlay();

	// Faces the enemy side (decreasing R, i.e. -Y per AxialToWorld2D's Y = 1.5 * HexRadius * R) -
	// fixed by the board's coordinate convention, not something that varies per champion. Set
	// here rather than the constructor: a level-placed instance's transform is applied after
	// construction and would otherwise override whatever rotation the constructor set.
	SetActorRotation(FRotator(0.f, -90.f, 0.f));

	if (ChampionData)
	{
		InitializeFromChampionData(ChampionData);
	}
}

void AChampionBase::InitializeFromChampionData(UChampionData* Data)
{
	if (!Data)
	{
		UE_LOG(LogTemp, Error, TEXT("AChampionBase::InitializeFromChampionData: null Data."));
		return;
	}

	ChampionData = Data;

	if (Mesh)
	{
		Mesh->SetSkeletalMesh(Data->SkeletalMesh);
		if (Data->AnimBlueprint)
		{
			Mesh->SetAnimInstanceClass(Data->AnimBlueprint);
		}
	}
}
