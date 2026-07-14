---
title: Vehicle Path Construction Contracts
type: REF
status: Active
category: Reference
tags: [vehicles, paths, networks, splines, replication, optimization]
---

# Vehicle Path Construction Contracts

## Construction Model

Vehicle paths are subsystem-managed directed networks, not ordinary isolated buildables. Their
authoritative model spans:

- `AFGVehicleSubsystem`: network ownership, registration, topology rebuilds, and replication.
- `UFGVehiclePathNetwork`: compact node and directed-segment topology.
- `AFGVehiclePathNode`: stable GUID and arriving/leaving segment relationships.
- `AFGVehiclePathSegment`: spline, endpoint nodes, validation, traversability, and reservations.

Path visualization uses lightweight instances. A visibility trace may therefore hit the shared
`AbstractInstanceManager` instead of the logical path node or segment. Inspection and Smart behavior
must resolve paths through `AFGVehicleSubsystem`, not through the rendered marker actor.

## Concrete Profiles

| Profile | Runtime segment class | Allowed presets | Validation entries | Observed mask |
|---|---|---|---:|---:|
| Universal Vehicle Path | `Build_VehiclePath_Universal_C` | Tractor, Truck, Explorer | 3 | `7` |
| Tractor Vehicle Path | `Build_VehiclePath_Tractor_C` | Tractor | 1 | `1` |
| Truck Vehicle Path | `Build_VehiclePath_Truck_C` | Truck | 1 | `1` |
| Explorer Vehicle Path | `Build_VehiclePath_Explorer_C` | Explorer | 1 | `1` |
| Factory Cart Vehicle Path | `Build_VehiclePath_FactoryCart_C` | Factory Cart | Pending built capture | Pending built capture |

Both profiles used `Build_VehiclePathNode_Default_C` endpoint nodes. The segment class and allowed
presets are therefore concrete-profile identity; the node class and directed-network model are shared.

### Universal Vehicle Path

The observed network was `node[0] -> segment[0] -> node[1]`:

- Network ID: `0` (volatile; not a durable identity).
- Node identity: stable GUID plus world location.
- Segment length: approximately `2618.998 cm`.
- Spline points: `12`, with world/local positions, tangents, directions, and up vectors.
- Traversability mask: `7`.
- Allowed presets: Tractor, Truck, and Explorer.
- Validation records: `3`.
- Path blocks: `1`.
- Collision extent: `120 cm`.
- Segment was locally significant, net significant, visualized, and not a junction block.

Both endpoint actors were replicated during inspection. The source node had one leaving connection;
the destination had one arriving connection. Both were trivial nodes.

### Tractor Vehicle Path

The observed Tractor-only network also contained two nodes and one directed segment:

- Segment class: `Build_VehiclePath_Tractor_C`.
- Segment length: approximately `2512.728 cm`.
- Spline points: `12`.
- Traversability mask: `1`.
- Allowed presets: Tractor only.
- Validation records and virtual lengths: one each.
- Collision extent: `120 cm`.
- Segment was locally significant, net significant, visualized, and not a junction block.

The observed segment had three path blocks, versus one on the captured Universal segment. Path-block
count depends on geometry and overlap/reservation partitioning and is not established as a class-level
difference by these captures.

### Truck Vehicle Path

The observed Truck-only network contained two default nodes and one directed segment:

- Segment class: `Build_VehiclePath_Truck_C`.
- Segment length: approximately `2412.219 cm`.
- Spline points: `11`.
- Traversability mask: `1`.
- Allowed presets: Truck only.
- Validation records and virtual lengths: one each.
- Collision extent: `120 cm`.
- Segment was locally significant, net significant, visualized, and not a junction block.

The mask is indexed against each network's local preset ordering. It is not a global vehicle identity:
both the Tractor-only and Truck-only captures use mask `1`, while the Universal capture uses mask `7`
for its three locally ordered presets. Consumers must interpret the mask with the network's preset order.

### Explorer Vehicle Path

The observed Explorer-only network contained two default nodes and one directed segment:

- Segment class: `Build_VehiclePath_Explorer_C`.
- Segment length: approximately `2111.728 cm`.
- Spline points: `10`.
- Traversability mask: `1` in the network-local preset order.
- Allowed presets: Explorer only.
- Validation records and virtual lengths: one each.
- Path blocks: `1`.
- Collision extent: `120 cm`.
- Segment was locally significant, net significant, visualized, and not a junction block.

### Factory Cart Vehicle Path

The Factory Cart profile was captured first as a valid Default-mode hologram:

- Hologram class: `Holo_VehiclePath_FactoryCart_C`.
- Recipe: `Recipe_VehiclePath_FactoryCart_C`.
- Built class: `Build_VehiclePath_FactoryCart_C`.
- Validation preset: `VehiclePathPreset_FactoryCart`.
- Supported routing: Default and Straight; Default was current through the public mode API.
- Live `VehiclePath` and serialized `mSplineData`: 8 points over approximately `1625.193 cm`.
- Temporary routing spline: 2 points over approximately `1625.772 cm`.
- Spline quantization length: `250 cm`.
- Endpoint snap distance: `800 cm`.
- No snapped path nodes or segments; segment reversal and forced-first-point state were false.

Built network topology, traversability mask, validation count, and path-block state remain to be
captured after construction.

## Identity And Topology Rules

- Use node GUIDs as stable node identity. Network IDs can change when topology is rebuilt.
- Segment direction is explicit through `FromNodeIndex` and `ToNodeIndex`; do not infer it from spline
  point order alone.
