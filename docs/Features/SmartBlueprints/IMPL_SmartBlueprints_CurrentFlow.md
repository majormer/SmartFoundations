---
title: Smart! Blueprints Current Flow
type: IMPL
date: 2026-07-07
status: Active
category: Features
tags: [blueprints, scaling, autoconnect, seams, multiplayer, spec-construction]
related: [../AutoConnect/IMPL_AutoConnect_CurrentFlow.md, ../Scaling/IMPL_Scaling_CurrentFlow.md, ../Multiplayer/PLAN_MP_ScalingConstruction_Impl.md]
---

# Smart! Blueprints Current Flow

**Smart! Blueprints** (Issue #168, shipped 34.0.0) lets a player scale a **vanilla blueprint**
into a grid of copies with Smart's grid controls (X/Y/Z, spacing, steps), and **auto-connects the
belts and pipes that reach each copy's edge to the matching connectors on its neighbours** — in a
single placement, previewed and priced before building, single-player and multiplayer.

The headline capability vanilla cannot do: an interior copy connects on **all four sides at once**
(a true 2D fabric of blueprints), and pipe blueprints also connect **vertically** into self-wiring
towers.

> This doc is the durable "how it works today" reference. It supersedes the three sprint docs that
> drove the build (`docs/Development/168-ScaleableBlueprints-Research.md`,
> `168-SmartBlueprints-Requirements.md`, `168-SmartBlueprints-SeamAutoConnect-Design.md`), whose
> durable content is folded in here.

---

## 1. Why blueprints were excluded, and the boundary problem

Blueprints were **hard-excluded** from Smart scaling (Issue #166): naive cloning broke blueprint
placement, so the adapter factory wrapped every `AFGBlueprintHologram` in `FSFUnsupportedAdapter`.
#168 replaces that gate with a real `FSFBlueprintAdapter`.

Smart's per-hologram spacing normally comes from `USFBuildableSizeRegistry` (a size profile keyed by
build class). **Blueprints have no build class in that table** — they are composites. The boundary
source is instead the hologram's own **`mLocalBounds`** (protected `FBox`; AccessTransformers Friend
grants `FSFBlueprintAdapter` access). `GetBuildingBounds()` returns those bounds transformed, falling
back to an 800³ cube if unavailable. Bounds are valid only after `LoadBlueprintToOtherWorld` stages
the blueprint, so timing matters (the adapter reads them at registration, after staging).

**Key files:** `Public/Holograms/Adapters/SFBlueprintAdapter.h/.cpp`, the gate swap in
`Private/Subsystem/SFSubsystem_HologramLifecycle.cpp` (~line 1656).

---

## 2. Grid construction — copies are REAL blueprint instances

Each grid cell spawns the **parent's own hologram class** (`Holo_Blueprint_C`), staged with the
parent's descriptor:

1. `SetBuildClass(parent's build class)` **before** `FinishSpawning` (else `AFGHologram::BeginPlay`
   asserts on `mBuildClass`, FGHologram.cpp:288).
2. `SetBlueprintDescriptor` + copy **`mBlueprintDescName`** (identity — the proxy resolves its
   descriptor by name; without it, child-built proxies are anonymous and invisible to vanilla
   snap / auto-connect / dismantle naming).
3. `LoadBlueprintToOtherWorld` (renders the contents; do NOT also call
   `AlignBuildableRootWithBounds` — it aligns internally, and a second call displaces the root off
   the grid, live 2026-07-06).

**Content-convention delta.** The parent's root was re-seated by the interactive build-gun flow; a
freshly staged clone carries the natural `LoadBlueprintToOtherWorld` convention. The two differ by a
**constant per-blueprint offset** (measured from the first blueprint-world buildable's visual-root
offset, parent vs clone). Child positioning applies this delta (rotated into the parent frame) so
clone contents tile exactly like the parent's. `SFHologramHelperService` measures and caches it at
first staging.

