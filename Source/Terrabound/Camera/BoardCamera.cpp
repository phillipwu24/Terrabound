// Copyright Epic Games, Inc. All Rights Reserved.

#include "BoardCamera.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"

ABoardCamera::ABoardCamera()
{
	PrimaryActorTick.bCanEverTick = false;
	AutoReceiveInput = EAutoReceiveInput::Player0;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SetRootComponent(SpringArm);
	SpringArm->bDoCollisionTest = false;
	SpringArm->bEnableCameraLag = false;
	SpringArm->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
}

void ABoardCamera::BeginPlay()
{
	Super::BeginPlay();

	// SpringArm is the actor's own root component, so its RelativeRotation IS the actor's
	// world rotation - only override Pitch (the fixed, authored angle); Yaw (and Roll) stay
	// whatever the actor was rotated to in the editor. Overwriting the whole rotator here was
	// clobbering Yaw back to 0 every time, no matter what was set on the placed actor.
	FRotator Rotation = SpringArm->GetRelativeRotation();
	Rotation.Pitch = -PitchDegrees;
	SpringArm->SetRelativeRotation(Rotation);

	SpringArm->TargetArmLength = FMath::Clamp(DefaultZoomDistance, MinZoomDistance, MaxZoomDistance);

	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetViewTarget(this);
	}

	if (InputComponent)
	{
		InputComponent->BindAxis(TEXT("CameraZoom"), this, &ABoardCamera::OnZoom);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BoardCamera: no InputComponent - is AutoReceiveInput set and a PlayerController present?"));
	}
}

void ABoardCamera::OnZoom(float Value)
{
	if (FMath::IsNearlyZero(Value))
	{
		return;
	}

	const float NewLength = SpringArm->TargetArmLength - Value * ZoomStepSize;
	SpringArm->TargetArmLength = FMath::Clamp(NewLength, MinZoomDistance, MaxZoomDistance);
}