- Interpret traversability masks only with the owning network's preset ordering. Bit positions are not
  globally assigned to Tractor, Truck, or Explorer.
- Preserve arriving and leaving node relationships when cloning or reconstructing a path.
- Compact network data remains available when individual actors are not replicated to a client.
- Server actor state is required for mutation, validation, reservation, and lifecycle operations.

## Hologram Routing Modes

Vehicle profile and routing mode are independent dimensions. A captured Explorer hologram retained
the same `Recipe_VehiclePath_Explorer_C` recipe and `Build_VehiclePath_Explorer_C` built class while
the player selected Straight routing.

Shared `AFGVehiclePathSegmentHologram` routing state includes:

- Default mode: `BuildMode_Default_C`.
- Optional Straight mode: `BuildMode_Straight_C` through `mBuildModeStraight`.
- `RouteHologramSpline()` as the route-generation step.
- Serialized/replicated `mSplineData` as the construction geometry.
- Private `mStraightMode` state selecting Straight behavior.
- Start/end locations and entry/exit directions.
- Optional snapped path nodes and segments, including segment slice positions.
- Segment reversal, custom end rotation, and multi-step placement state.

The inspected Straight Explorer preview was valid, had no snapped node/segment endpoints, and carried
17 serialized spline points matching the live `VehiclePath` spline component. It had no child
holograms.

A subsequent Straight Explorer preview was confirmed by the live HUD and runtime inspection:

- Live `VehiclePath` and serialized `mSplineData`: 14 points.
- Routed path length: approximately `3225.115 cm`.
- Temporary routing spline: 5 points and approximately `3227.754 cm`.
- `mIsSegmentReversed = false` and `mForceSnapFirstPoint = false`.

`mStraightMode` remained false during this confirmed Straight preview. It is serialized routing state,
not an authoritative indicator of the build gun's current mode. Current mode must be queried through
`AFGHologram::IsCurrentBuildMode` against the supported descriptors.

The build-mode selection screenshot showed Default and Straight as the complete route-mode choices for
this hologram. Straight mode changes generated geometry within the same concrete vehicle profile; it
does not select a different recipe or built segment class. The two Straight captures had 17 and 14
points respectively, confirming that point counts are geometry-dependent rather than fixed per mode.

A fresh post-relaunch runtime capture subsequently verified the public build-mode contract on a
Straight preview:

- Supported modes: `BuildMode_Default_C` and `BuildMode_Straight_C`.
- `BuildMode_Default_C.isCurrent = false`.
- `BuildMode_Straight_C.isCurrent = true`.
- `currentBuildMode = BuildMode_Straight_C`.
- Live `VehiclePath` and serialized `mSplineData`: 18 points over approximately `4025.961 cm`.
- Temporary routing spline: 5 points over approximately `4027.753 cm`.
- No snapped path nodes or segments; segment reversal and forced-first-point state were false.

The hologram did not survive the required mod reload; this is an independent sample rather than a
before/after comparison of one actor. Cross-launch comparisons are limited to stable class and
lifecycle contracts unless the geometry is deliberately reconstructed.

Smart support and optimization must preserve both axes:

- **Vehicle profile:** Universal, Tractor, Truck, or Explorer.
- **Routing mode:** Default or Straight.

Do not infer routing mode from the built class. Preserve the generated spline and the hologram mode
state through preview updates and construction-message serialization.

## Length And Auto-Connect Eligibility

Actual routed length is part of the vanilla-valid construction contract and should be considered by
future vehicle-path Auto-Connect. A candidate connection must be rejected if vanilla routing or path
validation would reject the resulting segment; Smart must not create a path merely because endpoints
are geometrically reachable.

Keep these concerns separate:

- configured vanilla minimum and maximum path lengths;
- actual routed spline length, not straight-line endpoint distance;
- endpoint snap distance and node-overlap radius;
- route-mode shape validation;
- per-preset traversability validation;
- any user-configured Smart Auto-Connect search threshold.

No Smart threshold is established by these captures. Runtime inspection exposes the generic path
scalars needed to inspect the configured vanilla limits directly.

## Optimization Constraints

An optimization may reduce visualization or repeated spline work, but it must not bypass the vehicle
subsystem's topology, validation, traversability, or replication paths. In particular, preserve:

- stable node GUIDs;
- directed endpoint relationships;
- spline geometry and length;
- allowed vehicle presets and per-preset validation;
- network topology invalidation/rebuild behavior;
- path-block intersection and reservation state;
- docking-station associations;
- server/client behavior when path actors are not locally replicated.

## Smart Compatibility

Smart does not currently construct vehicle paths. Future support should treat an entire selected path
or connected subgraph as the operation boundary. Scaling or cloning one rendered marker independently
would not produce a valid vanilla-equivalent path.

## Evidence

- Game: Satisfactory 1.2, CL 495413-era development environment.
- Evidence source: runtime subsystem, actor, spline, and hologram inspection.
- Verified: network, node actors, segment actor, directed topology, spline, presets, significance,
  visualization, validation count, path blocks, and traversability data for Universal and Tractor
  profiles plus the Truck and Explorer profiles.
- Hologram parity: not applicable to this recorded path; path creation/edit holograms require a
  separate capture. Explorer Straight hologram identity, routing descriptors, and spline parity have
  been captured. A fresh runtime capture directly verifies Straight through the public current-mode
  API. A Default preview remains to be captured as a separate sample.
