// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

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
#include "FGConstructDisqualifier.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"
#include "FGConstructDisqualifier.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "EngineUtils.h"  // For TActorIterator
#include "Core/Helpers/SFExtendChainHelper.h"
#if !UE_BUILD_SHIPPING
#include "HAL/PlatformStackWalk.h"
#endif

ASFConveyorBeltHologram::ASFConveyorBeltHologram()
{
    // Ensure ticking is enabled so we can correct world position over time
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    // Run late in the frame so we win any tug-of-war over transforms
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ASFConveyorBeltHologram::BeginPlay()
{
    Super::BeginPlay();
    
    // CRITICAL: Load hologram materials if not set
    // When spawning ASFConveyorBeltHologram directly (not via recipe), the materials may not be initialized
    // Load them from the standard hologram material paths used by vanilla
    if (!mValidPlacementMaterial)
    {
        // Standard valid placement material used by all holograms
        static const TCHAR* ValidMaterialPath = TEXT("/Game/FactoryGame/Buildable/-Shared/Material/HologramMaterial.HologramMaterial");
        mValidPlacementMaterial = LoadObject<UMaterialInstance>(nullptr, ValidMaterialPath);
        if (mValidPlacementMaterial)
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("🎨 Loaded mValidPlacementMaterial: %s"), *mValidPlacementMaterial->GetName());
        }
        else
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("🎨 Failed to load mValidPlacementMaterial from %s"), ValidMaterialPath);
        }
    }
    
    if (!mInvalidPlacementMaterial)
    {
        // Standard invalid placement material used by all holograms
        static const TCHAR* InvalidMaterialPath = TEXT("/Game/FactoryGame/Buildable/-Shared/Material/HologramMaterial_Invalid.HologramMaterial_Invalid");
        mInvalidPlacementMaterial = LoadObject<UMaterialInstance>(nullptr, InvalidMaterialPath);
        if (mInvalidPlacementMaterial)
        {
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎨 Loaded mInvalidPlacementMaterial: %s"), *mInvalidPlacementMaterial->GetName());
        }
        else
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("🎨 Failed to load mInvalidPlacementMaterial from %s"), InvalidMaterialPath);
        }
    }
    
    // Log NetMode to determine if we're spawning client-side or server-side
    ENetMode NetMode = GetNetMode();
    const TCHAR* NetModeStr = TEXT("UNKNOWN");
    switch (NetMode)
    {
        case NM_Standalone: NetModeStr = TEXT("Standalone"); break;
        case NM_DedicatedServer: NetModeStr = TEXT("DedicatedServer"); break;
        case NM_ListenServer: NetModeStr = TEXT("ListenServer"); break;
        case NM_Client: NetModeStr = TEXT("Client"); break;
    }
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🌐 BELT HOLOGRAM SPAWNED: %s | NetMode=%s (%d) | World=%s"), 
        *GetName(), NetModeStr, (int32)NetMode, GetWorld() ? *GetWorld()->GetName() : TEXT("NULL"));
    
    // Log role information
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Role=%d (3=Authority, 2=AutonomousProxy, 1=SimulatedProxy, 0=None) | RemoteRole=%d"), 
        (int32)GetLocalRole(), (int32)GetRemoteRole());
}

void ASFConveyorBeltHologram::Tick(float DeltaSeconds)
{
    // Log once to confirm Tick is actually running
    static bool bFirstTickLogged = false;
    if (!bFirstTickLogged)
    {
        bFirstTickLogged = true;
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔄 TICK IS RUNNING on %s | CanEverTick=%d | TickEnabled=%d"), 
            *GetName(), PrimaryActorTick.bCanEverTick, IsActorTickEnabled());
    }

    Super::Tick(DeltaSeconds);

    if (mSplineComponent && mSplineComponent->GetNumberOfSplinePoints() > 0)
    {
        const FVector ActorLoc = GetActorLocation();

        if (!PreviousLocationSample.IsNearlyZero(1.0f) && ActorLoc.IsNearlyZero(1.0f))
        {
            UE_LOG(LogSmartHologram, VeryVerbose,
                TEXT("🧪 BELT SNAP DETECTED: %s jumped from %s to origin in one frame (Parent=%s)"),
                *GetName(), *PreviousLocationSample.ToString(), *GetNameSafe(GetParentHologram()));
// Stack trace removed - UE5 compatibility
        }

        if (ActorLoc.IsNearlyZero(1.0f))
        {
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔄 TICK CORRECTION: Actor at origin, moving to spline start"));
            
            const FVector SplineStart = mSplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
            if (!SplineStart.IsNearlyZero(1.0f))
            {
                if (USceneComponent* RootComp = GetRootComponent())
                {
                    RootComp->SetWorldLocation(SplineStart, false, nullptr, ETeleportType::TeleportPhysics);
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   ✅ Root moved to %s"), *SplineStart.ToString());
                }
                else
                {
                    SetActorLocation(SplineStart);
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   ✅ Actor moved to %s"), *SplineStart.ToString());
                }
                
                // Keep it visible after correction
                SetActorHiddenInGame(false);
                if (USceneComponent* RootComp2 = GetRootComponent())
                {
                    RootComp2->MarkRenderStateDirty();
                }
            }
        }
        
        PreviousLocationSample = ActorLoc;

        // DEBUG VISUALIZATION: Draw a bright line along the belt path so we can see if ANYTHING renders
        if (mSplineComponent->GetNumberOfSplinePoints() >= 2)
        {
            const FVector Start = mSplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World);
            const FVector End = mSplineComponent->GetLocationAtSplinePoint(mSplineComponent->GetNumberOfSplinePoints() - 1, ESplineCoordinateSpace::World);
            
            // Draw thick bright cyan line for 0.1 seconds (refreshed every tick)
            // This is independent of hologram/mesh rendering - if we see this, the world/client is correct
            if (UWorld* World = GetWorld())
            {
                DrawDebugLine(World, Start, End, FColor::Cyan, false, 0.1f, 0, 10.0f);
                
                // Also draw boxes at each endpoint
                DrawDebugBox(World, Start, FVector(50, 50, 50), FColor::Green, false, 0.1f, 0, 5.0f);
                DrawDebugBox(World, End, FVector(50, 50, 50), FColor::Red, false, 0.1f, 0, 5.0f);
            }
        }
    }
}

void ASFConveyorBeltHologram::CheckValidPlacement()
{
    if (GetParentHologram() && Tags.Contains(FName(TEXT("SF_BeltAutoConnectChild"))))
    {
        Super::CheckValidPlacement();

        TArray<TSubclassOf<class UFGConstructDisqualifier>> Disqualifiers;
        GetConstructDisqualifiers(Disqualifiers);
        
        // Filter out timing-related disqualifiers that are artifacts of our preview system
        // FGCDInitializing appears because we check placement before hologram fully initializes
        int32 RealDisqualifierCount = 0;
        FString DisqualifierNames;
        bLastRejectTooSteep = false;
        bLastRejectInvalidShape = false;
        for (const TSubclassOf<UFGConstructDisqualifier>& Disqualifier : Disqualifiers)
        {
            if (Disqualifier)
            {
                FString Name = Disqualifier->GetName();
                DisqualifierNames += Name + TEXT(", ");

                // Skip only timing artifacts - NOT geometry issues
                // FGCDInitializing: appears because we check placement before hologram fully initializes
                // FGCDEncroachingSoftClearance: preview may temporarily overlap other objects during positioning
                //
                // DO NOT skip these - they indicate real problems that should reject the connection:
                // FGCDConveyorInvalidShape: belt routing is invalid (crazy curves, S-bends, etc.)
                // FGCDConveyorTooSteep: belt angle exceeds vanilla limits
                if (Name == TEXT("FGCDInitializing") ||
                    Name == TEXT("FGCDEncroachingSoftClearance"))
                {
                    continue;
                }
                // [#466] Record WHICH vanilla verdict fired so auto-connect can report the real
                // reason in the HUD skip summary and retry a different pairing.
                if (Name.Contains(TEXT("TooSteep")))
                {
                    bLastRejectTooSteep = true;
                }
                else if (Name.Contains(TEXT("InvalidShape")))
                {
                    bLastRejectInvalidShape = true;
                }
                RealDisqualifierCount++;
            }
        }

        bLastVanillaPlacementValid = (RealDisqualifierCount == 0);

        // Log detailed disqualifier info for debugging
        if (Disqualifiers.Num() > 0)
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("🚫 VANILLA DISQUALIFIERS [%s]: %s (Real: %d)"),
                *GetName(), *DisqualifierNames, RealDisqualifierCount);
        }

        EHologramMaterialState DesiredState = bLastVanillaPlacementValid
            ? EHologramMaterialState::HMS_OK
            : EHologramMaterialState::HMS_ERROR;

        if (AFGHologram* Parent = GetParentHologram())
        {
            if (Parent->GetHologramMaterialState() == EHologramMaterialState::HMS_ERROR)
            {
                DesiredState = EHologramMaterialState::HMS_ERROR;
            }
        }

        ResetConstructDisqualifiers();
        SetPlacementMaterialState(DesiredState);
        return;
    }

    // For child belt previews: Skip validation since we're just a visual preview
    // Belt children should never block parent placement
    if (GetParentHologram())
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("Belt child preview (has parent) - skipping validation"));
        // Mirror the Extend preview's authoritative state (frame-order independent), so the
        // child turns red on insufficient materials and back to cyan when affordable again.
        // Reading the parent directly can still see HMS_OK before the build gun applies the
        // parent's red this frame. Keep a parent-ERROR fallback for non-Extend parented previews.
        EHologramMaterialState DesiredState = USFExtendService::ResolveChildPreviewMaterialState(this);
        if (DesiredState != EHologramMaterialState::HMS_ERROR
            && GetParentHologram()->GetHologramMaterialState() == EHologramMaterialState::HMS_ERROR)
        {
            DesiredState = EHologramMaterialState::HMS_ERROR;
        }
        // The build gun paints previews from construct disqualifiers, not SetPlacementMaterialState.
        ResetConstructDisqualifiers();
        if (DesiredState == EHologramMaterialState::HMS_ERROR)
        {
            AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
        }
        SetPlacementMaterialState(DesiredState);
        return; // Don't call Super - prevents vanilla placement disqualifiers
    }
    
    // Also check data registry for validation control (used by EXTEND before AddChild is called)
    if (FSFHologramData* Data = USFHologramDataRegistry::GetData(const_cast<ASFConveyorBeltHologram*>(this)))
    {
        if (!Data->bNeedToCheckPlacement)
        {
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("Belt child preview (data registry) - skipping validation"));
            // The build gun paints previews from construct disqualifiers, not SetPlacementMaterialState. A
            // validation-disabled preview belt that is NEVER vanilla-AddChild'd (Smart Walking spawns belts
            // standalone) must CLEAR residual disqualifiers here, else TriggerMeshGeneration's
            // GetHologramMaterialState() recompute reads them and paints the belt red. Mirrors the parented path.
            ResetConstructDisqualifiers();
            SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
            return; // Don't call Super - prevents disqualifiers
        }
    }
    
    // For non-child belts: Use normal validation
    Super::CheckValidPlacement();
}

