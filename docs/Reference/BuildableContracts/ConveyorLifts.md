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
