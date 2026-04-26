// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "SFUpgradeAuditService.generated.h"

/**
 * Upgrade family types - groups of buildables that can upgrade to each other
 */
UENUM(BlueprintType)
enum class ESFUpgradeFamily : uint8
{
	None = 0,
	Belt,              // Conveyor belts Mk1-Mk6
	Lift,              // Conveyor lifts Mk1-Mk6
	Pipe,              // Pipelines Mk1-Mk2
	Pump,              // Pipeline pumps Mk1-Mk2
	PowerPole,         // Power poles Mk1-Mk3
	WallOutletSingle,  // Wall outlets single-sided Mk1-Mk3
	WallOutletDouble,  // Wall outlets double-sided Mk1-Mk3
	Tower,             // Power towers (audit-only, no upgrade)
	MAX
};

/**
 * Single audit entry for a buildable that can be upgraded
 */
USTRUCT(BlueprintType)
struct FSFUpgradeAuditEntry
{
	GENERATED_BODY()

	/** Reference to the buildable */
	UPROPERTY()
	TWeakObjectPtr<AFGBuildable> Buildable;

	/** Upgrade family this belongs to */
	UPROPERTY()
	ESFUpgradeFamily Family = ESFUpgradeFamily::None;

	/** Current tier (1-based: Mk1=1, Mk2=2, etc.) */
	UPROPERTY()
	int32 CurrentTier = 0;

	/** Maximum available tier for this family (based on unlocks) */
	UPROPERTY()
	int32 MaxAvailableTier = 0;

	/** Distance from scan origin (for sorting/filtering) */
	UPROPERTY()
	float DistanceFromOrigin = 0.0f;

	/** World location of the buildable */
	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	/** Whether this entry is upgradeable (CurrentTier < MaxAvailableTier) */
	bool IsUpgradeable() const { return CurrentTier < MaxAvailableTier && MaxAvailableTier > 0; }
};

/**
 * Tier bucket - groups entries by their current tier within a family
 */
USTRUCT(BlueprintType)
struct FSFUpgradeTierBucket
{
	GENERATED_BODY()

	/** Tier level (1=Mk1, 2=Mk2, etc.) */
	UPROPERTY()
	int32 Tier = 0;

	/** Count of buildables at this tier */
	UPROPERTY()
	int32 Count = 0;

	/** Entries in this bucket (may be empty if only counting) */
	UPROPERTY()
	TArray<FSFUpgradeAuditEntry> Entries;
};

/**
 * Family audit result - all tiers for a single upgrade family
 */
USTRUCT(BlueprintType)
struct FSFUpgradeFamilyResult
{
	GENERATED_BODY()

	/** The upgrade family */
	UPROPERTY()
	ESFUpgradeFamily Family = ESFUpgradeFamily::None;

	/** Total count of buildables in this family */
	UPROPERTY()
	int32 TotalCount = 0;

	/** Count of buildables that can be upgraded */
	UPROPERTY()
	int32 UpgradeableCount = 0;

	/** Tier buckets (sorted by tier ascending) */
	UPROPERTY()
	TArray<FSFUpgradeTierBucket> TierBuckets;

	/** Get display name for this family */
	FString GetFamilyDisplayName() const;
};

/**
 * Complete audit result snapshot
 */
USTRUCT(BlueprintType)
struct FSFUpgradeAuditResult
{
	GENERATED_BODY()

	/** Whether the audit completed successfully */
	UPROPERTY()
	bool bSuccess = false;

	/** Whether the audit is still in progress */
	UPROPERTY()
	bool bInProgress = false;

	/** Progress percentage (0-100) */
	UPROPERTY()
	float ProgressPercent = 0.0f;

	/** Scan origin (player location at start) */
	UPROPERTY()
	FVector ScanOrigin = FVector::ZeroVector;

	/** Scan radius used (0 = save-wide) */
	UPROPERTY()
	float ScanRadius = 0.0f;

	/** Timestamp when audit started */
	UPROPERTY()
	FDateTime StartTime;

	/** Timestamp when audit completed */
	UPROPERTY()
	FDateTime CompletionTime;

	/** Results by family */
	UPROPERTY()
	TArray<FSFUpgradeFamilyResult> FamilyResults;

	/** Total buildables scanned */
	UPROPERTY()
	int32 TotalScanned = 0;

	/** Total upgradeable buildables found */
	UPROPERTY()
	int32 TotalUpgradeable = 0;

	/** Get result for a specific family */
	const FSFUpgradeFamilyResult* GetFamilyResult(ESFUpgradeFamily Family) const;
};

/**
 * Audit scan parameters
 */
USTRUCT(BlueprintType)
struct FSFUpgradeAuditParams
{
	GENERATED_BODY()

	/** Scan origin (typically player location) */
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	/** Scan radius in cm (0 = save-wide scan) */
	UPROPERTY()
	float Radius = 0.0f;

	/** Families to include (empty = all families) */
	UPROPERTY()
	TArray<ESFUpgradeFamily> IncludeFamilies;

	/** Whether to store full entries or just counts */
	UPROPERTY()
	bool bStoreEntries = true;

	/** Maximum entries to store per family (0 = unlimited) */
	UPROPERTY()
	int32 MaxEntriesPerFamily = 0;

	/** Player who requested the audit (for RPC result delivery) */
	UPROPERTY()
	TWeakObjectPtr<class AFGPlayerController> RequestingPlayer;
};

