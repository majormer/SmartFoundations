// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkExistingOrigin.h"
#include "Features/Walk/SFWalkBusLayout.h"
#include "Features/Walk/SFWalkConveyance.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Hologram/FGHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "FGConstructDisqualifier.h"
#include "Components/SplineComponent.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP: client projects existing endpoints; authority resolves the
// exact actor/component identities again and quotes only newly constructed objects.
namespace
{
    struct FExistingWalkDraft
    {
        TArray<TWeakObjectPtr<AFGHologram>> Children;
        TArray<FItemAmount> Cost;
        bool bReady = false;
        bool bQuoteReady = false;
    };
    TMap<TWeakObjectPtr<AFGHologram>, FExistingWalkDraft> ExistingWalkDrafts;

    FTransform CellPose(const FTransform& Frame, int32 Lane, int32 Stack, const FSFWalkCommitSpec& Spec)
    {
        FSFPositionCalculator Calculator;
        FSFCounterState Counters;
        return FTransform(Frame.GetRotation(), Calculator.CalculateChildPosition(0,
            SFWalkBusLayout::Coordinate(Lane, Spec.Segments[0].NumLanes, Spec.ExistingLanes),
            SFWalkBusLayout::Coordinate(Stack, Spec.Segments[0].NumStacks, Spec.ExistingStacks),
            Frame.GetLocation(), Frame.Rotator(), FVector(0, Spec.LaneSpacing, Spec.StackSpacing), Counters));
    }
}

int32 USFWalkService::ExistingEndpointIndex(int32 Cell) const
{
    return SFWalkBusLayout::ExistingIndex(Cell, CrossSectionLanes, CrossSectionStacks, ExistingLanes, ExistingStacks);
}

bool USFWalkService::IsNewOriginCell(int32 Cell) const
{
    return IsResuming() ? ExistingEndpointIndex(Cell) == INDEX_NONE : Cell > 0;
}

bool USFWalkService::AreExistingEndpointsValid() const
{
    for (const auto& Endpoint : ExistingEndpoints)
    {
        if (!SFWalkExistingOrigin::Resolve(Endpoint, ConveyanceType, GetWorld())) { return false; }
    }
    return true;
}

FString USFWalkService::GetResumeSummary() const
{
    if (!IsResuming()) { return FString(); }
    const bool bReverse = ExistingEndpoints.ContainsByPredicate([](const auto& E) { return E.bReverseFlow; });
    const bool bForward = ExistingEndpoints.ContainsByPredicate([](const auto& E) { return !E.bReverseFlow; });
    return FText::Format(NSLOCTEXT("SmartWalking", "ExistingGridSummary",
        "Resume {0} x {1} | spacing {2} m / {3} m | {4} | added lanes use new supports"),
        FText::AsNumber(FMath::Abs(CrossSectionLanes)), FText::AsNumber(FMath::Abs(CrossSectionStacks)),
        FText::AsNumber(ExistingLaneSpacing / 100.f), FText::AsNumber(ExistingStackSpacing / 100.f),
        bReverse && bForward ? NSLOCTEXT("SmartWalking", "MixedFlow", "mixed belt directions")
            : NSLOCTEXT("SmartWalking", "NativeEndpoints", "native endpoints")).ToString();
}

bool USFWalkService::IsExistingCommitPrepared(AFGHologram* Seed)
{
    const auto* Draft = ExistingWalkDrafts.Find(Seed);
    return Draft && Draft->bReady;
}

bool USFWalkService::GetPreparedExistingCost(const AFGHologram* Seed, TArray<FItemAmount>& Cost)
{
    const auto* Draft = ExistingWalkDrafts.Find(const_cast<AFGHologram*>(Seed));
    if (!Draft || !Draft->bQuoteReady) { return false; }
    Cost = Draft->Cost;
    return true;
}

void USFWalkService::DiscardExistingCommit(AFGHologram* Seed)
{
    FExistingWalkDraft Draft;
    if (!ExistingWalkDrafts.RemoveAndCopyValue(Seed, Draft)) { return; }
    for (const auto& Weak : Draft.Children)
    {
        AFGHologram* Child = Weak.Get();
        if (!Child || Child == Seed) { continue; }
        if (AFGHologram* Parent = Child->GetParentHologram())
        {
            if (auto* Property = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
            { Property->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent)->Remove(Child); }
        }
        Child->Destroy();
    }
}

void USFWalkService::DiscardExistingForInstigator(APawn* Instigator, UClass* BuildClass)
{
    TArray<TWeakObjectPtr<AFGHologram>> Keys;
    ExistingWalkDrafts.GetKeys(Keys);
    for (const auto& Key : Keys)
    {
        AFGHologram* Seed = Key.Get();
        if (!Seed) { ExistingWalkDrafts.Remove(Key); continue; }
        if (Seed->GetConstructionInstigator() == Instigator && Seed->GetBuildClass() == BuildClass)
        { DiscardExistingCommit(Seed); }
    }
}

