#include "Features/AutoConnect/SFAutoConnectService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
// NOTE: SFDeferredCostService removed - child holograms automatically aggregate costs via GetCost()
#include "Subsystem/SFHologramDataService.h"
#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Hologram/FGPipelineHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Logging/LogMacros.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableManufacturer.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorAttachment.h"
#include "FGBuildableSubsystem.h"
#include "Buildables/FGBuildableStorage.h"
#include "FGPipeConnectionComponent.h"
#include "UObject/UnrealType.h"
#include "FGCentralStorageSubsystem.h"
#include "FGInventoryComponent.h"
#include "FGItemDescriptor.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "FGConstructDisqualifier.h"
#include "FGGameState.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Kismet/GameplayStatics.h"

USFAutoConnectService::USFAutoConnectService()
	: Subsystem(nullptr)
{
}

void USFAutoConnectService::Init(USFSubsystem* InSubsystem)
{
	if (!InSubsystem)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("SFAutoConnectService::Init: Subsystem is null"));
		return;
	}
	
	Subsystem = InSubsystem;
	UE_LOG(LogSmartFoundations, Log, TEXT("Auto-Connect Service initialized with BUILDING_SEARCH_RADIUS=%.0f cm"), BUILDING_SEARCH_RADIUS);
}

void USFAutoConnectService::Shutdown()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("SFAutoConnectService shutting down"));
	ClearBeltPreviewHelpers();
	Subsystem = nullptr;
}

void USFAutoConnectService::ClearBeltPreviewHelpers()
{
	if (DistributorBeltPreviews.Num() == 0)
	{
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🗑️ Clearing all belt preview helpers (%d distributors)"), 
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
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ All belt preview helpers cleared"));
}

// ========================================
// Main Entry Points
// ========================================

void USFAutoConnectService::OnDistributorHologramUpdated(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("OnDistributorHologramUpdated: ParentHologram is null"));
		return;
	}
	
	// PARENT HOLOGRAM CHECK: Only process actual distributors (splitters/mergers)
	if (!IsDistributorHologram(ParentHologram))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⏭️ Skipping non-distributor hologram: %s"), 
			*ParentHologram->GetName());
		return;
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 Parent hologram updated: %s - processing distributors"), *ParentHologram->GetName());
	
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
	
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Distributor update complete for %s (locked=%d)"), 
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🗑️ No belt previews created - removed distributor %s from map"), 
			*ParentHologram->GetName());
	}
	
	// NOTE: Child distributors are processed ONLY in the completion callback
	// See SFSubsystem.cpp completion callback in UpdateChildPositions()
	// This ensures children are positioned before their belt previews are created
}

