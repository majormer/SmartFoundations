// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Arrow Visualization using StaticMeshComponent Implementation

#include "Features/Arrows/SFArrowModule_StaticMesh.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"
#include "Core/Helpers/SFNetworkHelper.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "FGHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

FSFArrowModule_StaticMesh::FSFArrowModule_StaticMesh()
	: ArrowX(nullptr)
	, ArrowY(nullptr)
	, ArrowZ(nullptr)
	, ShaftX(nullptr)
	, ShaftY(nullptr)
	, ShaftZ(nullptr)
	, LabelX(nullptr)
	, LabelY(nullptr)
	, LabelZ(nullptr)
	, AssetManager()
	, ArrowMesh(nullptr)
	, ShaftMesh(nullptr)
	, MaterialX(nullptr)
	, MaterialY(nullptr)
	, MaterialZ(nullptr)
	, DynamicMaterialX(nullptr)
	, DynamicMaterialY(nullptr)
	, DynamicMaterialZ(nullptr)
	, DynamicShaftMaterialX(nullptr)
	, DynamicShaftMaterialY(nullptr)
	, DynamicShaftMaterialZ(nullptr)
	, PendingAttachTarget(nullptr)
	, Config()
	, ColorScheme()
	, CurrentLastAxis(ELastAxisInput::None)
	, bLeftShiftPressed(false)
	, bLeftCtrlPressed(false)
	, bCurrentlyVisible(false)
	, bInitialized(false)
	, LastKnownChildCount(0)
{
	UE_LOG(LogSmartFoundations, Log, TEXT("FSFArrowModule_StaticMesh: Created (Issue #213: composite arrows + text labels)"));
}

FSFArrowModule_StaticMesh::~FSFArrowModule_StaticMesh()
{
	Cleanup();
}

bool FSFArrowModule_StaticMesh::Initialize(UWorld* World, UObject* Outer, USFSubsystem* Subsystem)
{
	if (!World || !Outer)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("FSFArrowModule_StaticMesh::Initialize: Invalid World or Outer"));
		return false;
	}

	// Store subsystem reference for bounds calculation (Task #67)
	SubsystemRef = Subsystem;
	if (Subsystem)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("Initialize: Subsystem reference stored for bounds calculation"));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Initialize: No subsystem reference provided, bounds calculation will use fallbacks"));
	}

	// Task #58: Start async asset loading
	// Assets will be applied when AttachToHologram() is called and they're ready
	SF_LOG_ARROWS(Normal, TEXT("🚀 Initialize: Starting async asset load (Task #58)..."));
	
	bool bLoadStarted = AssetManager.LoadAssetsAsync([this, Outer](bool bSuccess, UStaticMesh* HeadMesh, UStaticMesh* CylinderMesh, UMaterialInterface* Material)
	{
		if (bSuccess)
		{
			ArrowMesh = HeadMesh;
			ShaftMesh = CylinderMesh;
			MaterialX = Material;
			MaterialY = Material;
			MaterialZ = Material;
			
			SF_LOG_ARROWS(Normal, TEXT("✅ Initialize: Assets loaded (head + shaft + material)"));
			
			if (PendingAttachTarget.IsValid())
			{
				SF_LOG_ARROWS(Normal, TEXT("⏳ Initialize: Deferred attachment pending - completing now"));
				CompleteDeferredAttachment();
			}
		}
		else
		{
			SF_LOG_ERROR(Arrows, TEXT("Asset load failed"));
		}
	});

	if (!bLoadStarted)
	{
		SF_LOG_ERROR(Arrows, TEXT("Initialize: Failed to start asset load"));
		return false;
	}

	// Create arrow head components (Cone) - mesh set later when assets load
	ArrowX = CreateArrowComponent(Outer, TEXT("SmartArrowX"));
	ArrowY = CreateArrowComponent(Outer, TEXT("SmartArrowY"));
	ArrowZ = CreateArrowComponent(Outer, TEXT("SmartArrowZ"));

	// Issue #213: Create shaft components (Cylinder)
	ShaftX = CreateArrowComponent(Outer, TEXT("SmartShaftX"));
	ShaftY = CreateArrowComponent(Outer, TEXT("SmartShaftY"));
	ShaftZ = CreateArrowComponent(Outer, TEXT("SmartShaftZ"));

	// Issue #213: Create text label components
	LabelX = CreateLabelComponent(Outer, TEXT("SmartLabelX"), LOCTEXT("ArrowLabel_X", "X"), ColorScheme.ColorX);
	LabelY = CreateLabelComponent(Outer, TEXT("SmartLabelY"), LOCTEXT("ArrowLabel_Y", "Y"), ColorScheme.ColorY);
	LabelZ = CreateLabelComponent(Outer, TEXT("SmartLabelZ"), LOCTEXT("ArrowLabel_Z", "Z"), ColorScheme.ColorZ);

	if (!ArrowX.IsValid() || !ArrowY.IsValid() || !ArrowZ.IsValid() ||
		!ShaftX.IsValid() || !ShaftY.IsValid() || !ShaftZ.IsValid())
	{
		SF_LOG_ERROR(Arrows, TEXT("Initialize: Failed to create arrow/shaft components"));
		Cleanup();
		return false;
	}

	// Initially hide everything
	ArrowX.Get()->SetVisibility(false);
	ArrowY.Get()->SetVisibility(false);
	ArrowZ.Get()->SetVisibility(false);
	ShaftX.Get()->SetVisibility(false);
	ShaftY.Get()->SetVisibility(false);
	ShaftZ.Get()->SetVisibility(false);
	if (LabelX.IsValid()) LabelX.Get()->SetVisibility(false);
	if (LabelY.IsValid()) LabelY.Get()->SetVisibility(false);
	if (LabelZ.IsValid()) LabelZ.Get()->SetVisibility(false);

	bInitialized = true;
	SF_LOG_ARROWS(Normal, TEXT("✅ Initialize: Complete (Issue #213: heads + shafts + labels ready, assets loading async)"));
	
	return true;
}

