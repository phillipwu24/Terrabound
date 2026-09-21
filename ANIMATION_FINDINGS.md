# ANIMATION_FINDINGS.md

Read-only investigation of the four Paragon Animation Blueprints (Grux, Kwang, LtBelica, Serath) via the Unreal MCP.
Facts only; no design. Investigated 2026-09-20.

## Method and limits

- Tools used: `AssetTools` (load/find/tags/exists/is_dirty), `ObjectTools` (list/get properties), `BlueprintTools` (`list_graphs`, `read_graph_dsl`, `find_nodes`, `get_node_infos`, `get_graph`, `list_variables`), `LogsToolset`.
- Not called: `compile_blueprint`, `save_assets`, `set_properties`, `move`, `delete`, `duplicate`, or any write tool.
- **Readable:** EventGraph (as DSL), every AnimGraph node and its pin wiring, transition *rule* graphs, state *content* graphs, node properties (slot names, sync groups, asset paths, layer setup), montage slot tracks, sequence length and root-motion flags, blend space axes and samples, asset-registry tags (notify *names*).
- **NOT READABLE via MCP:**
  - Which state connects to which (transition from/to pairs), the entry state, and conduits. `find_nodes` returns nothing on state-machine graphs, `get_node_infos` raises on `AnimStateNode`/`AnimStateTransitionNode`, and `BakedStateMachines` and graph `Nodes` are not exposed.
  - Notify trigger times and montage sections (`notifies`, `compositeSections` not exposed). Only notify *names* are readable, from the `AnimNotifyList` asset tag.
  - Compile status. Reading it would need `compile_blueprint`, which I did not run.
- Where I cite engine behaviour rather than something read through the MCP, it is marked **(engine semantics, not read)**.
- Asset-name patterns (`Death`, `Primary`, ...) were used only to *find candidates*. Everything reported about them (length, slot, root motion) was read from the assets.

## Read-only status

- Nothing was compiled, saved, renamed or moved. On disk, the four ABP `.uasset` mtimes are unchanged (Sep 11–14). `git status` shows only the pre-existing `MainBoard.umap`, `DESIGN.md` and `PLAN.md` changes.
- **In memory, all four ABPs now report `is_dirty = true`.** Grux went dirty during the first graph reads. The other three were `false` right after load and `true` after node inspection. Reading anim-graph nodes through the MCP appears to dirty the package. **Do not "Save All" in the editor before restarting or discarding.** Nothing has been written, but a save would write whatever the editor now holds.

---

## Shared structure (verified in all four ABPs; deltas listed per hero)

All four ABPs derive from `AnimInstance`. Class names: `Grux_AnimBlueprint_C`, `Kwang_AnimBlueprint_C`, `LtBelica_AnimBlueprint_C`, `Serath_AnimBlueprint_C`. All have `rootMotionMode = RootMotionFromMontagesOnly`, `bUseMainInstanceMontageEvaluationData = false`, and both linked-instance notify flags false.

### 1. Slot nodes

**Exactly one Slot node in every ABP: slot name `UpperBody`** (`AnimGraphNode_Slot_23`, top-level AnimGraph). There are no Slot nodes inside any state or transition graph. **There is no `DefaultSlot` node.**

### AnimGraph topology (all four)

```
Locomotion SM ──► AimOffset player ──► SaveCachedPose 'LocoPose'
GroundLocomotion SM ─► SaveCachedPose 'Ground_Loco'      (used inside the Locomotion SM state "Idle/Jogs")
UseCachedPose 'LocoPose' ─► Slot 'UpperBody' ─► SaveCachedPose 'CachedPose_UpperBody'
LayeredBlendPerBone( base = LocoPose, layer0 = CachedPose_UpperBody, weight 1.0, BranchFilter )
BlendPosesByBool( BlendPose_0 = LayeredBlend, BlendPose_1 = CachedPose_UpperBody,
                  BlendTime 0.2/0.2, bActiveValue = IsAccelerating AND NOT FullBody )
  ─► SaveCachedPose 'FullBody' ─► LocalToComponent ─► LegIK ─► ComponentToLocal ─► Output
```

