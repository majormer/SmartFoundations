#include "Features/PowerAutoConnect/SFPowerAutoConnectManager.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Hologram/FGHologram.h"
#include "FGPowerConnectionComponent.h"
#include "EngineUtils.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "FGCircuitConnectionComponent.h"
#include "SmartFoundations.h"  // For LogSmartFoundations
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"
#include "Resources/FGItemDescriptor.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Config/Smart_ConfigStruct.h"
#include "FGCentralStorageSubsystem.h"

FSFPowerAutoConnectManager::FSFPowerAutoConnectManager()
{
}

FSFPowerAutoConnectManager::~FSFPowerAutoConnectManager()
{
	// Clean up all power line previews when manager is destroyed
	ClearPowerLinePreviews();
}

void FSFPowerAutoConnectManager::Initialize(USFSubsystem* InSubsystem, USFAutoConnectService* InAutoConnectService)
{
	Subsystem = InSubsystem;
	AutoConnectService = InAutoConnectService;
}

void FSFPowerAutoConnectManager::ProcessAllPowerPoles(AFGHologram* ParentPoleHologram)
{
	if (!ParentPoleHologram || !Subsystem || !AutoConnectService)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ ProcessAllPowerPoles: Invalid pointers (Parent=%d, Subsystem=%d, Service=%d)"),
			ParentPoleHologram != nullptr, Subsystem != nullptr, AutoConnectService != nullptr);
		return;
	}

	// Issue #248: Detect "insert pole into existing wire" mode
	// When vanilla is inserting a pole into an existing wire, it creates 2 wire child holograms
	// (the split segments). If we add more wire children for auto-connect, vanilla's
	// AFGPowerPoleHologram::Construct() asserts out_children.Num() == 2 and crashes.
	// Detection: Check if parent pole has vanilla WIRE holograms as children (class contains "PowerLine" or "Wire")
	// NOTE: We must NOT skip for child power POLE holograms - those are Smart's scaled children!
	const TArray<AFGHologram*>& ExistingChildren = ParentPoleHologram->GetHologramChildren();
	for (AFGHologram* Child : ExistingChildren)
	{
		if (!Child) continue;
		
		// Only skip if this is a vanilla WIRE hologram (not a Smart wire, not a pole)
		// Smart wire children have our tag, vanilla wire children don't
		FString ChildClassName = Child->GetClass()->GetName();
		bool bIsWireHologram = ChildClassName.Contains(TEXT("PowerLine")) || ChildClassName.Contains(TEXT("Wire"));
		bool bIsSmartChild = Child->Tags.Contains(FName(TEXT("SF_PowerAutoConnectChild")));
		
		if (bIsWireHologram && !bIsSmartChild)
		{
			// Vanilla wire child detected - pole is in "insert into existing wire" mode
			// Skip Smart power auto-connect entirely to avoid assertion crash
			if (PowerLinePreviews.Num() > 0)
			{
				ClearPowerLinePreviews();
			}
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Skipped - pole has vanilla wire child: %s (class: %s)"), 
				*Child->GetName(), *ChildClassName);
			return;
		}
	}

	// Check if power auto-connect is enabled - if not, clear any existing previews and return
	if (!Subsystem->GetAutoConnectRuntimeSettings().bConnectPower)
	{
		if (PowerLinePreviews.Num() > 0)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Power auto-connect disabled - clearing previews"));
			ClearPowerLinePreviews();
		}
		return;
	}

	// Get all power pole child holograms
	TArray<AFGHologram*> AllPoles;
	
	// Add parent pole
	AllPoles.Add(ParentPoleHologram);
	
	// Add all child power poles
	const TArray<AFGHologram*>& Children = ParentPoleHologram->GetHologramChildren();
	for (AFGHologram* Child : Children)
	{
		if (Child && AutoConnectService->IsPowerPoleHologram(Child))
		{
			AllPoles.Add(Child);
		}
	}
	
	// Early exit if no poles at all
	if (AllPoles.Num() < 1)
	{
		// Only log if we have previews to clear
		if (PowerLinePreviews.Num() > 0)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: No poles - clearing previews"));
			ClearPowerLinePreviews();
		}
		return;
	}
	
	// Single parent pole is a valid case - can still connect to buildings
	// Only skip pole-to-pole connections if we have less than 2 poles
	bool bHasMultiplePoles = AllPoles.Num() >= 2;
	
	// DIRTY DETECTION: Check if grid topology or positions have changed
	bool bGridChanged = false;
	
	// Check for child count changes
	if (LastPoleTransforms.Num() != AllPoles.Num())
	{
		bGridChanged = true;
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Grid changed - pole count %d -> %d"), 
			LastPoleTransforms.Num(), AllPoles.Num());
	}
	else
	{
		// Check for position changes (use tolerance for floating point comparison)
		const float PositionTolerance = 1.0f; // 1cm tolerance
		for (AFGHologram* Pole : AllPoles)
		{
			if (!Pole) continue;
			
			FTransform* LastTransform = LastPoleTransforms.Find(Pole);
			if (!LastTransform)
			{
				bGridChanged = true;
				break;
			}
			
			// Compare positions
			FVector CurrentPos = Pole->GetActorLocation();
			FVector LastPos = LastTransform->GetLocation();
			if (!CurrentPos.Equals(LastPos, PositionTolerance))
			{
				bGridChanged = true;
				break;
			}
		}
	}
	
	// Skip processing if nothing has changed - BUT ALWAYS process building connections
	// Building connections need to be updated even if grid topology hasn't changed
	// (e.g., when moving build gun along Y-axis with existing poles)
	if (!bGridChanged)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessAllPowerPoles: No grid changes detected - processing building connections only"));
		
		// Phase 2: Always process building connections even if grid hasn't changed
		if (Subsystem && Subsystem->GetAutoConnectRuntimeSettings().bConnectPower)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessAllPowerPoles: Processing building connections (grid unchanged)"));
			ProcessBuildingConnections(AllPoles);
		}
		
		return;
	}
	
	// CRITICAL: Clean up orphaned previews for poles that no longer exist (removed children)
	CleanupOrphanedPreviews(AllPoles);
	
	// Clear planned pole connections when starting NEW preview phase (grid changed)
	// This must happen BEFORE ClearPowerLinePreviews, since that function doesn't clear them
	// (it can't because it's also called during build phase when we need to preserve them)
	// NOTE: Do NOT clear DeferredPoleConnections here - those are committed connections waiting
	// to be processed during build. Only clear PlannedPoleConnections (current preview session).
	if (Subsystem)
	{
		Subsystem->ClearPlannedPoleConnections();
		// Issue #244 FIX: Removed ClearDeferredPoleConnections() - was wiping committed connections
	}
	
	// CRITICAL: Clear ALL existing previews when grid changes
	// This ensures stale previews from old positions are removed before creating new ones
	// Also handles closest-pole-wins reassignment by starting fresh each update
	ClearPowerLinePreviews();
	
	// Update position cache
	LastPoleTransforms.Empty();
	for (AFGHologram* Pole : AllPoles)
	{
		if (Pole)
		{
			LastPoleTransforms.Add(Pole, Pole->GetActorTransform());
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessAllPowerPoles: START for parent %s (%d poles)"), 
		*ParentPoleHologram->GetName(), AllPoles.Num());

	// Phase 1: Pole-to-pole connections (only if we have multiple poles)
	if (bHasMultiplePoles)
	{
		// Analyze grid topology using AutoConnectService
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessAllPowerPoles: Analyzing grid topology for %d poles"), AllPoles.Num());
		TArray<FPowerPoleGridNode> Grid = AutoConnectService->AnalyzeGridTopology(AllPoles);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessAllPowerPoles: Grid topology found %d nodes"), Grid.Num());

		// Get PowerGridAxis setting: 0=Auto, 1=X, 2=Y, 3=X+Y
		const auto& Config = Subsystem->GetAutoConnectRuntimeSettings();
		int32 GridAxisMode = Config.PowerGridAxis;
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: GridAxisMode=%d (0=Auto, 1=X, 2=Y, 3=X+Y)"), GridAxisMode);
		
		// For Auto mode (0), determine which axis has more poles using GridCounters
		// GridCounters represent the user's logical grid (shown on HUD as X×Y×Z)
		bool bConnectX = true;
		bool bConnectY = true;
		
		if (GridAxisMode == 0) // Auto
		{
			// Use GridCounters to determine which axis has more poles
			// Example: 5×2×1 grid → X=5, Y=2 → connect along X (5 > 2)
			const FSFCounterState& CounterState = Subsystem->GetCounterState();
			int32 XCount = FMath::Abs(CounterState.GridCounters.X);
			int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
			
			// Connect on the axis with more poles (saves connections per pole)
			// A 5×2 grid: connect X gives 2 rows of 5 connected poles (8 total connections)
			//             connect Y gives 5 columns of 2 connected poles (5 total connections)
			// We want the longer chains (X=5), so connect along the higher count
			if (XCount > YCount)
			{
				bConnectX = true;
				bConnectY = false;
				UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Auto mode - X counter higher (%d > %d), connecting X only"), XCount, YCount);
			}
			else if (YCount > XCount)
			{
				bConnectX = false;
				bConnectY = true;
				UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Auto mode - Y counter higher (%d > %d), connecting Y only"), YCount, XCount);
			}
			else
			{
				// Tie - default to X axis
				bConnectX = true;
				bConnectY = false;
				UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Auto mode - Tie (%d = %d), defaulting to X axis"), XCount, YCount);
			}
		}
		else if (GridAxisMode == 1) // X only
		{
			bConnectX = true;
			bConnectY = false;
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: X-only mode"));
		}
		else if (GridAxisMode == 2) // Y only
		{
			bConnectX = false;
			bConnectY = true;
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Y-only mode"));
		}
		else // X+Y (mode 3)
		{
			bConnectX = true;
			bConnectY = true;
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: X+Y mode (full mesh)"));
		}

		// Get grid axes from parent pole for proper direction detection
		FVector GridXAxis = ParentPoleHologram->GetActorForwardVector();
		FVector GridYAxis = ParentPoleHologram->GetActorRightVector();
		
		// Create power line previews for each pole's neighbors (filtered by axis mode)
		int32 ConnectionsCreated = 0;
		static const TArray<AFGHologram*> EmptyArray;  // Reusable empty array to avoid allocations
		for (const FPowerPoleGridNode& Node : Grid)
		{
			// Use references to avoid copying - empty array for disabled axes
			const TArray<AFGHologram*>& FilteredXNeighbors = bConnectX ? Node.XAxisNeighbors : EmptyArray;
			const TArray<AFGHologram*>& FilteredYNeighbors = bConnectY ? Node.YAxisNeighbors : EmptyArray;

			ConnectPoleToNeighbors(Node.Pole, FilteredXNeighbors, FilteredYNeighbors, GridXAxis, GridYAxis);
			ConnectionsCreated += FilteredXNeighbors.Num() + FilteredYNeighbors.Num();
		}
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Created %d pole-to-pole connections"), ConnectionsCreated);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Single pole - skipping pole-to-pole connections"));
	}

	// Phase 2: Building connections (works for single pole or multiple poles)
	if (Subsystem && Subsystem->GetAutoConnectRuntimeSettings().bConnectPower)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessAllPowerPoles: Processing building connections"));
		ProcessBuildingConnections(AllPoles);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: Building connections disabled in config"));
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessAllPowerPoles: COMPLETE - Total previews: %d"), PowerLinePreviews.Num());
}

