// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

// SP/MP DIVERGENCE MAP — AutoConnect   (see CodeOrganization.md §5)
//  This service computes the belt/pipe plan and (in SP) wires it geometrically against built actors
//  via SF_*AutoConnectChild child holograms. The plan itself runs the same both ways; the divergence
//  is WHO holds the plan at construct time:
//  1. Plan capture  — [MP-SEAM] the CLIENT's fire-time previews are the only complete plan that ever
//                     exists (server re-derivation at the construct seam returns 0). The plan is staged
//                     with the scaling spec (ConduitPlanCost + conduit-plan children) via SFRCO.
//  2. Plan replay   — [MP-AUTH] the server appends the staged plan as SF_BeltAutoConnectChild children
//                     AFTER the grid; each belt then wires geometrically against BUILT actors, exactly
//                     as SP does. (Net seam: Hook B → SpawnConduitPlanChildren, SFGameInstanceModule_SpecHooks.cpp)

#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectServiceImpl.h"

USFAutoConnectService::USFAutoConnectService()
	: Subsystem(nullptr)
{
}

void USFAutoConnectService::Init(USFSubsystem* InSubsystem)
{
	if (!InSubsystem)
	{
		UE_LOG(LogSmartAutoConnect, Error, TEXT("SFAutoConnectService::Init: Subsystem is null"));
		return;
	}
	
	Subsystem = InSubsystem;
	UE_LOG(LogSmartAutoConnect, Log, TEXT("Auto-Connect Service initialized with BUILDING_SEARCH_RADIUS=%.0f cm"), BUILDING_SEARCH_RADIUS);
}

void USFAutoConnectService::Shutdown()
{
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("SFAutoConnectService shutting down"));
	ClearBeltPreviewHelpers();
	Subsystem = nullptr;
}

void USFAutoConnectService::ClearBeltPreviewHelpers()
{
	if (DistributorBeltPreviews.Num() == 0)
	{
		return;
	}

	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🗑️ Clearing all belt preview helpers (%d distributors)"), 
		DistributorBeltPreviews.Num());

	// Iterate through all distributors and destroy their preview helpers
	for (auto& Pair : DistributorBeltPreviews)
	{
		TArray<TSharedPtr<FBeltPreviewHelper>>& Helpers = Pair.Value;
		for (TSharedPtr<FBeltPreviewHelper>& Helper : Helpers)
		{
			if (Helper.IsValid())
			{
				Helper->DestroyPreview();
			}
		}
	}

	// Clear the maps
	DistributorBeltPreviews.Empty();
	StoredConnectorPairs.Empty();
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("✅ All belt preview helpers cleared"));
}

// ========================================
// Main Entry Points
// ========================================

void USFAutoConnectService::OnDistributorHologramUpdated(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("OnDistributorHologramUpdated: ParentHologram is null"));
		return;
	}
	
	// PARENT HOLOGRAM CHECK: Only process actual distributors (splitters/mergers)
	if (!IsDistributorHologram(ParentHologram))
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("⏭️ Skipping non-distributor hologram: %s"), 
			*ParentHologram->GetName());
		return;
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔄 Parent hologram updated: %s - processing distributors"), *ParentHologram->GetName());
	
	// Process the distributor and get its belt previews
	// NOTE: Children are processed separately in ProcessChildDistributors (called from completion callback)
	// NOTE: This may return updated existing previews OR newly created ones
	TArray<TSharedPtr<FBeltPreviewHelper>> Previews = ProcessSingleDistributor(ParentHologram);
	
	// Store the previews in the map
	// IMPORTANT: Don't destroy existing previews first - ProcessSingleDistributor already updated them in place
	if (Previews.Num() > 0)
	{
		// Store the previews (this also triggers cost aggregation and HUD update)
		StoreBeltPreviews(ParentHologram, Previews);
	
		// VISIBILITY & LOCKING: Ensure all belt children match parent's state (matches stackable pattern)
		bool bParentLocked = ParentHologram->IsHologramLocked();
		EHologramMaterialState ParentMaterialState = ParentHologram->GetHologramMaterialState();
	
		for (const TSharedPtr<FBeltPreviewHelper>& Preview : Previews)
		{
			if (Preview.IsValid() && Preview->IsPreviewValid())
			{
				AFGSplineHologram* BeltChild = Preview->GetHologram();
				if (BeltChild)
				{
					// Ensure visibility (critical for locked parent updates)
					BeltChild->SetActorHiddenInGame(false);
					
					// Match parent's material state (valid/invalid)
					BeltChild->SetPlacementMaterialState(ParentMaterialState);
					
					// Match parent's lock state
					if (bParentLocked && !BeltChild->IsHologramLocked())
					{
						BeltChild->LockHologramPosition(true);
					}
				}
			}
		}
	
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT(" Distributor update complete for %s (locked=%d)"), 
			*ParentHologram->GetName(), bParentLocked ? 1 : 0);
	}
	else
	{
		// No previews - clean up any old ones and remove entry from map
		if (TArray<TSharedPtr<FBeltPreviewHelper>>* OldPreviews = DistributorBeltPreviews.Find(ParentHologram))
		{
			for (TSharedPtr<FBeltPreviewHelper>& Helper : *OldPreviews)
			{
				if (Helper.IsValid())
				{
					Helper->DestroyPreview();
				}
			}
		}
		DistributorBeltPreviews.Remove(ParentHologram);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🗑️ No belt previews created - removed distributor %s from map"), 
			*ParentHologram->GetName());
	}
	
	// NOTE: Child distributors are processed ONLY in the completion callback
	// See SFSubsystem.cpp completion callback in UpdateChildPositions()
	// This ensures children are positioned before their belt previews are created
}

bool USFAutoConnectService::IsDistributorHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}
	
	// Check build class (what will be built) not hologram class
	// This is more accurate and avoids false positives from hologram class names
	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}
	
	FString BuildClassName = BuildClass->GetFName().ToString();
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("IsDistributorHologram: Checking '%s'"), *BuildClassName);
	
	// TODO: Consider using class-based hierarchy checks instead of string matching
	if (BuildClassName.Contains(TEXT("Splitter")) || 
		BuildClassName.Contains(TEXT("Merger")))
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("✅ Distributor detected: %s"), *BuildClassName);
		return true;
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("❌ Not a distributor: %s"), *BuildClassName);
	return false;
}

bool USFAutoConnectService::IsSplitterHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}
	
	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}
	
	return BuildClass->GetFName().ToString().Contains(TEXT("Splitter"));
}

bool USFAutoConnectService::IsMergerHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}
	
	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}
	
	return BuildClass->GetFName().ToString().Contains(TEXT("Merger"));
}

// ========================================
// Manifold Chaining
// ========================================

