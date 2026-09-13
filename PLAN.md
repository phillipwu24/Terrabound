# PLAN.md — Terrabound, Checkpoint 1

Companion to `CLAUDE.md` and `DESIGN.md`. This document covers **one checkpoint only**.

**Scope:** the playable board. Hex grid, TFT-style mouse controls, a shop, and 3–4 Paragon
champions that can be bought and placed. **No enemies, no combat, no waves, no trait effects.**

---

## Read order

1. `CLAUDE.md` — architecture invariants and working style. Highest authority.
2. `DESIGN.md` — full design rationale. The source of truth for design intent.
3. This file — build order and task breakdown for Checkpoint 1.

**Precedence:** if this document contradicts `CLAUDE.md` on an architecture invariant, `CLAUDE.md`
wins and the contradiction is a bug in this file — raise it. The one deliberate exception is
build order, below.

---

## Deliberate deviation from CLAUDE.md's build order

`CLAUDE.md` orders the build as: one enemy walking → one unit that shoots → blockers → terrain →
shop and traits, and says not to skip ahead.

**Checkpoint 1 intentionally reorders this at the project owner's direction.** The board, controls,
shop, and placement come first, so there is something playable to iterate against before combat
exists. Enemies and combat follow in a later checkpoint, and the plan gets rewritten then.

The reason `CLAUDE.md` puts the walking enemy first is sound: it establishes board crossing time,
and every other number is tuned against it. Task **7.1** preserves that — a debug path walker with
no AI, no combat, and no enemy class, purely to measure crossing time before this checkpoint
closes.

This is the only sanctioned departure. Everything else in `CLAUDE.md` applies in full.

---

## Invariants that bind this checkpoint

From `CLAUDE.md`. Restated because each one has a concrete consequence in the tasks below.

**Grid is data, not actors.** Flat array of tile structs; visual meshes read from it. Nothing
queries an actor for tile state — including the mouse-picking code in Phase 3. If a task seems to
need a trace, the tile struct is missing a field.

**Axial `(q, r)` storage, cube for distance.** Never store offset coordinates.

**Pointy-top, odd-r offset, parity fixed once.** This plan locks **odd rows shift right (+X)**.
Never flip it.

**Units do not block pathing; terrain does.** No pathing exists yet, but this fixes the data
model: `Occupant` and `bIsWalkable` are **independent fields**. Do not write
`bIsWalkable = Occupant == nullptr`. It is convenient now and it destroys the terrain hook later.

**One unit per tile.** `Occupant` is a single reference, not a list. Champions and enemies both
occupy exclusively — no stacking. This does not contradict the invariant above: a path may cross
an occupied tile, but a unit may not stop on one. Placement (5.1) enforces this; movement will
inherit it in a later checkpoint.

**Per-tile flags independent:** `bIsSpawn`, `bIsPlaceable`, `bIsWalkable`, plus occupant and
terrain refs.

**Player zone depth is config, never a constant.** Currently 5 rows / 35 hexes. Never hardcode it
or derive from it. "Front 5 rows" is from the player's viewpoint and means the **high** row
indices — rows 3–7, row 7 being the back row adjacent to the exit. Rows 0–2 are the enemy
side. Derive the zone as rows `BoardDepth - PlaceableRowCount` .. `BoardDepth - 1`.

**Range is measured in hexes.** Store as a tile count, convert at query time. No ranges are used
this checkpoint; store them in hexes anyway.

**Expose anything with no other way to change it.** Per `CLAUDE.md`, if a value can only be
changed by editing C++, it is `EditAnywhere` on its config or data asset. In this checkpoint that
means `BoardWidth`, `BoardDepth`, `PlaceableRowCount`, `HexRadius`, every economy number in 0.4,
every champion stat in 4.3, camera zoom and pitch, drag thresholds, and the spawn-enabled flags in
1.6. Values that already have a non-code way to change them — a unit's coordinate, a tile's
occupant — get setters and pure getters instead. If a variable's side of that line is unclear,
ask rather than guessing.

**Grid A*, not NavMesh.** Applies to task 7.1.

**Traits are GameplayTags.** Not `FName`, not an enum, not a string. See Phase 0.3.

**No GAS this checkpoint.** `CLAUDE.md` says introduce GAS at Step 2. Checkpoint 1 sits before
Step 1, so there is no AttributeSet, no AbilitySystemComponent, and no GameplayEffects. Champion
stats are stored on `ChampionData` as **initialization data for a future AttributeSet** — plain
values, read by nothing, feeding a GAS AttributeSet in a later checkpoint. Do not build a
stat system that GAS will later have to displace.

