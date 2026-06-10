---
title: Smart Multiplayer - Auto-Connect Server-Authoritative Wiring (#334)
type: PLAN
date: 2026-06-09
status: Active
category: Features
branch: feature/mp-server-construction
related:
  - ./DESIGN_MP_ConstructionModel.md
  - ./PLAN_MP_ScalingConstruction_Impl.md
issues:
  - 334
  - 176
---

# Auto-Connect Server-Authoritative Wiring (#334)

## Grounded findings (2026-06-09 investigation + live crash)

- **The server already runs the auto-connect DECISION pipeline.** The dedi's `USFSubsystem`
  mirrors the build-gun hologram (same symmetry the spec path relies on) and spawns its own
  belt preview children against it (live dedi log: `Belt hologram spawned ... (Tier 6)` on the
  server during a client's merger placement). The "ship the wiring plan to the server" problem
  largely does not exist - the server derives its own plan.
- **No post-build mutation path has an authority guard.** `USFSubsystem::OnActorSpawned` belt
  (~:144) / pipe (~:335) / stackable pipe (~:505) / stackable belt (~:690) sections and
  `FSFPowerAutoConnectManager::OnPowerPoleBuilt` (wire `SpawnActor` at :1245) run on every peer.
  In MP the CLIENT runs them against replicated actors and spawns client-only ghosts - the
  original #334 bug.
- **Client preview children must never cross the construct message** (server crash deserializing
  a stripped-spline belt child) - fixed by the fire-hook strip (commit 2f91ccf). Auto-connect in
  MP currently builds the parent but wires nothing.
- **The only client-only state the server's pipeline lacks is the runtime settings**
  (`FAutoConnectRuntimeSettings`: enables, belt/pipe tiers, power axis/range/reserve, routing
  modes - not replicated; server uses its own config defaults today).

## Architecture

Same intent->authority model as scaling:

1. Client previews locally (unchanged).
2. Client strips preview children at fire (done) and stages its **auto-connect runtime settings**
   alongside the scaling spec via the existing `USFRCO` staging channel.
3. The server's own subsystem - aiming mirror + staged settings - derives the wiring plan
   server-side and performs ALL post-build wiring under authority.
4. Clients never mutate: every post-build hook is authority-gated (`!IsClient`). SP standalone
   and listen-host hold authority, so their behaviour is unchanged.

## Increments

- **A (gates)**: authority-gate every post-build mutation listed above + the legacy blueprint-proxy
  grouping in OnActorSpawned (client-side ghost proxies for replicated actors). Mechanical; SP
  unchanged. THEN live-test a client merger+belts build and observe what the server's own pipeline
  already does - that data drives B/C.
- **B (settings sync)**: extend the staged intent (RCO) with the client's
  `FAutoConnectRuntimeSettings` snapshot; server uses the staging player's settings for decisions
  on that player's builds.
- **C (deltas)**: fix whatever the live test exposes - candidates: construct-message
  deserialization clobbering the server hologram's own preview children; connector-pair resolution
  against built (vs preview) actors; manufacturer production-recipe application for spec children;
  per-family verification (belts, pipes, power, stackables).

## Increment C live findings (2026-06-09 evening, three rounds) - REVISED DESIGN

Three approaches to belt wiring at the server construct seam were tried live; the evidence now
fixes the design:

1. **Re-derivation fails in that context**: `ProcessSingleDistributor` returns 0 previews for
   every distributor at the construct seam even with sane positions and 20+ nearby buildings
   found by the physics scan (the failure is deeper in the pair logic, context-dependent).
2. **Reusing the server's aim-time previews fails on timing**: `GetBeltPreviews(constructHolo)`
   is empty at the seam, then repopulates DURING the construct (the deserialize moves the
   hologram, retriggering the preview pipeline) - always one step behind the child-list
   enumeration. Maintainer also notes distributor belt previews are not construct-grade the way
   e.g. stackable-pole belt holograms are.
3. **The CLIENT is the only party with the complete, real plan at fire time** (its preview belts
   are full holograms with routed splines + snapped connectors; we strip them at fire anyway).

**REVISED increment C: ship the explicit belt plan in the staged intent. IMPLEMENTED 2026-06-09.**

The implementation simplified the sketched design in one important way: NO connector names, cell
indices, or replicated component refs cross the wire. `ASFConveyorBeltHologram::Construct`'s
`SF_BeltAutoConnectChild` path already wires a freshly built belt GEOMETRICALLY - each free end
connects to the nearest free, direction-compatible connector within 50cm among BUILT actors only
(the same mechanism SP uses for every auto-connect belt today). So the plan per belt is just
`{ BeltClass, Recipe, Location, Rotation, SplinePoints (local) }` (`FSFBeltPlanEntry`), and
endpoint resolution is implicit in the belt's geometry.

- **Capture** (`SFScalingSpecExpansion::CaptureBeltPlan`, client fire hook): BEFORE the strip
  destroys the previews, walk the service's stored previews for the parent + every child
  distributor and snapshot each belt preview's class/recipe/transform/routed spline into
  `Spec.BeltPlan`, plus its exact vanilla length-based `GetCost(false)` into `Spec.BeltPlanCost`
  (merged per item class).
- **Stage**: part of `FSFScalingSpec` via the existing `USFRCO::Server_StageScalingSpec` -
  overwrite semantics carry over, so a stale plan can't leak. Validation bounds: <=4096 belts,
  <=64 spline points each. A reliable-RPC ceiling guard at the fire hook refuses the fire
  (~>45KB estimated plan) BEFORE the previews are destroyed, keeping the grid live.
- **Replay** (`SFScalingSpecExpansion::SpawnBeltPlanChildren`, server Construct hook): after
  `ExpandScalingSpecIntoChildren`, spawn each plan belt as a fresh `ASFConveyorBeltHologram`
  child (Extend clone-spawner recipe: deferred spawn, `SetBuildClass`/`SetRecipe`,
  `SF_BeltAutoConnectChild` tag, `DisableValidation`, `SetSplineDataAndUpdate` before AND after
  `AddChild`). Appended AFTER the grid cells in `mChildren`, so the vanilla child-construct loop
  builds the distributors first and each belt then self-wires against built actors. Belts join
  `out_children` -> the spec group proxy registers them -> Smart Dismantle groups include them.
- **Cost**: the GetCost hook appends `Spec.BeltPlanCost` after the cell scaling - the staged
  belts are charged with the grid (closes the unpaid-belt interim gap).
- Belt-plan live-validated 2026-06-09/10: merger AND splitter rows, building belts + manifold
  lanes all built/wired/replicated/charged; belts in the Smart Dismantle group (post-construct
  sweep - spline buildables never reach the ConfigureActor base body the pre-BeginPlay hook
  patches; safe because conveyors are never lightweight).

## Increment D (2026-06-10): plan GENERALIZED to all conduit families - awaiting live test

`FSFBeltPlanEntry` -> `FSFConduitPlanEntry` with `ESFConduitPlanKind`
{Belt, Pipe, StackableBelt, StackablePipe, Wire}; `Spec.ConduitPlan` / `Spec.ConduitPlanCost`.

- **Capture** is now a single uniform walk of the parent's tagged preview children (all families
  attach to the parent - proven by the fire-hook strip): belt/pipe kinds snapshot the routed
  spline; wires snapshot the two endpoint connection LOCATIONS (`AFGWireHologram::GetConnection`);
  pipe entries record `bFloorHolePipe` (registry `PipeAutoConnectConn0 == null`); stackable kinds
  record their run index.
- **Replay** mirrors each family's client spawn recipe:
  - Belt: as before (`SF_BeltAutoConnectChild`, geometric self-wiring in Construct).
  - StackableBelt: `SF_StackableChild` + registry chain identity (synthetic high-offset
    StackChainId) -> stack-chain Construct path (coincidence wiring + #341 in-frame chain
    unification, whose parent hook is also live server-side).
  - Pipe: `SF_PipeAutoConnectChild` + registry `bIsPipeAutoConnectChild`;
    `PipeAutoConnectConn0` is ONLY a branch discriminator at the construct seam (never
    dereferenced) - set to one of the pipe holo's own components for the junction branch
    (deferred next-tick geometric wiring), left null for the floor-hole branch (passthrough
    snap registration by proximity).
  - StackablePipe: `SF_StackableChild` + `bIsStackablePipe` (overlap wiring + network merge in
    Construct; the OnActorSpawned manual build path dedups via `bHasBeenConstructed`).
  - Wire: resolve both endpoints by location against the PRE-construct hologram set (parent +
    grid children; vanilla remaps hologram connections to built poles - the SP mechanism), fall
    back to built world actors; spawn `ASFWireHologram` + `SetupWirePreview(C0, C1)`; vanilla
    Construct builds the AFGBuildableWire.
- Server-side safety nets that also run with authority: deferred belt wiring (next-tick,
  distributor parents), deferred pipe wiring (next-tick), stackable-pipe manual build+connect.

## Live-test checkpoints (increment D, all conduit families)

Belt checkpoints (merger/splitter rows, manifold lanes, dismantle group, costs) PASSED 2026-06-10.

1. Power-pole grid -> wires appear between grid poles on both sides, charged; check
   `[MP-334] SpawnConduitPlanChildren` and no "wire entry ... unresolved" warnings.
2. Pipe junction grid next to fluid buildings -> pipes build + wire (deferred wiring log:
   "PIPE AUTO-CONNECT DEFERRED WIRING"); floor-hole (passthrough) pipe placement wires through
   the foundation.
3. Stackable conveyor pole grid -> run belts build + wire + unify into chains (#341 in-frame log);
   wall poles / ceiling mounts / standard conveyor poles same.
4. Stackable pipe support grid -> run pipes build + wire + networks merge.
5. All conduits join the Smart Dismantle group (post-construct sweep covers every built child).
6. SP regression sweep: all auto-connect families unchanged (capture/stage is NM_Client-only).
