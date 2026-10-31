// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UObject/UnrealType.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/World.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "Equipment/FGEquipment.h"
#include "FGCharacterPlayer.h"
#include "FGConstructDisqualifier.h"
#include "FGPlayerController.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Buildables/FGBuildableMergerPriority.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelineJunction.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "FGFactoryConnectionComponent.h"
#include "FGBuildableSubsystem.h"
#include "FGInventoryComponent.h"
#include "FGCentralStorageSubsystem.h"
#include "FGBlueprintProxy.h"
#include "Buildables/FGBuildableBlueprintDesigner.h"
#include "FGCircuitConnectionComponent.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Core/Construction/SFConstructionOperationRecord.h"
#include "Core/Construction/SFFactorySettingsApplyPolicy.h"
#include "Core/Construction/SFConveyorCohort.h"
#include "Core/Construction/SFConveyorConnectionMutation.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendCommitSpec.h"
#include "Features/Extend/SFExtendWiringInvocation.h"
#include "Features/Extend/SFExtendLiftAttachmentReceipt.h"
#include "Features/Extend/SFExtendPipeCompatibilityReceipt.h"
#include "Features/Extend/SFExtendPowerConnections.h"
#include "Features/Extend/Net/SFExtendNativeInputTicket.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFRecipeManagementService.h"
#include "Data/SFHologramDataRegistry.h"

namespace
{
    const FName NativeInputEffect(TEXT("NativeBuildInput"));
    const FName NativeWiringEffect(TEXT("PostBuildWiring"));
    const FName FactorySettingsEffect(TEXT("FactorySettings"));

    struct FHologramFrame
    {
        TWeakObjectPtr<AFGHologram> Actor;
        TWeakObjectPtr<AFGBuildableBlueprintDesigner> Designer;
        TMap<FName, uint8> Directions;
        FTransform Transform;
        FObjectKey Class;
        FObjectKey BuildClass;
        FObjectKey Recipe;
        bool bDisabled = false;
        TArray<FSplinePointData> Spline;
        TOptional<FTransform> LiftTop;
        TOptional<bool> LiftReversed;
    };

    bool CaptureFrames(AFGHologram* Root, TArray<FHologramFrame>& Out)
    {
        TArray<AFGHologram*> Pending;
        Pending.Add(Root);
        TSet<AFGHologram*> Seen;
        while (!Pending.IsEmpty())
        {
            AFGHologram* Holo = Pending.Pop(EAllowShrinking::No);
            if (!IsValid(Holo) || Holo->IsActorBeingDestroyed() || Seen.Contains(Holo)) return false;
            Seen.Add(Holo);
            FHologramFrame Frame;
            Frame.Actor = Holo; Frame.Transform = Holo->GetActorTransform();
            Frame.Designer = Holo->GetBlueprintDesigner();
            TArray<UFGFactoryConnectionComponent*> Ports; Holo->GetComponents(Ports);
            for (auto* Port : Ports)
            {
                if (!IsValid(Port) || Frame.Directions.Contains(Port->GetFName())) return false;
                Frame.Directions.Add(Port->GetFName(), static_cast<uint8>(Port->GetDirection()));
            }
            if (Frame.Transform.ContainsNaN()) return false;
            Frame.Class = FObjectKey(Holo->GetClass()); Frame.BuildClass = FObjectKey(Holo->GetBuildClass().Get());
            Frame.Recipe = FObjectKey(Holo->GetRecipe().Get()); Frame.bDisabled = Holo->IsDisabled();
            if (const AFGSplineHologram* Spline = Cast<AFGSplineHologram>(Holo))
            {
                const FArrayProperty* Property = FindFProperty<FArrayProperty>(Spline->GetClass(), TEXT("mSplineData"));
                const FStructProperty* Element = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
                if (!Element || Element->Struct != FSplinePointData::StaticStruct() || Element->GetElementSize() != sizeof(FSplinePointData)) return false;
                const auto* Value = Property->ContainerPtrToValuePtr<TArray<FSplinePointData>>(Spline);
                if (!Value || Value->Num() < 0 || Value->Max() < Value->Num() || (Value->Num() > 0 && !Value->GetData())) return false;
                Frame.Spline = *Value;
            }
            if (const AFGConveyorLiftHologram* Lift = Cast<AFGConveyorLiftHologram>(Holo))
            {
                const FStructProperty* Top = FindFProperty<FStructProperty>(Lift->GetClass(), TEXT("mTopTransform"));
                if (!Top || Top->Struct != TBaseStructure<FTransform>::Get()) return false;
                Frame.LiftTop = *Top->ContainerPtrToValuePtr<FTransform>(Lift);
                const FBoolProperty* Reversed = FindFProperty<FBoolProperty>(Lift->GetClass(), TEXT("mIsReversed"));
                if (!Reversed) return false;
                Frame.LiftReversed = Reversed->GetPropertyValue_InContainer(Lift);
            }
            Out.Add(MoveTemp(Frame));
            Pending.Append(Holo->GetHologramChildren());
        }
        return true;
    }

    bool SameFrames(const TArray<FHologramFrame>& Before, const TArray<FHologramFrame>& Current)
    {
        if (Before.Num() != Current.Num()) return false;
        for (int32 Index = 0; Index < Before.Num(); ++Index)
        {
            const auto& A = Before[Index]; const auto& B = Current[Index];
            if (A.Actor != B.Actor || A.Class != B.Class || A.BuildClass != B.BuildClass || A.Recipe != B.Recipe
                || A.Designer != B.Designer || A.Directions.Num() != B.Directions.Num()
                || !A.Transform.Equals(B.Transform, 0.001) || A.bDisabled != B.bDisabled || A.Spline.Num() != B.Spline.Num()
                || A.LiftTop.IsSet() != B.LiftTop.IsSet() || A.LiftReversed != B.LiftReversed) return false;
            for (const auto& Port : A.Directions)
                if (!B.Directions.Contains(Port.Key) || B.Directions.FindRef(Port.Key) != Port.Value) return false;
            if (A.LiftTop && !A.LiftTop->Equals(*B.LiftTop, 0.001)) return false;
            for (int32 Point = 0; Point < A.Spline.Num(); ++Point)
                if (!FSplinePointData::StaticStruct()->CompareScriptStruct(&A.Spline[Point], &B.Spline[Point], PPF_None)) return false;
        }
        return true;
    }

    bool SameIntent(const FSFExtendCommitSpec& Before, const FSFExtendCommitSpec& Current)
    {
        if (!FSFExtendCommitSpec::StaticStruct()->CompareScriptStruct(&Before, &Current, PPF_None)) return false;
        // This local metadata intentionally is not a reflected RPC property.
        const auto& A = Before.RestoreTemplate.ChildHolograms;
        const auto& B = Current.RestoreTemplate.ChildHolograms;
        if (A.Num() != B.Num()) return false;
        for (int32 Index = 0; Index < A.Num(); ++Index)
        {
            if (A[Index].PowerPortCapacities.Num() != B[Index].PowerPortCapacities.Num()) return false;
            for (const auto& Port : A[Index].PowerPortCapacities)
            {
                const int32* Capacity = B[Index].PowerPortCapacities.Find(Port.Key);
                if (!Capacity || *Capacity != Port.Value) return false;
            }
        }
        return true;
    }

    bool SameTopology(const FSFCloneTopology& A, const FSFCloneTopology& B)
    {
        if (!FSFCloneTopology::StaticStruct()->CompareScriptStruct(&A, &B, PPF_None)
            || A.PowerPlanError != B.PowerPlanError || A.ChildHolograms.Num() != B.ChildHolograms.Num()) return false;
        for (int32 Index = 0; Index < A.ChildHolograms.Num(); ++Index)
        {
            const auto& Ports = A.ChildHolograms[Index].PowerPortCapacities;
            const auto& Other = B.ChildHolograms[Index].PowerPortCapacities;
            if (Ports.Num() != Other.Num()) return false;
            for (const auto& Port : Ports)
            {
                const int32* Value = Other.Find(Port.Key);
                if (!Value || *Value != Port.Value) return false;
            }
        }
        return true;
    }

    bool HasTypedSettingsMaterials(UFGInventoryComponent* Inventory, const FSFFactorySettingsSnapshot& Settings)
    {
        if (!Inventory) return true;
        TArray<FInventoryStack> Stacks;
        Inventory->GetInventoryStacks(Stacks);
        for (const auto& Stack : Stacks)
            if ((Stack.Item.GetItemClass().Get() == Settings.OverclockShardClass.Get()
                || Stack.Item.GetItemClass().Get() == Settings.ProductionBoostShardClass.Get())
                && (Stack.Item.HasState() || Stack.Item.LegacyItemStateActor != nullptr)) return true;
        return false;
    }
}

struct FSFExtendNativeInputOperation
{
    FSFExtendNativeInputOperation(FGuid InWorld, UFGBuildGunStateBuild* InReceiver, AFGHologram* InRoot)
        : World(InWorld), RootIdentity(InRoot), AdmissionIdentity(InRoot), Receiver(InReceiver), Root(InRoot), Ticket(InWorld, FObjectKey(InReceiver), FObjectKey(InRoot)) {}
    FGuid Id = FGuid::NewGuid();
    FGuid World;
    FObjectKey RootIdentity;
    FString NativeRefusal;
    FObjectKey AdmissionIdentity;
    bool bRemote = false;
    bool bRemoteEntryPermitted = false;
    bool bRemoteEntryConsumed = false;
    bool bRemoteRootPrepared = false;
    bool bRemoteEnvelopeEntered = false;
    bool bNativeConstructionInvoked = false;
    TFunction<void()> RejectBeforeDispatch;
    FSFConstructionOperationKey Key;
    TWeakObjectPtr<UFGBuildGunStateBuild> Receiver;
    TWeakObjectPtr<AFGHologram> Root;
    TWeakObjectPtr<AFGCharacterPlayer> Pawn;
    TWeakObjectPtr<AFGBuildGun> Gun;
    TWeakObjectPtr<AFGBuildableBlueprintDesigner> Designer;
    FObjectKey ActiveRecipe;
    FSFExtendNativeInputTicket Ticket;
    FSFExtendCommitSpec Intent;
    TSharedPtr<const FSFCloneTopology> JsonIntent;
    TArray<TSharedPtr<const FSFCloneTopology>> ScaledJsonIntents;
    TMap<FString, FObjectKey> SourceBindings;
    TArray<FHologramFrame> Frames;
    TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> SourceSeeds;
    TSharedPtr<FSFExtendWiringInvocation> Wiring;
    FSFConveyorCohort Before;
    FSFConveyorCohort::FCargoSnapshot BeforeCargo;
    struct FExpectedEdge
    {
        TWeakObjectPtr<UFGFactoryConnectionComponent> Port;
        TWeakObjectPtr<UFGFactoryConnectionComponent> Peer;
    };
    TArray<FExpectedEdge> ExpectedEdges;
    TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> Remaining;
    bool bConveyorWiringVerified = false;
    bool bDistributorSettingsVerified = false;
    bool bDistributorSettingsAttempted = false;
    TMap<FObjectKey, FSFCapturedDistributorSettings> DistributorSettingsReceipts;
    bool bCargoVerified = false;
    bool bTopologyObserved = false;
    int32 SettlementPhases = 0;
    bool bReconciliationReported = false;
    TMap<TSubclassOf<UFGItemDescriptor>, int64> QuotedCost;
    TMap<FString, TMap<TSubclassOf<UFGItemDescriptor>, int64>> PowerQuotes;
    TMap<TSubclassOf<UFGItemDescriptor>, int64> MaterialsBefore;
    TWeakObjectPtr<UFGInventoryComponent> PayerInventory;
    TWeakObjectPtr<AFGCentralStorageSubsystem> PayerStorage;
    bool bNoBuildCost = false;
    bool bPaymentPrepared = false;
    bool bPaymentObserved = false;
    bool bConstructionMaterialsVerified = false;
    struct FSettingsReceipt
    {
        TWeakObjectPtr<AFGBuildableFactory> Factory;
        TSubclassOf<UFGRecipe> Recipe;
        float PendingPotential = 1.0f;
        float PendingBoost = 1.0f;
        int32 OverclockItems = 0;
        int32 BoostItems = 0;
    };
    TSet<FObjectKey> SuppressedSettingsTargets;
    TArray<FSettingsReceipt> SettingsReceipts;
    TMap<TSubclassOf<UFGItemDescriptor>, int64> SettingsMaterialsBefore;
    int32 SettingsReadinessPhases = 0;
    bool bSettingsRequestConflict = false;
    bool bSettingsAttempted = false;
    bool bSettingsDone = false;
    bool bSettingsConserved = false;
    struct FPowerWireReceipt
    {
        TWeakObjectPtr<AFGBuildableWire> Wire;
        FObjectKey AttemptedIdentity;
        TWeakObjectPtr<UFGCircuitConnectionComponent> From;
        TWeakObjectPtr<UFGCircuitConnectionComponent> To;
        bool bConnected = false;
    };
    struct FPowerPortReceipt
    {
        TWeakObjectPtr<UFGCircuitConnectionComponent> Port;
        TSet<FObjectKey> OriginalWires;
    };
    TMap<FString, FPowerWireReceipt> PowerWires;
    TMap<FObjectKey, FPowerPortReceipt> PowerPorts;
    TSet<FObjectKey> AuxiliaryOutputs;
    bool bPowerFailed = false;
    bool bAdoptionFailed = false;
};