**No unit cap.** `DESIGN.md` explicitly defers it.

**Buying puts a champion on the bench, never on the board.** Every champion reaches a hex by being
dragged there. The only exception is the debug console command in 4.5. Bench slots are not hexes
and never enter the tile array.

---

## Layout

Per `CLAUDE.md`. Only the folders this checkpoint touches:

```
Source/Terrabound/
├── Grid/           HexCoordinates, HexTile, HexGrid, HexGridVisualizer
├── Pathfinding/    HexPathfinder            (task 7.1 only)
├── Units/          BoardUnitBase, ChampionBase
├── Economy/        ShopSystem, EconomyState, Bench
├── Data/           ChampionData, BoardConfig
└── TerraboundSettings.h   UDeveloperSettings holding the BoardConfig reference (1.4)

Content/Terrabound/
├── Blueprints/     Grid/ Champions/ Core/
├── Data/           Champions/ Traits/ DA_BoardConfig
├── Levels/
├── UI/
└── Materials/

Content/ (root, outside Terrabound/) — Paragon<CharacterName>/, one folder per
imported pack, left exactly where the importer puts it and exactly as imported
```

Do not create `Combat/`, `Abilities/`, `Terrain/`, or `Waves/` this checkpoint. Empty scaffolding
for unbuilt systems is on the "do not scaffold" list.

**Every task is tagged with who does it**, per `CLAUDE.md`'s division of labour:

- **[C++]** — Claude writes it directly. 22 of the 40 tasks.
- **[Editor]** — the owner does it in the Unreal Editor. Claude specifies exactly what to create
  (asset name, parent class, folder, values) and hands it over. Tasks 0.6, 2.1, 2.3, 4.1, 6.5, 7.2.
- **[C++ + Editor]** — both, with a `C++:` / `Editor:` line under the tag saying which half is
  which. Usually a C++ class plus the content asset created from it.

An editor step does not block a task. Hand it over, write the C++ side, and carry on.

**C++ / Blueprint split** per `CLAUDE.md`'s table: C++ owns the tile struct, grid array, board
generation, and economy state. Blueprint owns champion variants (derived from `ChampionBase`),
UI widgets and shop layout, tuning data, and level setup including spawn hex configuration.

---

## Hex math reference

Pointy-top, odd-r offset, odd rows shifted right. `R` = hex circumradius.

**Axial → world (2D, before projecting onto the board plane):**
```
x = R * (sqrt(3) * q  +  sqrt(3)/2 * r)
y = R * (3.0/2.0 * r)
```

**Offset (col, row) → axial:** `q = col - (row - (row & 1)) / 2` , `r = row`

**Axial → cube:** `x = q`, `z = r`, `y = -x - z`

**Axial distance:** `(abs(q) + abs(q + r) + abs(r)) / 2`

**Axial neighbours:** `(+1,0) (+1,-1) (0,-1) (-1,0) (-1,+1) (0,+1)`

**Forward moves toward the exit** (increasing `r`) are `(0,+1)` and `(-1,+1)` — two diagonals,
no straight-ahead neighbour, so movement zigzags. Intended, per `DESIGN.md`.

**World → axial** needs fractional axial coords then **cube rounding**: round all three cube
components, then correct the one with the largest rounding error. Rounding q and r independently
produces wrong tiles near edges, and those bugs present as pathing or input bugs.

Standard reference for all of the above: the Red Blob Games "Hexagonal Grids" article.

---

# Phase 0 — Setup

### 0.1 Module and folders  
**[C++]**
Create `Source/Terrabound/` with the folders listed in Layout. Project compiles clean.

### 0.2 `BoardConfig` (`Data/`)  
**[C++ + Editor]**
**C++:** the `UPrimaryDataAsset` class and its fields.  
**Editor:** create `DA_BoardConfig` from it and fill the values.

`UPrimaryDataAsset`: `BoardWidth` (7), `BoardDepth` (8), `PlaceableRowCount` (5), `HexRadius`.
Content asset at `Content/Terrabound/Data/DA_BoardConfig`. `HexRadius` is filled in by task 0.6.

**Spawn hexes do not live here.** `CLAUDE.md` specifies a **per-level** array of enabled spawn
tiles, configured in level setup. Defer to the enemy checkpoint; do not add a field for it now.

**Done when:** changing `BoardWidth` changes the generated grid with no recompile.

