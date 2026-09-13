// Copyright Epic Games, Inc. All Rights Reserved.

#include "TerraboundGameMode.h"
#include "Input/BoardPlayerController.h"

ATerraboundGameMode::ATerraboundGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ABoardPlayerController::StaticClass();
}
