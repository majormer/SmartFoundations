// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkConveyance.h"
#include "Features/AutoConnect/SFAutoConnectService.h"   // IsStackablePipelineSupportHologram (seed-type -> conveyance adapter)
#include "Shared/Conduits/SFConveyanceShape.h"            // shared span shape-validity rules (walk + hypertube AC)
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Hologram/FGHologram.h"
#include "FGRecipe.h"
#include "FGFactoryConnectionComponent.h"
#include "FGCharacterPlayer.h"            // player inventory for walk affordability (red previews when broke)
#include "FGPlayerController.h"           // resolve "Auto" belt/pipe tier client-side (the server has no local PC)
#include "FGInventoryComponent.h"
#include "FGCentralStorageSubsystem.h"
#include "Resources/FGItemDescriptor.h"  // UFGItemDescriptor key for the cost tally
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"   // FArrayProperty / FindFProperty for the safe-teardown reflection unlink

DEFINE_LOG_CATEGORY_STATIC(LogSmartWalk, Log, All);

// Safe hologram teardown — MIRRORS the grid's QueueChildForDestroy (SFHologramHelperService_Children.cpp): if the
// hologram is in a parent's vanilla mChildren, reflection-unlink it FIRST, then Destroy(). This keeps the build
// gun's per-tick AFGHologram::ResetConstructDisqualifiers recursion from ever dereferencing a freed mChildren
// entry. Walk holograms are standalone (no AddChild), so the unlink is normally a no-op — but this makes teardown
// crash-proof regardless of how a hologram came to have a parent.
static void SF_SafeDestroyHologram(AFGHologram* Holo)
{
    if (!IsValid(Holo))
    {
        return;
    }
    if (AFGHologram* Parent = Holo->GetParentHologram())
    {
        if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
        {
            if (TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent))
            {
                ChildrenArray->Remove(Holo);
            }
        }
    }
    Holo->Destroy();
}

// Belt-bus cross-section spacing (cm). The stackable pole footprint is ~(100,200,200); these pack lanes/levels
// edge-to-edge. Tunable — the maintainer can dial these once the bus shape is visible.
static constexpr float WALK_LANE_SPACING  = 200.0f;   // lateral gap between parallel lanes (perpendicular to heading)
static constexpr float WALK_STACK_SPACING = 200.0f;   // vertical gap between stacked belt levels

// Diagnostic helper: dump a hologram's WORLD transform, its LOCAL/relative transform (to whatever it is
// attached to — for a standalone walk preview this should EQUAL the world transform), the seed's world
// transform, and the delta between where we INTENDED to place it and where it actually landed. This is the
// single source of truth for "where did this thing go, in world vs local space" — log it at every placement.
static void SF_LogPlacement(const TCHAR* Tag, int32 Index, AFGHologram* Holo, const FTransform& IntendedWorld, AFGHologram* Seed)
{
    if (!IsValid(Holo))
    {
        UE_LOG(LogSmartWalk, Warning, TEXT("  [PLACE] %s[%d]: hologram INVALID"), Tag, Index);
        return;
    }
    const FVector  WLoc = Holo->GetActorLocation();
    const FRotator WRot = Holo->GetActorRotation();
    FVector  LLoc = FVector::ZeroVector;
    FRotator LRot = FRotator::ZeroRotator;
    bool bAttached = false;
    if (USceneComponent* Root = Holo->GetRootComponent())
    {
        LLoc = Root->GetRelativeLocation();
        LRot = Root->GetRelativeRotation();
        bAttached = (Root->GetAttachParent() != nullptr);
    }
    const FVector SeedLoc = IsValid(Seed) ? Seed->GetActorLocation() : FVector::ZeroVector;
    const FVector Delta   = WLoc - IntendedWorld.GetLocation();
    UE_LOG(LogSmartWalk, Log,
        TEXT("  [PLACE] %s[%d] %s | intended.world=%s yaw=%.1f | actual.world=%s yaw=%.1f | actual.local=%s yaw=%.1f (attached=%d) | seed.world=%s | delta(actual-intended)=%s |delta|=%.1f"),
        Tag, Index, *GetNameSafe(Holo),
        *IntendedWorld.GetLocation().ToString(), IntendedWorld.Rotator().Yaw,
        *WLoc.ToString(), WRot.Yaw,
        *LLoc.ToString(), LRot.Yaw, bAttached ? 1 : 0,
        *SeedLoc.ToString(),
        *Delta.ToString(), Delta.Size());
}

void USFWalkService::Initialize(USFSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogSmartWalk, Log, TEXT("Initialize: subsystem=%s"), *GetNameSafe(InSubsystem));
}

// Per-segment guard (also enforced in the Walk panel spinbox). A 0-length first/edited span tangles and can build
// reversed — a segment that wants the same point should just be dropped, so the floor is 1 m. (Turn is NOT capped:
// a 180° reversal is valid; the belt router pairs the connectors for it — see SFWalkConveyance::LinkOrUpdate.)
static constexpr float SF_WALK_MIN_ADVANCE_CM     = 100.0f;    // 1 m floor (no 0-length spans)
static constexpr float SF_WALK_DEFAULT_ADVANCE_CM = 5400.0f;   // first segment starts at the 54 m max — nobody walks at 1 m
static constexpr float SF_WALK_MAX_TURN_DEG       = 270.0f;    // a single segment can spiral up to a 270° loop (vanilla-valid wide arc); beyond that just wraps/overlaps

bool USFWalkService::EnterWalk(AFGHologram* InSeedHologram)
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> EnterWalk ENTER: seed=%s valid=%d recipe=%s bActive=%d"),
        *GetNameSafe(InSeedHologram), IsValid(InSeedHologram) ? 1 : 0,
        IsValid(InSeedHologram) ? *GetNameSafe(InSeedHologram->GetRecipe()) : TEXT("n/a"), bActive ? 1 : 0);

    if (bActive)
    {
        UE_LOG(LogSmartWalk, Log, TEXT("<<< EnterWalk EXIT: already active"));
        return true;
    }

    if (!IsValid(InSeedHologram) || !InSeedHologram->GetRecipe())
    {
        UE_LOG(LogSmartWalk, Warning, TEXT("<<< EnterWalk EXIT: invalid seed hologram or no recipe"));
        return false;
    }

    SeedHologram = InSeedHologram;
    // Seed straight from the held pole's transform. The run advances via the SAME established grid-placement
    // transform auto-connect uses (FSFPositionCalculator, in AccumulateFrame), which derives "forward" from the
    // pole's Yaw — so the walk extends the exact direction auto-connect would, no bespoke axis math needed.
    OriginFrame = InSeedHologram->GetActorTransform();
    UE_LOG(LogSmartWalk, Log, TEXT("EnterWalk: OriginFrame(seed.world) loc=%s yaw=%.1f | seed locked=%d hidden=%d"),
        *OriginFrame.GetLocation().ToString(), OriginFrame.Rotator().Yaw,
        InSeedHologram->IsHologramLocked() ? 1 : 0, InSeedHologram->IsHidden() ? 1 : 0);

    // Select the conveyance adapter from the held buildable: a stackable PIPELINE support lays pipes; otherwise
    // (a stackable conveyor pole) belts. The chosen type travels in the commit spec so the server reconstructs the
    // matching span. (Later this generalizes to an ISFWalkConveyance registry for hyper-tubes, tracks, etc.)
    ConveyanceType = USFAutoConnectService::IsStackablePipelineSupportHologram(InSeedHologram)
        ? ESFWalkConveyanceType::Pipe
        : ESFWalkConveyanceType::Belt;
    if (ConveyanceType == ESFWalkConveyanceType::Pipe)
    {
        USFWalkPipeConveyance* PipeConveyance = NewObject<USFWalkPipeConveyance>(this);
        PipeConveyance->SetSubsystem(Subsystem.Get());
        Conveyance = PipeConveyance;
    }
    else
    {
        USFWalkBeltConveyance* BeltConveyance = NewObject<USFWalkBeltConveyance>(this);
        BeltConveyance->SetSubsystem(Subsystem.Get());
        Conveyance = BeltConveyance;
    }
    UE_LOG(LogSmartWalk, Log, TEXT("EnterWalk: conveyance = %s"),
        ConveyanceType == ESFWalkConveyanceType::Pipe ? TEXT("PIPE") : TEXT("BELT"));

    // Fresh walk starts single-lane (1×1). The cross-section persists across segments via these counters.
    CrossSectionLanes = 1;
    CrossSectionStacks = 1;

    Segments.Reset();
    FSFWalkSegment Seg0;
    Seg0.Advance = SF_WALK_DEFAULT_ADVANCE_CM;   // start at the 54 m max — nobody builds 1 m segments (new segments still inherit the previous advance, even if reduced)
    Segments.Add(Seg0);   // segment 0, the first anchor forward
    ActiveIndex = 0;
    bActive = true;

    // Build the origin cross-section (seed = cell 0,0) BEFORE segment 0 so segment 0's belts connect back to it.
    RebuildOriginHolograms();
    SpawnSegmentHologram(0);

    UE_LOG(LogSmartWalk, Log, TEXT("<<< EnterWalk EXIT: seeded Path at %s, segments=%d active=%d"),
        *OriginFrame.GetLocation().ToString(), Segments.Num(), ActiveIndex);
    return true;
}

