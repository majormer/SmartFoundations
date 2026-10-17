// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/SFRailroadAttachmentHologramState.h"
#include "Features/Scaling/Holograms/SFRailroadAttachmentPreviewHologram.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFHologramDataService.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Hologram/FGRailroadAttachmentHologram.h"
#include "Buildables/FGBuildableRailroadTrack.h"
#include "FGCharacterPlayer.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — native attachment scaling (rail attachments)
//  1. Preview: selected meshes and one reusable native recipe validator per root.
//  2. [MP-AUTH] Local commits resolve all native cells before construction/payment.
//  3. [MP-SEAM] Authority independently resolves staged poses before parent validation;
//     no connection/track pointer from a preview authorizes native attachment.
namespace
{
    TMap<TWeakObjectPtr<AFGHologram>, TWeakObjectPtr<AFGHologram>> RailroadScratchCells;
    TMap<TWeakObjectPtr<AFGHologram>, bool> RailroadPreparedRoots;
    TMap<TWeakObjectPtr<AFGHologram>, TWeakObjectPtr<AFGHologram>> RailroadPreparedCells;
    TArray<TWeakObjectPtr<AFGHologram>> RailroadRequestCells;
    TSet<TWeakObjectPtr<AFGHologram>> RailroadReportedFailures;
    bool bRailroadRequestActive = false;

    USceneComponent* GetRailroadBinding(AFGHologram* Cell)
    {
        const FObjectPropertyBase* Binding = FindFProperty<FObjectPropertyBase>(Cell->GetClass(), TEXT("mSnappedConnection"));
        return Binding ? Cast<USceneComponent>(Binding->GetObjectPropertyValue_InContainer(Cell)) : nullptr;
    }
    AFGHologram* SpawnRailroadCell(AFGHologram* Root, const FTransform& Pose)
    {
        if (!SFRailroadAttachmentHologramState::IsHandled(Root) || !Root->GetRecipe() || !Root->GetWorld()) return nullptr;
        const FObjectPropertyBase* Parent = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
        if (!Parent) return nullptr;
        AFGHologram* Cell = AFGHologram::SpawnHologramFromRecipe(Root->GetRecipe(), Root->GetOwner(),
            Pose.GetLocation(), Root->GetConstructionInstigator(), [Root, Parent](AFGHologram* Spawned)
            { Parent->SetObjectPropertyValue_InContainer(Spawned, Root); });
        if (!IsValid(Cell) || Cell->GetClass() != Root->GetClass())
        { if (IsValid(Cell)) Cell->Destroy(); return nullptr; }
        Cell->SetConstructionInstigator(Root->GetConstructionInstigator());
        Cell->Tags.AddUnique(TEXT("SF_RailroadConstructionCell"));
        Cell->SetActorHiddenInGame(true);
        Cell->SetActorEnableCollision(false);
        Cell->SetActorTickEnabled(false);
        TArray<UPrimitiveComponent*> Primitives;
        Cell->GetComponents(Primitives);
        for (UPrimitiveComponent* Primitive : Primitives) Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        USFHologramDataService::MarkAsChild(Cell, Root, ESFChildHologramType::ScalingGrid);
        USFHologramDataService::EnableValidation(Cell);
        return Cell;
    }
    bool ResolveRailroadCell(AFGHologram* Cell, AFGHologram* Root, const FTransform& Pose)
    {
        if (!IsValid(Cell) || !IsValid(Root) || Root->GetUpgradedActor()) return false;
        // Native placement can unhide the hologram even when its resulting pose
        // is rejected. The reusable validator must never survive a frame visible.
        ON_SCOPE_EXIT { if (IsValid(Cell)) Cell->SetActorHiddenInGame(true); };
        Cell->SetScrollRotateValue(Root->GetScrollRotateValue());
        TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
        Root->GetSupportedBuildModes(Modes);
        for (const auto& Mode : Modes)
            if (Mode && Mode->IsChildOf(UFGHologramBuildModeDescriptor::StaticClass()))
            {
                const TSubclassOf<UFGHologramBuildModeDescriptor> HologramMode(Mode.Get());
                if (Root->IsCurrentBuildMode(HologramMode))
                { Cell->SetBuildModeOverride(HologramMode); Cell->OnBuildModeChanged(HologramMode); break; }
            }
        Cell->SetActorTransform(Pose);
        USceneComponent* RootBinding = GetRailroadBinding(Root);
        const FVector Aim = RootBinding ? Pose.TransformPosition(
            Root->GetActorTransform().InverseTransformPosition(RootBinding->GetComponentLocation())) : Pose.GetLocation();
        TArray<FHitResult> Hits;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartRailroadAttachmentCell), false, Cell);
        Query.AddIgnoredActor(Root);
        if (Root->GetConstructionInstigator()) Query.AddIgnoredActor(Root->GetConstructionInstigator());
        // Rail collision must participate in the trace that supplies native snap
        // candidates. A visibility-only floor trace can miss a valid rail node.
        // Keep the existing floor trace for independently placed end stops.
        const ECollisionChannel TraceChannel = RootBinding ? ECC_WorldStatic : ECC_Visibility;
        Root->GetWorld()->LineTraceMultiByChannel(Hits, Aim + FVector(0, 0, 500), Aim - FVector(0, 0, 500), TraceChannel, Query);
        for (const FHitResult& Hit : Hits)
        {
            if (!IsValid(Hit.GetActor()) || !Cell->IsValidHitResult(Hit) || (RootBinding && !Hit.GetActor()->IsA<AFGBuildableRailroadTrack>())) continue;
            Cell->ResetConstructDisqualifiers();
            // Native floor snapping treats a parented hologram as an already placed
            // companion. Resolve this independent cell as an ordinary native root,
            // then restore construction ownership before validation/cost/commit.
            // Its cell tag and Smart child metadata keep it outside root preparation.
            const FObjectPropertyBase* Parent = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
            if (!Parent) return false;
            Parent->SetObjectPropertyValue_InContainer(Cell, nullptr);
            Cell->UpdateHologramPlacement(Hit);
            Parent->SetObjectPropertyValue_InContainer(Cell, Root);
            const FTransform Resolved = Cell->GetActorTransform();
            const FVector Delta = Resolved.GetLocation() - Pose.GetLocation();
            // An ordinary free buffer's native floor trace can retain the collision
            // skin above the requested floor plane. Keep that native resolved pose;
            // never reposition an accepted native cell to hide the difference.
            // Rail-bound attachments require the exact requested pose and binding.
            const bool bNativeFloorSkin = !RootBinding && Cell->IsA<AFGRailroadAttachmentHologram>()
                && FMath::Abs(Delta.X) <= 0.2 && FMath::Abs(Delta.Y) <= 0.2 && FMath::Abs(Delta.Z) <= 2.0
                && Resolved.GetRotation().Equals(Pose.GetRotation(), 0.001)
                && Resolved.GetScale3D().Equals(Pose.GetScale3D(), 0.001);
            if (Cell->GetUpgradedActor() || (!Resolved.Equals(Pose, 0.2) && !bNativeFloorSkin)
                || (RootBinding && !GetRailroadBinding(Cell))) continue;
            if (Root->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Root->GetBlueprintDesigner());
            AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Root->GetConstructionInstigator());
            Cell->SetActorHiddenInGame(false);
            Cell->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
            const bool Valid = Cell->CanConstruct();
            Cell->SetActorHiddenInGame(true);
            return Valid;
        }
        return false;
    }
}