### 0.3 GameplayTags for traits  
**[C++ + Editor]**
**C++:** enable the GameplayTags module in `.Build.cs` and add native tag declarations if used.  
**Editor:** create the tag table asset and add `Trait.Woodland` / `Trait.Bruiser`.

Enable the GameplayTags module and declare `Trait.Woodland` and `Trait.Bruiser` in a tag table
at `Content/Terrabound/Data/Traits/`.

Exactly two traits, per MVP scope. Tags carry **no effects** this checkpoint — they are counted
for display only. This is not GAS; GameplayTags is a separate, cheap module, and using it now is
what prevents the parallel trait system `CLAUDE.md` forbids.

**Done when:** both tags resolve and are assignable on a data asset.

### 0.4 Economy placeholder config  
**[C++ + Editor]**
**C++:** the config class or data asset type, and every accessor that reads it.  
**Editor:** create the asset and enter the placeholder numbers.

One config file or data asset holding every economy number, each marked as a placeholder.

From `DESIGN.md` §4:
- `TierCostTable` — cost by tier, 1/2/3. **Cost is derived from tier, TFT-style.** It is not
  stored per champion; see 4.3.
- `BaseIncomePerWave` — 5
- `StartingGold` — not specified in `DESIGN.md`; pick a value, mark it unresolved
- `RerollCost` — not specified in `DESIGN.md`; pick 2, mark it unresolved
- `ShopSlotCount` — 5
- `BenchSlotCount` — 6
- `SellRefund` — full purchase price

**One carve-out:** the champion pool's tier→size and tier→roll-odds tables live in their own data
table (task 6.2), not here. They are a distribution rather than a scalar, they are edited as a unit,
and a table is the right shape for them. Still placeholder numbers, still marked as such. Nothing
else gets this exemption.

**Done when:** no economy number exists anywhere else in the project. Shop slot count, bench slot
count, and champion cost are all read from here, not from a literal in `ShopSystem`, `Bench`, or
a champion data asset.

### 0.5 Debug panel  
**[C++]**
Toggleable on-screen widget or console command set. Starts near-empty; every phase adds to it.
Never remove from it.

### 0.6 Import one Paragon character, set `HexRadius`  
**[Editor]**
Import a single character from the free Epic Paragon packs on Fab, left wherever the
importer puts it at `Content/` root (e.g. `Content/ParagonLtBelica/`) and exactly as
imported — never moved into `Content/Terrabound/`. Stand it in the level next to a
placeholder hex and pick a `HexRadius` that reads correctly at roughly the Phase 3.1
camera distance. Write the number into `DA_BoardConfig`.

**This is here, not in Phase 4, on purpose.** Task 2.1 sizes the tile mesh from `HexRadius` and 2.2
builds the whole board from it. Importing characters afterwards means discovering the scale
mismatch — which the Known Issues section below predicts — with the board already built, and
redoing 2.1 and 2.2. One character is enough to fix the radius; the rest come in 4.1.

Per `CLAUDE.md`, leave the pack exactly as imported.

**Done when:** `HexRadius` is set from a real character's footprint, not guessed.

---

# Phase 1 — Grid data

### 1.1 `HexCoordinates` (`Grid/`)  
**[C++]**
`FHexCoord` struct: `int32 Q, R`. Equality, `GetTypeHash`, `ToString`.
Static pure helpers in the same header: `AxialToCube`, `AxialDistance`, `GetNeighbours`,
`OffsetToAxial`, `AxialToOffset`, `AxialToWorld2D`, `World2DToAxial` (with cube rounding).
Pure functions only — no world access, no side effects. This makes them testable and keeps
coordinate math out of actor code.
**Done when:** usable as a `TMap` key and callable from Blueprint.

### 1.2 Coordinate round-trip tests  
**[C++]**
Automation tests: world → axial → world returns the same hex for all 56 tiles and for random
points inside each. Neighbour symmetry holds across the whole board, **both row parities**.
**Done when:** tests pass. **Do not proceed with failing tests.** Every system downstream inherits
these bugs, and they surface disguised as input and pathing bugs.

### 1.3 `HexTile` (`Grid/`)  
**[C++]**
`FHexTile`: `FHexCoord Coord`, `bool bIsSpawn`, `bool bIsPlaceable`, `bool bIsWalkable`,
`TWeakObjectPtr<ABoardUnitBase> Occupant`, `TWeakObjectPtr<AActor> Terrain`, `float PathCost`.
Fields independent. `Terrain` is declared now and stays null all checkpoint.

