---
title: Smart Upgrade — Current Implementation Flow
type: IMPL
date: 2026-04-27
status: Active
category: Features
tags: [upgrade, audit, traversal, conveyor, chain_actor, power, pipe]
related: [SFUpgradeExecutionService.h, SFUpgradeAuditService.h, SFUpgradeTraversalService.h, SFChainActorService.h]
---

# Smart Upgrade — Current Implementation Flow

**Last Updated:** 2026-04-27  
**Status:** Active, shipping  
**Purpose:** Canonical reference for the Smart Upgrade feature implementation

---

## Overview

Smart Upgrade can scan and upgrade existing logistics and power infrastructure. The main execution path is synchronous, uses vanilla hologram construction where possible, captures expected connections before destroying actors, repairs connections after replacement, then delegates conveyor chain actor stabilization to `USFChainActorService`.

The chain system is the fragile part. Large conveyor upgrades can leave orphaned tick groups and slow-materializing chain state. In-game orphan bounce repair is disabled and diagnostic-only because every in-game bounce variant tested during the April 2026 investigation caused crashes. See `RESEARCH_SmartUpgrade_ChainActorInvestigation.md` in the working repository for full crash analysis and findings.

---

## Current Status

- **Active and shipping.**
- **Synchronous execution** — all pending replacements process in one frame to prevent factory tick workers from observing partial conveyor topology.
- **Vanilla hologram upgrade path** for belts, lifts, and power poles where possible.
- **Connection capture and repair** preserves network topology across batch replacements.
- **Chain actor stabilization** uses a two-phase vanilla-rebuild queue rather than manual tick-group coalescing.

---

## Primary Code Files

| File | Role |
|------|------|
| `SFUpgradeExecutionService.h` / `.cpp` | Batch upgrade orchestration, cost, connection repair, chain service coordination |
| `SFUpgradeAuditService.h` / `.cpp` | Time-sliced scanning, family/tier classification, radius and save-wide audit |
| `SFUpgradeTraversalService.h` / `.cpp` | Connected-network traversal for belt, pipe, and power networks |
| `SFChainActorService.h` / `.cpp` | Conveyor chain actor invalidation, rebuild, diagnostic, and repair |

---

## User-Facing Surfaces

| Surface | Purpose |
|---------|---------|
| **Smart Upgrade Panel** | Family selection, target tier, radius, and execution controls |
| **Audit Tab** | Scan results showing count by tier and family |
| **Traversal Tab** | Network-walk results from an anchor buildable |
| **Chain Triage Controls** | Detect and Repair actions for chain actor diagnostics |

---

## Supported Families

| Family | Execution | Tiers | Notes |
|--------|-----------|-------|-------|
| Belt | Supported | Mk1–Mk6 | Uses vanilla hologram upgrade path + explicit chain stabilization |
| Lift | Supported | Mk1–Mk6 | Uses vanilla hologram upgrade path |
| Pipe | Supported | Mk1–Mk2 | Uses `ASFPipelineHologram` path; preserves indicator/no-indicator style |
| Pump | Audit/traversal context only | Mk1–Mk2 | Traversal can cross pumps; execution not implemented |
| PowerPole | Supported | Mk1–Mk3 | Vanilla hologram upgrade path |
| WallOutletSingle | Supported | Mk1–Mk3 | Vanilla hologram upgrade path |
| WallOutletDouble | Supported | Mk1–Mk3 | Vanilla hologram upgrade path |
| Tower | Audit/traversal context only | No execution | Power towers are not upgraded |
| Wire/PowerLine | Anchor only | N/A | Classified as `PowerPole` for traversal entry from a wire |

---

## Family and Tier Classification

`USFUpgradeTraversalService::GetUpgradeFamily` is the central runtime classifier for traversal and execution validation.

