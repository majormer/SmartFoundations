// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/SFConveyorLiftHologramState.h"
#include "Core/SFNativeHit.h"
#include "Features/Scaling/Holograms/SFConveyorLiftPreviewHologram.h"
#include "Features/Scaling/SFScalingSpec.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFHologramDataService.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Buildables/FGBuildableFoundation.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "EngineUtils.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "FGFactoryConnectionComponent.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGCharacterPlayer.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — conveyor lift scaling
//  1. Preview: selected meshes and one reusable native recipe validator per root.
//  2. [MP-AUTH] Local commits resolve all native cells before construction/payment.
//  3. [MP-SEAM] Authority independently resolves staged poses before parent validation;
//     no copied parent connection pointer authorizes a cell's attachment.
class FSFConveyorLiftNativePlacement
{
public:
    static bool SetInternalLock(AFGHologram* Cell, bool bLocked)
    {
        if (!IsValid(Cell) || !Cell->IsA<AFGConveyorLiftHologram>()
            || !Cell->ActorHasTag(TEXT("SF_LiftGridConstructionCell"))) return false;
        if (bLocked && !Cell->CanLockHologram()) return false;
        // LockHologramPosition publishes map/compass state for a player's active
        // hologram. A hidden validator has no player marker: repeatedly using that
        // UI operation here exhausts widgets/UObjects. Preserve the native local
        // lock state, then let CanNudge/NudgeTowardsWorldDirection enforce bounds.
        Cell->mHologramIsLocked = bLocked;
        if (bLocked) Cell->mHologramLockLocation = Cell->GetActorLocation();
        else Cell->mHologramNudgeOffset = FVector::ZeroVector;
        return true;
    }
};
namespace
{
    TMap<TWeakObjectPtr<AFGHologram>, TWeakObjectPtr<AFGHologram>> LiftGridScratchCells;
    TMap<TWeakObjectPtr<AFGHologram>, bool> LiftGridPreparedRoots;
    TMap<TWeakObjectPtr<AFGHologram>, TWeakObjectPtr<AFGHologram>> LiftGridPreparedCells;
    TArray<TWeakObjectPtr<AFGHologram>> LiftGridRequestCells;
    bool bLiftGridRequestActive = false;
    UFGFactoryConnectionComponent* GetBinding(AFGHologram* Hologram, int32 Index);

    bool MatchesSelectedPose(const FTransform& Actual, const FTransform& Requested)
    {
        return Actual.GetLocation().Equals(Requested.GetLocation(), 2.0)
            && Actual.GetRotation().Equals(Requested.GetRotation(), 0.001)
            && Actual.GetScale3D().Equals(Requested.GetScale3D(), 0.001);
    }