void USFWalkService::ExitWalk(bool bCommit)
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> ExitWalk ENTER: bActive=%d segments=%d bCommit=%d"), bActive ? 1 : 0, Segments.Num(), bCommit ? 1 : 0);
    // Slice 3: the ACTUAL build is the parameters-only commit spec the server reconstructs (staged at the fire
    // hook via Server_StageWalkCommit -> ReconstructWalkCommitOnServer). The client side only ever needs to tear
    // down its standalone PREVIEW holograms - whether we committed (server is building from the spec) or cancelled.
    ClearAll();
    SeedHologram = nullptr;
    bActive = false;
    UE_LOG(LogSmartWalk, Log, TEXT("<<< ExitWalk EXIT: preview torn down"));
}

FSFWalkCommitSpec USFWalkService::BuildCommitSpec() const
{
    FSFWalkCommitSpec Spec;
    AFGHologram* Seed = SeedHologram.Get();
    if (!bActive || !IsValid(Seed) || Segments.Num() == 0)
    {
        return Spec;   // bValid stays false = explicit clear (overwrite semantics)
    }

    Spec.OriginFrame = OriginFrame;
    Spec.Segments.Reserve(Segments.Num());
    for (const FSFWalkSegment& Seg : Segments)
    {
        FSFWalkCommitSegment Out;
        Out.Advance     = Seg.Advance;
        Out.TurnDegrees = Seg.TurnDegrees;
        Out.Rise        = Seg.Rise;
        Out.Shift       = Seg.Shift;
        Out.NumLanes    = Seg.NumLanes;
        Out.NumStacks   = Seg.NumStacks;
        Spec.Segments.Add(Out);
    }

    Spec.ConveyanceType = ConveyanceType;
    if (USFSubsystem* Sub = Subsystem.Get())
    {
        const auto& AC = Sub->GetAutoConnectRuntimeSettings();
        Spec.BeltRoutingMode = AC.BeltRoutingMode;
        Spec.BeltTier        = AC.BeltTierMain;
        Spec.PipeRoutingMode = AC.PipeRoutingMode;
        Spec.PipeTier        = AC.PipeTierMain;
        Spec.bPipeIndicator  = AC.bPipeIndicator;
        Spec.BeltDirection   = AC.StackableBeltDirection;

        // Resolve "Auto" (tier 0) to a concrete tier HERE, on the machine that owns the walk — a client (or the
        // listen-host) always has a local player. The server reconstruction runs with NO local PlayerController
        // (null on a dedicated server), where the tier resolver falls back to Mk1, so leaving Auto as 0 would
        // silently build Mk1 belts/pipes regardless of the player's unlocks. Pinning the concrete tier into the
        // spec also keeps the committed run matching the previewed tier and the staged cost.
        if (UWorld* W = Seed->GetWorld())
        {
            AFGPlayerController* LocalPC = Cast<AFGPlayerController>(W->GetFirstPlayerController());
            if (Spec.BeltTier == 0) { Spec.BeltTier = Sub->GetHighestUnlockedBeltTier(LocalPC); }
            if (Spec.PipeTier == 0) { Spec.PipeTier = Sub->GetHighestUnlockedPipeTier(LocalPC); }
        }
    }
    Spec.BuildClass = Seed->GetBuildClass();

    // Cost = every walk-spawned preview hologram (origin extras + each segment's poles + belts). The seed itself
    // (origin cell 0,0) is charged by vanilla as the build-gun's own hologram, so it is excluded here.
    auto AddCost = [&Spec](AFGHologram* Holo, const TCHAR* Kind)
    {
        if (!IsValid(Holo)) { return; }
        const TArray<FItemAmount> C = Holo->GetCost(/*includeChildren=*/false);
        // [cost-diag] one line per costed hologram (Verbose: BuildCommitSpec runs every preview frame now).
        UE_LOG(LogSmartWalk, Verbose, TEXT("  [cost] %s %s recipe=%s -> %d item type(s)%s"),
            Kind, *Holo->GetName(), Holo->GetRecipe() ? TEXT("yes") : TEXT("NULL"), C.Num(),
            C.Num() > 0 ? *FString::Printf(TEXT(" first.amt=%d"), C[0].Amount) : TEXT(""));
        for (const FItemAmount& Item : C)
        {
            bool bMerged = false;
            for (FItemAmount& Existing : Spec.Cost)
            {
                if (Existing.ItemClass == Item.ItemClass) { Existing.Amount += Item.Amount; bMerged = true; break; }
            }
            if (!bMerged) { Spec.Cost.Add(Item); }
        }
    };
    for (const TWeakObjectPtr<AFGHologram>& H : OriginHolograms)
    {
        if (H.Get() != Seed) { AddCost(H.Get(), TEXT("origin-pole")); }
    }
    for (const FSFWalkSegment& Seg : Segments)
    {
        for (const TWeakObjectPtr<AFGHologram>& H : Seg.Holograms) { AddCost(H.Get(), TEXT("pole")); }
        for (const TWeakObjectPtr<AFGHologram>& S : Seg.Spans)     { AddCost(S.Get(), TEXT("span")); }
    }

    // Block the commit when any segment is an invalid SHAPE (too long / too steep) — the same restriction the previews red.
    Spec.bValid = !HasInvalidSegmentShape();
    UE_LOG(LogSmartWalk, Verbose, TEXT("BuildCommitSpec: %d segment(s), beltMode=%d tier=%d, %d cost item type(s) TOTAL"),
        Spec.Segments.Num(), Spec.BeltRoutingMode, Spec.BeltTier, Spec.Cost.Num());
    return Spec;
}