bool USFExtendService::ValidateCapturedConstructionPlan(AFGHologram* Root) const
{
    if (!StoredCloneTopology) return true;
    TArray<FHologramFrame> Frames;
    if (!IsValid(Root) || !CaptureFrames(Root, Frames)) return false;
    // The same stock-class boundary used by completion must hold before effects,
    // not first be discovered after native construction/payment has happened.
    for (const auto& Frame : Frames)
    {
        if (Frame.bDisabled) continue;
        if (!Frame.Actor.IsValid() || Frame.Actor->GetBlueprintDesigner() != Root->GetBlueprintDesigner()) return false;
        auto* BuildClass = Cast<UClass>(Frame.BuildClass.ResolveObjectPtr());
        if (!BuildClass) continue; // nonmaterializing native helper/preview node
        const FString Package = BuildClass->GetOutermost()->GetName();
        if (!Package.StartsWith(TEXT("/Game/FactoryGame/")) && Package != TEXT("/Script/FactoryGame")) return false;
    }
    auto ValidateSettings = [&](const FSFCloneTopology& Plan)
    {
        if (!FSFExtendWiringPlan::HasExactLiftHoleRoles(Plan)) return false;
        for (const auto& Child : Plan.ChildHolograms)
        {
            if (Child.bPreviewOnly || !Child.bConstructible) continue;
            const bool bLiftHole = Child.Role == TEXT("passthrough") && (Child.BuildClass == TEXT("Build_FoundationPassthrough_Lift_C")
                || Child.BuildClass == TEXT("Build_FoundationPassthrough_Pipe_C"));
            if (Child.Role != TEXT("distributor") && !bLiftHole) continue;
            const FHologramFrame* Frame = Frames.FindByPredicate([&](const FHologramFrame& Value)
            {
                const auto* Data = USFHologramDataRegistry::GetData(Value.Actor.Get());
                return Data && Data->JsonCloneId == Child.HologramId;
            });
            UClass* Class = Frame ? Cast<UClass>(Frame->BuildClass.ResolveObjectPtr()) : nullptr;
            if (!Class) return false;
            if (bLiftHole)
            {
                FString Reason;
                if (!FSFExtendLiftAttachmentReceipt::ValidatePlacement(Root, Frame->Actor.Get(), Child, Reason))
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("Extend/Restore floor-hole plan rejected before construction: %s."), *Reason);
                    return false;
                }
                continue;
            }
            const bool bConfigurable = Class->IsChildOf(AFGBuildableSplitterSmart::StaticClass())
                || Class->IsChildOf(AFGBuildableMergerPriority::StaticClass());
            const bool bMounted = Class->GetName().Contains(TEXT("Lift"));
            if ((bConfigurable || bMounted || Child.DistributorSettings.bCaptured)
                && !Child.DistributorSettings.IsValidFor(Class->GetDefaultObject<AFGBuildable>())) return false;
        }
        return true;
    };
    if (!ValidateSettings(*StoredCloneTopology)) return false;
    for (const auto& Clone : ScaledExtendClones)
        if (Clone.CloneTopology && !ValidateSettings(*Clone.CloneTopology)) return false;
    return true;
}

bool USFExtendService::QueueRemoteBuildInput(UFGBuildGunStateBuild* Receiver, const FSFExtendCommitSpec& Intent,
    const FString& MessageDigest, TFunction<void(UFGBuildGunStateBuild*)> Dispatch,
    TFunction<void()> RejectBeforeDispatch)
{
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    auto* Gun = IsValid(Receiver) ? Receiver->GetBuildGun() : nullptr;
    auto* Pawn = Gun ? Cast<AFGCharacterPlayer>(Gun->GetInstigator()) : nullptr;
    auto* PC = Pawn ? Cast<AFGPlayerController>(Pawn->GetController()) : nullptr;
    if (!Owner || !IsValid(Receiver) || !IsValid(Gun) || !IsValid(PC) || !PC->HasAuthority()
        || PC->IsLocalController() || Receiver->GetWorld() != Subsystem->GetWorld()
        || !Intent.bValid || !Intent.RequestId.IsValid() || Intent.RequestSequence <= 0 || MessageDigest.IsEmpty()
        || Intent.AuthorityWorldGeneration != Owner->GetWorldGeneration() || !Dispatch)
    { if (RejectBeforeDispatch) RejectBeforeDispatch(); return true; }
    auto Operation = MakeShared<FSFExtendNativeInputOperation>(Intent.AuthorityWorldGeneration, Receiver, nullptr);
    Operation->Id = Intent.RequestId; Operation->bRemote = true; Operation->AdmissionIdentity = FObjectKey(Gun);
    Operation->Pawn = Pawn; Operation->Gun = Gun; Operation->Intent = Intent;
    Operation->RejectBeforeDispatch = MoveTemp(RejectBeforeDispatch);
    Operation->Key = {FObjectKey(PC), uint64(Intent.RequestSequence), ESFConstructionRequestStream::RemoteExtend};
    FSFConstructionOperationRequest Request;
    Request.Requester = PC; Request.RequestId = Intent.RequestId; Request.Sequence = Intent.RequestSequence;
    Request.WorldGeneration = Intent.AuthorityWorldGeneration; Request.Stream = ESFConstructionRequestStream::RemoteExtend;
    FString ValueIntent;
    FSFExtendCommitSpec::StaticStruct()->ExportText(ValueIntent, &Intent, nullptr, nullptr, PPF_None, nullptr);
    Request.IntentDigest = MessageDigest + TEXT("/") + ValueIntent;
    for (AActor* Actor : { static_cast<AActor*>(Pawn), static_cast<AActor*>(Gun), static_cast<AActor*>(Intent.SourceBuilding.Get()) })
        if (IsValid(Actor)) { Request.ExpectedCohort.Add(FObjectKey(Actor)); Request.AffectedActors.AddUnique(Actor); }
    const TWeakObjectPtr<USFExtendService> WeakService(this);
    const TWeakObjectPtr<USFConstructionOperationCoordinator> WeakOwner(Owner);
    const auto Admission = Owner->Submit(Request,
        [WeakService, WeakOwner, Operation, Dispatch = MoveTemp(Dispatch)](FSFConstructionOperationRecord& Record)
        {
            auto* Service = WeakService.Get(); auto* Coordinator = WeakOwner.Get();
            auto Reject = [&]() { Record.RequestCancellation(Operation->World); Record.SettleCancellation(Operation->World); };
            if (!Service || !Coordinator || !Coordinator->IsInMutationPhase() || !Service->Subsystem.IsValid()
                || !Operation->Receiver.IsValid() || !Operation->Pawn.IsValid() || !Operation->Gun.IsValid()
                || FObjectKey(Operation->Pawn->GetController()) != Operation->Key.Requester
                || Operation->Pawn->GetEquipmentInSlot(EEquipmentSlot::ES_ARMS) != Operation->Gun.Get()
                || Operation->Gun->GetCurrentState() != Operation->Receiver.Get()
                || !Service->Subsystem->IsSmartEnabledForPlayer(Cast<APlayerController>(Operation->Pawn->GetController())))
            { Reject(); return; }
            TStrongObjectPtr<USFExtendService> ServiceKeepAlive(Service);
            TStrongObjectPtr<UFGBuildGunStateBuild> ReceiverKeepAlive(Operation->Receiver.Get());
            TStrongObjectPtr<AFGCharacterPlayer> PawnKeepAlive(Operation->Pawn.Get());
            TStrongObjectPtr<AFGBuildGun> GunKeepAlive(Operation->Gun.Get());
            // Reinstall this copied request immediately before its complete native
            // envelope, never whichever preview happened to stage most recently.
            auto* PC = Cast<APlayerController>(Operation->Pawn->GetController());
            Service->Subsystem->StageExtendCommitForPlayer(PC, Operation->Intent);
            if (!Record.ClaimEffect(Operation->World, NativeInputEffect)) { Reject(); return; }
            if (!Record.ClaimEffect(Operation->World, FactorySettingsEffect))
            { Record.ObserveEffect(Operation->World, NativeInputEffect, ESFConstructionEffectOutcome::NoEffect); Reject(); return; }
            TGuardValue<TSharedPtr<FSFExtendNativeInputOperation>> Active(Service->ExecutingNativeBuildInput, Operation);
            TGuardValue<FSFConstructionOperationRecord*> RecordScope(Service->NativeInputRecordInCurrentStack, &Record);
            Operation->bRemoteEntryPermitted = true;
            Dispatch(Operation->Receiver.Get());
            Service->FinishOwnedNativeInputDispatch(Operation, Record);
            // Native validation/Construct may already consume the staged entry.
            // Clear only this exact request, preserving a newer independently staged plan.
            FSFExtendCommitSpec Remaining;
            if (Service->Subsystem.IsValid() && Operation->Pawn.IsValid()
                && Service->Subsystem->PeekExtendCommitForInstigator(Operation->Pawn.Get(), Operation->Intent.BuildClass, Remaining)
                && Remaining.RequestId == Operation->Id)
                Service->Subsystem->ConsumeExtendCommitForInstigator(Operation->Pawn.Get(), Operation->Intent.BuildClass, Remaining);
        },
        [WeakService, Operation](FSFConstructionOperationRecord& Record)
        { if (auto* Service = WeakService.Get()) Service->ObserveOwnedNativeInput(Operation, Record); },
        [WeakService, Operation](ESFConstructionOperationState)
        {
            if (!Operation->bRemoteEnvelopeEntered && Operation->RejectBeforeDispatch) Operation->RejectBeforeDispatch();
            if (auto* Service = WeakService.Get())
                if (auto* Existing = Service->NativeBuildInputs.Find(Operation->AdmissionIdentity); Existing && *Existing == Operation)
                    Service->NativeBuildInputs.Remove(Operation->AdmissionIdentity);
        });
    if (Admission == ESFConstructionAdmission::Accepted) NativeBuildInputs.Add(Operation->AdmissionIdentity, Operation);
    else if (Admission != ESFConstructionAdmission::Existing)
    {
        if (Operation->RejectBeforeDispatch) Operation->RejectBeforeDispatch();
        UE_LOG(LogSmartFoundations, Log, TEXT("Remote Extend/Restore rejected before native dispatch: request=%s admission=%d."), *Intent.RequestId.ToString(), int32(Admission));
    }
    return true;
}

bool USFExtendService::EnterRemoteNativeEnvelope(UFGBuildGunStateBuild* Receiver)
{
    const auto Operation = ExecutingNativeBuildInput;
    if (!Operation || !Operation->bRemote) return true;
    if (!CanExecuteOwnedInputWiring(Operation->Id) || Operation->Receiver.Get() != Receiver
        || !Operation->bRemoteEntryPermitted || Operation->bRemoteEnvelopeEntered) return false;
    Operation->bRemoteEnvelopeEntered = true;
    return true;
}

bool USFExtendService::AttachNativeInputOwner(USFExtendService* Owner)
{
    if (!IsValid(Owner) || Owner == this || ExecutingNativeBuildInput || !Owner->ExecutingNativeBuildInput
        || !Owner->ExecutingNativeBuildInput->bRemote || !Owner->NativeInputRecordInCurrentStack
        || Subsystem != Owner->Subsystem || !Owner->CanExecuteOwnedInputWiring(Owner->ExecutingNativeBuildInput->Id)) return false;
    ExecutingNativeBuildInput = Owner->ExecutingNativeBuildInput;
    NativeInputRecordInCurrentStack = Owner->NativeInputRecordInCurrentStack;
    return true;
}

void USFExtendService::DetachNativeInputOwner()
{
    NativeInputRecordInCurrentStack = nullptr;
    ExecutingNativeBuildInput.Reset();
}

