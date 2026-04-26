#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/UniquePtr.h"
#include "InputActionValue.h"
#include "Math/IntVector.h"

// Type includes - needed for inline members (enums, structs used by value)
#include "Features/Spacing/SFSpacingTypes.h"
#include "Features/Scaling/SFScalingTypes.h"
#include "Features/Arrows/FSFArrowTypes.h"
#include "HUD/SFHUDTypes.h"
#include "Config/Smart_ConfigStruct.h"

// Service includes - needed for FSFBuildingMetadata and FSplinePointData struct members
#include "Services/SFRecipeManagementService.h"
#include "Features/Extend/SFExtendService.h"

// TUniquePtr member includes - complete type required for destructor generation
#include "Subsystem/SFInputHandler.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFValidationService.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Features/Arrows/SFArrowModule_StaticMesh.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"

// Forward declaration for chain actor system
class AFGBuildableSubsystem;
class AFGBlueprintProxy;

// Forward declarations - replaces heavy includes (PIMPL-style)
class AFGPlayerController;
class AFGBuildableConveyorBelt;
class AFGHologram;
class AFGSplineHologram;
class AFGBuildable;
class AFGBuildableFactory;
class AFGBuildablePowerPole;
class AFGBuildablePipeline;
class AFGBuildableManufacturer;
class UFGRecipe;
class UFGItemDescriptor;
class UInputAction;
class UTexture2D;
class UCanvas;
class AHUD;
class UUserWidget;

// Smart! service forward declarations (TUniquePtr members are included above)
// Note: FSFPipeAutoConnectManager and FSFPowerAutoConnectManager are now included

// Smart! UObject service forward declarations
class USFAutoConnectService;
class USFAutoConnectOrchestrator;
class USFExtendService;
class USFRadarPulseService;
class USFRecipeManagementService;
class USFHudService;
class USFHintBarService;
class USFUpgradeAuditService;
class USFUpgradeExecutionService;
class USFGridStateService;
class USFGridSpawnerService;
class USFGridTransformService;
class USFChainActorService;

// Smart! hologram forward declarations
class ASFBuildableHologram;
class ASFFactoryHologram;
class ASFLogisticsHologram;
class ASFFoundationHologram;
class USmartSettingsFormWidget;

// Adapter interface forward declaration
class ISFHologramAdapter;

#ifndef SMART_ARROWS_ENABLED
#define SMART_ARROWS_ENABLED 1  // Arrow visualization enabled (Task 17)
#endif

/** Maximum grid size limit to prevent UObject exhaustion */
#ifndef SMART_MAX_GRID_SIZE
#define SMART_MAX_GRID_SIZE INT_MAX
#endif

#include "SFSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSFHologramLifecycleEvent, class AFGHologram*, Hologram);

/**
 * Smart! World Subsystem - Modern SML 3.11.x approach
 * Handles hologram scaling, input management, and Smart! features
 * 
 * INPUT DOCUMENTATION: See docs/Input/SMART_INPUT_SYSTEM.md for complete input reference
 */
UCLASS(BlueprintType, Blueprintable)
class SMARTFOUNDATIONS_API USFSubsystem : public UWorldSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
    /** Ensure GridSpawnerService exists (lazy init) */
    /** Get grid spawner service (creates on first access) - implemented in .cpp */
    USFGridSpawnerService* GetGridSpawnerService();
	USFSubsystem();

	/** Destructor - defaulted inline since header includes complete TUniquePtr types */
	virtual ~USFSubsystem() = default;

	/** UWorldSubsystem interface */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	/** FTickableGameObject interface - for progressive batch processing (Phase 4) */
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }
	virtual bool IsTickableWhenPaused() const override { return false; }
	virtual bool IsTickable() const override { return true; }
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }

    /** Arrow update tick (called via timer) */
    UFUNCTION()
    void TickArrows();

    /** React when a spawned child hologram is destroyed */
    UFUNCTION()
    void OnChildHologramDestroyed(AActor* DestroyedActor);

    /** React when the parent hologram is destroyed (e.g., when building is placed) */
    UFUNCTION()
    void OnParentHologramDestroyed(AActor* DestroyedActor);

	/** Get the Smart! subsystem instance */
	UFUNCTION(BlueprintCallable, Category = "Smart!")
	static USFSubsystem* Get(const UObject* WorldContext);

	/** Hologram lifecycle delegates for external integration (SmartCamera uses these) */
	UPROPERTY(BlueprintAssignable, Category = "Smart!|Holograms")
	FOnSFHologramLifecycleEvent OnHologramCreated;

	UPROPERTY(BlueprintAssignable, Category = "Smart!|Holograms")
	FOnSFHologramLifecycleEvent OnHologramDestroyed;

	/** Input system management */
	UFUNCTION(BlueprintCallable, Category = "Smart! Input")
	void SetupPlayerInput(AFGPlayerController* PlayerController);

	/** Check and setup input periodically */
	UFUNCTION()
	void CheckForPlayerController();

	/** Rebind shortly after SetupPlayerInput to catch post-initialization state */
	UFUNCTION()
	void RebindAfterDelay();

	/** Deferred post-load chain diagnostic; does not mutate conveyor state. */
	void RunPostLoadChainRepair();

    // ========================================
    // Facade Accessors for Grid Spawner Service (Phase 2)
    // ========================================
