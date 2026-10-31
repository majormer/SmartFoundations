// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Buildables/FGBuildableFactory.h"
#include "Hologram/FGHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "Core/Construction/SFConstructionOperationCoordinator.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendWiringInvocation.h"
#include "Subsystem/SFSubsystem.h"

namespace
{
    FGuid GetWiringWorldGeneration(const USFSubsystem* Subsystem)
    {
        const USFConstructionOperationCoordinator* Coordinator = Subsystem ? Subsystem->GetConstructionCoordinator() : nullptr;
        return Coordinator ? Coordinator->GetWorldGeneration() : FGuid();
    }
}

TSharedPtr<FSFExtendWiringInvocation> USFExtendService::BeginNativeWiringCapture(AFGHologram* Root, FNetConstructionID NativeId)
{
    if (!IsValid(Root) || !Root->HasAuthority() || Root->GetParentHologram() || bNativeWiringCaptureActive
        || !Subsystem.IsValid() || Root->GetWorld() != Subsystem->GetWorld()) return nullptr;
    const FGuid WorldGeneration = GetWiringWorldGeneration(Subsystem.Get());
    if (!WorldGeneration.IsValid()) return nullptr;
    const FGuid InputId = GetExecutingNativeInputId(Root);
    if (!InputId.IsValid()) return nullptr;
    NativeWiringInvocation = MakeShared<FSFExtendWiringInvocation>(WorldGeneration, FObjectKey(Root), NativeId, InputId);
    NativeWiringInvocation->bRequiresOwnedNativePhase = InputId.IsValid();
    if (StoredCloneTopology.IsValid())
    {
        auto& Capture = *NativeWiringInvocation;
        Capture.PreviewTopology = StoredCloneTopology;
        Capture.Plan = MakeShared<FSFExtendWiringPlan>(*StoredCloneTopology);
        for (const FSFScaledExtendClone& Clone : ScaledExtendClones)
        {
            if (!Clone.CloneTopology.IsValid()) continue;
            FSFScaledExtendClone Value;
            Value.GridX = Clone.GridX; Value.GridY = Clone.GridY; Value.GridZ = Clone.GridZ;
            Value.bIsSeed = Clone.bIsSeed;
            Value.CloneTopology = MakeShared<FSFCloneTopology>(*Clone.CloneTopology);
            Capture.Plan->IncludeRequirements(*Value.CloneTopology);
            Capture.ScaledPlans.Add(MoveTemp(Value));
        }
        if (!Capture.Plan->IsValid()) return nullptr;
        Capture.ExpectedClasses.Add(TEXT("parent"), Root->GetBuildClass().Get());
        TArray<AFGHologram*> Pending = Root->GetHologramChildren();
        TSet<AFGHologram*> Visited;
        while (!Pending.IsEmpty())
        {
            AFGHologram* Child = Pending.Pop(EAllowShrinking::No);
            if (!IsValid(Child) || Visited.Contains(Child)) continue;
            Visited.Add(Child);
            Pending.Append(Child->GetHologramChildren());
            const FSFHologramData* Data = USFHologramDataRegistry::GetData(Child);
            if (!Data || Data->JsonCloneId.IsEmpty()) continue;
            if (Capture.ExpectedClasses.Contains(Data->JsonCloneId)) return nullptr;
            Capture.ExpectedClasses.Add(Data->JsonCloneId, Child->GetBuildClass().Get());
        }
        for (const FString& Role : Capture.Plan->GetRequiredRoles())
            if (!Capture.ExpectedClasses.Contains(Role)) return nullptr;

        const auto* AdmittedSources = GetExecutingNativeInputSources();
        if (!AdmittedSources) return nullptr;
        // Both authority-local and remote inputs bind live graph provenance before
        // effects. Historical preset IDs never authorize a new world-name lookup.
        Capture.SourceBindings = *AdmittedSources;
        for (const FString& Name : Capture.Plan->GetRequiredSources())
            if (!Capture.SourceBindings.Contains(Name)) return nullptr;
    }
    bNativeWiringCaptureActive = true;
    CaptureOwnedInputOutputs(NativeWiringInvocation);
    return NativeWiringInvocation;
}