**`ABoardUnitBase` does not exist until 4.2. Forward-declare it** — `class ABoardUnitBase;` at the
top of the header. `TWeakObjectPtr` needs nothing more, and the struct is complete now rather than
being revisited later. **Do not create a stub class** (that is scaffolding, which `CLAUDE.md`
forbids) and **do not weaken the type to `AActor`** as a placeholder — that placeholder never gets
tightened back up.

`Occupant` is a single reference because **one unit stands on a tile at a time**, champion or
enemy, no stacking. It is not a list. Per `CLAUDE.md`, occupiable and passable are separate
questions: `bIsWalkable` says a path may cross the tile, `Occupant == nullptr` says a unit may
stand on it.

**Done when:** compiles with only a forward declaration; occupancy and walkability are separate
fields.

### 1.4 `HexGrid` (`Grid/`)  
**[C++]**
Flat `TArray<FHexTile>` owned by a `UWorldSubsystem`. Generates from `BoardConfig` on world init.
Index ↔ coord conversion. Accessors: `GetTile`, `IsValidCoord`, `SetOccupant`, `ClearOccupant`,
`GetPlayerZoneTiles`.

**How the subsystem finds `DA_BoardConfig`:** a `UDeveloperSettings` class
(e.g. `UTerraboundSettings`), registered under Project Settings, holds a
`TSoftObjectPtr<UBoardConfig>` pointing at the content asset. `HexGrid` reads
the settings singleton and resolves the soft reference on world init. One
place to point at the config, editable from Project Settings without touching
a level, and never duplicated per-level. Do not hardcode the asset path, and
do not require a level Blueprint to wire the reference in — that would make
board generation depend on level setup, which contradicts "generates on world
init."

Rows `0 .. BoardDepth - PlaceableRowCount - 1` are the enemy side; the rest are placeable, derived
from config at generation time.
**Done when:** 56 tiles, 35 placeable, and setting `PlaceableRowCount` to 6 yields 42 with no code
change.

### 1.5 Debug: grid state dump  
**[C++]**
Console command printing tile count, placeable count, and any tile's full state by coord.

### 1.6 Set `bIsSpawn` on a tile  
**[C++]**
Two entry points, both hitting the same C++ function on the `HexGrid` subsystem:

- Console command `SetSpawnFlag <q> <r> <0|1>` — debug only.
- `UFUNCTION(BlueprintCallable) SetSpawnFlag(FHexCoord Coord, bool bEnabled)` — callable from a
  level Blueprint so spawn tiles can be toggled during level setup without a recompile.

Spawn configuration proper is deferred to the enemy checkpoint (see 0.2), so nothing else this
checkpoint sets `bIsSpawn`. Without this, the spawn case in task **5.1** cannot be constructed and
that test cannot be written.

**This is a function, not exposed state.** Do not mark `FHexTile` or any of its fields
`UPROPERTY(EditAnywhere)` / `BlueprintReadWrite`. A Blueprint asks the grid to change a flag; the
grid still owns the array.

**What is forbidden is a Blueprint-held copy of live tile state**, which can drift out of sync with
`HexGrid` and produce two sources of truth that silently disagree. A per-level array of spawn
coordinates that the grid reads *once at generation* is not that — nothing reads it after startup,
so it cannot drift. `CLAUDE.md` endorses that array as the eventual designer workflow, and it
arrives with the enemy checkpoint alongside the rest of spawn configuration (see 0.2).

This setter is the Checkpoint 1 stand-in for it, existing so 5.1 has a fixture and so a level
Blueprint can flag tiles before the generation-time path exists. Keep it a setter; do not add the
array early, and do not treat this task as forbidding the array later.

**Done when:** 5.1's spawn-rejection test can set up its own fixture, a level Blueprint can flag
spawn tiles, and the 1.5 dump reflects changes made from either entry point.

---

# Phase 2 — Board visuals

Deliberately cheap. Do not spend time here; it gets replaced.

### 2.1 Hex tile mesh  
**[Editor]**
6-sided cylinder scaled thin, rotated to a pointy-top profile. Made in-engine, no DCC tool. Sized
from `HexRadius` with a small gap so borders read.

### 2.2 `HexGridVisualizer` (`Grid/`)  
**[C++ + Editor]**
**C++:** the visualizer actor, ISM component, and transform generation.  
**Editor:** place it in the level and assign the 2.1 mesh.

One actor with a `UInstancedStaticMeshComponent`, one instance per tile, transforms from
`AxialToWorld2D`. **Not 56 actors.** Reads from `HexGrid`; never writes to it.
**Done when:** the 7×8 board renders, rows visibly stagger, and it reads as a TFT board rather
than a ragged grid.

