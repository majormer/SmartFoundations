#pragma once

#include "Shared/Conduits/ConduitPreviewHelperTypes.h"
#include "FGFactoryConnectionComponent.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"

/**
 * FBeltPreviewHelper
 * 
 * Belt preview helper implementation using the unified conduit base class.
 * Handles belt-specific configuration, spline routing, and tick behavior for origin-snap correction.
 * 
 * Backward compatible with existing API - all public methods remain the same.
 */
class SMARTFOUNDATIONS_API FBeltPreviewHelper 
    : public TConduitPreviewHelper<UFGFactoryConnectionComponent, ASFConveyorBeltHologram>
{
    using Super = TConduitPreviewHelper<UFGFactoryConnectionComponent, ASFConveyorBeltHologram>;

public:
    FBeltPreviewHelper(UWorld* InWorld, int32 BeltTier = 1, AFGHologram* ParentHolo = nullptr);
    virtual ~FBeltPreviewHelper() = default;

    /** Update belt tier (forces hologram recreation) */
    void UpdateBeltTier(int32 NewTier) { UpdateTier(NewTier); }

    /** Get current belt tier */
    int32 GetBeltTier() const { return GetTier(); }

    /** Get the length of the belt spline in cm */
    float GetBeltLength() const { return GetLength(); }

    /** Get output connector (backward compatibility) */
    UFGFactoryConnectionComponent* GetOutputConnector() const { return GetStartConnector(); }

    /** Get input connector (backward compatibility) */
    UFGFactoryConnectionComponent* GetInputConnector() const { return GetEndConnector(); }

    /** Validate placement and register as child (legacy method, now handled by base class) */
    bool ValidatePlacementAndRegisterAsChild() { return IsPreviewValid(); }

protected:
    // ========================================
    // Virtual Hook Implementations
    // ========================================

    virtual bool ShouldEnableTick() const override;

    virtual FString GetConnectorType() const override;

    virtual TSubclassOf<AFGSplineHologram> GetHologramClass() const override;

    virtual TSubclassOf<AFGBuildable> GetBuildClass(USFSubsystem* Subsystem) const override;

    virtual void ConfigureHologram(AFGSplineHologram* SpawnedHologram, USFSubsystem* Subsystem) override;

    virtual void SetupSplineRouting(AFGSplineHologram* SpawnedHologram) override;

    virtual void FinalizeSpawn(AFGSplineHologram* SpawnedHologram) override;

private:
    void UpdateSplineEndpoints(UFGFactoryConnectionComponent* Output, UFGFactoryConnectionComponent* Input);
};
