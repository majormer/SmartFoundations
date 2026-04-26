// SFBuildableSizeRegistry_Walkways.cpp
// Walkway and catwalk buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterWalkways()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Walkways.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Walkways & Catwalks (SFBuildableSizeRegistry_Walkways.cpp)"));
	
	// ===================================
	// CATWALKS (with railings)
	// ===================================
	
	// Catwalk Straight
	RegisterProfile(
		TEXT("Build_CatwalkStraight_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_WalkwayStraight_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Catwalk Corner
	RegisterProfile(
		TEXT("Build_CatwalkCorner_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Catwalk T
	RegisterProfile(
		TEXT("Build_CatwalkT_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_WalkwayT_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Catwalk Cross
	RegisterProfile(
		TEXT("Build_CatwalkCross_C"),
		FVector(400.0f, 400.0f, 50.0f),  // 4m x 4m x 0.5m
		false,
		true,
		TEXT("Holo_WalkwayCross_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Catwalk Ramp
	RegisterProfile(
		TEXT("Build_CatwalkRamp_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_WalkwayStraight_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Catwalk Stairs
	RegisterProfile(
		TEXT("Build_CatwalkStairs_C"),
		FVector(400.0f, 400.0f, 400.0f),  // 4m x 4m x 4m
		false,
		true,
		TEXT("Holo_WalkwayStraight_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// WALKWAYS (without railings)
	// ===================================
	
	// Walkway Straight
	RegisterProfile(
		TEXT("Build_WalkwayStraight_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_WalkwayStraight_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Walkway Turn
	RegisterProfile(
		TEXT("Build_WalkwayTrun_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Walkway T
	RegisterProfile(
		TEXT("Build_WalkwayT_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_WalkwayT_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Walkway Cross
	RegisterProfile(
		TEXT("Build_WalkwayCross_C"),
		FVector(400.0f, 400.0f, 50.0f),  // 4m x 4m x 0.5m
		false,
		true,
		TEXT("Holo_WalkwayCross_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Walkway Ramp
	RegisterProfile(
		TEXT("Build_WalkwayRamp_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_WalkwayStraight_C -> FGWalkwayHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
}
