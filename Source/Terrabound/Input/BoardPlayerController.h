// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "../Grid/HexCoordinates.h"
#include "BoardPlayerController.generated.h"

class UHexGrid;
class AHexGridVisualizer;

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

	/** Deprojects the mouse and intersects the board's ground plane (Z=0) analytically - not a
	 * line trace, per the grid-is-data invariant. False if the cursor doesn't hit the plane. */
	bool DeprojectCursorToGroundPlane(FVector& OutHitPoint) const;

	/** Moves the carried actor to follow the cursor - the only per-tick part of dragging left;
	 * press/release/cancel are all event-driven, bound in SetupInputComponent. */
	void UpdateDragFollow();

	void OnSelectPressed();
	void OnSelectReleased();
	void OnCancelDrag();

	void BeginDrag(AActor* Target);
	void EndDrag(bool bCancel);

	/** Repositions the dragged actor with an immediate physics/collision sync (TeleportPhysics),
	 * rather than letting a physics-enabled component (e.g. a skeletal mesh's physics asset)
	 * potentially lag a frame behind a plain SetActorLocation. Cheap defensive correctness, not
	 * a fix for anything currently reproduced - the actual re-pick bug turned out to be the
	 * input mode (see BeginPlay). */
	static void TeleportActor(AActor* Actor, const FVector& NewLocation);

	TWeakObjectPtr<UHexGrid> HexGrid;
	TWeakObjectPtr<AHexGridVisualizer> Visualizer;

	bool bHasHoveredHex = false;
	FHexCoord HoveredHex;

	// PLAN.md 3.4: any actor tagged "Draggable" can be picked up. No ABoardUnitBase/Occupant
	// interaction here - that needs task 4.2 and Phase 5's real placement rules. This is purely
	// the input mechanic, tested against a placeholder actor.
	TWeakObjectPtr<AActor> DraggedActor;
	FVector DragOriginalLocation = FVector::ZeroVector;

	// Screen-space mouse position when the actor was picked up, used on release to tell a real
	// drag from a click-to-pick-up: moved past DragThresholdPixels means "drop now," otherwise
	// the actor stays carried until a second click drops it.
	FVector2D PressStartMousePos = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Board Player Controller")
	float DragThresholdPixels = 10.f;
};
