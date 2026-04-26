// SFBuildableSizeRegistry_Production.cpp
// Production building buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterProduction()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Production.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Production (SFBuildableSizeRegistry_Production.cpp)"));
	
	// ===================================
	// PRODUCTION BUILDINGS
	// ===================================
	
	// Quantum Encoder (Vanilla late-game building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=2200.000 Y=5000.000 Z=1450.000 (via spacing test: X+600, Y+3900, Z+1000)
	RegisterProfile(
		TEXT("Build_QuantumEncoder_C"),
		FVector(2200.0f, 5000.0f, 1450.0f),  // 22m x 50m x 14.5m footprint
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Hadron Collider
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=3700.000 Y=2700.000 Z=3200.000 (via spacing test: X+2700, Y+1700, Z+2400)
	RegisterProfile(
		TEXT("Build_HadronCollider_C"),
		FVector(3700.0f, 2700.0f, 3200.0f),  // 37m x 27m x 32m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Constructor (Basic production building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=800.000 Y=1000.000 Z=800.000 (via spacing test: X+0, Y+0, Z+200)
	RegisterProfile(
		TEXT("Build_ConstructorMk1_C"),
		FVector(800.0f, 1000.0f, 800.0f),  // 8m x 10m x 8m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Assembler (Mid-tier production building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=900.000 Y=1600.000 Z=1050.000 (via spacing test: X+0, Y+0, Z+750)
	RegisterProfile(
		TEXT("Build_AssemblerMk1_C"),
		FVector(900.0f, 1600.0f, 1050.0f),  // 9m x 16m x 10.5m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Manufacturer (Large production building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=1800.000 Y=2000.000 Z=1450.000 (via spacing test: X+0, Y+800, Z+350)
	RegisterProfile(
		TEXT("Build_ManufacturerMk1_C"),
		FVector(1800.0f, 2000.0f, 1450.0f),  // 18m x 20m x 14.5m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Packager (Fluid packaging building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=800.000 Y=800.000 Z=1200.000 (via spacing test: X+0, Y+0, Z+0 - perfect fit!)
	RegisterProfile(
		TEXT("Build_Packager_C"),
		FVector(800.0f, 800.0f, 1200.0f),  // 8m x 8m x 12m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Oil Refinery (Tall fluid processing building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=1000.000 Y=2200.000 Z=3000.000 (via spacing test: X+0, Y+200, Z+1500)
	RegisterProfile(
		TEXT("Build_OilRefinery_C"),
		FVector(1000.0f, 2200.0f, 3000.0f),  // 10m x 22m x 30m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Blender (Advanced production building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=1800.000 Y=1600.000 Z=1500.000 (via spacing test: X+0, Y+0, Z+800)
	RegisterProfile(
		TEXT("Build_Blender_C"),
		FVector(1800.0f, 1600.0f, 1500.0f),  // 18m x 16m x 15m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Converter (Resource conversion building)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=1600.000 Y=1600.000 Z=1750.000 (via spacing test: X+0, Y+0, Z+1350)
	RegisterProfile(
		TEXT("Build_Converter_C"),
		FVector(1600.0f, 1600.0f, 1750.0f),  // 16m x 16m x 17.5m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Smelter (Basic ore processing)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=500.000 Y=1000.000 Z=850.000 (via spacing test: X+0, Y+0, Z+400)
	RegisterProfile(
		TEXT("Build_SmelterMk1_C"),
		FVector(500.0f, 1000.0f, 850.0f),  // 5m x 10m x 8.5m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);

	// Foundry (Advanced ore processing)
	// Inheritance: FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=1000.000 Y=1000.000 Z=850.000 (via spacing test: X+0, Y+0, Z+400)
	RegisterProfile(
		TEXT("Build_FoundryMk1_C"),
		FVector(1000.0f, 1000.0f, 850.0f),  // 10m x 10m x 8.5m
		false,
		true,
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
}
