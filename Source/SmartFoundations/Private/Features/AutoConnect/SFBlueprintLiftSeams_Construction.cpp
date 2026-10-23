// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintLiftSeams.h"
#include "Features/AutoConnect/SFBlueprintLiftValidation.h"
#include "SmartFoundations.h"
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/Preview/BeltPreviewHelper.h"
#include "Features/AutoConnect/Preview/SFLiftRouteNativePreview.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteBeltHologram.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Subsystem/SFSubsystem.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Services/SFChainActorService.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "FGCharacterPlayer.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"

// SP/MP DIVERGENCE MAP — Blueprint lift-tail seams
// [MP-CLIENT] Capture exact authored mouths, priced native splines and costs.
// [MP-AUTH] Expand whole copies and compare the complete reconstructed plan before payment.
// [MP-SEAM] Resolve only actors returned by those copies; native belts own wiring/chains.
namespace
{
const FName BlueprintLiftSeamTag(TEXT("SF_BlueprintLiftSeam"));
struct FLiftSeamPreview
{
    FSFBlueprintLiftPlanEntry Entry;
    TSharedPtr<FBeltPreviewHelper> Helper;
};
struct FLiftSeamTransaction
{
    bool bRequired = false;
    bool bValid = false;
    bool bPrepared = false;
    bool bConstructed = false;
    bool bUpdating = false;
    bool bReportedUnavailable = false;
    FString Failure;
    TArray<FLiftSeamPreview> Routes;
};
TMap<TWeakObjectPtr<AFGBlueprintHologram>, FLiftSeamTransaction> BlueprintLiftTransactions;
const TArray<FSFBlueprintLiftPlanEntry>* BlueprintLiftReplayPlan = nullptr;
const AFGBlueprintHologram* BlueprintLiftReplayRoot = nullptr;
using FBlueprintCopies = TMap<FIntVector, AFGBlueprintHologram*>;
FBlueprintCopies CollectCopies(AFGBlueprintHologram* Root)
{
    FBlueprintCopies Copies;
    Copies.Add(FIntVector::ZeroValue, Root);
    for (auto* Child : Root->GetHologramChildren())
    {
        FIntVector Cell;
        if (auto* B = Cast<AFGBlueprintHologram>(Child); B && USFGridCoordComponent::TryGetCell(B, Cell)) Copies.Add(Cell, B);
    }
    for (const auto& Copy : Copies) FSFBlueprintSeamService::EnsureLiftTailConnectors(Copy.Value);
    return Copies;
}
bool LiftSamePose(const FTransform& A, const FTransform& B)
{
    return A.GetLocation().Equals(B.GetLocation(), .1f) && A.GetRotation().Equals(B.GetRotation(), .0001f)
        && A.GetScale3D().Equals(B.GetScale3D(), .0001f);
}
bool LiftSameCost(const TArray<FItemAmount>& A, const TArray<FItemAmount>& B)
{
    TMap<UClass*, int64> Totals;
    for (const auto& I : A) Totals.FindOrAdd(I.ItemClass.Get()) += I.Amount;
    for (const auto& I : B) Totals.FindOrAdd(I.ItemClass.Get()) -= I.Amount;
    for (const auto& I : Totals) if (I.Value) return false;
    return true;
}
FSFBlueprintLiftPort CaptureLiftMouth(AFGBlueprintHologram* Copy, FIntVector Cell, int32 Index)
{
    FSFBlueprintLiftPort E; E.Cell = Cell; E.Index = Index;
    auto* Original = FSFBlueprintSeamService::GetOriginalBeltConnector(Copy, Index);
    auto* Duplicate = Original ? FSFBlueprintSeamService::ResolveBeltConnector(Copy, Index, Original->GetFName()) : nullptr;
    if (!Original || !Duplicate || !Original->GetOwner()) return E;
    E.BuildClass = Original->GetOwner()->GetClass(); E.OriginalOwner = Original->GetOwner()->GetActorTransform();
    E.PortLocalPose = Original->GetComponentTransform().GetRelativeTransform(E.OriginalOwner);
    E.OwnerPose = E.PortLocalPose.Inverse() * Duplicate->GetComponentTransform();
    E.Name = Original->GetFName(); E.Direction = static_cast<int32>(Original->GetDirection());
    return E;
}
FString LiftRouteKey(const FSFBlueprintLiftPlanEntry& E)
{
    return E.From.Cell.ToString() + TEXT("|") + FString::FromInt(E.From.Index) + TEXT("|")
        + E.To.Cell.ToString() + TEXT("|") + FString::FromInt(E.To.Index);
}
bool SameLiftSpline(const TArray<FSplinePointData>& A, const TArray<FSplinePointData>& B)
{
    if (A.Num() != B.Num()) return false;
    for (int32 I = 0; I < A.Num(); ++I)
        if (!A[I].Location.Equals(B[I].Location, .1f) || !A[I].ArriveTangent.Equals(B[I].ArriveTangent, .1f)
            || !A[I].LeaveTangent.Equals(B[I].LeaveTangent, .1f)) return false;
    return true;
}
UFGFactoryConnectionComponent* ResolveBuiltLiftMouth(const FSFBlueprintLiftPort& E, const TArray<AActor*>& Built)
{
    UFGFactoryConnectionComponent* Found = nullptr;
    TSet<AActor*> Seen;
    for (auto* Actor : Built)
    {
        if (!IsValid(Actor) || Seen.Contains(Actor)) continue;
        Seen.Add(Actor);
        if (!Actor->IsA<AFGBuildableConveyorLift>() || Actor->GetClass() != E.BuildClass
            || !LiftSamePose(Actor->GetActorTransform(), E.OwnerPose)) continue;
        TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Actor);
        for (auto* Port : Ports)
            if (Port->GetFName() == E.Name && !Port->IsConnected() && static_cast<int32>(Port->GetDirection()) == E.Direction
                && LiftSamePose(Port->GetComponentTransform().GetRelativeTransform(Actor->GetActorTransform()), E.PortLocalPose))
            { if (Found) return nullptr; Found = Port; }
    }
    return Found;
}
}