TArray<TSharedPtr<FBeltPreviewHelper>> USFAutoConnectService::ProcessSingleDistributor(
	AFGHologram* DistributorHologram,
	TMap<UFGFactoryConnectionComponent*, AFGHologram*>* ReservedInputs)
{
	if (!DistributorHologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ProcessSingleDistributor: DistributorHologram is null"));
		return TArray<TSharedPtr<FBeltPreviewHelper>>();
	}
	
	// Get runtime settings from subsystem
	if (!Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ProcessSingleDistributor: Subsystem is null"));
		return TArray<TSharedPtr<FBeltPreviewHelper>>();
	}
	
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	
	// Check if auto-connect is enabled
	if (!RuntimeSettings.bEnabled)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ProcessSingleDistributor: Auto-connect disabled in runtime settings"));
		return TArray<TSharedPtr<FBeltPreviewHelper>>();
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Processing distributor: %s%s"), 
		*DistributorHologram->GetName(),
		ReservedInputs ? TEXT(" (with input reservation)") : TEXT(""));
	
	// Check if we already have belt previews for this distributor
	TArray<TSharedPtr<FBeltPreviewHelper>> BeltPreviewHelpers;
	if (TArray<TSharedPtr<FBeltPreviewHelper>>* ExistingPreviews = DistributorBeltPreviews.Find(DistributorHologram))
	{
		// Start with existing previews - we'll update or replace them
		BeltPreviewHelpers = *ExistingPreviews;
		
		// NOTE: Don't update tiers here - chain helpers use BeltTierMain while building helpers use BeltTierToBuilding
		// Tiers will be checked and updated when helpers are actually reused for building connections
	}

	// Get the buildable class from the distributor hologram
	UClass* BuildClass = DistributorHologram->GetBuildClass();
	if (!BuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ProcessSingleDistributor: BuildClass is null"));
		return BeltPreviewHelpers;
	}
	
	// Determine if this is a splitter or merger based on build class name
	FString BuildClassName = BuildClass->GetFName().ToString();
	bool bIsSplitter = BuildClassName.Contains(TEXT("Splitter"));
	bool bIsMerger = BuildClassName.Contains(TEXT("Merger"));
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Build class: %s | IsSplitter=%d | IsMerger=%d"), 
		*BuildClassName, bIsSplitter, bIsMerger);
	
	// Find distributor chains for manifold connections
	TArray<AFGHologram*> DistributorChains;
	FindDistributorChains(DistributorHologram, DistributorChains);
	
	// Find compatible buildings nearby
	TArray<AFGBuildable*> CompatibleBuildings;
	FindCompatibleBuildingsForDistributor(DistributorHologram, CompatibleBuildings);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🏭 Found %d compatible buildings nearby"), CompatibleBuildings.Num());
	
	// If no buildings or chains found, clean up existing previews and return empty array
	if (CompatibleBuildings.Num() == 0 && DistributorChains.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ⚠️ No compatible buildings or chains found - cleaning up existing belt previews"));
		
		// Clean up all existing belt previews (out of range)
		if (BeltPreviewHelpers.Num() > 0)
		{
			for (TSharedPtr<FBeltPreviewHelper>& Helper : BeltPreviewHelpers)
			{
				if (Helper.IsValid())
				{
					Helper->DestroyPreview();
				}
			}
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🧹 Cleaned up %d belt previews (all buildings out of range)"), 
				BeltPreviewHelpers.Num());
			BeltPreviewHelpers.Empty();
		}
		
		return BeltPreviewHelpers;
	}
	
	// If no existing belt previews, create them (first time)
	if (BeltPreviewHelpers.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 No existing belt previews - creating new ones"));
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Updating %d existing belt previews"), BeltPreviewHelpers.Num());
	}
	
	// Get distributor hologram's output connectors
	// Note: Query hologram actor directly - it has connector components with proper positions
	TArray<UFGFactoryConnectionComponent*> DistributorOutputs;
	TArray<UFGFactoryConnectionComponent*> DistributorInputs;
	
	// Query connector components directly from the hologram actor
	TArray<UFGFactoryConnectionComponent*> HologramConnectors;
	DistributorHologram->GetComponents<UFGFactoryConnectionComponent>(HologramConnectors);
	
	// Separate inputs and outputs using GetDirection()
	for (UFGFactoryConnectionComponent* Connector : HologramConnectors)
	{
		if (!Connector)
			continue;
			
		if (Connector->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
		{
			DistributorOutputs.Add(Connector);
		}
		else if (Connector->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
		{
			DistributorInputs.Add(Connector);
		}
	}
	
	// Debug: Log hologram class and connector counts
	UClass* DistributorBuildClass = DistributorHologram->GetBuildClass();
	if (DistributorBuildClass)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔍 Distributor hologram class: %s"), *DistributorBuildClass->GetName());
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   📋 Found %d inputs and %d outputs from hologram connectors"), 
			DistributorInputs.Num(), DistributorOutputs.Num());
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("   ❌ Failed to get build class from distributor hologram"));
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Distributor has %d inputs and %d outputs available"), 
		DistributorInputs.Num(), DistributorOutputs.Num());
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🏷️ Distributor type: %s"), 
		bIsSplitter ? TEXT("Splitter") : bIsMerger ? TEXT("Merger") : TEXT("Unknown"));
	
	// Count available side connectors (middle is reserved for manifold chaining)
	int32 SideConnectorCount = bIsSplitter ? (DistributorOutputs.Num() - 1) : (DistributorInputs.Num() - 1);
	if (SideConnectorCount < 0)
		SideConnectorCount = 0;
	
	const TCHAR* ConnectorType = bIsSplitter ? TEXT("outputs") : TEXT("inputs");
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 MANIFOLD: Reserved middle connector for chaining, using %d side %s for buildings"), 
		SideConnectorCount, ConnectorType);
	
	// NEW: Process distributor chains first (priority 1 for manifold chaining)
	UE_LOG(LogSmartFoundations, Log, TEXT("   Processing distributor chains for manifold connections"));
	
	// Create belt previews for distributor-to-distributor chains
	if (RuntimeSettings.bChainDistributors && DistributorChains.Num() > 0)
	{
		for (AFGHologram* ChainTarget : DistributorChains)
		{
			if (!ChainTarget)
				continue;
			if (ChainTarget == DistributorHologram)
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("   ⚠️ Skipping distributor chain to self: %s"), *DistributorHologram->GetName());
				continue;
			}
				
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔗 Creating distributor chain belt preview: %s -> %s"), 
				*DistributorHologram->GetName(), *ChainTarget->GetName());
			
			// Get connectors for manifold chaining
			// CRITICAL: Must connect OUTPUT -> INPUT (previous code could produce OUTPUT -> OUTPUT)
			UFGFactoryConnectionComponent* SourceConnector = nullptr;
			UFGFactoryConnectionComponent* TargetConnector = nullptr;
			
			if (bIsSplitter)
			{
				// Splitter chains forward: current output (facing next) → next input (facing current)
				SourceConnector = FindConnectorFacingTarget(DistributorHologram, ChainTarget, EFactoryConnectionDirection::FCD_OUTPUT);
				TargetConnector = FindConnectorFacingTarget(ChainTarget, DistributorHologram, EFactoryConnectionDirection::FCD_INPUT);
			}
			else if (bIsMerger)
			{
				// Merger chains backward: previous output (facing current) → current input (facing previous)
				SourceConnector = FindConnectorFacingTarget(ChainTarget, DistributorHologram, EFactoryConnectionDirection::FCD_OUTPUT);
				TargetConnector = FindConnectorFacingTarget(DistributorHologram, ChainTarget, EFactoryConnectionDirection::FCD_INPUT);
			}
			
			// Create belt preview if we have compatible connectors
			if (SourceConnector && TargetConnector)
			{
				if (SourceConnector == TargetConnector)
				{
					UE_LOG(LogSmartFoundations, Error,
						TEXT("      ❌ Distributor chain produced self-connection (%s on %s, ptr=%p) - skipping"),
						*SourceConnector->GetName(),
						*GetNameSafe(SourceConnector->GetOwner()),
						SourceConnector);
					continue;
				}
				
				// DEDUPLICATION: Check if we already have a belt preview for this connector pair
				// This prevents creating duplicate belts on re-evaluation
				bool bAlreadyExists = false;
				for (const TSharedPtr<FBeltPreviewHelper>& ExistingHelper : BeltPreviewHelpers)
				{
					if (ExistingHelper.IsValid() && 
						ExistingHelper->GetOutputConnector() == SourceConnector &&
						ExistingHelper->GetInputConnector() == TargetConnector)
					{
						bAlreadyExists = true;
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⏭️ Manifold belt already exists for this connection - skipping"));
						break;
					}
				}
				
				if (bAlreadyExists)
				{
					continue;
				}
				
				// Create belt preview helper for manifold (distributor-to-distributor)
				// Use BeltTierMain for manifold connections
				// Handle "Auto" (tier 0) by getting highest unlocked tier
				int32 BeltTier = RuntimeSettings.BeltTierMain;
				if (BeltTier == 0)
				{
					AFGPlayerController* PlayerController = Cast<AFGPlayerController>(
						DistributorHologram->GetConstructionInstigator()->GetController());
					BeltTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
				}
				// Belt preview should be child of source distributor (who the belt comes from)
				AFGHologram* ParentDistributor = bIsSplitter ? DistributorHologram : ChainTarget;
				TSharedPtr<FBeltPreviewHelper> BeltHelper = MakeShared<FBeltPreviewHelper>(GetWorld(), BeltTier, ParentDistributor);
				if (BeltHelper.IsValid())
				{
					UE_LOG(LogSmartFoundations, VeryVerbose,
						TEXT("      Connecting distributor chain: %s on %s (ptr=%p) -> %s on %s (ptr=%p)"),
						*SourceConnector->GetName(),
						*GetNameSafe(SourceConnector->GetOwner()),
						SourceConnector,
						*TargetConnector->GetName(),
						*GetNameSafe(TargetConnector->GetOwner()),
						TargetConnector);
					
					// Update the preview with connection endpoints
					BeltHelper->UpdatePreview(SourceConnector, TargetConnector);
					
					// Store helper for cleanup
					BeltPreviewHelpers.Add(BeltHelper);
					
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ✅ Distributor chain belt preview created"));
				}
				else
				{
					UE_LOG(LogSmartFoundations, Error, TEXT("      Failed to create distributor chain belt preview helper"));
				}
			}
			else
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("      No compatible connectors for distributor chain (%s -> %s)"),
				*DistributorHologram->GetName(), *GetNameSafe(ChainTarget));
			}
		}
	}
	
	// Deduplicate buildings (same building may appear multiple times in search results)
	TSet<AFGBuildable*> UniqueBuildingsSet;
	for (AFGBuildable* Building : CompatibleBuildings)
	{
		UniqueBuildingsSet.Add(Building);
	}
	
	// PRIORITIZATION FIX: Sort buildings by distance from splitter
	// This ensures closest buildings get outputs assigned first
	TArray<AFGBuildable*> SortedBuildings = UniqueBuildingsSet.Array();
	FVector SplitterPos = DistributorHologram->GetActorLocation();
	
	SortedBuildings.Sort([SplitterPos](const AFGBuildable& A, const AFGBuildable& B) {
		const FVector AX = A.GetActorLocation();
		const FVector BX = B.GetActorLocation();
		const float DistA = FVector::Dist2D(SplitterPos, AX); // XY priority
		const float DistB = FVector::Dist2D(SplitterPos, BX);
		
		// Primary: Distance (ascending - closest first)
		if (FMath::Abs(DistA - DistB) > 1.0f)
		{
			return DistA < DistB;
		}
		
		// Secondary: Name (stability)
		return A.GetName() < B.GetName();
	});
	
	// Log sorted order with distances for debugging
	UE_LOG(LogSmartFoundations, Log, TEXT("   🔢 Sorted %d buildings by distance from splitter at %s:"), 
		SortedBuildings.Num(), *SplitterPos.ToString());
	for (int32 i = 0; i < SortedBuildings.Num(); i++)
	{
		if (SortedBuildings[i])
		{
			float Dist = FVector::Dist2D(SplitterPos, SortedBuildings[i]->GetActorLocation());
			UE_LOG(LogSmartFoundations, Log, TEXT("      [%d] %s @ %.0f cm"), 
				i, *SortedBuildings[i]->GetName(), Dist);
		}
	}
	
	// Update existing belt preview helpers or create new ones as needed
	// Track which helpers were updated to detect out-of-range buildings
	TArray<bool> HelperUpdated;
	if (BeltPreviewHelpers.Num() > 0)
	{
		HelperUpdated.SetNumZeroed(BeltPreviewHelpers.Num());
	}
	
	// Process all buildings - update existing helpers or create new ones as needed
	// MANIFOLD FIX: Track which distributor outputs have already been assigned to prevent duplicate connections
	TSet<UFGFactoryConnectionComponent*> AssignedOutputs;
	
	int32 HelperIndex = 0;
	
	// ORCHESTRATOR REFACTOR: Building connections are now handled by USFAutoConnectOrchestrator::EvaluateConnections()
	// which uses global scoring to prevent crossovers. Skip building connections here to avoid duplicate belts.
	// Only manifold chains (distributor-to-distributor) are still handled below.
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ⏭️ Skipping building connections - handled by Orchestrator"));
	
	// Skip to manifold chain section (building connections disabled)
	// The for loop below is kept but will be skipped via this goto-equivalent pattern
	if (true) // Building connections disabled - Orchestrator handles them
	{
		goto SkipBuildingConnections;
	}
	
	// Buildings are always connected when auto-connect is enabled
	// (No separate toggle needed - controlled by RuntimeSettings.bEnabled)
	for (AFGBuildable* Building : SortedBuildings)
		{
		UE_LOG(LogSmartFoundations, Log, TEXT("   🏭 [%s] Processing building %d: %s"), 
			*DistributorHologram->GetName(), HelperIndex, *Building->GetName());
		
		bool bIsUpdatingExisting = (HelperIndex < BeltPreviewHelpers.Num());
		
		if (bIsUpdatingExisting)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔄 Updating existing belt preview"));
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ✨ Creating new belt preview"));
		}
		
		// Get building connectors based on distributor type
		TArray<UFGFactoryConnectionComponent*> BuildingInputs;
		TArray<UFGFactoryConnectionComponent*> BuildingOutputsLocal;
		GetBuildingConnectors(Building, BuildingInputs, BuildingOutputsLocal);
		
		// Check if we have compatible connectors for this distributor type
		bool bHasCompatibleConnectors = false;
		if (bIsSplitter)
		{
			// Splitter: Need building inputs to connect to
			bHasCompatibleConnectors = SideConnectorCount > 0 && BuildingInputs.Num() > 0;
		}
		else if (bIsMerger)
		{
			// Merger: Need building outputs to connect from
			bHasCompatibleConnectors = SideConnectorCount > 0 && BuildingOutputsLocal.Num() > 0;
		}
		
		if (bHasCompatibleConnectors)
		{
			UFGFactoryConnectionComponent* ClosestConnector = nullptr;
			UFGFactoryConnectionComponent* BuildingConnector = nullptr;
			float ClosestDistance = FLT_MAX;
			
			if (bIsSplitter)
			{
				// MANIFOLD FIX: Find closest UNASSIGNED distributor output to building input
				// CROSSING FIX: Only connect outputs to buildings in their direction to prevent crossings
				// INPUT FIX: Find the closest building input, not just the first one
				
				// Find the closest building input to any of the splitter's outputs
				// If using reservation system, check if input is already reserved by a closer distributor
				UFGFactoryConnectionComponent* ClosestBuildingInput = nullptr;
				float ClosestBuildingInputDistance = FLT_MAX;
				
				for (UFGFactoryConnectionComponent* BuildingInput : BuildingInputs)
				{
					FVector BuildingInputPos = BuildingInput->GetComponentLocation();
					
					// CRITICAL: Check reservation FIRST before considering this input
					if (ReservedInputs && ReservedInputs->Contains(BuildingInput))
					{
						// This input is already reserved - skip it entirely
						AFGHologram* ReservedBy = ReservedInputs->FindRef(BuildingInput);
						UE_LOG(LogSmartFoundations, Log, 
							TEXT("      ⏭️ [%s] SKIP input %s - RESERVED by %s"),
							*DistributorHologram->GetName(), *BuildingInput->GetName(), *GetNameSafe(ReservedBy));
						continue;
					}
					
					// Calculate planar (XY) distance
					const float DistanceToSplitterXY = FVector::Dist2D(SplitterPos, BuildingInputPos);

					// ALIGNMENT PRIORITY: Calculate score based on distance weighted by alignment
					// This ensures we pick inputs that are "straight" relative to the distributor,
					// even if they are physically further away (e.g. offset inputs on Quantum Encoder)
					FVector SplitterToInputDir = (BuildingInputPos - SplitterPos).GetSafeNormal();
					FVector SplitterForward = DistributorHologram->GetActorForwardVector();
					FVector SplitterRight = DistributorHologram->GetActorRightVector();
					
					// Check alignment with cardinal directions (Forward/Back/Left/Right)
					float ForwardDot = FMath::Abs(FVector::DotProduct(SplitterToInputDir, SplitterForward));
					float RightDot = FMath::Abs(FVector::DotProduct(SplitterToInputDir, SplitterRight));
					float MaxAlignment = FMath::Max(ForwardDot, RightDot);
					
					// STRICT ALIGNMENT: Filter out inputs that are not roughly cardinal (within 45 degrees)
					// This prevents "crossing" to incorrect inputs that are closer but at weird angles
					if (MaxAlignment < MIN_ANGLE_ALIGNMENT)
					{
						UE_LOG(LogSmartFoundations, Log,
							TEXT("      ⏭️ [%s] SKIP input %s - ALIGNMENT too low (%.2f < %.3f)"),
							*DistributorHologram->GetName(), *BuildingInput->GetName(), MaxAlignment, MIN_ANGLE_ALIGNMENT);
						continue;
					}
					
					// Score = Distance * (1 + Penalty)
					// Heavy penalty for misalignment (Factor 10.0) to overcome depth differences
					// e.g. Straight (1.0) -> Score = Dist * 1.0
					// e.g. Angled (0.9) -> Score = Dist * 2.0
					float AnglePenalty = 1.0f - MaxAlignment;
					float Score = DistanceToSplitterXY * (1.0f + AnglePenalty * ANGLE_PENALTY_MULTIPLIER);

					if (Score < ClosestBuildingInputDistance)
					{
						ClosestBuildingInputDistance = Score;
						ClosestBuildingInput = BuildingInput;
					}
				}
				
				if (!ClosestBuildingInput)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("      ❌ [%s] NO VALID INPUTS found for building %s (all reserved or misaligned)"), 
						*DistributorHologram->GetName(), *Building->GetName());
					continue;
				}
				
				BuildingConnector = ClosestBuildingInput;
				FVector BuildingConnectorPos = BuildingConnector->GetComponentLocation();
				
				// CLIPPING FIX: Check if splitter is on correct side of building input
				// Building inputs have normals pointing outward - splitter should be in front, not behind
				FVector BuildingInputNormal = BuildingConnector->GetConnectorNormal();
				FVector SplitterToBuildingDirection = (BuildingConnectorPos - SplitterPos).GetSafeNormal();
				float InputSideAlignment = FVector::DotProduct(BuildingInputNormal, SplitterToBuildingDirection);
				
				if (InputSideAlignment > 0.0f)
				{
					// Splitter is BEHIND the input (wrong side) - belt would clip through building
					UE_LOG(LogSmartFoundations, Log, TEXT("      ❌ [%s] WRONG SIDE - splitter behind input %s (alignment %.2f > 0)"), 
						*DistributorHologram->GetName(), *BuildingConnector->GetName(), InputSideAlignment);
					continue;
				}
				
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Selected closest building input: %s (Score: %.0f, side alignment: %.2f)"), 
					*BuildingConnector->GetName(), ClosestBuildingInputDistance, InputSideAlignment);
				
				// Get all side outputs (sorted left-to-right, excluding middle)
				TArray<UFGFactoryConnectionComponent*> SideOutputs;
				GetAllSideConnectors(DistributorHologram, SideOutputs);
				
				// Find the best side output for this building (angle check handled by CreateOrUpdateBeltPreview)
				for (UFGFactoryConnectionComponent* Output : SideOutputs)
				{
					if (!Output)
						continue;
					
					// Skip outputs that have already been assigned to another building
					if (AssignedOutputs.Contains(Output))
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Skipping already assigned output: %s"), *Output->GetName());
						continue;
					}
					
					FVector OutputPos = Output->GetComponentLocation();
					
					// CRITICAL: Check if the output connector's normal points toward the building
					// This is the primary filter - the output must FACE the building it connects to
					FVector OutputNormal = Output->GetConnectorNormal();
					FVector OutputToBuilding = (BuildingConnectorPos - OutputPos).GetSafeNormal();
					float OutputFacingAlignment = FVector::DotProduct(OutputNormal, OutputToBuilding);
					
					// Output normal must point toward building (within ~60 degrees, cos(60) = 0.5)
					if (OutputFacingAlignment < 0.5f)
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ❌ Output %s does NOT face building (alignment: %.2f < 0.5)"), 
							*Output->GetName(), OutputFacingAlignment);
						continue;
					}
					
					// CROSSING FIX: Check if building is in the same direction as this output
					// Get the direction from splitter to output (output's "side")
					FVector OutputDirection = (OutputPos - SplitterPos).GetSafeNormal();
					
					// Get the direction from splitter to building
					FVector BuildingDirection = (BuildingConnectorPos - SplitterPos).GetSafeNormal();
					
					// Use dot product to check if building is in the same hemisphere as the output
					// Dot product > 0 means less than 90 degrees apart (same general direction)
					float DirectionAlignment = FVector::DotProduct(OutputDirection, BuildingDirection);
					
					if (DirectionAlignment <= 0.0f)
					{
						// Building is on the wrong side of the splitter for this output - skip it
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Skipping output %s for building - wrong side (alignment: %.2f)"), 
							*Output->GetName(), DirectionAlignment);
						continue;
					}
					
					float Distance = FVector::Dist2D(OutputPos, BuildingConnectorPos); // XY priority
					
					// Weight the score by both distance and alignment (favor outputs pointing more directly at building)
					float Score = Distance / (DirectionAlignment + 0.1f); // Add small value to avoid division by zero
					
					if (Score < ClosestDistance)
					{
						ClosestDistance = Score;
						ClosestConnector = Output;
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Better match: output %s, distance %.0f, alignment %.2f, score %.2f"), 
							*Output->GetName(), Distance, DirectionAlignment, Score);
					}
				}
				
				// If no unassigned outputs available, skip this building
				if (!ClosestConnector)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("      ❌ [%s] NO UNASSIGNED OUTPUTS available for building %s"), 
						*DistributorHologram->GetName(), *Building->GetName());
					continue;
				}
			}
			else if (bIsMerger)
			{
				// MANIFOLD FIX: Apply same logic as splitters to mergers
				// Find closest UNASSIGNED merger input to building output
				// CROSSING FIX: Only connect inputs to buildings in their direction to prevent crossings
				// INPUT FIX: Find the closest building output, not just the first one
				
				// Get merger position for directional checks
				FVector MergerPos = DistributorHologram->GetActorLocation();
				
				// Find the closest UNRESERVED building output to the merger
				UFGFactoryConnectionComponent* ClosestBuildingOutput = nullptr;
				float ClosestBuildingOutputDistance = FLT_MAX;
				
				for (UFGFactoryConnectionComponent* BuildingOutput : BuildingOutputsLocal)
				{
					// MERGER FIX: Skip building outputs already reserved by other mergers
					if (ReservedInputs && ReservedInputs->Contains(BuildingOutput))
					{
						AFGHologram* ReservedBy = ReservedInputs->FindRef(BuildingOutput);
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      SKIPPING building output %s - already reserved by %s"), 
							*BuildingOutput->GetName(), *GetNameSafe(ReservedBy));
						continue;
					}
					
					FVector BuildingOutputPos = BuildingOutput->GetComponentLocation();
					float DistanceToMerger = FVector::Dist(MergerPos, BuildingOutputPos);
					
					// ALIGNMENT PRIORITY: Calculate score based on distance weighted by alignment
					FVector MergerToOutputDir = (BuildingOutputPos - MergerPos).GetSafeNormal();
					FVector MergerForward = DistributorHologram->GetActorForwardVector();
					FVector MergerRight = DistributorHologram->GetActorRightVector();
					
					float ForwardDot = FMath::Abs(FVector::DotProduct(MergerToOutputDir, MergerForward));
					float RightDot = FMath::Abs(FVector::DotProduct(MergerToOutputDir, MergerRight));
					float MaxAlignment = FMath::Max(ForwardDot, RightDot);
					
					// STRICT ALIGNMENT: Filter out outputs that are not roughly cardinal
					if (MaxAlignment < MIN_ANGLE_ALIGNMENT)
					{
						continue;
					}
					
					// Score = Distance * (1 + Penalty)
					float AnglePenalty = 1.0f - MaxAlignment;
					float Score = DistanceToMerger * (1.0f + AnglePenalty * ANGLE_PENALTY_MULTIPLIER);

					if (Score < ClosestBuildingOutputDistance)
					{
						ClosestBuildingOutputDistance = Score;
						ClosestBuildingOutput = BuildingOutput;
					}
				}
				
				if (!ClosestBuildingOutput)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⚠️ No building outputs found for %s"), *Building->GetName());
					continue;
				}
				
				BuildingConnector = ClosestBuildingOutput;
				FVector BuildingConnectorPos = BuildingConnector->GetComponentLocation();
				
				// Get all side inputs (sorted left-to-right, excluding middle)
				TArray<UFGFactoryConnectionComponent*> SideInputs;
				GetAllSideConnectors(DistributorHologram, SideInputs);
				
				// Now find the closest unassigned merger input to this building output
				ClosestConnector = nullptr;
				ClosestDistance = FLT_MAX;
				
				for (UFGFactoryConnectionComponent* MergerInput : SideInputs)
				{
					if (!MergerInput)
						continue;
					
					// Skip inputs that have already been assigned to another building
					if (AssignedOutputs.Contains(MergerInput))
					{
						continue;
					}
					
					FVector MergerInputPos = MergerInput->GetComponentLocation();
					
					// CRITICAL: Check if the merger input connector's normal points toward the building
					// This is the primary filter - the input must FACE the building it receives from
					FVector InputNormal = MergerInput->GetConnectorNormal();
					FVector InputToBuilding = (BuildingConnectorPos - MergerInputPos).GetSafeNormal();
					float InputFacingAlignment = FVector::DotProduct(InputNormal, InputToBuilding);
					
					// Input normal must point toward building (within ~60 degrees, cos(60) = 0.5)
					if (InputFacingAlignment < 0.5f)
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ❌ Input %s does NOT face building (alignment: %.2f < 0.5)"), 
							*MergerInput->GetName(), InputFacingAlignment);
						continue;
					}
					
					// CROSSING FIX: Check if building is in the same direction as this input
					// Get the direction from merger to input (input's "side")
					FVector InputDirection = (MergerInputPos - MergerPos).GetSafeNormal();
					
					// Get the direction from merger to building
					FVector BuildingDirection = (BuildingConnectorPos - MergerPos).GetSafeNormal();
					
					// Use dot product to check if building is in the same hemisphere as the input
					// Dot product > 0 means less than 90 degrees apart (same general direction)
					float DirectionAlignment = FVector::DotProduct(InputDirection, BuildingDirection);
					
					if (DirectionAlignment <= 0.0f)
					{
						// Building is on the wrong side of the merger for this input - skip it
						continue;
					}
					
					float Distance = FVector::Dist2D(MergerInputPos, BuildingConnectorPos); // XY priority
					
					// Weight the score by both distance and alignment (favor inputs pointing more directly at building)
					float Score = Distance / (DirectionAlignment + 0.1f); // Add small value to avoid division by zero
					
					if (Score < ClosestDistance)
					{
						ClosestDistance = Score;
						ClosestConnector = MergerInput;
					}
				}
				
				// If no unassigned inputs available, skip this building
				if (!ClosestConnector)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⚠️ No unassigned inputs available for building %s"), *Building->GetName());
					continue;
				}
				
				// CLIPPING FIX: Check if merger is on correct side of building output
				// Building outputs have normals pointing outward - merger should be in front, not behind
				FVector BuildingOutputNormal = BuildingConnector->GetConnectorNormal();
				FVector MergerToBuildingDirection = (BuildingConnectorPos - MergerPos).GetSafeNormal();
				float OutputSideAlignment = FVector::DotProduct(BuildingOutputNormal, MergerToBuildingDirection);
				
				if (OutputSideAlignment > 0.0f)
				{
					// Merger is BEHIND the output (wrong side) - belt would clip through building
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⚠️ Skipping building %s - merger is on wrong side of output (would clip through building)"), 
						*Building->GetName());
					continue;
				}
				
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      Selected closest building output: %s (Score: %.0f, side alignment: %.2f)"), 
					*BuildingConnector->GetName(), ClosestBuildingOutputDistance, OutputSideAlignment);
				
				// For merger inputs, we'll check angle at the merger input (not building output)
				// since merger inputs naturally have ~180° angle relative to building outputs
				UE_LOG(LogSmartFoundations, Log, TEXT("      📐 Merger input connection - angle check will be done at merger input"));
			}
			
			if (ClosestConnector && BuildingConnector)
			{
				// Get building connector position for angle calculations
				FVector BuildingConnectorPos = BuildingConnector->GetComponentLocation();
				if (bIsUpdatingExisting)
				{
					// Update existing helper
					if (BeltPreviewHelpers[HelperIndex].IsValid())
					{
						// Check if belt tier needs updating (building connections use BeltTierToBuilding)
						// Handle "Auto" (tier 0) by getting highest unlocked tier
						int32 DesiredTier = RuntimeSettings.BeltTierToBuilding;
						if (DesiredTier == 0)
						{
							AFGPlayerController* PlayerController = Cast<AFGPlayerController>(
								DistributorHologram->GetConstructionInstigator()->GetController());
							DesiredTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
						}
						if (BeltPreviewHelpers[HelperIndex]->GetBeltTier() != DesiredTier)
						{
							BeltPreviewHelpers[HelperIndex]->UpdateBeltTier(DesiredTier);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔧 Updated reused helper to Mk%d for building connection"), DesiredTier);
						}
						
						if (bIsSplitter)
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔄 Updating splitter connection: %s (splitter output, %.0f cm away) → %s (building input)"), 
								*ClosestConnector->GetName(), ClosestDistance, *BuildingConnector->GetName());
							
							// Use unified function: splitter output → building input
							if (!CreateOrUpdateBeltPreview(ClosestConnector, BuildingConnector, BeltPreviewHelpers[HelperIndex], 30.0f, false, DistributorHologram))
							{
								// Preview failed - skip to next building (helper will be cleaned up later)
								continue;
							}
							
							// MANIFOLD FIX: Mark splitter output as assigned
							AssignedOutputs.Add(ClosestConnector);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔒 Assigned output %s to prevent duplicates"), *ClosestConnector->GetName());
							
							// INPUT RESERVATION: Reserve this building input for this distributor
							if (ReservedInputs)
							{
								ReservedInputs->Add(BuildingConnector, DistributorHologram);
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🎯 Reserved input %s for %s"), 
									*BuildingConnector->GetName(), *DistributorHologram->GetName());
							}
						}
						else if (bIsMerger)
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔄 Updating merger connection: %s (building output, %.0f cm away) → %s (merger input)"), 
								*BuildingConnector->GetName(), ClosestDistance, *ClosestConnector->GetName());
							
							// Use unified function: building output → merger input
							if (!CreateOrUpdateBeltPreview(BuildingConnector, ClosestConnector, BeltPreviewHelpers[HelperIndex], 30.0f, false, DistributorHologram))
							{
								// Preview failed - skip to next building (helper will be cleaned up later)
								continue;
							}
							
							// MANIFOLD FIX: Mark merger input as assigned
							AssignedOutputs.Add(ClosestConnector);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔒 Assigned input %s to prevent duplicates"), *ClosestConnector->GetName());
							
							// OUTPUT RESERVATION: Reserve this building output for this merger
							if (ReservedInputs)
							{
								ReservedInputs->Add(BuildingConnector, DistributorHologram);
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🎯 Reserved building output %s for merger %s"), 
									*BuildingConnector->GetName(), *DistributorHologram->GetName());
							}
						}
						
						// Mark this helper as updated (building is still in range)
						HelperUpdated[HelperIndex] = true;
						
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ✅ Belt preview updated"));
					}
				}
				else
				{
					// Create new helper
					if (bIsSplitter)
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ✨ Creating splitter connection: %s (splitter output, %.0f cm away) → %s (building input)"), 
							*ClosestConnector->GetName(), ClosestDistance, *BuildingConnector->GetName());
						
						// Use unified function: splitter output → building input
						TSharedPtr<FBeltPreviewHelper> BeltHelper;
						if (!CreateOrUpdateBeltPreview(ClosestConnector, BuildingConnector, BeltHelper, 30.0f, false, DistributorHologram))
						{
							// Preview failed - skip this building
							continue;
						}
						
						// MANIFOLD FIX: Mark splitter output as assigned
						AssignedOutputs.Add(ClosestConnector);
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔒 Assigned output %s to prevent duplicates"), *ClosestConnector->GetName());
						
						// INPUT RESERVATION: Reserve this building input for this distributor
						if (ReservedInputs)
						{
							ReservedInputs->Add(BuildingConnector, DistributorHologram);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🎯 RESERVED input %s for %s"), 
								*BuildingConnector->GetName(), *DistributorHologram->GetName());
						}
						
						// Store helper for cleanup
						BeltPreviewHelpers.Add(BeltHelper);
					}
					else if (bIsMerger)
					{
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ✨ Creating merger connection: %s (building output, %.0f cm away) → %s (merger input)"), 
							*BuildingConnector->GetName(), ClosestDistance, *ClosestConnector->GetName());
						
						// Use unified function: building output → merger input
						TSharedPtr<FBeltPreviewHelper> BeltHelper;
						if (!CreateOrUpdateBeltPreview(BuildingConnector, ClosestConnector, BeltHelper, 30.0f, false, DistributorHologram))
						{
							// Preview failed - skip this building
							continue;
						}
						
						// MANIFOLD FIX: Mark merger input as assigned
						AssignedOutputs.Add(ClosestConnector);
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🔒 Assigned input %s to prevent duplicates"), *ClosestConnector->GetName());
						
						// OUTPUT RESERVATION: Reserve this building output for this merger
						if (ReservedInputs)
						{
							ReservedInputs->Add(BuildingConnector, DistributorHologram);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🎯 RESERVED building output %s for merger %s"), 
								*BuildingConnector->GetName(), *DistributorHologram->GetName());
						}
						
						// Store helper for cleanup
						BeltPreviewHelpers.Add(BeltHelper);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⚠️ No available connectors for connection"));
		}
		
		HelperIndex++;
	}
	