bool USFExtendService::PrepareRemoteNativeInput(AFGHologram* Root)
{
    auto Operation = ExecutingNativeBuildInput;
    if (!Operation || !Operation->bRemote)
    {
        auto* Pawn = IsValid(Root) ? Root->GetConstructionInstigator() : nullptr;
        auto* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
        // A remote Extend cannot enter legacy wiring without its paired owner.
        return Operation.IsValid() || (PC && PC->IsLocalController());
    }
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Owner || !Owner->IsInMutationPhase() || !NativeInputRecordInCurrentStack || Operation->bRemoteRootPrepared
        || !Operation->bRemoteEntryPermitted || !Operation->bRemoteEnvelopeEntered || !Operation->Pawn.IsValid()
        || !IsValid(Root) || Root->GetParentHologram() || Root->IsUpgrade()
        || Root->GetWorld() != Subsystem->GetWorld() || !Root->HasAuthority()
        || Root->GetConstructionInstigator() != Operation->Pawn.Get() || Root->GetBuildClass() != Operation->Intent.BuildClass
        || !ValidateCapturedConstructionPlan(Root)
        || HasTypedSettingsMaterials(Operation->Pawn->GetInventory(), Operation->Intent.FactorySettings)) return false;
    Operation->bRemoteEntryPermitted = false; Operation->bRemoteEntryConsumed = true;
    Operation->Root = Root; Operation->RootIdentity = FObjectKey(Root);
    Operation->Designer = Root->GetBlueprintDesigner();
    if (auto* Designer = Operation->Designer.Get(); Designer && !Owner->AdoptActor(Operation->Key, Designer)) return false;
    if (!Owner->AdoptActor(Operation->Key, Root) || !CaptureFrames(Root, Operation->Frames)) return false;
    if (!StoredCloneTopology) return false;
    if (StoredCloneTopology) Operation->JsonIntent = MakeShared<const FSFCloneTopology>(*StoredCloneTopology);
    for (const auto& Clone : ScaledExtendClones)
    {
        TSharedPtr<const FSFCloneTopology> Captured;
        if (Clone.CloneTopology) Captured = MakeShared<const FSFCloneTopology>(*Clone.CloneTopology);
        Operation->ScaledJsonIntents.Add(MoveTemp(Captured));
    }
    auto Bind = [&](AFGBuildable* Actor)
    {
        if (!IsValid(Actor) || !Owner->AdoptActor(Operation->Key, Actor)) return false;
        const FString Name = Actor->GetName();
        const FObjectKey Identity(Actor);
        if (const auto* Existing = Operation->SourceBindings.Find(Name); Existing && *Existing != Identity) return false;
        Operation->SourceBindings.Add(Name, Identity);
        return true;
    };
    if (!Operation->Intent.bIsRestore)
    {
        // ReconstructCommitOnServer just walked this authoritative graph. Cached
        // client replies and arbitrary actor-name lookup are not source authority.
        const auto& Topology = GetCurrentTopology();
        if (Topology.SourceBuilding.Get() != Operation->Intent.SourceBuilding.Get() || !Bind(Topology.SourceBuilding.Get())) return false;
        for (const auto* Chains : { &Topology.InputChains, &Topology.OutputChains })
            for (const auto& Chain : *Chains)
            {
                if (Chain.Distributor.IsValid() && !Bind(Chain.Distributor.Get())) return false;
                for (const auto& Conveyor : Chain.Conveyors)
                { if (!Bind(Conveyor.Get())) return false; Operation->SourceSeeds.AddUnique(Conveyor); }
                for (const auto& Actor : Chain.SupportPoles) if (!Bind(Actor.Get())) return false;
                for (const auto& Actor : Chain.Passthroughs) if (!Bind(Actor.Get())) return false;
            }
        for (const auto* Chains : { &Topology.PipeInputChains, &Topology.PipeOutputChains })
            for (const auto& Chain : *Chains)
            {
                if (Chain.Junction.IsValid() && !Bind(Chain.Junction.Get())) return false;
                for (const auto& Actor : Chain.Pipelines) if (!Bind(Actor.Get())) return false;
                for (const auto& Actor : Chain.SupportPoles) if (!Bind(Actor.Get())) return false;
            }
        for (const auto& Pole : Topology.PowerPoles) if (!Bind(Pole.PowerPole.Get())) return false;
    }
    if (Operation->JsonIntent)
    {
        FSFExtendWiringPlan Plan(*Operation->JsonIntent);
        for (const auto& Clone : Operation->ScaledJsonIntents) if (Clone) Plan.IncludeRequirements(*Clone);
        if (!Plan.IsValid()) return false;
        for (const auto& Name : Plan.GetRequiredSources()) if (!Operation->SourceBindings.Contains(Name)) return false;
    }
    if (!Operation->SourceSeeds.IsEmpty())
    {
        FString Reason;
        if (FSFConveyorCohortValidator::Capture(*Owner, Operation->SourceSeeds, Operation->Before, Reason) != ESFConveyorCohortStatus::CoherentStructure) return false;
        for (const auto& Actor : Operation->Before.AffectedActors) if (!Owner->AdoptActor(Operation->Key, Actor.Get())) return false;
        if (FSFConveyorCohortValidator::SnapshotCargo(*Owner, Operation->Before, Operation->BeforeCargo, Reason) != ESFConveyorCohortStatus::CoherentStructure) return false;
    }
    Operation->bRemoteRootPrepared = true;
    return true;
}

bool USFExtendService::QueueNativeBuildInput(UFGBuildGunStateBuild* Receiver, bool bReleased, bool bFinalStep,
    TFunction<void(UFGBuildGunStateBuild*, bool)> Dispatch)
{
    if (!Subsystem.IsValid() || !IsValid(Receiver)) return false;
    auto* Coordinator = Subsystem->GetConstructionCoordinator();
    AFGHologram* Root = Receiver->GetHologram();
    if (ExecutingNativeBuildInput && ExecutingNativeBuildInput->Receiver.Get() == Receiver)
    {
        // Only the explicitly permitted re-entry reaches the rest of the native hook chain.
        return !Coordinator || !Coordinator->IsInMutationPhase()
            || Receiver->GetWorld() != Subsystem->GetWorld()
            || !IsValid(Root) || Root->GetConstructionInstigator() != ExecutingNativeBuildInput->Pawn.Get()
            || !ExecutingNativeBuildInput->Pawn.IsValid()
            || FObjectKey(ExecutingNativeBuildInput->Pawn->GetController()) != ExecutingNativeBuildInput->Key.Requester
            || !ExecutingNativeBuildInput->Ticket.Consume(Coordinator->GetWorldGeneration(), FObjectKey(Receiver), FObjectKey(Root));
    }
    UWorld* World = Receiver->GetWorld();
    if (!World || World != Subsystem->GetWorld() || World->GetNetMode() == NM_Client) return false;
    if (IsValid(Root) && NativeBuildInputs.Contains(FObjectKey(Root))) return true;
    if (!bFinalStep) return false;
    if (!IsValid(Root) || !Root->HasAuthority() || Root->GetParentHologram()
        || Subsystem->GetActiveHologram() != Root || (!IsExtendModeActive() && !IsRestoredCloneTopologyActive())) return false;
    AFGCharacterPlayer* Pawn = Cast<AFGCharacterPlayer>(Root->GetConstructionInstigator());
    AFGPlayerController* PC = Pawn ? Cast<AFGPlayerController>(Pawn->GetController()) : nullptr;
    if (!PC || !FSFExtendNativeInputTicket::CanOwnLocalInput(World->GetNetMode(), PC->HasAuthority(),
        PC->IsLocalController() && Pawn->IsLocallyControlled(), bFinalStep)) return false;
    if (!Coordinator || !Dispatch) return true;
    auto Operation = MakeShared<FSFExtendNativeInputOperation>(Coordinator->GetWorldGeneration(), Receiver, Root);
    Operation->Pawn = Pawn; Operation->Gun = Receiver->GetBuildGun();
    Operation->Designer = Root->GetBlueprintDesigner();
    Operation->ActiveRecipe = FObjectKey(Receiver->GetActiveRecipe().Get());
    if (!BuildCommitSpecForMP(Root, Operation->Intent) || !CaptureFrames(Root, Operation->Frames))
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore native input refused before construction: immutable input capture unavailable."));
        return true;
    }
    const auto& Settings = Operation->Intent.FactorySettings;
    if ((Settings.bHasRecipe && !Settings.Recipe)
        || (Settings.bHasPotential && (!FMath::IsFinite(Settings.Potential) || Settings.Potential <= 0.0f))
        || (Settings.bHasProductionBoost && (!FMath::IsFinite(Settings.ProductionBoost) || Settings.ProductionBoost <= 0.0f))
        || Settings.OverclockShardCount < 0 || Settings.OverclockShardCount > 3
        || Settings.ProductionBoostShardCount < 0 || Settings.ProductionBoostShardCount > 2)
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore native input refused before construction: invalid factory settings values."));
        return true;
    }
    if (!StoredCloneTopology)
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore native input refused before construction: captured topology unavailable."));
        return true;
    }
    if (StoredCloneTopology) Operation->JsonIntent = MakeShared<const FSFCloneTopology>(*StoredCloneTopology);
    for (const auto& Clone : ScaledExtendClones)
    {
        TSharedPtr<const FSFCloneTopology> Copy;
        if (Clone.CloneTopology) Copy = MakeShared<const FSFCloneTopology>(*Clone.CloneTopology);
        Operation->ScaledJsonIntents.Add(MoveTemp(Copy));
    }
    if (!ValidateCapturedConstructionPlan(Root))
    {
        UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore native input refused before construction: captured construction plan is invalid, unsupported or missing required values; legacy presets may need recapture."));
        return true;
    }
    FSFConstructionOperationRequest Request;
    Request.Requester = PC; Request.RequestId = Operation->Id; Request.WorldGeneration = Operation->World;
    Request.Sequence = Subsystem->AllocateConstructionRequestSequence();
    Request.Stream = ESFConstructionRequestStream::AuthorityBuildInput;
    Request.IntentDigest = Operation->Id.ToString();
    Operation->Key = {FObjectKey(PC), Request.Sequence, Request.Stream};
    auto Include = [&](AActor* Actor)
    {
        if (IsValid(Actor)) { Request.ExpectedCohort.Add(FObjectKey(Actor)); Request.AffectedActors.AddUnique(Actor); }
    };
    auto BindSource = [&](AFGBuildable* Actor)
    {
        if (!IsValid(Actor)) return;
        Include(Actor);
        const FString Name = Actor->GetName();
        const FObjectKey Identity(Actor);
        if (const FObjectKey* Prior = Operation->SourceBindings.Find(Name))
        {
            if (*Prior != Identity) Operation->bAdoptionFailed = true;
        }
        else Operation->SourceBindings.Add(Name, Identity);
    };
    Include(Root); Include(Pawn); Include(Operation->Gun.Get()); Include(Operation->Intent.SourceBuilding.Get());
    Include(Operation->Designer.Get());
    if (!Operation->Intent.bIsRestore)
    {
        const auto& Topology = GetLastExtendTopology();
        BindSource(Topology.SourceBuilding.Get());
        for (const auto* Chains : { &Topology.InputChains, &Topology.OutputChains })
            for (const auto& Chain : *Chains)
            {
                BindSource(Chain.Distributor.Get());
                for (const auto& Conveyor : Chain.Conveyors)
                {
                    BindSource(Conveyor.Get()); Operation->SourceSeeds.AddUnique(Conveyor);
                }
                for (const auto& Actor : Chain.SupportPoles) BindSource(Actor.Get());
                for (const auto& Actor : Chain.Passthroughs) BindSource(Actor.Get());
            }
        for (const auto* Chains : { &Topology.PipeInputChains, &Topology.PipeOutputChains })
            for (const auto& Chain : *Chains)
            {
                BindSource(Chain.Junction.Get());
                for (const auto& Actor : Chain.Pipelines) BindSource(Actor.Get());
                for (const auto& Actor : Chain.SupportPoles) BindSource(Actor.Get());
            }
        for (const auto& Pole : Topology.PowerPoles) BindSource(Pole.PowerPole.Get());
    }
    if (Operation->bAdoptionFailed) return true;
    if (Operation->JsonIntent)
    {
        FSFExtendWiringPlan Plan(*Operation->JsonIntent);
        for (const auto& Clone : Operation->ScaledJsonIntents) if (Clone) Plan.IncludeRequirements(*Clone);
        if (!Plan.IsValid()) return true;
        for (const FString& Name : Plan.GetRequiredSources())
            if (!Operation->SourceBindings.Contains(Name))
            {
                UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore native input refused before construction: source reference lacks admitted live-graph provenance."));
                return true;
            }
    }
    const TWeakObjectPtr<USFExtendService> WeakService(this);
    const TWeakObjectPtr<USFConstructionOperationCoordinator> WeakCoordinator(Coordinator);
    const auto Admission = Coordinator->Submit(Request,
        [WeakService, WeakCoordinator, Operation, bReleased, Dispatch = MoveTemp(Dispatch)](FSFConstructionOperationRecord& Record)
        {
            auto* Service = WeakService.Get(); auto* Owner = WeakCoordinator.Get();
            auto Reject = [&](const TCHAR* Reason)
            {
                Record.RequestCancellation(Operation->World); Record.SettleCancellation(Operation->World);
                UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore native input rejected before construction: operation=%s reason=%s."), *Operation->Id.ToString(), Reason);
            };
            if (!Service || !Owner || !Owner->IsInMutationPhase() || Owner->GetWorldGeneration() != Operation->World
                || !Operation->Receiver.IsValid() || !Operation->Root.IsValid() || !Operation->Pawn.IsValid() || !Operation->Gun.IsValid())
            { Reject(TEXT("receiver_lifetime")); return; }
            // Native callbacks can tear down owners. Pin memory for this synchronous
            // stack without treating a destruction-requested actor as usable again.
            TStrongObjectPtr<USFExtendService> ServiceKeepAlive(Service);
            TStrongObjectPtr<UFGBuildGunStateBuild> ReceiverKeepAlive(Operation->Receiver.Get());
            TStrongObjectPtr<AFGHologram> RootKeepAlive(Operation->Root.Get());
            TStrongObjectPtr<AFGCharacterPlayer> PawnKeepAlive(Operation->Pawn.Get());
            TStrongObjectPtr<AFGBuildGun> GunKeepAlive(Operation->Gun.Get());
            if (Operation->Receiver->GetHologram() != Operation->Root.Get()
                || FObjectKey(Operation->Receiver->GetActiveRecipe().Get()) != Operation->ActiveRecipe
                || Operation->Pawn->GetEquipmentInSlot(EEquipmentSlot::ES_ARMS) != Operation->Gun.Get()
                || Operation->Root->GetConstructionInstigator() != Operation->Pawn.Get()
                || FObjectKey(Operation->Pawn->GetController()) != Operation->Key.Requester)
            { Reject(TEXT("active_equipment_or_root_changed")); return; }
            if (!Service->Subsystem.IsValid() || !Service->Subsystem->IsSmartEnabledForSession()
                || Service->Subsystem->GetActiveHologram() != Operation->Root.Get()
                || (!Service->IsExtendModeActive() && !Service->IsRestoredCloneTopologyActive()))
            { Reject(TEXT("session_or_mode_changed")); return; }
            FSFExtendCommitSpec Current;
            TArray<FHologramFrame> Frames;
            if (!Service->BuildCommitSpecForMP(Operation->Root.Get(), Current) || !SameIntent(Operation->Intent, Current)
                || !CaptureFrames(Operation->Root.Get(), Frames) || !SameFrames(Operation->Frames, Frames))
            { Reject(TEXT("intent_changed")); return; }
            if (Operation->JsonIntent.IsValid() != Service->StoredCloneTopology.IsValid()
                || (Operation->JsonIntent && !SameTopology(*Operation->JsonIntent, *Service->StoredCloneTopology)))
            { Reject(TEXT("json_intent_changed")); return; }
            if (Operation->ScaledJsonIntents.Num() != Service->ScaledExtendClones.Num())
            { Reject(TEXT("scaled_intent_changed")); return; }
            for (int32 Index = 0; Index < Operation->ScaledJsonIntents.Num(); ++Index)
            {
                const auto& Saved = Operation->ScaledJsonIntents[Index];
                const auto& CurrentClone = Service->ScaledExtendClones[Index].CloneTopology;
                if (Saved.IsValid() != CurrentClone.IsValid() || (Saved && !SameTopology(*Saved, *CurrentClone)))
                { Reject(TEXT("scaled_intent_changed")); return; }
            }
            // This is new construction, not the distinct native Upgrade/refund
            // adapter. The dispatcher reads its own upgrade target directly.
            const FObjectPropertyBase* Upgrade = FindFProperty<FObjectPropertyBase>(Operation->Receiver->GetClass(), TEXT("mUpgradedActor"));
            if (!Upgrade || Upgrade->GetObjectPropertyValue_InContainer(Operation->Receiver.Get()) || Operation->Root->IsUpgrade())
            { Reject(TEXT("native_upgrade_requires_separate_adapter")); return; }
            if (!Operation->SourceSeeds.IsEmpty())
            {
                FString Reason;
                if (FSFConveyorCohortValidator::Capture(*Owner, Operation->SourceSeeds, Operation->Before, Reason) != ESFConveyorCohortStatus::CoherentStructure)
                { Reject(TEXT("source_cohort_not_settled_or_supported")); return; }
                for (const auto& Actor : Operation->Before.AffectedActors)
                    if (!Owner->AdoptActor(Operation->Key, Actor.Get())) { Reject(TEXT("source_cohort_conflict")); return; }
                if (FSFConveyorCohortValidator::SnapshotCargo(*Owner, Operation->Before, Operation->BeforeCargo, Reason) != ESFConveyorCohortStatus::CoherentStructure)
                { Reject(TEXT("source_cargo_not_supported")); return; }
            }
            if (Operation->JsonIntent)
            {
                for (const auto& Child : Operation->JsonIntent->ChildHolograms)
                {
                    if (Child.bPreviewOnly || !Child.bConstructible || (Child.Role != TEXT("belt_segment") && Child.Role != TEXT("lift_segment")
                        && !(Child.Role == TEXT("lane_segment") && Child.LaneSegmentType == TEXT("belt")))) continue;
                    for (const FSFConnectionRef* Endpoint : { &Child.CloneConnections.ConveyorAny0, &Child.CloneConnections.ConveyorAny1 })
                    {
                        if (!Endpoint->Target.StartsWith(TEXT("source:"))) continue;
                        const FObjectKey* Binding = Operation->SourceBindings.Find(Endpoint->Target.Mid(7));
                        AActor* Actor = Binding ? Cast<AActor>(Binding->ResolveObjectPtr()) : nullptr;
                        if (!IsValid(Actor)) { Reject(TEXT("source_binding_expired")); return; }
                        TArray<UFGFactoryConnectionComponent*> Ports;
                        Actor->GetComponents(Ports);
                        UFGFactoryConnectionComponent* Match = nullptr;
                        for (auto* Port : Ports)
                            if (Port && Port->GetName() == Endpoint->Connector)
                            {
                                if (Match) { Reject(TEXT("source_port_ambiguous")); return; }
                                Match = Port;
                            }
                        if (!Match || Match->IsConnected()) { Reject(TEXT("source_port_missing_or_occupied")); return; }
                    }
                }
            }
            Operation->Root->ValidatePlacementAndCost(Operation->Pawn->GetInventory());
            if (!Operation->Root->CanConstruct()) { Reject(TEXT("native_placement_or_cost")); return; }
            if (HasTypedSettingsMaterials(Operation->Pawn->GetInventory(), Operation->Intent.FactorySettings))
            { Reject(TEXT("typed_settings_material_requires_adapter")); return; }
            if (!Operation->Ticket.Permit(Operation->World, FObjectKey(Operation->Receiver.Get()), FObjectKey(Operation->Root.Get()), true)
                || !Record.ClaimEffect(Operation->World, NativeInputEffect)) { Reject(TEXT("native_entry_not_claimed")); return; }
            if (!Record.ClaimEffect(Operation->World, FactorySettingsEffect))
            {
                Record.ObserveEffect(Operation->World, NativeInputEffect, ESFConstructionEffectOutcome::NoEffect);
                Reject(TEXT("settings_owner_not_claimed")); return;
            }
            TGuardValue<TSharedPtr<FSFExtendNativeInputOperation>> Active(Service->ExecutingNativeBuildInput, Operation);
            // This borrowed record is visible only in the current synchronous callback stack.
            TGuardValue<FSFConstructionOperationRecord*> RecordScope(Service->NativeInputRecordInCurrentStack, &Record);
            Dispatch(Operation->Receiver.Get(), bReleased);
            Service->FinishOwnedNativeInputDispatch(Operation, Record);
        },
        [WeakService, Operation](FSFConstructionOperationRecord& Record)
        {
            if (auto* Service = WeakService.Get()) Service->ObserveOwnedNativeInput(Operation, Record);
        },
        [WeakService, Operation](ESFConstructionOperationState)
        {
            if (auto* Service = WeakService.Get())
            {
                auto* Existing = Service->NativeBuildInputs.Find(Operation->AdmissionIdentity);
                if (Existing && *Existing == Operation) Service->NativeBuildInputs.Remove(Operation->AdmissionIdentity);
            }
        });
    if (Admission == ESFConstructionAdmission::Accepted) NativeBuildInputs.Add(FObjectKey(Root), Operation);
    else UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore native input admission refused before construction: operation=%s admission=%d."), *Operation->Id.ToString(), static_cast<int32>(Admission));
    return true;
}

