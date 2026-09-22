// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "../Data/EnemyData.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexTile.h"
#include "../Pathfinding/HexPathfinder.h"

namespace
{
	FVector HexToWorld(const UHexGrid& Grid, const FHexCoord& Coord)
	{
		const FVector2D Position2D = UHexCoordinateLibrary::AxialToWorld2D(Coord, Grid.GetHexRadius());
		return FVector(Position2D.X, Position2D.Y, 0.f);
	}
}

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;
	SetTeam(EBoardUnitTeam::Enemy);
}

void AEnemyBase::InitializeFromEnemyData(UEnemyData* Data)
{
	if (!Data)
	{
		UE_LOG(LogTemp, Error, TEXT("AEnemyBase::InitializeFromEnemyData: null Data."));
		return;
	}
	if (Data->RangeInHexes < 1 || Data->RangeInHexes > 2)
	{
		UE_LOG(LogTemp, Error, TEXT("AEnemyBase::InitializeFromEnemyData: %s has RangeInHexes %d; enemy range is 1 or 2."),
			*Data->GetName(), Data->RangeInHexes);
	}

	EnemyData = Data;

	if (Mesh)
	{
		Mesh->SetSkeletalMesh(Data->SkeletalMesh);
		if (Data->AnimBlueprint)
		{
			Mesh->SetAnimInstanceClass(Data->AnimBlueprint);
		}
	}
}

bool AEnemyBase::EnterBoard(const FHexCoord& StartCoord)
{
	UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid || !EnemyData || EnemyData->MoveSpeedHexesPerSecond <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("AEnemyBase::EnterBoard: needs a HexGrid and enemy data with a speed above 0."));
		return false;
	}

	if (!Grid->TryOccupy(StartCoord, this))
	{
		return false;
	}

	SnapToHex(StartCoord);
	// Face the exit until the first step sets a facing of its own (+R is toward the back row).
	SetActorRotation(FRotator(0.f, 90.f, 0.f));

	bMidStep = false;
	bHoldLogged = false;
	StepProgress = 0.f;
	ElapsedSeconds = 0.f;
	HeldSeconds = 0.f;
	StepsTaken = 0;
	bWalking = true;
	return true;
}

void AEnemyBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceMovement(DeltaSeconds);
}

void AEnemyBase::AdvanceMovement(float DeltaSeconds)
{
	if (!bWalking)
	{
		return;
	}

	UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	if (!Grid)
	{
		bWalking = false;
		return;
	}

	const float HexesPerSecond = EnemyData->MoveSpeedHexesPerSecond;
	ElapsedSeconds += DeltaSeconds;
	float Remaining = DeltaSeconds;

	while (Remaining > 0.f)
	{
		if (!bMidStep)
		{
			if (Grid->GetBackRowCoords().Contains(GetCurrentCoord()))
			{
				// Only the time actually used counts toward the crossing.
				ElapsedSeconds -= Remaining;
				ExitBoard();
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
		SetActorLocation(StepEnd);
		bMidStep = false;
		++StepsTaken;
	}
}

bool AEnemyBase::TryBeginNextStep(UHexGrid& Grid)
{
	const FHexCoord From = GetCurrentCoord();
	const TMap<FHexCoord, float> Field = FHexPathfinder::ComputeDistanceField(Grid, Grid.GetBackRowCoords());
	const TOptional<FHexCoord> Next = FHexPathfinder::ChooseNextStep(Grid, Field, From);
	if (!Next.IsSet())
	{
		LogHoldOnce(TEXT("no route to the exit"));
		return false;
	}

	// The pathfinder prices an occupied hex; this claim is where occupancy is actually enforced. The
	// claim and the release happen together, so the enemy holds exactly one tile at every moment.
	if (!Grid.TryOccupy(Next.GetValue(), this))
	{
		LogHoldOnce(FString::Printf(TEXT("next hex %s is occupied"), *Next.GetValue().ToString()));
		return false;
	}
	Grid.ClearOccupant(From);
	SetCurrentCoord(Next.GetValue());

	StepStart = GetActorLocation();
	StepEnd = HexToWorld(Grid, Next.GetValue());
	StepProgress = 0.f;
	bMidStep = true;
	bHoldLogged = false;

	const FVector Direction = StepEnd - StepStart;
	if (!Direction.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, Direction.Rotation().Yaw, 0.f));
	}
	return true;
}

void AEnemyBase::LogHoldOnce(const FString& Reason)
{
	if (bHoldLogged)
	{
		return;
	}
	bHoldLogged = true;
	UE_LOG(LogTemp, Display, TEXT("Enemy '%s': holding at %s - %s."),
		*EnemyData->DisplayName.ToString(), *GetCurrentCoord().ToString(), *Reason);
}

void AEnemyBase::ExitBoard()
{
	bWalking = false;
	UE_LOG(LogTemp, Display, TEXT("Enemy '%s': crossed the board in %.2fs (%d hexes at %.2f hexes/s, %.2fs held)."),
		*EnemyData->DisplayName.ToString(), ElapsedSeconds, StepsTaken, EnemyData->MoveSpeedHexesPerSecond, HeldSeconds);

	OnEnemyExited.Broadcast(this);
	// Destroyed() releases the tile.
	Destroy();
}

void AEnemyBase::Destroyed()
{
	// Release the claimed hex on any real destruction (exit now, death later). Destroyed() is not
	// called on world teardown, where the grid may already be gone. Only if this enemy still holds
	// the tile: one that never entered the board has a default coord another unit may occupy.
	UHexGrid* Grid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
	const FHexTile* Tile = Grid ? Grid->GetTile(GetCurrentCoord()) : nullptr;
	if (Tile && Tile->Occupant.Get() == this)
	{
		Grid->ClearOccupant(GetCurrentCoord());
	}

	Super::Destroyed();
}