### 2.3 Tile state material  
**[Editor]**
Per-instance custom data float driving colour. States: `Default`, `PlayerZone`, `EnemyZone`,
`Hovered`, `ValidPlacement`, `InvalidPlacement`, `Occupied`.
**Done when:** player zone is visually distinct from enemy zone and states are settable per-tile
from C++.

### 2.4 Debug coordinate overlay  
**[C++]**
Toggleable `(q, r)` text at each hex centre. Used to verify Phase 3 by eye.

---

# Phase 3 — Camera and controls

### 3.1 Camera  
**[C++ + Editor]**
**C++:** camera actor, zoom and pitch clamps, exposed limits.  
**Editor:** place it, frame the board by eye, save the values.

Fixed-angle camera looking down at the board, roughly TFT's framing. Zoom and slight pitch allowed;
free orbit is not.
**Done when:** all 56 hexes are on screen and legible at default zoom.

### 3.2 Hex under cursor  
**[C++]**
Deproject the mouse to a ray, intersect the board's ground plane, convert the hit point with
`World2DToAxial`.

**Do not line-trace against tile or unit actors.** There are no tile actors, and tracing against
units would couple input to rendering and quietly breach the grid-is-data invariant.
**Done when:** hovering any point returns the correct hex, including near edges and corners,
verified against the 2.4 overlay.

### 3.3 Hover feedback  
**[C++]**
Hovered tile switches to `Hovered`. Clears on leaving the board.
**Done when:** no flicker at tile boundaries.

### 3.4 Click and drag  
**[C++]**
Left-click press to select, hold to drag, release to drop. Right-click or Escape cancels and
returns the held champion to its origin. Dropping outside the board cancels.
**Done when:** a placeholder can be picked up and dropped on another hex, and cancel reliably
restores the original position.

---

# Phase 4 — Champions

### 4.1 Import the remaining Paragon assets  
**[Editor]**
One character is already in from task **0.6**, and `HexRadius` is already fixed against it. Import
the other 2–3 from the free Epic Paragon packs on Fab, each left wherever the importer puts it at
`Content/` root — never moved into `Content/Terrabound/`.

**Leave the packs exactly as imported.** Per `CLAUDE.md`, reorganizing them (moving them into
`Content/Terrabound/` or reorganizing inside them) causes redirector pain for no benefit.

**Watch for:** MOBA-scale, high-poly heroes. Do not change `HexRadius` to suit a later character —
scale the character. The radius was fixed in 0.6 and the board is already built against it.
**Done when:** all 3–4 skeletons and animation sets are intact and each character stands on a hex
at a scale that reads correctly at the 3.1 camera distance.

### 4.2 `BoardUnitBase` (`Units/`)  
**[C++]**
Shared base for champions and, later, enemies. Holds `FHexCoord CurrentCoord`, snap-to-hex
positioning, and a team flag. Nothing champion-specific.

Declared now because `ChampionBase` and `EnemyBase` both derive from it per `CLAUDE.md`'s layout,
and retrofitting a base class under a live class later is worse than writing a thin one now.
**Done when:** compiles, holds coordinate state, positions correctly on a hex.

### 4.3 `ChampionData` (`Data/`)  
**[C++ + Editor]**
**C++:** the `UPrimaryDataAsset` class.  
**Editor:** create the 3–4 champion assets and fill in tiers, meshes, anim BPs, and trait tags.

`UPrimaryDataAsset`: display name, tier (1–3), skeletal mesh, anim blueprint, and
`FGameplayTagContainer Traits` populated from `Trait.Woodland` / `Trait.Bruiser`.

**No `Cost` field.** Cost is derived from tier via the `TierCostTable` in the 0.4 config, TFT-style.
Storing it per champion would put an economy number outside the one config file, which 0.4 forbids,
and would let a champion's cost silently disagree with its tier.

Stat fields — max HP, attack damage, attack speed, **range in hexes** — are **AttributeSet
initialization data for a later checkpoint**. Store them; read them nowhere. Do not add
current-HP tracking, damage application, or any runtime stat mutation. That is GAS's job at
Step 2.

**Done when:** 3–4 assets exist with distinct tiers, each carrying one or both trait tags, and
each one's cost resolves through `TierCostTable` rather than being stored on the asset.

(`DESIGN.md` MVP says 2–3 champions; 3–4 is the owner's call for this checkpoint and costs
nothing, since they are data assets.)

