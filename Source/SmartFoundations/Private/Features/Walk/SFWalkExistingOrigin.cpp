// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkExistingOrigin.h"
#include "Features/Walk/SFWalkBusLayout.h"
#include "Features/Walk/SFWalkEndpointGrid.h"
#include "Core/SFNativeHit.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFHologramDataService.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipeHyper.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"
#include "FGLightweightBuildableSubsystem.h"
#include "Hologram/FGHologram.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"

namespace
{
    constexpr double MatchDistance = 2.0;
    constexpr double SearchRadius = 10000.0;

    USceneComponent* NativeStart(AFGHologram* Span)
    {
        if (!Span || !Span->Tags.Contains(TEXT("SF_WalkExistingOrigin"))) { return nullptr; }
        auto* Data = USFHologramDataRegistry::GetData(Span);
        if (!Data) { return nullptr; }
        for (USceneComponent* Candidate : {static_cast<USceneComponent*>(Data->StackableBeltConn0.Get()),
            static_cast<USceneComponent*>(Data->StackableBeltConn1.Get()),
            static_cast<USceneComponent*>(Data->StackablePipeConn0.Get()), static_cast<USceneComponent*>(Data->StackablePipeConn1.Get())})
        {
            auto* Owner = Candidate ? Candidate->GetOwner() : nullptr;
            if (IsValid(Owner) && (Owner->IsA<AFGBuildableConveyorBase>() || Owner->IsA<AFGBuildablePipeline>() || Owner->IsA<AFGBuildablePipeHyper>()))
            { return Candidate; }
        }
        return nullptr;
    }

    bool IsConduit(const AFGBuildable* Buildable, ESFWalkConveyanceType Type)
    {
        if (!IsValid(Buildable)) { return false; }
        if (Type == ESFWalkConveyanceType::Belt) { return Buildable->IsA<AFGBuildableConveyorBase>(); }
        if (Type == ESFWalkConveyanceType::Pipe) { return Buildable->IsA<AFGBuildablePipeline>(); }
        return Type == ESFWalkConveyanceType::Hypertube && Buildable->IsA<AFGBuildablePipeHyper>();
    }

    bool IsEligibleSupport(FName Class, ESFWalkConveyanceType Type)
    {
        return (Type == ESFWalkConveyanceType::Belt && Class == TEXT("Build_ConveyorPoleStackable_C"))
            || (Type == ESFWalkConveyanceType::Pipe && Class == TEXT("Build_PipeSupportStackable_C"))
            || (Type == ESFWalkConveyanceType::Hypertube && Class == TEXT("Build_HyperPoleStackable_C"));
    }

    bool IsExcludedSupport(FName Class)
    {
        return Class == TEXT("Build_ConveyorPole_C") || Class == TEXT("Build_ConveyorPoleWall_C")
            || Class == TEXT("Build_ConveyorCeilingAttachment_C") || Class == TEXT("Build_PipelineSupport_C")
            || Class == TEXT("Build_PipelineSupportWall_C") || Class == TEXT("Build_PipeHyperSupport_C")
            || Class == TEXT("Build_HyperTubeWallSupport_C") || Class == TEXT("Build_HyperTubeWallHole_C")
            || Class == TEXT("Build_FoundationPassthrough_Hypertube_C")
            || Class == TEXT("Build_PipelineSupportWallHole_C") || Class == TEXT("Build_FoundationPassthrough_Pipe_C")
            || Class == TEXT("Build_ConveyorWallHole_C");
    }

    void Connections(AActor* Actor, ESFWalkConveyanceType Type, TArray<USceneComponent*>& Out)
    {
        if (Type == ESFWalkConveyanceType::Belt)
        {
            TInlineComponentArray<UFGFactoryConnectionComponent*> Components(Actor);
            for (auto* Component : Components) { Out.Add(Component); }
        }
        else
        {
            TInlineComponentArray<UFGPipeConnectionComponentBase*> Components(Actor);
            for (auto* Component : Components)
            {
                if (Component->IsA<UFGPipeConnectionComponentHyper>() == (Type == ESFWalkConveyanceType::Hypertube))
                { Out.Add(Component); }
            }
        }
    }

