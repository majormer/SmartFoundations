// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintRailSeams.h"
#include "SmartFoundations.h"
#include "SFBlueprintRailSourceIdentity.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Hologram/FGBlueprintOpenConnectionManager.h"
#include "Buildables/FGBuildableRailroadTrack.h"
#include "FGConstructDisqualifier.h"
#include "Shared/Conduits/SFNativeRailGeometry.h"
#include "Hologram/FGRailroadSwitchControlHologram.h"
#include "Patching/NativeHookManager.h"
#include "Misc/ScopeExit.h"

// SP/MP DIVERGENCE MAP — repeated Blueprint rail seams
// [MP-CLIENT] Native managers choose and price endpoints; capture exact content identities.
// [MP-AUTH] Recreate the native preview before payment and require the captured decisions,
// geometry and cost to match. Remap only to this construction's exact built counterparts.
namespace
{
using FNativeRailManager = FGBlueprintOpenConnectionManager<UFGRailroadTrackConnectionComponent, AFGRailroadTrackHologram>;
const FName ManagedTag(TEXT("SF_BlueprintRailSeam"));
TMap<TWeakObjectPtr<AFGHologram>, uint32> ManagedNudgeSkips;
bool SamePose(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), .1)
        && A.GetRotation().Equals(B.GetRotation(), .0001)
        && A.GetScale3D().Equals(B.GetScale3D(), .0001);
}
FString OwnerKey(UClass* Class, const FTransform& Pose)
{
    const FVector P = Pose.GetLocation(); const FRotator R = Pose.Rotator();
    return FString::Printf(TEXT("%s|%.4f,%.4f,%.4f|%.4f,%.4f,%.4f"), *GetPathNameSafe(Class), P.X, P.Y, P.Z, R.Pitch, R.Yaw, R.Roll);
}
FString PortKey(const FSFBlueprintRailPort& Port)
{
    return Port.Cell.ToString() + TEXT("|") + OwnerKey(Port.BuildClass, Port.OriginalOwner) + TEXT("|") + Port.ComponentName.ToString();
}
FString EntryKey(const FSFBlueprintRailPlanEntry& Entry)
{
    FString Key = PortKey(Entry.To);
    for (const auto& From : Entry.From) Key += TEXT("|") + PortKey(From);
    return Key;
}
void Merge(TArray<FItemAmount>& Cost, const TArray<FItemAmount>& Extra)
{
    for (const auto& Item : Extra)
    {
        auto* Existing = Cost.FindByPredicate([&](const auto& X) { return X.ItemClass == Item.ItemClass; });
        if (Existing) Existing->Amount += Item.Amount; else Cost.Add(Item);
    }
}
bool SameCost(const TArray<FItemAmount>& A, const TArray<FItemAmount>& B)
{
    TMap<UClass*, int64> Counts;
    for (const auto& Item : A) Counts.FindOrAdd(Item.ItemClass) += Item.Amount;
    for (const auto& Item : B) Counts.FindOrAdd(Item.ItemClass) -= Item.Amount;
    for (const auto& Item : Counts) if (Item.Value) return false;
    return true;
}

}

struct FSFBlueprintRailSeams::FSession
{
    struct FRecord
    {
        TWeakObjectPtr<AFGBlueprintHologram> Blueprint;
        TArray<TWeakObjectPtr<AFGBuildable>> Sources;
        TArray<TWeakObjectPtr<AFGBlueprintHologram>> Neighbors;
        TUniquePtr<FGBlueprintOpenConnectionManagerBase> Native;
        TArray<TWeakObjectPtr<AFGHologram>> Children;
        TArray<TWeakObjectPtr<UFGRailroadTrackConnectionComponent>> Duplicates;
        TArray<TPair<int32, FSFBlueprintRailPlanEntry>> Captured;
        TMap<int32, TWeakObjectPtr<UFGRailroadTrackConnectionComponent>> Bindings;
        TMap<UFGRailroadTrackConnectionComponent*, int32> PreparedSourceIndices;
        bool bConstructed = false;
        FString FrameObservation;
        FNativeRailManager* Typed() { return static_cast<FNativeRailManager*>(Native.Get()); }
    };
    TMap<AFGBlueprintHologram*, FIntVector> Cells;
    TArray<TSharedPtr<FRecord>> Records;
    bool bPrepared = false;
    bool bMappingFailed = false;
    int32 NativeRemapCalls = 0;
    int32 NativeConstructCalls = 0;
    FString NativeFailure;
    void Fail(const FString& Reason) { bMappingFailed = true; if (NativeFailure.IsEmpty()) NativeFailure = Reason; }
    TMap<FString, TWeakObjectPtr<UFGRailroadTrackConnectionComponent>> BuiltPorts;
    TMap<UFGRailroadTrackConnectionComponent*, FSFBlueprintRailPort> PreparedPorts;
    TMap<UFGRailroadTrackConnectionComponent*, TArray<FSplinePointData>> PreparedWorldSplines;
    TMap<UFGRailroadTrackConnectionComponent*, TWeakObjectPtr<UFGRailroadTrackConnectionComponent>> PreviewRemaps;
    TArray<TWeakObjectPtr<AFGBuildable>> BuiltSeams;
    TArray<FItemAmount> PreparedCost;
};

TMap<TWeakObjectPtr<AFGBlueprintHologram>, TSharedPtr<FSFBlueprintRailSeams::FSession>>& FSFBlueprintRailSeams::Sessions()
{
    static TMap<TWeakObjectPtr<AFGBlueprintHologram>, TSharedPtr<FSession>> Value;
    return Value;
}