bool FSFBlueprintLiftSeams::SamePort(const FSFBlueprintLiftPort& A, const FSFBlueprintLiftPort& B)
{
    return A.Cell == B.Cell && A.Index == B.Index && A.BuildClass == B.BuildClass && A.Name == B.Name
        && A.Direction == B.Direction && LiftSamePose(A.OriginalOwner, B.OriginalOwner)
        && LiftSamePose(A.OwnerPose, B.OwnerPose) && LiftSamePose(A.PortLocalPose, B.PortLocalPose);
}

void FSFBlueprintLiftSeams::ResetPreview(AFGHologram* Hologram)
{
    auto* Root = Cast<AFGBlueprintHologram>(Hologram);
    if (auto* State = BlueprintLiftTransactions.Find(Root))
        for (auto& R : State->Routes) if (R.Helper)
        { RetireProbe(Root, R.Helper->GetHologram()); R.Helper->DestroyPreview(); }
    BlueprintLiftTransactions.Remove(Root);
    FSFBlueprintSeamService::ReleaseLiftTailConnectors(Root);
}

void FSFBlueprintLiftSeams::ResetAllPreviews()
{
    TArray<TWeakObjectPtr<AFGBlueprintHologram>> Roots;
    BlueprintLiftTransactions.GetKeys(Roots);
    for (const auto& Root : Roots) if (Root.IsValid()) ResetPreview(Root.Get());
    BlueprintLiftTransactions.Reset();
}

