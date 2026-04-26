// SFBuildableSizeRegistry_Storage.cpp
// Storage container and fluid tank buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterStorage()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Storage.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Storage (SFBuildableSizeRegistry_Storage.cpp)"));
	
	// ===================================
	// STORAGE CONTAINERS
	// ===================================
	
	// Storage Container Mk.1 (Scalable)
	// Measured: X=500.000 Y=1100.000 Z=400.000 (via spacing test: X+0, Y+0, Z+0)
	RegisterProfile(
		TEXT("Build_StorageContainerMk1_C"),
		FVector(500.0f, 1100.0f, 400.0f),  // 5m x 11m x 4m
		false,
		true,  // Scaling enabled
		TEXT("FGStackableStorageHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Storage Container Mk.2 (Scalable)
	// Measured: X=500.000 Y=1100.000 Z=800.000 (via spacing test: X+0, Y+0, Z+0)
	RegisterProfile(
		TEXT("Build_StorageContainerMk2_C"),
		FVector(500.0f, 1100.0f, 800.0f),  // 5m x 11m x 8m
		false,
		true,  // Scaling enabled
		TEXT("FGStackableStorageHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Central Storage (Scalable)
	// Measured: X=500.000 Y=1300.000 Z=400.000 (via spacing test: X+0, Y+200, Z+0)
	RegisterProfile(
		TEXT("Build_CentralStorage_C"),
		FVector(500.0f, 1300.0f, 400.0f),  // 5m x 13m x 4m
		false,
		true,  // Scaling enabled
		TEXT("FGStackableStorageHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Personal Storage Box (Scalable)
	// TODO: Validate dimensions after FGStorageBoxHologram scaling support is enabled
	RegisterProfile(
		TEXT("Build_StoragePlayer_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Estimated - needs validation
		false,
		true,  // Scaling enabled - needs FGStorageBoxHologram adapter support
		TEXT("Holo_StorageBox_C -> FGStorageBoxHologram -> FGBuildableHologram -> FGHologram"),
		false  // Not validated - scaling not yet working for this hologram type
	);
	
	// First Aid Storage Box (Scalable)
	// TODO: Validate dimensions after FGStorageBoxHologram scaling support is enabled
	RegisterProfile(
		TEXT("Build_StorageMedkit_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Estimated - needs validation
		false,
		true,  // Scaling enabled - needs FGStorageBoxHologram adapter support
		TEXT("FGStorageBoxHologram -> FGBuildableHologram -> FGHologram"),
		false  // Not validated - scaling not yet working for this hologram type
	);
	
	// Hazmat Storage Box (Scalable)
	// TODO: Validate dimensions after FGStorageBoxHologram scaling support is enabled
	RegisterProfile(
		TEXT("Build_StorageHazard_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Estimated - needs validation
		false,
		true,  // Scaling enabled - needs FGStorageBoxHologram adapter support
		TEXT("FGStorageBoxHologram -> FGBuildableHologram -> FGHologram"),
		false  // Not validated - scaling not yet working for this hologram type
	);
	
	// Stackable Shelf (Scalable)
	// Measured: X=400.000 Y=200.000 Z=200.000 (via spacing test: X+300, Y+100, Z+100)
	RegisterProfile(
		TEXT("Build_StackableShelf_C"),
		FVector(400.0f, 200.0f, 200.0f),  // 4m x 2m x 2m
		false,
		true,  // Scaling enabled
		TEXT("Holo_StackableShelf_C -> FGStackableShelfHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// FLUID STORAGE
	// ===================================
	
	// Fluid Buffer (Scalable)
	// Measured: X=600.000 Y=600.000 Z=800.000 (via spacing test: X+0, Y+0, Z+0)
	RegisterProfile(
		TEXT("Build_PipeStorageTank_C"),
		FVector(600.0f, 600.0f, 800.0f),  // 6m x 6m x 8m
		false,
		true,  // Scaling enabled
		TEXT("FGPipeReservoirHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Industrial Fluid Buffer (Scalable)
	// Measured: X=1400.000 Y=1400.000 Z=1200.000 (via spacing test: X+0, Y+0, Z+0)
	RegisterProfile(
		TEXT("Build_IndustrialTank_C"),
		FVector(1400.0f, 1400.0f, 1200.0f),  // 14m x 14m x 12m
		false,
		true,  // Scaling enabled
		TEXT("FGPipeReservoirHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
}
