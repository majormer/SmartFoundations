// SFBuildableSizeRegistry_Extractors.cpp
// Resource extractor buildable size registrations
// Separated from main registry for better organization
// Note: All extractors have scaling disabled due to resource node alignment requirements

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterExtractors()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Extractors.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Extractors (SFBuildableSizeRegistry_Extractors.cpp)"));
	
	// ===================================
	// RESOURCE EXTRACTORS (NO SCALING)
	// ===================================
	
	// Miner Mk.1 (Resource node extractor - scaling disabled)
	// Inheritance: FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Dimensions provided for reference only - scaling is disabled for resource extractors
	RegisterProfile(
		TEXT("Build_MinerMk1_C"),
		FVector(600.0f, 1400.0f, 400.0f),  // 6m x 14m x 4m (reference only)
		false,
		false,  // Scaling disabled - must align with resource nodes
		TEXT("FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Miner Mk.2 (Resource node extractor - scaling disabled)
	// Inheritance: FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Same footprint as Mk.1, same height
	RegisterProfile(
		TEXT("Build_MinerMk2_C"),
		FVector(600.0f, 1400.0f, 400.0f),  // 6m x 14m x 4m (reference only)
		false,
		false,  // Scaling disabled - must align with resource nodes
		TEXT("FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Miner Mk.3 (Resource node extractor - scaling disabled)
	// Inheritance: FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Same footprint as Mk.1/2, taller (6m vs 4m)
	RegisterProfile(
		TEXT("Build_MinerMk3_C"),
		FVector(600.0f, 1400.0f, 600.0f),  // 6m x 14m x 6m (reference only)
		false,
		false,  // Scaling disabled - must align with resource nodes
		TEXT("FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Oil Pump (Oil node extractor - scaling disabled)
	// Inheritance: FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	RegisterProfile(
		TEXT("Build_OilPump_C"),
		FVector(800.0f, 1300.0f, 1800.0f),  // 8m x 13m x 18m (reference only)
		false,
		false,  // Scaling disabled - must align with oil nodes
		TEXT("FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Water Extractor (Water body extractor - scaling enabled per Issue #197)
	// Inheritance: Holo_WaterPump_C -> FGWaterPumpHologram -> FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Note: Uses SFWaterPumpChildHologram for per-child water volume validation
	RegisterProfile(
		TEXT("Build_WaterPump_C"),
		FVector(2000.0f, 1800.0f, 2400.0f),  // 20m x 18m x 24m (12m model + 12m clearance for Z scaling)
		false,
		true,  // Scaling enabled (Issue #197) - each child validates water placement independently
		TEXT("Holo_WaterPump_C -> FGWaterPumpHologram -> FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Fracking Extractor (Satellite resource extractor - scaling disabled)
	// Inheritance: FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	RegisterProfile(
		TEXT("Build_FrackingExtractor_C"),
		FVector(500.0f, 500.0f, 570.0f),  // 5m x 5m x 5.7m (reference only)
		false,
		false,  // Scaling disabled - must align with fracking nodes
		TEXT("FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Fracking Smasher (Resource well extractor - scaling disabled)
	// Inheritance: FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	RegisterProfile(
		TEXT("Build_FrackingSmasher_C"),
		FVector(1500.0f, 1500.0f, 2000.0f),  // 15m x 15m x 20m (reference only)
		false,
		false,  // Scaling disabled - must align with resource wells
		TEXT("FGResourceExtractorHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
}
