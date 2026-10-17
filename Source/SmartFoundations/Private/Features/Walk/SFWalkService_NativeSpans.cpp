// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkNativeSpanAdapter.h"
#include "Hologram/FGHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — Native span path projection
//  1. [MP-CLIENT] Native rail/vehicle-path previews; first center span is the held seed.
//  2. [MP-AUTH] Vanilla span children reconstructed from parameter-only Walk spec.
namespace
{
    struct FPreparedNativeSpanDraft
    {
        TArray<TWeakObjectPtr<AFGHologram>> Tracks;
        TArray<FItemAmount> Cost;
        bool bReady = false;
        bool bQuoteReady = false;
    };
    TMap<TWeakObjectPtr<AFGHologram>, FPreparedNativeSpanDraft> PreparedNativeSpans;
}

bool USFWalkService::IsNativeSpanCommitPrepared(AFGHologram* Seed)
{
    const auto* Draft = PreparedNativeSpans.Find(Seed);
    return Draft && Draft->bReady;
}

bool USFWalkService::GetPreparedNativeSpanCost(const AFGHologram* Seed, TArray<FItemAmount>& Cost)
{
    const auto* Draft=PreparedNativeSpans.Find(const_cast<AFGHologram*>(Seed));
    if (!Draft || !Draft->bQuoteReady) { return false; }
    Cost=Draft->Cost;
    return true;
}

void USFWalkService::DiscardNativeSpanCommit(AFGHologram* Seed)
{
    FPreparedNativeSpanDraft Draft;
    if (!PreparedNativeSpans.RemoveAndCopyValue(Seed, Draft)) { return; }
    TArray<AFGHologram*> Tracks;
    for (const auto& Weak : Draft.Tracks)
    {
        if (auto* Track = Weak.Get()) { Tracks.Add(Track); }
    }
    FSFWalkNativeSpanAdapter::ClearConstruction(Tracks);
    for (auto* Track : Tracks)
    {
        if (Track == Seed) { continue; }
        // Match the established Walking teardown: unlink before destroying native children.
        if (AFGHologram* Parent = Track->GetParentHologram())
        {
            if (FArrayProperty* Property = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
            {
                Property->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent)->Remove(Track);
            }
        }
        Track->Destroy();
    }
}

void USFWalkService::RebuildNativeSpans(int32 StartIndex)
{
    AFGHologram* Seed = Cast<AFGHologram>(SeedHologram.Get());
    if (!FSFWalkNativeSpanAdapter::IsSupported(Seed)) { return; }
    for (int32 Index = FMath::Max(0, StartIndex); Index < Segments.Num(); ++Index)
    {
        FSFWalkSegment& Seg = Segments[Index];
        const FTransform From = Index == 0 ? OriginFrame : FrameAtIndex(Index - 1);
        const FTransform To = FrameAtIndex(Index);
        const int32 Lanes = FMath::Abs(Seg.NumLanes), Stacks = FMath::Abs(Seg.NumStacks);
        Seg.Spans.SetNum(Lanes * Stacks);
        for (int32 Lane = 0; Lane < Lanes; ++Lane)
        {
            for (int32 Stack = 0; Stack < Stacks; ++Stack)
            {
                const int32 Flat = Lane * Stacks + Stack;
                AFGHologram* Track = Cast<AFGHologram>(Seg.Spans[Flat].Get());
                if (!Track)
                {
                    Track = (Index == 0 && Flat == 0) ? Seed : Cast<AFGHologram>(AFGHologram::SpawnHologramFromRecipe(
                        Seed->GetRecipe(), Seed->GetOwner(), From.GetLocation(), Seed->GetInstigator()));
                    if (!Track) { continue; }
                    Seg.Spans[Flat] = Track;
                    if (Track != Seed)
                    {
                        USFHologramDataService::MarkAsChild(Track, Seed, ESFChildHologramType::WalkSegment);
                        Track->SetActorTickEnabled(false);
                        Track->SetActorEnableCollision(false);
                        USFHologramDataService::DisableValidation(Track);
                    }
                }
                const int32 SignedLane = Lane * (Seg.NumLanes > 0 ? 1 : -1);
                const int32 SignedStack = Stack * (Seg.NumStacks > 0 ? 1 : -1);
                FSFWalkNativeSpanAdapter::Route(Track, CrossSectionPose(From, SignedLane, SignedStack), CrossSectionPose(To, SignedLane, SignedStack), Seed);
            }
        }
        Seg.LastAppliedSegState = -1;
    }
    bWalkCostDirty = true;
}