| Pattern or type | Family |
|-----------------|--------|
| Class name contains `ConveyorBeltMk` | Belt |
| Class name contains `ConveyorLiftMk` | Lift |
| `AFGBuildablePipelinePump` or class name contains `PipelinePump` | None for upgrade consideration |
| `AFGBuildablePipeline` | Pipe |
| Class name contains `PowerPoleMk` | PowerPole |
| Class name contains `PowerPoleWallDouble` | WallOutletDouble |
| Class name contains `PowerPoleWall` | WallOutletSingle |
| Class name contains `PowerTower` | Tower |
| Class name contains `Wire` or `PowerLine` | PowerPole anchor |

`GetBuildableTier` has family-specific behavior:

| Family/type | Tier rule |
|-------------|-----------|
| Pipes | `MK2` means tier 2; otherwise tier 1 |
| Power poles and wall outlets | `Mk2`/`Mk3` map normally; unsuffixed variants are tier 1 |
| Generic Mk families | Search `Mk6` down to `Mk1` in class name |

---

## Traversal System

`USFUpgradeTraversalService::TraverseNetwork` returns an `FSFTraversalResult` containing entries, count by tier, total count, upgradeable count, and a max-limit flag.

Traversal defaults are defined by `FSFTraversalConfig`:

| Option | Default | Behavior |
|--------|---------|----------|
| `bCrossSplitters` | `true` | Belt traversal crosses splitters and mergers |
| `bCrossStorage` | `false` | Belt traversal crosses storage containers only when enabled |
| `bCrossTrainPlatforms` | `false` | Belt traversal crosses cargo platforms only when enabled |
| `bCrossFloorHoles` | `true` | Lift traversal can cross passthrough/floor-hole buildables |
| `bCrossPumps` | `true` | Pipe traversal crosses pumps |
| `MaxTraversalCount` | `10000` | Stops adding results at safety limit |

### Belt and Lift Traversal

`TraverseConveyorNetwork` walks `Connection0` and `Connection1` from a conveyor base. Belts and lifts are added to results. Intermediate splitters, mergers, storage containers, train platforms, and floor holes are crossed only according to config and are not themselves added as upgrade targets.

Machines are terminal unless they match a configured cross-through type. This prevents traversal from walking through arbitrary factory buildings.

### Pipe Traversal

`TraversePipelineNetwork` walks pipe connection components. Pipelines are added to results. Pumps are not added, but can be crossed when `bCrossPumps` is enabled.

### Power Traversal

`TraversePowerNetwork` starts from a power pole or a wire. For poles, it gathers power connection components, follows attached wires to their other endpoint, and recursively adds connected power poles and wall outlets. Non-pole powered buildings are not added.

Power traversal currently has a count cap but does not implement a separate hop cap or radius cap. Older notes recommended those; they are still good improvement candidates.

---

## Audit System

Audit scans buildables by family and tier and can store full entries or counts only.

Important audit structs:

| Struct | Purpose |
|--------|---------|
| `FSFUpgradeAuditEntry` | One buildable, family, current tier, max available tier, distance, location |
| `FSFUpgradeTierBucket` | Count and optional entries for one tier |
| `FSFUpgradeFamilyResult` | Counts and tier buckets for one family |
| `FSFUpgradeAuditResult` | Full audit snapshot, progress, origin, radius, timestamps, totals |
| `FSFUpgradeAuditParams` | Origin, radius, included families, entry storage options, requesting player |

Audit is the right place to add "remaining to upgrade" utility, nearest remaining target, and stale-cache UX. Some of that was described in older design notes, but should be implemented against `USFUpgradeAuditService` rather than kept as loose docs.

---

## Execution Flow

`USFUpgradeExecutionService::StartUpgrade` is the main execution entry point.

Current flow:

1. Reject if another upgrade is already in progress.
2. Validate family and target tier.
3. In radius mode, require valid source tier and require target tier to be greater than source tier.
4. Resolve target recipe with `GetUpgradeRecipe`.
5. Reset all per-run state.
6. Gather targets.
7. Save belt/lift connection pairs.
8. Save pipe connection pairs.
9. Capture the full expected connection manifest.
10. Process all pending upgrades synchronously in one frame.
11. Abort remaining targets if materials run out.
12. Call `CompleteUpgrade`.