FSFBlueprintRailPort FSFBlueprintRailSeams::CapturePort(UFGRailroadTrackConnectionComponent* Port, const TMap<AFGBlueprintHologram*, FIntVector>& Cells)
{
    FSFBlueprintRailPort Result;
    auto* Blueprint = Port ? Cast<AFGBlueprintHologram>(Port->GetOwner()) : nullptr;
    const FIntVector* Cell = Cells.Find(Blueprint);
    auto* Original = Blueprint ? Cast<UFGRailroadTrackConnectionComponent>(Blueprint->mDuplicateConnectionToOriginalMap.FindRef(Port)) : nullptr;
    auto* Owner = Original ? Cast<AFGBuildableRailroadTrack>(Original->GetOwner()) : nullptr;
    if (!Cell || !Owner) return Result;
    Result.Cell = *Cell; Result.BuildClass = Owner->GetClass(); Result.OriginalOwner = Owner->GetActorTransform();
    Result.ComponentName = Original->GetFName();
    Result.OwnerPose = Original->GetComponentTransform().GetRelativeTransform(Result.OriginalOwner).Inverse() * Port->GetComponentTransform();
    return Result;
}

void FSFBlueprintRailSeams::Discard(FSession& Session)
{
    for (auto& R : Session.Records)
    {
        if (R->Native) R->Native->ResetAutomaticConnections();
        if (auto* B = R->Blueprint.Get())
        {
            for (auto& Child : R->Children) if (auto* C = Child.Get())
            {
                B->mChildren.Remove(C);
                ManagedNudgeSkips.Remove(C);
                for (auto It = B->mChildrenNameLookupMap.CreateIterator(); It; ++It) if (It.Value() == C) It.RemoveCurrent();
                C->Destroy();
            }
            for (auto& Port : R->Duplicates) if (auto* C = Port.Get())
            {
                B->mDuplicateConnectionToOriginalMap.Remove(C);
                C->DestroyComponent();
            }
        }
        R->Native.Reset();
    }
    Session.Records.Reset(); Session.Cells.Reset();
}

void FSFBlueprintRailSeams::Reset(AFGHologram* Root)
{
    auto* Blueprint = Cast<AFGBlueprintHologram>(Root);
    if (auto* S = Sessions().Find(Blueprint)) { Discard(**S); Sessions().Remove(Blueprint); }
}
void FSFBlueprintRailSeams::ResetAll()
{
    for (auto& Pair : Sessions()) Discard(*Pair.Value);
    Sessions().Reset();
    ManagedNudgeSkips.Reset();
}