void ASFConveyorBeltHologram::PostHologramPlacement(const FHitResult& hitResult, bool callForChildren)
{
    // Check tags to determine context
    bool bIsExtendChild = Tags.Contains(FName(TEXT("SF_ExtendChild")));
    bool bIsAutoConnectChild = Tags.Contains(FName(TEXT("SF_BeltAutoConnectChild")));
    bool bIsStackableChild = Tags.Contains(FName(TEXT("SF_StackableChild")));
    
    // EXTEND CONTEXT: Call Super ONCE to establish snapped connections, then skip subsequent calls
    // Vanilla parent hologram calls PostHologramPlacement repeatedly during tick.
    // The first call is needed to wire connections via SetSnappedConnections.
    // Subsequent calls crash because spline mesh data gets garbage collected.
    if (bIsExtendChild)
    {
        if (bPostHologramPlacementCalled)
        {
            // Already called once - skip to prevent crash
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎯 BELT PostHologramPlacement: Extend child %s - skipping (already called)"), *GetName());
            return;
        }
        
        bPostHologramPlacementCalled = true;

        // Lane segments have their route authored entirely by Smart. Calling vanilla
        // post-placement on them can overwrite the lane route, but ordinary cloned
        // Extend belts need the one vanilla call so snapped connections are committed
        // before ConfigureComponents and chain registration run.
        if (Tags.Contains(FName(TEXT("SF_LaneSegment"))))
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("BELT PostHologramPlacement: Lane segment %s - skipping vanilla post-placement (Smart-managed route)"), *GetName());
            return;
        }
        
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Log, TEXT("🎯 BELT PostHologramPlacement: Extend child %s - calling Super once for connection wiring"), *GetName());
        Super::PostHologramPlacement(hitResult, callForChildren);

        // Module replay (#427): with a RAW VANILLA parent the per-tick PostHologramPlacement
        // cascade consumes this once-gate DURING PREVIEW, handing vanilla the PARENT's hit
        // result - vanilla resets mSplineData to the unstarted 2-zero-point state and the
        // JSON-authored route vanishes (belts invisible; pipes skip vanilla here entirely and
        // survive). The normal Extend preview never cascades (Smart parent blocks it), so the
        // one call there happens at construct with sane state. Keep the vanilla call (its
        // connection-commit side effects are load-bearing at construct) but restore the
        // Smart-authored route from the SetSplineDataAndUpdate backup if vanilla wiped it.
        if (mSplineComponent && mSplineComponent->GetSplineLength() <= KINDA_SMALL_NUMBER)
        {
            if (FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this))
            {
                if (HoloData->bHasBackupSplineData && HoloData->BackupSplineData.Num() >= 2)
                {
                    mSplineData = HoloData->BackupSplineData;
                    AFGSplineHologram::UpdateSplineComponent();
                    TriggerMeshGeneration();
                    ForceApplyHologramMaterial();
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Log,
                        TEXT("Belt %s spline wiped by vanilla PostHologramPlacement - restored %d points, lengthCm=%.0f"),
                        *GetName(),
                        HoloData->BackupSplineData.Num(),
                        mSplineComponent->GetSplineLength());
                }
            }
        }
        return;
    }

    // AUTO-CONNECT & STACKABLE CHILDREN: Skip vanilla spline update to prevent crash
    // These children have already generated their spline meshes BEFORE AddChild() was called
    // Vanilla's spline update tries to regenerate meshes but the connectors aren't in the right state
    if (bIsAutoConnectChild || bIsStackableChild)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎯 BELT PostHologramPlacement: %s child %s - skipping vanilla spline update (already generated)"), 
            bIsAutoConnectChild ? TEXT("Auto-connect") : TEXT("Stackable"), *GetName());
        // Don't call Super - meshes already exist and are correct
        return;
    }

    // For all other belts: Use normal behavior, but guard against null spline component.
    // When vanilla spawns belt children from a factory hologram (e.g. Constructor copy via middle-click),
    // the belt may not have its spline component initialized yet when PostHologramPlacement first fires.
    // BuildSplineMeshes dereferences mSplineComponent without a null check → EXCEPTION_ACCESS_VIOLATION.
    if (!mSplineComponent)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎯 BELT PostHologramPlacement: %s - skipping (mSplineComponent null, not yet initialized)"), *GetName());
        return;
    }
    Super::PostHologramPlacement(hitResult, callForChildren);
}

void ASFConveyorBeltHologram::SetPlacementMaterialState(EHologramMaterialState materialState)
{
    // Let the base class update stencil/render depth
    Super::SetPlacementMaterialState(materialState);

    // #497 set-once: the sweep below dirties the render proxy of EVERY spline mesh. Extend/Walk call
    // this per frame with an unchanged state, which forced the render thread to rebuild all proxies
    // every frame (the GPU-side lag). Once a state has been swept it holds — skip same-state re-calls.
    // Meshes created later are painted by TriggerMeshGeneration's trailing ApplySplineMeshMaterialState.
    if (bSplineMaterialStateApplied && materialState == LastAppliedSplineMaterialState)
    {
        return;
    }

    ApplySplineMeshMaterialState(materialState);
}

void ASFConveyorBeltHologram::ApplySplineMeshMaterialState(EHologramMaterialState materialState)
{
    TArray<USplineMeshComponent*> MeshComps;
    GetComponents<USplineMeshComponent>(MeshComps);
    const uint8 StencilValue = GetStencilForHologramMaterialState(materialState);

    // No meshes yet: leave the guard unarmed so the next call (or mesh generation) paints them.
    if (MeshComps.Num() == 0)
    {
        return;
    }

    for (USplineMeshComponent* Mesh : MeshComps)
    {
        if (Mesh)
        {
            Mesh->SetRenderCustomDepth(true);
            Mesh->SetCustomDepthStencilValue(StencilValue);
            Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);

            // Force render state update
            Mesh->MarkRenderStateDirty();
            Mesh->SetVisibility(true, true);
            Mesh->SetHiddenInGame(false);
        }
    }

    LastAppliedSplineMaterialState = materialState;
    bSplineMaterialStateApplied = true;

    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎨 BELT SetPlacementMaterialState: State=%d, SplineMeshes=%d, Stencil=%d"),
        (int32)materialState,
        MeshComps.Num(),
        (int32)StencilValue);
}

// ── Stacked-belt run registry (Issue #220) ──────────────────────────────────────
// Flat session list of built stacked-pole belts. The STACK-CHAIN handler connects a freshly
// built belt to its run neighbour(s) by GEOMETRIC COINCIDENCE against this list — NOT by
// StackChainId/StackChainIndex. Rationale, learned the hard way (THESIS §6.10/§6.11/§6.12):
//   - TWeakObjectPtr: a destroyed belt resolves to null and is NEVER dereferenced. (Raw pointers
//     in the Extend registry dangled across builds and caused the reversed-belt CTD via vanilla
//     CanConnectTo → GetOuterBlueprintDesigner.) We also never touch the Extend registry, so its
//     chain-fix finalize can't trip over stacked entries.
//   - Coincidence, not index: StackChainId/Index are placement-relative loop indices, so belts
//     from separate build drags (or abutting run segments) get misaligned keys and miss each
//     other (observed: a physically-shared junction left unwired). Matching connectors by world
//     position is ground truth — direction-agnostic and segment/order-independent.
// Process-global (fine for singleplayer; MP needs a per-world home — THESIS §10). The list only
// holds stacked belts (a small subset); dead entries self-null and are skipped on read. A spatial
// index would beat the linear scan if stacked-belt counts ever get large (future optimization).
static TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> GStackBuiltConveyors;

static void SF_RegisterStackBuilt(AFGBuildableConveyorBase* Belt)
{
    if (Belt)
    {
        GStackBuiltConveyors.AddUnique(Belt);
    }
}

// #341: drain the stack-built belts for in-frame chain registration from the parent pole Construct hook.
void ASFConveyorBeltHologram::DrainStackBuiltConveyors(TArray<AFGBuildableConveyorBase*>& OutBelts)
{
    for (const TWeakObjectPtr<AFGBuildableConveyorBase>& Weak : GStackBuiltConveyors)
    {
        if (AFGBuildableConveyorBase* Belt = Weak.Get())
        {
            OutBelts.Add(Belt);
        }
    }
    GStackBuiltConveyors.Empty();
}

// Connect `Ours` to the coincident (<=tol), free, CanConnectTo-compatible connector on any OTHER
// built stacked belt. Returns true if a connection was made. Safe on live belts (SetConnection is
// topology only — THESIS §2.6) and against dead belts (TWeakObjectPtr::Get()).
static bool SF_WireStackConnectorByCoincidence(UFGFactoryConnectionComponent* Ours, AFGBuildableConveyorBase* Self)
{
    if (!IsValid(Ours) || Ours->IsConnected())
    {
        return false;
    }
    const FVector OursLoc = Ours->GetComponentLocation();
    UFGFactoryConnectionComponent* Best = nullptr;
    float BestSq = FMath::Square(150.0f);  // junction connectors are ~coincident; belts are >= 2m long
    for (const TWeakObjectPtr<AFGBuildableConveyorBase>& Weak : GStackBuiltConveyors)
    {
        AFGBuildableConveyorBase* Other = Weak.Get();
        if (!Other || Other == Self)
        {
            continue;
        }
        UFGFactoryConnectionComponent* OtherConns[2] = { Other->GetConnection0(), Other->GetConnection1() };
        for (UFGFactoryConnectionComponent* Cand : OtherConns)
        {
            if (!IsValid(Cand) || Cand->IsConnected())
            {
                continue;
            }
            const float DSq = FVector::DistSquared(Cand->GetComponentLocation(), OursLoc);
            if (DSq <= BestSq && Ours->CanConnectTo(Cand))
            {
                Best = Cand;
                BestSq = DSq;
            }
        }
    }
    if (Best)
    {
        Ours->SetConnection(Best);
        return Ours->IsConnected();
    }
    return false;
}

