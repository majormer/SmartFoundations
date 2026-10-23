// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintVehicleSeams.h"
#include "SmartFoundations.h"
#include "Features/AutoConnect/SFBlueprintVehicleAdapter.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Features/Dismantle/Net/SFBlueprintGroupReplication.h"
#include "FGBlueprintProxy.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"
#include "FGCharacterPlayer.h"
#include "WheeledVehicles/FGVehiclePathNode.h"
#include "WheeledVehicles/FGVehiclePathSegment.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"
#include "Patching/NativeHookManager.h"

// SP/MP DIVERGENCE MAP — repeated Blueprint Vehicle Path seams
// [MP-CLIENT] Capture exact adjacent-cell content and native bridge geometry/cost.
// [MP-AUTH] Rebuild and compare before payment; map only each copy's own actors.
namespace SFBlueprintVehicleState
{
struct FReplacementOwnership
{
    TWeakObjectPtr<AFGBlueprintProxy> Owner;
    TWeakObjectPtr<AFGBlueprintProxy> Previous;
};
TMap<TWeakObjectPtr<AFGVehiclePathSegment>, TSharedPtr<FReplacementOwnership>> ReplacementOwners;

void RegisterReplacementOwnershipHook()
{
    static bool Registered = false;
    if (Registered) return;
    Registered = true;
    SUBSCRIBE_METHOD(AFGVehiclePathSegment::MoveConnectionToNodes,
        [](auto& Scope, AFGVehiclePathSegment* Original, AFGVehiclePathNode* Start, AFGVehiclePathNode* End)
        {
            const auto State = ReplacementOwners.FindRef(Original);
            auto* Replacement = Scope(Original, Start, End);
            // [MP-REPL] Native ownership is COND_InitialOnly. Preserve it inside
            // the native migration call, before the replacement's first net update.
            // Only exact paths captured by this Smart Blueprint operation qualify.
            if (State && IsValid(Replacement) && Replacement->HasAuthority()
                && State->Owner.IsValid() && State->Owner->HasAuthority()
                && Replacement->GetWorld() == State->Owner->GetWorld()
                && !Replacement->GetBlueprintDesigner())
            {
                State->Previous = Replacement->GetBlueprintProxy();
                Replacement->SetBlueprintProxy(State->Owner.Get());
                State->Owner->RegisterBuildable(Replacement);
            }
        });
}
bool VehicleSeamSamePose(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), .1) && A.GetRotation().Equals(B.GetRotation(), .0001)
        && A.GetScale3D().Equals(B.GetScale3D(), .0001);
}
TArray<FSplinePointData> VehicleSeamWorldSpline(const USplineComponent* Spline, const FTransform& From, const FTransform& To)
{
    TArray<FSplinePointData> Out;
    if (!Spline) return Out;
    for (int32 I = 0; I < Spline->GetNumberOfSplinePoints(); ++I)
    {
        FSplinePointData P;
        P.Location = To.TransformPosition(From.InverseTransformPosition(Spline->GetLocationAtSplinePoint(I, ESplineCoordinateSpace::World)));
        P.ArriveTangent = To.TransformVector(From.InverseTransformVector(Spline->GetArriveTangentAtSplinePoint(I, ESplineCoordinateSpace::World)));
        P.LeaveTangent = To.TransformVector(From.InverseTransformVector(Spline->GetLeaveTangentAtSplinePoint(I, ESplineCoordinateSpace::World)));
        Out.Add(P);
    }
    return Out;
}
bool VehicleSeamSameSpline(const TArray<FSplinePointData>& A, const TArray<FSplinePointData>& B, const FVector& Shift = FVector::ZeroVector)
{
    if (A.Num() != B.Num()) return false;
    for (int32 I = 0; I < A.Num(); ++I)
        if (!A[I].Location.Equals(B[I].Location + Shift, .1) || !A[I].ArriveTangent.Equals(B[I].ArriveTangent, .1)
            || !A[I].LeaveTangent.Equals(B[I].LeaveTangent, .1)) return false;
    return true;
}
FSFBlueprintVehicleMember VehicleSeamMember(AFGBlueprintHologram* Copy, const FIntVector& Cell, AFGBuildable* Actor)
{
    FSFBlueprintVehicleMember M; M.Cell = Cell;
    auto* Visual = IsValid(Actor) ? Copy->mBuildableToNewRoot.FindRef(Actor).Get() : nullptr;
    if (!IsValid(Visual)) return M;
    M.BuildClass = Actor->GetClass(); M.Original = Actor->GetActorTransform(); M.World = Visual->GetComponentTransform();
    if (const auto* Path = Cast<AFGVehiclePathSegment>(Actor)) M.Spline = VehicleSeamWorldSpline(Path->GetSplineComponent(), M.Original, M.World);
    return M;
}
bool VehicleSeamSameMember(const FSFBlueprintVehicleMember& A, const FSFBlueprintVehicleMember& B, const FVector& Shift)
{
    FTransform Expected = B.World; Expected.AddToTranslation(Shift);
    return A.Cell == B.Cell && A.BuildClass == B.BuildClass && VehicleSeamSamePose(A.Original, B.Original)
        && VehicleSeamSamePose(A.World, Expected) && VehicleSeamSameSpline(A.Spline, B.Spline, Shift);
}
FString VehicleSeamKey(const FSFBlueprintVehicleMember& M)
{
    return M.Cell.ToString() + GetPathNameSafe(M.BuildClass.Get()) + M.Original.ToString();
}
bool VehicleSeamSameCost(const TArray<FItemAmount>& A, const TArray<FItemAmount>& B)
{
    TMap<UClass*, int64> Left, Right;
    for (const auto& V : A) Left.FindOrAdd(V.ItemClass) += V.Amount;
    for (const auto& V : B) Right.FindOrAdd(V.ItemClass) += V.Amount;
    if (Left.Num() != Right.Num()) return false;
    for (const auto& V : Left) if (!Right.Contains(V.Key) || Right[V.Key] != V.Value) return false;
    return true;
}
struct FRecord
{
    TWeakObjectPtr<AFGBlueprintHologram> Source, Target;
    TSharedPtr<FSFBlueprintVehicleSelection> Native;
    FSFBlueprintVehiclePlanEntry Plan;
};
struct FSession
{
    TMap<AFGBlueprintHologram*, FIntVector> Cells;
    TMap<TWeakObjectPtr<AFGBuildable>, FTransform> Visuals, Originals;
    TArray<FRecord> Records;
    TMap<FIntVector, TArray<TWeakObjectPtr<AFGBuildable>>> Built;
    bool bPrepared = false;
    bool bAttempted = false;
};
TMap<TWeakObjectPtr<AFGBlueprintHologram>, TSharedPtr<FSession>> Sessions;