bool FSFArrowModule_StaticMesh::AttachToHologram(USceneComponent* HologramRootComponent)
{
	if (!bInitialized)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFArrowModule_StaticMesh::AttachToHologram: Not initialized"));
		return false;
	}

	if (!HologramRootComponent)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("FSFArrowModule_StaticMesh::AttachToHologram: Invalid hologram root component"));
		return false;
	}

	// SAFETY: Don't attach arrows during world teardown - assets may be invalid
	UWorld* World = HologramRootComponent->GetWorld();
	if (!World || World->bIsTearingDown)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFArrowModule_StaticMesh::AttachToHologram: Skipping during world teardown"));
		return false;
	}

	// Task #67 Cleanup: Safety check - auto-detach if already attached
	if (IsAttachedToHologram())
	{
		UE_LOG(LogSmartFoundations, Warning, 
			TEXT("AttachToHologram called while already attached - auto-detaching first to prevent component leaks"));
		DetachFromHologram();
	}

	AActor* HologramActor = HologramRootComponent->GetOwner();
	if (!HologramActor)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("FSFArrowModule_StaticMesh::AttachToHologram: No actor owner"));
		return false;
	}

	// Only create new components if they don't exist or are from a different hologram
	// This avoids recreating components unnecessarily
	// CRITICAL: Check ALL three arrow components, not just ArrowX
	// TWeakObjectPtr safety: IsValid() automatically handles dangling pointers
	const bool bNeedNewComponents = !ArrowX.IsValid() || !ArrowY.IsValid() || !ArrowZ.IsValid() ||
	                                !ArrowX.Get()->GetOwner() || !ArrowY.Get()->GetOwner() || !ArrowZ.Get()->GetOwner() ||
	                                ArrowX.Get()->GetOwner() != HologramActor;
	
	if (bNeedNewComponents)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Creating new arrow components for hologram %s"), *HologramActor->GetName());
		
		// Arrow heads (Cone)
		ArrowX = NewObject<UStaticMeshComponent>(HologramActor, UStaticMeshComponent::StaticClass(), TEXT("SmartArrowX"));
		ArrowY = NewObject<UStaticMeshComponent>(HologramActor, UStaticMeshComponent::StaticClass(), TEXT("SmartArrowY"));
		ArrowZ = NewObject<UStaticMeshComponent>(HologramActor, UStaticMeshComponent::StaticClass(), TEXT("SmartArrowZ"));
		
		// Issue #213: Arrow shafts (Cylinder)
		ShaftX = NewObject<UStaticMeshComponent>(HologramActor, UStaticMeshComponent::StaticClass(), TEXT("SmartShaftX"));
		ShaftY = NewObject<UStaticMeshComponent>(HologramActor, UStaticMeshComponent::StaticClass(), TEXT("SmartShaftY"));
		ShaftZ = NewObject<UStaticMeshComponent>(HologramActor, UStaticMeshComponent::StaticClass(), TEXT("SmartShaftZ"));

		// Issue #213: Text labels
		LabelX = CreateLabelComponent(HologramActor, TEXT("SmartLabelX"), LOCTEXT("ArrowLabel_X", "X"), ColorScheme.ColorX);
		LabelY = CreateLabelComponent(HologramActor, TEXT("SmartLabelY"), LOCTEXT("ArrowLabel_Y", "Y"), ColorScheme.ColorY);
		LabelZ = CreateLabelComponent(HologramActor, TEXT("SmartLabelZ"), LOCTEXT("ArrowLabel_Z", "Z"), ColorScheme.ColorZ);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Reusing existing arrow components for same hologram"));
	}
	
	// Configure all mesh components
	ConfigureArrowComponent(ArrowX.Get());
	ConfigureArrowComponent(ArrowY.Get());
	ConfigureArrowComponent(ArrowZ.Get());
	ConfigureArrowComponent(ShaftX.Get());
	ConfigureArrowComponent(ShaftY.Get());
	ConfigureArrowComponent(ShaftZ.Get());
	
	// Check if assets are fully loaded and ready
	const bool bAssetsReady = AssetManager.IsLoaded() && 
	                          FSFArrowAssetManager::IsStaticMeshFullyReady(ArrowMesh.Get()) &&
	                          FSFArrowAssetManager::IsStaticMeshFullyReady(ShaftMesh.Get()) &&
	                          FSFArrowAssetManager::IsMaterialFullyReady(MaterialX.Get());

	SF_LOG_ARROWS(Normal, TEXT("🔗 AttachToHologram: Assets ready=%s, attempting attachment"), bAssetsReady ? TEXT("YES") : TEXT("NO"));

	if (bAssetsReady)
	{
		SF_LOG_ARROWS(Normal, TEXT("✅ AttachToHologram: Assets ready, applying immediately"));
		
		if (!ApplyMeshAndMaterials(ArrowMesh.Get(), ShaftMesh.Get(), MaterialX.Get()))
		{
			SF_LOG_ERROR(Arrows, TEXT("AttachToHologram: Failed to apply mesh and materials"));
			return false;
		}

		// Attach heads, shafts, and labels to hologram
		auto AttachComp = [&](USceneComponent* Comp)
		{
			if (Comp)
			{
				Comp->AttachToComponent(HologramRootComponent, FAttachmentTransformRules::KeepRelativeTransform);
				if (!Comp->IsRegistered()) Comp->RegisterComponent();
			}
		};
		AttachComp(ArrowX.Get()); AttachComp(ArrowY.Get()); AttachComp(ArrowZ.Get());
		AttachComp(ShaftX.Get()); AttachComp(ShaftY.Get()); AttachComp(ShaftZ.Get());
		AttachComp(LabelX.Get()); AttachComp(LabelY.Get()); AttachComp(LabelZ.Get());
		
		SF_LOG_ARROWS(Normal, TEXT("✅ AttachToHologram: Arrows + shafts + labels attached"));
		return true;
	}
	else
	{
		// ⏳ Assets not ready - defer attachment
		SF_LOG_ARROWS(Normal, TEXT("⏳ AttachToHologram: Assets not ready yet, deferring attachment (will complete when assets load)"));
		
		PendingAttachTarget = HologramRootComponent;
		
		// Set timeout timer (5 seconds)
		World->GetTimerManager().SetTimer(
			DeferredAttachTimerHandle,
			[this]()
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("Deferred attachment TIMEOUT (5s) - arrows will not appear"));
				if (PendingAttachTarget.IsValid())
				{
					SF_LOG_WARNING(Arrows, TEXT("Deferred attachment TIMEOUT (5s) - arrows will not appear"));
					PendingAttachTarget.Reset();
				}
				UE_LOG(LogSmartFoundations, Log, TEXT("Deferred attachment timer completed and cleaned up"));
			},
			5.0f,  // 5 second timeout
			false   // No loop
		);
		
		UE_LOG(LogSmartFoundations, Log, TEXT("AttachToHologram: ⏳ Deferred attachment timer set (5s timeout)"));
		
		// Assets will call CompleteDeferredAttachment() when they finish loading
		return false;  // Not attached yet, but will be soon
	}
}

void FSFArrowModule_StaticMesh::DetachFromHologram()
{
	if (!bInitialized)
	{
		return;
	}
	
	// SAFETY: During world teardown, skip detachment entirely to avoid accessing destroyed parents
	if (ArrowX.IsValid() && ArrowX.Get()->GetWorld() && ArrowX.Get()->GetWorld()->bIsTearingDown)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("DetachFromHologram: Skipping detachment during world teardown"));
		return;
	}
	
	// ADDITIONAL SAFETY: Check if world is valid and not shutting down
	UWorld* ComponentWorld = nullptr;
	if (ArrowX.IsValid())
	{
		ComponentWorld = ArrowX.Get()->GetWorld();
	}
	
	if (!ComponentWorld || ComponentWorld->bIsTearingDown)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("DetachFromHologram: World invalid or tearing down, skipping detachment"));
		return;
	}

	// Safely detach, unregister, and hide all components
	auto SafeDetach = [](auto& Comp)
	{
		if (Comp.IsValid())
		{
			Comp.Get()->SetVisibility(false);
			if (Comp.Get()->IsRegistered())
			{
				Comp.Get()->UnregisterComponent();
			}
			if (Comp.Get()->GetAttachParent() && IsValid(Comp.Get()->GetAttachParent()))
			{
				Comp.Get()->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}
		}
	};

	// Arrow heads
	SafeDetach(ArrowX); SafeDetach(ArrowY); SafeDetach(ArrowZ);
	// Shafts (Issue #213)
	SafeDetach(ShaftX); SafeDetach(ShaftY); SafeDetach(ShaftZ);
	// Labels (Issue #213)
	SafeDetach(LabelX); SafeDetach(LabelY); SafeDetach(LabelZ);

	UE_LOG(LogSmartFoundations, Log, TEXT("Arrows + shafts + labels detached from hologram"));
}

