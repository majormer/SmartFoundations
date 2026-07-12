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
| Train platforms | Pending | Initial Train Station evidence captured |
| Factory buildings | Pending | Not inventoried |
| Structural buildables | Pending | Not inventoried |
| Power buildables | Pending | Not inventoried |
