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

## Repeated blueprint power

`FSFBlueprintPowerService` discovers pole sockets in the blueprint's staged content, including
internally wired poles with spare slots. The native duplicate-to-original connector map supplies
the exact original component name; socket-relative transforms recover its future owner's pivot
without depending on blueprint-world anchoring. The identity is content class, original transform,
and component name, not instance name or nearest preview position. Coincident ambiguous content is
omitted. `SmartFoundations.Power.BlueprintSocketTransforms` covers rotated owners and both faces.

Internal cables plus hidden bridges define connected groups; staging-world circuit IDs are not
assumed initialized. Each grid edge gets at most one cable per internal group. Different groups
remain electrically separate. A deterministic counterpart planner reserves each face's native
remaining slots, including the cost in slots of the blueprint's own cables. Auto uses a 3D
serpentine chain; explicit axis modes retain their grid edges. An overlong edge becomes dormant
instead of choosing a different socket. `SmartFoundations.Power.BlueprintNetworks` covers circuit
isolation, counterpart identity, 3D routing, limits, and recovery after changing spacing.

`SFPowerAutoConnectManager_Blueprints.cpp` shares the exact-wire preview owner and cleanup path
with wall outlets. Both Blueprint Seam Auto-Connect and Power Auto-Connect must be enabled.
The blueprint Construct hook captures exact wires before native construction and materializes
them after all copies, including their internal cables, exist. Resolution inspects only the
newly constructed actors and their native blueprint proxies; it does not search unrelated world
actors. Multiplayer's measured blueprint anchor adjustment also shifts new-owner power endpoints.
The shared authority materializer rechecks native capacity, distance and designer boundaries.
Preview cable cost remains in the parent quote, and blueprint-copy dismantle membership is unchanged.

## Extend and Restore construction boundary

Cable plans retain both owner IDs and connector names. Captured socket positions, native per-port
budgets, and range feed the preview and its cost; authority refreshes these values before accepting
construction. New clone ports use their new capacity, while an existing source port uses its remaining
capacity. Planned cables reserve slots together, including both edges on a middle factory or outlet.
Factory daisy-chain cables belong in this priced plan, not an additional inferred post-build loop.

Server Scaled Extend must preserve the freshly reconstructed first-copy topology, spawn the
additional cells, then merge their topologies onto that base before validation or materialization.
Spawning the children alone leaves their cable previews priced but omits their edges from the
server's capacity check and final wire pass. The local preview already performs this merge.
`SmartFoundations.Extend.Power.ServerMergedPlan` exercises the server entry's merge boundary with
pre-generated cell plans, including upper layers, named outlet faces, overbooking in an additional
cell, and successive requests. It does not simulate native spawning or prove live power flow.

Do not assume native construction charges before calling the hologram's `Construct`. Disassembly of
the Windows client FactoryGame module for Satisfactory 1.2.4 / CL 502094 establishes this sequence:

- `UFGBuildGunStateBuild::Server_ConstructHologram_Implementation` calls
  `ValidatePlacementAndCost` at RVA `0x5CD6E5`, then `CanConstruct` at `0x5CD6F1`, and enters
  `InternalConstructHologram` at `0x5CD866` only on the accepted branch.
- `InternalConstructHologram` calls the hologram's virtual construction entry at RVA `0x5BE9B3`.
  It queries the recursive cost later at `0x5BEF67`, then removes each required resource through
  `GrabItemsFromInventoryAndCentralStorage` at `0x5BEFE2`. It does not perform placement validation
  before that construction call. This evidence is binary inspection, not a gameplay test; the
  corresponding development-source bodies are stubs.

Consequently a safe plan check must run before native construction, not just before resource removal.
The request-scoped Extend validation hook covers the server's validation call; the internal-entry
hook explicitly validates local requests that have no prepared plan. Nested native calls share the
same prepared result, separate roots remain independent, and returning from the request discards
the result. Per-frame aiming must not reconstruct a server commit. Walking intent and native zoop
take precedence over an incidental stale Extend request.

Staged request lookup and consumption belong only to the root hologram. Clone factories share its
instigator and build class, so those keys alone do not establish request ownership. The dedicated
server can also track a remote build gun as `ActiveHologram`; that does not make it a local preview.
Ordinary scaling power wrappers require a locally controlled instigator, the active root, no
Extend/Restore session, and no staged or prepared request. Otherwise an early-returning local wrapper
can bypass the staged consumer and let a child reconstruct its parent's additional copies.
`SmartFoundations.Net.SpecConstructionOwnership` covers these dispatch guards and same-class child
rejection. It does not execute native construction; the per-operation `Extend construction` log
compares prepared and built factory counts for packaged acceptance.

`wire_cost` children are tagged `SF_ExtendWirePlan` and temporarily removed from the native child
array by `FSFExtendWirePreviewScope`. The exact endpoint plan is their sole materializer. Leaving
these previews in that array invokes the legacy `ASFWireHologram::Construct` raw-wire path as well,
creating an unconnected extra actor. The scope retains the preview actors and restores their order
and geometry before the build gun queries cost after construction. It does not intercept untagged
wire contracts or remove non-wire children. `PreviewConstructionScope` tests this exclusion,
nested scope behavior, and restoration without invoking native wire construction.

`SmartFoundations.Extend.Commit.RequestIsolation` verifies scope lifetime and root isolation;
`CostAgreement` verifies normalized item costs, including duplicate rows, negative quantities, and
wide aggregation. Neither test executes the native build-gun bodies. Packaged single-player and
multiplayer acceptance, inventory changes, and legacy preview-port availability remain separate
verification requirements.

## Blueprint Designer and cleanup

A wire cannot join different designers or join a designer to the outside world. Preview planning
must reject such pairs, and direct construction must stamp designer ownership before BeginPlay.
`SFWireDesigner::SpawnWireForEndpoints` handles that sequence and contained-list registration.
Failed partial connections must be dismantled, not merely destroyed, to remove native endpoint
wire references. This is a save-integrity boundary, not optional visual bookkeeping.
