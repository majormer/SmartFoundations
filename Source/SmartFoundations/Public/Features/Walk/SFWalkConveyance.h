// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SFWalkConveyance.generated.h"

class AFGHologram;
class USFSubsystem;

/**
 * Smart Walking — conveyance adapter seam (#356).
 *
 * The walk core is conveyance-agnostic: it hands the adapter two consecutive anchor holograms and the
 * adapter produces (and updates) the SPANNING element between them. Belts/pipes/hyper-tubes are pole
 * conveyances (anchor + spanning belt/pipe); train tracks / vehicle paths would be snap conveyances
 * (one piece) — the seam is the same because the core only deals in anchors, never splines.
 *
 * MVP ships exactly one adapter: USFWalkBeltConveyance. See docs/Sprints/CONCEPT_SmartWalking.md.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFWalkConveyance : public UObject
{
    GENERATED_BODY()

public:
    void SetSubsystem(USFSubsystem* InSubsystem);

    /**
     * Create (if ExistingSpan is null) or update the spanning element connecting FromAnchor → ToAnchor.
     * ParentForChild is the seed hologram. With bAddChildForBuild=false (preview) the span is spawned STANDALONE;
     * with bAddChildForBuild=true (Slice 3 commit, server-side) it is AddChild'd to ParentForChild so the vanilla
     * Construct cascade builds it (the Extend SpawnChildHolograms pattern). Returns the span hologram, or null.
     * Base class is a no-op.
     */
    virtual AFGHologram* LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild = false)
    {
        return nullptr;
    }

protected:
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;
};

/**
 * Belt conveyance (MVP): the spanning element is an SFConveyorBeltHologram routed between the two poles'
 * connectors, mirroring the proven stackable-pole belt path (coincidence-wired at Construct in Slice 3).
 */
UCLASS()
class SMARTFOUNDATIONS_API USFWalkBeltConveyance : public USFWalkConveyance
{
    GENERATED_BODY()

public:
    virtual AFGHologram* LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild = false) override;

private:
    /** First factory connector on a pole hologram (the stackable pole's SnapOnly0), or null. */
    static class UFGFactoryConnectionComponent* FirstConnector(AFGHologram* Pole);
};

/**
 * Pipe conveyance: the spanning element is an SFPipelineHologram routed between two stackable pipeline supports'
 * pipe connectors, mirroring USFWalkBeltConveyance and the proven stackable-pipe auto-connect path
 * (ApplyPipeBuildModeRouting + mSnappedConnectionComponents + tier recipe for cost; coincidence-wired at Construct).
 */
UCLASS()
class SMARTFOUNDATIONS_API USFWalkPipeConveyance : public USFWalkConveyance
{
    GENERATED_BODY()

public:
    virtual AFGHologram* LinkOrUpdate(AFGHologram* ExistingSpan, AFGHologram* FromAnchor, AFGHologram* ToAnchor, AFGHologram* ParentForChild, bool bAddChildForBuild = false) override;

private:
    /** First pipe connector on a pipeline-support hologram, or null. */
    static class UFGPipeConnectionComponentBase* FirstPipeConnector(AFGHologram* Support);
};
