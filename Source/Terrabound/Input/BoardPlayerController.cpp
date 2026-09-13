// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoardPlayerController.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexGridVisualizer.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

void ABoardPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// No UI widgets exist yet this checkpoint (shop/bench is Phase 6) - Game Only is enough,
	// just a visible cursor decoupled from camera control. Revisit when real UI arrives.
	bShowMouseCursor = true;
	SetInputMode(FInputModeGameOnly());

	HexGrid = GetWorld() ? GetWorld()->GetSubsystem<UHexGrid>() : nullptr;

	if (UWorld* World = GetWorld())
	{
		if (TActorIterator<AHexGridVisualizer> It(World); It)
		{
			Visualizer = *It;
		}
	}
}

void ABoardPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHoveredHex();
}

void ABoardPlayerController::UpdateHoveredHex()
{
	const bool bHadPreviousHex = bHasHoveredHex;
	const FHexCoord PreviousHex = HoveredHex;

	bHasHoveredHex = false;

	const UHexGrid* Grid = HexGrid.Get();
	if (Grid)
	{
		FVector RayOrigin, RayDirection;
		if (DeprojectMousePositionToWorld(RayOrigin, RayDirection))
		{
			// Analytic intersection with the board's ground plane (Z=0, same plane every tile
			// instance sits on in HexGridVisualizer) - not a line trace, per the grid-is-data
			// invariant.
			if (!FMath::IsNearlyZero(RayDirection.Z))
			{
				const float T = -RayOrigin.Z / RayDirection.Z;
				if (T >= 0.f)
				{
					const FVector HitPoint = RayOrigin + RayDirection * T;
					const FHexCoord Coord = UHexCoordinateLibrary::World2DToAxial(FVector2D(HitPoint.X, HitPoint.Y), Grid->GetHexRadius());
					if (Grid->IsValidCoord(Coord))
					{
						bHasHoveredHex = true;
						HoveredHex = Coord;
					}
				}
			}
		}
	}

	if (GEngine && bHasHoveredHex)
	{
		// Fixed key so this overwrites in place each frame rather than spamming the log/HUD.
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green, FString::Printf(TEXT("Hovered: %s"), *HoveredHex.ToString()));
	}

	// Nothing changed - skip the visual update entirely so a stationary cursor never resets and
	// reapplies a tile's state every frame (that redundant churn is what "no flicker at tile
	// boundaries" is guarding against, not the coordinate math itself).
	const bool bUnchanged = (bHadPreviousHex == bHasHoveredHex) && (!bHasHoveredHex || PreviousHex == HoveredHex);
	if (!bUnchanged)
	{
		ApplyHoverVisual(bHadPreviousHex, PreviousHex, bHasHoveredHex, HoveredHex);
	}
}

void ABoardPlayerController::ApplyHoverVisual(bool bHadPreviousHex, const FHexCoord& PreviousHex, bool bHasNewHex, const FHexCoord& NewHex)
{
	AHexGridVisualizer* Vis = Visualizer.Get();
	if (!Vis)
	{
		return;
	}

	if (bHadPreviousHex)
	{
		Vis->SetTileVisualState(PreviousHex, Vis->GetBaseVisualState(PreviousHex));
	}
	if (bHasNewHex)
	{
		Vis->SetTileVisualState(NewHex, EHexTileVisualState::Hovered);
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
