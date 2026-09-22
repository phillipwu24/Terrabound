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
#include "../Units/EnemyBase.h"
#include "../Data/ChampionData.h"
#include "../Data/EnemyData.h"
#include "../Economy/EconomyState.h"
#include "../Economy/ChampionPool.h"
#include "../Economy/Bench.h"
#include "../Economy/BenchVisualizer.h"
#include "../Economy/ChampionMerger.h"
#include "../Economy/ShopSystem.h"
#include "DebugPathWalker.h"

namespace
{
	// PLACEHOLDER, debug-only default, picked by eye in 7.1. UEnemyData::MoveSpeedHexesPerSecond has
	// since taken over the real value (seeded from this); goes away with the walker in PLAN.md 1.7.
	constexpr float DefaultWalkerHexesPerSecond = 2.f;

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

	void DebugSpawnPathWalker(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnPathWalker: no world."));
			return;
		}

		int32 Q = 0, R = 0;
		float HexesPerSecond = DefaultWalkerHexesPerSecond;
		const bool bParsedCoord = Args.Num() >= 2 && LexTryParseString(Q, *Args[0]) && LexTryParseString(R, *Args[1]);
		const bool bParsedSpeed = Args.Num() < 3 || LexTryParseString(HexesPerSecond, *Args[2]);
		if (!bParsedCoord || !bParsedSpeed || HexesPerSecond <= 0.f)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnPathWalker: usage: SpawnPathWalker <q> <r> [hexesPerSecond > 0]"));
			return;
		}

		const UHexGrid* Grid = World->GetSubsystem<UHexGrid>();
		if (!Grid)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnPathWalker: no HexGrid subsystem for this world."));
			return;
		}

		const FHexCoord Coord(Q, R);
		const FHexTile* Tile = Grid->GetTile(Coord);
		if (!Tile || !Tile->bIsSpawn)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnPathWalker: %s is not a spawn-flagged tile (see SetSpawnFlag)."), *Coord.ToString());
			return;
		}

		ADebugPathWalker* Walker = World->SpawnActor<ADebugPathWalker>();
		if (!Walker)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnPathWalker: failed to spawn ADebugPathWalker."));
			return;
		}

		Walker->StartWalking(Coord, HexesPerSecond);
		UE_LOG(LogTemp, Display, TEXT("SpawnPathWalker: walking from %s at %.2f hexes/s."), *Coord.ToString(), HexesPerSecond);
	}

	void DebugSpawnEnemy(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: no world."));
			return;
		}

		int32 Q = 0, R = 0;
		if (Args.Num() < 3 || !LexTryParseString(Q, *Args[1]) || !LexTryParseString(R, *Args[2]))
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: usage: SpawnEnemy <DataAssetName> <q> <r>"));
			return;
		}

		UHexGrid* Grid = World->GetSubsystem<UHexGrid>();
		if (!Grid)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: no HexGrid subsystem for this world."));
			return;
		}

		const FHexCoord Coord(Q, R);
		const FHexTile* Tile = Grid->GetTile(Coord);
		if (!Tile || !Tile->bIsSpawn)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: %s is not a spawn-flagged tile (see SetSpawnFlag)."), *Coord.ToString());
			return;
		}
		if (Tile->Occupant.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: %s is occupied."), *Coord.ToString());
			return;
		}

		const FString& AssetName = Args[0];
		const FString AssetPath = FString::Printf(TEXT("/Game/Terrabound/Data/Enemies/%s.%s"), *AssetName, *AssetName);
		UEnemyData* Data = LoadObject<UEnemyData>(nullptr, *AssetPath);
		if (!Data)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: couldn't load EnemyData '%s' at %s."), *AssetName, *AssetPath);
			return;
		}

		AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>();
		if (!Enemy)
		{
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: failed to spawn AEnemyBase."));
			return;
		}

		Enemy->InitializeFromEnemyData(Data);
		if (!Enemy->EnterBoard(Coord))
		{
			Enemy->Destroy();
			UE_LOG(LogTemp, Error, TEXT("SpawnEnemy: '%s' could not enter the board at %s."), *AssetName, *Coord.ToString());
			return;
		}

		UE_LOG(LogTemp, Display, TEXT("SpawnEnemy: spawned '%s' at %s."), *AssetName, *Coord.ToString());
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
			// any other ABoardUnitBase (e.g. an AEnemyBase, which has no ChampionData).
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

	void DebugRollChampionPool(const TArray<FString>& Args, UWorld* World)
	{
		int32 SampleCount = 10000;
		if (Args.Num() >= 1)
		{
			LexTryParseString(SampleCount, *Args[0]);
		}

		UDataTable* PoolTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Terrabound/Data/DT_ChampionPool.DT_ChampionPool"));
		UDataTable* OddsTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Terrabound/Data/DT_ChampionTierOdds.DT_ChampionTierOdds"));
		if (!PoolTable || !OddsTable)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugRollChampionPool: couldn't load DT_ChampionPool and/or DT_ChampionTierOdds."));
			return;
		}

		// Prints each roster row's champion and the Tier ChampionData actually resolves to, so a
		// skewed distribution can be traced to a specific asset rather than the odds table.
		TArray<FChampionPoolRow*> DebugRows;
		PoolTable->GetAllRows<FChampionPoolRow>(TEXT("DebugRollChampionPool"), DebugRows);
		for (const FChampionPoolRow* Row : DebugRows)
		{
			const UChampionData* Data = Row->ChampionData.LoadSynchronous();
			UE_LOG(LogTemp, Display, TEXT("  Roster: %s tier=%d poolSize=%d"),
				Data ? *Data->DisplayName.ToString() : TEXT("<unresolved>"),
				Data ? Data->Tier : -1,
				Row->PoolSize);
		}

		// A throwaway pool for sampling only - ShopSystem (6.3) will own the real one. Each draw is
		// immediately returned so the sample measures configured RollOdds, not pool depletion.
		UChampionPool* Pool = NewObject<UChampionPool>();
		Pool->Initialize(PoolTable, OddsTable);

		TMap<int32, int32> TierCounts;
		int32 Drawn = 0;
		for (int32 i = 0; i < SampleCount; ++i)
		{
			UChampionData* Champion = Pool->DrawRandomChampion();
			if (!Champion)
			{
				continue;
			}
			++Drawn;
			TierCounts.FindOrAdd(Champion->Tier)++;
			Pool->ReturnChampion(Champion);
		}

		UE_LOG(LogTemp, Display, TEXT("DebugRollChampionPool: %d/%d draws succeeded."), Drawn, SampleCount);
		TArray<int32> Tiers;
		TierCounts.GetKeys(Tiers);
		Tiers.Sort();
		for (int32 Tier : Tiers)
		{
			const int32 Count = TierCounts[Tier];
			UE_LOG(LogTemp, Display, TEXT("  Tier %d: %d (%.1f%%)"), Tier, Count, Drawn > 0 ? 100.f * Count / Drawn : 0.f);
		}
	}

	void DebugBenchState(const TArray<FString>& Args, UWorld* World)
	{
		const UBench* Bench = World ? World->GetSubsystem<UBench>() : nullptr;
		if (!Bench)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugBenchState: no Bench subsystem for this world."));
			return;
		}

		for (int32 SlotIndex = 0; SlotIndex < Bench->GetSlotCount(); ++SlotIndex)
		{
			const ABoardUnitBase* Occupant = Bench->GetChampionAt(SlotIndex);
			FString Label = TEXT("<empty>");
			if (Occupant)
			{
				Label = Occupant->GetName();
				if (const AChampionBase* Champion = Cast<AChampionBase>(Occupant))
				{
					if (const UChampionData* Data = Champion->GetChampionData())
					{
						Label = Data->DisplayName.ToString();
					}
				}
			}
			UE_LOG(LogTemp, Display, TEXT("Bench slot %d: %s"), SlotIndex, *Label);
		}
	}

	void DebugBenchAdd(const TArray<FString>& Args, UWorld* World)
	{
		if (!World)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugBenchAdd: no world."));
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugBenchAdd: usage: DebugBenchAdd <DataAssetName>"));
			return;
		}

		UBench* Bench = World->GetSubsystem<UBench>();
		if (!Bench)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugBenchAdd: no Bench subsystem for this world."));
			return;
		}

		// Debug only - bypasses the shop entirely, same spirit as SpawnChampion (PLAN.md 4.5).
		const FString& AssetName = Args[0];
		const FString AssetPath = FString::Printf(TEXT("/Game/Terrabound/Data/Champions/%s.%s"), *AssetName, *AssetName);
		UChampionData* Data = LoadObject<UChampionData>(nullptr, *AssetPath);
		if (!Data)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugBenchAdd: couldn't load ChampionData '%s' at %s."), *AssetName, *AssetPath);
			return;
		}

		// Same rule as a purchase: a full bench still takes a copy that completes a merge.
		UChampionMerger* Merger = World->GetSubsystem<UChampionMerger>();
		if (!Bench->HasFreeSlot() && !(Merger && Merger->WouldCompleteMerge(Data)))
		{
			UE_LOG(LogTemp, Error, TEXT("DebugBenchAdd: bench is full."));
			return;
		}

		AChampionBase* Champion = World->SpawnActor<AChampionBase>();
		if (!Champion)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugBenchAdd: failed to spawn AChampionBase."));
			return;
		}
		Champion->InitializeFromChampionData(Data);

		if (Merger && Merger->TryMerge(Champion))
		{
			UE_LOG(LogTemp, Display, TEXT("DebugBenchAdd: '%s' completed a merge."), *AssetName);
			return;
		}
		Bench->AddChampion(Champion);

		if (TActorIterator<ABenchVisualizer> It(World); It)
		{
			const int32 SlotIndex = Bench->FindSlotIndex(Champion);
			Champion->SetActorLocation(It->GetSlotTransform(SlotIndex).GetLocation());
		}

		UE_LOG(LogTemp, Display, TEXT("DebugBenchAdd: added '%s' to the bench."), *AssetName);
	}

	void DebugShopDump(const TArray<FString>& Args, UWorld* World)
	{
		const UShopSystem* Shop = World ? World->GetSubsystem<UShopSystem>() : nullptr;
		if (!Shop)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugShopDump: no ShopSystem subsystem for this world."));
			return;
		}

		for (int32 SlotIndex = 0; SlotIndex < Shop->GetSlotCount(); ++SlotIndex)
		{
			const UChampionData* Data = Shop->GetChampionAt(SlotIndex);
			UE_LOG(LogTemp, Display, TEXT("Shop slot %d: %s"), SlotIndex,
				Data ? *Data->DisplayName.ToString() : TEXT("<empty>"));
		}
	}

	void DebugShopReroll(const TArray<FString>& Args, UWorld* World)
	{
		UShopSystem* Shop = World ? World->GetSubsystem<UShopSystem>() : nullptr;
		if (!Shop)
		{
			UE_LOG(LogTemp, Error, TEXT("ShopReroll: no ShopSystem subsystem for this world."));
			return;
		}

		if (!Shop->Reroll())
		{
			UE_LOG(LogTemp, Error, TEXT("ShopReroll: reroll failed (insufficient gold?)."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("ShopReroll: rerolled."));
	}

	void DebugShopBuy(const TArray<FString>& Args, UWorld* World)
	{
		UShopSystem* Shop = World ? World->GetSubsystem<UShopSystem>() : nullptr;
		if (!Shop)
		{
			UE_LOG(LogTemp, Error, TEXT("ShopBuy: no ShopSystem subsystem for this world."));
			return;
		}

		int32 SlotIndex = 0;
		if (Args.Num() < 1 || !LexTryParseString(SlotIndex, *Args[0]))
		{
			UE_LOG(LogTemp, Error, TEXT("ShopBuy: usage: ShopBuy <slotIndex>"));
			return;
		}

		if (!Shop->Buy(SlotIndex))
		{
			UE_LOG(LogTemp, Error, TEXT("ShopBuy: buy failed (empty slot, insufficient gold, or full bench)."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("ShopBuy: bought slot %d."), SlotIndex);
	}

	void DebugSellHex(const TArray<FString>& Args, UWorld* World)
	{
		UHexGrid* Grid = World ? World->GetSubsystem<UHexGrid>() : nullptr;
		UShopSystem* Shop = World ? World->GetSubsystem<UShopSystem>() : nullptr;
		if (!Grid || !Shop)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellHex: no HexGrid/ShopSystem subsystem for this world."));
			return;
		}

		int32 Q = 0, R = 0;
		if (Args.Num() < 2 || !LexTryParseString(Q, *Args[0]) || !LexTryParseString(R, *Args[1]))
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellHex: usage: DebugSellHex <q> <r>"));
			return;
		}

		const FHexCoord Coord(Q, R);
		const FHexTile* Tile = Grid->GetTile(Coord);
		ABoardUnitBase* Occupant = Tile ? Tile->Occupant.Get() : nullptr;
		if (!Occupant)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellHex: %s has no occupant."), *Coord.ToString());
			return;
		}

		Grid->ClearOccupant(Coord);
		if (!Shop->Sell(Occupant))
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellHex: sell failed (not a champion?)."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("DebugSellHex: sold occupant of %s."), *Coord.ToString());
	}

	void DebugSellBench(const TArray<FString>& Args, UWorld* World)
	{
		UBench* Bench = World ? World->GetSubsystem<UBench>() : nullptr;
		UShopSystem* Shop = World ? World->GetSubsystem<UShopSystem>() : nullptr;
		if (!Bench || !Shop)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellBench: no Bench/ShopSystem subsystem for this world."));
			return;
		}

		int32 SlotIndex = 0;
		if (Args.Num() < 1 || !LexTryParseString(SlotIndex, *Args[0]))
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellBench: usage: DebugSellBench <slotIndex>"));
			return;
		}

		ABoardUnitBase* Occupant = Bench->GetChampionAt(SlotIndex);
		if (!Occupant)
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellBench: slot %d has no occupant."), SlotIndex);
			return;
		}

		Bench->SetSlot(SlotIndex, nullptr);
		if (!Shop->Sell(Occupant))
		{
			UE_LOG(LogTemp, Error, TEXT("DebugSellBench: sell failed (not a champion?)."));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("DebugSellBench: sold occupant of slot %d."), SlotIndex);
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