Older docs said execution should be throttled N items per tick. That is no longer the active implementation. The timer-era functions and constants still exist, but the current code processes synchronously because async/timer upgrades allowed factory tick workers to observe partially upgraded conveyor state.

---

## Target Gathering

### Traversal Mode

When `SpecificBuildables` is populated, execution uses those actors directly. Each valid actor is filtered to `0 < current tier < target tier`. `SourceTier` can be zero in this mode because mixed-tier traversal is allowed.

### Radius and Save-Wide Mode

For pipes, execution scans `AFGBuildablePipeline` actors at the source tier so indicator and no-indicator variants are both included. Pumps and junctions are excluded.

For other families, execution resolves an exact source class and scans world `AFGBuildable` actors with exact class match. Radius and max item limits are then applied.

For conveyor families, radius mode treats belts and lifts as connected cohorts so Smart Upgrade does not intentionally replace only part of a belt/lift chain. The current policy is exclusive at the radius boundary: a cohort that intersects the radius but has members outside the radius is skipped rather than partially upgraded. Revisit this inclusive/exclusive decision in a future pass, especially for conveyor groups that visibly straddle the radius search line.

---

## Pre-Upgrade State Capture

Smart captures connection state before any actor is destroyed.

### Conveyor Pair Capture

`SaveBatchConnectionPairs` stores direct conveyor-to-conveyor edges where both conveyors are in the batch:

```text
(old conveyor, local connection index) -> (old partner conveyor, partner connection index)
```

This protects connected belts/lifts that are both replaced in the same batch.

### Pipe Pair Capture

`SaveBatchPipeConnectionPairs` does the same for pipe-to-pipe connections.

### Expected Connection Manifest

`CaptureExpectedConnectionManifests` records every connected factory, pipe, and power edge for every pending buildable.

| Edge kind | Captured by | Repair behavior |
|-----------|-------------|-----------------|
| Factory | `UFGFactoryConnectionComponent` | Can repair by exact component `FName` |
| Pipe | `UFGPipeConnectionComponent` | Can repair by exact component `FName` |
| Power | `UFGCircuitConnectionComponent` | Can validate existing partner but cannot recreate missing wire actors |

The manifest uses old buildable pointers as identity keys. After destruction, those pointers must only be used for map lookup or after `IsValid` checks.

---

## Per-Family Replacement Details

### Belts

Belts use the vanilla hologram upgrade path plus explicit connection and chain stabilization.

Current belt steps:

1. Spawn hologram from target recipe.
2. Set blueprint designer context.
3. Create a hit result pointing at the old belt.
4. Call `TryUpgrade`.
5. Call `ValidatePlacementAndCost`.
6. Run `DoMultiStepPlacement` until complete.
7. Calculate per-item net cost from hologram base cost minus old belt refund.
8. Check inventory and central storage, then deduct or refund.
9. Call `GenerateAndUpdateSpline` for belt holograms.
10. Construct the new belt with a fresh net construction ID.
11. Destroy the hologram.
12. Call `PreUpgrade_Implementation` and `Upgrade_Implementation` on the old belt.
13. Transfer missing connections from old to new; avoid redundant transfer when `ConfigureComponents` already connected the new belt.
14. Capture the old chain actor before destroying the old belt.
15. Destroy old child actors and old belt.
16. Track the new belt in `UpgradedConveyors`.
17. Record old-to-new maps.

Important correction from older research: belt `Upgrade_Implementation` cannot be trusted to perform all chain transfer. The current code explicitly rebuilds affected chain actors after the batch.

### Pipes

Pipes use `ASFPipelineHologram` rather than the vanilla `SpawnHologramFromRecipe` path.

Current pipe steps:

1. Detect source indicator style from class name.
2. Resolve target recipe/class preserving indicator/no-indicator style.
3. Copy old spline point data.
4. Spawn Smart pipe hologram with deferred construction.
5. Set build class and recipe before finishing spawn.
6. Apply spline data.
7. Calculate per-item net cost from hologram base cost minus old pipe refund.
8. Construct new pipe.
9. Destroy hologram.
10. Call upgrade hooks on old pipe.
11. Transfer pipe endpoint connections.
12. Destroy old pipe and children.
13. Record old-to-new buildable map.