/**
 * Delegate broadcast when audit progress updates
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSFOnAuditProgressUpdated, float, ProgressPercent, int32, ScannedCount);

/**
 * Delegate broadcast when audit completes
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSFOnAuditCompleted, const FSFUpgradeAuditResult&, Result);

/**
 * Service responsible for auditing upgradeable buildables in the world
 * 
 * Features:
 * - Time-sliced scanning to avoid frame hitches
 * - Family-based grouping (belts, lifts, pipes, pumps, power poles, wall outlets)
 * - Tier bucketing within each family
 * - Radius-limited or save-wide scanning
 * - Progress reporting for UI feedback
 */
UCLASS()
class SMARTFOUNDATIONS_API USFUpgradeAuditService : public UObject
{
	GENERATED_BODY()

public:
	// ========================================
	// Initialization & Lifecycle
	// ========================================

	/** Initialize service with owning subsystem reference */
	void Initialize(class USFSubsystem* InSubsystem);

	/** Cleanup service resources */
	void Cleanup();

	/** Tick the service (called from subsystem tick) */
	void Tick(float DeltaTime);

	// ========================================
	// Audit Control
	// ========================================

	/** Start a new audit scan with specified parameters */
	UFUNCTION(BlueprintCallable, Category = "Smart! Upgrade")
	bool StartAudit(const FSFUpgradeAuditParams& Params);

	/** Start a quick audit centered on player with default radius */
	UFUNCTION(BlueprintCallable, Category = "Smart! Upgrade")
	bool StartQuickAudit(float Radius = 0.0f);

	/** Cancel an in-progress audit */
	UFUNCTION(BlueprintCallable, Category = "Smart! Upgrade")
	void CancelAudit();

	/** Check if an audit is currently in progress */
	UFUNCTION(BlueprintPure, Category = "Smart! Upgrade")
	bool IsAuditInProgress() const { return bAuditInProgress; }

	// ========================================
	// Results Access
	// ========================================

	/** Get the most recent audit result */
	UFUNCTION(BlueprintPure, Category = "Smart! Upgrade")
	const FSFUpgradeAuditResult& GetLastResult() const { return LastResult; }

	/** Internal: Inject results from server (called by RCO) */
	void InjectAuditResult(const FSFUpgradeAuditResult& Result);

	/** Check if we have valid cached results */
	UFUNCTION(BlueprintPure, Category = "Smart! Upgrade")
	bool HasValidResults() const { return LastResult.bSuccess; }

	/** Clear cached results */
	UFUNCTION(BlueprintCallable, Category = "Smart! Upgrade")
	void ClearResults();

	// ========================================
	// Family Detection
	// ========================================

	/** Determine the upgrade family for a buildable */
	UFUNCTION(BlueprintPure, Category = "Smart! Upgrade")
	static ESFUpgradeFamily GetUpgradeFamily(AFGBuildable* Buildable);

	/** Determine the tier for a buildable within its family */
	UFUNCTION(BlueprintPure, Category = "Smart! Upgrade")
	static int32 GetBuildableTier(AFGBuildable* Buildable);

	/** Get the maximum available tier for a family (based on unlocks) */
	UFUNCTION(BlueprintCallable, Category = "Smart! Upgrade")
	int32 GetMaxAvailableTier(ESFUpgradeFamily Family) const;

	/** Get display name for an upgrade family */
	UFUNCTION(BlueprintPure, Category = "Smart! Upgrade")
	static FString GetFamilyDisplayName(ESFUpgradeFamily Family);

	// ========================================
	// Events
	// ========================================

	/** Broadcast when audit progress updates */
	UPROPERTY(BlueprintAssignable, Category = "Smart! Upgrade")
	FSFOnAuditProgressUpdated OnAuditProgressUpdated;

	/** Broadcast when audit completes */
	UPROPERTY(BlueprintAssignable, Category = "Smart! Upgrade")
	FSFOnAuditCompleted OnAuditCompleted;

private:
	// ========================================
	// Internal Scanning
	// ========================================

	/** Gather all buildables to scan based on params */
	void GatherBuildablesToScan();

	/** Process a batch of buildables (time-sliced) */
	void ProcessScanBatch();

	/** Finalize audit and build results */
	void FinalizeAudit();

	/** Add entry to appropriate family bucket */
	void AddEntryToResults(const FSFUpgradeAuditEntry& Entry);

	/** Check if a family should be included in current scan */
	bool ShouldIncludeFamily(ESFUpgradeFamily Family) const;

	// ========================================
	// Scan State
	// ========================================

	/** Whether an audit is currently in progress */
	bool bAuditInProgress = false;

	/** Current scan parameters */
	FSFUpgradeAuditParams CurrentParams;

	/** Buildables pending scan */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AFGBuildable>> PendingBuildables;

	/** Current index in pending buildables */
	int32 CurrentScanIndex = 0;

	/** Number of buildables to process per tick */
	int32 BatchSize = 100;

	/** Accumulated results during scan */
	FSFUpgradeAuditResult WorkingResult;

	// ========================================
	// Cached Results
	// ========================================

	/** Last completed audit result */
	UPROPERTY(Transient)
	FSFUpgradeAuditResult LastResult;

	// ========================================
	// References
	// ========================================

	/** Owning subsystem reference */
	UPROPERTY(Transient)
	class USFSubsystem* Subsystem = nullptr;
};
