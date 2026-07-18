// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Walk/SFWalkConveyance.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/AutoConnect/SFAutoConnectService.h"   // MAX_PIPE_LENGTH — the single-span cap for belts AND pipes
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

// Walk span cap (CHORD / straight pole-to-pole distance). The vanilla belt+pipe limit is mMaxSplineLength = 5600.1 cm
// (~56m; FGConveyorBeltHologram.h:175 / FGPipelineHologram.h:206, SAME for both). The default 56m support interval is
// valid for a straight seven-foundation run; turns, rise, or shift can make the routed span exceed this separate cap.
// Beyond it, LinkOrUpdate refuses the span and the segment shows a gap, matching stackable AC's skip-when-too-far.
static constexpr float SF_WALK_MAX_SPAN_CM = USFAutoConnectService::MAX_PIPE_LENGTH;   // 5600 cm / 56 m — the single-span max (same as stackable auto-connect); over this the span is skipped and the segment reds

// #405 hypertube walk: the tube's own connector-to-connector chord cap (96 m), NOT the 56 m pipe cap above —
// the game router shows the tube up to a 96 m chord and drops it beyond (USFAutoConnectService::MAX_HYPERTUBE_LENGTH).
static constexpr float SF_WALK_MAX_HYPER_SPAN_CM = USFAutoConnectService::MAX_HYPERTUBE_LENGTH;   // 9600 cm / 96 m

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