### Lifts

Lifts use the vanilla hologram upgrade path.

Current lift steps:

1. Spawn hologram from target recipe.
2. Verify it is an `AFGConveyorLiftHologram`.
3. Set blueprint designer context.
4. Copy old top transform into `mTopTransform` via reflection.
5. Call `TryUpgrade`.
6. Calculate per-item net cost and apply inventory/central-storage changes.
7. Construct new lift.
8. Destroy hologram.
9. Call upgrade hooks on old lift.
10. Transfer factory connections.
11. Capture old chain actor.
12. Retire old lift and child actors using short lifespan.
13. Track new lift in `UpgradedConveyors`.
14. Record old-to-new maps.

### Power Poles and Wall Outlets

Power poles and wall outlets use the vanilla hologram upgrade path.

Current power steps:

1. Calculate recipe-based net cost minus old actor refund.
2. Spawn hologram from target recipe.
3. Set blueprint designer context.
4. Call `TryUpgrade`.
5. Construct new pole/outlet.
6. Destroy hologram.
7. Call upgrade hooks on old pole; this is expected to transfer wires.
8. Destroy old actor and children.
9. Record old-to-new buildable map.

Power edge repair is validation-only for missing wires. The code logs broken power edges but does not recreate wire actors.

---

## Cost and Refund Behavior

The active implementation applies costs per item during `ProcessSingleUpgrade`.

For each item:

1. Compute target cost.
2. Subtract old dismantle refund.
3. If net positive, ensure player inventory plus central storage can pay.
4. Deduct using the player's "take from inventory before central storage" preference.
5. If net negative, add refund to inventory.
6. Accumulate refund overflow.

At completion, overflow refund items are spawned in a dismantle crate near the player.

Older docs described several cost model options. The code currently uses immediate per-item cost handling, not pure vanilla aggregate deduction and not an all-or-nothing batch cost.

---

## Completion and Stabilization

`CompleteUpgrade` performs stabilization after all replacements.

Current order:

1. Clear upgrade timer.
2. **Pre-repair chain invalidation:** If conveyors were upgraded, call `USFChainActorService::InvalidateAndQueueVanillaRebuildForBelts` to invalidate affected chains and queue vanilla's next-frame rebuild **before** connection repair. This prevents thousands of `ClearConnection`/`SetConnection` calls from leaving old chain actors ticking stale segment/item state.
3. Fix saved belt/lift pair references.
4. Fix saved pipe pair references.
5. Validate and repair the expected connection manifest.
6. **Post-repair re-registration:** Call `USFChainActorService::ReRegisterAndQueueVanillaRebuildForBelts` so FactoryGame assigns bucket membership from the live graph instead of Smart manually coalescing tick groups.
7. Schedule deferred zombie purge after 3 seconds.
8. Clear cost state and spawn overflow crate if needed.
9. Mark result completed and broadcast delegates.
10. Clear per-run arrays and maps.

> **Correction from older docs:** The previous documentation incorrectly stated that `InvalidateAndRebuildForBelts` was called. The current code uses the two-phase `InvalidateAndQueueVanillaRebuildForBelts` + `ReRegisterAndQueueVanillaRebuildForBelts` approach. Manual tick-group coalescing repeatedly produced `NO_SEGMENTS` zombies; delegating to vanilla's queue-and-rebuild path resolved this.

---

## Connection Repair

### Pair Repair

`FixBatchConnectionReferences` and `FixBatchPipeConnectionReferences` repair edges where both endpoints were upgraded in the same batch.

The current belt pair repair reconnects exact connector indices and explicitly does not call `RemoveConveyor` / `AddConveyor`. Older docs claiming that pair repair performs per-belt remove/add are stale.

### Manifest Repair

`ValidateAndRepairConnections` verifies every captured pre-upgrade edge whose local actor was upgraded.

Partner resolution rules:

1. If the partner was also upgraded, use `OldToNewBuildableMap`.
2. Otherwise, use the original partner only if it is still valid.
3. If no partner can be resolved, mark the edge broken.

