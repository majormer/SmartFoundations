// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Hologram/FGWireHologram.h"
#include "FGPowerConnectionComponent.h"
#include "Buildables/FGBuildableWire.h"
#include "ItemAmount.h"
#include "Resources/FGItemDescriptor.h"
#include "SFWireHologram.generated.h"

/**
 * Smart Wire hologram for power line auto-connect previews.
 * Properly renders catenary wire curves with hologram materials.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFWireHologram : public AFGWireHologram
{
	GENERATED_BODY()

public:
	ASFWireHologram();

	virtual void BeginPlay() override;
	virtual TArray<FItemAmount> GetCost(bool includeChildren) const override;
	// #497: null-recipe guard — scale-daisy wire previews carry no recipe; skip vanilla GetBaseCost's
	// GetIngredients(nullptr) warning (synchronous per-frame disk-write spam). See GetCost for the length cost.
	virtual TArray<FItemAmount> GetBaseCost() const override;
	virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;
	virtual void CheckValidPlacement() override;
	virtual void SetPlacementMaterialState(EHologramMaterialState materialState) override;

	/**
	 * Set up the wire preview between two power connection components.
	 * This properly configures the wire endpoints and triggers mesh generation.
	 */
	void SetupWirePreview(UFGPowerConnectionComponent* StartConnection, UFGPowerConnectionComponent* EndConnection);

	/**
	 * Trigger mesh generation to create the visual wire representation.
	 */
	void TriggerMeshGeneration();

	/**
	 * Force update the wire mesh visibility and materials.
	 */
	void ForceVisibilityUpdate();

	/** Get wire length in cm */
	float GetWireLength() const;
	
	/** Set wire endpoints directly for cost calculation (Issue #229: extend wires without connections) */
	void SetWireEndpoints(const FVector& Start, const FVector& End);

	/**
	 * Issue #345: render a visible wire preview from raw world endpoints, for cases where there are no
	 * connection components yet (Extend/Scaled-Extend clone cables - the target poles aren't built).
	 * Caches the endpoints (also drives GetWireLength/GetCost), builds the catenary mesh, and unhides.
	 */
	void SetupWirePreviewFromPositions(const FVector& StartWorld, const FVector& EndWorld);

protected:
	virtual void ConfigureActor(class AFGBuildable* inBuildable) const override;

private:
	/** Create and configure a wire mesh component with catenary curve */
	void CreateWireMeshWithCatenary(const FVector& StartPos, const FVector& EndPos);

private:
	/** Cached start/end positions for the wire */
	FVector CachedStartPos;
	FVector CachedEndPos;

	/** Our custom wire mesh component for the preview */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PreviewWireMesh;

	/** Track if we've set up the wire */
	bool bWireConfigured;

	/**
	 * Issue #345: when true, the preview mesh uses an absolute world transform so it stays spanning its
	 * world endpoints even when a parent hologram repositions this child actor every frame (Extend).
	 * The auto-connect path leaves this false (its wire is re-created on preview updates).
	 */
	bool bUseAbsoluteMeshTransform = false;

public:
	/** [#497] Block vanilla's locked-parent nudge cascade — it bypasses SetHologramLocationAndRotation
	 *  and dragged every extend child to world origin each tick (see the .cpp override). */
	virtual void SetHologramNudgeLocation() override;

};
