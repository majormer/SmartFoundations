// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableSplitterSmart.h"
#include "Buildables/FGBuildableMergerPriority.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "FGFactoryConnectionComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "Features/Extend/SFExtendCloneTopology.h"

namespace
{
    TSet<FName> PortNames(AFGBuildable* Actor)
    {
        TSet<FName> Names;
        TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
        for (auto* Port : Ports) if (IsValid(Port)) Names.Add(Port->GetFName());
        if (Actor->HasAnyFlags(RF_ClassDefaultObject))
        {
            auto* Actual = Cast<UBlueprintGeneratedClass>(Actor->GetClass());
            for (UClass* Class = Actor->GetClass(); Actual && Class; Class = Class->GetSuperClass())
                if (auto* Blueprint = Cast<UBlueprintGeneratedClass>(Class); Blueprint && Blueprint->SimpleConstructionScript)
                    for (auto* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
                        if (Node && Cast<UFGFactoryConnectionComponent>(Node->GetActualComponentTemplate(Actual))) Names.Add(Node->GetVariableName());
        }
        return Names;
    }
    int32 CountPorts(AFGBuildable* Actor, EFactoryConnectionDirection Direction)
    {
        TSet<FName> Names;
        TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
        for (auto* Port : Ports)
            if (IsValid(Port) && Port->GetDirection() == Direction) Names.Add(Port->GetFName());
        if (Actor->HasAnyFlags(RF_ClassDefaultObject))
        {
            // Cooked Blueprint SCS sockets need not be instantiated on the CDO.
            auto* Actual = Cast<UBlueprintGeneratedClass>(Actor->GetClass());
            for (UClass* Class = Actor->GetClass(); Actual && Class; Class = Class->GetSuperClass())
                if (auto* Blueprint = Cast<UBlueprintGeneratedClass>(Class); Blueprint && Blueprint->SimpleConstructionScript)
                    for (auto* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
                        if (Node)
                            if (auto* Port = Cast<UFGFactoryConnectionComponent>(Node->GetActualComponentTemplate(Actual));
                                IsValid(Port) && Port->GetDirection() == Direction) Names.Add(Node->GetVariableName());
        }
        return Names.Num();
    }
}

bool FSFCapturedDistributorSettings::Capture(AFGBuildable* Actor)
{
    *this = FSFCapturedDistributorSettings();
    if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) return false;
    ClassPath = Actor->GetClass()->GetPathName();
    TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
    for (auto* Port : Ports)
    {
        if (!IsValid(Port) || FindDirection(Port->GetName())) return false;
        const auto Direction = Port->GetDirection();
        if (Direction != EFactoryConnectionDirection::FCD_INPUT && Direction != EFactoryConnectionDirection::FCD_OUTPUT) return false;
        auto& Role = ConnectionDirections.AddDefaulted_GetRef();
        Role.PortName = Port->GetName(); Role.Direction = static_cast<uint8>(Direction);
    }
    if (ConnectionDirections.IsEmpty()) return false;
    if (auto* Splitter = Cast<AFGBuildableSplitterSmart>(Actor))
    {
        Kind = 1;
        for (const auto& Rule : Splitter->GetSortRules())
        {
            auto& Value = Rules.AddDefaulted_GetRef();
            Value.ItemClass = Rule.ItemClass ? Rule.ItemClass->GetPathName() : FString();
            Value.OutputIndex = Rule.OutputIndex;
        }
    }
    else if (auto* Merger = Cast<AFGBuildableMergerPriority>(Actor))
    { Kind = 2; Priorities = Merger->GetInputPriorities(); }
    bCaptured = true;
    return IsValidFor(Actor);
}

bool FSFCapturedDistributorSettings::IsValidFor(AFGBuildable* Actor) const
{
    if (!bCaptured || !IsValid(Actor) || Actor->IsActorBeingDestroyed()
        || Actor->GetClass()->GetPathName() != ClassPath || Rules.Num() > 64 || Priorities.Num() > 64
        || ConnectionDirections.Num() > 32) return false;
    // Old floor presets can retain their static templates. A mounted variant's
    // direction cannot be recovered from class or height alone: require recapture.
    if (ConnectionDirections.IsEmpty() && Actor->GetClass()->GetName().Contains(TEXT("Lift"))) return false;
    TSet<FString> RoleNames;
    for (const auto& Role : ConnectionDirections)
    {
        if (Role.PortName.IsEmpty() || RoleNames.Contains(Role.PortName)
            || (Role.Direction != static_cast<uint8>(EFactoryConnectionDirection::FCD_INPUT)
                && Role.Direction != static_cast<uint8>(EFactoryConnectionDirection::FCD_OUTPUT))) return false;
        RoleNames.Add(Role.PortName);
    }
    if (!ConnectionDirections.IsEmpty())
    {
        const auto Names = PortNames(Actor);
        if (Names.Num() != ConnectionDirections.Num()) return false;
        for (const auto& Role : ConnectionDirections) if (!Names.Contains(FName(*Role.PortName))) return false;
    }
    if (auto* Splitter = Cast<AFGBuildableSplitterSmart>(Actor))
    {
        if (Kind != 1 || !Priorities.IsEmpty() || Rules.Num() > Splitter->GetMaxNumSortRules()) return false;
        const int32 Outputs = CountPorts(Actor, EFactoryConnectionDirection::FCD_OUTPUT);
        for (const auto& Rule : Rules)
            if (Rule.OutputIndex < INDEX_NONE || Rule.OutputIndex >= Outputs
                || (!Rule.ItemClass.IsEmpty() && !LoadClass<UFGItemDescriptor>(nullptr, *Rule.ItemClass))) return false;
        return true;
    }
    if (auto* Merger = Cast<AFGBuildableMergerPriority>(Actor))
        return Kind == 2 && Rules.IsEmpty() && Priorities.Num() == (Merger->HasAnyFlags(RF_ClassDefaultObject)
            ? CountPorts(Actor, EFactoryConnectionDirection::FCD_INPUT) : Merger->GetInputConnections().Num());
    return Kind == 0 && Rules.IsEmpty() && Priorities.IsEmpty();
}

bool FSFCapturedDistributorSettings::Matches(AFGBuildable* Actor) const
{
    if (!IsValidFor(Actor)) return false;
    if (!ConnectionDirections.IsEmpty())
    {
        TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
        if (Ports.Num() != ConnectionDirections.Num()) return false;
        for (auto* Port : Ports)
        {
            const auto* Expected = IsValid(Port) ? FindDirection(Port->GetName()) : nullptr;
            if (!Expected || *Expected != static_cast<uint8>(Port->GetDirection())) return false;
        }
    }
    if (auto* Splitter = Cast<AFGBuildableSplitterSmart>(Actor))
    {
        const auto Actual = Splitter->GetSortRules();
        if (Actual.Num() != Rules.Num()) return false;
        for (int32 Index = 0; Index < Actual.Num(); ++Index)
            if (Actual[Index].OutputIndex != Rules[Index].OutputIndex
                || (Actual[Index].ItemClass ? Actual[Index].ItemClass->GetPathName() : FString()) != Rules[Index].ItemClass) return false;
    }
    if (auto* Merger = Cast<AFGBuildableMergerPriority>(Actor)) return Merger->GetInputPriorities() == Priorities;
    return true;
}

bool FSFCapturedDistributorSettings::ApplyAndVerify(AFGBuildable* Actor) const
{
    if (!IsValidFor(Actor) || !Actor->HasAuthority() || !Actor->HasActorBegunPlay()) return false;
    TStrongObjectPtr<AFGBuildable> KeepAlive(Actor);
    if (auto* Splitter = Cast<AFGBuildableSplitterSmart>(Actor))
    {
        TArray<FSplitterSortRule> Values;
        for (const auto& Rule : Rules)
            Values.Emplace(Rule.ItemClass.IsEmpty() ? nullptr : LoadClass<UFGItemDescriptor>(nullptr, *Rule.ItemClass), Rule.OutputIndex);
        Splitter->SetSortRules(Values);
    }
    else if (auto* Merger = Cast<AFGBuildableMergerPriority>(Actor)) Merger->SetInputPriorities(Priorities);
    // Both setters broadcast native delegates. Retain memory, but reject a retired actor.
    return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Matches(Actor);
}

bool FSFCapturedDistributorSettings::ApplyDirectionsBeforeBeginPlay(AFGBuildable* Actor) const
{
    auto* Attachment = Cast<AFGBuildableConveyorAttachment>(Actor);
    if (!IsValid(Attachment) || !Actor->HasAuthority() || Actor->HasActorBegunPlay() || Actor->IsActorBeingDestroyed()
        || !IsValidFor(Actor->GetClass()->GetDefaultObject<AFGBuildable>())) return false;
    if (ConnectionDirections.IsEmpty()) return true; // Legacy static floor template.
    TArray<UFGFactoryConnectionComponent*> Ports; Actor->GetComponents(Ports);
    if (Ports.Num() != ConnectionDirections.Num()) return false;
    for (auto* Port : Ports)
    {
        const auto* Direction = IsValid(Port) ? FindDirection(Port->GetName()) : nullptr;
        if (!Direction) return false;
        if (auto* Peer = Port->GetConnection(); Peer
            && static_cast<uint8>(Peer->GetDirection()) == *Direction) return false;
    }
    for (auto* Port : Ports) Port->SetDirection(static_cast<EFactoryConnectionDirection>(*FindDirection(Port->GetName())));
    // Native BeginPlay must build mInputs/mOutputs from the final roles. Persist
    // those same roles for load; changing only post-BeginPlay ports leaves caches stale.
    Attachment->SaveCurrentDirections();
    return true;
}