public:
    const FIntVector& GetGridCounters() const { return GridCounters; }
    FIntVector& AccessGridCountersRef() { return GridCounters; }
    const FVector& GetCachedBuildingSize() const { return CachedBuildingSize; }
    const FVector& GetLastLoggedItemSize() const { return LastLoggedItemSize; }
    void SetLastLoggedItemSize(const FVector& In) { LastLoggedItemSize = In; }
    FSFValidationService* GetValidationService() const { return ValidationService.Get(); }
    FSFPositionCalculator* GetPositionCalculator() const { return PositionCalculator.Get(); }
    AFGPlayerController* GetLastController() const { return LastController.Get(); }
    bool IsArrowsRuntimeVisible() const { return bArrowsRuntimeVisible; }
    FSFArrowModule_StaticMesh* GetArrowModule() const { return ArrowModule.Get(); }
    TSharedPtr<class ISFHologramAdapter> GetCurrentAdapter() const { return CurrentAdapter; }
    ELastAxisInput GetLastAxisInput() const { return LastAxisInput; }
    bool IsSuppressChildUpdates() const { return bSuppressChildUpdates; }
    float& AccessBaselineHeightZRef() { return BaselineHeightZ; }
    const FVector& GetCachedAnchorOffset() const { return CachedAnchorOffset; }

	// ========================================
	// RPC Handler Methods (Called by SFRCO)
	// ========================================

	/**
	 * Apply scaling offset from server RPC
	 * Called by SFRCO after server validates the request
	 */
	void ApplyScalingFromRPC(AFGHologram* HologramActor, uint8 Axis, int32 Delta, int32 NewCounter);

	/**
	 * Reset all scaling offsets from server RPC
	 * Called by SFRCO after server validates the request
	 */
	void ResetScalingFromRPC(AFGHologram* HologramActor);

	/**
	 * Set spacing mode from server RPC
	 * Called by SFRCO after server validates the request
	 */
	void SetSpacingModeFromRPC(ESFSpacingMode NewMode);

    /** Set arrow visibility from server RPC
     * Called by SFRCO after server validates the request
     */
    void SetArrowVisibilityFromRPC(bool bVisible);

	/** Poll for active holograms and auto-register them */
	UFUNCTION()
	void PollForActiveHologram();

	// ========================================
	// Enhanced Input Action Handlers (Axis1D-based)
	// ========================================
	
	/** Grid Scaling - Axis1D handlers (replace ±X/Y/Z button pairs) */
	UFUNCTION()
	void OnScaleXChanged(const FInputActionValue& Value);

	UFUNCTION()
	void OnScaleYChanged(const FInputActionValue& Value);

	UFUNCTION()
	void OnScaleZChanged(const FInputActionValue& Value);

	/** Mouse Wheel - Unified context-aware handler
	 * Routes mouse wheel input to the active Smart! feature:
	 * - Spacing mode → Adjusts spacing counter
	 * - Steps mode → Adjusts steps counter
	 * - Stagger mode → Adjusts stagger counter
	 * - Default (no mode) → Continuous scaling
	 * Replaces chord-based mouse wheel bindings with code-based routing */
	UFUNCTION()
	void OnMouseWheelChanged(const FInputActionValue& Value);

	/** Grid Scaling - Modifier tracking for Z-axis wheel detection */
	UFUNCTION()
	void OnModifierScaleXPressed(const FInputActionValue& Value);
	
	UFUNCTION()
	void OnModifierScaleXReleased(const FInputActionValue& Value);

	UFUNCTION()
	void OnModifierScaleYPressed(const FInputActionValue& Value);
	
	UFUNCTION()
	void OnModifierScaleYReleased(const FInputActionValue& Value);

	/** Spacing - Mode tracking + Axis adjustment */
	UFUNCTION()
	void OnSpacingModeChanged(const FInputActionValue& Value);

	UFUNCTION()
	void OnSpacingCycleAxis();

	/** Steps - Mode tracking + Axis adjustment */
	UFUNCTION()
	void OnStepsModeChanged(const FInputActionValue& Value);

	/** Generic Cycle Axis - Context-aware mode swapper for all modal features
	 * Works with whichever mode is currently active:
	 * - Spacing mode (Semicolon) → Cycles spacing axis
	 * - Steps mode (I) → Cycles steps axis
	 * - Stagger mode (Y) → Cycles stagger axis (lateral grid offset)
	 * Bound to Num0 without chord - detects active mode automatically */
	UFUNCTION()
	void OnCycleAxis();

	/** Stagger - Mode tracking + Axis adjustment */
	UFUNCTION()
	void OnStaggerModeChanged(const FInputActionValue& Value);

	/** Rotation - Mode tracking + Axis adjustment (Radial/Arc placement) */
	UFUNCTION()
	void OnRotationModeChanged(const FInputActionValue& Value);

	/** UNIFIED VALUE ADJUSTMENT - Context-aware handler for all modal features
	 * Routes increase/decrease input to the currently active mode:
	 * - Spacing mode → Adjusts spacing counter
	 * - Steps mode → Adjusts steps counter
	 * - Stagger mode → Adjusts stagger counter
	 * - Default (no mode) → Adjusts ScaleX counter
	 * Replaces: OnSpacingAdjustChanged, OnStepsAdjustChanged, OnStaggerAdjustChanged */
	UFUNCTION()
	void OnValueIncreased(const FInputActionValue& Value);

	UFUNCTION()
	void OnValueDecreased(const FInputActionValue& Value);

	/** Toggle Arrows + Debug Spline Analyzer (Num1) */
    UFUNCTION()
    void OnToggleArrows();
    
	/** Settings Form Interface (Phase 0 Validation) */
	UFUNCTION()
	void OnToggleSettingsForm();

	/** Smart Upgrade Panel - opens when holding upgrade-capable hologram (belt/lift/pipe/pump/power pole/wall outlet) */
	UFUNCTION()
	void OnToggleUpgradePanel();

	/** Clear the upgrade panel widget reference (called by widget on close) */
	void ClearUpgradePanelWidget() { UpgradePanelWidget.Reset(); }

	/** Close button handler for fallback mode (when Blueprint isn't using USmartUpgradePanel) */
	UFUNCTION()
	void OnUpgradePanelCloseClicked();

	/** Check if current context supports Smart Upgrade (holding upgrade-capable hologram) */
	UFUNCTION(BlueprintCallable, Category = "Smart! Upgrade")
	bool IsUpgradeCapableContext() const;

	/** Debug: analyze nearby pipe splines (PrimaryFire debug key) */
	UFUNCTION()
	void OnDebugPrimaryFire();

	/** Find nearby buildings within specified radius (used by Auto-Connect Service) */
	TArray<AFGBuildable*> FindNearbyBuildings(FVector Center, float Radius);

	/** Hologram management */
	UFUNCTION(BlueprintCallable, Category = "Smart! Holograms")
	void RegisterActiveHologram(AFGHologram* Hologram);

	UFUNCTION(BlueprintCallable, Category = "Smart! Holograms")
	void UnregisterActiveHologram(AFGHologram* Hologram);

	AFGHologram* GetActiveHologram() const { return ActiveHologram.Get(); }
	
	AFGPlayerController* GetLastPlayerController() const { return LastController.Get(); }

	/** Build gun state tracking for diagnostics */
	UFUNCTION(BlueprintCallable, Category = "Smart! Debug")
	void OnBuildGunEquipped();

	UFUNCTION(BlueprintCallable, Category = "Smart! Debug")
	void OnBuildGunUnequipped();

	/** Track build gun state changes and log active input contexts */
	UFUNCTION(BlueprintCallable, Category = "Smart! Debug")
	void OnBuildGunStateChanged(const FString& NewStateName, const FString& OldStateName);

	/** Log all currently active input mapping contexts */
	UFUNCTION(BlueprintCallable, Category = "Smart! Debug")
	void LogActiveInputContexts(const FString& CallerContext);

	// ========================================
	// Build HUD Counter Display
	// ========================================

	  /** Build formatted counter display strings for HUD overlay
	 * Returns pair of strings: FirstLine (grid dimensions, offsets), SecondLine (mode indicators, warnings)
	 * Example: FirstLine = "4x3x2 P:1.5/2.0/0m", SecondLine = "Spacing:X+Y"
	 */
	TPair<FString, FString> BuildCounterDisplayLines() const;

	/** Get clean display name for recipe (removes Recipe_ prefix and _C suffix) */
	FString GetRecipeDisplayName(TSubclassOf<UFGRecipe> Recipe) const;

	  /** Get the primary product's icon texture for a recipe */
  UTexture2D* GetRecipePrimaryProductIcon(TSubclassOf<UFGRecipe> Recipe) const;

	/** Get recipe display name with first ingredient shown */
	FString GetRecipeWithIngredient(TSubclassOf<UFGRecipe> Recipe) const;

	  /** Get recipe display name with inputs and outputs as indented lines */
  FString GetRecipeWithInputsOutputs(TSubclassOf<UFGRecipe> Recipe) const;

  /** Get current recipe index and total for HUD display (proxy to recipe service) */
  void GetRecipeDisplayInfo(int32& OutCurrentIndex, int32& OutTotalRecipes) const;

  /** Get the active recipe class for HUD (proxy to recipe service) */
  TSubclassOf<UFGRecipe> GetActiveRecipe() const;

	/** Update HUD widget text (will be implemented with widget in 34.2+)
	 * For now, just builds and logs the strings
	 */
	  void UpdateCounterDisplay();

	/** Update widget visibility and text based on content */
	void UpdateWidgetDisplay(const FString& FirstLine, const FString& SecondLine);

	/** Draw counter text directly to HUD canvas */
	  void DrawCounterToHUD(AHUD* HUD, UCanvas* Canvas);

  // Lightweight modal-state accessors for HUD service (implemented in .cpp due to forward declarations)
  bool IsSpacingModeActive() const;
  bool IsStepsModeActive() const;
  bool IsStaggerModeActive() const;
  bool IsRotationModeActive() const;
  bool IsRecipeModeActive() const { return bRecipeModeActive; }
  bool IsAutoConnectSettingsModeActive() const { return bAutoConnectSettingsModeActive; }
  bool IsExtendModeActive() const;

  // Note: EXTEND mode is AUTOMATIC - no toggle needed!
  // Activates when pointing at a compatible building of the same type

  // Axis accessors - read from CounterState (authoritative source)
  ESFScaleAxis GetCurrentSpacingAxis() const { return GetCounterState().SpacingAxis; }
  ESFScaleAxis GetCurrentStepsAxis() const { return GetCounterState().StepsAxis; }
  ESFScaleAxis GetCurrentStaggerAxis() const { return GetCounterState().StaggerAxis; }
  ESFScaleAxis GetCurrentRotationAxis() const { return GetCounterState().RotationAxis; }

  // Modifier accessors for scaling highlight (implemented in .cpp due to forward declarations)
  bool IsModifierScaleXActive() const;
  bool IsModifierScaleYActive() const;

	// ========================================
	// Service Accessors
	// ========================================

	/** Get hologram helper service */
	FSFHologramHelperService* GetHologramHelper() const { return HologramHelper.Get(); }

	/** Get auto-connect service */
	USFAutoConnectService* GetAutoConnectService() const { return AutoConnectService; }

	/** Get extend service */
	USFExtendService* GetExtendService() const { return ExtendService; }

	/** Get radar pulse diagnostic service */
	USFRadarPulseService* GetRadarPulseService() const { return RadarPulseService; }

	/** Check if Smart! is actively scaling (grid > 1x1x1) — any axis with abs > 1 */
	bool IsSmartScalingActive() const
	{
		const FIntVector& G = GetCounterState().GridCounters;
		return FMath::Abs(G.X) > 1 || FMath::Abs(G.Y) > 1 || FMath::Abs(G.Z) > 1;
	}

	/** Get recipe management service */
	USFRecipeManagementService* GetRecipeManagementService() const { return RecipeManagementService; }

	/** Get upgrade audit service */
	USFUpgradeAuditService* GetUpgradeAuditService() const { return UpgradeAuditService; }

	/** Get upgrade execution service */
	USFUpgradeExecutionService* GetUpgradeExecutionService() const { return UpgradeExecutionService; }

	/** Get or create orchestrator for a parent distributor hologram */
	USFAutoConnectOrchestrator* GetOrCreateOrchestrator(AFGHologram* ParentHologram);

	/** Called when a power pole is built to create actual power connections */
	void OnPowerPoleBuilt(class AFGBuildablePowerPole* BuiltPole);

	/** Check if power pole connections are ready for auto-connect */
	bool ArePowerConnectionsReady(class AFGBuildablePowerPole* PowerPole);

	/** Process deferred power pole connections */
	void ProcessDeferredPowerPoleConnections();

	/** Add power pole to deferred connection queue */
	void QueuePowerPoleForDeferredConnection(class AFGBuildablePowerPole* PowerPole);

	/** Register a power pole as built from the grid system */
	void RegisterGridBuiltPowerPole(class AFGBuildablePowerPole* PowerPole);

	/** Check if a power pole was built from the grid system */
	bool IsGridBuiltPowerPole(class AFGBuildablePowerPole* PowerPole);

	/** Cleanup invalid entries from grid-built poles registry */
	void CleanupGridBuiltPowerPoles();

	/** Get all grid-built power poles (for position-based lookup) */
	const TArray<TWeakObjectPtr<AFGBuildablePowerPole>>& GetGridBuiltPowerPoles();

	/** 
	 * Register a pipe for deferred wiring after all junctions are built.
	 * Called from SFPipelineHologram::Construct() since OnActorSpawned fires
	 * before tags are transferred to the built actor.
	 * @param Pipe The built pipeline to register for deferred wiring
	 */
	void RegisterPipeForDeferredWiring(class AFGBuildablePipeline* Pipe);

	/** 
	 * Planned building connections from preview phase (for build-time execution)
	 * Key: Building that should be connected (weak pointer to avoid dangling refs)
	 * Value: Location of the pole hologram that won the bid for this building
	 * This map is populated during preview and consumed during build.
	 * Public so power managers can access it from both service and subsystem.
	 */
	TMap<TWeakObjectPtr<AFGBuildable>, FVector> PlannedBuildingConnections;
	
	/**
	 * Cached/committed building connections - snapshot taken when build starts.
	 * This prevents the NEW parent hologram (spawned after build) from overwriting
	 * the mappings that the build phase needs. The build phase reads from this cache.
	 * Uses weak pointers to safely handle buildings that may be destroyed before timer fires.
	 */
	TMap<TWeakObjectPtr<AFGBuildable>, FVector> CommittedBuildingConnections;
	
	/**
	 * Planned pole-to-pole connections from preview phase (for build-time execution)
	 * Stores pairs of pole locations that should be connected.
	 * This is essential for rotated grids where world-space axis alignment doesn't work.
	 */
	TArray<TPair<FVector, FVector>> PlannedPoleConnections;
	
	/**
	 * DEFERRED pole connections queue - persists across multiple builds until consumed.
	 * When a build happens, planned connections are ADDED to this queue (not overwritten).
	 * When poles spawn and connect, the used connections are REMOVED from this queue.
	 * This allows multiple overlapping builds without race conditions.
	 */
	TArray<TPair<FVector, FVector>> DeferredPoleConnections;
	
	/** Cache the current PlannedBuildingConnections for build phase consumption */
	void CommitBuildingConnections();
	
	/** Remove a connection from the deferred queue (called after successful connection) */
	void RemoveDeferredPoleConnection(const FVector& PoleA, const FVector& PoleB);
	
	/** Check if there are any deferred pole connections waiting */
	bool HasDeferredPoleConnections() const { return DeferredPoleConnections.Num() > 0; }

	/** Clear all deferred pole connections (called when grid axis changes or hologram unequipped) */
	void ClearDeferredPoleConnections();
	
	/** Get the deferred pole connections (for OnPowerPoleBuilt to use) */
	const TArray<TPair<FVector, FVector>>& GetDeferredPoleConnections() const { return DeferredPoleConnections; }

	/** Count unique pole connections (treats A→B and B→A as same connection) */
	int32 GetUniqueDeferredPoleConnectionCount() const;

	/** Add a planned pole-to-pole connection */
	void AddPlannedPoleConnection(const FVector& PoleA, const FVector& PoleB);

	/** Clear planned pole connections */
	void ClearPlannedPoleConnections() { PlannedPoleConnections.Empty(); }

