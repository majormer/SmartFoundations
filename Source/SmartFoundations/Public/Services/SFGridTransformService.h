#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SFGridTransformService.generated.h"

class USFSubsystem;
class AFGHologram;

/**
 * Grid Transform Service - Phase 3 Extraction
 * Handles transform change detection and propagation for grid children
 * Provides clean hook point for auto-connect movement detection
 */
UCLASS()
class USFGridTransformService : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(USFSubsystem* InSubsystem);

	/** Check for transform changes and trigger child updates
	 * @param CurrentHologram The active parent hologram to check
	 * @return true if transform changed and updates were triggered
	 */
	bool DetectAndPropagateTransformChange(AFGHologram* CurrentHologram);

	/** Get last known transform (read-only) */
	const FTransform& GetLastKnownTransform() const { return LastKnownTransform; }

	/** Clear cached transform (on hologram unregister) */
	void ClearCache() { LastKnownTransform = FTransform::Identity; }

private:
	TWeakObjectPtr<USFSubsystem> Subsystem;
	
	/** Cached transform for change detection */
	FTransform LastKnownTransform = FTransform::Identity;
};
