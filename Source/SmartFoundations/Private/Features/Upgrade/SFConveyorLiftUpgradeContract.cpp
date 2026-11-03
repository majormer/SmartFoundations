// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Upgrade/SFConveyorLiftUpgradeContract.h"
#include "SmartFoundations.h"
#include "UObject/UnrealType.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGFactoryConnectionComponent.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"

namespace SFConveyorLiftUpgradeContract
{
namespace
{
UFGFactoryConnectionComponent* Resolve(const FSFConveyorUpgradeOwner::FOperation& Operation, const FSFConveyorUpgradeOwner::FEdge& Edge)
{
    const auto* Replacement = Operation.Replacements.Find(Edge.Owner);
    auto* Actor = Replacement ? Replacement->Get() : Cast<AActor>(Edge.Owner.ResolveObjectPtr());
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return nullptr;
    UFGFactoryConnectionComponent* Result = nullptr;
    TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
    for (auto* Port : Ports)
        if (IsValid(Port) && Port->GetFName() == Edge.Port && Port->GetComponentLocation().Equals(Edge.Position, 0.1f)
            && static_cast<uint8>(Port->GetDirection()) == Edge.Direction)
        { if (Result) return nullptr; Result = Port; }
    return Result;
}
}

bool Prepare(AFGBuildableConveyorLift* Old, AFGHologram* Holo, FString& Reason)
{
    auto* Top = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform"));
    auto* Slots = FindFProperty<FArrayProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedPassthroughs"));
    auto* Object = Slots ? CastField<FObjectPropertyBase>(Slots->Inner) : nullptr;
    const auto Source = Old->GetSnappedPassthroughs();
    if (!Holo->IsA<AFGConveyorLiftHologram>() || !Top || Top->Struct != TBaseStructure<FTransform>::Get()
        || !Object || Source.Num() != 2)
    { Reason = TEXT("Native lift geometry/passthrough schema does not match the inspected contract"); return false; }
    const FTransform Value = Old->GetTopTransform();
    Top->CopyCompleteValue(Top->ContainerPtrToValuePtr<void>(Holo), &Value);
    FScriptArrayHelper Array(Slots, Slots->ContainerPtrToValuePtr<void>(Holo));
    Array.Resize(Source.Num());
    for (int32 Index = 0; Index < Source.Num(); ++Index) Object->SetObjectPropertyValue(Array.GetRawPtr(Index), Source[Index]);
    return true;
}

bool MatchesHologram(AFGBuildableConveyorLift* Old, AFGHologram* Holo)
{
    auto* Top = FindFProperty<FStructProperty>(Holo->GetClass(), TEXT("mTopTransform"));
    auto* Slots = FindFProperty<FArrayProperty>(Holo->GetClass(), TEXT("mSnappedPassthroughs"));
    auto* Object = Slots ? CastField<FObjectPropertyBase>(Slots->Inner) : nullptr;
    if (!Top || Top->Struct != TBaseStructure<FTransform>::Get() || !Object
        || !Top->ContainerPtrToValuePtr<FTransform>(Holo)->Equals(Old->GetTopTransform(), 0.1f)) return false;
    const auto Source = Old->GetSnappedPassthroughs();
    FScriptArrayHelper Array(Slots, Slots->ContainerPtrToValuePtr<void>(Holo));
    if (Array.Num() != Source.Num()) return false;
    for (int32 Index = 0; Index < Source.Num(); ++Index)
        if (Object->GetObjectPropertyValue(Array.GetRawPtr(Index)) != Source[Index]) return false;
    return true;
}

bool Capture(AFGBuildableConveyorLift* Lift, FSFConveyorUpgradeOwner::FOperation& Operation,
    USFConstructionOperationCoordinator& Owner, FString& Reason)
{
    auto Reject = [&]() { Reason = TEXT("Lift floor-hole bindings are not coherent, live and exclusively owned"); return false; };
    FSFConveyorUpgradeOwner::FLiftState State;
    State.Top = Lift->GetTopTransform(); State.bReversed = Lift->GetIsReversed();
    const auto Slots = Lift->GetSnappedPassthroughs();
    if (Slots.Num() != 2 || State.Top.ContainsNaN()) return Reject();
    auto CapturePort = [&](UFGFactoryConnectionComponent* Port, FSFConveyorUpgradeOwner::FEdge& Edge)
    {
        if (!IsValid(Port) || !IsValid(Port->GetOwner()) || !Owner.AdoptActor(Operation.Key, Port->GetOwner())) return false;
        Edge.Owner = FObjectKey(Port->GetOwner()); Edge.Port = Port->GetFName(); Edge.OriginalPort = Port;
        Edge.Position = Port->GetComponentLocation(); Edge.Direction = static_cast<uint8>(Port->GetDirection());
        return true;
    };
    for (auto* Hole : Slots)
    {
        FSFConveyorUpgradeOwner::FLiftHole Slot;
        if (Hole)
        {
            if (!IsValid(Hole) || !Owner.AdoptActor(Operation.Key, Hole)) return Reject();
            const FString Package = Hole->GetClass()->GetOutermost()->GetName();
            if (!Package.StartsWith(TEXT("/Game/FactoryGame/")) && Package != TEXT("/Script/FactoryGame")) return Reject();
            Slot.Hole = Hole; Slot.Pose = Hole->GetActorTransform(); Slot.Thickness = Hole->GetSnappedBuildingThickness();
            if (Slot.Pose.ContainsNaN() || !FMath::IsFinite(Slot.Thickness) || Slot.Thickness <= 0) return Reject();
            auto* Bottom = Hole->GetBottomSnappedConnection<UFGFactoryConnectionComponent>();
            auto* Top = Hole->GetTopSnappedConnection<UFGFactoryConnectionComponent>();
            if ((!Bottom || Bottom->GetOwner() != Lift) && (!Top || Top->GetOwner() != Lift)) return Reject();
            Slot.bBottom = Bottom != nullptr; Slot.bTop = Top != nullptr;
            if ((Bottom && !CapturePort(Bottom, Slot.Bottom)) || (Top && !CapturePort(Top, Slot.Top))) return Reject();
        }
        State.Slots.Add(MoveTemp(Slot));
    }
    Operation.Lifts.Add(FObjectKey(Lift), MoveTemp(State));
    return true;
}

bool Verify(const FSFConveyorUpgradeOwner::FOperation& Operation)
{
    for (const auto& Pair : Operation.Lifts)
    {
        const auto* Replacement = Operation.Replacements.Find(Pair.Key);
        auto* Lift = Cast<AFGBuildableConveyorLift>(Replacement ? Replacement->Get() : Pair.Key.ResolveObjectPtr());
        if (!IsValid(Lift) || Lift->IsActorBeingDestroyed() || !Lift->GetTopTransform().Equals(Pair.Value.Top, 0.1f)
            || Lift->GetIsReversed() != Pair.Value.bReversed) return false;
        const auto Slots = Lift->GetSnappedPassthroughs();
        if (Slots.Num() != Pair.Value.Slots.Num()) return false;
        for (int32 Index = 0; Index < Slots.Num(); ++Index)
        {
            const auto& Expected = Pair.Value.Slots[Index];
            auto* Hole = Expected.Hole.Get();
            if ((!Expected.Hole.IsExplicitlyNull() && !Hole) || Slots[Index] != Hole) return false;
            if (!Hole) continue;
            if (Hole->IsActorBeingDestroyed() || !Hole->GetActorTransform().Equals(Expected.Pose, 0.1f)
                || !FMath::IsNearlyEqual(Hole->GetSnappedBuildingThickness(), Expected.Thickness, 0.01f)) return false;
            auto* Bottom = Expected.bBottom ? Resolve(Operation, Expected.Bottom) : nullptr;
            auto* Top = Expected.bTop ? Resolve(Operation, Expected.Top) : nullptr;
            if ((Expected.bBottom && !Bottom) || (Expected.bTop && !Top)
                || Hole->GetBottomSnappedConnection<UFGFactoryConnectionComponent>() != Bottom
                || Hole->GetTopSnappedConnection<UFGFactoryConnectionComponent>() != Top) return false;
        }
    }
    return true;
}
}