SkipBuildingConnections:
	// Cleanup pass: Remove helpers for buildings that went out of range
	if (HelperUpdated.Num() > 0)
	{
		int32 RemovedCount = 0;
		for (int32 i = HelperUpdated.Num() - 1; i >= 0; i--)
		{
			if (!HelperUpdated[i])
			{
				// This helper wasn't updated - building went out of range
				if (BeltPreviewHelpers[i].IsValid())
				{
					BeltPreviewHelpers[i]->DestroyPreview();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🧹 AUTO-CONNECT CLEANUP: Removed belt preview for building that went out of range (helper index %d)"), i);
				}
				BeltPreviewHelpers.RemoveAt(i);
				RemovedCount++;
			}
		}
		
		if (RemovedCount > 0)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🧹 AUTO-CONNECT CLEANUP: Removed %d belt previews for out-of-range buildings"), RemovedCount);
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Auto-connect complete: %d belt previews created"), BeltPreviewHelpers.Num());
	return BeltPreviewHelpers;
}

bool USFAutoConnectService::CreateOrUpdateBeltPreview(
    UFGFactoryConnectionComponent* OutputConnector,
    UFGFactoryConnectionComponent* InputConnector,
    TSharedPtr<FBeltPreviewHelper>& BeltHelper,
	float MaxAngleDegrees /* = 30.f */,
    bool bSkipAngleValidation /* = false */,
    AFGHologram* ParentDistributor /* = nullptr */)
{
    if (!OutputConnector || !InputConnector)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("CreateOrUpdateBeltPreview: Invalid connectors"));
        return false;
    }

    const FVector OutputPos   = OutputConnector->GetComponentLocation();
    const FVector InputPos    = InputConnector->GetComponentLocation();
    const FVector InputNormal = InputConnector->GetConnectorNormal();
    const FVector OutputNormal= OutputConnector->GetConnectorNormal();

    const FVector BeltDirIn   = (OutputPos - InputPos).GetSafeNormal();  // direction AT input (from output)
    const FVector BeltDirOut  = (InputPos - OutputPos).GetSafeNormal();  // direction AT output (to input)

    // Log connector normals for debugging
    UE_LOG(LogSmartFoundations, Verbose,
        TEXT("   📐 Connector normals: OUT=(%.2f,%.2f,%.2f) IN=(%.2f,%.2f,%.2f) | BeltDir=(%.2f,%.2f,%.2f)"),
        OutputNormal.X, OutputNormal.Y, OutputNormal.Z,
        InputNormal.X, InputNormal.Y, InputNormal.Z,
        BeltDirOut.X, BeltDirOut.Y, BeltDirOut.Z);

    // ---- Check minimum belt length (0.5m = 50cm)
    const float BeltLength = FVector::Dist(OutputPos, InputPos);
    const float MinBeltLength = 50.0f;  // 0.5 meters in cm
    
    if (BeltLength < MinBeltLength)
    {
        UE_LOG(LogSmartFoundations, Log,
            TEXT("   ❌ BELT REJECTED - TOO SHORT: %.1f cm < %.1f cm minimum (%s → %s)"),
            BeltLength, MinBeltLength, *OutputConnector->GetName(), *InputConnector->GetName());
        
        // Clean up any existing preview helper
        if (BeltHelper.IsValid())
        {
            BeltHelper->DestroyPreview();
            BeltHelper.Reset();
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🗑️ Destroyed existing preview (belt too short)"));
        }
        return false;
    }

    // ---- Dual angle validation (skip for manifold connections)
    float AngleIn = 0.0f;
    float AngleOut = 0.0f;
    
    if (!bSkipAngleValidation)
    {
        float DotIn   = FVector::DotProduct(InputNormal, BeltDirIn);
        float DotOut  = FVector::DotProduct(OutputNormal, BeltDirOut);
        AngleIn = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotIn,  -1.f, 1.f)));
        AngleOut= FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotOut, -1.f, 1.f)));

        UE_LOG(LogSmartFoundations, VeryVerbose,
            TEXT("   Belt angle check: %s → %s | In=%.1f° Out=%.1f° (limit %.1f°)"),
            *OutputConnector->GetName(), *InputConnector->GetName(),
            AngleIn, AngleOut, MaxAngleDegrees);

        if (AngleIn > MaxAngleDegrees || AngleOut > MaxAngleDegrees)
        {
            UE_LOG(LogSmartFoundations, Log,
                TEXT("   BELT REJECTED - BAD ANGLE: %s → %s (In %.1f° / Out %.1f° > %.1f°)"),
                *OutputConnector->GetName(), *InputConnector->GetName(),
                AngleIn, AngleOut, MaxAngleDegrees);
            
            // Clean up any existing preview helper
            if (BeltHelper.IsValid())
            {
                BeltHelper->DestroyPreview();
                BeltHelper.Reset();
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Destroyed existing preview (bad angle)"));
            }
            return false;
        }
    }
    else
    {
        UE_LOG(LogSmartFoundations, VeryVerbose,
            TEXT("   Manifold connection - angle validation skipped"));
    }

    // ---- Create helper if missing
    if (!BeltHelper.IsValid())
    {
        // Get runtime settings for belt tier
        const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
        // Use BeltTierToBuilding for building connections
        // Handle "Auto" (tier 0) by getting highest unlocked tier
        int32 BeltTier = RuntimeSettings.BeltTierToBuilding;
        if (BeltTier == 0)
        {
            // No hologram context in this function - get player from world
            AFGPlayerController* PlayerController = GetWorld()->GetFirstPlayerController<AFGPlayerController>();
            BeltTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
        }
        // Pass ParentDistributor for child registration
        BeltHelper = MakeShared<FBeltPreviewHelper>(GetWorld(), BeltTier, ParentDistributor);
        if (!BeltHelper.IsValid())
        {
            UE_LOG(LogSmartFoundations, Error, TEXT("   ❌ Failed to create belt preview helper"));
            return false;
        }
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✨ Created new belt preview helper"));
    }

    BeltHelper->UpdatePreview(OutputConnector, InputConnector);
    if (!BeltHelper->ValidatePlacementAndRegisterAsChild())
    {
        UE_LOG(LogSmartFoundations, Log,
            TEXT("   ❌ BELT REJECTED - VANILLA: %s → %s"),
            *OutputConnector->GetName(), *InputConnector->GetName());
        BeltHelper.Reset();
        return false;
    }

    UE_LOG(LogSmartFoundations, Log,
        TEXT("   ✅ BELT CREATED: %s → %s (Length: %.1f cm, In %.1f° / Out %.1f° ≤ %.1f°)"),
        *OutputConnector->GetName(), *InputConnector->GetName(),
        BeltLength, AngleIn, AngleOut, MaxAngleDegrees);

    return true;
}

bool USFAutoConnectService::ConnectAnyConnectors(
    UFGFactoryConnectionComponent* OutputConnector,
    UFGFactoryConnectionComponent* InputConnector,
    AFGHologram* StorageHologram,
    bool bSkipAngleValidation)
{
    if (!OutputConnector || !InputConnector || !StorageHologram)
    {
        UE_LOG(LogSmartFoundations, Warning, TEXT("ConnectAnyConnectors: Invalid parameters"));
        return false;
    }

	// Get existing previews for storage hologram
	TArray<TSharedPtr<FBeltPreviewHelper>>* ExistingPreviews = GetBeltPreviews(StorageHologram);
	TArray<TSharedPtr<FBeltPreviewHelper>> UpdatedPreviews;
	
	if (ExistingPreviews)
	{
		UpdatedPreviews = *ExistingPreviews;
	}

	// Create new preview (used for manifold connections from orchestrator)
	// Get runtime settings for belt tier
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	// Use BeltTierMain for manifold/orchestrator connections
	// Handle "Auto" (tier 0) by getting highest unlocked tier
	int32 BeltTier = RuntimeSettings.BeltTierMain;
	if (BeltTier == 0)
	{
		// Get player controller from world (orchestrator doesn't have hologram context)
		AFGPlayerController* PlayerController = GetWorld()->GetFirstPlayerController<AFGPlayerController>();
		BeltTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
	}
	// Pass StorageHologram as parent for child registration
	TSharedPtr<FBeltPreviewHelper> NewPreview = MakeShared<FBeltPreviewHelper>(GetWorld(), BeltTier, StorageHologram);
	bool bSuccess = CreateOrUpdateBeltPreview(
		OutputConnector,
		InputConnector,
		NewPreview,
		30.0f,
		bSkipAngleValidation
	);

	if (bSuccess)
	{
		// Store connector pair for build handoff
		StoreConnectorPair(StorageHologram, OutputConnector, InputConnector);
		
		// Add to array and store
		UpdatedPreviews.Add(NewPreview);
		StoreBeltPreviews(StorageHologram, UpdatedPreviews);
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Connected %s → %s (stored on %s)"),
			*OutputConnector->GetName(), *InputConnector->GetName(), *StorageHologram->GetName());
		return true;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ❌ Failed to create connection %s → %s"),
		*OutputConnector->GetName(), *InputConnector->GetName());
	return false;
}

bool USFAutoConnectService::BuildBeltFromPreview(const TSharedPtr<FBeltPreviewHelper>& Preview)
{
	if (!Preview.IsValid())
	{
		return false;
	}

	UFGFactoryConnectionComponent* OutputConnector = Preview->GetOutputConnector();
	UFGFactoryConnectionComponent* InputConnector = Preview->GetInputConnector();
	if (!OutputConnector || !InputConnector)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: Missing connectors"));
		return false;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltFromPreview: Attempting belt from %s to %s"),
		*GetNameSafe(OutputConnector), *GetNameSafe(InputConnector));

	if (OutputConnector->IsConnected() || InputConnector->IsConnected())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BuildBeltFromPreview: Connectors already connected, skipping"));
		return false;
	}

	AFGSplineHologram* BaseHolo = Preview->GetHologram();
	ASFConveyorBeltHologram* SmartHolo = Cast<ASFConveyorBeltHologram>(BaseHolo);
	if (!SmartHolo)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: Preview hologram is not a conveyor belt hologram"));
		return false;
	}

	const TArray<FSplinePointData>& SplineData = SmartHolo->GetSplineData();
	if (SplineData.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: Preview hologram has no spline data"));
		return false;
	}

	UWorld* World = OutputConnector->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: World is null"));
		return false;
	}

	// Use the belt tier from the preview helper (which matches runtime settings)
	int32 BeltTier = Preview->GetBeltTier();
	FString BeltPath = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"), BeltTier, BeltTier, BeltTier);
	UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);
	if (!BeltClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: Failed to load Mk%d belt class from %s"), BeltTier, *BeltPath);
		return false;
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltFromPreview: Building Mk%d belt"), BeltTier);

	FVector SpawnPos = SmartHolo->GetActorLocation();
	AFGBuildableConveyorBelt* Belt = World->SpawnActor<AFGBuildableConveyorBelt>(
		BeltClass,
		SpawnPos,
		FRotator::ZeroRotator);
	if (!Belt)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: Failed to spawn belt"));
		return false;
	}

	AFGBuildableConveyorBelt* Resplined = AFGBuildableConveyorBelt::Respline(Belt, SplineData);
	if (!Resplined)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: Respline failed"));
		Belt->Destroy();
		return false;
	}

	Belt = Resplined;
	Belt->OnBuildEffectFinished();

	UFGFactoryConnectionComponent* BeltConnection0 = Belt->GetConnection0();
	UFGFactoryConnectionComponent* BeltConnection1 = Belt->GetConnection1();
	if (!BeltConnection0 || !BeltConnection1)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: Belt missing connection components"));
		Belt->Destroy();
		return false;
	}

	// Set connections FIRST - chain actor uses these to determine chain membership
	BeltConnection0->SetConnection(OutputConnector);
	BeltConnection1->SetConnection(InputConnector);

	// CRITICAL: Register belt with BuildableSubsystem AFTER connections are set
	// AddConveyor uses connections to determine which chain the belt joins.
	// Calling it before connections causes crashes in the parallel factory tick.
	AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
	if (BuildableSubsystem)
	{
		BuildableSubsystem->AddConveyor(Belt);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BuildBeltFromPreview: Registered belt with subsystem (ChainActor=%s)"),
			Belt->GetConveyorChainActor() ? *Belt->GetConveyorChainActor()->GetName() : TEXT("pending"));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltFromPreview: No BuildableSubsystem - belt will have no chain actor!"));
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltFromPreview: Built belt %s → %s"),
		*OutputConnector->GetName(), *InputConnector->GetName());

	return true;
}

