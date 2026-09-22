# PLAN.md — Terrabound, Checkpoint 2

> Checkpoint 1's plan is in git at `d469f22` (`git show d469f22:PLAN.md`). Read-only facts about
> the stock Paragon Animation Blueprints, used by tasks 1.8 and 3.8, are in `ANIMATION_FINDINGS.md`.

Companion to `CLAUDE.md` and `DESIGN.md`. This document covers **one checkpoint only**.

**Scope:** the first fights. Enemies that walk the board, GAS, a champion that shoots, and enemies
that stop and fight blockers. **No assassins, no mana or cast abilities, no terrain, no trait
effects, no chosen leak cost.**

**Shape:** three phases, in this order, each answering the riskiest open question first.

1. **Enemies walk** — no combat, no GAS. Build-order Step 1: does the board size and enemy speed
   produce a fight worth having?
2. **GAS and the first champion that shoots** — Step 2. The first real game loop.
3. **Blockers and aggro** — Step 3. Where the aggro model gets its real test.

---

## Read order

1. `CLAUDE.md` — architecture invariants and working style. Highest authority.
2. `DESIGN.md` — design intent.
3. This file — build order and task breakdown for Checkpoint 2.

**Precedence:** if this document contradicts `CLAUDE.md` on an architecture invariant, `CLAUDE.md`
wins and the contradiction is a bug in this file — raise it. Unlike Checkpoint 1, this plan follows
`CLAUDE.md`'s build order (Steps 1 → 2 → 3) with no reordering.

---

## Decisions this plan makes

Calls that `DESIGN.md`, `CLAUDE.md` and Checkpoint 1 do not settle, all approved. The tasks below
assume them; change one here first if it needs revisiting.

| # | Question | Decision |
|---|---|---|
| D1 | Which actions are allowed mid-wave? | **Prep only.** Buy, reroll, sell, heal, drag and placement all lock during combat. A mid-wave merge would destroy a fighting unit, and `DESIGN.md` already says no moving units mid-combat. |
| D2 | What starts a wave? | **The player**, via a `StartWave` console command and key. No prep timer. A Ready button is UI, which waits. |
| D3 | How does the player earn gold? | **Kill gold only; no flat per-wave income** (the unused `BaseIncomePerWave` is removed). Each enemy carries a flat `GoldOnKill` on `EnemyData` (placeholder 1; a per-type value is the same field with another number). It is granted automatically the moment the enemy dies, and the HUD shows a `+N` at the gold counter. Gold can't be spent until the wave ends (D1), so it builds up in combat and is spent in prep. `StartingGold` funds the first prep. Interest and streaks are separate systems, later. A leaking enemy drops no gold: that is an intended punishment for leaking, additional to any leak cost chosen later. |
| D4 | A champion dies: what happens to its pool copies? | **Return them to the pool, no refund.** Otherwise deaths shrink the pool for good and an endless run dries the shop up. A dead star-up returns every copy it is worth. |
| D5 | How do champions pick targets? | **Nearest enemy in range by hex distance**; hold until it dies, becomes untargetable, or leaves range. No free-hex test, because champions don't move. |
| D6 | Which attributes exist now? | **Health, MaxHealth, AttackDamage, AttackSpeed.** Mana and Armor arrive with the first cast ability, which is where they do something. |
| D7 | What does an attack do? | **Instant damage on activation** in Phase 2 (3.8 adds the hit delay from D16), cooldown `1 / AttackSpeed`. No windup or projectile. |
| D8 | Star-up and HP | **Star-up keeps the HP fraction** (max HP grows, current HP scales with it). Stat multipliers are placeholders: ×1.8 health and ×1.5 damage per level; attack speed and range stay flat (decided). |
| D9 | Paid heal shape | **1 gold per 20% of MaxHealth**, one step per press, prep only. Hover the unit (or carry it) and press `H`, mirroring sell's `E`. Debug-grade UI. |
| D10 | Damaged-sell rule (`DESIGN.md` §4) | **Not built.** Until it exists, selling and rebuying launders damage. Known, accepted for this checkpoint; don't test heal pricing that way. |
| D11 | Unit cap | **Flat config value**, placeholder **10**, marked unresolved, on `DA_BoardConfig`. It counts board champions only. A cap of 6 or less makes a full-row wall impossible, so tune it down during 4.2. The growth formula is deferred. |
| D12 | Where do spawn hexes live? | **A level-placed C++ actor** holding the array and `SpawnInterval`, not a level Blueprint graph. The array is data on the placed instance and needs no graph. |
| D13 | Wave data shape | **`DT_Waves`**, one row per wave, each row a roster of `{EnemyData, count}`. Waves past the table repeat the last row. The endless scaling formula is out of scope. |
| D14 | `SpawnInterval` starting value | **1.5 s**, marked unresolved, tuned in 1.7 against the 3.5 s crossing time. |
| D15 | Animation | **Walk, basic attack and death are in scope** (1.8, 3.8), played from C++ as montages on the `UpperBody` slot that every stock Paragon ABP already has (`ANIMATION_FINDINGS.md`). No new or edited Animation Blueprints; the Paragon folders stay as imported. The clips are fields you assign on the data assets. Ability animations wait for mana. |
| D16 | When does an attack's damage land? | **After a per-unit `AttackHitDelaySeconds`** (a data field, tuned by eye), because no montage in the packs has an impact notify to hang it on. It fizzles if the target died or became untargetable during the delay. Phase 2 stays instant until 3.8 adds it. |

