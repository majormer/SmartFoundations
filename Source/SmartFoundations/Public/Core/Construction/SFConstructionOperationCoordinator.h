// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FGBuildableSubsystem.h"
#include "Core/Construction/SFConstructionOperationTypes.h"
#include "SFConstructionOperationCoordinator.generated.h"

class FSFConstructionOperationRecord;
struct FSFConstructionOperationCoordinatorState;

/** Shared authoritative operation owner. Native registration precedes admission, outside handler dispatch. */
UCLASS(Transient)
class SMARTFOUNDATIONS_API USFConstructionOperationCoordinator : public UObject, public IFGFactoryTickHandlerInterface
{
    GENERATED_BODY()
public:
    USFConstructionOperationCoordinator();
    virtual ~USFConstructionOperationCoordinator() override;
    bool Initialize(UWorld* InWorld);
    /** Called only by the owning subsystem's ordinary tick, outside native handler iteration. */
    void RefreshNativeBinding();
    void Shutdown();
    virtual UWorld* GetWorld() const override;

    /** Caller supplies immutable intent and its exact original actor/payer identities.
     * The callback must capture and adopt any additional native ownership closure before effects.
     * Callbacks own their captured intent and must not retain the record reference. */
    ESFConstructionAdmission Submit(const FSFConstructionOperationRequest& Request,
        TFunction<void(FSFConstructionOperationRecord&)> Mutate,
        TFunction<void(FSFConstructionOperationRecord&)> Observe,
        TFunction<void(ESFConstructionOperationState)> OnSettled);
    bool AdoptActor(const FSFConstructionOperationKey& Key, AActor* Actor);
    /** One queued continuation per pending effect; always runs in a later native
     * pre-handler, never in a timer or the observer. Does not replay construction. */
    bool QueueContinuation(const FSFConstructionOperationKey& Key, FName EffectKey,
        TFunction<void(FSFConstructionOperationRecord&)> Continue);
    bool RequestCancellation(const FSFConstructionOperationKey& Key);
    TOptional<ESFConstructionOperationState> GetOperationState(const FSFConstructionOperationKey& Key) const;
    FGuid GetWorldGeneration() const;
    bool IsInMutationPhase() const;
    bool IsInObservationPhase() const;

    virtual void PreFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime) override;
    virtual void PostFactoryTick(AFGBuildableSubsystem* Subsystem, float DeltaTime) override;

private:
    bool TryBindNativeHandler();
    bool IsExpectedCallback(AFGBuildableSubsystem* Subsystem) const;
    void ReleaseSettled(const FSFConstructionOperationKey& Key);
    void FinalizeStoppedWork();

    UPROPERTY(Transient)
    TWeakObjectPtr<UWorld> World;
    UPROPERTY(Transient)
    TWeakObjectPtr<AFGBuildableSubsystem> NativeSubsystem;
    TSharedPtr<FSFConstructionOperationCoordinatorState> State;
    bool bShutdownQueued = false;
};