int32 USFWalkService::ReconstructNativeSpanCommit(AFGHologram* Hologram, const FSFWalkCommitSpec& Spec)
{
    AFGHologram* Seed = Cast<AFGHologram>(Hologram);
    if (!FSFWalkNativeSpanAdapter::MatchesType(Seed, Spec.ConveyanceType) || !Spec.bValid || Spec.Segments.IsEmpty() || Spec.OriginFrame.ContainsNaN()) { return -1; }
    if (IsNativeSpanCommitPrepared(Seed)) { return PreparedNativeSpans.FindChecked(Seed).Tracks.Num() - 1; }
    TArray<FSFWalkSegment> Path;
    int64 CellCount = 0;
    for (const FSFWalkCommitSegment& Item : Spec.Segments)
    {
        if (!FMath::IsFinite(Item.Advance) || !FMath::IsFinite(Item.TurnDegrees)
            || !FMath::IsFinite(Item.Rise) || !FMath::IsFinite(Item.Shift)
            || Item.NumLanes == 0 || Item.NumStacks == 0
            || Item.NumLanes < -256 || Item.NumLanes > 256
            || Item.NumStacks < -256 || Item.NumStacks > 256) { return -1; }
        CellCount += static_cast<int64>(FMath::Abs(Item.NumLanes)) * FMath::Abs(Item.NumStacks);
        if (CellCount > 4096) { return -1; }
        FSFWalkSegment Seg;
        Seg.Advance=Item.Advance; Seg.TurnDegrees=Item.TurnDegrees; Seg.Rise=Item.Rise; Seg.Shift=Item.Shift;
        Seg.NumLanes=Item.NumLanes; Seg.NumStacks=Item.NumStacks;
        Path.Add(Seg);
    }
    TArray<TArray<AFGHologram*>> Tracks;
    Tracks.SetNum(Path.Num());
    FPreparedNativeSpanDraft& Draft = PreparedNativeSpans.Add(Seed);
    int32 Spawned = 0;
    for (int32 Index = 0; Index < Path.Num(); ++Index)
    {
        const auto& Seg = Path[Index];
        const FTransform From = Index == 0 ? Spec.OriginFrame : AccumulateFrame(Path, Spec.OriginFrame, Index - 1);
        const FTransform To = AccumulateFrame(Path, Spec.OriginFrame, Index);
        const int32 Lanes=FMath::Abs(Seg.NumLanes), Stacks=FMath::Abs(Seg.NumStacks);
        Tracks[Index].SetNumZeroed(Lanes * Stacks);
        for (int32 Lane=0; Lane<Lanes; ++Lane)
        {
            for (int32 Stack=0; Stack<Stacks; ++Stack)
            {
                const int32 Flat=Lane*Stacks+Stack;
                AFGHologram* Track=(Index==0 && Flat==0) ? Seed : Cast<AFGHologram>(AFGHologram::SpawnHologramFromRecipe(
                    Seed->GetRecipe(), Seed->GetOwner(), From.GetLocation(), Seed->GetInstigator()));
                if (!Track) { DiscardNativeSpanCommit(Seed); return -1; }
                Draft.Tracks.Add(Track);
                if (!FSFWalkNativeSpanAdapter::Route(Track, FSFWalkNativeSpanAdapter::CrossSectionPose(From, Lane*(Seg.NumLanes>0?1:-1), Stack*(Seg.NumStacks>0?1:-1)),
                    FSFWalkNativeSpanAdapter::CrossSectionPose(To, Lane*(Seg.NumLanes>0?1:-1), Stack*(Seg.NumStacks>0?1:-1)), Seed)
                    || !FSFWalkNativeSpanAdapter::Validate(Track).IsEmpty()) { DiscardNativeSpanCommit(Seed); return -1; }
                Tracks[Index][Flat]=Track;
            }
        }
    }
    // Only attach after every native span exists and has passed its shape contract.
    for (int32 Index=0; Index<Path.Num(); ++Index)
    {
        const auto& Seg=Path[Index];
        const FTransform From=Index==0 ? Spec.OriginFrame : AccumulateFrame(Path,Spec.OriginFrame,Index-1);
        const FTransform To=AccumulateFrame(Path,Spec.OriginFrame,Index);
        const int32 Stacks=FMath::Abs(Seg.NumStacks);
        for (int32 Flat=0; Flat<Tracks[Index].Num(); ++Flat)
        {
            auto* Track=Tracks[Index][Flat];
            if (Track!=Seed)
            {
                Seed->AddChild(Track,Track->GetFName());
                const int32 Lane=(Flat/Stacks)*(Seg.NumLanes>0?1:-1), Stack=(Flat%Stacks)*(Seg.NumStacks>0?1:-1);
                if (!FSFWalkNativeSpanAdapter::Route(Track,FSFWalkNativeSpanAdapter::CrossSectionPose(From,Lane,Stack),
                    FSFWalkNativeSpanAdapter::CrossSectionPose(To,Lane,Stack),Seed)) { DiscardNativeSpanCommit(Seed); return -1; }
                ++Spawned;
            }
            auto* Previous=Index>0 && Tracks[Index-1].IsValidIndex(Flat) ? Tracks[Index-1][Flat] : nullptr;
            FSFWalkNativeSpanAdapter::StageConstruction(Track,Previous);
        }
    }
    Draft.bReady=true;
    // Native local construction pays after Construct. Keep its complete accepted
    // quote alive even if vanilla retires native child holograms during that call.
    Draft.Cost=Seed->GetCost(true);
    Draft.bQuoteReady=true;
    UE_LOG(LogSmartFoundations, Log, TEXT("Walking native-span reconstruction: %d spans, %d native children."), static_cast<int32>(CellCount), Spawned);
    return Spawned;
}
