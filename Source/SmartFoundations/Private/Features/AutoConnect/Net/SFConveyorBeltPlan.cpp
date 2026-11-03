// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Net/SFConveyorBeltPlan.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "UObject/ObjectKey.h"
#include "UObject/StrongObjectPtr.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGFactoryConnectionComponent.h"
#include "FGConstructDisqualifier.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "Resources/FGBuildingDescriptor.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"

// SP/MP DIVERGENCE MAP — ordinary generated conveyor plan
// [MP-AUTH] Both paths validate exact native belt geometry and complete mouth claims.
// [MP-SEAM] Remote path expands the authority tree before payment, never after it.
// Local native payment can follow Construct: retain the accepted authority quote.
namespace
{
struct FPreparedBelt
{
    TWeakObjectPtr<ASFConveyorBeltHologram> Preview;
    FSFConduitPlanEntry Plan;
};
struct FPrepared
{
    TWeakObjectPtr<UWorld> World;
    TSet<FObjectKey> OriginalChildren;
    TSet<FObjectKey> GeneratedChildren;
    TArray<FPreparedBelt> Belts;
    TArray<FItemAmount> Cost;
    TArray<FItemAmount> DeferredWireCost;
    bool bReconstructed = false;
    bool bReady = false;
};
TMap<TWeakObjectPtr<AFGHologram>, FPrepared> Prepared;
const AFGHologram* ComputingQuote = nullptr;
int32 RequestDepth = 0;
bool SameCost(const TArray<FItemAmount>& A, const TArray<FItemAmount>& B)
{
    TMap<UClass*, int64> Totals;
    for (const auto& I : A) { if (!I.ItemClass || I.Amount < 0) return false; Totals.FindOrAdd(I.ItemClass.Get()) += I.Amount; }
    for (const auto& I : B) { if (!I.ItemClass || I.Amount < 0) return false; Totals.FindOrAdd(I.ItemClass.Get()) -= I.Amount; }
    for (const auto& I : Totals) if (I.Value) return false;
    return true;
}
void Tree(AFGHologram* Root, TArray<AFGHologram*>& Out)
{
    Out.Add(Root);
    for (int32 Index = 0; Index < Out.Num() && Out.Num() <= 100000; ++Index)
        if (IsValid(Out[Index])) for (auto* Child : Out[Index]->GetHologramChildren())
            if (IsValid(Child) && Child->GetWorld() == Root->GetWorld()) Out.AddUnique(Child);
    if (Out.Num() > 100000) Out.Reset();
}
bool IsBelt(const FSFConduitPlanEntry& Entry)
{
    return Entry.Kind == ESFConduitPlanKind::Belt || Entry.Kind == ESFConduitPlanKind::StackableBelt;
}
bool NativeRecipe(const FSFConduitPlanEntry& Entry, AFGHologram* Root)
{
    auto* Manager = AFGRecipeManager::Get(Root->GetWorld());
    if (!Entry.BuildClass || !Entry.BuildClass->IsChildOf(AFGBuildableConveyorBelt::StaticClass())
        || !Entry.Recipe || !Manager || !Manager->IsRecipeAvailable(Entry.Recipe)) return false;
    const auto Products = UFGRecipe::GetProducts(Entry.Recipe);
    return Products.Num() == 1 && Products[0].ItemClass
        && Products[0].ItemClass->IsChildOf(UFGBuildingDescriptor::StaticClass())
        && UFGBuildingDescriptor::GetBuildableClass(Products[0].ItemClass.Get()) == Entry.BuildClass;
}
}

FSFConveyorBeltPlan::FRequestScope::FRequestScope()
{
    bEntered = IsInGameThread();
    if (bEntered) ++RequestDepth;
}
FSFConveyorBeltPlan::FRequestScope::~FRequestScope()
{
    if (bEntered && --RequestDepth == 0) FSFConveyorBeltPlan::EndRequest();
}

bool FSFConveyorBeltPlan::HasBelts(const FSFScalingSpec& Spec)
{
    return Spec.ConduitPlan.ContainsByPredicate([](const FSFConduitPlanEntry& E) { return IsBelt(E); });
}