void FSFBlueprintLiftSeams::UpdatePreview(AFGBlueprintHologram* Root, USFSubsystem* Subsystem, USFAutoConnectService* Service)
{
    if (!Root || !Subsystem || !Service) return;
    if (const auto* State = BlueprintLiftTransactions.Find(Root); State && (State->bPrepared || State->bUpdating)) return;
    for (auto It = BlueprintLiftTransactions.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    auto Copies = CollectCopies(Root);
    TArray<FSFBlueprintLiftAlignment::FTail> Tails;
    FSFBlueprintSeamService::GetOpenLiftTails(Root, Tails);
    const auto Pairing = FSFBlueprintLiftAlignment::PairTails(Tails, FSFBlueprintSeamService::LANE_MATCH_TOLERANCE,
        GetDefault<ASFConveyorBeltHologram>()->GetMaxSplineLength());
    bool HasZNeighbor = false;
    for (const auto& Copy : Copies) HasZNeighbor |= Copies.Contains(Copy.Key + FIntVector(0, 0, 1));
    const bool Required = HasZNeighbor && (!Pairing.Pairs.IsEmpty() || Pairing.bAmbiguous || Pairing.bInvalid);
    if (!Required) { ResetPreview(Root); return; }
    auto& State = BlueprintLiftTransactions.FindOrAdd(Root);
    TGuardValue<bool> Updating(State.bUpdating, true);
    State.bRequired = true; State.bValid = false;
    const bool ReplayThisRoot = BlueprintLiftReplayRoot == Root && BlueprintLiftReplayPlan;
    State.Failure = TEXT("no compatible native lift-tail layout");
    int32 Tier = ReplayThisRoot && !BlueprintLiftReplayPlan->IsEmpty()
        ? (*BlueprintLiftReplayPlan)[0].Tier : Subsystem->GetAutoConnectRuntimeSettings().BeltTierMain;
    if (!Tier) Tier = Subsystem->GetHighestUnlockedBeltTier(Subsystem->GetLastController());
    TArray<FLiftSeamPreview> Next;
    bool Valid = !Pairing.bAmbiguous && !Pairing.bInvalid && !Pairing.Pairs.IsEmpty() && Tier >= 1 && Tier <= 6;
    TArray<FIntVector> Cells; Copies.GetKeys(Cells);
    Cells.Sort([](const auto& A, const auto& B) { return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z; });
    for (const auto& Cell : Cells)
    {
        auto** Neighbor = Copies.Find(Cell + FIntVector(0, 0, 1));
        if (!Neighbor) continue;
        auto* A = Copies[Cell]; auto* B = *Neighbor;
        const bool ALower = A->GetActorLocation().Z < B->GetActorLocation().Z;
        if (!ValidateCopy(A) || !ValidateCopy(B))
        { Valid = false; State.Failure = TEXT("complete Blueprint copy failed native placement: ") + SFBlueprintLiftValidation::LastCopyFailure(); }
        for (const auto& Pair : Pairing.Pairs)
        {
            FLiftSeamPreview R;
            const bool FromA = ALower == Pair.bOutputInLowerCopy;
            R.Entry.From = CaptureLiftMouth(FromA ? A : B, FromA ? Cell : Cell + FIntVector(0, 0, 1), Pair.Output);
            R.Entry.To = CaptureLiftMouth(FromA ? B : A, FromA ? Cell + FIntVector(0, 0, 1) : Cell, Pair.Input);
            auto* From = FSFBlueprintSeamService::ResolveBeltConnector(FromA ? A : B, Pair.Output, R.Entry.From.Name);
            auto* To = FSFBlueprintSeamService::ResolveBeltConnector(FromA ? B : A, Pair.Input, R.Entry.To.Name);
            const FString Key = LiftRouteKey(R.Entry);
            for (auto& Old : State.Routes) if (LiftRouteKey(Old.Entry) == Key) { R.Helper = MoveTemp(Old.Helper); break; }
            if (R.Helper && R.Helper->GetTier() != Tier)
            { RetireProbe(Root, R.Helper->GetHologram()); R.Helper->DestroyPreview(); R.Helper.Reset(); }
            if (!R.Helper) R.Helper = MakeShared<FBeltPreviewHelper>(Root->GetWorld(), Tier, Root);
            if (auto* H = R.Helper->GetHologram(); H && H->IsHologramLocked()) H->LockHologramPosition(false);
            auto* Previous = R.Helper->GetHologram();
            bool RouteValid = false;
            if (Valid && ReplayThisRoot)
            {
                const auto* Expected = BlueprintLiftReplayPlan->FindByPredicate([&](const auto& E) { return LiftRouteKey(E) == Key; });
                if (Expected && SamePort(R.Entry.From, Expected->From) && SamePort(R.Entry.To, Expected->To))
                {
                    // Spawn through the normal helper, then validate the exact priced
                    // spline. Authority settings must not select a different route/tier.
                    R.Helper->UpdatePreview(From, To);
                    if (auto* Native = R.Helper->GetTypedHologram())
                    {
                        Native->SetActorTransform(Expected->Pose);
                        Native->SetSplineDataAndUpdate(Expected->Spline);
                        Native->CheckValidPlacement();
                        RouteValid = Native->GetLastVanillaPlacementValid();
                    }
                }
            }
            else if (Valid) RouteValid = Service->CreateOrUpdateBeltPreview(From, To, R.Helper,
                USFAutoConnectService::FACING_SANITY_ANGLE, false, Root);
            if (!RouteValid)
            {
                if (Valid) State.Failure = TEXT("exact mouth identity or native routed belt failed");
                RetireProbe(Root, Previous); Valid = false; Next.Add(MoveTemp(R)); continue;
            }
            auto* Belt = R.Helper->GetTypedHologram();
            auto* Recipes = AFGRecipeManager::Get(Root->GetWorld());
            if (!Belt || !Recipes || !Recipes->IsRecipeAvailable(Belt->GetRecipe()))
            { Valid = false; State.Failure = TEXT("native belt tier or recipe unavailable"); Next.Add(MoveTemp(R)); continue; }
            Belt->Tags.AddUnique(BlueprintLiftSeamTag);
            Belt->TriggerMeshGeneration(); Belt->ForceApplyHologramMaterial();
            R.Entry.Tier = Tier; R.Entry.BuildClass = Belt->GetBuildClass(); R.Entry.Recipe = Belt->GetRecipe();
            R.Entry.Pose = Belt->GetActorTransform(); R.Entry.Spline = Belt->GetSplineData(); R.Entry.Cost = Belt->GetCost(false);
            Next.Add(MoveTemp(R));
        }
    }
    for (auto& Old : State.Routes) if (Old.Helper)
    { RetireProbe(Root, Old.Helper->GetHologram()); Old.Helper->DestroyPreview(); }
    State.Routes = MoveTemp(Next);
    State.bValid = Valid && !State.Routes.IsEmpty();
    if (State.bValid) State.bReportedUnavailable = false;
    else if (!State.bReportedUnavailable)
    {
        State.bReportedUnavailable = true;
        Subsystem->ShowSmartNotice(NSLOCTEXT("SmartFoundations", "BlueprintLiftConnectionUnavailable",
            "Blueprint lift connections unavailable. Check endpoint direction, spacing and the selected belt tier.").ToString());
    }
    if (!State.bValid) for (auto& R : State.Routes) if (R.Helper)
    { RetireProbe(Root, R.Helper->GetHologram()); R.Helper->DestroyPreview(); }
}

void FSFBlueprintLiftSeams::Capture(AFGHologram* Hologram, FSFScalingSpec& Spec)
{
    Spec.BlueprintLiftSeams.Reset(); Spec.bBlueprintLiftSeamsRequired = false;
    if (const auto* State = BlueprintLiftTransactions.Find(Cast<AFGBlueprintHologram>(Hologram)))
    {
        Spec.bBlueprintLiftSeamsRequired = State->bRequired;
        if (State->bValid) for (const auto& R : State->Routes) Spec.BlueprintLiftSeams.Add(R.Entry);
    }
}

bool FSFBlueprintLiftSeams::IsBounded(const TArray<FSFBlueprintLiftPlanEntry>& Plan)
{
    if (Plan.Num() > 4096) return false;
    TSet<FString> Mouths;
    for (const auto& E : Plan)
    {
        if (E.Tier < 1 || E.Tier > 6 || !E.BuildClass || !E.Recipe || E.Pose.ContainsNaN()
            || E.Spline.Num() < 2 || E.Spline.Num() > 64 || E.Cost.IsEmpty() || E.Cost.Num() > 16) return false;
        for (const auto& Point : E.Spline)
            if (Point.Location.ContainsNaN() || Point.ArriveTangent.ContainsNaN() || Point.LeaveTangent.ContainsNaN()) return false;
        for (const auto& I : E.Cost) if (!I.ItemClass || I.Amount < 1 || I.Amount > 10000) return false;
        for (const auto* P : {&E.From, &E.To})
        {
            if (!P->BuildClass || !P->BuildClass->IsChildOf(AFGBuildableConveyorLift::StaticClass()) || P->Index < 0
                || P->Index > 65535 || P->Name.IsNone() || P->OriginalOwner.ContainsNaN() || P->OwnerPose.ContainsNaN()
                || P->PortLocalPose.ContainsNaN() || P->Direction < 0
                || P->Direction >= static_cast<int32>(EFactoryConnectionDirection::FCD_SNAP_ONLY)) return false;
            if (P->Cell.X < 0 || P->Cell.Y < 0 || P->Cell.Z < 0
                || P->Cell.X > 100000 || P->Cell.Y > 100000 || P->Cell.Z > 100000) return false;
            const FString Key = P->Cell.ToString() + TEXT("|") + FString::FromInt(P->Index);
            if (Mouths.Contains(Key)) return false;
            Mouths.Add(Key);
        }
        if (E.From.Cell.X != E.To.Cell.X || E.From.Cell.Y != E.To.Cell.Y || FMath::Abs(E.From.Cell.Z - E.To.Cell.Z) != 1) return false;
    }
    return true;
}

bool FSFBlueprintLiftSeams::Prepare(AFGBlueprintHologram* Root, const FSFScalingSpec& Spec)
{
    auto Reject = [&](const FString& Reason)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("Blueprint lift plan rejected before payment: %s."), *Reason);
        return false;
    };
    if (!Root || !Root->HasAuthority() || !Spec.bBlueprintLiftSeamsRequired || Spec.BlueprintLiftSeams.IsEmpty()
        || !IsBounded(Spec.BlueprintLiftSeams)) return Reject(TEXT("invalid root or bounded request"));
    if (Root->mBuildableToNewRoot.IsEmpty()) return Reject(TEXT("native root Blueprint content is not staged"));
    // The usual staged Blueprint grid expansion preserves the measured client cell basis.
    bool HasGrid = false;
    for (auto* Child : Root->GetHologramChildren()) { FIntVector Cell; HasGrid |= USFGridCoordComponent::TryGetCell(Child, Cell); }
    if (!HasGrid) SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(Root, Spec, Root->GetRecipe());
    for (auto* Child : Root->GetHologramChildren()) if (auto* Copy = Cast<AFGBlueprintHologram>(Child))
        if (Copy->mBlueprintDescriptor != Root->mBlueprintDescriptor
            || Copy->mBuildableToNewRoot.Num() != Root->mBuildableToNewRoot.Num())
            return Reject(TEXT("native copy descriptor or member population differs"));
    ResetPreview(Root);
    auto* SS = USFSubsystem::Get(Root->GetWorld());
    if (!SS) return Reject(TEXT("authority subsystem unavailable"));
    const FVector Shift = Root->GetActorRotation().RotateVector(
        SFScalingSpecExpansion::MeasureBlueprintContentAnchor(Root) - Spec.ClientParentAnchorRel);
    auto Replay = Spec.BlueprintLiftSeams;
    for (auto& Entry : Replay)
    {
        Entry.Pose.AddToTranslation(Shift); Entry.From.OwnerPose.AddToTranslation(Shift); Entry.To.OwnerPose.AddToTranslation(Shift);
    }
    TGuardValue<const TArray<FSFBlueprintLiftPlanEntry>*> ReplayScope(BlueprintLiftReplayPlan, &Replay);
    TGuardValue<const AFGBlueprintHologram*> ReplayRootScope(BlueprintLiftReplayRoot, Root);
    UpdatePreview(Root, SS, SS->GetAutoConnectService());
    auto* State = BlueprintLiftTransactions.Find(Root);
    if (!State || !State->bValid || State->Routes.Num() != Replay.Num())
        return Reject(FString::Printf(TEXT("layout=%s routes=%d expected=%d nativeMembers=%d"),
            State ? *State->Failure : TEXT("no eligible lift-tail transaction"), State ? State->Routes.Num() : 0,
            Replay.Num(), Root->mBuildableToNewRoot.Num()));
    for (int32 I = 0; I < State->Routes.Num(); ++I)
    {
        const auto& Actual = State->Routes[I].Entry;
        const auto& Expected = Replay[I];
        if (!SamePort(Actual.From, Expected.From) || !SamePort(Actual.To, Expected.To) || Actual.BuildClass != Expected.BuildClass
            || Actual.Recipe != Expected.Recipe || Actual.Tier != Expected.Tier || !LiftSamePose(Actual.Pose, Expected.Pose)
            || !SameLiftSpline(Actual.Spline, Expected.Spline) || !LiftSameCost(Actual.Cost, Expected.Cost))
            return Reject(FString::Printf(TEXT("priced plan mismatch from=%d to=%d class=%d recipe=%d tier=%d pose=%d spline=%d cost=%d"),
                SamePort(Actual.From, Expected.From), SamePort(Actual.To, Expected.To), Actual.BuildClass == Expected.BuildClass,
                Actual.Recipe == Expected.Recipe, Actual.Tier == Expected.Tier, LiftSamePose(Actual.Pose, Expected.Pose),
                SameLiftSpline(Actual.Spline, Expected.Spline), LiftSameCost(Actual.Cost, Expected.Cost)));
    }
    State->bPrepared = true;
    UE_LOG(LogSmartFoundations, Log, TEXT("Blueprint lift plan prepared before payment: %d routes, %d native members per copy."),
        State->Routes.Num(), Root->mBuildableToNewRoot.Num());
    return true;
}

