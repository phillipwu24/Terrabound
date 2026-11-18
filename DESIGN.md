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

**Revision, 2026-11-17.** Champions are now stationary. Play is structured as
rounds of waves with a Start button. Units physically stop enemies: route
planning ignores units, enemies walk into a champion on their route and fight it,
and enemies cannot pass each other. Damage lasts for a round, living champions
heal to full between rounds, and death is permanent. Waves trickle in on one
randomized wave timer with wave-progress spawn windows. Champion targeting is
defined. Sections touched: 1, 2 (Blocking model, Seal prevention), 3 (Rounds and
waves, Spawn pacing, Champion combat, Enemy aggro, Damage/healing/death,
Repositioning, Danger model), 4, 5, 6, 7.

---

## 1. Pitch

A single-player roguelike that fuses TFT-style autobattler mechanics with tower
defense. The player buys units from a shop, places them on a hex board, and
watches them defend against enemies that spawn on the far edge over the course
of each wave and push toward the player's side.

Unlike TFT, there is no opposing player board, and enemies arrive over time
rather than standing in formation at the start. Unlike tower defense, the
"towers" are champions with traits, synergies, and stat scaling, and the player
manages an economy between rounds.

**Champions are stationary.** Once a round starts, nothing the player owns
moves. Ranged champions shoot anything in range; melee champions hit whatever is
adjacent. Position is decided in prep and lived with for the whole round. This is
deliberate: when your units cannot move, the only thing that changes where a
fight happens is where the enemies walk — and that is what terrain controls.

**Fewer, stronger, more varied enemies — not hordes.** A wave is a handful of
distinct enemies (say, four melee and three ranged) arriving over time. Closer to
a TFT opponent's board delivered in pieces than to a PvZ horde.

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
  TFT visually and gives terrain more surface to work with. Flat-top (straight
  columns toward the exit) was considered and declined on 2026-11-17: it gives
  more lane-like paths but makes front lines and the spawn/exit edges jagged.
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
gets past your line and exits stops attacking your units and disappears, so
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

**Instrument it from the start.** Count leaks per round and cumulatively, and log
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
  calculated, are undecided.** It is one of the two levers (with small, trickled
  waves) that keep battles small-scale — see "Blocking model". A row is 7 wide,
  so a cap of 6 or less makes a full unit-only wall impossible, which matters
  because a champion holds everything queued behind the enemy fighting it. Bench champions do
  not count toward it. Swaps never change the board count, so they are always
  allowed; only bench→empty hex is subject to the cap, and the drag preview shows
  hexes invalid once the board is full.

### Spawn hexes

The entire far edge is spawnable in principle, but which hexes are live is a
**per-level designer choice** — not a player choice, not a runtime count.

Store as a per-level array of enabled spawn hexes, so a level can specify "left
three only" or "both flanks, no center." Leaves room for per-wave control later.

Within a wave, *which* enabled spawn hex a given enemy uses is not authored — it
spawns from a random free one. See "Spawn pacing".

Spawn hexes are never placeable. Keep per-tile flags independent: `is_spawn`,
`is_placeable`, `is_walkable`, plus occupant and terrain references.

### Blocking model

Terrain and units both stop movement, but in different ways, and keeping them
different is what keeps terrain meaningful:

| | Enemies plan routes around it? | Can anything move through it? | Role |
|---|---|---|---|
| **Terrain** | Yes — impassable in the search | No | **Redirects** — decides where enemies go |
| **Units** (champions and enemies alike) | No — invisible to route planning | No — must be killed or waited out | **Stops** — decides where enemies are held |

**Terrain redirects.** Enemies treat it as a wall and route around it.

**Units stop.** Enemies do not plan routes around champions. They walk their
route, and when a champion is in the way they stop and fight through it. If
enemies routed around units the way they route around trees, a line of cheap
champions would be a maze wall and terrain would be redundant.

**Nothing passes through anything.** One unit per hex, champion or enemy, and no
unit ever steps into an occupied hex — not a champion's and not another enemy's.
Enemies cannot walk past each other. This is TFT's rule rather than PvZ's, and it
suits small waves of distinct enemies: order and formation matter, and a tank at
the front of a wave really is in front. Small rosters trickling in over time are
what keep this from turning into traffic jams.

