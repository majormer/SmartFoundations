---
title: Buildable Construction Contract Catalog
type: REF
status: Active
category: Reference
tags: [buildables, holograms, construction, snapping, optimization]
---

# Buildable Construction Contract Catalog

## Purpose

This catalog documents the runtime contracts Smart must preserve when creating, transforming,
connecting, cloning, restoring, or optimizing vanilla construction. It is organized by construction
family, with concrete-class profiles inside each family.

A concrete class name alone is not a complete contract. Behavior may come from a shared hologram
base, a subsystem-managed network, owned companion actors, dynamic geometry, or replicated state.

## Standard Profile

Each family reference records:

1. Identity: display name, recipe, hologram, built class, and significant base classes.
2. Topology: named/indexed components, local transforms, direction, and valid relationships.
3. Construction relationships: parent, children, companions, ownership, and assignment fields.
4. Snap state: candidates, peers, thresholds, direction state, and invalid partial states.
5. Dynamic state: spline data, floor thickness, dimensions, customization, or other variable geometry.
6. Lifecycle: validation, construction, registration, replication, dismantle, and restore behavior.
7. Smart compatibility: Scaling, Auto-Connect, Extend/Restore, cloning, Blueprints, and optimization.
8. Evidence: game version, evidence source, and built/hologram parity status.

Raw diagnostic output is evidence, not the durable reference. Runtime object names and incidental
nearby state are omitted after the relevant contract has been verified.

## Default spacing

Use a matching native Ctrl-snap interval when one exists for the same class, mounting plane and
orientation. A snap to a different face or attachment plane does not establish adjacent-copy spacing.
Otherwise, round the horizontal footprint upward to the native placement grid after any required
projection. Raw mesh bounds alone are not a usable grid pitch.

For fixed-height families, fit physical geometry and applicable native clearance on whole native
nudge steps. Round upward, not to the nearest step: rounding down can overlap copies. Selected native
heights and functional segments retain their family rules; ladder handrails may intentionally overlap
the next segment. Static profiles must cover their applicable native variants, while articulated
families such as Snow Cannons measure the selected geometry. Placement arrows and wire guides do not
enlarge the physical footprint.

Pipeline and Hypertube wall supports retain their full native parent frame. A rounded local interval
must not turn a rotated run back into world-horizontal or world-vertical placement. See the
[controls model](../../Development/Controls-InteractionModel.md#rotatable-conduit-wall-supports).
Spacing never grants a new orientation or bypasses native attachment and placement restrictions.

The shared fixed-height implementation is in `SFScalingFootprint`; selected-height and articulated
families supply their own adapters. Profile data is maintained in `Content/Data/BuildableSizes.csv`.

## Catalog Status

This is the initial durable baseline, not a complete inventory. Issue #490 tracks the follow-up
research needed to complete the pending construction families, reconcile captured built/hologram
evidence, and promote verified contracts into this catalog. Future feature audits should use the
applicable family reference here, while treating entries marked Pending as unfinished research.

## Families

| Family | Reference | Status |
|---|---|---|
| Distributors and junctions | [Distributor Port Topology](../REF_DistributorPortTopology.md) | Active; migration pending |
| Vehicle paths | [Vehicle Paths](VehiclePaths.md) | Active |
| Passthroughs | Pending | Evidence captured |
| Spline buildables | Pending | Evidence captured |
| Conveyor lifts | [Conveyor Lifts](ConveyorLifts.md) | Active |
| Train platforms | Pending | Initial Train Station evidence captured |
| Factory buildings | Pending | Not inventoried |
| Structural buildables | Pending | Not inventoried |
| Power buildables | [Power Connectors](PowerConnectors.md) | Partial: wall outlets and ceiling-light ports; other families pending |
