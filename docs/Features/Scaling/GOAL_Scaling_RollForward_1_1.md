---
title: Scaling Roll-Forward Goal for 1.1
created: 2026-05-28
tags: [scaling, performance, holograms, goal, satisfactory-1.1]
---

# Scaling Roll-Forward Goal for 1.1

## Objective

Stabilize the `explore-scaling-issues` branch as a working 1.1 baseline before the Satisfactory 1.2 migration. Keep the performance direction, but stop at correctness plus focused cleanup: no broad redesign, no 1.2 API porting, and no new feature scope.

## Estimate

Expected work: **1 focused evening to 1.5 days**.

- Fix correctness regressions: 1-2 hours
- Verify representative scaling behavior in-game: 2-4 hours
- Clean diagnostics and stale/dead paths: 1-2 hours
- Optional small polish from findings: 1-3 hours

Risk: medium. The changed code sits in scaling child placement, validation, destruction, lock inheritance, and auto-connect timing.

## Keep

- Avoid calling `LockHologramPosition()` for every scaling child during positioning.
- Avoid duplicate `UpdateChildPositions()` calls that cancel/restart progressive batches.
- Keep direct child transform placement where needed to avoid floor snapping/anchor offset drift.
- Keep faster queued child destruction if in-game testing confirms it does not race build gun validation.

## Must Fix

1. Restore post-batch validation.
   - After progressive positioning completes, call `ValidatePlacementAndCost()` with the player inventory.
   - Validation must happen after final child transforms, not immediately after the batch is scheduled.

2. Restore post-batch auto-connect orchestration.
   - After progressive positioning completes, trigger the correct orchestrator path for distributor, pipe junction, power pole, stackable belt support, stackable pipe support, and passthrough pipe parents.
   - The helper currently says orchestration is deferred; the deferred callback must actually run.

3. Resolve tracked transform drift handling.
   - Either call `RefreshTrackedScalingChildTransforms()` from the subsystem tick for locked parents, or remove the tracking map if direct placement is sufficient.
   - Do not leave a dead corrective path.

4. Reduce temporary log noise.
   - Convert normal `[SF_SCALE_REGEN]`, `[SF_SCALE_BATCH]`, batch cancel, and belt-positioner diagnostics from `Warning` to `Log`/`Display` while actively testing, or `VeryVerbose` before merge.
   - Keep `Warning` only for real problems.

## Verify

Manual in-game verification is required. Do not build/package through the agent.

Cover at least:

- Simple foundations/factory buildings: scale up, scale down, lock, move, rotate, nudge, construct.
- Large locked grid: confirm FPS/UObject behavior improves and children remain visible/positioned.
- Splitters/mergers: scaled placement still auto-connects belts and cost/HUD updates are correct.
- Pipe junctions/floor holes: scaled placement still creates/updates pipe previews.
- Power poles: scaled placement still updates wire previews without runaway widget/UObject growth.
- Ceiling/wall/floodlight special cases: children remain valid/visible without exposing hidden clearance boxes.
- Water extractor: child validation is still driven by water placement, not overwritten by parent material state.

## Acceptance Criteria

- No known regression from commented-out validation or auto-connect callbacks.
- Scaling child transforms are stable for locked and unlocked previews.
- Large-grid behavior is measurably better or at least no worse than before this branch.
- Temporary investigation logs are not left as warnings.
- The branch is documented as the 1.1 scaling baseline to compare against the 1.2 port.

## Out of Scope

- Porting to Satisfactory 1.2 headers.
- Replacing Smart auto-connect with vanilla blueprint open-connection systems.
- Multiplayer support.
- Broad service refactors.
- New scaling features or new buildable support.