void FSFBlueprintRailSeams::Update(AFGBlueprintHologram* Root)
{
    if (!IsValid(Root)) return;
    for (auto It = Sessions().CreateIterator(); It; ++It)
        if (!It.Key().IsValid()) { Discard(*It.Value()); It.RemoveCurrent(); }
    TMap<AFGBlueprintHologram*, FIntVector> Cells;
    Cells.Add(Root, FIntVector::ZeroValue);
    for (AFGHologram* Child : Root->GetHologramChildren())
    {
        FIntVector Cell;
        if (auto* B = Cast<AFGBlueprintHologram>(Child); B && USFGridCoordComponent::TryGetCell(B, Cell)) Cells.Add(B, Cell);
    }
    if (Cells.Num() < 2) { Reset(Root); return; }
    auto& Session = Sessions().FindOrAdd(Root);
    bool SameCells = Session && Session->Cells.Num() == Cells.Num();
    if (SameCells) for (const auto& Pair : Cells)
    {
        const auto* Previous = Session->Cells.Find(Pair.Key);
        if (!Previous || *Previous != Pair.Value) { SameCells = false; break; }
    }
    if (SameCells) for (const auto& R : Session->Records)
        for (const auto& Source : R->Sources) if (!Source.IsValid()) SameCells = false;
    if (Session && Session->bPrepared) return;
    if (!SameCells)
    {
        if (Session) Discard(*Session);
        Session = MakeShared<FSession>(); Session->Cells = Cells;
        TArray<AFGBlueprintHologram*> Ordered; Cells.GenerateKeyArray(Ordered);
        Ordered.Sort([&](const auto& A, const auto& B) { return Cells[const_cast<AFGBlueprintHologram*>(&A)].ToString() < Cells[const_cast<AFGBlueprintHologram*>(&B)].ToString(); });
        for (auto* B : Ordered)
        {
            const FIntVector Cell = Cells[B];
            TArray<AFGBlueprintHologram*> Neighbors;
            for (const FIntVector Step : {FIntVector(1,0,0), FIntVector(0,1,0), FIntVector(0,0,1)})
                for (const auto& Pair : Cells) if (Pair.Value == Cell - Step) Neighbors.Add(Pair.Key);
            if (Neighbors.IsEmpty()) continue;
            TArray<AFGBuildable*> Sources;
            for (const auto& Pair : B->mBuildableToNewRoot) if (auto* Rail = Cast<AFGBuildableRailroadTrack>(Pair.Key.Get())) Sources.Add(Rail);
            if (Sources.IsEmpty()) continue;
            Sources.Sort([](const auto& A, const auto& B) { return OwnerKey(A.GetClass(), A.GetActorTransform()) < OwnerKey(B.GetClass(), B.GetActorTransform()); });
            auto Record = MakeShared<FSession::FRecord>(); Record->Blueprint = B;
            for (auto* Source : Sources) Record->Sources.Add(Source);
            TInlineComponentArray<UFGRailroadTrackConnectionComponent*> Before(B);
            const auto ChildrenBeforeInitialize = B->GetHologramChildren();
            // This is the same private-inheritance upcast used by the native
            // AFGBlueprintHologram::RegisterOpenConnectionManager template.
            Record->Native.Reset(reinterpret_cast<FGBlueprintOpenConnectionManagerBase*>(new FGBlueprintOpenRailroadConnectionManager(B)));
            Record->Native->mOnConnectionStateChanged.AddUObject(B, &AFGBlueprintHologram::OnOpenConnectionStateChanged);
            Record->Native->Initialize(Sources);
            for (auto* Child : B->GetHologramChildren()) if (Child && !ChildrenBeforeInitialize.Contains(Child))
            {
                Child->Tags.AddUnique(ManagedTag); Record->Children.AddUnique(Child);
            }
            TInlineComponentArray<UFGRailroadTrackConnectionComponent*> After(B);
            for (auto* C : After) if (!Before.Contains(C)) Record->Duplicates.Add(C);
            for (auto* Neighbor : Neighbors) { Record->Neighbors.Add(Neighbor); Record->Native->RegisterNearbyActor(Neighbor); }
            Session->Records.Add(Record);
        }
    }
    for (auto& R : Session->Records)
    {
        auto* B = R->Blueprint.Get(); if (!B) continue;
        const auto Before = B->GetHologramChildren();
        // Once selected, endpoints retain their identity through spacing changes.
        // Invalid geometry goes dormant rather than switching to a different lane.
        auto& States = const_cast<TArray<FNativeRailManager::OpenConnectionState>&>(R->Typed()->GetOpenConnectionStates());
        for (int32 I = 0; I < States.Num(); ++I)
            if (const auto* Binding = R->Bindings.Find(I); Binding && Binding->IsValid())
            {
                States[I].TargetConnection = Binding->Get(); States[I].HasSnappedTargetConnection = true;
            }
        // Bounded passive observation: retain the first routed world-frame failure.
        // Report through the existing diagnostic query, never per-frame Shipping logs.
        auto Frame = [](const AFGRailroadTrackHologram* Bridge)
        {
            if (!Bridge) return FString(TEXT("none"));
            const auto* RootComponent = Bridge->GetRootComponent();
            const auto* Spline = Bridge->mSplineComponent.Get();
            return FString::Printf(TEXT("actor=%s root=%s mobility=%d absolute=%d attach=%s relative=%s spline=%s locked=%d"),
                *Bridge->GetActorLocation().ToString(), *GetNameSafe(RootComponent),
                RootComponent ? static_cast<int32>(RootComponent->Mobility) : -1,
                RootComponent && RootComponent->IsUsingAbsoluteLocation(),
                RootComponent ? *GetNameSafe(RootComponent->GetAttachParent()) : TEXT("none"),
                RootComponent ? *RootComponent->GetRelativeLocation().ToString() : TEXT("none"),
                Spline ? *Spline->GetComponentLocation().ToString() : TEXT("none"), Bridge->IsHologramLocked());
        };
        FString BeforeNative;
        if (R->FrameObservation.IsEmpty())
            for (const auto& State : States) if (State.BridgeHologram) BeforeNative += Frame(State.BridgeHologram);
        bool PlaySnap = false; R->Native->UpdateAutomaticConnections(FHitResult(), PlaySnap);
        for (int32 I = 0; I < States.Num(); ++I)
        {
            auto& State = States[I];
            if (IsValid(State.TargetConnection)) R->Bindings.FindOrAdd(I) = State.TargetConnection;
            // The native manager can select a duplicated Blueprint endpoint, but
            // its bridge's aim path expects a built track owner. Route only that
            // exact selected preview pair with the ordinary native endpoint curve.
            // Retain its snapped components, switch children and native constructor.
            auto* TargetBlueprint = State.TargetConnection ? Cast<AFGBlueprintHologram>(State.TargetConnection->GetOwner()) : nullptr;
            if (!State.CanDirectlyConnect && IsValid(State.BridgeHologram)
                && TargetBlueprint && Session->Cells.Contains(TargetBlueprint))
            {
                auto* Bridge = State.BridgeHologram;
                const FVector Start = Bridge->mSnappedStartLocation;
                const FVector Tangent = Bridge->mSnappedStartTangent.GetSafeNormal();
                const bool NativeSource = !Tangent.IsNearlyZero() && !State.OpenConnections.IsEmpty()
                    && !State.OpenConnections.ContainsByPredicate([&](const auto* P)
                        { return !P || !IsValid(P->Connection) || !P->Connection->GetComponentLocation().Equals(Start, .1); });
                const bool Observe = R->FrameObservation.IsEmpty();
                FString Observation = Observe ? TEXT("beforeNative[") + BeforeNative + TEXT("] afterNative[") + Frame(Bridge) + TEXT("]") : FString();
                bool Valid = NativeSource;
                if (Valid)
                {
                    const FTransform From((-Tangent).Rotation(), Start);
                    const FTransform To(State.TargetConnection->GetComponentQuat(), State.TargetConnection->GetComponentLocation());
                    FSFNativeRailGeometry::RefreshPreview(Bridge, From, To);
                    if (Observe) Observation += TEXT(" afterRoute[") + Frame(Bridge) + TEXT("]");
                    // A Blueprint child's component refresh can restore its actor
                    // to the template origin. Apply the routed world frame after
                    // that refresh, before native placement/clearance validation.
                    const bool FrameMoved = Bridge->SetActorLocationAndRotation(Start, FRotator::ZeroRotator);
                    if (Observe) Observation += FString::Printf(TEXT(" expected=%s setter=%d afterSetter[%s]"), *Start.ToString(), FrameMoved, *Frame(Bridge));
                    auto MatchesEndpoints = [&]()
                    {
                        auto* Spline = Bridge->mSplineComponent.Get();
                        const int32 Last = Spline ? Spline->GetNumberOfSplinePoints() - 1 : -1;
                        return Last >= 1
                            && Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World).Equals(Start, .1)
                            && Spline->GetLocationAtSplinePoint(Last, ESplineCoordinateSpace::World).Equals(To.GetLocation(), .1);
                    };
                    Valid = MatchesEndpoints() && FSFNativeRailGeometry::Validate(Bridge).IsEmpty();
                    if (Valid)
                    {
                        Bridge->SetDisabled(false);
                        Bridge->CheckValidPlacement();
                        if (Observe) Observation += TEXT(" afterPlacement[") + Frame(Bridge) + TEXT("]");
                        Valid = Bridge->CanConstruct() && MatchesEndpoints();
                    }
                }
                Bridge->SetDisabled(!Valid);
                if (Valid) Bridge->SetActorHiddenInGame(false);
                State.IsValid = Valid;
                if (State.IsValid != State.PreviousIsValid)
                {
                    TArray<UFGConnectionComponent*> Connections;
                    for (const auto* P : State.OpenConnections) Connections.Add(P->Connection);
                    R->Native->mOnConnectionStateChanged.Broadcast(Connections, State.PreviousTargetConnection, State.TargetConnection, State.IsValid);
                    State.PreviousIsValid = State.IsValid;
                }
                if (Observe)
                {
                    Observation += FString::Printf(TEXT(" nativeSource=%d valid=%d cachedStart=%s cachedTangent=%s sources=%d"),
                        NativeSource, Valid, *Start.ToString(), *Tangent.ToString(), State.OpenConnections.Num());
                    for (const auto* P : State.OpenConnections)
                        if (P && P->Connection) Observation += TEXT(" source[") + P->Connection->GetComponentTransform().ToString() + TEXT("]");
                    R->FrameObservation = Observation + TEXT(" afterDelegate[") + Frame(Bridge) + TEXT("]");
                }
            }
            if (!TargetBlueprint && !State.CanDirectlyConnect && IsValid(State.BridgeHologram)
                && !FSFNativeRailGeometry::Validate(State.BridgeHologram).IsEmpty())
            {
                State.BridgeHologram->SetDisabled(true); State.IsValid = false;
            }
        }
        for (auto* Child : B->GetHologramChildren()) if (Child && !Before.Contains(Child))
        {
            Child->Tags.AddUnique(ManagedTag); R->Children.AddUnique(Child);
        }
    }
}

