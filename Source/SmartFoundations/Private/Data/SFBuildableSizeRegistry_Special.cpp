// SFBuildableSizeRegistry_Special.cpp
// Special and progression building buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterSpecial()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Special.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Special Buildings (SFBuildableSizeRegistry_Special.cpp)"));
	
	// ===================================
	// UNIQUE BUILDINGS (NO SCALING)
	// ===================================
	
	// HUB / Trading Post (Unique starting building - scaling disabled)
	// Inheritance: Holo_TradingPost_C -> FGTradingPostHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Unique building - only one per save, scaling not applicable
	RegisterProfile(
		TEXT("Build_TradingPost_C"),
		FVector(1400.0f, 2000.0f, 850.0f),  // 14m x 20m x 8.5m (reference only)
		false,
		false,  // Scaling disabled - unique building
		TEXT("Holo_TradingPost_C -> FGTradingPostHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Space Elevator (Unique end-game building - scaling disabled)
	// Inheritance: FGSpaceElevatorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Unique building - only one per save, scaling not applicable
	RegisterProfile(
		TEXT("Build_SpaceElevator_C"),
		FVector(1500.0f, 2000.0f, 550.0f),  // 15m x 20m x 5.5m (reference only)
		false,
		false,  // Scaling disabled - unique building
		TEXT("FGSpaceElevatorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// PROGRESSION BUILDINGS
	// ===================================
	
	// MAM (Molecular Analysis Machine - research building)
	// Inheritance: Holo_BuildingGradualBase_C -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Uses registry-based adapter selection (not AFGFactoryHologram)
	// Measured: X=600.000 Y=900.000 Z=450.000 (via spacing test: X-1400, Y-1100, Z-1550)
	RegisterProfile(
		TEXT("Build_Mam_C"),
		FVector(600.0f, 900.0f, 450.0f),  // 6m x 9m x 4.5m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_BuildingGradualBase_C -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// AWESOME Shop / Resource Sink Shop (Progression building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Gets Factory adapter automatically, no registry needed
	// Measured: X=400.000 Y=600.000 Z=500.000 (via spacing test: X-1100, Y-900, Z-500)
	RegisterProfile(
		TEXT("Build_ResourceSinkShop_C"),
		FVector(400.0f, 600.0f, 500.0f),  // 4m x 6m x 5m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// AWESOME Sink / Resource Sink (Point sink building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=1600.000 Y=1400.000 Z=1850.000 (via spacing test: X+0, Y+0, Z+900)
	RegisterProfile(
		TEXT("Build_ResourceSink_C"),
		FVector(1600.0f, 1400.0f, 1850.0f),  // 16m x 14m x 18.5m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// CRAFTING STATIONS
	// ===================================
	
	// Craft Bench / Work Bench (Early game crafting station)
	// Inheritance: Holo_BuildingGradualBase_C -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Uses registry-based adapter selection
	// Measured: X=400.000 Y=600.000 Z=300.000 (via spacing test: X-1600, Y-1400, Z-1700)
	RegisterProfile(
		TEXT("Build_WorkBench_C"),
		FVector(400.0f, 600.0f, 300.0f),  // 4m x 6m x 3m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_BuildingGradualBase_C -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Equipment Workshop (Equipment crafting station)
	// Inheritance: Holo_BuildingGradualBase_C -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Uses registry-based adapter selection
	// Measured: X=600.000 Y=1000.000 Z=500.000 (via spacing test: X-1400, Y-1000, Z-1500)
	RegisterProfile(
		TEXT("Build_Workshop_C"),
		FVector(600.0f, 1000.0f, 500.0f),  // 6m x 10m x 5m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_BuildingGradualBase_C -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// BLUEPRINT DESIGNERS
	// ===================================
	
	// Blueprint Designer Mk.1 (Blueprint creation station)
	// Inheritance: Holo_BlueprintDesigner_C -> FGBlueprintDesignerHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Uses registry-based adapter selection
	// Measured: X=4200.000 Y=4200.000 Z=3300.000 (via spacing test: X+2200, Y+2200, Z+1300)
	RegisterProfile(
		TEXT("Build_BlueprintDesigner_C"),
		FVector(4200.0f, 4200.0f, 3300.0f),  // 42m x 42m x 33m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_BlueprintDesigner_C -> FGBlueprintDesignerHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Blueprint Designer Mk.2 (Blueprint creation station)
	// Inheritance: FGBlueprintDesignerHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Uses registry-based adapter selection
	// Measured: X=5000.000 Y=5000.000 Z=4100.000 (via spacing test: X+3000, Y+3000, Z+2100)
	RegisterProfile(
		TEXT("Build_BlueprintDesigner_MK2_C"),
		FVector(5000.0f, 5000.0f, 4100.0f),  // 50m x 50m x 41m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("FGBlueprintDesignerHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Blueprint Designer Mk.3 (Blueprint creation station)
	// Inheritance: Holo_BlueprintDesigner_Mk3_C -> FGBlueprintDesignerHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Uses registry-based adapter selection
	// Measured: X=5800.000 Y=5800.000 Z=4900.000 (via spacing test: X+3800, Y+3800, Z+2900)
	RegisterProfile(
		TEXT("Build_BlueprintDesigner_Mk3_C"),
		FVector(5800.0f, 5800.0f, 4900.0f),  // 58m x 58m x 49m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_BlueprintDesigner_Mk3_C -> FGBlueprintDesignerHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
}