---

## Carried forward from Checkpoint 1

- **Crossing time:** empty board is 7 hexes; at **2 hexes/s** that is **3.5 s**. This seeds
  `EnemyData` speed and `SpawnInterval`. `OccupiedTileCost` (100, on `DA_BoardConfig`) must exceed
  the longest detour.
- **`UHexGrid::SetOccupant` overwrites.** It does not check occupancy, which is right for
  placement and wrong for a unit racing another into a hex. Task 1.1 adds a claim that fails.
- **`ADebugPathWalker` is throwaway.** Delete it and `SpawnPathWalker` when `EnemyBase` walks (1.7).
  Keep `FHexPathfinder` and its tests.
- **Star-up exists.** Units can be destroyed by merging, selling, and (new) dying. None recreates.
- **The unresolved damaged-sell rule** (`DESIGN.md` §4) waits on HP, which arrives in Phase 2, but
  is not built here (D10).
- **Still open, deliberately:** leak cost, aggro radius beyond range, checkmate, scoutable wave UI.

---

## Invariants that bind this checkpoint

From `CLAUDE.md`, restated because each has a concrete consequence below.

**One unit per tile, priced by the search, enforced by the step.** An enemy claims its next hex
when it *begins* the step and releases the one it left. Checking on arrival lets two enemies
mid-move into one hex both complete. Drift may decide who wins a race; it never puts two units on
one tile.

**Movement is one hex at a time, no stored path.** On each arrival, target check first. Locked and
in range: stop and attack. Locked and out of range: step toward a free hex within range of it. No
target: step toward the exit. "Toward" is lower *path* distance, never raw hex distance.

**Targeting stays out of GAS.** `TargetingComponent` selects; the ability receives the target as
data. No `AGameplayAbilityTargetActor`. `State.Untargetable` is a tag the scan filters on.

**Nearest uses path length; in range uses hex distance.** Range checks never run the search.
Acquisition is two conditions (in range **and** a free hex within range, the enemy's own hex
counting), filtered *before* ranking survivors by path length.

**Enemy range is 1 or 2 hexes.** Never scaled with wave number. Stored in hexes.

**Execution-order drift is accepted.** No stable processing order, no deterministic tie-break, no
replay guarantee. Do not add one.

**Units persist across waves.** HP lives on the unit's ability system component and survives
because the actor does. No wave-reset path may destroy and recreate units.

**GAS owns** health, attack damage, attack speed, abilities and their cooldowns, and stat effects
such as the star multiplier. **GAS does not own** target selection.

**Traits are GameplayTags.** No trait effects this checkpoint, so nothing new touches them.

**Blueprint exposure.** Every number below that is a tuning value is `EditAnywhere` on its data
asset or config. Live runtime state (HP, target lock, current coordinate) gets getters, not
`EditAnywhere`.

**Not built, per `CLAUDE.md`:** a chosen leak cost (counter and event only; forfeited kill gold
needs no code), an HP crystal, line-of-fire
blocking, a ranged archetype, Runners.

---

## Layout

New this checkpoint; nothing else is created.

```
Source/Terrabound/
├── Units/          + EnemyBase
├── Combat/         + TargetingComponent, TargetingRules
├── Abilities/      + CombatAttributeSet, GA_BasicAttack, damage/heal/star GameplayEffects
├── Waves/          + WaveManager, WaveDefinition, SpawnHexConfig
└── Data/           + EnemyData
```

**Deliberately not created:** `Terrain/`, `EnemyAssassin`, `TargetableInterface` (every target is
already a `BoardUnitBase`; an interface with one implementer is scaffolding), and `CombatResolver`
(damage is a GameplayEffect executed by the attribute set, which is the resolution).