- BlendPosesByBool: with `bActiveValue` true, the engine uses `BlendPose_0` (the layered blend, where the slot only affects the upper body). With false it uses `BlendPose_1` (the slot output over the whole body) **(engine semantics, not read)**.
- Layered blend branch filters (per hero below): `thigh_r` and `thigh_l` have blendDepth `-1` in every ABP. The upper-body root is `pelvis` (Grux depth 4, Kwang 3, Serath 4) or `spine_01` (Belica, depth 3).
- No Linked Anim Layer, Linked Anim Graph, sub-instance or inertialization nodes in any AnimGraph or state graph. No conduit graphs exist in any of the four graph lists.

### Variables (all four; Belica adds `PistolHolster`)

`Speed`, `IsInAir`, `Pitch`, `Roll`, `Yaw`, `RotationLastTick`, `YawDelta`, `IsAccelerating`, `Character`, `isAttacking`, `CurrentAttack`, `FullBody`. All defaults are 0/false (read from the CDO).

| Variable | What reads it (from the graphs) |
|---|---|
| `IsAccelerating` | BlendPosesByBool (with `NOT FullBody`); Ground Locomotion transition rules |
| `FullBody` | BlendPosesByBool |
| `Speed` | Ground Locomotion transition rules (`>0`, `==0`, Belica also `>100`); Grux only: LegIK Alpha (`Speed > 0`) |
| `IsInAir` | Locomotion SM transition rules; Ground Locomotion transition 1 (`NOT IsInAir`) |
| `YawDelta` | Run-state blend space X input (× 0.0 Grux, × −1.0 others) |
| `Pitch`, `Yaw` | AimOffset X/Y |
| `Roll`, `Character`, `isAttacking`, `CurrentAttack`, `RotationLastTick` | no read seen in AnimGraph, state or transition graphs (`RotationLastTick` feeds only the `YawDelta` calculation) |

### 2. EventGraph (identical pattern in all four)

- `EventBlueprintBeginPlay` → `Montage_Play(<Hero>/Animations/LevelStart_Montage, rate 1.0, ReturnValueType MontageLength, startPos 0.0, bStopAllMontages = true)`. **This is what plays the spawn animation.**
- `EventBlueprintInitializeAnimation` → `Cast to <Hero>PlayerCharacter (TryGetPawnOwner)` → `BindEvent to Attacking` → custom event sets `isAttacking = true`.
- `EventBlueprintUpdateAnimation`: reads `TryGetPawnOwner`, then `GetActorRotation`, `GetBaseAimRotation`, `GetMovementComponent → IsFalling`, `GetVelocity` (length), `Cast to Character → CharacterMovement → GetCurrentAcceleration` (length `> 0`), and curve `FullBody > 0`. **Everything after the pawn fetch is inside `IsValid(pawn)`.** Belica also reads curve `Ult_Pistol_ON`.
- `AnimNotify_SaveAttack` and `AnimNotify_ResetCombo`: cast the pawn owner to the hero's `PlayerCharacter`, then call `ComboAttackSave` / `ResetCombo`. (The DSL labels the function class `LtBelicaPlayerCharacter` in all four ABPs; not investigated.)

**Owner is NOT a pawn:** `TryGetPawnOwner` returns null for a non-`APawn` owner **(engine semantics, not read)**. Consequences read from the graph:

- The whole `IsValid` branch is skipped every frame, so `Speed`, `IsInAir`, `IsAccelerating`, `Pitch`/`Yaw`/`Roll`, `YawDelta`, `FullBody` (and Belica's `PistolHolster`) stay at their defaults (0/false). Nothing in the ABP overwrites them.
- The state machines therefore stay on their idle path and it **falls back to idle rather than breaking**. This matches the observed idle + LevelStart today.
- The Init-time cast fails and `BindEvent` is called on a null target. The `SaveAttack`/`ResetCombo` casts fail and the chain ends (no else-branch wired). Whether the null bind logs an "Accessed None" warning at runtime: NOT READABLE via MCP.
- `FullBody` false and `IsAccelerating` false means `bActiveValue` is false. Per the engine convention above, the pose is `CachedPose_UpperBody`, so a montage on slot `UpperBody` drives the **whole body**. That is consistent with LevelStart playing full-body today.

### 3. State machines (names as read; from→to wiring NOT READABLE via MCP)

- **`Locomotion`**: states `Idle/Jogs` (outputs cached `Ground_Loco`), `Jumps` (contains sub-machine `Jumps`: `JumpStart`, `JumpApex`, `Jump_PreLand`), and `Jump_Land` (`Ground_Loco` + additive land pose). Transition rule graphs: `IsInAir`, `IsInAir`, `NOT IsInAir`, constant `false`. The Jumps sub-machine has two transitions, both constant `false`.
- **`Ground Locomotion`**: states `Idle`, `JogStart`, `Run`, `JogStop` (per-hero clips below).
- **Spawn/LevelStart is not in any state machine.** It is a montage started from EventGraph BeginPlay. It returns to idle by the montage ending (`bEnableAutoBlendOut = true`, blend-out 0.25 s), after which the slot passes through the locomotion pose.

### 4. What starts a walk/jog

Which transition joins which states is NOT READABLE via MCP. What the transition rule graphs read (per hero below) is only `IsAccelerating`, `Speed` and `IsInAir`. The Run state's blend space is fed only by `YawDelta` (X) and a constant 0 (Y), so the clip it plays does not depend on `Speed` (see item 8).

### 5. Sync groups and slot conflicts

- Sync group **`Jog`** (method `SyncGroup`, role `CanBeLeader`) is used by the Run blend space player, `JogStart` and `JogStop` (Belica: Run and `JogStart` only; her `JogStop` is `DoNotSync`). Idle, all jump players and the aim-offset player are `DoNotSync`.
- Every montage read has `syncGroup = None`, so montages do not join `Jog`.
- Things a montage on slot `UpperBody` sits next to:
  - The slot is upstream of the LayeredBlendPerBone and BlendPosesByBool above.
  - `LegIK` is downstream of the montage pose.
  - The ABP starts its own `LevelStart_Montage` on `UpperBody` at BeginPlay (0.25 s blend-in, ~5 s long).
  - The skeleton's slot-group mapping is NOT READABLE via MCP (`Skeleton` exposes no slot-group property).

### 6. Compile status

NOT READABLE via MCP (needs `compile_blueprint`, not run). Evidence available: each generated class (`*_AnimBlueprint_C`) loads and exposes its variables. The session log holds no compile messages for any of the four ABPs. The only entries mentioning them are warnings from my own unreadable-property probes. `is_dirty` = true for all four in memory (see Read-only status).

---

## Per-hero detail

### Grux — `/Game/ParagonGrux/Characters/Heroes/Grux/Grux_AnimBlueprint`

**ABP items 1–6 (deltas from shared)**
- Layered blend: `pelvis` depth 4, `thigh_r`/`thigh_l` −1. AimOffset `Idle_Base_AO_BS`, Alpha 1.0.
- **LegIK Alpha = (`Speed > 0`) as float** (the others use constant 1.0).
- Ground Locomotion states: `Idle` = `Idle` (loop, no sync), `JogStart` = `Run_Fwd_Start`, `Run` = BlendSpace `BaseHero_Locomotion_BS`, `JogStop` = `Run_Fwd_Stop` (all three in group `Jog`).
- Ground Locomotion transition rules: T0 `IsAccelerating`; T1 `Speed>0 AND NOT IsInAir AND IsAccelerating`; T2 `Speed==0`; T6 `NOT IsAccelerating`; T7 `false`; T8 `IsAccelerating`.
- Jumps: `Jump_Start`, `JumpApex`, `Jump_Land` (play rate 0.1). `Jump_Land` state = `Ground_Loco` + `Jump_Land_Additive` (α 0.4, rate 1.2).
- Run state blend space inputs: X = `YawDelta × 0.0`, Y = 0.0.

**7. Primary-attack montages** (all `/Game/ParagonGrux/Characters/Heroes/Grux/Animations/`; slot `UpperBody`; blend-in 0.10 s, blend-out 0.25 s; `AnimNotifyList` = `SaveAttack; ResetCombo`)

| Montage | Length (s) | Segment sequence |
|---|---|---|
| `PrimaryAttack_LA_Montage` | 1.133 | `PrimaryAttack_LA` |
| `PrimaryAttack_RA_Montage` | 1.133 | `PrimaryAttack_RA` |
| `PrimaryAttack_LA_Fast_Montage` | 0.633 | `PrimaryAttack_LA_Fast` |
| `PrimaryAttack_RA_Fast_Montage` | 0.633 | `PrimaryAttack_RA_Fast` |
| `PrimaryAttack_FourStrikes_Montage` | 3.700 | `PrimaryAttack_FourStrikes` |

Impact/hit notify: **none exists by name.** The montage notify list holds only `SaveAttack` and `ResetCombo`. The underlying sequences' notify lists are empty. Notify times: NOT READABLE via MCP.

**8. Walk clip:** the Run blend space `Blendspaces/BaseHero_Locomotion_BS` has axes Direction (−180..180) × Speed (150..550). Inputs (0, 0) clamp to (0, 150), where the sample is **`Animations/Jog_Fwd`, 1.533 s, in-place** (`bEnableRootMotion` false, `bLoop` false on the asset; the loop comes from the player node's `bLoop = true`). `Run_Fwd` (0, 550) is in the blend space but not reachable at these fixed inputs. Start/stop clips: `Run_Fwd_Start` 1.033 s, `Run_Fwd_Stop` 0.533 s (in-place).

**9. Death sequences** (no death montage exists): `Animations/Death_A` 1.667 s, `Animations/Death_B` 1.633 s. Root motion off, `bLoop` false, no notifies.

**10. LevelStart:** `Animations/LevelStart_Montage`, 5.000 s, slot `UpperBody`, blend 0.25/0.25 s. Segment sequence `LevelStart` 5.000 s, root motion off.

### Kwang — `/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/Kwang_AnimBlueprint`

**ABP items 1–6 (deltas)**
- Layered blend: `pelvis` depth 3. AimOffset `Idle_AO`, Alpha 1.0. LegIK Alpha constant 1.0.
- Ground Locomotion: `Idle` = `Idle`; `JogStart` = `Jog_Fwd_Start`; `Run` = BlendSpace `JogFwdSlopeLean`; `JogStop` = `Jog_Fwd_Stop` (all three in group `Jog`).
- Rules: T0 `IsAccelerating`; T1 `Speed>0 AND NOT IsInAir AND IsAccelerating`; T6 `NOT IsAccelerating`; T7 `false`; T8 `false`. (No `Speed==0` rule graph.)
- Jumps: `Jump_Start`, `Jump_Apex`, `Jump_Land` (rate 0.2). Land additive `Jump_Recovery_Additive` (α 0.4, rate 1.2).
- Run blend space inputs: X = `YawDelta × −1.0`, Y = 0.0.

**7. Primary-attack montages** (all `/Game/ParagonKwang/Characters/Heroes/Kwang/Animations/`; slot `UpperBody`; blend-out 0.25 s; notifies `SaveAttack; ResetCombo`)

| Montage | Length (s) | Blend-in (s) | Segment sequence |
|---|---|---|---|
| `PrimaryAttack_A_Slow_Montage` | 1.200 | 0.15 | `PrimaryAttack_A_Slow` |
| `PrimaryAttack_B_Slow_Montage` | 1.267 | 0.10 | `PrimaryAttack_B_Slow` |
| `PrimaryAttack_C_Slow_Montage` | 1.267 | 0.10 | `PrimaryAttack_C_Slow` |
| `PrimaryAttack_D_Slow_Montage` | 1.467 | 0.10 | `PrimaryAttack_D_Slow` |

Impact/hit notify: none by name (only `SaveAttack`, `ResetCombo`; sequences have none). Times NOT READABLE via MCP.

**8. Walk clip:** `Blendspaces/JogFwdSlopeLean`, axes Lean (−45..45) × SlopeAngle (−25..25). Inputs (0, 0) hit the sample **`Animations/Jog_Fwd`, 1.767 s, in-place** (root motion off, `bLoop` false on the asset, has `LeftPlant`/`RightPlant` sync markers). `Jog_Fwd_Start` 2.467 s, `Jog_Fwd_Stop` 4.767 s (in-place).

**9. Death sequences** (no death montage): `Animations/Death_Bwd` 2.167 s (the only death clip). Root motion off.

**10. LevelStart:** `Animations/LevelStart_Montage`, 4.967 s, slot `UpperBody`, blend 0.25/0.25 s. Segment sequence `LevelStart` 4.967 s, root motion off.

### LtBelica — `/Game/ParagonLtBelica/Characters/Heroes/Belica/LtBelica_AnimBlueprint`

**ABP items 1–6 (deltas)**
- Layered blend: `spine_01` depth 3. **AimOffset `Idle_AimOffset` Alpha = 0.0.** LegIK Alpha constant 1.0.
- **Extra variable `PistolHolster`.** It is set from curve `Ult_Pistol_ON` (`× −1 + 1`) inside the `IsValid` branch, so it stays at its default 0 for a non-pawn owner.
- **Extra nodes after LegIK:** `CopyBone pistol_holster→pistol` (Alpha = `PistolHolster`), `CopyBone thigh_r→weapon` (Alpha = curve `WeaponThigh`), and `ModifyBone weapon`, then `ComponentToLocal`.
- Ground Locomotion: `Idle` = `Idle_Relaxed`; `JogStart` = `Jog_Fwd_Start` (group `Jog`); `Run` = BlendSpace `JogFwdSlopeLean` (group `Jog`); `JogStop` = `Jog_Fwd_Stop` (**no sync group**).
- Rules: T0 `IsAccelerating`; T1 `Speed>0 AND NOT IsInAir AND IsAccelerating`; T2 `Speed==0`; **T5 `Speed>100`**; T6 `NOT IsAccelerating`; **T7 `true` (constant)**; T8 `false`.
- Jumps: `Jump_Start`, `Jump_Apex`, `Jump_PreLand` (rate 0.2). Land additive `Jump_Recovery_Additive` (α 0.6, rate 1.2).
- Run blend space inputs: X = `YawDelta × −1.0`, Y = 0.0.

**7. Primary-attack montage** (`/Game/ParagonLtBelica/Characters/Heroes/Belica/Animations/`; slot `UpperBody`; notifies `SaveAttack; ResetCombo`)

| Montage | Length (s) | Blend in/out (s) | Segment sequence |
|---|---|---|---|
| `Primary_Fire_Med_Montage` | 1.000 | 0.0 / 0.25 | `Primary_Fire_Med` |

Also present: `R_Ability_Montage` (3.400 s, `UpperBody`, same notifies; an ability, not a primary attack). Sequences `Primary_Fire_Fast`, `Primary_Fire_Slow` (and `_MSA` variants) exist with no montage; I did not inspect them. Impact/hit notify: none by name. Times NOT READABLE via MCP.

**8. Walk clip:** `Blendspaces/JogFwdSlopeLean`, axes Lean (−45..45) × SlopeForwardAngle (−25..25). Inputs (0, 0) hit **`Animations/Jog_Fwd`, 1.333 s, in-place** (root motion off, `bLoop` false on the asset; sync markers present). `Jog_Fwd_Start` 3.433 s, `Jog_Fwd_Stop` 3.333 s (in-place).

**9. Death sequences** (no death montage): `Animations/Death_A` 1.933 s, `Animations/Death_B` 1.667 s. Root motion off.

**10. LevelStart:** `Animations/LevelStart_Montage`, 4.867 s, slot `UpperBody`, blend 0.25/0.25 s. The segment is `LevelStart` trimmed to 4.867 s of its 5.000 s.

### Serath — `/Game/ParagonSerath/Characters/Heroes/Serath/Serath_AnimBlueprint`

**ABP items 1–6 (deltas)**
- Layered blend: `pelvis` depth 4. AimOffset `Idle_AO_Blendspace`, Alpha 1.0. LegIK Alpha constant 1.0.
- Ground Locomotion: `Idle` = `Idle`; `JogStart` = `Jog_Fwd_Start`; `Run` = BlendSpace `JogFwdSlopeLean`; `JogStop` = `Jog_Fwd_Stop` (all three in group `Jog`).
- Rules: T0 `IsAccelerating`; T1 `Speed>0 AND NOT IsInAir AND IsAccelerating`; T2 `Speed==0`; T6 `NOT IsAccelerating`; T7 `false`; T8 `false`.
- Jumps: `Jump_Start`, `Jump_Apex`, `Jump_Land` (rate 0.1). Land additive `Jump_Recovery_Additive` (α 0.4, rate 1.2).
- Run blend space inputs: X = `YawDelta × −1.0`, Y = 0.0.

**7. Primary-attack montages** (all `/Game/ParagonSerath/Characters/Heroes/Serath/Animations/`; slot `UpperBody`; blend-in 0.10 s; notifies `SaveAttack; ResetCombo`)

| Montage | Length (s) | Montage RateScale | Blend-out (s) | Segment sequence |
|---|---|---|---|---|
| `Primary_Attack_A_Medium_Montage` | 1.400 | **2.0** | 0.25 | `Primary_Attack_A_Fast` |
| `Primary_Attack_B_Medium_Montage` | 1.400 | **2.0** | 0.25 | `Primary_Attack_B_Fast` |
| `Primary_Attack_C_Medium_120fps_Montage` | 1.033 | 1.0 | 0.10 | `Primary_Attack_C_Medium_120fps` (120 fps source) |
| `Primary_Attack_D_Medium_Montage` | 1.033 | 1.0 | 0.25 | `Primary_Attack_D_Medium` |

Impact/hit notify: none by name. Times NOT READABLE via MCP. With RateScale 2.0 the A and B montages play in about 0.70 s at play rate 1 (arithmetic from the values above, not read).

**8. Walk clip:** `Blendspaces/JogFwdSlopeLean`, axes LeanAngle (−45..45) × SlopeAngle (−20..20). Inputs (0, 0) hit **`Animations/Jog_Fwd`, 2.000 s, in-place** (root motion off, `bLoop` false on the asset; sync markers present). `Jog_Fwd_Start` 1.867 s, `Jog_Fwd_Stop` 5.100 s (in-place).

**9. Death sequences** (no death montage): `Animations/Death_Fwd` 1.567 s (the only death clip). Root motion off.

**10. LevelStart:** `Animations/LevelStart_Montage`, 5.000 s, slot `UpperBody`, blend 0.25/0.25 s. Its segment sequence is named **`Level_Start`** (no asset named `LevelStart` exists).

---

## Summary table

| Hero | 1 Slots | 2 Non-pawn owner | 3 State machines / spawn | 4 Walk gate | 5 Sync / conflicts | 6 Compile | 7 Primary attack montage(s), slot, impact notify | 8 Walk clip | 9 Death | 10 LevelStart |
|---|---|---|---|---|---|---|---|---|---|---|
| Grux | `UpperBody` only | vars stay 0/false → idle path, no break | Locomotion + Ground Locomotion; spawn = ABP BeginPlay `Montage_Play` | `IsAccelerating`, `Speed`, `IsInAir` (wiring unreadable) | group `Jog` (Run/Start/Stop); LegIK gated by `Speed>0` | NOT READABLE; dirty in memory | LA 1.133, RA 1.133, LA_Fast 0.633, RA_Fast 0.633, FourStrikes 3.700; `UpperBody`; no impact notify (times unreadable) | `Jog_Fwd` 1.533 s, in-place | `Death_A` 1.667 s, `Death_B` 1.633 s, root motion off | 5.000 s, `UpperBody` |
| Kwang | `UpperBody` only | same | same; T8 `false`, no `Speed==0` rule | same | group `Jog`; LegIK α 1.0 | NOT READABLE; dirty | A 1.200, B 1.267, C 1.267, D 1.467; `UpperBody`; no impact notify | `Jog_Fwd` 1.767 s, in-place | `Death_Bwd` 2.167 s, root motion off | 4.967 s, `UpperBody` |
| Belica | `UpperBody` only | same; `PistolHolster` stays 0 | same; adds CopyBone/ModifyBone after LegIK | same, plus `Speed>100` rule and a constant-`true` rule | group `Jog` (Run/Start only) | NOT READABLE; dirty | `Primary_Fire_Med` 1.000; `UpperBody`; no impact notify | `Jog_Fwd` 1.333 s, in-place | `Death_A` 1.933 s, `Death_B` 1.667 s, root motion off | 4.867 s, `UpperBody` |
| Serath | `UpperBody` only | same | same | same | group `Jog` | NOT READABLE; dirty | A 1.400 (rate 2.0), B 1.400 (rate 2.0), C 1.033, D 1.033; `UpperBody`; no impact notify | `Jog_Fwd` 2.000 s, in-place | `Death_Fwd` 1.567 s, root motion off | 5.000 s, `UpperBody` (sequence `Level_Start`) |

## Anything surprising

1. **Only one slot exists: `UpperBody`.** There is no `DefaultSlot` node in any ABP. Every montage in these packs uses `UpperBody`. A montage on `DefaultSlot` (which I believe is the engine default for dynamic slot playback, not read via MCP) would have no node to render it.
2. **The spawn animation is not state-machine driven.** The ABP starts `LevelStart_Montage` itself at BeginPlay with `bStopAllMontages = true`. It plays for about 5 s and only blends out when the montage ends.
3. **The ABP update logic never writes anything for a non-pawn owner.** Because the whole update branch sits inside `IsValid(pawn owner)`, `Speed`, `IsAccelerating`, `IsInAir`, `FullBody` and the rest stay at their defaults and nothing in the ABP would overwrite an externally-set value.
4. **The Run state is pinned to `Jog_Fwd`.** The blend space inputs are `YawDelta × constant` and a constant 0, so `Speed` never changes which clip plays. Grux's `Run_Fwd` sample (Speed 550) is unreachable at these inputs.
5. **`Jog_Fwd` and `Idle` have `bLoop = false` at the asset level.** Looping comes from the ABP player nodes (`bLoop`/`bLoopAnimation = true`).
6. **A montage's reach depends on `IsAccelerating`.** When it is false (always, for a non-pawn), the slot drives the whole body. When it is true and `FullBody` is 0, only bones from `pelvis`/`spine_01` upward are affected (thighs excluded).
7. **No hit/impact notify exists in any attack montage or its sequences.** The only notify names are `SaveAttack` and `ResetCombo`, and both call back into the hero `PlayerCharacter` via a cast that fails for a non-pawn actor.
8. **There are no death montages.** Deaths are bare `AnimSequence`s (Kwang and Serath have exactly one each).
9. **No root motion anywhere.** Every sequence read (idle, jog, start/stop, attack, death, LevelStart) has `bEnableRootMotion = false`.
10. **Start/stop clips are long for three heroes.** `Jog_Fwd_Start` and `Jog_Fwd_Stop` are 1.9–5.1 s for Kwang, Belica and Serath. This matters if the Ground Locomotion machine ever enters `JogStart`/`JogStop`.
11. **Serath's "Medium" montages use "Fast" sequences at montage RateScale 2.0.** Her LevelStart segment is a sequence named `Level_Start`.
12. **Belica differs most.** Aim offset Alpha is 0, she has a `PistolHolster` variable, extra bone-copy/modify nodes after LegIK, her `JogStop` is outside sync group `Jog`, and she has a `Speed>100` and a constant-`true` transition rule. She has one primary-attack montage, and it has 0.0 s blend-in.
13. **Grux is the only hero whose LegIK Alpha depends on `Speed`.**
14. **All four ABPs are dirty in memory after being read** (see Read-only status).