void FSFBlueprintRailSeams::Capture(AFGHologram* Root, TArray<FSFBlueprintRailPlanEntry>& Plan, TArray<FItemAmount>& Cost)
{
    Plan.Reset(); Cost.Reset();
    const auto* S = Sessions().Find(Cast<AFGBlueprintHologram>(Root)); if (!S) return;
    for (auto& R : (*S)->Records)
    {
        R->Captured.Reset();
        const auto& States = R->Typed()->GetOpenConnectionStates();
        for (int32 Index = 0; Index < States.Num(); ++Index)
        {
            const auto& State = States[Index];
            if (!State.IsValid || !IsValid(State.TargetConnection)) continue;
            FSFBlueprintRailPlanEntry Entry; Entry.bDirect = State.CanDirectlyConnect;
            Entry.To = CapturePort(State.TargetConnection, (*S)->Cells);
            for (const auto* From : State.OpenConnections) Entry.From.Add(CapturePort(From->Connection, (*S)->Cells));
            if (!Entry.To.BuildClass || Entry.From.ContainsByPredicate([](const auto& P) { return !P.BuildClass; })) continue;
            Entry.From.Sort([](const auto& A, const auto& B) { return PortKey(A) < PortKey(B); });
            if (!Entry.bDirect && IsValid(State.BridgeHologram))
            {
                auto* Bridge = State.BridgeHologram;
                auto* Spline = Bridge->mSplineComponent.Get();
                const int32 Last = Spline ? Spline->GetNumberOfSplinePoints() - 1 : -1;
                const bool Matches = Last >= 1 && !State.OpenConnections.IsEmpty()
                    && Spline->GetLocationAtSplinePoint(Last, ESplineCoordinateSpace::World).Equals(State.TargetConnection->GetComponentLocation(), .1)
                    && !State.OpenConnections.ContainsByPredicate([&](const auto* P)
                        { return !P || !IsValid(P->Connection) || !Spline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::World).Equals(P->Connection->GetComponentLocation(), .1); });
                if (Bridge->IsDisabled() || !Matches)
                {
                    // A late native preview refresh must not serialize or charge
                    // an origin-based spline merely because its length is valid.
                    Bridge->SetDisabled(true);
                    continue;
                }
                Entry.BridgePose = State.BridgeHologram->GetActorTransform(); Entry.Spline = State.BridgeHologram->GetSplineData();
            }
            R->Captured.Emplace(Index, Entry); Plan.Add(MoveTemp(Entry));
        }
        for (const auto& Child : R->Children) if (Child.IsValid() && !Child->IsDisabled()) Merge(Cost, Child->GetCost(false));
    }
    Plan.Sort([](const auto& A, const auto& B) { return EntryKey(A) < EntryKey(B); });
}

bool FSFBlueprintRailSeams::IsBounded(const TArray<FSFBlueprintRailPlanEntry>& Plan)
{
    if (Plan.Num() > 4096) return false;
    int64 PortCount = 0;
    TSet<FString> Entries;
    for (const auto& Entry : Plan)
    {
        PortCount += Entry.From.Num() + 1;
        if (PortCount > 4096) return false;
        if (Entry.From.IsEmpty() || Entry.From.Num() > 4096 || Entry.Spline.Num() > 64
            || (!Entry.bDirect && Entry.Spline.Num() < 2) || Entry.BridgePose.ContainsNaN()) return false;
        auto ValidPort = [](const FSFBlueprintRailPort& P) { return P.BuildClass && P.BuildClass->IsChildOf(AFGBuildableRailroadTrack::StaticClass()) && !P.ComponentName.IsNone() && !P.OriginalOwner.ContainsNaN() && !P.OwnerPose.ContainsNaN(); };
        if (!ValidPort(Entry.To)) return false;
        TSet<FString> Sources;
        for (const auto& From : Entry.From)
        {
            const int64 DX = static_cast<int64>(From.Cell.X) - Entry.To.Cell.X;
            const int64 DY = static_cast<int64>(From.Cell.Y) - Entry.To.Cell.Y;
            const int64 DZ = static_cast<int64>(From.Cell.Z) - Entry.To.Cell.Z;
            if (!ValidPort(From) || From.Cell != Entry.From[0].Cell
                || FMath::Abs(DX) + FMath::Abs(DY) + FMath::Abs(DZ) != 1 || Sources.Contains(PortKey(From))) return false;
            Sources.Add(PortKey(From));
        }
        const FString Key = EntryKey(Entry);
        if (Entries.Contains(Key)) return false;
        Entries.Add(Key);
        for (const auto& Point : Entry.Spline) if (Point.Location.ContainsNaN() || Point.ArriveTangent.ContainsNaN() || Point.LeaveTangent.ContainsNaN()) return false;
    }
    return true;
}

bool FSFBlueprintRailSeams::SamePortIdentity(const FSFBlueprintRailPort& A, const FSFBlueprintRailPort& B)
{
    // Euler text is not an identity: equivalent native transforms can print
    // different signed zeroes, wrapped angles or floating-point roundoff.
    return A.Cell == B.Cell && A.BuildClass == B.BuildClass && A.ComponentName == B.ComponentName
        && SamePose(A.OriginalOwner, B.OriginalOwner);
}