void USFAutoConnectService::FindDistributorChains(AFGHologram* DistributorHologram, TArray<AFGHologram*>& OutChainTargets)
{
	if (!DistributorHologram || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindDistributorChains: DistributorHologram or Subsystem is null"));
		return;
	}
	
	// Get hologram helper to find parent and children
	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (!HologramHelper)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   No HologramHelper available - no chain detection"));
		return;
	}
	
	// Use the active hologram as the grid parent; children are the scaled instances
	AFGHologram* ParentHologram = HologramHelper->GetActiveHologram();
	if (!ParentHologram)
	{
		// Fallback: treat the current distributor as the parent
		ParentHologram = DistributorHologram;
	}
	
	TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren = HologramHelper->GetSpawnedChildren();
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔗 Manifold Detection: Current=%s, Parent=%s, SpawnedChildren=%d"),
		*DistributorHologram->GetName(), *ParentHologram->GetName(), SpawnedChildren.Num());
	
	if (SpawnedChildren.Num() == 0)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   No spawned children - single distributor, no chaining"));
		return;
	}
	
	// Collect all distributor holograms (parent + children)
	TArray<AFGHologram*> AllDistributors;
	
	// Add parent if it's a distributor
	if (IsDistributorHologram(ParentHologram))
	{
		AllDistributors.Add(ParentHologram);
	}
	
	// Add children that are distributors
	for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
	{
		if (ChildPtr.IsValid() && IsDistributorHologram(ChildPtr.Get()))
		{
			AllDistributors.Add(ChildPtr.Get());
		}
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔗 Manifold Detection: Found %d distributors in grid"), AllDistributors.Num());
	
	if (AllDistributors.Num() < 2)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   Need at least 2 distributors for chaining"));
		return;
	}
	
	// Sort distributors spatially so chaining is stable and connects adjacent distributors.
	// The SpawnedChildren order is not guaranteed, and can cause missing links.
	{
		float MinX = TNumericLimits<float>::Max();
		float MaxX = TNumericLimits<float>::Lowest();
		float MinY = TNumericLimits<float>::Max();
		float MaxY = TNumericLimits<float>::Lowest();
		for (AFGHologram* D : AllDistributors)
		{
			if (!D) continue;
			const FVector LocalPos = ParentHologram->GetActorTransform().InverseTransformPosition(D->GetActorLocation());
			MinX = FMath::Min(MinX, LocalPos.X);
			MaxX = FMath::Max(MaxX, LocalPos.X);
			MinY = FMath::Min(MinY, LocalPos.Y);
			MaxY = FMath::Max(MaxY, LocalPos.Y);
		}
		const float RangeX = MaxX - MinX;
		const float RangeY = MaxY - MinY;
		const bool bUseX = RangeX >= RangeY;
		AllDistributors.Sort([ParentHologram, bUseX](const AFGHologram& A, const AFGHologram& B)
		{
			const FVector LA = ParentHologram->GetActorTransform().InverseTransformPosition(A.GetActorLocation());
			const FVector LB = ParentHologram->GetActorTransform().InverseTransformPosition(B.GetActorLocation());
			const float KA = bUseX ? LA.X : LA.Y;
			const float KB = bUseX ? LB.X : LB.Y;
			if (FMath::Abs(KA - KB) > 1.0f)
			{
				return KA < KB;
			}
			return A.GetName() < B.GetName();
		});
	}
	
	// Find the current distributor's index in the array
	int32 CurrentIndex = -1;
	for (int32 i = 0; i < AllDistributors.Num(); i++)
	{
		if (AllDistributors[i] == DistributorHologram)
		{
			CurrentIndex = i;
			break;
		}
	}
	
	if (CurrentIndex == -1)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("   Current distributor not found in array"));
		return;
	}
	
	// Determine if this is a splitter or merger
	bool bIsSplitter = IsSplitterHologram(DistributorHologram);
	bool bIsMerger = IsMergerHologram(DistributorHologram);
	
	// Add chain targets based on distributor type and position
	if (bIsSplitter)
	{
		// Splitters chain forward: current -> next (output to next input)
		if (CurrentIndex < AllDistributors.Num() - 1)
		{
			AFGHologram* NextDistributor = AllDistributors[CurrentIndex + 1];
			OutChainTargets.Add(NextDistributor);
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🔗 Splitter chain: %s -> %s"), 
				*DistributorHologram->GetName(), *NextDistributor->GetName());
		}
	}
	else if (bIsMerger)
	{
		// Mergers chain backward: previous -> current (previous output to current input)
		if (CurrentIndex > 0)
		{
			AFGHologram* PreviousDistributor = AllDistributors[CurrentIndex - 1];
			OutChainTargets.Add(PreviousDistributor);
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🔗 Merger chain: %s -> %s"), 
				*PreviousDistributor->GetName(), *DistributorHologram->GetName());
		}
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   ✅ Found %d distributor chain targets for %s"),
		OutChainTargets.Num(), *DistributorHologram->GetName());
}

// ========================================
// Building Search
// ========================================

void USFAutoConnectService::FindCompatibleBuildingsForDistributor(AFGHologram* DistributorHologram, TArray<AFGBuildable*>& OutCompatibleBuildings)
{
	if (!DistributorHologram || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindCompatibleBuildingsForDistributor: DistributorHologram or Subsystem is null"));
		return;
	}
	
	// Search for nearby buildings
	FVector DistributorLocation = DistributorHologram->GetActorLocation();
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🔍 Searching for buildings within %.0f cm radius"), BUILDING_SEARCH_RADIUS);
	TArray<AFGBuildable*> NearbyBuildings = Subsystem->FindNearbyBuildings(DistributorLocation, BUILDING_SEARCH_RADIUS);
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Found %d nearby buildings"), NearbyBuildings.Num());
	
	// Filter for compatible buildings - ONLY production/manufacturing buildings
	// Must exclude: storage containers, train stations, drones, vehicles, etc.
	for (AFGBuildable* Building : NearbyBuildings)
	{
		if (!Building)
			continue;
		
		// CRITICAL: Never connect to storage containers.
		// Storage containers are AFGBuildableFactory, so they pass the factory check below.
		if (Building->IsA(AFGBuildableStorage::StaticClass()))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - storage building"), *Building->GetName());
			continue;
		}
			
		// CRITICAL: Only allow factory production buildings (AFGBuildableFactory and subclasses)
		// This prevents connecting to storage, stations, vehicles, etc.
		if (!Building->IsA(AFGBuildableFactory::StaticClass()))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - not a factory building"), *Building->GetName());
			continue;
		}
		
		// Additional safety: Skip distributors (they're handled separately in manifold logic)
		FString BuildingClassName = Building->GetClass()->GetFName().ToString();
		if (BuildingClassName.Contains(TEXT("Splitter")) || BuildingClassName.Contains(TEXT("Merger")))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - distributor (handled by manifold logic)"), *Building->GetName());
			continue;
		}
		
		OutCompatibleBuildings.Add(Building);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ✅ Found compatible factory building: %s (%s)"), *Building->GetName(), *BuildingClassName);
	}
}

// RefreshParentCostState removed - vanilla ValidatePlacementAndCost() now handles this automatically
// via GetBaseCost() override in ASFConveyorAttachmentHologram

// ========================================
// Connector Management
// ========================================