void FSFArrowModule_StaticMesh::Cleanup()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("Cleanup: Starting arrow module cleanup"));
	
	// Task #58: Cancel any pending asset loads and deferred attachments
	AssetManager.CancelPendingLoads();
	PendingAttachTarget.Reset();
	
	// Task #67 Cleanup: Clear deferred attachment timer if active
	// Safe approach: Get world from component owner, not from dangling component pointers
	if (DeferredAttachTimerHandle.IsValid())
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("Cleanup: Found active deferred attachment timer"));
		
		// Try to get world from ArrowX's owner (subsystem) if component still valid
		if (ArrowX.IsValid())
		{
			if (UObject* Owner = ArrowX.Get()->GetOuter())
			{
				if (UWorld* World = Owner->GetWorld())
				{
					if (World->GetTimerManager().IsTimerActive(DeferredAttachTimerHandle))
					{
						World->GetTimerManager().ClearTimer(DeferredAttachTimerHandle);
						UE_LOG(LogSmartFoundations, Log, TEXT("Cleanup: ✅ Cleared deferred attachment timer"));
					}
					else
					{
						UE_LOG(LogSmartFoundations, Log, TEXT("Cleanup: Timer exists but not active"));
					}
				}
				else
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("Cleanup: Owner has no world"));
				}
			}
			else
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("Cleanup: ArrowX has no valid owner"));
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("Cleanup: ArrowX invalid, force invalidating timer"));
		}
		
		// If we can't get world safely, timer will be cleaned up by engine anyway
		DeferredAttachTimerHandle.Invalidate();
		UE_LOG(LogSmartFoundations, Log, TEXT("Cleanup: Timer handle invalidated"));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("Cleanup: No active deferred attachment timer"));
	}
	
	// Early exit if arrows were never initialized (never attached to a hologram)
	// This happens during world transitions when no build gun was ever equipped
	if (!ArrowX.IsValid() && !ArrowY.IsValid() && !ArrowZ.IsValid())
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("FSFArrowModule_StaticMesh::Cleanup: Nothing to clean up (arrows never attached)"));
		bInitialized = false;
		return;
	}
	
	// CRITICAL DECISION: NEVER manually detach components in Cleanup()
	// 
	// Problem: We can't reliably determine if world is shutting down
	// - Pointer checks catch some cases but not all
	// - IsValid() can return true for poison values
	// - GetWorld() can crash even after checks pass
	// - World state can change between check and action
	//
	// Solution: Let Unreal Engine handle ALL component cleanup automatically
	// - Components are owned by the hologram actor
	// - Engine cleans up actor components during world shutdown
	// - Manual DetachFromHologram() only called when hologram changes (RegisterActiveHologram)
	// 
	// Trade-off: May leak components if Cleanup() called during normal gameplay
	// Reality: Cleanup() is only called during subsystem Deinitialize (world transitions)
	// Verdict: Safe to skip manual detach - engine handles it
	
	UE_LOG(LogSmartFoundations, Log, TEXT("FSFArrowModule_StaticMesh::Cleanup: Skipping manual detach - letting engine clean up components"));

	// Just null out our references - engine will garbage collect the components
	ArrowX.Reset(); ArrowY.Reset(); ArrowZ.Reset();
	ShaftX.Reset(); ShaftY.Reset(); ShaftZ.Reset();
	LabelX.Reset(); LabelY.Reset(); LabelZ.Reset();
	
	// Clean up dynamic material instances
	DynamicMaterialX.Reset(); DynamicMaterialY.Reset(); DynamicMaterialZ.Reset();
	DynamicShaftMaterialX.Reset(); DynamicShaftMaterialY.Reset(); DynamicShaftMaterialZ.Reset();
	UE_LOG(LogSmartFoundations, Log, TEXT("Cleanup: All component references cleared"));

	bInitialized = false;
	UE_LOG(LogSmartFoundations, Log, TEXT("FSFArrowModule_StaticMesh: Cleaned up (Task #58: async loads cancelled, Task #67: dynamic materials cleared)"));
}

