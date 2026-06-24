// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * ASFConveyorBeltHologram - spline setup / auto-route / build-mode routing / mesh
 * generation-apply methods. Split out of SFConveyorBeltHologram.cpp (slice T8a, pure impl-split,
 * one class across two .cpp) to keep each file <2k. No behavior change.
 */

#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "SmartFoundations.h"
#include "Components/SplineMeshComponent.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Hologram/FGHologramBuildModeDescriptor.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildable.h"
#include "FGSplineComponent.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGBuildingDescriptor.h"
#include "DrawDebugHelpers.h"
#include "Data/SFHologramDataRegistry.h"
#include "Hologram/FGHologram.h"
#include "Subsystem/SFSubsystem.h"  // [#380] read configured BeltRoutingMode for lane belts
#include "FGConstructDisqualifier.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"
#include "FGConstructDisqualifier.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "EngineUtils.h"  // For TActorIterator
#include "Core/Helpers/SFExtendChainHelper.h"
#include "HAL/PlatformStackWalk.h"

void ASFConveyorBeltHologram::SetupBeltSpline(UFGFactoryConnectionComponent* StartConnector, UFGFactoryConnectionComponent* EndConnector)
{
    if (!StartConnector || !EndConnector)
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("SetupBeltSpline: Invalid connectors"));
        return;
    }
	if (StartConnector == EndConnector)
	{
		UE_LOG(LogSmartHologram, Verbose,
			TEXT("SetupBeltSpline: StartConnector == EndConnector (%s on %s, ptr=%p) on %s - refusing to build self-connection"),
			*StartConnector->GetName(),
			*GetNameSafe(StartConnector->GetOwner()),
			StartConnector,
			*GetName());
		return;
	}
    
    // Get connector positions and directions
    FVector StartPos = StartConnector->GetComponentLocation();
    FVector EndPos = EndConnector->GetComponentLocation();
    FVector StartNormal = StartConnector->GetConnectorNormal();
    FVector EndNormal = EndConnector->GetConnectorNormal();
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT SPLINE SETUP: %s"), *GetName());
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Start: %s (normal: %s)"), *StartPos.ToString(), *StartNormal.ToString());
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   End: %s (normal: %s)"), *EndPos.ToString(), *EndNormal.ToString());
    
    // Clear existing spline data
    mSplineData.Empty();
    
    // Calculate distance
    float Distance = FVector::Dist(StartPos, EndPos);
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   📐 Belt distance: %.1f cm"), Distance);
    
    // VANILLA 4-POINT BELT SPLINE: Belts use capped percentage formula
    // Point 0: Start connector (50cm tangent)
    // Point 1: Transition (100cm from start, 50cm arrive, 31% distance leave - max 600cm)
    // Point 2: Transition (100cm from end, 31% distance arrive - max 600cm, 50cm leave)
    // Point 3: End connector (50cm tangent)
    // 
    // Analysis from vanilla belts:
    // - 510cm belt: 158cm tangent (31.0%)
    // - 1924cm belt: 600cm tangent (31.2% but capped at 600cm)
    // - 2214cm belt: 600cm tangent (would be 687cm at 31%, but capped!)
    // Formula: CurveTangent = min(Distance * 0.31, 600.0)
    
    const float SmallTangent = 50.0f;                    // Fixed at connectors
    const float MaxCurveTangent = 600.0f;                // Maximum curve tangent length
    const float CurvePercent = 0.31f;                    // 31% of distance (from vanilla analysis)
    const float CurveTangent = FMath::Min(Distance * CurvePercent, MaxCurveTangent);  // Capped at 600cm
    const float TransitionOffset = 100.0f;               // FIXED 100cm
    
    // Calculate direction vector between connectors
    FVector Direction = (EndPos - StartPos).GetSafeNormal();
    
    // Point 0: Start connector
    FSplinePointData Point0;
    Point0.Location = StartPos;
    Point0.ArriveTangent = StartNormal * SmallTangent;
    Point0.LeaveTangent = StartNormal * SmallTangent;
    mSplineData.Add(Point0);
    
    // Point 1: Transition near start (100cm from start)
    FVector TransitionStart = StartPos + StartNormal * TransitionOffset;
    FSplinePointData Point1;
    Point1.Location = TransitionStart;
    Point1.ArriveTangent = StartNormal * SmallTangent;  // Small arriving from start
    Point1.LeaveTangent = Direction * CurveTangent;     // Large leaving toward middle
    mSplineData.Add(Point1);
    
    // Point 2: Transition near end (back from end toward start)
    // EndNormal points toward source, so we ADD it to go back from end toward start
    FVector TransitionEnd = EndPos + EndNormal * TransitionOffset;
    FSplinePointData Point2;
    Point2.Location = TransitionEnd;
    Point2.ArriveTangent = Direction * CurveTangent;    // Large arriving from middle
    Point2.LeaveTangent = -EndNormal * SmallTangent;    // Negate EndNormal to point toward destination
    mSplineData.Add(Point2);
    
    // Point 3: End connector
    FSplinePointData Point3;
    Point3.Location = EndPos;
    Point3.ArriveTangent = -EndNormal * SmallTangent;  // Negate EndNormal to match travel direction
    Point3.LeaveTangent = -EndNormal * SmallTangent;
    mSplineData.Add(Point3);
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   🔧 Vanilla 4-point belt: transitions (100cm), curve tangents (600cm), distance=%.1f cm"), Distance)
    
    // CRITICAL FIX: Position ACTOR and spline component at correct world location before setting points
    // This prevents coordinate system mismatch where spline points are interpreted as offsets
    
    // STEP 1: Position the actor itself at the belt start point
    SetActorLocation(StartPos);
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   🔧 Actor positioned at: %s"), *StartPos.ToString());
    
    if (mSplineComponent)
    {
        // STEP 2: Position spline component at the start of the belt (world space)
        FVector ComponentLocation = StartPos;
        mSplineComponent->SetWorldLocation(ComponentLocation);
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   🔧 Spline component positioned at: %s"), *ComponentLocation.ToString());
        
        // Convert ALL control points and tangents from world to local space
        // This is critical for proper spline rendering in the component's coordinate system
        for (int32 i = 0; i < mSplineData.Num(); i++)
        {
            FVector WorldLocation = mSplineData[i].Location;
            FVector WorldArriveTangent = mSplineData[i].ArriveTangent;
            FVector WorldLeaveTangent = mSplineData[i].LeaveTangent;
            
            // Transform to local space
            FVector LocalLocation = mSplineComponent->GetComponentTransform().InverseTransformPosition(WorldLocation);
            FVector LocalArriveTangent = mSplineComponent->GetComponentTransform().InverseTransformVector(WorldArriveTangent);
            FVector LocalLeaveTangent = mSplineComponent->GetComponentTransform().InverseTransformVector(WorldLeaveTangent);
            
            // Update spline data with local coordinates
            mSplineData[i].Location = LocalLocation;
            mSplineData[i].ArriveTangent = LocalArriveTangent;
            mSplineData[i].LeaveTangent = LocalLeaveTangent;
            
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   📋 Point %d: Local=%s, Tangent=%s"), 
                i, *LocalLocation.ToString(), *LocalArriveTangent.ToString());
        }
    }
    
    // Update the spline component to make the belt visible
    // CRITICAL: Must call AFGSplineHologram explicitly (same pattern as pipe hologram)
    AFGSplineHologram::UpdateSplineComponent();
    
    // Verify actor position after spline setup
    FVector ActorPosAfterSetup = GetActorLocation();
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   🔍 VERIFY: Actor position after SetupBeltSpline: X=%.1f Y=%.1f Z=%.1f"), 
        ActorPosAfterSetup.X, ActorPosAfterSetup.Y, ActorPosAfterSetup.Z);
    
    if (ActorPosAfterSetup.IsNearlyZero(1.0f))
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("   ❌ CRITICAL: Actor reset to origin after SetupBeltSpline! Parent class may have moved it."));
    }
    
    // Note: Mesh generation will be handled by delayed UpdateSplineComponent() call
    // This allows FinishSpawning() -> BeginPlay() to populate mesh assets first
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   ⏳ Spline setup complete - mesh generation deferred to subsystem"));
    
    // Force visibility and check mesh components
    SetActorHiddenInGame(false);
    if (USplineComponent* SplineComp = mSplineComponent)
    {
        SplineComp->SetVisibility(true, true);
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Spline component visibility set"));
    }
    
    // Check if we have any mesh components
    TArray<UActorComponent*> MeshComponents;
    GetComponents(UMeshComponent::StaticClass(), MeshComponents);
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Found %d mesh components"), MeshComponents.Num());
    
    for (UActorComponent* Comp : MeshComponents)
    {
        if (UMeshComponent* Mesh = Cast<UMeshComponent>(Comp))
        {
            Mesh->SetVisibility(true, true);
            Mesh->SetHiddenInGame(false);
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Mesh component visibility forced"));
        }
    }
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Belt spline configured with %d points"), mSplineData.Num());
    
    // CRITICAL: Also set snapped connections so vanilla system knows this belt is connected
    // This is what makes auto-connect belts work - they have connectors available at setup time
    // The snapped connections tell the game to create unified chain actors during build
    SetSnappedConnections(StartConnector, EndConnector);
    UE_LOG(LogSmartHologram, Verbose,
        TEXT("🔗 SetupBeltSpline: Set snapped connections [0]=%s on %s (ptr=%p), [1]=%s on %s (ptr=%p)"),
        StartConnector ? *StartConnector->GetName() : TEXT("null"),
        StartConnector ? *GetNameSafe(StartConnector->GetOwner()) : TEXT("null"),
        StartConnector,
        EndConnector ? *EndConnector->GetName() : TEXT("null"),
        EndConnector ? *GetNameSafe(EndConnector->GetOwner()) : TEXT("null"),
        EndConnector);
}