void USFAutoConnectService::GetBuildingConnectors(AFGBuildable* Building, TArray<UFGFactoryConnectionComponent*>& OutInputs, TArray<UFGFactoryConnectionComponent*>& OutOutputs)
{
	if (!Building)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("GetBuildingConnectors: Building is null"));
		return;
	}

	OutInputs.Empty();
	OutOutputs.Empty();

	// Gather all factory connection components on the actor (including child components)
	TArray<UFGFactoryConnectionComponent*> AllConnectors;
	Building->GetComponents<UFGFactoryConnectionComponent>(AllConnectors, /*bIncludeFromChildActors=*/true);

	if (AllConnectors.Num() == 0)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("GetBuildingConnectors: Building %s has NO UFGFactoryConnectionComponent components; falling back to overlap search"),
			*Building->GetName());
	}
	else
	{
		for (UFGFactoryConnectionComponent* Connector : AllConnectors)
		{
			if (!Connector)
			{
				continue;
			}

			// Skip connectors that are already connected
			if (Connector->IsConnected())
			{
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("GetBuildingConnectors: Skipping %s connector %s (already connected)"),
					*Building->GetName(), *Connector->GetName());
				continue;
			}

			const EFactoryConnectionDirection Dir = Connector->GetDirection();
			if (Dir == EFactoryConnectionDirection::FCD_INPUT)
			{
				OutInputs.Add(Connector);
			}
			else if (Dir == EFactoryConnectionDirection::FCD_OUTPUT)
			{
				OutOutputs.Add(Connector);
			}
			else if (Dir == EFactoryConnectionDirection::FCD_ANY || Dir == EFactoryConnectionDirection::FCD_SNAP_ONLY)
			{
				// Treat ambiguous connectors as both input and output candidates so
				// FindMatchingBuildableConnector can still locate a match by position.
				OutInputs.Add(Connector);
				OutOutputs.Add(Connector);
			}

			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("GetBuildingConnectors: %s connector %s direction=%d"),
				*Building->GetName(), *Connector->GetName(), static_cast<int32>(Dir));
		}
	}

	// If we still have no connectors, fall back to world overlap search. This is more
	// expensive but robust for cases where components are not directly owned by the
	// buildable or registration ordering is unusual.
	if (OutInputs.Num() == 0 && OutOutputs.Num() == 0)
	{
		if (UWorld* World = Building->GetWorld())
		{
			TArray<UFGFactoryConnectionComponent*> Nearby;
			const float SearchRadius = 1000.0f; // generous compared to 400 cm splitter size
			const int32 FoundCount = UFGFactoryConnectionComponent::FindAllOverlappingConnections(
				Nearby,
				World,
				Building->GetActorLocation(),
				SearchRadius,
				EFactoryConnectionDirection::FCD_ANY);

			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("GetBuildingConnectors: Overlap search around %s found %d potential connectors"),
				*Building->GetName(), FoundCount);

			for (UFGFactoryConnectionComponent* Connector : Nearby)
			{
				if (!Connector || Connector->GetOuterBuildable() != Building)
				{
					continue;
				}

				// Skip connectors that are already connected
				if (Connector->IsConnected())
				{
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("GetBuildingConnectors(overlap): Skipping %s connector %s (already connected)"),
						*Building->GetName(), *Connector->GetName());
					continue;
				}

				const EFactoryConnectionDirection Dir = Connector->GetDirection();
				if (Dir == EFactoryConnectionDirection::FCD_INPUT)
				{
					OutInputs.Add(Connector);
				}
				else if (Dir == EFactoryConnectionDirection::FCD_OUTPUT)
				{
					OutOutputs.Add(Connector);
				}
				else if (Dir == EFactoryConnectionDirection::FCD_ANY || Dir == EFactoryConnectionDirection::FCD_SNAP_ONLY)
				{
					OutInputs.Add(Connector);
					OutOutputs.Add(Connector);
				}

				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("GetBuildingConnectors(overlap): %s connector %s direction=%d"),
					*Building->GetName(), *Connector->GetName(), static_cast<int32>(Dir));
			}
		}
	}

	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("GetBuildingConnectors: Building %s has %d inputs, %d outputs"),
		*Building->GetName(), OutInputs.Num(), OutOutputs.Num());
}

UFGFactoryConnectionComponent* USFAutoConnectService::FindMatchingBuildableConnector(AFGBuildable* BuiltDistributor, UFGFactoryConnectionComponent* HologramConnector, const TArray<UFGFactoryConnectionComponent*>& BuiltConnectors) const
{
	if (!BuiltDistributor || !HologramConnector || BuiltConnectors.Num() == 0)
	{
		return nullptr;
	}

	const FVector HoloLoc = HologramConnector->GetComponentLocation();
	const FVector HoloNormal = HologramConnector->GetConnectorNormal();

	UFGFactoryConnectionComponent* Best = nullptr;
	float BestScore = FLT_MAX;

	for (UFGFactoryConnectionComponent* Candidate : BuiltConnectors)
	{
		if (!Candidate)
		{
			continue;
		}

		const FVector Loc = Candidate->GetComponentLocation();
		const FVector Normal = Candidate->GetConnectorNormal();

		const float Distance = FVector::Dist(HoloLoc, Loc);
		const float Alignment = FVector::DotProduct(HoloNormal, Normal);
		const float Score = Distance - Alignment * 100.0f;

		if (Score < BestScore)
		{
			BestScore = Score;
			Best = Candidate;
		}
	}

	return Best;
}

UFGFactoryConnectionComponent* USFAutoConnectService::FindMiddleConnector(AFGHologram* Distributor)
{
	if (!Distributor)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindMiddleConnector: Distributor is null"));
		return nullptr;
	}

	const bool bIsSplitter = IsSplitterHologram(Distributor);
	const bool bIsMerger   = IsMergerHologram(Distributor);
	if (!bIsSplitter && !bIsMerger)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindMiddleConnector: %s is not a splitter or merger"), *Distributor->GetName());
		return nullptr;
	}

	// Get all connectors
	TArray<UFGFactoryConnectionComponent*> Connectors;
	Distributor->GetComponents<UFGFactoryConnectionComponent>(Connectors);

	// Determine which connectors to inspect
	const EFactoryConnectionDirection TargetDir =
		bIsSplitter ? EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_INPUT;

	const FVector Forward = Distributor->GetActorForwardVector();
	const FVector Origin  = Distributor->GetActorLocation();

	UFGFactoryConnectionComponent* Best = nullptr;
	float BestAlignment = -FLT_MAX;

	for (UFGFactoryConnectionComponent* C : Connectors)
	{
		if (!C || C->GetDirection() != TargetDir)
			continue;

		FVector Dir = (C->GetComponentLocation() - Origin).GetSafeNormal();

		// ✅ Reverse alignment logic for mergers:
		// Splitters: middle points *with* Forward
		// Mergers:   middle points *against* Forward
		float Alignment = FVector::DotProduct(Dir, Forward);
		if (bIsMerger)
			Alignment = -Alignment;

		if (Alignment > BestAlignment)
		{
			BestAlignment = Alignment;
			Best = C;
		}
	}

	if (Best)
	{
		const TCHAR* Type = bIsSplitter ? TEXT("output") : TEXT("input");
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   FindMiddleConnector: %s middle %s = %s (alignment=%.2f)"),
			*Distributor->GetName(), Type, *Best->GetName(), BestAlignment);
	}
	else
	{
		const TCHAR* Type = bIsSplitter ? TEXT("outputs") : TEXT("inputs");
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindMiddleConnector: No %s found on %s"), Type, *Distributor->GetName());
	}

	return Best;
}

