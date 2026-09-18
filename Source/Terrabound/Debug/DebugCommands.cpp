// Copyright Epic Games, Inc. All Rights Reserved.

// The project's debug console commands, per PLAN.md 0.5. This file starts near-empty and
// every later phase adds one more FAutoConsoleCommand here — nothing is ever removed from it.
// FAutoConsoleCommand is self-registering and self-unregistering (module load/unload), so no
// startup or shutdown wiring is needed elsewhere.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "../Grid/HexGrid.h"
#include "../Grid/HexCoordinates.h"
#include "../Grid/HexTile.h"
#include "../Grid/HexGridVisualizer.h"
#include "../Units/ChampionBase.h"
#include "../Data/ChampionData.h"
#include "../Economy/EconomyState.h"

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

	void DebugSpawnChampion(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnChampion: no world."));
			return;
		}

		int32 Q = 0, R = 0;
		if (Args.Num() < 3 || !LexTryParseString(Q, *Args[1]) || !LexTryParseString(R, *Args[2]))
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnChampion: usage: SpawnChampion <DataAssetName> <q> <r>"));
			return;
		}

		UHexGrid* Grid = World->GetSubsystem<UHexGrid>();
		if (!Grid)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnChampion: no HexGrid subsystem for this world."));
			return;
		}

		const FHexCoord Coord(Q, R);
		if (!Grid->IsValidCoord(Coord))
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnChampion: %s is not a valid coord."), *Coord.ToString());
			return;
		}

		// Debug only - bypasses shop, bench, and Phase 5's placement validation entirely. Never
		// reuse this path for the shop's buy flow (PLAN.md 4.5).
		const FString& AssetName = Args[0];
		const FString AssetPath = FString::Printf(TEXT("/Game/Terrabound/Data/Champions/%s.%s"), *AssetName, *AssetName);
		UChampionData* Data = LoadObject<UChampionData>(nullptr, *AssetPath);
		if (!Data)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnChampion: couldn't load ChampionData '%s' at %s."), *AssetName, *AssetPath);
			return;
		}

		AChampionBase* Champion = World->SpawnActor<AChampionBase>();
		if (!Champion)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnChampion: failed to spawn AChampionBase."));
			return;
		}

		Champion->InitializeFromChampionData(Data);
		Champion->SnapToHex(Coord);
		Grid->SetOccupant(Coord, Champion);

		UE_LOG(LogTemp, Display, TEXT("SpawnChampion: spawned '%s' at %s."), *AssetName, *Coord.ToString());
	}

	void DebugOccupancy(const TArray<FString>& Args, UWorld* World)
	{
		const UHexGrid* Grid = World ? World->GetSubsystem<UHexGrid>() : nullptr;
		if (!Grid)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugOccupancy: no HexGrid subsystem for this world."));
			return;
		}

		int32 Count = 0;
		for (const FHexCoord& Coord : Grid->GetAllTileCoords())
		{
			const FHexTile* Tile = Grid->GetTile(Coord);
			ABoardUnitBase* Occupant = Tile ? Tile->Occupant.Get() : nullptr;
			if (!Occupant)
			{
				continue;
			}

			// Prefer the champion's authored display name; fall back to the actor's own name for
			// any other ABoardUnitBase (e.g. a future EnemyBase, which has no ChampionData).
			FString Label = Occupant->GetName();
			if (const AChampionBase* Champion = Cast<AChampionBase>(Occupant))
			{
				if (const UChampionData* Data = Champion->GetChampionData())
				{
					Label = Data->DisplayName.ToString();
				}
			}

			UE_LOG(LogTemp, Display, TEXT("Occupied %s: %s"), *Coord.ToString(), *Label);
			++Count;
		}
		UE_LOG(LogTemp, Display, TEXT("DebugOccupancy: %d occupied tile(s)."), Count);
	}

	void DebugGold(const TArray<FString>& Args, UWorld* World)
	{
		const UEconomyState* Economy = World ? World->GetSubsystem<UEconomyState>() : nullptr;
		if (!Economy)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugGold: no EconomyState subsystem for this world."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("Gold: %d"), Economy->GetGold());
	}

	void DebugCoordOverlay(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugCoordOverlay: no world."));
			return;
		}

		int32 Count = 0;
		for (TActorIterator<AHexGridVisualizer> It(World); It; ++It)
		{
			It->ToggleCoordOverlay();
			++Count;
		}
		UE_LOG(LogTemp, Display, TEXT("DebugCoordOverlay: toggled on %d HexGridVisualizer actor(s)."), Count);
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

static FAutoConsoleCommandWithWorldAndArgs SpawnChampionCommand(
	TEXT("SpawnChampion"),
	TEXT("SpawnChampion <DataAssetName> <q> <r> - debug-only: spawns a champion directly onto a hex, bypassing the shop and bench."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSpawnChampion)
);

static FAutoConsoleCommandWithWorldAndArgs DebugOccupancyCommand(
	TEXT("DebugOccupancy"),
	TEXT("Lists every occupied hex and its occupant."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugOccupancy)
);

static FAutoConsoleCommandWithWorldAndArgs DebugGoldCommand(
	TEXT("DebugGold"),
	TEXT("Logs the player's current gold."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugGold)
);

static FAutoConsoleCommandWithWorldAndArgs DebugCoordOverlayCommand(
	TEXT("DebugCoordOverlay"),
	TEXT("Toggles the (q, r) coordinate text overlay on all HexGridVisualizer actors."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugCoordOverlay)
);
