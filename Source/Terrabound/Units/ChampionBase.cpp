// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChampionBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "../Data/ChampionData.h"
#include "../TerraboundSettings.h"

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

	if (Mesh)
	{
		BaseMeshScale = Mesh->GetRelativeScale3D();
	}
	ApplyStarScale();
}

void AChampionBase::SetStarLevel(int32 NewStarLevel)
{
	StarLevel = FMath::Max(1, NewStarLevel);
	// Before BeginPlay BaseMeshScale isn't captured yet; BeginPlay applies the scale itself.
	if (HasActorBegunPlay())
	{
		ApplyStarScale();
	}
}

void AChampionBase::ApplyStarScale()
{
	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	if (!Mesh || !Settings || Settings->StarMeshScaleMultipliers.IsEmpty())
	{
		return;
	}

	const TArray<float>& Multipliers = Settings->StarMeshScaleMultipliers;
	const float Multiplier = Multipliers[FMath::Min(StarLevel - 1, Multipliers.Num() - 1)];
	Mesh->SetRelativeScale3D(BaseMeshScale * Multiplier);
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