bool FSFBlueprintRailSeams::Prepare(AFGBlueprintHologram* Root, const FSFScalingSpec& Spec, FString* Failure)
{
    auto Reject = [&](const FString& Reason) { if (Failure) *Failure = Reason; return false; };
    if (Failure) Failure->Reset();
    if (!Root || !IsBounded(Spec.BlueprintRailSeams)) return Reject(TEXT("invalid root or bounded plan"));
    if (Root->mBuildableToNewRoot.IsEmpty()) return Reject(TEXT("native root Blueprint content is not staged"));
    if (!Root->ActorHasTag(TEXT("SF_BlueprintRailPlanExpanded")))
    {
        bool HasGridChild = false;
        for (auto* Child : Root->GetHologramChildren())
        {
            FIntVector Cell;
            if (USFGridCoordComponent::TryGetCell(Child, Cell)) { HasGridChild = true; break; }
        }
        if (!HasGridChild) SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(Root, Spec, Root->GetRecipe());
        FSFScalingSpec Translated = Spec;
        const FVector Shift = Root->GetActorRotation().RotateVector(
            SFScalingSpecExpansion::MeasureBlueprintContentAnchor(Root) - Spec.ClientParentAnchorRel);
        for (auto& Entry : Translated.ConduitPlan)
        {
            Entry.Location += Shift; Entry.WireStart += Shift; Entry.WireEnd += Shift;
            if (!Entry.PowerStart.bExisting) Entry.PowerStart.OwnerLocation += Shift;
            if (!Entry.PowerEnd.bExisting) Entry.PowerEnd.OwnerLocation += Shift;
        }
        // Local grids already own their conduit previews. Only the stripped,
        // authority-reconstructed grid needs its captured conduit plan spawned.
        if (!HasGridChild) SFScalingSpecExpansion::SpawnConduitPlanChildren(Root, Translated);
        Root->Tags.AddUnique(TEXT("SF_BlueprintRailPlanExpanded"));
    }
    if (auto* S = Sessions().Find(Root)) (*S)->bPrepared = false;
    for (auto* Child : Root->GetHologramChildren())
        if (auto* Copy = Cast<AFGBlueprintHologram>(Child))
            if (Copy->mBlueprintDescriptor != Root->mBlueprintDescriptor
                || Copy->mBuildableToNewRoot.Num() != Root->mBuildableToNewRoot.Num())
                return Reject(TEXT("native Blueprint copy content differs from the initiating copy"));
    Update(Root);
    auto* Restored = Sessions().Find(Root);
    if (!Restored) return Reject(TEXT("no native grid rail session"));
    if (!RestoreBindings(**Restored, Spec.BlueprintRailSeams, Failure)) return false;
    Update(Root);
    TArray<FSFBlueprintRailPlanEntry> Actual; TArray<FItemAmount> Cost; Capture(Root, Actual, Cost);
    const FVector Shift = Root->GetActorRotation().RotateVector(SFScalingSpecExpansion::MeasureBlueprintContentAnchor(Root) - Spec.ClientParentAnchorRel);
    bool Matches = Actual.Num() == Spec.BlueprintRailSeams.Num() && SameCost(Cost, Spec.BlueprintRailCost);
    for (int32 I = 0; Matches && I < Actual.Num(); ++I)
    {
        const auto& A = Actual[I]; const auto& E = Spec.BlueprintRailSeams[I];
        auto PortMatches = [&](const auto& P, const auto& Q)
        {
            FTransform Expected = Q.OwnerPose; Expected.AddToTranslation(Shift);
            return SamePortIdentity(P, Q) && SamePose(P.OwnerPose, Expected);
        };
        Matches = A.bDirect == E.bDirect && A.From.Num() == E.From.Num() && PortMatches(A.To, E.To) && A.Spline.Num() == E.Spline.Num();
        for (int32 J = 0; Matches && J < A.From.Num(); ++J) Matches = PortMatches(A.From[J], E.From[J]);
        if (Matches && !E.bDirect)
        {
            FTransform Expected = E.BridgePose; Expected.AddToTranslation(Shift); Matches = SamePose(A.BridgePose, Expected);
            for (int32 J = 0; Matches && J < A.Spline.Num(); ++J)
                Matches = A.Spline[J].Location.Equals(E.Spline[J].Location, .1) && A.Spline[J].ArriveTangent.Equals(E.Spline[J].ArriveTangent, .1) && A.Spline[J].LeaveTangent.Equals(E.Spline[J].LeaveTangent, .1);
        }
    }
    if (auto* Session = Sessions().Find(Root))
    {
        auto& Prepared = **Session;
        Prepared.bPrepared = Matches;
        Prepared.PreparedCost.Reset();
        Prepared.PreparedPorts.Reset();
        Prepared.PreparedWorldSplines.Reset();
        for (const auto& R : Prepared.Records) R->PreparedSourceIndices.Reset();
        if (Matches)
            for (const auto& Cell : Prepared.Cells)
                for (const auto& Pair : Cell.Key->mDuplicateConnectionToOriginalMap)
                    if (auto* Preview = Cast<UFGRailroadTrackConnectionComponent>(Pair.Key.Get()))
                    {
                        const auto Identity = CapturePort(Preview, Prepared.Cells);
                        if (!Identity.BuildClass) continue;
                        Prepared.PreparedPorts.Add(Preview, Identity);
                        const auto* Original = Cast<UFGRailroadTrackConnectionComponent>(Pair.Value.Get());
                        if (const auto* Rail = Original ? Cast<AFGBuildableRailroadTrack>(Original->GetOwner()) : nullptr)
                            Prepared.PreparedWorldSplines.Add(Preview,
                                SFBlueprintRailSourceIdentity::InWorld(Rail->GetSplinePointData(), Identity.OwnerPose));
                        for (const auto& R : Prepared.Records) if (R->Blueprint.Get() == Cell.Key)
                            for (int32 I = 0; I < R->Sources.Num(); ++I)
                                if (Original && Original->GetOwner() == R->Sources[I].Get()) R->PreparedSourceIndices.Add(Preview, I);
                    }
        if (Matches)
        {
            // Freeze the native self-only costs while the complete validated
            // tree is present. Parent placement validation temporarily hides it.
            Prepared.PreparedCost = Root->GetCost(false);
            TArray<AFGHologram*> Pending = Root->GetHologramChildren();
            TSet<AFGHologram*> Seen; Seen.Add(Root);
            while (!Pending.IsEmpty())
            {
                auto* Child = Pending.Pop();
                if (!IsValid(Child) || Child->IsDisabled() || Seen.Contains(Child)) continue;
                Seen.Add(Child);
                Merge(Prepared.PreparedCost, Child->GetCost(false));
                Pending.Append(Child->GetHologramChildren());
            }
        }
    }
    if (!Matches && Failure) *Failure = FString::Printf(TEXT("native replay differs: seams=%d expected=%d costMatch=%d shift=%s"),
        Actual.Num(), Spec.BlueprintRailSeams.Num(), SameCost(Cost, Spec.BlueprintRailCost), *Shift.ToCompactString());
    return Matches;
}
bool FSFBlueprintRailSeams::IsPrepared(AFGHologram* Root)
{
    const auto* S = Sessions().Find(Cast<AFGBlueprintHologram>(Root)); return S && (*S)->bPrepared;
}