bool FSFConveyorBeltPlan::Prepare(AFGHologram* Root, const FSFScalingSpec& Input, bool bReconstruct)
{
    if (!IsInGameThread() || RequestDepth <= 0 || !IsValid(Root) || !Root->HasAuthority()
        || Root->GetNetMode() == NM_Client || !Root->GetWorld() || Root->GetWorld()->bIsTearingDown) return false;
    if (const auto* Existing = Prepared.Find(Root)) return Existing->bReady;
    FPrepared Initial; Initial.World = Root->GetWorld(); Initial.bReconstructed = bReconstruct;
    for (auto* Child : Root->GetHologramChildren()) if (Child) Initial.OriginalChildren.Add(FObjectKey(Child));
    Prepared.Add(Root, MoveTemp(Initial)); // Recursive admission cannot materialize a second tree.
    auto CaptureGenerated = [&]()
    {
        if (auto* State = Prepared.Find(Root); State && State->bReconstructed)
            for (auto* Child : Root->GetHologramChildren())
                if (Child && !State->OriginalChildren.Contains(FObjectKey(Child))) State->GeneratedChildren.Add(FObjectKey(Child));
    };
    auto Reject = [&](const TCHAR* Why)
    {
        CaptureGenerated();
        Root->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
        UE_LOG(LogSmartFoundations, Warning, TEXT("Conveyor plan rejected before payment: %s."), Why);
        return false;
    };
    FSFScalingSpec Spec = Input;
    TArray<FItemAmount> QuotedEntries;
    for (const auto& Entry : Spec.ConduitPlan)
    {
        if (!Entry.HasBoundedConveyorIntent())
            return Reject(*FString::Printf(TEXT("unbounded_belt_intent_before_preview_spawn kind=%d exact=%d class=%s recipe=%s from=%d/%d to=%d/%d points=%d cost=%d poseFinite=%d"),
                static_cast<int32>(Entry.Kind), Entry.bExactConveyorEndpoints, *GetNameSafe(Entry.BuildClass.Get()), *GetNameSafe(Entry.Recipe.Get()),
                Entry.ConveyorFrom.bPresent, Entry.ConveyorFrom.IsBounded(), Entry.ConveyorTo.bPresent, Entry.ConveyorTo.IsBounded(),
                Entry.SplinePoints.Num(), Entry.Cost.Num(), !Entry.Location.ContainsNaN() && !Entry.Rotation.ContainsNaN()));
        QuotedEntries.Append(Entry.Cost);
    }
    if (!SameCost(QuotedEntries, Spec.ConduitPlanCost)) return Reject(TEXT("aggregate_entry_quote_mismatch"));
    if (bReconstruct)
    {
        if (!Spec.bValid || Spec.BuildClass != Root->GetBuildClass()) return Reject(TEXT("authority_grid_intent_unavailable"));
        bool HasGrid = false;
        for (auto* Child : Root->GetHologramChildren())
            HasGrid |= Child && Child->ActorHasTag(TEXT("SF_GridChild"));
        if (!HasGrid && Spec.CellCount() > 1)
        {
            const int32 Expanded = SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(Root, Spec, Root->GetRecipe());
            if (Expanded != Spec.CellCount() - 1) return Reject(TEXT("incomplete_authority_grid"));
        }
        if (auto* Blueprint = Cast<AFGBlueprintHologram>(Root))
        {
            const FVector Shift = Root->GetActorRotation().RotateVector(
                SFScalingSpecExpansion::MeasureBlueprintContentAnchor(Blueprint) - Spec.ClientParentAnchorRel);
            for (auto& Entry : Spec.ConduitPlan)
            {
                Entry.Location += Shift; Entry.WireStart += Shift; Entry.WireEnd += Shift;
                if (!Entry.PowerStart.bExisting) Entry.PowerStart.OwnerLocation += Shift;
                if (!Entry.PowerEnd.bExisting) Entry.PowerEnd.OwnerLocation += Shift;
                Entry.ConveyorFrom.AddPreviewWorldOffset(Shift); Entry.ConveyorTo.AddPreviewWorldOffset(Shift);
            }
        }
        const int32 Made = SFScalingSpecExpansion::SpawnConduitPlanChildren(Root, Spec);
        int32 Expected = 0;
        for (const auto& E : Spec.ConduitPlan) Expected += E.Kind != ESFConduitPlanKind::Wire;
        if (Made != Expected) return Reject(TEXT("incomplete_authority_conduit_plan"));
    }
    CaptureGenerated(); // Do not adopt children created later by other feature preparation.
    TArray<AFGHologram*> Candidates; Tree(Root, Candidates);
    TSet<ASFConveyorBeltHologram*> Used;
    TSet<UFGFactoryConnectionComponent*> Claims;
    TArray<FPreparedBelt> Belts;
    for (const auto& Entry : Spec.ConduitPlan)
    {
        if (!IsBelt(Entry)) continue;
        if (!Entry.HasBoundedConveyorIntent() || !NativeRecipe(Entry, Root)) return Reject(TEXT("invalid_belt_identity_recipe_or_endpoints"));
        auto* From = Entry.ConveyorFrom.bPresent ? Entry.ConveyorFrom.Resolve(Root) : nullptr;
        auto* To = Entry.ConveyorTo.bPresent ? Entry.ConveyorTo.Resolve(Root) : nullptr;
        if ((Entry.ConveyorFrom.bPresent && !From) || (Entry.ConveyorTo.bPresent && !To)
            || (Entry.Kind == ESFConduitPlanKind::Belt && (!From || !To))) return Reject(TEXT("selected_belt_peer_unavailable"));
        if (From && To && !Entry.ConveyorFrom.IsPlacementAnchor() && !Entry.ConveyorTo.IsPlacementAnchor()
            && From->GetDirection() == To->GetDirection()) return Reject(TEXT("selected_belt_peers_have_same_flow"));
        // Adjacent belts share a positioning anchor. Only transfer mouths are exclusive.
        for (auto* Port : {From, To}) if (Port && Port->GetDirection() != EFactoryConnectionDirection::FCD_SNAP_ONLY)
        { if (Claims.Contains(Port)) return Reject(TEXT("duplicate_belt_mouth_claim")); Claims.Add(Port); }
        ASFConveyorBeltHologram* Found = nullptr;
        for (auto* Candidate : Candidates)
        {
            auto* Belt = Cast<ASFConveyorBeltHologram>(Candidate);
            if (!Belt || Used.Contains(Belt) || Belt->GetBuildClass() != Entry.BuildClass
                || !Belt->GetActorLocation().Equals(Entry.Location, .2)
                || !Belt->GetActorRotation().Equals(Entry.Rotation, .01)) continue;
            const auto& Points = Belt->GetSplineData();
            if (Points.Num() != Entry.SplinePoints.Num()) continue;
            bool Same = true;
            for (int32 I = 0; I < Points.Num(); ++I)
                Same &= Points[I].Location.Equals(Entry.SplinePoints[I].Location, .1)
                    && Points[I].ArriveTangent.Equals(Entry.SplinePoints[I].ArriveTangent, .1)
                    && Points[I].LeaveTangent.Equals(Entry.SplinePoints[I].LeaveTangent, .1);
            if (!Same) continue;
            if (Found) return Reject(TEXT("ambiguous_belt_preview_role")); Found = Belt;
        }
        if (!Found) return Reject(TEXT("selected_belt_preview_missing"));
        Used.Add(Found);
        if (!Found->GetRecipe()) Found->SetRecipe(Entry.Recipe);
        if (Found->GetRecipe() != Entry.Recipe) return Reject(TEXT("selected_belt_recipe_changed"));
        Found->SetSnappedConnections(From, To);
        if (!Found->ValidateNativeConstruction())
        {
            TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
            Found->GetConstructDisqualifiers(Disqualifiers);
            FString Details;
            for (const auto& Disqualifier : Disqualifiers) Details += GetNameSafe(Disqualifier.Get()) + TEXT(";");
            return Reject(*FString::Printf(TEXT("native_belt_placement_rejected: %s"), *Details));
        }
        if (!SameCost(Found->GetNativeConstructionCost(), Entry.Cost))
            return Reject(TEXT("native_belt_quote_changed"));
        Belts.Add({Found, Entry});
    }
    auto* State = Prepared.Find(Root);
    if (!State || State->World.Get() != Root->GetWorld()) return Reject(TEXT("authority_plan_lifetime_changed"));
    State->Belts = MoveTemp(Belts);
    if (bReconstruct) for (const auto& E : Spec.ConduitPlan)
        if (E.Kind == ESFConduitPlanKind::Wire) State->DeferredWireCost.Append(E.Cost);
    State->bReady = true;
    return RefreshCost(Root);
}

