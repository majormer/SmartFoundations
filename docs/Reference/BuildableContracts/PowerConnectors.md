---
title: Power Connector Construction Contracts
type: REF
status: Active
category: Reference
tags: [power, wall-outlets, lights, construction]
---

# Power Connector Construction Contracts

## Native capacity and identity

External cable capacity belongs to each `UFGCircuitConnectionComponent`, not its actor or
electrical circuit. Use `GetMaxNumConnections`, `GetNumConnections`, and `GetNumFreeConnections`
at evaluation and construction time. Native capacity incorporates daisy-chain research; factory
buildings and lights must not be assumed to have two connections before that research. Hidden
links are separate from external wires. `IsConnected()` means circuit membership, not wire count.
The native declarations in `FGCircuitConnectionComponent.h` distinguish these contracts.

Power Auto-Connect selects unconnected consumers, rather than filling every spare socket on an
already-connected building. Filtering must occur before assigning pole capacity: otherwise a
connected building can consume an assignment, fail preview creation, and starve another consumer.
`SFPowerBuildingTarget` shares eligibility and connector-range rules between preview and build.

## Wall outlets

The supported concrete built classes are `Build_PowerPoleWall_C`,
`Build_PowerPoleWall_Mk2_C`, `Build_PowerPoleWall_Mk3_C`, and their corresponding
`Build_PowerPoleWallDouble` variants. Placement is owned by `AFGPowerPoleWallHologram`.
Its `GetSnapConnection()` identifies the face used for the grid backbone.

For the double-sided Mk.1, live built evidence on Satisfactory 1.2.4 / CL 502094 shows:

| Component | Actor-local location (cm) | External cables observed | Native maximum |
|---|---|---|---|
| `PowerConnection1` | `(70, 0, 0)` | 2 | 4 |
| `PowerConnection2` | `(-70, 0, 0)` | 3 | 4 |

Both ports share a circuit and a hidden bridge. Five external cables on this actor are valid:
neither face exceeds four. The two port rotations are identical, so rotation cannot identify
the face. Component name and owner identity must survive preview-to-build materialization.
The opposite face is not spare capacity for an already-full face.

`SFPowerAutoConnectManager_WallOutlets.cpp` plans a single serpentine grid chain in Auto mode,
including Z transitions; X/Y/X+Y retain explicit axis edges. Only one face carries the backbone.
Both faces may serve nearby unconnected consumers. Each planned endpoint consumes its own port's
budget; building reservations apply separately to each source face. Wire insertion, upgrades, and
native zoop stand down. Ordinary ground poles retain their existing planner.

Native wall post-placement moves direct children to the snap port. `SFWallOutletPlacement`
preserves Smart grid and exact-wire transforms across this callback without suppressing native
placement checks. The automation test `SmartFoundations.Power.WallWirePlacement` covers this
boundary; otherwise the outlet copies and their wire geometry collapse at the parent.

Exact power plans carry existing actor references or new-owner class/position plus component
name. Authority resolves only the committed new actors, refuses ambiguous or missing endpoints,
rechecks native capacity and wire length, and uses the designer-aware wire construction path.
It never selects a nearest face as fallback. Standalone and multiplayer use the same materializer;
standalone temporarily excludes the already-priced wire previews from vanilla child construction.
Cable cost remains part of the parent quote. Runtime acceptance across all tiers and dedicated
multiplayer is separate from the observed Mk.1 native topology.

## Ceiling lights

`Build_CeilingLight_C` derives from `AFGBuildableLightSource`, not `AFGBuildableFactory`.
Its native `UFGPowerConnectionComponent` named `PowerConnection` was measured at actor-local
`(0, -600, -60)` cm on CL 502094. The built light exposed two slots with research unlocked;
its class default object exposed no instantiated connector. Inspect the live component rather
than inferring ports from the class default object or restricting consumers to factory subclasses.

In a live single-player pole-placement check, the preview selected this named component, included
one cable in the parent cost, and the placed Mk.1 pole connected successfully: the light then
reported one external wire, 1/2 slots used, and live power. This establishes that the basic case
works; it does not establish range-boundary or crowded-candidate behavior.

Range must be measured between actual sockets both before and after construction. A Mk.1 pole
socket is 700 cm above its pivot; comparing actor origins on commit can disagree with a valid
preview, especially for the light's six-metre horizontal offset. The regression test
`SmartFoundations.Power.BuildingTargets` covers eligible/free ports and an offset socket inside
the configured range whose actor origin would be incorrectly rejected.

## Blueprint Designer and cleanup

A wire cannot join different designers or join a designer to the outside world. Preview planning
must reject such pairs, and direct construction must stamp designer ownership before BeginPlay.
`SFWireDesigner::SpawnWireForEndpoints` handles that sequence and contained-list registration.
Failed partial connections must be dismantled, not merely destroyed, to remove native endpoint
wire references. This is a save-integrity boundary, not optional visual bookkeeping.