**Movement is decided one hex at a time.** Units keep no route. On arriving at a
hex an enemy chooses its next, and aggro takes precedence: with a target in range
it stops and attacks; with a target out of range it steps toward a free hex within
range of it; with none it steps toward the exit. "Toward" means lower *path*
distance from a search over the tile array — not raw hex distance, which stalls
behind terrain. Nothing is cached, so nothing goes stale when a hex fills or a tree
falls. The search ignores units entirely; occupancy is enforced only by the step.

**What a blocked enemy does:**

- **Its next step is a champion's hex** → that champion becomes its target and it
  fights from where it stands. If it was locked onto something else, the lock
  drops — it was walking, so there is no attack windup to reset.
- **The attack hex it wants is held by another enemy** → it sidesteps to another
  free hex within range of the same target. If none is free, it waits where it is,
  keeping its target, until one opens. This is how TFT units crowd around a
  target.
- **It is walking to the exit and its next step holds another enemy** → it waits
  for the hex to clear. This is a default, not a considered decision; if queues
  look bad, the first thing to try is letting it take an equally short
  alternative step, which pointy-top's two forward diagonals often provide (§4).

**Why this replaced the flat-cost model.** The previous rule (2026-09-20) priced
an occupied hex at one flat high cost in the search, so enemies flowed through
gaps around champions and only walked into one where a wall left no gap. That
made champions a soft second kind of terrain. With champions stationary, terrain
should be the only thing that decides where enemies walk; champions decide where
they are held. The original problem the flat cost solved — an enemy routing *into*
a hex it may not enter — is now answered directly: the champion on its route
becomes its target.

Waiting is a possible stall source. Small waves make it rare, but it matters more
than it used to: the next wave does not start until the current one resolves, so
one stuck enemy holds up the round. Deliberately not pre-solved — handle it if
playtesting shows it (§4).

### Seal prevention

The board is only 7 wide, so a single row of terrain closes it entirely.

Champions do not count toward a seal. Enemies fight through a champion rather
than routing around it, so a wall of champions is a fight, not a seal. Terrain is
different: enemies route around it, and any enemy without a target needs a route
to the exit. Seal the board with terrain and those enemies have nowhere to go and
stop.

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

With champions stationary, this matters more: once a round starts, where enemies
walk is the only thing about the fight's geography that can still be shaped, and
terrain is what shapes it.

This is why the exit survived the crystal being cut.

### Terrain destruction

Terrain is mostly indestructible by design — a strategic tool the player uses to
shape their own defense. But a few specific enemy archetypes can destroy it, and
bosses or large waves can stomp through.

Destroyed terrain is **not permanently lost**, but also **not free to restore**.
There must be a cost or cooldown — gold price, wave timer, regrowth condition.
Mechanism undecided.

### Champion combat

**Champions never move during a round.** No walking, no stepping toward an enemy.

- **Ranged champions** lock onto the **closest enemy in range** and keep that
  target until it dies, leaves range, or the lock is broken by a taunt or an
  interrupt. Then they re-scan. "Closest" is hex distance — a champion never
  walks, so path length means nothing to it. No free-hex test either, for the
  same reason.
- **Melee champions** attack enemies in adjacent hexes. Whether they lock the
  same way when several enemies are adjacent is open (§4); locking is the default
  assumption.
- **Healers heal during combat only.** Nothing heals between waves. The full heal
  at the start of prep is free, so a healer's value is keeping a board standing
  across the waves of a round.
- Taunts and interrupts use the general lock-break mechanism below — one code
  path for every way a lock ends, on either side.

Stationary champions put weight on range: a ranged champion's coverage is a fixed
circle of hexes for the whole round, and terrain's job is to steer enemies
through it.

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
- **Has target** → hold until the target dies. No re-scan. The exceptions are the
  general lock-break cases below — notably a champion blocking its route, which
  makes that champion the new target (§2, "Blocking model").
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
| Which target is nearest (enemies)? | **Path length** | Diverges from hex distance once terrain exists; affordable at 56 tiles |
| Is a target in range? | **Hex distance** | Closed-form on the coords, no A* in the tick loop |