void ASFConveyorBeltHologram::RouteLaneWithConfiguredMode(const FVector& StartPos, const FVector& StartNormal,
                                                          const FVector& EndPos, const FVector& EndNormal)
{
    // [#380] Honor the player's configured belt routing mode for Extend lane belts by driving the
    // VANILLA build-mode descriptors (ApplyBeltBuildModeRouting) - the same path auto-connect uses -
    // so Curve/Straight produce the real game spline. (Factory-internal belts are exact source clones
    // and do NOT use this; the manifold factory->distributor path is left untouched.)
    int32 BeltMode = 0;  // belt routing mode: 0=Default, 1=Curve, 2=Straight
    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
    {
        BeltMode = SmartSubsystem->GetAutoConnectRuntimeSettings().BeltRoutingMode;
    }
    ApplyBeltBuildModeRouting(BeltMode, StartPos, StartNormal, EndPos, EndNormal);
}

bool ASFConveyorBeltHologram::ApplyBeltBuildModeRouting(int32 BeltRoutingMode,
    const FVector& StartPos, const FVector& StartNormal, const FVector& EndPos, const FVector& EndNormal)
{
    // [#380] Smart spawns this belt hologram by C++ class (SpawnActor<ASFConveyorBeltHologram>), so the
    // BP-default build-mode descriptors (mBuildModeCurve / mBuildModeStraight) are NULL - which made the
    // vanilla path no-op and fall back to the flat AutoRouteSplineWithNormals (diagnostic: vanilla=0).
    // Lazily copy them from the belt's REAL hologram CDO: build class -> buildable CDO -> mHologramClass
    // -> its CDO. mBuildMode* are reached via the AccessTransformers friend on AFGConveyorBeltHologram.
    if (!mBuildModeCurve || !mBuildModeStraight)
    {
        if (const TSubclassOf<AActor> BuildClass = GetBuildClass())
        {
            if (const AFGBuildable* BuildableCDO = Cast<AFGBuildable>(BuildClass->GetDefaultObject()))
            {
                if (BuildableCDO->mHologramClass)
                {
                    if (const AFGConveyorBeltHologram* HoloCDO = Cast<AFGConveyorBeltHologram>(BuildableCDO->mHologramClass->GetDefaultObject()))
                    {
                        if (!mBuildModeCurve)    { mBuildModeCurve = HoloCDO->mBuildModeCurve; }
                        if (!mBuildModeStraight) { mBuildModeStraight = HoloCDO->mBuildModeStraight; }
                    }
                }
            }
        }
    }

    const bool bDescriptorsReady = (mBuildModeCurve != nullptr && mBuildModeStraight != nullptr);

    // [#380] Map Smart's belt routing mode (0=Default, 1=Curve, 2=Straight) to the vanilla belt build-mode
    // descriptor and let the GAME'S OWN AutoRouteSpline route the spline with real bends (mBendRadius) - identical
    // to what the build gun produces. We LOG which router actually wins (at Log level, so it shows in Shipping) so
    // testing — both Walking and stackable auto-connect — is never ambiguous: the in-game router should be the
    // primary path and the hand-rolled fallback a rare exception, NOT the 80% case. Filter the log by [BeltRoute].
    if (mSplineComponent)
    {
        TSubclassOf<UFGHologramBuildModeDescriptor> ModeDesc = nullptr;
        if (BeltRoutingMode == 1)      { ModeDesc = mBuildModeCurve; }
        else if (BeltRoutingMode == 2) { ModeDesc = mBuildModeStraight; }

        // Default (0) uses the belt's native build mode (no override). Curve/Straight override it.
        if (ModeDesc || BeltRoutingMode == 0)
        {
            if (ModeDesc) { SetBuildModeOverride(ModeDesc); }
            AutoRouteSpline(StartPos, StartNormal, EndPos, EndNormal);
            AFGSplineHologram::UpdateSplineComponent();
            const int32 Points = mSplineData.Num();
            const float Len = mSplineComponent ? mSplineComponent->GetSplineLength() : 0.0f;
            if (Points >= 2 && Len >= 50.0f)
            {
                UE_LOG(LogSmartHologram, Verbose, TEXT("[BeltRoute] IN-GAME AutoRouteSpline used (mode=%d points=%d len=%.0f descReady=%d) %s"),
                    BeltRoutingMode, Points, Len, bDescriptorsReady ? 1 : 0, *GetName());
                return true;
            }
            UE_LOG(LogSmartHologram, Verbose, TEXT("[BeltRoute] FALLBACK — in-game AutoRouteSpline returned a STUB (mode=%d points=%d len=%.0f descReady=%d) %s"),
                BeltRoutingMode, Points, Len, bDescriptorsReady ? 1 : 0, *GetName());
        }
        else
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("[BeltRoute] FALLBACK — no build-mode descriptor for mode=%d (descReady=%d) %s"),
                BeltRoutingMode, bDescriptorsReady ? 1 : 0, *GetName());
        }
    }
    else
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("[BeltRoute] FALLBACK — no spline component %s"), *GetName());
    }

    // Fallback: Smart's hand-rolled 4-point normals routing (descriptor unavailable or a stub in-game spline).
    AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
    return false;
}