bool USFAutoConnectService::BuildBeltsForDistributor(AFGHologram* DistributorHologram, AFGBuildable* BuiltDistributor)
{
	if (!DistributorHologram || !BuiltDistributor)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltsForDistributor: Invalid args (Holo=%s Built=%s)"),
			*GetNameSafe(DistributorHologram), *GetNameSafe(BuiltDistributor));
		return false;
	}

	// Get stored connector pairs from preview stage
	TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>>* ConnectorPairs = GetConnectorPairs(DistributorHologram);
	if (!ConnectorPairs || ConnectorPairs->Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltsForDistributor: No connector pairs stored for %s"),
			*GetNameSafe(DistributorHologram));
		return false;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltsForDistributor: Found %d stored connector pairs for hologram %s"),
		ConnectorPairs->Num(), *GetNameSafe(DistributorHologram));

	// Check if this is the parent distributor (not a child)
	bool bIsParentDistributor = true;
	if (USFSubsystem* SFSys = USFSubsystem::Get(BuiltDistributor->GetWorld()))
	{
		if (FSFHologramHelperService* HologramHelper = SFSys->GetHologramHelper())
		{
			const TArray<TWeakObjectPtr<AFGHologram>>& Children = HologramHelper->GetSpawnedChildren();
			for (const TWeakObjectPtr<AFGHologram>& ChildPtr : Children)
			{
				if (ChildPtr.Get() == DistributorHologram)
				{
					bIsParentDistributor = false;
					break;
				}
			}
		}
	}

	// Deduct belt costs ONLY for parent distributor (covers entire grid)
	if (bIsParentDistributor)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT(" Parent distributor - deducting belt costs for entire grid"));
		
		if (UWorld* World = BuiltDistributor->GetWorld())
		{
			if (AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(UGameplayStatics::GetPlayerCharacter(World, 0)))
			{
				if (UFGInventoryComponent* Inventory = Player->GetInventory())
				{
					// Check if "No Build Cost" cheat is enabled
					AFGGameState* GameState = World->GetGameState<AFGGameState>();
					if (GameState && GameState->GetCheatNoCost())
					{
						UE_LOG(LogSmartFoundations, Log, TEXT(" No Build Cost cheat enabled - skipping belt cost deduction"));
						// Skip deduction but allow belts to build (HUD still shows costs)
					}
					else
					{
						// REMOVED: Manual belt cost deduction
						// Belt costs are now automatically deducted by vanilla via child hologram Construct()
						// Each belt child has SetRecipe() called, so vanilla deducts costs during build
					} // End else (No Build Cost check)
				}
			}
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log, TEXT(" Child distributor - belt costs already deducted by parent"));
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltsForDistributor: Building %d belts for %s → %s"),
		ConnectorPairs->Num(), *GetNameSafe(DistributorHologram), *GetNameSafe(BuiltDistributor));

	// Get built distributor connectors for mapping
	TArray<UFGFactoryConnectionComponent*> BuiltInputs;
	TArray<UFGFactoryConnectionComponent*> BuiltOutputs;
	GetBuildingConnectors(BuiltDistributor, BuiltInputs, BuiltOutputs);

	int32 SuccessCount = 0;

	// Process each stored connector pair
	for (const TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>& Pair : *ConnectorPairs)
	{
		UFGFactoryConnectionComponent* HologramConnector = Pair.Key;
		UFGFactoryConnectionComponent* BuildingConnector = Pair.Value;

		if (!HologramConnector || !BuildingConnector)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltsForDistributor: Invalid connector pair"));
			continue;
		}

		// Determine which side is the hologram connector
		AActor* HologramOwner = HologramConnector->GetOwner();
		AActor* BuildingOwner = BuildingConnector->GetOwner();

		if (HologramOwner != DistributorHologram && BuildingOwner != DistributorHologram)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltsForDistributor: Pair doesn't involve distributor hologram"));
			continue;
		}

		// Identify if hologram connector is output or input
		const bool bHoloIsOutput = (HologramConnector->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT);
		const bool bHoloIsInput = (HologramConnector->GetDirection() == EFactoryConnectionDirection::FCD_INPUT);

		if (!bHoloIsOutput && !bHoloIsInput)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltsForDistributor: Hologram connector has no direction"));
			continue;
		}

		// Map hologram connector to built distributor connector
		TArray<UFGFactoryConnectionComponent*>& BuiltSideArray = bHoloIsOutput ? BuiltOutputs : BuiltInputs;
		UFGFactoryConnectionComponent* BuiltDistributorConnector = FindMatchingBuildableConnector(BuiltDistributor, HologramConnector, BuiltSideArray);

		if (!BuiltDistributorConnector)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltsForDistributor: No matching built connector for %s"), *GetNameSafe(HologramConnector));
			continue;
		}

		// Determine final connection direction (built distributor → building)
		UFGFactoryConnectionComponent* OutputConnector = nullptr;
		UFGFactoryConnectionComponent* InputConnector = nullptr;

		if (bHoloIsOutput)
		{
			// Hologram was output → building input, so built should follow same pattern
			OutputConnector = BuiltDistributorConnector;
			InputConnector = BuildingConnector;
		}
		else
		{
			// Hologram was input → building output, so built should follow same pattern
			OutputConnector = BuildingConnector;
			InputConnector = BuiltDistributorConnector;
		}

		if (!OutputConnector || !InputConnector)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltsForDistributor: Failed to determine connection direction"));
			continue;
		}

		// Check if connectors are already connected
		if (OutputConnector->IsConnected() || InputConnector->IsConnected())
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("BuildBeltsForDistributor: Connectors already connected, skipping"));
			continue;
		}

		// Establish the connection using vanilla SetConnection
		if (OutputConnector->CanConnectTo(InputConnector))
		{
			OutputConnector->SetConnection(InputConnector);
			SuccessCount++;
			UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltsForDistributor: Connected %s → %s"),
				*GetNameSafe(OutputConnector), *GetNameSafe(InputConnector));
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("BuildBeltsForDistributor: Cannot connect %s → %s"),
				*GetNameSafe(OutputConnector), *GetNameSafe(InputConnector));
		}
	}

	// Destroy preview holograms after all connections are established
	TArray<TSharedPtr<FBeltPreviewHelper>>* Previews = GetBeltPreviews(DistributorHologram);
	if (Previews)
	{
		for (TSharedPtr<FBeltPreviewHelper>& Preview : *Previews)
		{
			if (Preview.IsValid())
			{
				Preview->DestroyPreview();
			}
		}
		// Clear stored previews
		DistributorBeltPreviews.Remove(DistributorHologram);
	}

	// Clear stored connector pairs
	ClearConnectorPairs(DistributorHologram);

	UE_LOG(LogSmartFoundations, Log, TEXT("BuildBeltsForDistributor: Complete - %d/%d belts built successfully"),
		SuccessCount, ConnectorPairs->Num());

	return SuccessCount > 0;
}

// ========================================
// Distributor Detection
// ========================================

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
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("IsDistributorHologram: Checking '%s'"), *BuildClassName);
	
	// TODO: Consider using class-based hierarchy checks instead of string matching
	if (BuildClassName.Contains(TEXT("Splitter")) || 
		BuildClassName.Contains(TEXT("Merger")))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Distributor detected: %s"), *BuildClassName);
		return true;
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("❌ Not a distributor: %s"), *BuildClassName);
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindDistributorChains: DistributorHologram or Subsystem is null"));
		return;
	}
	
	// Get hologram helper to find parent and children
	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (!HologramHelper)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   No HologramHelper available - no chain detection"));
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
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔗 Manifold Detection: Current=%s, Parent=%s, SpawnedChildren=%d"), 
		*DistributorHologram->GetName(), *ParentHologram->GetName(), SpawnedChildren.Num());
	
	if (SpawnedChildren.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("   No spawned children - single distributor, no chaining"));
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
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔗 Manifold Detection: Found %d distributors in grid"), AllDistributors.Num());
	
	if (AllDistributors.Num() < 2)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("   Need at least 2 distributors for chaining"));
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("   Current distributor not found in array"));
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
			UE_LOG(LogSmartFoundations, Log, TEXT("   🔗 Splitter chain: %s -> %s"), 
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
			UE_LOG(LogSmartFoundations, Log, TEXT("   🔗 Merger chain: %s -> %s"), 
				*PreviousDistributor->GetName(), *DistributorHologram->GetName());
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("   ✅ Found %d distributor chain targets for %s"), 
		OutChainTargets.Num(), *DistributorHologram->GetName());
}

// ========================================
// Building Search
// ========================================

void USFAutoConnectService::FindCompatibleBuildingsForDistributor(AFGHologram* DistributorHologram, TArray<AFGBuildable*>& OutCompatibleBuildings)
{
	if (!DistributorHologram || !Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindCompatibleBuildingsForDistributor: DistributorHologram or Subsystem is null"));
		return;
	}
	
	// Search for nearby buildings
	FVector DistributorLocation = DistributorHologram->GetActorLocation();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🔍 Searching for buildings within %.0f cm radius"), BUILDING_SEARCH_RADIUS);
	TArray<AFGBuildable*> NearbyBuildings = Subsystem->FindNearbyBuildings(DistributorLocation, BUILDING_SEARCH_RADIUS);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Found %d nearby buildings"), NearbyBuildings.Num());
	
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
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ⏭️ Skipping %s - storage building"), *Building->GetName());
			continue;
		}
			
		// CRITICAL: Only allow factory production buildings (AFGBuildableFactory and subclasses)
		// This prevents connecting to storage, stations, vehicles, etc.
		if (!Building->IsA(AFGBuildableFactory::StaticClass()))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ⏭️ Skipping %s - not a factory building"), *Building->GetName());
			continue;
		}
		
		// Additional safety: Skip distributors (they're handled separately in manifold logic)
		FString BuildingClassName = Building->GetClass()->GetFName().ToString();
		if (BuildingClassName.Contains(TEXT("Splitter")) || BuildingClassName.Contains(TEXT("Merger")))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ⏭️ Skipping %s - distributor (handled by manifold logic)"), *Building->GetName());
			continue;
		}
		
		OutCompatibleBuildings.Add(Building);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ Found compatible factory building: %s (%s)"), *Building->GetName(), *BuildingClassName);
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetBuildingConnectors: Building is null"));
		return;
	}

	OutInputs.Empty();
	OutOutputs.Empty();

	// Gather all factory connection components on the actor (including child components)
	TArray<UFGFactoryConnectionComponent*> AllConnectors;
	Building->GetComponents<UFGFactoryConnectionComponent>(AllConnectors, /*bIncludeFromChildActors=*/true);

	if (AllConnectors.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBuildingConnectors: Building %s has NO UFGFactoryConnectionComponent components; falling back to overlap search"),
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
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBuildingConnectors: Skipping %s connector %s (already connected)"),
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

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBuildingConnectors: %s connector %s direction=%d"),
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
				EFactoryConnectionConnector::FCC_CONVEYOR,
				EFactoryConnectionDirection::FCD_ANY);

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBuildingConnectors: Overlap search around %s found %d potential connectors"),
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
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBuildingConnectors(overlap): Skipping %s connector %s (already connected)"),
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

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBuildingConnectors(overlap): %s connector %s direction=%d"),
					*Building->GetName(), *Connector->GetName(), static_cast<int32>(Dir));
			}
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetBuildingConnectors: Building %s has %d inputs, %d outputs"),
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindMiddleConnector: Distributor is null"));
		return nullptr;
	}

	const bool bIsSplitter = IsSplitterHologram(Distributor);
	const bool bIsMerger   = IsMergerHologram(Distributor);
	if (!bIsSplitter && !bIsMerger)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindMiddleConnector: %s is not a splitter or merger"), *Distributor->GetName());
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
		UE_LOG(LogSmartFoundations, Log, TEXT("   FindMiddleConnector: %s middle %s = %s (alignment=%.2f)"), 
			*Distributor->GetName(), Type, *Best->GetName(), BestAlignment);
	}
	else
	{
		const TCHAR* Type = bIsSplitter ? TEXT("outputs") : TEXT("inputs");
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindMiddleConnector: No %s found on %s"), Type, *Distributor->GetName());
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindConnectorFacingTarget: Source or Target is null"));
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Found %s facing target (alignment=%.2f): %s"),
			DirName, BestAlignment, *Best->GetName());
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindConnectorFacingTarget: No connector facing target found"));
	}

	return Best;
}

UFGFactoryConnectionComponent* USFAutoConnectService::FindSideConnector(AFGHologram* Distributor, int32 Index)
{
	if (!Distributor)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindSideConnector: Distributor is null"));
		return nullptr;
	}

	// Determine distributor type
	bool bIsSplitter = IsSplitterHologram(Distributor);
	bool bIsMerger = IsMergerHologram(Distributor);

	if (!bIsSplitter && !bIsMerger)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("FindSideConnector: %s is not a splitter or merger"), *Distributor->GetName());
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Found side %s %d: %s"), ConnectorType, Index, *SideConnectors[Index]->GetName());
		return SideConnectors[Index];
	}

	const TCHAR* ConnectorType = bIsSplitter ? TEXT("output") : TEXT("input");
	UE_LOG(LogSmartFoundations, Warning, TEXT("FindSideConnector: Side %s %d not found on %s (has %d sides)"), 
		ConnectorType, Index, *Distributor->GetName(), SideConnectors.Num());
	return nullptr;
}

void USFAutoConnectService::GetAllSideConnectors(AFGHologram* Distributor, TArray<UFGFactoryConnectionComponent*>& OutSideConnectors, EFactoryConnectionDirection DirectionOverride)
{
	OutSideConnectors.Empty();

	if (!Distributor)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetAllSideConnectors: Distributor is null"));
		return;
	}

	// Determine distributor type
	bool bIsSplitter = IsSplitterHologram(Distributor);
	bool bIsMerger = IsMergerHologram(Distributor);

	if (!bIsSplitter && !bIsMerger)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("GetAllSideConnectors: %s is not a splitter or merger"), *Distributor->GetName());
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
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Found %d side %s (sorted left-to-right)"), 
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
	
	UE_LOG(LogSmartFoundations, Log, TEXT("👶 Processing %d child holograms for parent %s"), 
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
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ⏭️ Skipping pending-destroy child: %s"), 
				*Child->GetName());
			continue;
		}
		
		AllDistributors.Add(Child);
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("   🎯 Processing %d total distributors (parent + children) with shared input reservation"), 
		AllDistributors.Num());
	
	// Process each distributor with shared reservation map
	for (int32 i = 0; i < AllDistributors.Num(); i++)
	{
		AFGHologram* Distributor = AllDistributors[i];
		
		bool bIsParent = (Distributor == ParentHologram);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   %s Processing distributor %d/%d: %s"), 
			bIsParent ? TEXT("🎯") : TEXT("👶"),
			i + 1, AllDistributors.Num(), *Distributor->GetName());
		
		// Process with shared reservation map
		TArray<TSharedPtr<FBeltPreviewHelper>> Previews = ProcessSingleDistributor(Distributor, &ReservedInputs);
		
		// Store in map keyed by the distributor hologram
		if (Previews.Num() > 0)
		{
			DistributorBeltPreviews.Emplace(Distributor, Previews);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ✅ Stored %d belt previews for %s"), 
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
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      🗑️ No belt previews for %s"), 
				*Distributor->GetName());
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("✅ Finished processing %d distributors (%d inputs reserved)"), 
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
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🗑️ Cleaning up %d belt previews for distributor: %s"), 
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
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Cleaned up belt previews for %s"), 
		*DistributorHologram->GetName());
}

void USFAutoConnectService::StoreConnectorPair(AFGHologram* DistributorHologram, UFGFactoryConnectionComponent* HologramConnector, UFGFactoryConnectionComponent* BuildingConnector)
{
	if (!DistributorHologram || !HologramConnector || !BuildingConnector)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("StoreConnectorPair: Invalid parameters"));
		return;
	}

	if (!StoredConnectorPairs.Contains(DistributorHologram))
	{
		StoredConnectorPairs.Add(DistributorHologram, TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>>());
	}

	TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*> Pair(HologramConnector, BuildingConnector);
	StoredConnectorPairs[DistributorHologram].Add(Pair);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StoreConnectorPair: Stored %s → %s for %s"),
		*GetNameSafe(HologramConnector), *GetNameSafe(BuildingConnector), *GetNameSafe(DistributorHologram));
}