### 4.4 `ChampionBase` (`Units/`)  
**[C++ + Editor]**
**C++:** `ChampionBase` and its initialisation from `ChampionData`.  
**Editor:** the per-champion Blueprints deriving from it, in `Blueprints/Champions/`.

Derives from `BoardUnitBase`. Initialised from a `ChampionData`. Idle animation only. Faces the
enemy side.

Per `CLAUDE.md`'s split, individual champions are **Blueprints deriving from `ChampionBase`**, in
`Content/Terrabound/Blueprints/Champions/`. Do not create a C++ class per champion.
**Done when:** a champion spawns on a specified hex and idles.

### 4.5 Debug spawn command  
**[C++]**
`SpawnChampion <DataAssetName> <q> <r>`. Places a champion directly onto a hex, bypassing both
the shop and the bench.

**Debug only.** This is the single exception to buy-to-bench, it exists so Phase 5 can be tested
before a shop exists, and it must never be reachable from normal play. Do not reuse this path for
the shop's buy flow in Phase 6.
**Done when:** every champion can be spawned to arbitrary hexes from console.

### 4.6 Team tint hook  
**[C++ + Editor]**
**C++:** the team flag and the parameter-setting call.  
**Editor:** the material parameter or outline shader it drives.

Material parameter or outline shader driven by the `BoardUnitBase` team flag. Player-side only for
now, but the switch exists — `DESIGN.md` calls for distinguishing sides by tint rather than by
model, since both sides draw from the same packs.

---

# Phase 5 — Placement

Built and tested against debug spawns. No shop dependency.

### 5.1 Placement validation  
**[C++]**
`CanPlaceAt(coord)` is true only when the coord is valid, `bIsPlaceable`, and `Occupant` is null.
`bIsSpawn` tiles are never placeable.

**Validation lives in `Grid/`, and stays there.** `CLAUDE.md`'s layout puts `PlacementValidator`
under `Terrain/`; that is a *different* validator and the two are not unified. Unit placement asks
"is this coord valid, placeable, and free" — a pure grid question with no pathfinding. Terrain
placement asks all of that **plus** "does this seal the board", which needs A* and arrives with the
terrain checkpoint. Two validators, one shared grid-level check underneath. Do not build
`PlacementValidator` now and do not move this into `Terrain/`.
**Done when:** unit tests cover valid interior, occupied, enemy-zone, spawn, and out-of-bounds.

**The spawn case needs a deliberately invalid fixture.** Real spawn tiles sit on the far edge,
which is row 0, which is already non-placeable because it is in the enemy zone. A test that flags
row 0 and asserts rejection passes because of the *zone* check and never reaches the spawn check —
it would stay green if the spawn guard were deleted entirely. Use the 1.6 setter to set `bIsSpawn`
on a tile **inside the placeable zone** (row 5, say). No real level would be configured that way,
which is exactly the point: zone and occupancy both pass, so `bIsSpawn` is the only thing that can
cause the rejection, and removing the guard breaks the test.

Same principle applies to the other cases — each fixture should leave its own guard as the only
possible reason for the result.

### 5.2 Placement preview  
**[C++]**
During a drag, valid hexes show `ValidPlacement`, invalid ones `InvalidPlacement`. Continuous
during the drag, not only on release.
**Done when:** dragging lights the legal hexes and the enemy zone stays dark.

### 5.3 Commit placement  
**[C++]**
On valid drop: set the tile's `Occupant`, clear the origin tile, move the actor to the new centre.
`bIsWalkable` is **not** touched — units do not block.
**Done when:** grid data and visual position never disagree, verified with the 1.5 dump.

### 5.4 Repositioning and swap  
**[C++]**
Drag a placed champion to another legal hex. Dropping on an occupied hex **swaps** the two, as in
TFT.
**Done when:** swap works both directions and leaves grid data consistent.

### 5.5 Occupancy debug view  
**[C++]**
Debug panel lists every occupied hex and its occupant, accurate mid-drag.

---

# Phase 6 — Economy and shop

### 6.1 `EconomyState` (`Economy/`)  
**[C++]**
Gold as an integer. `Add`, `Spend`, `CanAfford`, change delegate. Starting gold from the 0.4
placeholder config. C++ per `CLAUDE.md`'s split.

### 6.2 Champion pool  
**[C++ + Editor]**
**C++:** the pool class, draw and return logic, and the row struct.  
**Editor:** the data table asset and its placeholder rows.

Data table mapping tier → pool size and roll odds, per the 0.4 carve-out. Champions are drawn from
the pool and returned on sell. Mark the numbers as placeholders like everything else in 0.4.
**Done when:** a debug command rolls a large sample and the tier distribution matches config.

