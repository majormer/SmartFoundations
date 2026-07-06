// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"

#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "FGPlayerController.h"  // AFGPlayerController (don't rely on transitive unity-build includes)
#include "Subsystem/SFHologramHelperService.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGPipelineHologram.h"
#include "FGPipeConnectionComponent.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Subsystem/SFHologramDataService.h"

// Static counter for unique child names
int32 FSFPipeAutoConnectManager::PipeChildCounter = 0;

FSFPipeAutoConnectManager::FSFPipeAutoConnectManager()
	: Subsystem(nullptr)
	, AutoConnectService(nullptr)
{
}

void FSFPipeAutoConnectManager::Initialize(USFSubsystem* InSubsystem, USFAutoConnectService* InAutoConnectService)
{
	Subsystem = InSubsystem;
	AutoConnectService = InAutoConnectService;

	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("PipeAutoConnectManager initialized (Subsystem=%s, Service=%s)"),
		*GetNameSafe(Subsystem), *GetNameSafe(AutoConnectService));
}

void FSFPipeAutoConnectManager::ProcessAllJunctions(AFGHologram* ParentJunctionHologram)
{
	if (!ParentJunctionHologram || !Subsystem || !AutoConnectService)
	{
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		ClearPipePreviews();
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 ProcessAllJunctions: Skipped - Smart disabled for current action"));
		return;
	}
	
	// Collect ALL junctions (parent + children) - mirrors belt orchestrator
	TArray<AFGHologram*> AllJunctions;
	AllJunctions.Add(ParentJunctionHologram);
	
	// Collect junction children from BOTH sources to ensure we catch all children:
	// 1. HologramHelper->GetSpawnedChildren() - Smart's tracked children
	// 2. Parent's mChildren array - vanilla children (may include children added after grid spawn)
	
	TSet<AFGHologram*> SeenJunctions;
	SeenJunctions.Add(ParentJunctionHologram); // Don't add parent twice
	
	// Source 1: HologramHelper's spawned children (Smart's tracked children)
	if (FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper())
	{
		const TArray<TWeakObjectPtr<AFGHologram>>& SpawnedChildren = HologramHelper->GetSpawnedChildren();
		for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
		{
			if (ChildPtr.IsValid() && FSFPipeConnectorFinder::IsPipelineJunctionHologram(ChildPtr.Get()))
			{
				if (!SeenJunctions.Contains(ChildPtr.Get()))
				{
					AllJunctions.Add(ChildPtr.Get());
					SeenJunctions.Add(ChildPtr.Get());
				}
			}
		}
	}
	
	// Source 2: Vanilla mChildren array (catches children not yet in HologramHelper)
	TFunction<void(AFGHologram*)> CollectChildJunctions = [&](AFGHologram* Hologram)
	{
		if (!Hologram)
		{
			return;
		}
		
		const TArray<AFGHologram*>& Children = Hologram->GetHologramChildren();
		for (AFGHologram* Child : Children)
		{
			if (Child && FSFPipeConnectorFinder::IsPipelineJunctionHologram(Child))
			{
				if (!SeenJunctions.Contains(Child))
				{
					AllJunctions.Add(Child);
					SeenJunctions.Add(Child);
				}
				CollectChildJunctions(Child); // Recurse for nested children
			}
		}
	};
	
	CollectChildJunctions(ParentJunctionHologram);
	
	if (AllJunctions.Num() == 0)
	{
		return;
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Processing %d junctions with shared connector reservation"), AllJunctions.Num());
	
	// CRITICAL: Clean up pipe previews for junctions that no longer exist (removed children)
	// This must happen while preview maps still have data (before any forced clear)
	CleanupOrphanedPreviews(AllJunctions);
	
	// CRITICAL FIX: Use persistent reservation map instead of creating a new local one each frame
	// This prevents junctions from fighting over connectors and causing flickering
	// Clear any stale reservations from previous frames, but keep the map instance
	ReservedConnectors.Empty();

	// Skip-summary: fresh pipe tally per evaluation (read by the HUD)
	if (AutoConnectService)
	{
		AutoConnectService->GetSkipSummary().ResetPipes();
	}
	
	// Track which connector index the parent uses (for child restrictions)
	int32 ParentConnectorIdx = INDEX_NONE;
	
	// Track parent connection type to restrict children (e.g. if parent connects to Output, children can only connect to Output)
	EPipeConnectionType ParentConnectionType = EPipeConnectionType::PCT_ANY;
	bool bParentHasConnection = false;
	
	// Process each junction sequentially with shared reservation map
	for (AFGHologram* Junction : AllJunctions)
	{
		if (!Junction || !Junction->IsValidLowLevel())
		{
			continue;
		}
		
		bool bIsParent = (Junction == ParentJunctionHologram);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 PIPE: Processing junction %s %s"), 
			bIsParent ? TEXT("(PARENT)") : TEXT("(CHILD)"),
			*Junction->GetName());
		
		// For parent: process normally and track which connector index it uses
		if (bIsParent)
		{
			ProcessPipeJunctions(Junction, &ReservedConnectors, nullptr);
			
			// Store parent's chosen connector index for child restrictions
			int32* StoredIdx = ParentConnectorIndices.Find(ParentJunctionHologram);
			if (StoredIdx)
			{
				ParentConnectorIdx = *StoredIdx;
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Parent using connector index %d"), ParentConnectorIdx);
			}
			
			// Capture parent connection type (Input/Output) from child hologram or legacy preview
			// First try the new child hologram system
			TWeakObjectPtr<ASFPipelineHologram>* ChildPtr = BuildingPipeChildren.Find(ParentJunctionHologram);
			if (ChildPtr && ChildPtr->IsValid())
			{
				// Get the building connector from the reserved connectors map
				for (const auto& Pair : ReservedConnectors)
				{
					if (Pair.Value == ParentJunctionHologram && IsValid(Pair.Key))
					{
						ParentConnectionType = Pair.Key->GetPipeConnectionType();
						bParentHasConnection = true;
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Parent connected to %s type connector - restricting children"), 
							(ParentConnectionType == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"));
						break;
					}
				}
			}
			// Fallback to legacy preview helper (will be removed)
			else if (TSharedPtr<FPipePreviewHelper>* PreviewPtr = JunctionPipePreviews.Find(ParentJunctionHologram))
			{
				if (PreviewPtr && (*PreviewPtr).IsValid())
				{
					if (UFGPipeConnectionComponent* BuildingConn = (*PreviewPtr)->GetEndConnection())
					{
						ParentConnectionType = BuildingConn->GetPipeConnectionType();
						bParentHasConnection = true;
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Parent connected to %s type connector - restricting children (legacy)"), 
							(ParentConnectionType == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"));
					}
				}
			}
			
			// CONTEXT-AWARE SPACING: Auto-adjust spacing to match building width (both X and Y)
			// Only apply for parent junction on first connection or when building changes
			// First try the new child hologram system via reserved connectors
			UFGPipeConnectionComponent* BuildingConnector = nullptr;
			for (const auto& Pair : ReservedConnectors)
			{
				if (Pair.Value == ParentJunctionHologram)
				{
					BuildingConnector = Pair.Key;
					break;
				}
			}
			
			// Fallback to legacy preview helper (will be removed)
			if (!BuildingConnector)
			{
				TSharedPtr<FPipePreviewHelper>* PreviewPtr = JunctionPipePreviews.Find(ParentJunctionHologram);
				if (PreviewPtr && PreviewPtr->IsValid())
				{
					BuildingConnector = (*PreviewPtr)->GetEndConnection();
				}
			}
			
			if (BuildingConnector)
			{
				AActor* BuildingActor = BuildingConnector->GetOwner();
				AFGBuildable* Building = Cast<AFGBuildable>(BuildingActor);
				if (Building)
				{
					UClass* CurrentBuildingClass = Building->GetClass();
					
					// Check if we should apply spacing adjustment
					bool bShouldApply = false;
					if (!bContextSpacingApplied)
					{
						// First time connecting - apply it
						bShouldApply = true;
						UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🎯 CONTEXT-AWARE SPACING: First pipe connection detected"));
					}
					else if (LastTargetBuildingClass.IsValid() && LastTargetBuildingClass.Get() != CurrentBuildingClass)
					{
						// Building changed - reset and apply new spacing
						bShouldApply = true;
						UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🎯 CONTEXT-AWARE SPACING: Target building changed (%s → %s), re-adjusting"),
							*LastTargetBuildingClass->GetName(), *CurrentBuildingClass->GetName());
					}
					else
					{
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🎯 CONTEXT-AWARE SPACING: Skipping (already applied, user can fine-tune)"));
					}
					
					if (bShouldApply)
					{
						// Query building size from registry
						USFBuildableSizeRegistry::Initialize();
						FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(CurrentBuildingClass);
						
						// Calculate connector spacing: Building width minus 3m buffer
						// This accounts for the width of the pipe junction itself
						// Example: Manufacturer is 18m wide, but connectors are 15m apart (18m - 3m = 15m)
						float BuildingWidth = Profile.DefaultSize.X - 300.0f;  // 300cm = 3m buffer for junction width
						
						if (BuildingWidth > 0.0f)
						{
							// Get current counter state
							FSFCounterState NewState = Subsystem->GetCounterState();
							
							// Set both X and Y spacing to building width (junctions are orientation-agnostic)
							NewState.SpacingX = FMath::RoundToInt(BuildingWidth);
							NewState.SpacingY = FMath::RoundToInt(BuildingWidth);
							
							// Update state (triggers HUD refresh and child repositioning)
							Subsystem->UpdateCounterState(NewState);
							
							// Mark as applied and track building class
							bContextSpacingApplied = true;
							LastTargetBuildingClass = CurrentBuildingClass;
							
							UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🎯 CONTEXT-AWARE SPACING: Auto-adjusted pipes to %.1fm x %.1fm (building: %s, width: %.0fcm)"),
								BuildingWidth / 100.0f, BuildingWidth / 100.0f,
								*CurrentBuildingClass->GetName(), BuildingWidth);
						}
					}
				}
			}
		}
		// For children: restrict to parent's connector index and its opposite
		else if (ParentConnectorIdx != INDEX_NONE)
		{
			TArray<UFGPipeConnectionComponent*> JunctionConnectors;
			FSFPipeConnectorFinder::GetJunctionConnectors(Junction, JunctionConnectors);
			
			// Issue #206: Use normal-based opposite detection instead of hardcoded index mapping
			int32 OppositeIdx = GetOppositeConnectorByNormal(ParentConnectorIdx, JunctionConnectors);
			TArray<int32> AllowedIndices = { ParentConnectorIdx, OppositeIdx };
			
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Restricting child to indices %d, %d (opposite by normal)"), 
				ParentConnectorIdx, OppositeIdx);
			
			// Pass parent connection type constraint if available
			const EPipeConnectionType* TypeConstraint = bParentHasConnection ? &ParentConnectionType : nullptr;
			
			ProcessPipeJunctions(Junction, &ReservedConnectors, &AllowedIndices, TypeConstraint);
		}
		else
		{
			// Fallback: process child normally (no parent restrictions available yet)
			// Still pass type constraint if available, though unlikely if parent index not set
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🔧 PIPE: Processing child %s without parent restrictions (ParentConnectorIdx=%d)"), 
				*Junction->GetName(), ParentConnectorIdx);
			const EPipeConnectionType* TypeConstraint = bParentHasConnection ? &ParentConnectionType : nullptr;
			ProcessPipeJunctions(Junction, &ReservedConnectors, nullptr, TypeConstraint);
		}
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Finished processing %d junctions (%d connectors reserved, %d building A + %d building B + %d manifold children)"), 
		AllJunctions.Num(), ReservedConnectors.Num(), BuildingPipeChildren.Num(), BuildingPipeChildrenB.Num(), ManifoldPipeChildren.Num());
	
	// Phase 2: Evaluate junction-to-junction manifolds after Phase 1 (junction-to-building) completes
	// This creates pipe connections between junctions that share the same building connector
	EvaluatePipeConnections(ParentJunctionHologram);
	
	// ========================================================================
	// FINAL VISIBILITY REFRESH (from stackable pipe pattern)
	// ========================================================================
	// When parent is locked (during nudging), vanilla may hide children.
	// Ensure ALL tracked pipes have correct visibility regardless of updates.
	bool bParentLocked = ParentJunctionHologram->IsHologramLocked();
	EHologramMaterialState ParentMaterialState = ParentJunctionHologram->GetHologramMaterialState();
	
	// Refresh building pipe children (Side A)
	for (auto& Pair : BuildingPipeChildren)
	{
		if (Pair.Value.IsValid())
		{
			ASFPipelineHologram* Pipe = Pair.Value.Get();
			bool bPipeLocked = Pipe->IsHologramLocked();
			if (bParentLocked != bPipeLocked)
			{
				Pipe->LockHologramPosition(bParentLocked);
			}
			Pipe->SetActorHiddenInGame(false);
			Pipe->SetPlacementMaterialState(ParentMaterialState);
		}
	}
	
	// Issue #206: Refresh building pipe children (Side B)
	for (auto& Pair : BuildingPipeChildrenB)
	{
		if (Pair.Value.IsValid())
		{
			ASFPipelineHologram* Pipe = Pair.Value.Get();
			bool bPipeLocked = Pipe->IsHologramLocked();
			if (bParentLocked != bPipeLocked)
			{
				Pipe->LockHologramPosition(bParentLocked);
			}
			Pipe->SetActorHiddenInGame(false);
			Pipe->SetPlacementMaterialState(ParentMaterialState);
		}
	}
	
	// Refresh manifold pipe children
	for (auto& Pair : ManifoldPipeChildren)
	{
		if (Pair.Value.IsValid())
		{
			ASFPipelineHologram* Pipe = Pair.Value.Get();
			bool bPipeLocked = Pipe->IsHologramLocked();
			if (bParentLocked != bPipeLocked)
			{
				Pipe->LockHologramPosition(bParentLocked);
			}
			Pipe->SetActorHiddenInGame(false);
			Pipe->SetPlacementMaterialState(ParentMaterialState);
		}
	}
}