protected:

	/** Currently active hologram for scaling operations */
	UPROPERTY(BlueprintReadOnly, Category = "Smart! State")
	TWeakObjectPtr<AFGHologram> ActiveHologram;

	/** Current scaling offset accumulator (DEPRECATED - use GridCounters) */
	UPROPERTY(BlueprintReadOnly, Category = "Smart! Scaling")
	FVector CurrentScalingOffset = FVector::ZeroVector;

	/** Grid array counters (DEPRECATED - use CounterState.GridCounters)
	 * Kept temporarily for Blueprint compatibility during refactoring
	 * CRITICAL: Zero is never allowed - counters represent number of items!
	 * Positive = build forward/right/up, Negative = build backward/left/down
	 * Initial state is (1,1,1) = 1 foundation at origin */
	UPROPERTY(BlueprintReadOnly, Category = "Smart! Scaling")
	FIntVector GridCounters = FIntVector(1, 1, 1);

	/** Scaling step size in Unreal units (50 = 0.5m) */
	UPROPERTY(EditDefaultsOnly, Category = "Smart! Config")
	float ScaleStepSize = 50.0f;

    /** Are axis arrows currently visible (DEPRECATED - use bArrowsRuntimeVisible instead)
     * @deprecated Use bArrowsRuntimeVisible for runtime visibility state
     */
    UPROPERTY(BlueprintReadOnly, Category = "Smart! Features", meta = (DeprecatedProperty, DeprecationMessage = "Use bArrowsRuntimeVisible instead"))
    bool bArrowsVisible = true;

	/** Has input been set up for the current player */
	UPROPERTY()
	bool bInputSetupCompleted = false;

    /** Last axis input for arrow highlighting (non-replicated, local visualization state) */
    ELastAxisInput LastAxisInput = ELastAxisInput::None;

	/** Cached hologram transform to detect movement (optimization: only update arrows when hologram moves) */
	FTransform LastHologramTransform;

	/** Cached axis input to detect changes (optimization: only update visibility when input changes) */
	ELastAxisInput LastKnownAxisInput = ELastAxisInput::None;
	
	/** Cached child count to detect grid structure changes (triggers arrow position updates) */
	int32 LastChildCount = 0;
	
	/** Cached building size from adapter (calculated once at hologram registration, reused for all positioning) */
	FVector CachedBuildingSize = FVector(800.0f, 800.0f, 100.0f);
	
	/** Cached anchor offset for attachment-type pivot compensation (from registry profile) */
	FVector CachedAnchorOffset = FVector::ZeroVector;

	/** Cached multi-step hologram properties for child sync (Issue #200)
	 * Tracks parent's fixture angle and build step to detect changes and propagate to children */
	int32 CachedParentFixtureAngle = INT_MIN;
	uint8 CachedParentBuildStep = 0;

	/** Sync multi-step hologram properties (e.g. floodlight angle) from parent to children (Issue #200) */
	void SyncMultiStepHologramProperties();

	/** Timer handle for checking player controller availability */
	FTimerHandle PlayerControllerCheckTimer;

	/** Timer for deferred rebind */
	FTimerHandle DeferredRebindTimer;

	/** Timer for hologram polling */
	FTimerHandle HologramPollTimer;

	/** Timer for context monitoring */
	FTimerHandle ContextMonitorTimer;

	/** Timer for arrow tick updates */
	FTimerHandle ArrowTickTimer;

	/** Timer for periodic cleanup of dead hologram entries */
	FTimerHandle PeriodicCleanupTimer;

	/** Timer for debounced recipe regeneration */
	FTimerHandle RecipeRegenerationTimer;

	/** Timer for deferred power pole connections */
	FTimerHandle PowerPoleDeferredTimer;

	/** One-shot timer that runs post-load chain diagnostics. */
	FTimerHandle PostLoadChainRepairTimer;

	/** Pending power poles waiting for connections */
	UPROPERTY()
	TArray<TWeakObjectPtr<AFGBuildablePowerPole>> PendingPowerPoleConnections;

	/** Registry of power poles built from the grid system */
	UPROPERTY()
	TArray<TWeakObjectPtr<AFGBuildablePowerPole>> GridBuiltPowerPoles;

	/** Flag to track if wire costs have been deducted for the current build cycle */
	bool bPowerCostsDeductedThisCycle = false;

	/** Last detected player controller */
	UPROPERTY()
	TWeakObjectPtr<AFGPlayerController> LastController;

	/** Settings Form Widget (Phase 0 Validation) */
	UPROPERTY()
	TWeakObjectPtr<class USmartSettingsFormWidget> SettingsFormWidget;

	/** Settings Form Widget Class (Blueprint reference) - initialized in constructor */
	UPROPERTY(EditDefaultsOnly, Category = "Smart! UI")
	TSubclassOf<class USmartSettingsFormWidget> SettingsFormWidgetClass;

	/** Smart Upgrade Panel Widget */
	UPROPERTY()
	TWeakObjectPtr<class UUserWidget> UpgradePanelWidget;

	/** Track last known build gun state for diagnostics */
	UPROPERTY()
	FString LastBuildGunState;

    /** Last recipe we logged from the build gun (for change detection) */
    UPROPERTY()
    UClass* LastLoggedRecipeClass = nullptr;

    /** Last item size we logged (to avoid spam). Logged when it changes beyond a small tolerance. */
    UPROPERTY()
    FVector LastLoggedItemSize = FVector::ZeroVector;

    /** Current hologram adapter for grid calculations */
    TSharedPtr<class ISFHologramAdapter> CurrentAdapter;

    /** Flag to prevent repeated delegate subscriptions */
    bool bHasSubscribedToRecipeSampled = false;

    /** Last item size we logged (to avoid spam). Logged when it changes beyond a small tolerance. */
    TUniquePtr<FSFArrowModule_StaticMesh> ArrowModule;
    
    // ========================================
    // Phase 0 Extracted Modules (Task #61.6)
    // ========================================
    
    /** Input handler module - Enhanced Input management (Phase 0 extraction) */
    TUniquePtr<FSFInputHandler> InputHandler;
    
    /** Position calculator module - Grid position/offset calculations (Phase 0 extraction) */
    TUniquePtr<FSFPositionCalculator> PositionCalculator;
    
    /** Validation service module - Placement and grid validation (Phase 0 extraction) */
	TUniquePtr<FSFValidationService> ValidationService;
	
	/** Hologram helper service module - Lifecycle and grid management (Phase 0 extraction) */
	TUniquePtr<FSFHologramHelperService> HologramHelper;
	
	/** Auto-connect service - Belt preview connections for distributors (Refactor: Issue #XXX) */
	UPROPERTY()
	USFAutoConnectService* AutoConnectService;

	/** EXTEND service - Factory topology cloning (Issue #219) */
	UPROPERTY()
	USFExtendService* ExtendService;

	/** Radar Pulse diagnostic service - Object snapshot and diff system */
	UPROPERTY()
	USFRadarPulseService* RadarPulseService;

	/** Pipe Auto-Connect manager - feature-level coordinator (junctions + manifolds) */
	TUniquePtr<FSFPipeAutoConnectManager> PipeAutoConnectManager;

	/** Power Auto-Connect manager - feature-level coordinator (power poles + building connections) */
	TUniquePtr<FSFPowerAutoConnectManager> PowerAutoConnectManager;

	/** Auto-connect orchestrators - One per parent distributor hologram */
	UPROPERTY()
	TMap<AFGHologram*, USFAutoConnectOrchestrator*> AutoConnectOrchestrators;

    /** Recipe management service - Factory crafting recipe selection and application */
    UPROPERTY()
    USFRecipeManagementService* RecipeManagementService;

    /** Upgrade audit service - Scans world for upgradeable buildables */
    UPROPERTY()
    USFUpgradeAuditService* UpgradeAuditService;

    /** Upgrade execution service - Performs batch upgrades */
    UPROPERTY()
    USFUpgradeExecutionService* UpgradeExecutionService;
    
    /** HUD service - Binding and widget lifecycle */
    UPROPERTY()
    USFHudService* HudService;

    /** Hint bar service - Vanilla hint bar integration (Issue #281) */
    UPROPERTY()
    USFHintBarService* HintBarService;

    /** Chain actor service - Canonical chain invalidation + synchronous rebuild (v29.2.2) */
    UPROPERTY()
    USFChainActorService* ChainActorService = nullptr;

    // NOTE: DeferredCostService removed - child holograms automatically aggregate costs via GetCost()

    // NOTE: RecipeCostInjector removed - child holograms automatically aggregate costs via GetCost()

    /** Grid spawner service - Child spawn/update/destroy orchestration (Phase 2 extraction) */
    UPROPERTY()
    USFGridSpawnerService* GridSpawnerService = nullptr;

    /** Grid state service - Single source of truth for counters and axes */
    UPROPERTY()
    USFGridStateService* GridStateService = nullptr;

    /** Grid transform service - Movement detection and propagation (Phase 3 extraction) */
    UPROPERTY()
    USFGridTransformService* GridTransformService = nullptr;
    
    // NOTE: SpawnedChildren tracking moved to HologramHelper (Phase 2 extraction)
    // Use HologramHelper->GetSpawnedChildren() to access children
    
    /** Flag to prevent update cascades during mass child destruction */
    bool bSuppressChildUpdates = false;
    
    /** Flag set when large grid destruction detected - persists until all children cleared */
    bool bInMassDestruction = false;

    /** Baseline height when children were first spawned (for tracking vertical nudge delta) */
    float BaselineHeightZ = 0.0f;

    /** Flag to track if blueprint proxy was spawned recently (for blueprint building detection) */
    bool bBlueprintProxyRecentlySpawned = false;

    /** Blueprint proxy for the current Smart! grid placement session.
     * Groups all buildings placed in a single grid operation so they can be
     * dismantled together using vanilla's Blueprint Dismantle mode (R key).
     * Created on first building spawn when grid > 1x1x1, cleared on hologram unregister. */
    UPROPERTY()
    TWeakObjectPtr<AFGBlueprintProxy> CurrentBuildProxy;

    /** Track the hologram that owns the current proxy to detect new build sessions */
    UPROPERTY()
    TWeakObjectPtr<AFGHologram> CurrentProxyOwner;

