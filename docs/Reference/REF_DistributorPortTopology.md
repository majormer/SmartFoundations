---
title: Distributor Port Topology Reference
type: REF
date: 2026-07-11
status: Active
category: Reference
tags: [autoconnect, extend, restore, distributors, junctions, connectors, topology]
related: [../Features/AutoConnect/IMPL_AutoConnect_CurrentFlow.md, ../Features/Extend/IMPL_Extend_CurrentFlow.md]
---

# Distributor Port Topology Reference

## Purpose

This reference defines the stable, actor-local port topology of the vanilla belt distributors and pipe junctions used by Smart Auto-Connect, Extend, multiplayer construction, and Restore.

Port identity is the component's stable asset name, such as `Input1` or `Connection2`. The index returned by `GetComponents()` is not a topology identifier because Unreal component enumeration order is not guaranteed.

Actor rotation changes each port's world transform but does not change its name or actor-local role.

## Coordinate Convention

All positions below are relative to the distributor actor's root transform:

- `-X`: back
- `+X`: front
- `+Y`: one lateral side
- `-Y`: the other lateral side

World-facing labels are intentionally omitted. A rotated distributor retains the same local map.

## Verified Port Maps

| Distributor family | `-X` | `+X` | `+Y` | `-Y` |
|---|---|---|---|---|
| Pipeline Junction (Cross) | `Connection0` | `Connection1` | `Connection2` | `Connection3` |
| Pipeline T-Junction | `Connection0` | `Connection1` | **missing** | `Connection2` |
| Conveyor Splitter | `Input1` | `Output1` | `Output2` | `Output3` |
| Smart Splitter | `Input1` | `Output1` | `Output2` | `Output3` |
| Programmable Splitter | `Input1` | `Output1` | `Output2` | `Output3` |
| Conveyor Merger | `Input1` | `Output1` | `Input2` | `Input3` |
| Priority Merger | `Input1` | `Output1` | `Input2` | `Input3` |

Every listed connector is 100 cm from the actor origin on its listed local axis.

### Important T-Junction Difference

The T-junction is not the Cross junction with an identically numbered port removed. On the Cross, local `-Y` is `Connection3`; on the T-junction, local `-Y` is `Connection2`. The T-junction has no local `+Y` connector.

Code must therefore resolve ports through a class-specific named-port map. It must not infer the T-junction map by truncating or filtering the Cross map.

## Built and Hologram Parity

### Runtime Class Matrix

| Family | Built class | Hologram class | Hologram inspected |
|---|---|---|---|
| Pipeline Junction (Cross) | `Build_PipelineJunction_Cross_C` | `FGPipelineJunctionHologram` | Yes |
| Pipeline T-Junction | `Build_PipelineJunction_T_C` | `FGPipelineJunctionHologram` | Yes |
| Conveyor Splitter | `Build_ConveyorAttachmentSplitter_C` | `Holo_ConveyorAttachment_C` | Yes |
| Smart Splitter | `Build_ConveyorAttachmentSplitterSmart_C` | `Holo_ConveyorAttachment_C` | Yes |
| Programmable Splitter | `Build_ConveyorAttachmentSplitterProgrammable_C` | `Holo_ConveyorAttachment_C` | Yes |
| Conveyor Merger | `Build_ConveyorAttachmentMerger_C` | `Holo_ConveyorAttachment_C` | Yes |
| Priority Merger | `Build_ConveyorAttachmentMergerPriority_C` | `Holo_ConveyorAttachment_C` | Yes |
| Pipeline Floor Hole | `Build_FoundationPassthrough_Pipe_C` | `FGPassthroughHologram` | Yes |
| Conveyor Lift Floor Hole | `Build_FoundationPassthrough_Lift_C` | `FGPassthroughHologram` | Yes |
| Hypertube Floor Hole | `Build_FoundationPassthrough_Hypertube_C` | `FGPassthroughPipeHyperHologram` | Yes |
| Pipeline Wall Hole | `Build_PipelineSupportWallHole_C` | `Holo_PipelineSupportWallHole_C` | Yes |
| Hypertube Wall Hole | `Build_HyperTubeWallHole_C` | `Holo_PipelineSupportWallHole_C` | Yes |
| Conveyor Wall Hole | `Build_ConveyorWallHole_C` | `Holo_ConveyorWallHole_C` | Yes |
| Hypertube Junction | `Build_HyperTubeJunction_C` | `Holo_HyperTubeJunction_C` | Yes |
| Hypertube Branch | `Build_HypertubeTJunction_C` | `Holo_HypertubeTJunction_C` | Yes |

