# PLAN.md — Terrabound, Checkpoint 2

> Checkpoint 1's plan is in git at `d469f22` (`git show d469f22:PLAN.md`). Read-only facts about
> the stock Paragon Animation Blueprints, used by tasks 1.8 and 3.8, are in `ANIMATION_FINDINGS.md`.

Companion to `CLAUDE.md` and `DESIGN.md`. This document covers **one checkpoint only**.

**Scope:** the first fights. Enemies that trickle onto the board in rounds of waves, GAS, a
stationary champion that shoots, and enemies that stop and fight the champions in their way. **No
assassins, no mana or cast abilities, no terrain, no trait effects, no chosen leak cost.**

**Revised 2026-11-17** for the design revision in `DESIGN.md` (header): route planning ignores units
(new task 1.2b), `RoundManager` and `WaveManager` split (1.4a/1.4b), one randomized spawn timer per
wave with spawn windows, round-long damage with a free prep heal (the paid heal is gone), contested
hexes sidestep or wait. Decisions D0, D1, D8–D10, D13, D14 changed; D17–D20 are new.

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
| D0 | Rounds and waves (`DESIGN.md` §"Rounds and waves", §"Spawn pacing") | A **round** runs prep to prep and holds a fixed list of **waves**. Wave N+1 starts spawning **`WaveGapSeconds` after** wave N is resolved (every enemy dead or exited); the round's first wave starts at once. The gap (placeholder **5 s** in `MainBoard`, tuned by the user 2026-11-19; on `ASpawnHexConfig`) was added 2026-11-18 at the user's request; it is still Combat, with no input, healing or repositioning. The round ends when its last wave resolves, and play returns to Prep. Combat lasts the whole round. **One timer only:** the per-wave spawn timer (D14), which also counts down the gap. No prep timer, no stuck-wave timer. |
| D1 | Which actions are allowed mid-round? | **The board locks, the bench doesn't (TFT; revised 2026-11-18, replacing "Prep only").** During combat, which spans every wave of the round and the gaps between them, the player can still reroll, buy onto the bench, sell bench champions, and drag bench ↔ bench. A board champion can't be lifted, so it can't be moved, swapped or sold, and nothing can be dropped onto a hex. Merges use bench copies only. A trio that needs a board copy waits for `ChampionMerger::MergeAll`, which `RoundManager` runs on entering Prep, with the board copy surviving in place. A mid-round merge never destroys a fighting unit, and champions never move during a round. |
| D2 | What starts a round? | **The player**, via a `StartRound` console command and key (the **Start** in `DESIGN.md`), and a **Start button** next to the shop (added 2026-11-19 at the user's request, task 1.9; greyed out outside Prep). No prep timer. |
| D3 | How does the player earn gold? | **Kill gold only; no flat per-wave income** (the unused `BaseIncomePerWave` is removed). Each enemy carries a flat `GoldOnKill` on `EnemyData` (placeholder 1; a per-type value is the same field with another number). It is granted automatically the moment the enemy dies, and the HUD shows a `+N` at the gold counter. Gold earned mid-round can be spent mid-round on rerolls and bench buys (D1). `StartingGold` funds the first prep. Interest and streaks are separate systems, later. A leaking enemy drops no gold: that is an intended punishment for leaking, additional to any leak cost chosen later. |
| D4 | A champion dies: what happens to its pool copies? | **Return them to the pool, no refund.** Otherwise deaths shrink the pool for good and an endless run dries the shop up. A dead star-up returns every copy it is worth. |
| D5 | How do champions pick targets? | **Nearest enemy in range by hex distance**; hold until it dies, becomes untargetable, or leaves range. No free-hex test, because champions don't move. Melee champions (range 1) use the same rule, so they lock too — the default `DESIGN.md` §4 leaves open; flag it if it reads wrong in play. Taunt and interrupt are future lock breaks through the same path; neither is built. |
| D6 | Which attributes exist now? | **Health, MaxHealth, AttackDamage, AttackSpeed.** Mana and Armor arrive with the first cast ability, which is where they do something. |
| D7 | What does an attack do? | **Instant damage on activation** in Phase 2 (3.8 adds the hit delay from D16), cooldown `1 / AttackSpeed`. No windup or projectile. |
| D8 | Star-up and HP | **Star-up keeps the HP fraction** (max HP grows, current HP scales with it). Stat multipliers are placeholders: ×1.8 health and ×1.5 damage per level; attack speed and range stay flat (decided). The fraction is always 1 in practice: a mid-combat merge uses only bench copies, which take no damage, and the board catch-up merge runs on entering Prep, after the prep heal (D17); the rule stays as the neutral default. |
| D9 | Paid heal shape | **Withdrawn (2026-11-17): there is no paid heal.** Living champions heal to full for free at the start of prep (D17). |
| D10 | Damaged-sell rule (`DESIGN.md` §4) | **Withdrawn (2026-11-17): moot.** A board champion can only be sold in prep, when every living unit is at full HP. Bench champions, which can be sold mid-round, take no damage. Either way, sell-and-rebuy launders nothing. |
| D11 | Unit cap | **Flat config value**, placeholder **10**, marked unresolved, on `DA_BoardConfig`. It counts board champions only. A cap of 6 or less makes a full-row wall impossible, so tune it down during 4.2. The growth formula is deferred. |
| D12 | Where do spawn hexes live? | **A level-placed C++ actor** holding the array, not a level Blueprint graph. The array is data on the placed instance and needs no graph. A player-zone entry is an error: logged, its spawn flag cleared, and the hex kept a normal player hex. |
| D13 | Round data shape | **`DT_Rounds`**, one row per round. Each row holds its own list of waves (3 each for now; there is no separate waves-per-round number), and each wave a roster of `{EnemyData, Count, SpawnWindow}` with `SpawnWindow` one of Early / Any / Late. Rounds past the table repeat the last row. The endless scaling formula is out of scope. |
| D14 | Spawn timing | **One randomized timer per wave, not per hex.** The first enemy of a wave spawns at once; after each spawn the next delay is drawn uniformly from `[MinSpawnInterval, MaxSpawnInterval]` (placeholders **3.0 / 4.0 s** in `MainBoard`, tuned by the user 2026-11-19 from 1.0 / 2.0 s, which felt far too fast). When the delay has elapsed, a spawn hex is free and an eligible entry exists, that entry spawns on a **random free spawn hex**; otherwise the timer stays elapsed. The range lives on `ASpawnHexConfig` (level configuration, next to the hexes), tuned in 1.7 against the 14 s crossing time. A per-wave override is not added until play asks. |
| D15 | Animation | **Walk, basic attack and death are in scope** (1.8, 3.8), played from C++ as montages on the `UpperBody` slot that every stock Paragon ABP already has (`ANIMATION_FINDINGS.md`). **A spawning enemy holds still for a per-enemy `SpawnHoldSeconds`** while the ABP's own `LevelStart` montage plays, sped up from C++ to fit the hold, and only then starts walking (1.8; the user's call 2026-11-19, replacing "the walk overrides `LevelStart`"). No new or edited Animation Blueprints; the Paragon folders stay as imported. The clips are fields you assign on the data assets. Ability animations wait for mana. |
| D16 | When does an attack's damage land? | **After a per-unit `AttackHitDelaySeconds`** (a data field, tuned by eye), because no montage in the packs has an impact notify to hang it on. It fizzles if the target died or became untargetable during the delay. Phase 2 stays instant until 3.8 adds it. |
| D17 | Round-start heal | **`RoundManager` applies an instant heal-to-full GameplayEffect to every living board champion on entering Prep** (bench champions take no damage). It is an effect on surviving actors, never a reset or respawn. Dead champions stay dead (D4). Built in 2.7. |
| D18 | Route planning and units | **The search ignores units entirely** (`CLAUDE.md` "Terrain redirects. Units stop."). `OccupiedTileCost` is removed from `FHexPathfinder`, `UBoardConfig` and `UHexGrid` (1.2b). An enemy whose next step is a champion adopts it as target (3.2); one whose next step holds another enemy waits (default; an equally short free alternative step is the first fix if queues look bad). |
| D19 | Who owns round flow? | **Two world subsystems.** `URoundManager` (1.4a): phase (`Prep`/`Combat`), `StartRound`, round index, `DT_Rounds` lookup, leak counters, the prep heal. `UWaveManager` (1.4b): the current wave — roster, progress, spawn windows, spawn timer — and tells `RoundManager` when the wave resolves. |
| D20 | Spawn windows | **Progress = spawned ÷ roster size** (revised 2026-11-19 at the user's request after 1.6: it was resolved ÷ roster size, which made Late enemies wait for the rest of the wave to die or leak). Early is open while progress < `EarlyWindowEnd` (placeholder 0.10), Late once progress ≥ `LateWindowStart` (placeholder 0.95) **or once every non-Late enemy of the wave has spawned** (added in 1.4b: Late entries above 5% of a roster could otherwise hold progress below the threshold forever; under spawned counting this fallback nearly always fires first, and `LateWindowStart` is kept anyway), Any always. An Early entry still unspawned when its window closes becomes Any, so no entry is stranded. Thresholds live in `UTerraboundSettings` (project-wide wave rules, beside the star multipliers). **Order (1.4b):** Early entries are first in line while their window is open; otherwise the next enemy is uniformly random among the pending enemies (an entry with Count 3 is three times as likely). An enemy counts as resolved when its actor is destroyed (exit today, death from Phase 2); resolution decides only when the wave ends, not the windows. |

---

## Carried forward from Checkpoint 1

- **Crossing time:** empty board is 7 hexes; at **2 hexes/s** that is **3.5 s**. This seeds
  `EnemyData` speed and the spawn timer (D14); since 1.7 it is 0.5 hexes/s and 14 s. `OccupiedTileCost` (100, on `DA_BoardConfig`) is
  retired by 1.2b (D18).
- **`UHexGrid::SetOccupant` overwrites.** It does not check occupancy, which is right for
  placement and wrong for a unit racing another into a hex. Task 1.1 adds a claim that fails.
- **`ADebugPathWalker` is throwaway.** Delete it and `SpawnPathWalker` when `EnemyBase` walks (1.7). Done in 1.7.
  Keep `FHexPathfinder` and its tests.
- **Star-up exists.** Units can be destroyed by merging, selling, and (new) dying. None recreates.
- **Still open, deliberately:** leak cost, aggro radius beyond range, checkmate, scoutable round UI.

---

## Invariants that bind this checkpoint

From `CLAUDE.md`, restated because each has a concrete consequence below.

**Terrain redirects, units stop.** The search ignores units; only terrain is impassable. One unit
per tile is enforced by the step: an enemy claims its next hex when it *begins* the step and
releases the one it left. Checking on arrival lets two enemies mid-move into one hex both complete.
Drift may decide who wins a race; it never puts two units on one tile.

**Movement is one hex at a time, no stored path.** On each arrival, target check first. Locked and
in range: stop and attack. Locked and out of range: step toward a free hex within range of it. No
target: step toward the exit. "Toward" is lower *path* distance, never raw hex distance. A next
step onto a champion makes that champion the target; a next step onto an enemy waits.

**Champions never move during a round.**

**Targeting stays out of GAS.** `TargetingComponent` selects; the ability receives the target as
data. No `AGameplayAbilityTargetActor`. `State.Untargetable` is a tag the scan filters on.

**Nearest uses path length; in range uses hex distance.** Range checks never run the search.
Acquisition is two conditions (in range **and** a free hex within range, the enemy's own hex
counting), filtered *before* ranking survivors by path length.

**Enemy range is 1 or 2 hexes.** Never scaled with wave number. Stored in hexes.

**Execution-order drift is accepted.** No stable processing order, no deterministic tie-break, no
replay guarantee. Do not add one.

**HP persists across the waves of a round; death is permanent.** HP lives on the unit's ability
system component and survives because the actor does. The prep heal (D17) is an effect, not a
reset. No wave- or round-reset path may destroy and recreate units.

**GAS owns** health, attack damage, attack speed, abilities and their cooldowns, and stat effects
such as the star multiplier. **GAS does not own** target selection.

**Traits are GameplayTags.** No trait effects this checkpoint, so nothing new touches them.

**Blueprint exposure.** Every number below that is a tuning value is `EditAnywhere` on its data
asset or config. Live runtime state (HP, target lock, current coordinate) gets getters, not
`EditAnywhere`.

**Not built, per `CLAUDE.md`:** a chosen leak cost (counter and event only; forfeited kill gold
needs no code), an HP crystal, line-of-fire blocking, a ranged archetype, Runners, paid or
between-wave healing, stuck-wave timers.

---

## Layout

New this checkpoint; nothing else is created.

```
Source/Terrabound/
├── Units/          + EnemyBase
├── Combat/         + TargetingComponent, TargetingRules
├── Abilities/      + CombatAttributeSet, GA_BasicAttack, damage/heal/star GameplayEffects
├── Waves/          + SpawnHexConfig, RoundManager, WaveManager, WaveDefinition
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

The "lone champion is routed around" behaviour came from the flat occupied-hex cost, which the
2026-11-17 revision retires (1.2b).

### 1.2b Route planning ignores units (`Pathfinding/`, `Grid/`, `Data/`)
**[C++ + Editor]** — added 2026-11-17 (D18); do it after 1.3 is committed, before 1.4a.
**C++:** `FHexPathfinder::TryGetEnterCost` stops reading `Occupant`; only terrain (`bIsWalkable`)
affects the search. Remove `OccupiedTileCost` from `UBoardConfig`, its cached copy and getter on
`UHexGrid`, and the comments that describe the flat cost (`HexPathfinder.h`, `EnemyBase.cpp`). An
enemy whose next step is occupied keeps holding, as 1.2 already does; adopting a blocking champion
as target is 3.2. Rewrite the tests built on the old model: `HexPathfinder.OneOccupiedHex` and
`.FullOccupiedRow` assert the distance field is unchanged by occupants; `EnemyBase.LoneBlocker`
asserts the enemy never enters the champion's hex and holds when its next step is that hex (with
random ties it may also pass on the other diagonal — the fixture must account for that);
`EnemyBase.SealedRow` asserts it holds in front of a full row.
**Editor:** none required. `DA_BoardConfig` keeps a harmless stale serialized `OccupiedTileCost`
until it is next saved.

**Done when:** the build is clean, every headless test passes, and nothing in `Source/` mentions
`OccupiedTileCost`.

**Done (2026-11-18).** As built: `TryGetEnterCost` returns `PathCost` only; the field, cache and
getter are gone. `OneOccupiedHex`/`FullOccupiedRow` compare the field against the empty board's,
and `OneOccupiedHex` also requires the step to pick the occupied hex at least once in 50. To get a
deterministic hold, `LoneBlocker` gained a first pass from an edge tile whose *only* back-row
neighbour is blocked: the enemy holds there and doesn't route round. The two-exit pass stays and
passes either way. `SealedRow` now opens its gap on one of the enemy's own next steps, because a
gap further along the row no longer pulls it sideways. Mutation-checked (restoring an occupant cost
fails all three rewrites). `SealedRow` is a legality test and passes under both models.

### 1.3 Spawn hex config (`Waves/`)
**[C++ + Editor]**
**C++:** `ASpawnHexConfig`, a level-placed actor with `TArray<FHexCoord> SpawnHexes` (D12). The
spawn delay range (D14) is added to it in 1.4b, where it is first read. On `BeginPlay` it calls `UHexGrid::SetSpawnFlag` for each hex. A
player-zone hex is an error: logged, its flag explicitly cleared, kept a normal player hex. An
off-board hex is logged and skipped. The grid never holds a copy of the array; this is a value read
once at startup. Spawn hexes get their own visual state, appended to `EHexTileVisualState`, and the
grid fires a native `OnTileFlagsChanged` so the visualizer repaints regardless of `BeginPlay` order.
**Editor:** add the new state's branch to `M_HexTile`; place one actor in `MainBoard`, set 3
far-edge spawn hexes.

**Done when:** the flagged hexes render as spawn hexes and are not placeable. The `SetSpawnFlag`
console command may stay as a debug tool.

**Done (2026-11-18; PIE-verified by the user).** As built: `Waves/SpawnHexConfig.*`, visual state
`Spawn` = 9 appended to `EHexTileVisualState` (`M_HexTile` branch added), native
`UHexGrid::OnTileFlagsChanged(Coord)` fired by `SetSpawnFlag` only when the flag actually changes,
and one headless test (`Terrabound.Waves.SpawnHexConfig.Apply`); 21/21 pass. `MainBoard` holds one
config with spawn hexes (1,0), (3,0), (5,0); `SpawnEnemy DA_Enemy_Grux 3 0` works with no manual
`SetSpawnFlag`. `SetSpawnFlag` itself accepts any valid coord: refusing a player-zone spawn hex is
the config's rule, not a grid invariant. `ApplySpawnHexes()` is public so tests can call it.

### 1.4a `RoundManager` (`Waves/`)
**[C++]**
`URoundManager : UWorldSubsystem`, owning the run's flow (D19).

- **Phase:** `Prep` or `Combat`, `GetPhase()`, `OnPhaseChanged`. `StartRound()` is valid only in
  Prep, and refuses while a unit is carried (same reason `CanBuy` does). Combat spans every wave of
  the round (D0).
- **Rounds:** `FRoundRow : FTableRowBase` holds its list of waves, each a roster of
  `{TSoftObjectPtr<UEnemyData>, Count, SpawnWindow}` (D13). `DT_Rounds` is registered in
  `UTerraboundSettings` like the pool tables. Rounds past the table repeat the last row.
- **Flow:** `StartRound` hands wave 1 to `WaveManager`; each time `WaveManager` reports its wave
  resolved, the next wave starts at once (a wave gap was added later, see D0). After the last wave, the round ends: back to
  Prep, `RoundIndex` up. The prep heal (D17) hooks in here in 2.7. No income is granted (D3).
- **Leaks:** `OnEnemyExited` increments `LeaksThisRound` and `LeaksTotal`, logged per round and
  cumulatively. **No cost, no HP bar, nothing that reads the counter.**
- **Debug:** `StartRound`, `RoundDump`.

Until 1.4b exists, a wave can be stubbed as "resolved immediately" so the phase flow is testable
headless; the stub is removed by 1.4b.

**Done when:** a headless test drives Prep → Combat → (waves) → Prep with `RoundIndex` advancing,
and `StartRound` refuses in Combat and while carrying.

**Done (2026-11-18; PIE-verified by the user).** As built: `Waves/WaveDefinition.h` (`ESpawnWindow`,
`FWaveRosterEntry`, `FWaveDefinition`, `FRoundRow`; rows read in table order, names ignored),
`Waves/RoundManager.*`, `RoundsTable` in `UTerraboundSettings` (the picker is filtered to `FRoundRow`
tables), and the `StartRound` / `RoundDump` commands. Approved changes to the text above:
- **No stub.** Waves wait for `NotifyWaveResolved()`, the hook 1.4b calls, and the tests call it by
  hand. An immediate stub would have made Combat impossible to observe.
- **1.4b wires the leaks.** `NotifyEnemyExited()` counts them, but `OnEnemyExited` is per enemy, so
  `WaveManager` binds it on each enemy it spawns. Enemies from the debug `SpawnEnemy` command never
  count.
- **`OnPhaseChanged` is native.** `BlueprintAssignable` waits for a widget.
- **The carried-unit refusal is checked in PIE**, because the test world has no player controller.
- **The Start key (D2) is deferred to 1.7.** Console only for now.

Tests `Terrabound.Waves.RoundManager.Flow`, `.Leaks` and `.Refusals` are mutation-checked. Each
test world logs a "no RoundsTable" warning until 1.6 creates `DT_Rounds`.

### 1.4b `WaveManager` (`Waves/`)
**[C++ + Editor]**
**C++:** `UWaveManager : UWorldSubsystem`, owning the current wave (D19).

- **Spawn timer (D14):** first enemy at once; then a delay drawn from
  `[MinSpawnInterval, MaxSpawnInterval]`, added to `ASpawnHexConfig` (placeholders 1.0 / 2.0 s,
  `Min` ≤ `Max` validated). When it has elapsed, a spawn hex is free and an entry is eligible, spawn
  it on a random free spawn hex via `TryOccupy`; else stay elapsed. Spawn hexes are read from the
  grid's flags (set by 1.3); only the delay range is read from the actor.
- **Progress and windows (D20):** progress = resolved ÷ roster size, where resolved counts deaths
  and exits (revised 2026-11-19 to spawned ÷ roster size; see D20). Early / Any / Late eligibility
  as in D20, with a missed Early entry becoming Any.
- **Resolution:** the wave is resolved when its roster is fully spawned and none of its enemies is
  alive; it then tells `RoundManager`.
- **Debug:** `WaveDump` (roster left, progress, timer).

**Open for this task's plan:** which eligible entry spawns first (roster order or random), and
whether the timer's random draw needs a seed (the drift rule says no).
**Editor:** set the delay range on the `MainBoard` Spawn Hex Config actor.

**Done when:** `StartRound` runs a small round: each wave trickles in on the free spawn hexes at
the configured rate, crosses, exits and is counted; Late entries wait for progress; the next wave
starts only when the previous one is resolved; and the round resolves back to Prep. Headless tests
cover progress counting exits, a missed Early entry becoming Any, and occupancy delaying (never
advancing) a spawn. (Nothing earns gold yet: enemies only exit in this phase. Kills, and so income,
arrive in 2.2.)

**Done (2026-11-18; PIE-verified by the user with a minimal `DT_Rounds`: one round, two waves of 3
`DA_Enemy_Grux`).** As built: `Waves/WaveManager.*` (a `UTickableWorldSubsystem`; `Tick` calls a
public `AdvanceSpawning` for tests), `Min/MaxSpawnInterval` on `ASpawnHexConfig` (if Min > Max it
logs and swaps them), `EarlyWindowEnd` / `LateWindowStart` in `UTerraboundSettings`,
`RoundManager::StartWave` handing waves over, and the `WaveDump` command. The open questions are
settled in D20: Early goes first, then a uniform random pick per pending enemy, with no seed.
Approved additions:
- **Late also opens once every non-Late enemy has resolved** (also written into `DESIGN.md`).
- **An enemy counts as resolved when its actor is destroyed.**
- **A roster entry with no enemy asset is logged and counted as resolved.**
- **A missing `ASpawnHexConfig` warns and falls back to the default delay range.**

Tests `Terrabound.Waves.WaveManager.Timer`, `.Occupancy`, `.Exits`, `.Early` and `.Late` are
mutation-checked. `DT_Rounds` exists with that one round; 1.6 completes it.

**Revised 2026-11-19 (after 1.6):** progress counts spawned enemies, not resolved ones (D20), so a
Late entry follows the rest of its wave instead of waiting for it to die or leak. A skipped roster
entry now counts as spawned. `GetResolvedCount` became `GetSpawnedCount`, and `WaveDump` prints it.
`.Exits`, `.Early` and `.Late` were rewritten for the new rule and mutation-checked again.


### 1.5 Combat locks the board, not the bench (`Input/`, `Economy/`, `Waves/`)
**[C++]** — rewritten 2026-11-18 (D1 revised to TFT rules).
While `RoundManager` is in `Combat`:
- `BoardPlayerController` lifts only bench units, and refuses every drop onto a hex (the unit goes
  back to its slot, and hexes preview as invalid).
- `ChampionMerger` ignores board copies.
- `DebugSellHex` refuses.

Shop, reroll, buying onto the bench and bench sells are untouched. On entering Prep, `RoundManager`
calls `ChampionMerger::MergeAll` to complete trios held back by the lock. `StartRound` still refuses
while carrying.

**Done when:** in Combat, bench units can be bought, sold and moved between slots, but board units
can't be lifted and nothing lands on a hex. A trio split across bench and board merges when the round
ends, with the board copy surviving. In Prep everything behaves as before.

**Done (2026-11-18; PIE-verified by the user).** As specified. The board-unit sell guard sits in `DebugSellHex`, not
`ShopSystem::Sell`, because the debug command clears the hex before selling, so `Sell` can't tell
the unit was on the board. Test `Terrabound.Economy.ChampionMerger.Combat` is mutation-checked. A
unit carried when the round ends isn't counted by the catch-up merge; it merges on the next
purchase (accepted).

### 1.6 Enemy assets and first waves
**[Editor]**
Create in `Content/Terrabound/Data/Enemies/` one `UEnemyData` per enemy, named after the Paragon pack
it uses with `Enemy` in place of `Champion`: `DA_Champion_Grux` is mirrored by `DA_Enemy_Grux`. There
are no `A`/`B` names. Enemies reuse the champion packs TFT-style (the team tint tells the sides
apart), but each picks a *different mesh* from the pack than its champion uses. Make enough enemies
that at least one has `RangeInHexes` 1 and one has 2 (your choice of which packs), so Phase 3 can test
both. **`DA_Enemy_Grux` already exists** (range 1, made for 1.2's PIE check); the rest and `DT_Rounds`
remain. Placeholder stats, marked unresolved. Create `DT_Rounds` in `Data/Waves/` (row struct
`FRoundRow`) with rounds 1–3, each of 3 small waves (a handful of distinct enemies each) mixing
range-1 and range-2 enemies, mostly `Any` windows with at least one `Early` and one `Late` entry,
and point `UTerraboundSettings` at it.

**Done when:** `StartRound` in PIE spawns round 1's waves from data, one after another.

**Done (2026-11-19; PIE-verified by the user).** Playing it led to the D20 revision: Late entries
now wait for the rest of their wave to spawn, not to die or leak.

### 1.7 Measure and retire the walker
**[C++ + Editor]**
Watch a real round. Record the crossing time from the enemy's own log against the 3.5 s from
Checkpoint 1, and how each wave enters against the spawn timer (one lump, or a stream with an
opening, a middle and a tail). Tune the delay range on the Spawn Hex Config and wave sizes in
`DT_Rounds`, and the wave gap (`WaveGapSeconds`, placeholder 3 s).

**Tuning already done by the user (2026-11-19, while doing 1.6):** 3 s / 4 s interval, wave gap
5 s, and enemy speed 0.5 hexes/s, down from 2 (a 14 s crossing instead of 3.5 s). The user likes
this speed for now; what remains is to write the result down. 1.8's spawn hold doesn't change the
pacing: with three spawn hexes, a ~5 s hold frees a hex every ~1.7 s, faster than the 3–4 s timer.

Then delete
`Debug/DebugPathWalker.*` and the `SpawnPathWalker` command; keep `FHexPathfinder` and its tests.
Also add the **Start key** (D2), deferred from 1.4a: an input binding in `BoardPlayerController`
that calls `URoundManager::StartRound`, plus the input mapping in the editor.

**Expected, not a bug:** since routing ignores units (1.2b), any champion on an enemy's route
stops it here, and the enemies behind queue up, because nothing fights yet. Test on an empty board
or with champions out of the lanes. Task 3.2 removes this.

**Done when:** crossing time and the spawn-pacing verdict are written into this task as a
result, as in Checkpoint 1's 7.1, and the walker is gone.

**Done (2026-11-19; one PIE round on `MainBoard`, empty board, user's tuning).** As built: the Start
key is **Enter** (`StartRound` action in `Config/DefaultInput.ini`, bound in `BoardPlayerController`,
which only asks `URoundManager::StartRound`; refusals stay there). `Debug/DebugPathWalker.*` and
`SpawnPathWalker` are deleted. The C++ defaults now match the tuning (`MoveSpeedHexesPerSecond`
0.5, `ASpawnHexConfig` 3 / 4 s and a 5 s gap).
- **Crossing time: 14.00 s** for every enemy (7 hexes at 0.5 hexes/s, 0 s held), exactly as
  computed, against Checkpoint 1's 3.5 s.
- **Spawn pacing: a stream, not a lump.** Round 1 (2 waves of 3) ran 48 s from Start to Prep.
  Each wave's three enemies arrive 3.4–3.7 s apart, so a wave lasts about 21 s from first spawn
  to last exit and never has more than three enemies on the board. Each enemy crosses alone, with
  no queuing. The gap ran 5.00 s from the last exit to the next first spawn. The first spawn after
  Start came 0.56 s late, most likely the first synchronous load of the enemy asset. It isn't
  repeated later, so it's not chased.
- **Start key:** Enter during combat was refused and logged. The carried-unit refusal was not
  exercised in this run (it has been since 1.4a).
- **Noticed in content, not code:** every `DA_Enemy_*` had Display Name "Grux" (copied from
  `DA_Enemy_Grux`), so logs named every enemy Grux; the user fixed it. Round 1 of `DT_Rounds` has
  2 waves, where 1.6 asked for 3.

### 1.8 Walk animation (`Units/`, `Data/`)
**[C++ + Editor]**
**C++:** `UEnemyData` gains `WalkAnimation` (a `UAnimSequence`) and `WalkPlayRate`. `UTerraboundSettings`
gains `AnimMontageSlotName` (`UpperBody`, config: it is the same in all four packs and would only
change for a non-Paragon pack). `ABoardUnitBase` gets start and stop calls that play the sequence
as a looping dynamic montage on that slot. `AEnemyBase` starts it when a step begins, does not
restart it between consecutive steps, and stops it when the enemy holds, attacks or exits.
**Spawn hold (added 2026-11-19, D15):** `UEnemyData` also gains `SpawnHoldSeconds` (placeholder,
roughly the 5 s `LevelStart` length or less). An enemy that enters the board holds its spawn hex and
takes no step until the hold has elapsed, then starts walking and plays the walk loop. C++ sets the
play rate of the `LevelStart` montage the ABP started so it ends with the hold (no ABP edit). An enemy
in its hold **can be targeted and shot** like any enemy on a hex (the user's call, 2026-11-19); it is
not untargetable.
**Editor:** set `WalkAnimation` on each `DA_Enemy_*` to the pack's `Jog_Fwd`, tune `WalkPlayRate`
by eye so the feet don't slide at the current 0.5 hexes/s, and set `SpawnHoldSeconds`.

**Why a montage, from the findings (`ANIMATION_FINDINGS.md`):** the stock ABPs read velocity through
`TryGetPawnOwner`, which is null for our actors, so their locomotion variables never leave 0 and the
walk states never run. The clips are in-place (1.3 to 2.0 s, no root motion), and the ABP's own
`JogStart`/`JogStop` clips are 1.9 to 5.1 s for three heroes, so driving those variables from C++
is worse than playing `Jog_Fwd` directly. With `IsAccelerating` false, a montage on `UpperBody`
drives the whole body, the same route the spawn animation already takes.

**Spike first.** Confirm three things before building the rest: a looping dynamic montage on
`UpperBody` plays full-body on a non-pawn actor; C++ can find and re-rate the `LevelStart` montage
the ABP starts at spawn, and the walk takes over cleanly once it ends; and the loop is seamless.

**Done when:** enemies play their spawn animation in place for `SpawnHoldSeconds`, then visibly
walk their route, and stop walking when they hold or exit.

**Done (2026-11-19; PIE on `MainBoard`, checked by the user).** As built: `ABoardUnitBase` has
protected `PlayLoopingAnimation` / `StopLoopingAnimation`, which play a dynamic montage on
`AnimMontageSlotName` and loop it by pointing its single `Default` section at itself (3.8 reuses
them). `AEnemyBase` re-rates the active montage (the ABP's `LevelStart`) in `EnterBoard` to
`length / SpawnHoldSeconds`, takes no step until the hold has elapsed (leftover time carries into
the first step), starts the walk loop when a step begins, and stops it on a hold or exit.
`SpawnHoldSeconds` is **2 s** (the user's pick, also the C++ default); `WalkPlayRate` is tuned per
asset by eye.
- **Spike:** all three held. The montage on `UpperBody` drives the full body, `LevelStart` was
  found and re-rated (rate 2.50 for Grux's 5.00 s clip, 2.43 for Belica's 4.87 s), and the loop
  is seamless with no fallback needed. Only Grux and LtBelica spawned in the logged rounds; Kwang
  and Serath use the same code path and slot.
- **Crossing time is unchanged at 14.00 s.** The log now reports the hold separately
  ("crossed the board in 14.00s after a 2.00s spawn hold"), so the figure stays comparable to 1.7.
- **Tests:** new `EnemyBase.SpawnHold` (mutation-checked); the other enemy and wave tests set the
  hold to 0. 32/32 pass.

### 1.9 Round HUD (`Waves/`, UI)
**[C++ + Editor]** Added 2026-11-19 at the user's request.
**C++:** `BlueprintPure` getters the widgets bind to; no decision lives in a widget.
`URoundManager`: `CanStartRound()` (true only in Prep), with `StartRound` callable from Blueprint,
plus the round number. `UWaveManager`: whether the wave gap is running, the gap time remaining,
and the current wave index and wave count of the round (exact getter names and owners settled in
the task plan).
**Editor:** a **Start button** next to the shop, enabled from `CanStartRound`, calling
`StartRound`; a **"Round N · Wave X/Y" label**; a **"Next wave in N" countdown** shown only during
the wave gap.

**Done when:** a round can be started from the button, the button is greyed out in combat, the
label tracks the round and wave, and the countdown runs during each gap.

**Done (2026-11-19; PIE on `MainBoard`, checked by the user).** As built:
- **C++.** `URoundManager` has `CanStartRound()`, which runs the same checks as `StartRound`
  through a private `GetStartRefusal()`: Prep, no carried unit, round data. `StartRound` is
  `BlueprintCallable` and logs the refusal reason. `GetRoundStatusText()` reads "Round N · Prep"
  or "Round N · Wave X/Y". The wave index and count stay on `URoundManager`.
- **C++, gap.** `UWaveManager` has `IsInWaveGap()` (a later wave, nothing spawned yet, timer still
  running) and `GetWaveGapText()` ("Next wave in N", rounded up, empty outside the gap).
- **Editor.** `WBP_RoundPanel` holds the label, the countdown and the Start button. It lives in
  `WBP_Shop`'s `ShopBar`: a full-width bar made of a Fill spacer, `CardRow`, then a Fill column
  holding `RerollButton` and `WBP_RoundPanel`. That keeps the cards centred and the column next to
  them at any resolution and any card width. `GoldText` moved to the top right, right-aligned with
  the trait panel.
- **Tick, not bindings.** The panel's Event Tick sets the button's enabled state and both texts
  from the getters. UMG property bindings can't be created through the MCP tools, and the Tick has
  no Branch or Select. `OnClicked` calls `StartRound`.
- **Tests:** `RoundManager.Flow` and `.Refusals` cover `CanStartRound` and the label;
  `WaveManager.Gap` covers the gap and the countdown. 32/32 pass.

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

### 2.7 Prep heal (`Abilities/`, `Waves/`)
**[C++]**
Replaces the withdrawn paid heal (D9). A heal-to-full GameplayEffect (sets `Health` to `MaxHealth`;
instant). `URoundManager` applies it to every living board champion on entering Prep (D17). Dead
champions were already removed (2.2) and are not restored. Nothing costs gold; there is no `H`
key and no heal UI.

**Done when:** a champion damaged in a round starts the next prep at full HP, a champion that died
stays gone, and a champion damaged in wave 1 is still damaged in wave 2.

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
differ from hex distance. Units no longer affect the search (D18) and there is no terrain yet, so
(3) needs a non-walkable tile set up as a test-only stand-in for terrain.

### 3.2 Enemy decision loop (`Units/`, `Pathfinding/`)
**[C++]**
On each arrival, target check first:

- **Locked, in range:** stop and attack.
- **Locked, out of range:** distance field seeded at the *free hexes within range of the target*
  (`ComputeDistanceField` already takes several sources), step to the lowest neighbour.
- **No lock:** scan; if found, treat as above; otherwise the exit field, as in 1.2.

A blocked step depends on what holds the hex (D18):
- **A champion** → it becomes the target (dropping any other lock) and the enemy attacks from where
  it stands.
- **Another enemy, while walking to the exit** → wait and re-decide next tick (the default).
- **Another enemy, while heading for an attack hex** → a contested hex, handled in 3.3.

Nothing is stored between arrivals.

**Done when:** an enemy whose route runs into a champion stops and fights it; enemies queued behind
it wait; a full row of champions no longer stalls the wave forever (the wall is attacked instead).

### 3.3 Contested hexes
**[C++]**
The loser of a `TryOccupy` race **keeps its lock** and sidesteps: if another hex within range of
the same target is free, it heads there; if none is, it **waits where it is** and re-checks next
tick until one opens. It never falls back to exit-pathing. One re-decision per tick per enemy, so a
pathological board cannot loop.

**Done when:** a headless test with two enemies and one free attack hex shows exactly one takes
it and the other keeps its lock and waits (then takes a second free hex if one opens); a stress
test over many interleavings never leaves two units on a tile.

### 3.4 Enemy attacks
**[C++]**
Enemies use the same `UGA_BasicAttack` against a locked target in range. A champion killed goes
through the death path from 2.2.

**Done when:** an unprotected shooter is killed by enemies that reach it, and its tile and pool
copies are handled.

### 3.5 Lock breaking, verified
**[C++]**
No new mechanism: this checks that 2.3's single drop path covers every case for enemies too.
`SetUntargetable` (2.6) makes enemies locked on that champion drop and re-scan; an enemy locked on
one champion whose next step is another champion's hex switches to the blocker; a target killed by
a shooter drops every enemy locked on it. (A target whose attack hexes all fill is *not* a lock
break any more: the enemy waits, keeping its lock, 3.3.)

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
Buy, place, `StartRound`. Watch enemies trickle in, stop at the champions in their way, and get
killed by shooters, and each wave follow the last once it resolves. See HP persist across the waves
of a round, living champions return to full HP at prep and a dead one stay gone, see each kill's
`+N` and confirm nothing is spendable until the round ends, merge and sell in prep, start the next
round.
Confirm no errors and that grid data stays consistent: no unit ever on two tiles, no orphaned
occupant.

**Done when:** three rounds run back to back with no errors.

### 4.2 Playtest questions, answered in writing
**[Editor + C++]**
Record the answers here, as Checkpoint 1's 7.1 recorded crossing time. These are the questions
`DESIGN.md` §8 says only play can answer:

- **Does one cheap blocker trivialize a wave?** It now holds everything queued behind the enemy it
  fights. And does a full-row wall? Lower `UnitCap` toward 6 and see.
- **Crossing time against the spawn timer:** does a wave read as a stream with an opening, a middle
  and a tail, or as a lump? What delay range and wave size feel right?
- **Do queues look bad?** Enemies waiting behind enemies while an equally short free step exists
  (the D18 fallback is the first fix).
- **Does the cap change what you buy and place?**
- **Do rounds have a shape?** Does damage carried across three waves make wave 3 feel harder, and
  does the free prep heal plus permanent death give rounds real stakes?
- **Do leaks happen at all** against a competent board (informs whether a cost is needed)?
- **How hard does forfeited kill gold punish a leak on its own?** It is already a punishment. Does
  the run need a further chosen leak cost at all, or is the spiral already fast enough?
- **Does a wave ever stall** (enemies clogging the spawn hexes, so the wave never finishes
  entering and the round never advances)?

**Done when:** each has a written answer, and the next plan can start from them.

---

## Definition of done

- [ ] Enemies spawn on configured hexes, walk one hex at a time with no stored path, and exit
- [ ] `TryOccupy` is atomic; no test or PIE run ever puts two units on one tile
- [ ] Route planning ignores units; `OccupiedTileCost` is gone; occupancy is enforced by the step
- [ ] A wave trickles in on one randomized timer from random free spawn hexes, with spawn windows
      gated on progress (share spawned); a missed Early entry becomes Any
- [ ] A wave resolves when its roster is spawned and none of it is alive; the next wave starts only
      then (after the wave gap), and the round ends after its last wave. No prep timer, no
      stuck-wave timer
- [ ] `RoundManager` owns phase and round flow; `WaveManager` owns the current wave
- [ ] Leaks are counted per round and in total, with no cost and nothing reading the counter
- [ ] In combat the board is locked (no lifting board units, no drops on hexes, no merges using
      board copies) while shop, bench buys, bench sells and bench drags stay open; held-back merges
      complete on entering prep; `StartRound` refuses while carrying
- [ ] Every board unit has an ability system component; attributes come from data assets
- [ ] Targeting is not routed through GAS; `TargetingComponent` selects and passes data
- [ ] Damage, death, kill gold and pool return all work through one path
- [ ] Kill gold is the only income: granted on death, `+N` shown at the gold counter, unspendable
      until the round ends; no flat per-wave income exists
- [ ] HP persists across the waves of a round; living champions heal to full on entering Prep; dead
      champions stay dead; no path destroys and recreates a unit
- [ ] Star-up scales health and damage by configured factors; attack speed and range stay flat
- [ ] Enemy acquisition needs both conditions, filtered before path-length ranking
- [ ] An enemy whose next step is a champion adopts it as target; one blocked by an enemy waits
- [ ] Lock breaks through one mechanism (dies, untargetable, leaves range, blocking champion)
- [ ] Contested hexes resolve by re-check; the loser sidesteps or waits, keeping its lock
- [ ] Unit cap enforced for bench-to-empty only; preview shows invalid at the cap
- [ ] No paid heal and no damaged-sell rule exist
- [ ] Enemies play a walk loop, and attacks and deaths play their clips, with no new or edited
      Animation Blueprint and the Paragon folders untouched
- [ ] Enemies hold still through their spawn animation (`SpawnHoldSeconds`) before walking
- [ ] A Start button next to the shop starts the round; a label shows round and wave; a countdown
      runs during each wave gap
- [ ] `ADebugPathWalker` and `SpawnPathWalker` removed
- [ ] Crossing time and spawn-pacing verdict written down
- [ ] Playtest questions in 4.2 answered in writing
- [ ] No mana, armor, cast ability, terrain, or chosen leak cost anywhere

---

## Out of scope — do not build, do not scaffold

Assassins, the leap, mana, armor, and any cast ability (the next checkpoint's first ability).
Terrain, trees, `TerrainPieceBase`, `PlacementValidator`, seal prevention. Trait thresholds and
effects. Any change to sell value. A chosen leak cost (forfeited kill gold, D3, is already one and
needs no code). The checkmate lose condition (a run with no units will simply stall). Scoutable
round display. The endless scaling formula for rounds. A prep timer, a
stuck-wave timer or safety net (the per-wave spawn timer, D14, is the only timer). A per-wave
spawn-delay override. The "equally short alternative step" for queued enemies, until 4.2 asks for
it. Taunts and interrupts. HP bars, damage numbers, world-space popups and other UI (the HUD `+N`
gold popup, 2.9, and the round HUD, 1.9, are the exceptions). Interest and streaks. Ability animations (with
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
  montage on every spawn, including enemies; 1.8 holds the enemy in place for it and re-rates it
  to fit `SpawnHoldSeconds`.
- **Attack timing is approximate.** There is no impact notify, so the hit delay is tuned by eye
  per hero (D16). Paragon animation timing is tuned for a MOBA camera and may still want
  adjusting at this zoom.
- **Do not "Save All" after an ABP inspection.** Reading ABP graphs through the MCP marked all four
  ABPs dirty in memory; nothing was written.
- **Phase 1 stalls at any champion on a route** until Phase 3 lands (1.7).
- **Enemies queue behind each other** by design; whether the queues look bad is a 4.2 question.
- **Enemies shoot through trees that don't exist yet.** Range is hex distance by design.
- **A blocker wall may be too strong.** That is a 4.2 question, not a bug.
- **A leaking enemy drops no gold.** That is a real punishment, already in place and needing no
  code, and any leak cost chosen later stacks on top of it. Size that cost knowing leaks already
  hurt. Nothing else reads the counter.
- **Phase 1 rounds award no gold** (kills arrive in 2.2). Fine: the player spends part of
  `StartingGold` in the first prep and keeps the rest.

---

## After this checkpoint

Stop. Do not continue into assassins or terrain. The plan is rewritten from what the fights
actually feel like. Likely next: the first cast ability with mana (assassins), and one tree
(Step 4).