void ASFConveyorBeltHologram::AutoRouteSplineWithNormals(const FVector& StartPos, const FVector& StartNormal,
                                                          const FVector& EndPos, const FVector& EndNormal)
{
    UE_LOG(LogSmartHologram, Verbose, TEXT("🔍 AutoRouteSplineWithNormals: Routing belt spline with VANILLA 4-POINT structure"));
    
    // Use vanilla 4-point spline structure (same as SetupBeltSpline)
    if (mSplineComponent)
    {
        // Clear existing spline
        mSplineData.Empty();
        mSplineComponent->ClearSplinePoints();
        
        // Calculate distance and direction
        float Distance = FVector::Dist(StartPos, EndPos);
        FVector Direction = (EndPos - StartPos).GetSafeNormal();
        
        // VANILLA 4-POINT BELT SPLINE: Scaled for distance
        const float SmallTangent = 50.0f;                    // Fixed at connectors
        const float MaxCurveTangent = 600.0f;                // Maximum curve tangent length
        const float CurvePercent = 0.31f;                    // 31% of distance (from vanilla analysis)
        const float CurveTangent = FMath::Min(Distance * CurvePercent, MaxCurveTangent);  // Capped at 600cm
        
        // TransitionOffset must be scaled for short belts to prevent crossing
        // Use 15% of distance, capped at 100cm max
        const float TransitionOffset = FMath::Min(Distance * 0.15f, 100.0f);
        
        // Point 0: Start connector
        FSplinePointData Point0;
        Point0.Location = StartPos;
        Point0.ArriveTangent = StartNormal * SmallTangent;
        Point0.LeaveTangent = StartNormal * SmallTangent;
        mSplineData.Add(Point0);
        
        // Point 1: Transition near start
        FVector TransitionStart = StartPos + StartNormal * TransitionOffset;
        FSplinePointData Point1;
        Point1.Location = TransitionStart;
        Point1.ArriveTangent = StartNormal * SmallTangent;
        Point1.LeaveTangent = Direction * CurveTangent;
        mSplineData.Add(Point1);
        
        // Point 2: Transition near end (back from end toward start)
        FVector TransitionEnd = EndPos + EndNormal * TransitionOffset;
        FSplinePointData Point2;
        Point2.Location = TransitionEnd;
        Point2.ArriveTangent = Direction * CurveTangent;
        Point2.LeaveTangent = -EndNormal * SmallTangent;
        mSplineData.Add(Point2);
        
        // Point 3: End connector
        FSplinePointData Point3;
        Point3.Location = EndPos;
        Point3.ArriveTangent = -EndNormal * SmallTangent;
        Point3.LeaveTangent = -EndNormal * SmallTangent;
        mSplineData.Add(Point3);
        
        // CRITICAL: Position ACTOR at start point (same as SetupBeltSpline)
        SetActorLocation(StartPos);
        
        // Position spline component at start
        mSplineComponent->SetWorldLocation(StartPos);
        
        // Convert all 4 points to local coordinates
        FTransform SplineTransform = mSplineComponent->GetComponentTransform();
        for (int32 i = 0; i < mSplineData.Num(); i++)
        {
            mSplineData[i].Location = SplineTransform.InverseTransformPosition(mSplineData[i].Location);
            mSplineData[i].ArriveTangent = SplineTransform.InverseTransformVector(mSplineData[i].ArriveTangent);
            mSplineData[i].LeaveTangent = SplineTransform.InverseTransformVector(mSplineData[i].LeaveTangent);
        }
        
        // Update spline component - MUST use AFGSplineHologram explicitly (same as pipe)
        // Super::UpdateSplineComponent() calls AFGConveyorBeltHologram which doesn't transfer mSplineData
        AFGSplineHologram::UpdateSplineComponent();
        
        UE_LOG(LogSmartHologram, Verbose, TEXT("🔍 Auto-routed belt spline: %d points, %.1f cm, spline length=%.1f"),
            mSplineData.Num(), Distance, mSplineComponent ? mSplineComponent->GetSplineLength() : 0.0f);
    }
    else
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🔍 AutoRouteSplineWithNormals: No spline component!"));
    }
}

