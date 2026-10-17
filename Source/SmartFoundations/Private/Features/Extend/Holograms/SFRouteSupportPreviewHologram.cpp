// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Extend/Holograms/SFRouteSupportPreviewHologram.h"
#include "Features/Extend/SFExtendRouteSupports.h"
#include "Hologram/FGStackablePoleHologram.h"
#include "Hologram/FGWallAttachmentHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

// SP/MP DIVERGENCE MAP — fixed route supports
// Both peers preview the captured pose. Authority uses the recipe's native family
// to construct it, following the same child policy as selected-height supports.
AActor* ASFRouteSupportPreviewHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ConstructionID)
{
    if (!GetRecipe() || !GetBuildClass() || !SFExtendRouteSupports::IsFixedSupport(GetBuildClass()->GetFName())) return nullptr;
    const FTransform Pose = GetActorTransform();
    const auto* ParentProperty = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
    if (!ParentProperty) return nullptr;
    // Detached materialization avoids a second quoted child. Smart owns the pose;
    // do not replay aiming, snapping or clearance against a fresh world hit.
    AFGHologram* Native = AFGHologram::SpawnHologramFromRecipe(GetRecipe(), GetOwner(), Pose.GetLocation(), GetConstructionInstigator(),
        [this, &Pose, ParentProperty](AFGHologram* Cell)
        {
            Cell->SetActorEnableCollision(false);
            Cell->SetActorTransform(Pose);
            Cell->Tags.AddUnique(TEXT("SF_ExtendChild"));
            ParentProperty->SetObjectPropertyValue_InContainer(Cell, this);
        });
    if (!IsValid(Native)) return nullptr;
    const bool bFamilyMatches = SFExtendRouteSupports::IsFixedStackable(GetBuildClass()->GetFName())
        ? Native->IsA<AFGStackablePoleHologram>() : Native->IsA<AFGWallAttachmentHologram>();
    if (!bFamilyMatches || Native->GetBuildClass() != GetBuildClass()) { Native->Destroy(); return nullptr; }
    Native->SetConstructionInstigator(GetConstructionInstigator());
    Native->SetActorTickEnabled(false);
    USFHologramDataService::MarkAsChild(Native, GetParentHologram(), ESFChildHologramType::ExtendClone);
    USFHologramDataService::DisableValidation(Native);
    if (GetBlueprintDesigner()) Native->SetInsideBlueprintDesigner(GetBlueprintDesigner());
    Native->SetActorTransform(Pose);
    AActor* Built = Native->Construct(OutChildren, ConstructionID);
    for (AFGHologram* Child : Native->GetHologramChildren()) if (IsValid(Child)) Child->Destroy();
    Native->Destroy();
    return Built;
}