    AFGBuildablePassthrough* GetPassthrough(AFGHologram* Root, int32 Index)
    {
        const FArrayProperty* Property = FindFProperty<FArrayProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedPassthroughs"));
        const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
        if (!Inner) return nullptr;
        FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Root));
        return Array.IsValidIndex(Index) ? Cast<AFGBuildablePassthrough>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index))) : nullptr;
    }

    void AppendPassthroughHits(TArray<FHitResult>& Hits, AFGHologram* Root, int32 Index, const FTransform& Pose, const FCollisionQueryParams& Query)
    {
        AFGBuildablePassthrough* Hole = GetPassthrough(Root, Index);
        if (!IsValid(Hole)) return;
        const FVector Aim = Pose.TransformPosition(Root->GetActorTransform().InverseTransformPosition(Hole->GetActorLocation()));
        FVector Origin, Extent;
        Hole->GetActorBounds(false, Origin, Extent);
        const double Radius = Extent.X > 0 && Extent.Y > 0 ? FMath::Min(Extent.X, Extent.Y) * 0.85 : 90.0;
        const double Span = FMath::Max(300.0, Hole->GetSnappedBuildingThickness() * 0.5 + 150.0);
        const FStructProperty* Top = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform"));
        if (!Top || Index < 0 || Index > 1) return;
        const double Height = Top->ContainerPtrToValuePtr<FTransform>(Root)->GetTranslation().Z;
        // Native passthrough snapping selects the approached face. An ascending
        // first end leaves the top; its finishing end enters the bottom. Probing
        // every case from above instead hits an occupied opposite face.
        const double Face = (Index == 0 ? Height > 0 : Height < 0) ? 1.0 : -1.0;
        // A floor hole's opening is hollow. Probe its actual rim, as ordinary
        // placement does, rather than inventing a hit through the empty centre.
        const FVector X = Pose.TransformVectorNoScale(Root->GetActorTransform().InverseTransformVectorNoScale(Hole->GetActorForwardVector())) * Radius;
        const FVector Y = Pose.TransformVectorNoScale(Root->GetActorTransform().InverseTransformVectorNoScale(Hole->GetActorRightVector())) * Radius;
        for (const FVector& Offset : {FVector::ZeroVector, X, -X, Y, -Y})
        {
            TArray<FHitResult> Probe;
            Root->GetWorld()->LineTraceMultiByChannel(Probe, Aim + Offset + FVector(0, 0, Span * Face),
                Aim + Offset - FVector(0, 0, Span * Face), ECC_WorldStatic, Query);
            Hits.Append(Probe);
        }
    }

    bool ReservePassthroughFaces(AFGHologram* Cell, TMap<TWeakObjectPtr<AFGBuildablePassthrough>, uint8>& Reserved)
    {
        const FArrayProperty* Property = FindFProperty<FArrayProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedPassthroughs"));
        const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
        const FStructProperty* Top = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform"));
        if (!Inner || !Top) return false;
        const double Height = Top->ContainerPtrToValuePtr<FTransform>(Cell)->GetTranslation().Z;
        FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Cell));
        for (int32 Index = 0; Index < Array.Num(); ++Index)
        {
            UObject* Value = Inner->GetObjectPropertyValue(Array.GetRawPtr(Index));
            if (!Value) continue;
            auto* Hole = Cast<AFGBuildablePassthrough>(Value);
            if (!IsValid(Hole) || Index > 1 || FMath::IsNearlyZero(Height)) return false;
            // The starting end of an ascending lift leaves through the top face;
            // its finishing end enters through the bottom. Descending swaps these.
            const uint8 Face = (Index == 0 ? Height > 0 : Height < 0) ? 1 : 2;
            uint8& Faces = Reserved.FindOrAdd(Hole);
            if (Faces & Face) return false;
            Faces |= Face;
        }
        return true;
    }

    bool MatchesNativeFloorSnap(AFGHologram* Root, AFGHologram* Cell, const FHitResult& Hit, const FTransform& Pose)
    {
        if (MatchesSelectedPose(Cell->GetActorTransform(), Pose)) return true;
        // Ordinary foundation snapping can quantize a curved grid request. Show and
        // build that native result rather than repositioning it to an unsnapped pose.
        // Network/passthrough anchors still require their exact requested placement.
        AActor* Floor = Hit.GetActor();
        const FFloatProperty* Grid = FindFProperty<FFloatProperty>(AFGBuildableHologram::StaticClass(), TEXT("mGridSnapSize"));
        if (!SFConveyorLiftHologramState::CanReplayFloorPlacement(Root, Hit) || !Grid || GetBinding(Root, 0) || GetPassthrough(Root, 0)) return false;
        const float Step = Grid->GetPropertyValue_InContainer(Cell);
        const FTransform Actual = Cell->GetActorTransform();
        const FVector Delta = Floor->GetActorTransform().InverseTransformVectorNoScale(Actual.GetLocation() - Pose.GetLocation());
        return FMath::IsFinite(Step) && Step > 0 && FMath::Abs(Delta.X) <= Step * 0.5 + 0.1
            && FMath::Abs(Delta.Y) <= Step * 0.5 + 0.1 && FMath::Abs(Delta.Z) <= 2.0
            && Actual.GetRotation().Equals(Pose.GetRotation(), 0.001)
            && Actual.GetScale3D().Equals(Pose.GetScale3D(), 0.001);
    }

    UFGFactoryConnectionComponent* GetBinding(AFGHologram* Hologram, int32 Index)
    {
        const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedConnectionComponents"));
        return IsValid(Hologram) && Property && Index >= 0 && Index < Property->ArrayDim
            ? Cast<UFGFactoryConnectionComponent>(Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Hologram, Index))) : nullptr;
    }

    FVector RequestedBindingLocation(AFGHologram* Root, int32 Index, const FTransform& Pose)
    {
        UFGFactoryConnectionComponent* Source = GetBinding(Root, Index);
        // Native mouth clearance may separate the lift origin and its peer by several
        // metres. Transform the selected relative coordinate, never its object pointer.
        return Pose.TransformPosition(Root->GetActorTransform().InverseTransformPosition(Source->GetComponentLocation()));
    }

    bool MatchesRequestedBinding(AFGHologram* Cell, AFGHologram* Root, int32 Index, const FTransform& Pose)
    {
        const UFGFactoryConnectionComponent* Source = GetBinding(Root, Index);
        const UFGFactoryConnectionComponent* Binding = GetBinding(Cell, Index);
        return IsValid(Source) && IsValid(Binding) && Binding->GetOwner() != Root && Binding->GetOwner() != Cell
            && Binding->GetDirection() == Source->GetDirection()
            && Binding->GetComponentLocation().Equals(RequestedBindingLocation(Root, Index, Pose), 5.0);
    }

    bool ReplaySelectedNudge(AFGHologram* Cell, const FVector& Offset)
    {
        if (Offset.IsNearlyZero()) return true;
        if (!FSFConveyorLiftNativePlacement::SetInternalLock(Cell, true)) return false;
        if (!Cell->CanNudgeHologram()) return false;
        const float Distance = Cell->GetNudgeDistance();
        if (!FMath::IsFinite(Distance) || Distance <= 0) return false;
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const int32 Count = FMath::RoundToInt(Offset[Axis] / Distance);
            if (FMath::Abs(Count) > 128 || !FMath::IsNearlyEqual(Count * Distance, Offset[Axis], 0.1)) return false;
            FVector Direction = FVector::ZeroVector;
            Direction[Axis] = FMath::Sign(Count);
            for (int32 Step = 0; Step < FMath::Abs(Count); ++Step)
                if (Cell->NudgeTowardsWorldDirection(Direction) != ENudgeFailReason::NFR_Success) return false;
        }
        Cell->SetHologramNudgeLocation();
        return Cell->GetHologramNudgeOffset().Equals(Offset, 0.1);
    }

    bool ResolveSelectedEnd(AFGHologram* Cell, AFGHologram* Root, const FTransform& Pose)
    {
        // Replay the second native placement phase against real world geometry when
        // the selected parent end is attached. No parent's binding is assigned to a copy.
        const bool bConnection = GetBinding(Root, 1) != nullptr;
        const bool bPassthrough = GetPassthrough(Root, 1) != nullptr;
        if (!bConnection && !bPassthrough) return true;
        TArray<FHitResult> Hits;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartLiftSelectedEnd), false, Cell);
        Query.AddIgnoredActor(Root);
        if (Root->GetConstructionInstigator()) Query.AddIgnoredActor(Root->GetConstructionInstigator());
        if (bPassthrough) AppendPassthroughHits(Hits, Root, 1, Pose, Query);
        else
        {
            const FVector Location = RequestedBindingLocation(Root, 1, Pose);
            const FVector Normal = Pose.TransformVectorNoScale(Root->GetActorTransform().InverseTransformVectorNoScale(GetBinding(Root, 1)->GetForwardVector()));
            Root->GetWorld()->LineTraceMultiByChannel(Hits, Location + Normal * 300.0,
                Location - Normal * 300.0, ECC_WorldStatic, Query);
        }
        const FStructProperty* Top = FindFProperty<FStructProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mTopTransform"));
        FIntProperty* Step = FindFProperty<FIntProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mActivePointIdx"));
        if (!Top || !Step) return false;
        const FTransform Selected = *Top->ContainerPtrToValuePtr<FTransform>(Root);
        for (FHitResult Hit : Hits)
        {
            SFNativeHit::ResolveForPlacement(Hit);
            if (!IsValid(Hit.GetActor()) || !Cell->IsValidHitResult(Hit)) continue;
            Step->SetPropertyValue_InContainer(Cell, 1);
            Cell->PreHologramPlacement(Hit, false);
            // The build gun tries native attachment snapping before the ordinary
            // location update. A successful snap owns the pose and endpoint state;
            // running the location update instead does not reproduce that contract.
            if (!Cell->TrySnapToActor(Hit)) Cell->UpdateHologramPlacement(Hit);
            Cell->PostHologramPlacement(Hit, false);
            if ((!bConnection || MatchesRequestedBinding(Cell, Root, 1, Pose))
                && (!bPassthrough || SFConveyorLiftHologramState::MatchesSelectedPassthroughs(Root, Cell, Pose))
                && MatchesSelectedPose(Cell->GetActorTransform(), Pose)
                && MatchesSelectedPose(*Top->ContainerPtrToValuePtr<FTransform>(Cell), Selected)) return true;
        }
        return false;
    }

    AFGHologram* SpawnLiftGridCell(AFGHologram* Root, const FTransform& Pose)
    {
        if (!SFConveyorLiftHologramState::IsHandled(Root) || !Root->GetRecipe() || !Root->GetWorld()) return nullptr;
        const FObjectPropertyBase* Parent = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
        if (!Parent) return nullptr;
        AFGHologram* Cell = AFGHologram::SpawnHologramFromRecipe(Root->GetRecipe(), Root->GetOwner(),
            Pose.GetLocation(), Root->GetConstructionInstigator(), [Root, Parent](AFGHologram* Spawned)
            { Parent->SetObjectPropertyValue_InContainer(Spawned, Root); });
        if (!IsValid(Cell) || Cell->GetClass() != Root->GetClass())
        { if (IsValid(Cell)) Cell->Destroy(); return nullptr; }
        Cell->SetConstructionInstigator(Root->GetConstructionInstigator());
        // Native floor/hover and commingling rules depend on this before placement.
        if (Root->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Root->GetBlueprintDesigner());
        Cell->Tags.AddUnique(TEXT("SF_LiftGridConstructionCell"));
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
    bool ResolveLiftGridCell(AFGHologram* Cell, AFGHologram* Root, const FTransform& Pose)
    {
        if (!IsValid(Cell) || !IsValid(Root) || Root->GetUpgradedActor()) return false;
        ON_SCOPE_EXIT { if (IsValid(Cell)) Cell->SetActorHiddenInGame(true); };
        const FObjectPropertyBase* Parent = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
        FIntProperty* Step = FindFProperty<FIntProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mActivePointIdx"));
        if (!Parent || !Step) return false;
        const FVector Nudge = Root->GetHologramNudgeOffset();
        FTransform BasePose = Pose;
        BasePose.AddToTranslation(-Nudge);
        if (!FSFConveyorLiftNativePlacement::SetInternalLock(Cell, false)) return false;
        Cell->SetNudgeOffset(FVector::ZeroVector);
        // A reusable validator must not retain a peer from an earlier grid cell.
        for (const TCHAR* Name : {TEXT("mSnappedConnectionComponents"), TEXT("mUpgradedConveyorLift")})
            if (const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), Name))
                for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
                    Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Cell, Index), nullptr);
        if (const FArrayProperty* Holes = FindFProperty<FArrayProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedPassthroughs")))
            if (const FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(Holes->Inner))
            {
                FScriptArrayHelper Array(Holes, Holes->ContainerPtrToValuePtr<void>(Cell));
                for (int32 Index = 0; Index < Array.Num(); ++Index) Inner->SetObjectPropertyValue(Array.GetRawPtr(Index), nullptr);
            }
        // Resolve the initial floor/attachment as an ordinary native lift. Each cell
        // acquires its own bindings; selected root connection pointers are never copied.
        TArray<FHitResult> Hits;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(SmartLiftGridCell), false, Cell);
        Query.AddIgnoredActor(Root);
        if (Root->GetConstructionInstigator()) Query.AddIgnoredActor(Root->GetConstructionInstigator());
        const bool bAttachedStart = GetBinding(Root, 0) != nullptr;
        if (GetPassthrough(Root, 0)) AppendPassthroughHits(Hits, Root, 0, BasePose, Query);
        else if (bAttachedStart)
        {
            const FVector Location = RequestedBindingLocation(Root, 0, BasePose);
            const FVector Normal = BasePose.TransformVectorNoScale(Root->GetActorTransform().InverseTransformVectorNoScale(GetBinding(Root, 0)->GetForwardVector()));
            Root->GetWorld()->LineTraceMultiByChannel(Hits, Location + Normal * 300.0, Location - Normal * 300.0, ECC_WorldStatic, Query);
        }
        else
        {
            // A vertical grid cell may be a native-legal nudge above the original
            // mounting level. Include that level in the floor search; the native
            // nudge operations below still enforce step size, range and legality.
            const double OriginZ = Root->GetActorLocation().Z - Nudge.Z;
            const double Drop = 500.0 + FMath::Max(0.0, BasePose.GetLocation().Z - OriginZ);
            Root->GetWorld()->LineTraceMultiByChannel(Hits, BasePose.GetLocation() + FVector(0,0,150),
                BasePose.GetLocation() - FVector(0,0,Drop), ECC_Visibility, Query);
        }
        for (FHitResult Hit : Hits)
        {
            SFNativeHit::ResolveForPlacement(Hit);
            if (!IsValid(Hit.GetActor()) || !Cell->IsValidHitResult(Hit)) continue;
            Parent->SetObjectPropertyValue_InContainer(Cell, nullptr);
            ON_SCOPE_EXIT { Parent->SetObjectPropertyValue_InContainer(Cell, Root); };
            Step->SetPropertyValue_InContainer(Cell, 0);
            // Second-phase scroll rotates the top; mFirstStepYaw is not the body's
            // selected world yaw. Resolve the first end with its requested body
            // rotation, then restore the independently selected endpoint state.
            Cell->SetScrollRotateValue(FMath::RoundToInt(Pose.Rotator().Yaw));
            Cell->SetActorTransform(BasePose);
            Cell->ResetConstructDisqualifiers();
            Cell->PreHologramPlacement(Hit, false);
            // The build gun tries native attachment snapping before the ordinary
            // location update. A successful snap owns the pose and endpoint state;
            // running the location update instead does not reproduce that contract.
            if (!Cell->TrySnapToActor(Hit)) Cell->UpdateHologramPlacement(Hit);
            Cell->PostHologramPlacement(Hit, false);
            if (Cell->GetUpgradedActor()
                || (bAttachedStart && !MatchesRequestedBinding(Cell, Root, 0, BasePose))) continue;
            FVector SelectedNudge = Nudge;
            FTransform ResolvedPose = Cell->GetActorTransform();
            if (!MatchesNativeFloorSnap(Root, Cell, Hit, BasePose))
            {
                // Unlocking native nudge retains the selected position but clears its
                // offset. Reconstruct that legal floor displacement with native nudge,
                // rather than assigning a pose or rejecting a player's valid placement.
                // Native step size, CanNudge and range checks still authorize every move.
                if (!SFConveyorLiftHologramState::CanReplayFloorPlacement(Root, Hit) || bAttachedStart
                    || GetPassthrough(Root, 0)
                    || !ResolvedPose.GetRotation().Equals(BasePose.GetRotation(), 0.001)
                    || !ResolvedPose.GetScale3D().Equals(BasePose.GetScale3D(), 0.001)) continue;
                SelectedNudge += BasePose.GetLocation() - ResolvedPose.GetLocation();
            }
            ResolvedPose.AddToTranslation(SelectedNudge);
            SFConveyorLiftHologramState::Sync(Root, Cell);
            if (!ReplaySelectedNudge(Cell, SelectedNudge) || !MatchesSelectedPose(Cell->GetActorTransform(), ResolvedPose)) continue;
            const FTransform NativePose = Cell->GetActorTransform();
            const bool bAttachedEnd = GetBinding(Root, 1) || GetPassthrough(Root, 1);
            // Native end selection follows an unlocked, already-positioned first end.
            // Clearing internal lock state preserves its resolved pose without UI markers.
            if (bAttachedEnd && !FSFConveyorLiftNativePlacement::SetInternalLock(Cell, false)) continue;
            if (!ResolveSelectedEnd(Cell, Root, bAttachedEnd ? Pose : NativePose)) continue;
            // A copied selected state does not make an unattached cell equivalent to
            // a snapped parent. Require independently resolved native endpoint bindings.
            bool bBindingsMatch = true;
            for (int32 Index = 0; Index < 2; ++Index)
                bBindingsMatch &= !GetBinding(Root, Index) || IsValid(GetBinding(Cell, Index));
            bBindingsMatch &= SFConveyorLiftHologramState::MatchesSelectedPassthroughs(Root, Cell, Cell->GetActorTransform());
            if (!bBindingsMatch) continue;
            if (Root->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Root->GetBlueprintDesigner());
            AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Root->GetConstructionInstigator());
            Cell->SetActorHiddenInGame(false);
            Cell->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
            return Cell->CanConstruct();
        }
        return false;
    }
}

