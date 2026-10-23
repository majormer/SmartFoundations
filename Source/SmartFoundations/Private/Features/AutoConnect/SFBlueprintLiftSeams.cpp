// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintLiftSeams.h"
#include "Features/AutoConnect/SFBlueprintLiftValidation.h"
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/Preview/BeltPreviewHelper.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Hologram/FGBlueprintHologram.h"
#include "FGConstructDisqualifier.h"
#include "FGCharacterPlayer.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — Blueprint lift-tail alignment
// [MP-CLIENT] Align the priced preview's complete copies and publish normal spacing.
// Authority receives the resulting cell basis; exact seam preparation is separate.
namespace
{
struct FAlignmentState
{
    FString Key;
    FString BlueprintName;
    FString Verdict;
    int32 BaselineSpacing = 0;
    TOptional<int32> AppliedSpacing;
    bool bInitialized = false;
    bool bApplying = false;
    bool bManualSpacing = false;
};
TMap<TWeakObjectPtr<AFGBlueprintHologram>, FAlignmentState> LiftAlignmentStates;
const AFGHologram* NativeValidation = nullptr;
}

bool FSFBlueprintLiftSeams::IsNativeValidation(const AFGHologram* Hologram)
{
    return Hologram && NativeValidation == Hologram;
}

bool FSFBlueprintLiftSeams::ValidateCopy(AFGBlueprintHologram* Copy)
{
    // Validate the complete copy itself, independently of its visual child tree.
    // Restore the pre-probe placement state and ownership exactly on every exit.
    auto& Failure = SFBlueprintLiftValidation::LastCopyFailure();
    Failure.Reset();
    if (!IsValid(Copy)) { Failure = TEXT("invalid copy"); return false; }
    const auto Disqualifiers = Copy->mConstructDisqualifiers;
    const auto Material = Copy->GetHologramMaterialState();
    auto Children = MoveTemp(Copy->mChildren);
    const AFGHologram* Previous = NativeValidation;
    NativeValidation = Copy;
    ON_SCOPE_EXIT
    {
        NativeValidation = Previous;
        Copy->mChildren = MoveTemp(Children);
        Copy->mConstructDisqualifiers = Disqualifiers;
        Copy->SetPlacementMaterialState(Material);
    };
    Copy->ResetConstructDisqualifiers();
    static_cast<AFGHologram*>(Copy)->CheckValidPlacement();
    Copy->mConstructDisqualifiers.Remove(UFGCDInitializing::StaticClass());
    if (!Copy->CanConstruct())
    {
        TArray<FString> Reasons;
        for (const auto& D : Copy->mConstructDisqualifiers) Reasons.Add(GetNameSafe(D.Get()));
        Failure = FString::Printf(TEXT("copy=%s parent=%s reasons=%s material=%d locked=%d"),
            *Copy->GetName(), *GetNameSafe(Copy->GetParentHologram()), *FString::Join(Reasons, TEXT(",")),
            static_cast<int32>(Copy->GetHologramMaterialState()), Copy->IsHologramLocked());
        return false;
    }
    // Pending copies are holograms, so the ordinary world overlap query may
    // not see their collision-disabled contents. Use the native clearance
    // arbiter between their complete, transformed clearance sets as well.
    if (auto* Parent = Cast<AFGBlueprintHologram>(Copy->GetParentHologram()))
    {
        TArray<AFGHologram*> Peers = Parent->GetHologramChildren();
        Peers.Add(Parent);
        TArray<const FFGClearanceData*> OurData;
        static_cast<AFGHologram*>(Copy)->GetClearanceData(OurData);
        auto WorldBounds = [](const TArray<const FFGClearanceData*>& Data, const FTransform& Pose)
        {
            FBox Box(ForceInit);
            for (const auto* D : Data) if (D && D->IsValid())
                Box += D->ClearanceBox.TransformBy(D->RelativeTransform * Pose);
            return Box;
        };
        const FBox OurBounds = WorldBounds(OurData, Copy->GetActorTransform());
        for (auto* Hologram : Peers)
        {
            auto* Peer = Cast<AFGBlueprintHologram>(Hologram);
            FIntVector Cell;
            if (!Peer || Peer == Copy || (Peer != Parent && !USFGridCoordComponent::TryGetCell(Peer, Cell))) continue;
            TArray<const FFGClearanceData*> OtherData;
            static_cast<AFGHologram*>(Peer)->GetClearanceData(OtherData);
            const FBox OtherBounds = WorldBounds(OtherData, Peer->GetActorTransform());
            if (!OurBounds.IsValid || !OtherBounds.IsValid || !OurBounds.Intersect(OtherBounds)) continue;
            for (const auto* Ours : OurData) for (const auto* Other : OtherData)
                if (Ours && Other && static_cast<AFGHologram*>(Copy)->TestClearanceOverlap(*Ours, *Other, Peer)
                    == EClearanceOverlapResult::COR_Block)
                {
                    Failure = FString::Printf(TEXT("copy=%s hard native clearance against=%s"), *Copy->GetName(), *Peer->GetName());
                    return false;
                }
        }
    }
    return true;
}

