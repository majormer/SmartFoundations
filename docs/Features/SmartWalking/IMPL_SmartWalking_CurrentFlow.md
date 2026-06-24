# Smart Walking (#356) — Current Flow & Architecture

> Durable technical reference for the **shipped** Smart Walking feature (33.0.0). Harvested from the sprint
> `CONCEPT_/PLAN_/DESIGN_` docs (deleted on release). Player-facing how-to lives in the GitHub wiki; this is the
> implementation map + rationale + deferred roadmap a future contributor needs.

## What it is
A progressive, **per-segment** build mode. Where Scaling stamps a rigid uniform grid and Extend copies a module
along a straight offset, Smart Walking lays a **connected run** forward one segment at a time, and each segment
carries its own turn / rise / shift / spacing — so one continuous run rounds corners, changes grade, and routes to
a destination. **Shipped scope:** stackable conveyor poles (belts) + stackable pipeline supports (pipes).

## Architecture — layer, don't rewrite
The walk core is **conveyance-agnostic** and sits BESIDE the grid path, never inside `SFPositionCalculator`.

- **`USFWalkService`** (`Private/Features/Walk/`) — owns the **segment list (source of truth)** + the origin frame.
  Forward kinematics (`AccumulateFrame`/`FrameAtIndex`) derive every world frame from the per-segment deltas.
  `EnterWalk`/`ExitWalk`, `CommitActiveAndAdvance` (commit-on-Scale-X), `BackUp` (destructive), `RepositionFrom(i)`
  (re-derive frames i..N — the back-end for live steering and future committed-segment edit). Walk holograms are
  **STANDALONE** (`SpawnHologramFromRecipe`, world-anchored, NOT grid children), so they never fight the grid
  reposition / auto-connect sweep, and vanilla child-cost aggregation can't see them (see cost, below).
- **`USFWalkConveyance`** (the `ISFWalkConveyance` seam) — **frames-not-splines**: given `(fromFrame, toFrame,
  heading)` an adapter emits the segment's spanning element. `USFWalkBeltConveyance` + `USFWalkPipeConveyance` ship.
  `LinkOrUpdate(existing, from, to, parent, bAddChildForBuild, turn)` creates/re-routes the span between two poles'
  connectors. The seam is frames-not-splines specifically so a future **discrete-junction** adapter (train track)
  fits without surgery on the walk loop.
- **Conveyance config is PATH-LEVEL** — one tier + one routing (+ pipe style / belt direction) for the whole run.
  Mixing tiers on a continuous belt pins throughput to the slowest, so there is no per-segment conveyance and no
  mid-walk distributors (players drop mergers/splitters on the finished run vanilla-style).

## Data model
- **`FSFWalkSegment`** (runtime, `SFWalkTypes.h`) — four authored adjusters: **Advance** (Spacing), **TurnDegrees**
  (Rotation), **Rise** (Steps), **Shift** (Stagger) + `NumLanes`/`NumStacks` (the bus cross-section) + transient
  preview holograms. Heading and world position are **DERIVED**, never authored (at most a compass readout).
- **`FSFWalkCommitSpec`** (`Public/Features/Walk/SFWalkCommitSpec.h`) — the **parameters-only** wire spec the client
  ships on fire: `OriginFrame`, the per-segment deltas (`FSFWalkCommitSegment`), `ConveyanceType` (Belt/Pipe), the
  consumed AC settings (`BeltRoutingMode`, `BeltTier`, `PipeRoutingMode`, `PipeTier`, `bPipeIndicator`,
  `BeltDirection`), the seed `BuildClass`, the summed `Cost`, `bValid`.
  > **MP-SPEC COMPLETENESS RULE:** any client-chosen auto-connect setting the walk **consumes** must travel in this
  > spec **and** be reinstalled in `ReconstructWalkCommitOnServer` — else a dedicated server falls back to its own
  > runtime default. (`BeltDirection` was exactly this gap, found + fixed in 33.0.0.)

## Multiplayer commit path (server-authoritative; mirrors Extend)
The walk session has **zero** MP surface until the fire. The client previews STANDALONE holograms (discarded on
commit); on fire it ships `FSFWalkCommitSpec` via `USFRCO::Server_StageWalkCommit` (Server/Reliable/WithValidation);
the **server** reconstructs under `HasAuthority()` in the `AFGBuildableHologram::Construct` hook
(`SFGameInstanceModule_SpecHooks.cpp`) via `USFWalkService::ReconstructWalkCommitOnServer`, gated by the
`sf.MP.SpecConstruction` CVar. **Spec-path-only**: SP runs the same RPC, locally short-circuited — no dual-route.
- **Spec precedence (the 33.0.0 dedi root cause):** a stackable pole *also* stages a 1×1 scaling spec on a network
  client, so the Construct hook consumes **all** staged specs and a committed **walk wins** over the incidental
  scaling/Extend spec (otherwise the hook short-circuited on scaling and only the seed built on a dedi).
