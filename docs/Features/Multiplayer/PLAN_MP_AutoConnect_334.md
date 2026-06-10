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

**REVISED increment C: ship the explicit belt plan in the staged intent.**
- At the client fire hook, before stripping, capture per auto-connect belt:
  `{ DistributorCellIndex (grid linear index or parent=-1), DistributorConnectorName,
     TargetBuildable (replicated AFGBuildable* - serializes via the package map),
     TargetConnectorName, BeltRecipe, SplinePoints }`.
- Stage it with the scaling spec via `USFRCO` (extend the staged struct).
- Server, AFTER the grid constructs (out_children known): resolve each entry - cell index ->
  built distributor, connector by NAME on the built actor, target connector by name on the
  replicated buildable - then build each belt directly with the proven primitive
  (`ASFConveyorBeltHologram` + `SetSplineDataAndUpdate` + `SetSnappedConnections` + `Construct`),
  the same mechanism the Extend clone spawner uses. No preview machinery involved.
- Same pattern then extends to pipes (junction plan) and power (wire endpoint pairs - even
  simpler: two connection components + wire class).
- Charge the belts as part of the staged plan's cost (closes the unpaid-belt interim gap).

## Live-test checkpoints (after A)

1. Client builds a merger with auto-connect belts -> server log: does the server's
   OnActorSpawned belt section run? Do server-derived belts get BUILT and wired? Do they
   replicate to the client?
2. Client builds a power-pole grid -> do wires appear (server-spawned) on both sides? No ghost
   client wires?
3. SP regression: belts/pipes/power wire exactly as before (authority held locally).