UFGFactoryConnectionComponent* USFAutoConnectService::FindConnectorFacingTarget(
	AFGHologram* SourceDistributor,
	AFGHologram* TargetDistributor,
	EFactoryConnectionDirection Direction)
{
	if (!SourceDistributor || !TargetDistributor)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindConnectorFacingTarget: Source or Target is null"));
		return nullptr;
	}

	const FVector SourcePos = SourceDistributor->GetActorLocation();
	const FVector TargetPos = TargetDistributor->GetActorLocation();
	const FVector DirectionToTarget = (TargetPos - SourcePos).GetSafeNormal();

	// Get all connectors of the specified direction
	TArray<UFGFactoryConnectionComponent*> Connectors;
	SourceDistributor->GetComponents<UFGFactoryConnectionComponent>(Connectors);

	UFGFactoryConnectionComponent* Best = nullptr;
	float BestAlignment = -FLT_MAX;

	for (UFGFactoryConnectionComponent* C : Connectors)
	{
		if (!C || C->GetDirection() != Direction)
			continue;

		// Find connector whose normal points toward the target
		FVector ConnectorNormal = C->GetConnectorNormal();
		float Alignment = FVector::DotProduct(ConnectorNormal, DirectionToTarget);

		if (Alignment > BestAlignment)
		{
			BestAlignment = Alignment;
			Best = C;
		}
	}

	if (Best)
	{
		const TCHAR* DirName = (Direction == EFactoryConnectionDirection::FCD_OUTPUT) ? TEXT("output") : TEXT("input");
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Found %s facing target (alignment=%.2f): %s"),
			DirName, BestAlignment, *Best->GetName());
	}
	else
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindConnectorFacingTarget: No connector facing target found"));
	}

	return Best;
}

UFGFactoryConnectionComponent* USFAutoConnectService::FindSideConnector(AFGHologram* Distributor, int32 Index)
{
	if (!Distributor)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindSideConnector: Distributor is null"));
		return nullptr;
	}

	// Determine distributor type
	bool bIsSplitter = IsSplitterHologram(Distributor);
	bool bIsMerger = IsMergerHologram(Distributor);

	if (!bIsSplitter && !bIsMerger)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindSideConnector: %s is not a splitter or merger"), *Distributor->GetName());
		return nullptr;
	}

	// Get all connectors from the hologram
	TArray<UFGFactoryConnectionComponent*> AllConnectors;
	Distributor->GetComponents<UFGFactoryConnectionComponent>(AllConnectors);

	// Find the appropriate connectors based on type
	EFactoryConnectionDirection TargetDirection = bIsSplitter ? EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_INPUT;
	TArray<UFGFactoryConnectionComponent*> TargetConnectors;

	for (UFGFactoryConnectionComponent* Connector : AllConnectors)
	{
		if (Connector && Connector->GetDirection() == TargetDirection)
		{
			TargetConnectors.Add(Connector);
		}
	}

	if (TargetConnectors.Num() <= 1)
		return nullptr;

	// Identify middle connector using forward alignment (same logic as FindMiddleConnector)
	UFGFactoryConnectionComponent* MiddleConnector = FindMiddleConnector(Distributor);

	// Sort connectors by local Y position (left to right) for consistent ordering
	// This ensures index 0 = left side, index 1 = right side, regardless of rotation
	FTransform DistributorTransform = Distributor->GetTransform();
	TargetConnectors.Sort([DistributorTransform](const UFGFactoryConnectionComponent& A, const UFGFactoryConnectionComponent& B) {
		FVector LocalA = DistributorTransform.InverseTransformPosition(A.GetComponentLocation());
		FVector LocalB = DistributorTransform.InverseTransformPosition(B.GetComponentLocation());
		return LocalA.Y < LocalB.Y; // Left to right in local space
	});

	// Build side connectors array (excluding middle)
	TArray<UFGFactoryConnectionComponent*> SideConnectors;
	for (UFGFactoryConnectionComponent* Connector : TargetConnectors)
	{
		if (Connector != MiddleConnector)
		{
			SideConnectors.Add(Connector);
		}
	}

	// Return the requested side connector
	if (SideConnectors.IsValidIndex(Index))
	{
		const TCHAR* ConnectorType = bIsSplitter ? TEXT("output") : TEXT("input");
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Found side %s %d: %s"), ConnectorType, Index, *SideConnectors[Index]->GetName());
		return SideConnectors[Index];
	}

	const TCHAR* ConnectorType = bIsSplitter ? TEXT("output") : TEXT("input");
	UE_LOG(LogSmartAutoConnect, Warning, TEXT("FindSideConnector: Side %s %d not found on %s (has %d sides)"), 
		ConnectorType, Index, *Distributor->GetName(), SideConnectors.Num());
	return nullptr;
}

void USFAutoConnectService::GetAllSideConnectors(AFGHologram* Distributor, TArray<UFGFactoryConnectionComponent*>& OutSideConnectors, EFactoryConnectionDirection DirectionOverride)
{
	OutSideConnectors.Empty();

	if (!Distributor)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("GetAllSideConnectors: Distributor is null"));
		return;
	}

	// Determine distributor type
	bool bIsSplitter = IsSplitterHologram(Distributor);
	bool bIsMerger = IsMergerHologram(Distributor);

	if (!bIsSplitter && !bIsMerger)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("GetAllSideConnectors: %s is not a splitter or merger"), *Distributor->GetName());
		return;
	}

	// Get all connectors from the hologram
	TArray<UFGFactoryConnectionComponent*> AllConnectors;
	Distributor->GetComponents<UFGFactoryConnectionComponent>(AllConnectors);

	// Determine target direction: use override if provided, otherwise auto-detect
	EFactoryConnectionDirection TargetDirection =
		(DirectionOverride != EFactoryConnectionDirection::FCD_MAX)
			? DirectionOverride
			: (bIsSplitter ? EFactoryConnectionDirection::FCD_OUTPUT : EFactoryConnectionDirection::FCD_INPUT);
	
	// Filter connectors by direction
	TArray<UFGFactoryConnectionComponent*> TargetConnectors;
	for (UFGFactoryConnectionComponent* Connector : AllConnectors)
	{
		if (Connector && Connector->GetDirection() == TargetDirection)
		{
			TargetConnectors.Add(Connector);
		}
	}

	if (TargetConnectors.Num() <= 1)
		return;

	// Identify middle connector using forward alignment
	UFGFactoryConnectionComponent* MiddleConnector = FindMiddleConnector(Distributor);

	// Sort connectors by local Y position (left to right) for consistent ordering
	FTransform DistributorTransform = Distributor->GetTransform();
	TargetConnectors.Sort([DistributorTransform](const UFGFactoryConnectionComponent& A, const UFGFactoryConnectionComponent& B) {
		FVector LocalA = DistributorTransform.InverseTransformPosition(A.GetComponentLocation());
		FVector LocalB = DistributorTransform.InverseTransformPosition(B.GetComponentLocation());
		return LocalA.Y < LocalB.Y; // Left to right in local space
	});

	// Build side connectors array (excluding middle)
	for (UFGFactoryConnectionComponent* Connector : TargetConnectors)
	{
		if (Connector != MiddleConnector)
		{
			OutSideConnectors.Add(Connector);
		}
	}

	const TCHAR* ConnectorType = bIsSplitter ? TEXT("outputs") : TEXT("inputs");
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Found %d side %s (sorted left-to-right)"), 
		OutSideConnectors.Num(), ConnectorType);
}