void FSFPipeAutoConnectManager::ProcessPipeJunctions(
	AFGHologram* ParentJunctionHologram, 
	TMap<UFGPipeConnectionComponent*, AFGHologram*>* SharedReservedConnectors,
	const TArray<int32>* AllowedConnectorIndices,
	const EPipeConnectionType* AllowedConnectionType)
{
	if (!ParentJunctionHologram || !Subsystem || !AutoConnectService)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("PipeAutoConnectManager: Missing context for ProcessPipeJunctions"));
		return;
	}

	// Check if junction has moved since last update
	FTransform CurrentTransform = ParentJunctionHologram->GetActorTransform();
	FTransform* LastTransform = LastJunctionTransforms.Find(ParentJunctionHologram);
	
	bool bJunctionMoved = false;
	if (LastTransform)
	{
		FVector DeltaPos = CurrentTransform.GetLocation() - LastTransform->GetLocation();
		if (DeltaPos.Size() > 1.0f) // Moved more than 1cm
		{
			bJunctionMoved = true;
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT(" PIPE: Junction moved %.1f cm - updating preview"), DeltaPos.Size());
		}
	}
	
	// Store current transform
	LastJunctionTransforms.Add(ParentJunctionHologram, CurrentTransform);

	if (!FSFPipeConnectorFinder::IsPipelineJunctionHologram(ParentJunctionHologram))
	{
		return;
	}

	// Search radius accounts for large buildings with offset connectors (e.g., refinery at 3000cm tall)
	// Building center might be far, but connectors could still be close
	constexpr float SearchRadius = 4000.0f; // 40m - generous radius for building centers, actual connector distance checked later

	TArray<AFGBuildable*> NearbyBuildings;
	FSFPipeConnectorFinder::FindNearbyPipeBuildings(ParentJunctionHologram, SearchRadius, NearbyBuildings);

	UE_LOG(LogSmartAutoConnect, Verbose,
		TEXT("🔍 PIPE: Found %d buildings within %.0fm radius of %s"),
		NearbyBuildings.Num(), SearchRadius / 100.0f, *ParentJunctionHologram->GetName());

	if (NearbyBuildings.Num() == 0)
	{
		return;
	}

	int32 TotalConnectors = 0;
	UFGPipeConnectionComponent* BestJunctionConnector = nullptr;
	UFGPipeConnectionComponent* BestBuildingConnector = nullptr;
	
	// SCORING UPDATE: Use Score instead of DistanceSq
	// Score = Distance * (1 + MisalignmentPenalty)
	float BestScore = TNumericLimits<float>::Max();
	float BestActualDistance = TNumericLimits<float>::Max();
	
	FVector JunctionLocation = ParentJunctionHologram->GetActorLocation();
	FVector JunctionForward = ParentJunctionHologram->GetActorForwardVector();
	FVector JunctionRight = ParentJunctionHologram->GetActorRightVector();

	TArray<UFGPipeConnectionComponent*> JunctionConnectors;
	FSFPipeConnectorFinder::GetJunctionConnectors(ParentJunctionHologram, JunctionConnectors);

	for (AFGBuildable* Building : NearbyBuildings)
	{
		if (!Building)
		{
			continue;
		}
		
		// BUILDING TYPE FILTER: Only connect to valid production buildings
		// Skip loose pipes - they're not valid connection targets
		if (Building->IsA(AFGBuildablePipeline::StaticClass()))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - loose pipeline"), *Building->GetName());
			continue;
		}
		
		// Skip pipe supports - they're infrastructure, not production
		FString BuildingClassName = Building->GetClass()->GetFName().ToString();
		if (BuildingClassName.Contains(TEXT("PipelineSupport")) || BuildingClassName.Contains(TEXT("PipeSupport")))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - pipe support"), *Building->GetName());
			continue;
		}
		
		// Skip storage containers - they have pipe connections but aren't production buildings
		if (Building->IsA(AFGBuildableStorage::StaticClass()))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - storage building"), *Building->GetName());
			continue;
		}
		
		// Issue #187: Allow floor holes (passthroughs) as valid pipe auto-connect targets
		// Passthroughs have 2 pipe connections and should be treated like junctions
		// They are NOT AFGBuildableFactory subclasses, so we check before the factory filter
		if (Building->IsA(AFGBuildablePassthrough::StaticClass()))
		{
			// Passthrough allowed - check if it has pipe connections (only pipe floor holes)
			TArray<UFGPipeConnectionComponent*> PassthroughPipeConnectors;
			FSFPipeConnectorFinder::GetPipeConnectors(Building, PassthroughPipeConnectors);
			if (PassthroughPipeConnectors.Num() == 0)
			{
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - passthrough without pipe connections"), *Building->GetName());
				continue;
			}
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   ✅ Floor hole (passthrough) with %d pipe connections accepted as target"), PassthroughPipeConnectors.Num());
			// Fall through to connector matching below
		}
		// Only allow factory production buildings (AFGBuildableFactory and subclasses)
		// This prevents connecting to stations, vehicles, etc.
		else if (!Building->IsA(AFGBuildableFactory::StaticClass()))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - not a factory building"), *Building->GetName());
			continue;
		}
		
		// Skip pipeline junctions - they're handled separately (reuse BuildingClassName from above)
		if (BuildingClassName.Contains(TEXT("PipelineJunction")))
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping %s - pipeline junction (handled separately)"), *Building->GetName());
			continue;
		}

		TArray<UFGPipeConnectionComponent*> PipeConnectors;
		FSFPipeConnectorFinder::GetPipeConnectors(Building, PipeConnectors);
		
		float BuildingDistanceToJunction = FVector::Dist(Building->GetActorLocation(), JunctionLocation) / 100.0f; // meters
		UE_LOG(LogSmartAutoConnect, Verbose,
			TEXT("   📦 Building: %s | Distance: %.1fm | Unconnected Pipes: %d"),
			*Building->GetClass()->GetName(), BuildingDistanceToJunction, PipeConnectors.Num());
		
		TotalConnectors += PipeConnectors.Num();
		
		// STEP 1: Determine type of connector to target (Input vs Output)
		// If AllowedConnectionType is provided (from parent), force that type.
		// Otherwise, find the closest connector and use its type to determine side.
		EPipeConnectionType TargetConnectorType = EPipeConnectionType::PCT_ANY;
		
		if (AllowedConnectionType)
		{
			TargetConnectorType = *AllowedConnectionType;
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Building %s: Forcing type %s (from parent constraint)"), 
				*Building->GetName(), 
				(TargetConnectorType == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"));
		}
		else
		{
			// Find closest connector on this building to determine "side"
			UFGPipeConnectionComponent* ClosestBuildingConn = nullptr;
			float ClosestBuildingDistSq = TNumericLimits<float>::Max();
			
			for (UFGPipeConnectionComponent* BuildingConn : PipeConnectors)
			{
				if (!BuildingConn) continue;
				
				// Skip connectors already reserved by OTHER junctions
				if (SharedReservedConnectors)
				{
					AFGHologram** ExistingClaim = SharedReservedConnectors->Find(BuildingConn);
					if (ExistingClaim && *ExistingClaim != ParentJunctionHologram)
					{
						continue;
					}
				}
				
				float DistanceSq = FVector::DistSquared2D(JunctionLocation, BuildingConn->GetComponentLocation());
				if (DistanceSq < ClosestBuildingDistSq)
				{
					ClosestBuildingDistSq = DistanceSq;
					ClosestBuildingConn = BuildingConn;
				}
			}
			
			if (ClosestBuildingConn)
			{
				TargetConnectorType = ClosestBuildingConn->GetPipeConnectionType();
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Building %s: Closest side has %s connectors"), 
					*Building->GetName(), 
					(TargetConnectorType == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"));
			}
			else
			{
				continue; // No available connectors on this building
			}
		}
		
		// STEP 2: Evaluate all connectors matching the TargetConnectorType
		for (UFGPipeConnectionComponent* BuildingConn : PipeConnectors)
		{
			if (!BuildingConn)
			{
				continue;
			}
			
			// Skip connectors already reserved by OTHER junctions
			if (SharedReservedConnectors)
			{
				AFGHologram** ExistingClaim = SharedReservedConnectors->Find(BuildingConn);
				if (ExistingClaim && *ExistingClaim != ParentJunctionHologram)
				{
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Skipping reserved connector %s (claimed by %s)"),
						*BuildingConn->GetName(), *(*ExistingClaim)->GetName());
					continue;
				}
			}
			
			// SIDE FILTER: Only consider connectors matching the target type
			if (BuildingConn->GetPipeConnectionType() != TargetConnectorType)
			{
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Skipping connector %s (wrong side: %s vs %s)"),
					*BuildingConn->GetName(),
					(BuildingConn->GetPipeConnectionType() == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"),
					(TargetConnectorType == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"));
				continue;
			}

			FVector BuildingLocation = BuildingConn->GetComponentLocation();
			float Distance = FVector::Dist2D(JunctionLocation, BuildingLocation);

			// SCORING: Calculate weighted score based on distance and alignment
			// This prevents "crossing" pipes by penalizing misaligned connections
			FVector DirToBuilding = (BuildingLocation - JunctionLocation).GetSafeNormal();

			// Check alignment with junction's cardinal axes
			float ForwardDot = FMath::Abs(FVector::DotProduct(DirToBuilding, JunctionForward));
			float RightDot = FMath::Abs(FVector::DotProduct(DirToBuilding, JunctionRight));
			float MaxAlignment = FMath::Max(ForwardDot, RightDot);

			// Angle Penalty: 1.0 (perfectly aligned) -> 0.0 (misaligned)
			// Penalty multiplier: 10.0 (same as belts)
			// [#464] Plus level affinity (same fix as belts): the XY distance is Z-blind, so a
			// staggered other-level connector could out-score the same-level one. Same-level wins;
			// cross-level stays a valid fallback when no same-level candidate exists.
			float AnglePenalty = 1.0f - MaxAlignment;
			float Score = Distance * (1.0f + AnglePenalty * 10.0f)
				+ USFAutoConnectService::LevelAffinityPenalty(JunctionLocation, BuildingLocation);
			
			// Only proceed if this score beats our current best
			if (Score < BestScore)
			{
				UFGPipeConnectionComponent* ClosestJunctionConn = nullptr;
				float BestJunctionDistSq = TNumericLimits<float>::Max();
				
				// Find closest junction connector that meets all criteria
				for (int32 JunctionIdx = 0; JunctionIdx < JunctionConnectors.Num(); JunctionIdx++)
				{
					UFGPipeConnectionComponent* JunctionConn = JunctionConnectors[JunctionIdx];
					if (!JunctionConn)
					{
						continue;
					}
					
					// RESTRICTION: Check if this connector index is allowed (for child junctions)
					if (AllowedConnectorIndices && !AllowedConnectorIndices->Contains(JunctionIdx))
					{
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Skipping junction connector %d (not in allowed indices)"), JunctionIdx);
						continue;
					}

					// TYPE COMPATIBILITY: Junction and building connectors must be compatible
					// PRODUCER (output) connects to CONSUMER (input), and vice versa
					EPipeConnectionType JunctionConnType = JunctionConn->GetPipeConnectionType();
					EPipeConnectionType BuildingConnType = BuildingConn->GetPipeConnectionType();
					bool bTypeCompatible = (JunctionConnType != BuildingConnType);
					
					if (!bTypeCompatible)
					{
						UE_LOG(LogSmartAutoConnect, VeryVerbose, 
							TEXT("      Skipping junction[%d]: Type mismatch (J:%s B:%s - both same type)"),
							JunctionIdx,
							(JunctionConnType == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"),
							(BuildingConnType == EPipeConnectionType::PCT_CONSUMER) ? TEXT("INPUT") : TEXT("OUTPUT"));
						continue;
					}

					float JunctionDistSq = FVector::DistSquared2D(JunctionConn->GetComponentLocation(), BuildingLocation);
					if (JunctionDistSq < BestJunctionDistSq)
					{
						// ANGLE VALIDATION: Check both junction and building connector angles (30° max)
						FVector JunctionPos = JunctionConn->GetComponentLocation();
						FVector BuildingConnPos = BuildingConn->GetComponentLocation();
						FVector JunctionToBuildingDir = (BuildingConnPos - JunctionPos).GetSafeNormal();
						bool bJunctionAngleValid = FSFPipeConnectorFinder::IsConnectionAngleValid(JunctionConn, JunctionToBuildingDir, USFAutoConnectService::FACING_SANITY_ANGLE);
						bool bBuildingAngleValid = FSFPipeConnectorFinder::IsConnectionAngleValid(BuildingConn, -JunctionToBuildingDir, USFAutoConnectService::FACING_SANITY_ANGLE);
						
						// OPPOSITE-FACING CHECK: Connectors must face toward each other (dot product < 0)
						FVector JunctionNormal = JunctionConn->GetConnectorNormal();
						FVector BuildingNormal = BuildingConn->GetConnectorNormal();
						float NormalDotProduct = FVector::DotProduct(JunctionNormal, BuildingNormal);
						bool bNormalsOpposite = (NormalDotProduct < 0.0f);
						
						// DIRECTION CHECK: Building connector must face TOWARD the junction (not away)
						// The building connector's normal should point toward the junction location
						FVector BuildingToJunctionDir = (JunctionPos - BuildingConnPos).GetSafeNormal();
						float BuildingFacingDot = FVector::DotProduct(BuildingNormal, BuildingToJunctionDir);
						bool bBuildingFacesJunction = (BuildingFacingDot > 0.5f); // Must face toward junction (within ~60°)
						
						// DIRECTION CHECK: Junction connector must face TOWARD the building connector
						float JunctionFacingDot = FVector::DotProduct(JunctionNormal, JunctionToBuildingDir);
						bool bJunctionFacesBuilding = (JunctionFacingDot > 0.5f); // Must face toward building (within ~60°)
						
						if (bJunctionAngleValid && bBuildingAngleValid && bNormalsOpposite && bBuildingFacesJunction && bJunctionFacesBuilding)
						{
							BestJunctionDistSq = JunctionDistSq;
							ClosestJunctionConn = JunctionConn;
							
							UE_LOG(LogSmartAutoConnect, VeryVerbose, 
								TEXT("      Valid pair: Junction[%d]→Building (JFace=%.2f, BFace=%.2f)"), 
								JunctionIdx, JunctionFacingDot, BuildingFacingDot);
						}
						else
						{
							UE_LOG(LogSmartAutoConnect, VeryVerbose, 
								TEXT("      Rejected pair: Junction[%d]→Building (JAngle:%d BAngle:%d Opp:%d JFace:%d BFace:%d)"), 
								JunctionIdx, bJunctionAngleValid, bBuildingAngleValid, bNormalsOpposite, 
								bJunctionFacesBuilding, bBuildingFacesJunction);
						}
					}
				}

				// Only update best pair if we found a valid junction connector
				if (ClosestJunctionConn)
				{
					BestScore = Score; // Update best score
					BestActualDistance = Distance;
					BestJunctionConnector = ClosestJunctionConn;
					BestBuildingConnector = BuildingConn;
					
					UE_LOG(LogSmartAutoConnect, VeryVerbose, 
						TEXT("      New best candidate: Score=%.0f (Dist=%.0f, Align=%.2f)"), 
						BestScore, Distance, MaxAlignment);
				}
			}
		}
	}


	UE_LOG(LogSmartAutoConnect, VeryVerbose,
		TEXT("PipeAutoConnectManager::ProcessPipeJunctions - Junction=%s | Buildings=%d | PipeConnectors=%d"),
		*ParentJunctionHologram->GetName(), NearbyBuildings.Num(), TotalConnectors);

	if (!BestJunctionConnector || !BestBuildingConnector)
	{
		UE_LOG(LogSmartAutoConnect, Verbose,
			TEXT("   ❌ No valid pipe connection found (checked %d buildings, %d total connectors)"),
			NearbyBuildings.Num(), TotalConnectors);
		
		// Clean up any existing child hologram for this junction (moved out of range)
		TWeakObjectPtr<ASFPipelineHologram>* ExistingChild = BuildingPipeChildren.Find(ParentJunctionHologram);
		if (ExistingChild && ExistingChild->IsValid())
		{
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Removing child (no valid connection) for junction %s"), 
				*ParentJunctionHologram->GetName());
			RemovePipeChild(ParentJunctionHologram, ExistingChild->Get());
			BuildingPipeChildren.Remove(ParentJunctionHologram);
		}
		// Issue #206: Also clean up Side B
		TWeakObjectPtr<ASFPipelineHologram>* ExistingChildB = BuildingPipeChildrenB.Find(ParentJunctionHologram);
		if (ExistingChildB && ExistingChildB->IsValid())
		{
			RemovePipeChild(ParentJunctionHologram, ExistingChildB->Get());
			BuildingPipeChildrenB.Remove(ParentJunctionHologram);
		}
		return;
	}
	
	if (BestJunctionConnector && BestBuildingConnector)
	{
		float BestDistance = BestActualDistance;
		
		// Enforce maximum connection distance (25m - slightly larger than belt limit to account for vertical offset)
		constexpr float MaxConnectionDistance = 2500.0f; // 25m
		if (BestDistance > MaxConnectionDistance)
		{
			// Skip-summary: this pairing won selection and was dropped only by the distance gate
			if (AutoConnectService)
			{
				AutoConnectService->GetSkipSummary().PipesTooFar++;
			}
			UE_LOG(LogSmartAutoConnect, Verbose,
				TEXT("   ❌ Best connection rejected: too far (%.1fm > %.1fm limit)"),
				BestDistance / 100.0f, MaxConnectionDistance / 100.0f);
			
			// Clean up any existing child hologram for this junction (moved out of range)
			TWeakObjectPtr<ASFPipelineHologram>* ExistingChild = BuildingPipeChildren.Find(ParentJunctionHologram);
			if (ExistingChild && ExistingChild->IsValid())
			{
				UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Removing child (too far) for junction %s"), 
					*ParentJunctionHologram->GetName());
				RemovePipeChild(ParentJunctionHologram, ExistingChild->Get());
				BuildingPipeChildren.Remove(ParentJunctionHologram);
			}
			// Issue #206: Also clean up Side B
			{TWeakObjectPtr<ASFPipelineHologram>* ChildB = BuildingPipeChildrenB.Find(ParentJunctionHologram);
			if (ChildB && ChildB->IsValid()) { RemovePipeChild(ParentJunctionHologram, ChildB->Get()); BuildingPipeChildrenB.Remove(ParentJunctionHologram); }}
			return; // No preview if connection would be too long
		}
		
		// Enforce minimum connection distance based on angle
		// Short distances are OK if connectors are well-aligned (saves space)
		// But short distances with perpendicular offset cause crazy routing
		constexpr float MinConnectionDistanceWithAngle = 200.0f; // 2m if angle > 10°
		constexpr float MinConnectionDistanceStraight = 50.0f;   // 0.5m if angle ≤ 10° (well-aligned)
		constexpr float StraightAngleThreshold = 10.0f;          // Degrees
		
		// Calculate actual angle between connectors
		FVector JunctionToBuildingDir = (BestBuildingConnector->GetComponentLocation() - BestJunctionConnector->GetComponentLocation()).GetSafeNormal();
		FVector JunctionNormal = BestJunctionConnector->GetConnectorNormal();
		float AngleRadians = FMath::Acos(FMath::Clamp(FVector::DotProduct(JunctionNormal, JunctionToBuildingDir), -1.0f, 1.0f));
		float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
		
		float EffectiveMinDistance = (AngleDegrees <= StraightAngleThreshold) ? MinConnectionDistanceStraight : MinConnectionDistanceWithAngle;
		
		if (BestDistance < EffectiveMinDistance)
		{
			// Skip-summary: this pairing won selection and was dropped only by the min-distance gate
			if (AutoConnectService)
			{
				AutoConnectService->GetSkipSummary().PipesTooClose++;
			}
			UE_LOG(LogSmartAutoConnect, Verbose,
				TEXT("   ❌ Best connection rejected: too close (%.1fm < %.1fm minimum at %.1f° angle)"),
				BestDistance / 100.0f, EffectiveMinDistance / 100.0f, AngleDegrees);
			
			// Clean up any existing child hologram for this junction (moved too close)
			TWeakObjectPtr<ASFPipelineHologram>* ExistingChild = BuildingPipeChildren.Find(ParentJunctionHologram);
			if (ExistingChild && ExistingChild->IsValid())
			{
				UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Removing child (too close) for junction %s"), 
					*ParentJunctionHologram->GetName());
				RemovePipeChild(ParentJunctionHologram, ExistingChild->Get());
				BuildingPipeChildren.Remove(ParentJunctionHologram);
			}
			// Issue #206: Also clean up Side B
			{TWeakObjectPtr<ASFPipelineHologram>* ChildB = BuildingPipeChildrenB.Find(ParentJunctionHologram);
			if (ChildB && ChildB->IsValid()) { RemovePipeChild(ParentJunctionHologram, ChildB->Get()); BuildingPipeChildrenB.Remove(ParentJunctionHologram); }}
			return; // No preview if connection would be too short
		}
		
		UE_LOG(LogSmartAutoConnect, Verbose,
			TEXT("   ✅ Valid connection found: Distance %.1fm"), BestDistance / 100.0f);
		
		// Track which connector index this junction is using (for child restrictions)
		int32 ChosenConnectorIdx = GetConnectorIndex(ParentJunctionHologram, BestJunctionConnector);
		if (ChosenConnectorIdx != INDEX_NONE)
		{
			ParentConnectorIndices.Add(ParentJunctionHologram, ChosenConnectorIdx);
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Tracking connector index %d for junction %s (Side A: %s → %s)"), 
				ChosenConnectorIdx, *ParentJunctionHologram->GetName(),
				*BestJunctionConnector->GetName(), *BestBuildingConnector->GetName());
		}
		
		UE_LOG(LogSmartAutoConnect, VeryVerbose,
			TEXT("PipeAutoConnectManager::ProcessPipeJunctions - Selected pair: JunctionConn=%s (idx=%d) BuildingConn=%s Dist=%.1f cm"),
			*BestJunctionConnector->GetName(), ChosenConnectorIdx, *BestBuildingConnector->GetName(), BestDistance);

		// Create or update pipe preview
		UWorld* World = ParentJunctionHologram->GetWorld();
		if (World)
		{
			// Check if we already have a preview for this junction
			TSharedPtr<FPipePreviewHelper>* ExistingPreview = JunctionPipePreviews.Find(ParentJunctionHologram);
			
			if (ExistingPreview)
			{
				UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Found EXISTING preview for junction %s (ptr=%p, total previews=%d)"), 
					*ParentJunctionHologram->GetName(), ParentJunctionHologram, JunctionPipePreviews.Num());
			}
			else
			{
				UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: NO existing preview for junction %s (ptr=%p, total previews=%d)"), 
					*ParentJunctionHologram->GetName(), ParentJunctionHologram, JunctionPipePreviews.Num());
			}
			
			// Check if this junction was previously using a different connector
			if (SharedReservedConnectors)
			{
				UFGPipeConnectionComponent* OldConnector = nullptr;
				for (auto& Pair : *SharedReservedConnectors)
				{
					if (Pair.Value == ParentJunctionHologram && Pair.Key != BestBuildingConnector)
					{
						OldConnector = Pair.Key;
						break;
					}
				}
				
				// Unreserve old connector if junction moved to a different one
				if (OldConnector)
				{
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Unreserving old connector %s"), *OldConnector->GetName());
					SharedReservedConnectors->Remove(OldConnector);
				}
			}
			
			// Check if we already have a child hologram for this junction
			TWeakObjectPtr<ASFPipelineHologram>* ExistingChild = BuildingPipeChildren.Find(ParentJunctionHologram);
			
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD LOOKUP: Junction=%s, Found=%s, TotalChildren=%d"),
				*ParentJunctionHologram->GetName(), 
				(ExistingChild && ExistingChild->IsValid()) ? TEXT("YES") : TEXT("NO"),
				BuildingPipeChildren.Num());
			
			if (ExistingChild && ExistingChild->IsValid())
			{
				// UPDATE existing child hologram with new spline data (matches stackable pipe pattern)
				ASFPipelineHologram* PipeChild = ExistingChild->Get();
				
				// CHECK: If pipe tier/style settings changed, destroy and recreate with new tier
				const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
				int32 ConfigTier = RuntimeSettings.PipeTierToBuilding;  // Building connections use PipeTierToBuilding
				bool bWithIndicator = RuntimeSettings.bPipeIndicator;
				
				AFGPlayerController* PlayerController = Cast<AFGPlayerController>(PipeChild->GetWorld()->GetFirstPlayerController());
				UClass* ExpectedBuildClass = Subsystem->GetPipeClassFromConfig(ConfigTier, bWithIndicator, PlayerController);
				UClass* CurrentBuildClass = PipeChild->GetBuildClass();
				
				if (ExpectedBuildClass && CurrentBuildClass != ExpectedBuildClass)
				{
					// Tier/style changed - destroy existing and spawn new with correct tier
					UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Tier changed - recreating pipe for junction %s (old=%s, new=%s)"),
						*ParentJunctionHologram->GetName(),
						CurrentBuildClass ? *CurrentBuildClass->GetName() : TEXT("null"),
						*ExpectedBuildClass->GetName());
					
					RemovePipeChild(ParentJunctionHologram, PipeChild);
					BuildingPipeChildren.Remove(ParentJunctionHologram);
					
					// Spawn new child with correct tier
					ASFPipelineHologram* NewChild = SpawnPipeChild(
						ParentJunctionHologram,
						BestJunctionConnector,
						BestBuildingConnector,
						false);  // bIsManifold = false (junction→building)
					
					if (NewChild)
					{
						BuildingPipeChildren.Add(ParentJunctionHologram, NewChild);
						UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Recreated with new tier for junction %s"),
							*ParentJunctionHologram->GetName());
					}
					// Skip the update logic below since we just spawned a new child
				}
				else
				{
					// CRITICAL: Temporarily unlock child before updating (from HologramHelper pattern)
					// Parent lock blocks child transform updates - must unlock, update, then re-lock
					bool bParentLocked = ParentJunctionHologram->IsHologramLocked();
					bool bChildWasLocked = PipeChild->IsHologramLocked();
					if (bChildWasLocked)
					{
						PipeChild->LockHologramPosition(false);  // Unlock for update
					}
					
					// CRITICAL: Calculate connector positions using actor location + relative offset
					// This matches the stackable pipe pattern and works correctly when parent is locked
					// GetComponentLocation() may return stale positions when hologram is locked
					FVector JunctionPos = ParentJunctionHologram->GetActorLocation();
					FRotator JunctionRot = ParentJunctionHologram->GetActorRotation();
					FVector ConnectorRelative = BestJunctionConnector->GetRelativeLocation();
					FVector StartPos = JunctionPos + JunctionRot.RotateVector(ConnectorRelative);
					
					// Building connector uses GetComponentLocation since buildings don't move
					FVector EndPos = BestBuildingConnector->GetComponentLocation();
				
					// Calculate normals based on junction rotation (more reliable than GetConnectorNormal when locked)
					FVector StartNormal = JunctionRot.RotateVector(BestJunctionConnector->GetRelativeRotation().Vector());
					if (StartNormal.IsNearlyZero())
					{
						StartNormal = BestJunctionConnector->GetConnectorNormal();
					}
					FVector EndNormal = BestBuildingConnector->GetConnectorNormal();
				
					UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE UPDATE: Junction=%s @ %s, Start=%s, End=%s (parentLocked=%d)"),
						*ParentJunctionHologram->GetName(), *JunctionPos.ToString(), *StartPos.ToString(), *EndPos.ToString(), bParentLocked ? 1 : 0);
					
					// Update position and spline data (RuntimeSettings already declared above for tier check)
					PipeChild->SetActorLocation(StartPos);
					PipeChild->SetRoutingMode(RuntimeSettings.PipeRoutingMode);
					if (!PipeChild->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
					{
						// Fallback: Generate spline data manually
						const float Distance = FVector::Dist(StartPos, EndPos);
						const float SmallTangent = 50.0f;
						const float LargeTangent = Distance * 0.435f;
						const float FlatSectionLength = Distance * 0.047f;
						const float TransitionOffset = Distance * 0.070f;
						FVector Direction = (EndPos - StartPos).GetSafeNormal();
						
						TArray<FSplinePointData> SplinePoints;
						
						FSplinePointData Point0;
						Point0.Location = StartPos;
						Point0.ArriveTangent = StartNormal * SmallTangent;
						Point0.LeaveTangent = StartNormal * SmallTangent;
						SplinePoints.Add(Point0);
						
						FSplinePointData Point1;
						Point1.Location = StartPos + StartNormal * FlatSectionLength;
						Point1.ArriveTangent = StartNormal * SmallTangent;
						Point1.LeaveTangent = StartNormal * (SmallTangent * 0.99f);
						SplinePoints.Add(Point1);
						
						FSplinePointData Point2;
						Point2.Location = StartPos + StartNormal * TransitionOffset;
						Point2.ArriveTangent = StartNormal * SmallTangent;
						Point2.LeaveTangent = Direction * LargeTangent;
						SplinePoints.Add(Point2);
						
						FSplinePointData Point3;
						Point3.Location = EndPos + EndNormal * TransitionOffset;
						Point3.ArriveTangent = Direction * LargeTangent;
						Point3.LeaveTangent = -EndNormal * SmallTangent;
						SplinePoints.Add(Point3);
						
						FSplinePointData Point4;
						Point4.Location = EndPos + EndNormal * FlatSectionLength;
						Point4.ArriveTangent = -EndNormal * (SmallTangent * 0.99f);
						Point4.LeaveTangent = -EndNormal * SmallTangent;
						SplinePoints.Add(Point4);
						
						FSplinePointData Point5;
						Point5.Location = EndPos;
						Point5.ArriveTangent = -EndNormal * SmallTangent;
						Point5.LeaveTangent = -EndNormal * SmallTangent;
						SplinePoints.Add(Point5);
						
						PipeChild->SetSplineDataAndUpdate(SplinePoints);
					}
					
					// Regenerate mesh and apply hologram material
					PipeChild->TriggerMeshGeneration();
					PipeChild->ForceApplyHologramMaterial();
					
					// CRITICAL: Re-lock child if parent is locked (from HologramHelper pattern)
					// Children must match parent's lock state for visibility
					if (bParentLocked)
					{
						PipeChild->LockHologramPosition(true);
					}
					PipeChild->SetActorHiddenInGame(false);
					PipeChild->SetPlacementMaterialState(ParentJunctionHologram->GetHologramMaterialState());
					
					UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Updated existing child for junction %s (parentLocked=%d, childRelocked=%d)"), 
						*ParentJunctionHologram->GetName(), bParentLocked ? 1 : 0, bParentLocked ? 1 : 0);
				}  // End of else block for tier-unchanged update
			}
			else
			{
				// Spawn new child hologram using stackable pipe pattern (ASFPipelineHologram + AddChild)
				ASFPipelineHologram* NewChild = SpawnPipeChild(
					ParentJunctionHologram,
					BestJunctionConnector,
					BestBuildingConnector,
					false);  // bIsManifold = false (junction→building)
				
				if (NewChild)
				{
					BuildingPipeChildren.Add(ParentJunctionHologram, NewChild);
					UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Created building pipe for junction %s (now have %d children)"),
						*ParentJunctionHologram->GetName(), BuildingPipeChildren.Num());
				}
			}
			
			// DEPRECATED: Keep old preview system for transition (will be removed)
			if (ExistingPreview && ExistingPreview->IsValid())
			{
				// Update existing preview
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("Updating existing pipe preview (DEPRECATED)"));
				(*ExistingPreview)->UpdatePreview(BestJunctionConnector, BestBuildingConnector);
			}
			
			// Reserve this connector for this junction
			if (SharedReservedConnectors)
			{
				SharedReservedConnectors->Add(BestBuildingConnector, ParentJunctionHologram);
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Reserved connector %s for junction %s"),
					*BestBuildingConnector->GetName(), *ParentJunctionHologram->GetName());
			}
			
			// ========================================
			// Issue #206: Side B — Opposite-side building connection
			// A 4-way junction has 2 connectors for manifold and 2 for buildings.
			// Side A (above) found the best building. Now search the OPPOSITE
			// junction connector for a building on the other side.
			// ========================================
			{
				int32 OppositeIdx = GetOppositeConnectorByNormal(ChosenConnectorIdx, JunctionConnectors);
				
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   SIDE B: ChosenIdx=%d OppositeIdx=%d TotalConnectors=%d NearbyBuildings=%d"),
					ChosenConnectorIdx, OppositeIdx, JunctionConnectors.Num(), NearbyBuildings.Num());
				
				if (OppositeIdx != INDEX_NONE && OppositeIdx < JunctionConnectors.Num())
				{
					UFGPipeConnectionComponent* OppositeJunctionConn = JunctionConnectors[OppositeIdx];
					
					// Search for best building connector facing the opposite junction connector
					UFGPipeConnectionComponent* BestBConn = nullptr;
					float BestBScore = TNumericLimits<float>::Max();
					float BestBDist = TNumericLimits<float>::Max();
					
					for (AFGBuildable* Building : NearbyBuildings)
					{
						if (!Building) continue;
						
						// Same building type filters as Side A
						if (Building->IsA(AFGBuildablePipeline::StaticClass())) continue;
						FString BClassName = Building->GetClass()->GetFName().ToString();
						if (BClassName.Contains(TEXT("PipelineSupport")) || BClassName.Contains(TEXT("PipeSupport"))) continue;
						if (Building->IsA(AFGBuildableStorage::StaticClass())) continue;
						if (Building->IsA(AFGBuildablePassthrough::StaticClass()))
						{
							TArray<UFGPipeConnectionComponent*> PassConns;
							FSFPipeConnectorFinder::GetPipeConnectors(Building, PassConns);
							if (PassConns.Num() == 0) continue;
						}
						else if (!Building->IsA(AFGBuildableFactory::StaticClass()))
						{
							continue;
						}
						if (BClassName.Contains(TEXT("PipelineJunction"))) continue;
						
						TArray<UFGPipeConnectionComponent*> PipeConns;
						FSFPipeConnectorFinder::GetPipeConnectors(Building, PipeConns);
						
						for (UFGPipeConnectionComponent* BuildingConn : PipeConns)
						{
							if (!BuildingConn) continue;
							
							// Skip connectors already reserved (including Side A)
							if (SharedReservedConnectors)
							{
								AFGHologram** Claim = SharedReservedConnectors->Find(BuildingConn);
								if (Claim && *Claim != ParentJunctionHologram) continue;
							}
							// Skip the Side A building connector
							if (BuildingConn == BestBuildingConnector) continue;
							
							// Type compatibility: junction and building connectors must be opposite types
							EPipeConnectionType JConnType = OppositeJunctionConn->GetPipeConnectionType();
							EPipeConnectionType BConnType = BuildingConn->GetPipeConnectionType();
							if (JConnType == BConnType) continue;
							
							FVector BuildingConnPos = BuildingConn->GetComponentLocation();
							// #425: gate distance/direction on the OPPOSITE CONNECTOR, not the junction center, so
							// the scoring matches the facing validation below (center-vs-connector offset can
							// otherwise admit/reject the wrong building connector on large or rotated junctions).
							float Distance = FVector::Dist2D(OppositeJunctionConn->GetComponentLocation(), BuildingConnPos);
							
							// Scoring: distance + alignment penalty (same formula as Side A)
							// [#464] Plus level affinity (same fix as belts): prefer same-level building
							// connectors; cross-level remains a fallback.
							FVector DirToB = (BuildingConnPos - OppositeJunctionConn->GetComponentLocation()).GetSafeNormal();
							float FwdDot = FMath::Abs(FVector::DotProduct(DirToB, JunctionForward));
							float RtDot = FMath::Abs(FVector::DotProduct(DirToB, JunctionRight));
							float MaxAlign = FMath::Max(FwdDot, RtDot);
							float AngPen = 1.0f - MaxAlign;
							float Score = Distance * (1.0f + AngPen * 10.0f)
								+ USFAutoConnectService::LevelAffinityPenalty(OppositeJunctionConn->GetComponentLocation(), BuildingConnPos);
							
							if (Score < BestBScore)
							{
								// Validate facing/angle (same checks as Side A)
								FVector OppositeConnPos = OppositeJunctionConn->GetComponentLocation();
								FVector J2B = (BuildingConnPos - OppositeConnPos).GetSafeNormal();
								FVector JNorm = OppositeJunctionConn->GetConnectorNormal();
								FVector BNorm = BuildingConn->GetConnectorNormal();
								
								bool bNormalsOpp = (FVector::DotProduct(JNorm, BNorm) < 0.0f);
								bool bBFaces = (FVector::DotProduct(BNorm, (OppositeConnPos - BuildingConnPos).GetSafeNormal()) > 0.5f);
								bool bJFaces = (FVector::DotProduct(JNorm, J2B) > 0.5f);
								bool bJAngle = FSFPipeConnectorFinder::IsConnectionAngleValid(OppositeJunctionConn, J2B, USFAutoConnectService::FACING_SANITY_ANGLE);
								bool bBAngle = FSFPipeConnectorFinder::IsConnectionAngleValid(BuildingConn, -J2B, USFAutoConnectService::FACING_SANITY_ANGLE);
								
								if (bNormalsOpp && bBFaces && bJFaces && bJAngle && bBAngle)
								{
									BestBScore = Score;
									BestBDist = Distance;
									BestBConn = BuildingConn;
								}
							}
						}
					}
					
					// Validate distance for Side B (same thresholds as Side A)
					constexpr float MaxDist = 2500.0f;
					constexpr float MinDistAngled = 200.0f;
					constexpr float MinDistStraight = 50.0f;
					constexpr float StraightThresh = 10.0f;
					
					bool bSideBValid = false;
					if (BestBConn && BestBDist <= MaxDist)
					{
						FVector J2B = (BestBConn->GetComponentLocation() - OppositeJunctionConn->GetComponentLocation()).GetSafeNormal();
						float AngRad = FMath::Acos(FMath::Clamp(FVector::DotProduct(OppositeJunctionConn->GetConnectorNormal(), J2B), -1.0f, 1.0f));
						float AngDeg = FMath::RadiansToDegrees(AngRad);
						float MinDist = (AngDeg <= StraightThresh) ? MinDistStraight : MinDistAngled;
						bSideBValid = (BestBDist >= MinDist);
					}
					
					if (bSideBValid)
					{
						ParentConnectorIndicesB.Add(ParentJunctionHologram, OppositeIdx);
						
						// Create or update Side B child
						TWeakObjectPtr<ASFPipelineHologram>* ExistingChildB = BuildingPipeChildrenB.Find(ParentJunctionHologram);
						
						if (ExistingChildB && ExistingChildB->IsValid())
						{
							// Update existing Side B child
							ASFPipelineHologram* PipeChildB = ExistingChildB->Get();
							
							// Check if tier changed
							const auto& RS = Subsystem->GetAutoConnectRuntimeSettings();
							int32 CTier = RS.PipeTierToBuilding;
							bool bInd = RS.bPipeIndicator;
							AFGPlayerController* PC = Cast<AFGPlayerController>(PipeChildB->GetWorld()->GetFirstPlayerController());
							UClass* ExpectedClass = Subsystem->GetPipeClassFromConfig(CTier, bInd, PC);
							UClass* CurrentClass = PipeChildB->GetBuildClass();
							
							if (ExpectedClass && CurrentClass != ExpectedClass)
							{
								// Tier changed - recreate
								RemovePipeChild(ParentJunctionHologram, PipeChildB);
								BuildingPipeChildrenB.Remove(ParentJunctionHologram);
								ASFPipelineHologram* NewChildB = SpawnPipeChild(ParentJunctionHologram, OppositeJunctionConn, BestBConn, false);
								if (NewChildB)
								{
									BuildingPipeChildrenB.Add(ParentJunctionHologram, NewChildB);
								}
							}
							else
							{
								// Update position and spline
								bool bPLocked = ParentJunctionHologram->IsHologramLocked();
								if (PipeChildB->IsHologramLocked()) PipeChildB->LockHologramPosition(false);
								
								FVector JPos = ParentJunctionHologram->GetActorLocation();
								FRotator JRot = ParentJunctionHologram->GetActorRotation();
								FVector ConnRel = OppositeJunctionConn->GetRelativeLocation();
								FVector StartPos = JPos + JRot.RotateVector(ConnRel);
								FVector EndPos = BestBConn->GetComponentLocation();
								FVector StartNorm = JRot.RotateVector(OppositeJunctionConn->GetRelativeRotation().Vector());
								if (StartNorm.IsNearlyZero()) StartNorm = OppositeJunctionConn->GetConnectorNormal();
								FVector EndNorm = BestBConn->GetConnectorNormal();
								
								PipeChildB->SetActorLocation(StartPos);
								PipeChildB->SetRoutingMode(RS.PipeRoutingMode);
								if (!PipeChildB->TryUseBuildModeRouting(StartPos, StartNorm, EndPos, EndNorm))
								{
									PipeChildB->SetupPipeSpline(OppositeJunctionConn, BestBConn);
								}
								PipeChildB->TriggerMeshGeneration();
								PipeChildB->ForceApplyHologramMaterial();
								
								if (bPLocked) PipeChildB->LockHologramPosition(true);
								PipeChildB->SetActorHiddenInGame(false);
								PipeChildB->SetPlacementMaterialState(ParentJunctionHologram->GetHologramMaterialState());
							}
						}
						else
						{
							// Spawn new Side B child
							ASFPipelineHologram* NewChildB = SpawnPipeChild(ParentJunctionHologram, OppositeJunctionConn, BestBConn, false);
							if (NewChildB)
							{
								BuildingPipeChildrenB.Add(ParentJunctionHologram, NewChildB);
								UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE SIDE B: Created building pipe for junction %s (connector[%d])"),
									*ParentJunctionHologram->GetName(), OppositeIdx);
							}
						}
						
						// Reserve Side B connector
						if (SharedReservedConnectors)
						{
							SharedReservedConnectors->Add(BestBConn, ParentJunctionHologram);
						}
						
						UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   ✅ Side B: Valid connection found: Connector[%d] Distance %.1fm"),
							OppositeIdx, BestBDist / 100.0f);
					}
					else
					{
						// No valid Side B connection - clean up existing child
						TWeakObjectPtr<ASFPipelineHologram>* ExistingChildB = BuildingPipeChildrenB.Find(ParentJunctionHologram);
						if (ExistingChildB && ExistingChildB->IsValid())
						{
							RemovePipeChild(ParentJunctionHologram, ExistingChildB->Get());
							BuildingPipeChildrenB.Remove(ParentJunctionHologram);
						}
					}
				}
			}
		}
	}
	else
	{
		// CRITICAL FIX: No valid connector pair - DON'T destroy existing preview
		// Keep preview alive - connector might be valid again next frame (prevents flickering)
		// Only destroy previews on explicit cleanup, not during evaluation (matches belt behavior)
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("No valid pair for junction %s (keeping existing preview if any)"), 
			*ParentJunctionHologram->GetName());
		
		// Note: We don't unreserve connectors here either - let the reservation map clear naturally
		// on next evaluation cycle. This prevents preview destruction/recreation cycles.
	}
}