Champions use hex distance for both, since they do not move.

**Contested hexes are resolved by re-checking, not by reserving.** Two enemies can
acquire the same champion, see the same single free hex, and both head for it.
Occupancy is taken when an enemy **begins the step** into a hex and released from
the one it leaves — not on arrival, or two enemies mid-move into the same hex both
complete and stack. The one that loses the race **sidesteps**: if another hex in
range of the same champion is free, it heads there; if none is, it **waits where
it is, keeping its lock**, until one opens.

(This replaces an earlier rule where the loser dropped its lock and fell back to
exit-pathing. With enemies unable to pass each other, crowding around a target is
the TFT behaviour and the better read.)

This does not violate the no-flip-flop rule: the enemy keeps its target, and it
was walking anyway, so there is no windup to reset.

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
neighbours are all occupied by other champions cannot be acquired by any melee
enemy — there is nowhere to stand — so melee enemies fight the outer ring instead.
Range-2 enemies can still hit it from a ring further out where hexes are free.
Tight formations protect their interior from melee, which is a real positioning
decision and the same mechanism as the assassin bodyblock counter.

### Assassins (replaces Runners)

The backline threat. **Runners are cut** — an enemy that ignores everything and
sprints at a crystal is a damage check you either pass or fail, with no
positional counterplay. Assassins ask a question you answer by arranging your
board.

**Passive pathing is ordinary.** An assassin walks like any other enemy, is
blocked by terrain, champions and other enemies like any other enemy, and
acquires the nearest player unit in range like any other enemy. Out of combat it
is not special.

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
odds they live to cast at all.

**Watch for the inert assassin.** The self-leap fallback means a board with no
free hex in leap range turns the cast into nothing at all. With no unit cap
yet (one is planned), a late-game board that is simply *full* gets that for free, without the player ever
deciding to bodyblock. If assassins stop being a threat exactly when boards fill
up, the fallback is doing too much work and the cast should probably do something
on a failed landing — damage where it stands, or a shorter hop. Playtest question,
but this is the specific thing to watch.

**Scoping note:** the leap needs mana, which needs GAS. Assassins are not an
early enemy. First enemies are dumber than this.

### Consequence to watch: the blocker

Champions are hard stops for anything that walks into them, and enemies cannot
pass each other either. So a single cheap champion in a lane holds not only the
enemy fighting it but everything queued behind that enemy.

That is more stopping power than the earlier model gave a unit, and a lot for one
gold. Chip units become valuable time-buyers.

**Watch for the blocker wall.** With no ranged enemy archetype (see §5), a line
of cheap tanks may stop everything. Range-2 enemies can attack over the enemy in
front of them, and assassins leap past the line, but that is a lot of load on two
answers. The unit cap is the structural limit: at 6 or less a full wall is
impossible. If it still proves degenerate, ranged enemies are the natural fix —
and you'll know exactly why you're adding them.

### Lock breaking (parked, but build the hook now)

Inspired by Edge of Night: something makes a unit untargetable, every enemy
locked onto it drops the lock, and the normal "no target → scan" path takes over.

Build as a **general mechanism**, not a stealth special-case. Every way a lock
ends goes through one code path: target dies, target becomes untargetable, target
leaves range, a champion blocks the route (enemies), taunted, interrupted. One
flag or tag on the target where that applies. Stealth, blinks, taunts and
anything later just set it. The mechanism serves champions and enemies alike.

Note this is much stronger here than in TFT. Breaking a lock in TFT redirects
damage. Breaking it here can send a clump of enemies walking at the exit,
since they re-scan, find nothing in range, and resume pathing. Closer to a
repositioning tool than a survivability one. Wants a real board to test on.

### Rounds and waves

Two words, two things. Use "round", not "level" — level means an Unreal map.

