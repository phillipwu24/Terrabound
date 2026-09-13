// Copyright Epic Games, Inc. All Rights Reserved.

// The project's debug console commands, per PLAN.md 0.5. This file starts near-empty and
// every later phase adds one more FAutoConsoleCommand here — nothing is ever removed from it.
// FAutoConsoleCommand is self-registering and self-unregistering (module load/unload), so no
// startup or shutdown wiring is needed elsewhere.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexCoordinates.h"
#include "../Grid/HexTile.h"

namespace
{
	void DebugPing()
	{
		UE_LOG(LogTemp, Log, TEXT("Terrabound debug commands are online."));
	}

	void DebugGridDump(const TArray<FString>& Args, UWorld* World)
	{
		const UHexGrid* Grid = World ? World->GetSubsystem<UHexGrid>() : nullptr;
		if (!Grid)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugGridDump: no HexGrid subsystem for this world."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("HexGrid: %d tiles, %d placeable"), Grid->GetTileCount(), Grid->GetPlayerZoneTiles().Num());
	}

	void DebugTileState(const TArray<FString>& Args, UWorld* World)
	{
		const UHexGrid* Grid = World ? World->GetSubsystem<UHexGrid>() : nullptr;
		if (!Grid)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugTileState: no HexGrid subsystem for this world."));
			return;
		}

		int32 Q = 0, R = 0;
		if (Args.Num() < 2 || !LexTryParseString(Q, *Args[0]) || !LexTryParseString(R, *Args[1]))
		{
			UE_LOG(LogTemp, Error, TEXT("DebugTileState: usage: DebugTileState <q> <r>"));
			return;
		}

		const FHexCoord Coord(Q, R);
		const FHexTile* Tile = Grid->GetTile(Coord);
		if (!Tile)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugTileState: %s is not a valid coord."), *Coord.ToString());
			return;
		}

		UE_LOG(LogTemp, Display,
			TEXT("Tile %s: spawn=%d placeable=%d walkable=%d occupied=%d terrain=%d pathCost=%.2f"),
			*Tile->Coord.ToString(),
			Tile->bIsSpawn,
			Tile->bIsPlaceable,
			Tile->bIsWalkable,
			Tile->Occupant.IsValid(),
			Tile->Terrain.IsValid(),
			Tile->PathCost);
	}

	void DebugSetSpawnFlag(const TArray<FString>& Args, UWorld* World)
	{
		UHexGrid* Grid = World ? World->GetSubsystem<UHexGrid>() : nullptr;
		if (!Grid)
		{
			UE_LOG(LogTemp, Error, TEXT("SetSpawnFlag: no HexGrid subsystem for this world."));
			return;
		}

		int32 Q = 0, R = 0, Enabled = 0;
		if (Args.Num() < 3 || !LexTryParseString(Q, *Args[0]) || !LexTryParseString(R, *Args[1]) || !LexTryParseString(Enabled, *Args[2]))
		{
			UE_LOG(LogTemp, Error, TEXT("SetSpawnFlag: usage: SetSpawnFlag <q> <r> <0|1>"));
			return;
		}

		const FHexCoord Coord(Q, R);
		if (!Grid->SetSpawnFlag(Coord, Enabled != 0))
		{
			UE_LOG(LogTemp, Error, TEXT("SetSpawnFlag: %s is not a valid coord."), *Coord.ToString());
			return;
		}

		UE_LOG(LogTemp, Display, TEXT("SetSpawnFlag: %s spawn=%d"), *Coord.ToString(), Enabled != 0);
	}
}

static FAutoConsoleCommand DebugPingCommand(
	TEXT("DebugPing"),
	TEXT("Logs a line confirming the debug console command system is wired up."),
	FConsoleCommandDelegate::CreateStatic(&DebugPing)
);

static FAutoConsoleCommandWithWorldAndArgs DebugGridDumpCommand(
	TEXT("DebugGridDump"),
	TEXT("Logs the HexGrid's total tile count and placeable tile count."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugGridDump)
);

static FAutoConsoleCommandWithWorldAndArgs DebugTileStateCommand(
	TEXT("DebugTileState"),
	TEXT("DebugTileState <q> <r> - logs one tile's full state by axial coord."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugTileState)
);

static FAutoConsoleCommandWithWorldAndArgs SetSpawnFlagCommand(
	TEXT("SetSpawnFlag"),
	TEXT("SetSpawnFlag <q> <r> <0|1> - debug-only toggle of a tile's spawn flag."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSetSpawnFlag)
);
