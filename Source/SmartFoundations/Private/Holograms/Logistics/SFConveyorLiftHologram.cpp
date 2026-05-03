#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "Components/StaticMeshComponent.h"
#include "Core/Helpers/SFExtendChainHelper.h"
#include "Data/SFHologramDataRegistry.h"
#include "Features/Extend/SFExtendService.h"
#include "Services/SFHudService.h"
#include "Subsystem/SFSubsystem.h"

ASFConveyorLiftHologram::ASFConveyorLiftHologram()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void ASFConveyorLiftHologram::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 LIFT HOLOGRAM BeginPlay: %s"), *GetName());
    
    // Log mesh components for debugging
    TArray<UStaticMeshComponent*> MeshComps;
    GetComponents<UStaticMeshComponent>(MeshComps);
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Found %d StaticMeshComponents"), MeshComps.Num());
}

void ASFConveyorLiftHologram::Destroyed()
{
    // Clear lift height from HUD when hologram is destroyed
    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
    {
        if (USFHudService* HudService = SmartSubsystem->GetHudService())
        {
            HudService->ClearLiftHeight();
        }
    }
    
    Super::Destroyed();
}

void ASFConveyorLiftHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    Super::SetHologramLocationAndRotation(hitResult);
    
    // Skip HUD updates for child holograms (EXTEND children)
    if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
    {
        return;
    }
    
    // Get lift height from mTopTransform (relative Z position)
    FTransform TopTransform = GetTopTransform();
    float CurrentHeight = TopTransform.GetTranslation().Z;
    
    // Only update HUD if height changed (avoid spamming updates)
    if (FMath::Abs(CurrentHeight - CachedLiftHeight) > 1.0f)
    {
        CachedLiftHeight = CurrentHeight;
        
        // Calculate world height (top of lift relative to world origin)
        float WorldHeight = CurrentHeight + GetActorLocation().Z;
        
        // Update HUD
        if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
        {
            if (USFHudService* HudService = SmartSubsystem->GetHudService())
            {
                HudService->UpdateLiftHeight(CurrentHeight, WorldHeight);
            }
        }
    }
}

void ASFConveyorLiftHologram::CheckValidPlacement()
{
    // For child lift previews: Skip validation and force valid state
    // Lift children should never block parent placement
    // Check multiple indicators since GetParentHologram() may not be set during spawn
    bool bShouldSkipValidation = GetParentHologram() != nullptr;
    
    // Also check our custom data registry
    if (!bShouldSkipValidation)
    {
        if (FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this))
        {
            // Skip if marked as child OR if validation was explicitly disabled
            bShouldSkipValidation = HoloData->bIsChildHologram || 
                                    HoloData->ParentHologram != nullptr ||
                                    !HoloData->bNeedToCheckPlacement;
        }
    }
    
    // Also check for SF_ExtendChild tag
    if (!bShouldSkipValidation)
    {
        bShouldSkipValidation = Tags.Contains(FName(TEXT("SF_ExtendChild")));
    }
    
    if (bShouldSkipValidation)
    {
        // Force valid placement state - don't call Super which would add disqualifiers
        SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
        return;
    }
    
    // For non-child lifts: Use normal validation
    Super::CheckValidPlacement();
}

void ASFConveyorLiftHologram::SetSnappedConnections(UFGFactoryConnectionComponent* Connection0, UFGFactoryConnectionComponent* Connection1)
{
    // Use reflection to access the private mSnappedConnectionComponents array
    // This tells the vanilla system that the lift is already connected, enabling proper chain creation
    
    FProperty* SnappedProp = AFGConveyorLiftHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
    if (SnappedProp)
    {
        // mSnappedConnectionComponents is a C-style array of 2 UFGFactoryConnectionComponent*
        void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(this);
        UFGFactoryConnectionComponent** SnappedArray = static_cast<UFGFactoryConnectionComponent**>(PropAddr);
        
        if (SnappedArray)
        {
            SnappedArray[0] = Connection0;
            SnappedArray[1] = Connection1;
            
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Lift: Set snapped connections on %s: [0]=%s, [1]=%s"),
                *GetName(),
                Connection0 ? *Connection0->GetName() : TEXT("nullptr"),
                Connection1 ? *Connection1->GetName() : TEXT("nullptr"));
        }
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND Lift: Failed to find mSnappedConnectionComponents property on %s"), *GetName());
    }
}