bool SFRailroadAttachmentHologramState::IsHandled(const AFGHologram* Root)
{
    if (!IsValid(Root) || !Root->GetBuildClass() || Root->ActorHasTag(TEXT("SF_RailroadConstructionCell"))) return false;
    const FName Class = Root->GetBuildClass()->GetFName();
    return Root->IsA<AFGRailroadAttachmentHologram>() && Class == TEXT("Build_RailroadEndStop_C");
}
bool SFRailroadAttachmentHologramState::HasProxyGrid(AFGHologram* Root)
{
    if (!IsHandled(Root) || Root->GetParentHologram()) return false;
    for (AFGHologram* Child : Root->GetHologramChildren())
        if (IsValid(Child) && !Child->IsDisabled() && Child->IsA<ASFRailroadAttachmentPreviewHologram>()) return true;
    return false;
}
bool SFRailroadAttachmentHologramState::ValidatePreview(ASFRailroadAttachmentPreviewHologram* Preview)
{
    AFGHologram* Root = Preview->GetParentHologram();
    if (!IsHandled(Root)) return false;
    auto& Entry = RailroadScratchCells.FindOrAdd(Root);
    if (!Entry.IsValid()) Entry = SpawnRailroadCell(Root, Preview->GetRequestedPose());
    if (!Entry.IsValid() || !ResolveRailroadCell(Entry.Get(), Root, Preview->GetRequestedPose()))
    {
        if (Entry.IsValid() && !RailroadReportedFailures.Contains(Root))
        {
            RailroadReportedFailures.Add(Root);
            TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
            Entry->GetConstructDisqualifiers(Disqualifiers);
            UE_LOG(LogSmartFoundations, Log, TEXT("Rail attachment preview validation rejected: requested=%s resolved=%s poseMatch=%d disabled=%d hidden=%d canConstruct=%d disqualifiers=%d instigator=%d constructionPlayer=%d binding=%d upgraded=%d"),
                *Preview->GetRequestedPose().GetLocation().ToString(), *Entry->GetActorLocation().ToString(),
                Entry->GetActorTransform().Equals(Preview->GetRequestedPose(), 0.2), Entry->IsDisabled(), Entry->IsHidden(),
                Entry->CanConstruct(), Disqualifiers.Num(), IsValid(Entry->GetInstigator()), IsValid(Entry->GetConstructionInstigator()),
                IsValid(GetRailroadBinding(Entry.Get())), IsValid(Entry->GetUpgradedActor()));
        }
        return false;
    }
    Preview->ApplyResolvedPose(Entry->GetActorTransform());
    return true;
}
void SFRailroadAttachmentHologramState::ReleasePreview(ASFRailroadAttachmentPreviewHologram* Preview)
{
    AFGHologram* Root = Preview->GetParentHologram();
    if (IsValid(Root)) for (AFGHologram* Child : Root->GetHologramChildren())
        if (Child != Preview && IsValid(Child) && !Child->IsDisabled() && Child->IsA<ASFRailroadAttachmentPreviewHologram>()) return;
    TWeakObjectPtr<AFGHologram> Cell;
    if (RailroadScratchCells.RemoveAndCopyValue(Root, Cell) && Cell.IsValid()) Cell->Destroy();
    RailroadReportedFailures.Remove(Root);
    for (auto It = RailroadScratchCells.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
}
bool SFRailroadAttachmentHologramState::Prepare(AFGHologram* Root, const FSFScalingSpec* Spec)
{
    if (!bRailroadRequestActive || !IsHandled(Root) || Root->GetParentHologram() || !Root->HasAuthority()) return false;
    if (const bool* Existing = RailroadPreparedRoots.Find(Root)) return *Existing;
    RailroadPreparedRoots.Add(Root, false);
    TArray<ASFRailroadAttachmentPreviewHologram*> Previews;
    TArray<FTransform> Poses;
    for (AFGHologram* Child : Root->GetHologramChildren())
        if (auto* Preview = Cast<ASFRailroadAttachmentPreviewHologram>(Child); IsValid(Preview) && !Preview->IsDisabled())
        { Preview->RememberRequestedPose(); Previews.Add(Preview); Poses.Add(Preview->GetRequestedPose()); }
    if (Spec && Spec->CellCount() > 1 && Previews.IsEmpty())
    {
        if (!Spec->bValid || Spec->BuildClass != Root->GetBuildClass()) return false;
        const FSFCounterState& C = Spec->Counters;
        FSFPositionCalculator Calculator;
        int32 Index = 0;
        for (int32 Z = 0; Z < FMath::Abs(C.GridCounters.Z); ++Z)
            for (int32 Y = 0; Y < FMath::Abs(C.GridCounters.Y); ++Y)
                for (int32 X = 0; X < FMath::Abs(C.GridCounters.X); ++X)
                {
                    const int32 GX = X * FMath::Sign(C.GridCounters.X), GY = Y * FMath::Sign(C.GridCounters.Y), GZ = Z * FMath::Sign(C.GridCounters.Z);
                    const FVector Location = Calculator.CalculateChildPosition(GX, GY, GZ, Root->GetActorLocation(),
                        Root->GetActorRotation(), Spec->ItemSize, C, Index++, FVector::ZeroVector);
                    if (!X && !Y && !Z) continue;
                    FRotator Rotation = Root->GetActorRotation();
                    Rotation.Yaw += (C.RotationAxis == ESFScaleAxis::Y ? -GY : GX) * C.RotationZ;
                    Poses.Emplace(Rotation, Location);
                }
    }
    TArray<AFGHologram*> Accepted;
    for (const FTransform& Pose : Poses)
    {
        AFGHologram* Cell = SpawnRailroadCell(Root, Pose);
        if (!Cell) return false;
        RailroadRequestCells.Add(Cell);
        if (!ResolveRailroadCell(Cell, Root, Pose)) return false;
            Accepted.Add(Cell);
    }
    for (int32 Index = 0; Index < Accepted.Num(); ++Index)
        if (Previews.IsEmpty())
        {
            Root->AddChild(Accepted[Index], FName(*FString::Printf(TEXT("RailroadAttachmentGrid_%d"), Index)));
            Accepted[Index]->Tags.AddUnique(TEXT("SF_GridChild"));
            Accepted[Index]->SetActorHiddenInGame(false);
        }
        else RailroadPreparedCells.Add(Previews[Index], Accepted[Index]);
    RailroadPreparedRoots[Root] = true;
    return true;
}
AFGHologram* SFRailroadAttachmentHologramState::FindPreparedCell(AFGHologram* Preview)
{
    const auto* Cell = RailroadPreparedCells.Find(Preview);
    return bRailroadRequestActive && Cell ? Cell->Get() : nullptr;
}
bool SFRailroadAttachmentHologramState::IsRequestActive() { return bRailroadRequestActive; }
SFRailroadAttachmentHologramState::FRequestScope::FRequestScope()
{
    bOwnsRequest = !bRailroadRequestActive;
    bRailroadRequestActive = true;
}
SFRailroadAttachmentHologramState::FRequestScope::~FRequestScope()
{
    if (!bOwnsRequest) return;
    for (const auto& Cell : RailroadRequestCells) if (Cell.IsValid()) Cell->Destroy();
    RailroadRequestCells.Reset();
    RailroadPreparedRoots.Reset();
    RailroadPreparedCells.Reset();
    bRailroadRequestActive = false;
}
