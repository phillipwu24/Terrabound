// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "TerraboundSettings.generated.h"

class UBoardConfig;

/**
 * Project-wide settings, editable from Project Settings without touching a level. Currently
 * just the one soft reference HexGrid needs to find DA_BoardConfig at generation time (1.4).
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Terrabound"))
class TERRABOUND_API UTerraboundSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, config, Category = "Board")
	TSoftObjectPtr<UBoardConfig> BoardConfig;
};