Factory and pipe edges can be repaired by exact connector `FName`. Power edges can only be validated; missing wires are reported as broken.

---

## Conveyor Chain Actor System

Conveyor chain actor state is the highest-risk part of Smart Upgrade.

Smart captures old chain actors into `PreDestroyChainActors` from belts/lifts **before** destruction so stale original chains are included even if new belts have not joined a healthy chain yet. After all replacements and connection repair, the two-phase chain service calls ensure vanilla rebuilds fresh chain actors from the completed graph.

The chain stabilizer treats belts and lifts as the same chain family (both are `AFGBuildableConveyorBase`) even though they have distinct recipes and UI classification. A conveyor chain can legally span both belts and lifts. Partial upgrades (e.g., upgrading only belts around an unchanged lift) must be handled safely by the chain rebuild path.

For detailed investigation findings, crash analysis, and SmartMCP procedures, see `RESEARCH_SmartUpgrade_ChainActorInvestigation.md` in the working repository.

---

## Chain Triage and Repair

`DetectChainActorIssues` reports structural chain problems without mutating state:

| Field | Meaning |
|-------|---------|
| `ZombieChainCount` | Zero-segment chain actors |
| `SplitChainCount` | Adjacent same-level conveyors, including lifts, in different live chain actors |
| `OrphanedBeltCount` | Flat belts with no chain actor but at least one connected chained neighbor |
| `OrphanedTickGroupCount` | `FConveyorTickGroup` entries whose `TG->ChainActor == null` |
| `EmptyOrphanedTickGroupCount` | Orphaned TGs with no live conveyor entries |
| `LiveBeltsInOrphanedTickGroups` | Live belts and lifts inside orphaned TGs |
| `OrphanedBeltCandidates` | Orphaned TGs with a connected flat belt that can be reported as a representative candidate |
| `TickGroupBackPointerMismatchCount` | TGs whose live conveyor members do not all point back to `TG->ChainActor` |

`RepairAllChainActorIssues` currently:

1. Purges zombie chain actors.
2. Repairs split-chain pairs via `RepairSplitChains`.
3. Calls `RepairOrphanedBelts` — **diagnostic-only in the current build**. It counts orphaned TGs, empty TGs, live belts, and orphan candidates, then logs without mutating vanilla state. `OrphanedBeltCandidates` is the primary result field; `OrphanedBeltsRequeued` remains only as a deprecated compatibility alias.
4. Purges zombies again.

---

## Deferred Zombie Purge

`ScheduleDeferredZombiePurge(3.0f)` schedules a purge after inline rebuild.

The purge:

1. Finds `AFGConveyorChainActor` actors with zero segments.
2. Clears any TG still pointing at them.
3. Removes them from the subsystem's chain actor list.
4. Destroys them on the game thread outside the factory tick worker window.

The deferred path exists because immediate `Destroy()` can race `AFGConveyorChainActor::Factory_Tick` on ParallelFor workers.

---

## Bucket ID Invariant

Every belt caches a conveyor bucket ID that indexes `AFGBuildableSubsystem::mConveyorTickGroup`.

Do not remove entries from `mConveyorTickGroup`. Removing or `RemoveAll`-ing entries shifts later indices and can cause vanilla `RemoveConveyor` to assert because a belt's cached bucket ID points to the wrong TG.

Observed assertion:

```text
Assertion failed: mConveyorTickGroup[bucketID]->Conveyors.Contains(conveyorToRemove)
FGBuildableSubsystem.cpp:1886
```

Empty absorbed TGs are ugly but safer than shifting the array.

---

## Stability Improvements Worth Adding

### 1. Add a Post-Upgrade Settling State

Large belt upgrades can take a long time to visually/materially settle. During that window, triage can report transient orphan state.

Add an explicit "settling" state after large conveyor upgrades:

- Track `mConveyorGroupsPendingChainActors.Num()` over time.
- Delay chain triage suggestions until pending migration count has been stable for several seconds.
- In the UI, label chain diagnostics as "settling" rather than "broken" while materialization is still in progress.

