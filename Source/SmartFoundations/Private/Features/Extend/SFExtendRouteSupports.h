// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Buildables/FGBuildable.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"

namespace SFExtendRouteSupports
{
    inline bool IsFixedStackable(FName Class)
    {
        return Class == TEXT("Build_ConveyorPoleStackable_C") || Class == TEXT("Build_PipeSupportStackable_C");
    }

    inline bool IsPipe(FName Class)
    {
        return Class == TEXT("Build_PipelineSupport_C") || Class == TEXT("Build_PipeSupportStackable_C")
            || Class == TEXT("Build_PipelineSupportWall_C");
    }

    inline bool IsFixedSupport(FName Class)
    {
        return IsFixedStackable(Class) || Class == TEXT("Build_ConveyorPoleWall_C")
            || Class == TEXT("Build_ConveyorCeilingAttachment_C") || Class == TEXT("Build_PipelineSupportWall_C");
    }

    // Fixed stackable supports have no selected-height state. Keep the adjustable
    // pole contract separate; a missing selected state must never become a default pole.
    inline UFGConnectionComponent* GetAnchor(AFGBuildable* Support)
    {
        if (!IsValid(Support)) return nullptr;
        const FName Class = Support->GetClass()->GetFName();
        if (Class != TEXT("Build_ConveyorPole_C") && Class != TEXT("Build_PipelineSupport_C") && !IsFixedSupport(Class)) return nullptr;
        TInlineComponentArray<UFGConnectionComponent*> Connections;
        Support->GetComponents(Connections);
        UFGConnectionComponent* Anchor = nullptr;
        for (UFGConnectionComponent* Connection : Connections)
        {
            if (!IsValid(Connection) || Connection->GetFName() != TEXT("SnapOnly0")) continue;
            const auto* Conveyor = Cast<UFGFactoryConnectionComponent>(Connection);
            const auto* Pipe = Cast<UFGPipeConnectionComponent>(Connection);
            const bool bTyped = IsPipe(Class)
                ? Pipe && Pipe->GetPipeConnectionType() == EPipeConnectionType::PCT_SNAP_ONLY
                : Conveyor && Conveyor->GetDirection() == EFactoryConnectionDirection::FCD_SNAP_ONLY;
            if (!bTyped || Anchor || Connection->GetComponentLocation().ContainsNaN()) return nullptr;
            Anchor = Connection;
        }
        return Anchor;
    }
}
