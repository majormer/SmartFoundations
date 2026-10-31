// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Features/Walk/SFWalkExistingOrigin.h"
#include "Core/Construction/SFConveyorConstructionLinks.h"
#include "SFConveyorBeltGeometry.h"
#include "Core/Helpers/SFBuildEffectHelper.h"
#include "SmartFoundations.h"
#include "Core/Net/SFVisualSplineMeshPolicy.h"
#include "Components/SplineMeshComponent.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Hologram/FGHologramBuildModeDescriptor.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildable.h"
#include "FGSplineComponent.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"   // [#497] GetRawPlacementMaterialState (O(1) parent-state read)
#include "Hologram/FGHologram.h"
#include "FGConstructDisqualifier.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/Net/SFExtendCommitValidation.h"
#include "Subsystem/SFSubsystem.h"
#include "FGConstructDisqualifier.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "EngineUtils.h"  // For TActorIterator
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "Core/Helpers/SFExtendChainHelper.h"
#if !UE_BUILD_SHIPPING
#include "HAL/PlatformStackWalk.h"
#endif

struct FSFConveyorBeltConstructionPeers
{
    FNetConstructionID Id;
    FSFConveyorConstructionEndpoint From;
    FSFConveyorConstructionEndpoint To;
    bool bInvalidSelectedPeer = false;
};

void ASFConveyorBeltHologram::CaptureConstructionPeers(const FNetConstructionID& Id)
{
    FSFConveyorConstructionLinks::Prepare(this, Id);
    if (ConstructionPeers && ConstructionPeers->Id.NetPlayerID == Id.NetPlayerID
        && ConstructionPeers->Id.Server_ID == Id.Server_ID && ConstructionPeers->Id.Client_ID == Id.Client_ID) return;
    ConstructionPeers = MakeShared<FSFConveyorBeltConstructionPeers>();
    ConstructionPeers->Id = Id;
    const auto* Property = FindFProperty<FObjectPropertyBase>(
        AFGConveyorBeltHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
    auto Capture = [&](int32 Index)
    {
        auto* Port = Property && Property->ArrayDim > Index
            ? Cast<UFGFactoryConnectionComponent>(Property->GetObjectPropertyValue(
                Property->ContainerPtrToValuePtr<void>(this, Index))) : nullptr;
        if (Port && Port->GetDirection() == EFactoryConnectionDirection::FCD_SNAP_ONLY)
            return FSFConveyorConstructionEndpoint(); // Placement anchor, no transport edge.
        const auto Intent = FSFConveyorConstructionLinks::Capture(Port, &Id);
        ConstructionPeers->bInvalidSelectedPeer |= Port && (!Intent.bValid
            || Port->GetDirection() >= EFactoryConnectionDirection::FCD_ANY);
        return Intent;
    };
    ConstructionPeers->bInvalidSelectedPeer = !Property || Property->ArrayDim != 2;
    ConstructionPeers->From = Capture(0); ConstructionPeers->To = Capture(1);
    ConstructionPeers->bInvalidSelectedPeer |= ConstructionPeers->From.bValid && ConstructionPeers->To.bValid
        && ConstructionPeers->From.Direction == ConstructionPeers->To.Direction;
}

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

    }
}

