# Terrabound — Design

Autobattler / tower defense hybrid. Unreal Engine 5.8.

This document records decisions and, more importantly, why they were made.
Sections 4–5 are deliberately unresolved and stay that way until there is
something playable.

**How to read this document.** Everything here is the current best guess, not a
specification. Numbers are starting points chosen so there is something concrete
to tune away from — a reroll cost of 2 is not a target, it is a place to begin.
Structural decisions (the exit instead of a crystal, grid-is-data, buy-to-bench,
one unit per tile) are firmer, but none of them are load-bearing enough to
protect against a playtest that says otherwise.

The rule is: **when play contradicts the doc, the doc is wrong.** Change it in
the same sitting. A design document that has not been edited in a month is not a
stable design, it is an abandoned one.

**Companion documents.** `CLAUDE.md` is the operational version — architecture
invariants and working style. `PLAN.md` is the current checkpoint's task
breakdown. This file is the source of truth for design intent; where a decision
here conflicts with either of those on *what the game is*, this file wins.

---

## 1. Pitch

A single-player roguelike that fuses TFT-style autobattler mechanics with tower
defense. The player buys units from a shop, places them on a hex board, and
watches them defend against waves of enemies that spawn on the far edge and push
toward the player's side.

Unlike TFT, there is no opposing player board. Unlike tower defense, the "towers"
are champions with traits, synergies, and stat scaling, and the player manages an
economy between waves.

**Core hook: traits reshape the battlefield.** Trait synergies don't just buff
stats — they let the player physically alter the board, spawning terrain and
building a defense of their own design.

Run structure is endless with a soft cap. Play until you lose. Chase a high score.

---

## 2. Board

### Specification

- **7 hexes across, 8 rows deep. 56 total.**
- **Pointy-top hexes, odd-r offset rows**, laid out exactly like the TFT board.
  All 8 rows are 7 wide and stagger against each other — *not* ragged alternating
  7/6/7/6. Pick which parity shifts right and never change it; every neighbor
  lookup depends on it.
- Pointy-top means there is no straight-ahead neighbor. Enemies advancing toward
  the exit have two diagonal forward moves, so all movement zigzags. Matches
  TFT visually and gives terrain more surface to work with.
- **Axial coordinates `(q, r)`** for storage, cube for distance. Offset
  coordinates turn neighbor math into parity special-cases.
- **Range is measured in hexes**, converted at query time, so changing hex size
  doesn't silently rebalance every unit.

### The exit (no crystal)

There is **no HP crystal and no nexus**. The thing behind the back row is an
**exit**: an enemy that reaches the back row leaves the board and despawns.

**A leak currently costs nothing.** It increments a counter and nothing else.
This is temporary. A leak **will** cost something; what it costs — gold loss,
reduced income, a damaged unit, a score hit — is the part that is unresolved
(§4). The leak *event* is the same code regardless, so whatever is attached later
just subscribes to it.

**Until then, leaking is a relief valve rather than a pressure.** An enemy that
walks past your line and exits stops attacking your units and disappears, so
excess pressure drains out the back for free. That caps how much a wave can hurt
you no matter how large it is, which is exactly the property a leak cost has to
remove. Worth knowing while reading early playtest numbers: waves will feel
flatter than they eventually should.

Rationale: an HP crystal plus enemies sprinting at it is a PvZ shape, and it made
a whole enemy archetype into a pure damage check with no positional counterplay.
The exit keeps what the crystal was actually for — **a destination that pulls
enemies across the board** — without the bar. That pull is what gives terrain its
leverage; see "Why terrain needs a destination" below.

Implement as a single virtual goal node with zero-cost edges from every back-row
hex, so the exit is one source for the distance search rather than eight targets.
All 56 hexes stay playable.

**Instrument it from the start.** Count leaks per wave and cumulatively, and log
which enemy type leaked. Those numbers are what tell you whether leak damage is
even the right difficulty lever, and they can't be guessed from a document.

### Placement zone

- Player owns the **5 rows nearest the exit = 35 placeable hexes**. Units
  and terrain share them. "Front" is from the player's viewpoint and means the
  **high** row indices — rows 3–7, with row 7 the back row adjacent to the
  exit. Rows 0–2 are the enemy side.
- Enemies get 3 free rows before entering player territory.
- Do **not** hardcode the zone depth. It is the first dial to turn if the board
  feels cramped or permissive.
- **A cap on units placed on the board is planned; the value, and how it is
  calculated, are undecided.** It is one of the two levers (with slow, sparse
  spawning) that keep battles small-scale — see "Blocking model". A row is 7 wide,
  so a cap of 6 or less makes a full unit-only wall impossible. Bench champions do
  not count toward it. Swaps never change the board count, so they are always
  allowed; only bench→empty hex is subject to the cap, and the drag preview shows
  hexes invalid once the board is full.

### Spawn hexes

The entire far edge is spawnable in principle, but which hexes are live is a
**per-level designer choice** — not a player choice, not a runtime count.

Store as a per-level array of enabled spawn hexes, so a level can specify "left
three only" or "both flanks, no center." Leaves room for per-wave control later.

Spawn hexes are never placeable. Keep per-tile flags independent: `is_spawn`,
`is_placeable`, `is_walkable`, plus occupant and terrain references.

### Blocking model

Two hooks, priced differently:

| | Effect on pathing | Role |
|---|---|---|
| **Terrain** | Impassable | Shapes the route |
| **Units** (champions and enemies alike) | Passable to the search at one flat, high cost | Deals the damage; occupies its hex |