**A round** is a prep phase followed by a fixed list of waves — currently **3**,
a placeholder. The count is per-round data (each round's row lists its waves),
not a constant.

**Prep (between rounds).** The player buys, sells, and freely repositions
champions and terrain, on the board and bench. Every living champion heals to
full. Dead champions stay dead. The round begins when the player presses
**Start**. There is no prep timer.

**Waves run back to back with no player input.** A wave begins spawning only once
every enemy of the previous wave is resolved — dead or exited — and a short gap
(placeholder 3 s) has passed. The gap is a breather that marks where one wave
ends and the next begins; it is not a prep phase. Between waves
there is no repositioning and no healing; the board ends one wave in exactly the
state it starts the next. The round ends when its last wave is resolved, and play
returns to prep.

That gives a round a shape: three tests of one board, with damage accumulating
across them. Prep is where decisions are made; the round is where they are
judged.

Where a wave number used to stand for the run's progression ("wave 30", "a soft
cap around wave 50"), read round number. Leaks are counted per round and in total.

**A wave is a fixed roster.** A given wave is a specific set of enemies — say,
four Grux and three Belica. What is *not* authored is exactly when each enemy
spawns or which spawn hex it uses; both come from the spawn timer and from which
spawn hexes are free at the time. The roster is the design; the delivery varies
from run to run.

**Wave progress** is the share of the roster **resolved** — dead or exited: 0%
when the wave begins, 50% once half of it is gone. Exits count, so a leak can never
freeze progress short of a threshold. Progress gates *when* a roster entry is
allowed to spawn. Each entry carries a spawn window:

- **Early** — first in line while progress is low (e.g. under ~10%). An early
  entry that has not spawned by the time progress passes the threshold becomes
  **Any**: early means "front of the queue", not "now or never", so a wave can
  never strand an enemy that missed its window.
- **Any** — whenever a spawn is due.
- **Late** — only once progress is high (e.g. over ~95%), or once every non-late
  enemy of the wave is resolved. Without the second condition, a roster whose
  late entries exceed the share above the threshold could never reach it, and the
  wave would hang. This is the same "nothing is stranded" rule as for early entries.

The thresholds are placeholders. The main use is **fast enemies spawning early or
late**, when the board is clear enough for speed to mean something. Mid-wave, with
nobody able to pass anybody, a fast enemy stuck behind a slow one is just a slow
enemy.

Discrete waves beat continuous spawning because they give a readable unit you can
learn from, they make scoutable comps meaningful (a comp is a comp, not a rate),
and they have no congestion cliff late in a run.

### Spawn pacing

**One randomized timer per wave sets the rate, occupancy is a guard, windows set
eligibility.**

```
the first enemy of a wave spawns immediately
then: wait a random delay in [MinSpawnInterval, MaxSpawnInterval]
when it has elapsed AND a spawn hex is free AND some roster entry's window is open:
    spawn that entry on a random free spawn hex, draw the next delay
otherwise the timer stays elapsed and the spawn happens the moment all three hold
```

At most one enemy enters per delay, from whichever spawn hex is free. One timer
for the wave, not one per hex: the entry rate is one number to reason about — a
7-enemy wave takes roughly 7 × the mean delay to enter, however many spawn hexes
are enabled. The randomness matches "timing is not authored" better than a fixed
interval while still preventing bursts.

The timer does the design work. Occupancy only ever *delays* a spawn — it never
makes one happen sooner, and it exists to prevent spawning into an occupied tile.
The combination gives backpressure for free: normally the timer binds and pacing
is what you tuned; with enemies stacking at the door, occupancy binds and entry
slows on its own. With enemies unable to pass each other, a jam at the entrance
throttles arrivals directly.

History: a per-hex `SpawnInterval` timer came first; then occupancy-only spawning
(no timers) was adopted briefly on 2026-11-17 with the pacing moved to the wave
split, and rejected the same day because a whole wave lands in about a second.
Occupancy alone is a lump, not pacing.

If a wave ever cannot finish, the occupancy guard is a likely cause: enemies clog
back to the spawn hexes, spawning halts, and a roster that cannot finish entering
cannot resolve. Not worth pre-solving — just recognise it on sight.

`MinSpawnInterval` / `MaxSpawnInterval` are placeholder numbers (§4), held for
now on the level's spawn configuration next to the spawn hexes. The thing they
must be tuned against is **board crossing time** (3.5 s): if the whole roster
enters before the first enemy reaches your line, the wave is one big fight; if
entry outlasts a crossing, you get a stream with an opening, a middle and a tail.

**Number of enabled spawn hexes is a per-level difficulty dial** — for *width*,
not rate. One is a trickle into a chokepoint; five is a broad push that spreads
your line thin. The two dials stay independent: hex count decides how wide the
push is, the timer decides how fast it comes.

### Enemy range

**Enemy range is 1 or 2 hexes.** Melee is **range 1** — adjacent. Range 0 would
mean "same hex", which one-unit-per-tile makes impossible.

This is not a ranged *archetype* (see §5, parked). It is an engagement-density
control. A pointy-top hex has six neighbours, so with all-melee enemies at most
six can engage one blocker and the rest of the wave queues up doing nothing —
which looks broken and drags. Range 2 lets a second rank attack over the first,
which matters more now that enemies cannot walk past each other. Worth keeping a
few range-2 enemies in most waves.

Range 3 is reserved for something scary later.

**Do not scale range with wave number.** Range is the most discontinuous stat in
the game — 2 to 3 is a bigger jump than any HP or damage increase, it happens all
at once, and it silently invalidates board layouts that were working. To make
later waves pressure differently, scale the **proportion** of range-2 enemies in
a comp instead. Smoother, and each enemy's stats stay stable and readable.

### Scoutable waves

The upcoming round's waves have visible comps (e.g. "Wave 2: 4 Undead / 3
Bruiser") during prep, and can be scouted a round or two ahead. Preserves the "read the lobby and tech a counter"
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

Unresolved: the per-star multipliers, which wait on GAS. Whether a star-up heals
the survivor no longer matters: a merge only happens on a buy, buying is prep-only,
and every living champion is at full HP in prep. The decided rule — star-up keeps
the HP fraction — stays as the neutral default.

### Damage, healing, and death

**Damage lasts for the round.** A champion keeps its HP across the waves of a
round. Nothing heals between waves; the only healing during a round is what
healer champions do in combat. Damaged units function normally — HP is just HP.

**Living champions heal to full at the start of every prep phase**, for free.

**Death is permanent.** A champion that dies is gone for the run — not restored at
round start. HP is the round's resource; champions are the run's. Its pool copies
return to the pool, with no refund.

This replaces the earlier model, where damage persisted across the whole run and
between-wave healing was a paid action. What changes:

- A round is a multi-wave attrition test with a clean slate after it — except
  for the champions you lost.
- Healers are combat pieces, not economy pieces.
- Gold no longer has a healing sink, so "what absorbs gold late" loses a
  candidate (§4).
- Selling a damaged unit launders nothing — selling is prep-only, and in prep
  every living unit is already at full HP. The damaged-sell rule that the paid
  heal needed is gone with it.

### Repositioning

**Between rounds only.** Full repositioning in prep, like TFT — champions and
terrain, board and bench. Board to bench, bench to board, and bench to bench are
all free during prep. No repositioning during waves and none between waves.

### Gold collection

**Kill gold is the only income.** There is no flat per-wave or per-round payout:
each enemy carries a gold value, and the moment it dies that gold
**auto-collects** into the player's total. No mandatory clicking. The HUD shows a
`+N` at the gold counter for each kill, so the player watches income arrive. Gold
earned mid-round can't be spent until the round ends (shop and sell lock during
combat), so it builds up and is spent in prep. Interest, streaks and anything like them are separate
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

