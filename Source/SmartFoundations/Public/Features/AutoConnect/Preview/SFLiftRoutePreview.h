// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
class AFGHologram;
class ASFLiftRouteProxyHologram;
class UFGFactoryConnectionComponent;
class FSFLiftRouteNativePreview;

/** Owns lightweight lift and flat-belt cells for one already-assigned port pair. */
class FSFLiftRoutePreview
{
public:
    ~FSFLiftRoutePreview();
    static bool IsEnabled(const AFGHologram* Parent);
    bool Create(AFGHologram* Parent, UFGFactoryConnectionComponent* Distributor, UFGFactoryConnectionComponent* Building, int32 Tier);
private:
    TWeakObjectPtr<ASFLiftRouteProxyHologram> Lift;
    TWeakObjectPtr<ASFLiftRouteProxyHologram> Belt;
    TSharedPtr<FSFLiftRouteNativePreview> NativePreview;
};