void FSFPipeAutoConnectManager::ClearPipePreviews()
{
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Clearing all pipe children (%d building, %d buildingB, %d manifold, %d floorHole)"), 
		BuildingPipeChildren.Num(), BuildingPipeChildrenB.Num(), ManifoldPipeChildren.Num(), FloorHolePipeChildren.Num());
	
	// Clear Phase 1 child holograms (junction-to-building)
	for (auto& Pair : BuildingPipeChildren)
	{
		if (Pair.Value.IsValid() && Pair.Key)
		{
			RemovePipeChild(Pair.Key, Pair.Value.Get());
		}
	}
	BuildingPipeChildren.Empty();
	
	// Issue #206: Clear Phase 1 Side B child holograms
	for (auto& Pair : BuildingPipeChildrenB)
	{
		if (Pair.Value.IsValid() && Pair.Key)
		{
			RemovePipeChild(Pair.Key, Pair.Value.Get());
		}
	}
	BuildingPipeChildrenB.Empty();
	
	// Clear Phase 2 child holograms (junction-to-junction manifolds)
	for (auto& Pair : ManifoldPipeChildren)
	{
		if (Pair.Value.IsValid() && Pair.Key)
		{
			RemovePipeChild(Pair.Key, Pair.Value.Get());
		}
	}
	ManifoldPipeChildren.Empty();
	ManifoldTargetJunctions.Empty();  // Clear target junction tracking
	
	// Issue #187: Clear floor hole pipe child holograms
	for (auto& Pair : FloorHolePipeChildren)
	{
		if (Pair.Value.IsValid() && Pair.Key)
		{
			RemovePipeChild(Pair.Key, Pair.Value.Get());
		}
	}
	FloorHolePipeChildren.Empty();
	
	// DEPRECATED: Clear old preview helpers (will be removed)
	for (auto& Pair : JunctionPipePreviews)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->DestroyPreview();
		}
	}
	for (auto& Pair : ManifoldPipePreviews)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->DestroyPreview();
		}
	}
	JunctionPipePreviews.Empty();
	ManifoldPipePreviews.Empty();
	
	LastJunctionTransforms.Empty();  // Clear transform cache
	ParentConnectorIndices.Empty();  // Clear connector index tracking
	ParentConnectorIndicesB.Empty(); // Issue #206: Clear Side B connector index tracking
	ReservedConnectors.Empty();      // Clear persistent connector reservations
}