    bool Describe(USceneComponent* Component, ESFWalkConveyanceType Type, FSFWalkExistingEndpoint& Out,
        bool& Occupied)
    {
        auto* Buildable = Cast<AFGBuildable>(Component ? Component->GetOwner() : nullptr);
        if (!IsConduit(Buildable, Type)) { return false; }
        Out.Buildable = Buildable;
        Out.ComponentName = Component->GetFName();
        Out.Position = Component->GetComponentLocation();
        Out.Outward = Component->GetForwardVector().GetSafeNormal();
        if (auto* Belt = Cast<UFGFactoryConnectionComponent>(Component))
        {
            const auto Direction = Belt->GetDirection();
            if (Direction != EFactoryConnectionDirection::FCD_INPUT && Direction != EFactoryConnectionDirection::FCD_OUTPUT)
            { return false; }
            Out.bReverseFlow = Direction == EFactoryConnectionDirection::FCD_INPUT;
            Occupied = Belt->IsConnected();
        }
        else if (auto* Pipe = Cast<UFGPipeConnectionComponentBase>(Component))
        {
            if (Pipe->GetPipeConnectionType() == EPipeConnectionType::PCT_SNAP_ONLY || Pipe->IsSnappingToDisallowed()) { return false; }
            Occupied = Pipe->IsConnected();
        }
        else { return false; }
        return !Out.Position.ContainsNaN() && !Out.Outward.IsNearlyZero();
    }

    // Blueprint SCS connectors are not necessarily instantiated on the CDO. Read
    // their templates, including parent transforms, without promoting lightweight objects.
    void ConnectionOffsets(UClass* Class, TArray<FVector>& Out)
    {
        auto* Prototype = Class->GetDefaultObject<AFGBuildable>();
        TInlineComponentArray<UFGConnectionComponent*> Native(Prototype);
        for (auto* Component : Native)
        { Out.AddUnique(Component->GetComponentTransform().GetRelativeTransform(Prototype->GetActorTransform()).GetLocation()); }
        auto* Actual = Cast<UBlueprintGeneratedClass>(Class);
        if (!Actual) { return; }
        TMap<FName, FTransform> ParentTransforms;
        TInlineComponentArray<USceneComponent*> NativeParents(Prototype);
        for (auto* Component : NativeParents)
        { ParentTransforms.Add(Component->GetFName(), Component->GetComponentTransform().GetRelativeTransform(Prototype->GetActorTransform())); }
        TFunction<void(USCS_Node*, const FTransform&)> Visit;
        Visit = [&](USCS_Node* Node, const FTransform& Parent)
        {
            auto* Template = Cast<USceneComponent>(Node->GetActualComponentTemplate(Actual));
            const FTransform Transform = Template ? Template->GetRelativeTransform() * Parent : Parent;
            ParentTransforms.Add(Node->GetVariableName(), Transform);
            if (Template && Template->IsA<UFGConnectionComponent>()) { Out.AddUnique(Transform.GetLocation()); }
            for (auto* Child : Node->GetChildNodes()) { Visit(Child, Transform); }
        };
        TArray<UBlueprintGeneratedClass*> Hierarchy;
        for (UClass* Current = Class; Current; Current = Current->GetSuperClass())
        { if (auto* Blueprint = Cast<UBlueprintGeneratedClass>(Current)) { Hierarchy.Add(Blueprint); } }
        // Parent classes first, matching native SCS construction. Parent identity and
        // relative transforms are cooked properties; no Editor-only helper is used.
        for (int32 Index = Hierarchy.Num() - 1; Index >= 0; --Index)
        {
            auto* Blueprint = Hierarchy[Index];
            if (!Blueprint->SimpleConstructionScript) { continue; }
            for (auto* Root : Blueprint->SimpleConstructionScript->GetRootNodes())
            {
                const FTransform* Parent = ParentTransforms.Find(Root->ParentComponentOrVariableName);
                Visit(Root, Parent ? *Parent : FTransform::Identity);
            }
        }
    }

