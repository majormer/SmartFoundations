#pragma once

#include "CoreMinimal.h"
#include "Math/IntVector.h"
#include "Templates/SharedPointer.h"

class AFGHologram;
class ISFHologramAdapter;
class UWorld;

/**
 * Smart! Hologram Helper Service - Manages hologram lifecycle and grid operations
 * 
 * Extracted from SFSubsystem.cpp (Phase 0 Refactoring - Task #61.6)
 * 
 * Responsibilities:
 * - Register/unregister active holograms for Smart! features
 * - Poll for active holograms (auto-detection)
 * - Spawn and manage child hologram grids
 * - Safe deferred destruction of child holograms
 * - Hologram adapter factory (create appropriate adapter for hologram type)
 * - Future: Auto-Connect and Extend feature integration points (stubs)
 * 
 * Dependencies:
 * - Creates and manages hologram actors
 * - Binds to hologram destruction events
 * - Uses hologram adapters for type-specific behavior
 */
class SMARTFOUNDATIONS_API FSFHologramHelperService
{
public:
	// ========================================
	// Constants
	// ========================================

	/** Maximum number of grid children before engine crash (UObject limit protection) */
	static constexpr int32 GRID_CHILDREN_HARD_CAP = 2000;

	/** Threshold for triggering large grid destruction warning */
	static constexpr int32 LARGE_GRID_WARNING_THRESHOLD = 100;

	FSFHologramHelperService();
	~FSFHologramHelperService();

	/**
	 * Initialize the hologram helper service
	 * 
	 * @param InWorld World context
	 */
	void Initialize(UWorld* InWorld);

	/**
	 * Shutdown and cleanup all managed holograms
	 */
	void Shutdown();

	/**
	 * Register a hologram as the active Smart! target
	 * Triggers adapter creation and hologram preparation
	 * 
	 * @param Hologram Hologram to register
	 */
	void RegisterActiveHologram(AFGHologram* Hologram);

	/**
	 * Unregister the active hologram and cleanup
	 * 
	 * @param Hologram Hologram to unregister
	 */
	void UnregisterActiveHologram(AFGHologram* Hologram);

	/**
	 * Poll for active holograms in the world and auto-register
	 * Called periodically by timer
	 */
	void PollForActiveHologram();

	/**
	 * Regenerate the child hologram grid based on current dimensions
	 * Destroys old children and spawns new grid (Phase 2 - Task #61.6)
	 * 
	 * @param ParentHologram Parent hologram to spawn children from
	 * @param GridCounters Grid dimensions (X, Y, Z) - may be modified if size exceeds limit
	 * @param ValidationService Validation service for grid size checks
	 * @param CurrentAdapter Adapter for feature support checks
	 * @param LastController Controller for validation calls
	 * @param BaselineHeightZ Baseline height for first spawn tracking
	 * @param UpdateChildPositionsCallback Callback to update child positions after regeneration
	 */
	void RegenerateChildHologramGrid(
		AFGHologram* ParentHologram,
		FIntVector& GridCounters,
		class FSFValidationService* ValidationService,
		TSharedPtr<class ISFHologramAdapter> CurrentAdapter,
		APlayerController* LastController,
		float& BaselineHeightZ,
		TFunction<void()> UpdateChildPositionsCallback
	);

	/**
	 * Apply scaling delta to hologram and trigger child grid regeneration
	 * Extracted from SFSubsystem::ApplyScalingToHologram (Refactor: Phase 1)
	 * 
	 * @param Hologram Hologram to scale
	 * @param ScalingDelta Delta to apply to scaling offset
	 * @param CurrentScalingOffset Current cumulative scaling offset (will be updated)
	 * @param RegenerateGridCallback Callback to regenerate child hologram grid
	 */
	void ApplyScalingDelta(
		AFGHologram* Hologram,
		const FVector& ScalingDelta,
		FVector& CurrentScalingOffset,
		TFunction<void()> RegenerateGridCallback
	);

	/**
	 * Queue a child hologram for deferred destruction
	 * Destruction happens on next tick to avoid racing build gun validation
	 * 
	 * @param Child Child hologram to destroy
	 */
	void QueueChildForDestroy(AFGHologram* Child);

