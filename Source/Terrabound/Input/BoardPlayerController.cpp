// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoardPlayerController.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexGridVisualizer.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

void ABoardPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// GameOnly locks/captures the mouse to the viewport (fine for FPS-style look controls, wrong
	// for a point-and-click board game) - DefaultInput.ini's
	// DefaultViewportMouseCaptureMode=CapturePermanently_IncludingInitialMouseDown makes every
	// click re-trigger that capture. Use GameAndUI with locking explicitly off instead: plain
	// absolute desktop mouse coordinates, matching what's rendered and what hit-testing queries.
	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);

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
	UpdateDragFollow();
}

void ABoardPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	// Bound to both Pressed and DoubleClick defensively: Slate can reclassify a click landing
	// within DefaultInput.ini's DoubleClickTime of a previous one as DoubleClick instead of a
	// plain Pressed event, and dropping a carried actor is itself a click. Not the cause of the
	// re-pick bug that motivated this (that was the input mode, see BeginPlay) - kept because
	// it's a real, if narrower, Slate behavior worth not being tripped up by later.
	InputComponent->BindAction(TEXT("Select"), IE_Pressed, this, &ABoardPlayerController::OnSelectPressed);
	InputComponent->BindAction(TEXT("Select"), IE_DoubleClick, this, &ABoardPlayerController::OnSelectPressed);
	InputComponent->BindAction(TEXT("Select"), IE_Released, this, &ABoardPlayerController::OnSelectReleased);
	InputComponent->BindAction(TEXT("CancelDrag"), IE_Pressed, this, &ABoardPlayerController::OnCancelDrag);
}

bool ABoardPlayerController::DeprojectCursorToGroundPlane(FVector& OutHitPoint) const
{
	FVector RayOrigin, RayDirection;
	if (!DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		return false;
	}

	// Analytic intersection with the board's ground plane (Z=0, same plane every tile instance
	// sits on in HexGridVisualizer) - not a line trace, per the grid-is-data invariant.
	if (FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}
	const float T = -RayOrigin.Z / RayDirection.Z;
	if (T < 0.f)
	{
		return false;
	}

	OutHitPoint = RayOrigin + RayDirection * T;
	return true;
}

void ABoardPlayerController::UpdateHoveredHex()
{
	const bool bHadPreviousHex = bHasHoveredHex;
	const FHexCoord PreviousHex = HoveredHex;

	bHasHoveredHex = false;

	const UHexGrid* Grid = HexGrid.Get();
	FVector HitPoint;
	if (Grid && DeprojectCursorToGroundPlane(HitPoint))
	{
		const FHexCoord Coord = UHexCoordinateLibrary::World2DToAxial(FVector2D(HitPoint.X, HitPoint.Y), Grid->GetHexRadius());
		if (Grid->IsValidCoord(Coord))
		{
			bHasHoveredHex = true;
			HoveredHex = Coord;
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

void ABoardPlayerController::UpdateDragFollow()
{
	if (!DraggedActor.IsValid())
	{
		return;
	}

	FVector HitPoint;
	if (DeprojectCursorToGroundPlane(HitPoint))
	{
		// HitPoint.Z is always 0 (the ground plane) - keep the actor at its original height so a
		// pivot that isn't at ground level (e.g. a character's) doesn't sink during the drag.
		TeleportActor(DraggedActor.Get(), FVector(HitPoint.X, HitPoint.Y, DragOriginalLocation.Z));
	}
}

void ABoardPlayerController::OnSelectPressed()
{
	if (!DraggedActor.IsValid())
	{
		// Not carrying - a left-click press on a Draggable-tagged actor picks it up.
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit) && Hit.GetActor() && Hit.GetActor()->ActorHasTag(TEXT("Draggable")))
		{
			BeginDrag(Hit.GetActor());
		}
		return;
	}

	// Already carrying (from an earlier click-to-pick-up that stayed attached to the cursor -
	// see OnSelectReleased): a fresh press now is "click again to drop."
	EndDrag(/*bCancel=*/!bHasHoveredHex);
}

void ABoardPlayerController::OnSelectReleased()
{
	if (!DraggedActor.IsValid())
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	GetMousePosition(MouseX, MouseY);
	const float MovedPixels = FVector2D::Distance(FVector2D(MouseX, MouseY), PressStartMousePos);

	if (MovedPixels > DragThresholdPixels)
	{
		// Moved enough to count as a real drag - this release is the drop (dropped on a valid
		// hex snaps to it; dropped off-board cancels).
		EndDrag(/*bCancel=*/!bHasHoveredHex);
	}
	// Otherwise this was a click, not a drag: stay carried, following the cursor until a second
	// click drops it (OnSelectPressed, above).
}

void ABoardPlayerController::OnCancelDrag()
{
	if (DraggedActor.IsValid())
	{
		EndDrag(/*bCancel=*/true);
	}
}

void ABoardPlayerController::BeginDrag(AActor* Target)
{
	DraggedActor = Target;
	DragOriginalLocation = Target->GetActorLocation();
	GetMousePosition(PressStartMousePos.X, PressStartMousePos.Y);

	// A placeholder like the Paragon character has its own CharacterMovementComponent, which
	// simulates gravity every tick. Once we start driving its position directly via
	// SetActorLocation, that component fights us (a teleport reads as "now airborne," so it
	// starts falling from wherever we last placed it) - disable it so we're the only thing
	// moving this actor. Fine here since this is a position-driven placeholder, not a real
	// physics actor; real champions (task 4.2+) won't carry this component at all.
	if (UCharacterMovementComponent* Movement = Target->FindComponentByClass<UCharacterMovementComponent>())
	{
		Movement->DisableMovement();
	}
}

void ABoardPlayerController::EndDrag(bool bCancel)
{
	AActor* Actor = DraggedActor.Get();
	if (!Actor)
	{
		return;
	}

	if (bCancel)
	{
		TeleportActor(Actor, DragOriginalLocation);
	}
	else
	{
		const UHexGrid* Grid = HexGrid.Get();
		const float HexRadius = Grid ? Grid->GetHexRadius() : 0.f;
		const FVector2D SnapPos = UHexCoordinateLibrary::AxialToWorld2D(HoveredHex, HexRadius);
		TeleportActor(Actor, FVector(SnapPos.X, SnapPos.Y, DragOriginalLocation.Z));
	}

	DraggedActor = nullptr;
}

void ABoardPlayerController::TeleportActor(AActor* Actor, const FVector& NewLocation)
{
	Actor->SetActorLocation(NewLocation, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
}
