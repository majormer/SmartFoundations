// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "Core/Construction/SFConstructionOperationTypes.h"

/**
 * Value record retained by an authoritative operation owner, independently of preview state.
 * This enforces settlement bookkeeping; it supplies neither a worker fence nor native rollback.
 * The owner must establish authority, assign a world-generation token, own cohort conflicts,
 * and obtain actual evidence before verifying obligations. Effect keys name exact operation roles.
 */
class FSFConstructionOperationRecord
{
public:
    FSFConstructionOperationRecord(const FGuid& InRequestId, const FGuid& InWorldGeneration)
        : RequestId(InRequestId), WorldGeneration(InWorldGeneration) {}

    const FGuid& GetRequestId() const { return RequestId; }
    ESFConstructionOperationState GetState() const { return State; }
    bool IsSettled() const { return bSettled; }
    bool IsCancellationRequested() const { return bCancelRequested; }

    /** An applied effect can be known while its resulting obligations are not.
     * Preserve the immutable effect outcome and require reconciliation explicitly. */
    bool RequireReconciliation(const FGuid& CurrentWorld)
    {
        if (!CanUpdate(CurrentWorld)) return false;
        bReconciliationRequired = true;
        return RequestCancellation(CurrentWorld);
    }
    /** Continue an already claimed, not-yet-invoked phase. Uncertain attempts
     * must be reconciled, never automatically retried. This does not grant a fence. */
    bool CanContinueEffect(const FGuid& CurrentWorld, FName EffectKey) const
    {
        const auto* Outcome = Effects.Find(EffectKey);
        return CanUpdate(CurrentWorld) && !bCancelRequested && Outcome
            && *Outcome == ESFConstructionEffectOutcome::Pending;
    }

    bool BeginMutation(const FGuid& CurrentWorld)
    {
        if (!CanUpdate(CurrentWorld) || State != ESFConstructionOperationState::Admitted) return false;
        State = ESFConstructionOperationState::Materializing;
        return true;
    }

    /** Claim before invoking native work. A pending/failed attempt is never silently retried. */
    bool ClaimEffect(const FGuid& CurrentWorld, FName EffectKey)
    {
        if (!CanUpdate(CurrentWorld) || bMutationSealed || bCancelRequested
            || State != ESFConstructionOperationState::Materializing
            || EffectKey.IsNone() || Effects.Contains(EffectKey)) return false;
        Effects.Add(EffectKey, ESFConstructionEffectOutcome::Pending);
        return true;
    }

    /** Resolve an outstanding attempt from observed evidence; known outcomes cannot be rewritten. */
    bool ObserveEffect(const FGuid& CurrentWorld, FName EffectKey, ESFConstructionEffectOutcome Outcome)
    {
        if (!CanUpdate(CurrentWorld) || Outcome == ESFConstructionEffectOutcome::Pending) return false;
        ESFConstructionEffectOutcome* Existing = Effects.Find(EffectKey);
        if (!Existing || (*Existing != ESFConstructionEffectOutcome::Pending
            && *Existing != ESFConstructionEffectOutcome::Uncertain)) return false;
        *Existing = Outcome;
        if (bCancelRequested) UpdateCancellationState();
        return true;
    }

    bool SealMutation(const FGuid& CurrentWorld)
    {
        if (!CanUpdate(CurrentWorld) || bMutationSealed
            || State != ESFConstructionOperationState::Materializing) return false;
        bMutationSealed = true;
        State = ESFConstructionOperationState::AwaitingNativeSettlement;
        return true;
    }

    bool VerifyObligation(const FGuid& CurrentWorld, ESFConstructionObligation Obligation)
    {
        const uint8 Index = static_cast<uint8>(Obligation);
        if (!CanUpdate(CurrentWorld) || !bMutationSealed || HasUnresolvedEffects()
            || Index >= static_cast<uint8>(ESFConstructionObligation::Count)) return false;
        if (Obligation == ESFConstructionObligation::Resources && !HasNativeProof()) return false;
        VerifiedObligations |= 1u << Index;
        if (!bCancelRequested && HasNativeProof()) State = ESFConstructionOperationState::SettlingResources;
        return true;
    }

    bool Complete(const FGuid& CurrentWorld)
    {
        if (!CanUpdate(CurrentWorld) || bCancelRequested || !bMutationSealed
            || HasUnresolvedEffects() || !HasAllProof()) return false;
        State = ESFConstructionOperationState::Completed;
        bSettled = true;
        return true;
    }

    /** Stops new effects. Already invoked work remains owned until it is resolved and settled. */
    bool RequestCancellation(const FGuid& CurrentWorld)
    {
        if (!CanUpdate(CurrentWorld)) return false;
        bCancelRequested = true;
        bMutationSealed = true;
        UpdateCancellationState();
        return true;
    }

    bool SettleCancellation(const FGuid& CurrentWorld)
    {
        if (!CanUpdate(CurrentWorld) || !bCancelRequested || HasUnresolvedEffects()) return false;
        if (HasAppliedEffects() && !HasAllProof()) return false;
        UpdateCancellationState();
        bSettled = true;
        return true;
    }

    /** Finalize the lifetime of an owner whose world is ending, never its economic
     * obligations. Pending/partially proved effects become a terminal Uncertain
     * result; callers must retain that distinction when notifying their requester.
     * Only coordinator shutdown may use this path, after stopping new dispatch. */
    bool TerminateForWorldEnd(const FGuid& CurrentWorld)
    {
        if (bSettled) return CurrentWorld == WorldGeneration;
        if (!RequestCancellation(CurrentWorld)) return false;
        if (SettleCancellation(CurrentWorld)) return true;
        State = ESFConstructionOperationState::Uncertain;
        bSettled = true;
        return true;
    }

private:
    bool CanUpdate(const FGuid& CurrentWorld) const
    {
        return !bSettled && RequestId.IsValid() && WorldGeneration.IsValid() && CurrentWorld == WorldGeneration;
    }
    bool HasUnresolvedEffects() const
    {
        if (bReconciliationRequired) return true;
        for (const auto& Effect : Effects)
            if (Effect.Value == ESFConstructionEffectOutcome::Pending
                || Effect.Value == ESFConstructionEffectOutcome::Uncertain) return true;
        return false;
    }
    bool HasAppliedEffects() const
    {
        for (const auto& Effect : Effects)
            if (Effect.Value == ESFConstructionEffectOutcome::Applied) return true;
        return false;
    }
    bool HasNativeProof() const { return (VerifiedObligations & NativeProofMask) == NativeProofMask; }
    bool HasAllProof() const { return VerifiedObligations == AllProofMask; }
    void UpdateCancellationState()
    {
        State = HasUnresolvedEffects() ? ESFConstructionOperationState::Uncertain
            : HasAppliedEffects() ? ESFConstructionOperationState::PartiallyApplied
            : ESFConstructionOperationState::CancelledBeforeEffects;
    }

    static constexpr uint8 NativeProofMask = (1u << static_cast<uint8>(ESFConstructionObligation::Resources)) - 1u;
    static constexpr uint8 AllProofMask = (1u << static_cast<uint8>(ESFConstructionObligation::Count)) - 1u;
    FGuid RequestId;
    FGuid WorldGeneration;
    ESFConstructionOperationState State = ESFConstructionOperationState::Admitted;
    TMap<FName, ESFConstructionEffectOutcome> Effects;
    uint8 VerifiedObligations = 0;
    bool bMutationSealed = false;
    bool bCancelRequested = false;
    bool bSettled = false;
    bool bReconciliationRequired = false;
};