- **Auto-tier** resolves client-side in `BuildCommitSpec` (the server has no local `PlayerController`; server-side
  `Auto` resolution falls back to Mk1).
- **Cost:** Smart's affordability quote is client-side (`CanAffordWalk`, pawn inventory + central storage). The
  build-gun overlay is synthesized in the `GetCost` hook (Hook A) on **both** authority **and** client, because walk
  previews are standalone and invisible to vanilla child-cost aggregation.
- **#334-safe:** no client-only actors are spawned; previews are torn down on commit **and** cancel.

## Interaction
- **In-world controls, no input capture:** Scale-X = advance / back-up; Rotation / Spacing / Steps / Stagger apply to
  the **ACTIVE segment only**. The player stays free to move (or steer remotely via the SmartCamera PiP).
- **K toggles the Smart Walking panel** (`USFWalkPanelWidget`) — a segment table (editable Advance/Turn/Rise/Shift +
  compass exit-heading), tier / direction / routing / pipe-style dropdowns, and a `‹ Scaling` return. Panel controls
  call `SetAutoConnect*TierMain` etc. and `RecreateSpans`.
- **HUD:** a compact walk badge (segment count + compass heading + invalid-shape reason).
- **AC-settings while walking:** the build-HUD `U`-cycle is gated to skip the **moot enable toggle** (the run always
  lays its own conveyance) — `CycleAutoConnectSetting` post-pass + `OnRecipeModeChanged` entry, when
  `WalkService->IsActive()`. The stackable cycles already exclude to-building/chain/power, so only the toggle needed
  gating. Settings a walk reads: belt = {Tier, Routing, Direction}; pipe = {Tier, Routing, Style} — exactly the panel.

## Conveyance routing (`SFWalkConveyance`)
- **Through-routing:** each span exits the source pole along the **previous segment's heading** (`-(source facing)`)
  and arrives at the destination **along its heading** (`+(dest facing)`) — exit-opposite-entry at every shared pole,
  for any turn up to ~270°. Do **not** snap the end normal to the chord (chord ≠ heading on a turn → fold/X-cross).
- **Belt direction (Backward):** `Swap(From,To)` reverses the spline/flow, **and the exit normals must be NEGATED**
  too — otherwise the forward "leave along +heading" rule, now read from the downstream pole, points the exit away
  from the destination and the belt curls back ("returns into the back"). Pipes are bidirectional (no direction).
- **Two preview-refresh methods:** `RerouteSpans` (geometry/routing — light) vs `RecreateSpans` (destroy+recreate for
  a tier/style **CLASS** change). A tier or pipe-style change MUST use `RecreateSpans`.
- **Shape validity:** a span > 56 m, or (belts only) climbing > 30°, is invalid — reds the segment, blocks commit
  (`BuildCommitSpec.bValid`), and the HUD badge reports the reason.

## Camera
The SmartCamera PiP latches the path **FRONTIER** (active segment exit frame), **not** furthest-by-coordinate (which
breaks on a looping path). The same exit frame feeds the panel's compass column, so they can't disagree. SmartCamera
remains the standalone companion mod (fold-back deferred).

## Deferred roadmap (parked, captured)
- **More adapters:** hypertube (**prereq: hypertube auto-connect, #405**), train track + vehicle path (discrete
  junctions — the frames-not-splines seam already accommodates them), and Extend-along-a-path convergence.
- **Committed-segment editing UI** — the `RepositionFrom` math is there; the selection / multi-select / mid-path
  re-route UI is the missing piece.
- **Smart Camera fold-back** into the main mod.
- **Resume-walk-after-disconnect** — the pre-staged spec makes it nearly free.
- **Long-run time-slicing / lightweight distant-segment proxies** — the client preview-scale constraint on very long
  runs (the PiP only frames the active edge anyway).
- **Per-world home for `GStackBuiltConveyors`** — process-global works for SP and one-world-per-process dedi.

## Key files
- `Private/Features/Walk/SFWalkService.cpp`, `SFWalkConveyance.cpp` (+ `Public/` headers, `SFWalkCommitSpec.h`,
  `SFWalkTypes.h`)
- `Private/UI/SFWalkPanelWidget.cpp`
- `Private/Core/Net/SFGameInstanceModule_SpecHooks.cpp` (Construct + GetCost hooks), `SFGameInstanceModule_NetHooks.cpp`
  (client fire/stage), `SFRCO.cpp` (`Server_StageWalkCommit`)
- `Private/Subsystem/SFSubsystem_Config.cpp` (AC-settings cycle + gating + the HUD tier-refresh),
  `SFSubsystem_HologramCreation.cpp` (tier resolution)
- `Private/Services/SFHudService.cpp` (the walk badge)