bool FSFPipeAutoConnectManager::CreatePipePreviewBetweenConnectors(
	UFGPipeConnectionComponent* SourceConnector,
	UFGPipeConnectionComponent* TargetConnector,
	AFGHologram* StorageHologram,
	int32 PipeTier,
	bool bWithIndicator)
{
	if (!SourceConnector || !TargetConnector || !StorageHologram || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 CreatePipePreviewBetweenConnectors: Invalid parameters"));
		return false;
	}

	UWorld* World = Subsystem->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 CreatePipePreviewBetweenConnectors: No world"));
		return false;
	}

	// Check if we already have a preview for this hologram
	TSharedPtr<FPipePreviewHelper>* ExistingPreview = JunctionPipePreviews.Find(StorageHologram);
	
	if (ExistingPreview && ExistingPreview->IsValid())
	{
		// Update existing preview
		(*ExistingPreview)->UpdatePreview(SourceConnector, TargetConnector);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🔧 Updated existing pipe preview for %s"), *StorageHologram->GetName());
		return true;
	}

	// Create new pipe preview helper
	// Constructor: (UWorld*, int32 PipeTier, bool bWithIndicator, AFGHologram* ParentJunction)
	TSharedPtr<FPipePreviewHelper> NewPreview = MakeShared<FPipePreviewHelper>(World, PipeTier, bWithIndicator, StorageHologram);
	if (!NewPreview.IsValid())
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 Failed to create pipe preview helper"));
		return false;
	}

	// Update the preview with connector positions
	NewPreview->UpdatePreview(SourceConnector, TargetConnector);

	// Store the preview
	JunctionPipePreviews.Add(StorageHologram, NewPreview);

	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 Created pipe preview between connectors for %s (Tier=%d, Style=%s)"), 
		*StorageHologram->GetName(), PipeTier, bWithIndicator ? TEXT("Normal") : TEXT("Clean"));

	return true;
}

