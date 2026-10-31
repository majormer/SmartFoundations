// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "Features/Extend/SFExtendCloneTopology.h"

/** Captured value intent plus append-only construction role bindings. No cargo,
 * chain actor, bucket, payment or historical source identity is copied from a preset. */
class FSFExtendWiringPlan
{
public:
    explicit FSFExtendWiringPlan(const FSFCloneTopology& InTopology) : Topology(InTopology)
    {
        IncludeRequirements(Topology);
    }
    const FSFCloneTopology& GetTopology() const { return Topology; }
    const TSet<FString>& GetRequiredRoles() const { return RequiredRoles; }
    const TSet<FString>& GetRequiredSources() const { return RequiredSources; }
    const TMap<FString, FObjectKey>& GetBindings() const { return Bindings; }
    bool IsValid() const { return !bInvalid; }
    /** Every captured lift hole/face must name its new native clone, never a world search. */
    static bool HasExactLiftHoleRoles(const FSFCloneTopology& Value)
    {
        TMap<FString, const FSFCloneHologram*> Roles;
        for (const auto& Child : Value.ChildHolograms)
        {
            if (Child.bPreviewOnly || !Child.bConstructible) continue;
            if (Child.HologramId.IsEmpty() || Roles.Contains(Child.HologramId)) return false;
            Roles.Add(Child.HologramId, &Child);
        }
        TSet<FString> ClaimedFaces;
        for (const auto& Child : Value.ChildHolograms)
        {
            if (Child.bPreviewOnly || !Child.bConstructible || Child.Role != TEXT("passthrough")
                || Child.BuildClass != TEXT("Build_FoundationPassthrough_Lift_C")) continue;
            if (!Child.bHasPassthroughLinks || !FMath::IsFinite(Child.Thickness) || Child.Thickness <= 0) return false;
            for (const auto* Face : {&Child.PassthroughBottom, &Child.PassthroughTop})
            {
                if (Face->Target.IsEmpty()) { if (!Face->Connector.IsEmpty()) return false; continue; }
                const auto* Lift = Roles.Find(Face->Target);
                const FString Port = Face->Target + TEXT(".") + Face->Connector;
                if (!Lift || !(*Lift)->bHasLiftData || !(*Lift)->LiftData.PassthroughCloneIds.Contains(Child.HologramId)
                    || (Face->Connector != TEXT("ConveyorAny0") && Face->Connector != TEXT("ConveyorAny1"))
                    || ClaimedFaces.Contains(Port)) return false;
                ClaimedFaces.Add(Port);
            }
        }
        for (const auto& Child : Value.ChildHolograms)
        {
            if (Child.bPreviewOnly || !Child.bConstructible || !Child.bHasLiftData) continue;
            if (Child.LiftData.PassthroughCloneIds.Num() > 2) return false;
            for (const auto& HoleId : Child.LiftData.PassthroughCloneIds)
            {
                if (HoleId.IsEmpty()) continue;
                const auto* Found = Roles.Find(HoleId);
                if (!Found || (*Found)->Role != TEXT("passthrough") || (*Found)->BuildClass != TEXT("Build_FoundationPassthrough_Lift_C")
                    || !(*Found)->bHasPassthroughLinks || !FMath::IsFinite((*Found)->Thickness) || (*Found)->Thickness <= 0) return false;
                const auto& Bottom = (*Found)->PassthroughBottom;
                const auto& Top = (*Found)->PassthroughTop;
                bool bReferenced = false;
                for (const auto* Face : {&Bottom, &Top})
                {
                    if (Face->Target.IsEmpty()) { if (!Face->Connector.IsEmpty()) return false; continue; }
                    const auto* Lift = Roles.Find(Face->Target);
                    if (!Lift || !(*Lift)->bHasLiftData || (Face->Connector != TEXT("ConveyorAny0") && Face->Connector != TEXT("ConveyorAny1"))) return false;
                    bReferenced |= Face->Target == Child.HologramId;
                }
                if (!bReferenced) return false;
            }
        }
        return true;
    }
    /** Value-level eligibility only. Runtime classes, state, returned identities,
     * resources and native settlement still need their independent oracles. */
    static bool HasOnlyConveyorEffects(const FSFCloneTopology& Value)
    {
        for (const auto& Child : Value.ChildHolograms)
        {
            // A cost-only wire is not constructed by the child loop, but the
            // post-build power writer still materializes it. Do not hide it.
            if (Child.Role == TEXT("wire_cost") || !Child.PowerFrom.Target.IsEmpty() || !Child.PowerTo.Target.IsEmpty()) return false;
            if (Child.Role != TEXT("factory") && Child.Role != TEXT("distributor") && Child.Role != TEXT("belt_segment")
                && Child.Role != TEXT("lift_segment") && Child.Role != TEXT("route_support")
                && !(Child.Role == TEXT("lane_segment") && (Child.LaneSegmentType == TEXT("belt") || Child.LaneSegmentType == TEXT("lift")))) return false;
            for (const auto& Passthrough : Child.LiftData.PassthroughCloneIds) if (!Passthrough.IsEmpty()) return false;
        }
        return true;
    }
    static bool HasOnlyConveyorAndPricedPowerEffects(const FSFCloneTopology& Value)
    {
        FSFCloneTopology ConveyorPart = Value;
        for (int32 Index = ConveyorPart.ChildHolograms.Num() - 1; Index >= 0; --Index)
        {
            const auto& Child = ConveyorPart.ChildHolograms[Index];
            if (Child.Role == TEXT("wire_cost"))
            {
                if (Child.HologramId.IsEmpty() || Child.PowerFrom.Target.IsEmpty() || Child.PowerTo.Target.IsEmpty()
                    || Child.PowerFrom.Connector.IsEmpty() || Child.PowerTo.Connector.IsEmpty()) return false;
                ConveyorPart.ChildHolograms.RemoveAt(Index);
            }
            else if (Child.Role == TEXT("power_pole")) ConveyorPart.ChildHolograms.RemoveAt(Index);
        }
        return HasOnlyConveyorEffects(ConveyorPart);
    }