void FSFArrowModule_StaticMesh::UpdateArrows(UWorld* World, const FTransform& BaseTransform, ELastAxisInput HighlightedAxis, bool bVisible)
{
	// Log update frequency to diagnose UObject creation
	static int32 UpdateCount = 0;
	UpdateCount++;
	if (UpdateCount % 1000 == 0)  // Log every 1000 updates to reduce spam
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("UpdateArrows: Called %d times - checking for UObject leaks"), UpdateCount);
	}
	
	// DEBUG: Log every 10th call to reduce spam
	if (UpdateCount % 10 == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 UpdateArrows: DEBUG - Called #%d, Visible=%s"), UpdateCount, bVisible ? TEXT("YES") : TEXT("NO"));
	}
	
	if (!World || !ArrowX.IsValid() || !ArrowY.IsValid() || !ArrowZ.IsValid())
	{
		return;
	}

	// Early exit if arrows should be hidden
	if (!ArrowX.IsValid() || !ArrowY.IsValid() || !ArrowZ.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FSFArrowModule_StaticMesh::UpdateArrows: Components invalid or destroyed"));
		return;
	}

	bCurrentlyVisible = bVisible;
	CurrentLastAxis = HighlightedAxis;

	if (!bVisible)
	{
		ArrowX.Get()->SetVisibility(false);
		ArrowY.Get()->SetVisibility(false);
		ArrowZ.Get()->SetVisibility(false);
		if (ShaftX.IsValid()) ShaftX.Get()->SetVisibility(false);
		if (ShaftY.IsValid()) ShaftY.Get()->SetVisibility(false);
		if (ShaftZ.IsValid()) ShaftZ.Get()->SetVisibility(false);
		if (LabelX.IsValid()) LabelX.Get()->SetVisibility(false);
		if (LabelY.IsValid()) LabelY.Get()->SetVisibility(false);
		if (LabelZ.IsValid()) LabelZ.Get()->SetVisibility(false);
		return;
	}

	// Check if hologram is disabled and hide arrows if so
	// Also detect grid structure changes (child count changed)
	int32 CurrentChildCount = 0;
	bool bGridStructureChanged = false;
	
	if (SubsystemRef.IsValid())
	{
		AFGHologram* ActiveHologram = SubsystemRef->GetActiveHologram();
		if (ActiveHologram)
		{
			if (ActiveHologram->IsDisabled())
			{
				ArrowX.Get()->SetVisibility(false);
				ArrowY.Get()->SetVisibility(false);
				ArrowZ.Get()->SetVisibility(false);
				if (ShaftX.IsValid()) ShaftX.Get()->SetVisibility(false);
				if (ShaftY.IsValid()) ShaftY.Get()->SetVisibility(false);
				if (ShaftZ.IsValid()) ShaftZ.Get()->SetVisibility(false);
				if (LabelX.IsValid()) LabelX.Get()->SetVisibility(false);
				if (LabelY.IsValid()) LabelY.Get()->SetVisibility(false);
				if (LabelZ.IsValid()) LabelZ.Get()->SetVisibility(false);
				return;
			}
			
			// Check if grid structure changed
			CurrentChildCount = ActiveHologram->GetHologramChildren().Num();
			
			// Log child count for debugging (every 10th call)
			if (UpdateCount % 10 == 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 Child count check: Current=%d, Last=%d, Changed=%s"),
					CurrentChildCount, LastKnownChildCount, (CurrentChildCount != LastKnownChildCount) ? TEXT("YES") : TEXT("NO"));
			}
			
			if (CurrentChildCount != LastKnownChildCount)
			{
				bGridStructureChanged = true;
				
				// Log grid structure changes
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 UpdateArrows: Grid structure changed - %d children (was %d)"), 
					CurrentChildCount, LastKnownChildCount);
				
				LastKnownChildCount = CurrentChildCount;
			}
		}
	}

	// Task #67 Phase 1: Calculate dynamic hologram bounds
	FHologramBounds Bounds = CalculateHologramBounds();
	
	// Base location with dynamic Z offset (relative to hologram)
	float DynamicZOffset = Bounds.bIsValid ? Bounds.GetArrowZOffset() : Config.ZOffset;
	FVector RelativeLocation = FVector(0, 0, DynamicZOffset);
	
	// Log essential positioning data (every 10th call OR when grid structure changes)
	if (UpdateCount % 10 == 0 || bGridStructureChanged)
	{
		if (Bounds.bIsValid)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" ARROW POSITIONING: Dynamic Z=%.1f (TopZ=%.1f, Scale=%.2f)"), 
				DynamicZOffset, Bounds.TopZ, Bounds.GetArrowScaleFactor());
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" ARROW POSITIONING: Fallback Z=%.1f (bounds invalid)"), DynamicZOffset);
		}
	}

	// Mesh primitives point up (+Z in local space), rotated to point along axes
	// X-axis: Pitch -90, Yaw 180 — arrow points in grid expansion direction
	// Y-axis: Roll -90 — point along +Y
	// Z-axis: No rotation — point up
	
	FRotator RotationX = FRotator(-90.0f, 180.0f, 0.0f);
	UpdateSingleArrow(ArrowX.Get(), ShaftX.Get(), RelativeLocation, RotationX, ELastAxisInput::X, HighlightedAxis);
	
	FRotator RotationY = FRotator(0.0f, 0.0f, -90.0f);
	UpdateSingleArrow(ArrowY.Get(), ShaftY.Get(), RelativeLocation, RotationY, ELastAxisInput::Y, HighlightedAxis);
	
	FRotator RotationZ = FRotator::ZeroRotator;
	UpdateSingleArrow(ArrowZ.Get(), ShaftZ.Get(), RelativeLocation, RotationZ, ELastAxisInput::Z, HighlightedAxis);

	// Issue #213: Cache shaft midpoints for per-frame orbit tick
	auto GetShaftMidpoint = [&](const FRotator& Rot) -> FVector
	{
		const float TotalLengthScale = Config.VectorLength / 50.0f;
		const float ShaftLength = 100.0f * TotalLengthScale * 0.65f;  // 65% is shaft
		const FVector ConeAxis = Rot.RotateVector(FVector(0, 0, 1));
		return RelativeLocation + ConeAxis * (ShaftLength * 0.5f);  // Midpoint
	};
	CachedShaftMidpointX = GetShaftMidpoint(RotationX);
	CachedShaftMidpointY = GetShaftMidpoint(RotationY);
	CachedShaftMidpointZ = GetShaftMidpoint(RotationZ);

	// Initial label positions (TickLabelOrbits handles per-frame animation)
	UpdateLabel(LabelX.Get(), CachedShaftMidpointX, RotationX, World);
	UpdateLabel(LabelY.Get(), CachedShaftMidpointY, RotationY, World);
	UpdateLabel(LabelZ.Get(), CachedShaftMidpointZ, RotationZ, World);

	// Visibility: Show all when idle, show only active axis when modifier pressed
	const bool bModifierPressed = (HighlightedAxis != ELastAxisInput::None);
	
	auto SetAxisVisibility = [&](bool bShow,
		TWeakObjectPtr<UStaticMeshComponent>& Head,
		TWeakObjectPtr<UStaticMeshComponent>& Shaft,
		TWeakObjectPtr<UTextRenderComponent>& Label)
	{
		Head.Get()->SetVisibility(bShow);
		if (Shaft.IsValid()) Shaft.Get()->SetVisibility(bShow);
		if (Label.IsValid()) Label.Get()->SetVisibility(bShow);
	};

	if (bModifierPressed)
	{
		SetAxisVisibility(HighlightedAxis == ELastAxisInput::X, ArrowX, ShaftX, LabelX);
		SetAxisVisibility(HighlightedAxis == ELastAxisInput::Y, ArrowY, ShaftY, LabelY);
		SetAxisVisibility(HighlightedAxis == ELastAxisInput::Z, ArrowZ, ShaftZ, LabelZ);
	}
	else
	{
		SetAxisVisibility(true, ArrowX, ShaftX, LabelX);
		SetAxisVisibility(true, ArrowY, ShaftY, LabelY);
		SetAxisVisibility(true, ArrowZ, ShaftZ, LabelZ);
	}
	
	// Force render update when grid structure changes
	if (bGridStructureChanged)
	{
		SetAxisVisibility(false, ArrowX, ShaftX, LabelX);
		SetAxisVisibility(false, ArrowY, ShaftY, LabelY);
		SetAxisVisibility(false, ArrowZ, ShaftZ, LabelZ);
		
		if (bModifierPressed)
		{
			SetAxisVisibility(HighlightedAxis == ELastAxisInput::X, ArrowX, ShaftX, LabelX);
			SetAxisVisibility(HighlightedAxis == ELastAxisInput::Y, ArrowY, ShaftY, LabelY);
			SetAxisVisibility(HighlightedAxis == ELastAxisInput::Z, ArrowZ, ShaftZ, LabelZ);
		}
		else
		{
			SetAxisVisibility(true, ArrowX, ShaftX, LabelX);
			SetAxisVisibility(true, ArrowY, ShaftY, LabelY);
			SetAxisVisibility(true, ArrowZ, ShaftZ, LabelZ);
		}
	}
}

void FSFArrowModule_StaticMesh::SetHighlightedAxis(ELastAxisInput Axis)
{
	CurrentLastAxis = Axis;
}

void FSFArrowModule_StaticMesh::SetModifierKeys(bool bShift, bool bCtrl)
{
	bLeftShiftPressed = bShift;
	bLeftCtrlPressed = bCtrl;
}

