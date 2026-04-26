#include "Holograms/Core/SFBuildableChildHologram.h"
#include "SmartFoundations.h"
#include "Data/SFHologramDataRegistry.h"
#include "Subsystem/SFHologramDataService.h"

ASFBuildableChildHologram::ASFBuildableChildHologram()
{
	// Minimal constructor — configuration happens post-spawn via deferred construction
}

void ASFBuildableChildHologram::CheckValidPlacement()
{
	// Issue #200: Always valid — children are positioned by Smart! grid calculations.
	// Parent's CheckValidPlacement iterates mChildren and calls this on each child.
	// Without this override, children of ceiling lights would fail "Must be built on a ceiling"
	// and children of wall floodlights would fail "Must snap to a Wall or similar".
	ResetConstructDisqualifiers();
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
}

void ASFBuildableChildHologram::CheckClearance()
{
	// Issue #200: Skip clearance checks — children don't need independent clearance validation.
	// Smart! positions children via SetActorLocation; vanilla clearance detection would
	// add false "Encroaching another object's clearance" disqualifiers.
}

void ASFBuildableChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// Block parent from repositioning this child — Smart! handles positioning via SetActorLocation.
	// Without this, the parent's tick would reset children to the hit result position.
}

AActor* ASFBuildableChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	UE_LOG(LogSmartFoundations, Log, TEXT("SFBuildableChildHologram::Construct: Building %s from %s"),
		mBuildClass ? *mBuildClass->GetName() : TEXT("NULL"), *GetName());

	AActor* BuiltActor = Super::Construct(out_children, constructionID);

	if (BuiltActor)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("SFBuildableChildHologram::Construct: Successfully built %s"),
			*BuiltActor->GetName());
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("SFBuildableChildHologram::Construct: Construct returned nullptr for %s"),
			*GetName());
	}

	return BuiltActor;
}

void ASFBuildableChildHologram::Destroyed()
{
	USFHologramDataRegistry::ClearData(this);
	Super::Destroyed();
}