void FSFPowerAutoConnectManager::ProcessBuildingConnections(const TArray<AFGHologram*>& AllPoles)
{
	if (!Subsystem || AllPoles.Num() == 0) return;
	
	// Clear old planned mappings
	int32 OldCount = Subsystem->PlannedBuildingConnections.Num();
	Subsystem->PlannedBuildingConnections.Empty();
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessBuildingConnections: Cleared %d old mappings, processing %d poles"), OldCount, AllPoles.Num());
	
	// Get configuration - read range directly from config (not cached) so changes take effect immediately
	const auto& RuntimeConfig = Subsystem->GetAutoConnectRuntimeSettings();
	int32 UserReserved = RuntimeConfig.PowerReserved;
	
	// Read PowerConnectRange directly from fresh config so changes take effect without restarting
	FSmart_ConfigStruct FreshConfig = FSmart_ConfigStruct::GetActiveConfig(Subsystem);
	float RangeCm = static_cast<float>(FreshConfig.PowerConnectRange) * 100.0f;  // Convert meters to cm
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessBuildingConnections: Range=%.0fcm (%.1fm), Reserved=%d"),
		RangeCm, RangeCm / 100.0f, UserReserved);
	
	if (RangeCm <= 0.0f) return;
	
	UWorld* World = AllPoles[0]->GetWorld();
	if (!World) return;
	
	// ==========================================================================
	// STEP 1: Calculate available capacity for each pole
	// ==========================================================================
	TMap<AFGHologram*, int32> PoleCapacity;
	
	for (AFGHologram* Pole : AllPoles)
	{
		if (!Pole) continue;
		
		// Determine max connections by tier
		int32 MaxConnections = 4; // Default Mk1
		if (AFGBuildableHologram* BuildHologram = Cast<AFGBuildableHologram>(Pole))
		{
			UClass* BuildClass = BuildHologram->GetBuildClass();
			if (BuildClass)
			{
				FString BuildClassName = BuildClass->GetName();
				if (BuildClassName.Contains("Mk2")) MaxConnections = 7;
				else if (BuildClassName.Contains("Mk3")) MaxConnections = 10;
			}
		}
		
		// Count pole-to-pole connections already used
		int32 GridConnectionsUsed = 0;
		if (PowerLinePreviews.Contains(Pole))
		{
			for (const TSharedPtr<FPowerLinePreviewHelper>& Preview : PowerLinePreviews[Pole])
			{
				if (Preview.IsValid() && Preview->GetEndConnection())
				{
					AActor* TargetOwner = Preview->GetEndConnection()->GetOwner();
					if (TargetOwner)
					{
						FString TargetName = TargetOwner->GetClass()->GetName();
						if (TargetName.Contains(TEXT("PowerPole")) || TargetName.Contains(TEXT("Holo_PowerPole")))
						{
							GridConnectionsUsed++;
						}
					}
				}
			}
		}
		
		int32 ReservedSlots = FMath::Min(UserReserved, MaxConnections - 1);
		int32 Available = MaxConnections - GridConnectionsUsed - ReservedSlots;
		PoleCapacity.Add(Pole, FMath::Max(0, Available));
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessBuildingConnections: Pole %s capacity=%d (max=%d, grid=%d, reserved=%d)"),
			*Pole->GetName(), Available, MaxConnections, GridConnectionsUsed, ReservedSlots);
	}
	
	// ==========================================================================
	// STEP 2: Find all buildings with power connections in range of ANY pole
	// ==========================================================================
	// Store building → power port location for accurate distance calculations
	TMap<AFGBuildable*, FVector> BuildingPowerPortLocations;
	
	for (TActorIterator<AFGBuildable> It(World); It; ++It)
	{
		AFGBuildable* Building = *It;
		if (!Building || !IsValid(Building)) continue;
		if (Building->IsA(AFGBuildablePowerPole::StaticClass())) continue;
		
		TArray<UActorComponent*> PowerComps;
		Building->GetComponents(UFGPowerConnectionComponent::StaticClass(), PowerComps);
		if (PowerComps.Num() == 0) continue;
		
		// Get the actual power port location (not building center)
		// Use first available unconnected port, or first port if all connected
		FVector PowerPortLoc = Building->GetActorLocation(); // Fallback
		UFGPowerConnectionComponent* BestPort = nullptr;
		for (UActorComponent* Comp : PowerComps)
		{
			UFGPowerConnectionComponent* PowerConn = Cast<UFGPowerConnectionComponent>(Comp);
			if (PowerConn)
			{
				if (!BestPort || !PowerConn->IsConnected())
				{
					BestPort = PowerConn;
					PowerPortLoc = PowerConn->GetComponentLocation();
					if (!PowerConn->IsConnected()) break; // Found unconnected, use it
				}
			}
		}
		
		// Check if in range of any pole (use power port location for accurate distance)
		for (AFGHologram* Pole : AllPoles)
		{
			if (!Pole) continue;
			UFGPowerConnectionComponent* PoleConn = GetPowerConnection(Pole);
			FVector PoleLoc = PoleConn ? PoleConn->GetComponentLocation() : Pole->GetActorLocation();
			float Dist = FVector::Dist(PoleLoc, PowerPortLoc);
			if (Dist <= RangeCm)
			{
				BuildingPowerPortLocations.Add(Building, PowerPortLoc);
				break;
			}
		}
	}
	
	// ==========================================================================
	// STEP 3: Create sorted list of ALL (pole, building, distance) tuples
	// ==========================================================================
	struct FPoleBuildingPair
	{
		AFGHologram* Pole;
		AFGBuildable* Building;
		float Distance;
		
		FPoleBuildingPair(AFGHologram* P, AFGBuildable* B, float D) : Pole(P), Building(B), Distance(D) {}
	};
	
	TArray<FPoleBuildingPair> AllPairs;
	for (AFGHologram* Pole : AllPoles)
	{
		if (!Pole) continue;
		UFGPowerConnectionComponent* PoleConn = GetPowerConnection(Pole);
		FVector PoleLoc = PoleConn ? PoleConn->GetComponentLocation() : Pole->GetActorLocation();
		
		// Use cached power port locations for accurate distance (matches actual wire length)
		for (auto& BuildingEntry : BuildingPowerPortLocations)
		{
			AFGBuildable* Building = BuildingEntry.Key;
			FVector PowerPortLoc = BuildingEntry.Value;
			
			float Dist = FVector::Dist(PoleLoc, PowerPortLoc);
			if (Dist <= RangeCm)
			{
				AllPairs.Add(FPoleBuildingPair(Pole, Building, Dist));
			}
		}
	}
	
	// Sort by distance - closest pairs first
	AllPairs.Sort([](const FPoleBuildingPair& A, const FPoleBuildingPair& B) { return A.Distance < B.Distance; });
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessBuildingConnections: Found %d pole-building pairs within range"), AllPairs.Num());
	
	// ==========================================================================
	// STEP 4: Global greedy assignment - closest pole always wins
	// ==========================================================================
	// Because the list is sorted by distance, the FIRST entry for each building
	// is the closest pole to that building. We assign buildings in distance order,
	// so the closest pole always gets first claim (if it has capacity).
	
	TMap<AFGHologram*, TArray<AFGBuildable*>> PoleAssignments;
	TSet<AFGBuildable*> AssignedBuildings;
	
	for (const FPoleBuildingPair& Pair : AllPairs)
	{
		// Skip if building already assigned (a closer pole got it)
		if (AssignedBuildings.Contains(Pair.Building))
		{
			continue;
		}
		
		// Check if this pole has capacity
		int32* Capacity = PoleCapacity.Find(Pair.Pole);
		if (!Capacity || *Capacity <= 0)
		{
			continue;
		}
		
		// Assign building to this pole (it's the closest with capacity)
		PoleAssignments.FindOrAdd(Pair.Pole).Add(Pair.Building);
		AssignedBuildings.Add(Pair.Building);
		(*Capacity)--;
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessBuildingConnections: Assigned %s to %s (dist=%.1f)"),
			*Pair.Building->GetName(), *Pair.Pole->GetName(), Pair.Distance);
	}
	
	// ==========================================================================
	// STEP 5: Create previews for each pole's assigned buildings
	// ==========================================================================
	AFGHologram* ParentPole = AllPoles.Num() > 0 ? AllPoles[0] : nullptr;
	
	for (AFGHologram* Pole : AllPoles)
	{
		TArray<AFGBuildable*>* Assignments = PoleAssignments.Find(Pole);
		if (!Assignments || Assignments->Num() == 0)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ ProcessBuildingConnections: Pole %s has no building assignments"), *Pole->GetName());
			continue;
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessBuildingConnections: Creating %d previews for pole %s"),
			Assignments->Num(), *Pole->GetName());
		
		for (AFGBuildable* Building : *Assignments)
		{
			CreateBuildingPowerLinePreview(Pole, Building);
			
			// Store planned mapping for build phase
			// Use the hologram's actor location (not component location which might be offset)
			FVector PlannedLocation = Pole->GetActorLocation();
			Subsystem->PlannedBuildingConnections.Add(Building, PlannedLocation);
			
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessBuildingConnections: Stored mapping %s → pole at (%.0f, %.0f, %.0f)"),
				*Building->GetName(), PlannedLocation.X, PlannedLocation.Y, PlannedLocation.Z);
		}
		
		// CONTEXT-AWARE SPACING: Auto-adjust spacing to match building width (both X and Y)
		// Only apply for PARENT pole on first connection or when building changes
		if (Pole == ParentPole && Assignments->Num() > 0)
		{
			// Find the closest building to the parent pole (the one we used for spacing reference)
			AFGBuildable* ClosestBuilding = nullptr;
			float ClosestDistance = MAX_FLT;
			FVector ParentLoc = ParentPole->GetActorLocation();
			
			for (AFGBuildable* Building : *Assignments)
			{
				float Dist = FVector::Dist(ParentLoc, Building->GetActorLocation());
				if (Dist < ClosestDistance)
				{
					ClosestDistance = Dist;
					ClosestBuilding = Building;
				}
			}
			
			if (ClosestBuilding)
			{
				UClass* CurrentBuildingClass = ClosestBuilding->GetClass();
				
				// Check if we should apply spacing adjustment
				bool bShouldApply = false;
				if (!bContextSpacingApplied)
				{
					// First time connecting - apply it
					bShouldApply = true;
					UE_LOG(LogSmartFoundations, Log, TEXT("   ⚡ CONTEXT-AWARE SPACING: First power connection detected"));
				}
				else if (LastTargetBuildingClass.IsValid() && LastTargetBuildingClass.Get() != CurrentBuildingClass)
				{
					// Building changed - reset and apply new spacing
					bShouldApply = true;
					UE_LOG(LogSmartFoundations, Log, TEXT("   ⚡ CONTEXT-AWARE SPACING: Target building changed (%s → %s), re-adjusting"),
						*LastTargetBuildingClass->GetName(), *CurrentBuildingClass->GetName());
				}
				else
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ⚡ CONTEXT-AWARE SPACING: Skipping (already applied, user can fine-tune)"));
				}
				
				if (bShouldApply)
				{
					// Query building size from registry
					USFBuildableSizeRegistry::Initialize();
					FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(CurrentBuildingClass);
					
					// Calculate connector spacing: Building width minus 1m buffer
					// Power poles use smaller buffer than pipes/distributors since wires are flexible
					// Example: Constructor is 8m wide, poles are 7m apart (8m - 1m = 7m)
					float BuildingWidth = Profile.DefaultSize.X - 100.0f;  // 100cm = 1m buffer for pole width
					
					if (BuildingWidth > 0.0f)
					{
						// Get current counter state
						FSFCounterState NewState = Subsystem->GetCounterState();
						
						// Set both X and Y spacing to building width (poles are orientation-agnostic)
						NewState.SpacingX = FMath::RoundToInt(BuildingWidth);
						NewState.SpacingY = FMath::RoundToInt(BuildingWidth);
						
						// Update state (triggers HUD refresh and child repositioning)
						Subsystem->UpdateCounterState(NewState);
						
						// Mark as applied and track building class
						bContextSpacingApplied = true;
						LastTargetBuildingClass = CurrentBuildingClass;
						
						UE_LOG(LogSmartFoundations, Log, TEXT("   ⚡ CONTEXT-AWARE SPACING: Auto-adjusted poles to %.1fm x %.1fm (building: %s, width: %.0fcm)"),
							BuildingWidth / 100.0f, BuildingWidth / 100.0f,
							*CurrentBuildingClass->GetName(), BuildingWidth);
					}
				}
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ProcessBuildingConnections: COMPLETE - Stored %d building-to-pole mappings"),
		Subsystem->PlannedBuildingConnections.Num());
}

