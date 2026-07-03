---
title: Smart Scaling Child Hologram Type Selection
type: DESIGN
date: 2026-07-03
status: Active
category: Features
tags: [scaling, child_holograms, hologram_dispatch, auto_connect, override, tiers]
related: [./IMPL_Scaling_CurrentFlow.md, ../../ARCHITECTURE.md]
---

# Smart Scaling Child Hologram Type Selection

## Purpose

When Smart! scales a build, the active build-gun hologram becomes the **parent** of an X/Y/Z grid of
**child holograms**. This document is the authoritative policy for the question *"which child hologram
class does each grid cell get, and why?"* It exists because the naive answer — "a custom class per
buildable type" — is wrong and does not scale. The correct model is a small, bounded **three-tier**
taxonomy driven by **behavior**, not by type.

Source of truth for the live dispatch: `FSFHologramHelperService::RegenerateChildHologramGrid`
(`Source/SmartFoundations/Private/Subsystem/SFHologramHelperService.cpp`).

## The core principle

Two facts decide everything here:

1. **The "make a good grid child" behavior is generic and type-independent.** Every grid child, no
   matter what it builds, needs the same three things:
   - **Stay where Smart! puts it** — override `SetHologramLocationAndRotation` to a no-op so vanilla
     parent-propagation stops resetting the child toward the parent's hit result each frame (the
     historical "jump to origin"; see #418).
   - **Never block the parent's placement** — override `CheckValidPlacement` to force `HMS_OK` (grid
     children are positioned by Smart! math, not by snapping rules).
   - **Skip clearance** — override `CheckClearance` (children don't self-validate clearance).

   `ASFBuildableChildHologram` (`Source/SmartFoundations/Public/Holograms/Core/`) is the canonical
   carrier of this behavior. It is generic: `AFGBuildableHologram::Construct` builds *any* plain
   buildable from `mBuildClass`, and cost aggregates through the recipe (`AddChild` → parent).

2. **Specialization is needed only when a buildable's vanilla hologram does essential work of its own**
   — spline generation (belts/pipes), spawning its own sub-child poles, water-volume validation, sign
   text, aim/angle steps. That work lives in the *specific* vanilla hologram class, so the Smart child
   must subclass *that* class and layer the three generic overrides on top.

**Corollary:** the number of child classes is bounded by the number of distinct *behavior families*
(~6), plus one generic class, plus one default. It is **not** proportional to the number of buildable
types. A hundred machine variants all share the generic class.

## The three tiers

| Tier | When it applies | Child class | Count |
|------|-----------------|-------------|-------|
| **1 — Generic** | Plain buildables whose vanilla hologram does no essential specialized construction (foundations, walls, ceiling lights, wall attachments, machines, storage, pillars — every type without an explicit Tier-2 branch). **This is the default.** | `ASFBuildableChildHologram` | **one class, unbounded types** |
| **2 — Specialized family** | Buildables whose vanilla hologram does essential work the generic path cannot replicate. Each family subclasses its specific vanilla hologram + adds the three overrides. | one per family (see map) | ~6 |
| **3 — Vanilla-delegate** | STACKABLE conveyor/pipe/hypertube supports ONLY (explicit `IsStackableSupportHologram` branch): their vanilla holograms carry the stack/connection behavior the stackable AC preview builds against (#341/#354/#364). These children have no drift override and rely on the intended-transform re-apply. | recipe's own vanilla hologram via `SpawnChildHologram` | one path, three build classes |

## Current dispatch map

As implemented in `RegenerateChildHologramGrid` (in evaluation order, since the #418 tier true-up):

| Parent hologram (predicate) | Child class | Tier | Provenance |
|---|---|---|---|
| `AFGPassthroughHologram` | `ASFPassthroughChildHologram` | 2 | #187 |
| `AFGFloodlightHologram` | `ASFFloodlightChildHologram` | 2 | aim/angle step |
| `IsRegularConveyorPoleHologram(...)` | `ASFConveyorPoleChildHologram` | 2 | #354 (gated off the *stackable* pole) |
| `IsRegularPipelinePoleHologram(...)` | `ASFPipelinePoleChildHologram` | 2 | #364 |
| `AFGStandaloneSignHologram` | `ASFStandaloneSignChildHologram` | 2 | sign text |
| `AFGWaterPumpHologram` | `ASFWaterPumpChildHologram` | 2 | #197 / #428 (water-volume validation, tick kept ON; drift override added with #418) |
| `IsStackableSupportHologram(...)` | recipe's own vanilla hologram (`SpawnChildHologram`) | 3 | #341/#354/#364 — stackable AC preview depends on vanilla child behavior |
| *(generic `else` — everything remaining)* | `ASFBuildableChildHologram` (`SpawnBuildableChildHologram`) | 1 | #418 — consolidates the former #200 ceiling-light, #268 wall-attachment, and #418 foundation branches; replaces the raw-vanilla default that drifted |

Only vanilla-delegate (Tier-3) children receive the legacy generic post-spawn setup
(`bVanillaDelegateChildren` guard); Smart children are fully configured in their spawn paths and
must not get it (it would re-enable collision and clobber material state).

**Tick policy** (same function, unlocked-parent sweep): tick stays OFF for all Smart children —
Tier-1 stubs validation and Tier-2 mostly does too, so ticking bought nothing and cost real frame
time at 40K+ children. Exceptions: water pump children (tick ON — per-frame water-volume check)
and Tier-3 vanilla children (tick ON — preserve vanilla dynamic validation). A locked parent
disables tick on everything.

## Decision procedure for a new buildable type

```
Does the buildable's vanilla hologram do essential work beyond generic mBuildClass construction?
  (spline gen? spawns own sub-children? special validation/snapping? multi-step aim/angle/text?)
        │
        ├── NO  →  Tier 1. Route it to ASFBuildableChildHologram. No new class.
        │
        └── YES →  Tier 2. Subclass that vanilla hologram; add the three overrides
                   (SetHologramLocationAndRotation no-op, CheckValidPlacement→HMS_OK, CheckClearance skip).
                   Register it in the dispatch.
```

If unsure, start at **Tier 1** and test: if the preview builds and positions correctly and cost
aggregates, it *is* Tier 1. Only escalate to Tier 2 when a concrete behavior is missing.

## Constraints this policy serves

- **C1 — Cost must always trace from children.** All tiers carry the recipe (`SetRecipe` / stored
  production recipe) so `GetCost` aggregates to the parent via `AddChild`. Placeholder-only children
  are not permitted to break this link.
- **C2 — Children must support auto-connect (connectors), not be dumb meshes.** This is the reason
  Tier 2 exists for the conduit families.

### Critical caveat: drift-proof ≠ auto-connect

The three generic overrides fix **positioning for every type**. They do **not** by themselves give a
type **auto-connect**. AC *preview* for the conduit families (belts / pipes / hypertubes) requires the
specialized hologram to expose connectors/splines while previewing — that is Tier 2's job. For
non-conduit buildables (foundations, walls, machines — the bulk of scaling), Tier 1 is sufficient
because their connectors come from the *constructed* buildable, and stackable-AC keys off the grid
position map rather than the child's hologram class.

## Status & roadmap

**The Tier-1 widening landed with the #418 true-up** (2026-07-03): the generic `else` now routes
through `SpawnBuildableChildHologram` — every plain buildable is drift-proof, carries the stored
production recipe (scaled machines keep their selected recipe), and hides the ClearanceBox mesh.
All seven Smart child classes now have the drift override (the water pump child was the last
holdout). Remaining roadmap:

- **Stackable supports → Tier 1** (optional): the family checks are build-class-name based, so
  Tier-1 children would still be recognized — but the stackable AC preview builds belt/pipe spans
  against vanilla child behavior, so this move is gated on validating series-run wiring end-to-end.
- **Retire the `ScalingChildIntendedTransforms` re-apply tax**: with drift blocked everywhere
  except Tier-3, the N-tick refresh only earns its keep for stackables. Measure, then shrink its
  scope or remove it.
- **Coordinate-keyed positioning** (the Y-growth full-refresh): **implemented 2026-07-03**
  (`USFGridCoordComponent` — every grid child carries its unsigned cell; positioning batch and
  stackable-AC neighbor maps read it instead of decoding spawn order; shrink evicts vanished
  cells). Pending in-game validation.

## How to add a Tier-2 family

1. Subclass the specific vanilla hologram (e.g. `class ASFFooChildHologram : public AFGFooHologram`).
2. Add the three overrides. They are trivial stubs — consider sharing them via a macro/mixin so a new
   family costs ~5 lines rather than a reimplementation.
3. Preserve family-specific setup in a dedicated spawn branch (tick policy, validation, connectors).
4. Add the predicate to the dispatch **and** to the `bIsCustomChild` guard.
5. If the family participates in stackable series-run AC, confirm it in
   `SFAutoConnectService_Stackable`.

## Anti-patterns

- **A class per buildable type.** No. Tier 1 covers the long tail; only add Tier-2 classes for a
  distinct *behavior*.
- **Routing a specialized family through Tier 1.** Loses its essential construction (e.g. a belt would
  lose its spline). Specialized families must stay Tier 2.
- **Dumb-mesh placeholders that sever the cost/connector link.** Violates C1/C2.

## Future architecture note

The current dispatch is a hardcoded `if/else` chain. A natural evolution is a **registry/factory**
keyed on parent hologram class → spawn strategy (default = Tier 1, registered = Tier 2), mirroring the
`ISFHologramAdapter` pattern already in the codebase. This makes the taxonomy additive and
self-documenting and gives unknown types the safe Tier-1 path automatically. Non-blocking; the tier
*policy* above holds regardless of dispatch mechanism.

## Related

- `./IMPL_Scaling_CurrentFlow.md` — the end-to-end scaling runtime flow.
- `../../ARCHITECTURE.md` — subsystem/service overview.
- #418 — foundation override, time-budgeted reposition, clearance-box hide, Tier-1 widening,
  coordinate-keyed positioning (separate sprint spec).