bool ASFConveyorBeltHologram::TryUseBuildModeRouting(
	const FVector& StartPos,
	const FVector& StartNormal,
	const FVector& EndPos,
	const FVector& EndNormal)
{
	if (!mSplineComponent)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("🔍 TryUseBuildModeRouting FAILED: No mSplineComponent on %s"), *GetName());
		return false;
	}
	
	// Select routing mode (set by HUD auto-connect settings).
	// 0=Auto, 1=2D, 2=Straight, 3=Curve
	if (RoutingMode == 2)
	{
		// Straight: Force a simple 2-point spline (leave tangents as zero).
		SetActorLocation(StartPos);
		mSplineComponent->SetWorldLocation(StartPos);
		mSplineData.Empty();
		mSplineComponent->ClearSplinePoints();
		
		FSplinePointData Point0;
		Point0.Location = StartPos;
		Point0.ArriveTangent = FVector::ZeroVector;
		Point0.LeaveTangent = FVector::ZeroVector;
		mSplineData.Add(Point0);
		
		FSplinePointData Point1;
		Point1.Location = EndPos;
		Point1.ArriveTangent = FVector::ZeroVector;
		Point1.LeaveTangent = FVector::ZeroVector;
		mSplineData.Add(Point1);
		
		FTransform SplineTransform = mSplineComponent->GetComponentTransform();
		for (int32 i = 0; i < mSplineData.Num(); i++)
		{
			mSplineData[i].Location = SplineTransform.InverseTransformPosition(mSplineData[i].Location);
			mSplineData[i].ArriveTangent = SplineTransform.InverseTransformVector(mSplineData[i].ArriveTangent);
			mSplineData[i].LeaveTangent = SplineTransform.InverseTransformVector(mSplineData[i].LeaveTangent);
		}
	}
	else if (RoutingMode == 3)
	{
		// Curve: Use Smart's existing 4-point curve generator.
		AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
	}
	else if (RoutingMode == 1)
	{
		// 2D: Curve routing but flattened to horizontal plane.
		FVector FlatStartNormal = FVector(StartNormal.X, StartNormal.Y, 0.0f).GetSafeNormal();
		FVector FlatEndNormal = FVector(EndNormal.X, EndNormal.Y, 0.0f).GetSafeNormal();
		if (FlatStartNormal.IsNearlyZero())
		{
			FlatStartNormal = FVector::ForwardVector;
		}
		if (FlatEndNormal.IsNearlyZero())
		{
			FlatEndNormal = -FVector::ForwardVector;
		}
		AutoRouteSplineWithNormals(StartPos, FlatStartNormal, EndPos, FlatEndNormal);
	}
	else
	{
		// Auto: Use the engine's native routing logic (private in AFGConveyorBeltHologram).
		// Access is granted via AccessTransformers Friend.
		AutoRouteSpline(StartPos, StartNormal, EndPos, EndNormal);
	}
	
	// Ensure spline component is updated from mSplineData.
	AFGSplineHologram::UpdateSplineComponent();
	
	const int32 NewSplinePoints = mSplineData.Num();
	const float NewSplineLength = mSplineComponent->GetSplineLength();
	const float ExpectedDistance = FVector::Distance(StartPos, EndPos);
	
	// NOTE: Engine routing can legitimately return a 2-point straight spline for simple cases.
	// Treat routing as failed only when the result is clearly a placeholder/invalid (less than 50cm).
	if (NewSplinePoints < 2 || NewSplineLength < 50.0f)
	{
		UE_LOG(LogSmartHologram, Verbose,
			TEXT("🔍 TryUseBuildModeRouting FAILED: Stub spline after AutoRouteSpline on %s | Points=%d Len=%.1f Expected=%.1f | Start=%s StartN=%s End=%s EndN=%s"),
			*GetName(),
			NewSplinePoints,
			NewSplineLength,
			ExpectedDistance,
			*StartPos.ToString(),
			*StartNormal.ToString(),
			*EndPos.ToString(),
			*EndNormal.ToString());
		return false;
	}
	
	// Log at normal level only for the low-point-count cases we historically misclassified.
	// Keep the typical success path quiet to avoid log spam.
	if (NewSplinePoints <= 3)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("🔍 TryUseBuildModeRouting OK: Engine AutoRouteSpline produced %d points (Len=%.1f Expected=%.1f) on %s"),
			NewSplinePoints,
			NewSplineLength,
			ExpectedDistance,
			*GetName());
	}
	return true;
}

