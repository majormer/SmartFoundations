#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SFChainActorService.generated.h"

class USFSubsystem;
class AFGBuildableSubsystem;
class AFGConveyorChainActor;
class AFGBuildableConveyorBase;
struct FConveyorTickGroup;

/**
 * Result of a map-wide chain actor diagnostic scan.
 * Returned by USFChainActorService::DetectChainActorIssues().
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FSFChainDiagnosticResult
{
	GENERATED_BODY()

	/** Chain actors with zero segments (NO_SEGMENTS zombies). These will be destroyed on repair. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 ZombieChainCount = 0;

	/** Chain actors involved in SPLIT_CHAIN pairs (adjacent belts in different chains). These will be rebuilt on repair. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 SplitChainCount = 0;

	/** Flat belts (non-lifts) that are connected but are not owned by any chain actor. Repair reports these as candidates only. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 OrphanedBeltCount = 0;

	/** Tick groups with no chain actor assigned. This catches chain=NONE flow failures that chain-only diagnostics miss. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 OrphanedTickGroupCount = 0;

	/** Orphaned tick groups with no live conveyor entries. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 EmptyOrphanedTickGroupCount = 0;

	/** Live belts and lifts found inside orphaned tick groups. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 LiveBeltsInOrphanedTickGroups = 0;

	/** Orphaned tick groups with a connected flat belt that can be reported as a representative candidate. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 OrphanedBeltCandidates = 0;

	/** Tick groups whose live conveyor members do not all point back to TG->ChainActor. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Diagnostics")
	int32 TickGroupBackPointerMismatchCount = 0;

	/** Returns the orphan issue count without double-counting belt and tick-group views of the same failure. */
	int32 OrphanIssueCount() const { return FMath::Max(OrphanedBeltCount, OrphanedTickGroupCount); }

	/** Returns the total number of problematic chain actors found. */
	int32 TotalIssues() const { return ZombieChainCount + SplitChainCount + OrphanIssueCount() + TickGroupBackPointerMismatchCount; }

	/** Returns true if any issues were detected. */
	bool HasIssues() const { return TotalIssues() > 0; }

	/** Returns true if conveyor tick groups exist with TG->ChainActor == null. */
	bool HasOrphanTickGroups() const { return OrphanedTickGroupCount > 0; }
};

/**
 * Result of a map-wide chain actor repair pass.
 * Returned by USFChainActorService::RepairAllChainActorIssues().
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FSFChainRepairResult
{
	GENERATED_BODY()

	/** Number of NO_SEGMENTS zombie chain actors destroyed. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	int32 ZombiesPurged = 0;

	/** Number of tick groups rebuilt when repairing SPLIT_CHAIN pairs. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	int32 SplitGroupsRebuilt = 0;

	/** Number of orphaned tick groups found during the explicit triage repair scan. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	int32 OrphanedTickGroupCount = 0;

	/** Number of orphaned tick groups with no live conveyor entries. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	int32 EmptyOrphanedTickGroupCount = 0;

	/** Number of live belts found inside orphaned tick groups. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	int32 LiveBeltsInOrphanedTickGroups = 0;

	/** Number of orphaned tick groups with a valid connected flat belt that tooling could bounce after save/reload. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	int32 OrphanedBeltCandidates = 0;

	/** Number of live orphan tick-group conveyors re-registered through vanilla RemoveConveyor/AddConveyor. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	int32 OrphanedBeltsRequeued = 0;

	/** Representative flat belt names for orphaned tick groups, useful for operator recovery. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	TArray<FString> OrphanedBeltCandidateNames;

	/** JSON report path written when orphan candidates are found. Empty when no report was produced. */
	UPROPERTY(BlueprintReadOnly, Category = "Chain Repair")
	FString OrphanedBeltReportPath;

	/** Returns true if any repairs were performed. */
	bool AnyRepairsDone() const { return ZombiesPurged > 0 || SplitGroupsRebuilt > 0; }

	/** Returns true if the orphan diagnostic found candidates that were not repaired in-game. */
	bool HasOrphanCandidates() const { return OrphanedBeltCandidates > 0; }

	/** Returns true if the repair pass produced either repairs or diagnostic findings. */
	bool HasAnyResults() const { return AnyRepairsDone() || HasOrphanCandidates(); }
};