private:
	// ========================================
	// Centralized Counter State (HUD Refactoring)
	// ========================================
	
	/** Centralized counter state for HUD display
	 * Single source of truth for all counter values
	 * Use GetCounterState() / UpdateCounterState() for access
	 * DO NOT access individual counter variables directly!
	 */
	FSFCounterState CounterState;

public:
	/** Get current counter state (read-only)
     * Service is the authoritative source; local CounterState is kept in sync for migration.
     * Implementation in .cpp due to forward declaration of USFGridStateService.
     */
    const FSFCounterState& GetCounterState() const;

	/** Accessor for HUD service (read-only pointer) */

	/** Accessor for chain actor service (nullable if BuildableSubsystem is not yet available) */
	USFChainActorService* GetChainActorService() const { return ChainActorService; }
	USFHudService* GetHudService() const { return HudService; }

	/** Accessor for hint bar service */
	USFHintBarService* GetHintBarService() const { return HintBarService; }

	// NOTE: GetDeferredCostService() removed - child holograms automatically aggregate costs via GetCost()

	// NOTE: GetRecipeCostInjector() removed - child holograms automatically aggregate costs via GetCost()
	
	/** Update counter state and trigger HUD refresh */
	void UpdateCounterState(const FSFCounterState& NewState);

    /** Reset all counters to defaults and refresh HUD */
    void ResetCounters();