### 2. Add Large Upgrade Guidance

If a large conveyor upgrade still reports many orphaned belts after a settle timeout, recommend save/reload rather than in-game repair.

This matches observed behavior: save/reload repaired most of the 280 orphaned belts without crashing.

### 3. Improve Chain Diagnostics

Add a richer diagnostic result for orphaned TGs:

- number of orphaned TGs
- number of empty orphaned TGs
- live belts in orphaned TGs
- candidate belts that SmartMCP could bounce
- pending chain actor queue length
- oldest time since last upgrade/chain rebuild

This would make the UI less misleading than a flat "orphaned belts" count.

### 4. Add Power Traversal Caps

Power traversal can potentially walk very large circuits. Add optional hop and radius caps in addition to `MaxTraversalCount`.

Recommended controls:

- max poles/outlets
- max hops from anchor
- max radius from anchor
- explicit warning when traversal truncates

### 5. Harden Unlock Validation

Traversal computes `MaxAvailableTier` from unlock helpers, but execution resolves recipes/classes directly. Add an explicit server-side "target is unlocked/buildable" validation before execution starts, especially for power poles and wall outlets whose recipes may be unlock-gated.

---

## Refactor Assessment

Yes: the upgrade code is very complicated and should be refactored, but carefully. The complexity is not accidental; it comes from having to coordinate vanilla holograms, costs, actor destruction, connection transfer, chain actor rebuilds, multiplayer authority, and UI progress.

The current biggest maintainability issue is not one algorithm; it is that too many responsibilities live in `USFUpgradeExecutionService.cpp`.

### Current Pain Points

| Area | Problem |
|------|---------|
| Per-family upgrade code | Belt, pipe, lift, and power logic are long inline branches in one function |
| Cost/refund handling | Similar inventory/central-storage logic is duplicated per family |
| Connection capture/repair | Pair repair and full manifest repair are mixed into execution service |
| Chain stabilization | Critical and still intertwined with upgrade completion |
| Timer-era code | Old async/timer members remain although active execution is synchronous |
| Compatibility naming | `OrphanedBeltsRequeued` remains as a deprecated alias for older callers |
| Logs | Many active troubleshooting logs are useful now but too noisy for long-term release behavior |

### Recommended Refactor Shape

Keep behavior stable first. Do not combine refactor with new chain repair behavior.

Recommended extraction:

| New component | Responsibility |
|---------------|----------------|
| `USFUpgradeCostService` | Compute/apply per-item net cost, central storage deduction, refund overflow |
| `USFUpgradeConnectionManifestService` | Capture expected edges and repair/validate factory, pipe, and power edges |
| `ISFUpgradeFamilyExecutor` or family-specific helper functions | Belt, pipe, lift, and power replacement logic |
| `USFUpgradeChainStabilizationService` or stricter `USFChainActorService` boundary | Chain rebuild invocation, settling diagnostics, no direct UI semantics |
| `FSFUpgradeRunContext` | Holds all per-run state now spread across service members |

### Low-Risk Cleanup Order

1. Update `SFChainActorService.h` comments to match diagnostic-only orphan behavior.
2. Rename result field or UI label for orphan repair candidates.
3. Remove or quarantine stale bounce queue members if no in-game bounce path is planned.
4. Extract repeated cost/refund helpers without changing behavior.
5. Extract connection manifest capture/repair into a helper/service.
6. Extract family upgrade branches one at a time, starting with power poles because they are simplest.
7. Add settling diagnostics and UI guidance.

---

## Invariants to Preserve

- Do not build or package during investigation unless explicitly requested.
- Do not shift `mConveyorTickGroup` indices.
- Do not manually destroy active chain actors during upgrade rebuild.
- Do not pre-null belt chain actor pointers before vanilla removal.
- Do not ship in-game mass orphan-bounce repair until scheduling is proven safe.
- Prefer save/reload first for large-upgrade orphan cleanup; do not use SmartMCP actual bounce on high-count no-reload orphan sets.
- Trust current code and live logs over older speculative docs.