void FSFPipeAutoConnectManager::CleanupOrphanedPreviews(const TArray<AFGHologram*>& CurrentJunctions)
{
	// Find and remove pipe children for junctions that no longer exist (removed children)
	TArray<AFGHologram*> JunctionsToRemove;
	
	// Check for orphaned building pipe children (Side A)
	for (auto& Pair : BuildingPipeChildren)
	{
		AFGHologram* Junction = Pair.Key;
		
		if (!CurrentJunctions.Contains(Junction))
		{
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Cleaning up orphaned building pipe child for removed junction %s"), 
				*GetNameSafe(Junction));
			
			if (Pair.Value.IsValid() && Junction)
			{
				RemovePipeChild(Junction, Pair.Value.Get());
			}
			
			JunctionsToRemove.Add(Junction);
		}
	}
	
	// Issue #206: Check for orphaned building pipe children (Side B)
	for (auto& Pair : BuildingPipeChildrenB)
	{
		AFGHologram* Junction = Pair.Key;
		
		if (!CurrentJunctions.Contains(Junction))
		{
			if (Pair.Value.IsValid() && Junction)
			{
				RemovePipeChild(Junction, Pair.Value.Get());
			}
			
			if (!JunctionsToRemove.Contains(Junction))
			{
				JunctionsToRemove.Add(Junction);
			}
		}
	}
	
	// Check for orphaned manifold pipe children (source junction removed)
	for (auto& Pair : ManifoldPipeChildren)
	{
		AFGHologram* Junction = Pair.Key;
		
		if (!CurrentJunctions.Contains(Junction))
		{
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Cleaning up orphaned manifold pipe child for junction %s (source removed)"), 
				*GetNameSafe(Junction));
			
			// CRITICAL: When source junction is removed, it may already be invalid
			// Destroy the pipe child directly instead of trying to remove from invalid parent
			if (Pair.Value.IsValid())
			{
				ASFPipelineHologram* PipeChild = Pair.Value.Get();
				UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Destroying orphaned %s (parent removed)"), *PipeChild->GetName());
				PipeChild->Destroy();
			}
			
			if (!JunctionsToRemove.Contains(Junction))
			{
				JunctionsToRemove.Add(Junction);
			}
		}
	}
	
	// Check for manifold pipes whose TARGET junction was removed (source still exists)
	TArray<AFGHologram*> ManifoldTargetsToRemove;
	for (auto& Pair : ManifoldTargetJunctions)
	{
		AFGHologram* SourceJunction = Pair.Key;
		AFGHologram* TargetJunction = Pair.Value.Get();
		
		// If target junction is invalid or no longer in current junctions, clean up the manifold pipe
		// BUT keep the source junction - it still exists and should keep its building pipe
		if (!TargetJunction || !CurrentJunctions.Contains(TargetJunction))
		{
			TWeakObjectPtr<ASFPipelineHologram>* ManifoldChild = ManifoldPipeChildren.Find(SourceJunction);
			if (ManifoldChild && ManifoldChild->IsValid())
			{
				UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Cleaning up manifold pipe child for %s (target %s removed)"), 
					*GetNameSafe(SourceJunction), *GetNameSafe(TargetJunction));
				
				RemovePipeChild(SourceJunction, ManifoldChild->Get());
				ManifoldPipeChildren.Remove(SourceJunction);
			}
			
			// Mark for removal from ManifoldTargetJunctions only - NOT from JunctionsToRemove
			// The source junction is still valid and should keep its building pipe
			ManifoldTargetsToRemove.Add(SourceJunction);
		}
	}
	
	// Remove stale entries from ManifoldTargetJunctions
	for (AFGHologram* SourceJunction : ManifoldTargetsToRemove)
	{
		ManifoldTargetJunctions.Remove(SourceJunction);
	}
	
	// DEPRECATED: Check old preview helpers
	for (auto& Pair : JunctionPipePreviews)
	{
		AFGHologram* Junction = Pair.Key;
		
		if (!CurrentJunctions.Contains(Junction))
		{
			if (Pair.Value.IsValid())
			{
				Pair.Value->DestroyPreview();
			}
			
			if (!JunctionsToRemove.Contains(Junction))
			{
				JunctionsToRemove.Add(Junction);
			}
		}
	}
	
	for (auto& Pair : ManifoldPipePreviews)
	{
		AFGHologram* Junction = Pair.Key;
		
		if (!CurrentJunctions.Contains(Junction))
		{
			if (Pair.Value.IsValid())
			{
				Pair.Value->DestroyPreview();
			}
			
			if (!JunctionsToRemove.Contains(Junction))
			{
				JunctionsToRemove.Add(Junction);
			}
		}
	}
	
	// Remove from all maps and unreserve connectors
	for (AFGHologram* Junction : JunctionsToRemove)
	{
		BuildingPipeChildren.Remove(Junction);
		BuildingPipeChildrenB.Remove(Junction);  // Issue #206
		ManifoldPipeChildren.Remove(Junction);
		ManifoldTargetJunctions.Remove(Junction);
		JunctionPipePreviews.Remove(Junction);
		ManifoldPipePreviews.Remove(Junction);
		LastJunctionTransforms.Remove(Junction);
		ParentConnectorIndices.Remove(Junction);
		ParentConnectorIndicesB.Remove(Junction);  // Issue #206
		
		// Unreserve any connectors claimed by this orphaned junction
		for (auto It = ReservedConnectors.CreateIterator(); It; ++It)
		{
			if (It.Value() == Junction)
			{
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Unreserving connector %s (junction removed)"), *It.Key()->GetName());
				It.RemoveCurrent();
			}
		}
	}
	
	if (JunctionsToRemove.Num() > 0)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Cleaned up %d orphaned preview(s)"), JunctionsToRemove.Num());
	}
}