	/**
	 * Flush pending child hologram destructions
	 * Executes on next tick to ensure build gun state is stable
	 */
	void FlushPendingDestroy();

	/**
	 * Force-destroy all pending children immediately
	 * Use when build gun is unequipped or hologram cleared
	 */
	void ForceDestroyPendingChildren();

	/**
	 * Check if it's safe to destroy children using service state
	 */
	bool CanSafelyDestroyChildren() const;

	/**
	 * Check if it's safe to destroy children without racing build gun validation
	 * 
	 * @param ActiveHologram Currently active parent hologram
	 * @return true if destruction can proceed safely
	 */
	bool CanSafelyDestroyChildren(const AFGHologram* ActiveHologram) const;

	/**
	 * React to child hologram being destroyed
	 * Removes from tracking and updates state
	 * Phase 3: Accepts callback for transform updates
	 * 
	 * @param DestroyedActor Actor that was destroyed
	 * @param UpdateChildrenCallback Callback to update children transforms if needed
	 * @return true if transform update callback was invoked
	 */
	bool OnChildHologramDestroyed(AActor* DestroyedActor, TFunction<void()> UpdateChildrenCallback);

	/**
	 * React to parent hologram being destroyed
	 * Triggers cleanup of all children
	 * 
	 * @param DestroyedActor Actor that was destroyed
	 */
	void OnParentHologramDestroyed(AActor* DestroyedActor);

	/**
	 * Update children when parent transform changes
	 * Phase 3: Handles nudge, movement, rotation
	 * 
	 * @param ParentHologram Active parent hologram
	 * @param OldTransform Previous transform
	 * @param NewTransform Current transform
	 * @param BaselineHeightZ Baseline height for calculations
	 * @param UpdateChildPositionsCallback Callback to reposition children
	 * @param ValidateCallback Callback to validate after repositioning
	 */
	void UpdateChildrenForParentTransform(
		AFGHologram* ParentHologram,
		const FTransform& OldTransform,
		const FTransform& NewTransform,
		float BaselineHeightZ,
		TFunction<void()> UpdateChildPositionsCallback,
		TFunction<void()> ValidateCallback
	);

	/**
	 * Create appropriate adapter for a hologram
	 * Factory method that detects hologram type and creates matching adapter
	 * 
	 * @param Hologram Hologram to create adapter for
	 * @return Shared pointer to adapter, or nullptr if unsupported type
	 */
	TSharedPtr<ISFHologramAdapter> CreateHologramAdapter(AFGHologram* Hologram);

	// ========================================
	// State Queries
	// ========================================

	/** Get currently active hologram */
	AFGHologram* GetActiveHologram() const { return ActiveHologram.Get(); }

	/** Get array of spawned child holograms */
	const TArray<TWeakObjectPtr<AFGHologram>>& GetSpawnedChildren() const { return SpawnedChildren; }

	/** Get current child spawn counter (for unique naming) */
	int32 GetChildSpawnCounter() const { return ChildSpawnCounter; }

	/** Check if child updates are suppressed (during mass operations) */
	bool AreChildUpdatesSuppressed() const { return bSuppressChildUpdates; }

	/** Check if in mass destruction mode */
	bool IsInMassDestruction() const { return bInMassDestruction; }

	// ========================================
	// Lock Management (Phase 1 - Task #61.6)
	// ========================================

	/**
	 * Temporarily unlock a child hologram for positioning updates
	 * Should be called before SetActorLocation/Rotation on locked children
	 * 
	 * @param ChildHologram Child to unlock
	 * @param bParentWasLocked Whether parent hologram is currently locked
	 * @return true if child was unlocked (and needs restore later)
	 */
	bool TemporarilyUnlockChild(AFGHologram* ChildHologram, bool bParentWasLocked);

	/**
	 * Restore lock state to child hologram after positioning
	 * Should be called after positioning updates complete
	 * 
	 * @param ChildHologram Child to restore lock on
	 * @param bParentWasLocked Whether parent hologram is currently locked
	 * @param bSuppressUpdates Whether child updates are suppressed (skip locking if true)
	 */
	void RestoreChildLock(AFGHologram* ChildHologram, bool bParentWasLocked, bool bSuppressUpdates);

	// ========================================
	// Performance Optimization (Phase 2)
	// ========================================

