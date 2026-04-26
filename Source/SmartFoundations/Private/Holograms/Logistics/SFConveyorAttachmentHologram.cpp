#include "Holograms/Logistics/SFConveyorAttachmentHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"

AActor* ASFConveyorAttachmentHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	AActor* BuiltActor = Super::Construct(out_children, constructionID);
	AFGBuildable* BuiltDistributor = Cast<AFGBuildable>(BuiltActor);
	if (!BuiltDistributor)
	{
		return BuiltActor;
	}

	// CHILD HOLOGRAM REFACTOR: BuildBeltsForDistributor is no longer needed.
	// Belt children are now added via AddChild() and vanilla's Construct() builds them automatically.
	// This eliminates the "double-build" issue where belts were created both as children AND post-build.
	// See: IMPL_AutoConnect_ChildHologram_Refactor.md

	return BuiltActor;
}

TArray<FItemAmount> ASFConveyorAttachmentHologram::GetCost(bool includeChildren) const
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 ASFConveyorAttachmentHologram::GetCost() called! includeChildren=%d"), includeChildren);
	
	// Get base distributor cost from parent
	TArray<FItemAmount> TotalCost = Super::GetCost(includeChildren);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Base distributor cost: %d item types"), TotalCost.Num());
	
	// Add belt preview costs
	if (USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld()))
	{
		if (USFAutoConnectService* AutoConnect = Subsystem->GetAutoConnectService())
		{
			TArray<FItemAmount> BeltCosts = AutoConnect->GetBeltPreviewsCost(this);
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Belt preview costs: %d item types"), BeltCosts.Num());
			
			// Merge belt costs into total cost (using FactorySpawner's pattern)
			for (const FItemAmount& BeltCost : BeltCosts)
			{
				if (!BeltCost.ItemClass) continue;
				
				// Find existing item or add new one
				if (FItemAmount* Existing = TotalCost.FindByPredicate(
					[&](const FItemAmount& X) { return X.ItemClass == BeltCost.ItemClass; }))
				{
					Existing->Amount += BeltCost.Amount;
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰   Merged: %s +%d = %d"), *GetNameSafe(BeltCost.ItemClass), BeltCost.Amount, Existing->Amount);
				}
				else
				{
					TotalCost.Add(BeltCost);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰   Added: %s x%d"), *GetNameSafe(BeltCost.ItemClass), BeltCost.Amount);
				}
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Total cost returned: %d item types"), TotalCost.Num());
	for (const FItemAmount& Item : TotalCost)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰   → %s x%d"), *GetNameSafe(Item.ItemClass), Item.Amount);
	}
	
	return TotalCost;
}
