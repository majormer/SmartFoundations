// SFBuildableSizeRegistry_Barriers.cpp
// Railing, fence, and barrier buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterBarriers()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Barriers.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Railings, Fences & Barriers (SFBuildableSizeRegistry_Barriers.cpp)"));
	
	// ===================================
	// RAILINGS
	// ===================================
	
	// Railing 01
	RegisterProfile(
		TEXT("Build_Railing_01_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Railing_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// FENCES
	// ===================================
	
	// Fence 01
	RegisterProfile(
		TEXT("Build_Fence_01_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Railing_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Chain Link Fence
	RegisterProfile(
		TEXT("Build_ChainLinkFence_C"),
		FVector(50.0f, 400.0f, 200.0f),  // 0.5m x 4m x 2m
		false,
		true,
		TEXT("Holo_Barrier_C -> FGFenceHologram -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Tarp Fence
	RegisterProfile(
		TEXT("Build_TarpFence_C"),
		FVector(50.0f, 400.0f, 200.0f),  // 0.5m x 4m x 2m
		false,
		true,
		TEXT("Holo_Barrier_C -> FGFenceHologram -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// BARRIERS
	// ===================================
	
	// Barrier Corner
	RegisterProfile(
		TEXT("Build_Barrier_Corner_C"),
		FVector(100.0f, 100.0f, 100.0f),  // 1m x 1m x 1m
		false,
		true,
		TEXT("Holo_Barrier_Corner_C -> FGBarrierCornerHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Concrete Barrier 01
	RegisterProfile(
		TEXT("Build_Concrete_Barrier_01_C"),
		FVector(50.0f, 400.0f, 100.0f),  // 0.5m x 4m x 1m
		false,
		true,
		TEXT("Holo_Barrier_C -> FGFenceHologram -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Barrier Low 01
	RegisterProfile(
		TEXT("Build_Barrier_Low_01_C"),
		FVector(50.0f, 400.0f, 100.0f),  // 0.5m x 4m x 1m
		false,
		true,
		TEXT("Holo_Barrier_C -> FGFenceHologram -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Barrier Tall 01
	RegisterProfile(
		TEXT("Build_Barrier_Tall_01_C"),
		FVector(50.0f, 400.0f, 200.0f),  // 0.5m x 4m x 2m
		false,
		true,
		TEXT("Holo_Barrier_C -> FGFenceHologram -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
}
