# CLAUDE.md — Terrabound

Instructions for Claude Code working in this repository.

- `DESIGN.md` — full design rationale. Source of truth for design intent.
- `CLAUDE.md` — this file. Architecture invariants and working style. Applies always.
- `PLAN.md` — the current checkpoint's task breakdown. Source of truth for what to
  build right now and in what order.

## Project

Single-player roguelike fusing TFT-style autobattler mechanics with tower defense.
Player buys champions from a shop, places them on a hex board, and defends an
exit behind the back row against waves of enemies. Endless run, chase a high
score.

**Engine:** Unreal Engine 5.8
**Language:** C++ for all logic. Blueprints for data and assembly only.
**Team:** solo project.
**Status:** Checkpoint 1 in progress — board, controls, shop, placement. No
enemies, no combat, no GAS yet. See `PLAN.md`.

## Build command

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" TerraboundEditor Win64 Development -Project="C:\Users\Phillip\Documents\Unreal Projects\Terrabound\Terrabound.uproject" -WaitMutex -FromMsBuild
```

If a build fails due to a locked DLL, the editor is open. Tell me to close it
rather than trying to work around it.

## How to work with me

- **Plan before executing.** When I say "let's do task X," post a plan and wait
  for approval before creating or editing anything — every task, not just ones
  touching more than one file. The plan states: what you'll do step by step
  (files touched, classes and their parents, which of them owns what data, C++
  vs Blueprint per the split below); what I need to do in the editor, sequenced
  against your steps; and any design decision the task touches that isn't
  already settled by `DESIGN.md`, `CLAUDE.md`, or `PLAN.md` — surfaced here
  with a recommendation, never picked silently. I'll discuss changes or
  approve; execution starts only on approval.
- **Small diffs.** One system per change. Do not refactor adjacent code you were
  not asked to touch.
- **Scope your file list.** State which files you will create or modify before
  starting, and stay inside that list.
- **Keep asking as you go.** If execution turns up a decision the plan missed,
  stop and raise it the same way rather than guessing. Do not invent gameplay
  behaviour — the plan is expected to be imperfect, not the last chance to ask.

## Precedence

1. **Architecture invariants** in this file. These are expensive or impossible to
   reverse. Do not violate one without raising it first, even if `PLAN.md` appears
   to ask for it — that would be a bug in `PLAN.md`.
2. **`PLAN.md`** for build order, task scope, and what is out of scope now.
3. **`DESIGN.md`** for design intent — what the game is and why. Where `PLAN.md`
   and `DESIGN.md` disagree, `PLAN.md` wins on *sequencing and scope* and
   `DESIGN.md` wins on *design intent*. A `PLAN.md` task that contradicts
   `DESIGN.md` about how a mechanic should work is a bug in `PLAN.md`.

If a task requires a design decision that is in none of the three, flag it rather
than guessing. Most open questions are deliberately unanswered until there is
something playable.

## Architecture invariants

**The grid is data, not actors.** Tiles live in a flat array of structs
(coordinate, walkable flag, occupant ref, terrain ref, path cost). Visual hex
meshes read from that array. Pathfinding must never query an actor, trace, or
overlap. If a task seems to need pathing to know about an actor, the tile struct
is missing a field — add the field, don't add the trace.

Blueprints change live tile state through `BlueprintCallable` functions on the
grid subsystem rather than holding it. Never mark `FHexTile` or its fields
`EditAnywhere` or `BlueprintReadWrite` — a Blueprint asks the grid to change a
flag; the grid owns the array. Level *configuration* consumed once at generation
(the per-level spawn hex array) is an input, not a mirror, and is fine in a level
Blueprint. See "Blueprint exposure" under Working style.

**Axial coordinates `(q, r)`.** Convert to cube for distance. Never store offset
coordinates; they turn neighbor lookups into parity special-cases.

**Pointy-top hexes, odd-r offset rows.** All 8 rows are 7 wide and stagger
against each other, laid out like the TFT board. Which parity shifts right is
fixed once and never changes — every neighbor lookup depends on it. Currently
locked to **odd rows shift right (+X)**.

**Terrain blocks pathing. Units do not.** Terrain is impassable in the A* cost
function. Enemies path *through* a unit's hex and engage when a player unit
enters their aggro range — not on adjacency. A ranged enemy stops several hexes
short of the unit it acquired. Keeping these distinct is load-bearing: if units hard-block, players maze with bodies and
terrain becomes redundant. Concretely: `Occupant` and `bIsWalkable` are
independent fields. Never derive one from the other.

**One unit per tile. Passable and occupiable are different questions.** A hex
holds at most one `ABoardUnitBase` — champion or enemy, no stacking. This does
not contradict the invariant above: an enemy may path *through* a champion's hex
but may not *stop* on it. `bIsWalkable` answers "can a path cross this tile";
`Occupant == nullptr` answers "can a unit stand here". Code that conflates the
two will either let units stack or make units block pathing, and both are wrong.

When the tile an enemy wants to stop on is taken, it moves to another tile that
still has its target in range. If none is free it falls through to the ordinary
"nothing in range → path to the exit" branch. No special case — an enemy that
cannot attack is an enemy with no target.

**Range is measured in hexes.** Store champion and enemy range as a tile count.
Convert to world units at query time. Never store range in centimeters.

**Grid A*, not NavMesh.** Deterministic tile costs and dynamic terrain placement
want a hand-rolled A* over the tile array. NavMesh rebuilds mid-wave are a
headache to avoid.

**The player zone depth is a config value, not a constant.** Currently 5 rows.
This number will change. Never hardcode it or derive from it. Derive the zone
as rows `BoardDepth - PlaceableRowCount` through `BoardDepth - 1`.

## Gameplay Ability System

The project uses GAS for stats, abilities, and trait effects.

**Owned by GAS:** health, mana, attack damage, attack speed, armor, and any other
combat attribute; abilities with costs and cooldowns; trait bonuses as
GameplayEffects; buffs, debuffs, and status.

**Not owned by GAS:** target selection. The aggro model is custom (path-length
nearest, lock until death, two-condition acquisition) and does not map onto
`AGameplayAbilityTargetActor`. `TargetingComponent` selects the target and passes
it to the ability as data. Do not route targeting through GAS targeting actors.

**Traits are GameplayTags.** Trait counting is tag counting across the board;
threshold checks are tag queries; a trait bonus is a GameplayEffect applied to
every unit carrying the matching tag. Do not build a parallel trait system. This
holds from the first line of trait code, before GAS itself exists — GameplayTags
is a separate, cheap module and is in use during Checkpoint 1.

**Untargetable is a tag** (`State.Untargetable`). The targeting scan filters on
it. This is the general lock-break mechanism — stealth, blinks, and anything
later just apply the tag.

**Attributes must persist across waves.** Units keep their HP between waves; the
default GAS assumption of "reset each round" is wrong here. Unit actors survive
between waves and are never destroyed and respawned. Selling a champion is an
intentional removal and is exempt; no board-refresh or wave-reset path may
destroy and recreate units.

**GAS is not introduced until Step 2 of the design-rationale build order** — the
first champion that shoots something. Checkpoint 1 sits before that and contains
no AttributeSet, no AbilitySystemComponent, and no GameplayEffects. Stat fields on
`ChampionData` are initialization data for a future AttributeSet: stored, read by
nothing. Do not build a runtime stat system that GAS will later have to displace.

## Board

- 7 hexes across, 8 rows deep, 56 total
- **No crystal, no nexus.** An enemy reaching the back row exits and despawns.
  A leak increments a counter and currently costs nothing. A cost **is** planned;
  what it is has not been decided. Do not add an HP bar, and do not invent a
  penalty — implement the counter and the leak event, and leave the cost off
- Enemies path to a single virtual goal node with zero-cost edges from every
  back-row hex, so the A* stays an ordinary single-target search
- Player owns the 5 rows nearest the exit = 35 placeable hexes, shared by
  units and terrain. "Front" is from the player's viewpoint and means the
  **high** row indices: rows 3–7, with row 7 the back row adjacent to the
  exit. Rows 0–2 are the enemy side. Getting this inverted is an easy and
  expensive mistake
- Spawn hexes are a per-level array of enabled tiles, chosen by the designer.
  Never player-facing, never a runtime count. Spawn hexes are not placeable.
- Per-tile flags kept independent: `is_spawn`, `is_placeable`, `is_walkable`
- One unit per tile. `Occupant` is a single reference to `ABoardUnitBase`, which
  is why champions and enemies share that base — one field covers both sides.
- No unit cap. Revisit only if traits become trivially maxable.

## Shop and bench

- Player buys champions from a shop of fixed slots, with a gold reroll. Champions
  are drawn from a shared pool with tier-weighted odds and returned to it on sell.
- **Buying puts a champion on the bench. Never on the board.** There is no code
  path where a purchase spawns a unit onto a hex. Every champion reaches the board
  by being dragged there from the bench.
- Bench slots are drag sources and drop targets with the same interaction as
  hexes. Bench → board, board → bench, and bench → bench are all supported.
- A full bench blocks buying. The player must place or sell first.
- Dropping a champion onto an occupied hex swaps the two, as in TFT.
- Bench champions do not count toward trait totals, take no damage, and do
  nothing. Only the board counts.
- Bench slots are **not hexes** and are not part of the tile array. They are a
  separate small array of occupant refs. Do not extend the grid to cover them.
- **`Bench` owns that array.** `ShopSystem` asks it for free space and hands it a
  champion; the drag system asks it to add and remove. Neither holds bench state.
  A shop that owns the bench ends up owning placement, which is the wrong shape.
- The one exception to buy-to-bench is a debug console command used before the
  shop exists. It is debug-only and never reachable in normal play.

## Targeting

TFT rules. Getting this subtly wrong produces bugs that look like animation
problems, so implement it exactly:

- Aggro range and attack range are the same value
- **Acquisition needs two conditions, not one:** the candidate is in range by hex
  distance, **and** at least one hex within range of it is unoccupied. The
  enemy's own hex counts. Both tests are cheap — filter on them before ranking
  survivors by path length, so A* never runs on candidates about to be discarded
- **No target:** scan every tick for the nearest valid candidate, acquire
- **Has target:** hold until the target dies. No re-scan, no timer
- **No valid candidate:** continue pathing toward the exit
- **"Nearest" uses path length. "In range" uses hex distance.** Two measures, two
  questions, deliberately not unified. Range checks must not run A* — they happen
  every tick. Consequence accepted: enemies shoot through trees; terrain does not
  block line of fire
- The free-hex test belongs in acquisition, not in a fallback after it. An enemy
  that locks a target it cannot reach a firing position for would drop and
  re-acquire every tick while walking past — the exact oscillation the lock
  prevents
- **Enemy range is 1 or 2 hexes.** Melee is 1 (adjacent); range 0 is impossible
  under one-unit-per-tile. This is engagement density, not a ranged archetype —
  it stops the wave queuing behind one blocker's six neighbours. Do not scale
  range with wave number; scale the proportion of range-2 enemies instead
- **Assassins, not Runners.** Runners are cut. An assassin paths and is blocked
  by terrain like any other enemy. Its **leap is a mana-gated ability** that
  **always resolves** — no cancel, no refund, no retarget. It ignores terrain, it
  does **not** ignore occupancy (a packed backline has no legal landing hex), and
  when no legal landing hex exists it **leaps to the hex it already occupies**,
  spending the cast and keeping its current target. Leap distance is a per-enemy
  stat in hexes. After a kill it re-scans normally rather than walking off. Needs
  GAS, so not an early enemy

**Contested hexes: re-check, don't reserve.** Take occupancy when an enemy
*begins the step* into a hex and release the one it left. Checking only on arrival
lets two enemies mid-move into the same hex both complete, which stacks them. The
loser of a race drops its lock and re-scans on the same tick — another free hex in
range of the same champion means it re-acquires that champion, and only if none
exists does it fall through to exit-pathing. Dropping the lock is not walking away.

**Execution-order drift is accepted, not fought.** The logic is deterministic; the
execution is not perfectly so, and identical boards may resolve slightly
differently. Do not add a stable processing order, a deterministic tie-break for
"nearest", or a replay-determinism guarantee. This is a deliberate call, not an
oversight — do not "fix" it.

**One unit per hex is the exception and is enforced.** Drift may decide who wins a
race for a hex; it may never put two units on one. That comes from the occupancy
check on the step, not from ordering. Outcomes vary; legality does not.

Do not re-evaluate an existing target every tick. That causes flip-flop between
two equidistant units, resetting attack windup so neither takes damage. A
contested-hex loss is not this case — that enemy was walking, so there is no
windup to reset.

Build target loss as one general mechanism (target dies / becomes untargetable /
leaves range / loses its attack hex → drop lock → normal re-scan), not as a
stealth special-case.

## Build order

Two orderings are in play. Read both.

**Design-rationale order**, from `DESIGN.md`, sequenced to answer the riskiest
questions first:

1. **One enemy walking to the exit.** No units, no shop, no combat. Spawn, path
   across the grid, reach the back row, despawn. Establishes whether the
   board size and enemy speed produce a fight worth having.
2. **One unit that shoots it.** First real game loop. GAS arrives here.
3. **Blockers.** Melee unit on a hex, enemy engages instead of walking past.
   Where the aggro model gets its real test. Assassins come later still — the
   leap needs mana, so it cannot precede GAS at step 2.
4. **Terrain.** One tree, placed manually, no trait attached. Confirm pathing
   reroutes cleanly.
5. **Shop and traits.** Best understood, least likely to surprise.

**Current build order is `PLAN.md`**, which deliberately front-loads the board,
controls, shop, and placement so there is something playable to iterate against
before combat exists. Enemies and combat follow in a later checkpoint.

This is a sanctioned reordering, not drift. `PLAN.md` governs what gets built and
when; the list above governs *why* that sequence exists and still applies to
everything `PLAN.md` has not reached.

The one thing the reorder puts at risk is **board crossing time** — the number
every other number is tuned against. `PLAN.md` task 7.1 preserves it with a debug
path walker: A* across the grid, no AI, no combat, no `EnemyBase`. That task is
not optional, and Checkpoint 1 does not close without the number written down.

Do not skip ahead within `PLAN.md`, and do not start work beyond the current
checkpoint. The plan is rewritten after each checkpoint from what the game
actually feels like to play.

## Deliberately not built

Do not add these, do not scaffold for them, do not suggest them unprompted:
augments, bosses, items, mid-combat repositioning, enemy-side terrain,
depth-based stat bonuses, additional traits beyond the two in MVP scope,
stat degradation on wounded units, a ranged enemy archetype, an HP crystal or
nexus, Runners, line-of-fire blocking.

`PLAN.md` carries a second, checkpoint-scoped out-of-scope list covering systems
that *are* in the MVP but are not part of the current checkpoint. Both apply.
Empty folders and placeholder classes for unbuilt systems count as scaffolding.

Economy numbers are explicitly unresolved. Placeholder values only, kept in one
config file, clearly marked as placeholders.

## Working style

**C++ for all gameplay logic. Blueprints for data and assembly only.** Anything
that makes a gameplay decision, mutates game state, or runs per tick is C++.
Blueprints hold tuning values, wire up assets, and compose what C++ provides —
they do not implement behaviour. A Blueprint graph that branches on game state is
logic in the wrong place.

**Exception: graph-native tools.** Animation Blueprints (state machines, blend
spaces, transition rules), Niagara, and material graphs are logic, but they are
logic in their native form. Rewriting them in C++ is worse, not purer. They stay
Blueprints and are not violations of this rule.

The table below is the same rule applied per system, not a second rule.

| C++ | Blueprint |
|---|---|
| Tile struct, grid array, board generation | Champion/enemy variants (derive from C++ base) |
| A*, path caching, invalidation | Ability visuals, VFX, animation graphs |
| Targeting, aggro, lock/re-scan | Wave definitions, trait thresholds, tuning |
| Combat resolution, GAS attribute sets | UI widget layout and binding |
| Wave spawning, economy state | Level setup, spawn hex configuration |

The reason for the line: the moment a Blueprint owns tile state, something reads
it through an actor reference and the grid-is-data invariant quietly dies.

UI is the case most likely to drift. A widget Blueprint lays out the shop and
binds to values C++ exposes; it does not decide whether a card is affordable or
what a click does. Those are `ShopSystem` and `EconomyState` answering, with the
widget displaying the answer.

The practical reason, beyond architecture: **Blueprint graphs are binary assets
you cannot read or edit.** Logic that lives in a widget graph is logic you cannot
inspect, change, or debug, and any description of it in these docs is a guess.
Logic in `.cpp` is logic you can work on.

### Division of labour

**Work in C++ and in these markdown files. Leave the editor to me.**

Yours: `.h` and `.cpp` files, `.Build.cs`, config files, and these docs. Propose
and write those directly.

Mine: creating and editing Blueprints, data assets, materials, animation graphs,
levels, and anything else authored in the Unreal Editor. When a task needs one of
those, say precisely what to create — asset name, parent class, which folder,
which values — and I will do it and confirm. Do not treat a task as blocked
because it needs an editor step; hand me the step and carry on with the C++ side.

This is a current constraint, not a principle. If an Unreal MCP server gets
connected later, editing assets directly through it is fine and this section
should be revised rather than worked around.

### Layout

```
Source/Terrabound/
├── Grid/           HexCoordinates, HexTile, HexGrid, HexGridVisualizer
├── Pathfinding/    HexPathfinder, PathCache
├── Units/          BoardUnitBase, ChampionBase, EnemyBase, EnemyAssassin
├── Combat/         TargetingComponent, TargetableInterface, CombatResolver
├── Abilities/      AttributeSet, GameplayEffects, ability base classes
├── Terrain/        TerrainPieceBase, PlacementValidator
├── Economy/        ShopSystem, EconomyState, Bench
├── Waves/          WaveManager, WaveDefinition
└── Data/           ChampionData, EnemyData, BoardConfig (UDataAssets)