void FSFArrowModule_StaticMesh::SetArrowColors(const FArrowColorScheme& NewColorScheme)
{
	ColorScheme = NewColorScheme;

	if (!bInitialized)
	{
		return;
	}

	// Update material colors
	if (ArrowX.IsValid())
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(ArrowX.Get()->GetMaterial(0));
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(ColorScheme.ColorX));
		}
	}
	if (ArrowY.IsValid())
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(ArrowY.Get()->GetMaterial(0));
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(ColorScheme.ColorY));
		}
	}
	if (ArrowZ.IsValid())
	{
		UMaterialInstanceDynamic* DynMat = Cast<UMaterialInstanceDynamic>(ArrowZ.Get()->GetMaterial(0));
		if (DynMat)
		{
			DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(ColorScheme.ColorZ));
		}
	}
}

void FSFArrowModule_StaticMesh::SetArrowConfig(const FArrowConfig& NewConfig)
{
	Config = NewConfig;
}

ELastAxisInput FSFArrowModule_StaticMesh::CalculateHighlightedAxis() const
{
	// Priority 1: Modifier keys override LastAxis
	if (bLeftShiftPressed && bLeftCtrlPressed)
	{
		return ELastAxisInput::Z;
	}
	else if (bLeftShiftPressed)
	{
		return ELastAxisInput::X;
	}
	else if (bLeftCtrlPressed)
	{
		return ELastAxisInput::Y;
	}

	// Priority 2: Use LastAxis
	return CurrentLastAxis;
}

void FSFArrowModule_StaticMesh::UpdateSingleArrow(
	UStaticMeshComponent* Head,
	UStaticMeshComponent* Shaft,
	const FVector& RelativeLocation,
	const FRotator& RelativeRotation,
	ELastAxisInput Axis,
	ELastAxisInput HighlightedAxis)
{
	if (!Head || !IsValid(Head))
	{
		return;
	}

	const float HighlightMult = GetScaleForAxis(Axis, HighlightedAxis);
	
	// Issue #213: Composite arrow proportions
	// Total visual length from Config.VectorLength (default 200cm → scale factor 4.0)
	// Shaft = 65% of total, Head = 35%
	const float TotalLengthScale = Config.VectorLength / 50.0f;  // 200/50 = 4.0
	const float ShaftLengthScale = TotalLengthScale * 0.65f;     // 2.6
	const float HeadLengthScale = TotalLengthScale * 0.35f;      // 1.4
	
	// Thickness: shaft is thin, head is wider
	const float ShaftThickness = 0.15f * HighlightMult;  // Thin shaft
	const float HeadThickness = 0.5f * HighlightMult;    // Wide arrowhead
	
	// Engine meshes are 100 units tall with pivot at center (Z=50)
	const FVector ConeAxis = RelativeRotation.RotateVector(FVector(0, 0, 1));

	// --- SHAFT (Cylinder) ---
	// Shaft base at RelativeLocation, extends outward
	if (Shaft && IsValid(Shaft))
	{
		const float ShaftHalfHeight = (100.0f * ShaftLengthScale) * 0.5f;
		const FVector ShaftCenter = RelativeLocation + ConeAxis * ShaftHalfHeight;
		
		Shaft->SetRelativeLocation(ShaftCenter);
		Shaft->SetRelativeRotation(RelativeRotation);
		Shaft->SetRelativeScale3D(FVector(ShaftThickness, ShaftThickness, ShaftLengthScale));
		Shaft->UpdateComponentToWorld();
	}
	
	// --- HEAD (Cone) ---
	// Head base starts where shaft ends
	const float ShaftEnd = 100.0f * ShaftLengthScale;  // Full shaft length in scaled units
	const float HeadHalfHeight = (100.0f * HeadLengthScale) * 0.5f;
	const FVector HeadCenter = RelativeLocation + ConeAxis * (ShaftEnd + HeadHalfHeight);
	
	Head->SetRelativeLocation(HeadCenter);
	Head->SetRelativeRotation(RelativeRotation);
	Head->SetRelativeScale3D(FVector(HeadThickness, HeadThickness, HeadLengthScale));
	Head->UpdateComponentToWorld();
}

bool FSFArrowModule_StaticMesh::LoadAssets()
{
	// Load arrow mesh from engine
	// Using a simple cone primitive - guaranteed to exist
	ArrowMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone"));
	
	if (!ArrowMesh.IsValid())
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Failed to load arrow mesh from /Engine/BasicShapes/Cone"));
		return false;
	}

	// Load or create basic material
	// For now, we'll use a simple unlit material
	// TODO: Create proper materials in Content if needed
	MaterialX = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	MaterialY = MaterialX;
	MaterialZ = MaterialX;

	if (!MaterialX.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Failed to load material, arrows will use default"));
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("FSFArrowModule_StaticMesh: Assets loaded successfully"));
	return true;
}

UStaticMeshComponent* FSFArrowModule_StaticMesh::CreateArrowComponent(UObject* Outer, const FName& Name)
{
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Outer, Name);
	
	if (Component)
	{
		ConfigureArrowComponent(Component);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("Created arrow component: %s"), *Name.ToString());
	}
	
	return Component;
}

void FSFArrowModule_StaticMesh::ConfigureArrowComponent(UStaticMeshComponent* Component)
{
	if (!Component)
	{
		return;
	}
	
	// Configure component settings
	Component->SetMobility(EComponentMobility::Movable);  // CRITICAL: Must be movable
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCastShadow(false);
	Component->SetIsReplicated(false);  // Local only
	Component->bRenderInMainPass = true;
	Component->bRenderInDepthPass = true;
	Component->SetVisibility(false);
}

float FSFArrowModule_StaticMesh::GetScaleForAxis(ELastAxisInput Axis, ELastAxisInput HighlightedAxis) const
{
	if (Axis == HighlightedAxis)
	{
		return Config.HighlightScale;
	}
	
	return 1.0f;
}

// ============================================================================
// Task #58: Safe Asset Application and Deferred Attachment
// ============================================================================

void FSFArrowModule_StaticMesh::CompleteDeferredAttachment()
{
	SF_LOG_ARROWS(Normal, TEXT("⏳ CompleteDeferredAttachment: Called"));
	
	if (!PendingAttachTarget.IsValid())
	{
		SF_LOG_ARROWS(Normal, TEXT("⏳ CompleteDeferredAttachment: No pending attachment target"));
		return;
	}

	USceneComponent* HologramRootComponent = PendingAttachTarget.Get();
	UWorld* World = HologramRootComponent->GetWorld();
	
	if (!World || World->bIsTearingDown)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("CompleteDeferredAttachment: World is tearing down, aborting"));
		PendingAttachTarget.Reset();
		return;
	}

	// Validate assets are fully ready before applying
	if (!FSFArrowAssetManager::IsStaticMeshFullyReady(ArrowMesh.Get()) || 
		!FSFArrowAssetManager::IsStaticMeshFullyReady(ShaftMesh.Get()) ||
		!FSFArrowAssetManager::IsMaterialFullyReady(MaterialX.Get()))
	{
		SF_LOG_ARROWS(Normal, TEXT("⏳ CompleteDeferredAttachment: Assets still not ready, will retry or timeout"));
		return;
	}
	
	SF_LOG_ARROWS(Normal, TEXT("✅ CompleteDeferredAttachment: Assets NOW ready, applying mesh and materials"));

	if (!ApplyMeshAndMaterials(ArrowMesh.Get(), ShaftMesh.Get(), MaterialX.Get()))
	{
		SF_LOG_ERROR(Arrows, TEXT("CompleteDeferredAttachment: Failed to apply mesh and materials"));
		PendingAttachTarget.Reset();
		return;
	}

	// Attach all components (heads, shafts, labels) to hologram
	auto AttachAndRegister = [&](USceneComponent* Comp)
	{
		if (Comp)
		{
			Comp->AttachToComponent(HologramRootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			if (!Comp->IsRegistered()) Comp->RegisterComponent();
		}
	};
	AttachAndRegister(ArrowX.Get()); AttachAndRegister(ArrowY.Get()); AttachAndRegister(ArrowZ.Get());
	AttachAndRegister(ShaftX.Get()); AttachAndRegister(ShaftY.Get()); AttachAndRegister(ShaftZ.Get());
	AttachAndRegister(LabelX.Get()); AttachAndRegister(LabelY.Get()); AttachAndRegister(LabelZ.Get());

	// Clear timer
	if (World->GetTimerManager().IsTimerActive(DeferredAttachTimerHandle))
	{
		World->GetTimerManager().ClearTimer(DeferredAttachTimerHandle);
	}

	PendingAttachTarget.Reset();
	SF_LOG_ARROWS(Normal, TEXT("✅ CompleteDeferredAttachment: SUCCESS - Arrows attached to hologram"));
}