/**
 * Chain Actor Service — Canonical path for invalidating and rebuilding conveyor chain actors
 * after topology changes (mass upgrade, Extend / Scaled Extend, etc.).
 *
 * BACKGROUND
 * ==========
 * A conveyor chain actor (AFGConveyorChainActor) owns a contiguous run of belts and ticks
 * them as a single unit via ParallelFor. When Smart! changes belt topology (upgrading a
 * batch, cloning a manifold), the existing chain actors no longer match the live topology
 * and must be invalidated so vanilla can rebuild fresh chains.
 *
 * The naive "RemoveConveyorChainActor(chain) and let vanilla handle it next frame" path
 * (v29.2.1) is crash-safe but leaves a one-frame window where FConveyorTickGroup::ChainActor
 * is null and the old chain actors are Destroy()-pending but not yet GC'd. If a save fires
 * in that window the save captures the transitional state and load produces SPLIT_CHAIN
 * orphans. Extensive live in-game testing confirmed the correct pattern:
 *
 *   1. Resolve each affected chain to its owning FConveyorTickGroup.
 *   2. Call RemoveChainActorFromConveyorGroup(TG) — nulls TG->ChainActor AND every
 *      belt back-pointer in the group. Does NOT call Destroy() on the chain actor
 *      (no GC race, no ParallelFor race; the old actor becomes inert and GCs naturally).
 *   3. Union the cleared groups with mConveyorGroupsPendingChainActors (pre-existing
 *      pending migrations), empty the pending list, then call
 *      MigrateConveyorGroupToChainActor(TG) synchronously for each unique group.
 *      This restores TG->ChainActor to a valid new chain BEFORE control returns — the
 *      next TickFactoryActors pass cannot see a null ChainActor, and a save captured
 *      immediately after returns a consistent state.
 *
 * THINGS THAT LOOK LIKE SHORTCUTS AND ARE NOT
 * -------------------------------------------
 *   - chain->Destroy() after RemoveConveyorChainActor — races Factory_Tick on a
 *     ParallelFor worker thread; EXCEPTION_ACCESS_VIOLATION at FGConveyorChainActor.cpp:339.
 *   - belt->SetConveyorChainActor(nullptr) before removal — bypasses the "belt removed →
 *     chain self-delete" notification path. Original chain leaks as an orphan actor,
 *     serializes on save, crashes on load via GTestRegisterComponentTickFunctions == 0.
 *   - RemoveConveyor + AddConveyor orphan bounce — every in-game variant tested after
 *     large upgrades crashed or asserted. A controlled recovery tool can still use a bounce
 *     for small settled orphan sets after save/reload, but this service's orphan path is
 *     diagnostic-only and never mutates vanilla tick-group arrays.
 *   - Calling the service's phases out of order or skipping dedup — calling Remove or
 *     Migrate twice on the same FConveyorTickGroup corrupts state.
 *
 * CRASH HISTORY: see the "HISTORY — do not regress" comment in SFUpgradeExecutionService.cpp
 * and docs/Features/Upgrade/IMPL_SmartUpgrade_CurrentFlow.md.
 *
 * FRIEND ACCESS: this service is declared `Friend` on AFGBuildableSubsystem via
 * Config/AccessTransformers.ini so it can reach `mConveyorTickGroup`,
 * `mConveyorGroupsPendingChainActors`, `RemoveChainActorFromConveyorGroup`, and
 * `MigrateConveyorGroupToChainActor`. Callers should not try to reach these directly.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFChainActorService : public UObject
{
	GENERATED_BODY()

public:
	/** Bind to owning subsystem. */
	void Initialize(USFSubsystem* InSubsystem);

	/** Release references. */
	void Shutdown();

	/**
	 * Canonical entry point: invalidate the supplied chains, rebuild fresh chains
	 * synchronously on the same frame, and drain any pre-existing pending migrations.
	 *
	 * Null and already-destroyed chain entries are tolerated. Dedup of tick groups is
	 * performed internally — it is safe to pass overlapping chain sets.
	 *
	 * Returns the number of tick groups that were migrated (for logging).
	 */
	int32 InvalidateAndRebuildChains(
		const TSet<AFGConveyorChainActor*>& AffectedChains,
		const TSet<FConveyorTickGroup*>& ExplicitTickGroups = {});

	/**
	 * Convenience overload: supply a belt set; the service derives the affected chains
	 * from (a) each belt's current chain actor and (b) each belt's connected neighbours'
	 * chain actors. This matches the topology-change callsites in Upgrade and Extend.
	 *
	 * `ExtraChains` is unioned in — used by Mass Upgrade to include `PreDestroyChainActors`
	 * captured before old belts were destroyed.
	 */
	int32 InvalidateAndRebuildForBelts(
		const TArray<AFGBuildableConveyorBase*>& Belts,
		const TSet<AFGConveyorChainActor*>& ExtraChains);

	/**
	 * Upgrade-specific prototype: invalidate affected conveyor chains and queue the
	 * affected tick groups for vanilla's next-frame rebuild instead of synchronously
	 * calling MigrateConveyorGroupToChainActor. This follows FactoryGame's chain-level
	 * modification path and avoids the repeated SetStartAndEndConveyors endpoint-order
	 * recovery loop that still leaves mass-upgrade TGs orphaned.
	 */
	int32 InvalidateAndQueueVanillaRebuildForBelts(
		const TArray<AFGBuildableConveyorBase*>& Belts,
		const TSet<AFGConveyorChainActor*>& ExtraChains);

	/**
	 * Upgrade-specific fallback: after the full replacement graph has been connected,
	 * unregister and re-register the upgraded conveyors so FactoryGame assigns bucket
	 * membership from the live graph instead of Smart manually coalescing tick groups.
	 * Used after mass-upgrade endpoint repair and by explicit Triage recovery for
	 * already-loaded bad saves with orphaned conveyor tick groups.
	 */
	int32 ReRegisterAndQueueVanillaRebuildForBelts(
		const TArray<AFGBuildableConveyorBase*>& Belts,
		const TSet<AFGConveyorChainActor*>& ExtraChains);

	/**
	 * Post-load automated diagnostic: report chain actor issues without mutating conveyor state.
	 * Intended to be called once after the world is fully loaded (via a deferred timer from USFSubsystem).
	 * Load-time mutation is intentionally avoided because conveyor chain actors tick on worker threads.
	 */
	void RunPostLoadRepair();

	/**
	 * Destroy all AFGConveyorChainActor instances with zero chain segments (NO_SEGMENTS zombies).
	 * Safe, idempotent. Called by deferred post-upgrade cleanup and explicit repair flows.
	 * Public so UpgradeExecutionService can invoke it directly as a safety-net after
	 * InvalidateAndRebuildForBelts leaves zombies behind. Returns the number of actors destroyed.
	 */
	int32 PurgeZombieChainActors();

	/**
	 * Schedule a deferred call to PurgeZombieChainActors after a short delay, giving
	 * vanilla one or two factory ticks to settle pending migrations before we sweep.
	 * Used as the mass-upgrade safety net: the queued vanilla rebuild may leave inert
	 * NO_SEGMENTS zombies, but automatic orphan/split repair is intentionally avoided
	 * because those paths can recreate chain=NONE or backptr-null failures in live games.
	 */
	void ScheduleDeferredZombiePurge(float DelaySeconds);

	/**
	 * Scan the entire map for invalid chain actor states without making any repairs.
	 * Reports: NO_SEGMENTS zombie chain actors and SPLIT_CHAIN pairs (adjacent flat belts
	 * in different chain actors). Legitimate split boundaries (lift/cross-level) are excluded.
	 *
	 * Intended to be called from the UI Detect button before asking the player to confirm repair.
	 * Safe to call at any time; read-only.
	 */
	FSFChainDiagnosticResult DetectChainActorIssues() const;

	/**
	 * Perform a map-wide chain actor pass:
	 *   Phase 1 — Purge NO_SEGMENTS zombie chain actors (PurgeZombieChainActors).
	 *   Phase 2 — Detect and rebuild SPLIT_CHAIN pairs (RepairSplitChains).
	 *   Phase 3 — Re-register live conveyors from orphaned tick groups after the world has settled.
	 *
	 * This is the explicit repair action for the UI Repair button. Post-load checks are
	 * diagnostic-only and deliberately do not call this path. Returns a result struct
	 * summarising repairs and orphan candidates.
	 */
	FSFChainRepairResult RepairAllChainActorIssues();

	/**
	 * Diagnostic: dump a single failing tick group's topology to a timestamped JSON file
	 * under <Saved>/Logs/ChainDiag_<timestamp>.json so we can inspect the merged array
	 * ordering, per-belt connection neighbours, and chain-actor back-pointers offline.
	 * The optional pre-migrate snapshot lets us compare each belt's mConveyorChainActor
	 * back-pointer before vs after vanilla's Migrate ran — key for confirming whether
	 * stale back-pointers are what causes BuildChain to bail.
	 * No-op if the logs directory cannot be resolved.
	 */
	void DumpFailingTickGroupToJson(
		const FConveyorTickGroup* TG,
		const AFGConveyorChainActor* Zombie,
		const FString& Phase,
		const TArray<TPair<FString, FString>>& PreMigrateBackptrs = TArray<TPair<FString, FString>>()) const;

