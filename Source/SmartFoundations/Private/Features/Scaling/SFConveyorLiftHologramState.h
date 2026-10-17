// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
class AFGBuildableBlueprintDesigner;
class ASFConveyorLiftPreviewHologram;
struct FSFScalingSpec;

namespace SFConveyorLiftHologramState
{
    bool IsHandled(const AFGHologram* Root);
    void Sync(AFGHologram* Parent, AFGHologram* Child);
    bool TryGetDimensions(AFGHologram* Root, FVector& OutDimensions);
    bool HasProxyGrid(AFGHologram* Root);
    bool MatchesSelectedPassthroughs(AFGHologram* Root, AFGHologram* Cell, const FTransform& Pose);
    bool CanReplayFloorPlacement(AFGHologram* Root, const FHitResult& Hit);
    bool CanReplayFloorOwner(AActor* Floor, AFGBuildableBlueprintDesigner* Designer, const FVector& Normal);
    bool ValidatePreview(ASFConveyorLiftPreviewHologram* Preview);
    void ReleasePreview(ASFConveyorLiftPreviewHologram* Preview);
    bool Prepare(AFGHologram* Root, const FSFScalingSpec* Spec = nullptr);
    AFGHologram* FindPreparedCell(AFGHologram* Preview);
    bool IsRequestActive();
    class FRequestScope
    {
    public:
        FRequestScope();
        ~FRequestScope();
        FRequestScope(const FRequestScope&) = delete;
        FRequestScope& operator=(const FRequestScope&) = delete;
    private:
        bool bOwnsRequest = false;
    };
}
