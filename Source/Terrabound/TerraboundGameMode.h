// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TerraboundGameMode.generated.h"

/**
 * The project's one GameMode: no player-controlled pawn yet (Checkpoint 1 has no champion the
 * player possesses - the camera is a plain actor set via SetViewTarget, see BoardCamera), and
 * ABoardPlayerController owns cursor-to-hex input. Set as GlobalDefaultGameMode in
 * DefaultEngine.ini so every level gets this without a per-level World Settings override.
 */
UCLASS()
class TERRABOUND_API ATerraboundGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATerraboundGameMode();
};
