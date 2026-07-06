// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Blueprint Hologram Adapter (#168 Scaleable Blueprints)

#pragma once

#include "CoreMinimal.h"
#include "Holograms/Adapters/ISFHologramAdapter.h"

class AFGBlueprintHologram;

/**
 * FSFBlueprintAdapter - adapter for vanilla AFGBlueprintHologram composites (#168)
 *
 * Blueprints previously received FSFUnsupportedAdapter (the #166 guard) because naive
 * scaling cloned the hologram without its blueprint contents. This adapter makes the
 * composite scalable; the matching child-staging in SpawnChildHologram (descriptor copy +
 * LoadBlueprintToOtherWorld) is what fixes the #166-era break.
 *
 * BOUNDARY: blueprints have no build class in the size registry and no single clearance
 * box - the footprint comes from the hologram's own mLocalBounds (the composite bounds
 * vanilla computes when the blueprint is loaded; protected -> friend access via
 * AccessTransformers). Exact bounds matter: cells must tile edge-to-edge for the game's
 * OWN blueprint auto-connect (FGBlueprintOpenConnectionManager) to wire the seams -
 * Smart deliberately does no wiring here.
 */
class SMARTFOUNDATIONS_API FSFBlueprintAdapter : public FSFHologramAdapterBase
{
public:
	explicit FSFBlueprintAdapter(AFGBlueprintHologram* InHologram);
	virtual ~FSFBlueprintAdapter() = default;

	// ISFHologramAdapter interface
	virtual FBoxSphereBounds GetBuildingBounds() const override;
	virtual bool SupportsFeature(ESFFeature Feature) const override;
	virtual FString GetAdapterTypeName() const override;
};
