// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EconomyState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoldChanged, int32, NewGold);

/**
 * Owns the player's gold for the run (PLAN.md 6.1). Starting gold is read once from
 * DA_EconomyConfig (via UTerraboundSettings) on Initialize; nothing else about that config is
 * cached here - ShopSystem and Bench resolve their own fields from it independently.
 */
UCLASS()
class TERRABOUND_API UEconomyState : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Economy")
	int32 GetGold() const { return Gold; }

	UFUNCTION(BlueprintPure, Category = "Economy")
	bool CanAfford(int32 Amount) const { return Amount <= Gold; }

	/** Deducts Amount if affordable. Returns false and leaves Gold untouched otherwise. */
	UFUNCTION(BlueprintCallable, Category = "Economy")
	bool Spend(int32 Amount);

	/** Grants Amount gold. */
	UFUNCTION(BlueprintCallable, Category = "Economy")
	void Add(int32 Amount);

	UPROPERTY(BlueprintAssignable, Category = "Economy")
	FOnGoldChanged OnGoldChanged;

private:
	UPROPERTY()
	int32 Gold = 0;
};