bool SFConveyorLiftHologramState::CanReplayFloorPlacement(AFGHologram* Root, const FHitResult& Hit)
{
    return IsValid(Root) && CanReplayFloorOwner(Hit.GetActor(), Root->GetBlueprintDesigner(), Hit.ImpactNormal);
}

bool SFConveyorLiftHologramState::CanReplayFloorOwner(AActor* Floor, AFGBuildableBlueprintDesigner* Designer, const FVector& Normal)
{
    if (!IsValid(Floor)) return false;
    if (Cast<AFGBuildableFoundation>(Floor)) return true;
    // A Designer's internal floor is owned by the Designer itself. Its native
    // lift placement and nudge are legal too; another Designer or a wall is not
    // an equivalent floor. Placement, nudge range and final validation still run.
    return IsValid(Designer) && Floor == Designer && Normal.Equals(FVector::UpVector, 0.01);
}

bool SFConveyorLiftHologramState::MatchesSelectedPassthroughs(AFGHologram* Root, AFGHologram* Cell, const FTransform& Pose)
{
    if (!IsValid(Root) || !IsValid(Cell)) return false;
    const FArrayProperty* Property = FindFProperty<FArrayProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedPassthroughs"));
    const FObjectPropertyBase* Inner = Property ? CastField<FObjectPropertyBase>(Property->Inner) : nullptr;
    if (!Inner) return false;
    FScriptArrayHelper Source(Property, Property->ContainerPtrToValuePtr<void>(Root));
    FScriptArrayHelper Target(Property, Property->ContainerPtrToValuePtr<void>(Cell));
    if (Source.Num() != Target.Num()) return false;
    for (int32 Index = 0; Index < Source.Num(); ++Index)
    {
        UObject* Selected = Inner->GetObjectPropertyValue(Source.GetRawPtr(Index));
        if (!Selected) continue;
        auto* SourceHole = Cast<AFGBuildablePassthrough>(Selected);
        auto* TargetHole = Cast<AFGBuildablePassthrough>(Inner->GetObjectPropertyValue(Target.GetRawPtr(Index)));
        if (!IsValid(SourceHole) || !IsValid(TargetHole) || SourceHole->GetClass() != TargetHole->GetClass()) return false;
        const FVector Expected = Pose.TransformPosition(Root->GetActorTransform().InverseTransformPosition(SourceHole->GetActorLocation()));
        if (!TargetHole->GetActorLocation().Equals(Expected, 2.0)) return false;
    }
    return true;
}