bool FSFConveyorBeltPlan::IsPrepared(const AFGHologram* Root)
{
    const auto* State = Prepared.Find(const_cast<AFGHologram*>(Root));
    return State && State->bReady;
}
bool FSFConveyorBeltPlan::IsComputingQuote(const AFGHologram* Root) { return ComputingQuote == Root; }

bool FSFConveyorBeltPlan::RefreshCost(AFGHologram* Root)
{
    auto* State = Prepared.Find(Root);
    if (!State || !State->bReady || !IsValid(Root) || State->World.Get() != Root->GetWorld()) return false;
    const auto DeferredWireCost = State->DeferredWireCost;
    TGuardValue<const AFGHologram*> NativeQuote(ComputingQuote, Root);
    auto Cost = Root->GetCost(true); // Native full tree, no staged-plan multiplication/addition.
    for (const auto& I : DeferredWireCost)
    {
        auto* Existing = Cost.FindByPredicate([&](const FItemAmount& E) { return E.ItemClass == I.ItemClass; });
        if (Existing) Existing->Amount += I.Amount; else Cost.Add(I);
    }
    State = Prepared.Find(Root);
    if (!State) return false;
    State->Cost = MoveTemp(Cost);
    return true;
}
bool FSFConveyorBeltPlan::Revalidate(AFGHologram* Root)
{
    const auto* State = Prepared.Find(Root);
    if (!State || !State->bReady || !IsValid(Root) || State->World.Get() != Root->GetWorld()) return false;
    const auto Belts = State->Belts;
    TSet<UFGFactoryConnectionComponent*> Claims;
    bool Valid = true;
    for (const auto& Binding : Belts)
    {
        auto* Belt = Binding.Preview.Get();
        const auto& E = Binding.Plan;
        if (!IsValid(Belt) || Belt->GetWorld() != Root->GetWorld() || Belt->GetBuildClass() != E.BuildClass
            || Belt->GetRecipe() != E.Recipe || !Belt->GetActorLocation().Equals(E.Location, .2)
            || !Belt->GetActorRotation().Equals(E.Rotation, .01) || !Belt->ValidateNativeConstruction()
            || !SameCost(Belt->GetNativeConstructionCost(), E.Cost)) { Valid = false; break; }
        const auto& Points = Belt->GetSplineData();
        if (Points.Num() != E.SplinePoints.Num()) { Valid = false; break; }
        for (int32 I = 0; I < Points.Num(); ++I)
            Valid &= Points[I].Location.Equals(E.SplinePoints[I].Location, .1)
                && Points[I].ArriveTangent.Equals(E.SplinePoints[I].ArriveTangent, .1)
                && Points[I].LeaveTangent.Equals(E.SplinePoints[I].LeaveTangent, .1);
        auto* From = E.ConveyorFrom.bPresent ? E.ConveyorFrom.Resolve(Root) : nullptr;
        auto* To = E.ConveyorTo.bPresent ? E.ConveyorTo.Resolve(Root) : nullptr;
        Valid &= (!E.ConveyorFrom.bPresent || From) && (!E.ConveyorTo.bPresent || To)
            && Belt->GetSelectedFactoryPeer(0) == From && Belt->GetSelectedFactoryPeer(1) == To;
        for (auto* Port : {From, To}) if (Port && Port->GetDirection() != EFactoryConnectionDirection::FCD_SNAP_ONLY)
        { if (Claims.Contains(Port)) Valid = false; Claims.Add(Port); }
        if (!Valid) break;
    }
    if (!Valid)
    {
        if (auto* Current = Prepared.Find(Root)) Current->bReady = false;
        Root->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
    }
    return Valid;
}

