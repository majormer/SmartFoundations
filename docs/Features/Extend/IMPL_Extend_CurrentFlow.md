---
title: Smart Extend Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [extend, scaled_extend, manifold, topology, transforms, wiring]
related: [../Scaling/IMPL_Scaling_CurrentFlow.md, ../Transforms/IMPL_Transforms_CurrentFlow.md, ../AutoConnect/IMPL_AutoConnect_CurrentFlow.md, ../../Reference/REF_DistributorPortTopology.md]
---

# Smart Extend Current Flow

## Purpose

Extend clones an existing layout from a source building into a new adjacent placement. Scaled Extend layers the Scaling grid on top of Extend so multiple clones and rows can be generated from the same captured topology.

## Current Status

Extend and Scaled Extend are active. The current implementation is clone-topology driven: it captures a source topology, generates clone topologies with offsets, spawns child holograms for preview/build, then performs post-build wiring and chain/pipe stabilization.

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
| `Source/SmartFoundations/Public/Features/Extend/SFExtendCloneTopology.h` | Source/clone topology schema |
| `Source/SmartFoundations/Private/Features/Extend/SFExtendCloneTopology.cpp` | Topology capture, clone generation, transform storage, and preview-time connection wiring |
| `Source/SmartFoundations/Private/Features/Extend/SFExtendCloneSpawner.cpp` | Child hologram spawning from clone topology entries |
| `Source/SmartFoundations/Public/Features/Extend/SFWiringManifest.h` | Post-build wiring manifest |
| `Source/SmartFoundations/Private/Features/Extend/SFWiringManifest.cpp` | Belt, pipe, and power connection execution after build |

## Runtime Flow

1. `USFSubsystem::Tick` line-traces from the player and calls `USFExtendService::TryExtendFromBuilding` when the player is aiming at a candidate source.
2. `SFExtendDetectionService` validates that the aimed building can be extended by the currently held hologram.
3. `SFExtendTopologyService` walks the connected logistics and pipe topology around the source.
4. `FSFSourceTopology` captures the source topology into clone-topology structs.
5. `FSFCloneTopology::FromSource` generates clone positions from source data and the active offset.
6. `SFExtendHologramService` spawns child holograms and refreshes them while Extend stays active.
7. A construction-request-scoped check validates the power plan, reconstructing the authoritative multiplayer commit once where applicable. Aiming checks do not reconstruct commits.
8. Vanilla builds the parent and constructible children. Priced wire previews are temporarily excluded from this child loop; their exact endpoint plan creates the real wires afterward.
9. Built actors register back into `USFExtendService` by clone id. `FSFWiringManifest` reconnects belts and pipes; the explicit power plan connects named native sockets after owners exist.
10. Chain actors and pipe networks are stabilized after the topology is built. Wire previews are restored before the native build gun's post-construction cost query.

## Basic Extend

Basic Extend clones one source layout in the selected direction. Direction and placement are target-relative rather than world-global, so the clone follows the source building orientation.

Important behavior:

- The held hologram must match the intended source class/family.
- The source topology can include connected distributors, belts, lifts, pipes, junctions, and supported attachments.
- Recipes and configured distributor behavior are copied where the code has explicit support.
- Post-build wiring is deferred because many vanilla components are not ready at actor-spawn time.