int32 USFWalkService::ReconstructWalkCommitOnServer(AFGHologram* Seed, const FSFWalkCommitSpec& Spec)
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> ReconstructWalkCommitOnServer ENTER: seed=%s segments=%d"),
        *GetNameSafe(Seed), Spec.Segments.Num());
    if (!IsValid(Seed) || Spec.Segments.Num() == 0)
    {
        return 0;
    }
    TSubclassOf<UFGRecipe> Recipe = Seed->GetRecipe();
    if (!Recipe)
    {
        UE_LOG(LogSmartWalk, Warning, TEXT("<<< ReconstructWalkCommitOnServer: seed has no recipe"));
        return 0;
    }
    AActor* Owner = Seed->GetOwner();
    APawn* HologramInstigator = Cast<APawn>(Seed->GetInstigator());

    // Apply the client's routing mode + tier so the server routes the spans identically (its own runtime settings
    // default elsewhere; mirrors Extend's #380/#386 install before re-deriving), and build a LOCAL conveyance
    // adapter from the spec's type - the member Conveyance is null on a server with no live walk session (dedi),
    // so never rely on it here.
    USFWalkConveyance* BuildConveyance = nullptr;
    if (USFSubsystem* Sub = Subsystem.Get())
    {
        Sub->SetAutoConnectBeltRoutingMode(Spec.BeltRoutingMode);
        Sub->SetAutoConnectBeltTierMain(Spec.BeltTier);
        Sub->SetAutoConnectPipeRoutingMode(Spec.PipeRoutingMode);
        Sub->SetAutoConnectPipeTierMain(Spec.PipeTier);
        Sub->SetAutoConnectPipeIndicator(Spec.bPipeIndicator);
        Sub->SetAutoConnectStackableBeltDirection(Spec.BeltDirection);

        if (Spec.ConveyanceType == ESFWalkConveyanceType::Pipe)
        {
            USFWalkPipeConveyance* P = NewObject<USFWalkPipeConveyance>(this);
            P->SetSubsystem(Sub);
            BuildConveyance = P;
        }
        else
        {
            USFWalkBeltConveyance* B = NewObject<USFWalkBeltConveyance>(this);
            B->SetSubsystem(Sub);
            BuildConveyance = B;
        }
    }

    // Rebuild a LOCAL segment list from the spec deltas - no member-state mutation, so this is reentrant on the
    // server and never clobbers a listen-host's own live walk session.
    TArray<FSFWalkSegment> Segs;
    Segs.Reserve(Spec.Segments.Num());
    for (const FSFWalkCommitSegment& S : Spec.Segments)
    {
        FSFWalkSegment Seg;
        Seg.Advance = S.Advance; Seg.TurnDegrees = S.TurnDegrees; Seg.Rise = S.Rise; Seg.Shift = S.Shift;
        Seg.NumLanes = S.NumLanes; Seg.NumStacks = S.NumStacks;
        Segs.Add(Seg);
    }

    // Spawn one pole AddChild'd to the seed at a world pose (the vanilla scope() cascade builds it).
    auto SpawnBuildPole = [&](const FTransform& Pose) -> AFGHologram*
    {
        AFGHologram* Holo = AFGHologram::SpawnHologramFromRecipe(Recipe, Owner ? Owner : Seed, Pose.GetLocation(), HologramInstigator);
        if (!IsValid(Holo)) { return nullptr; }
        USFHologramDataService::DisableValidation(Holo);
        // AddChild ASSERTS on a duplicate child name (FGHologram.cpp:2470) - NAME_None collides on the 2nd child.
        // The hologram's own actor FName is unique within the world, so use it as the (unique) child name.
        Seed->AddChild(Holo, Holo->GetFName());
        // AddChild RE-BASES the child's transform relative to the seed (vanilla repositions children) - set the
        // WORLD pose AFTER so the pole lands where the deltas put it, not at the far-origin re-base that caused the
        // earlier horizon bug. (Mirrors Extend re-applying child geometry after AddChild.)
        Holo->SetActorLocationAndRotation(Pose.GetLocation(), Pose.Rotator());
        Holo->UpdateComponentTransforms();
        return Holo;
    };

    int32 Spawned = 0;

    // Origin cross-section: cells 1..N (cell 0,0 IS the seed = the parent being built). MVP bus is uniform, so the
    // origin uses segment 0's signed lane/stack counters.
    const int32 OLanes  = FMath::Max(1, FMath::Abs(Segs[0].NumLanes));
    const int32 OStacks = FMath::Max(1, FMath::Abs(Segs[0].NumStacks));
    const int32 OLaneSign  = (Segs[0].NumLanes  >= 1) ? 1 : -1;
    const int32 OStackSign = (Segs[0].NumStacks >= 1) ? 1 : -1;
    TArray<AFGHologram*> OriginPoles;
    OriginPoles.SetNumZeroed(OLanes * OStacks);
    for (int32 L = 0; L < OLanes; ++L)
    {
        for (int32 K = 0; K < OStacks; ++K)
        {
            const int32 Flat = L * OStacks + K;
            if (L == 0 && K == 0)
            {
                OriginPoles[Flat] = Seed;   // cell 0,0 = the seed itself
                continue;
            }
            const FTransform Pose = CrossSectionPose(Spec.OriginFrame, L * OLaneSign, K * OStackSign);
            OriginPoles[Flat] = SpawnBuildPole(Pose);
            if (OriginPoles[Flat]) { ++Spawned; }
        }
    }

    // Each segment's cross-section poles, at the frame the deltas derive (forward kinematics from the origin).
    TArray<TArray<AFGHologram*>> SegPoles;
    SegPoles.SetNum(Segs.Num());
    for (int32 i = 0; i < Segs.Num(); ++i)
    {
        const int32 Lanes  = FMath::Max(1, FMath::Abs(Segs[i].NumLanes));
        const int32 Stacks = FMath::Max(1, FMath::Abs(Segs[i].NumStacks));
        const int32 LaneSign  = (Segs[i].NumLanes  >= 1) ? 1 : -1;
        const int32 StackSign = (Segs[i].NumStacks >= 1) ? 1 : -1;
        const FTransform Center = AccumulateFrame(Segs, Spec.OriginFrame, i);
        SegPoles[i].SetNumZeroed(Lanes * Stacks);
        for (int32 L = 0; L < Lanes; ++L)
        {
            for (int32 K = 0; K < Stacks; ++K)
            {
                const int32 Flat = L * Stacks + K;
                const FTransform Pose = CrossSectionPose(Center, L * LaneSign, K * StackSign);
                SegPoles[i][Flat] = SpawnBuildPole(Pose);
                if (SegPoles[i][Flat]) { ++Spawned; }
            }
        }
    }

    // Spanning belts: predecessor pole (same flat cell) -> this segment's pole, AddChild'd + pre-wired so the
    // cascade builds + connects them. (Uniform MVP: predecessor and current share the same cell count/indexing.)
    int32 Spans = 0;
    if (BuildConveyance)
    {
        for (int32 i = 0; i < Segs.Num(); ++i)
        {
            const TArray<AFGHologram*>& Prev = (i == 0) ? OriginPoles : SegPoles[i - 1];
            const TArray<AFGHologram*>& Cur  = SegPoles[i];
            for (int32 Cell = 0; Cell < Cur.Num(); ++Cell)
            {
                AFGHologram* ToPole   = Cur.IsValidIndex(Cell)  ? Cur[Cell]  : nullptr;
                AFGHologram* FromPole = Prev.IsValidIndex(Cell) ? Prev[Cell] : nullptr;
                if (!IsValid(ToPole) || !IsValid(FromPole)) { continue; }
                if (BuildConveyance->LinkOrUpdate(nullptr, FromPole, ToPole, Seed, /*bAddChildForBuild=*/true, Segs[i].TurnDegrees))
                {
                    ++Spans;
                }
            }
        }
    }

    UE_LOG(LogSmartWalk, Log, TEXT("<<< ReconstructWalkCommitOnServer EXIT: %d pole(s) + %d span(s) AddChild'd to %s; vanilla construct will build them"),
        Spawned, Spans, *GetNameSafe(Seed));
    return Spawned + Spans;
}

void USFWalkService::CommitActiveAndAdvance()
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> CommitActiveAndAdvance ENTER: bActive=%d segments=%d active=%d"),
        bActive ? 1 : 0, Segments.Num(), ActiveIndex);
    if (!bActive)
    {
        UE_LOG(LogSmartWalk, Log, TEXT("<<< CommitActiveAndAdvance EXIT: not active"));
        return;
    }

    // Commit-on-scale: the active segment freezes; start a new active one that INHERITS the current adjusters
    // (turn/spacing/rise/shift) so the run keeps its settings as you advance — set a 45°/24 m segment once and
    // advance to sweep it, rather than re-dialing every segment (the auto-connect "set params then scale" model).
    // Holograms/belt are per-segment and intentionally not copied.
    FSFWalkSegment NewSeg;
    if (Segments.IsValidIndex(ActiveIndex))
    {
        const FSFWalkSegment& Prev = Segments[ActiveIndex];
        NewSeg.Advance     = Prev.Advance;
        NewSeg.TurnDegrees = Prev.TurnDegrees;
        NewSeg.Rise        = Prev.Rise;
        NewSeg.Shift       = Prev.Shift;
        NewSeg.NumLanes    = Prev.NumLanes;     // inherit the bus cross-section so the run stays uniform
        NewSeg.NumStacks   = Prev.NumStacks;
    }
    else
    {
        NewSeg.NumLanes  = CrossSectionLanes;
        NewSeg.NumStacks = CrossSectionStacks;
    }
    NewSeg.Advance = FMath::Max(NewSeg.Advance, SF_WALK_MIN_ADVANCE_CM);   // safety: never a 0-length new segment
    Segments.Add(NewSeg);
    ActiveIndex = Segments.Num() - 1;
    SpawnSegmentHologram(ActiveIndex);

    UE_LOG(LogSmartWalk, Log, TEXT("<<< CommitActiveAndAdvance EXIT: now %d segments, active=%d (inherited adv=%.0f turn=%.0f rise=%.0f shift=%.0f)"),
        Segments.Num(), ActiveIndex, NewSeg.Advance, NewSeg.TurnDegrees, NewSeg.Rise, NewSeg.Shift);
}