void USFAutoConnectService::ProcessChildDistributors(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		return;
	}
	
	// Get all child holograms directly from the parent
	const TArray<AFGHologram*>& Children = ParentHologram->GetHologramChildren();
	
	if (Children.Num() == 0)
	{
		return; // No children to process
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("👶 Processing %d child holograms for parent %s"),
		Children.Num(), *ParentHologram->GetName());
	
	// Create shared input reservation map for parent + all children
	// This allows distributors to compete for building inputs based on XY distance
	TMap<UFGFactoryConnectionComponent*, AFGHologram*> ReservedInputs;
	
	// Collect all distributors (parent + children)
	TArray<AFGHologram*> AllDistributors;
	if (IsDistributorHologram(ParentHologram))
	{
		AllDistributors.Add(ParentHologram);
	}
	
	for (AFGHologram* Child : Children)
	{
		if (!Child || !IsDistributorHologram(Child))
		{
			continue;
		}
		
		// CRITICAL FIX: Skip children marked for pending destruction to prevent race condition
		// If a child is queued for destroy, its belt previews were already cleaned up
		// Recreating them would leave orphaned previews after the child is destroyed
		if (Child->Tags.Contains(FName(TEXT("SF_GridChild_PendingDestroy"))))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping pending-destroy child: %s"), 
				*Child->GetName());
			continue;
		}
		
		AllDistributors.Add(Child);
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🎯 Processing %d total distributors (parent + children) with shared input reservation"),
		AllDistributors.Num());
	
	// Process each distributor with shared reservation map
	for (int32 i = 0; i < AllDistributors.Num(); i++)
	{
		AFGHologram* Distributor = AllDistributors[i];
		
		bool bIsParent = (Distributor == ParentHologram);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   %s Processing distributor %d/%d: %s"), 
			bIsParent ? TEXT("🎯") : TEXT("👶"),
			i + 1, AllDistributors.Num(), *Distributor->GetName());
		
		// Process with shared reservation map
		TArray<TSharedPtr<FBeltPreviewHelper>> Previews = ProcessSingleDistributor(Distributor, &ReservedInputs);
		
		// Store in map keyed by the distributor hologram
		if (Previews.Num() > 0)
		{
			DistributorBeltPreviews.Emplace(Distributor, Previews);
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ✅ Stored %d belt previews for %s"), 
				Previews.Num(), *Distributor->GetName());
		}
		else
		{
			// Clean up if no previews
			if (TArray<TSharedPtr<FBeltPreviewHelper>>* OldPreviews = DistributorBeltPreviews.Find(Distributor))
			{
				for (TSharedPtr<FBeltPreviewHelper>& Helper : *OldPreviews)
				{
					if (Helper.IsValid())
					{
						Helper->DestroyPreview();
					}
				}
			}
			DistributorBeltPreviews.Remove(Distributor);
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🗑️ No belt previews for %s"), 
				*Distributor->GetName());
		}
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("✅ Finished processing %d distributors (%d inputs reserved)"),
		AllDistributors.Num(), ReservedInputs.Num());
}

void USFAutoConnectService::CleanupDistributorPreviews(AFGHologram* DistributorHologram)
{
	if (!DistributorHologram)
	{
		return;
	}
	
	// Find belt previews for this distributor
	TArray<TSharedPtr<FBeltPreviewHelper>>* Previews = DistributorBeltPreviews.Find(DistributorHologram);
	
	if (!Previews)
	{
		// No previews to clean up
		return;
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🗑️ Cleaning up %d belt previews for distributor: %s"), 
		Previews->Num(), *DistributorHologram->GetName());
	
	// Destroy all belt preview holograms
	for (TSharedPtr<FBeltPreviewHelper>& Helper : *Previews)
	{
		if (Helper.IsValid())
		{
			Helper->DestroyPreview();
		}
	}
	
	// Remove from map
	DistributorBeltPreviews.Remove(DistributorHologram);
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("✅ Cleaned up belt previews for %s"), 
		*DistributorHologram->GetName());
}

void USFAutoConnectService::StoreConnectorPair(AFGHologram* DistributorHologram, UFGFactoryConnectionComponent* HologramConnector, UFGFactoryConnectionComponent* BuildingConnector)
{
	if (!DistributorHologram || !HologramConnector || !BuildingConnector)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("StoreConnectorPair: Invalid parameters"));
		return;
	}

	if (!StoredConnectorPairs.Contains(DistributorHologram))
	{
		StoredConnectorPairs.Add(DistributorHologram, TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>>());
	}

	TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*> Pair(HologramConnector, BuildingConnector);
	StoredConnectorPairs[DistributorHologram].Add(Pair);

	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("StoreConnectorPair: Stored %s → %s for %s"),
		*GetNameSafe(HologramConnector), *GetNameSafe(BuildingConnector), *GetNameSafe(DistributorHologram));
}

void USFAutoConnectService::ClearConnectorPairs(AFGHologram* DistributorHologram)
{
	if (DistributorHologram && StoredConnectorPairs.Contains(DistributorHologram))
	{
		int32 Count = StoredConnectorPairs[DistributorHologram].Num();
		StoredConnectorPairs.Remove(DistributorHologram);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("ClearConnectorPairs: Cleared %d pairs for %s"), Count, *GetNameSafe(DistributorHologram));
	}
}

TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>>* USFAutoConnectService::GetConnectorPairs(AFGHologram* DistributorHologram)
{
	if (DistributorHologram && StoredConnectorPairs.Contains(DistributorHologram))
	{
		return &StoredConnectorPairs[DistributorHologram];
	}
	return nullptr;
}