bool FSFArrowModule_StaticMesh::ApplyMeshAndMaterials(UStaticMesh* HeadMesh, UStaticMesh* CylinderMesh, UMaterialInterface* Material)
{
	SF_LOG_ARROWS(Normal, TEXT("🎨 ApplyMeshAndMaterials: Called (Issue #213: head + shaft)"));
	
	if (!ArrowX.IsValid() || !ArrowY.IsValid() || !ArrowZ.IsValid())
	{
		SF_LOG_ERROR(Arrows, TEXT("ApplyMeshAndMaterials: Arrow head components not created"));
		return false;
	}

	if (!FSFArrowAssetManager::IsStaticMeshFullyReady(HeadMesh))
	{
		SF_LOG_ERROR(Arrows, TEXT("ApplyMeshAndMaterials: Head mesh failed readiness check"));
		return false;
	}

	if (!FSFArrowAssetManager::IsStaticMeshFullyReady(CylinderMesh))
	{
		SF_LOG_ERROR(Arrows, TEXT("ApplyMeshAndMaterials: Shaft mesh failed readiness check"));
		return false;
	}

	if (!FSFArrowAssetManager::IsMaterialFullyReady(Material))
	{
		SF_LOG_ERROR(Arrows, TEXT("ApplyMeshAndMaterials: Material failed readiness check"));
		return false;
	}

	// Set head meshes (Cone)
	ArrowX.Get()->SetStaticMesh(HeadMesh);
	ArrowY.Get()->SetStaticMesh(HeadMesh);
	ArrowZ.Get()->SetStaticMesh(HeadMesh);

	// Issue #213: Set shaft meshes (Cylinder)
	if (ShaftX.IsValid()) ShaftX.Get()->SetStaticMesh(CylinderMesh);
	if (ShaftY.IsValid()) ShaftY.Get()->SetStaticMesh(CylinderMesh);
	if (ShaftZ.IsValid()) ShaftZ.Get()->SetStaticMesh(CylinderMesh);

	// Create or reuse dynamic material instances
	UObject* Outer = ArrowX.Get()->GetOuter();
	if (Material && Outer)
	{
		// Helper to create/reuse a dynamic material with color
		auto GetOrCreateDynMat = [&](TWeakObjectPtr<UMaterialInstanceDynamic>& DynMat, const FColor& Color)
		{
			if (!DynMat.IsValid())
			{
				DynMat = UMaterialInstanceDynamic::Create(Material, Outer);
				DynMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(Color));
			}
			return DynMat.Get();
		};

		// Head materials
		ArrowX.Get()->SetMaterial(0, GetOrCreateDynMat(DynamicMaterialX, ColorScheme.ColorX));
		ArrowY.Get()->SetMaterial(0, GetOrCreateDynMat(DynamicMaterialY, ColorScheme.ColorY));
		ArrowZ.Get()->SetMaterial(0, GetOrCreateDynMat(DynamicMaterialZ, ColorScheme.ColorZ));

		// Issue #213: Shaft materials (same colors, separate instances for potential future differentiation)
		if (ShaftX.IsValid()) ShaftX.Get()->SetMaterial(0, GetOrCreateDynMat(DynamicShaftMaterialX, ColorScheme.ColorX));
		if (ShaftY.IsValid()) ShaftY.Get()->SetMaterial(0, GetOrCreateDynMat(DynamicShaftMaterialY, ColorScheme.ColorY));
		if (ShaftZ.IsValid()) ShaftZ.Get()->SetMaterial(0, GetOrCreateDynMat(DynamicShaftMaterialZ, ColorScheme.ColorZ));
	}

	SF_LOG_ARROWS(Normal, TEXT("✅ ApplyMeshAndMaterials: SUCCESS - heads + shafts have mesh and materials"));
	return true;
}

// ============================================================================
// Issue #213: Text Label Creation and Billboard Update
// ============================================================================

UTextRenderComponent* FSFArrowModule_StaticMesh::CreateLabelComponent(UObject* Outer, const FName& Name, const FText& Text, const FColor& Color)
{
	if (!Outer)
	{
		return nullptr;
	}

	UTextRenderComponent* Label = NewObject<UTextRenderComponent>(Outer, Name);
	if (Label)
	{
		Label->SetMobility(EComponentMobility::Movable);
		Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Label->SetCastShadow(false);
		Label->SetIsReplicated(false);
		Label->SetVisibility(false);
		Label->SetText(Text);
		Label->SetTextRenderColor(FColor::White);  // White for contrast — shaft color identifies the axis
		Label->SetWorldSize(40.0f);  // 40cm tall text — readable at building distance
		Label->SetHorizontalAlignment(EHTA_Center);
		Label->SetVerticalAlignment(EVRTA_TextCenter);
	}
	return Label;
}

void FSFArrowModule_StaticMesh::UpdateLabel(
	UTextRenderComponent* Label,
	const FVector& ShaftMidpointRelative,
	const FRotator& ArrowRotation,
	UWorld* World)
{
	if (!Label || !IsValid(Label) || !World)
	{
		return;
	}

	// Issue #213: Animated orbit — label revolves around the shaft axis
	// ~1 revolution every 2 seconds so it's always visible from some angle
	constexpr float OrbitRadius = 50.0f;   // cm offset from shaft center
	constexpr float OrbitPeriod = 2.0f;    // seconds per revolution

	// Shaft axis in local (relative) space — orbit stays correct as hologram rotates
	const FVector ArrowAxisLocal = ArrowRotation.RotateVector(FVector(0, 0, 1));

	// Two perpendicular vectors to the shaft axis (stable basis for the orbit plane)
	FVector Perp1, Perp2;
	ArrowAxisLocal.FindBestAxisVectors(Perp1, Perp2);

	// Time-based angle (Fmod prevents precision loss during long play sessions)
	const float Angle = FMath::Fmod(World->GetTimeSeconds(), OrbitPeriod) / OrbitPeriod * 2.0f * PI;
	const FVector OrbitOffset = (FMath::Cos(Angle) * Perp1 + FMath::Sin(Angle) * Perp2) * OrbitRadius;

	Label->SetRelativeLocation(ShaftMidpointRelative + OrbitOffset);

	// Billboard: rotate text to face the camera for readability
	APlayerController* PC = World->GetFirstPlayerController();
	if (PC)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

		FVector LabelWorldPos = Label->GetComponentLocation();
		FVector FaceDir = CameraLocation - LabelWorldPos;
		if (!FaceDir.IsNearlyZero())
		{
			Label->SetWorldRotation(FaceDir.Rotation());
		}
	}

	Label->UpdateComponentToWorld();
}