bool FSFPowerAutoConnectManager::CreateBuildingPowerLinePreview(AFGHologram* SourcePole, AFGBuildable* TargetBuilding)
{
    if (!SourcePole || !TargetBuilding) return false;

    UFGPowerConnectionComponent* SourceConn = GetPowerConnection(SourcePole);
    
    // Find best connection on building
    UFGPowerConnectionComponent* TargetConn = nullptr;
    TArray<UActorComponent*> Components;
    TargetBuilding->GetComponents(UFGPowerConnectionComponent::StaticClass(), Components);
    
    for (UActorComponent* Comp : Components)
    {
        UFGPowerConnectionComponent* Conn = Cast<UFGPowerConnectionComponent>(Comp);
        if (Conn && (Conn->IsConnected() == 0))
        {
            TargetConn = Conn;
            break; // Just take the first free one
        }
    }

    if (!SourceConn || !TargetConn) return false;

    // Check if preview already exists for this pair
    if (PowerLinePreviews.Contains(SourcePole))
    {
        TArray<TSharedPtr<FPowerLinePreviewHelper>>& Previews = PowerLinePreviews[SourcePole];
        for (TSharedPtr<FPowerLinePreviewHelper>& Preview : Previews)
        {
            if (Preview.IsValid() && Preview->GetEndConnection() == TargetConn)
            {
                return Preview->UpdatePreview(SourceConn, TargetConn);
            }
        }
    }
    else
    {
        PowerLinePreviews.Add(SourcePole, TArray<TSharedPtr<FPowerLinePreviewHelper>>());
    }

    // Create new preview
    UWorld* World = Subsystem->GetWorld();
    if (!World)
    {
        return false;
    }

    TSharedPtr<FPowerLinePreviewHelper> NewPreview = MakeShared<FPowerLinePreviewHelper>(World, SourcePole);
    
    if (NewPreview->UpdatePreview(SourceConn, TargetConn))
    {
        PowerLinePreviews[SourcePole].Add(NewPreview);
        return true;
    }

    return false;
}

