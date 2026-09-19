// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "TraitDefinition.generated.h"

/**
 * One row per trait (PLAN.md 6.7): display info for a Trait.* GameplayTag. Pure display data,
 * consumed only by WBP_TraitPanel (Get Data Table Row nodes) - UTraitCounter never reads this
 * struct, it only counts tags. Adding a trait later is a new row here plus a new Trait.* tag, no
 * C++ or widget changes.
 */
USTRUCT(BlueprintType)
struct FTraitDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	// BlueprintReadOnly, not just EditAnywhere: Break/Split Struct nodes only expose fields that
	// are Blueprint-visible, and WBP_TraitPanel needs to Break the row from Get Data Table Row.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trait")
	TObjectPtr<UTexture2D> Icon;
};
