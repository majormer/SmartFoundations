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
| **1 — Generic** | Plain buildables whose vanilla hologram does no essential specialized construction (foundations, walls, ceiling lights, machines, storage, pillars, most single-click buildables). | `ASFBuildableChildHologram` | **one class, unbounded types** |
| **2 — Specialized family** | Buildables whose vanilla hologram does essential work the generic path cannot replicate. Each family subclasses its specific vanilla hologram + adds the three overrides. | one per family (see map) | ~6 |
| **3 — Default fallback** | Unknown / not-yet-classified types. | *Currently* raw vanilla (drifts — see Known Gap); *target* is the Tier-1 generic class. | one path |

## Current dispatch map

As implemented in `RegenerateChildHologramGrid` (in evaluation order):

| Parent hologram (predicate) | Child class | Tier | Provenance |
|---|---|---|---|
| `AFGPassthroughHologram` | `ASFPassthroughChildHologram` | 2 | #187 |
| `AFGCeilingLightHologram` | `ASFBuildableChildHologram` | 1 | |
| `AFGFloodlightHologram` | `ASFFloodlightChildHologram` | 2 | aim/angle step |
| `IsRegularConveyorPoleHologram(...)` | `ASFConveyorPoleChildHologram` | 2 | #354 (gated off the *stackable* pole) |
| `IsRegularPipelinePoleHologram(...)` | `ASFPipelinePoleChildHologram` | 2 | #364 |
| `AFGStandaloneSignHologram` | `ASFStandaloneSignChildHologram` | 2 | sign text |
| `AFGWallAttachmentHologram` | `ASFBuildableChildHologram` | 1 | #268 |
| `AFGWaterPumpHologram` | `ASFWaterPumpChildHologram` | 2 | #197 / #428 (water-volume validation, tick kept ON) |
| `AFGFoundationHologram` | `ASFBuildableChildHologram` | 1 | #418 |
| *(generic `else`)* | `SpawnChildHologram(...)` → raw vanilla per-recipe hologram | 3 | **drifts — see Known Gap** |

Fully-configured Tier-1/2 children are exempted from the generic post-spawn setup via the
`bIsCustomChild` guard in the same function (it would otherwise re-enable collision and clobber
material state).

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

## Known gap (roadmap)

The Tier-3 `else` still spawns the raw vanilla hologram, which has none of the overrides — so those
children **drift** and lean on the legacy `ScalingChildIntendedTransforms` + N-tick refresh tax to be
dragged back into place. **Planned:** route the `else` through Tier 1 (`ASFBuildableChildHologram`),
folding in the stored-production-recipe copy the current generic path performs
(`SFHologramHelperService_Children.cpp` `SpawnChildHologram`, ~lines 938–955) so scaled machines keep
their selected recipe. That single change makes every plain buildable drift-proof and lets the refresh
tax be retired (measure first). Tracked under #418.

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
