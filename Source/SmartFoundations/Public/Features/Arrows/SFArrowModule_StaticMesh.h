// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Arrow Visualization using StaticMeshComponent (Task #17 - Robust Implementation)

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "FSFArrowTypes.h"
#include "SFArrowAssetManager.h"

class USFSubsystem;

/**
 * FSFArrowModule_StaticMesh - Production-ready arrow visualization using StaticMeshComponent
 * 
 * ADVANTAGES OVER DrawDebug:
 * - ✅ Works in all build configurations (PIE, Development, Shipping)
 * - ✅ Proper lighting and material support
 * - ✅ Can use actual 3D arrow meshes
 * - ✅ Material-based coloring (easy to change at runtime)
 * - ✅ Reliable rendering in packaged builds
 * 
 * IMPLEMENTATION:
 * - Creates 3 UStaticMeshComponent instances (X, Y, Z axes)
 * - Uses arrow mesh from Content or engine defaults
 * - Materials for color coding (Red, Green, Blue)
 * - Updates transforms each frame when visible
 * 
 * MESH OPTIONS (Issue #213 - Composite Arrow):
 * - Arrow Head: /Engine/BasicShapes/Cone (arrowhead)
 * - Arrow Shaft: /Engine/BasicShapes/Cylinder (shaft)
 * - Text Labels: UTextRenderComponent with billboard behavior
 * 
 * @see docs/Features/Scaling/ARROW_SYSTEM_ANALYSIS.md
 * @see Task #17 in tasks/tasks.json
 * 
 * USAGE:
 * ```cpp
 * FSFArrowModule_StaticMesh ArrowModule;
 * ArrowModule.Initialize(GetWorld(), RootComponent);
 * 
 * // Each frame:
 * ArrowModule.UpdateArrows(
 *     GetWorld(),
 *     HologramTransform,
 *     CurrentLastAxis,
 *     bArrowsVisible
 * );
 * 
 * // On destroy:
 * ArrowModule.Cleanup();
 * ```
 */
class SMARTFOUNDATIONS_API FSFArrowModule_StaticMesh
{
public:
	/** Constructor */
	FSFArrowModule_StaticMesh();
	
	/** Destructor - cleanup components */
	~FSFArrowModule_StaticMesh();

	/**
	 * Initialize arrow visualization system
	 * 
	 * Creates arrow components and loads assets asynchronously.
	 * Call this once when the subsystem starts up.
	 * 
	 * @param World Game world for component creation
	 * @param Outer Owner object for components
	 * @param Subsystem Reference to subsystem for grid information (Task #67)
	 * @return true if initialization started successfully
	 */
	bool Initialize(UWorld* World, UObject* Outer, class USFSubsystem* Subsystem = nullptr);

	/**
	 * Attach arrows to hologram's root component
	 * 
	 * Call this when a new hologram becomes active.
	 * Arrows will follow hologram automatically after attachment.
	 * 
	 * @param HologramRootComponent Root component of the hologram to attach to
	 * @return true if attachment succeeded
	 */
	bool AttachToHologram(USceneComponent* HologramRootComponent);

	/**
	 * Detach arrows from current hologram
	 * 
	 * Call this when hologram is destroyed or when switching holograms.
	 */
	void DetachFromHologram();

	/**
	 * Clean up arrow components
	 * 
	 * Destroys all components and releases references.
	 * Call this on subsystem shutdown.
	 */
	void Cleanup();

	/**
	 * Update arrows each frame
	 * 
	 * Updates transforms and visibility for all arrow components.
	 * 
	 * @param World World context
	 * @param HologramTransform Current hologram transform
	 * @param LastAxis Last axis that was manipulated
	 * @param bVisible Whether arrows should be shown
	 */
	void UpdateArrows(
		UWorld* World,
		const FTransform& HologramTransform,
		ELastAxisInput LastAxis,
		bool bVisible
	);

	/**
	 * Set highlighted axis
	 * 
	 * @param Axis Axis to highlight
	 */
	void SetHighlightedAxis(ELastAxisInput Axis);

	/**
	 * Update modifier key states
	 * 
	 * @param bShift LeftShift pressed
	 * @param bCtrl LeftCtrl pressed
	 */
	void SetModifierKeys(bool bShift, bool bCtrl);

	/**
	 * Update color scheme
	 * 
	 * @param ColorScheme New colors for arrows
	 */
	void SetArrowColors(const FArrowColorScheme& ColorScheme);

	/**
	 * Update configuration
	 * 
	 * @param Config New configuration
	 */
	void SetArrowConfig(const FArrowConfig& Config);

	/** Check if arrows are initialized */
	bool IsInitialized() const { return bInitialized; }

	/** Check if arrows are currently visible */
	bool AreArrowsVisible() const { return bCurrentlyVisible; }

	/** Check if arrows are attached to a hologram */
	bool IsAttachedToHologram() const { return ArrowX.IsValid() && ArrowX->GetAttachParent() != nullptr; }

	/**
	 * Tick label orbits every frame (Issue #213)
	 * 
	 * Lightweight per-frame update for animated label orbits.
	 * Call this from TickArrows() OUTSIDE the change gate so labels
	 * animate even when the hologram is stationary.
	 * 
	 * @param World World context for time and camera
	 */
	void TickLabelOrbits(UWorld* World);

	/** Enable/disable orbit animation on labels (labels stay at fixed offset when disabled) */
	void SetOrbitEnabled(bool bEnabled);