Falls naturally out of permanent death plus gold; no new systems needed. Nice
property: a player down to one unit and no gold can theoretically claw back if
that unit survives a round. Rare, but memorable.

**Not chosen:** gold acting directly as health. Spending all your gold to field
the strongest possible board should never be what kills you.

### Win condition / run structure

Effectively endless, with a soft "you win" ceiling most runs never reach.
Replayability comes from high scores — "I got to round 8 last time, can I hit
10?" The ceiling was set around wave 50 before rounds existed; restate it in
rounds once wave counts are tuned.

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

**HP carries across waves, so attributes need explicit handling.** GAS attributes
normally live and die with the actor, and most GAS material assumes a reset each
round. Here, HP persists across the waves of a round. Champions are never
destroyed and respawned between waves or rounds. The prep full heal is an
explicit effect applied to living champions, not a reset or respawn. Dead
champions are removed and not restored.

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
isn't a cliff. Small rosters soften this too.

Being overrun is also **spatial**: enemies that stop to fight occupy hexes, and
the ones behind them cannot pass, so a growing clump presses against your line
and, as front champions die, pushes deeper into your zone until enemies stop next
to your backline. Falling behind exposes the squishy things. Good failure mode —
don't design it out.

**Attrition.** Damage carries across the waves of a round, so a bad first wave
makes the second and third harder. Across rounds, the only carryover is death —
HP resets, losses don't.

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
is no flat per-wave or per-round income; kills are the income), reroll cost, unit
tier pricing, gold in vs gold out across a long run.