The following built actors were inspected at runtime with stable component names and actor-relative locations:

- `Build_PipelineJunction_Cross_C`
- `Build_PipelineJunction_T_C`
- `Build_ConveyorAttachmentSplitter_C`
- `Build_ConveyorAttachmentSplitterSmart_C`
- `Build_ConveyorAttachmentSplitterProgrammable_C`
- `Build_ConveyorAttachmentMerger_C`
- `Build_ConveyorAttachmentMergerPriority_C`

Active build-gun holograms were then inspected for the four fundamental layouts:

- Pipeline Junction (Cross)
- Pipeline T-Junction
- Conveyor Splitter
- Conveyor Merger

For each hologram, connector names and actor-relative locations exactly matched its built actor. Both pipe holograms were rotated approximately 90 degrees in world space during inspection, confirming that rotation changes world positions but not the local named-port topology.

This parity permits one class-specific topology contract to be used during preview, source capture, construction, and post-build wiring.

## Passthroughs, Wall Holes, and Hypertube Junctions

These attachment classes are adjacent to distributor topology because Auto-Connect and Extend may capture or route through them. They do not all follow the four-face distributor model.

### Floor Holes

| Build class | Top | Bottom | Built-actor representation |
|---|---|---|---|
| `Build_FoundationPassthrough_Hypertube_C` | `Connection0`, normal `+Z`; location follows snapped depth | `Connection1`, normal `-Z`; location follows snapped depth | Owns two `FGPipeConnectionComponentHyper` components |
| `Build_FoundationPassthrough_Pipe_C` | `TopSnappedConnection` semantic role at center Z `+ thickness / 2` | `BottomSnappedConnection` semantic role at center Z `- thickness / 2` | Owns no pipe connection components after construction; stores references to the snapped external connections |
| `Build_FoundationPassthrough_Lift_C` | `TopSnappedConnection` semantic role at center Z `+ thickness / 2` | `BottomSnappedConnection` semantic role at center Z `- thickness / 2` | Owns no factory connection components after construction; stores references to the snapped external connections |

Floor-hole height is dynamic. `mSnappedBuildingThickness` reflects the snapped foundation/floor depth, and the logical top/bottom faces move with half that thickness. The inspected 4 m instances had visible endpoint meshes at local Z `+197` and `-197` cm, but those values are evidence for those instances only and must never be hardcoded as the topology contract. Their actual top/bottom connection identities belong to the snapped pipe or lift, not to named components on the built passthrough actor.

### Wall Holes

The built wall-hole actors expose one snap-only attachment point rather than two through-connections:

| Build class | Snap component | Component class | Observed role |
|---|---|---|---|
| `Build_PipelineSupportWallHole_C` | `SnapOnly0` | `FGPipeConnectionComponent` | Pipeline support snap |
| `Build_HyperTubeWallHole_C` | `PipeHyperSupportSnap` | `FGPipeConnectionComponentHyper` | Hypertube support snap |
| `Build_ConveyorWallHole_C` | `SnapOnly` | `FGFactoryConnectionComponent` | Conveyor support snap |

These are placement/support anchors. Do not model them as a two-sided passthrough without separate evidence from the connected spline buildable or hologram.

### Hypertube Junctions

Hypertube junctions use stable named ports but have non-cardinal local layouts:

| Build class | Port | Actor-relative location (cm) |
|---|---|---:|
| `Build_HyperTubeJunction_C` | `Connection0` | `(-206.962, -121.340, 0)` |
| `Build_HyperTubeJunction_C` | `Connection1` | `(206.962, -121.340, 0)` |
| `Build_HyperTubeJunction_C` | `Connection2` | `(0, 240, 0)` |
| `Build_HypertubeTJunction_C` | `Connection0` | `(229.783, 0, 0)` |
| `Build_HypertubeTJunction_C` | `Connection1` | `(-251.980, 0, 0)` |
| `Build_HypertubeTJunction_C` | `Connection2` | `(-116.132, 201.097, 0)` |

These values were captured from built actors. Hologram parity remains unverified and must be recorded before these layouts are used as a preview-time topology contract.

### Attachment Evidence Status

