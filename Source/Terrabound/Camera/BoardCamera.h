// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BoardCamera.generated.h"

class USpringArmComponent;
class UCameraComponent;

/**
 * Fixed-angle camera looking down at the board (PLAN.md 3.1), roughly TFT's framing. Zoom via
 * mouse wheel is the only player input; pitch is a fixed authored value, and there is no free
 * orbit. A plain actor, not a pawn: AutoReceiveInput + SetViewTarget make it the active camera
 * without a GameMode/PlayerController change. Placed and framed by eye in the editor.
 */
UCLASS()
class TERRABOUND_API ABoardCamera : public AActor
{
	GENERATED_BODY()

public:
	ABoardCamera();

protected:
	virtual void BeginPlay() override;

private:
	void OnZoom(float Value);

	UPROPERTY(VisibleAnywhere, Category = "Board Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Board Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Fixed downward pitch in degrees. Not player-adjustable — no free orbit. */
	UPROPERTY(EditAnywhere, Category = "Board Camera")
	float PitchDegrees = 55.f;

	UPROPERTY(EditAnywhere, Category = "Board Camera")
	float MinZoomDistance = 600.f;

	UPROPERTY(EditAnywhere, Category = "Board Camera")
	float MaxZoomDistance = 2500.f;

	UPROPERTY(EditAnywhere, Category = "Board Camera")
	float DefaultZoomDistance = 1500.f;

	/** World units the arm length changes per unit of mouse wheel input. */
	UPROPERTY(EditAnywhere, Category = "Board Camera")
	float ZoomStepSize = 100.f;
};