void USFExtendService::FinishOwnedNativeInputDispatch(const TSharedPtr<FSFExtendNativeInputOperation>& Operation,
    FSFConstructionOperationRecord& Record)
{
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Owner) return;
    const TWeakObjectPtr<USFExtendService> WeakService(this);
    const bool bOutputs = Operation->Wiring && !Operation->Wiring->ActualOutputs.IsEmpty();
    if (!Operation->bNativeConstructionInvoked && !bOutputs && !Operation->bPaymentObserved)
    {
        // Both known dispatcher envelopes reach physical construction/payment
        // exclusively through the marked InternalConstruct continuation. This
        // explicit boundary distinguishes validation rejection from a null output
        // after invocation, which remains uncertain and must never auto-retry.
        Record.ObserveEffect(Operation->World, NativeInputEffect, ESFConstructionEffectOutcome::NoEffect);
        Record.ObserveEffect(Operation->World, FactorySettingsEffect, ESFConstructionEffectOutcome::NoEffect);
        Operation->bSettingsDone = Operation->bSettingsConserved = true;
        Record.RequestCancellation(Operation->World);
        Record.SettleCancellation(Operation->World);
        UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore cancelled before native construction/payment boundary: operation=%s reason=%s."), *Operation->Id.ToString(), Operation->NativeRefusal.IsEmpty() ? TEXT("native_dispatch_did_not_reach_construction") : *Operation->NativeRefusal);
        return;
    }
    Record.ObserveEffect(Operation->World, NativeInputEffect,
        bOutputs && !Operation->bAdoptionFailed ? ESFConstructionEffectOutcome::Applied : ESFConstructionEffectOutcome::Uncertain);
    if (bOutputs && Operation->bConveyorWiringVerified && Operation->bDistributorSettingsVerified && Operation->bCargoVerified
        && Operation->bConstructionMaterialsVerified && !Operation->bAdoptionFailed && ValidateOwnedPowerWiring(Operation))
    {
        if (!Owner->QueueContinuation(Operation->Key, FactorySettingsEffect,
            [WeakService, Operation](FSFConstructionOperationRecord& NextRecord)
            {
                if (auto* NextService = WeakService.Get()) NextService->ContinueOwnedFactorySettings(Operation, NextRecord);
            }))
            Record.ObserveEffect(Operation->World, FactorySettingsEffect, ESFConstructionEffectOutcome::Uncertain);
    }
    else
    {
        // All legacy settings writes were intercepted. No additional
        // settings are attempted on a construction whose wiring is uncertain.
        Operation->bSettingsDone = Operation->bSettingsConserved = true;
        Record.ObserveEffect(Operation->World, FactorySettingsEffect, ESFConstructionEffectOutcome::NoEffect);
    }
    UE_LOG(LogSmartFoundations, Log,
        TEXT("Extend/Restore native input dispatched: operation=%s nativeEntry=%d outputs=%d; graph/cargo/resources/deferred settlement unverified."),
        *Operation->Id.ToString(), (Operation->bRemote ? Operation->bRemoteEntryConsumed : Operation->Ticket.WasConsumed()) ? 1 : 0, bOutputs ? Operation->Wiring->ActualOutputs.Num() : 0);
}