bool FSFBlueprintRailSeams::GetPreparedCost(const AFGHologram* Root, TArray<FItemAmount>& Cost)
{
    if (!Root || !Root->HasAuthority()) return false;
    const auto* Session = Sessions().Find(Cast<AFGBlueprintHologram>(const_cast<AFGHologram*>(Root)));
    if (!Session || !(*Session)->bPrepared || (*Session)->bMappingFailed || (*Session)->PreparedCost.IsEmpty()) return false;
    Cost = (*Session)->PreparedCost;
    return true;
}

FString FSFBlueprintRailSeams::DescribeNativeProgress(AFGBlueprintHologram* Root)
{
    const auto* S = Sessions().Find(Root);
    if (!S) return TEXT("session=none");
    FString Result = FString::Printf(TEXT("remaps=%d constructCalls=%d ports=%d failed=%d reason=%s"),
        (*S)->NativeRemapCalls, (*S)->NativeConstructCalls, (*S)->BuiltPorts.Num(),
        (*S)->bMappingFailed, *(*S)->NativeFailure);
    for (const auto& Record : (*S)->Records)
    {
        if (!Record->FrameObservation.IsEmpty()) Result += TEXT(" frame{") + Record->FrameObservation + TEXT("}");
        uint32 Skips = 0;
        for (const auto& Child : Record->Children)
            if (const auto* Count = ManagedNudgeSkips.Find(Child)) Skips += *Count;
        Result += FString::Printf(TEXT(" managedNudgeSkips=%u"), Skips);
    }
    return Result;
}

AActor* FSFBlueprintRailSeams::ConstructCopy(AFGBlueprintHologram* Copy, TFunctionRef<AActor*()> NativeConstruct)
{
    bool PreparedCopy = false;
    for (const auto& Pair : Sessions())
        if (Pair.Value->bPrepared && Pair.Value->Cells.Contains(Copy)) { PreparedCopy = true; break; }
    if (!PreparedCopy || !Copy->HasAuthority() || Copy->AreAutomaticConnectionsEnabled()) return NativeConstruct();

    // Default mode normally skips the entire native remap/construction phase.
    // Run that phase for the already validated Smart plan, without discovering
    // additional external connections or changing the player's selected mode.
    // These are the copy's ordinary managers, not our separate priced managers.
    // Reset keeps their source identities while clearing unpriced target/bridge
    // state; the native rail callback then constructs only our prepared seams.
    for (const auto& Manager : Copy->mOpenConnectionManagers) Manager->ResetAutomaticConnections();
    const auto PreviousOverride = Copy->mBuildModeOverride;
    Copy->mBuildModeOverride = Copy->mBlueprintAutoConnectBuildMode;
    ON_SCOPE_EXIT { Copy->mBuildModeOverride = PreviousOverride; };
    return NativeConstruct();
}

void FSFBlueprintRailSeams::InstallNativeHooks()
{
#if !WITH_EDITOR
    static bool Installed = false;
    if (Installed) return;
    Installed = true;
    SUBSCRIBE_METHOD_VIRTUAL(AFGHologram::SetHologramNudgeLocation, GetMutableDefault<AFGHologram>(),
        [](auto& Scope, AFGHologram* Self)
        {
            if (IsValid(Self) && (Self->ActorHasTag(ManagedTag) || Self->ActorHasTag(TEXT("SF_BlueprintVehicleSeam"))))
            {
                // Native parent placement also nudges its children. These seam
                // previews already have a routed world frame, not player nudge
                // state. SML forwards an early return unless explicitly cancelled.
                auto& Count = ManagedNudgeSkips.FindOrAdd(Self);
                Count = FMath::Min(Count + 1, 1000000u);
                Scope.Cancel();
                return;
            }
            Scope(Self);
        });
    auto* Sample = new FGBlueprintOpenRailroadConnectionManager(nullptr);
    TUniquePtr<FGBlueprintOpenConnectionManagerBase> Base(reinterpret_cast<FGBlueprintOpenConnectionManagerBase*>(Sample));
    SUBSCRIBE_METHOD_VIRTUAL(FGBlueprintOpenConnectionManagerBase::HandleBuildableConnectionRemapping, Base.Get(),
        [](auto& Scope, FGBlueprintOpenConnectionManagerBase* Self, AFGBuildable* Built, int32 Index)
        {
            ObserveNativeRemap(Self, Built, Index);
            Scope(Self, Built, Index);
        });
    SUBSCRIBE_METHOD_VIRTUAL(FGBlueprintOpenRailroadConnectionManager::Construct, Sample,
        [](auto& Scope, FGBlueprintOpenRailroadConnectionManager* Self, TArray<AFGBuildable*>& Built, FNetConstructionID ID)
        {
            ConstructAtNativeBoundary(reinterpret_cast<FGBlueprintOpenConnectionManagerBase*>(Self), Built, ID);
            Scope(Self, Built, ID);
        });
#endif
}

