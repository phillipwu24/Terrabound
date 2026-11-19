// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnemyBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
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
	SpawnHoldRemaining = EnemyData->SpawnHoldSeconds;
	RateSpawnMontageToHold();
	bWalking = true;
	return true;
}

void AEnemyBase::RateSpawnMontageToHold()
{
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance || SpawnHoldRemaining <= 0.f)
	{
		// No hold: the first step's walk loop takes over the slot from the spawn montage.
		return;
	}

	// The ABP starts its spawn montage at its own BeginPlay, which has already run by now.
	UAnimMontage* SpawnMontage = AnimInstance->GetCurrentActiveMontage();
	if (!SpawnMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("Enemy '%s': no spawn montage playing to fit to the %.2fs spawn hold."),
			*EnemyData->DisplayName.ToString(), SpawnHoldRemaining);
		return;
	}

	AnimInstance->Montage_SetPlayRate(SpawnMontage, SpawnMontage->GetPlayLength() / SpawnHoldRemaining);
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

	float Remaining = DeltaSeconds;

	// The spawn hold comes first; time left over at its end carries into the first step.
	if (SpawnHoldRemaining > 0.f)
	{
		const float HoldUsed = FMath::Min(SpawnHoldRemaining, Remaining);
		SpawnHoldRemaining -= HoldUsed;
		Remaining -= HoldUsed;
		if (Remaining <= 0.f)
		{
			return;
		}
	}

	const float HexesPerSecond = EnemyData->MoveSpeedHexesPerSecond;
	ElapsedSeconds += Remaining;

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
				StopLoopingAnimation();
				HeldSeconds += Remaining;
				return;
			}
			// Consecutive steps keep the loop running rather than restarting it.
			if (!IsLoopingAnimationPlaying())
			{
				PlayLoopingAnimation(EnemyData->WalkAnimation, EnemyData->WalkPlayRate);
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

	// The pathfinder ignores units; this claim is the only place a unit blocks the step. The
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
	StopLoopingAnimation();
	UE_LOG(LogTemp, Display, TEXT("Enemy '%s': crossed the board in %.2fs after a %.2fs spawn hold (%d hexes at %.2f hexes/s, %.2fs held)."),
		*EnemyData->DisplayName.ToString(), ElapsedSeconds, EnemyData->SpawnHoldSeconds, StepsTaken, EnemyData->MoveSpeedHexesPerSecond, HeldSeconds);

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