AActor* ASFConveyorLiftHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    // Check if this is an EXTEND child hologram
    if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND LIFT CONSTRUCT: %s entering EXTEND path (JsonCloneId will be set by spawn)"),
            *GetName());
        
        // ============================================================
        // SNAPPED CONNECTIONS: Update to point to BUILT buildables
        // ============================================================
        // Use the shared helper for chain connection resolution.
        // This implements the "reverse build order" strategy where conveyors
        // are built from HIGHEST index to LOWEST.

        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        USFExtendService* ExtendService = nullptr;

        if (FSFExtendChainHelper::IsExtendChainMember(HoloData))
        {
            if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
            {
                ExtendService = SmartSubsystem->GetExtendService();
                if (ExtendService)
                {
                    auto Targets = FSFExtendChainHelper::ResolveChainConnections(HoloData, ExtendService, TEXT("Lift"));
                    if (Targets.bHasValidTargets())
                    {
                        SetSnappedConnections(Targets.Conn0Target, Targets.Conn1Target);
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND Chain: Updated %s snapped connections → Conn0=%s, Conn1=%s"),
                            *GetName(),
                            Targets.Conn0Target ? *FString::Printf(TEXT("%s on %s"), *Targets.Conn0Target->GetName(), *Targets.Conn0Target->GetOwner()->GetName()) : TEXT("nullptr"),
                            Targets.Conn1Target ? *FString::Printf(TEXT("%s on %s"), *Targets.Conn1Target->GetName(), *Targets.Conn1Target->GetOwner()->GetName()) : TEXT("nullptr"));
                    }
                }
            }
        }

        // Build the lift via vanilla mechanism
        AActor* BuiltActor = Super::Construct(out_children, constructionID);

        if (BuiltActor)
        {
            // Register hologram → buildable mapping for post-build wiring
            if (!HoloData)
            {
                HoloData = USFHologramDataRegistry::GetData(this);
            }
            if (HoloData)
            {
                HoloData->bWasBuilt = true;
                HoloData->CreatedActor = Cast<AFGBuildable>(BuiltActor);

                // Register with ExtendService for post-build wiring using shared helper
                if (AFGBuildableConveyorBase* ConveyorBase = Cast<AFGBuildableConveyorBase>(BuiltActor))
                {
                    if (!ExtendService && FSFExtendChainHelper::IsExtendChainMember(HoloData))
                    {
                        if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
                        {
                            ExtendService = SmartSubsystem->GetExtendService();
                        }
                    }
                    FSFExtendChainHelper::RegisterBuiltConveyor(HoloData, ConveyorBase, ExtendService);
                }

                // Register with ExtendService for JSON-based post-build wiring
                if (!HoloData->JsonCloneId.IsEmpty())
                {
                    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
                    {
                        if (USFExtendService* LocalExtendService = SmartSubsystem->GetExtendService())
                        {
                            LocalExtendService->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
                        }
                    }
                }
            }

            // Ensure the lift is properly finalized (matches belt child pattern)
            if (AFGBuildable* Buildable = Cast<AFGBuildable>(BuiltActor))
            {
                Buildable->OnBuildEffectFinished();
            }

            // Log the mapping for debugging
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: ✅ Lift hologram %s → Buildable %s (ID: %u, ChainId: %d)"),
                *GetName(), *BuiltActor->GetName(), BuiltActor->GetUniqueID(),
                HoloData ? HoloData->ExtendChainId : -1);

            // Log lift connection info
            if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(BuiltActor))
            {
                UFGFactoryConnectionComponent* Conn0 = Lift->GetConnection0();
                UFGFactoryConnectionComponent* Conn1 = Lift->GetConnection1();
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND:   Conn0=%s @ %s, Conn1=%s @ %s, Height=%.1f"),
                    Conn0 ? *Conn0->GetName() : TEXT("null"),
                    Conn0 ? *Conn0->GetComponentLocation().ToString() : TEXT("N/A"),
                    Conn1 ? *Conn1->GetName() : TEXT("null"),
                    Conn1 ? *Conn1->GetComponentLocation().ToString() : TEXT("N/A"),
                    Lift->GetHeight());
            }
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND: ❌ Lift Construct returned nullptr!"));
        }

        return BuiltActor;
    }
    
    // For non-EXTEND lifts: Build normally
    return Super::Construct(out_children, constructionID);
}

void ASFConveyorLiftHologram::SetTopTransform(const FTransform& InTopTransform)
{
    // Access mTopTransform via reflection (it's protected in base class)
    if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
    {
        FTransform* TopTransformPtr = TopTransformProp->ContainerPtrToValuePtr<FTransform>(this);
        if (TopTransformPtr)
        {
            *TopTransformPtr = InTopTransform;
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 LIFT: Set mTopTransform to %s"), 
                *InTopTransform.GetLocation().ToString());
        }
    }
}