// ========================================
// Scaling Operations (Phase 0: Public for InputHandler access)
// ========================================

    /** Apply axis-specific scaling based on input */
    void ApplyAxisScaling(ESFScaleAxis Axis, int32 StepDelta, const TCHAR* DebugLabel);

private:
	// ========================================
	// Internal Scaling Operations
	// ========================================

	/** Apply scaling to the hologram by a delta amount */
	void ApplyScalingToHologram(const FVector& ScaleDelta);

	/** Modifier key state for Z-axis wheel detection */
	bool bModifierScaleXActive = false;
	bool bModifierScaleYActive = false;

	/** Hologram lock ownership tracking (Task 52)
	 * Tracks whether WE locked the hologram (vs. vanilla hold system)
	 * - true: We locked it with modifiers, we'll unlock when modifiers released
	 * This prevents us from unlocking holograms that vanilla hold locked */
	bool bLockedByModifier = false;

	/** Auto-Hold state tracking (Issue #273)
	 * bAutoHoldActive: true when Smart! locked the hologram due to a grid modification
	 * bAutoHoldUserOverrode: true when user pressed Hold to manually release our auto-lock;
	 *   suppresses re-locking until the next grid change */
	bool bAutoHoldActive = false;
	bool bAutoHoldUserOverrode = false;
	
	/** Lazy initialization flag (Task #58) - true after first hologram registration */
	bool bSubsystemInitialized = false;
	
	/** Feature mode state */
	bool bSpacingModeActive = false;
	bool bStepsModeActive = false;
	bool bStaggerModeActive = false;
	bool bRotationModeActive = false;
	
	// All counter and axis fields migrated to CounterState
	// Deprecated mirrors removed - use GetCounterState() accessors
	
	/** Check if any Smart! modal features are currently active (Task 52)
	 * @return true if any of X/Z modifiers, Spacing, Steps, or Stagger modes are active */
	bool IsAnyModalFeatureActive() const;

public:
	/** Try to acquire hologram lock for Smart! features (Task 52)
	 * Only locks if not already locked by vanilla hold system
	 * @return true if we acquired the lock, false if already locked by vanilla hold */
	bool TryAcquireHologramLock();
	
	/** Try to release hologram lock for Smart! features (Task 52)
	 * Only unlocks if WE locked it AND no other Smart! modal features are active */
	void TryReleaseHologramLock();
	
	/** Get the world position of the furthest top hologram in the grid
	 * This is the hologram at grid position (maxX, maxY, 1) - the top of the column furthest from the parent
	 * Used by SmartCamera to position its PiP viewport for optimal grid viewing
	 * @return World position of the furthest top hologram, or parent position if no grid */
	FVector GetFurthestTopHologramPosition() const;
	/** Regenerate child hologram grid based on current counters */
	void RegenerateChildHologramGrid();
	
	/** Update positions of all child holograms in grid */
	void UpdateChildPositions();

	/** Ensure children match current parent transform (called on transform changes) */
	void UpdateChildrenForCurrentTransform();

	/** Calculate world position for child at grid index */
	FVector CalculateChildPosition(int32 X, int32 Y, int32 Z, const FVector& ParentLocation, const FRotator& ParentRotation, const FVector& ItemSize, int32 GridIndex = 0) const;

	/** Factory: Create appropriate adapter for a hologram */
	TSharedPtr<class ISFHologramAdapter> CreateHologramAdapter(AFGHologram* Hologram);

	/** Queue a child hologram for destruction on the next tick */
	void QueueChildForDestroy(AFGHologram* Child);

	/** Flush queued child destructions (runs on next tick) */
	void FlushPendingDestroy();

    /** Force-destroy any queued children regardless of state (use when build gun is unequipped or hologram cleared) */
    void ForceDestroyPendingChildren();

    /** Determine if it's safe to actually destroy children without racing build gun validation */
    bool CanSafelyDestroyChildren() const;

	/** Disable vanilla Build Gun mapping context to prevent conflicts with Smart! wheel input */
	void DisableVanillaBuildGunContext();

	/** Re-enable vanilla Build Gun mapping context when Smart! modifiers released */
	void EnableVanillaBuildGunContext();

	/** Whether a flush has been scheduled for next tick */
	bool bPendingDestroyScheduled = false;

	/** Pending children to destroy safely next tick */
	TArray<TWeakObjectPtr<AFGHologram>> PendingDestroyChildren;

    /** Timer used to schedule next-tick flush */
    FTimerHandle PendingDestroyTick;

    // ========================================
    // Configuration System (Task 69)
    // ========================================

	/** Runtime arrow visibility state (affected by Num1 toggle, initialized from config) */
	bool bArrowsRuntimeVisible = true;

	/** Cached configuration loaded on startup */
	struct FSmart_ConfigStruct CachedConfig;

	/** Has configuration been loaded yet */
	bool bConfigLoaded = false;

