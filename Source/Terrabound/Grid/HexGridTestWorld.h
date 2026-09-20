// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HexGrid.h"

namespace HexGridTests
{
	/**
	 * Scoped throwaway UWorld so a test can get a live, populated UHexGrid subsystem - a
	 * WorldSubsystem only exists alongside a UWorld, so anything past pure coordinate math (CanPlaceAt,
	 * the pathfinder) needs a real generated grid. Shared by HexGridPlacementTests.cpp and
	 * HexPathfinderTests.cpp; the grid is generated from whatever DA_BoardConfig Project Settings points at.
	 */
	class FScopedGridWorld
	{
	public:
		FScopedGridWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			WorldContext = &GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext->SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FScopedGridWorld()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
		}

		UHexGrid* GetGrid() const { return World->GetSubsystem<UHexGrid>(); }
		UWorld* GetWorld() const { return World; }

	private:
		UWorld* World = nullptr;
		FWorldContext* WorldContext = nullptr;
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
