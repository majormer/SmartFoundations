using System;
using System.Collections.Generic;

namespace SmartFoundations
{
    [AccessTransformers]
    public static class SmartFoundationsAccessTransformers
    {
        // Access to protected spline component for dynamic belt preview updates
        [AccessTransformer(typeof(global::AFGSplineHologram), "mSplineComponent")]
        [AccessTransformer(typeof(global::AFGSplineHologram), "UpdateSplineComponent")]
        
        // Alternative: Access to AutoRouteSpline method for belt routing
        [AccessTransformer(typeof(global::AFGConveyorBeltHologram), "AutoRouteSpline")]
    }
}
