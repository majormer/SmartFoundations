// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Walk/SFWalkConveyance.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Hologram/FGHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"   // ASFPipelineHologram + AFGPipelineHologram + UFGPipeConnectionComponentBase (pipe adapter)
#include "FGFactoryConnectionComponent.h"
#include "FGPlayerController.h"
#include "FGRecipe.h"   // UFGRecipe for the belt-tier recipe (length-based GetCost needs it)
#include "UObject/UnrealType.h"   // FProperty reflection for the pipe's mSnappedConnectionComponents

// Distinct category from SFWalkService.cpp's LogSmartWalk: the module is a UNITY build, so two file-local
// DEFINE_LOG_CATEGORY_STATIC(LogSmartWalk) in the same TU collide (struct redefinition). Belt routing logs land
// under "LogSmartWalkBelt"; filter the log by "SmartWalk" to catch both.
DEFINE_LOG_CATEGORY_STATIC(LogSmartWalkBelt, Log, All);

void USFWalkConveyance::SetSubsystem(USFSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
}

UFGFactoryConnectionComponent* USFWalkBeltConveyance::FirstConnector(AFGHologram* Pole)
{
    if (!IsValid(Pole))
    {
        return nullptr;
    }
    TArray<UFGFactoryConnectionComponent*> Connectors;
    Pole->GetComponents<UFGFactoryConnectionComponent>(Connectors);
    return Connectors.Num() > 0 ? Connectors[0] : nullptr;
}

