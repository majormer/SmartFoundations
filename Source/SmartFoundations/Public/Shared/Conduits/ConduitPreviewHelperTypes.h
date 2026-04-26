#pragma once

#include "Shared/Conduits/ConduitPreviewHelper.h"
#include "Hologram/FGSplineHologram.h"

/**
 * TConduitPreviewHelper
 * 
 * Template class that adds connector-specific functionality to the base helper.
 * Handles connector type differences (factory vs pipe) while maintaining common logic.
 * 
 * @param TConnector - Connector component type (UFGFactoryConnectionComponent, UFGPipeConnectionComponent, etc.)
 * @param THologram - Hologram type (ASFConveyorBeltHologram, ASFPipelineHologram, etc.)
 */
template<typename TConnector, typename THologram>
class TConduitPreviewHelper : public FConduitPreviewHelper
{
public:
	TConduitPreviewHelper(UWorld* InWorld, int32 InTier, AFGHologram* InParent)
		: FConduitPreviewHelper(InWorld, InTier, InParent)
	{
	}

	virtual ~TConduitPreviewHelper() = default;

	// ========================================
	// Connector-Specific Interface
	// ========================================

	/** Update or create preview between two connectors */
	void UpdatePreview(TConnector* Start, TConnector* End)
	{
		if (!World.IsValid() || !Start || !End)
		{
			HidePreview();
			return;
		}

		StartConnector = Start;
		EndConnector = End;

		// Calculate spawn location from start connector
		FVector SpawnLocation = GetConnectorLocation(Start);

		// Ensure hologram exists
		EnsureSpawned(SpawnLocation);

		if (!Hologram.IsValid())
		{
			return;
		}

		// Update spline routing
		SetupSplineRouting(Hologram.Get());

		// Make visible
		Hologram->SetActorHiddenInGame(false);
	}

	/** Update tier (forces hologram recreation) */
	void UpdateTier(int32 NewTier)
	{
		if (NewTier != Tier)
		{
			Tier = NewTier;
			DestroyPreview(); // Will be recreated on next UpdatePreview
		}
	}

	/** Get start connector */
	TConnector* GetStartConnector() const { return StartConnector.Get(); }

	/** Get end connector */
	TConnector* GetEndConnector() const { return EndConnector.Get(); }

	/** Get typed hologram */
	THologram* GetTypedHologram() const { return Cast<THologram>(Hologram.Get()); }

protected:
	// ========================================
	// Connector Helpers (Overridable)
	// ========================================

	/** Get connector location (can be overridden for different connector types) */
	virtual FVector GetConnectorLocation(TConnector* Connector) const
	{
		return Connector->GetComponentLocation();
	}

	/** Get connector normal (can be overridden for different connector types) */
	virtual FVector GetConnectorNormal(TConnector* Connector) const
	{
		return Connector->GetConnectorNormal();
	}

protected:
	TWeakObjectPtr<TConnector> StartConnector;
	TWeakObjectPtr<TConnector> EndConnector;
};