// ============================================================================
// Issue #213: Per-frame label orbit animation
// ============================================================================

void FSFArrowModule_StaticMesh::TickLabelOrbits(UWorld* World)
{
	if (!World || !bCurrentlyVisible)
	{
		return;
	}

	// If labels are hidden by user config, ensure they stay hidden
	if (!bLabelsUserVisible)
	{
		if (LabelX.IsValid()) LabelX->SetVisibility(false);
		if (LabelY.IsValid()) LabelY->SetVisibility(false);
		if (LabelZ.IsValid()) LabelZ->SetVisibility(false);
		return;
	}

	// Arrow rotations are constants (same as in UpdateArrows)
	static const FRotator RotationX(-90.0f, 180.0f, 0.0f);
	static const FRotator RotationY(0.0f, 0.0f, -90.0f);
	static const FRotator RotationZ(FRotator::ZeroRotator);

	if (bOrbitEnabled)
	{
		// Animated orbit (original behavior)
		UpdateLabel(LabelX.Get(), CachedShaftMidpointX, RotationX, World);
		UpdateLabel(LabelY.Get(), CachedShaftMidpointY, RotationY, World);
		UpdateLabel(LabelZ.Get(), CachedShaftMidpointZ, RotationZ, World);
	}
	else
	{
		// Static position: place at shaft midpoint with billboard facing only
		auto BillboardOnly = [&](UTextRenderComponent* Label, const FVector& MidpointRelative)
		{
			if (!Label || !IsValid(Label)) return;
			Label->SetRelativeLocation(MidpointRelative);
			APlayerController* PC = World->GetFirstPlayerController();
			if (PC)
			{
				FVector CameraLocation;
				FRotator CameraRotation;
				PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
				FVector FaceDir = CameraLocation - Label->GetComponentLocation();
				if (!FaceDir.IsNearlyZero())
				{
					Label->SetWorldRotation(FaceDir.Rotation());
				}
			}
			Label->UpdateComponentToWorld();
		};
		BillboardOnly(LabelX.Get(), CachedShaftMidpointX);
		BillboardOnly(LabelY.Get(), CachedShaftMidpointY);
		BillboardOnly(LabelZ.Get(), CachedShaftMidpointZ);
	}
}

void FSFArrowModule_StaticMesh::SetOrbitEnabled(bool bEnabled)
{
	bOrbitEnabled = bEnabled;
}

void FSFArrowModule_StaticMesh::SetLabelsVisible(bool bVisible)
{
	bLabelsUserVisible = bVisible;
	if (!bVisible)
	{
		if (LabelX.IsValid()) LabelX->SetVisibility(false);
		if (LabelY.IsValid()) LabelY->SetVisibility(false);
		if (LabelZ.IsValid()) LabelZ->SetVisibility(false);
	}
}

// ============================================================================
// Task #67 Phase 1: Hologram Bounds Calculation
// ============================================================================

FHologramBounds FSFArrowModule_StaticMesh::CalculateHologramBounds() const
{
	SF_LOG_ARROWS(Normal, TEXT("🔍 CalculateHologramBounds: Starting bounds calculation"));
	
	// Default to invalid bounds
	FHologramBounds InvalidBounds;
	
	// Check if we have subsystem reference
	if (!SubsystemRef.IsValid())
	{
		SF_LOG_ARROWS(Normal, TEXT("❌ CalculateHologramBounds: No subsystem reference, using default bounds"));
		return InvalidBounds;
	}
	
	SF_LOG_ARROWS(Normal, TEXT("✅ CalculateHologramBounds: Subsystem reference valid"));
	
	// Get buildable size
	FVector BuildableSize = GetBuildableSize();
	SF_LOG_ARROWS(Normal, TEXT("📏 CalculateHologramBounds: Buildable size detected = (%.1f, %.1f, %.1f)"), 
		BuildableSize.X, BuildableSize.Y, BuildableSize.Z);
	
	if (BuildableSize.IsNearlyZero())
	{
		SF_LOG_ARROWS(Normal, TEXT("❌ CalculateHologramBounds: Buildable size is zero, using default bounds"));
		return InvalidBounds;
	}
	
	// Calculate highest child Z for multi-level grids
	float HighestChildZ = CalculateHighestChildZ();
	SF_LOG_ARROWS(Normal, TEXT("📐 CalculateHologramBounds: Highest child Z calculated = %.1f"), HighestChildZ);
	
	// For arrow positioning, we need the grid height, not world coordinates
	// Get the actual grid Z dimension from the subsystem
	float GridHeightZ = BuildableSize.Z;  // Default to single buildable height
	
	if (SubsystemRef.IsValid())
	{
		AFGHologram* ActiveHologram = SubsystemRef->GetActiveHologram();
		if (ActiveHologram)
		{
			const TArray<AFGHologram*>& Children = ActiveHologram->GetHologramChildren();
			if (Children.Num() > 0)
			{
				// Multi-level grid: calculate actual grid height
				// Find the highest child relative to parent position
				FVector ParentPosition = ActiveHologram->GetActorLocation();
				float MaxRelativeZ = 0.0f;
				
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔍 BOUNDS: Parent at Z=%.1f, %d children"), 
					ParentPosition.Z, Children.Num());
				
				for (AFGHologram* Child : Children)
				{
					if (Child)
					{
						float ChildZ = Child->GetActorLocation().Z;
						
						// Skip children that haven't been positioned yet (still at world origin Z=0)
						// Newly spawned children will be at Z=0 until UpdateChildPositions() runs
						if (FMath::IsNearlyZero(ChildZ, 1.0f))
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Child at Z=0.0 (unpositioned, skipping)"));
							continue;
						}
						
						// Calculate relative Z WITHOUT Abs() - we only want children ABOVE parent
						// If child is below parent, RelativeZ will be negative and Max() will ignore it
						float RelativeZ = ChildZ - ParentPosition.Z;
						MaxRelativeZ = FMath::Max(MaxRelativeZ, RelativeZ);
						
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Child at Z=%.1f, relative=%.1f"), 
							ChildZ, RelativeZ);
					}
				}
				
				// Grid height = highest relative position + buildable height
				GridHeightZ = MaxRelativeZ + BuildableSize.Z;
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📐 BOUNDS: Grid height = %.1f (max relative=%.1f + buildable=%.1f)"), 
					GridHeightZ, MaxRelativeZ, BuildableSize.Z);
			}
		}
	}
	
	// Calculate bounds using grid height, not world coordinates
	FVector Center = FVector(0, 0, GridHeightZ / 2.0f);  // Center at half height
	FVector Extents = FVector(BuildableSize.X / 2.0f, BuildableSize.Y / 2.0f, GridHeightZ / 2.0f);
	float TopZ = GridHeightZ;
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📐 BOUNDS FINAL: TopZ=%.1f (should be grid height)"), TopZ);
	
	FHologramBounds CalculatedBounds(Center, Extents, TopZ);
	
	SF_LOG_ARROWS(Normal, TEXT("🎯 CalculateHologramBounds: FINAL RESULTS - Size=(%.1f,%.1f,%.1f) TopZ=%.1f ArrowOffset=%.1f"), 
		BuildableSize.X, BuildableSize.Y, BuildableSize.Z, TopZ, CalculatedBounds.GetArrowZOffset());
	
	return CalculatedBounds;
}

