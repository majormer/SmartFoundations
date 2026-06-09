---
title: Smart Multiplayer — Scaling Construction Implementation Plan
type: PLAN
date: 2026-06-09
status: Draft (in progress)
category: Features
branch: feature/mp-server-construction
related:
  - ./DESIGN_MP_ConstructionModel.md
  - ../Scaling/IMPL_Scaling_CurrentFlow.md
issues:
  - 176
---

# Smart Multiplayer — Scaling Construction Implementation Plan

Concrete implementation of the spec-based construction model (see `DESIGN_MP_ConstructionModel.md`)
for the **scaling** feature. Extend is planned separately (`PLAN_MP_ExtendConstruction_Strategy.md`).

## Grounded current-state facts (verified in code)

- Plain scaling parent = the **vanilla** build-gun hologram (`USFSubsystem::RegisterActiveHologram`
  sets `ActiveHologram = Hologram`, no swap). The `ASFFactoryHologram` swap
  (`USFExtendService::SwapToSmartFactoryHologram`, proven working) only runs in the Extend path.
- Grid children are real holograms (`ASFSmartChildHologram` / `ASFBuildableChildHologram` / logistics
  variants), tagged `SF_GridChild`, `AddChild`'d onto the parent (`SFHologramHelperService`).
- Commit rides the vanilla build-gun fire → `UFGBuildGunStateBuild::Server_ConstructHologram`, which
  serializes the parent **plus `mChildren`** into one reliable message → 64 KB ceiling (~135 cells).
- Existing safety: `RegisterClientGridChunkFireHook` cancels a client fire when a `SF_GridChild`
  grid exceeds `SF_MP_OVERSIZED_CELLS` (130).
- Per-cell transforms are computed by `USFPositionCalculator::CalculateChildPosition(X,Y,Z, parentLoc,
  parentRot, itemSize, FSFCounterState, gridIndex, anchorOffset)` — **deterministic** from
  `FSFCounterState` + parent transform + item size. This is reproducible server-side.
- **Scaling grids are uniform** — one build class + one recipe for the whole grid. The spec is small
  regardless of cell count.
- Cost today = vanilla `AFGHologram::GetCost(includeChildren=true)` over `mChildren`. With children
  off the wire we must compute cost from the spec (the `CanAffordExtendCost` pattern already does
  per-item availability checks).

## Chosen approach: ride the natural fire + compact spec + server expansion (Option A)

Rationale: keep construction on the **proven rails** (the build-gun fire *does* construct — verified
to ~135 cells; the failure was payload size, not the fire). Slice 0 proved that *re-driving*
construction off the natural fire is treacherous (sync/deferred re-fire never construct;
hand-serialized `Server_ConstructHologram` crashed the dedicated server). So we keep the single
natural fire and shrink the payload to O(1), letting the server expand.

RCO transport (Option B) remains the fallback / the path for Extend and batch ops (Restore), where
there is no single hologram to ride.

### Flow

1. **Preview — unchanged.** Client builds the `SF_GridChild` preview grid locally exactly as today.
2. **Swap the scaling parent to `ASFFactoryHologram`** (reuse the proven Extend swap) so the parent
   can carry a spec and override `Construct`. Done at grid-activation time, before any fire.
3. **Spec capture.** `ASFFactoryHologram` holds an `FSFScalingSpec` (below). It is populated from the
   authoritative `FSFCounterState` + base transform + build class + recipe + item size + anchor
   offset whenever the grid changes.
4. **Strip children from the wire.** Just before the fire serializes, the grid children are **not**
   part of the serialized `mChildren` (kept as a client-local preview list). The serialized message
   is parent + `FSFScalingSpec` only (O(1)).
5. **Spec serialization.** `ASFFactoryHologram` overrides `SerializeConstructMessage` (the
   `IFGConstructionMessageInterface` hook vanilla uses for `mDesiredZoop`-style custom fields) to
   write/read `FSFScalingSpec` alongside the base hologram data.
6. **Server expansion.** `ASFFactoryHologram::Construct` (server, `HasAuthority`) reads the spec,
   reconstructs each cell transform via `USFPositionCalculator`, and constructs one buildable per
   cell — appending each to `out_children`. Results are real, replicated, individually-interactable
   buildables. Recipe applied per the uniform spec.
7. **Cost.** Server validates total cost = per-building cost x cell count against the instigator's
   inventory + Dimensional Depot (generalize `CanAffordExtendCost`); charge server-side.
8. **Time-slicing (scale slice).** For large N, `Construct` enqueues the spec into a server
   tick-driven construction queue (build K cells/frame) instead of building all in one frame; the
   first buildable returns immediately so the build registers, the rest stream in. (Implemented as a
   second slice; synchronous construct first to validate the model.)
9. **Failure / reconciliation.** Per-cell construction failure is isolated (skip that cell, continue).
   No oversized-blob all-or-nothing; the Slice 0 orphan-ghost class of bug cannot occur because the
   client never shipped N objects.

## FSFScalingSpec (data model)

```
USTRUCT()
struct FSFScalingSpec
{
    TSubclassOf<class UFGRecipe>     Recipe;        // uniform recipe for the grid (drives build class + cost)
    FTransform                       BaseTransform; // parent/origin cell world transform
    FSFCounterState                  Counters;      // grid counts + spacing/steps/stagger/rotation
    FVector                          ItemSize;      // cell size used by the position calculator
    FVector                          AnchorOffset;  // attachment anchor compensation
    bool                             bValid = false;
};
```

Compact and fixed-size (no per-cell data). Serializes in a handful of bytes + the recipe class ref.

## Files to change

| File | Change |
|---|---|
| `Holograms/Core/SFFactoryHologram.h/.cpp` | Add `FSFScalingSpec mScalingSpec`; override `SerializeConstructMessage`; expand spec in `Construct` (server); spec-based cost in `CheckCanAfford` |
| `Features/Scaling/SFScalingTypes.h` (or new) | `FSFScalingSpec` |
| Scaling activation (`SFSubsystem` lifecycle) | Swap plain-scaling parent to `ASFFactoryHologram`; populate `mScalingSpec` on grid change |
| `Services/SFGridSpawnerService` / `SFHologramHelperService` | Keep preview children off the serialized `mChildren` (client-local preview list) |
| `Module/SFGameInstanceModule.cpp` | Replace the oversized-grid *refusal* guard with the spec path once validated (keep guard as fallback while gated) |
| `Subsystem/SFPositionCalculator` | Reused as-is server-side (no change; ensure determinism) |

## Validation gates (require the user's MP test — the testing blocker)

1. Does a client build-gun fire deliver the spec-carrying `ASFFactoryHologram` (children stripped) to
   the server and run `Construct` server-side? (Expected yes — small payload on the proven fire.)
2. Does `SerializeConstructMessage` round-trip `FSFScalingSpec` correctly client->server?
3. Does server-side per-cell construction produce correctly-placed, replicated, interactable
   buildables on all clients?
4. Cost charged once, correctly, server-side.
5. Large-N time-slicing does not hitch the server.

These cannot be verified without an in-game MP session; they are the points to test when available.

## Slice order

- **S1** swap scaling parent + `FSFScalingSpec` + capture + serialize override (compile-clean).
- **S2** server `Construct` expansion (synchronous) + spec-based cost. *Validation gate 1-4.*
- **S3** strip children from wire + retire the oversized refusal guard behind the new path.
- **S4** server-side time-sliced construction queue. *Validation gate 5.*
- **S5** failure isolation + preview reconciliation polish.
