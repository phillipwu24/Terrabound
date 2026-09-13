// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoardPlayerController.h"
#include "../Grid/HexGrid.h"
#include "Engine/Engine.h"

void ABoardPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// No UI widgets exist yet this checkpoint (shop/bench is Phase 6) - Game Only is enough,
	// just a visible cursor decoupled from camera control. Revisit when real UI arrives.
	bShowMouseCursor = true;
	SetInputMode(FInputModeGameOnly());

	HexGrid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;
}

void ABoardPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHoveredHex();
}

void ABoardPlayerController::UpdateHoveredHex()
{
	bHasHoveredHex = false;

	const UHexGrid* Grid = HexGrid.Get();
	if (!Grid)
	{
		return;
	}

	FVector RayOrigin, RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return;
	}

	// Analytic intersection with the board's ground plane (Z=0, same plane every tile instance
	// sits on in HexGridVisualizer) - not a line trace, per the grid-is-data invariant.
	if (FMath::IsNearlyZero(RayDirection.Z))
	{
		return;
	}
	const float T = -RayOrigin.Z / RayDirection.Z;
	if (T < 0.f)
	{
		return;
	}
	const FVector HitPoint = RayOrigin + RayDirection * T;

	const FHexCoord Coord = UHexCoordinateLibrary::World2DToAxial(FVector2D(HitPoint.X, HitPoint.Y), Grid->GetHexRadius());
	if (!Grid->IsValidCoord(Coord))
	{
		return;
	}

	bHasHoveredHex = true;
	HoveredHex = Coord;

	if (GEngine)
	{
		// Fixed key so this overwrites in place each frame rather than spamming the log/HUD.
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green, FString::Printf(TEXT("Hovered: %s"), *Coord.ToString()));
	}
}

bool ABoardPlayerController::GetHoveredHex(FHexCoord& OutCoord) const
{
	if (bHasHoveredHex)
	{
		OutCoord = HoveredHex;
	}
	return bHasHoveredHex;
}