void USFExtendService::EndNativeWiringCapture(const TSharedPtr<FSFExtendWiringInvocation>& Invocation)
{
    if (Invocation && Invocation == NativeWiringInvocation)
    {
        if (Invocation->bRequiresOwnedNativePhase && CanExecuteOwnedInputWiring(Invocation->GetOperationId())
            && !Invocation->HasUnverifiedInvocation())
            RunNativePostBuildWiring(Invocation, Cast<AFGBuildableFactory>(Invocation->GetParentIdentity().ResolveObjectPtr()));
        bNativeWiringCaptureActive = false;
    }
}

TSharedPtr<FSFExtendWiringInvocation> USFExtendService::CaptureNativeWiringInvocation() const
{
    return bNativeWiringCaptureActive ? NativeWiringInvocation : nullptr;
}

void USFExtendService::BindNativeWiringParent(AFGHologram* Root, FNetConstructionID NativeId, AActor* Parent, const TArray<AActor*>& Children)
{
    if (bNativeWiringCaptureActive && NativeWiringInvocation && IsValid(Root) && IsValid(Parent)
        && Root->HasAuthority() && Parent->HasAuthority() && Root->GetWorld() == Parent->GetWorld())
    {
        if (!NativeWiringInvocation->BindParent(GetWiringWorldGeneration(Subsystem.Get()), FObjectKey(Root), NativeId, FObjectKey(Parent))) return;
        NativeWiringInvocation->ActualOutputs.Add(FObjectKey(Parent));
        for (AActor* Child : Children)
            if (IsValid(Child)) NativeWiringInvocation->ActualOutputs.Add(FObjectKey(Child));
        if (NativeWiringInvocation->Plan) NativeWiringInvocation->Plan->RegisterRole(TEXT("parent"), FObjectKey(Parent));
        CaptureOwnedInputOutputs(NativeWiringInvocation);
    }
}

