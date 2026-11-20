// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatAttributeSet.h"
#include "../Units/ChampionBase.h"
#include "../Units/EnemyBase.h"
#include "../Data/ChampionData.h"
#include "../Data/EnemyData.h"
#include "../Grid/HexGridTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace CombatAttributeSetTests
{
	/** Each unit's attributes must equal the data asset values it was initialized from, Health at MaxHealth. */
	static void TestAttributes(FAutomationTestBase& Test, const FString& Who, const ABoardUnitBase& Unit,
		float MaxHealth, float AttackDamage, float AttackSpeed)
	{
		const UAbilitySystemComponent* ASC = Unit.GetAbilitySystemComponent();
		if (!Test.TestNotNull(*(Who + TEXT(" has an AbilitySystemComponent")), ASC))
		{
			return;
		}
		Test.TestEqual(*(Who + TEXT(" Health starts at MaxHealth")), ASC->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()), MaxHealth);
		Test.TestEqual(*(Who + TEXT(" MaxHealth")), ASC->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()), MaxHealth);
		Test.TestEqual(*(Who + TEXT(" AttackDamage")), ASC->GetNumericAttribute(UCombatAttributeSet::GetAttackDamageAttribute()), AttackDamage);
		Test.TestEqual(*(Who + TEXT(" AttackSpeed")), ASC->GetNumericAttribute(UCombatAttributeSet::GetAttackSpeedAttribute()), AttackSpeed);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatAttributeSetInitTest, "Terrabound.Abilities.Init", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCombatAttributeSetInitTest::RunTest(const FString& Parameters)
{
	using namespace CombatAttributeSetTests;

	HexGridTests::FScopedGridWorld TestWorld;
	UWorld* World = TestWorld.GetWorld();

	// Values distinct from the data asset defaults and from each other, so a swapped or default field shows.
	UChampionData* ChampionData = NewObject<UChampionData>(GetTransientPackage());
	ChampionData->MaxHealth = 650.f;
	ChampionData->AttackDamage = 55.f;
	ChampionData->AttackSpeed = 0.75f;
	AChampionBase* Champion = World->SpawnActor<AChampionBase>();
	Champion->InitializeFromChampionData(ChampionData);
	TestAttributes(*this, TEXT("Champion"), *Champion, 650.f, 55.f, 0.75f);

	UEnemyData* EnemyData = NewObject<UEnemyData>(GetTransientPackage());
	EnemyData->MaxHealth = 320.f;
	EnemyData->AttackDamage = 18.f;
	EnemyData->AttackSpeed = 1.25f;
	AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>();
	Enemy->InitializeFromEnemyData(EnemyData);
	TestAttributes(*this, TEXT("Enemy"), *Enemy, 320.f, 18.f, 1.25f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
