// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoardPlayerController.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexGridVisualizer.h"
#include "../Economy/Bench.h"
#include "../Economy/BenchVisualizer.h"
#include "../Units/BoardUnitBase.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"

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
	Bench = GetWorld() ? GetWorld()->GetSubsystem<UBench>() : nullptr;

	if (UWorld* World = GetWorld())
	{
		if (TActorIterator<AHexGridVisualizer> It(World); It)
		{
			Visualizer = *It;
		}
		if (TActorIterator<ABenchVisualizer> BenchIt(World); BenchIt)
		{
			BenchVisualizer = *BenchIt;
		}
	}

	if (ShopWidgetClass)
	{
		if (UUserWidget* ShopWidget = CreateWidget<UUserWidget>(this, ShopWidgetClass))
		{
			ShopWidget->AddToViewport();
		}
	}
}

void ABoardPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	UpdateHoveredHex();
	UpdateHoveredBenchSlot();
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

void ABoardPlayerController::UpdateHoveredBenchSlot()
{
	bHasHoveredBenchSlot = false;
	HoveredBenchSlot = INDEX_NONE;

	const ABenchVisualizer* Vis = BenchVisualizer.Get();
	FVector HitPoint;
	if (Vis && DeprojectCursorToGroundPlane(HitPoint))
	{
		int32 SlotIndex = INDEX_NONE;
		if (Vis->FindNearestSlot(FVector2D(HitPoint.X, HitPoint.Y), SlotIndex))
		{
			bHasHoveredBenchSlot = true;
			HoveredBenchSlot = SlotIndex;
		}
	}
}