AActor* ASFConveyorBeltHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    // CRITICAL DIAGNOSTIC: Log spline data BEFORE Construct
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT Construct: %s - BEFORE Super::Construct, mSplineData has %d points"), 
        *GetName(), mSplineData.Num());
    for (int32 i = 0; i < mSplineData.Num(); i++)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT Construct:   Point[%d] Location=%s"), 
            i, *mSplineData[i].Location.ToString());
    }
    
    // ============================================================
    // STACKABLE CHAIN MEMBER — build fresh, then wire by coincidence (THESIS §6.9–§6.12)
    // ============================================================
    // A run of stacked-pole belts is a chain. Build a REAL belt (so cost aggregates and the build gun
    // never sees a null return), then AFTER Super::Construct connect each of its connectors to the
    // physically-coincident, free, compatible connector on any other built stacked belt
    // (SF_WireStackConnectorByCoincidence). Geometry — not StackChainId/Index — is the basis: the
    // index is placement-relative, so multi-drag/abutting segments miss each other (THESIS §6.12).
    // Order-agnostic: the first belt at a junction registers open; whichever neighbour builds second
    // wires it. Run-ends stay open and snappable. We never touch the Extend registry.
    {
        FSFHologramData* StackHolo = USFHologramDataRegistry::GetData(this);
        if (Tags.Contains(FName(TEXT("SF_StackableChild"))) && StackHolo &&
            StackHolo->StackChainId >= 0 && StackHolo->StackChainIndex >= 0)
        {
            const int32 ChainId = StackHolo->StackChainId;
            const int32 Index = StackHolo->StackChainIndex;

            // Do NOT pre-snap to a specific connector index. A REVERSED belt has Connection0/Connection1
            // swapped relative to position, so an index-based pairing wires non-coincident connectors
            // (peers ended up ~100m apart, consuming the run-end connectors the player needs to snap to).
            // We wire post-construct BY GEOMETRY instead. Clearing the snap also stops vanilla from
            // pre-wiring a wrong pair during ConfigureComponents.
            SetSnappedConnections(nullptr, nullptr);

            AActor* BuiltActor = Super::Construct(out_children, constructionID);

            bool bWired0 = false, bWired1 = false;
            if (AFGBuildableConveyorBase* BuiltConveyor = Cast<AFGBuildableConveyorBase>(BuiltActor))
            {
                // Wire each of our connectors to the coincident free neighbour connector among ALL built
                // stacked belts — by world position, NOT StackChainId/Index. Index is placement-relative,
                // so belts from separate build drags / abutting segments miss each other (THESIS §6.12).
                // Post-Super::Construct our connectors have real positions, so coincidence is ground truth.
                // Whichever belt builds second at a shared junction wires it; run-ends stay open (snappable).
                bWired0 = SF_WireStackConnectorByCoincidence(BuiltConveyor->GetConnection0(), BuiltConveyor);
                bWired1 = SF_WireStackConnectorByCoincidence(BuiltConveyor->GetConnection1(), BuiltConveyor);

                // Add to the flat stacked-belt list (TWeakObjectPtr) for later-built neighbours to find.
                SF_RegisterStackBuilt(BuiltConveyor);
            }

            UE_LOG(LogSmartHologram, Verbose, TEXT("🚧 STACK-CHAIN: %s built [chain=%d idx=%d] wired c0=%s c1=%s"),
                *GetName(), ChainId, Index,
                bWired0 ? TEXT("Y") : TEXT("-"), bWired1 ? TEXT("Y") : TEXT("-"));

            return BuiltActor;
        }
    }

    // Check if this is an EXTEND child hologram
    // Phase 2: EXTEND belt children now BUILD (vanilla handles them as children)
    // Previously returned nullptr to prevent building, but now we want them to build
    // Connections will be made post-construction via SetConnection()
    if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: Belt hologram %s Construct() called - building as child"), 
            *GetName());
        
        // ============================================================
        // SNAPPED CONNECTIONS: Update to point to BUILT buildables
        // ============================================================
        // Use FSFExtendChainHelper to resolve chain connections.
        // This implements the "reverse build order" strategy where conveyors
        // are built from highest to lowest index, allowing each to connect
        // to already-built neighbors.

        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        USFExtendService* ExtendService = nullptr;

        if (FSFExtendChainHelper::IsExtendChainMember(HoloData))
        {
            if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
            {
                ExtendService = SmartSubsystem->GetExtendService();
                if (ExtendService)
                {
                    auto Targets = FSFExtendChainHelper::ResolveChainConnections(HoloData, ExtendService, TEXT("Belt"));
                    if (Targets.bHasValidTargets())
                    {
                        SetSnappedConnections(Targets.Conn0Target, Targets.Conn1Target);
                        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND Chain: Updated %s snapped connections → Conn0=%s, Conn1=%s"),
                            *GetName(),
                            Targets.Conn0Target ? *FString::Printf(TEXT("%s on %s"), *Targets.Conn0Target->GetName(), *Targets.Conn0Target->GetOwner()->GetName()) : TEXT("nullptr"),
                            Targets.Conn1Target ? *FString::Printf(TEXT("%s on %s"), *Targets.Conn1Target->GetName(), *Targets.Conn1Target->GetOwner()->GetName()) : TEXT("nullptr"));
                    }
                }
            }
        }

        // PHASE 2: Build the belt! Vanilla will handle it as a child.
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
                // Register with ExtendService for post-build wiring
                if (FSFExtendChainHelper::IsExtendChainMember(HoloData))
                {
                    if (!ExtendService)
                    {
                        if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
                        {
                            ExtendService = SmartSubsystem->GetExtendService();
                        }
                    }
                    if (ExtendService)
                    {
                        if (AFGBuildableConveyorBase* ConveyorBase = Cast<AFGBuildableConveyorBase>(BuiltActor))
                        {
                            FSFExtendChainHelper::RegisterBuiltConveyor(HoloData, ConveyorBase, ExtendService);
                        }
                    }
                }

                // Register with ExtendService for JSON-based post-build wiring
                if (!HoloData->JsonCloneId.IsEmpty())
                {
                    if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
                    {
                        if (USFExtendService* ExtendSvc = SmartSubsystem->GetExtendService())
                        {
                            ExtendSvc->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
                        }
                    }
                }
            }
            
            // Log the mapping for debugging
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: ✅ Belt hologram %s → Buildable %s (ID: %u, ChainId: %d)"), 
                *GetName(), *BuiltActor->GetName(), BuiltActor->GetUniqueID(),
                HoloData ? HoloData->ExtendChainId : -1);
            
            // Log built belt's connection info AND chain actor
            if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(BuiltActor))
            {
                UFGFactoryConnectionComponent* Conn0 = Belt->GetConnection0();
                UFGFactoryConnectionComponent* Conn1 = Belt->GetConnection1();
                AFGConveyorChainActor* ChainActor = Belt->GetConveyorChainActor();
                
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND:   Conn0=%s @ %s, Conn1=%s @ %s"),
                    Conn0 ? *Conn0->GetName() : TEXT("null"),
                    Conn0 ? *Conn0->GetComponentLocation().ToString() : TEXT("N/A"),
                    Conn1 ? *Conn1->GetName() : TEXT("null"),
                    Conn1 ? *Conn1->GetComponentLocation().ToString() : TEXT("N/A"));
                
                // CRITICAL: Log chain actor to diagnose unified vs separate chains
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ CHAIN ACTOR: Belt %s → ChainActor=%s (ptr=0x%p)"),
                    *Belt->GetName(),
                    ChainActor ? *ChainActor->GetName() : TEXT("NULL"),
                    ChainActor);
            }
            
            // Ensure the belt is properly finalized
            if (AFGBuildable* Buildable = Cast<AFGBuildable>(BuiltActor))
            {
                Buildable->OnBuildEffectFinished();
            }
            
            // MANIFOLD BELT WIRING: TEMPORARILY DISABLED - causing crash in Factory_UpdateRadioactivity
            // TODO: Re-enable after fixing chain actor issues
            // if (HoloData && HoloData->bIsManifoldBelt && HoloData->ManifoldSourceBeltConnector)
            // {
            //     if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
            //     {
            //         if (USFExtendService* ExtendService = SmartSubsystem->GetExtendService())
            //         {
            //             if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(BuiltActor))
            //             {
            //                 ExtendService->WireManifoldBelt(Belt, HoloData->ManifoldSourceBeltConnector, HoloData->ManifoldBeltCloneChainId);
            //             }
            //         }
            //     }
            // }
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Warning, TEXT("🔧 EXTEND: ❌ Belt Construct returned nullptr!"));
        }
        
        return BuiltActor;
    }
    
    // For Auto-Connect belts: Build and then wire connections
    UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 AUTO-CONNECT: Belt hologram %s Construct() called - building belt"),
        *GetName());
    
    // Build the belt
    AActor* BuiltActor = Super::Construct(out_children, constructionID);
    
    // Check if this is a stackable belt - skip wiring in Construct() for stackable belts
    // Stackable belts are named "StackableBelt_N" and use deferred wiring in SFSubsystem
    bool bIsStackableBelt = GetName().Contains(TEXT("StackableBelt_"));
    
    // Post-build wiring for Auto-Connect belts (but NOT stackable belts)
    // The belt is built but not wired - we need to find nearby connectors and wire them
    AFGBuildableConveyorBelt* BuiltBelt = Cast<AFGBuildableConveyorBelt>(BuiltActor);
    if (!bIsStackableBelt && BuiltBelt)
    {
        UFGFactoryConnectionComponent* BeltConn0 = BuiltBelt->GetConnection0();  // Input end
        UFGFactoryConnectionComponent* BeltConn1 = BuiltBelt->GetConnection1();  // Output end
        
        if (BeltConn0 && BeltConn1)
        {
            const float WiringRadius = 50.0f;  // 50cm tolerance for finding nearby connectors
            
            // Find and wire Conn0 (belt input) to nearby OUTPUT connector
            if (!BeltConn0->IsConnected())
            {
                FVector Conn0Loc = BeltConn0->GetComponentLocation();
                UFGFactoryConnectionComponent* BestMatch = nullptr;
                float BestDist = WiringRadius;
                
                // Search all factory connection components in range
                // CRITICAL: Only wire to BUILT actors (AFGBuildable), not holograms!
                for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
                {
                    AFGBuildable* Buildable = *It;
                    if (Buildable == BuiltBelt) continue;
                    
                    TArray<UFGFactoryConnectionComponent*> Connectors;
                    Buildable->GetComponents<UFGFactoryConnectionComponent>(Connectors);
                    
                    for (UFGFactoryConnectionComponent* Conn : Connectors)
                    {
                        if (!Conn || Conn->IsConnected()) continue;
                        // Conn0 is INPUT, so we need an OUTPUT to connect to
                        if (Conn->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT) continue;
                        
                        float Dist = FVector::Dist(Conn0Loc, Conn->GetComponentLocation());
                        if (Dist < BestDist)
                        {
                            BestDist = Dist;
                            BestMatch = Conn;
                        }
                    }
                }
                
                if (BestMatch)
                {
                    BeltConn0->SetConnection(BestMatch);
                    UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 AUTO-CONNECT WIRING: %s.Conn0 → %s.%s (dist=%.1f)"),
                        *BuiltBelt->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
                }
            }
            
            // Find and wire Conn1 (belt output) to nearby INPUT connector
            if (!BeltConn1->IsConnected())
            {
                FVector Conn1Loc = BeltConn1->GetComponentLocation();
                UFGFactoryConnectionComponent* BestMatch = nullptr;
                float BestDist = WiringRadius;
                
                // Search all factory connection components in range
                // CRITICAL: Only wire to BUILT actors (AFGBuildable), not holograms!
                for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
                {
                    AFGBuildable* Buildable = *It;
                    if (Buildable == BuiltBelt) continue;
                    
                    TArray<UFGFactoryConnectionComponent*> Connectors;
                    Buildable->GetComponents<UFGFactoryConnectionComponent>(Connectors);
                    
                    for (UFGFactoryConnectionComponent* Conn : Connectors)
                    {
                        if (!Conn || Conn->IsConnected()) continue;
                        // Conn1 is OUTPUT, so we need an INPUT to connect to
                        if (Conn->GetDirection() != EFactoryConnectionDirection::FCD_INPUT) continue;
                        
                        float Dist = FVector::Dist(Conn1Loc, Conn->GetComponentLocation());
                        if (Dist < BestDist)
                        {
                            BestDist = Dist;
                            BestMatch = Conn;
                        }
                    }
                }
                
                if (BestMatch)
                {
                    BeltConn1->SetConnection(BestMatch);
                    UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 AUTO-CONNECT WIRING: %s.Conn1 → %s.%s (dist=%.1f)"),
                        *BuiltBelt->GetName(), *BestMatch->GetOwner()->GetName(), *BestMatch->GetName(), BestDist);
                }
            }
        }
    }
    
    return BuiltActor;
}

void ASFConveyorBeltHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
    // For EXTEND child belts, skip vanilla's SetHologramLocationAndRotation entirely (mirrors
    // ASFPipelineHologram - the belt was the ONLY spline child without this guard). The Module
    // replay preview (#427) seeds children under the build gun's RAW VANILLA factory hologram;
    // its per-tick propagation reaches us with the PARENT's hit result, and vanilla belt code
    // regenerates mSplineData from that hit - wiping the JSON-authored route back to the
    // unstarted 2-zero-point state (zero-length, invisible belt; pipes in the same preview
    // survived because they skip). The normal Extend preview never hit this because
    // ASFFactoryHologram blocks propagation while Extend is active. Position is managed by the
    // extend service (ChildIntendedPositions / RefreshChildPositions), same as pipes/distributors.
    if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
    {
        return;
    }

    // For Auto-Connect belts (no parent): Normal behavior
    Super::SetHologramLocationAndRotation(hitResult);
}

TArray<FItemAmount> ASFConveyorBeltHologram::GetBaseCost() const
{
	// #497: Preview belt children are commonly spawned without a recipe (mRecipe == null); their cost is
	// length-based and computed in GetCost. Vanilla AFGHologram::GetBaseCost would call
	// UFGRecipe::GetIngredients(nullptr), which logs "FGRecipe::GetIngredients: class was nullpeter"
	// once per child per frame (~89/frame in a conveyor blueprint) — each line a synchronous disk write
	// via the UE log + Sentry breadcrumb, the confirmed source of the Extend stutter/frame-loss.
	// A null recipe has no base cost, so skip vanilla entirely (which returns empty anyway, just noisily).
	if (!GetRecipe())
	{
		return TArray<FItemAmount>();
	}
	return Super::GetBaseCost();
}

TArray<FItemAmount> ASFConveyorBeltHologram::GetCost(bool includeChildren) const
{
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT GetCost() CALLED on %s (includeChildren=%d)"), *GetName(), includeChildren);
	
	// Get base cost from parent. In Satisfactory 1.2 vanilla AFGConveyorBeltHologram::GetCost already
	// returns the length-based belt material cost.
	TArray<FItemAmount> TotalCost = Super::GetCost(includeChildren);

	// #348: Trust the vanilla belt cost when present. This method used to assume Super returned nothing
	// for belts (see the old comment) and ALWAYS added its own length-scaled cost on top - which
	// double-counted the belt, so the build-gun preview charged 2x what dismantling the belt refunds.
	// The manual length calculation below is now a fallback that only runs when vanilla returned no
	// cost (e.g. the spline data was reset before this query, which the restoration logic recovers).
	if (TotalCost.Num() > 0)
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT GetCost: using vanilla cost (%d item types), skipping manual calc"), TotalCost.Num());
		return TotalCost;
	}

	// Calculate belt cost based on spline length (fallback when Super returned no cost)
	if (mSplineComponent)
	{
		float BeltLengthCm = mSplineComponent->GetSplineLength();
		float LengthInMeters = BeltLengthCm / 100.0f;
		
		// If the spline hasn't been initialized yet, try to restore from backup
		if (LengthInMeters <= KINDA_SMALL_NUMBER)
		{
			// DEFENSIVE RESTORATION: Check if we have backup data that should be restored
			// This handles cases where mSplineData was reset by vanilla code after SetSplineDataAndUpdate
			if (FSFHologramData* HoloData = USFHologramDataRegistry::GetData(const_cast<ASFConveyorBeltHologram*>(this)))
			{
				if (HoloData->bHasBackupSplineData && HoloData->BackupSplineData.Num() >= 2)
				{
					UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: Spline length is zero but backup has %d points - RESTORING!"), 
						HoloData->BackupSplineData.Num());
					
					// Restore spline data
					ASFConveyorBeltHologram* MutableThis = const_cast<ASFConveyorBeltHologram*>(this);
					MutableThis->mSplineData = HoloData->BackupSplineData;
					
					// Update spline component
					MutableThis->UpdateSplineComponent();
					
					// Recalculate length after restoration
					BeltLengthCm = mSplineComponent->GetSplineLength();
					LengthInMeters = BeltLengthCm / 100.0f;
					
					UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: After restoration, spline length = %.1f cm"), BeltLengthCm);
					
					// Also trigger mesh generation to make it visible
					MutableThis->TriggerMeshGeneration();
				}
			}
			
			// If still zero after restoration attempt, skip cost calculation
			if (LengthInMeters <= KINDA_SMALL_NUMBER)
			{
				UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: Spline length is zero - skipping belt cost calculation"));
				return TotalCost;
			}
		}
		
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: Spline length = %.1f cm (%.1f m)"), BeltLengthCm, LengthInMeters);
		
		// Get belt class and determine tier
		if (mBuildClass)
		{
			// Extract tier from class name (e.g., Build_ConveyorBeltMk3_C -> tier 3)
			FString ClassName = mBuildClass->GetName();
			int32 BeltTier = 1; // Default Mk1
			
			// Parse tier from class name
			if (ClassName.Contains(TEXT("Mk1"))) BeltTier = 1;
			else if (ClassName.Contains(TEXT("Mk2"))) BeltTier = 2;
			else if (ClassName.Contains(TEXT("Mk3"))) BeltTier = 3;
			else if (ClassName.Contains(TEXT("Mk4"))) BeltTier = 4;
			else if (ClassName.Contains(TEXT("Mk5"))) BeltTier = 5;
			else if (ClassName.Contains(TEXT("Mk6"))) BeltTier = 6;
			
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: Class = %s, Tier = Mk%d"), *ClassName, BeltTier);
			
			// Get recipe for this belt class
			UWorld* World = GetWorld();
			if (World)
			{
				AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
				if (RecipeManager)
				{
					// mBuildClass is a TSubclassOf<AActor>; route through UClass* and validate it's an AFGBuildable
					UClass* BuildClass = mBuildClass;
					TSubclassOf<AFGBuildable> BeltBuildableClass;
					if (BuildClass && BuildClass->IsChildOf(AFGBuildable::StaticClass()))
					{
						BeltBuildableClass = BuildClass;
					}
					else
					{
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: BuildClass is not an AFGBuildable, cannot look up descriptor"));
						return TotalCost;
					}
					TSubclassOf<UFGBuildingDescriptor> Descriptor = RecipeManager->FindBuildingDescriptorByClass(BeltBuildableClass);
					
					if (Descriptor)
					{
						TArray<TSubclassOf<UFGRecipe>> Recipes = RecipeManager->FindRecipesByProduct(Descriptor, false, true);
						if (Recipes.Num() > 0)
						{
							const UFGRecipe* RecipeCDO = Recipes[0]->GetDefaultObject<UFGRecipe>();
							if (RecipeCDO)
							{
								const TArray<FItemAmount>& BaseCost = RecipeCDO->GetIngredients();
								
								// Recipe: 1 ingredient provides 2m of belt
								// Use exact division and round, matching vanilla's cost calculation
								for (const FItemAmount& Cost : BaseCost)
								{
									FItemAmount ScaledCost = Cost;
									float ExactSets = LengthInMeters / 2.0f;
									int32 SetsNeeded = FMath::RoundToInt(ExactSets);
									ScaledCost.Amount = SetsNeeded * Cost.Amount;
									TotalCost.Add(ScaledCost);
									
									UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: Material %s, Base=%d, Length=%.2fm, ExactSets=%.2f, Rounded=%d, Cost=%d"), 
										*Cost.ItemClass->GetName(), Cost.Amount, LengthInMeters, ExactSets, SetsNeeded, ScaledCost.Amount);
								}
							}
						}
						else
						{
							UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: No recipe found for Mk%d belt"), BeltTier);
						}
					}
					else
					{
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: No descriptor found for belt class"));
					}
				}
			}
		}
		else
		{
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: No build class set!"));
		}
	}
	else
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: No spline component!"));
	}
	
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 BELT: Returning %d item types"), TotalCost.Num());
	return TotalCost;
}