void FSFBlueprintLiftSeams::RetireProbe(AFGBlueprintHologram* Root, AFGHologram* Probe)
{
    if (!Probe) return;
    Probe->SetDisabled(true);
    Probe->SetActorTickEnabled(false);
    Root->mChildren.Remove(Probe);
    for (auto It = Root->mChildrenNameLookupMap.CreateIterator(); It; ++It)
        if (It.Value() == Probe) It.RemoveCurrent();
}

FString FSFBlueprintLiftSeams::Describe(AFGBlueprintHologram* Root)
{
    const auto* State = LiftAlignmentStates.Find(Root);
    return State ? State->Verdict : TEXT("no lift-tail alignment evaluated");
}

bool FSFBlueprintLiftSeams::Align(AFGBlueprintHologram* Root, USFSubsystem* Subsystem, USFAutoConnectService* Service)
{
    if (!Root || !Subsystem || !Service || !Subsystem->GetHologramHelper()) return false;
    auto* Initiator = Root->GetConstructionInstigator();
    if (Subsystem->GetActiveHologram() != Root || !Initiator || !Initiator->IsLocallyControlled()) return false;
    for (auto It = LiftAlignmentStates.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    auto& State = LiftAlignmentStates.FindOrAdd(Root);
    if (State.bApplying) return false;
    TGuardValue<bool> Applying(State.bApplying, true);
    FSFCounterState Counters = Subsystem->GetCounterState();
    const auto* NameProperty = FindFProperty<FStrProperty>(Root->GetClass(), TEXT("mBlueprintDescName"));
    const FString BlueprintName = NameProperty ? NameProperty->GetPropertyValue_InContainer(Root) : Root->GetName();
    if (State.bInitialized && State.BlueprintName != BlueprintName)
    {
        const bool RestoreAutomatic = State.AppliedSpacing.IsSet() && Counters.SpacingZ == State.AppliedSpacing.GetValue();
        if (RestoreAutomatic) Counters.SpacingZ = State.BaselineSpacing;
        State.Key.Reset(); State.AppliedSpacing.Reset(); State.bManualSpacing = false;
        State.BaselineSpacing = Counters.SpacingZ; State.BlueprintName = BlueprintName;
        if (RestoreAutomatic)
        {
            Subsystem->UpdateCounterState(Counters);
            return true;
        }
    }
    if (!State.bInitialized)
    {
        State.BaselineSpacing = Counters.SpacingZ;
        State.BlueprintName = BlueprintName;
        State.bInitialized = true;
    }
    const bool AutomaticSpacing = State.AppliedSpacing.IsSet() ? Counters.SpacingZ == State.AppliedSpacing.GetValue()
        : Counters.SpacingZ == State.BaselineSpacing;
    if (!AutomaticSpacing)
    {
        State.BaselineSpacing = Counters.SpacingZ;
        State.bManualSpacing = true;
    }
    FSFBlueprintSeamService::EnsureLiftTailConnectors(Root);
    TArray<FSFBlueprintLiftAlignment::FTail> Tails;
    FSFBlueprintSeamService::GetOpenLiftTails(Root, Tails);
    const auto* BeltDefaults = GetDefault<ASFConveyorBeltHologram>();
    const double MaximumLength = BeltDefaults->GetMaxSplineLength();
    const auto Pairing = FSFBlueprintLiftAlignment::PairTails(Tails,
        FSFBlueprintSeamService::LANE_MATCH_TOLERANCE, MaximumLength);

    FString Key;
    FSFCounterState::StaticStruct()->ExportText(Key, &Counters, nullptr, nullptr, PPF_None, nullptr);
    Key += Root->GetActorTransform().ToString() + Subsystem->GetCachedBuildingSize().ToString();
    for (const auto& Tail : Tails)
    {
        Key += FString::FromInt(Tail.Index) + Tail.Position.ToString() + Tail.Normal.ToString() + FString::FromInt(Tail.bOutput);
        if (const auto* Original = FSFBlueprintSeamService::GetOriginalBeltConnector(Root, Tail.Index))
            Key += Original->GetPathName() + Original->GetOwner()->GetActorTransform().ToString();
    }
    const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
    Key += FString::Printf(TEXT("tier=%d routing=%d"), Settings.BeltTierMain, Settings.BeltRoutingMode);
    TMap<FIntVector, AFGBlueprintHologram*> Copies;
    Copies.Add(FIntVector::ZeroValue, Root);
    for (auto* Child : Root->GetHologramChildren())
    {
        FIntVector Cell;
        if (auto* Copy = Cast<AFGBlueprintHologram>(Child); Copy && USFGridCoordComponent::TryGetCell(Copy, Cell))
        {
            FSFBlueprintSeamService::EnsureLiftTailConnectors(Copy);
            Copies.Add(Cell, Copy);
            Key += Cell.ToString() + Copy->GetActorTransform().ToString();
        }
    }
    if (State.Key == Key) return false;
    State.Key = Key;
    auto RestoreBaseline = [&](const TCHAR* Verdict)
    {
        State.Verdict = FString(Verdict) + TEXT("; ") + FSFBlueprintSeamService::DescribeLiftSources(Root);
        if (State.AppliedSpacing.IsSet() && Counters.SpacingZ == State.AppliedSpacing.GetValue()
            && Counters.SpacingZ != State.BaselineSpacing)
        {
            Counters.SpacingZ = State.BaselineSpacing;
            State.AppliedSpacing.Reset();
            Subsystem->UpdateCounterState(Counters);
            return true;
        }
        return false;
    };
    if (FMath::Abs(Counters.GridCounters.Z) < 2 || Pairing.Pairs.IsEmpty())
        return RestoreBaseline(Pairing.bAmbiguous ? TEXT("ambiguous lift mouths; default spacing retained")
            : Pairing.bInvalid ? TEXT("invalid lift mouths; default spacing retained") : TEXT("no vertical lift-tail pair; default spacing retained"));

    const auto* InclineProperty = FindFProperty<FFloatProperty>(AFGConveyorBeltHologram::StaticClass(), TEXT("mMaxIncline"));
    const double Incline = InclineProperty ? InclineProperty->GetPropertyValue_InContainer(BeltDefaults) : 0.0;
    if (!FMath::IsFinite(Incline) || Incline <= 0.0 || Incline >= 90.0)
        return RestoreBaseline(TEXT("native belt incline unavailable; default spacing retained"));
    TArray<FSFBlueprintLiftAlignment::FPitchRange> Ranges;
    for (const auto& Pair : Pairing.Pairs)
    {
        const auto* Output = Tails.FindByPredicate([&](const auto& T) { return T.Index == Pair.Output; });
        const auto* Input = Tails.FindByPredicate([&](const auto& T) { return T.Index == Pair.Input; });
        if (!Output || !Input) return RestoreBaseline(TEXT("lift mouth identity changed"));
        const double Reach = FVector::Dist2D(Output->Position, Input->Position);
        // Necessary chord bounds only. Every accepted pitch still runs native
        // routed shape, clearance and complete-copy placement validation below.
        const double Rise = FSFBlueprintLiftAlignment::MaximumVerticalReach(MaximumLength, Reach, Incline);
        Ranges.Add({FMath::Max(.1, Pair.LevelPitch - Rise), Pair.LevelPitch + Rise, Pair.LevelPitch});
    }
    const double Height = Subsystem->GetCachedBuildingSize().Z;
    const double CurrentPitch = Height + Counters.SpacingZ;
    const double DefaultPitch = Height + State.BaselineSpacing;
    int32 Tier = Settings.BeltTierMain;
    if (Tier == 0) Tier = Subsystem->GetHighestUnlockedBeltTier(Subsystem->GetLastController());
    if (Tier < 1 || Tier > 6) return RestoreBaseline(TEXT("no unlocked belt tier"));
    TArray<TSharedPtr<FBeltPreviewHelper>> Probes;
    ON_SCOPE_EXIT
    {
        for (auto& Probe : Probes) if (Probe)
        {
            RetireProbe(Root, Probe->GetHologram());
            Probe->DestroyPreview();
        }
    };
    auto NativeLayoutValid = [&](double Pitch)
    {
        // Match the existing one-decimal-metre Panel and integer-cm state.
        const int32 Spacing = FMath::RoundToInt((Pitch - Height) / 10.0) * 10;
        Pitch = Height + Spacing;
        if (Pitch <= 0.0) return false;
        TMap<AFGBlueprintHologram*, FTransform> Before;
        ON_SCOPE_EXIT
        {
            for (const auto& Copy : Before)
            {
                Copy.Key->SetActorTransform(Copy.Value);
            }
            for (auto& Probe : Probes) if (Probe && Probe->GetHologram()) Probe->GetHologram()->SetDisabled(true);
        };
        for (const auto& Cell : Copies) if (Cell.Key != FIntVector::ZeroValue)
        {
            auto* Copy = Cell.Value;
            Before.Add(Copy, Copy->GetActorTransform());
            const double Advance = Cell.Key.Z * (Counters.GridCounters.Z < 0 ? -1.0 : 1.0);
            const FVector Target = Before[Copy].GetLocation() + FVector(0, 0, Advance * (Pitch - CurrentPitch));
            Copy->SetActorLocation(Target);
            if (!Copy->GetActorLocation().Equals(Target, .1)) return false;
        }
        for (const auto& Cell : Copies)
            if (Cell.Key != FIntVector::ZeroValue && !ValidateCopy(Cell.Value)) return false;
        int32 ProbeIndex = 0;
        for (const auto& Cell : Copies)
        {
            if (Cell.Key.Z == 0) continue;
            auto* const* Neighbor = Copies.Find(Cell.Key - FIntVector(0, 0, 1));
            if (!Neighbor) return false;
            auto* Lower = Cell.Value->GetActorLocation().Z < (*Neighbor)->GetActorLocation().Z ? Cell.Value : *Neighbor;
            auto* Upper = Lower == Cell.Value ? *Neighbor : Cell.Value;
            for (const auto& Pair : Pairing.Pairs)
            {
                const auto* OriginalOut = FSFBlueprintSeamService::GetOriginalBeltConnector(Root, Pair.Output);
                const auto* OriginalIn = FSFBlueprintSeamService::GetOriginalBeltConnector(Root, Pair.Input);
                if (!OriginalOut || !OriginalIn) return false;
                auto* From = FSFBlueprintSeamService::ResolveBeltConnector(Pair.bOutputInLowerCopy ? Lower : Upper, Pair.Output, OriginalOut->GetFName());
                auto* To = FSFBlueprintSeamService::ResolveBeltConnector(Pair.bOutputInLowerCopy ? Upper : Lower, Pair.Input, OriginalIn->GetFName());
                if (Probes.Num() <= ProbeIndex) Probes.Add(MakeShared<FBeltPreviewHelper>(Root->GetWorld(), Tier, Root));
                auto& Probe = Probes[ProbeIndex++];
                if (Probe && Probe->GetHologram()) Probe->GetHologram()->SetDisabled(false);
                auto* PreviousProbe = Probe ? Probe->GetHologram() : nullptr;
                if (!Service->CreateOrUpdateBeltPreview(From, To, Probe, USFAutoConnectService::FACING_SANITY_ANGLE, false, Root))
                {
                    RetireProbe(Root, PreviousProbe);
                    return false;
                }
                if (auto* Belt = Probe ? Probe->GetTypedHologram() : nullptr)
                {
                    Belt->SetActorHiddenInGame(true);
                    Belt->SetActorTickEnabled(false);
                }
            }
        }
        return ProbeIndex > 0;
    };
    // Explicit player spacing remains usable when native validation permits it.
    if (State.bManualSpacing && NativeLayoutValid(CurrentPitch))
    { State.AppliedSpacing.Reset(); State.Verdict = TEXT("native-valid explicit Z spacing retained"); return false; }
    const auto Result = FSFBlueprintLiftAlignment::SolveSharedPitch(DefaultPitch, Ranges, NativeLayoutValid);
    if (Result.Status != FSFBlueprintLiftAlignment::EStatus::Aligned)
        return RestoreBaseline(TEXT("no common native-valid lift-tail pitch; default spacing retained"));
    const int32 Spacing = FMath::RoundToInt((Result.Pitch - Height) / 10.0) * 10;
    State.AppliedSpacing = Spacing;
    State.bManualSpacing = false;
    State.Verdict = FString::Printf(TEXT("shared native-valid pitch=%.1f cm pairs=%d nativeLayouts=%d"), Height + Spacing, Pairing.Pairs.Num(), Result.NativeChecks);
    if (Counters.SpacingZ == Spacing) return false;
    Counters.SpacingZ = Spacing;
    Subsystem->UpdateCounterState(Counters);
    return true;
}