void FSFBlueprintRailSeams::ObserveNativeRemap(FGBlueprintOpenConnectionManagerBase* Manager, AFGBuildable* Built, int32 Index)
{
    auto* Blueprint = Manager ? Manager->GetHologram() : nullptr;
    if (!Blueprint || !Blueprint->HasAuthority() || !IsValid(Built)) return;
    for (auto& Pair : Sessions())
    {
        auto& Session = *Pair.Value;
        if (!Session.Cells.Contains(Blueprint)) continue;
        for (const auto& R : Session.Records) if (R->Native.Get() == Manager) return;
        ++Session.NativeRemapCalls;
        auto* Native = static_cast<FNativeRailManager*>(Manager);
        TSet<UFGRailroadTrackConnectionComponent*> Remapped;
        TInlineComponentArray<UFGRailroadTrackConnectionComponent*> ActualPorts(Built);
        for (const auto& State : Native->GetOpenConnectionStates())
            for (const auto* Port : State.OpenConnections)
            {
                if (Port->BlueprintBuildableIndex != Index) continue;
                // Native manager initialization/remapping can restore preview
                // components to template-local coordinates. Their live pose is
                // no longer the validated construction frame at this boundary.
                const auto* Prepared = Session.PreparedPorts.Find(Port->Connection);
                if (!Prepared) { Session.Fail(TEXT("native remap has no prevalidated preview identity")); continue; }
                const auto& Identity = *Prepared;
                const auto* ExpectedSpline = Session.PreparedWorldSplines.Find(Port->Connection);
                const auto* ActualPort = ActualPorts.IsValidIndex(Port->ConnectionIndex) ? ActualPorts[Port->ConnectionIndex] : nullptr;
                const auto* ActualRail = Cast<AFGBuildableRailroadTrack>(Built);
                const bool MatchesFrame = ExpectedSpline && ActualRail && SFBlueprintRailSourceIdentity::Matches(
                    Identity.OwnerPose, *ExpectedSpline, Built->GetActorTransform(), ActualRail->GetSplinePointData());
                if (!Identity.BuildClass || Built->GetClass() != Identity.BuildClass
                    || !MatchesFrame || !ActualPort || ActualPort->GetFName() != Identity.ComponentName)
                {
                    Session.Fail(FString::Printf(TEXT("native source remap index=%d component=%d prepared=%d class=%d pose=%d actualPorts=%d expected=%s actual=%s"),
                        Index, Port->ConnectionIndex, Prepared != nullptr, Built->GetClass() == Identity.BuildClass,
                        MatchesFrame, ActualPorts.Num(),
                        *Identity.OwnerPose.ToHumanReadableString(), *Built->GetActorTransform().ToHumanReadableString()));
                    continue;
                }
                auto* Actual = ActualPorts[Port->ConnectionIndex];
                const FString Key = PortKey(Identity);
                if (const auto* Previous = Session.BuiltPorts.Find(Key); Previous && Previous->Get() != Actual)
                { Session.Fail(TEXT("native remap assigned two actors to one content port")); continue; }
                Session.BuiltPorts.Add(Key, Actual);
                // Include every native preview alias of this exact component,
                // including the additional manager's own duplicated representation.
                for (const auto& Alias : Session.PreparedPorts)
                    if (SamePortIdentity(Alias.Value, Identity)) Session.PreviewRemaps.Add(Alias.Key, Actual);
                Remapped.Add(Port->Connection);
            }
        for (const auto& R : Session.Records) if (R->Blueprint.Get() == Blueprint)
        {
            TSet<int32> Indices;
            for (auto* Preview : Remapped) if (const auto* I = R->PreparedSourceIndices.Find(Preview)) Indices.Add(*I);
            for (int32 I : Indices) R->Native->HandleBuildableConnectionRemapping(Built, I);
        }
        return;
    }
}

void FSFBlueprintRailSeams::ConstructAtNativeBoundary(FGBlueprintOpenConnectionManagerBase* Manager, TArray<AFGBuildable*>& Built, FNetConstructionID ID)
{
    auto* Blueprint = Manager ? Manager->GetHologram() : nullptr;
    if (!Blueprint || !Blueprint->HasAuthority()) return;
    for (auto& Pair : Sessions())
    {
        auto& Session = *Pair.Value;
        if (!Session.Cells.Contains(Blueprint)) continue;
        for (const auto& R : Session.Records) if (R->Native.Get() == Manager) return;
        ++Session.NativeConstructCalls;
        for (const auto& R : Session.Records)
        {
            if (R->Blueprint.Get() != Blueprint || R->bConstructed) continue;
            if (Session.bMappingFailed) return;
            auto& States = const_cast<TArray<FNativeRailManager::OpenConnectionState>&>(R->Typed()->GetOpenConnectionStates());
            for (const auto& Captured : R->Captured)
            {
                const auto* Target = Session.BuiltPorts.Find(PortKey(Captured.Value.To));
                if (!States.IsValidIndex(Captured.Key) || !Target || !Target->IsValid())
                { Session.Fail(TEXT("native construction target has no mapped content port")); return; }
                auto& State = States[Captured.Key];
                State.TargetConnection = Target->Get();
                State.HasSnappedTargetConnection = true;
                State.CanDirectlyConnect = Captured.Value.bDirect;
                for (const auto& From : Captured.Value.From)
                {
                    const auto* Actual = Session.BuiltPorts.Find(PortKey(From));
                    if (!Actual || !Actual->IsValid()
                        || !State.OpenConnections.ContainsByPredicate([&](const auto* P) { return P->Connection == Actual->Get(); }))
                    { Session.Fail(TEXT("native construction source has no mapped content port")); return; }
                }
                if (!State.CanDirectlyConnect && !FSFNativeRailGeometry::Validate(State.BridgeHologram).IsEmpty())
                { Session.Fail(TEXT("native construction bridge geometry changed")); return; }
            }
            TArray<AFGHologram*> Pending;
            for (const auto& Child : R->Children) if (Child.IsValid()) Pending.Add(Child.Get());
            TSet<AFGHologram*> Visited;
            while (!Pending.IsEmpty())
            {
                auto* Child = Pending.Pop(); if (!IsValid(Child) || Visited.Contains(Child)) continue;
                Visited.Add(Child); Pending.Append(Child->GetHologramChildren());
                if (auto* Track = Cast<AFGRailroadTrackHologram>(Child))
                    for (int32 I = 0; I < 2; ++I)
                    {
                        auto* Old = Track->mSnappedConnectionComponents[I].Get();
                        if (const auto* Actual = Session.PreviewRemaps.Find(Old); Actual && Actual->IsValid())
                        {
                            Track->mSnappedConnectionComponents[I] = Actual->Get();
                            if (Old && Old->GetTrack() && Track->mSnappedRailroadTrack == Old->GetTrack())
                                Track->mSnappedRailroadTrack = Cast<AFGBuildableRailroadTrack>(Actual->Get()->GetOwner());
                        }
                    }
                if (auto* Switch = Cast<AFGRailroadSwitchControlHologram>(Child))
                    if (const auto* Actual = Session.PreviewRemaps.Find(Switch->GetControlledConnection()); Actual && Actual->IsValid())
                        Switch->ReplaceControlledConnection(Actual->Get());
            }
            // Preserve the priced geometry. Re-routing from actors which have not
            // received BeginPlay would substitute partially initialized components.
            TArray<AFGBuildable*> New;
            R->Native->Construct(New, ID);
            for (auto* Actor : New) if (Actor) { Built.AddUnique(Actor); Session.BuiltSeams.AddUnique(Actor); }
            R->bConstructed = true;
        }
        return;
    }
}