void ASFConveyorBeltHologram::TriggerMeshGeneration()
{
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration CALLED on %s - mSplineData has %d points"), *GetName(), mSplineData.Num());
    
    if (!mSplineComponent)
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: mSplineComponent is NULL!"));
        return;
    }
    
    // Log ALL mSplineData points for debugging
    for (int32 i = 0; i < mSplineData.Num(); i++)
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT mSplineData[%d]: Loc=%s"), i, *mSplineData[i].Location.ToString());
    }
    
    // Push mSplineData into spline component
    // CRITICAL: Must call AFGSplineHologram explicitly (same pattern as pipe hologram)
    // Super::UpdateSplineComponent() calls AFGConveyorBeltHologram which doesn't transfer mSplineData properly
    AFGSplineHologram::UpdateSplineComponent();
    
    // Log spline stats for debugging
    const int32 PointCount = mSplineComponent->GetNumberOfSplinePoints();
    const float SplineLength = mSplineComponent->GetSplineLength();
    
    // DIAGNOSTIC: Log actual spline component points after UpdateSplineComponent
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT After UpdateSplineComponent: %d points, %.1f cm length"), PointCount, SplineLength);
    if (PointCount > 1)
    {
        // Compare both methods to see if they return different values
        FVector FirstPoint = mSplineComponent->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
        FVector SecondPoint = mSplineComponent->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local);
        FVector LastPoint = mSplineComponent->GetLocationAtSplinePoint(PointCount - 1, ESplineCoordinateSpace::Local);
        
        FVector FirstPos, FirstTan, SecondPos, SecondTan;
        mSplineComponent->GetLocationAndTangentAtSplinePoint(0, FirstPos, FirstTan, ESplineCoordinateSpace::Local);
        mSplineComponent->GetLocationAndTangentAtSplinePoint(1, SecondPos, SecondTan, ESplineCoordinateSpace::Local);
        
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT GetLocationAtSplinePoint: [0]=%s, [1]=%s, [last]=%s"),
            *FirstPoint.ToString(), *SecondPoint.ToString(), *LastPoint.ToString());
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT GetLocationAndTangentAtSplinePoint: [0]=%s, [1]=%s"),
            *FirstPos.ToString(), *SecondPos.ToString());
    }
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎯 BELT TriggerMeshGeneration: %d points, %.1f cm"), PointCount, SplineLength);
    
    if (PointCount < 2)
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: Not enough spline points (%d)"), PointCount);
        return;
    }
    
    // Get existing mesh components
    TArray<USplineMeshComponent*> MeshComps;
    GetComponents<USplineMeshComponent>(MeshComps);
    
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT: Initial mesh components: %d"), MeshComps.Num());
    
    // CRITICAL FIX: Destroy existing mesh components from base class - they don't update properly
    // The base class creates default meshes that don't respond to SetStartAndEnd() correctly
    // Same fix as SFPipelineHologram::TriggerMeshGeneration()
    for (USplineMeshComponent* OldMesh : MeshComps)
    {
        if (OldMesh)
        {
            OldMesh->DestroyComponent();
        }
    }
    MeshComps.Empty();
    
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT: Destroyed old meshes, will create fresh ones"));
    
    // Get belt mesh and mesh length from build class CDO - this ensures they match the actual tier
    UStaticMesh* BeltMesh = nullptr;
    float MeshLength = 200.0f; // Default fallback
    
    if (mBuildClass)
    {
        if (AFGBuildableConveyorBelt* BeltCDO = Cast<AFGBuildableConveyorBelt>(mBuildClass->GetDefaultObject()))
        {
            BeltMesh = BeltCDO->GetSplineMesh();
            MeshLength = BeltCDO->GetMeshLength();
            UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: Got from CDO: Mesh=%s, MeshLength=%.1f"),
                BeltMesh ? *BeltMesh->GetName() : TEXT("NULL"),
                MeshLength);
        }
        else
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: mBuildClass=%s but CDO cast failed"), *mBuildClass->GetName());
        }
    }
    else
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: mBuildClass is NULL!"));
    }
    
    // Fallback to hardcoded Mk5 mesh if CDO lookup failed
    if (!BeltMesh)
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: CDO mesh lookup failed, using fallback"));
        BeltMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk5/Mesh/SM_ConveyorBelt_Mk5.SM_ConveyorBelt_Mk5"));
    }
    if (!BeltMesh)
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: Failed to load belt mesh!"));
        return;
    }
    
    // Ensure mesh length is valid (use CDO value, not bounds calculation)
    if (MeshLength <= 0.0f)
    {
        MeshLength = 200.0f; // Default belt mesh length
    }
    
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: Mesh=%s, MeshLength=%.1f cm, SplineLength=%.1f cm"),
        *BeltMesh->GetName(), MeshLength, SplineLength);
    
    // CRITICAL FIX: Calculate segments based on spline length / mesh length (like vanilla)
    // NOT based on spline point count - that causes severe stretching
    const int32 RequiredSegments = FMath::Max(1, FMath::CeilToInt(SplineLength / MeshLength));
    
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: Need %d segments (%.1f cm each) for %.1f cm spline"),
        RequiredSegments, SplineLength / RequiredSegments, SplineLength);
    
    // Create mesh components
    while (MeshComps.Num() < RequiredSegments)
    {
        USplineMeshComponent* NewMesh = NewObject<USplineMeshComponent>(this);
        if (NewMesh)
        {
            NewMesh->SetStaticMesh(BeltMesh);
            NewMesh->SetMobility(EComponentMobility::Movable);
            NewMesh->SetForwardAxis(ESplineMeshAxis::X);
            NewMesh->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
            
            NewMesh->RegisterComponent();
            NewMesh->AttachToComponent(mSplineComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
            NewMesh->SetVisibility(true, true);
            
            MeshComps.Add(NewMesh);
        }
    }

    if (FArrayProperty* SplineMeshesProp = FindFProperty<FArrayProperty>(AFGConveyorBeltHologram::StaticClass(), TEXT("mSplineMeshes")))
    {
        if (TArray<USplineMeshComponent*>* NativeSplineMeshes = SplineMeshesProp->ContainerPtrToValuePtr<TArray<USplineMeshComponent*>>(this))
        {
            NativeSplineMeshes->Reset();
            for (USplineMeshComponent* MeshComp : MeshComps)
            {
                if (MeshComp)
                {
                    NativeSplineMeshes->Add(MeshComp);
                }
            }
        }
    }
    
    // Calculate segment length for even distribution
    const float SegmentLength = SplineLength / RequiredSegments;
    
    // Update each segment by sampling the spline at regular distance intervals
    for (int32 SegmentIdx = 0; SegmentIdx < RequiredSegments && SegmentIdx < MeshComps.Num(); SegmentIdx++)
    {
        USplineMeshComponent* MeshComp = MeshComps[SegmentIdx];
        if (MeshComp)
        {
            MeshComp->SetStaticMesh(BeltMesh);
            
            // Sample spline at distance intervals (not at spline points)
            const float StartDist = SegmentIdx * SegmentLength;
            const float EndDist = (SegmentIdx + 1) * SegmentLength;
            
            FVector StartPos = mSplineComponent->GetLocationAtDistanceAlongSpline(StartDist, ESplineCoordinateSpace::Local);
            FVector EndPos = mSplineComponent->GetLocationAtDistanceAlongSpline(EndDist, ESplineCoordinateSpace::Local);
            FVector StartTangent = mSplineComponent->GetTangentAtDistanceAlongSpline(StartDist, ESplineCoordinateSpace::Local);
            FVector EndTangent = mSplineComponent->GetTangentAtDistanceAlongSpline(EndDist, ESplineCoordinateSpace::Local);
            
            // Normalize tangents to segment length for proper mesh scaling
            StartTangent = StartTangent.GetSafeNormal() * SegmentLength;
            EndTangent = EndTangent.GetSafeNormal() * SegmentLength;
            
            MeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
            MeshComp->SetVisibility(true, true);
            MeshComp->SetHiddenInGame(false);
            MeshComp->MarkRenderStateDirty();
            
            if (SegmentIdx == 0)
            {
                UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT Segment[0]: Start=%s End=%s (dist %.1f-%.1f)"),
                    *StartPos.ToString(), *EndPos.ToString(), StartDist, EndDist);
            }
        }
    }
    
    UE_LOG(LogSmartHologram, Verbose, TEXT("🎯 BELT TriggerMeshGeneration: Created %d segments of %.1f cm each"),
        MeshComps.Num(), SegmentLength);
    
    // Apply the current hologram material state to newly generated meshes. Must bypass the #497
    // set-once guard: the mesh SET just changed even though the state value may not have.
    ApplySplineMeshMaterialState(GetHologramMaterialState());
}