**Construction.** `AFGBlueprintHologram::Construct` is a **self-contained override** — it builds the
blueprint's own contents and never enters `AFGBuildableHologram::Construct` nor the base
child-construct loop. So a dedicated hook (`SFGameInstanceModule_SpecHooks.cpp`,
`SUBSCRIBE_METHOD_VIRTUAL(AFGBlueprintHologram::Construct, …)`) snapshots the staged `SF_GridChild`
blueprint children (and the seam conduit children — see §3), runs the original, then constructs each
child with the same construction id. Seam conduits fire **after** every copy so they wire against
BUILT actors.

---

## 3. Seam auto-connect — the model

Vanilla's blueprint auto-connect (`FGBlueprintOpenConnectionManager`) is **interactive-only** (aim
overlap + per-frame `UpdateAutomaticConnections(hitResult)`), so programmatic clones can never
initiate connections (live-confirmed: after the identity fix, hand-held blueprints connect TO our
clones, but the clones initiate nothing). That is the proven vanilla gap, so **Smart wires the
seams** with its own proven belt/pipe machinery. Vanilla/Smart domains are disjoint (vanilla wires
parent ↔ already-built world; Smart wires grid-member ↔ grid-member), so no forced disable is needed.

**The locked model (`FSFBlueprintSeamService`):**

- **Pairs by connector INDEX, computed ONCE, in local space, untransformed.** Vanilla duplicates one
  connection component per open content connector (`DuplicateConnectionComponent`) in a deterministic
  content-spawn order, so every clone enumerates its dup connectors in the **same order**. A seam
  pair is `(outIndex k) ↔ (inIndex m)`. Index pairs survive every transform by construction — nothing
  re-searches, nothing drifts. (Dup names embed per-world instance ids that DIFFER between clones, so
  name-sorting is wrong; **enumeration order** is the stable key.)
- **Geometry frame = HOLOGRAM-LOCAL, from the parent's DUP connectors.** That frame is the grid frame
  (grid X/Y = parent local X/Y). The originals' blueprint-world frame is NOT usable for geometry —
  live 2026-07-07 it read 180°-flipped vs the dup frame (the same content-convention mismatch as the
  clone delta). Originals still provide **openness** (`mDuplicateConnectionToOriginalMap` — the dup's
  original blueprint-world connector must be unconnected) and **belt flow direction** (belt ends are
  `FCD_ANY`; direction is fixed by which conveyor end they are — Connection0=in, Connection1=out).
  Both are frame-free.
- **Transforms move endpoints, never pairs.** Per evaluation, for each adjacent clone pair along each
  axis, look up the two dup components BY INDEX and hand them to the existing preview machinery.
  Spacing widens → same pair, longer conduit. Too far/steep → **the game declines the shape** (#466
  arbiter) and the skip is reported on the HUD; the pair persists dormant and reappears when transforms
  allow. Pairing and validity are separate concerns.
- **Sidedness from ACTUAL positions.** Grid cell-index order is NOT spatial order (scaling toward
  −X/−Y places cell N+1 at the more-negative coordinate). "Lower/Upper" for a pair is decided from the
  clones' actual parent-local positions, not their cell indices (live 2026-07-07: index-based sidedness
  put every endpoint on the far face → 180° facing → every belt "too steep").

**Evaluation** runs on the orchestrator's debounced grid-change/movement cadence
(`OnBlueprintSeamsChanged`, 100 ms coalesce), keyed by `(clone-pair, pair-index)` so previews update
in place and orphan-remove when the grid shrinks or a pair goes dormant.

**Key files:** `Public/Features/AutoConnect/SFBlueprintSeamService.h`,
`Private/Features/AutoConnect/SFBlueprintSeamService.cpp` (pair search),
`SFAutoConnectService_BlueprintSeams.cpp` (evaluation + spawn).

---

## 4. Wiring at construct — belts vs pipes

Preview update-in-place must match the create path (both learned the hard way):