void USFExtendService::ObserveOwnedNativeInput(const TSharedPtr<FSFExtendNativeInputOperation>& Operation,
    FSFConstructionOperationRecord& Record)
{
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    const TWeakObjectPtr<USFExtendService> WeakService(this);
    if (!Operation || !WeakService.IsValid() || !Owner || !Owner->IsInObservationPhase()
        || Owner->GetWorldGeneration() != Operation->World || Operation->bReconciliationReported) return;
    auto Uncertain = [&](const FString& Reason)
    {
        Operation->bReconciliationReported = true;
        Record.RequireReconciliation(Operation->World);
        UE_LOG(LogSmartFoundations, Warning, TEXT("Extend/Restore operation %s requires reconciliation: %s. No success, rollback or automatic retry is reported."),
            *Operation->Id.ToString(), *Reason);
    };


    if (Record.IsCancellationRequested() && !Operation->bSettingsAttempted && !Operation->bSettingsDone)
    {
        Operation->bSettingsDone = Operation->bSettingsConserved = true;
        Record.ObserveEffect(Operation->World, FactorySettingsEffect, ESFConstructionEffectOutcome::NoEffect);
    }
    if (!Operation->bConveyorWiringVerified
        || !Operation->bCargoVerified || !Operation->bDistributorSettingsVerified || Operation->bAdoptionFailed || Operation->bTopologyObserved
        || !Operation->bSettingsDone || !Operation->bSettingsConserved
        || !WeakService->ValidateOwnedDistributorSettings(Operation)
        || !WeakService->ValidateOwnedPowerWiring(Operation))
    {
        if (!Operation->bTopologyObserved && ++Operation->SettlementPhases >= 120)
            Uncertain(TEXT("Native output, wiring, cargo or settings obligations did not settle"));
        return;
    }
    for (const auto& Edge : Operation->ExpectedEdges)
    {
        auto* Port = Edge.Port.Get(); auto* Peer = Edge.Peer.Get();
        if (!IsValid(Port) || Edge.Peer.IsStale() || Port->GetConnection() != Peer
            || Port->IsConnected() != (Peer != nullptr)
            || (Peer && (Peer->GetConnection() != Port || !Peer->IsConnected())))
        { Uncertain(TEXT("An admitted reciprocal edge changed before settlement")); return; }
    }
    if (!Operation->Remaining.IsEmpty())
    {
        FSFConveyorCohort Settled;
        FString Reason;
        const auto Status = FSFConveyorCohortValidator::Capture(*Owner, Operation->Remaining, Settled, Reason);
        if (Status != ESFConveyorCohortStatus::CoherentStructure)
        {
            if (Status != ESFConveyorCohortStatus::NativePending || ++Operation->SettlementPhases >= 120) Uncertain(Reason);
            return;
        }
        for (const auto& Actor : Settled.AffectedActors)
            if (!Owner->AdoptActor(Operation->Key, Actor.Get()))
            { Operation->bAdoptionFailed = true; Uncertain(TEXT("Settled native membership conflicts with another operation")); return; }
    }
    FSFExtendLiftAttachmentReceipt LiftAttachments;
    FString AttachmentReason;
    if (!Operation->Wiring || !Operation->Wiring->Plan
        || !LiftAttachments.Verify(Operation->Wiring->Plan->GetTopology(), Operation->Wiring->Plan->GetBindings(),
            Operation->Wiring->ActualOutputs, *Owner, Operation->Key, AttachmentReason))
    { Uncertain(AttachmentReason); return; }
    for (const auto& Clone : Operation->Wiring->ScaledPlans)
        if (!Clone.CloneTopology || !LiftAttachments.Verify(*Clone.CloneTopology, Operation->Wiring->Plan->GetBindings(),
            Operation->Wiring->ActualOutputs, *Owner, Operation->Key, AttachmentReason))
        { Uncertain(AttachmentReason); return; }
    FSFExtendPipeCompatibilityReceipt PipeSiblings;
    if (!PipeSiblings.Verify(Operation->Wiring->Plan->GetTopology(), Operation->Wiring->Plan->GetBindings(),
        Operation->Wiring->SourceBindings, Operation->Wiring->ActualOutputs, *Owner, Operation->Key, AttachmentReason))
    { Uncertain(AttachmentReason); return; }
    for (const auto& Clone : Operation->Wiring->ScaledPlans)
        if (!Clone.CloneTopology || !PipeSiblings.Verify(*Clone.CloneTopology, Operation->Wiring->Plan->GetBindings(),
            Operation->Wiring->SourceBindings, Operation->Wiring->ActualOutputs, *Owner, Operation->Key, AttachmentReason))
    { Uncertain(AttachmentReason); return; }
    bool bNativeProof = true;
    for (auto Obligation : {ESFConstructionObligation::Topology, ESFConstructionObligation::NativeMembership,
        ESFConstructionObligation::Cargo}) bNativeProof &= Record.VerifyObligation(Operation->World, Obligation);
    if (!bNativeProof) return;
    bool bCompleteReceipts = Operation->Wiring && Operation->Wiring->Plan
        && FSFExtendWiringPlan::HasSupportedEffects(Operation->Wiring->Plan->GetTopology());
    if (Operation->Wiring)
    {
        for (const auto& Clone : Operation->Wiring->ScaledPlans)
            bCompleteReceipts &= Clone.CloneTopology && FSFExtendWiringPlan::HasSupportedEffects(*Clone.CloneTopology);
        TSet<FObjectKey> AllOutputs = Operation->Wiring->ActualOutputs;
        AllOutputs.Append(Operation->AuxiliaryOutputs);
        for (const auto& Identity : AllOutputs)
        {
            auto* Built = Cast<AFGBuildable>(Identity.ResolveObjectPtr());
            auto* Designer = Operation->Designer.Get();
            if (!IsValid(Built) || Built->IsActorBeingDestroyed() || Operation->Designer.IsStale()
                || Built->GetBlueprintDesigner() != Designer) { bCompleteReceipts = false; continue; }
            if (Designer)
            {
                int32 Occurrences = 0;
                for (auto* Member : Designer->GetBuildablesInBlueprintDesigner()) Occurrences += Member == Built;
                bCompleteReceipts &= Occurrences == 1 && Owner->AdoptActor(Operation->Key, Designer);
                // Inside a Designer, native membership replaces dismantle proxies.
                bCompleteReceipts &= Built->GetBlueprintProxy() == nullptr;
            }
            const FString Package = Built->GetClass()->GetOutermost()->GetName();
            bCompleteReceipts &= Package.StartsWith(TEXT("/Game/FactoryGame/")) || Package == TEXT("/Script/FactoryGame");
            // Configurable outputs were applied from the frozen plan and read back.
            if (Built->IsA<AFGBuildablePipeline>() || Built->IsA<AFGBuildablePipelineJunction>())
                bCompleteReceipts &= PipeSiblings.Actors.Contains(Identity);
            if (Built->IsA<AFGBuildablePassthrough>())
                bCompleteReceipts &= LiftAttachments.Holes.Contains(Identity) || PipeSiblings.Actors.Contains(Identity);
            if (auto* Lift = Cast<AFGBuildableConveyorLift>(Built))
                for (auto* Passthrough : Lift->GetSnappedPassthroughs())
                    if (Passthrough) bCompleteReceipts &= LiftAttachments.Lifts.Contains(Identity);
            if (auto* Group = Built->GetBlueprintProxy())
            {
                if (!IsValid(Group) || Group->IsActorBeingDestroyed() || Group->GetWorld() != Owner->GetWorld())
                { bCompleteReceipts = false; continue; }
                int32 Occurrences = 0;
                for (auto* Member : Group->GetBuildables()) Occurrences += Member == Built;
                bCompleteReceipts &= Occurrences == 1 && Owner->AdoptActor(Operation->Key, Group);
            }
        }
    }
    Operation->bTopologyObserved = true;
    if (!bCompleteReceipts || !Operation->bConstructionMaterialsVerified || !Operation->bSettingsConserved)
    {
        Uncertain(TEXT("Returned actor, group, Designer or resource receipts do not match the admitted plan"));
        return;
    }
    if (bCompleteReceipts && Operation->bConstructionMaterialsVerified && Operation->bSettingsConserved)
    {
        Record.VerifyObligation(Operation->World, ESFConstructionObligation::DeferredWork);
        Record.VerifyObligation(Operation->World, ESFConstructionObligation::Resources);
        if (!Record.Complete(Operation->World)) Record.SettleCancellation(Operation->World);
    }
    UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore conveyor settlement observed: operation=%s members=%d edges=%d cargo=verified completeReceipts=%d settled=%d; other adapters remain required."),
        *Operation->Id.ToString(), Operation->Remaining.Num(), Operation->ExpectedEdges.Num(), bCompleteReceipts ? 1 : 0, Record.IsSettled() ? 1 : 0);
}

FGuid USFExtendService::GetExecutingNativeInputId(AFGHologram* Root) const
{
    return ExecutingNativeBuildInput && ExecutingNativeBuildInput->Root.Get() == Root
        ? ExecutingNativeBuildInput->Id : FGuid();
}

bool USFExtendService::MarkOwnedNativeConstructionInvoked(AFGHologram* Root)
{
    const auto Operation = ExecutingNativeBuildInput;
    if (!Operation || Operation->bNativeConstructionInvoked || Operation->Root.Get() != Root
        || !Operation->bPaymentPrepared || !CanExecuteOwnedInputWiring(Operation->Id)) return false;
    Operation->bNativeConstructionInvoked = true;
    return true;
}

const TMap<FString, FObjectKey>* USFExtendService::GetExecutingNativeInputSources() const
{
    return ExecutingNativeBuildInput ? &ExecutingNativeBuildInput->SourceBindings : nullptr;
}

bool USFExtendService::CanExecuteOwnedInputWiring(const FGuid& Id) const
{
    const auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    return ExecutingNativeBuildInput && ExecutingNativeBuildInput->Id == Id && NativeInputRecordInCurrentStack
        && Owner && Owner->IsInMutationPhase() && Owner->GetWorldGeneration() == ExecutingNativeBuildInput->World
        && !ExecutingNativeBuildInput->bAdoptionFailed;
}

bool USFExtendService::ClaimOwnedInputWiring()
{
    return ExecutingNativeBuildInput && NativeInputRecordInCurrentStack
        && NativeInputRecordInCurrentStack->ClaimEffect(ExecutingNativeBuildInput->World, NativeWiringEffect);
}

void USFExtendService::ObserveOwnedInputWiring()
{
    if (ExecutingNativeBuildInput && NativeInputRecordInCurrentStack)
        NativeInputRecordInCurrentStack->ObserveEffect(ExecutingNativeBuildInput->World, NativeWiringEffect,
            ExecutingNativeBuildInput->bConveyorWiringVerified && ExecutingNativeBuildInput->bCargoVerified
                && ValidateOwnedDistributorSettings(ExecutingNativeBuildInput)
                && ValidateOwnedPowerWiring(ExecutingNativeBuildInput)
                ? ESFConstructionEffectOutcome::Applied : ESFConstructionEffectOutcome::Uncertain);
}

bool USFExtendService::PrepareOwnedNativePayment(AFGHologram* Root)
{
    auto Operation = ExecutingNativeBuildInput;
    if (!Operation) return false; // Extend/Restore construction requires its operation owner.
    if (!CanExecuteOwnedInputWiring(Operation->Id) || Operation->Root.Get() != Root || Operation->bPaymentPrepared
        || !Operation->Pawn.IsValid()) return false;
    auto* Inventory = Operation->Pawn->GetInventory();
    auto* Storage = AFGCentralStorageSubsystem::Get(Root->GetWorld());
    if (!IsValid(Inventory) || !IsValid(Storage)) return false;
    Operation->PayerInventory = Inventory;
    Operation->PayerStorage = Storage;
    Operation->bNoBuildCost = Inventory->GetNoBuildCost();
    // Read the prepared full-tree native quote while the native request's cost
    // scopes are alive. Do not charge here or reconstruct a recipe-only price.
    for (const FItemAmount& Entry : Root->GetCost(true))
    {
        if (!Entry.ItemClass || Entry.Amount < 0) return false;
        Operation->QuotedCost.FindOrAdd(Entry.ItemClass) += Entry.Amount;
    }
    TMap<TSubclassOf<UFGItemDescriptor>, int64> WireTotals;
    for (const auto& Frame : Operation->Frames)
    {
        auto* Holo = Frame.Actor.Get();
        auto* Class = Cast<UClass>(Frame.BuildClass.ResolveObjectPtr());
        if (!Holo || !Class || !Class->IsChildOf(AFGBuildableWire::StaticClass())) continue;
        const auto* Data = USFHologramDataRegistry::GetData(Holo);
        if (!Data || Data->JsonCloneId.IsEmpty() || Operation->PowerQuotes.Contains(Data->JsonCloneId)) return false;
        auto& Quote = Operation->PowerQuotes.Add(Data->JsonCloneId);
        for (const auto& Entry : Holo->GetCost(false))
        {
            if (!Entry.ItemClass || Entry.Amount <= 0) return false;
            Quote.FindOrAdd(Entry.ItemClass) += Entry.Amount;
            WireTotals.FindOrAdd(Entry.ItemClass) += Entry.Amount;
        }
        if (Quote.IsEmpty() && !Operation->bNoBuildCost) return false;
    }
    for (const auto& Entry : WireTotals)
        if (Operation->QuotedCost.FindRef(Entry.Key) < Entry.Value) return false;
    for (const auto& Entry : Operation->QuotedCost)
    {
        const int64 Count = int64(Inventory->GetNumItems(Entry.Key)) + Storage->GetNumItemsFromCentralStorage(Entry.Key);
        if (Count < 0 || (!Operation->bNoBuildCost && Count < Entry.Value)) return false;
        Operation->MaterialsBefore.Add(Entry.Key, Count);
    }
    Operation->bPaymentPrepared = true;
    return true;
}

bool USFExtendService::IsExecutingOwnedNativeInput() const
{
    return ExecutingNativeBuildInput.IsValid() && NativeInputRecordInCurrentStack != nullptr;
}

bool USFExtendService::CaptureOwnedFactorySettingsRequest(AFGBuildable* Target, AFGCharacterPlayer* Player)
{
    auto Operation = ExecutingNativeBuildInput;
    if (!Operation || !NativeInputRecordInCurrentStack) return false;
    // Native callbacks may arrive before the returned-output list exists. Record
    // identities now, then constrain all of them to that exact list before applying.
    if (!Subsystem.IsValid() || !IsValid(Target) || Target->GetWorld() != Subsystem->GetWorld() || !Target->HasAuthority()
        || Player != Operation->Pawn.Get()) Operation->bSettingsRequestConflict = true;
    if (Target) Operation->SuppressedSettingsTargets.Add(FObjectKey(Target));
    return true;
}

bool USFExtendService::ValidateOwnedDistributorSettings(const TSharedPtr<FSFExtendNativeInputOperation>& Operation) const
{
    if (!Operation || !Operation->bDistributorSettingsVerified || !Subsystem.IsValid()) return false;
    for (const auto& Pair : Operation->DistributorSettingsReceipts)
    {
        auto* Built = Cast<AFGBuildable>(Pair.Key.ResolveObjectPtr());
        if (!IsValid(Built) || !Built->HasAuthority() || Built->GetWorld() != Subsystem->GetWorld()
            || !Pair.Value.Matches(Built)) return false;
    }
    return true;
}