void USFWalkService::BackUp()
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> BackUp ENTER: bActive=%d segments=%d active=%d"),
        bActive ? 1 : 0, Segments.Num(), ActiveIndex);
    if (!bActive || Segments.Num() <= 1)
    {
        UE_LOG(LogSmartWalk, Log, TEXT("<<< BackUp EXIT: nothing to pop (keep >=1 segment)"));
        return;   // keep at least the seed segment
    }

    DestroySegmentHolograms(Segments.Last());
    Segments.Pop();
    ActiveIndex = Segments.Num() - 1;

    UE_LOG(LogSmartWalk, Log, TEXT("<<< BackUp EXIT: now %d segments, active=%d"), Segments.Num(), ActiveIndex);
}

void USFWalkService::SetActiveAdjusters(float Advance, float TurnDegrees, float Rise, float Shift)
{
    if (!bActive || !Segments.IsValidIndex(ActiveIndex))
    {
        return;
    }

    FSFWalkSegment& Seg = Segments[ActiveIndex];
    UE_LOG(LogSmartWalk, Log, TEXT(">>> SetActiveAdjusters ENTER: active=%d old(adv=%.0f turn=%.0f rise=%.0f shift=%.0f) -> new(adv=%.0f turn=%.0f rise=%.0f shift=%.0f)"),
        ActiveIndex, Seg.Advance, Seg.TurnDegrees, Seg.Rise, Seg.Shift, Advance, TurnDegrees, Rise, Shift);
    Seg.Advance = FMath::Max(Advance, SF_WALK_MIN_ADVANCE_CM);   // min 1 m — no 0-length spans
    Seg.TurnDegrees = FMath::Clamp(TurnDegrees, -SF_WALK_MAX_TURN_DEG, SF_WALK_MAX_TURN_DEG);   // up to a 270° loop
    Seg.Rise = Rise;
    Seg.Shift = Shift;

    // Re-derive every frame from this segment forward (forward kinematics).
    RepositionFrom(ActiveIndex);
    UE_LOG(LogSmartWalk, Log, TEXT("<<< SetActiveAdjusters EXIT: repositioned from %d"), ActiveIndex);
}

void USFWalkService::SetSegmentAtIndex(int32 Index, float Advance, float TurnDegrees, float Rise, float Shift)
{
    if (!bActive || !Segments.IsValidIndex(Index))
    {
        return;
    }

    FSFWalkSegment& Seg = Segments[Index];
    Seg.Advance = FMath::Max(Advance, SF_WALK_MIN_ADVANCE_CM);   // min 1 m — no 0-length spans
    Seg.TurnDegrees = FMath::Clamp(TurnDegrees, -SF_WALK_MAX_TURN_DEG, SF_WALK_MAX_TURN_DEG);   // up to a 270° loop
    Seg.Rise = Rise;
    Seg.Shift = Shift;

    // Re-derive + reposition every frame from this segment forward (committed-segment edit → live downstream rebuild).
    RepositionFrom(Index);
    UE_LOG(LogSmartWalk, Log, TEXT("[Walk] SetSegmentAtIndex %d -> adv=%.0f turn=%.0f rise=%.0f shift=%.0f"),
        Index, Advance, TurnDegrees, Rise, Shift);
}

void USFWalkService::NudgeActive(float dAdvance, float dTurn, float dRise, float dShift)
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> NudgeActive ENTER: active=%d deltas(dAdv=%.0f dTurn=%.0f dRise=%.0f dShift=%.0f) bActive=%d"),
        ActiveIndex, dAdvance, dTurn, dRise, dShift, bActive ? 1 : 0);
    if (!bActive || !Segments.IsValidIndex(ActiveIndex))
    {
        UE_LOG(LogSmartWalk, Log, TEXT("<<< NudgeActive EXIT: not active / invalid active index"));
        return;
    }

    FSFWalkSegment& Seg = Segments[ActiveIndex];
    SetActiveAdjusters(Seg.Advance + dAdvance, Seg.TurnDegrees + dTurn, Seg.Rise + dRise, Seg.Shift + dShift);
}

void USFWalkService::AdjustCrossSection(int32 DeltaLanes, int32 DeltaStacks)
{
    if (!bActive)
    {
        return;
    }
    // Replicate GridStateService::ApplyAxisScaling on each SIGNED counter: |val| = count, sign = side, and the
    // forbidden values 0 and -1 are skipped — so the count steps …-3,-2,1,2,3… and scrolling back through center
    // flips the bus to the OTHER side. Same directional behaviour as stackable-pole grid scaling.
    auto GridStep = [](int32 Val, int32 Delta) -> int32
    {
        if (Delta == 0) { return Val; }
        int32 NewVal = Val + Delta;
        if (NewVal == 0 || NewVal == -1) { NewVal = (Delta > 0) ? 1 : -2; }
        return NewVal;
    };
    const int32 OldLanes = CrossSectionLanes;
    const int32 OldStacks = CrossSectionStacks;
    CrossSectionLanes  = GridStep(CrossSectionLanes, DeltaLanes);
    CrossSectionStacks = GridStep(CrossSectionStacks, DeltaStacks);
    UE_LOG(LogSmartWalk, Log, TEXT(">>> AdjustCrossSection: laneCtr %d->%d  stackCtr %d->%d  (d=%d,%d) segments=%d"),
        OldLanes, CrossSectionLanes, OldStacks, CrossSectionStacks, DeltaLanes, DeltaStacks, Segments.Num());

    if (OldLanes == CrossSectionLanes && OldStacks == CrossSectionStacks)
    {
        return;   // no change, skip the rebuild
    }

    // MVP uniform: every segment gets the new cross-section. (Per-segment mode later would set the active one only.)
    for (FSFWalkSegment& Seg : Segments)
    {
        Seg.NumLanes  = CrossSectionLanes;
        Seg.NumStacks = CrossSectionStacks;
    }
    // The pole COUNT changed, so we can't just reposition — destroy + respawn the whole cross-section.
    RebuildHolograms();
}

void USFWalkService::RebuildHolograms()
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> RebuildHolograms: segments=%d cross-section=%dx%d"),
        Segments.Num(), CrossSectionLanes, CrossSectionStacks);
    // The origin cross-section is segment 0's predecessor — rebuild it FIRST so segment 0's belts find its cells.
    RebuildOriginHolograms();
    for (FSFWalkSegment& Seg : Segments)
    {
        DestroySegmentHolograms(Seg);
    }
    // Respawn in order so each segment's predecessor poles already exist when its belts are linked.
    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        SpawnSegmentHologram(i);
    }
    UE_LOG(LogSmartWalk, Log, TEXT("<<< RebuildHolograms done"));
}

