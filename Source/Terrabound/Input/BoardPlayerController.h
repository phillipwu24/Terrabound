// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "../Grid/HexCoordinates.h"
#include "BoardPlayerController.generated.h"

class UHexGrid;
class AHexGridVisualizer;
class ABoardUnitBase;
enum class EHexTileVisualState : uint8;

/**
 * Owns cursor-to-hex resolution (PLAN.md 3.2), the shared basis for 3.3's hover feedback and
 * 3.4's click-and-drag. Deprojects the mouse to a ray and intersects the board's ground plane
 * analytically - never a line trace against tile or unit actors, per CLAUDE.md's grid-is-data
 * invariant (there are no tile actors to trace against, and tracing units would couple input to
 * rendering).
 */
UCLASS()
class TERRABOUND_API ABoardPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** True and fills OutCoord if the mouse currently hovers a valid on-board tile. */
	bool GetHoveredHex(FHexCoord& OutCoord) const;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

private:
	void UpdateHoveredHex();
	void ApplyHoverVisual(bool bHadPreviousHex, const FHexCoord& PreviousHex, bool bHasNewHex, const FHexCoord& NewHex);

	/** ValidPlacement/InvalidPlacement per HexGrid::CanPlaceAt (PLAN.md 5.1). */
	EHexTileVisualState GetPlacementVisualState(const FHexCoord& Coord) const;

	/** ValidPlacementHovered/InvalidPlacementHovered - the "you are here" variant applied only to
	 * the hex under the cursor while dragging, layered on top of the board-wide preview. */
	EHexTileVisualState GetHoveredPlacementVisualState(const FHexCoord& Coord) const;

	/** Recolors every tile to its placement-validity state - the TFT-style "light up the legal
	 * zone" moment when a drag begins (PLAN.md 5.2). */
	void ShowPlacementPreview();

	/** Reverts every tile to its resting PlayerZone/EnemyZone state, then reapplies plain Hovered
	 * to whatever's under the cursor so ordinary 3.3 hover feedback resumes immediately. */
	void ClearPlacementPreview();

	/** True if the hovered hex is a legal drop target for the unit currently being dragged - the
	 * board-level CanPlaceAt check, gating the drop rather than only coloring it (PLAN.md 5.3). */
	bool CanDropOnHoveredHex() const;

	/** Deprojects the mouse and intersects the board's ground plane (Z=0) analytically - not a
	 * line trace, per the grid-is-data invariant. False if the cursor doesn't hit the plane. */
	bool DeprojectCursorToGroundPlane(FVector& OutHitPoint) const;

	/** Moves the carried unit to follow the cursor - the only per-tick part of dragging left;
	 * press/release/cancel are all event-driven, bound in SetupInputComponent. */
	void UpdateDragFollow();

	void OnSelectPressed();
	void OnSelectReleased();
	void OnCancelDrag();

	void BeginDrag(ABoardUnitBase* Unit);
	void EndDrag(bool bCancel);

	/** Repositions the dragged unit with an immediate physics/collision sync (TeleportPhysics),
	 * rather than letting a physics-enabled component potentially lag a frame behind a plain
	 * SetActorLocation. Used only by UpdateDragFollow's per-tick cursor-following - landing at the
	 * end of a drag goes through ABoardUnitBase::SnapToHex instead (see EndDrag). */
	static void TeleportActor(AActor* Actor, const FVector& NewLocation);

	TWeakObjectPtr<UHexGrid> HexGrid;
	TWeakObjectPtr<AHexGridVisualizer> Visualizer;

	bool bHasHoveredHex = false;
	FHexCoord HoveredHex;

	// PLAN.md 5.3: only a Player-team ABoardUnitBase can be picked up - checked by type in
	// OnSelectPressed, not by tag. The 3.4 placeholder actor is no longer draggable now that this
	// reads real board state.
	TWeakObjectPtr<ABoardUnitBase> DraggedUnit;

	// Captured in BeginDrag, which also clears this tile's Occupant immediately - lifting a unit
	// frees its hex right away (TFT-accurate), so dropping it back on itself is an ordinary valid
	// placement rather than a special case, and EndDrag lands here on a cancel.
	FHexCoord DragOriginCoord;

	// Screen-space mouse position when the actor was picked up, used on release to tell a real
	// drag from a click-to-pick-up: moved past DragThresholdPixels means "drop now," otherwise
	// the actor stays carried until a second click drops it.
	FVector2D PressStartMousePos = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Board Player Controller")
	float DragThresholdPixels = 10.f;
};