AFGBuildable* VehicleSeamResolve(const FSession& Session, const FSFBlueprintVehicleMember& Identity)
{
    const auto* Members = Session.Built.Find(Identity.Cell);
    AFGBuildable* Match = nullptr;
    if (!Members) return nullptr;
    for (const auto& Entry : *Members)
    {
        auto* Actor = Entry.Get();
        if (!Actor || Actor->GetClass() != Identity.BuildClass) continue;
        bool Matches = false;
        if (const auto* Path = Cast<AFGVehiclePathSegment>(Actor))
            Matches = VehicleSeamSameSpline(VehicleSeamWorldSpline(Path->GetSplineComponent(), FTransform::Identity, FTransform::Identity), Identity.Spline);
        else Matches = VehicleSeamSamePose(Actor->GetActorTransform(), Identity.World);
        if (Matches) { if (Match) return nullptr; Match = Actor; }
    }
    return Match;
}
}

void FSFBlueprintVehicleSeams::Reset(AFGHologram* Root)
{
    SFBlueprintVehicleState::Sessions.Remove(Cast<AFGBlueprintHologram>(Root));
}
void FSFBlueprintVehicleSeams::ResetAll() { SFBlueprintVehicleState::Sessions.Reset(); }

void FSFBlueprintVehicleSeams::Update(AFGBlueprintHologram* Root)
{
    using namespace SFBlueprintVehicleState;
    if (!IsValid(Root)) return;
    auto* Player = Cast<AFGCharacterPlayer>(Root->GetConstructionInstigator());
    auto* Inventory = IsValid(Player) ? Player->GetInventory() : nullptr;
    // Native bridge validation checks affordability even for zero-cost paths.
    // Defer incomplete previews instead of passing a missing inventory to it.
    if (!IsValid(Inventory)) { Reset(Root); return; }
    for (auto It = Sessions.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    auto& Slot = Sessions.FindOrAdd(Root);
    if (Slot && Slot->bPrepared) return;
    TMap<AFGBlueprintHologram*, FIntVector> Cells; Cells.Add(Root, FIntVector::ZeroValue);
    for (auto* Child : Root->GetHologramChildren())
    {
        FIntVector Cell;
        if (auto* Copy = Cast<AFGBlueprintHologram>(Child); Copy && USFGridCoordComponent::TryGetCell(Copy, Cell)) Cells.Add(Copy, Cell);
    }
    if (Cells.Num() < 2) { Reset(Root); return; }
    TMap<TWeakObjectPtr<AFGBuildable>, FTransform> Visuals, Originals;
    for (const auto& Cell : Cells) for (const auto& Item : Cell.Key->mBuildableToNewRoot)
        if (IsValid(Item.Key.Get()) && IsValid(Item.Value.Get())
            && (Item.Key->IsA<AFGVehiclePathNode>() || Item.Key->IsA<AFGVehiclePathSegment>()))
        {
            Visuals.Add(Item.Key.Get(), Item.Value->GetComponentTransform());
            Originals.Add(Item.Key.Get(), Item.Key->GetActorTransform());
        }
    bool Unchanged = Slot && Slot->Cells.OrderIndependentCompareEqual(Cells) && Slot->Visuals.Num() == Visuals.Num();
    if (Unchanged) for (const auto& V : Visuals)
        if (!Slot->Visuals.Contains(V.Key) || !VehicleSeamSamePose(Slot->Visuals[V.Key], V.Value)
            || !Slot->Originals.Contains(V.Key) || !VehicleSeamSamePose(Slot->Originals[V.Key], Originals[V.Key])) { Unchanged = false; break; }
    if (Unchanged) return;
    Slot.Reset();
    auto Session = MakeShared<FSession>(); Session->Cells = Cells; Session->Visuals = Visuals; Session->Originals = Originals;
    Slot = Session;
    // Evaluate both directions. Grid index signs do not imply native path flow.
    for (const auto& Source : Cells) for (const auto& Target : Cells)
    {
        const FIntVector D = Source.Value - Target.Value;
        if (FMath::Abs(D.X) + FMath::Abs(D.Y) + FMath::Abs(D.Z) != 1) continue;
        for (const auto& Item : Source.Key->mBuildableToNewRoot)
        {
            auto* Tail = Cast<AFGVehiclePathNode>(Item.Key.Get());
            if (!Tail || Tail->GetArrivingConnections().Num() != 1 || !Tail->GetLeavingConnections().IsEmpty()) continue;
            for (const auto& Other : Target.Key->mBuildableToNewRoot)
            {
                auto* Path = Cast<AFGVehiclePathSegment>(Other.Key.Get());
                auto* Head = Path ? Path->GetStartNode() : nullptr;
                if (!Head || !Head->GetArrivingConnections().IsEmpty() || Head->GetLeavingConnections().Num() != 1) continue;
                auto Native = FSFBlueprintVehicleAdapter::Preview(Source.Key, Tail, Target.Key, Path);
                if (!Native || !Native->IsValid()) continue;
                FRecord R; R.Source = Source.Key; R.Target = Target.Key; R.Native = Native;
                R.Plan.Tail = VehicleSeamMember(Source.Key, Source.Value, Tail);
                R.Plan.Head = VehicleSeamMember(Target.Key, Target.Value, Head);
                R.Plan.Incoming = VehicleSeamMember(Source.Key, Source.Value, Tail->GetArrivingConnections()[0]);
                R.Plan.Outgoing = VehicleSeamMember(Target.Key, Target.Value, Path);
                TArray<AFGHologram*> Bridges; Native->GetBridges(Bridges);
                bool Valid = true;
                for (auto* Bridge : Bridges) if (!Bridge->IsDisabled())
                {
                    Bridge->ValidatePlacementAndCost(Inventory);
                    if (!Bridge->CanConstruct() || !R.Plan.Bridge.IsEmpty()) { Valid = false; break; }
                    // Native Vehicle Paths keep a temporary routing spline as
                    // well as the actual path. Capture only the latter.
                    const auto* Property = FindFProperty<FObjectPropertyBase>(Bridge->GetClass(), TEXT("mPathSplineComponent"));
                    const auto* Spline = Property ? Cast<USplineComponent>(Property->GetObjectPropertyValue_InContainer(Bridge)) : nullptr;
                    if (!IsValid(Spline)) { Valid = false; break; }
                    R.Plan.Bridge = VehicleSeamWorldSpline(Spline, FTransform::Identity, FTransform::Identity);
                    R.Plan.Cost = Bridge->GetCost(false);
                }
                if (Valid) Session->Records.Add(MoveTemp(R));
            }
        }
    }
    // Never silently choose one of several viable claims on the same mouth.
    TMap<FString, int32> Claims;
    for (const auto& R : Session->Records) { ++Claims.FindOrAdd(VehicleSeamKey(R.Plan.Tail)); ++Claims.FindOrAdd(VehicleSeamKey(R.Plan.Head)); }
    Session->Records.RemoveAll([&](const FRecord& R) { return Claims[VehicleSeamKey(R.Plan.Tail)] != 1 || Claims[VehicleSeamKey(R.Plan.Head)] != 1; });
    Session->Records.Sort([](const FRecord& A, const FRecord& B) { return VehicleSeamKey(A.Plan.Tail) + VehicleSeamKey(A.Plan.Head) < VehicleSeamKey(B.Plan.Tail) + VehicleSeamKey(B.Plan.Head); });
}

void FSFBlueprintVehicleSeams::Capture(AFGHologram* Root, TArray<FSFBlueprintVehiclePlanEntry>& Plan)
{
    Plan.Reset();
    const auto* S = SFBlueprintVehicleState::Sessions.Find(Cast<AFGBlueprintHologram>(Root));
    if (S) for (const auto& Record : (*S)->Records) Plan.Add(Record.Plan);
}
bool FSFBlueprintVehicleSeams::IsBounded(const TArray<FSFBlueprintVehiclePlanEntry>& Plan)
{
    if (Plan.Num() > 4096) return false;
    TSet<FString> Claims;
    auto SplineValid = [](const TArray<FSplinePointData>& Points, bool Required)
    {
        if (Points.Num() > 256 || (Required && Points.Num() < 2)) return false;
        for (const auto& P : Points) if (P.Location.ContainsNaN() || P.ArriveTangent.ContainsNaN() || P.LeaveTangent.ContainsNaN()) return false;
        return true;
    };
    for (const auto& E : Plan)
    {
        for (const auto* M : {&E.Tail, &E.Head, &E.Incoming, &E.Outgoing})
            if (FMath::Abs(int64(M->Cell.X)) > 4096 || FMath::Abs(int64(M->Cell.Y)) > 4096 || FMath::Abs(int64(M->Cell.Z)) > 4096) return false;
        const FIntVector D = E.Tail.Cell - E.Head.Cell;
        if (FMath::Abs(int64(D.X)) + FMath::Abs(int64(D.Y)) + FMath::Abs(int64(D.Z)) != 1
            || E.Incoming.Cell != E.Tail.Cell || E.Outgoing.Cell != E.Head.Cell) return false;
        for (const auto* M : {&E.Tail, &E.Head, &E.Incoming, &E.Outgoing})
        {
            const bool Segment = M == &E.Incoming || M == &E.Outgoing;
            UClass* Class = M->BuildClass.Get();
            if (!Class || (!Class->GetPathName().StartsWith(TEXT("/Game/FactoryGame/")) && !Class->GetPathName().StartsWith(TEXT("/Script/FactoryGame.")))
                || !Class->IsChildOf(Segment ? AFGVehiclePathSegment::StaticClass() : AFGVehiclePathNode::StaticClass())
                || M->Original.ContainsNaN() || M->World.ContainsNaN() || !SplineValid(M->Spline, Segment)) return false;
        }
        for (const auto* Node : {&E.Tail, &E.Head})
        {
            const FString K = SFBlueprintVehicleState::VehicleSeamKey(*Node);
            if (Claims.Contains(K)) return false;
            Claims.Add(K);
        }
        if (!SplineValid(E.Bridge, false) || E.Cost.Num() > 64) return false;
        for (const auto& Cost : E.Cost) if (!Cost.ItemClass || Cost.Amount < 0) return false;
    }
    return true;
}
bool FSFBlueprintVehicleSeams::Prepare(AFGBlueprintHologram* Root, const FSFScalingSpec& Spec, FString* Failure)
{
    using namespace SFBlueprintVehicleState;
    auto Reject = [&](const TCHAR* Reason) { if (Failure) *Failure = Reason; return false; };
    if (!IsValid(Root) || !IsBounded(Spec.BlueprintVehicleSeams)) return Reject(TEXT("invalid native vehicle plan"));
    auto* Prior = Sessions.Find(Root);
    if (Prior && (*Prior)->bAttempted) return Reject(TEXT("native vehicle construction already attempted"));
    if (Prior) (*Prior)->bPrepared = false;
    bool HasCells = false;
    for (auto* Child : Root->GetHologramChildren()) { FIntVector Cell; if (USFGridCoordComponent::TryGetCell(Child, Cell)) HasCells = true; }
    if (!HasCells) SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(Root, Spec, Root->GetRecipe());
    Update(Root);
    auto* Slot = Sessions.Find(Root);
    if (!Slot) return Reject(TEXT("missing native vehicle copy session"));
    const FVector Shift = Root->GetActorRotation().RotateVector(SFScalingSpecExpansion::MeasureBlueprintContentAnchor(Root) - Spec.ClientParentAnchorRel);
    auto& Session = **Slot;
    if (Session.Records.Num() != Spec.BlueprintVehicleSeams.Num()) return Reject(TEXT("native vehicle endpoint count changed"));
    for (int32 I = 0; I < Session.Records.Num(); ++I)
    {
        const auto& A = Session.Records[I].Plan; const auto& E = Spec.BlueprintVehicleSeams[I];
        if (!VehicleSeamSameMember(A.Tail, E.Tail, Shift) || !VehicleSeamSameMember(A.Head, E.Head, Shift)
            || !VehicleSeamSameMember(A.Incoming, E.Incoming, Shift) || !VehicleSeamSameMember(A.Outgoing, E.Outgoing, Shift)
            || !VehicleSeamSameSpline(A.Bridge, E.Bridge, Shift) || !VehicleSeamSameCost(A.Cost, E.Cost))
            return Reject(TEXT("native vehicle identity, geometry or cost changed"));
    }
    Session.bPrepared = true;
    return true;
}
bool FSFBlueprintVehicleSeams::IsPrepared(AFGHologram* Root)
{
    const auto* S = SFBlueprintVehicleState::Sessions.Find(Cast<AFGBlueprintHologram>(Root)); return S && (*S)->bPrepared;
}
void FSFBlueprintVehicleSeams::RecordCopy(AFGBlueprintHologram* Copy, AActor* Main, const TArray<AActor*>& Members)
{
    for (auto& S : SFBlueprintVehicleState::Sessions) if (S.Value->bPrepared && S.Value->Cells.Contains(Copy))
    {
        auto& Actors = S.Value->Built.FindOrAdd(S.Value->Cells[Copy]);
        if (auto* B = Cast<AFGBuildable>(Main)) Actors.AddUnique(B);
        for (auto* A : Members) if (auto* B = Cast<AFGBuildable>(A)) Actors.AddUnique(B);
    }
}
void FSFBlueprintVehicleSeams::Construct(AFGBlueprintHologram* Root, FNetConstructionID ID, TArray<AActor*>& Bridges)
{
    using namespace SFBlueprintVehicleState;
    auto* Slot = Sessions.Find(Root);
    if (!Slot || !(*Slot)->bPrepared || (*Slot)->bAttempted || !Root->HasAuthority()) return;
    auto& S = **Slot;
    struct FResolved { FRecord* Record; AFGVehiclePathNode* Tail; AFGVehiclePathNode* Head; AFGVehiclePathSegment* Incoming; AFGVehiclePathSegment* Target; };
    TArray<FResolved> Ready;
    // Resolve the entire batch before the first native network mutation.
    for (auto& R : S.Records)
    {
        auto* Tail = Cast<AFGVehiclePathNode>(VehicleSeamResolve(S, R.Plan.Tail));
        auto* Head = Cast<AFGVehiclePathNode>(VehicleSeamResolve(S, R.Plan.Head));
        auto* Incoming = Cast<AFGVehiclePathSegment>(VehicleSeamResolve(S, R.Plan.Incoming));
        auto* Target = Cast<AFGVehiclePathSegment>(VehicleSeamResolve(S, R.Plan.Outgoing));
        if (!Tail || !Head || !Incoming || !Target || Incoming->GetEndNode() != Tail || Target->GetStartNode() != Head)
        {
            UE_LOG(LogSmartFoundations, Warning, TEXT("Blueprint Vehicle Path seams rejected: constructed copy identity differs."));
            return;
        }
        Ready.Add({&R, Tail, Head, Incoming, Target});
    }
    S.bAttempted = true;
    struct FConstructedPath
    {
        TWeakObjectPtr<AFGVehiclePathSegment> Original;
        TWeakObjectPtr<AFGBlueprintProxy> Proxy;
        UClass* Class = nullptr;
        TArray<FGuid> Starts, Ends;
        TArray<FSplinePointData> Spline;
        TSharedPtr<FReplacementOwnership> Ownership;
    };
    TArray<FConstructedPath> Paths;
    RegisterReplacementOwnershipHook();
    TMap<AFGVehiclePathSegment*, int32> PathIndices;
    // Native direct joins may retire/recreate a target segment without returning
    // it as a bridge. Capture ownership and stable geometry before that mutation.
    for (const auto& Cell : S.Built) for (const auto& Actor : Cell.Value)
        if (auto* Path = Cast<AFGVehiclePathSegment>(Actor.Get()); Path && !PathIndices.Contains(Path))
        {
            FConstructedPath P;
            P.Original = Path; P.Proxy = Path->GetBlueprintProxy(); P.Class = Path->GetClass();
            P.Ownership = MakeShared<FReplacementOwnership>();
            P.Ownership->Owner = P.Proxy;
            ReplacementOwners.Add(Path, P.Ownership);
            if (auto* Node = Path->GetStartNode()) P.Starts.Add(Node->GetPathNodeGUID());
            if (auto* Node = Path->GetEndNode()) P.Ends.Add(Node->GetPathNodeGUID());
            P.Spline = VehicleSeamWorldSpline(Path->GetSplineComponent(), FTransform::Identity, FTransform::Identity);
            PathIndices.Add(Path, Paths.Num()); Paths.Add(MoveTemp(P));
        }
    for (const auto& R : Ready)
    {
        // Either meeting node may survive native migration. The untouched far
        // endpoints and these exact meeting GUIDs identify the same directed span.
        Paths[PathIndices.FindChecked(R.Incoming)].Ends.AddUnique(R.Head->GetPathNodeGUID());
        Paths[PathIndices.FindChecked(R.Target)].Starts.AddUnique(R.Tail->GetPathNodeGUID());
    }
    TArray<TPair<int32, int32>> Submitted;
    TArray<TWeakObjectPtr<AFGBuildable>> NativeBridges;
    for (const auto& R : Ready)
    {
        // Keep the exact per-copy counterpart already verified above. The
        // node's native connection list is transient during Blueprint remapping.
        auto* Incoming = R.Incoming;
        bool Attempted = false; TArray<AFGBuildable*> New;
        const bool Pass = FSFBlueprintVehicleAdapter::Construct(R.Record->Source.Get(), R.Record->Native, R.Tail, R.Head, R.Target, ID, New, Attempted);
        for (auto* B : New) if (IsValid(B)) { Bridges.AddUnique(B); NativeBridges.AddUnique(B); }
        if (!Pass && !Attempted) break;
        // Native node migration can finish after the construction callback.
        // A submitted mutation must neither be replayed nor stop later pairs
        // merely because its actor references have not settled in this frame.
        Submitted.Add({PathIndices.FindChecked(Incoming), PathIndices.FindChecked(R.Target)});
    }
    const int32 Expected = Ready.Num();
    TArray<TWeakObjectPtr<AActor>> ConstructedCopies;
    for (const auto& Cell : S.Built) for (const auto& Actor : Cell.Value)
        if (Actor.IsValid()) ConstructedCopies.AddUnique(Actor.Get());
    for (const auto& Actor : NativeBridges) if (Actor.IsValid()) ConstructedCopies.AddUnique(Actor.Get());
    const TWeakObjectPtr<UWorld> World = Root->GetWorld();
    if (World.IsValid()) World->GetTimerManager().SetTimerForNextTick([World, Submitted, NativeBridges, Expected, ConstructedCopies, Paths]()
    {
        if (!World.IsValid()) return;
        World->GetTimerManager().SetTimerForNextTick([World, Submitted, NativeBridges, Expected, ConstructedCopies, Paths]()
        {
            if (!World.IsValid()) return;
            TArray<AFGVehiclePathSegment*> Current;
            TSet<AFGBlueprintProxy*> PreviousGroups;
            for (const auto& P : Paths)
            {
                ReplacementOwners.Remove(P.Original);
                if (P.Ownership && P.Ownership->Previous.IsValid()
                    && P.Ownership->Previous != P.Proxy) PreviousGroups.Add(P.Ownership->Previous.Get());
            }
            TSet<FGuid> RelevantEnds;
            for (const auto& P : Paths) for (const auto& Guid : P.Ends) if (Guid.IsValid()) RelevantEnds.Add(Guid);
            TMultiMap<FGuid, AFGVehiclePathSegment*> ReplacementsByEnd;
            bool IndexedReplacements = false;
            int32 Replacements = 0;
            for (const auto& P : Paths)
            {
                auto Matches = [&](AFGVehiclePathSegment* Path)
                {
                    if (!IsValid(Path) || !Path->HasAuthority() || Path->GetWorld() != World.Get() || Path->GetClass() != P.Class
                        || !IsValid(Path->GetStartNode()) || !IsValid(Path->GetEndNode())) return false;
                    return Path->GetStartNode()->GetPathNodeGUID().IsValid() && Path->GetEndNode()->GetPathNodeGUID().IsValid()
                        && P.Starts.Contains(Path->GetStartNode()->GetPathNodeGUID())
                        && P.Ends.Contains(Path->GetEndNode()->GetPathNodeGUID());
                };
                AFGVehiclePathSegment* Match = Matches(P.Original.Get()) ? P.Original.Get() : nullptr;
                if (!Match)
                {
                    if (!IndexedReplacements)
                    {
                        IndexedReplacements = true;
                        for (TActorIterator<AFGVehiclePathSegment> It(World.Get()); It; ++It)
                            if (IsValid(*It) && IsValid(It->GetEndNode()))
                            {
                                const FGuid Guid = It->GetEndNode()->GetPathNodeGUID();
                                if (RelevantEnds.Contains(Guid)) ReplacementsByEnd.Add(Guid, *It);
                            }
                    }
                    TArray<AFGVehiclePathSegment*> Candidates;
                    for (const auto& Guid : P.Ends)
                    {
                        TArray<AFGVehiclePathSegment*> EndingHere;
                        ReplacementsByEnd.MultiFind(Guid, EndingHere);
                        for (auto* Candidate : EndingHere) if (Matches(Candidate)) Candidates.AddUnique(Candidate);
                    }
                    if (Candidates.Num() == 1) Match = Candidates[0];
                    else for (auto* Candidate : Candidates)
                        if (VehicleSeamSameSpline(P.Spline, VehicleSeamWorldSpline(Candidate->GetSplineComponent(), FTransform::Identity, FTransform::Identity)))
                        {
                            if (Match) { Match = nullptr; break; }
                            Match = Candidate;
                        }
                }
                Current.Add(Match);
                if (!Match || Match == P.Original.Get() || !P.Proxy.IsValid() || !P.Proxy->HasAuthority()
                    || Match->GetBlueprintDesigner() || P.Proxy->GetWorld() != World.Get()) continue;
                if (auto* Previous = Match->GetBlueprintProxy(); IsValid(Previous) && Previous != P.Proxy.Get())
                    PreviousGroups.Add(Previous);
                Match->SetBlueprintProxy(P.Proxy.Get());
                P.Proxy->RegisterBuildable(Match);
                Match->ForceNetUpdate();
                P.Proxy->ForceNetUpdate();
                ++Replacements;
            }
            // Use the same exact per-copy counterparts as native wiring, including
            // native replacements and each copy's main actor.
            TArray<AActor*> LiveCopies;
            TSet<AFGBlueprintProxy*> Groups;
            for (const auto& Actor : ConstructedCopies) if (Actor.IsValid())
            {
                LiveCopies.Add(Actor.Get());
                if (auto* Member = Cast<AFGBuildable>(Actor.Get()))
                    if (auto* Proxy = Member->GetBlueprintProxy(); IsValid(Proxy)) Groups.Add(Proxy);
            }
            for (auto* Path : Current) if (Path) LiveCopies.AddUnique(Path);
            for (auto* Proxy : PreviousGroups) Groups.Add(Proxy);
            int32 Before = 0, After = 0;
            for (auto* Proxy : Groups) Before += FSFBlueprintGroupReplication::FindReassignedMembers(Proxy).Num();
            FSFBlueprintGroupReplication::ReconcileConstructedGroups(LiveCopies);
            for (auto* Proxy : PreviousGroups) if (IsValid(Proxy) && Proxy->HasAuthority())
            {
                for (auto* Member : FSFBlueprintGroupReplication::FindReassignedMembers(Proxy)) Proxy->UnregisterBuildable(Member);
                // This operation's retired native actors can leave dead entries in
                // the early observer group. Only authority owns these references.
                TSet<AFGBuildable*> Retired;
                for (auto* Member : Proxy->GetBuildables()) if (!IsValid(Member)) Retired.Add(Member);
                for (auto* Member : Retired) Proxy->UnregisterBuildable(Member);
                Proxy->ValidateExistanceOtherwiseSelfDestruct();
                if (IsValid(Proxy)) Proxy->ForceNetUpdate();
            }
            for (auto* Proxy : Groups) if (IsValid(Proxy)) After += FSFBlueprintGroupReplication::FindReassignedMembers(Proxy).Num();
            UE_LOG(LogSmartFoundations, Log, TEXT("Blueprint Vehicle Path groups: %d exact copy members, %d native replacement owners restored, %d confirmed stale memberships retired, %d remaining."),
                LiveCopies.Num(), Replacements, Before - After, After);
            int32 Connected = 0;
            for (const auto& Pair : Submitted)
            {
                auto* Incoming = Current.IsValidIndex(Pair.Key) ? Current[Pair.Key] : nullptr;
                auto* Outgoing = Current.IsValidIndex(Pair.Value) ? Current[Pair.Value] : nullptr;
                if (!Incoming || !Outgoing) continue;
                auto* Cursor = Incoming->GetEndNode();
                TSet<AFGVehiclePathNode*> Seen;
                while (IsValid(Cursor) && !Seen.Contains(Cursor))
                {
                    if (Cursor == Outgoing->GetStartNode()) { ++Connected; break; }
                    Seen.Add(Cursor);
                    AFGVehiclePathSegment* Next = nullptr;
                    bool Ambiguous = false;
                    for (const auto& B : NativeBridges)
                        if (auto* Path = Cast<AFGVehiclePathSegment>(B.Get()); Path && Path->GetStartNode() == Cursor)
                        {
                            if (Next) { Ambiguous = true; break; }
                            Next = Path;
                        }
                    Cursor = !Ambiguous && Next ? Next->GetEndNode() : nullptr;
                }
            }
            UE_LOG(LogSmartFoundations, Log, TEXT("Blueprint Vehicle Path seams: connected %d/%d directed pairs after native migration, %d native submissions, %d new members."), Connected, Expected, Submitted.Num(), NativeBridges.Num());
        });
    });
}
