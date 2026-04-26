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

protected:
	virtual void ConfigureActor(class AFGBuildable* inBuildable) const override;

private:
	/** Create and configure a wire mesh component with catenary curve */
	void CreateWireMeshWithCatenary(const FVector& StartPos, const FVector& EndPos);

	/** Apply hologram material to all wire meshes */
	void ApplyHologramMaterial(UMaterialInterface* Material);

private:
	/** Cached start/end positions for the wire */
	FVector CachedStartPos;
	FVector CachedEndPos;

	/** Our custom wire mesh component for the preview */
	UPROPERTY()
	UStaticMeshComponent* PreviewWireMesh;

	/** Track if we've set up the wire */
	bool bWireConfigured;
};