bool USFExtendService::ApplyOwnedDistributorSettings()
{
    const auto Operation = ExecutingNativeBuildInput;
    if (!Operation || !CanExecuteOwnedInputWiring(Operation->Id) || !Operation->Wiring || !Operation->Wiring->Plan) return false;
    if (Operation->bDistributorSettingsAttempted) return ValidateOwnedDistributorSettings(Operation);
    // Validate the whole batch before invoking any native configuration delegate.
    auto Include = [&](const FSFCloneTopology& Plan)
    {
        for (const auto& Child : Plan.ChildHolograms)
        {
            if (Child.Role != TEXT("distributor") || Child.bPreviewOnly || !Child.bConstructible) continue;
            const auto* Identity = Operation->Wiring->Plan->GetBindings().Find(Child.HologramId);
            auto* Built = Identity ? Cast<AFGBuildable>(Identity->ResolveObjectPtr()) : nullptr;
            if (!IsValid(Built) || !Operation->Wiring->ActualOutputs.Contains(FObjectKey(Built))) return false;
            const bool bConfigurable = Built->IsA<AFGBuildableSplitterSmart>() || Built->IsA<AFGBuildableMergerPriority>();
            if (!Child.DistributorSettings.bCaptured && !bConfigurable) continue;
            if (!Child.DistributorSettings.IsValidFor(Built) || Operation->DistributorSettingsReceipts.Contains(FObjectKey(Built))) return false;
            Operation->DistributorSettingsReceipts.Add(FObjectKey(Built), Child.DistributorSettings);
        }
        return true;
    };
    bool bValid = Include(Operation->Wiring->Plan->GetTopology());
    for (const auto& Clone : Operation->Wiring->ScaledPlans) if (Clone.CloneTopology) bValid &= Include(*Clone.CloneTopology);
    if (!bValid) return false;
    Operation->bDistributorSettingsAttempted = true;
    // Work from a value copy: setters broadcast delegates that may retire actors.
    const auto Receipts = Operation->DistributorSettingsReceipts;
    for (const auto& Pair : Receipts)
    {
        auto* Built = Cast<AFGBuildable>(Pair.Key.ResolveObjectPtr());
        if (!IsValid(Built) || !Built->HasAuthority() || !Subsystem.IsValid() || Built->GetWorld() != Subsystem->GetWorld()
            || !Pair.Value.ApplyAndVerify(Built)) return false;
    }
    Operation->bDistributorSettingsVerified = true;
    return ValidateOwnedDistributorSettings(Operation);
}

void USFExtendService::ContinueOwnedFactorySettings(const TSharedPtr<FSFExtendNativeInputOperation>& Operation,
    FSFConstructionOperationRecord& Record)
{
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Operation || !Owner || !Owner->IsInMutationPhase() || Owner->GetWorldGeneration() != Operation->World
        || Operation->bSettingsAttempted || Operation->bSettingsDone || !Record.CanContinueEffect(Operation->World, FactorySettingsEffect)) return;
    auto Fail = [&](const TCHAR* Reason)
    {
        // Invoked attempts are never retried. A pre-invocation failure has a known
        // no-effect settings outcome, but earlier construction remains applied.
        Record.ObserveEffect(Operation->World, FactorySettingsEffect,
            Operation->bSettingsAttempted ? ESFConstructionEffectOutcome::Uncertain : ESFConstructionEffectOutcome::NoEffect);
        Record.RequestCancellation(Operation->World);
        if (!Operation->bSettingsAttempted) Operation->bSettingsDone = Operation->bSettingsConserved = true;
        UE_LOG(LogSmartFoundations, Warning, TEXT("Extend/Restore factory settings unsettled: operation=%s attempted=%d targets=%d reason=%s; no retry of invoked effects."),
            *Operation->Id.ToString(), Operation->bSettingsAttempted ? 1 : 0, Operation->SettingsReceipts.Num(), Reason);
    };
    if (!Operation->Wiring || Operation->bAdoptionFailed || Operation->bSettingsRequestConflict)
    { Fail(TEXT("settings_request_identity")); return; }
    for (const auto& Identity : Operation->SuppressedSettingsTargets)
        if (!Operation->Wiring->ActualOutputs.Contains(Identity)) { Fail(TEXT("settings_target_not_returned")); return; }
    auto* Player = Operation->Pawn.Get();
    auto* Inventory = IsValid(Player) ? Player->GetInventory() : nullptr;
    auto* Recipes = Subsystem->GetRecipeManagementService();
    if (!Inventory || Inventory != Operation->PayerInventory.Get() || !Recipes)
    { Fail(TEXT("payer_or_settings_service_lifetime")); return; }
    if (!ValidateOwnedPowerWiring(Operation)) { Fail(TEXT("power_wiring_changed_before_settings")); return; }
    if (!ValidateOwnedDistributorSettings(Operation)) { Fail(TEXT("distributor_settings_changed_before_factory_settings")); return; }
    // Recheck physical identity before any later material transfer. Normal input
    // may dismantle or replace an endpoint between construction and this phase.
    for (const auto& Edge : Operation->ExpectedEdges)
    {
        auto* Port = Edge.Port.Get(); auto* Peer = Edge.Peer.Get();
        if (!IsValid(Port) || Edge.Peer.IsStale() || Port->GetConnection() != Peer
            || Port->IsConnected() != (Peer != nullptr) || (Peer && Peer->GetConnection() != Port))
        { Fail(TEXT("graph_changed_before_settings")); return; }
    }
    if (!Operation->Remaining.IsEmpty())
    {
        FSFConveyorCohort Current;
        FString Reason;
        const auto Status = FSFConveyorCohortValidator::Capture(*Owner, Operation->Remaining, Current, Reason);
        if (Status != ESFConveyorCohortStatus::CoherentStructure && Status != ESFConveyorCohortStatus::NativePending)
        { Fail(TEXT("native_membership_changed_before_settings")); return; }
        for (const auto& Actor : Current.AffectedActors)
            if (!Owner->AdoptActor(Operation->Key, Actor.Get())) { Fail(TEXT("native_membership_conflict_before_settings")); return; }
    }
    TStrongObjectPtr<AFGCharacterPlayer> PlayerPin(Player);
    TStrongObjectPtr<UFGInventoryComponent> InventoryPin(Inventory);
    const auto& Settings = Operation->Intent.FactorySettings;
    TArray<AFGBuildableFactory*> Targets;
    TArray<TStrongObjectPtr<AFGBuildableFactory>> TargetPins;
    const UClass* RootClass = Operation->Frames.IsEmpty() ? nullptr : Cast<UClass>(Operation->Frames[0].BuildClass.ResolveObjectPtr());
    for (const auto& Identity : Operation->Wiring->ActualOutputs)
    {
        auto* Factory = Cast<AFGBuildableFactory>(Identity.ResolveObjectPtr());
        if (!IsValid(Factory) || Factory->GetClass() != RootClass) continue;
        if (Factory->IsActorBeingDestroyed() || Factory->GetWorld() != Owner->GetWorld() || !Factory->HasAuthority()
            || !Owner->AdoptActor(Operation->Key, Factory)) { Fail(TEXT("returned_factory_lifetime_or_conflict")); return; }
        Targets.Add(Factory);
        TargetPins.Emplace(Factory);
    }
    if (Targets.IsEmpty()) { Fail(TEXT("missing_returned_factory")); return; }
    Targets.Sort([](const AFGBuildableFactory& A, const AFGBuildableFactory& B) { return A.GetPathName() < B.GetPathName(); });
    bool bReady = true;
    for (auto* Factory : Targets)
    {
        const bool bNeedsInventory = (Settings.bHasPotential && Settings.Potential > 1.0f && Factory->GetCanChangePotential())
            || (Settings.bHasProductionBoost && Settings.ProductionBoost > 1.0f && Factory->CanChangeProductionBoost());
        bReady &= Factory->HasActorBegunPlay() && (!bNeedsInventory || Factory->GetPotentialInventory());
    }
    if (!bReady)
    {
        if (++Operation->SettingsReadinessPhases >= 120) { Fail(TEXT("native_inventory_readiness_limit")); return; }
        TWeakObjectPtr<USFExtendService> WeakService(this);
        if (!Owner->QueueContinuation(Operation->Key, FactorySettingsEffect,
            [WeakService, Operation](FSFConstructionOperationRecord& NextRecord)
            { if (auto* Service = WeakService.Get()) Service->ContinueOwnedFactorySettings(Operation, NextRecord); }))
            Fail(TEXT("readiness_continuation_not_owned"));
        return;
    }
    TSubclassOf<UFGItemDescriptor> OverclockClass = Settings.OverclockShardClass.Get();
    TSubclassOf<UFGItemDescriptor> BoostClass = Settings.ProductionBoostShardClass.Get();
    if (OverclockClass && BoostClass && OverclockClass == BoostClass) { Fail(TEXT("ambiguous_shard_types")); return; }
    for (auto* Factory : Targets)
        if (auto* Potential = Factory->GetPotentialInventory(); Potential && !Potential->IsEmpty())
        { Fail(TEXT("new_factory_potential_inventory_not_empty")); return; }
    // These are separate from construction materials: the existing applier
    // takes shards from player inventory, not central storage or a free-build credit.
    for (const auto& Class : {OverclockClass, BoostClass})
        if (Class) Operation->SettingsMaterialsBefore.Add(Class, Inventory->GetNumItems(Class));
    if (HasTypedSettingsMaterials(Inventory, Settings)) { Fail(TEXT("typed_settings_material_requires_adapter")); return; }
    // No readiness retry is legal beyond this point. Record one attempt per exact
    // returned factory before invoking the existing bounded native slot helper.
    Operation->bSettingsAttempted = true;
    TGuardValue<TSharedPtr<FSFExtendNativeInputOperation>> Active(ExecutingNativeBuildInput, Operation);
    TGuardValue<FSFConstructionOperationRecord*> RecordScope(NativeInputRecordInCurrentStack, &Record);
    bool bAppliedAnything = false;
    for (auto* Factory : Targets)
    {
        FSFExtendNativeInputOperation::FSettingsReceipt Receipt;
        Receipt.Factory = Factory;
        auto* Manufacturer = Cast<AFGBuildableManufacturer>(Factory);
        const TSubclassOf<UFGRecipe> BeforeRecipe = Manufacturer ? Manufacturer->GetCurrentRecipe() : nullptr;
        const float BeforePotential = Factory->GetPendingPotential();
        const float BeforeBoost = Factory->GetPendingProductionBoost();
        const bool bRecipe = Settings.bHasRecipe && Settings.Recipe && Manufacturer
            && Recipes->IsRecipeCompatibleWithBuilding(Settings.Recipe, Factory);
        const bool bOverclock = Settings.bHasPotential && Settings.Potential > 1.0f && Factory->GetCanChangePotential()
            && Settings.OverclockShardClass && FSFFactorySettingsApplyPolicy::IsShardTypeAllowed(EPowerShardType::PST_Overclock,
                UFGPowerShardDescriptor::GetPowerShardType(Settings.OverclockShardClass));
        const bool bBoost = Settings.bHasProductionBoost && Settings.ProductionBoost > 1.0f && Factory->CanChangeProductionBoost()
            && Settings.ProductionBoostShardClass && FSFFactorySettingsApplyPolicy::IsShardTypeAllowed(EPowerShardType::PST_ProductionBoost,
                UFGPowerShardDescriptor::GetPowerShardType(Settings.ProductionBoostShardClass));
        const int32 ExpectedOverclock = bOverclock ? FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(
            Settings.OverclockShardCount, 0, Inventory->GetNumItems(OverclockClass)) : 0;
        const int32 ExpectedBoost = bBoost ? FSFFactorySettingsApplyPolicy::GetAffordableShardTarget(
            Settings.ProductionBoostShardCount, 0, Inventory->GetNumItems(BoostClass)) : 0;
        Operation->SettingsReceipts.Add(Receipt); // retain this identity even on an unexpected partial result
        if (!Recipes->InvokeOwnedFactorySettings(*Owner, Factory, Player, Settings)) { Fail(TEXT("owned_settings_invocation_refused")); return; }
        if (!IsValid(Factory) || Factory->IsActorBeingDestroyed() || Factory->GetWorld() != Owner->GetWorld()
            || !IsValid(Player) || Player->IsActorBeingDestroyed() || !IsValid(Inventory) || Player->GetInventory() != Inventory)
        { Fail(TEXT("factory_retired_during_settings_invocation")); return; }
        auto& Observed = Operation->SettingsReceipts.Last();
        Observed.Recipe = Manufacturer ? Manufacturer->GetCurrentRecipe() : nullptr;
        Observed.PendingPotential = Factory->GetPendingPotential(); Observed.PendingBoost = Factory->GetPendingProductionBoost();
        auto* Potential = Factory->GetPotentialInventory();
        Observed.OverclockItems = Potential && OverclockClass ? Potential->GetNumItems(OverclockClass) : 0;
        Observed.BoostItems = Potential && BoostClass ? Potential->GetNumItems(BoostClass) : 0;
        float ExpectedPotential = BeforePotential, ExpectedProductionBoost = BeforeBoost;
        if (ExpectedOverclock > 0)
            FSFFactorySettingsApplyPolicy::TryResolvePendingValue(Settings.Potential, Factory->GetCurrentMaxPotential(), ExpectedPotential);
        if (ExpectedBoost > 0)
            FSFFactorySettingsApplyPolicy::TryResolvePendingValue(Settings.ProductionBoost, Factory->GetCurrentMaxProductionBoost(), ExpectedProductionBoost);
        if (Observed.Recipe != (bRecipe ? Settings.Recipe : BeforeRecipe)
            || Observed.OverclockItems != ExpectedOverclock || Observed.BoostItems != ExpectedBoost
            || !FMath::IsNearlyEqual(Observed.PendingPotential, ExpectedPotential, 0.0001f)
            || !FMath::IsNearlyEqual(Observed.PendingBoost, ExpectedProductionBoost, 0.0001f))
        { Fail(TEXT("factory_settings_readback_differs")); return; }
        if (Potential)
        {
            TArray<FInventoryStack> Installed;
            Potential->GetInventoryStacks(Installed);
            for (const auto& Stack : Installed)
                if (!Operation->SettingsMaterialsBefore.Contains(Stack.Item.GetItemClass()) || Stack.Item.HasState()
                    || Stack.Item.LegacyItemStateActor != nullptr) { Fail(TEXT("unexpected_installed_material")); return; }
        }
        bAppliedAnything |= Observed.Recipe != BeforeRecipe || ExpectedOverclock > 0 || ExpectedBoost > 0
            || Observed.PendingPotential != BeforePotential || Observed.PendingBoost != BeforeBoost;
    }
    for (const auto& Entry : Operation->SettingsMaterialsBefore)
    {
        int64 Actual = Inventory->GetNumItems(Entry.Key);
        for (auto* Factory : Targets)
            if (auto* Potential = Factory->GetPotentialInventory()) Actual += Potential->GetNumItems(Entry.Key);
        if (Actual != Entry.Value) { Fail(TEXT("settings_material_conservation_differs")); return; }
    }
    if (Operation->bSettingsRequestConflict) { Fail(TEXT("reentrant_settings_request_conflict")); return; }
    for (const auto& Identity : Operation->SuppressedSettingsTargets)
        if (!Operation->Wiring->ActualOutputs.Contains(Identity)) { Fail(TEXT("reentrant_settings_target_not_returned")); return; }
    Operation->bSettingsDone = Operation->bSettingsConserved = true;
    Record.ObserveEffect(Operation->World, FactorySettingsEffect,
        bAppliedAnything ? ESFConstructionEffectOutcome::Applied : ESFConstructionEffectOutcome::NoEffect);
    UE_LOG(LogSmartFoundations, Log, TEXT("Extend/Restore factory settings observed: operation=%s factories=%d materialTypes=%d conserved=1; whole-operation completion still requires remaining obligations."),
        *Operation->Id.ToString(), Operation->SettingsReceipts.Num(), Operation->SettingsMaterialsBefore.Num());
}