void FSFPowerAutoConnectManager::ConnectPoleToNeighbors(
	AFGHologram* Pole,
	const TArray<AFGHologram*>& XNeighbors,
	const TArray<AFGHologram*>& YNeighbors,
	const FVector& GridXAxis,
	const FVector& GridYAxis)
{
	if (!Pole)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ ConnectPoleToNeighbors: Pole is null"));
		return;
	}

	FVector PolePos = Pole->GetActorLocation();

	// Connect to X-axis neighbors (BOTH directions: positive and negative along grid X axis)
	if (XNeighbors.Num() > 0)
	{
		// Find closest neighbor in positive X direction and closest in negative X direction
		AFGHologram* ClosestPositiveX = nullptr;
		AFGHologram* ClosestNegativeX = nullptr;
		float MinPositiveDistance = MAX_FLT;
		float MinNegativeDistance = MAX_FLT;
		
		for (AFGHologram* Neighbor : XNeighbors)
		{
			if (!Neighbor) 
			{
				continue;
			}
			
			FVector NeighborPos = Neighbor->GetActorLocation();
			FVector Delta = NeighborPos - PolePos;
			
			// Use dot product to determine direction along grid X axis (not world X)
			float DeltaGridX = FVector::DotProduct(Delta, GridXAxis);
			float Distance = FVector::Dist(PolePos, NeighborPos);
			
			if (DeltaGridX > 0) // Positive grid X direction
			{
				if (Distance < MinPositiveDistance)
				{
					MinPositiveDistance = Distance;
					ClosestPositiveX = Neighbor;
				}
			}
			else // Negative grid X direction
			{
				if (Distance < MinNegativeDistance)
				{
					MinNegativeDistance = Distance;
					ClosestNegativeX = Neighbor;
				}
			}
		}
		
		// Connect to both directions
		if (ClosestPositiveX)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ConnectPoleToNeighbors: Creating X+ preview from %s to %s"), 
				*Pole->GetName(), *ClosestPositiveX->GetName());
			CreatePowerLinePreview(Pole, ClosestPositiveX);
		}
		
		if (ClosestNegativeX)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ConnectPoleToNeighbors: Creating X- preview from %s to %s"), 
				*Pole->GetName(), *ClosestNegativeX->GetName());
			CreatePowerLinePreview(Pole, ClosestNegativeX);
		}
	}

	// Connect to Y-axis neighbors (BOTH directions: positive and negative along grid Y axis)
	if (YNeighbors.Num() > 0)
	{
		// Find closest neighbor in positive Y direction and closest in negative Y direction
		AFGHologram* ClosestPositiveY = nullptr;
		AFGHologram* ClosestNegativeY = nullptr;
		float MinPositiveDistance = MAX_FLT;
		float MinNegativeDistance = MAX_FLT;
		
		for (AFGHologram* Neighbor : YNeighbors)
		{
			if (!Neighbor) 
			{
				continue;
			}
			
			FVector NeighborPos = Neighbor->GetActorLocation();
			FVector Delta = NeighborPos - PolePos;
			
			// Use dot product to determine direction along grid Y axis (not world Y)
			float DeltaGridY = FVector::DotProduct(Delta, GridYAxis);
			float Distance = FVector::Dist(PolePos, NeighborPos);
			
			if (DeltaGridY > 0) // Positive grid Y direction
			{
				if (Distance < MinPositiveDistance)
				{
					MinPositiveDistance = Distance;
					ClosestPositiveY = Neighbor;
				}
			}
			else // Negative grid Y direction
			{
				if (Distance < MinNegativeDistance)
				{
					MinNegativeDistance = Distance;
					ClosestNegativeY = Neighbor;
				}
			}
		}
		
		// Connect to both directions
		if (ClosestPositiveY)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ConnectPoleToNeighbors: Creating Y+ preview from %s to %s"), 
				*Pole->GetName(), *ClosestPositiveY->GetName());
			CreatePowerLinePreview(Pole, ClosestPositiveY);
		}
		
		if (ClosestNegativeY)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ConnectPoleToNeighbors: Creating Y- preview from %s to %s"), 
				*Pole->GetName(), *ClosestNegativeY->GetName());
			CreatePowerLinePreview(Pole, ClosestNegativeY);
		}
	}
}

bool FSFPowerAutoConnectManager::CreatePowerLinePreview(AFGHologram* SourcePole, AFGHologram* TargetPole)
{
	if (!SourcePole || !TargetPole || !Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ CreatePowerLinePreview: Invalid input (Source=%d, Target=%d, Subsystem=%d)"),
			SourcePole != nullptr, TargetPole != nullptr, Subsystem != nullptr);
		return false;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ CreatePowerLinePreview: Attempting connection %s → %s"),
		*SourcePole->GetName(), *TargetPole->GetName());

	// Get power connection components
	UFGPowerConnectionComponent* SourceConnection = GetPowerConnection(SourcePole);
	UFGPowerConnectionComponent* TargetConnection = GetPowerConnection(TargetPole);

	if (!SourceConnection || !TargetConnection)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ CreatePowerLinePreview: Missing power connections - Source=%s Target=%s (SourcePole=%s, TargetPole=%s)"),
			SourceConnection ? TEXT("OK") : TEXT("NULL"), 
			TargetConnection ? TEXT("OK") : TEXT("NULL"),
			*SourcePole->GetName(), *TargetPole->GetName());
		return false;
	}

	// Check if preview already exists for this pole pair (both directions)
	// Since power lines are bi-directional, prevent duplicate connections A→B and B→A
	
	// CRITICAL: Validate SourcePole before using as map key
	if (!SourcePole || !IsValid(SourcePole))
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("⚡ CreatePowerLinePreview: CRITICAL - SourcePole is null or invalid for map access"));
		return false;
	}
	
	if (PowerLinePreviews.Contains(SourcePole))
	{
		TArray<TSharedPtr<FPowerLinePreviewHelper>>& Previews = PowerLinePreviews[SourcePole];
		
		// Find existing preview to this target connection
		for (TSharedPtr<FPowerLinePreviewHelper>& Preview : Previews)
		{
			if (Preview.IsValid() && Preview->GetEndConnection() == TargetConnection)
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("⚡ CreatePowerLinePreview: Connection already exists (same direction)"));
				return Preview->UpdatePreview(SourceConnection, TargetConnection);
			}
		}
	}
	
	// Check if reverse connection already exists (Target→Source when trying Source→Target)
	
	// CRITICAL: Validate TargetPole before using as map key
	if (!TargetPole || !IsValid(TargetPole))
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("⚡ CreatePowerLinePreview: CRITICAL - TargetPole is null or invalid for map access"));
		return false;
	}
	
	if (PowerLinePreviews.Contains(TargetPole))
	{
		TArray<TSharedPtr<FPowerLinePreviewHelper>>& ReversePreviews = PowerLinePreviews[TargetPole];
		
		// Find existing preview from target to source
		for (TSharedPtr<FPowerLinePreviewHelper>& Preview : ReversePreviews)
		{
			if (Preview.IsValid() && Preview->GetEndConnection() == SourceConnection)
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("⚡ CreatePowerLinePreview: Connection already exists (reverse direction) - skipping duplicate"));
				return true; // Connection already exists in reverse direction
			}
		}
	}
	
	// Always ensure SourcePole has an entry in the map (regardless of TargetPole status)
	if (!PowerLinePreviews.Contains(SourcePole))
	{
		// CRITICAL: Double-check SourcePole validity before map operation
		if (!SourcePole || !IsValid(SourcePole))
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("⚡ CreatePowerLinePreview: CRITICAL - SourcePole became null before map Add"));
			return false;
		}
		
		PowerLinePreviews.Add(SourcePole, TArray<TSharedPtr<FPowerLinePreviewHelper>>());
	}

	// Create new preview
	UWorld* World = Subsystem->GetWorld();
	if (!World)
	{
		return false;
	}

	TSharedPtr<FPowerLinePreviewHelper> NewPreview = MakeShared<FPowerLinePreviewHelper>(World, SourcePole);
	
	if (NewPreview->UpdatePreview(SourceConnection, TargetConnection))
	{
		// CRITICAL: Final validation before array access
		if (!SourcePole || !IsValid(SourcePole) || !PowerLinePreviews.Contains(SourcePole))
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("⚡ CreatePowerLinePreview: CRITICAL - SourcePole invalid before array Add"));
			return false;
		}
		
		PowerLinePreviews[SourcePole].Add(NewPreview);
		
		// Store planned pole-to-pole connection for build phase
		// This ensures rotated grids work correctly during build
		FVector SourceLoc = SourcePole->GetActorLocation();
		FVector TargetLoc = TargetPole->GetActorLocation();
		Subsystem->AddPlannedPoleConnection(SourceLoc, TargetLoc);
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ CreatePowerLinePreview: SUCCESS - Preview created"));
		return true;
	}

	UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ CreatePowerLinePreview: FAILED - UpdatePreview returned false"));
	return false;
}