    void ExcludedAnchors(UWorld* World, TArray<FVector>& Out)
    {
        auto Add = [&Out](AFGBuildable* Object, const FTransform& Transform, bool bTemplate)
        {
            TInlineComponentArray<UFGConnectionComponent*> Components(Object);
            for (auto* Component : Components)
            {
                const FVector Point = bTemplate
                    ? Transform.TransformPosition(Component->GetComponentTransform().GetRelativeTransform(Object->GetActorTransform()).GetLocation())
                    : Component->GetComponentLocation();
                Out.Add(Point);
            }
        };
        for (TActorIterator<AFGBuildable> It(World); It; ++It)
        {
            if (IsExcludedSupport(It->GetClass()->GetFName())) { Add(*It, It->GetActorTransform(), false); }
        }
        if (auto* Lightweight = AFGLightweightBuildableSubsystem::Get(World))
        {
            for (const auto& Pair : Lightweight->GetAllLightweightBuildableInstances())
            {
                if (!Pair.Key || !IsExcludedSupport(Pair.Key->GetFName())) { continue; }
                TArray<FVector> Offsets;
                ConnectionOffsets(Pair.Key.Get(), Offsets);
                for (const auto& Instance : Pair.Value)
                {
                    if (!Instance.IsValid()) { continue; }
                    for (const auto& Offset : Offsets) { Out.Add(Instance.Transform.TransformPosition(Offset)); }
                }
            }
        }
    }

    bool IsExcluded(const FVector& Position, const TArray<FVector>& Anchors)
    {
        return Anchors.ContainsByPredicate([&](const FVector& Anchor) { return Anchor.Equals(Position, MatchDistance); });
    }
}

bool SFWalkExistingOrigin::TryConnectorOffset(AFGHologram* Seed, ESFWalkConveyanceType Type, FVector& Offset)
{
    if (!IsValid(Seed)) { return false; }
    // Support snap anchors are not transport endpoints. The native Hypertube
    // stackable may use a generic pipe anchor or its converted Hyper anchor.
    TArray<USceneComponent*> Components;
    Seed->GetComponents(Components);
    USceneComponent* Anchor = nullptr;
    for (USceneComponent* Component : Components)
    {
        if (!IsValid(Component)) { continue; }
        const FName Name = Component->GetFName();
        if (Name != TEXT("SnapOnly0") && !(Type == ESFWalkConveyanceType::Hypertube && Name == TEXT("Snap Only"))) { continue; }
        const bool bMatchingKind = Type == ESFWalkConveyanceType::Belt
            ? Component->IsA<UFGFactoryConnectionComponent>()
            : (Type == ESFWalkConveyanceType::Pipe || Type == ESFWalkConveyanceType::Hypertube)
                && Component->IsA<UFGPipeConnectionComponentBase>();
        if (!bMatchingKind || Anchor) { return false; }
        Anchor = Component;
    }
    if (!Anchor) { return false; }
    Offset = Seed->GetActorTransform().InverseTransformPosition(Anchor->GetComponentLocation());
    return !Offset.ContainsNaN();
}

