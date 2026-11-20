// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "CombatAttributeSet.generated.h"

#define TERRABOUND_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * Every board unit's combat stats (PLAN.md 2.1, D6), owned by its ABoardUnitBase. Starting values
 * are copied once from ChampionData/EnemyData when the unit is created; from then on GAS owns them.
 * Not replicated: single-player.
 */
UCLASS()
class TERRABOUND_API UCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	TERRABOUND_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	TERRABOUND_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData AttackDamage;
	TERRABOUND_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackDamage)

	/** Attacks per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData AttackSpeed;
	TERRABOUND_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, AttackSpeed)

	/** Meta attribute: damage effects write here, and PLAN.md 2.2 turns it into a Health change. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData IncomingDamage;
	TERRABOUND_ATTRIBUTE_ACCESSORS(UCombatAttributeSet, IncomingDamage)
};
