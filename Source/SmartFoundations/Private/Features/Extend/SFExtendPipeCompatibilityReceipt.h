// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Core/Construction/SFConstructionOperationTypes.h"
#include "Features/Extend/SFExtendCloneTopology.h"

class USFConstructionOperationCoordinator;
class USplineComponent;

/** Final native preview curve and its coordinate frame, captured before construction. */
struct FSFExtendPreparedPipeSpline
{
    FSFSplineData Spline;
    FTransform LocalToWorld;

    bool MatchesGeometry(const USplineComponent* Component) const;
};

/** Readback for existing pipe siblings in an Extend/Restore operation. This does
 * not change their construction/network adapter or certify a fluid simulation study. */
struct FSFExtendPipeCompatibilityReceipt
{
    TSet<FObjectKey> Actors;
    bool Verify(const FSFCloneTopology& Plan, const TMap<FString, FObjectKey>& Built,
        const TMap<FString, FObjectKey>& Sources, const TSet<FObjectKey>& ActualOutputs,
        const TMap<FString, FSFExtendPreparedPipeSpline>& PreparedSplines,
        const TMap<FString, TWeakObjectPtr<UClass>>& ExpectedClasses,
        USFConstructionOperationCoordinator& Owner, const FSFConstructionOperationKey& Key, FString& Reason);
};