AFGHologram* USFWalkBeltConveyance::LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild)
{
    UE_LOG(LogSmartWalkBelt, Log, TEXT(">>> LinkOrUpdate ENTER: existing=%s from=%s to=%s parent(seed)=%s build=%d"),
        *GetNameSafe(ExistingSpan), *GetNameSafe(FromAnchor), *GetNameSafe(ToAnchor), *GetNameSafe(ParentForChild), bAddChildForBuild ? 1 : 0);
    USFSubsystem* Sub = Subsystem.Get();
    if (!Sub || !IsValid(FromAnchor) || !IsValid(ToAnchor))
    {
        UE_LOG(LogSmartWalkBelt, Warning, TEXT("<<< LinkOrUpdate EXIT: invalid sub/anchors (sub=%d from=%d to=%d)"),
            Sub ? 1 : 0, IsValid(FromAnchor) ? 1 : 0, IsValid(ToAnchor) ? 1 : 0);
        return ExistingSpan;
    }

    // #356 belt direction: Backward (StackableBeltDirection == 1) reverses flow by swapping which pole is source vs
    // target (mirrors SFAutoConnectService_Stackable). Applied here so BOTH the create and update paths below honor it.
    if (Sub->GetAutoConnectRuntimeSettings().StackableBeltDirection == 1)
    {
        Swap(FromAnchor, ToAnchor);
    }

    UFGFactoryConnectionComponent* FromConn = FirstConnector(FromAnchor);
    UFGFactoryConnectionComponent* ToConn = FirstConnector(ToAnchor);

    const FVector StartPos = FromConn ? FromConn->GetComponentLocation() : FromAnchor->GetActorLocation();
    const FVector EndPos = ToConn ? ToConn->GetComponentLocation() : ToAnchor->GetActorLocation();

    // #356: route exactly like auto-connect / Scaled-Extend — exit each pole along its CONNECTOR FACING, not
    // straight at the partner. On a turn the downstream pole is rotated, so its connector normal rotates with it,
    // and the game's own AutoRouteSpline (via ApplyBeltBuildModeRouting below) curves the belt through the turn.
    // The previous straight ToTarget made every belt a chord, so turns never curved.
    const FVector Dir = (EndPos - StartPos);
    const FVector DirN = Dir.GetSafeNormal();

    // Resolve a usable horizontal facing for a connector: prefer the real connector normal (what auto-connect
    // uses); if it is degenerate or near-vertical (stackable snap connectors point up), fall back to the pole's
    // authored heading (forward vector), which the walk orients to the segment frame.
    auto ResolveFacing = [](UFGFactoryConnectionComponent* Conn, AFGHologram* Anchor) -> FVector
    {
        FVector N = Conn ? Conn->GetConnectorNormal().GetSafeNormal() : FVector::ZeroVector;
        if (N.IsNearlyZero() || FMath::Abs(N.Z) > 0.9f)
        {
            N = IsValid(Anchor) ? Anchor->GetActorForwardVector().GetSafeNormal() : FVector::ZeroVector;
        }
        return N;
    };
    FVector StartNormal = ResolveFacing(FromConn, FromAnchor);
    FVector EndNormal   = ResolveFacing(ToConn, ToAnchor);

    // Spline convention (matches SetupBeltSpline): StartNormal exits toward the partner, EndNormal exits toward
    // the source. Orient each facing accordingly (handles which connector index we grabbed) without fighting a
    // genuine turn — for turns under 90° the sign is preserved, so the curve survives.
    if (!DirN.IsNearlyZero())
    {
        if (FVector::DotProduct(StartNormal, DirN)  < 0.0f) { StartNormal = -StartNormal; }
        if (FVector::DotProduct(EndNormal,  -DirN)  < 0.0f) { EndNormal   = -EndNormal; }
    }
    if (StartNormal.IsNearlyZero()) { StartNormal = DirN; }
    if (EndNormal.IsNearlyZero())   { EndNormal   = -DirN; }

    const int32 RoutingMode = Sub->GetAutoConnectRuntimeSettings().BeltRoutingMode;
    UE_LOG(LogSmartWalkBelt, Log, TEXT("  LinkOrUpdate routing: StartPos.world=%s EndPos.world=%s | StartN=%s EndN=%s | mode=%d len=%.1f | fromConn=%s toConn=%s"),
        *StartPos.ToString(), *EndPos.ToString(), *StartNormal.ToString(), *EndNormal.ToString(),
        RoutingMode, Dir.Size(), FromConn ? TEXT("yes") : TEXT("NULL"), ToConn ? TEXT("yes") : TEXT("NULL"));

    // Update path: re-route an existing belt to follow moved anchors (steering / back-up).
    if (ASFConveyorBeltHologram* Existing = Cast<ASFConveyorBeltHologram>(ExistingSpan))
    {
        Existing->SetActorLocation(StartPos);
        Existing->SetSnappedConnections(FromConn, ToConn);   // #356: re-wire so a direction toggle flips flow on the existing belt, not just geometry
        Existing->ApplyBeltBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
        Existing->TriggerMeshGeneration();
        Existing->ForceApplyHologramMaterial();
        UE_LOG(LogSmartWalkBelt, Log, TEXT("<<< LinkOrUpdate EXIT (UPDATE): belt=%s actor.world=%s"),
            *GetNameSafe(Existing), *Existing->GetActorLocation().ToString());
        return Existing;
    }

    // Create path: spawn a new belt span between the two poles (mirrors the stackable-pole belt creation).
    // Resolve the belt tier FIRST: BeltTierMain defaults to 0 ("Auto"), which GetBeltClassForTier rejects (tier
    // must be 1-6) — so a raw 0 silently produced a null class and NO belt. Resolve Auto → the player's highest
    // unlocked tier, exactly as auto-connect's stackable path does.
    int32 BeltTier = Sub->GetAutoConnectRuntimeSettings().BeltTierMain;
    AFGPlayerController* PC = nullptr;
    if (UWorld* W = ToAnchor->GetWorld())
    {
        PC = Cast<AFGPlayerController>(W->GetFirstPlayerController());
    }
    if (BeltTier == 0)
    {
        BeltTier = Sub->GetHighestUnlockedBeltTier(PC);
    }
    UClass* BeltBuildClass = Sub->GetBeltClassForTier(BeltTier, PC);
    if (!BeltBuildClass)
    {
        return nullptr;
    }
    // Resolve the belt RECIPE for this tier too: vanilla AFGConveyorBeltHologram::GetCost derives the length-based
    // cost from the recipe's ingredients-per-meter x spline length. Without a recipe GetCost returns 0 -> the walk
    // committed FREE belts (the build class alone is enough to BUILD, but not to COST). Mirrors the established
    // child-hologram order (SetBuildClass + SetRecipe before FinishSpawning) the stackable/Extend children use.
    const TSubclassOf<UFGRecipe> BeltRecipe = Sub->GetBeltRecipeForTier(BeltTier);

    UWorld* World = ToAnchor->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = ToAnchor->GetOwner();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = true;

    ASFConveyorBeltHologram* Belt = World->SpawnActor<ASFConveyorBeltHologram>(
        ASFConveyorBeltHologram::StaticClass(), StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Belt)
    {
        return nullptr;
    }

    Belt->SetReplicates(false);
    Belt->SetReplicateMovement(false);
    Belt->SetBuildClass(BeltBuildClass);
    if (BeltRecipe)
    {
        Belt->SetRecipe(BeltRecipe);   // length-based GetCost needs the recipe (else free belts) - mRecipe is null on a fresh spawn, so the SetRecipe !mRecipe assert is satisfied
    }
    Belt->Tags.AddUnique(FName(TEXT("SF_StackableChild")));

    USFHologramDataService::DisableValidation(Belt);
    USFHologramDataService::MarkAsChild(Belt, ToAnchor, ESFChildHologramType::WalkSegment);

    // Store connector references + a non-negative chain gate; the actual wiring is by connector COINCIDENCE
    // at Construct (Slice 3 commit), not by chain index — direction/segment-agnostic by design.
    if (FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(Belt))
    {
        HoloData->bIsStackableBelt = true;
        HoloData->StackableBeltConn0 = FromConn;
        HoloData->StackableBeltConn1 = ToConn;
        HoloData->StackChainId = 0;
        HoloData->StackChainIndex = 0;
    }

    Belt->FinishSpawning(FTransform(StartPos));
    Belt->SetSnappedConnections(FromConn, ToConn);
    Belt->ApplyBeltBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
    Belt->SetActorHiddenInGame(false);
    Belt->SetActorEnableCollision(false);
    Belt->SetActorTickEnabled(false);
    Belt->RegisterAllComponents();
    Belt->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
    if (bAddChildForBuild && IsValid(ParentForChild))
    {
        // Slice 3 COMMIT (server-side at the construct seam): AddChild to the seed so the vanilla scope()
        // cascade builds it (the Extend SpawnChildHolograms pattern). Safe here - this is a one-shot construct,
        // not the per-frame preview tick that the standalone path avoids (where AddChild's stale mChildren entry
        // crashed the build gun's ResetConstructDisqualifiers recursion). SetSnappedConnections above carries the
        // wiring into the build.
        ParentForChild->AddChild(Belt, Belt->GetFName());   // unique child name (NAME_None asserts on duplicates)
        // AddChild RE-BASES/repositions the child (vanilla), so re-route in WORLD space AFTER it - the committed
        // belt then keeps the exact shape it previewed (mirrors Extend re-applying spline data after AddChild).
        Belt->ApplyBeltBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
    }
    // PREVIEW (bAddChildForBuild=false): deliberately left STANDALONE - AddChild re-bases the belt to the
    // far-from-origin seed and its stale mChildren entry crashes the build gun's per-tick disqualifier recursion
    // on Destroy(). The belt is routed in WORLD space and tracked by SFWalkService.
    Belt->TriggerMeshGeneration();
    Belt->ForceApplyHologramMaterial();

    UE_LOG(LogSmartWalkBelt, Log, TEXT("<<< LinkOrUpdate EXIT (CREATE): belt=%s actor.world=%s | tier=%d class=%s"),
        *GetNameSafe(Belt), *Belt->GetActorLocation().ToString(), BeltTier, *GetNameSafe(BeltBuildClass));
    return Belt;
}