bool USFExtendService::RunNativePostBuildWiring(const TSharedPtr<FSFExtendWiringInvocation>& Invocation, AFGBuildableFactory* Parent)
{
    if (!Invocation || !NativeWiringInvocation || !IsValid(Parent) || !Parent->HasAuthority()
        || !Subsystem.IsValid() || Parent->GetWorld() != Subsystem->GetWorld()
        || Invocation != NativeWiringInvocation || Invocation->HasUnverifiedInvocation()
        || !Invocation->bRequiresOwnedNativePhase) return false;
    if (Invocation->bRequiresOwnedNativePhase && !CanExecuteOwnedInputWiring(Invocation->GetOperationId())) return false;
    // The Construct hook reaches here before native payment. Leave the claim
    // untouched; EndNativeWiringCapture invokes us after the native continuation.
    if (Invocation->bRequiresOwnedNativePhase && !HasObservedOwnedNativePayment()) return false;
    if (Invocation->Plan)
    {
        for (const auto& Source : Invocation->SourceBindings)
        {
            const AActor* Actor = Cast<AActor>(Source.Value.ResolveObjectPtr());
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != Parent->GetWorld()) return false;
        }
        if (!Invocation->Plan->Seal(Invocation->ActualOutputs))
        {
            UE_LOG(LogSmartFoundations, Warning,
                TEXT("Extend/Restore wiring rejected: operation=%s requiredRoles=%d boundRoles=%d actualOutputs=%d; native construction effects remain unverified."),
                *Invocation->GetOperationId().ToString(), Invocation->Plan->GetRequiredRoles().Num(),
                Invocation->Plan->GetBindings().Num(), Invocation->ActualOutputs.Num());
            return false;
        }
        for (const auto& Binding : Invocation->Plan->GetBindings())
        {
            if (!Invocation->ActualOutputs.Contains(Binding.Value)) continue;
            AActor* Actor = Cast<AActor>(Binding.Value.ResolveObjectPtr());
            const TWeakObjectPtr<UClass>* Expected = Invocation->ExpectedClasses.Find(Binding.Key);
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != Parent->GetWorld()
                || !Actor->HasAuthority() || !Expected || Expected->Get() != Actor->GetClass()) return false;
            Invocation->ActorView.Add(Binding.Key, Actor);
        }
    }
    else if (!HasPendingPostBuildWiring()) return false;
    if (!Invocation->TryClaim(NativeWiringInvocation->GetOperationId(), GetWiringWorldGeneration(Subsystem.Get()), FObjectKey(Parent))) return false;
    if (Invocation->bRequiresOwnedNativePhase && !ClaimOwnedInputWiring()) return false;
    TGuardValue<TSharedPtr<FSFExtendWiringInvocation>> Context(ExecutingNativeWiringInvocation, Invocation);
    if (Invocation->bRequiresOwnedNativePhase)
    {
        // Validate the entire conveyor plan before extra edge effects. Never let
        // legacy writers respawn the returned actors or rebuild their chains.
        if (!ApplyOwnedDistributorSettings() || !ExecuteOwnedConveyorWiring())
        {
            ObserveOwnedInputWiring();
            return false;
        }
    }
    WireBuiltChildConnections(Parent);
    if (Invocation->bRequiresOwnedNativePhase) ObserveOwnedInputWiring();
    if (Invocation->CanRetirePreview(StoredCloneTopology))
    {
        // All post-dispatch obligations use the captured plan and output receipts.
        // Leaving these consumed preview registries populated makes the next
        // Restore arm wait forever on HasPendingPostBuildWiring. Do not destroy
        // actors, discard the owned operation, or clear a newer preview here.
        StoredCloneTopology.Reset();
        JsonSpawnedHolograms.Empty();
        JsonBuiltActors.Empty();
        PowerPoleWiringData.Empty();
        ScaledExtendClones.Empty();
    }
    UE_LOG(LogSmartFoundations, Log,
        TEXT("Extend/Restore wiring invoked: operation=%s parent=%s capturedPlan=%d roles=%d outputs=%d sources=%d; native settlement unverified."),
        *Invocation->GetOperationId().ToString(), *Parent->GetName(), Invocation->Plan.IsValid() ? 1 : 0,
        Invocation->ActorView.Num(), Invocation->ActualOutputs.Num(), Invocation->SourceBindings.Num());
    return true; // invoked, not verified or settled
}

bool USFExtendService::IsExecutingCapturedWiring() const
{
    return ExecutingNativeWiringInvocation && ExecutingNativeWiringInvocation->Plan;
}

bool USFExtendService::IsExecutingOwnedConveyorWiring() const
{
    return ExecutingNativeWiringInvocation && ExecutingNativeWiringInvocation->bRequiresOwnedNativePhase;
}

const FSFCloneTopology* USFExtendService::GetCapturedWiringTopology() const
{
    return IsExecutingCapturedWiring() ? &ExecutingNativeWiringInvocation->Plan->GetTopology() : nullptr;
}

const TMap<FString, TObjectPtr<AActor>>* USFExtendService::GetCapturedWiringActors() const
{
    return IsExecutingCapturedWiring() ? &ExecutingNativeWiringInvocation->ActorView : nullptr;
}

const TArray<FSFScaledExtendClone>* USFExtendService::GetCapturedScaledWiringPlans() const
{
    return IsExecutingCapturedWiring() ? &ExecutingNativeWiringInvocation->ScaledPlans : nullptr;
}

AFGBuildable* USFExtendService::ResolveCapturedWiringSource(const FString& Name) const
{
    if (!IsExecutingCapturedWiring()) return nullptr;
    const FObjectKey* Key = ExecutingNativeWiringInvocation->SourceBindings.Find(Name);
    return Key ? Cast<AFGBuildable>(Key->ResolveObjectPtr()) : nullptr;
}