### 6.3 `ShopSystem` (`Economy/`)  
**[C++]**
`ShopSlotCount` slots, read from the 0.4 config — currently 5, not a literal. `Reroll()` draws at
configured odds and costs `RerollCost` gold. `Buy(slotIndex)` checks gold, asks `Bench` whether it
has space, deducts, empties the slot, and hands the champion to `Bench` to place in its first free
slot.

**`ShopSystem` does not own bench state.** It queries and calls `Bench` (task 6.4); the array lives
there. A shop that owns the bench ends up owning placement, and then board → bench drags have to
route through the shop, which is the wrong shape.

**Buying never spawns a champion onto a hex.** `ShopSystem` has no knowledge of `HexGrid` and no
reference to a coordinate. If a purchase can reach the board without a drag, the flow is wrong.

A full bench blocks buying — the buy button disables and the attempt fails cleanly.
**Done when:** buying without enough gold or with a full bench fails cleanly and leaves state
untouched.

### 6.4 `Bench` (`Economy/`)  
**[C++]**
`BenchSlotCount` slots, read from the 0.4 config — currently 6, not a literal.
**The only way a champion reaches the board.**

**`Bench` owns the slot array.** It exposes `HasFreeSlot`, `AddChampion`, `RemoveChampion`,
`GetChampionAt`, and a change delegate. `ShopSystem` and the drag system both call into it; neither
holds bench state. This is why it is its own class rather than a field on `ShopSystem` — the shop
is one of two writers, not the owner.

Bench slots are drag sources and drop targets using the same interaction as board hexes from
Phase 3.4 and Phase 5 — one drag system, two kinds of destination, not two parallel systems.
Supported directions: bench → board, board → bench, bench → bench.

Bench slots are **not hexes** and are not part of the tile array. Do not widen `HexGrid` to cover
them.

Board → bench is a real move, not just an undo: `DESIGN.md` calls for pulling wounded units back
during prep once persistent damage exists.

**Done when:** all three drag directions work, grid occupancy stays correct across every
transition, and a full bench blocks buying.

### 6.5 Shop and bench UI  
**[Editor]**
Widget with `ShopSlotCount` cards (portrait, name, cost from `TierCostTable`, trait tags), gold
display, reroll button with cost, and the bench row rendered beneath the board. Cards disable when
unaffordable or when the bench is full, with the reason legible. Blueprint, per the split. Function
over polish.

**After 6.4 on purpose** — the bench row renders from `Bench`'s array, so the array has to exist
first.

### 6.6 Sell  
**[C++ + Editor]**
**C++:** refund, pool return, tile clear, and the sell entry point.  
**Editor:** the sell zone widget or input binding.

Drag to a sell zone, or select and press a key. Refunds gold, returns the champion to the pool,
frees the tile.

Note for later: `CLAUDE.md` requires that units are **never destroyed and respawned between
waves**, so attributes persist. Selling is an intentional removal and is exempt — but do not build
any board-refresh or wave-reset path that destroys and recreates champions.

### 6.7 Trait counter UI  
**[C++ + Editor]**
**C++:** the tag count across board occupants and a change delegate.  
**Editor:** the panel widget bound to it.

Panel listing the two trait tags and how many board champions carry each. **Board only — bench
champions do not count**, as in TFT. Implemented as a **tag count across the board**, which is the
same mechanism thresholds will use later. No thresholds, no effects, no unlocks.
**Done when:** counts update live on place, remove, swap, and sell, and moving a champion to the
bench decrements its traits.

---

# Phase 7 — Checkpoint validation

### 7.1 `HexPathfinder` and debug walker (`Pathfinding/`)  
**[C++ + Editor]**
**C++:** A*, the virtual goal node, the walker, and crossing-time logging.  
**Editor:** a placeholder capsule and a level with spawn tiles flagged.

Hand-rolled A* over the tile array. Grid A*, not NavMesh.

Per `CLAUDE.md`, enemies path to **a single virtual goal node with zero-cost edges from every
back-row hex**, so the search stays an ordinary single-target A*. Implement the goal node now — it
is three lines and it keeps the search shape correct from the start.

The goal is an **exit**, not a crystal: reaching the back row means leaving the board. There is no
objective actor to build, now or later. The leak counter that eventually hangs off this arrives
with the enemy checkpoint; do not add it here.