void USFAutoConnectService::ClearConnectorPairs(AFGHologram* DistributorHologram)
{
	if (DistributorHologram && StoredConnectorPairs.Contains(DistributorHologram))
	{
		int32 Count = StoredConnectorPairs[DistributorHologram].Num();
		StoredConnectorPairs.Remove(DistributorHologram);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("ClearConnectorPairs: Cleared %d pairs for %s"), Count, *GetNameSafe(DistributorHologram));
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📦 Stored %d belt previews for %s"), 
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
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Calculated belt costs: %d item types from %d/%d valid previews for %s"), 
			TotalCost.Num(), ValidPreviews, TotalPreviews, *DistributorHologram->GetName());
		
		// Check if this distributor is a child (has a parent) or the root parent
		AFGHologram* ParentHologramPtr = DistributorHologram->GetParentHologram();
		bool bIsChildDistributor = (ParentHologramPtr != nullptr);
		
		// If this is a child, trigger parent to re-aggregate
		AFGHologram* RootDistributor = bIsChildDistributor ? ParentHologramPtr : DistributorHologram;
		
		if (bIsChildDistributor)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Child distributor (parent=%s) - cached costs, will trigger parent re-aggregation"), 
				*GetNameSafe(ParentHologramPtr));
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Root/Parent distributor - aggregating grid costs and updating HUD"));
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
				
				// Check if "No Build Cost" cheat is enabled
				AFGGameState* GameState = World->GetGameState<AFGGameState>();
				bool bNoBuildCost = GameState && GameState->GetCheatNoCost();
				
				// Only perform affordability validation if cheat is disabled
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
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Cannot afford belt materials AFTER distributor costs: %s - Have: %d, Distributors: %d, Remaining: %d, Belts need: %d"), 
								*GetNameSafe(BeltCost.ItemClass), TotalAvailable, DistributorConsumption, RemainingAfterDistributors, BeltCost.Amount);
							break;
						}
					}
					
					// Add or remove affordability disqualifier based on belt costs
					if (!bCanAffordBelts)
					{
						DistributorHologram->AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Hologram marked INVALID - cannot afford belt materials"));
					}
					else
					{
						// Player can afford - ensure vanilla validation runs normally
						DistributorHologram->ValidatePlacementAndCost(Inventory);
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 Hologram validated - player can afford belt materials"));
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
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🗑️ Cleared belt previews for %s"), 
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetBeltPreviewsCost: null hologram"));
		return TotalCost;
	}

	// Use const overload - no const_cast needed
	const TArray<TSharedPtr<FBeltPreviewHelper>>* Previews = GetBeltPreviews(DistributorHologram);
	if (!Previews)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetBeltPreviewsCost: No belt previews found for %s"), 
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
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetBeltPreviewsCost: Calculated %d item types from %d valid previews for %s"), 
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("UpdatePipePreviews: Missing junction or subsystem"));
		return;
	}

	// Get runtime settings from subsystem
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	
	// Check if pipe auto-connect is enabled
	if (!RuntimeSettings.bPipeAutoConnectEnabled)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("UpdatePipePreviews: Pipe auto-connect disabled in runtime settings"));
		return;
	}

	// Get or create the pipe auto-connect manager for this junction
	TSharedPtr<FSFPipeAutoConnectManager>* ManagerPtr = PipeAutoConnectManagers.Find(JunctionHologram);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE MANAGER LOOKUP: Junction=%s (ptr=%p), Found=%s, TotalManagers=%d"), 
		*JunctionHologram->GetName(), JunctionHologram, ManagerPtr ? TEXT("YES") : TEXT("NO"), PipeAutoConnectManagers.Num());
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		// Create new manager for this junction
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE: Creating NEW manager for junction %s (reason: %s)"), 
			*JunctionHologram->GetName(), !ManagerPtr ? TEXT("not found") : TEXT("invalid"));
		TSharedPtr<FSFPipeAutoConnectManager> NewManager = MakeShared<FSFPipeAutoConnectManager>();
		NewManager->Initialize(Subsystem, this);
		PipeAutoConnectManagers.Add(JunctionHologram, NewManager);
		ManagerPtr = PipeAutoConnectManagers.Find(JunctionHologram);
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE: Reusing EXISTING manager for junction %s"), 
			*JunctionHologram->GetName());
	}

	if (ManagerPtr && ManagerPtr->IsValid())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 Updating pipe previews for junction %s"), *JunctionHologram->GetName());
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 Clearing pipe previews for junction %s (keeping manager)"), *JunctionHologram->GetName());
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
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 Clearing all pipe managers (%d total)"), PipeAutoConnectManagers.Num());
	
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
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetPipePreviewsCost: null hologram"));
		return TotalCost;
	}

	// Use const overload - no const_cast needed at call site
	const FSFPipeAutoConnectManager* Manager = GetPipeManager(JunctionHologram);
	if (!Manager)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetPipePreviewsCost: No pipe manager found for %s"), 
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
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetPipePreviewsCost: Calculated %d item types from %d valid previews for %s"),  
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

bool USFAutoConnectService::IsBeltSupportHologram(AFGHologram* Hologram)
{
	return IsStackableConveyorPoleHologram(Hologram) 
		|| IsCeilingConveyorSupportHologram(Hologram) 
		|| IsWallConveyorPoleHologram(Hologram);
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

void USFAutoConnectService::ProcessFloorHolePipes(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !Subsystem)
	{
		return;
	}
	
	// Get or create the pipe auto-connect manager for this hologram
	TSharedPtr<FSFPipeAutoConnectManager>* ManagerPtr = PipeAutoConnectManagers.Find(ParentHologram);
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		// Create new manager for this hologram
		TSharedPtr<FSFPipeAutoConnectManager> NewManager = MakeShared<FSFPipeAutoConnectManager>();
		NewManager->Initialize(Subsystem, this);
		PipeAutoConnectManagers.Add(ParentHologram, NewManager);
		ManagerPtr = PipeAutoConnectManagers.Find(ParentHologram);
	}
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		return;
	}
	
	FSFPipeAutoConnectManager* Manager = ManagerPtr->Get();
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		Manager->ClearFloorHolePipePreviews();
		return;
	}
	
	// Get runtime settings
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	if (!RuntimeSettings.bEnabled || !RuntimeSettings.bPipeAutoConnectEnabled)
	{
		Manager->ClearFloorHolePipePreviews();
		return;
	}
	
	// Delegate to PipeAutoConnectManager which has the pipe spawning infrastructure
	Manager->ProcessFloorHolePipes(ParentHologram);
}

void USFAutoConnectService::ProcessStackableConveyorPoles(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 ProcessStackableConveyorPoles: null parent hologram or subsystem"));
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		CleanupAllStackableBelts(ParentHologram);
		UE_LOG(LogSmartFoundations, Log, TEXT("🚧 ProcessStackableConveyorPoles: Skipped - Smart disabled for current action"));
		return;
	}

	// Get runtime settings
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	if (!RuntimeSettings.bEnabled || !RuntimeSettings.bStackableBeltEnabled)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🚧 ProcessStackableConveyorPoles: Auto-connect disabled (global=%d, stackable belt=%d) - clearing belt children"),
			RuntimeSettings.bEnabled, RuntimeSettings.bStackableBeltEnabled);
		
		// Clear all existing stackable belt children when disabled
		CleanupAllStackableBelts(ParentHologram);
		return;
	}
	
	// Track active pole pairs for orphan removal at end
	TSet<uint64> ActivePolePairs;

	// ========================================================================
	// GRID-BASED BELT CONNECTIONS (same approach as pipes)
	// ========================================================================
	// Connect poles that are X-neighbors in the grid (same Y, same Z, adjacent X).
	// This is the same logic used by ProcessStackablePipeSupports().
	//
	// Grid iteration order (how children are spawned): Z → X → Y
	// For each Z layer:
	//   For each X column:
	//     For each Y row:
	//       Spawn child (skipping [0,0,0] which is parent)
	//
	// To find X-neighbors, we need poles at [X, Y, Z] and [X+1, Y, Z].
	// ========================================================================

	// Get hologram helper to find children
	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (!HologramHelper)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 No HologramHelper available"));
		return;
	}

	// Get grid dimensions from CounterState to understand the grid structure
	const FSFCounterState& CounterState = Subsystem->GetCounterState();
	int32 XCount = FMath::Abs(CounterState.GridCounters.X);
	int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
	int32 ZCount = FMath::Abs(CounterState.GridCounters.Z);

	// Collect ALL poles for belt connections (parent + children)
	TArray<AFGHologram*> AllPoles;
	AllPoles.Add(ParentHologram);

	TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren = HologramHelper->GetSpawnedChildren();
	for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
	{
		if (ChildPtr.IsValid() && IsBeltSupportHologram(ChildPtr.Get()))
		{
			AllPoles.Add(ChildPtr.Get());
		}
	}

	if (AllPoles.Num() < 2)
	{
		return;
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 Found %d belt support poles (parent + %d children), Grid[%d,%d,%d]"), 
		AllPoles.Num(), AllPoles.Num() - 1, XCount, YCount, ZCount);

	// Get belt tier from settings
	int32 BeltTier = RuntimeSettings.BeltTierMain;
	if (BeltTier == 0)
	{
		AFGPlayerController* PlayerController = Cast<AFGPlayerController>(
			ParentHologram->GetConstructionInstigator()->GetController());
		BeltTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
	}

	// Get belt direction from settings (0=Forward, 1=Backward)
	bool bReverseDirection = RuntimeSettings.StackableBeltDirection == 1;

	// Issue #268: Wall conveyor poles scale along Y axis instead of X.
	// Detect this so we connect Y-neighbors instead of X-neighbors.
	bool bConnectAlongY = IsWallConveyorPoleHologram(ParentHologram);

	// Build a map from grid position [X,Y,Z] to hologram for fast neighbor lookup
	TMap<int64, AFGHologram*> GridToHologram;
	
	auto PackGridPos = [](int32 X, int32 Y, int32 Z) -> int64 {
		return ((int64)(X + 128) << 16) | ((int64)(Y + 128) << 8) | (int64)(Z + 128);
	};
	
	// Parent is at [0,0,0]
	GridToHologram.Add(PackGridPos(0, 0, 0), ParentHologram);
	
	// Map children to their grid positions based on spawn order
	// Spawn order: Z → X → Y, skipping [0,0,0]
	int32 ChildIndex = 0;
	for (int32 Z = 0; Z < ZCount && ChildIndex < SpawnedChildren.Num(); ++Z)
	{
		for (int32 X = 0; X < XCount && ChildIndex < SpawnedChildren.Num(); ++X)
		{
			for (int32 Y = 0; Y < YCount && ChildIndex < SpawnedChildren.Num(); ++Y)
			{
				if (X == 0 && Y == 0 && Z == 0)
				{
					continue; // Skip parent position
				}
				
				if (SpawnedChildren[ChildIndex].IsValid() && IsBeltSupportHologram(SpawnedChildren[ChildIndex].Get()))
				{
					GridToHologram.Add(PackGridPos(X, Y, Z), SpawnedChildren[ChildIndex].Get());
				}
				ChildIndex++;
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 Mapped %d conveyor poles to grid positions"), GridToHologram.Num());

	// Connect neighbors along the primary axis:
	// - Most belt supports: X-neighbors ([X,Y,Z] -> [X+1,Y,Z])
	// - Wall conveyor poles: Y-neighbors ([X,Y,Z] -> [X,Y+1,Z]) because they scale along Y
	int32 BeltIndex = 0;
	const int32 PrimaryCount = bConnectAlongY ? YCount : XCount;
	for (int32 Z = 0; Z < ZCount; ++Z)
	{
		for (int32 Y = 0; Y < YCount; ++Y)
		{
			for (int32 X = 0; X < XCount; ++X)
			{
				// Skip if we're at the last position along the primary axis
				if (bConnectAlongY && Y >= YCount - 1) continue;
				if (!bConnectAlongY && X >= XCount - 1) continue;

				AFGHologram** SourcePtr = GridToHologram.Find(PackGridPos(X, Y, Z));
				AFGHologram** TargetPtr = bConnectAlongY
					? GridToHologram.Find(PackGridPos(X, Y + 1, Z))
					: GridToHologram.Find(PackGridPos(X + 1, Y, Z));
				
				if (!SourcePtr || !TargetPtr || !*SourcePtr || !*TargetPtr)
				{
					continue; // One or both neighbors missing
				}
				
				// ========================================================================
				// DISTANCE AND ANGLE RESTRICTIONS
				// ========================================================================
				// Skip if poles are too far apart (>50m) or angle too steep (>30°)
				FVector Pos1 = (*SourcePtr)->GetActorLocation();
				FVector Pos2 = (*TargetPtr)->GetActorLocation();
				float Distance = FVector::Dist(Pos1, Pos2);
				
				// Distance restriction: >56m is too far (game engine limit)
				if (Distance > MAX_PIPE_LENGTH)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("🚧 Belt skipped: distance %.1f cm > 56m between [%d,%d,%d] and [%d,%d,%d]"),
						Distance, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				// Angle restriction: >30° from horizontal
				FVector DirectionVec = (Pos2 - Pos1).GetSafeNormal();
				float VerticalComponent = FMath::Abs(DirectionVec.Z);
				float AngleDegrees = FMath::RadiansToDegrees(FMath::Asin(VerticalComponent));
				if (AngleDegrees > 30.0f)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("🚧 Belt skipped: angle %.1f° > 30° between [%d,%d,%d] and [%d,%d,%d]"),
						AngleDegrees, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				// Direction: Forward connects X -> X+1, Backward connects X+1 -> X
				AFGHologram* SourcePole = bReverseDirection ? *TargetPtr : *SourcePtr;
				AFGHologram* TargetPole = bReverseDirection ? *SourcePtr : *TargetPtr;

				// Get the SnapOnly0 connector from each pole for positioning
				UFGFactoryConnectionComponent* SourceConnector = nullptr;
				UFGFactoryConnectionComponent* TargetConnector = nullptr;

				TArray<UFGFactoryConnectionComponent*> SourceConnectors;
				SourcePole->GetComponents<UFGFactoryConnectionComponent>(SourceConnectors);
				if (SourceConnectors.Num() > 0)
				{
					SourceConnector = SourceConnectors[0];
				}
				
				TArray<UFGFactoryConnectionComponent*> TargetConnectors;
				TargetPole->GetComponents<UFGFactoryConnectionComponent>(TargetConnectors);
				if (TargetConnectors.Num() > 0)
				{
					TargetConnector = TargetConnectors[0];
				}

				// Track this pole pair as active
				uint64 PairKey = MakePolePairKey(SourcePole, TargetPole);
				ActivePolePairs.Add(PairKey);

				// Create or update belt hologram child
				AFGHologram* BeltChild = UpdateOrCreateBeltForPolePair(
					ParentHologram, SourcePole, TargetPole,
					SourceConnector, TargetConnector, BeltTier, BeltIndex++);
				
				if (!BeltChild)
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 ⚠️ Failed to create belt for grid [%d,%d,%d] -> [%d,%d,%d]"),
						X, Y, Z, X + 1, Y, Z);
				}
			}
		}
	}
	
	// Remove orphaned belts (poles that were removed from grid)
	RemoveOrphanedBelts(ParentHologram, ActivePolePairs);
	
	// Ensure visibility is correct for all tracked belts
	FStackableBeltState* FinalState = StackableBeltStates.Find(ParentHologram);
	if (FinalState)
	{
		bool bParentLocked = ParentHologram->IsHologramLocked();
		for (auto& Pair : FinalState->BeltsByPolePair)
		{
			if (Pair.Value.IsValid())
			{
				AFGHologram* Belt = Pair.Value.Get();
				Belt->SetActorHiddenInGame(false);
				Belt->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
				
				if (bParentLocked && !Belt->IsHologramLocked())
				{
					Belt->LockHologramPosition(true);
				}
			}
		}
	}
}

