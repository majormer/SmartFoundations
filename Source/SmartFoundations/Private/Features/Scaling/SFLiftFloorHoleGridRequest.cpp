// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFLiftFloorHoleGridRequest.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ObjectKey.h"
#include "UObject/StrongObjectPtr.h"
#include "Hologram/FGHologram.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "FGConstructDisqualifier.h"
#include "FGBuildableSubsystem.h"
#include "UObject/UnrealType.h"
#include "Core/Construction/SFNativeConstructionContext.h"
#include "Features/Scaling/SFFoundationPassthroughState.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Features/Scaling/Holograms/SFFoundationPassthroughPreviewHologram.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Holograms/Logistics/SFPassthroughChildHologram.h"
#include "Core/Construction/SFNativeFloorHolePlacement.h"

// SP/MP DIVERGENCE MAP — lift floor-hole grids
// [MP-AUTH] Resolve native mounting/geometry/price for all cells before parent effects.
// [MP-SEAM] Local visual proxies reuse standalone prepared cells; remote grids
// use the exact authority-expanded cells. No live lift/face/connection is changed.
namespace
{
struct FRoot
{
    TWeakObjectPtr<UWorld> World;
    TSet<FObjectKey> Original;
    TArray<TWeakObjectPtr<AFGHologram>> Generated;
    TArray<FItemAmount> Cost;
    FGuid Id = FGuid::NewGuid();
    TWeakObjectPtr<AActor> Payer;
    bool bReady = false;
    bool bUncertain = false;
};
struct FCell
{
    TWeakObjectPtr<AFGHologram> Root;
    FObjectKey RootKey;
    TWeakObjectPtr<UWorld> World;
    TArray<TWeakObjectPtr<AActor>> Foundations;
    TArray<FTransform> FoundationPoses;
    TWeakObjectPtr<AFGHologram> Native;
    TWeakObjectPtr<AActor> Built;
    FTransform Pose;
    FNetConstructionID AttemptId;
    float Thickness = 0;
    bool bAttached = false;
    bool bClaimed = false;
    bool bObserved = false;
    bool bResultValid = false;
};
TMap<TWeakObjectPtr<AFGHologram>, FRoot> Roots;
TMap<TWeakObjectPtr<AFGHologram>, TWeakObjectPtr<AFGHologram>> Proxies;
TMap<TWeakObjectPtr<AFGHologram>, FCell> Cells;
int32 Depth = 0;
struct FUncertain
{
    FObjectKey WorldKey;
    FRoot Request;
    TArray<FCell> Receipts;
};
TMap<FGuid, FUncertain> Uncertain;
void BindCleanup()
{
    static const auto Handle = FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* World, bool, bool)
    {
        int32 Outstanding = 0;
        for (auto It = Uncertain.CreateIterator(); It; ++It)
            if (It.Value().WorldKey == FObjectKey(World)) { ++Outstanding; It.RemoveCurrent(); }
        if (Outstanding) UE_LOG(LogSmartFoundations, Warning, TEXT("World ended with %d unresolved lift floor-hole grid receipt(s); no payment settlement or rollback inferred."), Outstanding);
    });
    (void)Handle;
}
}
FSFLiftFloorHoleGridRequest::FScope::FScope()
{
    bEntered = IsInGameThread(); if (bEntered) ++Depth;
}
FSFLiftFloorHoleGridRequest::FScope::~FScope()
{
    if (bEntered && --Depth == 0) FSFLiftFloorHoleGridRequest::EndRequest();
}
bool FSFLiftFloorHoleGridRequest::IsHandled(const AFGHologram* Root)
{
    return IsValid(Root) && !Root->ActorHasTag(TEXT("SF_NativeFloorHoleValidator"))
        && SFFoundationPassthroughState::IsHandled(Root) && Root->GetBuildClass()->GetFName() == TEXT("Build_FoundationPassthrough_Lift_C");
}
bool FSFLiftFloorHoleGridRequest::Prepare(AFGHologram* Root, const FSFScalingSpec* Spec)
{
    if (!IsInGameThread() || Depth <= 0 || !IsHandled(Root) || !Root->HasAuthority()
        || Root->GetNetMode() == NM_Client || !Root->GetWorld() || Root->GetWorld()->bIsTearingDown) return false;
    if (const auto* Existing = Roots.Find(Root)) return Existing->bReady;
    TArray<AFGHologram*> Visuals;
    for (auto* Child : Root->GetHologramChildren())
        if (IsValid(Child) && !Child->IsDisabled() && Child->IsA<ASFFoundationPassthroughPreviewHologram>()) Visuals.Add(Child);
    if (Visuals.IsEmpty() && !Spec) return true;
    FRoot Request; Request.World = Root->GetWorld(); Request.Payer = Root->GetConstructionInstigator();
    BindCleanup();
    for (auto* Child : Root->GetHologramChildren()) if (Child) Request.Original.Add(FObjectKey(Child));
    Roots.Add(Root, MoveTemp(Request));
    auto Reject = [&](const FString& Why)
    {
        Root->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
        UE_LOG(LogSmartFoundations, Warning, TEXT("Lift floor-hole grid rejected before payment: %s."), *Why);
        return false;
    };
    float Thickness = 0;
    if (!SFFoundationPassthroughState::ReadThickness(Root, Thickness)) return Reject(TEXT("selected_thickness_unavailable"));
    FString RootReason;
    if (!FSFNativeFloorHolePlacement::Validate(Root, Root, Root->GetActorTransform(), Thickness, RootReason)) return Reject(RootReason);
    TArray<AFGHologram*> NativeCells;
    if (!Visuals.IsEmpty())
    {
        for (auto* Preview : Visuals)
        {
            auto* Proxy = Cast<ASFFoundationPassthroughPreviewHologram>(Preview);
            Proxy->SyncGeometry(Root);
            if (!Proxy->IsGeometryReady()) return Reject(TEXT("selected_proxy_geometry_unavailable"));
            auto* Cell = SFFoundationPassthroughState::SpawnConstructionCell(Root, Preview,
                TEXT("PreparedLiftFloorHole"), Preview->GetActorTransform(), false);
            if (!Cell) return Reject(TEXT("native_cell_spawn_failed"));
            FCell Record; Record.Root = Root; Record.RootKey = FObjectKey(Root); Record.World = Root->GetWorld(); Record.Native = Cell;
            Record.Pose = Preview->GetActorTransform(); Record.Thickness = Thickness;
            Cells.Add(Cell, Record); Proxies.Add(Preview, Cell); NativeCells.Add(Cell);
        }
    }
    else
    {
        if (!Spec->bValid || Spec->BuildClass != Root->GetBuildClass()) return Reject(TEXT("invalid_grid_intent"));
        const int32 Made = Spec->CellCount() > 1
            ? SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(Root, *Spec, Root->GetRecipe()) : 0;
        // Snapshot this expansion's outputs now. Cleanup must not sweep up
        // another callback's later children merely because they share a tag.
        if (auto* State = Roots.Find(Root))
            for (auto* Child : Root->GetHologramChildren())
                if (IsValid(Child) && !State->Original.Contains(FObjectKey(Child))
                    && Child->ActorHasTag(TEXT("SF_GridChild"))) State->Generated.Add(Child);
        for (auto* Cell : Root->GetHologramChildren())
            if (IsValid(Cell) && Cell->IsA<ASFPassthroughChildHologram>() && Cell->ActorHasTag(TEXT("SF_GridChild"))
                && Cell->GetBuildClass() == Root->GetBuildClass())
            {
                FCell Record; Record.Root = Root; Record.RootKey = FObjectKey(Root); Record.World = Root->GetWorld(); Record.Native = Cell;
                Record.Pose = Cell->GetActorTransform(); Record.Thickness = Thickness; Record.bAttached = true;
                Cells.Add(Cell, Record); NativeCells.Add(Cell);
            }
        if (Made != Spec->CellCount() - 1 || NativeCells.Num() != Made) return Reject(TEXT("incomplete_native_grid"));
    }
    TArray<FTransform> Accepted;
    Accepted.Add(Root->GetActorTransform());
    for (auto* Cell : NativeCells)
    {
        const auto Pose = Cell->GetActorTransform();
        for (const auto& Other : Accepted) if (Other.Equals(Pose, .1f)) return Reject(TEXT("duplicate_floor_hole_cell"));
        Accepted.Add(Pose);
        FString Reason;
        if (!FSFNativeFloorHolePlacement::Validate(Root, Cell, Pose, Thickness, Reason)) return Reject(Reason);
        auto* Property = FindFProperty<FArrayProperty>(Cell->GetClass(), TEXT("mSnappedFoundations"));
        auto* Object = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
        auto* Record = Cells.Find(Cell);
        if (!Object || !Record) return Reject(TEXT("native_mounting_context_unavailable"));
        FScriptArrayHelper Mounted(Property, Property->ContainerPtrToValuePtr<void>(Cell));
        for (int32 Index = 0; Index < Mounted.Num(); ++Index)
        {
            auto* Foundation = Cast<AActor>(Object->GetObjectPropertyValue(Mounted.GetRawPtr(Index)));
            if (!IsValid(Foundation)) return Reject(TEXT("native_mounting_foundation_expired"));
            Record->Foundations.Add(Foundation); Record->FoundationPoses.Add(Foundation->GetActorTransform());
        }
    }
    for (const auto& Pair : Proxies)
    {
        auto* Cell = Pair.Value.Get(); auto* Preview = Pair.Key.Get();
        const auto* Record = Cells.Find(Cell);
        if (!Record || Record->Root.Get() != Root) continue;
        if (!IsValid(Preview)) return Reject(TEXT("priced_proxy_retired"));
        TMap<UClass*, int64> Quote;
        for (const auto& I : Cell->GetCost(false)) Quote.FindOrAdd(I.ItemClass.Get()) += I.Amount;
        for (const auto& I : Preview->GetCost(false)) Quote.FindOrAdd(I.ItemClass.Get()) -= I.Amount;
        for (const auto& I : Quote) if (I.Value) return Reject(TEXT("proxy_native_price_mismatch"));
    }
    // The normal native quote sums the selected root and the validated cells (or
    // their same-price visual proxies). Keep it through local post-Construct payment.
    const auto Cost = Root->GetCost(true);
    auto* State = Roots.Find(Root);
    if (!State) return Reject(TEXT("request_lifetime_changed"));
    State->Cost = Cost; State->bReady = true;
    return true;
}
bool FSFLiftFloorHoleGridRequest::IsPrepared(const AFGHologram* Root)
{
    const auto* State = Roots.Find(const_cast<AFGHologram*>(Root)); return State && State->bReady;
}
AFGHologram* FSFLiftFloorHoleGridRequest::FindCell(AFGHologram* Preview)
{
    const auto* Cell = Proxies.Find(Preview); return Depth > 0 && Cell ? Cell->Get() : nullptr;
}
bool FSFLiftFloorHoleGridRequest::ClaimCell(AFGHologram* Cell, const FNetConstructionID& Id)
{
    auto* Record = Cells.Find(Cell);
    auto* Request = Record ? Roots.Find(Record->Root) : nullptr;
    auto Reject = [&]() { if (Request) Request->bUncertain = true; return false; };
    if (!IsInGameThread() || Depth <= 0 || !Record || !Request || !Request->bReady || Request->bUncertain || Record->bClaimed
        || !IsValid(Cell) || Cell->IsActorBeingDestroyed() || !Cell->HasAuthority()
        || !Record->World.IsValid() || Record->World->bIsTearingDown || !Request->Payer.IsValid()
        || Cell->GetConstructionInstigator() != Request->Payer.Get()
        || Cell->GetWorld() != Record->World.Get() || !Cell->GetActorTransform().Equals(Record->Pose, .1f)
        || !FSFNativeConstructionContext::MatchesInput(Record->RootKey, Id, Cell->GetWorld())) return Reject();
    auto* Property = FindFProperty<FArrayProperty>(Cell->GetClass(), TEXT("mSnappedFoundations"));
    auto* Object = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
    if (!Object || Record->Foundations.Num() != Record->FoundationPoses.Num()) return Reject();
    FScriptArrayHelper Mounted(Property, Property->ContainerPtrToValuePtr<void>(Cell));
    if (Mounted.Num() != Record->Foundations.Num()) return Reject();
    for (int32 Index = 0; Index < Mounted.Num(); ++Index)
    {
        const auto& Weak = Record->Foundations[Index];
        if (!Weak.IsValid() || Weak->IsActorBeingDestroyed() || Weak->GetWorld() != Record->World.Get()
            || Object->GetObjectPropertyValue(Mounted.GetRawPtr(Index)) != Weak.Get()
            || !Weak->GetActorTransform().Equals(Record->FoundationPoses[Index], .01f)) return Reject();
    }
    float Thickness = 0;
    if (!SFFoundationPassthroughState::ReadThickness(Cell, Thickness) || !FMath::IsNearlyEqual(Thickness, Record->Thickness, .01f)) return Reject();
    Record->AttemptId = Id; Record->bClaimed = true; return true;
}
void FSFLiftFloorHoleGridRequest::ObserveCell(AFGHologram* Cell, AActor* Built)
{
    auto* Record = Cells.Find(Cell);
    if (!Record || !Record->bClaimed || Record->bObserved) return;
    Record->bObserved = true; Record->Built = Built;
    auto* Hole = Cast<AFGBuildablePassthrough>(Built);
    Record->bResultValid = IsValid(Hole) && !Hole->IsActorBeingDestroyed() && Hole->GetWorld() == Record->World.Get()
        && Hole->GetClass() == Cell->GetBuildClass().Get() && Hole->GetActorTransform().Equals(Record->Pose, .1f)
        && FMath::IsNearlyEqual(Hole->GetSnappedBuildingThickness(), Record->Thickness, .01f);
    if (!Record->bResultValid)
    {
        if (auto* Request = Roots.Find(Record->Root)) Request->bUncertain = true;
        UE_LOG(LogSmartFoundations, Warning, TEXT("Lift floor-hole cell did not match its native prepared result; parent materialization/payment remain separate and are not certified."));
    }
}
bool FSFLiftFloorHoleGridRequest::GetPreparedCost(const AFGHologram* Root, TArray<FItemAmount>& Cost)
{
    const auto* State = Roots.Find(const_cast<AFGHologram*>(Root));
    if (!State || !State->bReady) return false; Cost = State->Cost; return true;
}
void FSFLiftFloorHoleGridRequest::EndRequest()
{
    auto OldCells = MoveTemp(Cells); Cells.Reset();
    auto OldRoots = MoveTemp(Roots); Roots.Reset(); Proxies.Reset();
    // Preserve exact input/output identities and accepted quote for a failed
    // native batch independently of the soon-to-be-retired preview cells.
    for (const auto& Pair : OldRoots)
    {
        int32 ExpectedCells = 0, ValidCells = 0;
        FUncertain Retained; Retained.WorldKey = FObjectKey(Pair.Value.World.Get()); Retained.Request = Pair.Value;
        for (const auto& Cell : OldCells) if (Cell.Value.Root == Pair.Key)
        {
            Retained.Receipts.Add(Cell.Value);
            ExpectedCells += Pair.Value.bReady;
            ValidCells += Cell.Value.bResultValid && Cell.Value.Built.IsValid();
        }
        if (Pair.Value.bReady && (Pair.Value.bUncertain || ValidCells != ExpectedCells))
        {
            Uncertain.Add(Pair.Value.Id, MoveTemp(Retained));
            UE_LOG(LogSmartFoundations, Warning, TEXT("Lift floor-hole grid %s retains partial native results (%d/%d valid cells); exact receipts and quote retained, parent payment unverified, no automatic retry or refund."),
                *Pair.Value.Id.ToString(), ValidCells, ExpectedCells);
        }
    }
    int32 Expected = 0, Attempted = 0, Observed = 0;
    for (const auto& Pair : OldCells)
    {
        const auto& Record = Pair.Value;
        const auto* Owner = OldRoots.Find(Record.Root);
        Expected += Owner && Owner->bReady;
        Attempted += Record.bClaimed; Observed += Record.bResultValid && Record.Built.IsValid();
        auto* Cell = Record.Native.Get(); auto* Root = Record.Root.Get();
        if (Record.bAttached && IsValid(Root) && Cell)
        {
            Root->mChildren.Remove(Cell);
            for (auto It = Root->mChildrenNameLookupMap.CreateIterator(); It; ++It)
                if (It.Value() == Cell) It.RemoveCurrent();
        }
        if (IsValid(Cell)) Cell->Destroy();
    }
    // Remove even a failed expansion's unregistered preview nodes, never built actors.
    for (const auto& Pair : OldRoots)
        if (auto* Root = Pair.Key.Get(); IsValid(Root))
        {
            for (const auto& Weak : Pair.Value.Generated)
                if (auto* Child = Weak.Get(); IsValid(Child))
                {
                    Root->mChildren.Remove(Child);
                    for (auto It = Root->mChildrenNameLookupMap.CreateIterator(); It; ++It) if (It.Value() == Child) It.RemoveCurrent();
                    if (IsValid(Child)) Child->Destroy();
                }
        }
    if (Expected)
        UE_LOG(LogSmartFoundations, Log, TEXT("Lift floor-hole grid native results: expected=%d attempted=%d validObserved=%d; native parent payment is separate."), Expected, Attempted, Observed);
}