A debug capsule spawns on a far-edge hex, follows the path centre-to-centre at a configurable
speed, and despawns at the goal. **No combat, no AI, no `EnemyBase` class, no targeting.**
Champion-occupied hexes are **passable at no extra cost** — this is the units-don't-block invariant
and this is the moment to verify it holds.

Log the crossing time.
**Done when:** the walker crosses the board and the crossing time is written down. That number is
what the next checkpoint's tuning starts from.

### 7.2 Full-loop smoke test  
**[Editor]**
Start PIE with starting gold. Reroll. Buy 3–4 champions and confirm each lands **on the bench, not
the board**. Drag them onto hexes. Reposition, swap, pull one back to the bench. Fill the bench and
confirm buying is blocked. Sell from both bench and board. Watch trait counts throughout.
**Done when:** no errors and grid data stays consistent.

---

## Definition of done

- [ ] 7×8 pointy-top board renders with correct odd-r staggering
- [ ] Grid is a flat data array; nothing queries actors for tile state
- [ ] Coordinate round-trip and neighbour-symmetry tests pass on both row parities
- [ ] `bIsWalkable` and `Occupant` are independent; placement never touches walkability
- [ ] `FHexTile` compiles on a forward declaration; no stub `ABoardUnitBase`, no `AActor` placeholder
- [ ] Spawn-rejection test uses a placeable-zone fixture, so it fails if the guard is removed
- [ ] Player zone (35 hexes) derived from config, not hardcoded
- [ ] Camera frames all 56 hexes legibly at default zoom
- [ ] Mouse hover resolves to the correct hex anywhere on the board, with no traces
- [ ] No line traces against tile or unit actors anywhere in the input path
- [ ] Click-and-drag placement with valid/invalid preview
- [ ] 3–4 Paragon champions spawn, scale correctly, and idle on hexes
- [ ] Traits are GameplayTags; trait counts are tag counts
- [ ] Shop rolls, rerolls, buys, and sells against `EconomyState`
- [ ] Buying lands a champion on the bench; no code path spawns one onto a hex
- [ ] A full bench blocks buying, legibly
- [ ] Champions can be placed, repositioned, swapped, benched, and sold
- [ ] Bench works in all three drag directions; bench champions excluded from trait counts
- [ ] `Bench` owns the slot array; `ShopSystem` holds no bench state
- [ ] All economy numbers live in one file, marked placeholder, including shop slots, bench
      slots, and the tier cost table
- [ ] Champion cost derives from tier; no `Cost` field on `ChampionData`
- [ ] `HexRadius` was set against a real Paragon character before the board was built
- [ ] Board crossing time measured and recorded
- [ ] No GAS, no AttributeSet, no AbilitySystemComponent anywhere

---

## Out of scope — do not build, do not scaffold

Enemies, `EnemyBase`, `EnemyAssassin`. Assassin leaps, mana. Enemy range values. Combat,
`CombatResolver`. Wave rosters and spawn pacing (`SpawnInterval`). The leak counter. `TargetingComponent`,
`TargetableInterface`, aggro, target locking, `State.Untargetable`. GAS in any form — AttributeSet,
AbilitySystemComponent, GameplayEffects, abilities. Waves, `WaveManager`, `WaveDefinition`,
scouting. Terrain, trees, `TerrainPieceBase`, `PlacementValidator`, seal prevention. Trait thresholds or effects. Path
caching and dirty-flag invalidation (7.1 recomputes each spawn; that is fine at 56 tiles). Any
objective actor, HP crystal, or nexus — the design has none. Persistent damage, healing.
Checkmate, score. Augments, items, bosses,
mid-combat repositioning, enemy-side terrain, depth-based stat bonuses, stat degradation, traits
beyond the two in MVP scope. Save/load. Audio. Textures, VFX. LODs and performance work.

Several of these are one small step from something in this plan. That is why the list is explicit.

---

## Known issues to expect, not solve

- **Paragon scale and animation timing.** Tuned for a MOBA camera. Attack windups and root motion
  will need adjusting to read at autobattler zoom. Not a Checkpoint 1 problem.
- **Performance with a full board.** High-poly heroes built for a handful on screen; 35 champions
  plus a wave will want LODs and possibly lower-fidelity enemy variants. Do not pre-optimise.
- **Hex radius vs character scale.** These fight each other. Task **0.6** settles it before the
  board is built: pick a radius that fits the characters and leave it alone. Range is stored in
  hexes precisely so this can change later without rebalancing anything.

---

## After this checkpoint

Stop. Do not continue into enemies. The plan is rewritten from what the board actually feels like
to play.
