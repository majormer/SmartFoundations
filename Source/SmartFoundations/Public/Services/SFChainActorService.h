// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

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
 * Narrow factory/wire cleanup; no conveyor topology mutation or chain repair.
 * Extend/Restore and conveyor Upgrade use the retained construction operation;
 * this service is not a second settlement or world-wide recovery owner.
 *
 * Native retirement, worker fences and group/bucket invariants are documented in
 * docs/Reference/THESIS_Belts_ChainActors_Placement.md. Historical failure evidence
 * includes FGConveyorChainActor.cpp:339 and GTestRegisterComponentTickFunctions==0;
 * do not re-test destroyed-chain reuse or reinstallation to rediscover them.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFChainActorService : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(USFSubsystem* InSubsystem);
    void Shutdown();

    /** Existing nonconveyor safety guards; not conveyor group repair. */
    static int32 ScrubNullWireEntries(class AFGBuildable* Buildable);
    /** Retired ABI: native lifecycle alone owns factory-array membership/caches. Returns zero. */
    int32 RemoveBuildableFromFactoryTickArrays(class AFGBuildable* Buildable);
    int32 ScrubFactoryTickArrays();

    /** Read-only diagnostic for a caller-owned native group snapshot. */
    void DumpFailingTickGroupToJson(const FConveyorTickGroup* TG,
        const AFGConveyorChainActor* Zombie, const FString& Phase,
        const TArray<TPair<FString, FString>>& PreMigrateBackptrs = TArray<TPair<FString, FString>>()) const;
private:
    AFGBuildableSubsystem* GetBuildableSubsystem() const;

    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;
};
