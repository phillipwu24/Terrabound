// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "../Grid/HexCoordinates.h"
#include "BoardPlayerController.generated.h"

class UHexGrid;
class AHexGridVisualizer;
class UBench;
class ABenchVisualizer;
class ABoardUnitBase;
enum class EHexTileVisualState : uint8;

/**
 * Owns cursor-to-hex resolution (PLAN.md 3.2), the shared basis for 3.3's hover feedback and
 * 3.4's click-and-drag. Deprojects the mouse to a ray and intersects the board's ground plane
 * analytically - never a line trace against tile or unit actors, per CLAUDE.md's grid-is-data
 * invariant (there are no tile actors to trace against, and tracing units would couple input to
 * rendering).
 *
 * PLAN.md 6.4 extends the same drag state machine to a second kind of destination: bench slots
 * (ABenchVisualizer), resolved via the same ground-plane hit point rather than a UMG widget.
 * "Origin"/"destination" throughout are generalized as an EDragLocationKind (Hex or BenchSlot) -
 * see EndDrag for the single rule ("whatever's at Destination goes to Origin") that covers board
 * placement, swap, and all three bench transitions with no special-casing per direction.
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
	enum class EDragLocationKind : uint8
	{
		Hex,
		BenchSlot
	};

	void UpdateHoveredHex();
	void UpdateHoveredBenchSlot();
	void ApplyHoverVisual(bool bHadPreviousHex, const FHexCoord& PreviousHex, bool bHasNewHex, const FHexCoord& NewHex);

	/** ValidPlacement/InvalidPlacement per HexGrid::CanPlaceOrSwapAt - occupied-but-otherwise-legal
	 * hexes read as valid, since dropping there swaps rather than fails (PLAN.md 5.4). */
	EHexTileVisualState GetPlacementVisualState(const FHexCoord& Coord) const;

	/** ValidPlacementHovered/InvalidPlacementHovered - the "you are here" variant applied only to
	 * the hex under the cursor while dragging, layered on top of the board-wide preview. */
	EHexTileVisualState GetHoveredPlacementVisualState(const FHexCoord& Coord) const;

	/** Recolors every tile to its placement-validity state - the TFT-style "light up the legal
	 * zone" moment when a drag begins (PLAN.md 5.2). Hex-only; bench slots have no equivalent
	 * per-slot preview (PLAN.md 6.4 doesn't call for one - a slot always accepts a drop). */
	void ShowPlacementPreview();

	/** Reverts every tile to its resting PlayerZone/EnemyZone state, then reapplies plain Hovered
	 * to whatever's under the cursor so ordinary 3.3 hover feedback resumes immediately. */
	void ClearPlacementPreview();

	/** True if the current drop target (hovered hex or hovered bench slot) will accept the unit
	 * being dragged - gates the drop rather than only coloring it. A hex must pass
	 * CanPlaceOrSwapAt; a bench slot has no equivalent restriction and always accepts. */
	bool CanCommitDrop() const;

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

	/** Whoever currently occupies Kind/Coord/BenchSlot, or nullptr if it's empty/invalid. */
	ABoardUnitBase* GetOccupantAt(EDragLocationKind Kind, const FHexCoord& Coord, int32 BenchSlot) const;

	/** Commits Unit into Kind/Coord/BenchSlot: updates HexGrid or Bench occupancy and moves the
	 * actor there (SnapToHex for a hex, the bench visualizer's slot transform otherwise). */
	void PlaceUnitAt(ABoardUnitBase* Unit, EDragLocationKind Kind, const FHexCoord& Coord, int32 BenchSlot);

	/** Repositions the dragged unit with an immediate physics/collision sync (TeleportPhysics),
	 * rather than letting a physics-enabled component potentially lag a frame behind a plain
	 * SetActorLocation. Used only by UpdateDragFollow's per-tick cursor-following - landing at the
	 * end of a drag goes through PlaceUnitAt instead (see EndDrag). */
	static void TeleportActor(AActor* Actor, const FVector& NewLocation);

	TWeakObjectPtr<UHexGrid> HexGrid;
	TWeakObjectPtr<AHexGridVisualizer> Visualizer;
	TWeakObjectPtr<UBench> Bench;
	TWeakObjectPtr<ABenchVisualizer> BenchVisualizer;

	bool bHasHoveredHex = false;
	FHexCoord HoveredHex;

	bool bHasHoveredBenchSlot = false;
	int32 HoveredBenchSlot = INDEX_NONE;

	// PLAN.md 5.3/6.4: only a Player-team ABoardUnitBase can be picked up - checked by type in
	// OnSelectPressed, not by tag. The 3.4 placeholder actor is no longer draggable now that this
	// reads real board/bench state.
	TWeakObjectPtr<ABoardUnitBase> DraggedUnit;

	// Captured in BeginDrag, which also clears this origin's occupancy immediately - lifting a
	// unit frees its hex or bench slot right away (TFT-accurate), so dropping it back on itself
	// is an ordinary valid placement rather than a special case, and EndDrag lands here on a
	// cancel. Only one of DragOriginCoord/DragOriginBenchSlot is meaningful, per DragOriginKind.
	EDragLocationKind DragOriginKind = EDragLocationKind::Hex;
	FHexCoord DragOriginCoord;
	int32 DragOriginBenchSlot = INDEX_NONE;

	// Screen-space mouse position when the actor was picked up, used on release to tell a real
	// drag from a click-to-pick-up: moved past DragThresholdPixels means "drop now," otherwise
	// the actor stays carried until a second click drops it.
	FVector2D PressStartMousePos = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Board Player Controller")
	float DragThresholdPixels = 10.f;
};