void USFAutoConnectService::StoreBeltPreviews(AFGHologram* DistributorHologram, const TArray<TSharedPtr<FBeltPreviewHelper>>& Previews)
{
	if (!DistributorHologram)
	{
		return;
	}

	if (Previews.Num() > 0)
	{
		DistributorBeltPreviews.Emplace(DistributorHologram, Previews);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("📦 Stored %d belt previews for %s"), 
		Previews.Num(), *DistributorHologram->GetName());
		
		// CRITICAL: Calculate and cache belt costs NOW for GetCost() hook
		TMap<TSubclassOf<UFGItemDescriptor>, int32> CostMap;
		int32 TotalPreviews = 0;
		int32 ValidPreviews = 0;
		
		for (const TSharedPtr<FBeltPreviewHelper>& Preview : Previews)
		{
			TotalPreviews++;
			if (!Preview.IsValid() || !Preview->IsPreviewValid())
			{
				continue;
			}
			
			ValidPreviews++;
			for (const FItemAmount& Cost : Preview->GetPreviewCost())
			{
				if (Cost.ItemClass)
				{
					int32& CurrentAmount = CostMap.FindOrAdd(Cost.ItemClass, 0);
					CurrentAmount += Cost.Amount;
				}
			}
		}
		
		// Convert map to array
		TArray<FItemAmount> TotalCost;
		for (const TPair<TSubclassOf<UFGItemDescriptor>, int32>& Pair : CostMap)
		{
			FItemAmount ItemCost;
			ItemCost.ItemClass = Pair.Key;
			ItemCost.Amount = Pair.Value;
			TotalCost.Add(ItemCost);
		}
		
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 Calculated belt costs: %d item types from %d/%d valid previews for %s"), 
			TotalCost.Num(), ValidPreviews, TotalPreviews, *DistributorHologram->GetName());
		
		// Check if this distributor is a child (has a parent) or the root parent
		AFGHologram* ParentHologramPtr = DistributorHologram->GetParentHologram();
		bool bIsChildDistributor = (ParentHologramPtr != nullptr);
		
		// If this is a child, trigger parent to re-aggregate
		AFGHologram* RootDistributor = bIsChildDistributor ? ParentHologramPtr : DistributorHologram;
		
		if (bIsChildDistributor)
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 Child distributor (parent=%s) - cached costs, will trigger parent re-aggregation"), 
				*GetNameSafe(ParentHologramPtr));
		}
		else
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 Root/Parent distributor - aggregating grid costs and updating HUD"));
		}
		
		// Update Smart! HUD with belt costs (bypasses vanilla UI caching issues)
		if (UWorld* World = DistributorHologram->GetWorld())
		{
			if (USFSubsystem* SFSys = USFSubsystem::Get(World))
			{
				// Get player inventory for HUD and affordability checks
				UFGInventoryComponent* Inventory = nullptr;
				AFGPlayerController* PlayerController = SFSys->GetLastController();
				if (PlayerController)
				{
					AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PlayerController->GetPawn());
					if (Character)
					{
						Inventory = Character->GetInventory();
					}
				}
				
				// REMOVED: Manual cost aggregation - vanilla now handles via child hologram GetCost()
				// Belt costs automatically aggregate when parent hologram's GetCost() is called
				TArray<FItemAmount> GridTotalCost;
				
				// Get central storage and distributor costs for affordability checking and HUD
				UWorld* LocalWorld = RootDistributor->GetWorld();
				AFGCentralStorageSubsystem* CentralStorage = LocalWorld ? AFGCentralStorageSubsystem::Get(LocalWorld) : nullptr;
				
				// CRITICAL: Get distributor costs that vanilla will deduct FIRST (use root)
				TArray<FItemAmount> DistributorCosts = RootDistributor->GetCost(false);
				
				// Add costs from all grid children
				if (FSFHologramHelperService* HologramHelper = SFSys->GetHologramHelper())
				{
					const TArray<TWeakObjectPtr<AFGHologram>>& Children = HologramHelper->GetSpawnedChildren();
					for (const TWeakObjectPtr<AFGHologram>& ChildPtr : Children)
					{
						AFGHologram* ChildHolo = ChildPtr.Get();
						if (ChildHolo && IsDistributorHologram(ChildHolo))
						{
							for (const FItemAmount& ChildCost : ChildHolo->GetCost(false))
							{
								if (ChildCost.ItemClass)
								{
									FItemAmount* Existing = DistributorCosts.FindByPredicate([&](const FItemAmount& X) {
										return X.ItemClass == ChildCost.ItemClass;
									});
									if (Existing)
									{
										Existing->Amount += ChildCost.Amount;
									}
									else
									{
										DistributorCosts.Add(ChildCost);
									}
								}
							}
						}
					}
				}
				
				// NOTE: DeferredCostService removed - child holograms automatically aggregate costs via GetCost()
				
				// Check if free building is active. Prefer the inventory's GetNoBuildCost(), which
				// covers BOTH the session-wide cheat (GetCheatNoCost) AND the per-player game-mode
				// "No Build Cost" rule that Advanced Game Settings / Creative Mode toggles. The old
				// GameState->GetCheatNoCost() only saw the session cheat, so Creative Mode players
				// were still charged for auto-connected belts/pipes/power.
				AFGGameState* GameState = World->GetGameState<AFGGameState>();
				bool bNoBuildCost = Inventory ? Inventory->GetNoBuildCost()
											  : (GameState && GameState->GetCheatNoCost());

				// Only perform affordability validation if free building is disabled
				if (!bNoBuildCost && Inventory)
				{
					// Check if player can afford belt materials AFTER distributor costs are deducted
					bool bCanAffordBelts = true;
					for (const FItemAmount& BeltCost : GridTotalCost)
					{
						if (!BeltCost.ItemClass) continue;
						
						// Get total available materials
						int32 InCentralStorage = CentralStorage ? CentralStorage->GetNumItemsFromCentralStorage(BeltCost.ItemClass) : 0;
						int32 InPersonalInventory = Inventory->GetNumItems(BeltCost.ItemClass);
						int32 TotalAvailable = InCentralStorage + InPersonalInventory;
						
						// Subtract distributor costs that vanilla will deduct first
						int32 DistributorConsumption = 0;
						for (const FItemAmount& DistCost : DistributorCosts)
						{
							if (DistCost.ItemClass == BeltCost.ItemClass)
							{
								DistributorConsumption = DistCost.Amount;
								break;
							}
						}
						
						int32 RemainingAfterDistributors = TotalAvailable - DistributorConsumption;
						
						if (RemainingAfterDistributors < BeltCost.Amount)
						{
							bCanAffordBelts = false;
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 Cannot afford belt materials AFTER distributor costs: %s - Have: %d, Distributors: %d, Remaining: %d, Belts need: %d"), 
								*GetNameSafe(BeltCost.ItemClass), TotalAvailable, DistributorConsumption, RemainingAfterDistributors, BeltCost.Amount);
							break;
						}
					}
					
					// Add or remove affordability disqualifier based on belt costs
					if (!bCanAffordBelts)
					{
						DistributorHologram->AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 Hologram marked INVALID - cannot afford belt materials"));
					}
					else
					{
						// Player can afford - ensure vanilla validation runs normally
						DistributorHologram->ValidatePlacementAndCost(Inventory);
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 Hologram validated - player can afford belt materials"));
					}
				}
			}
		}
	}
	else
	{
		// Remove entries if no previews
		DistributorBeltPreviews.Remove(DistributorHologram);
		
		// NOTE: DeferredCostService removed - child holograms automatically aggregate costs via GetCost()
		// Vanilla ValidatePlacementAndCost will automatically re-validate affordability
		
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🗑️ Cleared belt previews for %s"), 
			*DistributorHologram->GetName());
	}
}

TArray<TSharedPtr<FBeltPreviewHelper>>* USFAutoConnectService::GetBeltPreviews(AFGHologram* DistributorHologram)
{
	if (!DistributorHologram)
	{
		return nullptr;
	}

	return DistributorBeltPreviews.Find(DistributorHologram);
}

const TArray<TSharedPtr<FBeltPreviewHelper>>* USFAutoConnectService::GetBeltPreviews(const AFGHologram* DistributorHologram) const
{
	if (!DistributorHologram)
	{
		return nullptr;
	}

	// Map lookup with const key - safe because we're just reading
	return DistributorBeltPreviews.Find(const_cast<AFGHologram*>(DistributorHologram));
}