void USFWalkService::RebuildOriginHolograms()
{
    AFGHologram* Seed = SeedHologram.Get();

    // Tear down previously-spawned origin cells — but NEVER the seed (cell 0,0), which the build gun owns.
    for (const TWeakObjectPtr<AFGHologram>& WeakHolo : OriginHolograms)
    {
        AFGHologram* Holo = WeakHolo.Get();
        if (Holo && Holo != Seed)
        {
            SF_SafeDestroyHologram(Holo);
        }
    }
    OriginHolograms.Reset();

    if (!IsValid(Seed))
    {
        return;
    }

    // The origin uses the same uniform cross-section as the segments, placed at the SEED frame. Cell (0,0) = seed.
    const int32 Lanes     = FMath::Max(1, FMath::Abs(CrossSectionLanes));
    const int32 Stacks    = FMath::Max(1, FMath::Abs(CrossSectionStacks));
    const int32 LaneSign  = (CrossSectionLanes  >= 1) ? 1 : -1;
    const int32 StackSign = (CrossSectionStacks >= 1) ? 1 : -1;
    OriginHolograms.Reserve(Lanes * Stacks);
    for (int32 LaneRow = 0; LaneRow < Lanes; ++LaneRow)
    {
        for (int32 StackRow = 0; StackRow < Stacks; ++StackRow)
        {
            if (LaneRow == 0 && StackRow == 0)
            {
                OriginHolograms.Add(Seed);   // cell (0,0) IS the held seed pole — don't spawn or own it
                continue;
            }
            const int32 SignedLane  = LaneRow * LaneSign;
            const int32 SignedStack = StackRow * StackSign;
            const FTransform Pose = CrossSectionPose(OriginFrame, SignedLane, SignedStack);
            OriginHolograms.Add(SpawnOnePole(Pose, /*Index*/ -1, SignedLane, SignedStack));
        }
    }
    UE_LOG(LogSmartWalk, Log, TEXT("RebuildOriginHolograms: %d origin cells (cell0=seed=%s)"),
        OriginHolograms.Num(), *GetNameSafe(Seed));
}

FTransform USFWalkService::AccumulateFrame(const TArray<FSFWalkSegment>& Segs, const FTransform& Origin, int32 EndIndex, bool bTrace)
{
    // Reuse the ESTABLISHED grid placement transform (FSFPositionCalculator — the same one foundations, belts,
    // and auto-connect's scale-out use). Each segment is one forward step (X=1) from the running frame: the
    // calculator derives "forward" from the frame's Yaw (the 180-Yaw-90 mapping), so the run extends the exact
    // direction auto-connect would. The Turn then advances the heading via the standard FRotator (advance-then-
    // rotate: the anchor sits one Advance straight ahead, the turn becomes the exit heading for the next leg).
    FSFPositionCalculator Calc;
    FVector Loc = Origin.GetLocation();
    FRotator Rot = Origin.Rotator();
    const int32 Last = FMath::Min(EndIndex, Segs.Num() - 1);
    if (bTrace)
    {
        UE_LOG(LogSmartWalk, Log, TEXT("  [KIN] AccumulateFrame: origin.world=%s yaw=%.1f  endIndex=%d  (resolving to last=%d of %d segs)"),
            *Loc.ToString(), Rot.Yaw, EndIndex, Last, Segs.Num());
    }
    for (int32 i = 0; i <= Last; ++i)
    {
        const FSFWalkSegment& S = Segs[i];

        // One step via the established placement transform. RotationZ selects the mode INSIDE
        // CalculateChildPosition: 0 → straight advance; non-zero → its ARC branch (auto-connect's rotation
        // transform / CalculateRotationOffset), so a Turn bends the run on a smooth curve instead of faceting.
        // ItemSize.X carries the step distance (Advance) — it is both the linear spacing and the arc length.
        const FVector PreLoc = Loc;
        const FRotator PreRot = Rot;
        FSFCounterState CS;
        CS.RotationZ = S.TurnDegrees;
        FVector NewLoc = Calc.CalculateChildPosition(1, 0, 0, Loc, Rot, FVector(S.Advance, 0.0f, 0.0f), CS);
        NewLoc.Z += S.Rise;   // vertical rise (a single step doesn't carry SpacingZ; apply directly)
        // Shift (lateral) is reserved for the single-lane MVP.

        Rot += FRotator(0.0f, S.TurnDegrees, 0.0f);   // advance the heading (exit heading for the next leg)
        Loc = NewLoc;

        if (bTrace)
        {
            UE_LOG(LogSmartWalk, Log, TEXT("  [KIN]   step %d: adv=%.0f turn=%.0f rise=%.0f | preLoc=%s preYaw=%.1f -> postLoc=%s postYaw=%.1f (step delta=%s)"),
                i, S.Advance, S.TurnDegrees, S.Rise, *PreLoc.ToString(), PreRot.Yaw, *Loc.ToString(), Rot.Yaw, *(Loc - PreLoc).ToString());
        }
    }
    if (bTrace)
    {
        UE_LOG(LogSmartWalk, Log, TEXT("  [KIN] AccumulateFrame RESULT: world=%s yaw=%.1f"), *Loc.ToString(), Rot.Yaw);
    }
    return FTransform(Rot, Loc);
}

FTransform USFWalkService::FrameAtIndex(int32 EndIndex, bool bTrace) const
{
    return AccumulateFrame(Segments, OriginFrame, EndIndex, bTrace);
}

FString USFWalkService::RunSelfTest()
{
    // Three straight forward steps (no turn) via the ESTABLISHED placement transform. At Yaw 0 the grid
    // "forward" maps to -X (the 180-Yaw-90 mapping), so the head lands 3*800 back along -X. (Turn/arc bend
    // routes through CalculateChildPosition's rotation branch and is validated in-game.)
    TArray<FSFWalkSegment> Segs;
    FSFWalkSegment S0; S0.Advance = 800.0f; Segs.Add(S0);
    FSFWalkSegment S1; S1.Advance = 800.0f; Segs.Add(S1);
    FSFWalkSegment S2; S2.Advance = 800.0f; Segs.Add(S2);

    const FTransform Head = AccumulateFrame(Segs, FTransform::Identity, 2);
    const FVector P = Head.GetLocation();
    const float Yaw = Head.Rotator().Yaw;

    const FVector ExpectedP(-2400.0f, 0.0f, 0.0f);
    const float ExpectedYaw = 0.0f;
    const bool bPosOk = P.Equals(ExpectedP, 1.0f);
    const bool bYawOk = FMath::IsNearlyEqual(FRotator::NormalizeAxis(Yaw - ExpectedYaw), 0.0f, 0.1f);

    const FString Result = FString::Printf(
        TEXT("%s — head=(%.1f, %.1f, %.1f) yaw=%.1f  | expected=(-2400.0, 0.0, 0.0) yaw=0.0"),
        (bPosOk && bYawOk) ? TEXT("PASS") : TEXT("FAIL"), P.X, P.Y, P.Z, Yaw);

    UE_LOG(LogSmartWalk, Log, TEXT("[SelfTest] %s"), *Result);
    return Result;
}

FTransform USFWalkService::GetHeadFrame() const
{
    return FrameAtIndex(ActiveIndex);
}

TArray<FSFWalkSegmentView> USFWalkService::GetSegmentViews() const
{
    TArray<FSFWalkSegmentView> Views;
    Views.Reserve(Segments.Num());
    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        FSFWalkSegmentView V;
        V.Index = i;
        V.Advance = Segments[i].Advance;
        V.TurnDegrees = Segments[i].TurnDegrees;
        V.Rise = Segments[i].Rise;
        V.Shift = Segments[i].Shift;
        V.ExitHeadingDeg = FrameAtIndex(i).Rotator().Yaw;
        V.bActive = (i == ActiveIndex);
        Views.Add(V);
    }
    return Views;
}

void USFWalkService::RerouteSpans()
{
    if (bActive)
    {
        // Frames/poles are unchanged; RepositionFrom re-invokes the conveyance per segment, which re-reads the
        // (now-changed) belt routing mode and re-routes each span. Cheap no-op on the pole transforms.
        RepositionFrom(0);
    }
}

void USFWalkService::RecreateSpans()
{
    if (!bActive)
    {
        return;
    }
    // A tier change must re-spawn the belts: LinkOrUpdate's UPDATE branch only re-routes geometry on the existing
    // hologram (old Mk class); the belt class is resolved only in the CREATE branch (null ExistingSpan). Destroy
    // every span the same way DestroySegmentHolograms does (reflection-unlink then Destroy), null the slots, then
    // RepositionFrom re-runs UpdateSegmentSpans so each now-null slot takes the CREATE path with the current tier.
    // Poles/frames are unchanged, so only the spans churn.
    for (FSFWalkSegment& Seg : Segments)
    {
        for (const TWeakObjectPtr<AFGHologram>& WeakSpan : Seg.Spans)
        {
            SF_SafeDestroyHologram(WeakSpan.Get());
        }
        Seg.Spans.Reset();
    }
    RepositionFrom(0);
}