Content/Terrabound/
├── Blueprints/     Grid/ Champions/ Enemies/ Terrain/ Core/
├── Data/           Champions/ Enemies/ Waves/ Traits/ DA_BoardConfig
├── Levels/
├── UI/
├── Materials/
└── VFX/

Content/ (root, outside Terrabound/)
└── Paragon<CharacterName>/   one folder per imported pack, wherever the
                              importer puts it by default
```

This is the destination layout. Create folders as their systems get built, not in
advance — see "Deliberately not built".

**Paragon packs live at `Content/` root** (e.g. `Content/ParagonLtBelica/`), not
under `Content/Terrabound/` — wherever the importer puts them by default, left
exactly as imported. Moving a pack afterward means fixing up redirectors on
every future import for no benefit, so don't move them and don't suggest it.
`Blueprints/Champions/` is where the champion Blueprints that reference these
packs live (children of the future `AChampionBase` C++ class) — that folder
holds gameplay Blueprints, not imported character packs.

`Data/` is separate from `Blueprints/` deliberately — nearly every number in this
project is wrong and will be edited constantly, so tuning values should be
openable without loading a Blueprint graph.

### Blueprint exposure

**The test: is there any way to change this without editing C++?**

If there isn't, expose it. Nearly every number in this project is wrong and will
be retuned many times, and a value that needs a recompile to change is a value
that doesn't get tuned.

Expose, as `UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=...)`:

- Design and balance parameters — max HP, attack damage, attack speed, range in
  hexes, tier, costs, income, reroll cost, shop and bench slot counts, trait
  thresholds
- Board configuration — `BoardWidth`, `BoardDepth`, `PlaceableRowCount`,
  `HexRadius`
- Level configuration — which far-edge hexes are spawn-enabled, camera framing,
  zoom and pitch limits, drag thresholds
- Anything else where the only alternative is a code change

Also expose, as `UFUNCTION(BlueprintCallable)`, any operation a level Blueprint
or a designer might reasonably invoke, and `BlueprintPure` getters for reading
runtime state.

**Don't** expose values that already have a non-code way to change them, or that
would create a second copy of live runtime state. A unit's `CurrentCoord` is set
by dragging it; a tile's `Occupant` is set by the placement flow. Those get
setters and pure getters, not `EditAnywhere`. Two writable copies of the same
runtime value produce desyncs that present as unrelated bugs.

Level configuration is not runtime state. A per-level array of spawn coordinates
that the grid reads once at generation is an input, not a mirror, and belongs in
the level Blueprint — that is the designer workflow `DESIGN.md` calls for. The
distinction that matters is *drift*: a value read once at startup cannot disagree
with the grid afterwards, because nothing reads it afterwards. A Blueprint-held
copy of live tile state can, and is forbidden.

That generation-time array arrives with the enemy checkpoint. Until then, spawn
flags are set through a `BlueprintCallable` setter on the grid (`PLAN.md` 1.6).
Both are legitimate; neither puts tile state in a Blueprint.

**When unsure, ask.** Name the specific variable and ask whether it should be
exposed. Do not guess in either direction.

### General

- Prototype code. Prefer readable and throwaway over abstract and future-proof.
- Keep tunable numbers in data assets or a config, not scattered in code.
- Coordinate math stays pure — no world access, no side effects — so it stays
  testable. The round-trip and neighbor-symmetry tests are a hard gate; do not
  build on top of them while they fail.
