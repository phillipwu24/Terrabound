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

private:
	void UpdateHoveredHex();
	void ApplyHoverVisual(bool bHadPreviousHex, const FHexCoord& PreviousHex, bool bHasNewHex, const FHexCoord& NewHex);

	TWeakObjectPtr<UHexGrid> HexGrid;
	TWeakObjectPtr<AHexGridVisualizer> Visualizer;

	bool bHasHoveredHex = false;
	FHexCoord HoveredHex;
};
