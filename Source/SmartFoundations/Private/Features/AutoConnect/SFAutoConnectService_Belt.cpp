// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * USFAutoConnectService - belt-distributor auto-connect processing. Split out of SFAutoConnectService.cpp (slice AC,
 * pure impl-split, one class across .cpp) to keep each file <2k. No behavior change.
 */

#include "Features/AutoConnect/SFAutoConnectServiceImpl.h"

TArray<TSharedPtr<FBeltPreviewHelper>> USFAutoConnectService::ProcessSingleDistributor(
	AFGHologram* DistributorHologram,
	TMap<UFGFactoryConnectionComponent*, AFGHologram*>* ReservedInputs)
{
	if (!DistributorHologram)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("ProcessSingleDistributor: DistributorHologram is null"));
		return TArray<TSharedPtr<FBeltPreviewHelper>>();
	}
	
	// Get runtime settings from subsystem
	if (!Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("ProcessSingleDistributor: Subsystem is null"));
		return TArray<TSharedPtr<FBeltPreviewHelper>>();
	}
	
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	
	// Check if auto-connect is enabled
	if (!RuntimeSettings.bEnabled)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("ProcessSingleDistributor: Auto-connect disabled in runtime settings"));
		return TArray<TSharedPtr<FBeltPreviewHelper>>();
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT(" Processing distributor: %s%s"), 
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
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("ProcessSingleDistributor: BuildClass is null"));
		return BeltPreviewHelpers;
	}
	
	// Determine if this is a splitter or merger based on build class name
	FString BuildClassName = BuildClass->GetFName().ToString();
	bool bIsSplitter = BuildClassName.Contains(TEXT("Splitter"));
	bool bIsMerger = BuildClassName.Contains(TEXT("Merger"));
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   📋 Build class: %s | IsSplitter=%d | IsMerger=%d"), 
		*BuildClassName, bIsSplitter, bIsMerger);
	
	// Find distributor chains for manifold connections
	TArray<AFGHologram*> DistributorChains;
	FindDistributorChains(DistributorHologram, DistributorChains);
	
	// Find compatible buildings nearby
	TArray<AFGBuildable*> CompatibleBuildings;
	FindCompatibleBuildingsForDistributor(DistributorHologram, CompatibleBuildings);
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🏭 Found %d compatible buildings nearby"), CompatibleBuildings.Num());
	
	// If no buildings or chains found, clean up existing previews and return empty array
	if (CompatibleBuildings.Num() == 0 && DistributorChains.Num() == 0)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⚠️ No compatible buildings or chains found - cleaning up existing belt previews"));
		
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
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🧹 Cleaned up %d belt previews (all buildings out of range)"), 
				BeltPreviewHelpers.Num());
			BeltPreviewHelpers.Empty();
		}
		
		return BeltPreviewHelpers;
	}
	
	// If no existing belt previews, create them (first time)
	if (BeltPreviewHelpers.Num() == 0)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   📋 No existing belt previews - creating new ones"));
	}
	else
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   📋 Updating %d existing belt previews"), BeltPreviewHelpers.Num());
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
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🔍 Distributor hologram class: %s"), *DistributorBuildClass->GetName());
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   📋 Found %d inputs and %d outputs from hologram connectors"), 
			DistributorInputs.Num(), DistributorOutputs.Num());
	}
	else
	{
		UE_LOG(LogSmartAutoConnect, Error, TEXT("   ❌ Failed to get build class from distributor hologram"));
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Distributor has %d inputs and %d outputs available"), 
		DistributorInputs.Num(), DistributorOutputs.Num());
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🏷️ Distributor type: %s"), 
		bIsSplitter ? TEXT("Splitter") : bIsMerger ? TEXT("Merger") : TEXT("Unknown"));
	
	// Count available side connectors (middle is reserved for manifold chaining)
	int32 SideConnectorCount = bIsSplitter ? (DistributorOutputs.Num() - 1) : (DistributorInputs.Num() - 1);
	if (SideConnectorCount < 0)
		SideConnectorCount = 0;
	
	const TCHAR* ConnectorType = bIsSplitter ? TEXT("outputs") : TEXT("inputs");
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🔗 MANIFOLD: Reserved middle connector for chaining, using %d side %s for buildings"), 
		SideConnectorCount, ConnectorType);
	
	// NEW: Process distributor chains first (priority 1 for manifold chaining)
	UE_LOG(LogSmartAutoConnect, Log, TEXT("   Processing distributor chains for manifold connections"));
	
	// Create belt previews for distributor-to-distributor chains
	if (RuntimeSettings.bChainDistributors && DistributorChains.Num() > 0)
	{
		for (AFGHologram* ChainTarget : DistributorChains)
		{
			if (!ChainTarget)
				continue;
			if (ChainTarget == DistributorHologram)
			{
				UE_LOG(LogSmartAutoConnect, Warning, TEXT("   ⚠️ Skipping distributor chain to self: %s"), *DistributorHologram->GetName());
				continue;
			}
				
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🔗 Creating distributor chain belt preview: %s -> %s"), 
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
					UE_LOG(LogSmartAutoConnect, Error,
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ⏭️ Manifold belt already exists for this connection - skipping"));
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
					UE_LOG(LogSmartAutoConnect, VeryVerbose,
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
					
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ✅ Distributor chain belt preview created"));
				}
				else
				{
					UE_LOG(LogSmartAutoConnect, Error, TEXT("      Failed to create distributor chain belt preview helper"));
				}
			}
			else
			{
				UE_LOG(LogSmartAutoConnect, Warning, TEXT("      No compatible connectors for distributor chain (%s -> %s)"),
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
	UE_LOG(LogSmartAutoConnect, Log, TEXT("   🔢 Sorted %d buildings by distance from splitter at %s:"), 
		SortedBuildings.Num(), *SplitterPos.ToString());
	for (int32 i = 0; i < SortedBuildings.Num(); i++)
	{
		if (SortedBuildings[i])
		{
			float Dist = FVector::Dist2D(SplitterPos, SortedBuildings[i]->GetActorLocation());
			UE_LOG(LogSmartAutoConnect, Log, TEXT("      [%d] %s @ %.0f cm"), 
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
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ⏭️ Skipping building connections - handled by Orchestrator"));
	
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
		UE_LOG(LogSmartAutoConnect, Log, TEXT("   🏭 [%s] Processing building %d: %s"), 
			*DistributorHologram->GetName(), HelperIndex, *Building->GetName());
		
		bool bIsUpdatingExisting = (HelperIndex < BeltPreviewHelpers.Num());
		
		if (bIsUpdatingExisting)
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔄 Updating existing belt preview"));
		}
		else
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ✨ Creating new belt preview"));
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
						UE_LOG(LogSmartAutoConnect, Log, 
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
						UE_LOG(LogSmartAutoConnect, Log,
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
					UE_LOG(LogSmartAutoConnect, Log, TEXT("      ❌ [%s] NO VALID INPUTS found for building %s (all reserved or misaligned)"), 
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
					UE_LOG(LogSmartAutoConnect, Log, TEXT("      ❌ [%s] WRONG SIDE - splitter behind input %s (alignment %.2f > 0)"), 
						*DistributorHologram->GetName(), *BuildingConnector->GetName(), InputSideAlignment);
					continue;
				}
				
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Selected closest building input: %s (Score: %.0f, side alignment: %.2f)"), 
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Skipping already assigned output: %s"), *Output->GetName());
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ❌ Output %s does NOT face building (alignment: %.2f < 0.5)"), 
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Skipping output %s for building - wrong side (alignment: %.2f)"), 
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Better match: output %s, distance %.0f, alignment %.2f, score %.2f"), 
							*Output->GetName(), Distance, DirectionAlignment, Score);
					}
				}
				
				// If no unassigned outputs available, skip this building
				if (!ClosestConnector)
				{
					UE_LOG(LogSmartAutoConnect, Log, TEXT("      ❌ [%s] NO UNASSIGNED OUTPUTS available for building %s"), 
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      SKIPPING building output %s - already reserved by %s"), 
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
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ⚠️ No building outputs found for %s"), *Building->GetName());
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ❌ Input %s does NOT face building (alignment: %.2f < 0.5)"), 
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
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ⚠️ No unassigned inputs available for building %s"), *Building->GetName());
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
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ⚠️ Skipping building %s - merger is on wrong side of output (would clip through building)"), 
						*Building->GetName());
					continue;
				}
				
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Selected closest building output: %s (Score: %.0f, side alignment: %.2f)"), 
					*BuildingConnector->GetName(), ClosestBuildingOutputDistance, OutputSideAlignment);
				
				// For merger inputs, we'll check angle at the merger input (not building output)
				// since merger inputs naturally have ~180° angle relative to building outputs
				UE_LOG(LogSmartAutoConnect, Log, TEXT("      📐 Merger input connection - angle check will be done at merger input"));
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
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔧 Updated reused helper to Mk%d for building connection"), DesiredTier);
						}
						
						if (bIsSplitter)
						{
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔄 Updating splitter connection: %s (splitter output, %.0f cm away) → %s (building input)"), 
								*ClosestConnector->GetName(), ClosestDistance, *BuildingConnector->GetName());
							
							// Use unified function: splitter output → building input
							if (!CreateOrUpdateBeltPreview(ClosestConnector, BuildingConnector, BeltPreviewHelpers[HelperIndex], 30.0f, false, DistributorHologram))
							{
								// Preview failed - skip to next building (helper will be cleaned up later)
								continue;
							}
							
							// MANIFOLD FIX: Mark splitter output as assigned
							AssignedOutputs.Add(ClosestConnector);
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔒 Assigned output %s to prevent duplicates"), *ClosestConnector->GetName());
							
							// INPUT RESERVATION: Reserve this building input for this distributor
							if (ReservedInputs)
							{
								ReservedInputs->Add(BuildingConnector, DistributorHologram);
								UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🎯 Reserved input %s for %s"), 
									*BuildingConnector->GetName(), *DistributorHologram->GetName());
							}
						}
						else if (bIsMerger)
						{
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔄 Updating merger connection: %s (building output, %.0f cm away) → %s (merger input)"), 
								*BuildingConnector->GetName(), ClosestDistance, *ClosestConnector->GetName());
							
							// Use unified function: building output → merger input
							if (!CreateOrUpdateBeltPreview(BuildingConnector, ClosestConnector, BeltPreviewHelpers[HelperIndex], 30.0f, false, DistributorHologram))
							{
								// Preview failed - skip to next building (helper will be cleaned up later)
								continue;
							}
							
							// MANIFOLD FIX: Mark merger input as assigned
							AssignedOutputs.Add(ClosestConnector);
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔒 Assigned input %s to prevent duplicates"), *ClosestConnector->GetName());
							
							// OUTPUT RESERVATION: Reserve this building output for this merger
							if (ReservedInputs)
							{
								ReservedInputs->Add(BuildingConnector, DistributorHologram);
								UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🎯 Reserved building output %s for merger %s"), 
									*BuildingConnector->GetName(), *DistributorHologram->GetName());
							}
						}
						
						// Mark this helper as updated (building is still in range)
						HelperUpdated[HelperIndex] = true;
						
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ✅ Belt preview updated"));
					}
				}
				else
				{
					// Create new helper
					if (bIsSplitter)
					{
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ✨ Creating splitter connection: %s (splitter output, %.0f cm away) → %s (building input)"), 
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔒 Assigned output %s to prevent duplicates"), *ClosestConnector->GetName());
						
						// INPUT RESERVATION: Reserve this building input for this distributor
						if (ReservedInputs)
						{
							ReservedInputs->Add(BuildingConnector, DistributorHologram);
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🎯 RESERVED input %s for %s"), 
								*BuildingConnector->GetName(), *DistributorHologram->GetName());
						}
						
						// Store helper for cleanup
						BeltPreviewHelpers.Add(BeltHelper);
					}
					else if (bIsMerger)
					{
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ✨ Creating merger connection: %s (building output, %.0f cm away) → %s (merger input)"), 
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
						UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🔒 Assigned input %s to prevent duplicates"), *ClosestConnector->GetName());
						
						// OUTPUT RESERVATION: Reserve this building output for this merger
						if (ReservedInputs)
						{
							ReservedInputs->Add(BuildingConnector, DistributorHologram);
							UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      🎯 RESERVED building output %s for merger %s"), 
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
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      ⚠️ No available connectors for connection"));
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
					UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🧹 AUTO-CONNECT CLEANUP: Removed belt preview for building that went out of range (helper index %d)"), i);
				}
				BeltPreviewHelpers.RemoveAt(i);
				RemovedCount++;
			}
		}
		
		if (RemovedCount > 0)
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("🧹 AUTO-CONNECT CLEANUP: Removed %d belt previews for out-of-range buildings"), RemovedCount);
		}
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ✅ Auto-connect complete: %d belt previews created"), BeltPreviewHelpers.Num());
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
        UE_LOG(LogSmartAutoConnect, Warning, TEXT("CreateOrUpdateBeltPreview: Invalid connectors"));
        return false;
    }

    const FVector OutputPos   = OutputConnector->GetComponentLocation();
    const FVector InputPos    = InputConnector->GetComponentLocation();
    const FVector InputNormal = InputConnector->GetConnectorNormal();
    const FVector OutputNormal= OutputConnector->GetConnectorNormal();

    const FVector BeltDirIn   = (OutputPos - InputPos).GetSafeNormal();  // direction AT input (from output)
    const FVector BeltDirOut  = (InputPos - OutputPos).GetSafeNormal();  // direction AT output (to input)

    // Log connector normals for debugging
    UE_LOG(LogSmartAutoConnect, Verbose,
        TEXT("   📐 Connector normals: OUT=(%.2f,%.2f,%.2f) IN=(%.2f,%.2f,%.2f) | BeltDir=(%.2f,%.2f,%.2f)"),
        OutputNormal.X, OutputNormal.Y, OutputNormal.Z,
        InputNormal.X, InputNormal.Y, InputNormal.Z,
        BeltDirOut.X, BeltDirOut.Y, BeltDirOut.Z);

    // ---- Check minimum belt length (0.5m = 50cm)
    const float BeltLength = FVector::Dist(OutputPos, InputPos);
    const float MinBeltLength = 50.0f;  // 0.5 meters in cm
    
    if (BeltLength < MinBeltLength)
    {
        UE_LOG(LogSmartAutoConnect, Log,
            TEXT("   ❌ BELT REJECTED - TOO SHORT: %.1f cm < %.1f cm minimum (%s → %s)"),
            BeltLength, MinBeltLength, *OutputConnector->GetName(), *InputConnector->GetName());
        
        // Clean up any existing preview helper
        if (BeltHelper.IsValid())
        {
            BeltHelper->DestroyPreview();
            BeltHelper.Reset();
            UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   🗑️ Destroyed existing preview (belt too short)"));
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

        UE_LOG(LogSmartAutoConnect, VeryVerbose,
            TEXT("   Belt angle check: %s → %s | In=%.1f° Out=%.1f° (limit %.1f°)"),
            *OutputConnector->GetName(), *InputConnector->GetName(),
            AngleIn, AngleOut, MaxAngleDegrees);

        if (AngleIn > MaxAngleDegrees || AngleOut > MaxAngleDegrees)
        {
            UE_LOG(LogSmartAutoConnect, Log,
                TEXT("   BELT REJECTED - BAD ANGLE: %s → %s (In %.1f° / Out %.1f° > %.1f°)"),
                *OutputConnector->GetName(), *InputConnector->GetName(),
                AngleIn, AngleOut, MaxAngleDegrees);
            
            // Clean up any existing preview helper
            if (BeltHelper.IsValid())
            {
                BeltHelper->DestroyPreview();
                BeltHelper.Reset();
                UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   Destroyed existing preview (bad angle)"));
            }
            return false;
        }
    }
    else
    {
        UE_LOG(LogSmartAutoConnect, VeryVerbose,
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
            UE_LOG(LogSmartAutoConnect, Error, TEXT("   ❌ Failed to create belt preview helper"));
            return false;
        }
        UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ✨ Created new belt preview helper"));
    }

    BeltHelper->UpdatePreview(OutputConnector, InputConnector);
    if (!BeltHelper->ValidatePlacementAndRegisterAsChild())
    {
        UE_LOG(LogSmartAutoConnect, Log,
            TEXT("   ❌ BELT REJECTED - VANILLA: %s → %s"),
            *OutputConnector->GetName(), *InputConnector->GetName());
        BeltHelper.Reset();
        return false;
    }

    UE_LOG(LogSmartAutoConnect, Log,
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
        UE_LOG(LogSmartAutoConnect, Warning, TEXT("ConnectAnyConnectors: Invalid parameters"));
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
		
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ✅ Connected %s → %s (stored on %s)"),
			*OutputConnector->GetName(), *InputConnector->GetName(), *StorageHologram->GetName());
		return true;
	}

	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   ❌ Failed to create connection %s → %s"),
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
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: Missing connectors"));
		return false;
	}

	UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltFromPreview: Attempting belt from %s to %s"),
		*GetNameSafe(OutputConnector), *GetNameSafe(InputConnector));

	if (OutputConnector->IsConnected() || InputConnector->IsConnected())
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("BuildBeltFromPreview: Connectors already connected, skipping"));
		return false;
	}

	AFGSplineHologram* BaseHolo = Preview->GetHologram();
	ASFConveyorBeltHologram* SmartHolo = Cast<ASFConveyorBeltHologram>(BaseHolo);
	if (!SmartHolo)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: Preview hologram is not a conveyor belt hologram"));
		return false;
	}

	const TArray<FSplinePointData>& SplineData = SmartHolo->GetSplineData();
	if (SplineData.Num() == 0)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: Preview hologram has no spline data"));
		return false;
	}

	UWorld* World = OutputConnector->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: World is null"));
		return false;
	}

	// Use the belt tier from the preview helper (which matches runtime settings)
	int32 BeltTier = Preview->GetBeltTier();
	FString BeltPath = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/ConveyorBeltMk%d/Build_ConveyorBeltMk%d.Build_ConveyorBeltMk%d_C"), BeltTier, BeltTier, BeltTier);
	UClass* BeltClass = LoadObject<UClass>(nullptr, *BeltPath);
	if (!BeltClass)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: Failed to load Mk%d belt class from %s"), BeltTier, *BeltPath);
		return false;
	}
	
	UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltFromPreview: Building Mk%d belt"), BeltTier);

	FVector SpawnPos = SmartHolo->GetActorLocation();
	AFGBuildableConveyorBelt* Belt = World->SpawnActor<AFGBuildableConveyorBelt>(
		BeltClass,
		SpawnPos,
		FRotator::ZeroRotator);
	if (!Belt)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: Failed to spawn belt"));
		return false;
	}

	AFGBuildableConveyorBelt* Resplined = AFGBuildableConveyorBelt::Respline(Belt, SplineData);
	if (!Resplined)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: Respline failed"));
		Belt->Destroy();
		return false;
	}

	Belt = Resplined;
	Belt->OnBuildEffectFinished();

	UFGFactoryConnectionComponent* BeltConnection0 = Belt->GetConnection0();
	UFGFactoryConnectionComponent* BeltConnection1 = Belt->GetConnection1();
	if (!BeltConnection0 || !BeltConnection1)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: Belt missing connection components"));
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
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("BuildBeltFromPreview: Registered belt with subsystem (ChainActor=%s)"),
			Belt->GetConveyorChainActor() ? *Belt->GetConveyorChainActor()->GetName() : TEXT("pending"));
	}
	else
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltFromPreview: No BuildableSubsystem - belt will have no chain actor!"));
	}

	UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltFromPreview: Built belt %s → %s"),
		*OutputConnector->GetName(), *InputConnector->GetName());

	return true;
}