Copied recipe, Power Shard, and Somersloop settings use the shared deferred,
authority-only application and per-machine inventory budget described in
[Copied Factory Settings and Item Conservation](../Scaling/IMPL_Scaling_CurrentFlow.md#copied-factory-settings-and-item-conservation).
This contract also applies to Scaled Extend and Restore; copying settings never
authorizes creation of unfunded items.

## Scaled Extend

Scaled Extend is active when Extend mode is active and the clone count, row count, or layer count is greater than one. `USFExtendService::OnScaledExtendStateChanged` tracks the grid-derived state, and `IsScaledExtendActive` reports whether the enhanced mode is currently in use.

Scaled Extend uses the same transform state as Scaling for clone offsets. Spacing, Steps, Stagger, and Z Rotation should be documented as implemented transform inputs. X/Y rotation axes should not be described as active because the current transform pipeline only implements Z rotation.

Pipe-lane source-port selection uses the horizontal displacement from the source factory
to the held first clone as its fixed principal axis. That pose pair is available both
locally and during server reconstruction. An additional cell's total world offset is
not an Extend-forward vector: it also includes its row and layer placement. Choosing
the least-rotated additional cell previously let a 34 m row offset at 5 degrees turn
the source-port facing score below the 0.30 threshold, omitting seven of eight pipe
lanes before spline routing while leaving the parent green. The first-clone axis also
avoids selecting a different port as later clones arc around the source. The
`SmartFoundations.Extend.Pipes.RowIndependentAxis` regression exercises the real clone
planner with both row sides, both Extend directions, layers, distant clones and occupied
source ports; routing and built connectivity still require native gameplay checks.

See [../Transforms/IMPL_Transforms_CurrentFlow.md](../Transforms/IMPL_Transforms_CurrentFlow.md).

### Cell Identity and Vertical Layers

Live Scaled Extend enumerates Chain/Rows/Layers as (X,Y,Z). The existing source
(0,0,0) is never rebuilt, and (1,0,0) is the held parent. Every other row/layer
starts with its own X=0 seed. Layer offsets use signed Z times factory height plus
Spacing Z, in world vertical, while horizontal rotation and steps remain unchanged.
Stagger remains unavailable in live Extend.

Row pitch is measured from `ScaledExtendBaseTopology`, the preserved single-copy
layout, never the merged `StoredCloneTopology`. Measuring the merged layout feeds
previous rows back into the next resize: the #534 reproduction had a 25 m first
row gap and a 75 m next gap. The count-only path must keep both positions and
identities independent of the current grid dimensions.

Live cell prefixes are `sc_X_Y_Z_`; they are transient construction identities,
not the persisted Restore prefix format. Preview reuse compares all three
coordinates. Logistics and power predecessors are the preceding X cell in the
same row/layer; seeds have no source-to-clone seam. Multiplayer commits carry
Grid Z along with XY and the world transform, and authority uses the same spawn
pipeline. Client and server binaries must be updated together for this schema.

The panel exposes Grid Z and Spacing Z. Both scale modifiers plus wheel and the
vertical numpad keys scale layers. Spacing cycles through Z in classic mode and
Chain/Rows/Vertical in Player Relative mode; steps and rotation remain horizontal
progression targets. The HUD shows the layer count when greater than one.

`SmartFoundations.Extend.Grid3D` covers cell enumeration, count-independent IDs,
row pitch, and signed vertical placement. Gameplay remains the validation gate
for actual construction, costs, designer bounds, and multiplayer wiring.

## Wiring and Stabilization

### Restored Module Layers

Saved modules replay through `SFExtendRestoreReplayService`, independently of live
Extend's live-target controls. Restore enumerates X, Y, and Z for both factory
children and their captured infrastructure. The base cell remains the parent;
additional cells have separate identities. Base-layer IDs retain `rr_X_Y_` compatibility,
and upper layers use `rr_X_Y_Z_`. Belts and pipes chain along X within each row/layer;
duplicating a floor does not invent a vertical logistics connection. Pole chains are
also scoped to their layer.

`SFRestoreGrid::Placement` defines the preview factory transforms, and
the camera uses the same XYZ placement through `CalculateRestoredScaledClonePlacement`.
Z uses the signed layer index times factory height plus Z spacing, with stack stagger
in the parent's horizontal frame. Existing XY rotation and steps remain unchanged.
The stored topology fallback measures row width from the unexpanded template, not
the full repeated layout.

The failure addressed by #509 was three separate XY-only loops (topology expansion,
factory spawning, and movement refresh), compounded by XY-only post-build ID parsers.
Changing only the preview loop would leave upper-floor factory targets unwireable.
`SmartFoundations.Restore.Grid3D` covers placement and identity invariants; live
construction and network behavior still require gameplay validation.

### Built Belt Coordinate Frame

FactoryGame's `FGBuildableConveyorBelt.h` explicitly requires zero actor rotation.
Preview and saved clone topology may retain rotated local spline frames, but
`ASFConveyorBeltHologram::ConfigureActor` bakes that rotation into the new built
belt's points and both tangents after native direction correction and before
component setup and registration. The origin, point order, world-space curve,
and intended connections are preserved. This construction boundary is shared
by Extend, Scaled Extend, Restore, and server reconstruction; it does not rewrite
existing actors on load or change the stored Restore schema.

Violating this contract causes #504: inserting a splitter into an 8 m rotated
Smart-built lane left the second section at zero yaw with local-X spline data,
moving its far endpoint 5.44 m from the still-logically-connected merger. A
same-tier manual belt on the same span used zero actor yaw and split correctly
(insertion positions differed by 4.62 cm). The private split implementation is
not available; its observed output and the public class contract establish the
required representation. `SmartFoundations.Conveyor.CanonicalGeometry` covers
the captured straight span, coordinate-frame invariance of curved/sloped spans,
both tangent vectors, neutral input, and repeat application.

### Connection Registration

Extend does not rely on every preview-time snapped connection surviving vanilla construction. The post-build wiring manifest is the authoritative repair step for final connections.

Distributor connector identity and built/hologram parity are defined in [Distributor Port Topology Reference](../../Reference/REF_DistributorPortTopology.md). Extend capture, clone planning, multiplayer reconstruction, and Restore replay must preserve those stable named ports end to end.

Extend source capture records the distributor class, factory-side connector name, occupied connector names, and connector world positions. Clone planning resolves the two eligible lane ports from the shared named topology before creating distributor or segment holograms; an invalid recognized orientation drops the whole branch before preview and cost generation. `LaneFromConnector` and `LaneToConnector` already carry those exact names through the wiring manifest, multiplayer server reconstruction, and Restore JSON, so this contract does not require a schema migration.

Branch-owned attachments follow the same exclusion decision. Inline pumps and valves are chain segments and disappear with an excluded branch. Floor holes preserve the stable actor IDs of their top/bottom snapped conduits and are omitted when all snapped owners belong to excluded chains. Wall holes have no logical connection ownership, so their existing conduit-segment overlap test determines whether they belong to an excluded branch. Power poles are captured independently from the source factory's own power connection and remain valid even when an unrelated pipe branch is excluded.