// ====================================================================================================
// USFWalkPipeConveyance — pipe adapter. Mirrors the belt adapter above but lays an ASFPipelineHologram
// span between two stackable pipeline supports, using the pipe routing/cost/snapped-connection path proven
// by stackable-pipe auto-connect (SFAutoConnectService_Stackable.cpp UpdateOrCreatePipeForPolePair).
// ====================================================================================================

UFGPipeConnectionComponentBase* USFWalkPipeConveyance::FirstPipeConnector(AFGHologram* Support)
{
    if (!IsValid(Support))
    {
        return nullptr;
    }
    TArray<UFGPipeConnectionComponentBase*> Connectors;
    Support->GetComponents<UFGPipeConnectionComponentBase>(Connectors);
    return Connectors.Num() > 0 ? Connectors[0] : nullptr;
}

AFGHologram* USFWalkPipeConveyance::LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild)
{
    UE_LOG(LogSmartWalkBelt, Log, TEXT(">>> [Pipe] LinkOrUpdate ENTER: existing=%s from=%s to=%s parent(seed)=%s build=%d"),
        *GetNameSafe(ExistingSpan), *GetNameSafe(FromAnchor), *GetNameSafe(ToAnchor), *GetNameSafe(ParentForChild), bAddChildForBuild ? 1 : 0);
    USFSubsystem* Sub = Subsystem.Get();
    if (!Sub || !IsValid(FromAnchor) || !IsValid(ToAnchor))
    {
        return ExistingSpan;
    }

    UFGPipeConnectionComponentBase* FromConn = FirstPipeConnector(FromAnchor);
    UFGPipeConnectionComponentBase* ToConn = FirstPipeConnector(ToAnchor);

    const FVector StartPos = FromConn ? FromConn->GetComponentLocation() : FromAnchor->GetActorLocation();
    const FVector EndPos = ToConn ? ToConn->GetComponentLocation() : ToAnchor->GetActorLocation();

    // Exit each support along its FACING (the walk orients the support to the segment heading) so a turn bows the
    // pipe toward the connector normal; keep the chord's pitch so a height delta doesn't ramp. Same intent as the
    // shared SF_ResolveSupportExitNormal in the stackable-pipe AC path (which we can't call across the TU boundary).
    const FVector Dir = (EndPos - StartPos);
    const FVector DirN = Dir.GetSafeNormal();
    auto ResolveFacing = [](AFGHologram* Anchor, const FVector& Chord) -> FVector
    {
        const FVector ChordH = FVector(Chord.X, Chord.Y, 0.0f).GetSafeNormal();
        FVector FacingH = IsValid(Anchor) ? Anchor->GetActorForwardVector() : FVector::ZeroVector;
        FacingH.Z = 0.0f;
        FacingH = FacingH.GetSafeNormal();
        if (FacingH.IsNearlyZero() || ChordH.IsNearlyZero()) { return Chord; }
        if (FVector::DotProduct(FacingH, ChordH) < 0.0f) { FacingH = -FacingH; }
        if (FVector::DotProduct(FacingH, ChordH) < 0.5f) { return Chord; }
        return (FacingH + FVector(0.0f, 0.0f, Chord.Z)).GetSafeNormal();
    };
    FVector StartNormal = ResolveFacing(FromAnchor, DirN);
    FVector EndNormal   = ResolveFacing(ToAnchor, -DirN);
    if (StartNormal.IsNearlyZero()) { StartNormal = DirN; }
    if (EndNormal.IsNearlyZero())   { EndNormal   = -DirN; }

    const int32 RoutingMode = Sub->GetAutoConnectRuntimeSettings().PipeRoutingMode;

    // Update path: re-route an existing pipe to follow moved anchors (steering / back-up).
    if (ASFPipelineHologram* Existing = Cast<ASFPipelineHologram>(ExistingSpan))
    {
        Existing->SetActorLocation(StartPos);
        Existing->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
        Existing->TriggerMeshGeneration();
        Existing->ForceApplyHologramMaterial();
        UE_LOG(LogSmartWalkBelt, Log, TEXT("<<< [Pipe] LinkOrUpdate EXIT (UPDATE): pipe=%s"), *GetNameSafe(Existing));
        return Existing;
    }

    // Create path: resolve pipe tier + indicator (mirrors the stackable-pipe AC create path; Auto -> highest unlocked).
    int32 PipeTier = Sub->GetAutoConnectRuntimeSettings().PipeTierMain;
    const bool bWithIndicator = Sub->GetAutoConnectRuntimeSettings().bPipeIndicator;
    AFGPlayerController* PC = nullptr;
    if (UWorld* W = ToAnchor->GetWorld())
    {
        PC = Cast<AFGPlayerController>(W->GetFirstPlayerController());
    }
    if (PipeTier == 0)
    {
        PipeTier = Sub->GetHighestUnlockedPipeTier(PC);
    }
    UClass* PipeBuildClass = Sub->GetPipeClassFromConfig(PipeTier, bWithIndicator, PC);
    if (!PipeBuildClass)
    {
        return nullptr;
    }
    const TSubclassOf<UFGRecipe> PipeRecipe = Sub->GetPipeRecipeForTier(PipeTier, bWithIndicator);

    UWorld* World = ToAnchor->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = ToAnchor->GetOwner();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = true;

    ASFPipelineHologram* Pipe = World->SpawnActor<ASFPipelineHologram>(
        ASFPipelineHologram::StaticClass(), StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Pipe)
    {
        return nullptr;
    }

    Pipe->SetReplicates(false);
    Pipe->SetReplicateMovement(false);
    Pipe->SetBuildClass(PipeBuildClass);
    if (PipeRecipe)
    {
        Pipe->SetRecipe(PipeRecipe);   // length-based GetCost needs the recipe (else free pipes) - mRecipe null on fresh spawn satisfies the assert
    }
    Pipe->Tags.AddUnique(FName(TEXT("SF_StackableChild")));

    USFHologramDataService::DisableValidation(Pipe);
    USFHologramDataService::MarkAsChild(Pipe, ToAnchor, ESFChildHologramType::WalkSegment);

    if (FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(Pipe))
    {
        HoloData->bIsStackablePipe = true;
        HoloData->StackablePipeConn0 = FromConn;
        HoloData->StackablePipeConn1 = ToConn;
        HoloData->StackablePipeIndex = 0;
    }

    Pipe->FinishSpawning(FTransform(StartPos));

    // Pre-wire the snapped connections. ASFPipelineHologram has no SetSnappedConnections method, so set the vanilla
    // mSnappedConnectionComponents[0/1] by reflection - exactly like the stackable-pipe AC create path.
    if (FromConn || ToConn)
    {
        if (FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents")))
        {
            if (void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(Pipe))
            {
                UFGPipeConnectionComponentBase** SnappedArray = static_cast<UFGPipeConnectionComponentBase**>(PropAddr);
                if (SnappedArray)
                {
                    SnappedArray[0] = FromConn;
                    SnappedArray[1] = ToConn;
                }
            }
        }
    }

    Pipe->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
    Pipe->SetActorHiddenInGame(false);
    Pipe->SetActorEnableCollision(false);
    Pipe->SetActorTickEnabled(false);
    Pipe->RegisterAllComponents();
    Pipe->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
    if (bAddChildForBuild && IsValid(ParentForChild))
    {
        // Slice 3 COMMIT (server-side at the construct seam): AddChild so the vanilla scope() cascade builds it.
        ParentForChild->AddChild(Pipe, Pipe->GetFName());   // unique child name (NAME_None asserts on duplicates)
        Pipe->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);   // re-route after AddChild re-bases
    }
    Pipe->TriggerMeshGeneration();
    Pipe->ForceApplyHologramMaterial();

    UE_LOG(LogSmartWalkBelt, Log, TEXT("<<< [Pipe] LinkOrUpdate EXIT (CREATE): pipe=%s actor.world=%s | tier=%d class=%s"),
        *GetNameSafe(Pipe), *Pipe->GetActorLocation().ToString(), PipeTier, *GetNameSafe(PipeBuildClass));
    return Pipe;
}
