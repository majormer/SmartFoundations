// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Net/SFLiftRoutePlan.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteProxyHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteBeltHologram.h"
#include "Features/AutoConnect/Preview/SFLiftRouteNativePreview.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGFactoryConnectionComponent.h"
#include "FGRecipe.h"
#include "EngineUtils.h"

// SP/MP DIVERGENCE MAP — lift-assisted Auto-Connect
// [MP-CLIENT] Capture the priced native route and exact endpoints at fire time.
// [MP-AUTH] Recreate/validate the entire plan before native payment, without re-pairing.
namespace
{
TMap<TWeakObjectPtr<AFGHologram>, TArray<FItemAmount>> PreparedCosts;
const FName PreparedTag(TEXT("SF_LiftPlanPrepared"));
void Walk(AFGHologram* Root, TArray<AFGHologram*>& Out)
{
    if (!IsValid(Root) || Out.Contains(Root)) return;
    Out.Add(Root);
    for (AFGHologram* Child : Root->GetHologramChildren()) Walk(Child, Out);
}
bool SamePose(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), .1f)
        && A.GetRotation().Equals(B.GetRotation(), .0001f)
        && A.GetScale3D().Equals(B.GetScale3D(), .0001f);
}
bool SameCost(const TArray<FItemAmount>& A, const TArray<FItemAmount>& B)
{
    TMap<UClass*, int32> Totals;
    for (const auto& I : A) Totals.FindOrAdd(I.ItemClass.Get()) += I.Amount;
    for (const auto& I : B) Totals.FindOrAdd(I.ItemClass.Get()) -= I.Amount;
    for (const auto& I : Totals) if (I.Value) return false;
    return true;
}
FSFLiftRouteEndpoint CapturePort(UFGFactoryConnectionComponent* Port, const TMap<AFGHologram*, int32>& Indices)
{
    FSFLiftRouteEndpoint E;
    if (!IsValid(Port) || !IsValid(Port->GetOwner())) return E;
    E.PortName = Port->GetFName(); E.PortPose = Port->GetComponentTransform();
    E.Direction = static_cast<int32>(Port->GetDirection()); E.OwnerPose = Port->GetOwner()->GetActorTransform();
    if (auto* H = Cast<AFGHologram>(Port->GetOwner()))
    {
        E.BuildClass = H->GetBuildClass();
        if (const int32* Index = Indices.Find(H)) E.RouteIndex = *Index;
    }
    else { E.bExisting = true; E.ExistingActor = Cast<AFGBuildable>(Port->GetOwner()); E.BuildClass = Port->GetOwner()->GetClass(); }
    return E;
}
UFGFactoryConnectionComponent* Resolve(const FSFLiftRouteEndpoint& E, AFGHologram* Root, const TArray<AFGHologram*>& Routes)
{
    if (E.PortName.IsNone()) return nullptr;
    AActor* Owner = nullptr;
    if (E.RouteIndex != INDEX_NONE)
    {
        if (!Routes.IsValidIndex(E.RouteIndex)) return nullptr;
        Owner = Routes[E.RouteIndex];
    }
    else if (E.bExisting)
    {
        Owner = E.ExistingActor;
        if (!IsValid(Owner) || Owner->GetClass() != E.BuildClass
            || !SamePose(Owner->GetActorTransform(), E.OwnerPose)) return nullptr;
    }
    else
    {
        TArray<AFGHologram*> Candidates; Walk(Root, Candidates);
        for (auto* H : Candidates)
            if (!H->IsA<ASFLiftRouteProxyHologram>() && H->GetBuildClass() == E.BuildClass
                && SamePose(H->GetActorTransform(), E.OwnerPose))
            { if (Owner) return nullptr; Owner = H; }
    }
    if (!IsValid(Owner)) return nullptr;
    TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Owner);
    for (auto* Port : Ports)
        if (Port->GetFName() == E.PortName && !Port->IsConnected()
            && static_cast<int32>(Port->GetDirection()) == E.Direction
            && SamePose(Port->GetComponentTransform(), E.PortPose)) return Port;
    return nullptr;
}
}
void SFLiftRoutePlan::Capture(AFGHologram* Parent, TArray<FSFLiftRoutePlanEntry>& Entries)
{
    Entries.Reset(); TArray<AFGHologram*> Tree; Walk(Parent, Tree); TMap<AFGHologram*, int32> Indices;
    for (auto* H : Tree) if (auto* P = Cast<ASFLiftRouteProxyHologram>(H))
    {
        FSFLiftRoutePlanEntry E; E.bLift = P->IsLift(); E.BuildClass = P->GetBuildClass(); E.Recipe = P->GetRecipe();
        E.Pose = P->GetActorTransform(); E.Top = P->GetTop(); E.Spline = P->GetRouteSpline(); E.Cost = P->GetCost(false);
        for (int32 Tier = 1; Tier <= 6; ++Tier)
            if (E.BuildClass && E.BuildClass->GetName().Contains(FString::Printf(TEXT("Mk%d_"), Tier))) E.Tier = Tier;
        E.From = CapturePort(P->GetRouteFrom(), Indices); E.To = CapturePort(P->GetRouteTo(), Indices);
        Indices.Add(H, Entries.Num()); Entries.Add(MoveTemp(E));
    }
}
bool SFLiftRoutePlan::IsBounded(const TArray<FSFLiftRoutePlanEntry>& Entries)
{
    if (Entries.Num() > 4096) return false;
    for (int32 I = 0; I < Entries.Num(); ++I)
    {
        const auto& E = Entries[I];
        if (!E.BuildClass || !E.Recipe || E.Tier < 1 || E.Tier > 6 || E.Pose.ContainsNaN() || E.Top.ContainsNaN()
            || E.Spline.Num() > 64 || (!E.bLift && E.Spline.Num() < 2) || E.Cost.IsEmpty() || E.Cost.Num() > 16) return false;
        for (const auto& Point : E.Spline)
            if (Point.Location.ContainsNaN() || Point.ArriveTangent.ContainsNaN() || Point.LeaveTangent.ContainsNaN()) return false;
        for (const auto& Cost : E.Cost) if (!Cost.ItemClass || Cost.Amount < 1 || Cost.Amount > 10000) return false;
        for (const auto* P : {&E.From, &E.To})
        {
            if (P->PortName.IsNone()) { if (P == &E.From || !E.bLift) return false; continue; }
            if (!P->BuildClass || P->OwnerPose.ContainsNaN() || P->PortPose.ContainsNaN()
                || P->RouteIndex < INDEX_NONE || P->RouteIndex >= I || P->Direction < 0 || P->Direction > 1) return false;
        }
    }
    return true;
}
bool SFLiftRoutePlan::Prepare(AFGHologram* Parent, const TArray<FSFLiftRoutePlanEntry>& Entries)
{
    if (!IsValid(Parent) || !IsBounded(Entries)) return false;
    if (Parent->ActorHasTag(PreparedTag)) return ValidateTree(Parent);
    if (!Parent->GetBlueprintDesigner())
        for (TActorIterator<AFGBuildableBlueprintDesigner> It(Parent->GetWorld()); It; ++It)
            if (It->IsLocationInsideDesigner(Parent->GetActorLocation())) { Parent->SetInsideBlueprintDesigner(*It); break; }
    TArray<AFGHologram*> Routes;
    for (const auto& E : Entries)
    {
        auto* From = Resolve(E.From, Parent, Routes);
        auto* To = E.To.PortName.IsNone() ? nullptr : Resolve(E.To, Parent, Routes);
        if (!From || (!E.To.PortName.IsNone() && !To)) return false;
        auto Pool = FSFLiftRouteNativePreview::Acquire(Parent, E.Tier);
        AFGHologram* Native = nullptr;
        if (E.bLift)
        {
            auto* Lift = Pool->ConfigureLift(Parent, From, E.Pose, E.Top);
            if (!Lift || (To && !Lift->TrySetDirectPeer(To)) || !Lift->IsNativePlacementValid()) return false;
            Native = Lift;
        }
        else
        {
            auto* Belt = Pool->ConfigureBelt(Parent, From, To);
            if (!Belt) return false;
            Belt->SetActorTransform(E.Pose); Belt->SetSplineDataAndUpdate(E.Spline);
            if (!Belt->IsNativePlacementValid()) return false;
            Native = Belt;
        }
        if (Native->GetBuildClass() != E.BuildClass || Native->GetRecipe() != E.Recipe || !SameCost(Native->GetCost(false), E.Cost)) return false;
        auto* Proxy = Cast<ASFLiftRouteProxyHologram>(ASFBuildableChildHologram::SpawnPreview(Native, E.Pose, ESFChildHologramType::AutoConnect, NAME_None, ASFLiftRouteProxyHologram::StaticClass()));
        if (!Proxy || !Proxy->Capture(Native, Pool, From, To, E.bLift))
        {
            if (Proxy) Proxy->Destroy();
            Pool->Hide();
            return false;
        }
        Proxy->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
        AFGHologram* Owner = Parent;
        if (!E.bLift && E.From.RouteIndex != INDEX_NONE) Owner = Routes[E.From.RouteIndex];
        else if (!E.bLift && E.To.RouteIndex != INDEX_NONE) Owner = Routes[E.To.RouteIndex];
        Owner->AddChild(Proxy, Proxy->GetFName()); Routes.Add(Proxy); Pool->Hide();
    }
    Parent->Tags.AddUnique(PreparedTag);
    TArray<FItemAmount> Cost = Parent->GetCost(true);
    PreparedCosts.Add(Parent, Cost);
    for (auto It = PreparedCosts.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    return ValidateTree(Parent);
}
bool SFLiftRoutePlan::ValidateTree(AFGHologram* Parent)
{
    TArray<AFGHologram*> Tree; Walk(Parent, Tree);
    TSet<UFGFactoryConnectionComponent*> Reserved;
    for (auto* H : Tree) if (auto* P = Cast<ASFLiftRouteProxyHologram>(H))
    {
        if (!P->ValidateCapturedRoute()) return false;
        for (auto* Port : {P->GetRouteFrom(), P->GetRouteTo()})
            if (Port && Cast<AFGBuildable>(Port->GetOwner()))
            { if (Reserved.Contains(Port)) return false; Reserved.Add(Port); }
    }
    return true;
}
bool SFLiftRoutePlan::GetPreparedCost(const AFGHologram* Parent, TArray<FItemAmount>& OutCost)
{
    if (const auto* Cost = PreparedCosts.Find(const_cast<AFGHologram*>(Parent))) { OutCost = *Cost; return true; }
    return false;
}