| System | Post-build handling |
|--------|---------------------|
| Belts and lifts | Reconnect via factory connection components and stabilize conveyor chain actors. |
| Pipes | Reconnect via pipe connection components and rebuild pipe networks. |
| Power | Create cloned wires where source topology and capacity allow it. |

### Exact Constructed Owners

Clone IDs are registered at `ConfigureActor`, with `SFExtendBuiltActors::RegisterChildren`
providing a fallback restricted to the actual `Construct` result. The fallback requires a unique
class and full-transform match (1 cm position tolerance), not a nearby world actor. Ambiguous,
wrong-class, wrong-floor, differently rotated or scaled candidates do not resolve. Duplicate array
references to the same constructed actor are not separate candidates. The base factory is the
actual returned parent, never whichever factory happened to trigger a spawn callback first.

This prevents the old Restore 30 m and Scaled Extend 5 m world searches from selecting an existing
factory as a wiring destination. `SmartFoundations.Extend.ConstructedOwners` covers these selection
invariants. Native subclass registration and finished connection behavior still require gameplay
validation; a missing owner must remain missing rather than widening the search.

### Pipe Floor-Hole Attachments

Pipeline floor holes retain native thickness, top/bottom external pipe owner IDs and named ports
through capture, clone remapping, Restore serialization and cell prefixes. `bHasPassthroughLinks`
distinguishes an intentionally unattached face from an older preset without attachment metadata.
Repeated discovery of one source hole is deduplicated before assigning its clone ID.

`SFExtendPassthroughLinks::Apply` considers only uniquely registered, constructible new pipeline
holes and pipes. A logical face is at the hole's actor-local Z plus/minus half its native thickness,
transformed into world space. Selection requires a same-cell endpoint within 1 cm in all three
dimensions and, for captured links, the exact owner and port. Occupied faces are untouched;
ambiguous matches or two holes claiming one endpoint are rejected. Legacy recovery is limited to
a unique same-cell geometric match, not a world scan.

The previous XY-only predicate could accept holes 1 m, 10 m and 50 m away vertically while searching
a 100 m world radius (#542). Both pipe-hole relinking loops now use the scoped helper. The separate
legacy conveyor-lift passthrough path is not covered by this pipeline-hole contract.
`SmartFoundations.Extend.Passthrough.ScopedIdentity` and `CapturedPlan` cover selection/capture,
including thickness, rotated holes, stacked cells, empty captured faces and ambiguity. Proving that
native fluid connections work and unrelated saved records remain unchanged requires a disposable
save comparison; these contract tests do not establish that gameplay result.

### Lane Socket Directions

`SFExtendLaneNormals::VerifyCapture` checks connector directions against the shared named distributor
port catalog. Verified directions rotate with their owners; Restore must not replace them with the
endpoint chord simply because spacing or steps make the lane diagonal (#545).

For legacy captures, `RecoverLegacy` requires a unique named distributor/pipe-junction owner, a valid
catalogued port, and agreement with its 100 cm socket geometry. Unknown classes, absent T-junction
ports, ambiguous owners or mismatched geometry do not become verified. `RepairUnverified` retains
the earlier #422 chord recovery only for each unverified or invalid endpoint. Verification flags
and normals persist in the actual Restore JSON.

`SmartFoundations.Restore.Lanes.SocketNormals` and `LegacyRecovery` exercise rotated, offset,
stepped and stacked layouts and malformed legacy data. Physical routing modes and socket approaches
remain native gameplay checks, separate from the #504 built-belt coordinate-frame contract.

### Power Endpoint Ownership and Cost

Power plans preserve exact owner/connector pairs and independent per-face budgets (#543/#544).
Existing source-to-clone endpoints remain fixed when the clone rotates; the clone endpoint and
stored parent pose move together. First and additional cells use the same remapping and refresh
their actual catenary preview and cached cost after transformations (#546).

Factory daisy chains are explicit priced edges, including the MAM prerequisite and poleless/continue
chain options. Restore regenerates adjacent-X edges in each row/layer without bridging missing cells.
There is no extra inferred post-build factory chain. For native request timing, designer boundaries,
preview-only wire construction and remaining runtime verification requirements, see
[Power Connector Construction Contracts](../../Reference/BuildableContracts/PowerConnectors.md#extend-and-restore-construction-boundary).

## Important Caveats

- The old deferred-build queue design is not the current implementation; the active path is child holograms plus JSON topology plus post-build wiring.
- Wiring remains tightly coupled to built-actor registration and clone ids.
- Large or dense belt clones share the same chain actor fragility as SmartUpgrade and should use `USFChainActorService` rather than direct bucket-level conveyor APIs.
- Floor holes, pumps, valves, and passthrough behavior has evolved since older plans; trust the current topology and JSON code before older planning docs.

## Archived Inputs

The previous Extend current-flow doc, audits, floor-hole plan, historical Extend research, power Extend notes, and logistics connection notes were moved to `docs/Archive/2026/features-consolidation/superseded/Extend/`.