public:
	/** Get cached configuration */
	const FSmart_ConfigStruct& GetCachedConfig() const { return CachedConfig; }

	// ========================================
	// Recipe Copying System - Public Access
	// ========================================
	
	/** Stored production recipe from sampled building for copying to child buildings */
	UPROPERTY(Transient)
	TSubclassOf<UFGRecipe> StoredProductionRecipe;
	
	/** Cached display name for stored production recipe (performance optimization) */
	FString StoredRecipeDisplayName;
	
	/** Whether we have a valid stored production recipe (public for helper service access) */
	UPROPERTY(Transient)
	bool bHasStoredProductionRecipe = false;
	
	/** Store production recipe from source building during middle mouse sampling */
	void StoreProductionRecipeFromBuilding(class AFGBuildable* SourceBuilding);

	/** Apply stored production recipe to target building after construction */
	void ApplyStoredProductionRecipeToBuilding(class AFGBuildable* TargetBuilding);

	/** Clear stored production recipe */
	void ClearStoredProductionRecipe();

	// ========================================
	// Recipe Selector System - Unified State Management
	// ========================================
	
	/** Recipe source tracking for priority system */
	enum class ERecipeSource {
		None,              // No recipe set
		Copied,            // From middle-click sampling  
		ManuallySelected   // From U + Num8/5 selector
	};

	/** Whether recipe mode is currently active (U key held) */
	bool bRecipeModeActive = false;

	/** Unified active recipe (single source of truth) */
	UPROPERTY(Transient)
	TSubclassOf<UFGRecipe> ActiveRecipe;

	/** Source of the current active recipe */
	ERecipeSource ActiveRecipeSource = ERecipeSource::None;

	/** Array of all discovered/unlocked recipes */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UFGRecipe>> UnlockedRecipes;

	/** Cached sorted recipes for current hologram type */
	UPROPERTY(Transient)
	TArray<TSubclassOf<UFGRecipe>> SortedFilteredRecipes;

	/** Current index in the filtered unlocked recipes array */
	int32 CurrentRecipeIndex = 0;

	/** Recipe mode handler (U key press/release) */
	UFUNCTION()
	void OnRecipeModeChanged(const FInputActionValue& Value);

	/** Recipe cycling functions */
	void CycleRecipeForward(int32 AccumulatedSteps);
	void CycleRecipeBackward(int32 AccumulatedSteps);
	void SetActiveRecipeByIndex(int32 Index);
	void AddRecipeToUnlocked(TSubclassOf<UFGRecipe> Recipe);
	TArray<TSubclassOf<UFGRecipe>> GetFilteredRecipesForCurrentHologram();
	void ClearAllRecipes();
	
	/** Apply active recipe to parent hologram */
	void ApplyRecipeToParentHologram();
	bool IsRecipeCompatibleWithHologram(TSubclassOf<UFGRecipe> Recipe, UClass* HologramBuildClass);

	/** Check if building is a production building that supports recipes */
	bool IsProductionBuilding(class AFGBuildable* Building) const;

	/** Called when build gun samples a recipe (middle-mouse, Ctrl+C, or Copy Settings button) */
	UFUNCTION()
	void OnBuildGunRecipeSampled(TSubclassOf<class UFGRecipe> SampledRecipe);
	
	// ========================================
	// Auto-Connect Settings Mode (Context-Aware U Button)
	// ========================================
	
	/** Auto-connect settings available for navigation with U button on distributors/junctions */
	enum class EAutoConnectSetting
	{
		Enabled,                 // Global on/off toggle
		BeltTierMain,           // Belt tier for main connections (distributor-to-distributor)
		BeltTierToBuilding,     // Belt tier for building connections (distributor-to-building)
		ChainDistributors,      // Enable distributor chaining (manifolds)
		BeltRoutingMode,        // Belt routing mode: Auto, 2D, Straight, Curve
		PipeTierMain,           // Pipe tier for main connections (junction-to-junction)
		PipeTierToBuilding,     // Pipe tier for building connections (junction-to-building)
		PipeIndicator,          // Pipe style: Normal (with indicators) vs Clean (no indicators)
		PipeRoutingMode,        // Pipe routing mode: Auto, 2D, Straight, Curve
		StackableBeltEnabled,   // Stackable Pole: Belt auto-connect on/off
		StackableBeltTier,      // Stackable Pole: Belt tier (reuses BeltTierMain value)
		StackableBeltDirection, // Stackable Pole: Belt direction (Forward/Backward)
		PowerEnabled,           // Power: Global on/off toggle for power
		PowerReserved,          // Power: Reserved connections per pole (0-5)
		PowerGridAxis           // Power: Grid connection axis (Auto, X, Y, X+Y)
	};
	
	/** Whether auto-connect settings mode is active (U held on distributor or pipe junction) */
	bool bAutoConnectSettingsModeActive = false;
	
	/** Current setting being adjusted */
	EAutoConnectSetting CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
	
	/** Temporary runtime overrides for current placement (reset when hologram changes) */
	struct FAutoConnectRuntimeSettings
	{
		bool bEnabled = true;          // Belt auto-connect enabled
		int32 BeltTierMain = 0;        // 0=Auto, 1-6=Mk1-Mk6
		int32 BeltTierToBuilding = 0;  // 0=Auto, 1-6=Mk1-Mk6
		bool bChainDistributors = true;  // Enable distributor chaining (manifolds)
		int32 BeltRoutingMode = 0;     // 0=Default, 1=Curve, 2=Straight (matches vanilla belt build modes)
		bool bPipeAutoConnectEnabled = true;  // Pipe auto-connect enabled
		int32 PipeTierMain = 0;        // 0=Auto, 1-2=Mk1-Mk2
		int32 PipeTierToBuilding = 0;  // 0=Auto, 1-2=Mk1-Mk2
		bool bPipeIndicator = true;    // true=Normal (with indicators), false=Clean (no indicators)
		int32 PipeRoutingMode = 0;     // 0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical (matches vanilla)
		
		// Stackable Pole Settings (shared for belt/pipe support structures)
		bool bStackableBeltEnabled = true;  // Stackable conveyor pole belt auto-connect enabled
		int32 StackableBeltDirection = 0;   // Belt direction: 0=Forward (along grid X+), 1=Backward (against grid X-)
		
		// Power Settings
		bool bConnectPower = true;     // Power auto-connect enabled
		bool bExtendPower = true;      // Include power poles when using Extend (Issue #229)
		int32 PowerReserved = 1;       // Reserved connections per pole (0-5)
		int32 PowerGridAxis = 0;       // 0=Auto, 1=X, 2=Y, 3=X+Y
		float PowerBuildingRange = 5000.0f; // Search radius for buildings in cm (default 50m, max 30000cm/300m)
		
		bool bInitialized = false;     // Track if settings have been modified by user
		
		/** Initialize from config */
		void InitFromConfig(const FSmart_ConfigStruct& Config)
		{
			bEnabled = Config.bAutoConnectEnabled;
			BeltTierMain = Config.BeltLevelMain;
			BeltTierToBuilding = Config.BeltLevelToBuilding;
			bChainDistributors = Config.bAutoConnectDistributors;
			BeltRoutingMode = Config.BeltRoutingMode;
			bStackableBeltEnabled = Config.bStackableBeltEnabled;
			bPipeAutoConnectEnabled = Config.bPipeAutoConnectEnabled;
			PipeTierMain = Config.PipeLevelMain;
			PipeTierToBuilding = Config.PipeLevelToBuilding;
			bPipeIndicator = Config.PipeIndicator;
			PipeRoutingMode = Config.PipeRoutingMode;
			
			// Power settings from config
			bConnectPower = Config.bPowerAutoConnectEnabled;
			bExtendPower = Config.bExtendPowerEnabled;
			PowerReserved = Config.PowerConnectReserved;
			PowerGridAxis = Config.PowerConnectMode;
			PowerBuildingRange = static_cast<float>(Config.PowerConnectRange) * 100.0f;  // Convert from meters to cm
			
			bInitialized = false;  // Reset to not modified when initialized from config
		}
	};
	
	/** Runtime settings for current placement (overrides config while active) */
	FAutoConnectRuntimeSettings AutoConnectRuntimeSettings;
	
	/** Cycle to next auto-connect setting */
	void CycleAutoConnectSetting();
	
	/** Adjust current auto-connect setting value */
	void AdjustAutoConnectSetting(int32 Delta);
	
	/** Get display string for current auto-connect setting */
	FString GetAutoConnectSettingDisplayString() const;
	
	/** Get display strings for settings that differ from global config */
	TArray<FString> GetDirtyAutoConnectSettings() const;
	
	/** Check if current hologram is an auto-connect capable type (distributor, pipe junction, power pole, stackable support) */
	bool IsCurrentHologramAutoConnectCapable() const;
	
	/** Get current auto-connect runtime settings (for use by auto-connect service) */
	const FAutoConnectRuntimeSettings& GetAutoConnectRuntimeSettings() const { return AutoConnectRuntimeSettings; }
	
	/** Reset runtime settings to match config (called when hologram changes) */
	void ResetAutoConnectRuntimeSettings();
	
	// ========================================
	// One-Shot Smart Disable (Issue #198)
	// Double-tap CycleAxis (no modifier) to disable auto-connect for next action
	// ========================================
	
	/** One-shot flag to disable Smart auto-connect for the next placement action
	 *  Set by double-tapping IA_Smart_CycleAxis with no modifier held
	 *  Reset when: hologram changes, build gun unequipped, or placement completes */
	bool bDisableSmartForNextAction = false;
	
	/** Timestamp of last CycleAxis tap for double-tap detection */
	double LastCycleAxisTapTime = 0.0;
	
	/** Time window for double-tap detection (seconds) */
	static constexpr double DoubleTapWindow = 1.0;
	
	/** Check if Smart is temporarily disabled for current action */
	bool IsSmartDisabledForCurrentAction() const { return bDisableSmartForNextAction; }
	
	/** Reset the one-shot disable flags (auto-connect and Extend) */
	void ResetSmartDisableFlag();
	
	// ========================================
	// Extend Toggle (Issue #257)
	// Double-tap CycleAxis also disables Extend for session
	// Config menu provides persistent disable
	// ========================================
	
	/** One-shot flag to disable Extend for current session.
	 *  Set by double-tapping IA_Smart_CycleAxis (same trigger as auto-connect disable).
	 *  Reset when: hologram changes, build gun unequipped (same as auto-connect). */
	bool bExtendDisabledForSession = false;
	
	/** Persistent config flag: false = Extend globally disabled from settings menu */
	bool bExtendEnabledByConfig = true;
	
	/** Check if Extend is currently disabled (session OR config) */
	bool IsExtendDisabled() const { return bExtendDisabledForSession || !bExtendEnabledByConfig; }
	
	// === Belt Auto-Connect Setters (for Settings Form) ===
	
	/** Set belt auto-connect enabled */
	void SetAutoConnectBeltEnabled(bool bEnabled);
	
	/** Set belt tier for main connections (0=Auto, 1-6=Mk1-Mk6) */
	void SetAutoConnectBeltTierMain(int32 Tier);
	
	/** Set belt tier for building connections (0=Auto, 1-6=Mk1-Mk6) */
	void SetAutoConnectBeltTierToBuilding(int32 Tier);
	
	/** Set belt chain distributors enabled */
	void SetAutoConnectBeltChain(bool bEnabled);
	
	/** Set stackable belt direction */
	void SetAutoConnectStackableBeltDirection(int32 Direction);
	
	/** Set belt routing mode (0=Default, 1=Curve, 2=Straight) */
	void SetAutoConnectBeltRoutingMode(int32 Mode);
	
	// === Pipe Auto-Connect Setters (for Settings Form) ===
	
	/** Set pipe auto-connect enabled */
	void SetAutoConnectPipeEnabled(bool bEnabled);
	
	/** Set pipe tier for main connections (0=Auto, 1-2=Mk1-Mk2) */
	void SetAutoConnectPipeTierMain(int32 Tier);
	
	/** Set pipe tier for building connections (0=Auto, 1-2=Mk1-Mk2) */
	void SetAutoConnectPipeTierToBuilding(int32 Tier);
	
	/** Set pipe indicator style (true=Normal with indicators, false=Clean) */
	void SetAutoConnectPipeIndicator(bool bIndicator);
	
	/** Set pipe routing mode (0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical) */
	void SetAutoConnectPipeRoutingMode(int32 Mode);
	
	/** Get highest unlocked pipe tier for current player */
	int32 GetHighestUnlockedPipeTier(AFGPlayerController* PlayerController);
	
	/** Check if clean (no indicator) pipes are unlocked for player */
	bool AreCleanPipesUnlocked(AFGPlayerController* PlayerController);
	
	// === Power Auto-Connect Setters (for Settings Form) ===
	
	/** Set power auto-connect enabled */
	void SetAutoConnectPowerEnabled(bool bEnabled);
	
	/** Set power grid axis (0=Auto, 1=X, 2=Y, 3=X+Y) */
	void SetAutoConnectPowerGridAxis(int32 Axis);
	
	/** Set power reserved slots (0-5) */
	void SetAutoConnectPowerReserved(int32 Reserved);
	
	/** Trigger auto-connect preview refresh (for Apply Immediately mode) */
	void TriggerAutoConnectRefresh();
	
	// ========================================
	// Building Registry System (Session Tracking)
	// ========================================
	
	/** Registry of all Smart-created buildings this session */
	UPROPERTY()
	TMap<AFGBuildable*, FSFBuildingMetadata> SmartBuildingRegistry;
	
	/** Current placement group ID (increments with each placement operation) */
	int32 CurrentPlacementGroupID = 0;
	
	/** Track buildings in current placement operation */
	TArray<AFGBuildable*> CurrentPlacementBuildings;
	
	/** Register a Smart-created building in the session registry */
	void RegisterSmartBuilding(AFGBuildable* Building, int32 IndexInGroup, bool bIsParent);
	
	/** Apply recipes to all buildings in current placement group */
	void ApplyRecipesToCurrentPlacement();
	
	/** Handle actor spawn to apply stored recipes from child holograms */
	UFUNCTION()
	void OnActorSpawned(AActor* SpawnedActor);
	
	/** Find recipe for spawned building using fuzzy matching (class + spatial proximity) */
	TSubclassOf<class UFGRecipe> FindRecipeForSpawnedBuilding(class AFGBuildableManufacturer* SpawnedBuilding);
	
	/** Apply recipe to building after delay (for timing issues) */
	UFUNCTION()
	void ApplyRecipeDelayed(class AFGBuildableManufacturer* ManufacturerBuilding, TSubclassOf<class UFGRecipe> Recipe);
	
	/** Clear blueprint proxy flag (called by timer) */
	UFUNCTION()
	void ClearBlueprintProxyFlag();
	
	/** Clear current placement tracking (called after recipes applied) */
	void ClearCurrentPlacement();

	// ========================================
	// Phase 4: Runtime Hologram Swapping
	// ========================================
	
	/** Attempt to swap vanilla hologram to custom Smart hologram
	 * @param OriginalHologram The vanilla hologram created by Satisfactory
	 * @return The original hologram if no swap needed, or the new custom hologram
	 */
	AFGHologram* TrySwapToSmartHologram(AFGHologram* OriginalHologram);


	// ========================================
	// Smart Auto-Connect: Distributor Lifecycle
	// ========================================
	
	/** Hook called when distributor hologram moves (per-frame update) */
	void OnDistributorHologramUpdated(AFGHologram* DistributorHologram);
	
	// DEPRECATED: OnPipeJunctionHologramUpdated removed - use USFAutoConnectOrchestrator::OnPipeJunctionsMoved() instead
	// Note: Auto-connect functions are now handled by SFAutoConnectService / Orchestrator pattern

	// ========================================
	// Belt Tier Configuration Helpers (PUBLIC for Auto-Connect)
	// ========================================
	
	/** Get belt class for specified tier (1-6), returns nullptr if tier unavailable
	 * @param Tier Belt tier (1=Mk1, 2=Mk2, ..., 6=Mk6)
	 * @param PlayerController Player to check unlock status (optional, skips unlock check if null)
	 * @return Belt class if available and unlocked, nullptr otherwise
	 */
	UClass* GetBeltClassForTier(int32 Tier, AFGPlayerController* PlayerController = nullptr);
	
	/** Get belt class based on config (handles "Auto" mode and unlock validation)
	 * @param ConfigTier Configured tier (0=Auto, 1-6=specific tier)
	 * @param PlayerController Player to check unlocks and determine auto tier
	 * @return Belt class if available, nullptr if tier unavailable/locked (disables that belt category)
	 */
	UClass* GetBeltClassFromConfig(int32 ConfigTier, AFGPlayerController* PlayerController);
	
	/** Get highest unlocked belt tier for player (for "Auto" mode)
	 * @param PlayerController Player to check unlocks
	 * @return Highest unlocked tier (1-6), or 1 if player is null
	 */
	int32 GetHighestUnlockedBeltTier(AFGPlayerController* PlayerController);
	
	/** Get highest unlocked power pole tier for player
	 * @param PlayerController Player to check unlocks
	 * @return Highest unlocked tier (1-3), or 1 if player is null
	 */
	int32 GetHighestUnlockedPowerPoleTier(AFGPlayerController* PlayerController);
	
	/** Get highest unlocked wall outlet tier for player
	 * Single and double wall outlets unlock independently in vanilla progression,
	 * so callers must indicate which family they are probing (Issue #267).
	 * @param PlayerController Player to check unlocks
	 * @param bDouble false = single-sided wall outlets (Build_PowerPoleWall*), true = double-sided (Build_PowerPoleWallDouble*)
	 * @return Highest unlocked tier (1-3), or 1 if player is null
	 */
	int32 GetHighestUnlockedWallOutletTier(AFGPlayerController* PlayerController, bool bDouble = false);
	
	/** Get pipe class based on tier and indicator style
	 * @param Tier Pipe tier (1=Mk1, 2=Mk2)
	 * @param bWithIndicator true=Normal (with flow indicators), false=Clean (no indicators)
	 * @param PlayerController Player to check unlock status (optional, skips unlock check if null)
	 * @return Pipe class if available and unlocked, nullptr otherwise
	 */
	UClass* GetPipeClassForTier(int32 Tier, bool bWithIndicator, AFGPlayerController* PlayerController = nullptr);
	
	/**
	 * Get belt recipe for a specific tier
	 * @param Tier Belt tier (1=Mk1, 2=Mk2, 3=Mk3, 4=Mk4, 5=Mk5, 6=Mk6)
	 * @return Belt recipe class if found, nullptr otherwise
	 */
	TSubclassOf<UFGRecipe> GetBeltRecipeForTier(int32 Tier);
	
	/** Get pipe recipe for a specific tier
	 * @param Tier Pipe tier (1=Mk1, 2=Mk2)
	 * @param bWithIndicator True for normal pipes, false for clean/no-indicator pipes
	 * @return Pipe recipe class if found, nullptr otherwise
	 */
	TSubclassOf<UFGRecipe> GetPipeRecipeForTier(int32 Tier, bool bWithIndicator);
	
	/** Get pipe class based on config (handles "Auto" mode and unlock validation)
	 * @param ConfigTier Configured tier (0=Auto, 1-2=specific tier)
	 * @param bWithIndicator true=Normal (with flow indicators), false=Clean (no indicators)
	 * @param PlayerController Player to check unlocks and determine auto tier
	 * @return Pipe class if available, nullptr if tier unavailable/locked (disables that pipe category)
	 */
	UClass* GetPipeClassFromConfig(int32 ConfigTier, bool bWithIndicator, AFGPlayerController* PlayerController);