ESFWalkOriginSelection SFWalkExistingOrigin::Discover(AFGHologram* Seed, AController* Controller,
    ESFWalkConveyanceType Type, FSFWalkCommitSpec& Origin, FString& Reason)
{
    if (!Seed || !Controller || !Seed->GetBuildClass() || !IsEligibleSupport(Seed->GetBuildClass()->GetFName(), Type)) { return ESFWalkOriginSelection::NewWalk; }
    FVector Eye; FRotator View;
    Controller->GetPlayerViewPoint(Eye, View);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartWalkExistingOrigin), true);
    Query.AddIgnoredActor(Seed);
    if (Controller->GetPawn()) { Query.AddIgnoredActor(Controller->GetPawn()); }
    FHitResult Hit;
    if (!Seed->GetWorld()->LineTraceSingleByChannel(Hit, Eye, Eye + View.Vector() * 8000.0, ECC_WorldStatic, Query))
    { return ESFWalkOriginSelection::NewWalk; }
    auto* HitBuildable = Cast<AFGBuildable>(SFNativeHit::ResolveActor(Hit));
    UClass* HitClass = HitBuildable ? HitBuildable->GetClass() : nullptr;
    FLightweightBuildableInstanceRef Instance;
    if (!HitClass)
    {
        if (auto* Manager = Cast<AAbstractInstanceManager>(Hit.GetActor()))
        {
            FInstanceHandle Handle;
            if (Manager->ResolveHit(Hit, Handle) && AFGLightweightBuildableSubsystem::ResolveLightweightInstance(Handle, Instance))
            { HitClass = Instance.GetBuildableClass(); }
        }
    }
    if (!HitClass) { return ESFWalkOriginSelection::NewWalk; }
    const bool bConduit = IsConduit(HitBuildable, Type);
    const bool bSupport = IsEligibleSupport(HitClass->GetFName(), Type);
    if (!bConduit && !bSupport)
    {
        if (!IsExcludedSupport(HitClass->GetFName())) { return ESFWalkOriginSelection::NewWalk; }
        Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeNoAttachment", "Walking cannot resume from a wall or floor attachment.").ToString();
        return ESFWalkOriginSelection::Rejected;
    }
    FVector Selection = Hit.ImpactPoint;
    TArray<USceneComponent*> HitConnections;
    if (HitBuildable) { Connections(HitBuildable, Type, HitConnections); }
    double Best = 300.0 * 300.0;
    USceneComponent* SelectedComponent = nullptr;
    bool bFound = false;
    for (auto* Component : HitConnections)
    {
        const double Distance = FVector::DistSquared(Component->GetComponentLocation(), Hit.ImpactPoint);
        if (Distance < Best)
        { Best = Distance; SelectedComponent = Component; Selection = Component->GetComponentLocation(); bFound = true; }
    }
    if (Instance.IsValid())
    {
        TArray<FVector> Offsets; ConnectionOffsets(HitClass, Offsets);
        for (const FVector& Offset : Offsets)
        {
            const FVector Point = Instance.GetBuildableTransform().TransformPosition(Offset);
            const double Distance = FVector::DistSquared(Point, Hit.ImpactPoint);
            if (Distance < Best) { Best = Distance; Selection = Point; bFound = true; }
        }
    }
    if (!bFound) { Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeAimEnd", "Aim at an exposed end of the existing run.").ToString(); return ESFWalkOriginSelection::Rejected; }
    TArray<FVector> Excluded;
    ExcludedAnchors(Seed->GetWorld(), Excluded);
    if (IsExcluded(Selection, Excluded))
    { Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeExcludedAttachment", "This endpoint belongs to an excluded wall or floor attachment.").ToString(); return ESFWalkOriginSelection::Rejected; }

    TArray<FSFWalkExistingEndpoint> Endpoints;
    TArray<bool> Occupied;
    int32 Selected = INDEX_NONE;
    for (TActorIterator<AFGBuildable> It(Seed->GetWorld()); It; ++It)
    {
        if (!IsConduit(*It, Type)) { continue; }
        TArray<USceneComponent*> Components;
        Connections(*It, Type, Components);
        for (auto* Component : Components)
        {
            FSFWalkExistingEndpoint Endpoint; bool bOccupied = false;
            if (!Describe(Component, Type, Endpoint, bOccupied) || FVector::Dist(Selection, Endpoint.Position) > SearchRadius) { continue; }
            const int32 Index = Endpoints.Add(Endpoint); Occupied.Add(bOccupied);
            if (bConduit ? Component == SelectedComponent : Endpoint.Position.Equals(Selection, MatchDistance))
            {
                if (Selected != INDEX_NONE) { Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeAmbiguousEnd", "More than one endpoint matches this attachment.").ToString(); return ESFWalkOriginSelection::Rejected; }
                Selected = Index;
            }
            if (Endpoints.Num() > 4096) { Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeTooManyEnds", "Too many nearby endpoints; select a smaller cross-section.").ToString(); return ESFWalkOriginSelection::Rejected; }
        }
    }
    if (!Endpoints.IsValidIndex(Selected) || Occupied[Selected])
    { Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeUnoccupiedEnd", "Select an unoccupied conduit endpoint.").ToString(); return ESFWalkOriginSelection::Rejected; }
    const FVector Forward = Endpoints[Selected].Outward;
    // Walking's existing pole frame is horizontal; native vertical starts require a separate frame contract.
    if (FMath::Abs(Forward.Z) > 0.01)
    { Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeHorizontalEnd", "Select a horizontal endpoint for Walking; rise is available on the continuation.").ToString(); return ESFWalkOriginSelection::Rejected; }
    const FVector Lateral = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
    TArray<FSFWalkEndpointSample> Samples;
    for (int32 Index = 0; Index < Endpoints.Num(); ++Index)
    {
        const auto& Endpoint = Endpoints[Index];
        FSFWalkEndpointSample Sample;
        Sample.Identity = Index; Sample.Position = Endpoint.Position; Sample.Outward = Endpoint.Outward;
        Sample.bOccupied = Occupied[Index]; Sample.bEligible = !IsExcluded(Endpoint.Position, Excluded);
        Sample.bReverseFlow = Endpoint.bReverseFlow; Samples.Add(Sample);
    }
    const auto Grid = SFWalkEndpointGrid::Resolve(Samples, Selected, Lateral, FVector::UpVector, MatchDistance);
    if (Grid.Result != ESFWalkEndpointGridResult::Ready)
    { Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeInvalidGrid", "The endpoint cross-section is occupied, irregular or ambiguous.").ToString(); return ESFWalkOriginSelection::Rejected; }
    Origin.ExistingLanes = Grid.Lanes; Origin.ExistingStacks = Grid.Stacks;
    Origin.ExistingEndpoints.Reset();
    for (int32 Identity : Grid.Identities) { Origin.ExistingEndpoints.Add(Endpoints[Identity]); }
    Origin.LaneSpacing = Grid.Lanes > 1 ? Grid.LaneSpacing : 200.0f;
    Origin.StackSpacing = Grid.Stacks > 1 ? Grid.StackSpacing : 200.0f;
    const FRotator Rotation = (-Forward).Rotation();
    FVector Offset;
    if (!TryConnectorOffset(Seed, Type, Offset))
    {
        Reason = NSLOCTEXT("SmartFoundations", "Walk_ResumeMissingAnchor", "The native support snap anchor is unavailable.").ToString();
        return ESFWalkOriginSelection::Rejected;
    }
    Origin.OriginFrame = FTransform(Rotation, Grid.Origin - Rotation.RotateVector(Offset));
    FSFWalkCommitSegment Shape; Shape.NumLanes = Grid.Lanes; Shape.NumStacks = Grid.Stacks;
    Origin.Segments = {Shape};
    return ESFWalkOriginSelection::Existing;
}

USceneComponent* SFWalkExistingOrigin::Resolve(const FSFWalkExistingEndpoint& Endpoint,
    ESFWalkConveyanceType Type, UWorld* World)
{
    if (!IsValid(Endpoint.Buildable) || Endpoint.Buildable->GetWorld() != World || !IsConduit(Endpoint.Buildable, Type)) { return nullptr; }
    TArray<USceneComponent*> Components;
    Connections(Endpoint.Buildable, Type, Components);
    USceneComponent* Found = nullptr;
    for (auto* Component : Components)
    {
        if (Component->GetFName() != Endpoint.ComponentName) { continue; }
        FSFWalkExistingEndpoint Current; bool bOccupied = false;
        if (Found || !Describe(Component, Type, Current, bOccupied) || bOccupied
            || !Current.Position.Equals(Endpoint.Position, MatchDistance)
            || !Current.Outward.Equals(Endpoint.Outward, 0.001f) || Current.bReverseFlow != Endpoint.bReverseFlow) { return nullptr; }
        Found = Component;
    }
    return Found;
}

bool SFWalkExistingOrigin::Validate(const FSFWalkCommitSpec& Spec, UWorld* World)
{
    if (Spec.ExistingEndpoints.IsEmpty() || Spec.ExistingEndpoints.Num() > 4096 || Spec.Segments.IsEmpty()
        || !FMath::IsFinite(Spec.LaneSpacing) || !FMath::IsFinite(Spec.StackSpacing)
        || Spec.LaneSpacing <= 0.0f || Spec.StackSpacing <= 0.0f) { return false; }
    const int32 Lanes = Spec.Segments[0].NumLanes, Stacks = Spec.Segments[0].NumStacks;
    if (!SFWalkBusLayout::IsValid(Lanes, Stacks, Spec.ExistingLanes, Spec.ExistingStacks)
        || static_cast<int64>(Spec.ExistingLanes) * Spec.ExistingStacks != Spec.ExistingEndpoints.Num()) { return false; }
    TArray<FVector> Excluded; ExcludedAnchors(World, Excluded);
    TSet<USceneComponent*> Unique;
    for (const auto& Endpoint : Spec.ExistingEndpoints)
    {
        auto* Component = Resolve(Endpoint, Spec.ConveyanceType, World);
        if (!Component || Unique.Contains(Component) || IsExcluded(Endpoint.Position, Excluded)) { return false; }
        Unique.Add(Component);
    }
    for (const auto& Segment : Spec.Segments)
    { if (Segment.NumLanes != Lanes || Segment.NumStacks != Stacks) { return false; } }
    return true;
}


bool SFWalkExistingOrigin::ValidateGeometry(const FSFWalkCommitSpec& Spec, const FVector& Offset)
{
    if (Spec.ExistingEndpoints.IsEmpty() || Spec.ExistingEndpoints.Num() > 4096 || Spec.Segments.IsEmpty()
        || Spec.Segments.Num() > 4096 || Spec.OriginFrame.ContainsNaN()
        || !Spec.OriginFrame.GetScale3D().Equals(FVector::OneVector) || Offset.ContainsNaN()
        || !FMath::IsFinite(Spec.LaneSpacing) || !FMath::IsFinite(Spec.StackSpacing)
        || Spec.LaneSpacing <= 0.f || Spec.StackSpacing <= 0.f) { return false; }
    const int32 Lanes = Spec.Segments[0].NumLanes, Stacks = Spec.Segments[0].NumStacks;
    if (!SFWalkBusLayout::IsValid(Lanes, Stacks, Spec.ExistingLanes, Spec.ExistingStacks)
        || static_cast<int64>(Spec.ExistingLanes) * Spec.ExistingStacks != Spec.ExistingEndpoints.Num()) { return false; }
    // Added starting supports count against the same bounded materialization budget.
    int64 Cells = static_cast<int64>(FMath::Abs(Lanes)) * FMath::Abs(Stacks) - Spec.ExistingEndpoints.Num();
    for (const auto& Item : Spec.Segments)
    {
        if (Item.NumLanes != Lanes || Item.NumStacks != Stacks || !FMath::IsFinite(Item.Advance)
            || Item.Advance < 100.f || !FMath::IsFinite(Item.TurnDegrees) || FMath::Abs(Item.TurnDegrees) > 270.f
            || !FMath::IsFinite(Item.Rise) || !FMath::IsFinite(Item.Shift)) { return false; }
        Cells += static_cast<int64>(FMath::Abs(Lanes)) * FMath::Abs(Stacks);
        if (Cells > 4096) { return false; }
    }
    FSFPositionCalculator Calculator;
    FSFCounterState Counters;
    for (int32 Cell = 0; Cell < Spec.ExistingEndpoints.Num(); ++Cell)
    {
        const auto& Endpoint = Spec.ExistingEndpoints[Cell];
        const FVector Base = Calculator.CalculateChildPosition(0, Cell / Spec.ExistingStacks, Cell % Spec.ExistingStacks,
            Spec.OriginFrame.GetLocation(), Spec.OriginFrame.Rotator(), FVector(0, Spec.LaneSpacing, Spec.StackSpacing), Counters);
        const FVector Position = Base + Spec.OriginFrame.TransformVectorNoScale(Offset);
        if (!Position.Equals(Endpoint.Position, 2.f)
            || !(-Spec.OriginFrame.GetUnitAxis(EAxis::X)).Equals(Endpoint.Outward, .001f)) { return false; }
    }
    return true;
}


bool SFWalkExistingOrigin::IsNativeStartPosition(AFGHologram* Span, const FVector& Position)
{
    auto* Start = NativeStart(Span);
    return Start && Start->GetComponentLocation().Equals(Position, MatchDistance);
}

bool SFWalkExistingOrigin::ConnectNativeStart(AFGHologram* Span, AFGBuildable* BuiltSpan)
{
    auto* Start = NativeStart(Span);
    if (!Start || !IsValid(BuiltSpan)) { return false; }
    if (auto* BeltStart = Cast<UFGFactoryConnectionComponent>(Start))
    {
        TInlineComponentArray<UFGFactoryConnectionComponent*> Components(BuiltSpan);
        for (auto* Own : Components)
        {
            if (!Own->GetComponentLocation().Equals(Start->GetComponentLocation(), MatchDistance)) { continue; }
            if (Own->IsConnected()) { return Own->GetConnection() == BeltStart; }
            if (BeltStart->IsConnected() || !Own->CanConnectTo(BeltStart)) { return false; }
            Own->SetConnection(BeltStart);
            return Own->GetConnection() == BeltStart;
        }
    }
    else if (auto* PipeStart = Cast<UFGPipeConnectionComponentBase>(Start))
    {
        TInlineComponentArray<UFGPipeConnectionComponentBase*> Components(BuiltSpan);
        for (auto* Own : Components)
        {
            if (!Own->GetComponentLocation().Equals(Start->GetComponentLocation(), MatchDistance)) { continue; }
            if (Own->IsConnected()) { return Own->GetConnection() == PipeStart; }
            if (PipeStart->IsConnected() || !Own->CanConnectTo(PipeStart)) { return false; }
            Own->SetConnection(PipeStart);
            return Own->GetConnection() == PipeStart;
        }
    }
    return false;
}


void SFWalkExistingOrigin::SetAnchorPose(AFGHologram* Hologram, const FTransform& Pose)
{
    if (!IsValid(Hologram)) { return; }
    const bool bLocked = Hologram->IsHologramLocked();
    if (bLocked) { Hologram->LockHologramPosition(false); }
    Hologram->SetActorTransform(Pose);
    Hologram->UpdateComponentTransforms();
    if (bLocked) { Hologram->LockHologramPosition(true); }
    // A conduit can be a valid Walking origin but an invalid aim for the support
    // recipe, leaving the held hologram hidden. Restore visibility with the
    // validated, locked Walking pose. Do not clear native placement disqualifiers.
    Hologram->SetActorHiddenInGame(false);
}