// Override ConfigureActor to let parent class handle mesh initialization
void ASFConveyorBeltHologram::ConfigureActor(class AFGBuildable* inBuildable) const
{
    // CRITICAL DIAGNOSTIC: Log what's in mSplineData before ConfigureActor
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Hologram=%s, Buildable=%s"), 
        *GetName(), inBuildable ? *inBuildable->GetName() : TEXT("NULL"));
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: mSplineData has %d points"), mSplineData.Num());
    
    for (int32 i = 0; i < mSplineData.Num(); i++)
    {
        const FSplinePointData& Point = mSplineData[i];
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor:   Point[%d] Location=%s"), 
            i, *Point.Location.ToString());
    }
    
    // CRITICAL FIX: Check if we have backup data that should be restored
    // This handles the case where mSplineData was reset to default 2-point spline by vanilla
    if (FSFHologramData* HoloData = USFHologramDataRegistry::GetData(const_cast<ASFConveyorBeltHologram*>(this)))
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Registry entry found - bHasBackup=%d, BackupPoints=%d"),
            HoloData->bHasBackupSplineData ? 1 : 0, HoloData->BackupSplineData.Num());
        
        // If backup has more points than current data, OR if current data appears wrong, we need to restore
        // This detects the case where:
        // 1. 5+ point spline was reset to 2-point default
        // 2. 2-point manifold spline was zeroed out
        // 3. 2-point manifold spline was replaced with vanilla default
        bool bNeedsRestore = false;
        if (HoloData->bHasBackupSplineData && HoloData->BackupSplineData.Num() > 0)
        {
            if (HoloData->BackupSplineData.Num() > mSplineData.Num())
            {
                // More points in backup - definitely reset
                bNeedsRestore = true;
            }
            else if (mSplineData.Num() >= 2 && HoloData->BackupSplineData.Num() >= 2)
            {
                // Same point count - check if current data differs significantly from backup
                // This handles:
                // - Zeroed spline (all points at origin)
                // - Vanilla default spline replacing our manifold spline
                const FVector& CurrentEnd = mSplineData[1].Location;
                const FVector& BackupEnd = HoloData->BackupSplineData[1].Location;
                
                // If backup has a significant length (manifold = 1800cm) but current is different, restore
                float BackupLength = BackupEnd.Size();
                
                // Restore if: backup is substantial AND current differs significantly from backup
                if (BackupLength > 500.0f && !CurrentEnd.Equals(BackupEnd, 10.0f))
                {
                    bNeedsRestore = true;
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Backup end=%.1f, Current end=%.1f - differs, will restore"),
                        BackupEnd.Size(), CurrentEnd.Size());
                }
                else if (CurrentEnd.IsNearlyZero(1.0f) && !BackupEnd.IsNearlyZero(1.0f))
                {
                    // Current is zeroed but backup isn't
                    bNeedsRestore = true;
                }
            }
        }
        
        if (bNeedsRestore)
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 BELT ConfigureActor: mSplineData was reset! Restoring %d points from backup (had %d)"),
                HoloData->BackupSplineData.Num(), mSplineData.Num());
            
            // Restore spline data (need to cast away const for this emergency restore)
            ASFConveyorBeltHologram* MutableThis = const_cast<ASFConveyorBeltHologram*>(this);
            MutableThis->mSplineData = HoloData->BackupSplineData;
            
            // Also update the spline component
            if (MutableThis->mSplineComponent)
            {
                MutableThis->UpdateSplineComponent();
            }
            
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Restored mSplineData now has %d points"), 
                mSplineData.Num());
            
            // Log the restored points
            for (int32 i = 0; i < mSplineData.Num(); i++)
            {
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor:   RestoredPoint[%d] Location=%s"), 
                    i, *mSplineData[i].Location.ToString());
            }
        }
    }
    else
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: No registry entry found for this hologram"));
    }
    
    if (mSplineComponent)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: SplineComponent has %d points, length=%.1f cm"),
            mSplineComponent->GetNumberOfSplinePoints(), mSplineComponent->GetSplineLength());
    }
    else
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 BELT ConfigureActor: mSplineComponent is NULL!"));
    }
    
    // CRITICAL: Record expected Point[0] BEFORE Super call to detect if vanilla reverses the spline
    FVector ExpectedPoint0 = mSplineData.Num() > 0 ? mSplineData[0].Location : FVector::ZeroVector;
    int32 ExpectedNumPoints = mSplineData.Num();
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Expected Point[0]=%s with %d total points"), 
        *ExpectedPoint0.ToString(), ExpectedNumPoints);
    
    // Parent AFGConveyorBeltHologram handles mesh asset initialization automatically
    // No need to access private members - just call base implementation
    Super::ConfigureActor(inBuildable);
    
    // CRITICAL FIX: Detect and correct spline reversal
    // Vanilla's ConfigureActor may reverse the spline based on connection state.
    // For EXTEND child belts without snapped connections, this can cause incorrect reversal.
    // We detect this by comparing the buildable's Point[0] with our expected Point[0].
    if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(inBuildable))
    {
        TArray<FSplinePointData>& BeltSplineData = const_cast<TArray<FSplinePointData>&>(Belt->GetSplinePointData());
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: AFTER Super - Buildable has %d spline points"), 
            BeltSplineData.Num());
        
        for (int32 i = 0; i < BeltSplineData.Num(); i++)
        {
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor:   BuildablePoint[%d] Location=%s"), 
                i, *BeltSplineData[i].Location.ToString());
        }
        
        // Check if vanilla reversed the spline by comparing Point[0]
        // If the buildable's Point[0] doesn't match our expected Point[0], but the LAST point does,
        // then vanilla reversed the spline and we need to reverse it back
        if (BeltSplineData.Num() == ExpectedNumPoints && ExpectedNumPoints >= 2)
        {
            FVector ActualPoint0 = BeltSplineData[0].Location;
            FVector ActualPointN = BeltSplineData[ExpectedNumPoints - 1].Location;
            
            // Check if Point[0] matches expected (within tolerance)
            bool bPoint0Matches = ActualPoint0.Equals(ExpectedPoint0, 1.0f);
            // Check if last point matches our expected Point[0] (i.e., it was reversed)
            bool bPointNMatchesExpected0 = ActualPointN.Equals(ExpectedPoint0, 1.0f);
            
            if (!bPoint0Matches && bPointNMatchesExpected0)
            {
                UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 BELT ConfigureActor: Vanilla reversed the spline! Correcting..."));
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: ActualPoint[0]=%s, ExpectedPoint[0]=%s, ActualPoint[N]=%s"),
                    *ActualPoint0.ToString(), *ExpectedPoint0.ToString(), *ActualPointN.ToString());
                
                // Reverse the spline data back to correct order
                TArray<FSplinePointData> CorrectedSplineData;
                for (int32 i = BeltSplineData.Num() - 1; i >= 0; i--)
                {
                    FSplinePointData CorrectedPoint = BeltSplineData[i];
                    // Swap arrive and leave tangents since we're reversing direction
                    FVector TempTangent = CorrectedPoint.ArriveTangent;
                    CorrectedPoint.ArriveTangent = -CorrectedPoint.LeaveTangent;  // Negate because direction is reversed
                    CorrectedPoint.LeaveTangent = -TempTangent;
                    CorrectedSplineData.Add(CorrectedPoint);
                }
                
                // Apply corrected data via mutable pointer
                TArray<FSplinePointData>* MutableSplineData = Belt->GetMutableSplinePointData();
                if (MutableSplineData)
                {
                    *MutableSplineData = CorrectedSplineData;
                }
                
                // Note: SplineComponent will be updated during buildable's BeginPlay/PostLoad
                // We only need to fix the mSplineData which determines the actual belt behavior
                
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Spline corrected! New Point[0]=%s"),
                    MutableSplineData && MutableSplineData->Num() > 0 ? *(*MutableSplineData)[0].Location.ToString() : TEXT("EMPTY"));
            }
            else if (bPoint0Matches)
            {
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Spline direction is correct (Point[0] matches)"));
            }
            else
            {
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Spline direction unclear - Point[0] doesn't match expected, but neither does Point[N]"));
            }
        }
    }
    
    // ============================================================
    // CRITICAL: Establish connections directly in ConfigureActor
    // ============================================================
    // Snapped connections don't work for belt-to-belt or belt-to-distributor
    // connections (they work for lifts but not belts). We need to call
    // SetConnection() directly here to establish the actual connections
    // AND ensure proper chain actor creation.
    //
    // This is called DURING Construct(), so the belt is being built and
    // can be properly integrated into the conveyor chain system.
    
    // Check for EXTEND child via registry (more reliable than Tags in const method)
    FSFHologramData* HoloData = USFHologramDataRegistry::GetData(const_cast<ASFConveyorBeltHologram*>(this));
    bool bIsExtendChild = HoloData && HoloData->ExtendChainId >= 0 && HoloData->ExtendChainIndex >= 0;
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Chain connection check - HoloData=%s, ChainId=%d, ChainIndex=%d, bIsExtendChild=%d"),
        HoloData ? TEXT("valid") : TEXT("null"),
        HoloData ? HoloData->ExtendChainId : -999,
        HoloData ? HoloData->ExtendChainIndex : -999,
        bIsExtendChild ? 1 : 0);
    
    if (bIsExtendChild && HoloData)
    {
        AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(inBuildable);
        USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld());
        USFExtendService* ExtendService = SmartSubsystem ? SmartSubsystem->GetExtendService() : nullptr;
        
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: EXTEND child - Belt=%s, ExtendService=%s"),
            Belt ? *Belt->GetName() : TEXT("null"),
            ExtendService ? TEXT("valid") : TEXT("null"));
        
        if (Belt && ExtendService)
        {
            UFGFactoryConnectionComponent* BeltConn0 = Belt->GetConnection0();
            
            if (HoloData->ExtendChainIndex > 0)
            {
                // Connect to previous conveyor
                AFGBuildableConveyorBase* PrevConveyor = ExtendService->GetBuiltConveyor(
                    HoloData->ExtendChainId, HoloData->ExtendChainIndex - 1);
                
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: Looking for prev conveyor chain %d index %d → %s"),
                    HoloData->ExtendChainId, HoloData->ExtendChainIndex - 1,
                    PrevConveyor ? *PrevConveyor->GetName() : TEXT("NOT FOUND"));
                
                if (PrevConveyor && BeltConn0)
                {
                    UFGFactoryConnectionComponent* PrevConn1 = PrevConveyor->GetConnection1();
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: PrevConn1=%s (connected=%d), BeltConn0=%s (connected=%d)"),
                        PrevConn1 ? *PrevConn1->GetName() : TEXT("null"),
                        PrevConn1 ? PrevConn1->IsConnected() : -1,
                        BeltConn0 ? *BeltConn0->GetName() : TEXT("null"),
                        BeltConn0 ? BeltConn0->IsConnected() : -1);
                    
                    if (PrevConn1 && !PrevConn1->IsConnected() && !BeltConn0->IsConnected())
                    {
                        PrevConn1->SetConnection(BeltConn0);
                        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: ✅ Connected %s.Conn1 → %s.Conn0 (CHAIN LINK)"),
                            *PrevConveyor->GetName(), *Belt->GetName());
                    }
                    else
                    {
                        UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 BELT ConfigureActor: ⚠️ Cannot connect - already connected or null"));
                    }
                }
            }
            else
            {
                // First belt - connect to distributor
                AFGBuildable* Distributor = ExtendService->GetBuiltDistributor(HoloData->ExtendChainId);
                
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: First belt (index 0) - Distributor=%s, BeltConn0=%s (connected=%d)"),
                    Distributor ? *Distributor->GetName() : TEXT("NOT FOUND"),
                    BeltConn0 ? *BeltConn0->GetName() : TEXT("null"),
                    BeltConn0 ? BeltConn0->IsConnected() : -1);
                
                if (Distributor && BeltConn0 && !BeltConn0->IsConnected())
                {
                    TArray<UFGFactoryConnectionComponent*> DistConns;
                    Distributor->GetComponents<UFGFactoryConnectionComponent>(DistConns);
                    
                    // Find the appropriate distributor connection
                    EFactoryConnectionDirection NeededDir = HoloData->bIsInputChain ? 
                        EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_INPUT;
                    
                    UFGFactoryConnectionComponent* BestDistConn = nullptr;
                    float BestDist = FLT_MAX;
                    
                    for (UFGFactoryConnectionComponent* DistConn : DistConns)
                    {
                        if (DistConn && !DistConn->IsConnected() && DistConn->GetDirection() == NeededDir)
                        {
                            float Dist = FVector::Dist(BeltConn0->GetComponentLocation(), DistConn->GetComponentLocation());
                            if (Dist < BestDist)
                            {
                                BestDist = Dist;
                                BestDistConn = DistConn;
                            }
                        }
                    }
                    
                    if (BestDistConn)
                    {
                        BestDistConn->SetConnection(BeltConn0);
                        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 BELT ConfigureActor: ✅ Connected %s.%s → %s.Conn0 (DISTRIBUTOR LINK)"),
                            *Distributor->GetName(), *BestDistConn->GetName(), *Belt->GetName());
                    }
                    else
                    {
                        UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 BELT ConfigureActor: ⚠️ No suitable distributor connection found (needed dir=%d)"),
                            (int32)NeededDir);
                    }
                }
            }
        }
    }
    
    // ============================================================
    // STACKABLE BELT: Wiring handled in OnActorSpawned (deferred)
    // ============================================================
    // ConfigureActor runs too early - neighbor belts aren't registered yet.
    // Belt-to-belt wiring is done in USFSubsystem::OnActorSpawned after
    // all belts are built, same pattern as pipes.
    bool bIsStackableChild = Tags.Contains(FName(TEXT("SF_StackableChild")));
    if (bIsStackableChild && HoloData && HoloData->bIsStackableBelt)
    {
        AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(inBuildable);
        if (Belt)
        {
            UE_LOG(LogSmartHologram, Verbose, TEXT("🚧 STACKABLE BELT: Built %s (index %d) - wiring deferred to OnActorSpawned"),
                *Belt->GetName(), HoloData->StackableBeltIndex);
        }
    }
}