TArray<FItemAmount> USFAutoConnectService::GetBeltPreviewsCost(const AFGHologram* DistributorHologram) const
{
	TArray<FItemAmount> TotalCost;

	if (!DistributorHologram)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 GetBeltPreviewsCost: null hologram"));
		return TotalCost;
	}

	// Use const overload - no const_cast needed
	const TArray<TSharedPtr<FBeltPreviewHelper>>* Previews = GetBeltPreviews(DistributorHologram);
	if (!Previews)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 GetBeltPreviewsCost: No belt previews found for %s"), 
			*DistributorHologram->GetName());
		return TotalCost;
	}
	
	// Accumulate costs from all belt previews
	TMap<TSubclassOf<UFGItemDescriptor>, int32> CostMap;
	int32 ValidCount = 0;
	
	for (const TSharedPtr<FBeltPreviewHelper>& Preview : *Previews)
	{
		if (!Preview.IsValid() || !Preview->IsPreviewValid())
		{
			continue;
		}
		
		ValidCount++;
		TArray<FItemAmount> PreviewCost = Preview->GetPreviewCost();
		for (const FItemAmount& Cost : PreviewCost)
		{
			if (Cost.ItemClass)
			{
				int32& CurrentAmount = CostMap.FindOrAdd(Cost.ItemClass, 0);
				CurrentAmount += Cost.Amount;
			}
		}
	}
	
	// Convert map back to array
	for (const TPair<TSubclassOf<UFGItemDescriptor>, int32>& Pair : CostMap)
	{
		FItemAmount ItemCost;
		ItemCost.ItemClass = Pair.Key;
		ItemCost.Amount = Pair.Value;
		TotalCost.Add(ItemCost);
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 GetBeltPreviewsCost: Calculated %d item types from %d valid previews for %s"), 
		TotalCost.Num(), ValidCount, *DistributorHologram->GetName());
	
	return TotalCost;
}

void USFAutoConnectService::ClearBeltCostsForDistributor(AFGHologram* DistributorHologram)
{
	// REMOVED: CachedBeltCosts tracking - costs now aggregate via child hologram GetCost()
	// This method is kept for API compatibility but does nothing
}

// REMOVED: Belt cost proxy system (UpdateBeltCostProxy, EnsureCostProxy, DestroyCostProxy)
// Belt costs now aggregate automatically via vanilla child hologram GetCost() mechanism

// ========================================
// Pipe Auto-Connect Implementation
// ========================================

void USFAutoConnectService::UpdatePipePreviews(AFGHologram* JunctionHologram)
{
	if (!JunctionHologram || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("UpdatePipePreviews: Missing junction or subsystem"));
		return;
	}

	// Get runtime settings from subsystem
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	
	// Check if pipe auto-connect is enabled
	if (!RuntimeSettings.bPipeAutoConnectEnabled)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("UpdatePipePreviews: Pipe auto-connect disabled in runtime settings"));
		return;
	}

	// Get or create the pipe auto-connect manager for this junction
	TSharedPtr<FSFPipeAutoConnectManager>* ManagerPtr = PipeAutoConnectManagers.Find(JunctionHologram);
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 PIPE MANAGER LOOKUP: Junction=%s (ptr=%p), Found=%s, TotalManagers=%d"), 
		*JunctionHologram->GetName(), JunctionHologram, ManagerPtr ? TEXT("YES") : TEXT("NO"), PipeAutoConnectManagers.Num());
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		// Create new manager for this junction
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 PIPE: Creating NEW manager for junction %s (reason: %s)"), 
			*JunctionHologram->GetName(), !ManagerPtr ? TEXT("not found") : TEXT("invalid"));
		TSharedPtr<FSFPipeAutoConnectManager> NewManager = MakeShared<FSFPipeAutoConnectManager>();
		NewManager->Initialize(Subsystem, this);
		PipeAutoConnectManagers.Add(JunctionHologram, NewManager);
		ManagerPtr = PipeAutoConnectManagers.Find(JunctionHologram);
	}
	else
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 PIPE: Reusing EXISTING manager for junction %s"), 
			*JunctionHologram->GetName());
	}

	if (ManagerPtr && ManagerPtr->IsValid())
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 Updating pipe previews for junction %s"), *JunctionHologram->GetName());
		(*ManagerPtr)->ProcessAllJunctions(JunctionHologram);

		// NOTE: DeferredCostService removed - child holograms automatically aggregate costs via GetCost()
	}
}

void USFAutoConnectService::ClearPipePreviews(AFGHologram* JunctionHologram)
{
	if (!JunctionHologram)
	{
		return;
	}

	// Find and clear the pipe auto-connect manager for this junction
	// CRITICAL: Don't remove the manager from the map - just clear its pipes
	// This prevents duplicate managers from being created on subsequent updates
	TSharedPtr<FSFPipeAutoConnectManager>* ManagerPtr = PipeAutoConnectManagers.Find(JunctionHologram);
	if (ManagerPtr && ManagerPtr->IsValid())
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 Clearing pipe previews for junction %s (keeping manager)"), *JunctionHologram->GetName());
		(*ManagerPtr)->ClearPipePreviews();
		// DON'T remove from map - keep the manager alive for reuse
		// NOTE: DeferredCostService removed - child holograms automatically aggregate costs via GetCost()
	}
}

bool USFAutoConnectService::IsPipelineJunctionHologram(const AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	// Check if hologram class name contains "PipelineJunction"
	FString ClassName = Hologram->GetClass()->GetName();
	return ClassName.Contains(TEXT("PipelineJunction"));
}

void USFAutoConnectService::ClearAllPipeManagers()
{
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 Clearing all pipe managers (%d total)"), PipeAutoConnectManagers.Num());
	
	// Clear all pipes from all managers
	for (auto& Pair : PipeAutoConnectManagers)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->ClearPipePreviews();
		}
	}
	
	// Now remove all managers
	PipeAutoConnectManagers.Empty();
}

FSFPipeAutoConnectManager* USFAutoConnectService::GetPipeManager(AFGHologram* JunctionHologram)
{
	if (!JunctionHologram)
	{
		return nullptr;
	}

	if (TSharedPtr<FSFPipeAutoConnectManager>* ManagerPtr = PipeAutoConnectManagers.Find(JunctionHologram))
	{
		if (ManagerPtr->IsValid())
		{
			return ManagerPtr->Get();
		}
	}

	return nullptr;
}

const FSFPipeAutoConnectManager* USFAutoConnectService::GetPipeManager(const AFGHologram* JunctionHologram) const
{
	if (!JunctionHologram)
	{
		return nullptr;
	}

	// Map lookup with const key - safe because we're just reading
	if (const TSharedPtr<FSFPipeAutoConnectManager>* ManagerPtr = PipeAutoConnectManagers.Find(const_cast<AFGHologram*>(JunctionHologram)))
	{
		if (ManagerPtr->IsValid())
		{
			return ManagerPtr->Get();
		}
	}

	return nullptr;
}