UFGPowerConnectionComponent* FSFPowerAutoConnectManager::GetPowerConnection(AFGHologram* PoleHologram)
{
	if (!PoleHologram)
	{
		return nullptr;
	}

	// Get all power connection components from the pole hologram
	TArray<UActorComponent*> Components;
	PoleHologram->GetComponents(UFGPowerConnectionComponent::StaticClass(), Components);

	// Return the first valid power connection
	for (UActorComponent* Comp : Components)
	{
		UFGPowerConnectionComponent* PowerConn = Cast<UFGPowerConnectionComponent>(Comp);
		if (PowerConn)
		{
			return PowerConn;
		}
	}
	
	// If no power connections found, try circuit connections as fallback
	// (Power poles may have their connections registered as circuit connections)
	TArray<UActorComponent*> CircuitComponents;
	PoleHologram->GetComponents(UFGCircuitConnectionComponent::StaticClass(), CircuitComponents);
	
	for (UActorComponent* Comp : CircuitComponents)
	{
		// Power connection IS a circuit connection, so this cast should work
		UFGPowerConnectionComponent* PowerConn = Cast<UFGPowerConnectionComponent>(Comp);
		if (PowerConn)
		{
			return PowerConn;
		}
	}
	
	UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ GetPowerConnection: No power connections found on %s (had %d components, %d circuit components)"), 
		*PoleHologram->GetName(), Components.Num(), CircuitComponents.Num());
	
	return nullptr;
}

void FSFPowerAutoConnectManager::ClearPowerLinePreviews()
{
	for (auto& Pair : PowerLinePreviews)
	{
		for (TSharedPtr<FPowerLinePreviewHelper>& Preview : Pair.Value)
		{
			if (Preview.IsValid())
			{
				Preview->DestroyPreview();
			}
		}
	}

	PowerLinePreviews.Empty();
	LastPoleTransforms.Empty();
	
	// NOTE: Do NOT clear PlannedPoleConnections here!
	// ClearPowerLinePreviews is called during build phase (when parent hologram is destroyed),
	// and we need the planned connections to persist until CommitBuildingConnections copies them.
	// PlannedPoleConnections is cleared at the start of ProcessAllPowerPoles when starting new preview phase.
}

void FSFPowerAutoConnectManager::CleanupOrphanedPreviews(const TArray<AFGHologram*>& CurrentPoles)
{
	// Find previews for poles that no longer exist
	TArray<AFGHologram*> OrphanedKeys;
	
	for (auto& Pair : PowerLinePreviews)
	{
		if (!CurrentPoles.Contains(Pair.Key))
		{
			OrphanedKeys.Add(Pair.Key);
		}
	}

	// Remove orphaned previews
	for (AFGHologram* Orphan : OrphanedKeys)
	{
		if (TArray<TSharedPtr<FPowerLinePreviewHelper>>* Previews = PowerLinePreviews.Find(Orphan))
		{
			for (TSharedPtr<FPowerLinePreviewHelper>& Preview : *Previews)
			{
				if (Preview.IsValid())
				{
					Preview->DestroyPreview();
				}
			}
		}
		
		PowerLinePreviews.Remove(Orphan);
		LastPoleTransforms.Remove(Orphan);
	}
}