void ASFConveyorBeltHologram::CheckValidPlacement()
{
    // Existing-origin Walking reconstructs native spans before payment. These
    // must run native validation, not the ordinary visual-only stackable path.
    if (Tags.Contains(FName(TEXT("SF_WalkExistingOrigin"))))
    {
        Super::CheckValidPlacement();
        return;
    }

    // [#497] Stackable AC belts are Smart-routed previews between poles the parent already
    // validates; vanilla per-frame validation on them is pure cost — capture 4 (1,134-pole grid
    // at rest) showed CheckClearance Chaos overlap queries per belt per frame from the parent's
    // ValidatePlacementAndCost recursion. Validation is disabled for these children by design
    // (DisableValidation at spawn); skip vanilla entirely.
    if (GetParentHologram() && Tags.Contains(FName(TEXT("SF_StackableChild"))))
    {
        return;
    }

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
            if (USFHologramDataService::GetRawPlacementMaterialState(Parent) == EHologramMaterialState::HMS_ERROR)
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
        // parent's red this frame. For other parented previews, read only the parent's own
        // state: the aggregate getter includes this child and can feed its previous error
        // back into itself after the parent's placement or affordability has recovered.
        EHologramMaterialState DesiredState = USFExtendService::ResolveChildPreviewMaterialState(this);
        const bool bOwnerRejectedPreview = DesiredState == EHologramMaterialState::HMS_ERROR;
        if (!SFExtendCommitValidation::IsAuthoritativeAdmission(this)
            && DesiredState != EHologramMaterialState::HMS_ERROR
            && USFHologramDataService::GetRawPlacementMaterialState(GetParentHologram()) == EHologramMaterialState::HMS_ERROR)
        {
            DesiredState = EHologramMaterialState::HMS_ERROR;
        }
        // The build gun paints previews from construct disqualifiers, not SetPlacementMaterialState.
        ResetConstructDisqualifiers();
        // Only the independently evaluated operation state may add a blocking
        // child verdict. The parent's raw color can already include last frame's
        // child disqualifiers: converting that inherited color into a new
        // unaffordable verdict latches Restore red after materials are supplied.
        // Native parent placement/cost validation remains responsible for its
        // own rejection; inherited color is visual feedback, not another gate.
        if (bOwnerRejectedPreview)
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
        // [#497] Vanilla's one allowed call runs GenerateAndUpdateSpline with the PARENT's hit
        // result, and that does SetActorLocationAndRotation on this belt to that hit — world
        // origin during preview (origin-trap confirmed: FGConveyorBeltHologram.cpp:1624). The
        // connection-commit side effects are load-bearing (see below); the move is not. Save
        // the Smart-authored transform and put it back if vanilla relocated us.
        const FTransform SmartAuthoredTransform = GetActorTransform();
        Super::PostHologramPlacement(hitResult, callForChildren);
        if (!GetActorLocation().Equals(SmartAuthoredTransform.GetLocation(), 0.5f) ||
            !GetActorQuat().Equals(SmartAuthoredTransform.GetRotation(), 0.001f))
        {
            SetActorTransform(SmartAuthoredTransform);
        }

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
    // [#497] Child previews: vanilla runs TWO per-frame writers with CONFLICTING values — the
    // parent cascade writes the parent's state (red while unaffordable/blocked) and per-child
    // ValidatePlacementAndCost writes the child's own validity (OK — Smart children skip
    // validation). The pair oscillates red<->OK every frame, defeating the set-once gate below and
    // rebuilding every spline proxy per frame (capture 5, 1,200-pole grid: 1 fps at rest). Reject
    // the "child is fine" write while the parent shows a problem; non-OK states always pass so a
    // child can still paint its own error (e.g. #437 routed-shape-invalid).
    if (materialState == EHologramMaterialState::HMS_OK)
    {
        if (AFGHologram* Parent = GetParentHologram())
        {
            // Raw read, NOT GetHologramMaterialState(): the vanilla getter walks the child array
            // per call, which made this guard quadratic at scale (13-second frames at 8,238
            // children). See USFHologramDataService::GetRawPlacementMaterialState.
            if (USFHologramDataService::GetRawPlacementMaterialState(Parent) != EHologramMaterialState::HMS_OK)
            {
                return;
            }
        }
    }

    // #497 set-once: BOTH the vanilla Super sweep and our spline sweep dirty render proxies. The
    // early-out must come BEFORE Super — capture 4 (1,134-pole stackable grid at rest) showed the
    // parent cascade calling this per frame per belt with an unchanged state, and the unguarded
    // Super::SetPlacementMaterialState alone re-applied materials to every component each frame
    // (render-thread spline-mesh churn + texture-streaming churn = the idle lag). Once a state has
    // been swept it holds. Meshes created later are painted by TriggerMeshGeneration's trailing
    // ApplySplineMeshMaterialState.
    if (bSplineMaterialStateApplied && materialState == LastAppliedSplineMaterialState)
    {
        return;
    }

    // Let the base class update stencil/render depth
    Super::SetPlacementMaterialState(materialState);

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

// Capture one unambiguous physical neighbor in the current constructed run.
// Acceptance only retains intent; native-phase settlement owns the live edge.
static bool SF_QueueStackConnectorByCoincidence(AFGHologram* Context, const FNetConstructionID& Id,
    UFGFactoryConnectionComponent* Ours, AFGBuildableConveyorBase* Self)
{
    if (!IsValid(Ours) || !IsValid(Self) || !Self->HasAuthority()) return false; // [MP-AUTH]
    if (Ours->IsConnected() || Ours->GetConnection())
    {
        auto* Peer = Ours->GetConnection();
        if (!IsValid(Peer) || !Ours->IsConnected() || !Peer->IsConnected() || Peer->GetConnection() != Ours)
        {
            FSFConveyorConstructionLinks::RejectPrepared(Context, Id, TEXT("native_stack_edge_nonreciprocal"));
            return false;
        }
        return FSFConveyorConstructionLinks::Defer(Context, Id,
            {{FSFConveyorConstructionLinks::Capture(Ours), FSFConveyorConstructionLinks::Capture(Peer)}});
    }
    UFGFactoryConnectionComponent* Match = nullptr;
    for (const auto& Weak : GStackBuiltConveyors)
    {
        auto* Other = Weak.Get();
        if (!IsValid(Other) || Other == Self || Other->IsActorBeingDestroyed()
            || Other->GetWorld() != Self->GetWorld() || !Other->HasAuthority()) continue;
        for (auto* Candidate : {Other->GetConnection0(), Other->GetConnection1()})
        {
            if (!IsValid(Candidate) || Candidate->IsConnected() || !Ours->CanConnectTo(Candidate)
                || !Candidate->GetComponentLocation().Equals(Ours->GetComponentLocation(), 2.0)) continue;
            if (Match && Match != Candidate) return false; // Never select by iteration order.
            Match = Candidate;
        }
    }
    return Match && FSFConveyorConstructionLinks::Defer(Context, Id,
        {{FSFConveyorConstructionLinks::Capture(Ours), FSFConveyorConstructionLinks::Capture(Match)}});
}

AActor* ASFConveyorBeltHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
    TStrongObjectPtr<ASFConveyorBeltHologram> KeepPreview(this);
    if (!Tags.Contains(FName(TEXT("SF_ExtendChild"))) && !FSFConveyorConstructionLinks::Prepare(this, constructionID))
    {
        UE_LOG(LogSmartHologram, Warning, TEXT("Conveyor construction has no retained authoritative connection context; native construction not invoked."));
        return nullptr;
    }
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
    // never sees a null return), then AFTER Super::Construct retain exact connection intent to the
    // physically-coincident, free, compatible connector on any other built stacked belt
    // (SF_QueueStackConnectorByCoincidence). Geometry — not StackChainId/Index — is the basis: the
    // index is placement-relative, so multi-drag/abutting segments miss each other (THESIS §6.12).
    // Order-agnostic: the first belt at a junction registers open; whichever neighbour builds second
    // queues it for native-phase settlement. Run-ends stay open and snappable. We never touch the Extend registry.
    {
        FSFHologramData* StackHolo = USFHologramDataRegistry::GetData(this);
        if (Tags.Contains(FName(TEXT("SF_StackableChild"))) && !Tags.Contains(FName(TEXT("SF_ExtendChild")))
            && StackHolo && StackHolo->bIsStackableBelt)
        {
            const int32 ChainId = StackHolo->StackChainId;
            const int32 Index = StackHolo->StackChainIndex;
            const auto ExplicitFrom = FSFConveyorConstructionLinks::Capture(StackHolo->StackableBeltConn0.Get(), &constructionID);
            const auto ExplicitTo = FSFConveyorConstructionLinks::Capture(StackHolo->StackableBeltConn1.Get(), &constructionID);
            const bool bSelectedWalkOrigin = ActorHasTag(TEXT("SF_WalkExistingOrigin"));
            const auto WalkOrigin = FSFConveyorConstructionLinks::Capture(
                Cast<UFGFactoryConnectionComponent>(SFWalkExistingOrigin::GetSelectedStart(this)), &constructionID);
            if (bSelectedWalkOrigin && (!WalkOrigin.bValid || WalkOrigin.bPreview
                || WalkOrigin.Direction < 0 || WalkOrigin.Direction >= static_cast<int32>(EFactoryConnectionDirection::FCD_ANY)))
            {
                FSFConveyorConstructionLinks::RejectPrepared(this, constructionID, TEXT("selected_walk_origin_expired_before_construction"));
                return nullptr;
            }

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
                bool OriginRetained = !bSelectedWalkOrigin;
                bool FromRetained = !ExplicitFrom.bValid || ExplicitFrom.Direction >= static_cast<int32>(EFactoryConnectionDirection::FCD_ANY);
                bool ToRetained = !ExplicitTo.bValid || ExplicitTo.Direction >= static_cast<int32>(EFactoryConnectionDirection::FCD_ANY);
                auto Wire = [&](UFGFactoryConnectionComponent* Own)
                {
                    if (!IsValid(Own)) return false;
                    // An explicitly selected origin never falls back to a nearby different belt.
                    if (bSelectedWalkOrigin && WalkOrigin.Position.Equals(Own->GetComponentLocation(), 2.0))
                    {
                        OriginRetained = FSFConveyorConstructionLinks::Defer(this, constructionID,
                            {{FSFConveyorConstructionLinks::Capture(Own), WalkOrigin}});
                        if (!ExplicitFrom.bPreview && ExplicitFrom.ExistingPort == WalkOrigin.ExistingPort) FromRetained = OriginRetained;
                        if (!ExplicitTo.bPreview && ExplicitTo.ExistingPort == WalkOrigin.ExistingPort) ToRetained = OriginRetained;
                        return OriginRetained;
                    }
                    const FSFConveyorConstructionEndpoint* Explicit = nullptr;
                    for (const auto* Candidate : {&ExplicitFrom, &ExplicitTo})
                    {
                        // Pole preview connectors are positioning anchors, not
                        // ports on the materialized pole. Genuine transport
                        // previews resolve through their native identity receipt.
                        if (!Candidate->bValid
                            || Candidate->Direction < 0 || Candidate->Direction >= static_cast<int32>(EFactoryConnectionDirection::FCD_ANY)
                            || !Candidate->Position.Equals(Own->GetComponentLocation(), 2.0)) continue;
                        if (Explicit && (Explicit->bPreview != Candidate->bPreview
                            || Explicit->ExistingPort != Candidate->ExistingPort || Explicit->PreviewOwner != Candidate->PreviewOwner
                            || Explicit->PortName != Candidate->PortName)) return false;
                        Explicit = Candidate;
                    }
                    if (Explicit)
                    {
                        const bool Retained = FSFConveyorConstructionLinks::Defer(this, constructionID,
                            {{FSFConveyorConstructionLinks::Capture(Own), *Explicit}});
                        if (Explicit == &ExplicitFrom) FromRetained = Retained;
                        if (Explicit == &ExplicitTo) ToRetained = Retained;
                        return Retained;
                    }
                    return SF_QueueStackConnectorByCoincidence(this, constructionID, Own, BuiltConveyor);
                };
                bWired0 = Wire(BuiltConveyor->GetConnection0());
                bWired1 = Wire(BuiltConveyor->GetConnection1());
                if (!OriginRetained || !FromRetained || !ToRetained)
                    FSFConveyorConstructionLinks::RejectPrepared(this, constructionID, TEXT("selected_stack_or_walk_origin_links_not_retained"));

                // Add to the flat stacked-belt list (TWeakObjectPtr) for later-built neighbours to find.
                SF_RegisterStackBuilt(BuiltConveyor);
            }
            else FSFConveyorConstructionLinks::RejectPrepared(this, constructionID, TEXT("native_stack_belt_materialization_failed"));

            UE_LOG(LogSmartHologram, Verbose, TEXT("🚧 STACK-CHAIN: %s built [chain=%d idx=%d] native-connected or queued c0=%s c1=%s"),
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
        
        FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
        auto* Smart = USFSubsystem::Get(GetWorld());
        auto* ExtendService = Smart ? Smart->GetExtendService() : nullptr;
        if (!ExtendService || !ExtendService->IsExecutingOwnedNativeInput())
        {
            UE_LOG(LogSmartHologram, Warning, TEXT("Extend belt construction rejected without its retained native operation."));
            return nullptr;
        }
        // The complete immutable role graph is connected by the owned mutation
        // adapter after native materialization. Never replace it with chain indices.
        SetSnappedConnections(nullptr, nullptr);

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
                FSFBuildEffectHelper::Finish(Buildable);
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
    
    // Capture the exact selected peer values BEFORE native Construct resets the
    // preview tree. A future timer must not rediscover a nearby free connector.
    if (!ConstructionPeers || ConstructionPeers->Id.NetPlayerID != constructionID.NetPlayerID
        || ConstructionPeers->Id.Server_ID != constructionID.Server_ID || ConstructionPeers->Id.Client_ID != constructionID.Client_ID)
        CaptureConstructionPeers(constructionID);
    if (ConstructionPeers->bInvalidSelectedPeer)
    {
        FSFConveyorConstructionLinks::RejectPrepared(this, constructionID, TEXT("selected_native_belt_peer_or_schema_unavailable"));
        return nullptr;
    }
    const auto From = ConstructionPeers->From;
    const auto To = ConstructionPeers->To;
    ConstructionPeers.Reset();
    // A preview component cannot become a live belt's peer. Its actual built
    // owner will be resolved through ConfigureActor's identity receipt instead.
    if (From.bPreview || To.bPreview) SetSnappedConnections(nullptr, nullptr);
    AActor* BuiltActor = Super::Construct(out_children, constructionID);
    if (auto* Belt = Cast<AFGBuildableConveyorBelt>(BuiltActor))
    {
        TArray<FSFConveyorConstructionEdge> Edges;
        auto AddPeer = [&](const FSFConveyorConstructionEndpoint& Peer)
        {
            if (!Peer.bValid) return true;
            UFGFactoryConnectionComponent* Mouth = nullptr;
            for (auto* Port : {Belt->GetConnection0(), Belt->GetConnection1()})
                if (IsValid(Port) && ((Peer.Direction == static_cast<int32>(EFactoryConnectionDirection::FCD_OUTPUT)
                        && Port->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
                    || (Peer.Direction == static_cast<int32>(EFactoryConnectionDirection::FCD_INPUT)
                        && Port->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)))
                { if (Mouth) return false; Mouth = Port; }
            if (!Mouth) return false;
            Edges.Add({FSFConveyorConstructionLinks::Capture(Mouth), Peer});
            return true;
        };
        if (!AddPeer(From) || !AddPeer(To))
        {
            FSFConveyorConstructionLinks::RejectPrepared(this, constructionID, TEXT("native_belt_flow_mouth_unresolved"));
            return BuiltActor;
        }
        if (!Edges.IsEmpty() && !FSFConveyorConstructionLinks::Defer(this, constructionID, Edges))
            FSFConveyorConstructionLinks::RejectPrepared(this, constructionID, TEXT("native_belt_links_not_retained"));
    }
    else FSFConveyorConstructionLinks::RejectPrepared(this, constructionID, TEXT("native_belt_materialization_failed"));
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

	// #497 cost cache (see header). The health check stays IN FRONT of the cache: a vanilla-reset
	// (zero-length) spline must fall through to the full path so the #357 defensive restoration
	// below still repairs the preview. Gated to extend preview children — they have no hologram
	// children of their own, so the includeChildren flag cannot change the result.
	const bool bSplineHealthy = mSplineComponent && mSplineComponent->GetSplineLength() > KINDA_SMALL_NUMBER;
	if (bSelfCostCacheValid && bSplineHealthy && Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		return CachedSelfCost;
	}

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
		if (bSplineHealthy)
		{
			CachedSelfCost = TotalCost;
			bSelfCostCacheValid = true;
		}
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

	// #497: cache the computed fallback cost while the spline is healthy (a zero-length spline
	// leaves the cache invalid so the restoration path gets another chance next call).
	if (mSplineComponent && mSplineComponent->GetSplineLength() > KINDA_SMALL_NUMBER)
	{
		CachedSelfCost = TotalCost;
		bSelfCostCacheValid = true;
	}
	return TotalCost;
}

void ASFConveyorBeltHologram::TriggerMeshGeneration()
{
    InvalidateCostCache();  // #497: spline (and therefore length-based cost) is changing
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
    
    // [#511] STOP HERE ON A DEDICATED SERVER. Everything above is data — mSplineData pushed into
    // the spline component, which construction uses. Everything below builds the VISUAL spline
    // mesh segments, which a headless server has no use for and, since 1.2.4, cannot survive:
    // cooked server builds strip static-mesh render data, and SetStaticMesh on a fresh
    // USplineMeshComponent faulted inside USplineMeshComponent::UpdateBounds (SIGSEGV reading
    // 0xffffffff00000401), taking the whole server down during auto-connect evaluation.
    //
    // Same rule FSFArrowModule_StaticMesh already applies ("client cosmetics ... no render data
    // under the null renderer"). mSplineMeshes is written back to vanilla purely so the game knows
    // about the segments; nothing in the construction path reads it.
    if (!SFVisualSplineMeshPolicy::ShouldGenerate(GetNetMode()))
    {
        UE_LOG(LogSmartHologram, Verbose,
            TEXT("[#511] BELT TriggerMeshGeneration: skipped on dedicated server (client cosmetics)."));
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
        if (Tags.Contains(FName(TEXT("SF_ExtendChild")))
            && BeltSplineData.Num() == ExpectedNumPoints && ExpectedNumPoints >= 2)
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
    // #504: AFGBuildableConveyorBelt explicitly requires zero actor rotation.
    // Native insertion splits can retain local spline data but drop the actor's
    // rotation on the second half. Bake orientation into the NEW belt's points
    // and tangents after direction correction, before component setup/registration.
    // The preview/topology stays in its original frame. This shared construction
    // boundary also covers Scaled Extend, Restore and authoritative reconstruction;
    // it never scans or changes existing saved belts, and needs no render resources.
    if (AFGBuildableConveyorBelt* Belt = Cast<AFGBuildableConveyorBelt>(inBuildable))
    {
        if (TArray<FSplinePointData>* Points = Belt->GetMutableSplinePointData())
        {
            FTransform BuiltTransform = Belt->GetActorTransform();
            if (SFConveyorBeltGeometry::BakeActorRotation(BuiltTransform, *Points))
            {
                Belt->SetActorTransform(BuiltTransform);
            }
        }
    }

    // ConfigureActor owns geometry only. The retained construction owner wires
    // exact roles after all native actors exist; no chain-index/nearest fallback.
}

void ASFConveyorBeltHologram::ConfigureComponents(AFGBuildable* inBuildable) const
{
    // Preserve native pre-BeginPlay configuration and its one initial registration.
    // Additional ordinary edges use ConstructionLinks; Extend uses its owned cohort.
    Super::ConfigureComponents(inBuildable);
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
    const auto* Property = FindFProperty<FObjectPropertyBase>(
        AFGConveyorBeltHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
    if (!Property || Property->ArrayDim != 2)
    {
        UE_LOG(LogSmartHologram, Warning, TEXT("Native belt snapped-connection schema is unavailable; selected peers were not modified."));
        return;
    }
    // Respect reflected TObjectPtr storage in every target; raw-pointer array
    // aliasing is not a valid native property assignment contract.
    Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, 0), Connection0);
    Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(this, 1), Connection1);
}

void ASFConveyorBeltHologram::SetHologramNudgeLocation()
{
	// [#497] Extend children: block vanilla's locked-parent nudge cascade, which bypasses the
	// SF_ExtendChild SetHologramLocationAndRotation guard via plain SetActorLocation and dragged
	// every extend child to world origin each tick (origin-trap stack: FGBuildGunBuild.cpp:320 ->
	// FGHologram.cpp:440 -> :2120). Non-extend instances keep vanilla behavior.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		return;
	}
	Super::SetHologramNudgeLocation();
}
