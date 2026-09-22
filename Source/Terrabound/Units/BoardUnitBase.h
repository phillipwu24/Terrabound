// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Grid/HexCoordinates.h"
#include "BoardUnitBase.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;

/**
 * Which side of the board a unit belongs to. Two values only - CLAUDE.md's "Deliberately not
 * built" list has no third faction in scope, so this isn't written to leave room for one.
 */
UENUM(BlueprintType)
enum class EBoardUnitTeam : uint8
{
	Player,
	Enemy
};

/**
 * Shared base for champions and enemies (PLAN.md 4.2). Holds board coordinate and team
 * identity and knows how to snap itself to a hex's world position. Deliberately thin: nothing
 * champion-specific lives here - stats, cost, and abilities are ChampionData/ChampionBase's job
 * (task 4.3/4.4).
 *
 * Not an ACharacter and has no movement component. Units are snap-positioned from grid data, never
 * physics/gravity-driven - CLAUDE.md's grid-is-data invariant extends to unit placement: a unit's
 * world position is derived from CurrentCoord, not simulated.
 *
 * SnapToHex does not touch HexGrid::Occupant. Whatever moves a unit (the debug spawn command,
 * task 4.5; the drag/placement system, Phase 5; an enemy walking, via TryOccupy) calls
 * HexGrid::SetOccupant/TryOccupy/ClearOccupant itself -
 * a hex-to-hex swap touches two tiles at once, and that orchestration belongs with the code that
 * already knows both coordinates, not smuggled into a per-unit method.
 */
UCLASS()
class TERRABOUND_API ABoardUnitBase : public AActor
{
	GENERATED_BODY()

public:
	ABoardUnitBase();

	/**
	 * Moves this unit to Coord's world position (via HexGrid's HexRadius) and updates
	 * CurrentCoord. Does not touch HexGrid occupancy - see class comment.
	 */
	UFUNCTION(BlueprintCallable, Category = "Board Unit")
	void SnapToHex(const FHexCoord& Coord);

	/** Live runtime state, not EditAnywhere - see CLAUDE.md's Blueprint exposure rule. Set through
	 * SnapToHex, or by a walking unit's own claim (SetCurrentCoord). */
	UFUNCTION(BlueprintPure, Category = "Board Unit")
	FHexCoord GetCurrentCoord() const { return CurrentCoord; }

	UFUNCTION(BlueprintPure, Category = "Board Unit")
	EBoardUnitTeam GetTeam() const { return Team; }

	/**
	 * Set once at spawn (debug spawn command, later real spawning) - not meant to change a
	 * unit's side mid-life. Kept as a function rather than an EditAnywhere property because it
	 * also re-applies the team tint (see ApplyTeamTint); a raw editable property would give
	 * Blueprint a second way to change team that skips the tint update.
	 */
	UFUNCTION(BlueprintCallable, Category = "Board Unit")
	void SetTeam(EBoardUnitTeam NewTeam);

protected:
	virtual void BeginPlay() override;

	/**
	 * Sets CurrentCoord without moving the actor, for a unit that walks between hexes: it claims the
	 * destination in the grid when a step begins, so the coord must follow the claim while the actor
	 * is still visually mid-step. Everything else moves through SnapToHex.
	 */
	void SetCurrentCoord(const FHexCoord& Coord) { CurrentCoord = Coord; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Board Unit")
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/**
	 * Uniform click/drag hit volume, sized independently of the mesh so every champion is
	 * equally easy to pick up regardless of its actual model's silhouette (Grux vs Kwang, see
	 * task 4.1). The skeletal mesh itself is given no collision - it has no PhysicsAsset and is
	 * not meant to be traced against. Blocks only the Visibility channel, which is what
	 * ABoardPlayerController's cursor trace uses; every other channel is ignored so this doesn't
	 * interfere with anything else that traces the board later.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Board Unit")
	TObjectPtr<UCapsuleComponent> HitBox;

private:
	/**
	 * Applies PlayerTintColor/EnemyTintColor (per UTerraboundSettings) as an overlay material on
	 * Mesh - the permanent, always-on visual identifier DESIGN.md calls for, since both sides draw
	 * from the same Paragon packs. No-ops if Mesh or the world isn't ready yet (SetTeam can run
	 * from a constructor, before either is safe); BeginPlay re-applies once it's safe to create a
	 * dynamic material instance.
	 */
	void ApplyTeamTint();

	FHexCoord CurrentCoord;

	EBoardUnitTeam Team = EBoardUnitTeam::Player;
};