The cost is a routing preference, not a wall. Enemies flow through any gap in
preference to a unit's hex; where a wall leaves no gap, the search routes through
it. The enemy walks up, cannot enter (one unit per hex), and aggro makes it fight
the blocker. "Break the wall or kill a unit" falls out of rules already in place,
and a sealed board needs no "no route exists" special case.

**The cost is flat.** Every occupied hex costs the same — it never reads HP, tier,
or side — so routing never funnels the wave into whichever part of the wall is
cheapest to hit. Which unit an enemy fights is decided by aggro (nearest by path
length, within range), not by routing. One consequence, accepted: depth counts, so
a wall one unit thick in one place and two thick in another draws the approach
toward the thin spot until a champion comes into range; aggro takes over at
range 1–2.

**Why units block.** The earlier rule was that units did not affect pathing. It
was replaced because it required an enemy to walk *into* a hex it is not allowed to
enter, and because the worry it guarded against — mazing with bodies — is bounded
here. The board is 7 wide (it mazes nothing, see "Why terrain needs a
destination"), a unit cap is planned, and battles are meant to be small-scale with
slow spawning, so a board clogged with bodies should not arise. Terrain keeps its
own job: it picks the lane, costs no unit slot, and cannot be killed by the wave.

**One unit per hex, always.** Blocking and occupancy are separate rules. The
search *prices* an occupied hex; the step *enforces* occupancy — a unit never
enters an occupied hex, whatever the search planned. Nothing stacks: one champion
or one enemy per tile, never two.

**Movement is decided one hex at a time.** Units keep no route. On arriving at a
hex an enemy chooses its next, and aggro takes precedence: with a target in range
it stops and attacks; with a target out of range it steps toward a free hex within
range of it; with none it steps toward the exit. "Toward" means lower *path*
distance from a search over the tile array — not raw hex distance, which stalls
behind terrain. Nothing is cached, so nothing goes stale when a hex fills or a tree
falls.

**When the hex it wants to stop on is taken, it moves to another hex that still
has its target in range.** If no such hex is free, it falls through to the
ordinary "nothing in range → path to the exit" branch and keeps walking. No
special case: an enemy that cannot attack is an enemy with no target, and that
state already has a rule.

The one genuinely new state is an enemy with no reachable attack hex *and* no
path to the exit, which waits. That is a possible stall source, and it is left
alone on purpose — spawn pacing, wave size and terrain limits all move it, and
which of them to reach for is unanswerable until there is a board to watch.

### Seal prevention

The board is only 7 wide, so a single row of terrain closes it entirely.

Less critical than it first appears: enemies always have a target once units are
in range, so they can't be locked out of fighting. The rule exists because
**every** enemy falls back to pathing at the exit whenever nothing is in range —
including one that has a target but no free hex to attack it from — and that path
has to exist. Seal the board and those enemies have nowhere to go and stop.

Reject any terrain placement leaving zero valid path from spawn to exit. Run A* on
the placement preview and grey out illegal hexes. Needed from the moment terrain
is placeable.

---

## 3. Systems

### Traits as terrain (main hook)

Trait thresholds unlock terrain the player can place. Example: 3 Woodland lets
you place trees, which block pathing and funnel enemies into chokepoints of your
own making.

Terrain occupies hexes, so every piece placed is a hex *not* holding a unit.
Hexes become a real currency and terrain has genuine opportunity cost. A
6-Woodland player leans on terrain with a skeleton crew; a 6-Bruiser player runs
a wall of bodies.

**Framing that matters:** terrain is *one build direction*, not the spine of the
game. Do not design the board, the economy, or the enemies around mazing. Players
should be able to win with no terrain at all.

Non-MVP trait ideas: Frost freezes ground in a radius and slows crossers. Arcane
opens portal pairs letting units rotate flanks.

### Why terrain needs a destination

Worth stating plainly, because it constrains the whole design: **terrain only has
leverage because enemies are walking somewhere that isn't your units.**

Trees are not walls. At 2 Woodland you get one or two; at 7 maybe four. On a
7-wide board that mazes nothing. What a tree does is decide **which lane an enemy
walks up**, and therefore whose range envelopes it crosses and for how long. The
value of a tree is measured in **extra seconds under fire**, not in path length.

That play — ignore a region of the board, redirect the enemies spawning there
into your cluster — only exists if enemies have a pull toward the exit. If
enemies simply walked at the nearest unit, they would already be coming to you
and a tree would delay them by a second and nothing more.

This is why the exit survived the crystal being cut.

### Terrain destruction

Terrain is mostly indestructible by design — a strategic tool the player uses to
shape their own defense. But a few specific enemy archetypes can destroy it, and
bosses or large waves can stomp through.

Destroyed terrain is **not permanently lost**, but also **not free to restore**.
There must be a cost or cooldown — gold price, wave timer, regrowth condition.
Mechanism undecided.

### Enemy aggro (TFT rules)

Aggro range and attack range are the same value.

**Acquisition needs two things, not one.** A candidate is targetable only when it
is **in range by hex distance** *and* **at least one hex within range of it is
unoccupied** — somewhere the enemy can actually stand and attack from. Its own
current hex counts; an enemy already in position attacks from where it is rather
than looking for somewhere to move.

Both conditions are cheap. Hex distance is closed-form on the coords and the free
hex test is a handful of array lookups, so filter candidates on both *before*
ranking the survivors by path length. Running A* against candidates you are about
to discard is the expensive mistake.

Putting the free-hex test in acquisition rather than after it is what keeps the
lock stable. An enemy that locked a target it cannot reach a firing position for
would drop and re-acquire it every tick while walking past, which is exactly the
oscillation the lock exists to prevent.

- **No target** → scan every tick for the nearest player unit that satisfies both
  conditions, acquire.
- **Has target** → hold until the target dies. No re-scan.
- **No valid candidate** → keep pathing toward the exit.
- **No player units on the board at all** → everything paths to the exit. This is
  also the state the checkmate lose condition lives in.

The lock is real TFT behaviour — it's why TFT units chase someone past a closer
enemy. Re-evaluating every tick instead causes target flip-flop between two units
at equal distance, resetting attack windup so neither takes damage.

The reroute case falls out naturally: an enemy walking to the exit is rerouted by
a tree, a unit comes into range on the new path, it acquires.

**Two different distance measures, deliberately.** They answer different
questions and must not be unified:

| Question | Measure | Why |
|---|---|---|
| Which target is nearest? | **Path length** | Diverges from hex distance once terrain exists; affordable at 56 tiles |
| Is a target in range? | **Hex distance** | Closed-form on the coords, no A* in the tick loop |

**Contested hexes are resolved by re-checking, not by reserving.** Two enemies can
acquire the same champion, see the same single free hex, and both head for it.
Occupancy is taken when an enemy **begins the step** into a hex and released from
the one it leaves — not on arrival, or two enemies mid-move into the same hex both
complete and stack. The one that loses the race drops its lock and re-scans on the
same tick, which re-runs acquisition: another free hex in range of the same
champion means it re-acquires that champion and heads there, and only if none
exists does it fall through to exit-pathing. Dropping the lock is not walking
away.

This does not violate the no-flip-flop rule. The lock protects attack windup, and
an enemy that loses a contested hex was still walking, so there is no windup to
reset. Different situation from re-evaluating a target mid-fight.

**Small execution-order variation is expected and accepted.** The logic is
deterministic; the execution is not perfectly so. A unit surviving a tick longer
than it did last time, or a different enemy winning a race to the same hex, will
change how a fight plays out from run to run even on an identical board. That is
TFT's behaviour too, and it comes from tick boundaries and float accumulation
rather than from anyone rolling dice. It is fine, it is a little bit of life in
the combat, and **no effort goes into eliminating it** — no stable processing
order, no deterministic tie-break for "nearest", no replay determinism guarantee.

**One thing is not left to ordering: one unit per hex.** Drift may decide *who*
wins a race for a hex; it may never produce two units standing on one. That is
enforced by the occupancy check on the step itself, not by processing enemies in a
fixed sequence. Outcomes are allowed to vary; legality is not.

If an unreproducible wave ever eats real debugging time, a per-wave seed is the
fix and it can be added then. Nothing here forecloses it.

Known consequence, accepted for now: an enemy 2 hexes away with a tree between
them is in range and will shoot through the tree. Terrain does **not** block line
of fire. That's the complexity being declined; a "blocks line of fire" terrain
type is the eventual fix (§5).

**Second known consequence, and this one is intended.** A champion whose six
neighbours are all occupied cannot be acquired by any melee enemy — there is
nowhere to stand — so melee streams past it. Range-2 enemies can still hit it from
a ring further out where hexes are free. Tight formations protect their interior
from melee, which is a real positioning decision and the same mechanism as the
assassin bodyblock counter. It looks like enemies ignoring a unit the first time
you see it. It isn't.

### Assassins (replaces Runners)

The backline threat. **Runners are cut** — an enemy that ignores everything and
sprints at a crystal is a damage check you either pass or fail, with no
positional counterplay. Assassins ask a question you answer by rearranging your
board.

**Passive pathing is ordinary.** An assassin walks like any other enemy, is
blocked by terrain like any other enemy, and acquires the nearest player unit in
range like any other enemy. Out of combat it is not special.

**The leap is an ability, gated on mana.** TFT-style: the assassin builds mana by
attacking and by taking damage. On cast it leaps past the frontline onto a
backline unit and attacks it.

- **The leap always resolves.** There is no cancel, no refund, no retarget branch.
  Whatever the board looks like, the cast produces a landing hex.
- **The leap ignores terrain.** Partly so the above stays true, but mainly so
  terrain does not hard-counter the archetype. A tree wall that made assassins
  unable to reach the backline would leave the Woodland build with no answer to
  them at all, which is worse than the complexity it saves.
- **The leap does not ignore occupancy.** One unit per hex still holds, so a
  packed backline has no free landing hex. This is the bodyblock counter.
- **With no legal landing hex, the assassin leaps to the hex it is already on**
  and carries on attacking its current target. The cast is spent, the lock is
  kept, nothing moves. This is what keeps "always resolves" true without a
  special case — the degenerate landing hex is its own.
- **Leap distance is a per-enemy stat in hexes.** A short-range one hops the
  front line; a long-range one reaches your carry. One number, a family of
  enemies.
- **After the kill it re-scans normally** and takes whatever is nearest —
  usually another backline unit, since it is standing among them. It does not
  walk off. This reuses the existing lock-break path with no new code.

**Why this has real counterplay.** TFT assassins leap at round start, so
positioning is decided before you can react. Gating on mana means the assassin
must *survive your frontline long enough to cast*. That is a window:

- Kill it before it casts — the main counter, rewards frontline damage and burst
- Pack the backline so there is no legal landing hex
- Bait with a cheap unit in a back corner and waste the cast
- Spread so one leap reaches only one target
- A high-armour blocker feeds it less mana than a squishy one

**Terrain still works on assassins**, indirectly: it shapes which lane they walk
up and how long they are under fire before reaching your line, which lowers the
odds they live to cast at all. That matters — terrain-immune assassins would be a
hard counter to the Woodland build with no answer.

**Watch for the inert assassin.** The self-leap fallback means a board with no
free hex in leap range turns the cast into nothing at all. With no unit cap
yet (one is planned), a late-game board that is simply *full* gets that for free, without the player ever
deciding to bodyblock. If assassins stop being a threat exactly when boards fill
up, the fallback is doing too much work and the cast should probably do something
on a failed landing — damage where it stands, or a shorter hop. Playtest question,
but this is the specific thing to watch.

**Scoping note:** the leap needs mana, which needs GAS. Assassins are not an
early enemy. First enemies are dumber than this.

### Consequence to watch (Step 3)

Units are hard stops. An enemy that acquires a target
stops and fights until one of them dies — so a single cheap unit anywhere along
the route halts every regular enemy that comes into its range.

That's a lot of stopping power for one gold. Chip units become valuable
time-buyers; very PvZ, where one wall-nut buys enormous time.

**Watch for the blocker wall.** With no ranged enemy archetype (see §5), a line
of cheap tanks may stop everything, and assassins become the only answer to it.
That is a lot of load on one archetype. If it proves degenerate, ranged enemies
are the natural fix — and you'll know exactly why you're adding them.

### Lock breaking (parked, but build the hook now)

Inspired by Edge of Night: something makes a unit untargetable, every enemy
locked onto it drops the lock, and the normal "no target → scan" path takes over.

Build as a **general mechanism**, not a stealth special-case — target dies,
target becomes untargetable, target leaves range: one code path, one flag on the
target. Stealth, blinks, and anything later just set the flag.

Note this is much stronger here than in TFT. Breaking a lock in TFT redirects
damage. Breaking it here can send a clump of enemies walking at the exit,
since they re-scan, find nothing in range, and resume pathing. Closer to a
repositioning tool than a survivability one. Wants a real board to test on.

### Wave structure and spawn pacing

**Waves are discrete, with a fixed roster.** Wave 8 is a specific set of, say,
10 enemies. The wave ends when the roster is resolved — every enemy dead or
exited. No timer for now; if a stall turns out to be possible, add one then.

If one does appear, the occupancy guard below is the likely cause: enemies clog
back to the spawn hexes, spawning halts, and a roster that cannot finish entering
cannot resolve. Not worth pre-solving — just recognise it on sight rather than
debugging it from scratch.

Discrete beats continuous because it gives a readable round you can learn from,
it makes scoutable comps meaningful (a comp is a comp, not a rate), and it has no
congestion cliff at wave 30.

**Spawn pacing: timer sets the rate, occupancy is a guard.**

```
spawn when (SpawnInterval elapsed for this hex) AND (hex is free)
```

The timer does the design work. Occupancy only ever *delays* a spawn — it never
makes one happen sooner, and it exists to prevent spawning into an occupied tile.

Occupancy alone was tried and rejected: with 3 spawn hexes clearing in about a
second of walking, a 10-enemy roster lands in ~4 seconds. That's a lump, not
pacing.

The combination gives backpressure for free. Normal wave, the timer binds and
pacing is what you tuned. Wave going badly and enemies stacking at the door,
occupancy binds and entry slows on its own.

`SpawnInterval` is a placeholder number (§4). The thing it must be tuned against
is **board crossing time**: if the whole roster enters before the first enemy
reaches your line, the wave is one big fight; if entry outlasts a crossing, you
get a stream with an opening, a middle and a tail.

**Number of enabled spawn hexes is a per-level difficulty dial.** One is a
trickle into a chokepoint; five is a broad push that spreads your line thin.
Free lever, already in the data model.

### Enemy range

**Enemy range is 1 or 2 hexes.** Melee is **range 1** — adjacent. Range 0 would
mean "same hex", which one-unit-per-tile makes impossible.

This is not a ranged *archetype* (see §5, parked). It is an engagement-density
control. A pointy-top hex has six neighbours, so with all-melee enemies exactly
six can engage one blocker and the rest of the wave queues up doing nothing —
which looks broken and makes late waves drag. Range 2 lets a second rank attack
over the first.

Range 3 is reserved for something scary later.

**Do not scale range with wave number.** Range is the most discontinuous stat in
the game — 2 to 3 is a bigger jump than any HP or damage increase, it happens all
at once, and it silently invalidates board layouts that were working. To make
later waves pressure differently, scale the **proportion** of range-2 enemies in
a comp instead. Smoother, and each enemy's stats stay stable and readable.

### Scoutable waves

Waves have visible comps (e.g. "Wave 12: 6 Undead / 3 Bruiser") and can be
scouted a round or two ahead. Preserves the "read the lobby and tech a counter"
skill from TFT without needing other players, and gives the PVE AI a personality
to play against.

### Shop and bench

The shop is a row of champion slots refreshed by a gold reroll, drawn from a
shared pool with tier-weighted odds. Standard TFT shape.

**Buying puts a champion on the bench, never on the board.** The bench is a small
row of slots beneath the board. Every champion reaches the board by being dragged
there from the bench — there is no path where a purchase spawns a unit directly
onto a hex. Buying and placing are two separate decisions, and separating them is
what makes placement feel deliberate rather than automatic.

Consequences that matter:

- **Buying is safe; placing is the commitment.** The player can buy a champion
  they aren't sure where to put and sit on it. The shop stays a low-friction
  impulse and the board stays considered.
- **The bench is a holding area, not a second board.** Bench champions do not
  count toward trait totals, take no damage, and do nothing. Only the board
  counts.
- **A full bench is a real constraint.** With every slot occupied, the player must
  place or sell before buying again. This is a mild pressure toward committing,
  which is the right direction.

Dropping a champion onto an occupied hex swaps the two, as in TFT. Selling is
available from both bench and board and returns the champion to the pool.

Bench size, shop slot count, reroll cost, and sell refund are all placeholder
numbers — see Section 4.

### Star levels

As in TFT: three copies of the same champion at the same star level merge into one
copy a level higher. Everyone starts at 1 star; the cap is 3 (`MaxStarLevel`), and
the copies-per-merge count is `CopiesPerStarUp`, both in `EconomyConfig`.

- **The check runs when a copy is bought, not when units are moved.** Moving a unit
  doesn't change what the player owns, so there is nothing to re-check. It cascades:
  a new 2-star can complete a trio of 2-stars.
- **Bench and board both count.** The survivor is a board copy if there is one, else
  a bench copy; it is upgraded in place and keeps its position. The other two are
  removed.
- **A full bench doesn't block the copy that completes a merge** — it is consumed,
  never benched.
- **Buying is blocked while a unit is being carried.** A carried unit is on neither
  the bench nor the board, so a merge would miss it.
- **A merged unit is worth its copies.** Selling a 2-star refunds three copies' cost
  and returns three copies to the pool; a merge itself returns nothing.
- **Traits are unaffected.** A trait counts distinct champions, so a 2-star Grux is
  still one Bruiser, same as three Gruxes were.
- **Visual:** the mesh (not the actor, so the click target stays uniform) is scaled
  up per star level.

**Stat scaling (decided):** attack speed and range stay flat across star levels.
A champion's reach and cadence don't change; range in particular is a hex count the
targeting model depends on. Health and attack damage are the stats that scale,
multiplicatively per level, as in TFT (roughly ×1.8 health and ×1.5 damage per
level there, so 3-star is ×3.24 and ×2.25); the actual multipliers are tuning data
read by the champion's initial GameplayEffect once GAS exists.

Unresolved: the per-star multipliers, and whether a star-up heals the survivor.
Both wait on GAS and interact with persistent damage (below) and with the
sell-value rule in Section 4, "Selling damaged units".

### Persistent damage

Units do **not** reset to full HP between waves. Wounded stays wounded, dead
stays dead. Damaged units function normally — HP is just HP.

Healing between waves is a paid action. Death is not the routine expense; death
is what happens when you fall behind on the routine expense. This makes backline
healers and shielders genuine **economy** pieces — a support that heals 30%
between waves is worth its slot in gold saved.

That only holds if healing can't be bypassed by selling a wounded unit and buying
a fresh one — see Section 4, "Selling damaged units".

### Free repositioning between waves

Full repositioning in the shop/prep phase, like TFT. No moving units mid-combat.

Repositioning covers the bench too: board to bench, bench to board, and bench to
bench are all free during prep. Pulling a badly-hurt unit off the board to the
bench is a legitimate move, and one the player will reach for often once
persistent damage bites.

Persistent damage and free repositioning interact well: rotate wounded units
back, fresh ones forward, park the damaged near healers. Fiddly board management
that autobattler players enjoy.

### Gold collection

**Kill gold is the wave income.** There is no flat per-wave payout: each enemy
carries a gold value, and the moment it dies that gold **auto-collects** into the
player's total. No mandatory clicking. The HUD shows a `+N` at the gold counter for
each kill, so the player watches income arrive. Gold earned mid-wave can't be spent
until the wave ends (shop, sell and heal all lock during combat), so it builds up
and is spent in prep. Interest, streaks and anything like them are separate
systems, not part of this. Rare and elite enemies drop a bounty the player has a
few seconds to click for.

Consequence, and intended: an enemy that leaks drops nothing, so a leak forfeits
that enemy's gold. That is a punishment for leaking in its own right — see "What a
leak costs".

Rationale: PvZ sun-clicking works because that game has downtime. Autobattler
combat is dense and the pleasure is watching it resolve. Attention gets rewarded,
never taxed.

### Lose condition — "checkmate"

The player loses when they have no units on the board **and** none on the bench
**and** not enough gold to buy one. No moves left to play.

The bench clause matters: a champion sitting on the bench is a move the player
still has, so an empty board alone is not a loss. The condition is "no piece can
reach the board", not "the board is empty".

Falls naturally out of persistent death plus gold; no new systems needed. Nice
property: a player down to one unit and no gold can theoretically claw back if
that unit clears a wave. Rare, but memorable.

**Not chosen:** gold acting directly as health. Spending all your gold to field
the strongest possible board should never be what kills you.

### Win condition / run structure

Effectively endless. Soft cap around wave 50 as a "you win" ceiling most runs
never reach. Replayability comes from high scores — "I got to 23 last time, can I
hit 30?"

### Gameplay Ability System

Stats, abilities, and trait effects are built on GAS.

**What it covers:** health, mana, attack damage, attack speed, armor, and other
combat attributes; abilities with mana costs and cooldowns; trait bonuses;
buffs, debuffs, and status effects.

**The part that matters most is GameplayTags.** Trait counting becomes tag
counting across the board, threshold checks become tag queries, and a trait bonus
becomes a GameplayEffect applied to every unit carrying the matching tag. This is
close to how TFT works internally, and hand-rolling an equivalent later is
miserable. Do not build a parallel trait system alongside it.

**A trait counts distinct champions, not bodies**, as in TFT. Champion identity is
its `ChampionData` asset, so two Gruxes are one Bruiser; the bonus itself still
applies to every unit carrying the tag. That makes thresholds a roster-size
question: "7 Woodland" needs seven *different* Woodland champions, so threshold
values must be set against the roster that exists, not against an ideal one. Every
copy of a champion is still a body, so a wall of identical cheap tanks stays a
legitimate blocker — it just adds no trait count.

**Mana maps cleanly.** TFT units gain mana on attack and on damage taken, then
cast at a threshold — an attribute with two gain sources and an ability with a
cost. Standard GAS shape.

**Targeting stays out of GAS.** The aggro model is custom (path-length nearest,
lock until death, two-condition acquisition) and doesn't map onto
`AGameplayAbilityTargetActor`, which solves a different problem.
`TargetingComponent` selects the target and hands it to the ability as data.

The one exception is the untargetable flag, which fits naturally as a tag
(`State.Untargetable`) that the targeting scan filters on. That is the general
lock-break mechanism described above — stealth, blinks, and anything later just
apply the tag.

**Persistent damage needs explicit handling.** GAS attributes normally live and
die with the actor, and most GAS material assumes a reset each round. Units here
keep their HP across waves. Simplest approach: unit actors survive between waves
and are never destroyed and respawned. Otherwise attribute values have to be
serialized out and restored.

**Introduce it at Step 2, not Step 1.** GAS has real setup cost — ability system
component, attribute set, effect boilerplate, plugin config that's easy to get
subtly wrong. Step 1 is one enemy pathing across a grid and needs none of it.
Budget real time for the first ability; the second will be much faster.

### Danger model

Two failure modes, and they are different.

**Being overrun.** Your kill rate versus their arrival rate. Ahead of the curve,
the front of a roster dies before the back has spawned — that is the reward for a
strong board, and it is legible while it happens. Behind the curve, the clump
grows and never recovers within the wave, because nothing of yours gets stronger
mid-wave while their numbers do.

Note this is a **threshold, not a slope**: just above the line is a clean wave,
just below is a collapse, and a small swing in board strength can separate them.
That is sharp for a scaling curve and it is the most likely source of a wave that
feels unfair. The natural softener is AoE damage, which gets *better* as the
clump grows — worth having at least one AoE-ish champion so the failure curve
isn't a cliff.

Being overrun is also **spatial**: enemies that stop to fight occupy hexes, so a
growing clump pushes deeper into your zone until enemies stop next to your
backline. Falling behind compresses your formation and exposes the squishy
things. Good failure mode — don't design it out.

**Attrition.** Persistent damage means a bad wave makes the next wave harder.

**These two are not yet independent.** With leaks free, the only cost of being
overrun is the damage your units take while the clump grows — which is attrition.
Overrun is currently the mechanism and attrition is the whole bill. They separate
once a leak costs something, and that is a large part of what the leak cost is
for. Until then, wave design has one real pressure to lean on, not two.

---

## 4. Unresolved — deferred until playtesting

### Economy — all numbers

Explicitly deferred. Cannot be tuned from a document.

Open questions: gold per kill (flat or by enemy type?), interest and streaks (there
is no flat per-wave income; kills are the income), reroll cost, unit tier pricing,
healing cost, gold in vs gold out across a 20+ wave run.

**What absorbs gold at wave 30+?** Once the board is full and units are maxed,
something has to keep consuming income or the run becomes a formality until the
wall. Candidates: terrain rebuilds, scaling healing costs, unit replacement.

Rough starting points, purely so there's something to push against — all of these
are wrong, they're just wrong in a specific enough way to learn from:

- Unit cost 1/2/3 by tier
- No flat wave income; kill gold only, about 1 gold per enemy to start
- Healing roughly 1 gold per 20% HP restored
- 5 shop slots, 6 bench slots, reroll 2 gold, sell refunds full purchase price
  (at full HP — see "Selling damaged units")

### Selling damaged units (healing laundering)

**The problem.** Damage persists between waves and healing is a paid action, so a
healer is worth its slot in gold saved. But if selling refunds the full price, a
player can sell a wounded unit and buy a fresh one for nothing — a free full heal,
and healers stop mattering. With a roster of 3–4 champions and 5 shop slots, the
sold champion is almost always in the shop to buy back, so shop randomness is not
enough friction. Applies mainly to 1-star units: selling a 2-star returns 3 copies
and rebuying needs 3 copies out of the shop plus a merge (rerolls, bench room); a
3-star needs 9.

**Leading candidate (not decided; needs GAS, since HP doesn't exist before it).**
Sell value depends on HP, binary:

- **At full HP:** full purchase price. Buying the wrong champion, or a unit that
  fought without taking damage, still sells for full price.
- **Below full HP:** a lower refund, `DamagedSellRefundPercentage` of price. A
  fraction of price rather than a raw gold amount so it scales with tier and star.
- **Merged units:** copies × (full price or damaged refund), so a damaged 2-star
  gets 3× the damaged refund.
- Tunable `FullPriceHpThreshold`, default 100%, so the cliff below can be softened
  (e.g. 95%) without changing the rule.

HP becomes a second currency: undamaged units are liquid, wounded ones aren't, and
healing back to full restores resale value on top of fighting capacity.

**The ratio that has to hold.** Any sell-based rule is capped: the worst penalty is
losing the whole price. If a full heal costs more than a unit, replacing a badly
hurt unit is always cheaper than healing it, whatever the sell rule. At roughly
1 gold per 20% HP a full heal is 5 gold, and a 1–3 gold unit can never be penalized
by 5. So **price minus damaged refund must be at least the cost of a full heal.**
Either scale unit prices up (e.g. 10/20/30 with a 50% damaged refund) or make
healing cheaper. Either way this moves only `EconomyConfig` numbers, all of which
are placeholders.

**Known downsides.**
- **A cliff.** 1 HP of damage costs the whole price gap when selling, and healing
  the last sliver is suddenly worth the whole gap, so players will feel pushed to
  top off. That may be a feature (healers matter more); `FullPriceHpThreshold`
  softens it if not.
- **The penalty doesn't grow with damage.** A unit at 99% and one at 10% lose the
  same amount on sale; the ratio rule above is what keeps that from being a leak.
- **The UI must show the sell value** and its change, or it reads as a hidden tax.

**Related rules to hold.**
- **Benching must not heal.** Bench units take no damage and do nothing; if
  they also recover HP, that is a free heal that skips the sell rule entirely.
  Healing happens only through the paid action.
- **Star-up keeps the HP fraction** (decided). A star-up scales max
  HP, and carrying over absolute HP would leave a merged unit looking badly
  hurt; preserving the fraction is neutral — no free heal, no penalty. A bonus heal
  on star-up (e.g. 50% of missing HP) is not exploitable, since a merge costs two
  extra copies, but it makes merges double as heals; wait until healing prices
  feel right in play.

**Considered, not chosen.** A flat sell fee (punishes selling healthy units,
coarse at 1–3 gold, gives up "sell is a free undo"); a sell value proportional to
HP fraction (coarse at integer gold, needs a floor rule to bite at all); accepting
the leak for 1-stars (bounded by a cheap unit's price, but it guts the healer's
economic role).

### What a leak costs

**That it will cost something is decided. What, is not.** Candidates: gold loss
on leak, reduced income for the wave, damage to a random unit on the way out, a
score penalty.

The instrument comes first. Leaks per wave and cumulative leaks against a
competent board at wave 5 versus wave 15 tell you which lever fits and how hard
it needs to pull. Until a cost exists, leaking drains pressure off a wave for
free, so early difficulty readings are softer than the finished game should be —
read them with that in mind rather than tuning wave sizes up to compensate.

One punishment is already in place: kill gold is the only income, so an enemy that
leaks drops no gold and the player forfeits that enemy's share. It needs no code; it
falls out of the income model. It is a real cost, and **additional** to whatever is
chosen above — the chosen cost stacks on top, so size it knowing leaks already
hurt. The counter itself still costs nothing directly and the decision above stays
open, but the "leaking is free" caveat is softer than it reads.

### Spawn interval

`SpawnInterval` per spawn hex. Must be tuned against board crossing time, which
Checkpoint 1 measures. Placeholder until then.

### The death spiral

Conditional on leaks eventually costing something (see "What a leak costs"). If
they hurt the economy, a spiral is possible. This is probably **correct** for
an endless high-score game — it makes endings decisive instead of a slow bleed,
and gives the run a story ("wave 23 went badly and it all came apart").

Requirement: the spiral must be **fast**. Two or three waves to resolution. If
playtesting shows someone limping for six waves knowing they're dead, tighten it.

### Terrain regrowth specifics

Cost? Cooldown? Wave-based timer? Trait-dependent? Undecided.

### Items

Barely discussed. Unclear whether the game wants them at all.

---

## 5. Parked — good ideas, not now

Liked or at least interesting, but explicitly excluded from MVP to avoid feature
sprawl.

- **Augments.** TFT-style, three choices at waves 3/9/15. Cheapest way to make
  runs feel different. Strong candidate for first post-MVP addition.
- **Mid-combat repositioning.** A limited number of "commands" per wave to drag a
  unit during the fight. Revisit after the prototype proves the basic loop.
- **Bosses.** Board-eating bosses that permanently destroy hexes and force
  re-comping into tighter formations. Boss design generally is a later problem.
- **Enemy-side terrain.** Waves that bring their own — corruption tiles blocking
  tree spawns, engineers bridging chokepoints. Good for giving late waves
  personality beyond stat scaling.
- **Subtractive terrain trait.** Something like Tide or Earthshaper that removes
  hexes / collapses ground into chasms. Natural counterpart to Woodland.
- **Wounded units perform worse.** Damaged units losing effectiveness, not just
  sitting closer to death. Cut because it's a whole stat-degradation system
  hiding in one bullet — which stats, on what curve, communicated how? HP alone
  communicates "this unit is in trouble" fine for a prototype. Revisit only if
  playtesting shows attrition doesn't feel like anything.
- **Leaks damaging the economy.** A leak costing gold, income, or a shop slot
  rather than a generic HP bar. This is the leading candidate for what a leak
  eventually does (§4), but it is a tuning problem wearing a design idea's
  clothes — it needs real income curves and real leak counts first.

- **Ranged enemy archetype.** Enemies that outrange your board and must be
  approached. Cut from MVP as too much complexity to balance right now — not
  rejected, deferred. Note the current range-1/2 values are engagement-density
  control, not this. The trigger to revisit: if a wall of cheap tanks stops
  everything and assassins are carrying the entire job of answering it.
- **Terrain that blocks line of fire.** Range uses hex distance, so enemies
  currently shoot through trees. That is the accepted complexity trade, and this
  is its eventual fix — as a specific terrain type, not a global rule. Cheap
  middle ground if it looks bad sooner: a straight-line hex trace between the two
  coords, no pathfinding.
- **Terrain that blocks assassin leaps.** A targeted answer to a specific threat
  rather than a generic wall, which is what makes it interesting.
- **Pits.** Terrain that assassins clear but ground units cannot. Same family as
  above — terrain types that different archetypes interact with differently.

### Rejected (for now)

- Gold that must be manually picked up off the ground by units
- Depth bonuses (front row = more damage, back row = more range). Range should be
  a property of the champion, not the hex
- Salvage/unlock enemy types into your shop by killing them
- Calling waves early for bonus interest
- Two separate lanes / two spawn entrances. Fights against the clarity of "the
  board is the lane"
- **An HP crystal / nexus.** Cut. A bar behind the board plus enemies sprinting
  at it is a PvZ shape, and it made an entire archetype a pure damage check. The
  exit keeps the pull that terrain needs without the bar
- **Runners.** Replaced by assassins. No positional counterplay

---

## 6. MVP scope

Deliberately minimal. Everything not on this list is a later problem.

- 2 traits
- 2–3 champions
- A few enemy types, including at least one assassin and one terrain-breaker.
  Enemy range 1 or 2 only; no ranged archetype
- One terrain type (trees)
- A shop, with a bench that bought champions land on
- A basic economy
- Persistent damage plus between-wave healing
- Wave spawning with scoutable comps
- Lose condition (checkmate)
- An exit behind the back row with a leak counter that does nothing yet
- Endless waves

### Art and assets

All characters — player champions and enemies — use the free **Paragon** asset
packs from Epic on Fab. Free for use in Unreal Engine projects.

Good fit: already-rigged hero characters with full animation sets (idle, run,
attack, ability, death), which is exactly the coverage an autobattler needs and
the thing that usually blocks a solo prototype. Zero art pipeline for MVP.

Differentiate player units from enemies by material tint or an outline shader
rather than by picking visually distinct characters, since the same packs supply
both sides.

**Watch for** (not a blocker, just know it's coming): Paragon characters are
high-poly MOBA heroes built for a handful on screen at once, not 35 units plus a
wave. Expect to need LODs and possibly lower-fidelity enemy variants once boards
fill. Their scale and animation timing are tuned for a MOBA, not a hex board —
root motion and attack windups will need adjusting to read clearly at autobattler
zoom.

---

## 7. Build order

Ordered to answer the riskiest questions fastest, not in the order that feels
most natural.

**This section is the design rationale for the sequence, not the current work
queue.** `PLAN.md` holds the active checkpoint, which deliberately front-loads
the board, controls, shop, bench, and placement so there is something playable to
iterate against before combat exists. Enemies and combat follow in a later
checkpoint. The reordering is sanctioned; the reasoning below still explains why
each step exists and still governs everything `PLAN.md` has not reached.

The one thing the reorder puts at risk is Step 1's payload — board crossing time,
the number every other number is tuned against. `PLAN.md` preserves it with a
debug walker: it steps hex by hex across the grid via the pathfinder, no AI, no
combat, no enemy class.

**Step 1 — One enemy walking to the exit.**
No units, no shop, no combat. Spawn point, pathfinding across the hex grid, reach
the back row, despawn, increment the leak counter. Least fun milestone in the
project and the most important, because it establishes whether the board size and
enemy speed produce a fight that lasts 20 seconds or 4. Every other number is
tuned against that one — including `SpawnInterval`.

**Step 2 — One unit that shoots it.**
Now there's a game loop. Ugly, but real.

**Step 3 — Blockers.**
Melee unit stands on a hex, enemy engages instead of walking past. Ordinary
enemies only — assassins need mana and therefore GAS, so they arrive after step 2
at the earliest, and realistically alongside the first real abilities. Where the
aggro model gets its real answer, and where you find out whether pathing handles
"a unit is in the way" gracefully or whether enemies pile up stupidly.

**Step 4 — Terrain.**
One tree, placed manually, no trait attached yet. Confirm it reroutes pathing
cleanly. This is the whole hook — much better to learn it's finicky in week two
than month three.

**Step 5 — Shop and traits.**
Last, because these are the best-understood parts and the least likely to
surprise you.

### Technical notes

**Grid pathfinding, not NavMesh.** Hex-based blocking and dynamic terrain and
occupancy want deterministic tile costs, and NavMesh rebuilds mid-wave are a
headache worth avoiding. The search runs as a distance query over the tile array
and is consulted one step at a time; "A*" elsewhere in these docs means it.

**The grid is data, not actors.** A flat array of tile structs — coordinate,
walkable flag, occupant reference, terrain reference, path cost. Visual hex
meshes read from it; pathfinding never touches an actor. This is the decision
that hurts most to reverse: if pathing queries actors, you end up doing traces
mid-wave and the terrain system gets welded to rendering.

**No stored paths.** Enemies keep no route. Each arrival on a hex re-asks the
distance search for the next step, so there is nothing to invalidate when terrain
is destroyed or a hex fills — the next arrival simply sees the new board. One
search per arrival is cheap at 56 tiles; if it ever isn't, share one search across
every enemy heading for the exit. Do not build a path cache or dirty flag until
measurement says so.

---

## 8. The next real questions

**Is 35 placeable hexes the right amount?**
Units and terrain share the same 35. Enough for a wall-of-bodies build and for
chokepoint / lane-narrowing terrain builds. A deep winding maze probably isn't
available at this depth — that's fine, mazing is one build among several. Step 4
tells you whether terrain builds feel cramped.

If they do, two dials in order of preference:

1. Extend the player's zone to 6 rows, keeping the board 8 deep
2. Deepen the board to 10 rows and keep the half-and-half split

Don't pre-solve this. Just don't hardcode the zone depth.

**Does one cheap unit trivialize a wave?**
Units are hard stops, so a single unit in range halts every regular enemy that
reaches it. Step 3 answers whether that's a satisfying
time-buying strategy or a degenerate one. With no ranged enemy archetype, a wall
of cheap tanks is the specific version to watch, and assassins are currently the
only answer to it.

**How does spawn interval relate to board crossing time?**
The ratio decides whether a wave is one big fight or a stream with a tail. Both
numbers are placeholders; crossing time is measured in Checkpoint 1 and
`SpawnInterval` is set against it afterwards.

**Do leaks need to cost anything?**
The counter is instrumented and inert. If a competent board leaks nothing until
wave 12, leak damage isn't the difficulty lever and the exit stays a despawn.

Everything after that is a playtest question, not a design-document question.