- **Unlock before re-route:** the conduit routers reposition their actor with `SetActorLocation`,
  which is a **no-op on a locked hologram** — and the finalize pass locks every seam conduit while
  the parent is locked (always, during scaling). Without an unlock → route → relock sandwich, later
  transforms updated only spline data while the actor stayed at the old seam.
- **Re-mesh after re-route:** `TriggerMeshGeneration` ran only in `FinalizeSpawn` (creation), so a
  transformed seam kept rendering old geometry. Update now re-meshes.

At **construct**:

- **Belts** wire **synchronously** in `ASFConveyorBeltHologram::Construct`: build, then immediately
  scan for the neighbour's real connector within 50 cm and `SetConnection`. The blueprint construct
  hook fires seam conduits after all copies, so the neighbours exist this frame.
- **Pipes** need `WireBlueprintSeamPipe` (on the subsystem), because:
  1. Vanilla's pipe `Construct` wires the built pipe to its **snapped connections** — which for a
     seam pipe are the clone HOLOGRAM's dup connectors (set for pole suppression). That "connection"
     reports `IsConnected()==true` for exactly one frame and dangles when the hologram dies, silently
     skipping all real wiring. The fix **clears any hologram-owned / non-buildable peer** before
     scanning.
  2. The deferred junction-pipe path only `MarkForFullRebuild`s; it never `MergeNetworks`. Seam pipes
     wire synchronously (the copies exist by then) AND explicitly **merge the two copies' pipe
     networks**, or fluid never crosses the seam.

Costs aggregate via `AddChild` (seam conduits parent to the blueprint parent). Declined conduits count
in the existing `FSFAutoConnectSkipSummary` (too steep / invalid shape / too far / **too close**).

---

## 5. Z seams, spacing default, and the "too close" cue

- **Z seams (pipes only, v1):** the pair table carries Z pairs from day one (axis-uniform search).
  The evaluator services Z for **pipes** — pipes run vertical natively, so a bottom copy's up-facing
  port wires to the copy above (stacked towers). **Belt Z pairs stay cached but unserviced**: vertical
  belt transport is a conveyor LIFT, preview machinery Smart does not have yet (real v2 work).
- **Spacing default:** picking up a blueprint defaults spacing to **1 m on every axis** (seam conduits
  need a physical gap — a conduit under ~0.5 m can't be built). One-shot latch
  (`bBlueprintSpacingDefaultApplied`): applied only on the transition INTO blueprint building, so
  post-fire respawns and repeated pickups keep whatever the player set (including 0 for a deliberate
  flush grid).
- **"Too close":** a blueprint whose ports sit deep inside its bounds can leave <0.5 m between mating
  ports even at 1 m spacing. Those skips report as **"too close"** on the HUD (widen spacing) rather
  than vanishing silently.

Known v1 gap: at flush tiling (<0.5 m port gap) no conduit fits, and coincident ports are not
direct-wired.

---

## 6. Multiplayer

MP reuses Smart's **spec-construction model** (`docs/Features/Multiplayer/`): a network client's fire
strips the preview children and stages a **compact spec** (grid + transforms) over an RCO; the server
re-expands and builds. The blueprint-specific gap and its fix:

- **The gap:** the generic server-side re-expansion is hooked at `AFGBuildableHologram::Construct`,
  which the blueprint's self-contained `Construct` **never enters** — so a client-fired blueprint grid
  built only the parent on a dedicated server. The blueprint `Construct` hook now consumes the staged
  spec on authority and runs `ExpandScalingSpecIntoChildren` (blueprint cell staging existed) then
  `SpawnConduitPlanChildren`. Seam belts/pipes ride the **same #334 conduit-plan** capture/replay as
  every other auto-connect family (the client's seam previews were already captured by the tag-driven
  plan walk).
- **Spec carries blueprint truth:** `ItemSize` from the adapter's cached bounds (the registry has no
  composite profile — its 8×8×4 m fallback gave the server a wrong pitch); the measured cell **basis
  vectors** (the preview's actual per-axis pitch — the server position calculator provably disagrees
  with the client preview for composites); and the client parent's measured **content anchor**.