private:

	/** Load configuration from SML config system */
	void LoadConfiguration();

	/** Clean up state during world transitions and save loads */
	void CleanupStateForWorldTransition();

	/** Create and initialize HUD widgets */
	void InitializeWidgets();

	/** Ensure HUD binding is active (lazy initialization) */
	void EnsureHUDBinding();

	/** Clean up HUD widgets */
	void CleanupWidgets();

	/** Initialize hologram destruction callbacks for deterministic cleanup */
	void InitializeHologramCleanup();

	/** Handle actor destruction for automatic hologram registry cleanup */
	UFUNCTION()
	void OnActorDestroyed(AActor* DestroyedActor);

	/** Start periodic cleanup of dead hologram entries */
	void StartPeriodicCleanup();

	/** Stop periodic cleanup timer */
	void StopPeriodicCleanup();

	/** Perform periodic cleanup of dead hologram entries */
	UFUNCTION()
	void OnPeriodicCleanup();

	// ========================================
	// Hologram Creation Functions (Phase 4 Implementation)
	// ========================================
	
	/** Create a custom foundation hologram to replace a vanilla one */
	ASFFoundationHologram* CreateCustomFoundationHologram(AFGHologram* OriginalHologram);
	
	/** Create a custom factory hologram to replace a vanilla one */
	ASFFactoryHologram* CreateCustomFactoryHologram(AFGHologram* OriginalHologram);
	
	/** Create a custom logistics hologram to replace a vanilla one */
	ASFLogisticsHologram* CreateCustomLogisticsHologram(AFGHologram* OriginalHologram);
	
	/** Copy essential properties from original hologram to custom hologram */
	void CopyHologramProperties(AFGHologram* Source, AFGHologram* Destination);
	
	/** Replace original hologram with custom one in build gun system */
	bool ReplaceHologramInBuildGun(AFGHologram* OriginalHologram, AFGHologram* CustomHologram);

	// ========================================
	// Auto-Connect Service Integration
	// ========================================
	// Note: Belt preview helpers are now managed by SFAutoConnectService

	/** Charge player inventory for belt construction cost based on belt length
	 * @param BeltClass Belt class to get cost for
	 * @param PlayerController Player to charge
	 * @param BeltLengthCm Length of the belt in centimeters (UE units)
	 * @return true if cost was successfully charged, false if player can't afford or error
	 */
	bool ChargePlayerForBelt(UClass* BeltClass, AFGPlayerController* PlayerController, float BeltLengthCm);

	/** Charge player inventory for pipe construction cost based on pipe length
	 * @param PipeClass Pipe class to get cost for
	 * @param PlayerController Player to charge
	 * @param PipeLengthCm Length of the pipe in centimeters (UE units)
	 * @return true if cost was successfully charged, false if player can't afford or error
	 */
	bool ChargePlayerForPipe(UClass* PipeClass, AFGPlayerController* PlayerController, float PipeLengthCm);

	// ========================================
	// Debug Tools
	// ========================================

	/** Analyze nearby vanilla pipes to reverse-engineer spline formulas
	 * @param Radius Search radius in cm (default 5000cm = 50m)
	 * Logs detailed spline data including tangent scale factors
	 */
	void AnalyzeNearbyPipeSplines(float Radius = 5000.0f);

	// ========================================
	// Stackable Pipe Support Deferred Build (Issue #220)
	// ========================================
	// Data extracted from pipe previews before hologram destruction
	// Used to build pipes when stackable pipe supports spawn
	
	/** Data structure for deferred stackable pipe build */
	struct FStackablePipeBuildData
	{
		FVector SourceHologramPosition;
		FVector TargetHologramPosition;
		TArray<FSplinePointData> SplineData;
		int32 PipeTier = 0;
	};
	
	/** Pending pipe build data extracted before hologram destruction */
	TArray<FStackablePipeBuildData> PendingStackablePipeBuildData;
	
	/** Hologram positions for matching to spawned actors */
	TArray<FVector> PendingStackablePipeHologramPositions;
	
	/** Parent position for sorting */
	FVector PendingStackablePipeParentPosition;
	
	/** Scale axis for sorting */
	FVector PendingStackablePipeScaleAxis;
	
	/** Whether we have pending stackable pipe build data */
	bool bStackablePipeBuildPending = false;
	
	/** Expected number of stackable pipe supports to spawn */
	int32 StackablePipeSupportExpectedCount = 0;
	
	/** Number of stackable pipe supports spawned so far */
	int32 StackablePipeSupportSpawnedCount = 0;