bool USFAutoConnectService::BuildBeltsForDistributor(AFGHologram* DistributorHologram, AFGBuildable* BuiltDistributor)
{
	if (!DistributorHologram || !BuiltDistributor)
	{
		UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltsForDistributor: Invalid args (Holo=%s Built=%s)"),
			*GetNameSafe(DistributorHologram), *GetNameSafe(BuiltDistributor));
		return false;
	}

	// Get stored connector pairs from preview stage
	TArray<TPair<UFGFactoryConnectionComponent*, UFGFactoryConnectionComponent*>>* ConnectorPairs = GetConnectorPairs(DistributorHologram);
	if (!ConnectorPairs || ConnectorPairs->Num() == 0)
	{
		UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltsForDistributor: No connector pairs stored for %s"),
			*GetNameSafe(DistributorHologram));
		return false;
	}

	UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltsForDistributor: Found %d stored connector pairs for hologram %s"),
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
		UE_LOG(LogSmartAutoConnect, Log, TEXT(" Parent distributor - deducting belt costs for entire grid"));
		
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
						UE_LOG(LogSmartAutoConnect, Log, TEXT(" No Build Cost cheat enabled - skipping belt cost deduction"));
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
		UE_LOG(LogSmartAutoConnect, Log, TEXT(" Child distributor - belt costs already deducted by parent"));
	}

	UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltsForDistributor: Building %d belts for %s → %s"),
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
			UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltsForDistributor: Invalid connector pair"));
			continue;
		}

		// Determine which side is the hologram connector
		AActor* HologramOwner = HologramConnector->GetOwner();
		AActor* BuildingOwner = BuildingConnector->GetOwner();

		if (HologramOwner != DistributorHologram && BuildingOwner != DistributorHologram)
		{
			UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltsForDistributor: Pair doesn't involve distributor hologram"));
			continue;
		}

		// Identify if hologram connector is output or input
		const bool bHoloIsOutput = (HologramConnector->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT);
		const bool bHoloIsInput = (HologramConnector->GetDirection() == EFactoryConnectionDirection::FCD_INPUT);

		if (!bHoloIsOutput && !bHoloIsInput)
		{
			UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltsForDistributor: Hologram connector has no direction"));
			continue;
		}

		// Map hologram connector to built distributor connector
		TArray<UFGFactoryConnectionComponent*>& BuiltSideArray = bHoloIsOutput ? BuiltOutputs : BuiltInputs;
		UFGFactoryConnectionComponent* BuiltDistributorConnector = FindMatchingBuildableConnector(BuiltDistributor, HologramConnector, BuiltSideArray);

		if (!BuiltDistributorConnector)
		{
			UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltsForDistributor: No matching built connector for %s"), *GetNameSafe(HologramConnector));
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
			UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltsForDistributor: Failed to determine connection direction"));
			continue;
		}

		// Check if connectors are already connected
		if (OutputConnector->IsConnected() || InputConnector->IsConnected())
		{
			UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("BuildBeltsForDistributor: Connectors already connected, skipping"));
			continue;
		}

		// Establish the connection using vanilla SetConnection
		if (OutputConnector->CanConnectTo(InputConnector))
		{
			OutputConnector->SetConnection(InputConnector);
			SuccessCount++;
			UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltsForDistributor: Connected %s → %s"),
				*GetNameSafe(OutputConnector), *GetNameSafe(InputConnector));
		}
		else
		{
			UE_LOG(LogSmartAutoConnect, Warning, TEXT("BuildBeltsForDistributor: Cannot connect %s → %s"),
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

	UE_LOG(LogSmartAutoConnect, Log, TEXT("BuildBeltsForDistributor: Complete - %d/%d belts built successfully"),
		SuccessCount, ConnectorPairs->Num());

	return SuccessCount > 0;
}

// ========================================
// Distributor Detection
// ========================================