bool USFWalkService::CanAffordWalk() const
{
    if (!bActive)
    {
        return true;
    }

    // Player inventory — if we can't resolve it, never block (mirrors Extend's CanAffordExtendCost).
    UFGInventoryComponent* Inventory = nullptr;
    if (Subsystem.IsValid())
    {
        if (AController* Ctrl = Subsystem->GetLastController())
        {
            if (AFGCharacterPlayer* Char = Cast<AFGCharacterPlayer>(Ctrl->GetPawn()))
            {
                Inventory = Char->GetInventory();
            }
        }
    }
    if (!Inventory)
    {
        return true;
    }
    if (Inventory->GetNoBuildCost())
    {
        return true;   // free build (creative / No Build Cost)
    }

    // Sum the cost of every walk hologram (poles + spans + origin cells) EXCLUDING the seed (OriginHolograms[0], which
    // the build gun charges separately). Walk holograms are standalone (not AddChild'd), so sum each one's own GetCost.
    TMap<TSubclassOf<UFGItemDescriptor>, int32> Totals;
    auto AddCost = [&Totals](AFGHologram* H)
    {
        if (!IsValid(H)) { return; }
        for (const FItemAmount& IA : H->GetCost(/*includeChildren=*/false))
        {
            if (IA.ItemClass && IA.Amount > 0)
            {
                Totals.FindOrAdd(IA.ItemClass) += IA.Amount;
            }
        }
    };
    for (int32 i = 1; i < OriginHolograms.Num(); ++i)   // skip [0] = seed
    {
        AddCost(OriginHolograms[i].Get());
    }
    for (const FSFWalkSegment& Seg : Segments)
    {
        for (const TWeakObjectPtr<AFGHologram>& W : Seg.Holograms) { AddCost(W.Get()); }
        for (const TWeakObjectPtr<AFGHologram>& W : Seg.Spans)     { AddCost(W.Get()); }
    }

    AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(GetWorld());
    for (const TPair<TSubclassOf<UFGItemDescriptor>, int32>& Pair : Totals)
    {
        int32 Available = Inventory->GetNumItems(Pair.Key);
        if (CentralStorage)
        {
            Available += CentralStorage->GetNumItemsFromCentralStorage(Pair.Key);
        }
        if (Available < Pair.Value)
        {
            return false;
        }
    }
    return true;
}

void USFWalkService::SyncToSeedTransform()
{
    AFGHologram* Seed = SeedHologram.Get();
    if (!Seed)
    {
        return;
    }

    const FTransform SeedXf = Seed->GetActorTransform();
    if (SeedXf.GetLocation().Equals(OriginFrame.GetLocation(), 1.0f))
    {
        return;   // seed hasn't moved since last frame — nothing to re-anchor
    }

    // Seed was nudged (the build-gun nudge bypasses the position lock) — re-anchor the WHOLE run to it so it moves
    // rigidly with the parent, mirroring how nudging an auto-connect belt pole carries its run.
    OriginFrame = SeedXf;

    // Re-place the origin cross-section cells around the new seed frame (skip cell 0 = seed; it moved itself). Cells are
    // flat-indexed laneRow*|stacks|+stackRow, same as RebuildOriginHolograms.
    const int32 Stacks    = FMath::Max(1, FMath::Abs(CrossSectionStacks));
    const int32 LaneSign  = (CrossSectionLanes  >= 1) ? 1 : -1;
    const int32 StackSign = (CrossSectionStacks >= 1) ? 1 : -1;
    for (int32 i = 0; i < OriginHolograms.Num(); ++i)
    {
        AFGHologram* Holo = OriginHolograms[i].Get();
        if (!IsValid(Holo) || Holo == Seed) { continue; }
        const FTransform Pose = CrossSectionPose(OriginFrame, (i / Stacks) * LaneSign, (i % Stacks) * StackSign);
        Holo->SetActorLocationAndRotation(Pose.GetLocation(), Pose.Rotator());
        Holo->UpdateComponentTransforms();
    }

    // Re-derive + reposition every segment's poles + spans from the new origin.
    RepositionFrom(0);
}

FString USFWalkService::GetSegmentShapeError(int32 Index) const
{
    if (!Segments.IsValidIndex(Index)) { return FString(); }
    const TArray<TWeakObjectPtr<AFGHologram>>& Prev = (Index == 0) ? OriginHolograms : Segments[Index - 1].Holograms;
    const TArray<TWeakObjectPtr<AFGHologram>>& Cur  = Segments[Index].Holograms;
    if (!Prev.IsValidIndex(0) || !Cur.IsValidIndex(0)) { return FString(); }   // no center cell to measure -> can't flag
    AFGHologram* A = Prev[0].Get();
    AFGHologram* B = Cur[0].Get();
    if (!IsValid(A) || !IsValid(B)) { return FString(); }
    // Shared span SHAPE rules (belt/pipe/hyper). Walk POLICY: any invalid segment blocks the commit
    // (see HasInvalidSegmentShape). Belt gets the 30deg slope gate; pipe is exempt (routes at any angle).
    const SFConveyanceShape::EKind Kind = (GetConveyanceType() == ESFWalkConveyanceType::Pipe)
        ? SFConveyanceShape::EKind::Pipe : SFConveyanceShape::EKind::Belt;
    return SFConveyanceShape::EvaluateSpan(A->GetActorLocation(), B->GetActorLocation(), Kind,
        USFAutoConnectService::MAX_PIPE_LENGTH, Index + 1);
}

bool USFWalkService::IsSegmentShapeValid(int32 Index) const
{
    return GetSegmentShapeError(Index).IsEmpty();
}

bool USFWalkService::HasInvalidSegmentShape() const
{
    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        if (!IsSegmentShapeValid(i)) { return true; }
    }
    return false;
}

FString USFWalkService::GetInvalidShapeReason() const
{
    for (int32 i = 0; i < Segments.Num(); ++i)
    {
        const FString Err = GetSegmentShapeError(i);
        if (!Err.IsEmpty()) { return Err; }
    }
    return FString();
}

void USFWalkService::RefreshWalkValidity()
{
    if (!bActive)
    {
        return;
    }

    SyncToSeedTransform();   // nudge-the-parent: keep the run anchored to the (possibly nudged) seed
    // Standalone walk holograms aren't build-gun-ticked, so the FGCDInitializing disqualifier added during init is never
    // cleared and they'd render RED. Clear it every frame, THEN set the material state by AFFORDABILITY: HMS_OK (cyan)
    // if the player can afford the whole walk, HMS_ERROR (red) if not — so a broke walk shows red instead of silently
    // building nothing and clearing. (Order matters: clear disqualifiers FIRST, then material state.)
    const bool bAfford = CanAffordWalk();
    const EHologramMaterialState State = bAfford ? EHologramMaterialState::HMS_OK : EHologramMaterialState::HMS_ERROR;

    bool bAnyInvalidShape = false;
    for (int32 SegIdx = 0; SegIdx < Segments.Num(); ++SegIdx)
    {
        const FSFWalkSegment& Seg = Segments[SegIdx];
        // Invalid SHAPE = span too long (>56m) or, for belts, too steep (>30deg from horizontal). Red that segment's
        // poles AND span (mirrors the auto-connect length/slope restrictions; pipes skip the slope cap). A too-long
        // segment has no span (LinkOrUpdate skips it), but its poles still red so the gap reads as invalid, not missing.
        const bool bShapeValid = IsSegmentShapeValid(SegIdx);
        if (!bShapeValid) { bAnyInvalidShape = true; }
        const EHologramMaterialState SegState = (bAfford && bShapeValid) ? EHologramMaterialState::HMS_OK : EHologramMaterialState::HMS_ERROR;
        for (const TWeakObjectPtr<AFGHologram>& WeakHolo : Seg.Holograms)
        {
            if (AFGHologram* Holo = WeakHolo.Get())
            {
                Holo->ResetConstructDisqualifiers();
                Holo->SetPlacementMaterialState(SegState);
            }
        }
        for (const TWeakObjectPtr<AFGHologram>& WeakBelt : Seg.Spans)
        {
            if (AFGHologram* Belt = WeakBelt.Get())
            {
                Belt->ResetConstructDisqualifiers();
                Belt->SetPlacementMaterialState(SegState);
            }
        }
    }
    // Origin cross-section cells too — but skip cell 0 (the seed); handled below.
    AFGHologram* SeedH = SeedHologram.Get();
    for (const TWeakObjectPtr<AFGHologram>& WeakHolo : OriginHolograms)
    {
        if (AFGHologram* Holo = WeakHolo.Get())
        {
            if (Holo == SeedH) { continue; }
            Holo->ResetConstructDisqualifiers();
            Holo->SetPlacementMaterialState(State);
        }
    }

    // The SEED (parent) is the build gun's own hologram — it only knows its own 1-pole cost, so it stays cyan even when
    // the TOTAL walk is unaffordable. Force it red too when broke (parent AND children red); when affordable, leave it
    // to the build gun's own validation so we don't fight it.
    if ((!bAfford || bAnyInvalidShape) && SeedH)
    {
        SeedH->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
    }
}