bool USFExtendService::HasObservedOwnedNativePayment() const
{
    return ExecutingNativeBuildInput && ExecutingNativeBuildInput->bPaymentObserved
        && ExecutingNativeBuildInput->bConstructionMaterialsVerified;
}

void USFExtendService::NoteOwnedNativeInputRefusal(AFGHologram* Root, const TCHAR* Stage)
{
    const auto Operation = ExecutingNativeBuildInput;
    if (!Operation || !Operation->NativeRefusal.IsEmpty() || Operation->bNativeConstructionInvoked
        || !IsValid(Root) || !Subsystem.IsValid() || Root->GetWorld() != Subsystem->GetWorld()) return;
    TArray<FString> Reasons;
    TArray<AFGHologram*> Candidates = Root->GetHologramChildren();
    Candidates.Insert(Root, 0);
    for (AFGHologram* Candidate : Candidates)
    {
        if (!IsValid(Candidate) || Candidate->CanConstruct()) continue;
        TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
        Candidate->GetConstructDisqualifiers(Disqualifiers);
        for (const auto& Disqualifier : Disqualifiers)
        {
            Reasons.AddUnique(GetNameSafe(Candidate->GetClass()) + TEXT(":") + GetNameSafe(Disqualifier.Get())
                + (Candidate->IsHidden() ? TEXT(":hidden") : TEXT(":visible")));
            if (Reasons.Num() >= 8) break;
        }
        if (Reasons.Num() >= 8) break;
    }
    Operation->NativeRefusal = FString::Printf(TEXT("%s root=%s canConstruct=%d reasons=[%s]"),
        Stage, *GetNameSafe(Root->GetClass()), Root->CanConstruct() ? 1 : 0, *FString::Join(Reasons, TEXT(",")));
}

void USFExtendService::ObserveOwnedNativePayment()
{
    auto Operation = ExecutingNativeBuildInput;
    if (!Operation || !Operation->bPaymentPrepared || Operation->bPaymentObserved) return;
    Operation->bPaymentObserved = true;
    auto* Inventory = Operation->PayerInventory.Get(); auto* Storage = Operation->PayerStorage.Get();
    if (!CanExecuteOwnedInputWiring(Operation->Id) || !Inventory || !Storage || !Operation->Pawn.IsValid()
        || Operation->Pawn->GetInventory() != Inventory || Inventory->GetNoBuildCost() != Operation->bNoBuildCost) return;
    for (const auto& Entry : Operation->MaterialsBefore)
    {
        const int64 Actual = int64(Inventory->GetNumItems(Entry.Key)) + Storage->GetNumItemsFromCentralStorage(Entry.Key);
        const int64 Expected = Entry.Value - (Operation->bNoBuildCost ? 0 : Operation->QuotedCost.FindRef(Entry.Key));
        if (Actual != Expected) return;
    }
    Operation->bConstructionMaterialsVerified = true;
}

bool USFExtendService::ExecuteOwnedPowerWire(const FSFCloneHologram& Plan, UClass* WireClass,
    UFGCircuitConnectionComponent* From, UFGCircuitConnectionComponent* To)
{
    auto Operation = ExecutingNativeBuildInput;
    auto Invocation = ExecutingNativeWiringInvocation;
    if (!Operation || !Invocation || !CanExecuteOwnedInputWiring(Invocation->GetOperationId())) return false;
    auto Reject = [&]() { Operation->bPowerFailed = true; return false; };
    if (Plan.Role != TEXT("wire_cost") || Plan.HologramId.IsEmpty() || Operation->PowerWires.Contains(Plan.HologramId)
        || !Operation->PowerQuotes.Contains(Plan.HologramId)
        || !IsValid(From) || !IsValid(To) || Plan.PowerFrom.Connector != From->GetName() || Plan.PowerTo.Connector != To->GetName()
        || !Plan.bHasSplineData || Plan.SplineData.Points.Num() < 2
        || !From->GetComponentLocation().Equals(Plan.SplineData.Points[0].World.ToFVector(), 0.1)
        || !To->GetComponentLocation().Equals(Plan.SplineData.Points.Last().World.ToFVector(), 0.1)) return Reject();
    auto ResolveOwner = [&](const FString& Role) -> AActor*
    {
        if (Role.StartsWith(TEXT("source:")))
        {
            const auto* Key = Operation->SourceBindings.Find(Role.Mid(7));
            return Key ? Cast<AActor>(Key->ResolveObjectPtr()) : nullptr;
        }
        const auto* Identity = Invocation->Plan->GetBindings().Find(Role);
        return Identity ? Cast<AActor>(Identity->ResolveObjectPtr()) : nullptr;
    };
    if (From->GetOwner() != ResolveOwner(Plan.PowerFrom.Target) || To->GetOwner() != ResolveOwner(Plan.PowerTo.Target)) return Reject();
    auto* Owner = Subsystem->GetConstructionCoordinator();
    for (auto* Port : {From, To})
    {
        if (Port->GetWorld() != Owner->GetWorld() || !Owner->AdoptActor(Operation->Key, Port->GetOwner())) return Reject();
        const FObjectKey Key(Port);
        if (Operation->PowerPorts.Contains(Key)) continue;
        FSFExtendNativeInputOperation::FPowerPortReceipt Receipt;
        Receipt.Port = Port;
        TArray<AFGBuildableWire*> Wires; Port->GetWires(Wires);
        for (auto* Wire : Wires)
        {
            if (!IsValid(Wire) || Wire->IsActorBeingDestroyed() || Receipt.OriginalWires.Contains(FObjectKey(Wire))
                || (Wire->GetConnection(0) != Port && Wire->GetConnection(1) != Port)
                || !Owner->AdoptActor(Operation->Key, Wire)) return Reject();
            Receipt.OriginalWires.Add(FObjectKey(Wire));
        }
        Operation->PowerPorts.Add(Key, MoveTemp(Receipt));
    }
    auto* Inventory = Operation->PayerInventory.Get(); auto* Storage = Operation->PayerStorage.Get();
    if (!Inventory || !Storage) return Reject();
    TStrongObjectPtr<UFGInventoryComponent> InventoryPin(Inventory);
    TStrongObjectPtr<AFGCentralStorageSubsystem> StoragePin(Storage);
    TMap<TSubclassOf<UFGItemDescriptor>, int64> Before;
    for (const auto& Entry : Operation->QuotedCost)
        Before.Add(Entry.Key, int64(Inventory->GetNumItems(Entry.Key)) + Storage->GetNumItemsFromCentralStorage(Entry.Key));
    FSFExtendNativeInputOperation::FPowerWireReceipt Receipt;
    Receipt.From = From; Receipt.To = To;
    // Claim the exact priced role before invoking the existing native connector.
    Operation->PowerWires.Add(Plan.HologramId, MoveTemp(Receipt));
    AFGBuildableWire* Wire = nullptr;
    const bool bConnected = SFExtendPowerConnections::Connect(Owner->GetWorld(), WireClass, From, To, &Wire);
    // Native callbacks can re-enter services; do not retain a TMap entry reference across them.
    auto* Attempt = Operation->PowerWires.Find(Plan.HologramId);
    if (!Attempt) return Reject();
    Attempt->bConnected = bConnected; Attempt->Wire = Wire;
    if (Wire) Attempt->AttemptedIdentity = FObjectKey(Wire);
    if (!bConnected || !IsValid(Wire) || Wire->IsActorBeingDestroyed() || !IsValid(Inventory) || !IsValid(Storage)
        || Storage->IsActorBeingDestroyed()) return Reject();
    Operation->AuxiliaryOutputs.Add(FObjectKey(Wire));
    if (!Owner->AdoptActor(Operation->Key, Wire)) return Reject();
    for (const auto& Entry : Before)
        if (int64(Inventory->GetNumItems(Entry.Key)) + Storage->GetNumItemsFromCentralStorage(Entry.Key) != Entry.Value) return Reject();
    return true;
}

bool USFExtendService::ValidateOwnedPowerWiring(const TSharedPtr<FSFExtendNativeInputOperation>& Operation) const
{
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Owner || (!Owner->IsInMutationPhase() && !Owner->IsInObservationPhase()) || !Operation || Operation->bPowerFailed
        || !Operation->Wiring || !Operation->Wiring->Plan) return false;
    TSet<FString> ExpectedRoles;
    auto Include = [&](const FSFCloneTopology& Topology)
    { for (const auto& Child : Topology.ChildHolograms) if (Child.Role == TEXT("wire_cost")) ExpectedRoles.Add(Child.HologramId); };
    Include(Operation->Wiring->Plan->GetTopology());
    for (const auto& Clone : Operation->Wiring->ScaledPlans) if (Clone.CloneTopology) Include(*Clone.CloneTopology);
    if (ExpectedRoles.Num() != Operation->PowerWires.Num() || Operation->AuxiliaryOutputs.Num() != ExpectedRoles.Num()) return false;
    for (const auto& Role : ExpectedRoles)
    {
        const auto* Receipt = Operation->PowerWires.Find(Role);
        auto* Wire = Receipt ? Receipt->Wire.Get() : nullptr;
        auto* From = Receipt ? Receipt->From.Get() : nullptr; auto* To = Receipt ? Receipt->To.Get() : nullptr;
        if (!Receipt || !Receipt->bConnected || !IsValid(Wire) || Wire->IsActorBeingDestroyed() || !Wire->HasAuthority()
            || Wire->GetWorld() != Owner->GetWorld() || !IsValid(From) || !IsValid(To)
            || Wire->GetConnection(0) != From || Wire->GetConnection(1) != To || !Operation->AuxiliaryOutputs.Contains(FObjectKey(Wire))) return false;
        const double Limit = SFExtendPowerConnections::MaxWireLength(Wire, From, To);
        const double Distance = FVector::Dist(From->GetComponentLocation(), To->GetComponentLocation());
        if (!FMath::IsFinite(Limit) || Limit <= 0 || !FMath::IsFinite(Distance) || Distance > Limit) return false;
    }
    for (const auto& Entry : Operation->PowerPorts)
    {
        auto* Port = Entry.Value.Port.Get();
        if (!IsValid(Port) || !IsValid(Port->GetOwner()) || Port->GetOwner()->IsActorBeingDestroyed()
            || Port->GetWorld() != Owner->GetWorld() || Port->GetNumConnections() > Port->GetMaxNumConnections()) return false;
        TSet<FObjectKey> Expected = Entry.Value.OriginalWires;
        for (const auto& Wire : Operation->PowerWires)
            if (Wire.Value.From.Get() == Port || Wire.Value.To.Get() == Port) Expected.Add(Wire.Value.AttemptedIdentity);
        TArray<AFGBuildableWire*> Actual; Port->GetWires(Actual);
        if (Actual.Num() != Expected.Num() || Port->GetNumConnections() != Actual.Num()) return false;
        TSet<FObjectKey> Seen;
        for (auto* Wire : Actual)
        {
            if (!IsValid(Wire) || Wire->IsActorBeingDestroyed() || !Expected.Contains(FObjectKey(Wire)) || Seen.Contains(FObjectKey(Wire))) return false;
            Seen.Add(FObjectKey(Wire));
        }
    }
    return true;
}