void FSFPowerAutoConnectManager::OnPowerPoleBuilt(AFGBuildablePowerPole* BuiltPole, bool bCostsPreDeducted)
{
	if (!BuiltPole || !Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Invalid parameters"));
		return;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Processing built pole %s"), *BuiltPole->GetName());

	// Check if this pole was built from the grid system
	// Only connect to other grid-built poles, not random poles in the world
	if (!Subsystem->IsGridBuiltPowerPole(BuiltPole))
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: %s is not grid-built - skipping auto-connections"), *BuiltPole->GetName());
		return;
	}

	TArray<AFGBuildablePowerPole*> NearbyPoles;
	
	// Search for existing power poles in the world (reduced radius to prevent connection explosion)
	for (TActorIterator<AFGBuildablePowerPole> It(BuiltPole->GetWorld()); It; ++It)
	{
		AFGBuildablePowerPole* Pole = *It;
		if (Pole && Pole != BuiltPole)
		{
			// Only connect to poles that are also grid-built
			if (!Subsystem->IsGridBuiltPowerPole(Pole))
			{
				continue;
			}
			
			float Distance = FVector::Dist(BuiltPole->GetActorLocation(), Pole->GetActorLocation());
			if (Distance <= 3000.0f) // 30m max distance (reduced from 100m)
			{
				NearbyPoles.Add(Pole);
			}
		}
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Found %d nearby poles"), NearbyPoles.Num());

	// Create power connections to nearby poles (limit connections per pole to prevent explosion)
	int32 MaxConnectionsPerPole = GetMaxConnectionsForPole(BuiltPole);
	int32 ConnectionsMade = 0;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Max connections for this pole: %d"), MaxConnectionsPerPole);
	
	// Count existing connections on this built pole
	TArray<UFGCircuitConnectionComponent*> BuiltCircuitConnectionsAll;
	BuiltPole->GetComponents(UFGCircuitConnectionComponent::StaticClass(), BuiltCircuitConnectionsAll);
	int32 ExistingConnections = 0;
	for (UFGCircuitConnectionComponent* Conn : BuiltCircuitConnectionsAll)
	{
		if (Conn && (Conn->IsConnected() != 0))
		{
			ExistingConnections++;
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Built pole has %d existing connections (max allowed: %d)"), 
		ExistingConnections, MaxConnectionsPerPole);
	
	// Skip if already at max connections
	if (ExistingConnections >= MaxConnectionsPerPole)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Built pole already at max connections - skipping"));
		return;
	}
	
	int32 RemainingConnections = MaxConnectionsPerPole - ExistingConnections;
	
	// Track pole-to-pole connections made to reduce building connections accordingly
	int32 PoleToPoleConnectionsMade = 0;
	
	// Use DEFERRED pole connections queue (persists across builds until consumed!)
	// This replaces the old axis-aligned neighbor detection that failed for non-axis-aligned grids
	FVector BuiltLocation = BuiltPole->GetActorLocation();
	const float PositionTolerance = 50.0f;  // Tolerance for position matching
	
	// Find all planned neighbors for this pole's position from the deferred queue
	TArray<FVector> PlannedNeighborLocations;
	const TArray<TPair<FVector, FVector>>& DeferredConnections = Subsystem->GetDeferredPoleConnections();
	for (const auto& Connection : DeferredConnections)
	{
		// Check if this built pole matches either end of a planned connection
		if (Connection.Key.Equals(BuiltLocation, PositionTolerance))
		{
			PlannedNeighborLocations.AddUnique(Connection.Value);
		}
		else if (Connection.Value.Equals(BuiltLocation, PositionTolerance))
		{
			PlannedNeighborLocations.AddUnique(Connection.Key);
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Found %d planned neighbors from deferred queue (queue size: %d)"), 
		PlannedNeighborLocations.Num(), DeferredConnections.Num());
	
	// Find built poles at planned neighbor locations
	// IMPORTANT: Search ALL grid-built poles, not just NearbyPoles (which has a 30m limit)
	// The deferred queue has exact positions we need, so we search by position
	TArray<AFGBuildablePowerPole*> PolesToConnect;
	const TArray<TWeakObjectPtr<AFGBuildablePowerPole>>& AllGridBuiltPoles = Subsystem->GetGridBuiltPowerPoles();
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Searching %d grid-built poles for matches"), AllGridBuiltPoles.Num());
	
	for (const FVector& NeighborLoc : PlannedNeighborLocations)
	{
		for (const TWeakObjectPtr<AFGBuildablePowerPole>& WeakPole : AllGridBuiltPoles)
		{
			if (!WeakPole.IsValid()) continue;
			AFGBuildablePowerPole* GridPole = WeakPole.Get();
			if (GridPole != BuiltPole && GridPole->GetActorLocation().Equals(NeighborLoc, PositionTolerance))
			{
				PolesToConnect.AddUnique(GridPole);
				break;
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Found %d poles to connect (from planned connections)"), PolesToConnect.Num());
	
	// Temporary map to track which connections we attempted
	TMap<UFGCircuitConnectionComponent*, bool> AttemptedConnections;
	
	for (AFGBuildablePowerPole* NearbyPole : PolesToConnect)
	{
		if (ConnectionsMade >= RemainingConnections)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Reached remaining connections (%d) - stopping"), RemainingConnections);
			break;
		}
		
		// Check if nearby pole already has max connections (using its own tier limit)
		int32 NearbyMaxConnections = GetMaxConnectionsForPole(NearbyPole);
		TArray<UFGCircuitConnectionComponent*> NearbyCircuitConnectionsAll;
		NearbyPole->GetComponents(UFGCircuitConnectionComponent::StaticClass(), NearbyCircuitConnectionsAll);
		int32 NearbyExistingConnections = 0;
		for (UFGCircuitConnectionComponent* Conn : NearbyCircuitConnectionsAll)
		{
			if (Conn && (Conn->IsConnected() != 0))
			{
				NearbyExistingConnections++;
			}
		}
		
		if (NearbyExistingConnections >= NearbyMaxConnections)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Nearby pole already at max connections (%d/%d) - skipping"), NearbyExistingConnections, NearbyMaxConnections);
			continue;
		}
		
		// Check if poles have circuit connections before accessing them
		// Power poles have both power connections (for power distribution) and circuit connections (for wires)
		TArray<UFGCircuitConnectionComponent*> BuiltCircuitConnections;
		TArray<UFGCircuitConnectionComponent*> NearbyCircuitConnections;
		
		// Get circuit connection components (for wire connections)
		BuiltPole->GetComponents(UFGCircuitConnectionComponent::StaticClass(), BuiltCircuitConnections);
		NearbyPole->GetComponents(UFGCircuitConnectionComponent::StaticClass(), NearbyCircuitConnections);
		
		if (BuiltCircuitConnections.Num() == 0 || NearbyCircuitConnections.Num() == 0)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Pole has no circuit connections - skipping"));
			continue;
		}
		
		// Get first available circuit connection components
		UFGCircuitConnectionComponent* BuiltConn = BuiltCircuitConnections[0];
		UFGCircuitConnectionComponent* NearbyConn = NearbyCircuitConnections[0];
		
		// Skip if we already tried this specific pair of poles
		if (AttemptedConnections.Contains(NearbyConn))
		{
			continue;
		}
		
		// Mark this nearby pole as attempted
		AttemptedConnections.Add(NearbyConn, true);
		
		// Check if these two poles are ALREADY connected by a wire
		// This is more reliable than the reservation system during build phase
		bool bAlreadyConnected = false;
		TArray<AFGBuildableWire*> BuiltWires;
		BuiltConn->GetWires(BuiltWires);
		for (AFGBuildableWire* Wire : BuiltWires)
		{
			if (Wire)
			{
				// Check if this wire connects to the nearby pole
				UFGCircuitConnectionComponent* OtherConn = (Wire->GetConnection(0) == BuiltConn) ? Wire->GetConnection(1) : Wire->GetConnection(0);
				if (OtherConn && OtherConn->GetOwner() == NearbyPole)
				{
					bAlreadyConnected = true;
					break;
				}
			}
		}
		
		if (bAlreadyConnected)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Already connected to %s - skipping"), *NearbyPole->GetName());
			continue;
		}
		
		// Issue #244: Spawn wire directly since ConfigureActor defers pole-to-pole connections
		// The child wire holograms exist for cost calculation but don't create persistent wires
		// because unconnected wires are destroyed by the game. Now that both poles are built,
		// we spawn the wire directly and connect it.
		UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
		if (WireClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			
			AFGBuildableWire* NewWire = BuiltPole->GetWorld()->SpawnActor<AFGBuildableWire>(WireClass, BuiltPole->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
			if (NewWire)
			{
				bool bConnected = NewWire->Connect(BuiltConn, NearbyConn);
				if (bConnected)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Spawned and connected wire between %s and %s"), 
						*BuiltPole->GetName(), *NearbyPole->GetName());
					ConnectionsMade++;
					PoleToPoleConnectionsMade++;
				}
				else
				{
					UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Wire spawned but Connect() failed - destroying"));
					NewWire->Destroy();
				}
			}
			else
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Failed to spawn wire"));
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Failed to load wire class"));
		}
		
		// Remove this connection from the deferred queue since it's been processed
		Subsystem->RemoveDeferredPoleConnection(BuiltLocation, NearbyPole->GetActorLocation());
	}

	// Also check for building connections (power poles to buildings)
	// This would connect to nearby buildings that need power
	
	// Count current connections again to check if we have room for building connections
	int32 CurrentConnections = 0;
	TArray<UFGCircuitConnectionComponent*> BuiltCircuitConnectionsCheck;
	BuiltPole->GetComponents(UFGCircuitConnectionComponent::StaticClass(), BuiltCircuitConnectionsCheck);
	for (UFGCircuitConnectionComponent* Conn : BuiltCircuitConnectionsCheck)
	{
		if (Conn && (Conn->IsConnected() != 0))
		{
			CurrentConnections++;
		}
	}
	
	// Get dynamic max connections and reserved slots for this pole
	MaxConnectionsPerPole = GetMaxConnectionsForPole(BuiltPole);
	int32 ReservedSlots = GetReservedSlotsForPole(BuiltPole);
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Current connections: %d/%d (pole-to-pole made: %d, checking buildings)"), 
		CurrentConnections, MaxConnectionsPerPole, PoleToPoleConnectionsMade);
	
	// Calculate available slots for buildings
	// Available = MaxConnections - CurrentConnections - ReservedSlots
	int32 AvailableForBuildings = MaxConnectionsPerPole - CurrentConnections - ReservedSlots;
	
	// Skip building connections if no room for buildings
	if (AvailableForBuildings <= 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Already at max connections - skipping building connections"));
		return;
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Available slots for buildings: %d (total: %d, used: %d, reserved: %d)"), 
		AvailableForBuildings, MaxConnectionsPerPole, CurrentConnections, ReservedSlots);
	
	int32 BuildingConnectionsMade = 0;
	int32 MaxBuildingConnections = AvailableForBuildings;
	
	// ========== FIND NEARBY BUILDINGS TO CONNECT ==========
	// Use a direct approach: find buildings within range that have previews planned
	// This avoids location matching issues with child holograms
	
	// Get the pole's actual location for searching
	FVector PoleLoc = BuiltPole->GetActorLocation();
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Searching for buildings to connect at (%.1f, %.1f, %.1f)"), 
		PoleLoc.X, PoleLoc.Y, PoleLoc.Z);
	
	if (!Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: No subsystem - cannot access planned mappings"));
		return;
	}
	
	// Get config for range - read directly from fresh config so changes take effect immediately
	FSmart_ConfigStruct FreshConfig = FSmart_ConfigStruct::GetActiveConfig(Subsystem);
	float RangeCm = static_cast<float>(FreshConfig.PowerConnectRange) * 100.0f;  // Convert meters to cm
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: CommittedBuildingConnections has %d entries, range=%.0fcm (%.1fm)"), 
		Subsystem->CommittedBuildingConnections.Num(), RangeCm, RangeCm / 100.0f);
	
	// Find buildings that:
	// 1. Are in the planned mappings (had previews)
	// 2. Are within range of THIS pole
	// 3. Haven't been connected yet (still in map)
	TArray<TPair<AFGBuildable*, float>> BuildingsWithDistance;
	
	for (auto& Pair : Subsystem->CommittedBuildingConnections)
	{
		// Use weak pointer safely - check validity BEFORE accessing
		if (!Pair.Key.IsValid())
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ OnPowerPoleBuilt: Skipping invalid/destroyed building in CommittedBuildingConnections"));
			continue;
		}
		
		AFGBuildable* Building = Pair.Key.Get();
		FVector PlannedPoleLocation = Pair.Value;
		
		// Double-check after Get() - should be redundant but safe
		if (!Building) continue;
		
		// CRITICAL: Only connect to buildings that were specifically assigned to THIS pole
		// The map stores Building → PoleLocation, so we must match locations
		float LocationDiff = FVector::Dist(PoleLoc, PlannedPoleLocation);
		if (LocationDiff > 200.0f) // 200cm tolerance for hologram vs built position differences
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Building %s was assigned to pole at (%.0f, %.0f) - this pole at (%.0f, %.0f) - SKIPPING"),
				*Building->GetName(), PlannedPoleLocation.X, PlannedPoleLocation.Y, PoleLoc.X, PoleLoc.Y);
			continue;
		}
		
		// Check distance from THIS pole to the building
		float Distance = FVector::Dist(PoleLoc, Building->GetActorLocation());
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Building %s assigned to THIS pole, distance %.1f (max: %.1f)"), 
			*Building->GetName(), Distance, RangeCm);
		
		if (Distance <= RangeCm)
		{
			BuildingsWithDistance.Add(TPair<AFGBuildable*, float>(Building, Distance));
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Building %s is in range (%.1f cm)"), *Building->GetName(), Distance);
		}
	}
	
	// Sort by distance - closest buildings first
	BuildingsWithDistance.Sort([](const TPair<AFGBuildable*, float>& A, const TPair<AFGBuildable*, float>& B) {
		return A.Value < B.Value;
	});
	
	// Extract just the buildings
	TArray<AFGBuildable*> BuildingsToConnect;
	for (auto& Pair : BuildingsWithDistance)
	{
		BuildingsToConnect.Add(Pair.Key);
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Found %d buildings in range from planned mappings"), BuildingsToConnect.Num());
	
	// Connect to the planned buildings
	for (AFGBuildable* Building : BuildingsToConnect)
	{
		// Safety check - building might have been destroyed since we collected it
		if (!Building || !IsValid(Building))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ OnPowerPoleBuilt: Skipping invalid/destroyed building in BuildingsToConnect"));
			continue;
		}
		
		if (BuildingConnectionsMade >= MaxBuildingConnections)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Reached max building connections (%d) - stopping"), MaxBuildingConnections);
			break;
		}
		
		// Find an available power connection on the building
		TArray<UActorComponent*> PowerComps;
		Building->GetComponents(UFGCircuitConnectionComponent::StaticClass(), PowerComps);
		
		UFGCircuitConnectionComponent* BuildingConn = nullptr;
		for (UActorComponent* Comp : PowerComps)
		{
			UFGCircuitConnectionComponent* PowerConn = Cast<UFGCircuitConnectionComponent>(Comp);
			if (PowerConn && (PowerConn->IsConnected() == 0))
			{
				BuildingConn = PowerConn;
				break;
			}
		}
		
		if (!BuildingConn)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Building %s has no available power connection - skipping"), *Building->GetName());
			continue;
		}
		
		// Get pole circuit connection for distance calculation
		TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
		BuiltPole->GetComponents(UFGCircuitConnectionComponent::StaticClass(), PoleCircuitConns);
		if (PoleCircuitConns.Num() == 0)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Pole has no circuit connections for building connection"));
			continue;
		}
		
		// Calculate distance for cable cost
		FVector StartPos = PoleCircuitConns[0]->GetComponentLocation();
		FVector EndPos = BuildingConn->GetComponentLocation();
		float Distance = FVector::Dist(StartPos, EndPos);
		
		// Deduct cable cost before spawning wire (skip if costs pre-deducted at grid level)
		if (!bCostsPreDeducted && !DeductCableCost(BuiltPole->GetWorld(), Distance))
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Cannot afford power line to building %s (not enough cables)"), *Building->GetName());
			continue;
		}
		
		// Create the connection
		UClass* PowerLineClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
		if (!PowerLineClass)
		{
			UE_LOG(LogSmartFoundations, Error, TEXT("⚡ OnPowerPoleBuilt: Failed to load PowerLine class for building connection"));
			continue;
		}
		
		FActorSpawnParameters SpawnParams;
		SpawnParams.bDeferConstruction = true;
		AFGBuildableWire* PowerWire = BuiltPole->GetWorld()->SpawnActor<AFGBuildableWire>(PowerLineClass, SpawnParams);
		
		if (PowerWire)
		{
			FTransform WireTransform(FRotator::ZeroRotator, PoleLoc);
			PowerWire->FinishSpawning(WireTransform);
			PowerWire->OnBuildEffectFinished();
			
			// Use the circuit connection we already have from distance calculation
			UFGCircuitConnectionComponent* PoleCircuitConn = PoleCircuitConns[0];
			if (PoleCircuitConn && PowerWire->Connect(PoleCircuitConn, BuildingConn))
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: Connected to building %s (deducted cables)"), 
					*Building->GetName());
				BuildingConnectionsMade++;
				
				// Remove from cached connections so other poles don't try to connect
				Subsystem->CommittedBuildingConnections.Remove(Building);
			}
			else
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ OnPowerPoleBuilt: Failed to connect pole to building %s"), 
					*Building->GetName());
				PowerWire->Destroy();
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ OnPowerPoleBuilt: COMPLETE - Connected %d buildings (from planned mappings)"), BuildingConnectionsMade);
}