void ASFConveyorBeltHologram::SetSplineDataAndUpdate(const TArray<FSplinePointData>& InSplineData)
{
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 SetSplineDataAndUpdate: Setting %d spline points on %s"), InSplineData.Num(), *GetName());
    
    // Log input data
    for (int32 i = 0; i < InSplineData.Num(); i++)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 SetSplineDataAndUpdate:   Input[%d] Location=%s"), 
            i, *InSplineData[i].Location.ToString());
    }
    
    // Copy spline data
    mSplineData = InSplineData;
    
    // CRITICAL: Store backup in registry so ConfigureActor can restore if vanilla resets it
    // This happens when AddChild() or other vanilla code resets mSplineData before Construct()
    FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
    if (!HoloData)
    {
        HoloData = USFHologramDataRegistry::AttachData(this);
    }
    if (HoloData)
    {
        HoloData->bHasBackupSplineData = true;
        HoloData->BackupSplineData = InSplineData;
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 SetSplineDataAndUpdate: Stored %d points in backup registry"), InSplineData.Num());
    }
    
    // Verify copy
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 SetSplineDataAndUpdate: After copy, mSplineData has %d points"), mSplineData.Num());
    
    // Update the spline component from the data
    // CRITICAL: Must call AFGSplineHologram explicitly (same pattern as pipe hologram)
    if (mSplineComponent)
    {
        AFGSplineHologram::UpdateSplineComponent();
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 SetSplineDataAndUpdate: UpdateSplineComponent called, spline length=%.1f cm"), 
            mSplineComponent->GetSplineLength());
    }
    else
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 SetSplineDataAndUpdate: No spline component!"));
    }
}