bool FSFConveyorBeltPlan::GetPreparedCost(const AFGHologram* Root, TArray<FItemAmount>& Cost)
{
    const auto* State = Prepared.Find(const_cast<AFGHologram*>(Root));
    if (!State || !State->bReady || ComputingQuote == Root) return false;
    Cost = State->Cost; return true;
}

void FSFConveyorBeltPlan::EndRequest()
{
    auto Retiring = MoveTemp(Prepared);
    Prepared.Reset(); // Native Destroy callbacks cannot invalidate this iteration or see a stale quote.
    for (auto& Entry : Retiring)
    {
        auto* Root = Entry.Key.Get();
        if (!IsValid(Root) || !Entry.Value.bReconstructed || Root->GetWorld() != Entry.Value.World.Get()) continue;
        TStrongObjectPtr<AFGHologram> KeepRoot(Root);
        const auto Children = Root->GetHologramChildren();
        for (auto* Child : Children)
        {
            if (!IsValid(Root) || Root->IsActorBeingDestroyed()) break;
            if (!Child || !Entry.Value.GeneratedChildren.Contains(FObjectKey(Child))) continue;
            Root->mChildren.Remove(Child);
            for (auto It = Root->mChildrenNameLookupMap.CreateIterator(); It; ++It)
                if (It.Value() == Child) It.RemoveCurrent();
            if (IsValid(Child)) Child->Destroy();
        }
    }
}