FVector FSFArrowModule_StaticMesh::GetBuildableSize() const
{
	SF_LOG_ARROWS(Normal, TEXT("🔍 GetBuildableSize: Starting buildable size detection"));
	
	// Check if we have subsystem reference
	if (!SubsystemRef.IsValid())
	{
		SF_LOG_ARROWS(Normal, TEXT("❌ GetBuildableSize: No subsystem reference, using fallback size"));
		return FVector(800.0f, 800.0f, 200.0f);  // Default foundation size
	}
	
	SF_LOG_ARROWS(Normal, TEXT("✅ GetBuildableSize: Subsystem reference valid"));
	
	// Get active hologram from subsystem
	AFGHologram* ActiveHologram = SubsystemRef->GetActiveHologram();
	if (!ActiveHologram)
	{
		SF_LOG_ARROWS(Normal, TEXT("❌ GetBuildableSize: No active hologram, using fallback size"));
		return FVector(800.0f, 800.0f, 200.0f);  // Default foundation size
	}
	
	SF_LOG_ARROWS(Normal, TEXT("✅ GetBuildableSize: Active hologram found"));
	
	// Get build class from hologram
	UClass* BuildClass = ActiveHologram->GetBuildClass();
	if (!BuildClass)
	{
		SF_LOG_ARROWS(Normal, TEXT("❌ GetBuildableSize: No build class, using fallback size"));
		return FVector(800.0f, 800.0f, 200.0f);  // Default foundation size
	}
	
	// Log the build class for debugging
	FName ClassName = BuildClass->GetFName();
	FString ClassNameStr = ClassName.ToString();
	SF_LOG_ARROWS(Normal, TEXT("🏗️ GetBuildableSize: Build class detected = '%s'"), *ClassNameStr);
	
	// Use SFBuildableSizeRegistry to get actual size
	FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildClass);
	FVector DetectedSize = Profile.DefaultSize;
	
	if (DetectedSize.IsNearlyZero())
	{
		SF_LOG_ARROWS(Normal, TEXT("⚠️ GetBuildableSize: Registry returned zero size for '%s', using default"), *ClassNameStr);
		DetectedSize = FVector(800.0f, 800.0f, 200.0f);  // Default fallback
	}
	else
	{
		SF_LOG_ARROWS(Normal, TEXT("✅ GetBuildableSize: Registry returned size = (%.1f, %.1f, %.1f)"), 
			DetectedSize.X, DetectedSize.Y, DetectedSize.Z);
	}
	
	// Also try to get actual bounds from hologram as additional verification
	FBox HologramBounds = ActiveHologram->GetComponentsBoundingBox(true);
	SF_LOG_ARROWS(Normal, TEXT("📦 GetBuildableSize: Actual hologram bounds = (%.1f, %.1f, %.1f)"), 
		HologramBounds.GetSize().X, HologramBounds.GetSize().Y, HologramBounds.GetSize().Z);
	
	return DetectedSize;
}

float FSFArrowModule_StaticMesh::CalculateHighestChildZ() const
{
	SF_LOG_ARROWS(Normal, TEXT("🔍 CalculateHighestChildZ: Starting highest child Z calculation"));
	
	// Check if we have subsystem reference
	if (!SubsystemRef.IsValid())
	{
		SF_LOG_ARROWS(Normal, TEXT("❌ CalculateHighestChildZ: No subsystem reference, using single level"));
		return 200.0f;  // Default single foundation height
	}
	
	SF_LOG_ARROWS(Normal, TEXT("✅ CalculateHighestChildZ: Subsystem reference valid"));
	
	// Get active hologram from subsystem
	AFGHologram* ActiveHologram = SubsystemRef->GetActiveHologram();
	if (!ActiveHologram)
	{
		SF_LOG_ARROWS(Normal, TEXT("❌ CalculateHighestChildZ: No active hologram, using single level"));
		return 200.0f;  // Default single foundation height
	}
	
	SF_LOG_ARROWS(Normal, TEXT("✅ CalculateHighestChildZ: Active hologram found"));
	
	// Get hologram children to determine grid structure
	const TArray<AFGHologram*>& Children = ActiveHologram->GetHologramChildren();
	SF_LOG_ARROWS(Normal, TEXT("👶 CalculateHighestChildZ: Found %d hologram children"), Children.Num());
	
	if (Children.Num() == 0)
	{
		// Single hologram - use its height
		FVector BuildableSize = GetBuildableSize();
		SF_LOG_ARROWS(Normal, TEXT("🏠 CalculateHighestChildZ: Single hologram detected, using height=%.1f"), BuildableSize.Z);
		return BuildableSize.Z;
	}
	
	// Multi-hologram grid - find highest child
	float HighestZ = 0.0f;
	FVector BuildableSize = GetBuildableSize();
	
	SF_LOG_ARROWS(Normal, TEXT("🏗️ CalculateHighestChildZ: Multi-hologram grid detected, analyzing %d children"), Children.Num());
	
	for (int32 i = 0; i < Children.Num(); i++)
	{
		AFGHologram* Child = Children[i];
		if (!Child) 
		{
			SF_LOG_ARROWS(Normal, TEXT("⚠️ CalculateHighestChildZ: Child %d is null, skipping"), i);
			continue;
		}
		
		// Get child's world position
		FVector ChildPosition = Child->GetActorLocation();
		
		// Calculate child's top Z (position + half height)
		float ChildTopZ = ChildPosition.Z + (BuildableSize.Z / 2.0f);
		
		SF_LOG_ARROWS(Normal, TEXT("📍 CalculateHighestChildZ: Child %d at Z=%.1f, top Z=%.1f"), 
			i, ChildPosition.Z, ChildTopZ);
		
		if (ChildTopZ > HighestZ)
		{
			HighestZ = ChildTopZ;
			SF_LOG_ARROWS(Normal, TEXT("🔺 CalculateHighestChildZ: New highest Z=%.1f from child %d"), HighestZ, i);
		}
	}
	
	// If no valid children found, use base height
	if (HighestZ == 0.0f)
	{
		HighestZ = BuildableSize.Z;
		SF_LOG_ARROWS(Normal, TEXT("⚠️ CalculateHighestChildZ: No valid children found, using base height=%.1f"), HighestZ);
	}
	
	SF_LOG_ARROWS(Normal, TEXT("🎯 CalculateHighestChildZ: FINAL RESULT - Grid with %d children, highest Z=%.1f"), 
		Children.Num(), HighestZ);
	
	return HighestZ;
}

#undef LOCTEXT_NAMESPACE