void USFWalkService::SpawnSegmentHologram(int32 Index)
{
    if (!Segments.IsValidIndex(Index))
    {
        return;
    }

    AFGHologram* Seed = SeedHologram.Get();
    if (!IsValid(Seed) || !Seed->GetRecipe())
    {
        UE_LOG(LogSmartWalk, Warning, TEXT("SpawnSegmentHologram[%d]: seed hologram/recipe invalid"), Index);
        return;
    }

    FSFWalkSegment& Seg = Segments[Index];
    // Signed cross-section counters (grid convention): |N| = count, sign = side. Rows are 0..count-1; the SIGNED
    // index fed to placement = row * sign, so +Y counter fans right, -Y fans left (one side per scroll).
    const int32 Lanes     = FMath::Max(1, FMath::Abs(Seg.NumLanes));
    const int32 Stacks    = FMath::Max(1, FMath::Abs(Seg.NumStacks));
    const int32 LaneSign  = (Seg.NumLanes  >= 1) ? 1 : -1;
    const int32 StackSign = (Seg.NumStacks >= 1) ? 1 : -1;
    const FTransform Center = FrameAtIndex(Index, /*bTrace*/ true);
    UE_LOG(LogSmartWalk, Log, TEXT(">>> SpawnSegmentHologram[%d] ENTER: cross-section %dx%d (lanesCtr=%d stacksCtr=%d) center.world=%s yaw=%.1f"),
        Index, Lanes, Stacks, Seg.NumLanes, Seg.NumStacks, *Center.GetLocation().ToString(), Center.Rotator().Yaw);

    // Spawn the full lane×stack cross-section at this segment's center frame. Flat index = laneRow*Stacks + stackRow.
    Seg.Holograms.Reset();
    Seg.Holograms.Reserve(Lanes * Stacks);
    for (int32 LaneRow = 0; LaneRow < Lanes; ++LaneRow)
    {
        for (int32 StackRow = 0; StackRow < Stacks; ++StackRow)
        {
            const int32 SignedLane  = LaneRow * LaneSign;
            const int32 SignedStack = StackRow * StackSign;
            const FTransform Pose = CrossSectionPose(Center, SignedLane, SignedStack);
            Seg.Holograms.Add(SpawnOnePole(Pose, Index, SignedLane, SignedStack));   // null kept to preserve flat indexing
        }
    }

    UE_LOG(LogSmartWalk, Log, TEXT("<<< SpawnSegmentHologram[%d] EXIT: spawned %d poles"), Index, Seg.Holograms.Num());

    // Link this segment's cross-section to its predecessor's with one belt per cell.
    UpdateSegmentSpans(Index);
}

AFGHologram* USFWalkService::SpawnOnePole(const FTransform& Pose, int32 Index, int32 Lane, int32 Stack)
{
    AFGHologram* Seed = SeedHologram.Get();
    if (!IsValid(Seed))
    {
        return nullptr;
    }
    TSubclassOf<UFGRecipe> Recipe = Seed->GetRecipe();
    AActor* BuildGunOwner = Seed->GetOwner();
    APawn* HologramInstigator = Cast<APawn>(Seed->GetInstigator());

    // Standalone spawn (NOT a grid child, NOT AddChild'd) — world-anchored, WalkSegment-tagged so the grid sweep
    // skips it; kept cyan by the per-frame RefreshWalkValidity (the build gun never ticks it). AddChild is avoided
    // deliberately: it re-bases the pole to the far seed (horizon) AND the build gun's ResetConstructDisqualifiers
    // recursion crashes on the stale mChildren entry when the pole is Destroy()'d (no public RemoveChild).
    AFGHologram* Holo = AFGHologram::SpawnHologramFromRecipe(
        Recipe, BuildGunOwner ? BuildGunOwner : Seed, Pose.GetLocation(), HologramInstigator);
    if (!IsValid(Holo))
    {
        UE_LOG(LogSmartWalk, Warning, TEXT("  SpawnOnePole seg%d L%d S%d: SpawnHologramFromRecipe returned null"), Index, Lane, Stack);
        return nullptr;
    }

    Holo->SetActorLocationAndRotation(Pose.GetLocation(), Pose.Rotator());
    Holo->UpdateComponentTransforms();   // refresh connector world transforms so the belt routes to the right spots
    USFHologramDataService::DisableValidation(Holo);
    USFHologramDataService::MarkAsChild(Holo, Seed, ESFChildHologramType::WalkSegment);
    Holo->SetActorHiddenInGame(false);
    Holo->SetActorEnableCollision(false);
    Holo->SetActorTickEnabled(false);
    Holo->RegisterAllComponents();
    Holo->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
    Holo->ForceNetUpdate();
    SF_LogPlacement(*FString::Printf(TEXT("seg%d L%d S%d"), Index, Lane, Stack), Index, Holo, Pose, Seed);
    return Holo;
}

FTransform USFWalkService::CrossSectionPose(const FTransform& Center, int32 SignedLane, int32 SignedStack) const
{
    // SignedLane/SignedStack are DIRECTIONAL indices (the caller derives them from the segment's signed counter):
    // +N = N lanes to the right of center, -N = N to the left — exactly the grid's signed-axis convention, so
    // Scale-Y grows ONE side per scroll (no forced mirror). Cell (0,0) = the seed's line (path centerline).
    if (SignedLane == 0 && SignedStack == 0)
    {
        return Center;
    }
    // Reuse the established placement offsets: X=0 (already advanced to Center), Y=SignedLane (lateral, perpendicular
    // to the heading), Z=SignedStack (vertical). ItemSize carries the per-cell spacing; CS default → RotationZ=0 →
    // linear (the turn is already baked into Center). Rotation stays the segment heading so every lane faces down-path.
    FSFPositionCalculator Calc;
    FSFCounterState CS;
    const FVector ItemSize(0.0f, WALK_LANE_SPACING, WALK_STACK_SPACING);
    const FVector Loc = Calc.CalculateChildPosition(0, SignedLane, SignedStack, Center.GetLocation(), Center.Rotator(), ItemSize, CS);
    return FTransform(Center.Rotator(), Loc);
}

AFGHologram* USFWalkService::PredecessorAnchorAt(int32 Index, int32 PoleIndex) const
{
    if (Index <= 0)
    {
        // Segment 0's backward link is the ORIGIN cross-section (cell 0 = the seed; the rest are walk-spawned),
        // so the bus connects to a full-width origin from the very first set.
        return OriginHolograms.IsValidIndex(PoleIndex) ? OriginHolograms[PoleIndex].Get() : nullptr;
    }
    if (Segments.IsValidIndex(Index - 1) && Segments[Index - 1].Holograms.IsValidIndex(PoleIndex))
    {
        return Segments[Index - 1].Holograms[PoleIndex].Get();
    }
    return nullptr;
}

