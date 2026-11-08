// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Buildables/FGBuildableFactory.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGPipelineHologram.h"
#include "FGSplineComponent.h"
#include "Components/SplineComponent.h"
#include "UObject/UnrealType.h"
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
            if (auto* Pipe = Cast<AFGPipelineHologram>(Child))
            {
                const FArrayProperty* Property = FindFProperty<FArrayProperty>(Pipe->GetClass(), TEXT("mSplineData"));
                const FStructProperty* Element = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
                if (!Element || Element->Struct != FSplinePointData::StaticStruct()
                    || Element->GetElementSize() != sizeof(FSplinePointData)) return nullptr;
                const auto* Points = Property->ContainerPtrToValuePtr<TArray<FSplinePointData>>(Pipe);
                if (!Points || Points->Num() < 2) return nullptr;
                const auto* SplineProperty = FindFProperty<FObjectPropertyBase>(Pipe->GetClass(), TEXT("mSplineComponent"));
                const auto* SplineComponent = SplineProperty
                    ? Cast<USplineComponent>(SplineProperty->GetObjectPropertyValue_InContainer(Pipe)) : nullptr;
                if (!SplineComponent) return nullptr;
                FSFExtendPreparedPipeSpline Prepared;
                Prepared.LocalToWorld = SplineComponent->GetComponentTransform();
                for (const auto& Point : *Points)
                {
                    FSFSplinePoint Value;
                    Value.Local = FSFVec3(Point.Location);
                    Value.World = FSFVec3(Prepared.LocalToWorld.TransformPosition(Point.Location));
                    Value.ArriveTangent = FSFVec3(Point.ArriveTangent);
                    Value.LeaveTangent = FSFVec3(Point.LeaveTangent);
                    Prepared.Spline.Points.Add(Value);
                }
                Capture.PreparedPipeSplines.Add(Data->JsonCloneId, MoveTemp(Prepared));
            }
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
    if (!Invocation) return false;
    Invocation->ClearRefusal();
    if (!Invocation || !NativeWiringInvocation || !IsValid(Parent) || !Parent->HasAuthority()
        || !Subsystem.IsValid() || Parent->GetWorld() != Subsystem->GetWorld()
        || Invocation != NativeWiringInvocation || Invocation->HasUnverifiedInvocation()
        || !Invocation->bRequiresOwnedNativePhase) return Invocation->Refuse(TEXT("wiring_context_or_parent_unavailable"));
    if (Invocation->bRequiresOwnedNativePhase && !CanExecuteOwnedInputWiring(Invocation->GetOperationId()))
        return Invocation->Refuse(TEXT("owned_mutation_phase_unavailable"));
    // The Construct hook reaches here before native payment. Leave the claim
    // untouched; EndNativeWiringCapture invokes us after the native continuation.
    if (Invocation->bRequiresOwnedNativePhase && !HasObservedOwnedNativePayment())
        return Invocation->Refuse(TEXT("native_payment_not_verified"));
    if (Invocation->Plan)
    {
        for (const auto& Source : Invocation->SourceBindings)
        {
            const AActor* Actor = Cast<AActor>(Source.Value.ResolveObjectPtr());
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != Parent->GetWorld())
                return Invocation->Refuse(TEXT("source_actor_unavailable"));
        }
        if (!Invocation->Plan->Seal(Invocation->ActualOutputs))
        {
            UE_LOG(LogSmartFoundations, Warning,
                TEXT("Extend/Restore wiring rejected: operation=%s requiredRoles=%d boundRoles=%d actualOutputs=%d; native construction effects remain unverified."),
                *Invocation->GetOperationId().ToString(), Invocation->Plan->GetRequiredRoles().Num(),
                Invocation->Plan->GetBindings().Num(), Invocation->ActualOutputs.Num());
            return Invocation->Refuse(TEXT("required_output_roles_not_sealed"));
        }
        for (const auto& Binding : Invocation->Plan->GetBindings())
        {
            if (!Invocation->ActualOutputs.Contains(Binding.Value)) continue;
            AActor* Actor = Cast<AActor>(Binding.Value.ResolveObjectPtr());
            const TWeakObjectPtr<UClass>* Expected = Invocation->ExpectedClasses.Find(Binding.Key);
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() || Actor->GetWorld() != Parent->GetWorld()
                || !Actor->HasAuthority() || !Expected || Expected->Get() != Actor->GetClass())
                return Invocation->Refuse(TEXT("output_role_class_or_lifetime_mismatch"));
            Invocation->ActorView.Add(Binding.Key, Actor);
        }
    }
    else if (!HasPendingPostBuildWiring()) return Invocation->Refuse(TEXT("wiring_plan_unavailable"));
    if (!Invocation->TryClaim(NativeWiringInvocation->GetOperationId(), GetWiringWorldGeneration(Subsystem.Get()), FObjectKey(Parent)))
        return Invocation->Refuse(TEXT("wiring_invocation_claim_refused"));
    if (Invocation->bRequiresOwnedNativePhase && !ClaimOwnedInputWiring())
        return Invocation->Refuse(TEXT("wiring_effect_claim_refused"));
    TGuardValue<TSharedPtr<FSFExtendWiringInvocation>> Context(ExecutingNativeWiringInvocation, Invocation);
    if (Invocation->bRequiresOwnedNativePhase)
    {
        // Validate the entire conveyor plan before extra edge effects. Never let
        // legacy writers respawn the returned actors or rebuild their chains.
        if (!ApplyOwnedDistributorSettings())
        {
            ObserveOwnedInputWiring();
            return Invocation->Refuse(TEXT("distributor_settings_not_verified"));
        }
        if (!ExecuteOwnedConveyorWiring())
        {
            ObserveOwnedInputWiring();
            return Invocation->Refuse(TEXT("conveyor_connections_or_cargo_not_verified"));
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