void ABoardPlayerController::ApplyHoverVisual(bool bHadPreviousHex, const FHexCoord& PreviousHex, bool bHasNewHex, const FHexCoord& NewHex)
{
	AHexGridVisualizer* Vis = Visualizer.Get();
	if (!Vis)
	{
		return;
	}

	if (DraggedUnit.IsValid())
	{
		// The board is already showing its board-wide placement-validity colors
		// (ShowPlacementPreview, PLAN.md 5.2) - drop the previously-hovered hex back to that plain
		// state and lift the newly-hovered one to its brighter "you are here" variant, instead of
		// the ordinary plain-Hovered swap below.
		if (bHadPreviousHex)
		{
			Vis->SetTileVisualState(PreviousHex, GetPlacementVisualState(PreviousHex));
		}
		if (bHasNewHex)
		{
			Vis->SetTileVisualState(NewHex, GetHoveredPlacementVisualState(NewHex));
		}
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

EHexTileVisualState ABoardPlayerController::GetPlacementVisualState(const FHexCoord& Coord) const
{
	const UHexGrid* Grid = HexGrid.Get();
	const bool bCanPlace = Grid && Grid->CanPlaceOrSwapAt(Coord);
	return bCanPlace ? EHexTileVisualState::ValidPlacement : EHexTileVisualState::InvalidPlacement;
}

EHexTileVisualState ABoardPlayerController::GetHoveredPlacementVisualState(const FHexCoord& Coord) const
{
	const UHexGrid* Grid = HexGrid.Get();
	const bool bCanPlace = Grid && Grid->CanPlaceOrSwapAt(Coord);
	return bCanPlace ? EHexTileVisualState::ValidPlacementHovered : EHexTileVisualState::InvalidPlacementHovered;
}

void ABoardPlayerController::ShowPlacementPreview()
{
	AHexGridVisualizer* Vis = Visualizer.Get();
	const UHexGrid* Grid = HexGrid.Get();
	if (!Vis || !Grid)
	{
		return;
	}

	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		Vis->SetTileVisualState(Coord, GetPlacementVisualState(Coord));
	}
}

void ABoardPlayerController::ClearPlacementPreview()
{
	AHexGridVisualizer* Vis = Visualizer.Get();
	const UHexGrid* Grid = HexGrid.Get();
	if (!Vis || !Grid)
	{
		return;
	}

	for (const FHexCoord& Coord : Grid->GetAllTileCoords())
	{
		Vis->SetTileVisualState(Coord, Vis->GetBaseVisualState(Coord));
	}

	// Resume ordinary hover feedback immediately rather than waiting for the next hex change.
	if (bHasHoveredHex)
	{
		Vis->SetTileVisualState(HoveredHex, EHexTileVisualState::Hovered);
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

bool ABoardPlayerController::CanCommitDrop() const
{
	if (bHasHoveredHex)
	{
		const UHexGrid* Grid = HexGrid.Get();
		return Grid && Grid->CanPlaceOrSwapAt(HoveredHex);
	}
	// A bench slot has no zone/occupancy restriction - it always accepts a drop, whether that
	// lands as a plain placement or a swap with whoever's already there (see EndDrag).
	return bHasHoveredBenchSlot;
}

void ABoardPlayerController::UpdateDragFollow()
{
	if (!DraggedUnit.IsValid())
	{
		return;
	}

	FVector HitPoint;
	if (DeprojectCursorToGroundPlane(HitPoint))
	{
		// Board units always sit at Z=0 (ABoardUnitBase::SnapToHex) - no need to preserve a
		// captured height.
		TeleportActor(DraggedUnit.Get(), FVector(HitPoint.X, HitPoint.Y, 0.f));
	}
}

void ABoardPlayerController::OnSelectPressed()
{
	if (!DraggedUnit.IsValid())
	{
		// Not carrying - a left-click press on a Player-team board unit picks it up, wherever it
		// currently stands (a hex or a bench slot - HitBox pickup detection doesn't care which).
		FHitResult Hit;
		if (GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			if (ABoardUnitBase* Unit = Cast<ABoardUnitBase>(Hit.GetActor()))
			{
				if (Unit->GetTeam() == EBoardUnitTeam::Player)
				{
					BeginDrag(Unit);
				}
			}
		}
		return;
	}

	// Already carrying (from an earlier click-to-pick-up that stayed attached to the cursor -
	// see OnSelectReleased): a fresh press now is "click again to drop."
	EndDrag(/*bCancel=*/!CanCommitDrop());
}

void ABoardPlayerController::OnSelectReleased()
{
	if (!DraggedUnit.IsValid())
	{
		return;
	}

	float MouseX = 0.f, MouseY = 0.f;
	GetMousePosition(MouseX, MouseY);
	const float MovedPixels = FVector2D::Distance(FVector2D(MouseX, MouseY), PressStartMousePos);

	if (MovedPixels > DragThresholdPixels)
	{
		// Moved enough to count as a real drag - this release is the drop (dropped on a legal
		// target commits; dropped off-board and off-bench cancels).
		EndDrag(/*bCancel=*/!CanCommitDrop());
	}
	// Otherwise this was a click, not a drag: stay carried, following the cursor until a second
	// click drops it (OnSelectPressed, above).
}

void ABoardPlayerController::OnCancelDrag()
{
	if (DraggedUnit.IsValid())
	{
		EndDrag(/*bCancel=*/true);
	}
}

void ABoardPlayerController::BeginDrag(ABoardUnitBase* Unit)
{
	DraggedUnit = Unit;
	GetMousePosition(PressStartMousePos.X, PressStartMousePos.Y);

	// Lifting a unit frees its origin immediately (TFT-accurate) - this is also what lets
	// dropping it back on itself take the ordinary commit path in EndDrag rather than a special
	// case. A unit currently on the bench is found there first; anything else is assumed to be on
	// the board, per its own CurrentCoord (same trust level the pre-6.4 code already had here).
	UBench* BenchSub = Bench.Get();
	const int32 BenchSlot = BenchSub ? BenchSub->FindSlotIndex(Unit) : INDEX_NONE;
	if (BenchSlot != INDEX_NONE)
	{
		DragOriginKind = EDragLocationKind::BenchSlot;
		DragOriginBenchSlot = BenchSlot;
		BenchSub->SetSlot(BenchSlot, nullptr);
	}
	else
	{
		DragOriginKind = EDragLocationKind::Hex;
		DragOriginCoord = Unit->GetCurrentCoord();
		if (UHexGrid* Grid = HexGrid.Get())
		{
			Grid->ClearOccupant(DragOriginCoord);
		}
	}

	ShowPlacementPreview();

	// Paint the cursor's starting hex with its "you are here" variant immediately - otherwise it
	// would show plain ValidPlacement/InvalidPlacement until the cursor next moves to a new hex.
	if (AHexGridVisualizer* Vis = bHasHoveredHex ? Visualizer.Get() : nullptr)
	{
		Vis->SetTileVisualState(HoveredHex, GetHoveredPlacementVisualState(HoveredHex));
	}
}

void ABoardPlayerController::EndDrag(bool bCancel)
{
	ABoardUnitBase* Unit = DraggedUnit.Get();
	if (!Unit)
	{
		return;
	}

	// A cancel lands back on wherever it was lifted from; a commit lands on whichever destination
	// the cursor is over (CanCommitDrop has already confirmed it'll accept the drop).
	EDragLocationKind DestKind = DragOriginKind;
	FHexCoord DestCoord = DragOriginCoord;
	int32 DestBenchSlot = DragOriginBenchSlot;
	if (!bCancel)
	{
		if (bHasHoveredHex)
		{
			DestKind = EDragLocationKind::Hex;
			DestCoord = HoveredHex;
		}
		else if (bHasHoveredBenchSlot)
		{
			DestKind = EDragLocationKind::BenchSlot;
			DestBenchSlot = HoveredBenchSlot;
		}
	}

	// Whoever currently occupies Destination goes back to Origin - one rule covering board
	// placement, board<->board swap, and all three bench transitions. BeginDrag already vacated
	// the origin, so on a cancel (Destination == Origin) this always finds nobody there.
	if (ABoardUnitBase* Bumped = GetOccupantAt(DestKind, DestCoord, DestBenchSlot))
	{
		PlaceUnitAt(Bumped, DragOriginKind, DragOriginCoord, DragOriginBenchSlot);
	}
	PlaceUnitAt(Unit, DestKind, DestCoord, DestBenchSlot);

	DraggedUnit = nullptr;
	ClearPlacementPreview();
}

ABoardUnitBase* ABoardPlayerController::GetOccupantAt(EDragLocationKind Kind, const FHexCoord& Coord, int32 BenchSlot) const
{
	if (Kind == EDragLocationKind::Hex)
	{
		const UHexGrid* Grid = HexGrid.Get();
		const FHexTile* Tile = Grid ? Grid->GetTile(Coord) : nullptr;
		return Tile ? Tile->Occupant.Get() : nullptr;
	}

	const UBench* BenchSub = Bench.Get();
	return BenchSub ? BenchSub->GetChampionAt(BenchSlot) : nullptr;
}

void ABoardPlayerController::PlaceUnitAt(ABoardUnitBase* Unit, EDragLocationKind Kind, const FHexCoord& Coord, int32 BenchSlot)
{
	if (Kind == EDragLocationKind::Hex)
	{
		if (UHexGrid* Grid = HexGrid.Get())
		{
			Grid->SetOccupant(Coord, Unit);
		}
		Unit->SnapToHex(Coord);
		return;
	}

	if (UBench* BenchSub = Bench.Get())
	{
		BenchSub->SetSlot(BenchSlot, Unit);
	}
	if (const ABenchVisualizer* Vis = BenchVisualizer.Get())
	{
		// Location only, like SnapToHex - a unit's facing is set once (ChampionBase's "face the
		// enemy side") and never touched by repositioning, on the board or the bench.
		Unit->SetActorLocation(Vis->GetSlotTransform(BenchSlot).GetLocation());
	}
}

void ABoardPlayerController::TeleportActor(AActor* Actor, const FVector& NewLocation)
{
	Actor->SetActorLocation(NewLocation, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
}