    static bool HasSupportedEffects(const FSFCloneTopology& Value)
    {
        if (!HasExactLiftHoleRoles(Value)) return false;
        FSFCloneTopology Remaining = Value;
        for (int32 Index = Remaining.ChildHolograms.Num() - 1; Index >= 0; --Index)
        {
            auto& Child = Remaining.ChildHolograms[Index];
            if (Child.Role == TEXT("passthrough"))
            {
                if ((Child.BuildClass != TEXT("Build_FoundationPassthrough_Lift_C") && Child.BuildClass != TEXT("Build_FoundationPassthrough_Pipe_C")) || !Child.bHasPassthroughLinks
                    || !FMath::IsFinite(Child.Thickness) || Child.Thickness <= 0) return false;
                Remaining.ChildHolograms.RemoveAt(Index);
            }
            else if (Child.Role == TEXT("pipe_segment") || Child.Role == TEXT("pipe_attachment")
                || (Child.Role == TEXT("lane_segment") && Child.LaneSegmentType == TEXT("pipe")))
                Remaining.ChildHolograms.RemoveAt(Index); // separately verified native pipe siblings
            else Child.LiftData.PassthroughCloneIds.Reset();
        }
        return HasOnlyConveyorAndPricedPowerEffects(Remaining);
    }

    void IncludeRequirements(const FSFCloneTopology& Additional)
    {
        if (bSealed) { bInvalid = true; return; }
        for (const FSFCloneHologram& Child : Additional.ChildHolograms)
        {
            if (Child.bPreviewOnly || !Child.bConstructible) continue;
            const bool bConveyor = Child.Role == TEXT("belt_segment") || Child.Role == TEXT("lift_segment")
                || (Child.Role == TEXT("lane_segment") && (Child.LaneSegmentType == TEXT("belt") || Child.LaneSegmentType == TEXT("lift")));
            if (!bConveyor) continue;
            if (Child.HologramId.IsEmpty()) { bInvalid = true; continue; }
            RequiredRoles.Add(Child.HologramId);
            RequireEndpoint(Child.CloneConnections.ConveyorAny0);
            RequireEndpoint(Child.CloneConnections.ConveyorAny1);
            for (const FString& Passthrough : Child.LiftData.PassthroughCloneIds)
                if (!Passthrough.IsEmpty()) RequiredRoles.Add(Passthrough);
        }
    }

    bool RegisterRole(const FString& Role, const FObjectKey& Actor)
    {
        if (bInvalid || bSealed || Role.IsEmpty() || Actor == FObjectKey()) return false;
        if (const FObjectKey* Existing = Bindings.Find(Role))
        {
            if (*Existing != Actor) bInvalid = true;
            return !bInvalid;
        }
        Bindings.Add(Role, Actor);
        return true;
    }

    bool Seal(const TSet<FObjectKey>& ActualOutputs)
    {
        if (bInvalid || bSealed) return false;
        TSet<FObjectKey> Claimed;
        for (const FString& Role : RequiredRoles)
        {
            const FObjectKey* Actor = Bindings.Find(Role);
            if (!Actor || !ActualOutputs.Contains(*Actor) || Claimed.Contains(*Actor)) return false;
            Claimed.Add(*Actor);
        }
        bSealed = true;
        return true;
    }

private:
    void RequireEndpoint(const FSFConnectionRef& Endpoint)
    {
        if (Endpoint.Target.IsEmpty() || Endpoint.Target == TEXT("external")) return;
        if (Endpoint.Connector.IsEmpty()) { bInvalid = true; return; }
        if (Endpoint.Target.StartsWith(TEXT("source:")))
        {
            const FString Name = Endpoint.Target.Mid(7);
            if (Name.IsEmpty()) bInvalid = true;
            else RequiredSources.Add(Name);
        }
        else RequiredRoles.Add(Endpoint.Target);
    }
    const FSFCloneTopology Topology;
    TSet<FString> RequiredRoles;
    TSet<FString> RequiredSources;
    TMap<FString, FObjectKey> Bindings;
    bool bInvalid = false;
    bool bSealed = false;
};