bool USFExtendService::PrepareOwnedLiftAttachments(AFGHologram* Lift)
{
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    auto Invocation = NativeWiringInvocation;
    if (!IsExecutingOwnedNativeInput() || !Owner || !Owner->IsInMutationPhase() || !IsValid(Lift)
        || !Lift->IsA<AFGConveyorLiftHologram>() || !Invocation || !Invocation->Plan) return false;
    const auto* Data = USFHologramDataRegistry::GetData(Lift);
    // A native player parent retains its own native placement attachments.
    // Captured clone children must use only this construction's role bindings.
    if (!Data || Data->JsonCloneId.IsEmpty()) return ExecutingNativeBuildInput->Root.Get() == Lift;
    const FSFCloneHologram* Planned = nullptr;
    bool bAmbiguous = false;
    auto Find = [&](const FSFCloneTopology& Topology)
    {
        for (const auto& Child : Topology.ChildHolograms)
            if (Child.HologramId == Data->JsonCloneId)
            { if (Planned) bAmbiguous = true; else Planned = &Child; }
    };
    Find(Invocation->Plan->GetTopology());
    for (const auto& Clone : Invocation->ScaledPlans) if (Clone.CloneTopology) Find(*Clone.CloneTopology);
    if (!Planned || bAmbiguous || !Planned->bHasLiftData || !Lift->GetBuildClass()
        || Lift->GetBuildClass()->GetName() != Planned->BuildClass || Planned->LiftData.PassthroughCloneIds.Num() > 2) return false;
    // The clone registry is a description, not native placement state. Apply both
    // height sign/top rotation and reverse flow before native ConfigureActor reads them.
    auto* Clone = Cast<ASFConveyorLiftHologram>(Lift);
    if (!Clone || !Clone->ApplyCapturedNativeState(Planned->LiftData.TopTransform.ToFTransform(),
        Planned->LiftData.bIsReversed)) return false;
    AFGBuildablePassthrough* Holes[2] = {nullptr, nullptr};
    for (int32 Index = 0; Index < Planned->LiftData.PassthroughCloneIds.Num(); ++Index)
    {
        const auto& Role = Planned->LiftData.PassthroughCloneIds[Index];
        if (Role.IsEmpty()) continue;
        const auto* Identity = Invocation->Plan->GetBindings().Find(Role);
        auto* Hole = Identity ? Cast<AFGBuildablePassthrough>(Identity->ResolveObjectPtr()) : nullptr;
        if (!IsValid(Hole) || !Hole->HasActorBegunPlay() || Hole->IsActorBeingDestroyed()
            || Hole->GetWorld() != Owner->GetWorld() || !Hole->HasAuthority()
            || Hole->GetClass()->GetName() != TEXT("Build_FoundationPassthrough_Lift_C")
            || !Owner->AdoptActor(ExecutingNativeBuildInput->Key, Hole)) return false;
        // Native ConfigureComponents chooses the actual face. Existing occupants
        // must belong to this same construction, never to a nearby/source lift.
        for (auto* Port : {Hole->GetBottomSnappedConnection<UFGFactoryConnectionComponent>(), Hole->GetTopSnappedConnection<UFGFactoryConnectionComponent>()})
        {
            if (!Port) continue;
            if (!IsValid(Port) || !IsValid(Port->GetOwner())) return false;
            bool bBound = false;
            for (const auto& Pair : Invocation->Plan->GetBindings()) bBound |= Pair.Value == FObjectKey(Port->GetOwner());
            if (!bBound) return false;
        }
        Holes[Index] = Hole;
    }
    auto* Slots = FindFProperty<FArrayProperty>(AFGConveyorLiftHologram::StaticClass(), TEXT("mSnappedPassthroughs"));
    auto* Object = Slots ? CastField<FObjectPropertyBase>(Slots->Inner) : nullptr;
    if (!Object) return false;
    FScriptArrayHelper Array(Slots, Slots->ContainerPtrToValuePtr<void>(Lift));
    Array.Resize(2);
    for (int32 Index = 0; Index < 2; ++Index) Object->SetObjectPropertyValue(Array.GetRawPtr(Index), Holes[Index]);
    return true;
}

bool USFExtendService::ExecuteOwnedConveyorWiring()
{
    auto Operation = ExecutingNativeBuildInput;
    auto Invocation = ExecutingNativeWiringInvocation;
    auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr;
    if (!Operation || !Invocation || !Invocation->Plan || !Owner
        || !CanExecuteOwnedInputWiring(Invocation->GetOperationId())) return false;
    auto Reject = [&](const TCHAR* Reason)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("Extend/Restore conveyor wiring unsettled: operation=%s reason=%s; returned construction remains owned, no legacy repair attempted."),
            *Operation->Id.ToString(), Reason);
        return false;
    };
    TArray<FSFConveyorConnectionPair> Edges;
    TMap<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*> IntendedPeers;
    TArray<TWeakObjectPtr<AFGBuildableConveyorBase>> Seeds;
    Seeds.Append(Operation->SourceSeeds);
    auto ResolveActor = [&](const FString& Role) -> AActor*
    {
        if (Role.StartsWith(TEXT("source:")))
        {
            const FObjectKey* Key = Operation->SourceBindings.Find(Role.Mid(7));
            return Key ? Cast<AActor>(Key->ResolveObjectPtr()) : nullptr;
        }
        const auto* Identity = Invocation->Plan->GetBindings().Find(Role);
        return Identity ? Cast<AActor>(Identity->ResolveObjectPtr()) : nullptr;
    };
    auto FindExactPort = [&](AActor* Actor, const FString& Name) -> UFGFactoryConnectionComponent*
    {
        if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || !Actor->HasAuthority() || Actor->GetWorld() != Owner->GetWorld()) return nullptr;
        TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
        UFGFactoryConnectionComponent* Match = nullptr;
        for (auto* Port : Ports)
            if (IsValid(Port) && Port->GetName() == Name)
            { if (Match) return nullptr; Match = Port; }
        return Match;
    };
    auto IncludeTopology = [&](const FSFCloneTopology& Topology)
    {
        for (const auto& Child : Topology.ChildHolograms)
        {
            if (Child.bPreviewOnly || !Child.bConstructible) continue;
            const bool bConveyor = Child.Role == TEXT("belt_segment") || Child.Role == TEXT("lift_segment")
                || (Child.Role == TEXT("lane_segment") && (Child.LaneSegmentType == TEXT("belt") || Child.LaneSegmentType == TEXT("lift")));
            if (!bConveyor) continue;
            AActor* Actor = ResolveActor(Child.HologramId);
            auto* Conveyor = Cast<AFGBuildableConveyorBase>(Actor);
            if (!IsValid(Conveyor)) return false;
            Seeds.AddUnique(Conveyor);
            const FSFConnectionRef* Endpoints[] = { &Child.CloneConnections.ConveyorAny0, &Child.CloneConnections.ConveyorAny1 };
            const TCHAR* Names[] = { TEXT("ConveyorAny0"), TEXT("ConveyorAny1") };
            for (int32 Index = 0; Index < 2; ++Index)
            {
                const auto& Endpoint = *Endpoints[Index];
                if (Endpoint.Target.IsEmpty() || Endpoint.Target == TEXT("external")) continue;
                auto* A = FindExactPort(Actor, Names[Index]);
                auto* B = FindExactPort(ResolveActor(Endpoint.Target), Endpoint.Connector);
                if (!A || !B || A == B || A->GetOwner() == B->GetOwner()) return false;
                if ((IntendedPeers.Contains(A) && IntendedPeers[A] != B) || (IntendedPeers.Contains(B) && IntendedPeers[B] != A)) return false;
                // Idempotence requires both exact reciprocal peers; "connected"
                // to an unrelated port is a contradiction, never a successful skip.
                const bool bExact = A->GetConnection() == B && B->GetConnection() == A && A->IsConnected() && B->IsConnected();
                if (!bExact && (A->IsConnected() || B->IsConnected() || A->GetConnection() || B->GetConnection() || !A->CanConnectTo(B))) return false;
                if (!IntendedPeers.Contains(A)) Edges.Add({A, B});
                IntendedPeers.Add(A, B); IntendedPeers.Add(B, A);
                for (auto* EndpointActor : {A->GetOwner(), B->GetOwner()})
                {
                    if (!Owner->AdoptActor(Operation->Key, EndpointActor)) return false;
                    if (auto* Member = Cast<AFGBuildableConveyorBase>(EndpointActor)) Seeds.AddUnique(Member);
                }
            }
        }
        return true;
    };
    if (!IncludeTopology(Invocation->Plan->GetTopology())) return Reject(TEXT("missing_conflicting_or_incompatible_exact_endpoint"));
    for (const auto& Clone : Invocation->ScaledPlans)
        if (Clone.CloneTopology && !IncludeTopology(*Clone.CloneTopology)) return Reject(TEXT("scaled_endpoint_plan_invalid"));
    // Include all returned conveyors, including native-connected ones absent from
    // extra wiring edges. BeginPlay owns their initial registration.
    for (const auto& Identity : Invocation->ActualOutputs)
        if (auto* Conveyor = Cast<AFGBuildableConveyorBase>(Identity.ResolveObjectPtr())) Seeds.AddUnique(Conveyor);
    if (Seeds.IsEmpty())
    {
        Operation->bConveyorWiringVerified = Operation->bCargoVerified = true;
        return true;
    }
    FSFConveyorCohort After;
    FSFConveyorCohort::FCargoSnapshot AfterCargo;
    FString Reason;
    bool bEffectsStarted = false;
    if (!FSFConveyorConnectionMutation::Apply(*Owner, Operation->Key, Seeds, Edges, After, AfterCargo, bEffectsStarted, Reason))
        return Reject(*Reason);
    auto SameCargo = [](const auto& Expected, const auto& Actual)
    {
        if (Expected.Num() != Actual.Num()) return false;
        for (int32 Index = 0; Index < Expected.Num(); ++Index)
            if (!Expected[Index].SameValue(Actual[Index]) || !FMath::IsNearlyEqual(Expected[Index].Offset, Actual[Index].Offset, 0.01f)) return false;
        return true;
    };
    for (const auto& Pair : Operation->BeforeCargo)
    {
        const auto* Actual = AfterCargo.Find(Pair.Key);
        if (!Actual || !SameCargo(Pair.Value, *Actual)) return Reject(TEXT("source_cargo_changed_during_construction"));
    }
    if (!FSFConveyorCohortValidator::VerifyBoundaryInventories(*Owner, Operation->Before, Reason)) return Reject(*Reason);
    for (const auto& Pair : AfterCargo)
        if (Invocation->ActualOutputs.Contains(Pair.Key) && !Pair.Value.IsEmpty()) return Reject(TEXT("new_conveyor_inherited_cargo"));
    // Freeze every affected actual port, not only added edges, for the post-native oracle.
    TSet<UFGFactoryConnectionComponent*> SeenPorts;
    for (const auto& Actor : After.AffectedActors)
    {
        if (!Actor.IsValid()) return Reject(TEXT("affected_actor_expired"));
        TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
        for (auto* Port : Ports)
        {
            if (!IsValid(Port) || SeenPorts.Contains(Port)) continue;
            auto* Peer = Port->GetConnection();
            if (Port->IsConnected() != (Peer != nullptr) || (Peer && Peer->GetConnection() != Port)) return Reject(TEXT("nonreciprocal_result"));
            if (auto** Intended = IntendedPeers.Find(Port); Intended && *Intended != Peer) return Reject(TEXT("intended_edge_missing"));
            SeenPorts.Add(Port); Operation->ExpectedEdges.Add({Port, Peer});
        }
    }
    for (const auto& Pair : After.Members)
        Operation->Remaining.AddUnique(Cast<AFGBuildableConveyorBase>(Pair.Key.ResolveObjectPtr()));
    Operation->bConveyorWiringVerified = Operation->bCargoVerified = true;
    return true;
}

void USFExtendService::CaptureOwnedInputOutputs(const TSharedPtr<FSFExtendWiringInvocation>& Invocation)
{
    if (!Invocation || !CanExecuteOwnedInputWiring(Invocation->GetOperationId())) return;
    ExecutingNativeBuildInput->Wiring = Invocation;
    auto* Owner = Subsystem->GetConstructionCoordinator();
    for (const FObjectKey& Key : Invocation->ActualOutputs)
        if (!Owner->AdoptActor(ExecutingNativeBuildInput->Key, Cast<AActor>(Key.ResolveObjectPtr()))) ExecutingNativeBuildInput->bAdoptionFailed = true;
}

void USFExtendService::ShutdownNativeBuildInputs()
{
    if (auto* Owner = Subsystem.IsValid() ? Subsystem->GetConstructionCoordinator() : nullptr)
        for (const auto& Pending : NativeBuildInputs) Owner->RequestCancellation(Pending.Value->Key);
    NativeBuildInputs.Empty();
}