int32 USFWalkService::ReconstructExistingCommit(AFGHologram* Seed, const FSFWalkCommitSpec& Spec)
{
    // [MP-AUTH] Never accept client cost, endpoint pose, class or lane mapping on trust.
    if (!IsValid(Seed) || !Spec.bValid || !Seed->GetRecipe() || Seed->GetBuildClass() != Spec.BuildClass
        || Spec.OriginFrame.ContainsNaN() || !Spec.OriginFrame.GetScale3D().Equals(FVector::OneVector)
        || !SFWalkExistingOrigin::Validate(Spec, Seed->GetWorld())) { return -1; }
    const bool bMatchingSeed = Spec.ConveyanceType == ESFWalkConveyanceType::Hypertube
        ? USFAutoConnectService::IsStackableHypertubeSupportHologram(Seed)
        : Spec.ConveyanceType == ESFWalkConveyanceType::Pipe
            ? USFAutoConnectService::IsStackablePipelineSupportHologram(Seed)
                && !USFAutoConnectService::IsStackableHypertubeSupportHologram(Seed)
            : Spec.ConveyanceType == ESFWalkConveyanceType::Belt
                && Seed->GetBuildClass()->GetFName() == TEXT("Build_ConveyorPoleStackable_C");
    if (!bMatchingSeed) { return -1; }
    const int32 Lanes = FMath::Abs(Spec.Segments[0].NumLanes), Stacks = FMath::Abs(Spec.Segments[0].NumStacks);
    FVector Offset;
    if (!SFWalkExistingOrigin::TryConnectorOffset(Seed, Spec.ConveyanceType, Offset)) { return -1; }
    if (!SFWalkExistingOrigin::ValidateGeometry(Spec, Offset)) { return -1; }
    if (IsExistingCommitPrepared(Seed)) { return ExistingWalkDrafts.FindChecked(Seed).Children.Num(); }

    TArray<FSFWalkSegment> Path;
    int64 Cells = Lanes * Stacks - Spec.ExistingEndpoints.Num();
    for (const auto& Item : Spec.Segments)
    {
        if (!FMath::IsFinite(Item.Advance) || Item.Advance < 100.f || !FMath::IsFinite(Item.TurnDegrees)
            || FMath::Abs(Item.TurnDegrees) > 270.f || !FMath::IsFinite(Item.Rise) || !FMath::IsFinite(Item.Shift)) { return -1; }
        Cells += static_cast<int64>(Lanes) * Stacks;
        if (Cells > 4096) { return -1; }
        FSFWalkSegment Segment;
        Segment.Advance = Item.Advance; Segment.TurnDegrees = Item.TurnDegrees;
        Segment.Rise = Item.Rise; Segment.Shift = Item.Shift;
        Segment.NumLanes = Lanes; Segment.NumStacks = Stacks;
        Path.Add(Segment);
    }
    USFSubsystem* Sub = Subsystem.Get();
    if (!Sub) { return -1; }
    Sub->SetAutoConnectBeltRoutingMode(Spec.BeltRoutingMode);
    Sub->SetAutoConnectBeltTierMain(Spec.BeltTier);
    Sub->SetAutoConnectPipeRoutingMode(Spec.PipeRoutingMode);
    Sub->SetAutoConnectPipeTierMain(Spec.PipeTier);
    Sub->SetAutoConnectPipeIndicator(Spec.bPipeIndicator);
    Sub->SetAutoConnectHypertubeRoutingMode(Spec.HypertubeRoutingMode);
    USFWalkConveyance* Adapter = Spec.ConveyanceType == ESFWalkConveyanceType::Pipe
        ? static_cast<USFWalkConveyance*>(NewObject<USFWalkPipeConveyance>(this))
        : Spec.ConveyanceType == ESFWalkConveyanceType::Hypertube
            ? static_cast<USFWalkConveyance*>(NewObject<USFWalkHypertubeConveyance>(this))
            : static_cast<USFWalkConveyance*>(NewObject<USFWalkBeltConveyance>(this));
    Adapter->SetSubsystem(Sub);
    FExistingWalkDraft& Draft = ExistingWalkDrafts.Add(Seed);
    auto Fail = [&]() { DiscardExistingCommit(Seed); return -1; };
    auto ExistingIndex = [&](int32 Cell) {
        return SFWalkBusLayout::ExistingIndex(Cell, Spec.Segments[0].NumLanes, Spec.Segments[0].NumStacks,
            Spec.ExistingLanes, Spec.ExistingStacks);
    };
    TArray<AFGHologram*> AddedOrigins;
    AddedOrigins.SetNumZeroed(Lanes * Stacks);
    for (int32 Cell = 0; Cell < Lanes * Stacks; ++Cell)
    {
        if (ExistingIndex(Cell) != INDEX_NONE) { continue; }
        const FTransform Pose = CellPose(Spec.OriginFrame, Cell / Stacks, Cell % Stacks, Spec);
        AFGHologram* Support = AFGHologram::SpawnHologramFromRecipe(
            Seed->GetRecipe(), Seed->GetOwner(), Pose.GetLocation(), Seed->GetInstigator());
        if (!Support) { return Fail(); }
        Draft.Children.Add(Support);
        Seed->AddChild(Support, Support->GetFName());
        SFWalkExistingOrigin::SetAnchorPose(Support, Pose);
        USFHologramDataService::EnableValidation(Support);
        AddedOrigins[Cell] = Support;
    }
    TArray<TArray<AFGHologram*>> Supports;
    Supports.SetNum(Path.Num());
    for (int32 Index = 0; Index < Path.Num(); ++Index)
    {
        const FTransform Frame = AccumulateFrame(Path, Spec.OriginFrame, Index);
        if (Frame.ContainsNaN() || Frame.GetLocation().Size() > 1.e7) { return Fail(); }
        Supports[Index].SetNumZeroed(Lanes * Stacks);
        for (int32 Cell = 0; Cell < Lanes * Stacks; ++Cell)
        {
            const FTransform Pose = CellPose(Frame, Cell / Stacks, Cell % Stacks, Spec);
            AFGHologram* Support = Index == 0 && Cell == 0 ? Seed : AFGHologram::SpawnHologramFromRecipe(
                Seed->GetRecipe(), Seed->GetOwner(), Pose.GetLocation(), Seed->GetInstigator());
            if (!Support) { return Fail(); }
            if (Support != Seed)
            {
                Draft.Children.Add(Support);
                Seed->AddChild(Support, Support->GetFName());
            }
            SFWalkExistingOrigin::SetAnchorPose(Support, Pose);
            USFHologramDataService::EnableValidation(Support);
            Supports[Index][Cell] = Support;
        }
    }
    for (int32 Index = 0; Index < Path.Num(); ++Index)
    {
        for (int32 Cell = 0; Cell < Lanes * Stacks; ++Cell)
        {
            const int32 Original = ExistingIndex(Cell);
            USceneComponent* Start = Index == 0 && Original != INDEX_NONE
                ? SFWalkExistingOrigin::Resolve(Spec.ExistingEndpoints[Original], Spec.ConveyanceType, Seed->GetWorld()) : nullptr;
            if (Index == 0 && Original != INDEX_NONE && !Start) { return Fail(); }
            const bool bReverse = Original != INDEX_NONE ? Spec.ExistingEndpoints[Original].bReverseFlow : Spec.BeltDirection == 1;
            Adapter->SetExistingStart(Start, true, bReverse);
            AFGHologram* From = Index > 0 ? Supports[Index - 1][Cell]
                : Original == INDEX_NONE ? AddedOrigins[Cell] : Seed;
            AFGHologram* Span = Adapter->LinkOrUpdate(nullptr, From,
                Supports[Index][Cell], Seed, true, Path[Index].TurnDegrees);
            if (!Span) { return Fail(); }
            Draft.Children.Add(Span);
            USFHologramDataService::EnableValidation(Span);
            if (auto* Pipe = Cast<ASFPipelineHologram>(Span))
            {
                bool bTooLong = false;
                const float Limit = Spec.ConveyanceType == ESFWalkConveyanceType::Hypertube
                    ? USFAutoConnectService::MAX_HYPERTUBE_LENGTH : USFAutoConnectService::MAX_PIPE_LENGTH;
                if (!Pipe->ValidateCurrentSpline(Limit, bTooLong)) { return Fail(); }
            }
            if (auto* Belt = Cast<ASFConveyorBeltHologram>(Span))
            {
                if (!Belt->GetSplineComponent() || Belt->GetSplineComponent()->GetSplineLength() > USFAutoConnectService::MAX_PIPE_LENGTH)
                { return Fail(); }
            }
        }
    }
    // Every object above is new. Existing supports/conduits never enter the child list,
    // group registration, destruction path or quote. Native validation runs before payment.
    auto AddCost = [&Draft](AFGHologram* Hologram)
    {
        for (const FItemAmount& Item : Hologram->GetCost(false))
        {
            FItemAmount* Found = Draft.Cost.FindByPredicate([&](const auto& Other) { return Other.ItemClass == Item.ItemClass; });
            if (Found) { Found->Amount += Item.Amount; } else { Draft.Cost.Add(Item); }
        }
    };
    AddCost(Seed);
    for (const auto& Child : Draft.Children) { AddCost(Child.Get()); }
    Draft.bQuoteReady = true;
    Draft.bReady = true;
    UE_LOG(LogSmartFoundations, Log, TEXT("Walking resume prepared: %d existing endpoints, %lld new supports, %lld new spans."),
        Spec.ExistingEndpoints.Num(), Cells, static_cast<int64>(Lanes) * Stacks * Path.Num());
    return Draft.Children.Num();
}
