// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/Holograms/SFHypertubePassthroughConstructionHologram.h"
#include "SmartFoundations.h"

void ASFHypertubePassthroughConstructionHologram::SetSelectedThickness(float Thickness)
{
    mSnappedBuildingThickness = Thickness;
    RebuildMeshesAndUpdateClearance();
}

void ASFHypertubePassthroughConstructionHologram::SetHologramLocationAndRotation(const FHitResult& Hit) {}
void ASFHypertubePassthroughConstructionHologram::SetHologramNudgeLocation() {}

void ASFHypertubePassthroughConstructionHologram::CheckValidPlacement()
{
    // Grid transforms and validation belong to the native root, as for existing floor-hole cells.
    ResetConstructDisqualifiers();
    SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}