Content adds `Data/Enemies/` and `Data/Waves/`. Per `CLAUDE.md`, enemy stats live in data assets,
not Blueprints.

**Every task is tagged with who does it:** **[C++]** (Claude), **[Editor]** (you; Claude specifies
asset name, parent class, folder and values), or **[C++ + Editor]** with a line for each half. An
editor step never blocks a task: hand it over, write the C++ side, carry on.

---

# Phase 1 — Enemies walk

No combat, no GAS. An enemy spawns, crosses, exits, and is counted.

### 1.1 Claimable occupancy (`Grid/`)
**[C++]**
`UHexGrid::TryOccupy(Coord, Unit)`: succeeds only if the tile exists and has no other valid
occupant; sets it and broadcasts `OnOccupancyChanged`. Placement keeps using `SetOccupant`, which
overwrites unconditionally (its callers check `CanPlaceAt` first). An enemy calls `TryOccupy` on
the next hex when it begins a step and `ClearOccupant` on the hex it leaves, at the same moment.

**Done when:** a headless test shows a second claimant on the same tile fails, and the tile never
holds two units however the claims interleave.

**Done (2026-09-21, `5d4e2c9`).** Two headless tests (`TryOccupy.Contested`, `.EdgeCases`),
mutation-checked. It checks occupancy only: not walkable, spawn or placeable. A destroyed
occupant counts as empty; re-claiming your own tile succeeds without a broadcast. The failed-claim
no-broadcast rule is untested (`OnOccupancyChanged` is a dynamic delegate; a test would need a
`UCLASS` listener).

### 1.2 `EnemyData` and `EnemyBase` (`Data/`, `Units/`)
**[C++]**
`UEnemyData` (`UPrimaryDataAsset`): `DisplayName`, `SkeletalMesh`, `AnimBlueprint` (idle only,
D15), `MaxHealth`, `AttackDamage`, `AttackSpeed`, `RangeInHexes` (1 or 2, validated), `MoveSpeedHexesPerSecond`
(seed **2.0**, in hexes per second so a `HexRadius` change never retunes it), `GoldOnKill`
(placeholder 1, D3). Stat fields are init data for Phase 2, read by nothing until then, except
speed.

`AEnemyBase : ABoardUnitBase`: `Team` is Enemy (the tint hook already exists),
`InitializeFromEnemyData`. Spawned generically like `AChampionBase` and initialised from its data
asset; **no per-enemy Blueprint**.

Walking, ported from `ADebugPathWalker`: on each arrival, ask `FHexPathfinder` for the next hex
toward the exit (distance field seeded at the back row); begin the step with `TryOccupy`; time left
over after an arrival carries into the next step so crossing time is frame-rate independent. If
the claim fails, hold and re-decide next tick. Face the step direction. On reaching the back row:
release the tile, fire `OnEnemyExited`, destroy.

**Done when:** an enemy spawned by console crosses the empty board, exits, and logs its crossing
time; a lone champion in its lane is routed around; it never enters an occupied hex.