bool FSFBlueprintLiftSeams::IsPrepared(AFGHologram* Root)
{
    const auto* State = BlueprintLiftTransactions.Find(Cast<AFGBlueprintHologram>(Root));
    return State && State->bPrepared;
}

void FSFBlueprintLiftSeams::Construct(AFGBlueprintHologram* Root, const TArray<AActor*>& Copies, FNetConstructionID ID, TArray<AActor*>& Seams)
{
    auto* State = BlueprintLiftTransactions.Find(Root);
    if (!State || !State->bPrepared || State->bConstructed || !Root->HasAuthority()) return;
    struct FReady
    {
        TSharedPtr<FSFLiftRouteNativePreview> Pool;
        ASFLiftRouteBeltHologram* Belt = nullptr;
        UFGFactoryConnectionComponent* From = nullptr;
        UFGFactoryConnectionComponent* To = nullptr;
    };
    TArray<FReady> Ready;
    // Resolve and configure every exact native mouth before creating any seam actor.
    for (const auto& R : State->Routes)
    {
        auto* From = ResolveBuiltLiftMouth(R.Entry.From, Copies); auto* To = ResolveBuiltLiftMouth(R.Entry.To, Copies);
        auto Pool = FSFLiftRouteNativePreview::Acquire(Root, R.Entry.Tier)->CreateConstructionPreview();
        auto* Belt = Pool ? Pool->ConfigureBelt(Root, From, To) : nullptr;
        if (!Belt)
        {
            UE_LOG(LogSmartFoundations, Error, TEXT("Prepared Blueprint lift seam could not resolve its exact constructed mouths."));
            return;
        }
        Belt->SetActorTransform(R.Entry.Pose); Belt->SetSplineDataAndUpdate(R.Entry.Spline);
        if (Belt->GetBuildClass() != R.Entry.BuildClass || Belt->GetRecipe() != R.Entry.Recipe
            || !Belt->IsNativePlacementValid() || !LiftSameCost(Belt->GetCost(false), R.Entry.Cost))
        {
            UE_LOG(LogSmartFoundations, Error, TEXT("Prepared Blueprint lift seam changed after copy construction."));
            return;
        }
        Ready.Add({Pool, Belt, From, To});
    }
    State->bConstructed = true;
    TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>> Connections;
    TArray<AFGBuildableConveyorBase*> Affected;
    for (auto& R : Ready)
    {
        TArray<AActor*> Children;
        auto* Built = R.Belt->Construct(Children, ID);
        if (Built) Seams.AddUnique(Built);
        if (auto* Belt = Cast<AFGBuildableConveyorBelt>(Built))
        {
            Connections.Add({Belt->GetConnection0(), R.From});
            Connections.Add({Belt->GetConnection1(), R.To});
            Affected.AddUnique(Belt);
            for (auto* Port : {R.From, R.To})
                if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(Port->GetOwner())) Affected.AddUnique(Conveyor);
        }
        for (auto* A : Children) Seams.AddUnique(A);
        R.Pool->Hide();
    }
    auto CountConnectedMouths = [&Seams]()
    {
        int32 Count = 0;
        for (auto* Actor : Seams)
        {
            TInlineComponentArray<UFGFactoryConnectionComponent*> Ports(Actor);
            for (auto* Port : Ports) Count += IsValid(Port) && Port->IsConnected();
        }
        return Count;
    };
    const int32 ConnectedBeforeRelease = CountConnectedMouths();
    // Construction cells have operation-local ownership. Observe the committed
    // connections after their normal destruction, before returning to the parent.
    Ready.Reset();
    if (Connections.Num() != State->Routes.Num() * 2)
    {
        UE_LOG(LogSmartFoundations, Error, TEXT("Blueprint lift seam native construction returned an incomplete belt batch."));
        return;
    }
    // Blueprint copies are already materialized here. Complete wiring only after
    // native construction has initialized the new belt and its temporary cells
    // have retired. ConfigureComponents alone does not establish these links.
    // Check the whole exact batch before changing any remaining connection.
    for (const auto& Pair : Connections)
    {
        auto* Mouth = Pair.Key;
        auto* Peer = Pair.Value;
        if (!IsValid(Mouth) || !IsValid(Peer)
            || Mouth->IsConnected() != Peer->IsConnected()
            || (Mouth->IsConnected() && Mouth->GetConnection() != Peer)
            || (Peer->IsConnected() && Peer->GetConnection() != Mouth)
            || (!Mouth->IsConnected() && !Peer->IsConnected() && !Mouth->CanConnectTo(Peer)))
        {
            UE_LOG(LogSmartFoundations, Error, TEXT("Blueprint lift seam native connection changed during materialization."));
            return;
        }
    }
    bool Changed = false;
    for (const auto& Pair : Connections)
        if (!Pair.Key->IsConnected() && !Pair.Value->IsConnected())
        {
            Pair.Key->SetConnection(Pair.Value);
            Changed = true;
        }
    // Only this operation's new conveyors participate; preserve source chains.
    if (Changed)
        if (auto* Sub = USFSubsystem::Get(Root->GetWorld()))
            if (auto* Chains = Sub->GetChainActorService()) Chains->InvalidateAndRebuildForBelts(Affected, {});
    UE_LOG(LogSmartFoundations, Log,
        TEXT("Blueprint lift seams constructed: %d expected, %d new actors, connected mouths before/after cell release=%d/%d."),
        State->Routes.Num(), Seams.Num(), ConnectedBeforeRelease, CountConnectedMouths());
}