	/**
	 * Begin batch reposition operation - suppress transform cascades
	 * Enables transform guard to prevent O(n²) cascade validation during bulk positioning
	 * Must be paired with EndRepositionChildren()
	 */
	void BeginRepositionChildren();

	/**
	 * End batch reposition operation - restore normal transform behavior
	 * Logs elapsed time for profiling
	 * Must be paired with BeginRepositionChildren()
	 */
	void EndRepositionChildren();

	/**
	 * Check if currently in batch reposition mode
	 * During batch reposition, transform cascades are suppressed for performance
	 * 
	 * @return true if BeginRepositionChildren() has been called without matching End
	 */
	bool IsInBatchReposition() const { return bInBatchReposition; }

	// ========================================
	// UObject Warning System (Phase 5)
	// ========================================

	/** UObject utilization warning levels */
	enum class EUObjectWarningLevel : uint8
	{
		None,      // < 50% headroom used
		Yellow,    // 50-75% headroom used
		Orange,    // 75-90% headroom used
		Red,       // 90-95% headroom used
		Critical   // > 95% headroom used
	};

	/**
	 * Check UObject utilization and display warnings if needed
	 * Called during grid regeneration to warn users about memory limits
	 * 
	 * @param ChildCount Number of children in grid
	 * @param GridCounters Grid dimensions for display
	 * @return Warning level, Critical triggers hard cap
	 */
	EUObjectWarningLevel CheckUObjectUtilization(int32 ChildCount, const FIntVector& GridCounters);

	// ========================================
	// Progressive Batch Reposition (Phase 4)
	// ========================================

	/** Grid index for progressive batching */
	struct FGridIndex
	{
		int32 X;
		int32 Y;
		int32 Z;
		int32 ChildArrayIndex;  // Index in SpawnedChildren array
	};

	/**
	 * Begin progressive batch reposition operation
	 * Spreads child positioning across multiple frames to eliminate freezes
	 * 
	 * @param GridIndices Pre-computed grid positions for all children
	 * @param UpdateCallback Called once per child to perform positioning
	 * @param CompletionCallback Called when all children positioned
	 * @param ParentHologram Parent hologram (checked for validity each frame)
	 */
	void BeginProgressiveBatchReposition(
		const TArray<FGridIndex>& GridIndices,
		TFunction<void(int32)> UpdateCallback,
		TFunction<void()> CompletionCallback,
		AFGHologram* ParentHologram
	);

	/**
	 * Tick progressive batch reposition (process one batch per frame)
	 * Called from USFSubsystem::Tick()
	 * 
	 * @param DeltaTime Time since last frame
	 */
	void TickProgressiveBatchReposition(float DeltaTime);

	/**
	 * Cancel progressive batch reposition (e.g., parent destroyed)
	 */
	void CancelProgressiveBatchReposition();

	/**
	 * Check if progressive batch is active
	 * @return true if batch reposition in progress
	 */
	bool IsProgressiveBatchActive() const { return bProgressiveBatchActive; }

	/**
	 * Get current batch progress
	 * @return Progress from 0.0 to 1.0
	 */
	float GetProgressiveBatchProgress() const;

	/**
	 * Get grid index for current batch operation (accessed by callback)
	 * @param IndexInBatch Index within current batch
	 * @return Grid index data
	 */
	const FGridIndex& GetBatchGridIndex(int32 IndexInBatch) const;

	// ========================================
	// Future Feature Integration Points
	// (Stubs for Auto-Connect and Extend features)
	// ========================================

	/**
	 * Check if two holograms can be auto-connected
	 * STUB - To be implemented for Auto-Connect feature
	 * 
	 * @param Source Source hologram
	 * @param Target Target hologram
	 * @return true if auto-connect is possible
	 */
	bool CanAutoConnect(const AFGHologram* Source, const AFGHologram* Target) const;

	/**
	 * Apply auto-connection between two holograms
	 * STUB - To be implemented for Auto-Connect feature
	 * 
	 * @param Source Source hologram
	 * @param Target Target hologram
	 */
	void ApplyAutoConnect(AFGHologram* Source, AFGHologram* Target);