FTransform ASFConveyorLiftHologram::GetTopTransform() const
{
    // Access mTopTransform via reflection
    if (FStructProperty* TopTransformProp = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform")))
    {
        const FTransform* TopTransformPtr = TopTransformProp->ContainerPtrToValuePtr<FTransform>(this);
        if (TopTransformPtr)
        {
            return *TopTransformPtr;
        }
    }
    return FTransform::Identity;
}

void ASFConveyorLiftHologram::ConfigureComponents(AFGBuildable* inBuildable) const
{
    // Call parent first
    Super::ConfigureComponents(inBuildable);
    
    AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(inBuildable);
    if (!Conveyor)
    {
        return;
    }
    
    // Only do this for EXTEND children with JSON connection targets
    const FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
    if (!HoloData || HoloData->JsonCloneId.IsEmpty())
    {
        return;
    }
    
    // Get ExtendService to look up already-built actors
    USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld());
    if (!SmartSubsystem)
    {
        return;
    }
    USFExtendService* ExtendService = SmartSubsystem->GetExtendService();
    if (!ExtendService)
    {
        return;
    }
    
    UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0();
    UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1();
    
    // Log connection state BEFORE we try anything
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: %s (CloneId=%s) checking targets: Conn0→%s.%s, Conn1→%s.%s"),
        *Conveyor->GetName(),
        *HoloData->JsonCloneId,
        *HoloData->Conn0TargetCloneId, *HoloData->Conn0TargetConnectorName.ToString(),
        *HoloData->Conn1TargetCloneId, *HoloData->Conn1TargetConnectorName.ToString());
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: %s - Conn0 already connected: %s, Conn1 already connected: %s"),
        *Conveyor->GetName(),
        Conn0 && Conn0->IsConnected() ? TEXT("YES") : TEXT("NO"),
        Conn1 && Conn1->IsConnected() ? TEXT("YES") : TEXT("NO"));
    
    bool bMadeConnection = false;
    
    // === CONN0: Try to connect to target ===
    if (!HoloData->Conn0TargetCloneId.IsEmpty() && Conn0 && !Conn0->IsConnected())
    {
        // Look up target - either by clone ID or by source actor name (for lane segments)
        AFGBuildable* TargetBuildable = nullptr;
        if (HoloData->Conn0TargetCloneId.StartsWith(TEXT("source:")))
        {
            FString SourceActorName = HoloData->Conn0TargetCloneId.Mid(7);
            TargetBuildable = ExtendService->GetSourceBuildableByName(SourceActorName);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
                *SourceActorName, TargetBuildable ? *TargetBuildable->GetName() : TEXT("NOT FOUND"));
        }
        else
        {
            TargetBuildable = ExtendService->GetBuiltActorByCloneId(HoloData->Conn0TargetCloneId);
        }
        if (TargetBuildable)
        {
            // Find the target connector by name
            TArray<UFGFactoryConnectionComponent*> TargetConns;
            TargetBuildable->GetComponents<UFGFactoryConnectionComponent>(TargetConns);
            
            for (UFGFactoryConnectionComponent* TargetConn : TargetConns)
            {
                if (TargetConn && TargetConn->GetFName() == HoloData->Conn0TargetConnectorName)
                {
                    if (!TargetConn->IsConnected())
                    {
                        Conn0->SetConnection(TargetConn);
                        bMadeConnection = true;
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: ✅ Connected %s.Conn0 → %s.%s"),
                            *Conveyor->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName());
                    }
                    break;
                }
            }
        }
        else
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: Conn0 target buildable '%s' not yet built"),
                *HoloData->Conn0TargetCloneId);
        }
    }
    
    // === CONN1: Try to connect to target ===
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: %s - Conn1 check: TargetCloneId='%s', Conn1 valid=%s, Conn1 connected=%s"),
        *Conveyor->GetName(),
        *HoloData->Conn1TargetCloneId,
        Conn1 ? TEXT("YES") : TEXT("NO"),
        (Conn1 && Conn1->IsConnected()) ? TEXT("YES") : TEXT("NO"));
    
    if (!HoloData->Conn1TargetCloneId.IsEmpty() && Conn1 && !Conn1->IsConnected())
    {
        // Look up target - either by clone ID or by source actor name (for lane segments)
        AFGBuildable* TargetBuildable = nullptr;
        if (HoloData->Conn1TargetCloneId.StartsWith(TEXT("source:")))
        {
            FString SourceActorName = HoloData->Conn1TargetCloneId.Mid(7);
            TargetBuildable = ExtendService->GetSourceBuildableByName(SourceActorName);
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
                *SourceActorName, TargetBuildable ? *TargetBuildable->GetName() : TEXT("NOT FOUND"));
        }
        else
        {
            TargetBuildable = ExtendService->GetBuiltActorByCloneId(HoloData->Conn1TargetCloneId);
        }
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: %s - Conn1 target lookup '%s' -> %s"),
            *Conveyor->GetName(),
            *HoloData->Conn1TargetCloneId,
            TargetBuildable ? *TargetBuildable->GetName() : TEXT("NULL"));
        if (TargetBuildable)
        {
            // Find the target connector by name
            TArray<UFGFactoryConnectionComponent*> TargetConns;
            TargetBuildable->GetComponents<UFGFactoryConnectionComponent>(TargetConns);
            
            for (UFGFactoryConnectionComponent* TargetConn : TargetConns)
            {
                if (TargetConn && TargetConn->GetFName() == HoloData->Conn1TargetConnectorName)
                {
                    if (!TargetConn->IsConnected())
                    {
                        Conn1->SetConnection(TargetConn);
                        bMadeConnection = true;
                        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: ✅ Connected %s.Conn1 → %s.%s"),
                            *Conveyor->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName());
                    }
                    break;
                }
            }
        }
        else
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: Conn1 target buildable '%s' not yet built"),
                *HoloData->Conn1TargetCloneId);
        }
    }
    
    // === CHAIN MANAGEMENT ===
    // Issue #260: EXTEND lifts must NOT call AddConveyor here.
    // Vanilla's BeginPlay handles initial bucket registration.
    // Our CreateChainActors (Archengius pattern) handles chain rebuild
    // via RemoveConveyorChainActor after all wiring is complete.
    // Calling AddConveyor here causes double-add → bucket corruption → crash.
    // See RESEARCH_MassUpgrade_ChainActorSafety.md (Archengius insight).
    bool bIsExtendLift = HoloData != nullptr &&
        (!HoloData->Conn0TargetCloneId.IsEmpty() || !HoloData->Conn1TargetCloneId.IsEmpty());
    
    if (bIsExtendLift)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Log, TEXT("⛓️ LIFT ConfigureComponents: %s - EXTEND lift (skipping AddConveyor — chain rebuild handles registration)"),
            *Conveyor->GetName());
    }
    else if (bMadeConnection)
    {
        AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(GetWorld());
        if (BuildableSubsystem)
        {
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: %s - adding to subsystem"),
                *Conveyor->GetName());
            
            BuildableSubsystem->AddConveyor(Conveyor);
            
            AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: %s - initial chain: %s (%d segments)"),
                *Conveyor->GetName(),
                ChainActor ? *ChainActor->GetName() : TEXT("NULL"),
                ChainActor ? ChainActor->GetNumChainSegments() : 0);
        }
    }
    else
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ LIFT ConfigureComponents: %s - no connections made (targets not yet built)"),
            *Conveyor->GetName());
    }
}