AFGHologram* USFWalkBeltConveyance::LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild, float SegmentTurnDeg)
{
    UE_LOG(LogSmartWalkBelt, Verbose, TEXT(">>> LinkOrUpdate ENTER: existing=%s from=%s to=%s parent(seed)=%s build=%d"),
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
    const bool bBackward = (Sub->GetAutoConnectRuntimeSettings().StackableBeltDirection == 1);
    if (bBackward)
    {
        Swap(FromAnchor, ToAnchor);
    }

    UFGFactoryConnectionComponent* FromConn = FirstConnector(FromAnchor);
    UFGFactoryConnectionComponent* ToConn = FirstConnector(ToAnchor);

    const FVector StartPos = FromConn ? FromConn->GetComponentLocation() : FromAnchor->GetActorLocation();
    const FVector EndPos = ToConn ? ToConn->GetComponentLocation() : ToAnchor->GetActorLocation();

    // Belt length cap (see SF_WALK_MAX_SPAN_CM above): a walk segment's 3D span (Advance + Rise + Shift) can exceed the
    // vanilla belt limit, and our chord is ~2m under the spline limit to leave room for the turn curve. Refuse it here:
    // return null (no span). The caller (UpdateSegmentSpans) destroys any now-too-long existing span; the commit path
    // then builds no belt for that segment (mirrors stackable AC's skip-when-too-far).
    if (FVector::Dist(StartPos, EndPos) > SF_WALK_MAX_SPAN_CM)
    {
        UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< LinkOrUpdate EXIT: belt span %.0f cm > %.0f cm cap — skipped (segment too long)"),
            FVector::Dist(StartPos, EndPos), SF_WALK_MAX_SPAN_CM);
        return nullptr;
    }

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
    // ENTRY (start): exit the source pole on the side OPPOSITE the previous segment's end — i.e. CONTINUE the previous
    // belt's travel through the shared pole, so two belts never come out the same side (maintainer's rule; vanilla lets
    // you pick either free connection point). The walk orients each pole facing -heading, so the source pole's facing
    // points BACK toward the previous belt's end; -(facing) is the continuation/heading. A reversal then routes as a
    // wide arc back into itself (e.g. a 270° loop), like a manual belt — not the inverted/straight shapes the old
    // conditional sign-flip gave (it left a PERPENDICULAR start unflipped at a 180° turn, so the belt folded back).
    // ENTRY (start): leave the source pole along the PREVIOUS segment's heading — continue the prior belt's travel
    // through the shared pole. The walk faces each pole -heading, so -(facing) IS that heading. This puts the exit
    // opposite the previous belt's entry at the shared pole (maintainer's rule), for every turn angle — no special case.
    FVector StartNormal = -ResolveFacing(FromConn, FromAnchor);
    // EXIT (end): arrive at the destination pole ALONG ITS HEADING. The pole faces back along that heading, so its
    // facing IS the arrival normal, and the belt enters opposite where the NEXT segment leaves (the through-route).
    // Do NOT snap EndN toward the chord: on a turn the chord != the heading, and the old chord sign-flip inverted the
    // arrival, folding the belt back at the shared pole (the 270° X-crossing). Straight runs (chord == heading) and the
    // 180° case (chord perpendicular to facing -> dot 0, never flipped) are unaffected.
    FVector EndNormal   = ResolveFacing(ToConn, ToAnchor);
    // #356 Backward: the swap above reversed the spline (source/dest) so flow runs the other way, but StartNormal/
    // EndNormal are still computed with the forward "leave along +heading" rule — now from the DOWNSTREAM pole, so the
    // exit points AWAY from the (now upstream) destination and the belt curls back to reach it ("returns into the back"
    // / wrong exit). Reverse both exits so a Backward run lays a STRAIGHT reversed belt, not a U-turn — the maintainer's
    // rule that a reverse flips every pole exit. (The DirN fallbacks below already reflect the swapped geometry.)
    if (bBackward) { StartNormal = -StartNormal; EndNormal = -EndNormal; }
    if (StartNormal.IsNearlyZero()) { StartNormal = DirN; }
    if (EndNormal.IsNearlyZero())   { EndNormal   = -DirN; }

    const int32 RoutingMode = Sub->GetAutoConnectRuntimeSettings().BeltRoutingMode;
    UE_LOG(LogSmartWalkBelt, Verbose, TEXT("  LinkOrUpdate routing: StartPos.world=%s EndPos.world=%s | StartN=%s EndN=%s | mode=%d len=%.1f turn=%.0f | fromConn=%s toConn=%s"),
        *StartPos.ToString(), *EndPos.ToString(), *StartNormal.ToString(), *EndNormal.ToString(),
        RoutingMode, Dir.Size(), SegmentTurnDeg, FromConn ? TEXT("yes") : TEXT("NULL"), ToConn ? TEXT("yes") : TEXT("NULL"));

    // Update path: re-route an existing belt to follow moved anchors (steering / back-up).
    if (ASFConveyorBeltHologram* Existing = Cast<ASFConveyorBeltHologram>(ExistingSpan))
    {
        Existing->SetActorLocation(StartPos);
        Existing->SetSnappedConnections(FromConn, ToConn);   // #356: re-wire so a direction toggle flips flow on the existing belt, not just geometry
        Existing->ApplyBeltBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
        Existing->TriggerMeshGeneration();
        Existing->ForceApplyHologramMaterial();
        UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< LinkOrUpdate EXIT (UPDATE): belt=%s actor.world=%s"),
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

    // [#497 L5] BEFORE FinishSpawning: BeginPlay registers the clearance detector inert
    // (the existing post-spawn disable below then finds nothing to tear down).
    Belt->SetActorEnableCollision(false);
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

    UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< LinkOrUpdate EXIT (CREATE): belt=%s actor.world=%s | tier=%d class=%s"),
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

AFGHologram* USFWalkPipeConveyance::LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild, float SegmentTurnDeg)
{
    UE_LOG(LogSmartWalkBelt, Verbose, TEXT(">>> [Pipe] LinkOrUpdate ENTER: existing=%s from=%s to=%s parent(seed)=%s build=%d"),
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

    // Pipe length cap (see SF_WALK_MAX_SPAN_CM above). Refuse an over-long span (return null, no pipe); the caller
    // destroys any now-too-long existing one.
    if (FVector::Dist(StartPos, EndPos) > SF_WALK_MAX_SPAN_CM)
    {
        UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< [Pipe] LinkOrUpdate EXIT: span %.0f cm > %.0f cm cap — skipped (segment too long)"),
            FVector::Dist(StartPos, EndPos), SF_WALK_MAX_SPAN_CM);
        return nullptr;
    }

    // ENTRY: leave the source support along the PREVIOUS heading (continue the prior span's travel through the shared
    // support); the support faces -heading, so -(forward) is that heading. EXIT: arrive at the dest support along ITS
    // heading (its facing IS the arrival normal). Both come straight from the support facings — NOT snapped to the
    // chord: on a turn the chord != the heading, and snapping inverted the arrival, folding the span at the shared
    // support (the same X-crossing the belts had). Keep each chord's pitch (Z) so a height delta doesn't ramp.
    const FVector Dir = (EndPos - StartPos);
    const FVector DirN = Dir.GetSafeNormal();
    FVector StartFwd = IsValid(FromAnchor) ? FromAnchor->GetActorForwardVector() : FVector::ZeroVector;
    StartFwd.Z = 0.0f; StartFwd = StartFwd.GetSafeNormal();
    FVector StartNormal = StartFwd.IsNearlyZero() ? DirN : (-StartFwd + FVector(0.0f, 0.0f, DirN.Z)).GetSafeNormal();
    FVector EndFwd = IsValid(ToAnchor) ? ToAnchor->GetActorForwardVector() : FVector::ZeroVector;
    EndFwd.Z = 0.0f; EndFwd = EndFwd.GetSafeNormal();
    FVector EndNormal = EndFwd.IsNearlyZero() ? -DirN : (EndFwd + FVector(0.0f, 0.0f, DirN.Z)).GetSafeNormal();
    if (StartNormal.IsNearlyZero()) { StartNormal = DirN; }
    if (EndNormal.IsNearlyZero())   { EndNormal   = -DirN; }

    const int32 RoutingMode = Sub->GetAutoConnectRuntimeSettings().PipeRoutingMode;
    UE_LOG(LogSmartWalkBelt, Verbose, TEXT("  [Pipe] LinkOrUpdate routing: StartPos=%s EndPos=%s | StartN=%s EndN=%s | mode=%d len=%.1f turn=%.0f"),
        *StartPos.ToString(), *EndPos.ToString(), *StartNormal.ToString(), *EndNormal.ToString(),
        RoutingMode, Dir.Size(), SegmentTurnDeg);

    // Update path: re-route an existing pipe to follow moved anchors (steering / back-up).
    if (ASFPipelineHologram* Existing = Cast<ASFPipelineHologram>(ExistingSpan))
    {
        Existing->SetActorLocation(StartPos);
        Existing->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
        Existing->TriggerMeshGeneration();
        Existing->ForceApplyHologramMaterial();
        UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< [Pipe] LinkOrUpdate EXIT (UPDATE): pipe=%s"), *GetNameSafe(Existing));
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

    // [#497 L5] BEFORE FinishSpawning: BeginPlay registers the clearance detector inert
    // (the existing post-spawn disable below then finds nothing to tear down).
    Pipe->SetActorEnableCollision(false);
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

    UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< [Pipe] LinkOrUpdate EXIT (CREATE): pipe=%s actor.world=%s | tier=%d class=%s"),
        *GetNameSafe(Pipe), *Pipe->GetActorLocation().ToString(), PipeTier, *GetNameSafe(PipeBuildClass));
    return Pipe;
}

// ====================================================================================================
// USFWalkHypertubeConveyance — hypertube adapter (#405). Mirrors the PIPE adapter above (same ASFPipelineHologram
// span, same routing/cost/snapped-connection path), with four substitutions + Z-flattened exit normals: the 96 m
// chord cap (SF_WALK_MAX_HYPER_SPAN_CM, not the 56 m pipe cap), the unlock-gated single build class
// (GetHypertubeClassFromConfig + Recipe_PipeHyper_C), HypertubeRoutingMode, and FLATTENED exit normals (tubes leave
// the support horizontal and let the game router climb — mirrors SFHypertube::BuildOrUpdateSpan's StartNormal.Z=0).
// We mirror rather than call BuildOrUpdateSpan because that helper ALWAYS AddChild's the span; the walk preview tick
// must keep spans standalone and AddChild only at the server commit (bAddChildForBuild).
// ====================================================================================================

UFGPipeConnectionComponentBase* USFWalkHypertubeConveyance::FirstPipeConnector(AFGHologram* Support)
{
    if (!IsValid(Support)) { return nullptr; }
    TArray<UFGPipeConnectionComponentBase*> Conns;
    Support->GetComponents<UFGPipeConnectionComponentBase>(Conns);   // catches UFGPipeConnectionComponentHyper (subclass)
    return Conns.Num() > 0 ? Conns[0] : nullptr;
}

AFGHologram* USFWalkHypertubeConveyance::LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild, float SegmentTurnDeg)
{
    UE_LOG(LogSmartWalkBelt, Verbose, TEXT(">>> [Hyper] LinkOrUpdate ENTER: existing=%s from=%s to=%s parent(seed)=%s build=%d"),
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

    // Hypertube length cap = the 96 m connector chord cap (NOT the 56 m pipe cap): over this the router drops the
    // tube, so refuse the span (return null) and the caller destroys any now-too-long existing one + reds the segment.
    if (FVector::Dist(StartPos, EndPos) > SF_WALK_MAX_HYPER_SPAN_CM)
    {
        UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< [Hyper] LinkOrUpdate EXIT: span %.0f cm > %.0f cm cap — skipped (segment too long)"),
            FVector::Dist(StartPos, EndPos), SF_WALK_MAX_HYPER_SPAN_CM);
        return nullptr;
    }

    // ENTRY/EXIT normals from the support facings (same as the pipe adapter), THEN FLATTENED to horizontal — the
    // hypertube-specific deviation: tubes must leave the support level and let the game router climb (mirrors
    // SFHypertube::BuildOrUpdateSpan's StartNormal.Z=0 / EndNormal.Z=0). Do NOT keep the chord pitch as pipes do.
    const FVector Dir = (EndPos - StartPos);
    const FVector DirN = Dir.GetSafeNormal();
    FVector StartFwd = IsValid(FromAnchor) ? FromAnchor->GetActorForwardVector() : FVector::ZeroVector;
    StartFwd.Z = 0.0f; StartFwd = StartFwd.GetSafeNormal();
    FVector StartNormal = StartFwd.IsNearlyZero() ? FVector(DirN.X, DirN.Y, 0.0f).GetSafeNormal() : (-StartFwd);
    FVector EndFwd = IsValid(ToAnchor) ? ToAnchor->GetActorForwardVector() : FVector::ZeroVector;
    EndFwd.Z = 0.0f; EndFwd = EndFwd.GetSafeNormal();
    FVector EndNormal = EndFwd.IsNearlyZero() ? FVector(-DirN.X, -DirN.Y, 0.0f).GetSafeNormal() : EndFwd;
    if (StartNormal.IsNearlyZero()) { StartNormal = FVector(DirN.X, DirN.Y, 0.0f).GetSafeNormal(); }
    if (EndNormal.IsNearlyZero())   { EndNormal   = FVector(-DirN.X, -DirN.Y, 0.0f).GetSafeNormal(); }

    const int32 RoutingMode = Sub->GetAutoConnectRuntimeSettings().HypertubeRoutingMode;
    UE_LOG(LogSmartWalkBelt, Verbose, TEXT("  [Hyper] LinkOrUpdate routing: StartPos=%s EndPos=%s | StartN=%s EndN=%s | mode=%d len=%.1f turn=%.0f"),
        *StartPos.ToString(), *EndPos.ToString(), *StartNormal.ToString(), *EndNormal.ToString(),
        RoutingMode, Dir.Size(), SegmentTurnDeg);

    // Update path: re-route an existing tube to follow moved anchors (steering / back-up).
    if (ASFPipelineHologram* Existing = Cast<ASFPipelineHologram>(ExistingSpan))
    {
        Existing->SetActorLocation(StartPos);
        Existing->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
        Existing->TriggerMeshGeneration();
        Existing->ForceApplyHologramMaterial();
        UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< [Hyper] LinkOrUpdate EXIT (UPDATE): tube=%s"), *GetNameSafe(Existing));
        return Existing;
    }

    // Create path: resolve the single, unlock-gated hypertube build class (Build_PipeHyper_C) + its recipe (cost).
    AFGPlayerController* PC = nullptr;
    if (UWorld* W = ToAnchor->GetWorld())
    {
        PC = Cast<AFGPlayerController>(W->GetFirstPlayerController());
    }
    UClass* HyperBuildClass = Sub->GetHypertubeClassFromConfig(PC);
    if (!HyperBuildClass)
    {
        return nullptr;
    }

    UWorld* World = ToAnchor->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = ToAnchor->GetOwner();
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParams.bDeferConstruction = true;

    ASFPipelineHologram* Tube = World->SpawnActor<ASFPipelineHologram>(
        ASFPipelineHologram::StaticClass(), StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Tube)
    {
        return nullptr;
    }

    Tube->SetReplicates(false);
    Tube->SetReplicateMovement(false);
    Tube->SetBuildClass(HyperBuildClass);
    if (UClass* HyperRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeHyper.Recipe_PipeHyper_C")))
    {
        Tube->SetRecipe(TSubclassOf<UFGRecipe>(HyperRecipeClass));   // length-based GetCost needs the recipe (else free tubes)
    }
    Tube->Tags.AddUnique(FName(TEXT("SF_StackableChild")));

    USFHologramDataService::DisableValidation(Tube);
    USFHologramDataService::MarkAsChild(Tube, ToAnchor, ESFChildHologramType::WalkSegment);

    if (FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(Tube))
    {
        HoloData->bIsStackablePipe = true;   // the hypertube connector IS a UFGPipeConnectionComponentBase — reuse the fields
        HoloData->StackablePipeConn0 = FromConn;
        HoloData->StackablePipeConn1 = ToConn;
        HoloData->StackablePipeIndex = 0;
    }

    // [#497 L5] BEFORE FinishSpawning: BeginPlay registers the clearance detector inert
    // (the existing post-spawn disable below then finds nothing to tear down).
    Tube->SetActorEnableCollision(false);
    Tube->FinishSpawning(FTransform(StartPos));

    if (FromConn || ToConn)
    {
        if (FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents")))
        {
            if (void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(Tube))
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

    Tube->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
    Tube->SetActorHiddenInGame(false);
    Tube->SetActorEnableCollision(false);
    Tube->SetActorTickEnabled(false);
    Tube->RegisterAllComponents();
    Tube->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
    if (bAddChildForBuild && IsValid(ParentForChild))
    {
        // Slice 3 COMMIT (server-side at the construct seam): AddChild so the vanilla scope() cascade builds it.
        ParentForChild->AddChild(Tube, Tube->GetFName());   // unique child name (NAME_None asserts on duplicates)
        Tube->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);   // re-route after AddChild re-bases
    }
    Tube->TriggerMeshGeneration();
    Tube->ForceApplyHologramMaterial();

    UE_LOG(LogSmartWalkBelt, Verbose, TEXT("<<< [Hyper] LinkOrUpdate EXIT (CREATE): tube=%s actor.world=%s | class=%s"),
        *GetNameSafe(Tube), *Tube->GetActorLocation().ToString(), *GetNameSafe(HyperBuildClass));
    return Tube;
}