void FSFBlueprintRailSeams::Construct(AFGBlueprintHologram* Root, const TArray<AActor*>& Copies, FNetConstructionID ID, TArray<AActor*>& Seams)
{
    auto* S = Sessions().Find(Root); if (!S) return;
    bool Complete = !(*S)->bMappingFailed;
    for (const auto& R : (*S)->Records) if (!R->Captured.IsEmpty() && !R->bConstructed) Complete = false;
    for (const auto& Actor : (*S)->BuiltSeams) if (Actor.IsValid()) Seams.AddUnique(Actor.Get());
    if (!Complete) UE_LOG(LogSmartFoundations, Error, TEXT("Blueprint rail seams could not resolve their exact endpoints at the native construction boundary: %s."), *DescribeNativeProgress(Root));
    Reset(Root);
}

bool FSFBlueprintRailSeams::RestoreBindings(FSession& Session, const TArray<FSFBlueprintRailPlanEntry>& Plan, FString* Failure)
{
    auto Reject = [&](const FString& Reason) { if (Failure) *Failure = Reason; return false; };
    for (auto& R : Session.Records)
    {
        R->Bindings.Reset(); R->Native->ResetAutomaticConnections();
        // Only the captured counterpart can participate in authority replay.
        // Do not discover a different nearby port as a substitute.
        for (const auto& Neighbor : R->Neighbors) if (Neighbor.IsValid()) R->Native->UnregisterNearbyActor(Neighbor.Get());
    }
    for (const auto& Entry : Plan)
    {
        TSharedPtr<FSession::FRecord> Record;
        for (const auto& R : Session.Records)
            if (const auto* Cell = Session.Cells.Find(R->Blueprint.Get()); Cell && *Cell == Entry.From[0].Cell) Record = R;
        if (!Record) return Reject(TEXT("captured source cell has no native rail manager"));
        const auto& States = Record->Typed()->GetOpenConnectionStates();
        int32 StateIndex = INDEX_NONE;
        for (int32 I = 0; I < States.Num(); ++I)
        {
            const auto& Sources = States[I].OpenConnections;
            bool SameSources = Sources.Num() == Entry.From.Num();
            TSet<int32> Claimed;
            for (const auto& Expected : Entry.From)
            {
                int32 Match = INDEX_NONE;
                for (int32 J = 0; J < Sources.Num(); ++J)
                    if (SamePortIdentity(CapturePort(Sources[J]->Connection, Session.Cells), Expected))
                    {
                        if (Match != INDEX_NONE) { SameSources = false; break; }
                        Match = J;
                    }
                if (Match == INDEX_NONE || Claimed.Contains(Match)) SameSources = false;
                Claimed.Add(Match);
            }
            if (SameSources)
            {
                if (StateIndex != INDEX_NONE) return Reject(TEXT("captured source identifies multiple native rail states"));
                StateIndex = I;
            }
        }
        if (StateIndex == INDEX_NONE)
        {
            FString Available;
            for (int32 I = 0; I < FMath::Min(States.Num(), 4); ++I)
                for (const auto* Source : States[I].OpenConnections)
                    if (Available.Len() < 1200) Available += PortKey(CapturePort(Source->Connection, Session.Cells)) + TEXT(";");
            return Reject(FString::Printf(TEXT("captured source does not match native states: expected=%s available=%s"),
                *PortKey(Entry.From[0]), *Available));
        }
        if (Record->Bindings.Contains(StateIndex)) return Reject(TEXT("captured source state is claimed twice"));
        UFGRailroadTrackConnectionComponent* Target = nullptr;
        for (const auto& Cell : Session.Cells) if (Cell.Value == Entry.To.Cell)
            for (const auto& Pair : Cell.Key->mDuplicateConnectionToOriginalMap)
            {
                auto* Original = Cast<UFGRailroadTrackConnectionComponent>(Pair.Value);
                auto* Dup = Cast<UFGRailroadTrackConnectionComponent>(Pair.Key);
                if (!Original || !Dup || Original->GetOwner()->GetClass() != Entry.To.BuildClass
                    || Original->GetFName() != Entry.To.ComponentName
                    || !SamePose(Original->GetOwner()->GetActorTransform(), Entry.To.OriginalOwner)) continue;
                if (Target && Target != Dup) return Reject(TEXT("captured target identifies multiple native rail components"));
                Target = Dup;
            }
        if (!Target) return Reject(TEXT("captured target has no matching native rail component"));
        Record->Bindings.Add(StateIndex, Target);
    }
    return true;
}