void USFAutoConnectService::ProcessStackablePipelineSupports(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 ProcessStackablePipelineSupports: null parent hologram or subsystem"));
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		CleanupAllStackablePipes(ParentHologram);
		UE_LOG(LogSmartFoundations, Log, TEXT("🔧 ProcessStackablePipelineSupports: Skipped - Smart disabled for current action"));
		return;
	}

	// Get runtime settings
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	if (!RuntimeSettings.bEnabled || !RuntimeSettings.bPipeAutoConnectEnabled)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🔧 ProcessStackablePipelineSupports: Auto-connect disabled (global=%d, pipe=%d) - clearing pipe children"),
			RuntimeSettings.bEnabled, RuntimeSettings.bPipeAutoConnectEnabled);
		
		// Clear all existing stackable pipe children when disabled
		CleanupAllStackablePipes(ParentHologram);
		return;
	}
	
	// Track active pole pairs for orphan removal at end
	TSet<uint64> ActivePolePairs;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 ProcessStackablePipelineSupports: Starting pipeline support analysis for %s"), 
		*ParentHologram->GetName());

	// ========================================================================
	// STACKABLE PIPE SUPPORT AUTO-CONNECT
	// ========================================================================
	// This system automatically spawns pipe holograms between scaled stackable
	// pipe supports (poles). Key concepts:
	//
	// COORDINATE SYSTEMS:
	// - World Space: Absolute game coordinates (e.g., X=292000, Y=-127300)
	// - Local Space: Relative to an actor's origin (e.g., X=0, Y=0 at actor center)
	// - Spline data for pipe holograms must be in LOCAL space relative to the
	//   pipe hologram's spawn position. If you use world coordinates, the pipe
	//   will render at the wrong location (offset by the world position).
	//
	// CONNECTOR POSITIONS:
	// - Pole holograms have PCT_SNAP_ONLY connectors (SnapOnly0) that are just
	//   snap points, not fluid connections. Pipes snap TO these but don't
	//   actually connect to them for fluid flow.
	// - When hologram is LOCKED, GetComponentLocation() may return stale data.
	//   Use GetActorLocation() + RotateVector(GetRelativeLocation()) instead.
	//
	// PIPE-TO-PIPE WIRING:
	// - Actual fluid connections are pipe-to-pipe at pole locations
	// - After build, FindCompatibleOverlappingConnection() finds adjacent pipes
	// - SetConnection() wires them together, then MergeNetworks() for fluid flow
	//
	// HOLOGRAM SPAWNING PATTERN (same as Extend):
	// 1. SpawnActor<ASFPipelineHologram> with bDeferConstruction=true
	// 2. SetBuildClass() BEFORE FinishSpawning (or GetDefaultBuildable crashes)
	// 3. Tag with SF_StackableChild for PostHologramPlacement skip
	// 4. FinishSpawning() - creates mSplineComponent
	// 5. SetSplineDataAndUpdate() - sets mSplineData in LOCAL coordinates
	// 6. AddChild() - registers with parent for vanilla build system
	// 7. TriggerMeshGeneration() - creates visible spline mesh components
	// 8. ForceApplyHologramMaterial() - applies hologram material to meshes
	// ========================================================================
	
	// Get hologram helper to find children
	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (!HologramHelper)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 No HologramHelper available"));
		return;
	}

	// Get grid dimensions from CounterState to understand the grid structure
	// Children are spawned in order: Z → X → Y (innermost), skipping [0,0,0] (parent)
	const FSFCounterState& CounterState = Subsystem->GetCounterState();
	int32 XCount = FMath::Abs(CounterState.GridCounters.X);
	int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
	int32 ZCount = FMath::Abs(CounterState.GridCounters.Z);
	
	// Collect ALL supports for pipe connections (parent + children)
	// Parent is at grid position [0,0,0], children follow in spawn order
	TArray<AFGHologram*> AllSupports;
	
	// Include parent hologram first (grid position [0,0,0])
	AllSupports.Add(ParentHologram);
	
	// Then add all children in their spawn order (which matches grid iteration order)
	TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren = HologramHelper->GetSpawnedChildren();
	for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
	{
		if (ChildPtr.IsValid() && IsStackablePipelineSupportHologram(ChildPtr.Get()))
		{
			AllSupports.Add(ChildPtr.Get());
		}
	}

	if (AllSupports.Num() < 2)
	{
		// Need at least 2 supports for auto-connect - skip silently
		return;
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 Found %d stackable pipeline supports (parent + %d children), Grid[%d,%d,%d]"), 
		AllSupports.Num(), AllSupports.Num() - 1, XCount, YCount, ZCount);
	
	// ========================================================================
	// GRID-BASED PIPE CONNECTIONS
	// ========================================================================
	// Connect poles that are X-neighbors in the grid (same Y, same Z, adjacent X).
	// This works correctly with stepping because we use grid indices, not world positions.
	//
	// Grid iteration order (how children are spawned): Z → X → Y
	// For each Z layer:
	//   For each X column:
	//     For each Y row:
	//       Spawn child (skipping [0,0,0] which is parent)
	//
	// To find X-neighbors, we need poles at [X, Y, Z] and [X+1, Y, Z].
	// ========================================================================

	// Get pipe tier and style from global settings
	int32 PipeTier = RuntimeSettings.PipeTierMain;
	bool bWithIndicator = RuntimeSettings.bPipeIndicator;
	
	// Handle tier 0 (Auto mode) - resolve to highest unlocked tier
	if (PipeTier == 0)
	{
		AFGPlayerController* PlayerController = Cast<AFGPlayerController>(
			ParentHologram->GetConstructionInstigator()->GetController());
		PipeTier = Subsystem->GetHighestUnlockedPipeTier(PlayerController);
		if (PipeTier == 0) PipeTier = 2; // Default to Mk2 if detection fails
		UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE PIPE: Auto tier resolved to Mk%d"), PipeTier);
	}

	// Build a map from grid position [X,Y,Z] to hologram for fast neighbor lookup
	// Key = packed grid position, Value = hologram pointer
	TMap<int64, AFGHologram*> GridToHologram;
	
	auto PackGridPos = [](int32 X, int32 Y, int32 Z) -> int64 {
		// Pack X, Y, Z into a single int64 key (each can be -128 to 127)
		return ((int64)(X + 128) << 16) | ((int64)(Y + 128) << 8) | (int64)(Z + 128);
	};
	
	// Parent is at [0,0,0]
	GridToHologram.Add(PackGridPos(0, 0, 0), ParentHologram);
	
	// Map children to their grid positions based on spawn order
	// Spawn order: Z → X → Y, skipping [0,0,0]
	int32 ChildIndex = 0;
	for (int32 Z = 0; Z < ZCount && ChildIndex < SpawnedChildren.Num(); ++Z)
	{
		for (int32 X = 0; X < XCount && ChildIndex < SpawnedChildren.Num(); ++X)
		{
			for (int32 Y = 0; Y < YCount && ChildIndex < SpawnedChildren.Num(); ++Y)
			{
				if (X == 0 && Y == 0 && Z == 0)
				{
					continue; // Skip parent position
				}
				
				if (SpawnedChildren[ChildIndex].IsValid() && IsStackablePipelineSupportHologram(SpawnedChildren[ChildIndex].Get()))
				{
					GridToHologram.Add(PackGridPos(X, Y, Z), SpawnedChildren[ChildIndex].Get());
				}
				ChildIndex++;
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 Mapped %d supports to grid positions"), GridToHologram.Num());

	// Connect X-neighbors: for each grid position, connect to X+1 if it exists
	int32 PipeIndex = 0;
	for (int32 Z = 0; Z < ZCount; ++Z)
	{
		for (int32 Y = 0; Y < YCount; ++Y)
		{
			for (int32 X = 0; X < XCount - 1; ++X)  // Stop at XCount-1 since we connect to X+1
			{
				AFGHologram** SourcePtr = GridToHologram.Find(PackGridPos(X, Y, Z));
				AFGHologram** TargetPtr = GridToHologram.Find(PackGridPos(X + 1, Y, Z));
				
				if (!SourcePtr || !TargetPtr || !*SourcePtr || !*TargetPtr)
				{
					continue; // One or both neighbors missing
				}
				
				// ========================================================================
				// DISTANCE AND ANGLE RESTRICTIONS
				// ========================================================================
				// Skip if poles are too far apart (>50m) or angle too steep (>30°)
				FVector Pos1 = (*SourcePtr)->GetActorLocation();
				FVector Pos2 = (*TargetPtr)->GetActorLocation();
				float Distance = FVector::Dist(Pos1, Pos2);
				
				// Distance restriction: >56m is too far (game engine limit)
				if (Distance > MAX_PIPE_LENGTH)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("🔧 Pipe skipped: distance %.1f cm > 56m between [%d,%d,%d] and [%d,%d,%d]"),
						Distance, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				// Angle restriction: >30° from horizontal
				FVector DirectionVec = (Pos2 - Pos1).GetSafeNormal();
				float VerticalComponent = FMath::Abs(DirectionVec.Z);
				float AngleDegrees = FMath::RadiansToDegrees(FMath::Asin(VerticalComponent));
				if (AngleDegrees > 30.0f)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("🔧 Pipe skipped: angle %.1f° > 30° between [%d,%d,%d] and [%d,%d,%d]"),
						AngleDegrees, X, Y, Z, X + 1, Y, Z);
					continue;
				}
				
				AFGHologram* SourceSupport = *SourcePtr;
				AFGHologram* TargetSupport = *TargetPtr;
				
				// Get the SnapOnly0 connector from each pole hologram for accurate positioning
				UFGPipeConnectionComponent* SourceConnector = nullptr;
				UFGPipeConnectionComponent* TargetConnector = nullptr;
				
				TArray<UFGPipeConnectionComponent*> SourceConnectors;
				SourceSupport->GetComponents<UFGPipeConnectionComponent>(SourceConnectors);
				if (SourceConnectors.Num() > 0)
				{
					SourceConnector = SourceConnectors[0];
				}
				
				TArray<UFGPipeConnectionComponent*> TargetConnectors;
				TargetSupport->GetComponents<UFGPipeConnectionComponent>(TargetConnectors);
				if (TargetConnectors.Num() > 0)
				{
					TargetConnector = TargetConnectors[0];
				}
				
				// Track this pole pair as active
				uint64 PairKey = MakePolePairKey(SourceSupport, TargetSupport);
				ActivePolePairs.Add(PairKey);
				
				// Update or create pipe for this pole pair
				UpdateOrCreatePipeForPolePair(
					ParentHologram,
					SourceSupport,
					TargetSupport,
					SourceConnector,
					TargetConnector,
					PipeTier,
					bWithIndicator,
					PipeIndex++);
			}
		}
	}
	
	// Remove pipes for pole pairs that no longer exist (e.g., grid scaled down)
	RemoveOrphanedPipes(ParentHologram, ActivePolePairs);
	
	// ========================================================================
	// VISIBILITY REFRESH FOR ALL TRACKED PIPES
	// ========================================================================
	// When parent is locked (during scaling), vanilla may hide children.
	// Ensure ALL tracked pipes have correct visibility regardless of pole pair changes.
	// This runs AFTER all updates to catch any pipes that weren't individually updated.
	FStackablePipeState* FinalState = StackablePipeStates.Find(ParentHologram);
	if (FinalState)
	{
		bool bParentLocked = ParentHologram->IsHologramLocked();
		EHologramMaterialState ParentMaterialState = ParentHologram->GetHologramMaterialState();
		
		for (auto& Pair : FinalState->PipesByPolePair)
		{
			if (Pair.Value.IsValid())
			{
				ASFPipelineHologram* Pipe = Cast<ASFPipelineHologram>(Pair.Value.Get());
				if (Pipe)
				{
					// GRID SCALING PATTERN: Children must match parent's lock state for visibility
					// (from SFHologramHelperService::RestoreChildLock documentation)
					bool bPipeLocked = Pipe->IsHologramLocked();
					if (bParentLocked && !bPipeLocked)
					{
						Pipe->LockHologramPosition(true);  // Match parent's locked state
					}
					else if (!bParentLocked && bPipeLocked)
					{
						Pipe->LockHologramPosition(false);  // Match parent's unlocked state
					}
					
					// Ensure visibility and material state
					Pipe->SetActorHiddenInGame(false);
					Pipe->SetPlacementMaterialState(ParentMaterialState);
				}
			}
		}
	}
}

// ========================================================================
// POLE-PAIR BASED PIPE TRACKING
// ========================================================================

uint64 USFAutoConnectService::MakePolePairKey(const AFGHologram* PoleA, const AFGHologram* PoleB)
{
	if (!PoleA || !PoleB) return 0;
	
	uint32 IdA = PoleA->GetUniqueID();
	uint32 IdB = PoleB->GetUniqueID();
	
	// Sort IDs so (A,B) and (B,A) produce the same key
	uint32 LowId = FMath::Min(IdA, IdB);
	uint32 HighId = FMath::Max(IdA, IdB);
	
	return ((uint64)LowId << 32) | (uint64)HighId;
}

