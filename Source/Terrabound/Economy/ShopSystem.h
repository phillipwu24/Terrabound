// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShopSystem.generated.h"

class UChampionData;
class UChampionPool;
class UEconomyConfig;
class ABoardUnitBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnShopChanged);

/**
 * Owns the shop's slot array and the shared champion pool (PLAN.md 6.3). ShopSlotCount slots,
 * and the pool/odds data tables, are read from DA_EconomyConfig / UTerraboundSettings on
 * Initialize - same pattern as UBench/UEconomyState. A UWorldSubsystem for the same reason UBench
 * is one: the future shop widget and debug commands both need to reach it independently.
 *
 * Does not own Bench state - it calls UBench's public API (HasFreeSlot/AddChampion) rather than
 * holding a slot array of its own, per CLAUDE.md. Never touches UHexGrid or takes a coordinate -
 * Buy lands a champion on the bench, never on a hex.
 */
UCLASS()
class TERRABOUND_API UShopSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetSlotCount() const { return Slots.Num(); }

	UFUNCTION(BlueprintPure, Category = "Shop")
	UChampionData* GetChampionAt(int32 SlotIndex) const;

	/** SlotIndex's champion's gold cost, via TierCostTable. 0 if the slot is empty. */
	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetCostAt(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Shop")
	int32 GetRerollCost() const;

	/** True if Buy(SlotIndex) would currently succeed - the slot is filled, its cost is
	 * affordable, and the bench has room. Lets the shop widget grey out a card without
	 * re-deriving Buy's own rules. */
	UFUNCTION(BlueprintPure, Category = "Shop")
	bool CanBuy(int32 SlotIndex) const;

	/**
	 * Returns every non-null slot's champion to the pool, spends RerollCost gold, and draws a
	 * fresh champion into every slot. Fails cleanly (nothing changed) if RerollCost isn't
	 * affordable.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool Reroll();

	/**
	 * Buys SlotIndex: fails cleanly (nothing changed) if the slot is empty, its tier cost isn't
	 * affordable, or the bench is full. On success, spends the gold, spawns the champion onto the
	 * bench, and empties the slot - it stays empty until the next Reroll.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool Buy(int32 SlotIndex);

	/**
	 * Sells Unit: refunds SellRefundPercentage of its tier cost, returns its ChampionData to the
	 * pool, and destroys the actor. Fails cleanly if Unit isn't a champion. Does not touch HexGrid
	 * or Bench - by the time a unit can be sold it's being carried (PLAN.md 6.6: sell is
	 * carry-then-press-a-key), and BeginDrag already vacated its origin tile/slot on pickup.
	 */
	UFUNCTION(BlueprintCallable, Category = "Shop")
	bool Sell(ABoardUnitBase* Unit);

	UPROPERTY(BlueprintAssignable, Category = "Shop")
	FOnShopChanged OnShopChanged;

private:
	/** Draws a fresh champion into every slot, including slots already null. */
	void FillAllSlots();

	UPROPERTY()
	TArray<TObjectPtr<UChampionData>> Slots;

	UPROPERTY()
	TObjectPtr<UChampionPool> Pool;

	// Held onto (not just read once) because Buy/Reroll need TierCostTable and RerollCost on
	// every call, not only at Initialize.
	UPROPERTY()
	TObjectPtr<UEconomyConfig> Config;
};