**What absorbs gold late?** Once the board is full and units are maxed, something
has to keep consuming income or the run becomes a formality until the wall.
Healing is free now, so it is no longer a candidate. Remaining candidates: terrain
rebuilds, replacing dead units.

Rough starting points, purely so there's something to push against — all of these
are wrong, they're just wrong in a specific enough way to learn from:

- Unit cost 1/2/3 by tier
- No flat income; kill gold only, about 1 gold per enemy to start
- 5 shop slots, 6 bench slots, reroll 2 gold, sell refunds full purchase price

(The "Selling damaged units" problem and its leading-candidate rule were dropped
on 2026-11-17: they existed because healing was a paid action and damage outlived
the round. With a free full heal every prep and selling locked to prep, there is
nothing to launder.)

### What a leak costs

**That it will cost something is decided. What, is not.** Candidates: gold loss
on leak, damage to a random unit on the way out, a score penalty. Do not build one
until it is chosen.

This matters more now: HP resets every round, so with leaks free, a bad round
costs only the champions that died in it. Leaks are the likely source of the
missing pressure.

The instrument comes first. Leaks per round and cumulative leaks against a
competent board early versus late tell you which lever fits and how hard it needs
to pull. Until a cost exists, leaking drains pressure off a wave for
free, so early difficulty readings are softer than the finished game should be —
read them with that in mind rather than tuning wave sizes up to compensate.

One punishment is already in place: kill gold is the only income, so an enemy that
leaks drops no gold and the player forfeits that enemy's share. It needs no code; it
falls out of the income model. It is a real cost, and **additional** to whatever is
chosen above — the chosen cost stacks on top, so size it knowing leaks already
hurt. The counter itself still costs nothing directly and the decision above stays
open, but the "leaking is free" caveat is softer than it reads.

### Wave split and spawn timing

How many enemies go in each wave, how many waves in each round (currently 3), and
the spawn delay range (`MinSpawnInterval` / `MaxSpawnInterval`, placeholder
1.0–2.0 s). Tune them together against board crossing time (3.5 s, measured in
Checkpoint 1). A per-wave override of the delay range is one field on the round
data if later rounds should trickle faster; not added until play asks for it.

### Spawn windows

The early/late progress thresholds (~10% / ~95%) are placeholders.

### Stuck waves

The next wave waits on the current one, so an enemy that can neither reach a
target nor the exit — or is boxed in behind other enemies — freezes the round.
Deferred until it is seen in play. Do not add a wave timer or safety net. If
needed, candidates include leftover enemies speeding up after a while, or leaving
through the exit.

### Enemy queued behind another enemy

An enemy walking to the exit whose next hex holds another enemy currently waits.
If queues look bad, try letting it take an equally short free alternative step —
a small change to the next-step choice.

### Melee champion locking

When several enemies are adjacent, does a melee champion lock onto one until it
dies, as ranged champions do? Locking is the default assumption.

### The death spiral

Conditional on leaks eventually costing something (see "What a leak costs"). If
they hurt the economy, a spiral is possible. This is probably **correct** for
an endless high-score game — it makes endings decisive instead of a slow bleed,
and gives the run a story ("round 6 went badly and it all came apart").