	/** Show/hide axis text labels entirely */
	void SetLabelsVisible(bool bVisible);

private:
	// ===== Arrow Head Components (Cone) =====
	TWeakObjectPtr<UStaticMeshComponent> ArrowX;
	TWeakObjectPtr<UStaticMeshComponent> ArrowY;
	TWeakObjectPtr<UStaticMeshComponent> ArrowZ;

	// ===== Arrow Shaft Components (Cylinder) - Issue #213 =====
	TWeakObjectPtr<UStaticMeshComponent> ShaftX;
	TWeakObjectPtr<UStaticMeshComponent> ShaftY;
	TWeakObjectPtr<UStaticMeshComponent> ShaftZ;

	// ===== Text Label Components - Issue #213 =====
	TWeakObjectPtr<UTextRenderComponent> LabelX;
	TWeakObjectPtr<UTextRenderComponent> LabelY;
	TWeakObjectPtr<UTextRenderComponent> LabelZ;

	/** Asset manager for safe async loading (Task #58) */
	FSFArrowAssetManager AssetManager;

	/** Reference to subsystem for grid information (Task #67) */
	TWeakObjectPtr<class USFSubsystem> SubsystemRef;

	/** Arrow head mesh (Cone) - cached after successful load */
	TWeakObjectPtr<UStaticMesh> ArrowMesh;

	/** Arrow shaft mesh (Cylinder) - cached after successful load (Issue #213) */
	TWeakObjectPtr<UStaticMesh> ShaftMesh;

	/** Materials for coloring (cached after successful load) - TWeakObjectPtr for safety */
	TWeakObjectPtr<UMaterialInterface> MaterialX;  // Red
	TWeakObjectPtr<UMaterialInterface> MaterialY;  // Green
	TWeakObjectPtr<UMaterialInterface> MaterialZ;  // Blue
	
	/** Dynamic material instances for arrow heads (reused to prevent UObject leaks) */
	TWeakObjectPtr<UMaterialInstanceDynamic> DynamicMaterialX;  // Red
	TWeakObjectPtr<UMaterialInstanceDynamic> DynamicMaterialY;  // Green
	TWeakObjectPtr<UMaterialInstanceDynamic> DynamicMaterialZ;  // Blue

	/** Dynamic material instances for shafts - Issue #213 */
	TWeakObjectPtr<UMaterialInstanceDynamic> DynamicShaftMaterialX;
	TWeakObjectPtr<UMaterialInstanceDynamic> DynamicShaftMaterialY;
	TWeakObjectPtr<UMaterialInstanceDynamic> DynamicShaftMaterialZ;
	
	/** Pending attachment - hologram waiting for assets to load */
	TWeakObjectPtr<USceneComponent> PendingAttachTarget;
	
	/** Timer handle for deferred attachment timeout */
	FTimerHandle DeferredAttachTimerHandle;

	/** Current configuration */
	FArrowConfig Config;

	/** Current color scheme */
	FArrowColorScheme ColorScheme;

	/** Last axis state */
	ELastAxisInput CurrentLastAxis;

	/** Modifier key states */
	bool bLeftShiftPressed;
	bool bLeftCtrlPressed;

	/** Visibility state */
	bool bCurrentlyVisible;

	/** Initialization state */
	bool bInitialized;

	/** Config toggles for orbit and labels */
	bool bOrbitEnabled = true;
	bool bLabelsUserVisible = true;

	/** Last known child count for detecting grid changes */
	int32 LastKnownChildCount;

	// ===== Cached state for per-frame label orbit (Issue #213) =====
	FVector CachedShaftMidpointX;
	FVector CachedShaftMidpointY;
	FVector CachedShaftMidpointZ;

	/** Calculate effective highlighted axis */
	ELastAxisInput CalculateHighlightedAxis() const;

	/** Update single composite arrow (head + shaft) */
	void UpdateSingleArrow(
		UStaticMeshComponent* Head,
		UStaticMeshComponent* Shaft,
		const FVector& Location,
		const FRotator& Rotation,
		ELastAxisInput Axis,
		ELastAxisInput HighlightedAxis
	);

	/** Update text label position and billboard rotation - Issue #213 */
	void UpdateLabel(
		UTextRenderComponent* Label,
		const FVector& ArrowTipLocation,
		const FRotator& ArrowRotation,
		UWorld* World
	);

	/** Create a text label component - Issue #213 */
	UTextRenderComponent* CreateLabelComponent(UObject* Outer, const FName& Name, const FText& Text, const FColor& Color);

	/** Load arrow mesh and materials (DEPRECATED - use AssetManager) */
	bool LoadAssets();

	/** Create arrow component */
	UStaticMeshComponent* CreateArrowComponent(UObject* Outer, const FName& Name);

	/** Configure arrow component settings */
	void ConfigureArrowComponent(UStaticMeshComponent* Component);

	/** Get scale for axis (1.0 or HighlightScale) */
	float GetScaleForAxis(ELastAxisInput Axis, ELastAxisInput HighlightedAxis) const;
	
	/** Complete deferred attachment after assets load (Task #58) */
	void CompleteDeferredAttachment();
	
	/** Apply mesh and materials to arrow head and shaft components (Task #58, Issue #213) */
	bool ApplyMeshAndMaterials(UStaticMesh* HeadMesh, UStaticMesh* CylinderMesh, UMaterialInterface* Material);

	/** Calculate hologram bounds for dynamic positioning (Task #67 Phase 1) */
	FHologramBounds CalculateHologramBounds() const;

	/** Get buildable size from registry or fallback */
	FVector GetBuildableSize() const;

	/** Calculate highest child Z for multi-level grids */
	float CalculateHighestChildZ() const;
};