bool ASFConveyorLiftHologram::SetupUpgradeTarget(AFGBuildableConveyorLift* InUpgradeTarget)
{
    if (!InUpgradeTarget)
    {
        return false;
    }
    
    // Create a fake hit result pointing at the target lift
    FHitResult HitResult;
    HitResult.bBlockingHit = true;
    HitResult.Location = InUpgradeTarget->GetActorLocation();
    HitResult.ImpactPoint = HitResult.Location;
    HitResult.ImpactNormal = FVector::UpVector;
    HitResult.Normal = FVector::UpVector;
    HitResult.HitObjectHandle = FActorInstanceHandle(InUpgradeTarget);
    
    // Call TryUpgrade which will set mUpgradedConveyorLift internally
    return TryUpgrade(HitResult);
}

void ASFConveyorLiftHologram::ForceApplyHologramMaterial()
{
    // Get the current material state
    EHologramMaterialState CurrentState = GetHologramMaterialState();
    
    // Apply to all static mesh components
    TArray<UStaticMeshComponent*> MeshComps;
    GetComponents<UStaticMeshComponent>(MeshComps);
    
    UMaterialInterface* Material = nullptr;
    switch (CurrentState)
    {
        case EHologramMaterialState::HMS_OK:
        case EHologramMaterialState::HMS_WARNING:
            Material = mValidPlacementMaterial;
            break;
        case EHologramMaterialState::HMS_ERROR:
            Material = mInvalidPlacementMaterial;
            break;
    }
    
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎨 LIFT ForceApplyHologramMaterial: State=%d, MeshComponents=%d"), 
        (int32)CurrentState, MeshComps.Num());
    
    if (Material)
    {
        for (UStaticMeshComponent* MeshComp : MeshComps)
        {
            if (MeshComp)
            {
                const int32 SlotCount = MeshComp->GetNumMaterials();
                for (int32 Slot = 0; Slot < SlotCount; ++Slot)
                {
                    MeshComp->SetMaterial(Slot, Material);
                }
                MeshComp->MarkRenderStateDirty();
            }
        }
    }
}