void USFWalkService::UpdateSegmentSpans(int32 Index)
{
    if (!Conveyance || !Segments.IsValidIndex(Index))
    {
        return;
    }
    FSFWalkSegment& Seg = Segments[Index];
    const int32 Count = Seg.Holograms.Num();
    Seg.Spans.SetNum(Count);   // one span slot (belt or pipe) per cross-section cell, same flat index as Holograms

    int32 Made = 0;
    for (int32 PoleIdx = 0; PoleIdx < Count; ++PoleIdx)
    {
        AFGHologram* ToAnchor = Seg.Holograms[PoleIdx].Get();
        AFGHologram* FromAnchor = PredecessorAnchorAt(Index, PoleIdx);
        if (!IsValid(ToAnchor) || !IsValid(FromAnchor))
        {
            continue;   // missing pole (e.g. a failed spawn) — skip this cell's belt this pass
        }
        AFGHologram* OldSpan = Seg.Spans[PoleIdx].Get();
        AFGHologram* NewSpan = Conveyance->LinkOrUpdate(OldSpan, FromAnchor, ToAnchor, SeedHologram.Get(), false, Seg.TurnDegrees);
        if (!NewSpan && IsValid(OldSpan))
        {
            // LinkOrUpdate dropped the span (segment too long for one belt/pipe, > MAX_PIPE_LENGTH) — destroy the stale
            // one so it doesn't linger at the old geometry. Mirrors stackable auto-connect leaving a gap when too far.
            SF_SafeDestroyHologram(OldSpan);
        }
        Seg.Spans[PoleIdx] = NewSpan;
        if (NewSpan) { ++Made; }
    }
    UE_LOG(LogSmartWalk, Log, TEXT("  UpdateSegmentSpans[%d]: %d span(s) over %d cells"), Index, Made, Count);
}

void USFWalkService::RepositionFrom(int32 StartIndex)
{
    const int32 From = FMath::Max(0, StartIndex);
    UE_LOG(LogSmartWalk, Log, TEXT(">>> RepositionFrom ENTER: startIndex=%d (clamped=%d) segments=%d"),
        StartIndex, From, Segments.Num());
    for (int32 i = From; i < Segments.Num(); ++i)
    {
        FSFWalkSegment& Seg = Segments[i];
        const int32 Stacks    = FMath::Max(1, FMath::Abs(Seg.NumStacks));
        const int32 LaneSign  = (Seg.NumLanes  >= 1) ? 1 : -1;
        const int32 StackSign = (Seg.NumStacks >= 1) ? 1 : -1;
        const FTransform Center = FrameAtIndex(i, /*bTrace*/ true);
        for (int32 PoleIdx = 0; PoleIdx < Seg.Holograms.Num(); ++PoleIdx)
        {
            if (AFGHologram* Holo = Seg.Holograms[PoleIdx].Get())
            {
                const FTransform Pose = CrossSectionPose(Center, (PoleIdx / Stacks) * LaneSign, (PoleIdx % Stacks) * StackSign);
                Holo->SetActorLocationAndRotation(Pose.GetLocation(), Pose.Rotator());
                Holo->UpdateComponentTransforms();   // refresh connectors after the move (no re-register here)
            }
        }
        // Re-route this segment's belts; iterating in order means the predecessor poles are already repositioned.
        UpdateSegmentSpans(i);
    }
    UE_LOG(LogSmartWalk, Log, TEXT("<<< RepositionFrom EXIT"));
}

void USFWalkService::DestroySegmentHolograms(FSFWalkSegment& Segment)
{
    UE_LOG(LogSmartWalk, Log, TEXT("  DestroySegmentHolograms: belts=%d holos=%d"),
        Segment.Spans.Num(), Segment.Holograms.Num());
    for (const TWeakObjectPtr<AFGHologram>& WeakBelt : Segment.Spans)
    {
        SF_SafeDestroyHologram(WeakBelt.Get());   // reflection-unlink then Destroy (mirror grid's safe teardown)
    }
    Segment.Spans.Reset();

    for (const TWeakObjectPtr<AFGHologram>& WeakHolo : Segment.Holograms)
    {
        SF_SafeDestroyHologram(WeakHolo.Get());
    }
    Segment.Holograms.Reset();
}

void USFWalkService::ClearAll()
{
    UE_LOG(LogSmartWalk, Log, TEXT(">>> ClearAll ENTER: segments=%d originCells=%d"), Segments.Num(), OriginHolograms.Num());
    for (FSFWalkSegment& Seg : Segments)
    {
        DestroySegmentHolograms(Seg);
    }
    Segments.Reset();
    // Tear down the spawned origin cells — but NOT the seed (cell 0,0), which the build gun owns.
    AFGHologram* Seed = SeedHologram.Get();
    for (const TWeakObjectPtr<AFGHologram>& WeakHolo : OriginHolograms)
    {
        AFGHologram* Holo = WeakHolo.Get();
        if (Holo && Holo != Seed)
        {
            SF_SafeDestroyHologram(Holo);
        }
    }
    OriginHolograms.Reset();
    ActiveIndex = INDEX_NONE;
    UE_LOG(LogSmartWalk, Log, TEXT("<<< ClearAll EXIT"));
}

// ============================================================================
// Test scaffolding (Slice 0): console commands to drive the walk without the
// Slice 4 panel/HUD. Removed/replaced when the Walk widget lands.
//   Smart.Walk.Toggle   — enter/exit walk mode on the held hologram
//   Smart.Walk.Advance  — commit the active segment and start a new one
//   Smart.Walk.BackUp   — destructive back-up of the active segment
// ============================================================================

static void SF_WalkToggleCmd(UWorld* World)
{
    if (USFSubsystem* S = USFSubsystem::Get(World))
    {
        S->ToggleWalkMode();
    }
}

static void SF_WalkAdvanceCmd(UWorld* World)
{
    if (USFSubsystem* S = USFSubsystem::Get(World))
    {
        if (USFWalkService* W = S->GetWalkService())
        {
            W->CommitActiveAndAdvance();
        }
    }
}

static void SF_WalkBackUpCmd(UWorld* World)
{
    if (USFSubsystem* S = USFSubsystem::Get(World))
    {
        if (USFWalkService* W = S->GetWalkService())
        {
            W->BackUp();
        }
    }
}

static FAutoConsoleCommandWithWorld GSFWalkToggle(
    TEXT("Smart.Walk.Toggle"), TEXT("Smart Walking: enter/exit walk mode on the held hologram"),
    FConsoleCommandWithWorldDelegate::CreateStatic(&SF_WalkToggleCmd));

static FAutoConsoleCommandWithWorld GSFWalkAdvance(
    TEXT("Smart.Walk.Advance"), TEXT("Smart Walking: commit the active segment and start a new one"),
    FConsoleCommandWithWorldDelegate::CreateStatic(&SF_WalkAdvanceCmd));

static FAutoConsoleCommandWithWorld GSFWalkBackUp(
    TEXT("Smart.Walk.BackUp"), TEXT("Smart Walking: destructive back-up of the active segment"),
    FConsoleCommandWithWorldDelegate::CreateStatic(&SF_WalkBackUpCmd));

static void SF_WalkTurnCmd(const TArray<FString>& Args, UWorld* World)
{
    const float Deg = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 15.0f;
    if (USFSubsystem* S = USFSubsystem::Get(World))
    {
        S->WalkNudgeActive(0.0f, Deg, 0.0f, 0.0f);
    }
}

static void SF_WalkRiseCmd(const TArray<FString>& Args, UWorld* World)
{
    const float Cm = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 200.0f;
    if (USFSubsystem* S = USFSubsystem::Get(World))
    {
        S->WalkNudgeActive(0.0f, 0.0f, Cm, 0.0f);
    }
}

static FAutoConsoleCommandWithWorldAndArgs GSFWalkTurn(
    TEXT("Smart.Walk.Turn"), TEXT("Smart Walking: turn the active segment by <degrees> (right = positive; default 15)"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SF_WalkTurnCmd));

static FAutoConsoleCommandWithWorldAndArgs GSFWalkRise(
    TEXT("Smart.Walk.Rise"), TEXT("Smart Walking: raise the active segment by <cm> (default 200)"),
    FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SF_WalkRiseCmd));
