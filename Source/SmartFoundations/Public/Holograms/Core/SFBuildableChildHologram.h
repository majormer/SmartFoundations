#pragma once

#include "CoreMinimal.h"
#include "FactoryGame/Public/Hologram/FGBuildableHologram.h"
#include "Data/SFHologramDataRegistry.h"
#include "SFBuildableChildHologram.generated.h"

/**
 * Generic Smart! child hologram for buildings with special placement requirements.
 * Used for building types whose CheckValidPlacement() override would fail on children
 * (e.g., ceiling lights require ceiling snap, wall floodlights require wall snap).
 *
 * Follows the deferred-construction spawn pattern:
 * 1. SpawnActor with bDeferConstruction
 * 2. SetBuildClass + SetRecipe (triggers mesh/visual creation)
 * 3. FinishSpawning
 * 4. AddChild to parent
 * 5. DisableValidation + MarkAsChild via USFHologramDataService
 * 6. Disable collisions and tick post-spawn
 *
 * Issue #200: Created for ceiling lights, wall floodlights, and any future
 * building type whose children can't satisfy parent's placement constraints.
 */
UCLASS()
class SMARTFOUNDATIONS_API ASFBuildableChildHologram : public AFGBuildableHologram
{
	GENERATED_BODY()

public:
	ASFBuildableChildHologram();

	/** Always-valid placement — children are positioned by Smart!, not vanilla snapping */
	virtual void CheckValidPlacement() override;

	/** Skip clearance checks — children don't need independent clearance validation */
	virtual void CheckClearance() override;

	/** Block parent from repositioning this child — Smart! handles positioning via SetActorLocation */
	virtual void SetHologramLocationAndRotation(const FHitResult& hitResult) override;

	/** Delegate to vanilla buildable construction using mBuildClass */
	virtual AActor* Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID) override;

	/** Cleanup data structure on destruction */
	virtual void Destroyed() override;

	/** Public setter for build class (mBuildClass is protected on AFGBuildableHologram) */
	void SetChildBuildClass(UClass* InBuildClass) { mBuildClass = InBuildClass; }
};