- **Positions are MEASURED, never inferred.** The staging convention varies **per blueprint AND per
  context** (client interactive parent vs client preview child vs server construct-message parent vs
  server spec cell). The final model: the server measures its own parent's content anchor and each
  staged cell's anchor, tiles each cell to the measured server parent, and shifts the conduit plan by
  the rotated (measured-server-anchor − client-anchor) difference — **zero when conventions agree,
  exact when they differ**. A carried constant provably fixed one blueprint (FluidGrid) and broke
  another (TestBP) by exactly itself; measuring seats both. (`MeasureBlueprintContentAnchor`.)
- **Per-copy proxies (no group proxy).** Unlike the generic grid path, blueprint parents get NO group
  proxy: each copy creates its own per-copy blueprint proxy — the **individual-dismantle** model the
  feature ships with (dismantle one copy → remove just that copy).
- **Per-placement caps (v1):** a blueprint grid is actor-heavy and always carries a conduit plan
  (which excludes it from the deferred time-sliced expansion path), so one MP placement is bounded by
  **~2000 expanded buildings** (the #418 inline-expansion / server-freeze limit) AND **~45 KB of
  staged conduit plan (~170 conduits)** (the reliable-RPC ceiling). Over either, the fire is refused
  and the reason shows on the HUD ("build in smaller sections"). SP has no such caps. Lifting them =
  teaching the deferred-expansion path to carry conduit plans (follow-up).

**Live-validated on a dedicated server 2026-07-07:** TestBP belt grid + a 26-copy FluidGrid pipe
3×3×3 (54/54 seam pipes wired, fluid flows); both blueprints seat their conduits flush; client→server
join/fire and the HUD guard notices all working.

---

## 7. Scope summary

| Area | v1 (shipped 34.0.0) | Deferred |
|---|---|---|
| Axes | X/Y belts+pipes; Z **pipes** | Z **belts** (need conveyor-lift previews — v2) |
| Modes | Single-player + multiplayer | — |
| Dismantle | Individual (per-copy proxy) | "Merge grid into one blueprint" toggle (maintainer decision pending) |
| Flush tiling | Not direct-wired (<0.5 m gap); "too close" HUD cue | Direct port-to-port wiring at flush |
| MP scale | ~2000 buildings / ~170 conduits per placement | Deferred-expansion-with-plan to lift the caps |
| Transforms | Spacing, steps | Stagger / rotation on blueprint grids (non-affine; calculator fallback, not serviced) |

---

## 8. Key files

| Concern | File |
|---|---|
| Adapter / bounds | `Public/Holograms/Adapters/SFBlueprintAdapter.*`, gate in `SFSubsystem_HologramLifecycle.cpp` |
| Grid staging + content delta | `SFHologramHelperService.cpp`, `SFScalingSpecExpansion.cpp` (cell path) |
| Seam pair search | `SFBlueprintSeamService.h/.cpp` (AccessTransformers Friend for `mDuplicateConnectionToOriginalMap`) |
| Seam evaluation + spawn | `SFAutoConnectService_BlueprintSeams.cpp`, orchestrator `OnBlueprintSeamsChanged` |
| Belt/pipe construct wiring | `SFConveyorBeltHologram.cpp` (50 cm sync), `SFSubsystem_AutoConnect.cpp` (`WireBlueprintSeamPipe`) |
| SP+MP construct hook | `Core/Net/SFGameInstanceModule_SpecHooks.cpp` (blueprint `Construct` hook) |
| MP fire guards + notices | `Core/Net/SFGameInstanceModule_NetHooks.cpp` |
| MP spec (basis/anchor) | `Public/Features/Scaling/SFScalingSpec.h`, `SFScalingSpecExpansion.cpp` |