**Done (2026-09-21, `e459675`; PIE-verified by the user with Grux as `DA_Enemy_Grux`).** As built:
`Data/EnemyData.h`, `Units/EnemyBase.*`, `SpawnEnemy <DataAssetName> <q> <r>` (spawn-flagged,
unoccupied hex), four headless tests (`Terrabound.Units.EnemyBase.*`, mutation-checked). Empty-board
crossing measured at **3.50 s**. Calls made: `CurrentCoord` is the *claimed* hex from the moment a
step begins (protected `SetCurrentCoord` on `BoardUnitBase`); `OnEnemyExited` is a native multicast
delegate carrying the enemy (a dynamic one can't be bound by a test); the tile is released in
`Destroyed()`, not `EndPlay`; facing snaps to the step direction with no turn interpolation; the
distance field is recomputed on every decision, including every tick of a hold. `AEnemyBase` exposes
`AdvanceMovement(dt)` publicly so tests can drive it. `RangeInHexes` is validated by an editor clamp
plus a runtime error log, not stored on the enemy, so nothing clamps at runtime.

### 1.3 Spawn hex config (`Waves/`)
**[C++ + Editor]**
**C++:** `ASpawnHexConfig`, a level-placed actor with `TArray<FHexCoord> SpawnHexes` and
`SpawnInterval` (D12, D14). On `BeginPlay` it calls `UHexGrid::SetSpawnFlag` for each hex and hands
`SpawnInterval` to `UWaveManager`. It logs an error for any hex in the player zone. The grid never
holds a copy of the array; this is a value read once at startup.
**Editor:** place one in `MainBoard`, set 3 far-edge spawn hexes.

**Done when:** the flagged hexes render as spawn hexes and are not placeable. The `SetSpawnFlag`
console command may stay as a debug tool.

### 1.4 `WaveManager` (`Waves/`)
**[C++]**
`UWorldSubsystem` owning the run's flow.

- **Phase:** `Prep` or `Combat`, `GetPhase()`, `OnPhaseChanged`. `StartWave()` is valid only in
  Prep, and refuses while a unit is carried (same reason `CanBuy` does).
- **Waves:** `FWaveRow : FTableRowBase` holds a roster of `{TSoftObjectPtr<UEnemyData>, Count}`
  (D13). `DT_Waves` is registered in `UTerraboundSettings` like the pool tables. Waves past the
  table repeat the last row.
- **Spawn pacing:** a spawn happens when `SpawnInterval` has elapsed for that hex **and** a
  `TryOccupy` on it succeeds. The timer sets the rate; occupancy only ever delays.
- **Resolution:** the wave ends when the roster is fully spawned and no enemy is alive. Back to
  Prep, `WaveIndex` up. No timer, and no income is granted here (D3): gold comes only from kills.
  (`BaseIncomePerWave` was already removed from `EconomyConfig`.)
- **Leaks:** `OnEnemyExited` increments `LeaksThisWave` and `LeaksTotal`, logged per wave and
  cumulatively. **No cost, no HP bar, nothing that reads the counter.**
- **Debug:** `StartWave`, `WaveDump`.

**Done when:** `StartWave` runs a small wave: enemies spawn paced, cross, exit, are counted, and
the wave resolves back to Prep. (Nothing earns gold yet: enemies only exit in this phase. Kills, and
so income, arrive in 2.2.)

### 1.5 Prep-phase gating (`Input/`, `Economy/`)
**[C++]**
While `Combat`: `ShopSystem::CanBuy`, `Buy`, `Reroll` and `Sell` refuse; `BoardPlayerController`
refuses pickup and drop (D1). `StartWave` refuses while carrying, so no drag is ever live when
combat begins. Only *spending* locks: the gold counter keeps updating during Combat (D3).

**Done when:** in Combat the shop cards grey and no unit can be lifted; in Prep everything behaves
as before.

### 1.6 Enemy assets and first waves
**[Editor]**
Create in `Content/Terrabound/Data/Enemies/`: `DA_Enemy_A` (`RangeInHexes` 1) and `DA_Enemy_B`
(`RangeInHexes` 2), from `UEnemyData`, reusing already-imported Paragon meshes (the team tint tells
the sides apart). Enemies reuse the champion packs TFT-style, but pick *different meshes* from a pack
than its champion uses. **`DA_Enemy_Grux` already exists** (range 1, made for 1.2's PIE check; it stands in for
`DA_Enemy_A`, so keep it or rename it, but the `SpawnEnemy` command takes the asset name); `DA_Enemy_B`
and `DT_Waves` remain. Placeholder stats, marked unresolved. Create `DT_Waves` in `Data/Waves/`
(row struct `FWaveRow`) with waves 1–3, small rosters mixing both enemies, and point
`UTerraboundSettings` at it.

**Done when:** `StartWave` in PIE spawns wave 1's roster from data.

### 1.7 Measure and retire the walker
**[C++ + Editor]**
Watch a real wave. Record the crossing time from the enemy's own log against the 3.5 s from
Checkpoint 1, and how the stream looks against `SpawnInterval` (one lump, or a stream with an
opening, a middle and a tail). Tune `SpawnInterval`. Then delete `Debug/DebugPathWalker.*` and the
`SpawnPathWalker` command; keep `FHexPathfinder` and its tests.

**Expected, not a bug:** a full row of champions stops the wave forever here, because nothing
fights yet. Test waves with a gap. Task 3.2 removes this.

**Done when:** crossing time and the `SpawnInterval` verdict are written into this task as a
result, as in Checkpoint 1's 7.1, and the walker is gone.

### 1.8 Walk animation (`Units/`, `Data/`)
**[C++ + Editor]**
**C++:** `UEnemyData` gains `WalkAnimation` (a `UAnimSequence`) and `WalkPlayRate`. `UTerraboundSettings`
gains `AnimMontageSlotName` (`UpperBody`, config: it is the same in all four packs and would only
change for a non-Paragon pack). `ABoardUnitBase` gets start and stop calls that play the sequence
as a looping dynamic montage on that slot. `AEnemyBase` starts it when a step begins, does not
restart it between consecutive steps, and stops it when the enemy holds, attacks or exits.
**Editor:** set `WalkAnimation` on each `DA_Enemy_*` to the pack's `Jog_Fwd`, and tune
`WalkPlayRate` by eye so the feet don't slide at 2 hexes/s.

**Why a montage, from the findings (`ANIMATION_FINDINGS.md`):** the stock ABPs read velocity through
`TryGetPawnOwner`, which is null for our actors, so their locomotion variables never leave 0 and the
walk states never run. The clips are in-place (1.3 to 2.0 s, no root motion), and the ABP's own
`JogStart`/`JogStop` clips are 1.9 to 5.1 s for three heroes, so driving those variables from C++
is worse than playing `Jog_Fwd` directly. With `IsAccelerating` false, a montage on `UpperBody`
drives the whole body, the same route the spawn animation already takes.

**Spike first.** Confirm three things before building the rest: a looping dynamic montage on
`UpperBody` plays full-body on a non-pawn actor; it overrides the 5 s `LevelStart` montage the ABP
starts at spawn (enemies must not stand through it); and the loop is seamless.

**Done when:** enemies visibly walk their route, stop walking when they hold or exit, and are not
held up by the spawn animation.

---

# Phase 2 — GAS and the first champion that shoots

### 2.1 GAS foundation (`Abilities/`, `Units/`)
**[C++ + Editor]**
**C++:** add `GameplayAbilities` and `GameplayTasks` to `Terrabound.Build.cs`; confirm the
`GameplayAbilities` plugin is enabled in `Terrabound.uproject` (`Terrabound.uproject` lists
`GASToolsets`, which may not imply it; add the entry if it's missing). Put a `UAbilitySystemComponent` on `ABoardUnitBase` and implement
`IAbilitySystemInterface`, so champions and enemies share one setup. `UCombatAttributeSet`:
`Health`, `MaxHealth`, `AttackDamage`, `AttackSpeed` (D6), plus a meta `IncomingDamage`. Both
`InitializeFromChampionData` and `InitializeFromEnemyData` write the data asset values into the
attribute set, with `Health` starting at `MaxHealth`. The stat fields on the data assets stop
being "read by nothing".
**Editor:** none expected beyond the plugin check.

Budget real time here: the first ability is where GAS setup costs are paid.

**Done when:** a `DumpAttributes` console command prints a board unit's attributes and they match
its data asset.

### 2.2 Damage, death and kills (`Abilities/`, `Units/`)
**[C++]**
A damage GameplayEffect writes `IncomingDamage` (magnitude by caller); the attribute set applies it
to `Health` in `PostGameplayEffectExecute`, clamps at 0 and clears the meta attribute. At 0, once:
add tag `State.Dead` and fire the unit's death event.

- **Enemy death:** grant `GoldOnKill` through a new `UEconomyState::GrantIncome(Amount)`, which
  does `Add` and broadcasts `OnIncomeGranted(Amount)` (refunds keep using plain `Add`, so a sell
  shows no `+N`); clear its tile, tell `WaveManager`, destroy.
- **Champion death (D4):** clear its tile, return every pool copy its star level is worth (extract
  `Sell`'s copy-return into a shared helper), destroy. No refund. Dead stays dead.

`State.Dead` and `State.Untargetable` are declared as native GameplayTags in C++, so no tag-table
edit is needed.

**Done when:** a `DamageUnit <q> <r> <amount>` debug command kills a unit through the real path,
its gold and pool effects land, and a unit reduced below full HP keeps that HP across a wave.

### 2.3 `TargetingComponent` (`Combat/`)
**[C++]**
One component on every board unit. It holds the lock (a weak reference), and `TargetingRules`
holds the pure rule functions (`IsInRange` by hex distance, more in 3.1) so they are testable
without actors.

Champion mode (D5): with no lock, scan every tick for the nearest live, targetable enemy in range;
acquire. With a lock, hold. **Drop the lock, as one general mechanism, when the target dies, gains
`State.Untargetable` or `State.Dead`, or leaves range**, then re-scan. This is not a stealth
special case; anything later just applies the tag.

**Done when:** a headless test covers `IsInRange` and the candidate filter (dead and untargetable
are skipped).

### 2.4 Basic attack (`Abilities/`)
**[C++]**
`UGA_BasicAttack`, granted at init to every board unit (no Blueprint needed). When the component
holds a target in range and the ability is off cooldown it activates: apply the damage effect to
the target with the attacker's `AttackDamage` (D7), and commit a cooldown effect whose duration is
`1 / AttackSpeed`. The component picks the target and passes it in as data. No damage-type or armor
maths.

**Done when:** a range-2+ champion kills an enemy standing in its range, at the attack rate its data
implies.

### 2.5 Star-level stat effect (`Abilities/`)
**[C++]**
`AChampionBase::SetStarLevel` applies a GameplayEffect with multiplicative modifiers on `MaxHealth`
and `AttackDamage` (replacing any earlier star effect). The magnitudes are two arrays in
`UTerraboundSettings` next to `StarMeshScaleMultipliers`: placeholders ×1, ×1.8, ×3.24 health and
×1, ×1.5, ×2.25 damage. **Attack speed and range are never touched** (decided). Current `Health`
scales with `MaxHealth` so the HP fraction is kept (D8).

**Done when:** merging two units to a 2-star raises max HP and damage by the configured factors and
leaves the HP fraction unchanged.

### 2.6 Debug health overlay
**[C++]**
`HealthOverlay` toggles per-tick debug text above every board unit: current and max HP, star level.
A real HP bar is UI and waits. Also `SetUntargetable <q> <r> <0|1>` for 3.5.

**Done when:** damage, persistence across a wave, and star scaling are all readable on screen.

### 2.7 Paid heal (`Economy/`, `Input/`)
**[C++]**
`EconomyConfig` gains `HealStepFraction` (0.2) and `HealCostPerStep` (1), placeholders (D9).
`UShopSystem::Heal(Unit)` beside `Sell`, since both are gold-for-unit transactions: prep only,
refuses at full HP or without gold, applies an instant heal effect of one step. `BoardPlayerController`
binds `H` to the carried unit, else the unit under the cursor (the pickup trace already finds it).
The damaged-sell rule is **not** built (D10).

**Done when:** a damaged champion heals 20% per press for 1 gold, and it refuses in Combat.

### 2.8 Champion stats
**[Editor]**
On the four `DA_Champion_*` assets: set `MaxHealth`, `AttackDamage`, `AttackSpeed` and
`RangeInHexes` so there are **two blockers** (range 1, high health) and **two shooters** (range 3,
low health). Also check `Tier`: all four read 1 at one point in Checkpoint 1, and cost derives from it.
Placeholder numbers.

**Done when:** a shooter placed in a wave's lane kills a wave-1 enemy that walks into its range.

### 2.9 Gold-gain popup
**[Editor]**
In the HUD widget (`WBP_HUD`), show a `+N` at the gold counter each time
`UEconomyState::OnIncomeGranted` fires (D3), then fade it. The widget only displays the amount C++
passes; it computes nothing. Several kills at once may stack or add up: your layout call.
World-space popups at the enemy's position are polish and out of scope.

**Done when:** each kill shows its `+N` and the counter rises by that amount; a sell or any other
gold change shows no `+N`.

---

# Phase 3 — Blockers and aggro

### 3.1 Enemy acquisition (`Combat/`)
**[C++]**
Enemy mode for `TargetingComponent`, per `CLAUDE.md`. A candidate is a live, targetable champion
that is (a) within `RangeInHexes` by hex distance **and** (b) has at least one hex within range of
it that is unoccupied, the enemy's own hex counting. Filter on both **first**, then rank the
survivors by path length. Add the pathfinder query that ranking needs (path length from an enemy's
hex to a candidate, from the existing search). Scan every tick while unlocked.

**Done when:** headless tests show (1) in range with every attack hex occupied is *not*
acquirable, (2) the enemy's own hex counts as free, (3) "nearest" follows path length and can
differ from hex distance, using a wall of units.

### 3.2 Enemy decision loop (`Units/`, `Pathfinding/`)
**[C++]**
On each arrival, target check first:

- **Locked, in range:** stop and attack.
- **Locked, out of range:** distance field seeded at the *free hexes within range of the target*
  (`ComputeDistanceField` already takes several sources), step to the lowest neighbour.
- **No lock:** scan; if found, treat as above; otherwise the exit field, as in 1.2.

A blocked step (1.2's failed claim) is a contested hex, handled in 3.3. Nothing is stored between
arrivals.

**Done when:** enemies walking a lane stop at a blocker and stay; a full row of champions no
longer stalls the wave forever (the wall is attacked instead).

### 3.3 Contested hexes
**[C++]**
The loser of a `TryOccupy` race drops its lock and re-scans on the same tick. Another free hex in
range of the same champion means it re-acquires that champion. Only if none exists does it fall
through to the exit. Dropping the lock is not walking away. Once per tick per enemy, so a
pathological board cannot loop.

**Done when:** a headless test with two enemies and one free attack hex shows exactly one
succeeds and the other drops its lock; a stress test over many interleavings never leaves two
units on a tile.

### 3.4 Enemy attacks
**[C++]**
Enemies use the same `UGA_BasicAttack` against a locked target in range. A champion killed goes
through the death path from 2.2.

**Done when:** an unprotected shooter is killed by enemies that reach it, and its tile and pool
copies are handled.

### 3.5 Lock breaking, verified
**[C++]**
No new mechanism: this checks that 2.3's single drop path covers every case for enemies too.
`SetUntargetable` (2.6) makes enemies locked on that champion drop and re-scan; a target whose
last free attack hex gets filled makes the approaching enemy drop and fall back to the exit; a
target killed by a shooter drops every enemy locked on it.

**Done when:** each of those is observed in PIE and none needs a special case in the code.

### 3.6 Unit cap (`Grid/`, `Input/`)
**[C++ + Editor]**
**C++:** `UBoardConfig::UnitCap` (D11, placeholder 10, marked unresolved) and a derived read on
`UHexGrid` for the count of board champions (enemies never count). Per `CLAUDE.md`'s settled
details: bench units don't count; a swap, board to board or bench onto an occupied hex, never
changes the count and is always allowed; only **bench to empty hex** is capped; the drag preview
shows hexes invalid once the board is at the cap. A merge or a death lowers the count.
**Editor:** the value in `DA_BoardConfig`.

**Done when:** at the cap a bench unit cannot be dropped on an empty hex but can swap; the preview
marks hexes invalid; lowering the cap below the current count evicts nothing.

### 3.7 Roster and wave tuning
**[Editor]**
Set enemy and champion stats and wave rows so a wave-1 fight has a start, middle and end, and waves
2–3 press harder. Placeholder numbers throughout.

**Done when:** a sensible board wins wave 1 without losing a unit and loses some HP by wave 3.

### 3.8 Attack and death animations (`Units/`, `Abilities/`, `Data/`)
**[C++ + Editor]**
**C++:** `UChampionData` and `UEnemyData` both gain `AttackMontages` (an array, cycled),
`AttackHitDelaySeconds` (D16) and `DeathAnimation` (a `UAnimSequence`).
- **Attack:** `ABoardUnitBase::PlayAttackAnimation(IntervalSeconds)` turns the unit to face its
  target, plays the next montage, and speeds it up (never slows it) to fit inside the attack
  interval. `UGA_BasicAttack` calls it on activation and applies damage after
  `AttackHitDelaySeconds`, skipping the damage if the target died or became untargetable.
- **Death:** the 2.2 death path calls `PlayDeathAnimation()`: disable the HitBox, play the clip as a
  dynamic montage on the slot, destroy the actor when it ends. The tile is freed and `State.Dead`
  is set at the moment of death, so a dying unit is neither targetable nor blocking.

**Editor:** on each data asset, assign montages (for example Grux's `PrimaryAttack_LA_Fast_Montage`
and `PrimaryAttack_RA_Fast_Montage`), a death sequence, and tune the hit delay by eye per hero.

**From the findings:** attack montages already carry the `UpperBody` slot, so no slot setup is
needed. None has an impact notify, only `SaveAttack` and `ResetCombo`; those cast the pawn owner
and the casts fail harmlessly for our actors. Serath's A and B montages have a RateScale of 2.0.
There are no death montages: Kwang and Serath have one death sequence each, Grux and Belica two.
No clip uses root motion.

**Done when:** attacks play a swing with damage landing at the tuned moment; a dying unit plays its
death clip and disappears at the end without blocking a hex; console shows no errors from the
ABP's notifies.

---

# Phase 4 — Checkpoint validation

### 4.1 Full-loop smoke test
**[Editor]**
Buy, place, `StartWave`. Watch enemies stop at blockers and shooters kill them. See HP persist,
heal a unit, see each kill's `+N` and confirm nothing is spendable until the wave ends, merge and sell in prep, start the next wave. Confirm no
errors and that grid data stays consistent: no unit ever on two tiles, no orphaned occupant.

**Done when:** three waves run back to back with no errors.

### 4.2 Playtest questions, answered in writing
**[Editor + C++]**
Record the answers here, as Checkpoint 1's 7.1 recorded crossing time. These are the questions
`DESIGN.md` §8 says only play can answer:

- **Does one cheap blocker trivialize a wave?** And does a full-row wall? Lower `UnitCap` toward 6
  and see.
- **Crossing time against `SpawnInterval`:** lump, or stream?
- **Does the cap change what you buy and place?**
- **Does persistent damage plus a 1-gold heal feel like an economy, or a chore?**
- **Do leaks happen at all** against a competent board (informs whether a cost is needed)?
- **How hard does forfeited kill gold punish a leak on its own?** It is already a punishment. Does
  the run need a further chosen leak cost at all, or is the spiral already fast enough?
- **Does the wave ever stall** (enemies clogging the spawn hexes)?

**Done when:** each has a written answer, and the next plan can start from them.

---

## Definition of done

- [ ] Enemies spawn on configured hexes, walk one hex at a time with no stored path, and exit
- [ ] `TryOccupy` is atomic; no test or PIE run ever puts two units on one tile
- [ ] A wave resolves when the roster is spawned and no enemy is alive
- [ ] Leaks are counted per wave and in total, with no cost and nothing reading the counter
- [ ] Shop, bench, sell, heal and drag are prep-only; `StartWave` refuses while carrying
- [ ] Every board unit has an ability system component; attributes come from data assets
- [ ] Targeting is not routed through GAS; `TargetingComponent` selects and passes data
- [ ] Damage, death, kill gold and pool return all work through one path
- [ ] Kill gold is the only income: granted on death, `+N` shown at the gold counter, unspendable
      until the wave ends; no flat per-wave income exists
- [ ] HP persists across waves; no path destroys and recreates a unit
- [ ] Star-up scales health and damage by configured factors; attack speed and range stay flat
- [ ] Enemy acquisition needs both conditions, filtered before path-length ranking
- [ ] Lock breaks through one mechanism (dies, untargetable, leaves range, loses attack hex)
- [ ] Contested hexes resolve by re-check; the loser re-scans the same tick
- [ ] Unit cap enforced for bench-to-empty only; preview shows invalid at the cap
- [ ] Paid heal works and refuses in Combat, at full HP, or without gold
- [ ] Enemies play a walk loop, and attacks and deaths play their clips, with no new or edited
      Animation Blueprint and the Paragon folders untouched
- [ ] `ADebugPathWalker` and `SpawnPathWalker` removed
- [ ] Crossing time and `SpawnInterval` verdict written down
- [ ] Playtest questions in 4.2 answered in writing
- [ ] No mana, armor, cast ability, terrain, or chosen leak cost anywhere

---

## Out of scope — do not build, do not scaffold

Assassins, the leap, mana, armor, and any cast ability (the next checkpoint's first ability).
Terrain, trees, `TerrainPieceBase`, `PlacementValidator`, seal prevention. Trait thresholds and
effects. The damaged-sell rule and any change to sell value. A chosen leak cost (forfeited kill gold, D3, is already one and needs no code). The checkmate lose
condition (a run with no units will simply stall). Scoutable wave display. The endless scaling
formula for waves. HP bars, damage numbers, world-space popups and other UI (the HUD `+N` gold popup, 2.9, is the one exception); a Ready button; a real heal UI. Interest and streaks. Ability animations (with
mana), animation notifies, and any new or edited Animation Blueprint, including driving the stock
ABPs' `Speed` and `IsAccelerating` from C++. Projectiles, windup, line-of-fire
blocking. A ranged enemy archetype, `TargetableInterface`, `CombatResolver`. Augments, items,
bosses, mid-combat repositioning, enemy-side terrain, depth bonuses, stat degradation. Save/load,
audio, textures, VFX, LODs, performance work.

Several of these are one small step from something in this plan. That is why the list is explicit.

---

## Known issues to expect, not solve

- **The stock ABPs are pawn-shaped.** They idle and play the spawn animation fine but never see
  velocity, which is why walking is played directly (1.8). They also start a 5 s `LevelStart`
  montage on every spawn, including enemies; the 1.8 spike checks the walk overrides it.
- **Attack timing is approximate.** There is no impact notify, so the hit delay is tuned by eye
  per hero (D16). Paragon animation timing is tuned for a MOBA camera and may still want
  adjusting at this zoom.
- **Do not "Save All" after an ABP inspection.** Reading ABP graphs through the MCP marked all four
  ABPs dirty in memory; nothing was written.
- **Phase 1 stalls at a full wall** until Phase 3 lands (1.7).
- **Enemies shoot through trees that don't exist yet.** Range is hex distance by design.
- **A blocker wall may be too strong.** That is a 4.2 question, not a bug.
- **Sell-and-rebuy launders damage** (D10).
- **A leaking enemy drops no gold.** That is a real punishment, already in place and needing no
  code, and any leak cost chosen later stacks on top of it. Size that cost knowing leaks already
  hurt. Nothing else reads the counter.
- **Phase 1 waves award no gold** (kills arrive in 2.2). Fine: the player spends part of
  `StartingGold` in the first prep and keeps the rest.

---

## After this checkpoint

Stop. Do not continue into assassins or terrain. The plan is rewritten from what the fights
actually feel like. Likely next: the first cast ability with mana (assassins), and one tree
(Step 4).