void ASFConveyorBeltHologram::ConfigureComponents(AFGBuildable* inBuildable) const
{
    // Call parent first
    Super::ConfigureComponents(inBuildable);
    
    // ========================================================================
    // PRE-TICK WIRING: Make connections and rebuild chain DURING construction
    // 
    // This runs DURING construction, AFTER ConfigureActor but BEFORE the
    // buildable is fully registered with the subsystem's tick arrays.
    // This is the SAME timing AutoLink uses - safe from parallel factory tick.
    // 
    // We use connection target info stored in FSFHologramData (from JSON manifest)
    // to find already-built targets and establish connections.
    // ========================================================================
    
    AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(inBuildable);
    if (!Conveyor)
    {
        return;
    }
    
    // Get hologram data for connection targets
    FSFHologramData* HoloData = USFHologramDataRegistry::GetData(const_cast<ASFConveyorBeltHologram*>(this));
    if (!HoloData)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: %s - no HoloData, skipping pre-tick wiring"),
            *Conveyor->GetName());
        return;
    }
    
    // Get services for looking up built actors
    USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld());
    USFExtendService* ExtendService = SmartSubsystem ? SmartSubsystem->GetExtendService() : nullptr;
    
    if (!ExtendService)
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: %s - no ExtendService, skipping pre-tick wiring"),
            *Conveyor->GetName());
        return;
    }
    
    UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0();
    UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1();
    bool bMadeConnection = false;
    
    // ========================================================================
    // STACKABLE BELT WIRING (Issue #220)
    // ========================================================================
    // For stackable pole belts, we search for nearby built pole connectors
    // and wire to them. This is similar to how stackable pipes work.
    if (HoloData->bIsStackableBelt)
    {
        UE_LOG(LogSmartHologram, Verbose, TEXT("⛓️ STACKABLE BELT ConfigureComponents: %s - wiring to nearby pole connectors"),
            *Conveyor->GetName());

        const float SearchRadius = 100.0f;  // 1m search radius for pole connectors
        
        // Connect Conn0 (belt output) to nearby pole connector
        if (Conn0 && !Conn0->IsConnected())
        {
            FVector SearchLoc = Conn0->GetComponentLocation();
            UFGFactoryConnectionComponent* NearestConn = nullptr;
            float NearestDist = SearchRadius;
            
            // Search all buildables for factory connectors near this endpoint
            // Include stackable poles AND other conveyor belts (for belt-to-belt connections)
            for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
            {
                AFGBuildable* Buildable = *It;
                if (Buildable == Conveyor) continue;
                
                // Search stackable poles and conveyor belts
                FString BuildableName = Buildable->GetClass()->GetName();
                bool bIsStackablePole = BuildableName.Contains(TEXT("ConveyorPoleStackable"));
                bool bIsConveyorBelt = BuildableName.Contains(TEXT("ConveyorBelt"));
                if (!bIsStackablePole && !bIsConveyorBelt) continue;
                
                TArray<UFGFactoryConnectionComponent*> Connectors;
                Buildable->GetComponents<UFGFactoryConnectionComponent>(Connectors);
                
                for (UFGFactoryConnectionComponent* Conn : Connectors)
                {
                    if (!Conn || Conn->IsConnected()) continue;
                    // For belt-to-belt: Conn0 is INPUT, so we need to find an OUTPUT
                    if (bIsConveyorBelt && Conn->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT) continue;
                    
                    float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());
                    if (Dist < NearestDist)
                    {
                        NearestDist = Dist;
                        NearestConn = Conn;
                    }
                }
            }
            
            if (NearestConn)
            {
                Conn0->SetConnection(NearestConn);
                bMadeConnection = true;
                UE_LOG(LogSmartHologram, Verbose, TEXT("⛓️ STACKABLE BELT: ✅ Wired %s.Conn0 → %s.%s (dist=%.1f)"),
                    *Conveyor->GetName(), *NearestConn->GetOwner()->GetName(), *NearestConn->GetName(), NearestDist);
            }
        }
        
        // Connect Conn1 (belt output) to nearby pole connector or other belt input
        if (Conn1 && !Conn1->IsConnected())
        {
            FVector SearchLoc = Conn1->GetComponentLocation();
            UFGFactoryConnectionComponent* NearestConn = nullptr;
            float NearestDist = SearchRadius;
            
            // Search all buildables for factory connectors near this endpoint
            // Include stackable poles AND other conveyor belts (for belt-to-belt connections)
            for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
            {
                AFGBuildable* Buildable = *It;
                if (Buildable == Conveyor) continue;
                
                // Search stackable poles and conveyor belts
                FString BuildableName = Buildable->GetClass()->GetName();
                bool bIsStackablePole = BuildableName.Contains(TEXT("ConveyorPoleStackable"));
                bool bIsConveyorBelt = BuildableName.Contains(TEXT("ConveyorBelt"));
                if (!bIsStackablePole && !bIsConveyorBelt) continue;
                
                TArray<UFGFactoryConnectionComponent*> Connectors;
                Buildable->GetComponents<UFGFactoryConnectionComponent>(Connectors);
                
                for (UFGFactoryConnectionComponent* Conn : Connectors)
                {
                    if (!Conn || Conn->IsConnected()) continue;
                    // For belt-to-belt: Conn1 is OUTPUT, so we need to find an INPUT
                    if (bIsConveyorBelt && Conn->GetDirection() != EFactoryConnectionDirection::FCD_INPUT) continue;
                    
                    float Dist = FVector::Dist(SearchLoc, Conn->GetComponentLocation());
                    if (Dist < NearestDist)
                    {
                        NearestDist = Dist;
                        NearestConn = Conn;
                    }
                }
            }
            
            if (NearestConn)
            {
                Conn1->SetConnection(NearestConn);
                bMadeConnection = true;
                UE_LOG(LogSmartHologram, Verbose, TEXT("⛓️ STACKABLE BELT: ✅ Wired %s.Conn1 → %s.%s (dist=%.1f)"),
                    *Conveyor->GetName(), *NearestConn->GetOwner()->GetName(), *NearestConn->GetName(), NearestDist);
            }
        }
        
        // Do NOT call AddConveyor here — chain creation is deferred to the
        // SFSubsystem timer which uses Respline (same pattern as Extend).
        // Calling AddConveyor during ConfigureComponents risks double-add:
        // vanilla's initialization may have already registered the belt,
        // causing chain mismatch → crash in TickFactoryActors.
        UE_LOG(LogSmartHologram, Verbose, TEXT("⛓️ STACKABLE BELT: %s wiring complete (connections=%s) - chain creation deferred to Respline"),
            *Conveyor->GetName(), bMadeConnection ? TEXT("YES") : TEXT("NO"));
        
        // Early return for stackable belts - they don't use the clone ID system
        return;
    }
    
    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: %s (CloneId=%s) checking targets: Conn0→%s.%s, Conn1→%s.%s"),
        *Conveyor->GetName(),
        *HoloData->JsonCloneId,
        *HoloData->Conn0TargetCloneId, *HoloData->Conn0TargetConnectorName.ToString(),
        *HoloData->Conn1TargetCloneId, *HoloData->Conn1TargetConnectorName.ToString());
    
    // === CONN0 CONNECTION (EXTEND belts) ===
    if (Conn0 && !Conn0->IsConnected() && !HoloData->Conn0TargetCloneId.IsEmpty())
    {
        // Look up the target buildable - either by clone ID or by source actor name
        AFGBuildable* TargetBuildable = nullptr;
        
        // Check for "source:" prefix (lane segments connecting to existing buildables)
        if (HoloData->Conn0TargetCloneId.StartsWith(TEXT("source:")))
        {
            // Extract the actor name after "source:"
            FString SourceActorName = HoloData->Conn0TargetCloneId.Mid(7);  // Skip "source:"
            TargetBuildable = ExtendService->GetSourceBuildableByName(SourceActorName);
            
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
                *SourceActorName, TargetBuildable ? *TargetBuildable->GetName() : TEXT("NOT FOUND"));
        }
        else
        {
            TargetBuildable = ExtendService->GetBuiltActorByCloneId(HoloData->Conn0TargetCloneId);
        }
        
        if (TargetBuildable)
        {
            // Find the target connector by name
            UFGFactoryConnectionComponent* TargetConn = nullptr;
            TArray<UFGFactoryConnectionComponent*> TargetConns;
            TargetBuildable->GetComponents<UFGFactoryConnectionComponent>(TargetConns);
            
            for (UFGFactoryConnectionComponent* TC : TargetConns)
            {
                if (TC && TC->GetFName() == HoloData->Conn0TargetConnectorName)
                {
                    TargetConn = TC;
                    break;
                }
            }
            
            if (TargetConn && !TargetConn->IsConnected())
            {
                Conn0->SetConnection(TargetConn);
                bMadeConnection = true;
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: ✅ Connected %s.Conn0 → %s.%s"),
                    *Conveyor->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName());
            }
            else
            {
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: Conn0 target %s.%s not found or already connected"),
                    *TargetBuildable->GetName(), *HoloData->Conn0TargetConnectorName.ToString());
            }
        }
        else
        {
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: Conn0 target buildable '%s' not yet built"),
                *HoloData->Conn0TargetCloneId);
        }
    }
    
    // === CONN1 CONNECTION ===
    if (Conn1 && !Conn1->IsConnected() && !HoloData->Conn1TargetCloneId.IsEmpty())
    {
        // Look up the target buildable - either by clone ID or by source actor name
        AFGBuildable* TargetBuildable = nullptr;
        
        // Check for "source:" prefix (lane segments connecting to existing buildables)
        if (HoloData->Conn1TargetCloneId.StartsWith(TEXT("source:")))
        {
            // Extract the actor name after "source:"
            FString SourceActorName = HoloData->Conn1TargetCloneId.Mid(7);  // Skip "source:"
            TargetBuildable = ExtendService->GetSourceBuildableByName(SourceActorName);
            
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
                *SourceActorName, TargetBuildable ? *TargetBuildable->GetName() : TEXT("NOT FOUND"));
        }
        else
        {
            TargetBuildable = ExtendService->GetBuiltActorByCloneId(HoloData->Conn1TargetCloneId);
        }
        
        if (TargetBuildable)
        {
            // Find the target connector by name
            UFGFactoryConnectionComponent* TargetConn = nullptr;
            TArray<UFGFactoryConnectionComponent*> TargetConns;
            TargetBuildable->GetComponents<UFGFactoryConnectionComponent>(TargetConns);
            
            for (UFGFactoryConnectionComponent* TC : TargetConns)
            {
                if (TC && TC->GetFName() == HoloData->Conn1TargetConnectorName)
                {
                    TargetConn = TC;
                    break;
                }
            }
            
            if (TargetConn && !TargetConn->IsConnected())
            {
                Conn1->SetConnection(TargetConn);
                bMadeConnection = true;
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: ✅ Connected %s.Conn1 → %s.%s"),
                    *Conveyor->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName());
            }
            else
            {
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: Conn1 target %s.%s not found or already connected"),
                    *TargetBuildable->GetName(), *HoloData->Conn1TargetConnectorName.ToString());
            }
        }
        else
        {
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: Conn1 target buildable '%s' not yet built"),
                *HoloData->Conn1TargetCloneId);
        }
    }
    
    // === CHAIN REBUILD ===
    // If we made any connections, manually add to subsystem then Remove→Add to rebuild
    // The conveyor isn't in the subsystem yet during ConfigureComponents, so we:
    // 1. Add it to subsystem (creates initial chain)
    // 2. Remove it (clears chain)
    // 3. Add it again (rebuilds chain with connections considered)
    // This happens BEFORE factory tick starts, so it's safe.
    //
    // For LANE SEGMENTS connecting to source distributors:
    // We need to walk from the clone distributor through all conveyors to the factory,
    // then process them in order (Remove all, Add all) to build a unified chain.
    if (bMadeConnection)
    {
        // NOTE: Chain actors may be NULL at this point because connected conveyors
        // may not have chains yet. CreateChainActors() in SFWiringManifest will
        // rebuild all chains after all wiring is complete.
        AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(GetWorld());
        if (BuildableSubsystem)
        {
            // Check if this is an EXTEND child belt (has HoloData from JSON with wiring targets)
            // Stackable belts also have HoloData but should NOT skip AddConveyor
            // EXTEND belts have Conn0TargetCloneId or Conn1TargetCloneId set for deferred wiring
            bool bIsExtendBelt = HoloData != nullptr && 
                (!HoloData->Conn0TargetCloneId.IsEmpty() || !HoloData->Conn1TargetCloneId.IsEmpty());
            
            if (bIsExtendBelt)
            {
                // Check if this is a lane segment (connects to source distributor)
                bool bIsLaneSegment = HoloData->Conn0TargetCloneId.StartsWith(TEXT("source:")) ||
                    HoloData->Conn1TargetCloneId.StartsWith(TEXT("source:"));
                
                UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: %s - EXTEND %s (skipping AddConveyor - Respline handles chain creation better)"),
                    *Conveyor->GetName(),
                    bIsLaneSegment ? TEXT("LANE SEGMENT") : TEXT("BELT SEGMENT"));
                // DON'T add EXTEND belts to subsystem here - CreateChainActors will use Respline
            }
            else
            {
                // Check if this is a stackable pole belt (deferred chain registration)
                // Stackable belts are named "StackableBelt_N" and need to wait for all belts
                // to be wired together before calling AddConveyor to create unified chains
                FString BeltName = Conveyor->GetName();
                bool bIsStackableBelt = BeltName.Contains(TEXT("StackableBelt_"));
                
                if (bIsStackableBelt)
                {
                    UE_LOG(LogSmartHologram, Verbose, TEXT("⛓️ BELT ConfigureComponents: %s - STACKABLE BELT - deferring AddConveyor until all connections made"),
                        *Conveyor->GetName());
                    // Don't call AddConveyor yet - SFSubsystem will call it after all belts are wired
                }
                else
                {
                    // Non-EXTEND, non-stackable belts - add to subsystem normally
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: %s - adding to subsystem"),
                        *Conveyor->GetName());
                    
                    BuildableSubsystem->AddConveyor(Conveyor);
                    
                    AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
                    UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: %s - initial chain: %s (%d segments)"),
                        *Conveyor->GetName(),
                        ChainActor ? *ChainActor->GetName() : TEXT("NULL"),
                        ChainActor ? ChainActor->GetNumChainSegments() : 0);
                }
            }
        }
    }
    else
    {
        UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⛓️ BELT ConfigureComponents: %s - no connections made (targets not yet built)"),
            *Conveyor->GetName());
    }
}