private:
	/** Resolve BuildableSubsystem from the owning world. */
	AFGBuildableSubsystem* GetBuildableSubsystem() const;

	/**
	 * Find the FConveyorTickGroup that owns a chain. Primary: TG->ChainActor == Chain.
	 * Fallback: TG->Conveyors contains any segment belt from the chain (handles orphan
	 * chains where TG->ChainActor points elsewhere but the belts still live in the group).
	 * Returns nullptr if no owning group can be identified (chain is fully orphaned;
	 * nothing to migrate — vanilla GC cleans it up naturally).
	 */
	FConveyorTickGroup* FindOwningTickGroup(
		AFGBuildableSubsystem* BuildableSub,
		AFGConveyorChainActor* Chain) const;

	/**
	 * Detect and repair SPLIT_CHAIN pairs world-wide.
	 * A SPLIT_CHAIN occurs when belt A's output connects to belt B but A and B belong to
	 * different chain actors. Calls InvalidateAndRebuildChains on all affected chains, with
	 * Phase 2.5 merging to produce correct contiguous runs.
	 * Returns the number of tick groups migrated.
	 */
	int32 RepairSplitChains();

	/**
	 * Finds orphaned tick groups and re-registers their live conveyors through vanilla's
	 * RemoveConveyor/AddConveyor path. Intended only for explicit Triage repair after the
	 * save has loaded and the world is stable, not for automatic load-time mutation.
	 * Returns the number of live conveyors submitted for re-registration.
	 */
	int32 RepairOrphanedBelts(FSFChainRepairResult* OutResult = nullptr);

private:
	UPROPERTY()
	TWeakObjectPtr<USFSubsystem> Subsystem;

	/** Handle for the deferred post-upgrade zombie purge timer. */
	FTimerHandle DeferredPurgeTimerHandle;

	/**
	 * Disabled orphan-bounce compatibility queue.
	 *
	 * Retained only so old object layouts/calls remain harmless while the in-game bounce
	 * path is disabled. The queue is never populated; ProcessNextPendingBounce clears it.
	 */
	UPROPERTY()
	TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> PendingBounceQueue;

	FTimerHandle BounceTimerHandle;

	/** Disabled timer callback retained as a safety stub; it never bounces belts. */
	void ProcessNextPendingBounce();
};