static FAutoConsoleCommandWithWorldAndArgs SpawnPathWalkerCommand(
	TEXT("SpawnPathWalker"),
	TEXT("SpawnPathWalker <q> <r> [hexesPerSecond] - debug-only: spawns a pathfinding test walker on a spawn-flagged hex; it walks to the back row and logs its crossing time."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSpawnPathWalker)
);

static FAutoConsoleCommandWithWorldAndArgs SpawnEnemyCommand(
	TEXT("SpawnEnemy"),
	TEXT("SpawnEnemy <DataAssetName> <q> <r> - debug-only: spawns an enemy on a spawn-flagged hex; it walks to the back row, exits, and logs its crossing time."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSpawnEnemy)
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

static FAutoConsoleCommandWithWorldAndArgs DebugRollChampionPoolCommand(
	TEXT("DebugRollChampionPool"),
	TEXT("DebugRollChampionPool [sampleCount=10000] - rolls a throwaway champion pool and logs the tier distribution."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugRollChampionPool)
);

static FAutoConsoleCommandWithWorldAndArgs DebugBenchStateCommand(
	TEXT("DebugBenchState"),
	TEXT("Lists every bench slot and its occupant."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugBenchState)
);

static FAutoConsoleCommandWithWorldAndArgs DebugBenchAddCommand(
	TEXT("DebugBenchAdd"),
	TEXT("DebugBenchAdd <DataAssetName> - debug-only: spawns a champion directly onto the bench, bypassing the shop."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugBenchAdd)
);

static FAutoConsoleCommandWithWorldAndArgs DebugShopDumpCommand(
	TEXT("DebugShopDump"),
	TEXT("Lists every shop slot and its champion."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugShopDump)
);

static FAutoConsoleCommandWithWorldAndArgs ShopRerollCommand(
	TEXT("ShopReroll"),
	TEXT("Rerolls the shop: returns current slots to the pool and draws fresh ones at RerollCost gold."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugShopReroll)
);

static FAutoConsoleCommandWithWorldAndArgs ShopBuyCommand(
	TEXT("ShopBuy"),
	TEXT("ShopBuy <slotIndex> - buys the champion in that shop slot onto the bench."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugShopBuy)
);

static FAutoConsoleCommandWithWorldAndArgs DebugSellHexCommand(
	TEXT("DebugSellHex"),
	TEXT("DebugSellHex <q> <r> - debug-only: sells the champion occupying that hex."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSellHex)
);

static FAutoConsoleCommandWithWorldAndArgs DebugSellBenchCommand(
	TEXT("DebugSellBench"),
	TEXT("DebugSellBench <slotIndex> - debug-only: sells the champion in that bench slot."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugSellBench)
);

static FAutoConsoleCommandWithWorldAndArgs DebugCoordOverlayCommand(
	TEXT("DebugCoordOverlay"),
	TEXT("Toggles the (q, r) coordinate text overlay on all HexGridVisualizer actors."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DebugCoordOverlay)
);