bool ASFConveyorBeltHologram::SetupUpgradeTarget(AFGBuildableConveyorBelt* InUpgradeTarget)
{
    if (!InUpgradeTarget)
    {
        return false;
    }
    
    // Create a fake hit result pointing at the target belt
    FHitResult HitResult;
    HitResult.bBlockingHit = true;
    HitResult.Location = InUpgradeTarget->GetActorLocation();
    HitResult.ImpactPoint = HitResult.Location;
    HitResult.ImpactNormal = FVector::UpVector;
    HitResult.Normal = FVector::UpVector;
    HitResult.HitObjectHandle = FActorInstanceHandle(InUpgradeTarget);
    
    // Call TryUpgrade which will set mUpgradedConveyorBelt internally
    return TryUpgrade(HitResult);
}

void ASFConveyorBeltHologram::SetSnappedConnections(UFGFactoryConnectionComponent* Connection0, UFGFactoryConnectionComponent* Connection1)
{
    // Use reflection to access the private mSnappedConnectionComponents array
    // This tells the vanilla system that the belt is already connected, preventing child pole spawning
    
    FProperty* SnappedProp = AFGConveyorBeltHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
    if (SnappedProp)
    {
        // mSnappedConnectionComponents is a C-style array of 2 UFGFactoryConnectionComponent*
        void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(this);
        UFGFactoryConnectionComponent** SnappedArray = static_cast<UFGFactoryConnectionComponent**>(PropAddr);
        
        if (SnappedArray)
        {
            SnappedArray[0] = Connection0;
            SnappedArray[1] = Connection1;
            
            UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND Belt: Set snapped connections on %s: [0]=%s, [1]=%s"),
                *GetName(),
                Connection0 ? *Connection0->GetName() : TEXT("nullptr"),
                Connection1 ? *Connection1->GetName() : TEXT("nullptr"));
        }
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Warning, TEXT("🔧 EXTEND Belt: Failed to find mSnappedConnectionComponents property on %s"), *GetName());
    }
}