	/**
	 * Check if hologram can be extended
	 * STUB - To be implemented for Extend feature
	 * 
	 * @param Hologram Hologram to check
	 * @return true if extend is possible
	 */
	bool CanExtend(const AFGHologram* Hologram) const;

	/**
	 * Apply extend operation to hologram
	 * STUB - To be implemented for Extend feature
	 * 
	 * @param Hologram Hologram to extend
	 */
	void ApplyExtend(AFGHologram* Hologram);

private:
	/** World context */
	TWeakObjectPtr<UWorld> WorldContext;

	/** Currently active hologram for Smart! features */
	TWeakObjectPtr<AFGHologram> ActiveHologram;

	/** Array of spawned child holograms */
	TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren;

	/** Pending children to destroy safely on next tick */
	TArray<TWeakObjectPtr<AFGHologram>> PendingDestroyChildren;

	/** Global counter for unique child names (prevents name collisions) */
	int32 ChildSpawnCounter = 0;

	/** Flag to prevent update cascades during mass child destruction */
	bool bSuppressChildUpdates = false;

	/** Flag set when large grid destruction detected */
	bool bInMassDestruction = false;

	/** Whether a flush has been scheduled for next tick */
	bool bPendingDestroyScheduled = false;

	// ========================================
	// Performance Optimization State (Phase 2)
	// ========================================

	/** Flag indicating batch reposition operation in progress */
	bool bInBatchReposition = false;

	/** Start time of batch reposition (for elapsed time logging) */
	double BatchRepositionStartTime = 0.0;

	// ========================================
	// Progressive Batch Reposition State (Phase 4)
	// ========================================

	/** Batch state structure */
	struct FProgressiveBatchState
	{
		// Grid data
		TArray<FGridIndex> GridIndices;
		int32 TotalChildren = 0;
		int32 CurrentIndex = 0;
		
		// Callbacks
		TFunction<void(int32)> UpdateCallback;
		TFunction<void()> CompletionCallback;
		
		// Progress tracking
		double StartTime = 0.0;
		int32 FrameCount = 0;
		
		// Parent hologram (weak ref - check validity each tick)
		TWeakObjectPtr<AFGHologram> ParentHologram;
		
		// Batch config
		int32 ChildrenPerFrame = 200;
		
		// Reset to default state
		void Reset()
		{
			GridIndices.Empty();
			TotalChildren = 0;
			CurrentIndex = 0;
			UpdateCallback = nullptr;
			CompletionCallback = nullptr;
			StartTime = 0.0;
			FrameCount = 0;
			ParentHologram.Reset();
			ChildrenPerFrame = 200;
		}
	};

	/** Progressive batch state */
	FProgressiveBatchState BatchState;

	/** Flag indicating progressive batch is active */
	bool bProgressiveBatchActive = false;

	// ========================================
	// UObject Warning System State (Phase 5)
	// ========================================

	/** Current warning level */
	EUObjectWarningLevel CurrentWarningLevel = EUObjectWarningLevel::None;

	/** Last grid size that triggered a warning (prevent spam) */
	int32 LastWarningGridSize = 0;

	// ========================================
	// Zoop Conflict Detection (Issue #160)
	// ========================================

	/** Flag indicating Zoop is currently active and Smart! scaling is disabled */
	bool bZoopActive = false;

public:
	/** Check if Zoop is currently active (Smart! scaling disabled) */
	bool IsZoopActive() const { return bZoopActive; }

	/** Clear Zoop flag (called when hologram changes) */
	void ClearZoopState() { bZoopActive = false; }

private:

	/**
	 * Complete progressive batch reposition
	 * Logs results and fires completion callback
	 */
	void CompleteBatchReposition();

	/**
	 * Spawn a single child hologram at specified position/rotation
	 * 
	 * @param ParentHologram Parent to spawn from
	 * @param ChildName Unique name for child (prevents collision assertions)
	 * @param Position World position for child
	 * @param Rotation World rotation for child
	 * @return Spawned child hologram, or nullptr if failed
	 */
	AFGHologram* SpawnChildHologram(
		AFGHologram* ParentHologram,
		FName ChildName,
		const FVector& Position,
		const FRotator& Rotation
	);

	/**
	 * Destroy all current child holograms
	 * Helper for regenerating grid
	 */
	void DestroyAllChildren();
};
