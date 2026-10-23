// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

class AFGHologram;
class ASFLiftRouteHologram;
class ASFLiftRouteBeltHologram;
class ASFLiftRouteProxyHologram;
class UFGFactoryConnectionComponent;

/** One reusable native lift/belt validator pair per grid, independent of cell count. */
class FSFLiftRouteNativePreview
{
public:
    static TSharedPtr<FSFLiftRouteNativePreview> Acquire(AFGHologram* Parent, int32 Tier);
    ~FSFLiftRouteNativePreview();
    ASFLiftRouteHologram* ConfigureLift(AFGHologram* Context, UFGFactoryConnectionComponent* Anchor,
        const FTransform& Pose, const FTransform& Top);
    ASFLiftRouteBeltHologram* ConfigureBelt(AFGHologram* Context, UFGFactoryConnectionComponent* From,
        UFGFactoryConnectionComponent* To);
    AFGHologram* Prepare(ASFLiftRouteProxyHologram* Proxy);
    TSharedPtr<FSFLiftRouteNativePreview> CreateConstructionPreview() const;
    void RegisterProxy(ASFLiftRouteProxyHologram* Proxy);
    void ReleaseProxy(ASFLiftRouteProxyHologram* Proxy);
    void Hide();
private:
    int32 Tier = 1;
    TWeakObjectPtr<ASFLiftRouteHologram> Lift;
    TWeakObjectPtr<ASFLiftRouteBeltHologram> Belt;
    TSet<TWeakObjectPtr<ASFLiftRouteProxyHologram>> Proxies;
};