bool FSFPowerAutoConnectManager::ReserveConnection(UFGCircuitConnectionComponent* Connection, AFGHologram* Pole)
{
	if (!Connection)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ ReserveConnection: Invalid connection parameter"));
		return false;
	}

	// For built poles, Pole can be nullptr - that's ok
	if (!Pole)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ReserveConnection: Reserving connection for built pole"));
	}

	// Check if already reserved
	if (ReservedConnectors.Contains(Connection))
	{
		AFGHologram* ReservedPole = ReservedConnectors[Connection];
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ReserveConnection: Connection already reserved by pole %s"), 
			ReservedPole ? *ReservedPole->GetName() : TEXT("built pole"));
		return false;
	}

	// Reserve the connection
	ReservedConnectors.Add(Connection, Pole);
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ReserveConnection: Reserved connection for pole %s"), 
		Pole ? *Pole->GetName() : TEXT("built pole"));
	return true;
}

void FSFPowerAutoConnectManager::ReleaseConnection(UFGCircuitConnectionComponent* Connection)
{
	if (!Connection)
	{
		return;
	}

	if (ReservedConnectors.Contains(Connection))
	{
		AFGHologram* ReservedPole = ReservedConnectors[Connection];
		ReservedConnectors.Remove(Connection);
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ReleaseConnection: Released connection from pole %s"), 
			ReservedPole ? *ReservedPole->GetName() : TEXT("null"));
	}
}

bool FSFPowerAutoConnectManager::IsConnectionReserved(UFGCircuitConnectionComponent* Connection) const
{
	if (!Connection)
	{
		return false;
	}

	return ReservedConnectors.Contains(Connection);
}

void FSFPowerAutoConnectManager::ClearAllReservations()
{
	int32 Count = ReservedConnectors.Num();
	ReservedConnectors.Empty();
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ClearAllReservations: Cleared %d reservations"), Count);
}

void FSFPowerAutoConnectManager::ResetSpacingState()
{
	bContextSpacingApplied = false;
	LastTargetBuildingClass = nullptr;
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ResetSpacingState: Reset spacing tracking"));
}

int32 FSFPowerAutoConnectManager::GetMaxConnectionsForPole(AFGBuildablePowerPole* PowerPole) const
{
	if (!PowerPole || !Subsystem)
	{
		return 4; // Default fallback
	}

	// Determine pole tier based on the actual pole class
	// The tier is determined by what the user built, not settings
	int32 BaseConnections = 4; // Default (Mk1)
	UClass* PoleClass = PowerPole->GetClass();
	
	if (PoleClass)
	{
		FString ClassName = PoleClass->GetName();
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ GetMaxConnectionsForPole: Pole class = %s"), *ClassName);
		
		if (ClassName.Contains(TEXT("PowerPoleMk2")))
		{
			BaseConnections = 7;
		}
		else if (ClassName.Contains(TEXT("PowerPoleMk3")))
		{
			BaseConnections = 10;
		}
		else if (ClassName.Contains(TEXT("PowerPoleMk4")))
		{
			BaseConnections = 4; // Mk4 doesn't exist, fallback to Mk1
		}
	}
	
	// Get user reservation from runtime settings
	const auto& RuntimeConfig = Subsystem->GetAutoConnectRuntimeSettings();
	int32 UserReserved = RuntimeConfig.PowerReserved;
	
	// Apply user reservation limit (if set)
	// UserReserved = 0 means no reservation, otherwise it's the number of slots to reserve
	// The actual connection limit remains the base tier limit
	int32 FinalLimit = BaseConnections;
	int32 ReservedSlots = 0;
	if (UserReserved > 0)
	{
		ReservedSlots = FMath::Min(UserReserved, BaseConnections - 1); // Reserve at most all but 1 slot
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ GetMaxConnectionsForPole: Base=%d, UserReserved=%d, Final=%d, ReservedSlots=%d"), 
		BaseConnections, UserReserved, FinalLimit, ReservedSlots);
	
	return FinalLimit;
}