public:
	/** Cache hologram positions for deferred stackable pipe build (called from ProcessStackablePipelineSupports) */
	void CacheStackablePipeHologramPositions(const TArray<AFGHologram*>& AllSupports, AFGHologram* ParentHologram);
	
	/** Clear the stackable pipe build cache */
	void ClearStackablePipeBuildCache();

	// ========================================
	// Chain Actor Rebuild System (Issue #220 - Stackable Belt Fix)
	// ========================================
	// Uses a deferred timer to safely rebuild chains after all belts are placed.

	/** Queue a belt for chain rebuild after a short delay */
	void QueueChainRebuild(AFGBuildableConveyorBelt* Belt);

	/** Traverse connections to collect all belts in a connected chain */
	TSet<AFGBuildableConveyorBelt*> CollectChainBelts(AFGBuildableConveyorBelt* StartBelt);

	/** 
	 * Cache stackable belt preview data for building (Issue #220)
	 * Called by orchestrator when belt previews are valid, before hologram is destroyed
	 */
	void CacheStackableBeltPreviewsForBuild();

private:
	/** Execute the deferred chain rebuild */
	void ExecuteDeferredChainRebuild();

	/** Belts pending chain rebuild - processed after timer fires */
	TSet<TWeakObjectPtr<AFGBuildableConveyorBelt>> PendingChainRebuilds;

	/** Timer handle for deferred chain rebuild */
	FTimerHandle ChainRebuildTimerHandle;
};