void ASFConveyorBeltHologram::StartContinuousPositionCorrection(UFGFactoryConnectionComponent* Output, UFGFactoryConnectionComponent* Input)
{
    // Cache connectors for use in timer callback
    CachedOutputConnector = Output;
    CachedInputConnector = Input;
    
    if (!Output || !Input)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   ❌ CONTINUOUS CORRECTION: Invalid connectors, cannot start"));
        return;
    }
    
    // Stop any existing timer
    StopContinuousPositionCorrection();
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   🔁 CONTINUOUS CORRECTION: Starting timer to rebuild belt every 0.033s (30fps)"));
    
    // Set up recurring timer (every ~33ms = 30fps)
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().SetTimer(
            ContinuousPositionCorrectionTimer,
            [this]()
            {
                // Validate we still have valid connectors and actor
                if (!CachedOutputConnector.IsValid() || !CachedInputConnector.IsValid())
                {
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   ⚠️ CONTINUOUS CORRECTION: Connectors invalid, stopping timer"));
                    StopContinuousPositionCorrection();
                    return;
                }
                
                // Check if actor is at origin
                FVector ActorLoc = GetActorLocation();
                if (ActorLoc.IsNearlyZero(1.0f))
                {
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   🔁 CONTINUOUS CORRECTION: Actor at origin, rebuilding belt"));
                    
                    // Rebuild spline and trigger mesh generation
                    SetupBeltSpline(CachedOutputConnector.Get(), CachedInputConnector.Get());
                    TriggerMeshGeneration();
                    
                    // Force visibility
                    SetActorHiddenInGame(false);
                }
            },
            0.033f,  // Every 33ms (30fps)
            true     // Loop
        );
    }
}

