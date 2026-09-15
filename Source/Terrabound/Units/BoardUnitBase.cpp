// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoardUnitBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../Grid/HexGrid.h"
#include "../TerraboundSettings.h"

ABoardUnitBase::ABoardUnitBase()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	// No PhysicsAsset is assigned to units, so this has no collision anyway - set explicitly so
	// it's clear the mesh is never meant to be traced against. HitBox is the click/drag target.
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Corrective offset for the Paragon packs' rig convention: their rest-pose forward is the
	// mesh's local +Y, not Unreal's default +X. Without this, an actor rotation meant to face a
	// world direction (e.g. ChampionBase's "face the enemy side") visually faces 90 degrees off
	// from what the rotation math intends. Applied here rather than per-champion since every
	// board unit - champions now, enemies later - is built from these same packs.
	Mesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	HitBox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(RootComponent);
	HitBox->SetCapsuleRadius(40.f);
	HitBox->SetCapsuleHalfHeight(90.f);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void ABoardUnitBase::SnapToHex(const FHexCoord& Coord)
{
	const UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid)
	{
		UE_LOG(LogTemp, Error, TEXT("ABoardUnitBase::SnapToHex: no HexGrid subsystem for this world."));
		return;
	}

	const FVector2D WorldPos2D = UHexCoordinateLibrary::AxialToWorld2D(Coord, Grid->GetHexRadius());
	SetActorLocation(FVector(WorldPos2D.X, WorldPos2D.Y, 0.f));
	CurrentCoord = Coord;
}

void ABoardUnitBase::SetTeam(EBoardUnitTeam NewTeam)
{
	Team = NewTeam;
	ApplyTeamTint();
}

void ABoardUnitBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyTeamTint();
}

void ABoardUnitBase::ApplyTeamTint()
{
	if (!Mesh || !GetWorld())
	{
		return;
	}

	const UTerraboundSettings* Settings = GetDefault<UTerraboundSettings>();
	UMaterialInterface* BaseOverlay = Settings ? Settings->TeamTintOverlayMaterial.LoadSynchronous() : nullptr;
	if (!BaseOverlay)
	{
		UE_LOG(LogTemp, Error, TEXT("ABoardUnitBase::ApplyTeamTint: no TeamTintOverlayMaterial set in Project Settings > Terrabound."));
		return;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseOverlay, this);
	const FLinearColor TintColor = (Team == EBoardUnitTeam::Player) ? Settings->PlayerTintColor : Settings->EnemyTintColor;
	MID->SetVectorParameterValue(TEXT("TintColor"), TintColor);
	Mesh->SetOverlayMaterial(MID);
}
