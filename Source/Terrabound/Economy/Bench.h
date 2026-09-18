// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Bench.generated.h"

class ABoardUnitBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBenchChanged);

/**
 * Owns the bench's slot array (PLAN.md 6.4) - the only way a champion reaches the board.
 * BenchSlotCount slots, read from DA_EconomyConfig via UTerraboundSettings on Initialize, same
 * pattern as UHexGrid/UEconomyState. A UWorldSubsystem rather than an object ShopSystem owns:
 * once bench slots got their own world-space presence (BenchVisualizer, the drag system), the
 * shop, the visualizer, and the drag controller all need to reach this independently, same as
 * they reach UHexGrid - a shop-owned object would force everything else through ShopSystem for
 * no reason.
 *
 * Bench slots are not hexes and are not part of HexGrid's tile array - see CLAUDE.md.
 */
UCLASS()
class TERRABOUND_API UBench : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Bench")
	int32 GetSlotCount() const { return Slots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Bench")
	bool HasFreeSlot() const;

	UFUNCTION(BlueprintPure, Category = "Bench")
	ABoardUnitBase* GetChampionAt(int32 SlotIndex) const;

	/** Index of the slot Unit currently occupies, or INDEX_NONE if it isn't on the bench. */
	UFUNCTION(BlueprintPure, Category = "Bench")
	int32 FindSlotIndex(ABoardUnitBase* Unit) const;

	/** Places Unit in the first free slot. Returns false if the bench is full. */
	UFUNCTION(BlueprintCallable, Category = "Bench")
	bool AddChampion(ABoardUnitBase* Unit);

	/** Removes Unit from wherever it is on the bench. Returns false if it wasn't on the bench. */
	UFUNCTION(BlueprintCallable, Category = "Bench")
	bool RemoveChampion(ABoardUnitBase* Unit);

	/**
	 * Sets SlotIndex's contents directly (Unit may be null to clear). Returns false for an
	 * out-of-range index. The primitive AddChampion/RemoveChampion are built on - used directly
	 * by the drag system, which already knows the exact slot a drop landed on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Bench")
	bool SetSlot(int32 SlotIndex, ABoardUnitBase* Unit);

	UPROPERTY(BlueprintAssignable, Category = "Bench")
	FOnBenchChanged OnBenchChanged;

private:
	UPROPERTY()
	TArray<TObjectPtr<ABoardUnitBase>> Slots;
};