bool SFConveyorLiftHologramState::IsHandled(const AFGHologram* Root)
{
    return IsValid(Root) && Root->IsA<AFGConveyorLiftHologram>() && Root->GetBuildClass()
        && !Root->ActorHasTag(TEXT("SF_LiftGridConstructionCell"));
}
void SFConveyorLiftHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
    if (!IsHandled(Parent) || !IsValid(Child)) return;
    if (auto* Preview = Cast<ASFConveyorLiftPreviewHologram>(Child)) { Preview->SyncGeometry(Parent); return; }
    if (!Child->IsA<AFGConveyorLiftHologram>()) return;
    const FTransform Pose = Child->GetActorTransform();
    // Native build modes can reset the placement phase: set mode first, selected state last.
    TArray<TSubclassOf<UFGBuildGunModeDescriptor>> Modes;
    Parent->GetSupportedBuildModes(Modes);
    for (const auto& Mode : Modes)
        if (Mode && Mode->IsChildOf(UFGHologramBuildModeDescriptor::StaticClass()))
        {
            const TSubclassOf<UFGHologramBuildModeDescriptor> Selected(Mode.Get());
            if (Parent->IsCurrentBuildMode(Selected))
            {
                const FClassProperty* Override = FindFProperty<FClassProperty>(AFGHologram::StaticClass(), TEXT("mBuildModeOverride"));
                if (!Override || Override->GetObjectPropertyValue_InContainer(Child) != Selected.Get()) Child->SetBuildModeOverride(Selected);
                break;
            }
        }
    bool bTopChanged = false, bArrowChanged = false;
    for (const TCHAR* Name : {TEXT("mTopTransform"), TEXT("mActivePointIdx"), TEXT("mIsReversed"), TEXT("mArrowDirection")})
        if (FProperty* Property = FindFProperty<FProperty>(AFGConveyorLiftHologram::StaticClass(), Name))
        {
            const bool bChanged = !Property->Identical_InContainer(Parent, Child);
            if (FCString::Strcmp(Name, TEXT("mTopTransform")) == 0) bTopChanged = bChanged;
            if (FCString::Strcmp(Name, TEXT("mArrowDirection")) == 0 || FCString::Strcmp(Name, TEXT("mIsReversed")) == 0) bArrowChanged |= bChanged;
            Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Child), Property->ContainerPtrToValuePtr<void>(Parent));
        }
    if (FFloatProperty* Yaw = FindFProperty<FFloatProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mFirstStepYaw")))
        Yaw->SetPropertyValue_InContainer(Child, Yaw->GetPropertyValue_InContainer(Parent)
            + FMath::FindDeltaAngleDegrees(Parent->GetActorRotation().Yaw, Pose.Rotator().Yaw));
    if (bTopChanged || bArrowChanged) if (UFunction* Refresh = Child->FindFunction(TEXT("OnRep_TopTransform"))) Child->ProcessEvent(Refresh, nullptr);
    if (bArrowChanged) if (UFunction* Refresh = Child->FindFunction(TEXT("OnRep_ArrowDirection"))) Child->ProcessEvent(Refresh, nullptr);
    Child->SetActorTransform(Pose);
    TInlineComponentArray<UFGFactoryConnectionComponent*> SourceConnections, TargetConnections;
    Parent->GetComponents(SourceConnections); Child->GetComponents(TargetConnections);
    for (auto* Source : SourceConnections) for (auto* Target : TargetConnections)
        if (Source->GetFName() == Target->GetFName()) Target->SetDirection(Source->GetDirection());
}
bool SFConveyorLiftHologramState::TryGetDimensions(AFGHologram* Root, FVector& OutDimensions)
{
    const auto* Lift = Cast<AFGConveyorLiftHologram>(Root);
    if (!Lift || !Root->GetBuildClass()) return false;
    OutDimensions = FVector(200, 200, FMath::Max(100.0f, Lift->GetHeight() + 300.0f));
    return true;
}
bool SFConveyorLiftHologramState::HasProxyGrid(AFGHologram* Root)
{
    if (!IsHandled(Root) || Root->GetParentHologram()) return false;
    for (AFGHologram* Child : Root->GetHologramChildren())
        if (IsValid(Child) && !Child->IsDisabled() && Child->IsA<ASFConveyorLiftPreviewHologram>()) return true;
    return false;
}
bool SFConveyorLiftHologramState::ValidatePreview(ASFConveyorLiftPreviewHologram* Preview)
{
    AFGHologram* Root = Preview->GetParentHologram();
    if (!IsHandled(Root)) return false;
    auto& Entry = LiftGridScratchCells.FindOrAdd(Root);
    if (!Entry.IsValid()) Entry = SpawnLiftGridCell(Root, Preview->GetRequestedPose());
    if (!Entry.IsValid() || !ResolveLiftGridCell(Entry.Get(), Root, Preview->GetRequestedPose()))
    {
        return false;
    }
    Preview->SyncGeometry(Entry.Get());
    Preview->ApplyResolvedPose(Entry->GetActorTransform());
    return true;
}
void SFConveyorLiftHologramState::ReleasePreview(ASFConveyorLiftPreviewHologram* Preview)
{
    AFGHologram* Root = Preview->GetParentHologram();
    if (IsValid(Root)) for (AFGHologram* Child : Root->GetHologramChildren())
        if (Child != Preview && IsValid(Child) && !Child->IsDisabled() && Child->IsA<ASFConveyorLiftPreviewHologram>()) return;
    TWeakObjectPtr<AFGHologram> Cell;
    if (LiftGridScratchCells.RemoveAndCopyValue(Root, Cell) && Cell.IsValid())
    { FSFConveyorLiftNativePlacement::SetInternalLock(Cell.Get(), false); Cell->Destroy(); }
    for (auto It = LiftGridScratchCells.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
}
bool SFConveyorLiftHologramState::Prepare(AFGHologram* Root, const FSFScalingSpec* Spec)
{
    if (!bLiftGridRequestActive || !IsHandled(Root) || Root->GetParentHologram() || !Root->HasAuthority()) return false;
    if (const bool* Existing = LiftGridPreparedRoots.Find(Root)) return *Existing;
    // [MP-SEAM] Native Designer references are not reliable in construct messages.
    // Restore authoritative containment before native placement/payment validation,
    // rather than waiting for the later Construct seam to stamp the built children.
    if (Spec && !Root->GetBlueprintDesigner() && Root->GetWorld()->GetNetMode() != NM_Client)
        for (TActorIterator<AFGBuildableBlueprintDesigner> It(Root->GetWorld()); It; ++It)
            if (IsValid(*It) && It->IsLocationInsideDesigner(Root->GetActorLocation()))
            { Root->SetInsideBlueprintDesigner(*It); break; }
    LiftGridPreparedRoots.Add(Root, false);
    TArray<ASFConveyorLiftPreviewHologram*> Previews;
    TArray<FTransform> Poses;
    for (AFGHologram* Child : Root->GetHologramChildren())
        if (auto* Preview = Cast<ASFConveyorLiftPreviewHologram>(Child); IsValid(Preview) && !Preview->IsDisabled())
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
    TArray<FTransform> AcceptedPoses;
    AcceptedPoses.Add(Root->GetActorTransform());
    TSet<UFGFactoryConnectionComponent*> Reserved;
    TMap<TWeakObjectPtr<AFGBuildablePassthrough>, uint8> ReservedHoleFaces;
    if (!ReservePassthroughFaces(Root, ReservedHoleFaces)) return false;
    for (int32 Index = 0; Index < 2; ++Index) if (auto* Binding = GetBinding(Root, Index)) Reserved.Add(Binding);
    for (const FTransform& Pose : Poses)
    {
        AFGHologram* Cell = SpawnLiftGridCell(Root, Pose);
        if (!Cell) return false;
        LiftGridRequestCells.Add(Cell);
        if (!ResolveLiftGridCell(Cell, Root, Pose)) return false;
        for (const FTransform& ExistingPose : AcceptedPoses)
            if (MatchesSelectedPose(Cell->GetActorTransform(), ExistingPose)) return false;
        AcceptedPoses.Add(Cell->GetActorTransform());
        if (!ReservePassthroughFaces(Cell, ReservedHoleFaces)) return false;
        for (int32 Index = 0; Index < 2; ++Index)
            if (auto* Binding = GetBinding(Cell, Index))
            {
                if (Reserved.Contains(Binding)) return false;
                Reserved.Add(Binding);
            }
            Accepted.Add(Cell);
    }
    for (int32 Index = 0; Index < Accepted.Num(); ++Index)
        if (Previews.IsEmpty())
        {
            Root->AddChild(Accepted[Index], FName(*FString::Printf(TEXT("LiftGridCell_%d"), Index)));
            Accepted[Index]->Tags.AddUnique(TEXT("SF_GridChild"));
            Accepted[Index]->SetActorHiddenInGame(false);
        }
        else LiftGridPreparedCells.Add(Previews[Index], Accepted[Index]);
    LiftGridPreparedRoots[Root] = true;
    return true;
}
AFGHologram* SFConveyorLiftHologramState::FindPreparedCell(AFGHologram* Preview)
{
    const auto* Cell = LiftGridPreparedCells.Find(Preview);
    return bLiftGridRequestActive && Cell ? Cell->Get() : nullptr;
}
bool SFConveyorLiftHologramState::IsRequestActive() { return bLiftGridRequestActive; }
SFConveyorLiftHologramState::FRequestScope::FRequestScope()
{
    bOwnsRequest = !bLiftGridRequestActive;
    bLiftGridRequestActive = true;
}
SFConveyorLiftHologramState::FRequestScope::~FRequestScope()
{
    if (!bOwnsRequest) return;
    for (const auto& Cell : LiftGridRequestCells) if (Cell.IsValid())
    { FSFConveyorLiftNativePlacement::SetInternalLock(Cell.Get(), false); Cell->Destroy(); }
    LiftGridRequestCells.Reset();
    LiftGridPreparedRoots.Reset();
    LiftGridPreparedCells.Reset();
    bLiftGridRequestActive = false;
}