| Family | Built actor captured | Hologram parity captured |
|---|---|---|
| Hypertube Floor Hole | Yes | Yes; named upper/lower connectors match and locations follow snapped depth |
| Pipeline Floor Hole | Yes, passthrough roles and physical anchors | Yes; no owned connector components on either representation |
| Conveyor Lift Floor Hole | Yes, passthrough roles and physical anchors | Yes; no owned connector components on either representation |
| Pipeline Wall Hole | Yes | Pending |
| Hypertube Wall Hole | Yes | Pending |
| Conveyor Wall Hole | Yes | Pending |
| Hypertube Junction | Yes | Pending |
| Hypertube Branch | Yes | Pending |

### Current Pipeline Floor-Hole Auto-Connect

Pipeline Floor Hole Auto-Connect uses semantic top/bottom endpoints rather than named connector components:

1. `IsPassthroughPipeHologram` accepts only an `AFGPassthroughHologram` whose build class contains `Passthrough_Pipe`. Conveyor Lift and Hypertube floor holes are excluded.
2. Processing is debounced by 0.10 seconds and evaluates the parent plus scaled pipe-floor-hole children.
3. The hologram's dynamic `mSnappedBuildingThickness` is read by reflection. Top and bottom endpoints are synthesized at the actor center plus or minus half that thickness. The inspected 4 m sample therefore used logical Z offsets of plus or minus 200 cm; its visible endpoint meshes were inset to plus or minus 197 cm. Neither value applies to floor holes snapped through a different depth.
4. Nearby unconnected factory pipe connectors are searched within 25 m. Existing pipes, supports, storage, junctions, passthroughs, and non-factory actors are excluded.
5. Connector height relative to the hole center chooses the top or bottom face. Three-dimensional distance from that face ranks candidates.
6. The implementation selects one nearest connector overall and stores one pipe child per floor hole. It cannot currently preview or build simultaneous top and bottom Auto-Connect pipes for the same hole.
7. The pipe child starts at the synthesized face position with an up or down normal. Its building-side connection is handled by vanilla pipe construction.
8. After construction, a null `PipeAutoConnectConn0` registry value identifies the floor-hole branch. Smart finds the nearest built `AFGBuildablePassthrough` within 1 m in XY, determines top or bottom from the pipe endpoint Z, and calls `SetTopSnappedConnection` or `SetBottomSnappedConnection` with the built pipe connection.
9. Multiplayer conduit replay preserves only the floor-hole branch discriminator and repeats the post-build proximity registration on authority.

This path has three topology risks relevant to future work:

- one child slot collapses two valid faces into one winner;
- post-build passthrough lookup is proximity-based and does not carry the source floor-hole identity through the construction plan; and
- the lookup considers every `AFGBuildablePassthrough`, not only Pipeline Floor Holes.

Hypertube Floor Holes do not use this path. They own explicit `Connection0`/`Connection1` components and belong to Hypertube transport topology rather than Pipeline Auto-Connect.

## Manifold Resolution Contract

For a recognized distributor class, Smart should resolve a manifold from:

```text
Distributor class + named factory-connected port
    -> opposite factory-side port, when applicable
    -> previous manifold-lane port
    -> next manifold-lane port
    -> valid or invalid orientation
```

The existing connection from the source factory establishes the factory-side named port. The two perpendicular named ports form the manifold lane. Extend direction determines which is previous and which is next; it does not redefine connector identity.

### Invalid T-Junction Branches

If a selected T-junction orientation requires the missing local `+Y` port, the manifold branch is invalid. Smart must discard the whole branch before creating previews, costs, clone topology, or wiring data.

Smart must not:

- substitute the opposite factory-side port;
- choose the nearest remaining connector;
- treat a perpendicular connector as an opposite connector; or
- allow preview-time and build-time wiring to choose different fallbacks.

## Ownership and Persistence

The named-port resolver is a shared topology contract. Changes must be coordinated across:

- Pipe Auto-Connect discovery, preview, validation, and construction;
- Extend source capture, clone planning, lane generation, and post-build wiring;
- multiplayer commit capture and server reconstruction; and
- Extend/Restore serialization, import/export, schema migration, and replay.

If persisted topology begins storing explicit factory/previous/next port names, version the schema deliberately and validate older saved modules before migration. Geometry-derived legacy fields must not be reinterpreted as named-port identities without validation.

## Unknown or Modded Distributors

The verified maps above apply to the listed vanilla classes. Unknown modded distributor classes may use a separately validated geometric fallback, but fallback behavior must fail closed when a continuous lane cannot be proven. It must never silently invent an opposite or missing lane port.