void ASFConveyorBeltHologram::StopContinuousPositionCorrection()
{
    UWorld* World = GetWorld();
    if (World && ContinuousPositionCorrectionTimer.IsValid())
    {
        World->GetTimerManager().ClearTimer(ContinuousPositionCorrectionTimer);
        ContinuousPositionCorrectionTimer.Invalidate();
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   🛑 CONTINUOUS CORRECTION: Timer stopped"));
    }
    
    CachedOutputConnector.Reset();
    CachedInputConnector.Reset();
}

void ASFConveyorBeltHologram::ForceApplyHologramMaterial()
{
    EHologramMaterialState CurrentState = GetHologramMaterialState();
    
    // Call our overridden SetPlacementMaterialState which properly applies
    // hologram materials to dynamically created spline mesh components.
    SetPlacementMaterialState(CurrentState);
    
    // DEBUG: Log what materials are actually on the spline meshes
    TArray<USplineMeshComponent*> SplineMeshes;
    GetComponents<USplineMeshComponent>(SplineMeshes);
    
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎨 ForceApplyHologramMaterial: Applied material state %d, found %d spline meshes"),
        (int32)CurrentState, SplineMeshes.Num());
    
    for (int32 i = 0; i < SplineMeshes.Num(); i++)
    {
        USplineMeshComponent* SplineMesh = SplineMeshes[i];
        if (SplineMesh)
        {
            UMaterialInterface* Mat = SplineMesh->GetMaterial(0);
            UStaticMesh* Mesh = SplineMesh->GetStaticMesh();
            
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   [%d] Mesh=%s, Material=%s, Visible=%d, Hidden=%d, RenderCustomDepth=%d"),
                i,
                Mesh ? *Mesh->GetName() : TEXT("NULL"),
                Mat ? *Mat->GetName() : TEXT("NULL"),
                SplineMesh->IsVisible(),
                SplineMesh->bHiddenInGame,
                SplineMesh->bRenderCustomDepth);
        }
    }
}

