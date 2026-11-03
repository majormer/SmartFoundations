---
title: Conveyor Lift Construction Contract
type: REF
status: Active
category: Reference
tags: [conveyors, lifts, scaling, snapping, proxies]
---

# Conveyor Lift Construction Contract

## Native selected state

Conveyor Lift Mk.1–Mk.6 share `AFGConveyorLiftHologram` placement behavior.
Their recipes and built classes differ; use the selected recipe for quote and
construction rather than assuming all tiers use the same materials.

The first-end body yaw, placement phase, `mTopTransform`, arrow direction and
reverse state are distinct choices. Second-phase scroll changes the top direction;
it is not the body's world yaw. `mFirstStepYaw` alone does not recover the selected
body rotation. Preserve signed height and the two named factory connectors,
`ConveyorAny0` and `ConveyorAny1`, including their local transforms and directions.

## Placement and attachment

Replay the native placement sequence: pre-placement, `TrySnapToActor`, ordinary
placement update only if snapping fails, then post-placement. A successful snap
owns its pose and attachment state; a subsequent ordinary update can overwrite it.
Resolve lightweight world-hit instances before interpreting their native owners.

Each repeated lift requires its own compatible endpoint peers and floor-hole
faces. Preserve endpoint index, approached face, height sign and ownership; equal
passthrough-array lengths do not prove equivalent placement. Reserve all endpoint
and face assignments before constructing any lift. Never reuse an occupied face
or assign the parent's live connection pointer to a copy.

For an unattached floor placement, retain the native foundation snap result within
its normal grid quantization. Reconstruct additional displacement through native
nudge operations, including their step size, range and disqualifiers. Unlocking
nudge can leave a selected world pose while clearing its offset, so an empty offset
is not proof that the selected placement was never nudged.

A Designer's internal floor belongs to its Designer actor, not a foundation actor.
Only the selected Designer's upward-facing floor can supply that replay contract;
another Designer or a wall cannot. Containment and final native validation still
apply. For vertical repetition, search far enough to find the original mounting
level and then require native nudge authorization. Finding a floor is not itself
permission to place a floating lift.

## Preview and materialization

Smart retains one native player parent and uses `ASFConveyorLiftPreviewHologram`
for its repeated visual cells. `SFConveyorLiftHologramState` reuses one hidden native
validator per parent and creates brief native cells for materialization. Internal
cells must clear previous peers, passthroughs and accumulated nudge when reused.

Internal cells do not need player map/compass lock markers. Calling the player's
lock publication operation repeatedly for those cells exhausts widgets; use the
tag-checked internal lock seam and retain native `CanNudgeHologram` and
`NudgeTowardsWorldDirection` checks. Cancel, shrink and recipe changes must retire
the validator and proxy children.

## Direct Auto-Connect docks

A lift can replace both the height change and the connecting belt when its remote
mouth can dock directly to the other endpoint. Native connector markers do not
coincide in this case: the peer's outward normal and the sum of both connector
clearances determine the mouth position. Require that position, opposite-facing
normals, native connector compatibility and full native lift placement validation.
A short gap or a nearby parallel shaft is not a direct dock.

The direct route records both native snapped connections. Preserve exact endpoint
identity through construction, including the newly built distributor, and resolve
reversed lifts by physical mouth position and flow direction rather than assuming
preview and built connector indices match. Reusing a native validator must clear
the previous second endpoint before preparing another route. A direct route has
one lift preview and its native cost, with no belt child or belt charge.

## Lift-assisted Auto-Connect

`FSFLiftRoutePreview` retains the existing distributor-to-factory assignment and
changes only how the chosen pair is connected. Belts Only retains ordinary belt
routing. Lift at Distributor puts the level belt at the machine-port height;
Lift at Building puts it at distributor height. A native direct dock omits the
belt and its cost.

Visible lift/belt proxies share native validation cells. Each committed route
uses a fresh native construction cell, exact endpoint identity and captured
geometry. The authority reconstructs the bounded plan before payment and checks
current endpoint availability, pose, direction, native placement and cost. A
failed route rejects the whole placement.

For a generated belt, setting its snapped peers and spline does not initialize
the native hologram's connector directions. Call the native
`SetupSnappedConnectionDirections` after binding those peers, including when
replacing preview peers with the actual constructed actors. Native belt
`ConfigureComponents` copies the hologram directions onto the built connectors
before testing compatibility and connecting them. A correctly shaped, priced
belt can otherwise be constructed with neither endpoint connected. See the
[native construction contract](../THESIS_Belts_ChainActors_Placement.md#native-belt-direction-initialization)
for the binary evidence.

Reconstruction can add children after the build gun has reset its placement
state. Reset the complete prepared tree before validation, validate the player
parent without asking generated children for independent aiming state, and keep
the complete accepted quote while doing so. Restore the child tree before native
construction. This must not clear genuine parent placement or route failures.

Inside a Blueprint Designer, discover owned factories from its native member
list as well as the ordinary overlap broad phase. Keep the same distance and
port-selection rules. Both preview proxies and native construction cells must
inherit the Designer context; otherwise construction can miss the Designer's
save and clear ownership.

## Costs, ownership and lifecycle

Quotes must match the selected geometry and native length-based cost multiplier.
Validate and reserve the entire grid before normal-cost construction; a failed
cell must not leave a partly paid or partly constructed batch. Built actors remain
ordinary vanilla lifts with native connections and conveyor-chain registration.

Outside a Designer, the completed grid receives normal Smart dismantle grouping.
Inside a Designer, native Designer membership is the owner contract. Establish
that containment before cell placement validation, including authority-side
reconstruction of multiplayer requests. A nearby-world query can omit
Designer-owned actors; inspect the Designer's native membership when observing
construction, reload or dismantling.

## Evidence and regression boundaries

`SFConveyorLiftScalingTests.cpp` covers selected signed height/top/flow state,
independent bindings, tier profiles, passthrough actor-slot correspondence and the
owned-floor classifier. Live acceptance tracked in [issue #555](https://github.com/majormer/SmartFoundations/issues/555)
must separately establish payment, native topology/operation, grouping/refunds,
actual reload, Designer ownership and cleanup. A preview or save request alone is
not those outcomes. Actor-count reductions are representation evidence; measure
frame times before claiming a performance improvement.