Requirement: the spiral must be **fast**. A round or two to resolution. If
playtesting shows someone limping along knowing they're dead, tighten it.

### Terrain regrowth specifics

Cost? Cooldown? Wave-based timer? Trait-dependent? Undecided.

### Items

Barely discussed. Unclear whether the game wants them at all.

---

## 5. Parked — good ideas, not now

Liked or at least interesting, but explicitly excluded from MVP to avoid feature
sprawl.

- **Augments.** TFT-style, three choices at fixed rounds. Cheapest way to make
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
  communicates "this unit is in trouble" fine for a prototype.
- **Leaks damaging the economy.** A leak costing gold, income, or a shop slot
  rather than a generic HP bar. This is the leading candidate for what a leak
  eventually does (§4), but it is a tuning problem wearing a design idea's
  clothes — it needs real income curves and real leak counts first.
- **Spawn variety beyond the per-level array.** Brainstormed 2026-11-17, not
  adopted yet: a pre-placed enemy vanguard at wave start with the rest streaming
  in behind it; spawn hexes that change per wave and are shown in prep; rifts
  that open mid-wave with a warning; enemy nests that spawn until destroyed.
  Worth revisiting if waves start to feel samey.
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
- **Enemies passing through each other (PvZ-style).** Small, distinct waves want
  order and formation to matter
- **Enemies walking through champions' hexes, or routing around them.** Replaced
  by "units stop": enemies fight through champions instead. Both the original
  "units don't block" rule and the 2026-09-20 flat occupied-hex cost are retired
- **Champions moving during combat.** Champions are stationary (§1)
- **Paid between-wave healing / run-long persistent damage.** Replaced by a free
  full heal each prep and permanent death; the damaged-sell rule went with them
- **Occupancy-only spawning and per-hex spawn timers.** Replaced by one
  randomized timer per wave
- **Flat-top hexes.** Considered; pointy-top kept

---

## 6. MVP scope

Deliberately minimal. Everything not on this list is a later problem.

- 2 traits
- 2–3 champions, stationary, with TFT-style targeting
- A few enemy types, including at least one assassin and one terrain-breaker.
  Enemy range 1 or 2 only; no ranged archetype
- One terrain type (trees)
- A shop, with a bench that bought champions land on
- A basic economy
- Rounds of three waves with a Start button; prep between rounds only
- Damage that lasts the round, full heal between rounds, permanent death
- Wave spawning on a randomized timer from free spawn hexes, with spawn windows
  and scoutable comps
- Lose condition (checkmate)
- An exit behind the back row with a leak counter that does nothing yet
- Endless rounds

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
tuned against that one — including how many enemies go in a wave.

**Step 2 — One unit that shoots it.**
Now there's a game loop. Ugly, but real.

**Step 3 — Blockers.**
Melee unit stands on a hex, enemy walks into it and fights instead of passing.
Ordinary enemies only — assassins need mana and therefore GAS, so they arrive
after step 2 at the earliest, and realistically alongside the first real
abilities. Where the aggro model gets its real answer, and where you find out
whether enemies crowd a blocker gracefully or pile up stupidly.

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

**Route planning ignores units.** Only terrain is impassable in the search. Units
block at the moment of stepping, through the occupancy check. That split is the
code-level form of "terrain redirects, units stop".

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
Champions are hard stops and enemies cannot pass each other, so a single unit in
a lane holds the enemy fighting it and everything queued behind. Step 3 answers
whether that's a satisfying time-buying strategy or a degenerate one. With no
ranged enemy archetype, a wall of cheap tanks is the specific version to watch;
the unit cap is the first dial.

**How fast should a wave enter against board crossing time?**
Wave size and the spawn delay range decide whether a wave is one big fight or a
stream with a tail. Crossing time was measured in Checkpoint 1 (3.5 s); both are
set against it.

**Do leaks need to cost anything?**
The counter is instrumented and inert. With HP resetting every round, leaks are
the likeliest source of round-to-round pressure — but if a competent board leaks
nothing for many rounds, leak damage isn't the difficulty lever and the exit
stays a despawn.

Everything after that is a playtest question, not a design-document question.