AFGHologram* USFAutoConnectService::UpdateOrCreatePipeForPolePair(
	AFGHologram* ParentHologram,
	AFGHologram* SourcePole,
	AFGHologram* TargetPole,
	UFGPipeConnectionComponent* SourceConnector,
	UFGPipeConnectionComponent* TargetConnector,
	int32 PipeTier,
	bool bWithIndicator,
	int32 PipeIndex)
{
	if (!ParentHologram || !SourcePole || !TargetPole || !Subsystem)
	{
		return nullptr;
	}
	
	uint64 PairKey = MakePolePairKey(SourcePole, TargetPole);
	FStackablePipeState& State = StackablePipeStates.FindOrAdd(ParentHologram);
	
	// Get pole positions - these should be WORLD positions
	FVector SourcePolePos = SourcePole->GetActorLocation();
	FVector TargetPolePos = TargetPole->GetActorLocation();
	
	// Calculate positions using pole actor location + connector relative offset
	FVector StartPos, EndPos;
	if (SourceConnector)
	{
		FVector ConnectorRelative = SourceConnector->GetRelativeLocation();
		StartPos = SourcePolePos + SourcePole->GetActorRotation().RotateVector(ConnectorRelative);
	}
	else
	{
		// No connector component on hologram - use pole location + Z offset for snap point
		StartPos = SourcePolePos + FVector(0, 0, 200.0f);
	}
	
	if (TargetConnector)
	{
		FVector ConnectorRelative = TargetConnector->GetRelativeLocation();
		EndPos = TargetPolePos + TargetPole->GetActorRotation().RotateVector(ConnectorRelative);
	}
	else
	{
		// No connector component on hologram - use pole location + Z offset for snap point
		EndPos = TargetPolePos + FVector(0, 0, 200.0f);
	}
	
	// DEBUG: Log positions to diagnose pipe placement issue
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE POSITIONS [%d]: SourcePole=%s @ %s, TargetPole=%s @ %s"),
		PipeIndex, *SourcePole->GetName(), *SourcePolePos.ToString(), *TargetPole->GetName(), *TargetPolePos.ToString());
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE ENDPOINTS [%d]: Start=%s, End=%s, Dist=%.1f"),
		PipeIndex, *StartPos.ToString(), *EndPos.ToString(), FVector::Dist(StartPos, EndPos));
	
	// Issue #291 (pipe variant): route straight toward the partner pole in 3D. Pole forward vector
	// was causing the same bulge/ramp seen on belts when pole forward didn't exactly match the
	// direction between pole endpoints (slight Z tilt or perpendicular pole rotation).
	const FVector Direction = (EndPos - StartPos);
	const FVector ToTarget = Direction.GetSafeNormal();
	const FVector ToSource = (-Direction).GetSafeNormal();

	FVector StartNormal = ToTarget;
	FVector EndNormal = ToSource;

	if (StartNormal.IsNearlyZero())
	{
		StartNormal = SourcePole ? SourcePole->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
	}
	if (EndNormal.IsNearlyZero())
	{
		EndNormal = TargetPole ? (-TargetPole->GetActorForwardVector()).GetSafeNormal() : -FVector::ForwardVector;
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE PIPE: Routing endpoints StartN=%s EndN=%s"), *StartNormal.ToString(), *EndNormal.ToString());
	
	// ========================================================================
	// BUILD 6-POINT SPLINE WITH 50CM STRAIGHT SECTIONS AT EACH END
	// ========================================================================
	// Stackable poles have connectors facing each other horizontally.
	// The pipe should stick out 50cm straight (parallel to ground) from each
	// pole before curving to meet in the middle.
	
	float Distance = FVector::Dist(StartPos, EndPos);
	
	// Skip if too long (game engine limit)
	if (Distance > MAX_PIPE_LENGTH)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 STACKABLE PIPE: Skipping pipe %d - distance %.1f exceeds max %.1f"),
			PipeIndex, Distance, MAX_PIPE_LENGTH);
		return nullptr;
	}
	
	// Horizontal direction between poles (parallel to ground)
	FVector HorizontalDir = EndPos - StartPos;
	HorizontalDir.Z = 0;  // Remove vertical component
	HorizontalDir = HorizontalDir.GetSafeNormal();
	
	// End position in local space
	FVector EndLocal = EndPos - StartPos;
	
	// 50cm straight section at each end
	const float StraightLength = 50.0f;
	
	// Calculate the key positions first
	FVector Point1Pos = HorizontalDir * StraightLength;                    // 50cm out from start
	FVector Point4Pos = EndLocal - HorizontalDir * StraightLength;         // 50cm in from end
	
	// The curve direction is the direct path from Point 1 to Point 4
	FVector CurveVector = Point4Pos - Point1Pos;
	FVector CurveDir = CurveVector.GetSafeNormal();
	float CurveLength = CurveVector.Size();
	
	// Tangent strengths - scale with curve length for smooth transitions
	const float HorizontalTangent = 50.0f;
	float CurveTangent = FMath::Max(50.0f, CurveLength * 0.25f);
	
	// Build 6-point spline in LOCAL space (relative to StartPos)
	TArray<FSplinePointData> SplinePoints;
	
	// Point 0: Start connector - pipe exits horizontally
	FSplinePointData Point0;
	Point0.Location = FVector::ZeroVector;
	Point0.ArriveTangent = HorizontalDir * HorizontalTangent;
	Point0.LeaveTangent = HorizontalDir * HorizontalTangent;
	SplinePoints.Add(Point0);
	
	// Point 1: End of 50cm straight section from start - transition to curve
	FSplinePointData Point1;
	Point1.Location = Point1Pos;
	Point1.ArriveTangent = HorizontalDir * HorizontalTangent;
	Point1.LeaveTangent = CurveDir * CurveTangent;
	SplinePoints.Add(Point1);
	
	// Point 2: 1/3 along the curve section
	FSplinePointData Point2;
	Point2.Location = Point1Pos + CurveDir * (CurveLength * 0.33f);
	Point2.ArriveTangent = CurveDir * CurveTangent;
	Point2.LeaveTangent = CurveDir * CurveTangent;
	SplinePoints.Add(Point2);
	
	// Point 3: 2/3 along the curve section
	FSplinePointData Point3;
	Point3.Location = Point1Pos + CurveDir * (CurveLength * 0.67f);
	Point3.ArriveTangent = CurveDir * CurveTangent;
	Point3.LeaveTangent = CurveDir * CurveTangent;
	SplinePoints.Add(Point3);
	
	// Point 4: Start of 50cm straight section at end - transition from curve
	FSplinePointData Point4;
	Point4.Location = Point4Pos;
	Point4.ArriveTangent = CurveDir * CurveTangent;
	Point4.LeaveTangent = HorizontalDir * HorizontalTangent;
	SplinePoints.Add(Point4);
	
	// Point 5: End connector - pipe enters horizontally
	FSplinePointData Point5;
	Point5.Location = EndLocal;
	Point5.ArriveTangent = HorizontalDir * HorizontalTangent;
	Point5.LeaveTangent = HorizontalDir * HorizontalTangent;
	SplinePoints.Add(Point5);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 STACKABLE PIPE: 6-point spline (dist=%.1f, curve=%.1f)"), Distance, CurveLength);
	
	// Check if we already have a pipe for this pole pair
	TWeakObjectPtr<AFGHologram>* ExistingPipePtr = State.PipesByPolePair.Find(PairKey);
	if (ExistingPipePtr && ExistingPipePtr->IsValid())
	{
		ASFPipelineHologram* ExistingPipe = Cast<ASFPipelineHologram>(ExistingPipePtr->Get());
		if (ExistingPipe)
		{
			// Check if tier/indicator has changed - if so, we need to recreate the pipe
			UClass* DesiredBuildClass = Subsystem->GetPipeClassFromConfig(PipeTier, bWithIndicator, nullptr);
			UClass* CurrentBuildClass = ExistingPipe->GetBuildClass();
			
			if (DesiredBuildClass && CurrentBuildClass != DesiredBuildClass)
			{
				// Tier or indicator changed - destroy existing pipe and create new one below
				UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE PIPE: Tier/indicator changed for pair 0x%016llX - recreating pipe"), PairKey);
				
				// Remove from parent's mChildren array
				FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
				if (ChildrenProp)
				{
					TArray<AFGHologram*>* ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
					if (ParentChildrenArray)
					{
						ParentChildrenArray->Remove(ExistingPipe);
					}
				}
				
				ExistingPipe->Destroy();
				State.PipesByPolePair.Remove(PairKey);
				ExistingPipePtr = nullptr;  // Fall through to create new pipe
			}
			else
			{
			// ========================================================================
			// GRID SCALING PATTERN: Temporary unlock -> Update -> Restore lock
			// Same pattern as SFGridSpawnerService uses for grid children visibility
			// ========================================================================
			bool bParentLocked = ParentHologram->IsHologramLocked();
			bool bPipeWasLocked = ExistingPipe->IsHologramLocked();
			
			// Step 1: Temporarily unlock for positioning (same as TemporarilyUnlockChild)
			if (bParentLocked && bPipeWasLocked)
			{
				ExistingPipe->LockHologramPosition(false);
			}
			
			// Step 2: Update position and spline data
			ExistingPipe->SetActorLocation(StartPos);
			if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
			{
				const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
				ExistingPipe->SetRoutingMode(Settings.PipeRoutingMode);
			}
			if (!ExistingPipe->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
			{
				ExistingPipe->SetSplineDataAndUpdate(SplinePoints);
			}
			
			// Step 3: Ensure visibility state is correct
			ExistingPipe->SetActorHiddenInGame(false);
			ExistingPipe->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
			
			// Step 4: Regenerate mesh and apply hologram material
			ExistingPipe->TriggerMeshGeneration();
			ExistingPipe->ForceApplyHologramMaterial();
			
			// Step 5: Restore lock state (same as RestoreChildLock)
			// Children must match parent's lock state for visibility
			if (bParentLocked)
			{
				ExistingPipe->LockHologramPosition(true);
			}
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 STACKABLE PIPE: Updated existing pipe %s for pair 0x%016llX (locked=%d)"), 
				*ExistingPipe->GetName(), PairKey, bParentLocked ? 1 : 0);
			
			return ExistingPipe;
			}  // end else (tier unchanged)
		}  // end if (ExistingPipe)
	}  // end if (ExistingPipePtr)
	
	// CREATE new pipe hologram
	UClass* PipeBuildClass = Subsystem->GetPipeClassFromConfig(PipeTier, bWithIndicator, nullptr);
	if (!PipeBuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 STACKABLE PIPE: No pipe build class for tier %d"), PipeTier);
		return nullptr;
	}
	
	static int32 StackablePipeCounter = 0;
	FName ChildName(*FString::Printf(TEXT("StackablePipe_%d"), StackablePipeCounter++));
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = ChildName;
	SpawnParams.Owner = ParentHologram->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;
	
	UWorld* World = ParentHologram->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	
	ASFPipelineHologram* PipeChild = World->SpawnActor<ASFPipelineHologram>(
		ASFPipelineHologram::StaticClass(),
		StartPos,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!PipeChild)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🔧 STACKABLE PIPE: SpawnActor returned null"));
		return nullptr;
	}
	
	PipeChild->SetReplicates(false);
	PipeChild->SetReplicateMovement(false);
	PipeChild->SetBuildClass(PipeBuildClass);
	PipeChild->Tags.AddUnique(FName(TEXT("SF_StackableChild")));
	
	// Set the pipe recipe so vanilla can aggregate costs for children
	TSubclassOf<UFGRecipe> PipeRecipe = Subsystem->GetPipeRecipeForTier(PipeTier, bWithIndicator);
	if (PipeRecipe)
	{
		PipeChild->SetRecipe(PipeRecipe);
	}
	
	USFHologramDataService::DisableValidation(PipeChild);
	USFHologramDataService::MarkAsChild(PipeChild, ParentHologram, ESFChildHologramType::AutoConnect);
	
	// Store data for post-build wiring
	FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(PipeChild);
	if (HoloData)
	{
		HoloData->bIsStackablePipe = true;
		HoloData->StackablePipeConn0 = SourceConnector;
		HoloData->StackablePipeConn1 = TargetConnector;
		HoloData->StackablePipeIndex = PipeIndex;
	}
	
	PipeChild->FinishSpawning(FTransform(StartPos));
	
	// Set snapped connections for validation
	if (SourceConnector || TargetConnector)
	{
		FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
		if (SnappedProp)
		{
			void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(PipeChild);
			UFGPipeConnectionComponentBase** SnappedArray = static_cast<UFGPipeConnectionComponentBase**>(PropAddr);
			if (SnappedArray)
			{
				SnappedArray[0] = SourceConnector;
				SnappedArray[1] = TargetConnector;
			}
		}
	}
	
	if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
	{
		const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
		PipeChild->SetRoutingMode(Settings.PipeRoutingMode);
	}
	if (!PipeChild->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		PipeChild->SetSplineDataAndUpdate(SplinePoints);
	}
	PipeChild->SetActorHiddenInGame(false);
	PipeChild->SetActorEnableCollision(false);
	PipeChild->SetActorTickEnabled(false);
	PipeChild->RegisterAllComponents();
	PipeChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
	
	ParentHologram->AddChild(PipeChild, ChildName);
	
	PipeChild->TriggerMeshGeneration();
	PipeChild->ForceApplyHologramMaterial();
	
	// Track by pole-pair key
	State.PipesByPolePair.Add(PairKey, PipeChild);
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE PIPE: Created new pipe %s for pair 0x%016llX (dist=%.1f)"), 
		*ChildName.ToString(), PairKey, Distance);
	
	return PipeChild;
}

void USFAutoConnectService::RemoveOrphanedPipes(AFGHologram* ParentHologram, const TSet<uint64>& ActivePolePairs)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackablePipeState* State = StackablePipeStates.Find(ParentHologram);
	if (!State)
	{
		return;
	}
	
	// Get parent's mChildren array for removal
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	// Find and remove pipes for pole pairs that are no longer active
	TArray<uint64> KeysToRemove;
	for (auto& Pair : State->PipesByPolePair)
	{
		if (!ActivePolePairs.Contains(Pair.Key))
		{
			KeysToRemove.Add(Pair.Key);
			
			if (Pair.Value.IsValid())
			{
				AFGHologram* Pipe = Pair.Value.Get();
				if (ParentChildrenArray)
				{
					ParentChildrenArray->Remove(Pipe);
				}
				UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE PIPE: Removing orphaned pipe %s (pair 0x%016llX)"), 
					*Pipe->GetName(), Pair.Key);
				Pipe->Destroy();
			}
		}
	}
	
	for (uint64 Key : KeysToRemove)
	{
		State->PipesByPolePair.Remove(Key);
	}
}

void USFAutoConnectService::CleanupAllStackablePipes(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackablePipeState* State = StackablePipeStates.Find(ParentHologram);
	if (!State || State->PipesByPolePair.Num() == 0)
	{
		return;
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE PIPE CLEANUP: Removing all %d pipes for %s"), 
		State->PipesByPolePair.Num(), *ParentHologram->GetName());
	
	// Get parent's mChildren array for removal
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	for (auto& Pair : State->PipesByPolePair)
	{
		if (Pair.Value.IsValid())
		{
			AFGHologram* Pipe = Pair.Value.Get();
			if (ParentChildrenArray)
			{
				ParentChildrenArray->Remove(Pipe);
			}
			Pipe->Destroy();
		}
	}
	
	State->PipesByPolePair.Empty();
	StackablePipeStates.Remove(ParentHologram);
}

void USFAutoConnectService::FinalizeBeltChildrenVisibility(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		return;
	}
	
	// Get belt previews for this distributor
	TArray<TSharedPtr<FBeltPreviewHelper>>* Previews = DistributorBeltPreviews.Find(ParentHologram);
	if (!Previews || Previews->Num() == 0)
	{
		return;
	}
	
	// Finalize visibility and locking for all belt children (matches stackable pattern)
	bool bParentLocked = ParentHologram->IsHologramLocked();
	EHologramMaterialState ParentMaterialState = ParentHologram->GetHologramMaterialState();
	
	for (const TSharedPtr<FBeltPreviewHelper>& Preview : *Previews)
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
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Finalized visibility for %d belt children of %s (locked=%d)"), 
		Previews->Num(), *ParentHologram->GetName(), bParentLocked ? 1 : 0);
}

void USFAutoConnectService::ProcessPowerPoles(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ ProcessPowerPoles: null parent hologram"));
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (Subsystem && Subsystem->IsSmartDisabledForCurrentAction())
	{
		// Clear any existing previews since we're disabled
		ClearAllPowerPreviews();
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessPowerPoles: Skipped - Smart disabled for current action"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessPowerPoles: Starting power pole analysis for %s"), 
		*ParentHologram->GetName());

	// Get or create power manager for this parent hologram
	TSharedPtr<FSFPowerAutoConnectManager>* ManagerPtr = PowerAutoConnectManagers.Find(ParentHologram);
	
	if (!ManagerPtr || !ManagerPtr->IsValid())
	{
		// Create new manager
		TSharedPtr<FSFPowerAutoConnectManager> NewManager = MakeShared<FSFPowerAutoConnectManager>();
		NewManager->Initialize(Subsystem, this);
		PowerAutoConnectManagers.Add(ParentHologram, NewManager);
		ManagerPtr = &PowerAutoConnectManagers[ParentHologram];
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessPowerPoles: Created new power manager"));
	}

	// Process all power poles through the manager
	if (ManagerPtr->IsValid())
	{
		(*ManagerPtr)->ProcessAllPowerPoles(ParentHologram);
		
		// Power pole affordability is now handled by GetCost hook - vanilla ValidatePlacementAndCost
	// will automatically check the total cost (poles + wires) and add disqualifiers
	}
}

void USFAutoConnectService::ClearAllPowerPreviews()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ClearAllPowerPreviews: Clearing all power line previews (%d managers)"), 
		PowerAutoConnectManagers.Num());
	
	// CRITICAL: Commit planned connections BEFORE clearing anything!
	// This is called when build happens (hologram destroyed), so we need to preserve
	// the planned connections for the deferred OnPowerPoleBuilt processing.
	// The new hologram will trigger ProcessAllPowerPoles which would clear PlannedPoleConnections
	// BEFORE OnActorSpawned can call CommitBuildingConnections, so we do it here first.
	if (Subsystem)
	{
		Subsystem->CommitBuildingConnections();
	}
	
	// Clear all power lines from all managers
	for (auto& Pair : PowerAutoConnectManagers)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->ClearPowerLinePreviews();
			Pair.Value->ClearAllReservations();
			Pair.Value->ResetSpacingState();
		}
	}
	
	// Now remove all managers
	PowerAutoConnectManagers.Empty();
	
	// Note: power wire costs now aggregate through real child holograms.
}

FSFPowerAutoConnectManager* USFAutoConnectService::GetPowerManager(AFGHologram* PoleHologram)
{
	if (!PoleHologram)
	{
		return nullptr;
	}
	
	if (TSharedPtr<FSFPowerAutoConnectManager>* ManagerPtr = PowerAutoConnectManagers.Find(PoleHologram))
	{
		if (ManagerPtr->IsValid())
		{
			return ManagerPtr->Get();
		}
	}
	
	return nullptr;
}