int32 FSFPowerAutoConnectManager::GetReservedSlotsForPole(AFGBuildablePowerPole* PowerPole) const
{
	if (!PowerPole || !Subsystem)
	{
		return 0;
	}

	// Get user reservation from runtime settings
	const auto& RuntimeConfig = Subsystem->GetAutoConnectRuntimeSettings();
	int32 UserReserved = RuntimeConfig.PowerReserved;
	
	if (UserReserved <= 0)
	{
		return 0;
	}
	
	// Get base connection limit to calculate reservation
	// Use hardcoded tier values based on actual pole class
	int32 BaseConnections = 4; // Default (Mk1)
	UClass* PoleClass = PowerPole->GetClass();
	
	if (PoleClass)
	{
		FString ClassName = PoleClass->GetName();
		
		if (ClassName.Contains(TEXT("PowerPoleMk2")))
		{
			BaseConnections = 7;
		}
		else if (ClassName.Contains(TEXT("PowerPoleMk3")))
		{
			BaseConnections = 10;
		}
		else if (ClassName.Contains(TEXT("PowerPoleMk4")))
		{
			BaseConnections = 4; // Mk4 doesn't exist, fallback to Mk1
		}
	}
	
	// Reserve slots but leave at least 1 available
	return FMath::Min(UserReserved, BaseConnections - 1);
}

void FSFPowerAutoConnectManager::GetConnectionInfo(AFGBuildablePowerPole* PowerPole, int32& OutConnectedCount, int32& OutTotalCircuitConnections, int32& OutTotalPowerConnections) const
{
	OutConnectedCount = 0;
	OutTotalCircuitConnections = 0;
	OutTotalPowerConnections = 0;
	
	if (!PowerPole)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ GetConnectionInfo: Power pole is null"));
		return;
	}
	
	// Get circuit connections (used for wire connections)
	TArray<UFGCircuitConnectionComponent*> CircuitConnections;
	PowerPole->GetComponents(UFGCircuitConnectionComponent::StaticClass(), CircuitConnections);
	OutTotalCircuitConnections = CircuitConnections.Num();
	
	// Count connected circuit connections
	for (UFGCircuitConnectionComponent* Conn : CircuitConnections)
	{
		if (Conn && (Conn->IsConnected() != 0))
		{
			OutConnectedCount++;
		}
	}
	
	// Get power connections (used for power distribution)
	TArray<UFGPowerConnectionComponent*> PowerConnections;
	PowerPole->GetComponents(UFGPowerConnectionComponent::StaticClass(), PowerConnections);
	OutTotalPowerConnections = PowerConnections.Num();
	
	// Log detailed info
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ GetConnectionInfo: Pole %s - Connected: %d/%d circuit, %d power components"), 
		*PowerPole->GetName(), OutConnectedCount, OutTotalCircuitConnections, OutTotalPowerConnections);
	
	// Log each circuit connection status
	for (int32 i = 0; i < CircuitConnections.Num(); i++)
	{
		UFGCircuitConnectionComponent* Conn = CircuitConnections[i];
		if (Conn)
		{
			bool bIsConnected = (Conn->IsConnected() != 0);
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡   Circuit[%d]: %s"), 
				i, bIsConnected ? TEXT("Connected") : TEXT("Available"));
		}
	}
}

void FSFPowerAutoConnectManager::DebugLogAllPowerPoleConnections() const
{
	if (!Subsystem)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DebugLogAllPowerPoleConnections: Subsystem is null"));
		return;
	}

	UWorld* World = Subsystem->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DebugLogAllPowerPoleConnections: World is null"));
		return;
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ========== POWER POLE CONNECTION DEBUG =========="));
	
	int32 TotalPoles = 0;
	int32 TotalConnected = 0;
	int32 TotalAvailable = 0;
	
	// Iterate all power poles in the world
	for (TActorIterator<AFGBuildablePowerPole> It(World); It; ++It)
	{
		AFGBuildablePowerPole* Pole = *It;
		if (!Pole) continue;
		
		TotalPoles++;
		
		// Get connection info
		int32 ConnectedCount, TotalCircuit, TotalPower;
		GetConnectionInfo(Pole, ConnectedCount, TotalCircuit, TotalPower);
		
		// Get max connections for this pole
		int32 MaxConnections = GetMaxConnectionsForPole(Pole);
		
		// Get pole location for context
		FVector Location = Pole->GetActorLocation();
		
		// Get pole tier
		FString PoleTier = TEXT("Unknown");
		UClass* PoleClass = Pole->GetClass();
		if (PoleClass)
		{
			FString ClassName = PoleClass->GetName();
			if (ClassName.Contains(TEXT("PowerPoleMk1"))) PoleTier = TEXT("Mk1");
			else if (ClassName.Contains(TEXT("PowerPoleMk2"))) PoleTier = TEXT("Mk2");
			else if (ClassName.Contains(TEXT("PowerPoleMk3"))) PoleTier = TEXT("Mk3");
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Pole [%s] %s at (%.0f,%.0f,%.0f): %d/%d connections (%d available)"), 
			*PoleTier, *Pole->GetName(), Location.X, Location.Y, Location.Z, 
			ConnectedCount, MaxConnections, MaxConnections - ConnectedCount);
		
		TotalConnected += ConnectedCount;
		TotalAvailable += (MaxConnections - ConnectedCount);
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ========== SUMMARY =========="));
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Total Poles: %d"), TotalPoles);
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Total Connected: %d"), TotalConnected);
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ Total Available: %d"), TotalAvailable);
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ ======================================"));
}

bool FSFPowerAutoConnectManager::DeductCableCost(UWorld* World, float DistanceInCm)
{
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DeductCableCost: No world"));
		return false;
	}
	
	// Calculate cables needed: 1 per 25 meters, rounded up, minimum 1
	float DistanceInMeters = DistanceInCm / 100.0f;
	int32 CablesNeeded = FMath::CeilToInt(DistanceInMeters / 25.0f);
	CablesNeeded = FMath::Max(1, CablesNeeded);
	
	// Get cable item class
	TSubclassOf<UFGItemDescriptor> CableClass = LoadClass<UFGItemDescriptor>(
		nullptr,
		TEXT("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C"));
	if (!*CableClass)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DeductCableCost: Failed to load Cable class"));
		return false;
	}
	
	// Get central storage subsystem (dimensional storage)
	AFGCentralStorageSubsystem* CentralStorage = AFGCentralStorageSubsystem::Get(World);
	
	// Get player inventory
	AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
	if (!PC)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DeductCableCost: No player controller"));
		return false;
	}
	
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!Character)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DeductCableCost: No player character"));
		return false;
	}
	
	UFGInventoryComponent* Inventory = Character->GetInventory();
	if (!Inventory)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DeductCableCost: No player inventory"));
		return false;
	}
	
	// Match vanilla behavior: check dimensional storage first, then personal inventory
	int32 AvailableInCentral = CentralStorage ? CentralStorage->GetNumItemsFromCentralStorage(CableClass) : 0;
	int32 AvailableInPersonal = Inventory->GetNumItems(CableClass);
	int32 TotalAvailable = AvailableInCentral + AvailableInPersonal;
	
	if (TotalAvailable < CablesNeeded)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ DeductCableCost: Not enough cables (%d/%d available)"), TotalAvailable, CablesNeeded);
		return false;
	}
	
	// Deduct from central storage first
	int32 Remaining = CablesNeeded;
	if (CentralStorage && Remaining > 0)
	{
		int32 RemovedFromCentral = CentralStorage->TryRemoveItemsFromCentralStorage(CableClass, Remaining);
		Remaining -= RemovedFromCentral;
		
		if (RemovedFromCentral > 0)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⚡ DeductCableCost: Deducted %d cables from central storage"), RemovedFromCentral);
		}
	}
	
	// Remove remainder from personal inventory
	if (Remaining > 0)
	{
		Inventory->Remove(CableClass, Remaining);
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ DeductCableCost: Deducted %d cables from personal inventory"), Remaining);
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ DeductCableCost: Deducted %d cables total (distance: %.1fm)"), CablesNeeded, DistanceInMeters);
	
	return true;
}
