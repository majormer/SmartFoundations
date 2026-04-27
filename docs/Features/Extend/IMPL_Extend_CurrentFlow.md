---
title: Smart Extend Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [extend, scaled_extend, manifold, topology, transforms, wiring]
related: [../Scaling/IMPL_Scaling_CurrentFlow.md, ../Transforms/IMPL_Transforms_CurrentFlow.md, ../AutoConnect/IMPL_AutoConnect_CurrentFlow.md]
---

# Smart Extend Current Flow

## Purpose

Extend clones an existing layout from a source building into a new adjacent placement. Scaled Extend layers the Scaling grid on top of Extend so multiple clones and rows can be generated from the same captured topology.

## Current Status

Extend and Scaled Extend are active. The current implementation is JSON/topology driven: it captures a source topology, generates clone topologies with offsets, spawns child holograms for preview/build, then performs post-build wiring and chain/pipe stabilization.

## Primary Code Files

| File | Role |
|------|------|
| `Source/SmartFoundations/Public/Features/Extend/SFExtendService.h` | Main Extend orchestrator and Scaled Extend state |
| `Source/SmartFoundations/Private/Features/Extend/SFExtendService.cpp` | Activation, refresh, build registration, wiring, validation |
| `Source/SmartFoundations/Public/Features/Extend/SFExtendDetectionService.h` | Source-target validation and direction handling |
| `Source/SmartFoundations/Private/Features/Extend/SFExtendDetectionService.cpp` | Valid target detection and direction availability |
| `Source/SmartFoundations/Public/Features/Extend/SFExtendTopologyService.h` | Captured topology model and walking service |
| `Source/SmartFoundations/Private/Features/Extend/SFExtendTopologyService.cpp` | Belt, lift, pipe, distributor, and junction topology walking |
| `Source/SmartFoundations/Public/Features/Extend/SFExtendHologramService.h` | Preview/child hologram management |
| `Source/SmartFoundations/Private/Features/Extend/SFExtendHologramService.cpp` | Child spawning, tracking, and refresh |
| `Source/SmartFoundations/Public/Features/Extend/SFManifoldJSON.h` | Serializable source/clone topology schema |
| `Source/SmartFoundations/Private/Features/Extend/SFManifoldJSON.cpp` | Topology capture, clone generation, child spawning, transform storage |
| `Source/SmartFoundations/Public/Features/Extend/SFWiringManifest.h` | Post-build wiring manifest |
| `Source/SmartFoundations/Private/Features/Extend/SFWiringManifest.cpp` | Belt, pipe, and power connection execution after build |

## Runtime Flow

1. `USFSubsystem::Tick` line-traces from the player and calls `USFExtendService::TryExtendFromBuilding` when the player is aiming at a candidate source.
2. `SFExtendDetectionService` validates that the aimed building can be extended by the currently held hologram.
3. `SFExtendTopologyService` walks the connected logistics and pipe topology around the source.
4. `FSFSourceTopology` captures the source topology into the JSON schema.
5. `FSFCloneTopology::FromSource` generates clone positions from source data and the active offset.
6. `SFExtendHologramService` spawns child holograms and refreshes them while Extend stays active.
7. Vanilla build constructs the parent and child holograms.
8. Built actors register back into `USFExtendService` by clone id.
9. `FSFWiringManifest` reconnects belts, lifts, pipes, and power after components exist.
10. Chain actors and pipe networks are stabilized after the topology is built.

## Basic Extend

Basic Extend clones one source layout in the selected direction. Direction and placement are target-relative rather than world-global, so the clone follows the source building orientation.

Important behavior:

- The held hologram must match the intended source class/family.
- The source topology can include connected distributors, belts, lifts, pipes, junctions, and supported attachments.
- Recipes and configured distributor behavior are copied where the code has explicit support.
- Post-build wiring is deferred because many vanilla components are not ready at actor-spawn time.

## Scaled Extend

Scaled Extend is active when Extend mode is active and the clone count or row count is greater than one. `USFExtendService::OnScaledExtendStateChanged` tracks the grid-derived state, and `IsScaledExtendActive` reports whether the enhanced mode is currently in use.

Scaled Extend uses the same transform state as Scaling for clone offsets. Spacing, Steps, Stagger, and Z Rotation should be documented as implemented transform inputs. X/Y rotation axes should not be described as active because the current transform pipeline only implements Z rotation.

See [../Transforms/IMPL_Transforms_CurrentFlow.md](../Transforms/IMPL_Transforms_CurrentFlow.md).

## Wiring and Stabilization

Extend does not rely on every preview-time snapped connection surviving vanilla construction. The post-build wiring manifest is the authoritative repair step for final connections.

| System | Post-build handling |
|--------|---------------------|
| Belts and lifts | Reconnect via factory connection components and stabilize conveyor chain actors. |
| Pipes | Reconnect via pipe connection components and rebuild pipe networks. |
| Power | Create cloned wires where source topology and capacity allow it. |

## Important Caveats

- The old deferred-build queue design is not the current implementation; the active path is child holograms plus JSON topology plus post-build wiring.
- Wiring remains tightly coupled to built-actor registration and clone ids.
- Large or dense belt clones share the same chain actor fragility as SmartUpgrade and should use `USFChainActorService` rather than direct bucket-level conveyor APIs.
- Floor holes, pumps, valves, and passthrough behavior has evolved since older plans; trust the current topology and JSON code before older planning docs.

## Archived Inputs

The previous Extend current-flow doc, audits, floor-hole plan, historical Extend research, power Extend notes, and logistics connection notes were moved to `docs/Archive/2026/features-consolidation/superseded/Extend/`.
