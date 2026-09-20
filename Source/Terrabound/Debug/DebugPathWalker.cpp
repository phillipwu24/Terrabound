// Copyright Epic Games, Inc. All Rights Reserved.

#include "DebugPathWalker.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexTile.h"
#include "../Pathfinding/HexPathfinder.h"

ADebugPathWalker::ADebugPathWalker()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Debug-only, so an engine asset by path is fine here (same call the debug commands make). The
	// engine ships no capsule mesh; a cylinder is the stand-in. No collision, so it never
	// interferes with the hover/drag trace.
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.5f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, 75.f)); // half the scaled height, so it stands on the board plane

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
}

void ADebugPathWalker::StartWalking(const FHexCoord& StartCoord, float InHexesPerSecond)
{
	const UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid || InHexesPerSecond <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("ADebugPathWalker::StartWalking: needs a HexGrid and a speed above 0."));
		return;
	}

	CurrentCoord = StartCoord;
	SetActorLocation(HexToWorld(*Grid, StartCoord));
	HexesPerSecond = InHexesPerSecond;
	bWalking = true;
}

void ADebugPathWalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bWalking)
	{
		return;
	}

	const UHexGrid* Grid = GetWorld()->GetSubsystem<UHexGrid>();
	if (!Grid)
	{
		bWalking = false;
		return;
	}

	ElapsedSeconds += DeltaSeconds;
	float Remaining = DeltaSeconds;

	// Time left over after an arrival carries into the next step, so the crossing time doesn't
	// depend on frame rate.
	while (Remaining > 0.f)
	{
		if (!bMidStep)
		{
			if (Grid->GetBackRowCoords().Contains(CurrentCoord))
			{
				// Only the time actually used counts toward the crossing.
				ElapsedSeconds -= Remaining;
				LogCrossing();
				Destroy();
				return;
			}

			if (!TryBeginNextStep(*Grid))
			{
				HeldSeconds += Remaining;
				return;
			}
		}

		const float SecondsToArrive = (1.f - StepProgress) / HexesPerSecond;
		if (Remaining < SecondsToArrive)
		{
			StepProgress += Remaining * HexesPerSecond;
			SetActorLocation(FMath::Lerp(StepStart, StepEnd, StepProgress));
			return;
		}

		Remaining -= SecondsToArrive;
		CurrentCoord = StepTarget;
		SetActorLocation(StepEnd);
		bMidStep = false;
		++StepsTaken;
	}
}

bool ADebugPathWalker::TryBeginNextStep(const UHexGrid& Grid)
{
	const TMap<FHexCoord, float> Field = FHexPathfinder::ComputeDistanceField(Grid, Grid.GetBackRowCoords());
	const TOptional<FHexCoord> Next = FHexPathfinder::ChooseNextStep(Grid, Field, CurrentCoord);
	if (!Next.IsSet())
	{
		LogHoldOnce(TEXT("no route to the exit"));
		return false;
	}

	// The pathfinder prices an occupied hex; this is where occupancy is actually enforced.
	const FHexTile* NextTile = Grid.GetTile(Next.GetValue());
	if (NextTile && NextTile->Occupant.IsValid())
	{
		LogHoldOnce(FString::Printf(TEXT("next hex %s is occupied"), *Next.GetValue().ToString()));
		return false;
	}

	StepTarget = Next.GetValue();
	StepStart = GetActorLocation();
	StepEnd = HexToWorld(Grid, StepTarget);
	StepProgress = 0.f;
	bMidStep = true;
	bHoldLogged = false;
	return true;
}

void ADebugPathWalker::LogHoldOnce(const FString& Reason)
{
	if (bHoldLogged)
	{
		return;
	}
	bHoldLogged = true;
	UE_LOG(LogTemp, Display, TEXT("DebugPathWalker: holding at %s - %s."), *CurrentCoord.ToString(), *Reason);
}

void ADebugPathWalker::LogCrossing() const
{
	UE_LOG(LogTemp, Display, TEXT("DebugPathWalker: crossed the board in %.2fs (%d hexes at %.2f hexes/s, %.2fs held)."),
		ElapsedSeconds, StepsTaken, HexesPerSecond, HeldSeconds);
}

FVector ADebugPathWalker::HexToWorld(const UHexGrid& Grid, const FHexCoord& Coord) const
{
	const FVector2D Position2D = UHexCoordinateLibrary::AxialToWorld2D(Coord, Grid.GetHexRadius());
	return FVector(Position2D.X, Position2D.Y, 0.f);
}
