// Copyright Epic Games, Inc. All Rights Reserved.

// The project's debug console commands, per PLAN.md 0.5. This file starts near-empty and
// every later phase adds one more FAutoConsoleCommand here — nothing is ever removed from it.
// FAutoConsoleCommand is self-registering and self-unregistering (module load/unload), so no
// startup or shutdown wiring is needed elsewhere.

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

namespace
{
	void DebugPing()
	{
		UE_LOG(LogTemp, Log, TEXT("Terrabound debug commands are online."));
	}
}

static FAutoConsoleCommand DebugPingCommand(
	TEXT("DebugPing"),
	TEXT("Logs a line confirming the debug console command system is wired up."),
	FConsoleCommandDelegate::CreateStatic(&DebugPing)
);