TArray<FPowerPoleGridNode> USFAutoConnectService::AnalyzeGridTopology(const TArray<AFGHologram*>& AllPoles)
{
	TArray<FPowerPoleGridNode> GridNodes;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ AnalyzeGridTopology: Analyzing %d poles"), AllPoles.Num());
	
	if (AllPoles.Num() == 0)
	{
		return GridNodes;
	}
	
	// Issue #244: Use GRID POSITIONS instead of spatial distance for neighbor detection
	// This correctly handles transforms like stepping, rotation, stagger that change positions
	// Grid positions are computed from array index - children spawn in Z→X→Y order
	// Parent is at (0,0,0), first child at (0,1,0), etc.
	
	// Get grid dimensions from subsystem
	USFSubsystem* SmartSubsystem = USFSubsystem::Get(AllPoles[0]->GetWorld());
	if (!SmartSubsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ AnalyzeGridTopology: No subsystem - falling back to empty"));
		return GridNodes;
	}
	
	const FSFCounterState& CounterState = SmartSubsystem->GetCounterState();
	int32 XCount = FMath::Abs(CounterState.GridCounters.X);
	int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
	int32 ZCount = FMath::Abs(CounterState.GridCounters.Z);
	int32 XDir = CounterState.GridCounters.X >= 0 ? 1 : -1;
	int32 YDir = CounterState.GridCounters.Y >= 0 ? 1 : -1;
	int32 ZDir = CounterState.GridCounters.Z >= 0 ? 1 : -1;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ AnalyzeGridTopology: Grid dimensions %dx%dx%d, dirs [%d,%d,%d]"),
		XCount, YCount, ZCount, XDir, YDir, ZDir);
	
	// Build a map from grid position to pole hologram
	// AllPoles[0] = parent at (0,0,0)
	// AllPoles[1+] = children in Z→X→Y spawn order (skipping 0,0,0)
	TMap<FIntVector, AFGHologram*> GridPositionToPole;
	TMap<AFGHologram*, FIntVector> PoleToGridPosition;
	
	// Parent is always at (0,0,0)
	if (AllPoles.Num() > 0 && AllPoles[0])
	{
		FIntVector ParentPos(0, 0, 0);
		GridPositionToPole.Add(ParentPos, AllPoles[0]);
		PoleToGridPosition.Add(AllPoles[0], ParentPos);
	}
	
	// Children are spawned in Z→X→Y order, skipping (0,0,0)
	int32 ChildIndex = 0;
	for (int32 Z = 0; Z < ZCount; ++Z)
	{
		for (int32 X = 0; X < XCount; ++X)
		{
			for (int32 Y = 0; Y < YCount; ++Y)
			{
				if (X == 0 && Y == 0 && Z == 0)
				{
					continue; // Skip parent position
				}
				
				int32 ArrayIndex = ChildIndex + 1; // +1 because parent is at index 0
				if (ArrayIndex >= AllPoles.Num())
				{
					break;
				}
				
				AFGHologram* Pole = AllPoles[ArrayIndex];
				if (Pole)
				{
					FIntVector GridPos(X * XDir, Y * YDir, Z * ZDir);
					GridPositionToPole.Add(GridPos, Pole);
					PoleToGridPosition.Add(Pole, GridPos);
				}
				
				ChildIndex++;
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ AnalyzeGridTopology: Mapped %d poles to grid positions"), PoleToGridPosition.Num());
	
	// Now find neighbors by grid position (adjacent = differ by 1 in exactly one axis)
	for (AFGHologram* Pole : AllPoles)
	{
		if (!Pole)
		{
			continue;
		}
		
		FIntVector* GridPosPtr = PoleToGridPosition.Find(Pole);
		if (!GridPosPtr)
		{
			continue;
		}
		
		FIntVector GridPos = *GridPosPtr;
		
		FPowerPoleGridNode Node;
		Node.Pole = Pole;
		Node.GridPosition = FIntVector2(GridPos.X, GridPos.Y);
		
		// Find X-axis neighbors (differ by 1 in X, same Y and Z)
		FIntVector XPosNeighbor(GridPos.X + XDir, GridPos.Y, GridPos.Z);
		FIntVector XNegNeighbor(GridPos.X - XDir, GridPos.Y, GridPos.Z);
		
		if (AFGHologram** XPosPtr = GridPositionToPole.Find(XPosNeighbor))
		{
			Node.XAxisNeighbors.Add(*XPosPtr);
		}
		if (AFGHologram** XNegPtr = GridPositionToPole.Find(XNegNeighbor))
		{
			Node.XAxisNeighbors.Add(*XNegPtr);
		}
		
		// Find Y-axis neighbors (differ by 1 in Y, same X and Z)
		FIntVector YPosNeighbor(GridPos.X, GridPos.Y + YDir, GridPos.Z);
		FIntVector YNegNeighbor(GridPos.X, GridPos.Y - YDir, GridPos.Z);
		
		if (AFGHologram** YPosPtr = GridPositionToPole.Find(YPosNeighbor))
		{
			Node.YAxisNeighbors.Add(*YPosPtr);
		}
		if (AFGHologram** YNegPtr = GridPositionToPole.Find(YNegNeighbor))
		{
			Node.YAxisNeighbors.Add(*YNegPtr);
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Pole %s at grid[%d,%d,%d]: %d X-neighbors, %d Y-neighbors"),
			*Pole->GetName(), GridPos.X, GridPos.Y, GridPos.Z,
			Node.XAxisNeighbors.Num(), Node.YAxisNeighbors.Num());
		
		GridNodes.Add(Node);
	}
	
	return GridNodes;
}

bool USFAutoConnectService::AreGridAxisNeighbors(AFGHologram* PoleA, AFGHologram* PoleB, 
	const FVector& GridXAxis, const FVector& GridYAxis, bool bXAxis)
{
	if (!PoleA || !PoleB)
	{
		return false;
	}
	
	FVector PosA = PoleA->GetActorLocation();
	FVector PosB = PoleB->GetActorLocation();
	
	// Maximum connection distance: 100m (10000cm)
	float Distance = FVector::Dist(PosA, PosB);
	if (Distance > 10000.0f)
	{
		return false;
	}
	
	FVector Delta = PosB - PosA;
	
	// Project delta onto grid axes
	// GridXAxis = Forward (columns), GridYAxis = Right (rows)
	float DeltaGridX = FVector::DotProduct(Delta, GridXAxis);  // Component along grid X (Forward)
	float DeltaGridY = FVector::DotProduct(Delta, GridYAxis);  // Component along grid Y (Right)
	float DeltaZ = Delta.Z; // Z is always world Z (height)
	
	// Tolerance for "same axis" detection (1m = 100cm)
	const float AxisTolerance = 100.0f;
	
	if (bXAxis)
	{
		// X-axis neighbors: Small Y component and Z difference, significant X component
		// This checks if they're on the same axis, but NOT if they're immediate neighbors
		// The immediate neighbor check is done in ConnectPoleToNeighbors by finding closest
		return FMath::Abs(DeltaGridY) < AxisTolerance && FMath::Abs(DeltaZ) < AxisTolerance;
	}
	else
	{
		// Y-axis neighbors: Small X component and Z difference, significant Y component
		return FMath::Abs(DeltaGridX) < AxisTolerance && FMath::Abs(DeltaZ) < AxisTolerance;
	}
}

int32 USFAutoConnectService::CalculateCableCost(float Distance)
{
	// Convert cm to meters
	float DistanceInMeters = Distance / 100.0f;
	
	// 1 Cable per 25 meters, rounded up
	int32 CablesNeeded = FMath::CeilToInt(DistanceInMeters / 25.0f);
	
	return FMath::Max(1, CablesNeeded); // Minimum 1 cable
}

// ========================================
// STACKABLE CONVEYOR POLE BELT FUNCTIONS
// ========================================

AFGHologram* USFAutoConnectService::UpdateOrCreateBeltForPolePair(
	AFGHologram* ParentHologram,
	AFGHologram* SourcePole,
	AFGHologram* TargetPole,
	UFGFactoryConnectionComponent* SourceConnector,
	UFGFactoryConnectionComponent* TargetConnector,
	int32 BeltTier,
	int32 BeltIndex)
{
	if (!ParentHologram || !SourcePole || !TargetPole || !Subsystem)
	{
		return nullptr;
	}
	
	uint64 PairKey = MakePolePairKey(SourcePole, TargetPole);
	FStackableBeltState& State = StackableBeltStates.FindOrAdd(ParentHologram);
	
	// Get connector positions for spline routing
	// NOTE: Stackable pole connectors are SnapOnly0 - they're snap points, not directional connectors
	// The connector normal points UP (vertical), not toward the next pole
	// For belt routing, we need HORIZONTAL direction between poles
	FVector StartPos = SourceConnector ? SourceConnector->GetComponentLocation() : SourcePole->GetActorLocation();
	FVector EndPos = TargetConnector ? TargetConnector->GetComponentLocation() : TargetPole->GetActorLocation();
	
	// Issue #291: For stackable/ceiling poles, the belt must exit the pole pointing DIRECTLY at
	// the partner pole (including pitch from any height delta). Using the pole's forward vector
	// produced a visible bulge/ramp at the pole whenever pole forward didn't exactly match
	// ToTarget — e.g., slight Z delta between poles (Dot > 0 so the old sign flip didn't fire),
	// or pole rotation perpendicular to the grid axis (Dot ≈ 0, also no flip → S-curve).
	//
	// Issue #268: Wall conveyor poles exit belts perpendicular to the wall (RightVector),
	// which is a deliberate orientation choice on the pole itself — preserve that behavior.
	const bool bUseRightVector = IsWallConveyorPoleHologram(ParentHologram);

	FVector Direction = (EndPos - StartPos);
	const FVector ToTarget = Direction.GetSafeNormal();
	const FVector ToSource = (-Direction).GetSafeNormal();

	FVector StartNormal;
	FVector EndNormal;

	if (bUseRightVector)
	{
		// Wall poles: keep existing "RightVector with sign correction" behavior (#268).
		StartNormal = SourcePole ? SourcePole->GetActorRightVector() : ToTarget;
		StartNormal = StartNormal.GetSafeNormal();
		if (!StartNormal.IsNearlyZero() && !ToTarget.IsNearlyZero() && FVector::DotProduct(StartNormal, ToTarget) < 0.0f)
		{
			StartNormal *= -1.0f;
		}

		EndNormal = TargetPole ? TargetPole->GetActorRightVector() : ToSource;
		EndNormal = EndNormal.GetSafeNormal();
		if (!EndNormal.IsNearlyZero() && !ToSource.IsNearlyZero() && FVector::DotProduct(EndNormal, ToSource) < 0.0f)
		{
			EndNormal *= -1.0f;
		}
	}
	else
	{
		// Stackable/ceiling poles: route straight toward the partner pole in 3D. Pole forward
		// is only used as a degenerate fallback for coincident endpoints.
		StartNormal = ToTarget;
		EndNormal = ToSource;
	}

	// Degenerate fallbacks (overlapping poles / zero direction)
	if (StartNormal.IsNearlyZero())
	{
		StartNormal = SourcePole ? SourcePole->GetActorForwardVector().GetSafeNormal() : FVector::ForwardVector;
	}
	if (EndNormal.IsNearlyZero())
	{
		EndNormal = TargetPole ? (-TargetPole->GetActorForwardVector()).GetSafeNormal() : -FVector::ForwardVector;
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🚧 STACKABLE BELT: Routing from %s to %s, StartN=%s EndN=%s, Dist=%.1f"),
		*StartPos.ToString(),
		*EndPos.ToString(),
		*StartNormal.ToString(),
		*EndNormal.ToString(),
		Direction.Size());
	
	// Check if we already have a belt for this pole pair
	TWeakObjectPtr<AFGHologram>* ExistingBeltPtr = State.BeltsByPolePair.Find(PairKey);
	if (ExistingBeltPtr && ExistingBeltPtr->IsValid())
	{
		ASFConveyorBeltHologram* ExistingBelt = Cast<ASFConveyorBeltHologram>(ExistingBeltPtr->Get());
		if (ExistingBelt)
		{
			// Check if tier has changed - if so, recreate the belt
			UClass* DesiredBuildClass = Subsystem->GetBeltClassForTier(BeltTier, nullptr);
			UClass* CurrentBuildClass = ExistingBelt->GetBuildClass();
			
			if (DesiredBuildClass && CurrentBuildClass != DesiredBuildClass)
			{
				// Tier changed - destroy existing belt and create new one below
				UE_LOG(LogSmartFoundations, Log, TEXT("🚧 STACKABLE BELT: Tier changed for pair 0x%016llX - recreating belt"), PairKey);
				
				FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
				if (ChildrenProp)
				{
					TArray<AFGHologram*>* ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
					if (ParentChildrenArray)
					{
						ParentChildrenArray->Remove(ExistingBelt);
					}
				}
				
				ExistingBelt->Destroy();
				State.BeltsByPolePair.Remove(PairKey);
				ExistingBeltPtr = nullptr;
			}
			else
			{
				// Update existing belt position and spline
				bool bParentLocked = ParentHologram->IsHologramLocked();
				bool bBeltWasLocked = ExistingBelt->IsHologramLocked();
				
				if (bParentLocked && bBeltWasLocked)
				{
					ExistingBelt->LockHologramPosition(false);
				}
				
				ExistingBelt->SetActorLocation(StartPos);
				if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
				{
					const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
					ExistingBelt->SetRoutingMode(Settings.BeltRoutingMode);
				}
				
				// Try using build mode for automatic spline routing, fallback to custom routing
				if (!ExistingBelt->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
				{
					// Build mode not available, use custom routing
					ExistingBelt->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
				}
				
				ExistingBelt->SetActorHiddenInGame(false);
				ExistingBelt->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
				ExistingBelt->TriggerMeshGeneration();
				ExistingBelt->ForceApplyHologramMaterial();
				
				if (bParentLocked)
				{
					ExistingBelt->LockHologramPosition(true);
				}
				
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🚧 STACKABLE BELT: Updated existing belt %s for pair 0x%016llX"), 
					*ExistingBelt->GetName(), PairKey);
				
				return ExistingBelt;
			}
		}
	}
	
	// CREATE new belt hologram
	UClass* BeltBuildClass = Subsystem->GetBeltClassForTier(BeltTier, nullptr);
	if (!BeltBuildClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 STACKABLE BELT: No belt build class for tier %d"), BeltTier);
		return nullptr;
	}
	
	static int32 StackableBeltCounter = 0;
	FName ChildName(*FString::Printf(TEXT("StackableBelt_%d"), StackableBeltCounter++));
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = ChildName;
	SpawnParams.Owner = ParentHologram->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;
	
	UWorld* World = ParentHologram->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	
	ASFConveyorBeltHologram* BeltChild = World->SpawnActor<ASFConveyorBeltHologram>(
		ASFConveyorBeltHologram::StaticClass(),
		StartPos,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!BeltChild)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("🚧 STACKABLE BELT: SpawnActor returned null"));
		return nullptr;
	}
	
	BeltChild->SetReplicates(false);
	BeltChild->SetReplicateMovement(false);
	BeltChild->SetBuildClass(BeltBuildClass);
	BeltChild->Tags.AddUnique(FName(TEXT("SF_StackableChild")));
	
	USFHologramDataService::DisableValidation(BeltChild);
	USFHologramDataService::MarkAsChild(BeltChild, ParentHologram, ESFChildHologramType::AutoConnect);
	
	// Store data for post-build wiring (ConfigureComponents will use this)
	FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(BeltChild);
	if (HoloData)
	{
		HoloData->bIsStackableBelt = true;
		HoloData->StackableBeltIndex = BeltIndex;
		// Store pole connector references for ConfigureComponents to wire after construction
		HoloData->StackableBeltConn0 = SourceConnector;  // Belt output connects to source pole
		HoloData->StackableBeltConn1 = TargetConnector;  // Belt input connects to target pole
	}
	
	BeltChild->FinishSpawning(FTransform(StartPos));
	
	// Set snapped connections to prevent child pole spawning
	BeltChild->SetSnappedConnections(SourceConnector, TargetConnector);
	
	if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
	{
		const auto& Settings = SmartSubsystem->GetAutoConnectRuntimeSettings();
		BeltChild->SetRoutingMode(Settings.BeltRoutingMode);
	}
	
	// Try using build mode for automatic spline routing, fallback to custom routing
	if (!BeltChild->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		// Build mode not available, use custom routing
		BeltChild->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
	}
	
	BeltChild->SetActorHiddenInGame(false);
	BeltChild->SetActorEnableCollision(false);
	BeltChild->SetActorTickEnabled(false);
	BeltChild->RegisterAllComponents();
	BeltChild->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
	
	ParentHologram->AddChild(BeltChild, ChildName);
	
	BeltChild->TriggerMeshGeneration();
	BeltChild->ForceApplyHologramMaterial();
	
	// Track by pole-pair key
	State.BeltsByPolePair.Add(PairKey, BeltChild);
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🚧 STACKABLE BELT: Created new belt %s for pair 0x%016llX"), 
		*ChildName.ToString(), PairKey);
	
	return BeltChild;
}

void USFAutoConnectService::RemoveOrphanedBelts(AFGHologram* ParentHologram, const TSet<uint64>& ActivePolePairs)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackableBeltState* State = StackableBeltStates.Find(ParentHologram);
	if (!State)
	{
		return;
	}
	
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	TArray<uint64> KeysToRemove;
	for (auto& Pair : State->BeltsByPolePair)
	{
		if (!ActivePolePairs.Contains(Pair.Key))
		{
			KeysToRemove.Add(Pair.Key);
			
			if (Pair.Value.IsValid())
			{
				AFGHologram* Belt = Pair.Value.Get();
				if (ParentChildrenArray)
				{
					ParentChildrenArray->Remove(Belt);
				}
				UE_LOG(LogSmartFoundations, Log, TEXT("🚧 STACKABLE BELT: Removing orphaned belt %s (pair 0x%016llX)"), 
					*Belt->GetName(), Pair.Key);
				Belt->Destroy();
			}
		}
	}
	
	for (uint64 Key : KeysToRemove)
	{
		State->BeltsByPolePair.Remove(Key);
	}
}

void USFAutoConnectService::CleanupAllStackableBelts(AFGHologram* ParentHologram)
{
	if (!ParentHologram)
	{
		return;
	}
	
	FStackableBeltState* State = StackableBeltStates.Find(ParentHologram);
	if (!State || State->BeltsByPolePair.Num() == 0)
	{
		return;
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🚧 CleanupAllStackableBelts: Cleaning up %d belt children for %s"),
		State->BeltsByPolePair.Num(), *ParentHologram->GetName());
	
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	TArray<AFGHologram*>* ParentChildrenArray = nullptr;
	if (ChildrenProp)
	{
		ParentChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentHologram);
	}
	
	for (auto& Pair : State->BeltsByPolePair)
	{
		if (Pair.Value.IsValid())
		{
			AFGHologram* Belt = Pair.Value.Get();
			if (ParentChildrenArray)
			{
				ParentChildrenArray->Remove(Belt);
			}
			Belt->Destroy();
		}
	}
	
	State->BeltsByPolePair.Empty();
	StackableBeltStates.Remove(ParentHologram);
}