TArray<FItemAmount> USFAutoConnectService::GetPipePreviewsCost(const AFGHologram* JunctionHologram) const
{
	TArray<FItemAmount> TotalCost;

	if (!JunctionHologram)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 GetPipePreviewsCost: null hologram"));
		return TotalCost;
	}

	// Use const overload - no const_cast needed at call site
	const FSFPipeAutoConnectManager* Manager = GetPipeManager(JunctionHologram);
	if (!Manager)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 GetPipePreviewsCost: No pipe manager found for %s"), 
			*JunctionHologram->GetName());
		return TotalCost;
	}
	
	// Aggregate costs from all pipe previews (junction-to-building + manifolds)
	TMap<TSubclassOf<UFGItemDescriptor>, int32> AggregatedCosts;
	int32 ValidCount = 0;
	
	// Phase 1: junction -> building previews
	const TMap<AFGHologram*, TSharedPtr<FPipePreviewHelper>>& BuildingPreviews = Manager->GetBuildingPipePreviews();
	for (const auto& Pair : BuildingPreviews)
	{
		if (Pair.Value.IsValid())
		{
			for (const FItemAmount& Cost : Pair.Value->GetPreviewCost())
			{
				if (Cost.ItemClass)
				{
					AggregatedCosts.FindOrAdd(Cost.ItemClass) += Cost.Amount;
					ValidCount++;
				}
			}
		}
	}
	
	// Phase 2: junction <-> junction manifold previews
	const TMap<AFGHologram*, TSharedPtr<FPipePreviewHelper>>& ManifoldPreviews = Manager->GetManifoldPipePreviews();
	for (const auto& Pair : ManifoldPreviews)
	{
		if (Pair.Value.IsValid())
		{
			for (const FItemAmount& Cost : Pair.Value->GetPreviewCost())
			{
				if (Cost.ItemClass)
				{
					AggregatedCosts.FindOrAdd(Cost.ItemClass) += Cost.Amount;
					ValidCount++;
				}
			}
		}
	}
	
	// Convert aggregated costs to array
	for (const auto& Pair : AggregatedCosts)
	{
		FItemAmount ItemCost;
		ItemCost.ItemClass = Pair.Key;
		ItemCost.Amount = Pair.Value;
		TotalCost.Add(ItemCost);
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("💰 GetPipePreviewsCost: Calculated %d item types from %d valid previews for %s"),  
		TotalCost.Num(), ValidCount, *JunctionHologram->GetName());
	
	return TotalCost;
}

// ========================================
// Power Auto-Connect Implementation
// ========================================

bool USFAutoConnectService::IsPowerPoleHologram(const AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}

	FString ClassName = BuildClass->GetName();
	
	// Check if this is a power pole (Mk1, Mk2, or Mk3)
	// Exclude wall-mounted outlets
	return ClassName.Contains(TEXT("PowerPoleMk")) && !ClassName.Contains(TEXT("Wall"));
}

// ========================================
// Support Structure Detection (Issue #220)
// ========================================

bool USFAutoConnectService::IsStackableConveyorPoleHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}

	FString ClassName = BuildClass->GetName();
	
	// Only stackable conveyor poles - Build_ConveyorPoleStackable_C
	return ClassName.Contains(TEXT("ConveyorPoleStackable"));
}

bool USFAutoConnectService::IsStackablePipelineSupportHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}

	FString ClassName = BuildClass->GetName();
	
	// Only stackable pipeline supports - Build_PipeSupportStackable_C
	return ClassName.Contains(TEXT("PipeSupportStackable"));
}

bool USFAutoConnectService::IsStackableSupportHologram(AFGHologram* Hologram)
{
	return IsStackableConveyorPoleHologram(Hologram) || IsStackablePipelineSupportHologram(Hologram);
}

// ========================================
// Issue #268: Ceiling/Wall Conveyor Support Detection
// ========================================

bool USFAutoConnectService::IsCeilingConveyorSupportHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	const FString HologramClassName = Hologram->GetClass() ? Hologram->GetClass()->GetName() : TEXT("");
	const UClass* BuildClass = Hologram->GetBuildClass();
	const FString BuildClassName = BuildClass ? BuildClass->GetName() : TEXT("");
	
	return BuildClassName.Contains(TEXT("ConveyorCeilingAttachment"))
		|| HologramClassName.Contains(TEXT("ConveyorCeilingAttachment"));
}

bool USFAutoConnectService::IsWallConveyorPoleHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	const FString HologramClassName = Hologram->GetClass() ? Hologram->GetClass()->GetName() : TEXT("");
	const UClass* BuildClass = Hologram->GetBuildClass();
	const FString BuildClassName = BuildClass ? BuildClass->GetName() : TEXT("");
	
	return BuildClassName.Contains(TEXT("ConveyorPoleWall"))
		|| HologramClassName.Contains(TEXT("ConveyorPoleWall"));
}

bool USFAutoConnectService::IsRegularConveyorPoleHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}

	const FString ClassName = BuildClass->GetName();

	// #354: the STANDARD conveyor pole only - Build_ConveyorPole_C. Must NOT match the Stackable or Wall
	// variants (their class names also contain "ConveyorPole"), so exact-match or exclude those explicitly.
	return ClassName == TEXT("Build_ConveyorPole_C")
		|| (ClassName.Contains(TEXT("ConveyorPole"))
			&& !ClassName.Contains(TEXT("ConveyorPoleStackable"))
			&& !ClassName.Contains(TEXT("ConveyorPoleWall")));
}

bool USFAutoConnectService::IsRegularPipelinePoleHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}

	// #364: the STANDARD pipeline support only - Build_PipelineSupport_C. Must NOT match the
	// Stackable support (Build_PipeSupportStackable_C), the Wall support
	// (Build_PipelineSupportWall_C), or the Wall Hole (Build_PipelineSupportWallHole_C).
	const FString ClassName = BuildClass->GetName();
	return ClassName == TEXT("Build_PipelineSupport_C");
}

bool USFAutoConnectService::IsBeltSupportHologram(AFGHologram* Hologram)
{
	return IsStackableConveyorPoleHologram(Hologram)
		|| IsCeilingConveyorSupportHologram(Hologram)
		|| IsWallConveyorPoleHologram(Hologram)
		|| IsRegularConveyorPoleHologram(Hologram);   // #354: standard conveyor pole
}

bool USFAutoConnectService::IsWallPipelineSupportHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}
	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}
	// #364: the WALL pipeline support - exact match so the Wall Hole
	// (Build_PipelineSupportWallHole_C) is NOT caught.
	return BuildClass->GetName() == TEXT("Build_PipelineSupportWall_C");
}

bool USFAutoConnectService::IsPipeSupportHologram(AFGHologram* Hologram)
{
	return IsStackablePipelineSupportHologram(Hologram)
		|| IsRegularPipelinePoleHologram(Hologram)    // #364: standard pipeline support
		|| IsWallPipelineSupportHologram(Hologram);   // #364: wall pipeline support
}

// ========================================
// Issue #187: Floor Hole Pipe Auto-Connect
// ========================================

bool USFAutoConnectService::IsPassthroughPipeHologram(const AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}
	
	// Must be a passthrough hologram
	if (!Hologram->IsA(AFGPassthroughHologram::StaticClass()))
	{
		return false;
	}
	
	// Check build class name for pipe passthrough (hologram actors don't carry pipe connection components)
	// Build_FoundationPassthrough_Pipe_C = pipe floor hole
	// Build_FoundationPassthrough_Lift_C = conveyor lift floor hole
	// Build_FoundationPassthrough_Hypertube_C = hypertube floor hole
	UClass* BuildClass = Hologram->GetBuildClass();
	if (!BuildClass)
	{
		return false;
	}
	FString ClassName = BuildClass->GetFName().ToString();
	return ClassName.Contains(TEXT("Passthrough_Pipe"));
}
