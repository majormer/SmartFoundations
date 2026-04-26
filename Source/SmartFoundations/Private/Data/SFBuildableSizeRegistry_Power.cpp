// SFBuildableSizeRegistry_Power.cpp
// Power generation and infrastructure buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterPower()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Power.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Power (SFBuildableSizeRegistry_Power.cpp)"));
	
	// ===================================
	// POWER GENERATORS
	// ===================================
	
	// Biomass Burner (Early game power generator)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=800.000 Y=800.000 Z=700.000 (via spacing test: X+0, Y+0, Z+0 - perfect fit!)
	RegisterProfile(
		TEXT("Build_GeneratorBiomass_Automated_C"),
		FVector(800.0f, 800.0f, 700.0f),  // 8m x 8m x 7m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Coal Generator (Mid-tier power generator)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=1000.000 Y=2600.000 Z=3550.000 (via spacing test: X+0, Y+600, Z+2650)
	RegisterProfile(
		TEXT("Build_GeneratorCoal_C"),
		FVector(1000.0f, 2600.0f, 3550.0f),  // 10m x 26m x 35.5m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Fuel Generator (Advanced power generator)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=2000.000 Y=2000.000 Z=2600.000 (via spacing test: X+150, Y+150, Z+2050)
	// Note: Base size was 1838.478, rounded to nearest 50cm for X/Y dimensions
	RegisterProfile(
		TEXT("Build_GeneratorFuel_C"),
		FVector(2000.0f, 2000.0f, 2600.0f),  // 20m x 20m x 26m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Geothermal Generator (Resource node power generator - scaling disabled)
	// Inheritance: FGGeoThermalGeneratorHologram -> FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Must align with geothermal nodes - scaling not applicable
	RegisterProfile(
		TEXT("Build_GeneratorGeoThermal_C"),
		FVector(2000.0f, 1900.0f, 2000.0f),  // 20m x 19m x 20m (reference only)
		false,
		false,  // Scaling disabled - must align with geothermal nodes
		TEXT("FGGeoThermalGeneratorHologram -> FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Nuclear Power Plant (End-game power generator)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=3600.000 Y=4200.000 Z=3900.000 (via spacing test: X+2400, Y+2200, Z+3100)
	RegisterProfile(
		TEXT("Build_GeneratorNuclear_C"),
		FVector(3600.0f, 4200.0f, 3900.0f),  // 36m x 42m x 39m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Alien Power Augmenter (Somersloop power building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=3000.000 Y=3000.000 Z=3600.000 (via spacing test: X+1000, Y+1000, Z+1600)
	RegisterProfile(
		TEXT("Build_AlienPowerBuilding_C"),
		FVector(3000.0f, 3000.0f, 3600.0f),  // 30m x 30m x 36m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// POWER INFRASTRUCTURE
	// ===================================
	
	// Power Line (Scaling disabled - makes no sense to scale)
	// Note: Dimensions are reference only
	RegisterProfile(
		TEXT("Build_PowerLine_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - power lines don't make sense to scale
		TEXT("FGPowerPoleHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Power Pole Mk.1 (Scalable)
	// Measured: X=100.000 Y=100.000 Z=800.000 (via spacing test: X-100, Y-100, Z+100)
	RegisterProfile(
		TEXT("Build_PowerPoleMk1_C"),
		FVector(100.0f, 100.0f, 800.0f),  // 1m x 1m x 8m
		false,
		true,  // Scaling enabled
		TEXT("FGPowerPoleHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Power Pole Mk.2 (Scalable)
	// Measured: X=100.000 Y=100.000 Z=850.000 (via spacing test: X-100, Y-100, Z+150)
	RegisterProfile(
		TEXT("Build_PowerPoleMk2_C"),
		FVector(100.0f, 100.0f, 850.0f),  // 1m x 1m x 8.5m
		false,
		true,  // Scaling enabled
		TEXT("FGPowerPoleHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Power Pole Mk.3 (Scalable)
	// Measured: X=100.000 Y=100.000 Z=1000.000 (via spacing test: X-100, Y-100, Z+300)
	RegisterProfile(
		TEXT("Build_PowerPoleMk3_C"),
		FVector(100.0f, 100.0f, 1000.0f),  // 1m x 1m x 10m
		false,
		true,  // Scaling enabled
		TEXT("FGPowerPoleHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Power Tower (Scalable)
	// Measured: X=1200.000 Y=1400.000 Z=3000.000 (via spacing test: X+800, Y+1000, Z+1500)
	RegisterProfile(
		TEXT("Build_PowerTower_C"),
		FVector(1200.0f, 1400.0f, 3000.0f),  // 12m x 14m x 30m
		false,
		true,  // Scaling enabled
		TEXT("FGPowerPoleHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Power Tower Platform (Scalable)
	// Measured: X=1200.000 Y=1400.000 Z=3000.000 (validated - same as Power Tower)
	RegisterProfile(
		TEXT("Build_PowerTowerPlatform_C"),
		FVector(1200.0f, 1400.0f, 3000.0f),  // 12m x 14m x 30m
		false,
		true,  // Scaling enabled
		TEXT("FGPowerPoleHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated - same dimensions as Power Tower
	);
	
	// Power Switch (Scalable)
	// Measured: X=200.000 Y=100.000 Z=450.000 (via spacing test: X+0, Y-100, Z+250)
	// Note: Same dimensions as Priority Power Switch
	RegisterProfile(
		TEXT("Build_PowerSwitch_C"),
		FVector(200.0f, 100.0f, 450.0f),  // 2m x 1m x 4.5m
		false,
		true,  // Scaling enabled
		TEXT("FGPowerSwitchHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Priority Power Switch (Scalable)
	// Measured: X=200.000 Y=100.000 Z=450.000 (via spacing test: X+0, Y-100, Z+250)
	// Note: Same dimensions as regular Power Switch
	RegisterProfile(
		TEXT("Build_PriorityPowerSwitch_C"),
		FVector(200.0f, 100.0f, 450.0f),  // 2m x 1m x 4.5m
		false,
		true,  // Scaling enabled
		TEXT("FGPowerSwitchHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Power Storage (Scalable)
	// Measured: X=600.000 Y=600.000 Z=1200.000 (via spacing test: X-200, Y-200, Z+0)
	RegisterProfile(
		TEXT("Build_PowerStorageMk1_C"),
		FVector(600.0f, 600.0f, 1200.0f),  // 6m x 6m x 12m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Wall Power Outlet Mk.1 (Scaling disabled - wall attachment)
	// Note: Dimensions are reference only
	RegisterProfile(
		TEXT("Build_PowerPoleWall_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall-mounted
		TEXT("FGPowerPoleWallHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Wall Power Outlet Mk.2 (Scaling disabled - wall attachment)
	// Note: Dimensions are reference only
	RegisterProfile(
		TEXT("Build_PowerPoleWallDouble_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall-mounted
		TEXT("FGPowerPoleWallHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Wall Power Outlet Mk.3 (Scaling disabled - wall attachment)
	// Note: Dimensions are reference only
	RegisterProfile(
		TEXT("Build_PowerPoleWallDouble_Mk2_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall-mounted
		TEXT("FGPowerPoleWallHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Wall Power Outlet Mk.4 (Scaling disabled - wall attachment)
	// Note: Dimensions are reference only
	RegisterProfile(
		TEXT("Build_PowerPoleWallDouble_Mk3_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall-mounted
		TEXT("FGPowerPoleWallHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
}
