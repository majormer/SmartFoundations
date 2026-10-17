// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFHypertubePassthroughConstructionHologram.h"

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
