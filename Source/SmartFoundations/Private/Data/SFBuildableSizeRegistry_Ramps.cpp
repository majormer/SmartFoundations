// SFBuildableSizeRegistry_Ramps.cpp
// Ramp-related buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterRamps()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Ramps.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Ramps (SFBuildableSizeRegistry_Ramps.cpp)"));
	
	// ===================================
	// STANDARD RAMPS
	// ===================================
	
	// Ramp 8x1 (Standard)
	RegisterProfile(
		TEXT("Build_Ramp_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Ramp 8x2 (Standard)
	RegisterProfile(
		TEXT("Build_Ramp_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Ramp 8x4 (Standard)
	RegisterProfile(
		TEXT("Build_Ramp_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// RAMP CORNER VARIANTS (UP)
	// ===================================
	
	RegisterProfile(
		TEXT("Build_Ramp_UpCorner_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_CornerRamp_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_Ramp_UpCorner_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_Ramp_UpCorner_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// RAMP CORNER VARIANTS (DOWN)
	// ===================================
	
	RegisterProfile(
		TEXT("Build_Ramp_DownCorner_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_CornerRamp_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_Ramp_DownCorner_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_Ramp_DownCorner_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// INVERTED RAMPS
	// ===================================
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// INVERTED RAMP CORNERS (U-CORNER)
	// ===================================
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_UCorner_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_UCorner_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_UCorner_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// INVERTED RAMP CORNERS (D-CORNER)
	// ===================================
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_DCorner_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_DCorner_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	RegisterProfile(
		TEXT("Build_InvertedRamp_DCorner_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// DIAGONAL RAMPS
	// ===================================
	
	// Diagonal Ramp 8x1 (Variant 01)
	RegisterProfile(
		TEXT("Build_Ramp_Diagonal_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_CornerRamp_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Diagonal Ramp 8x2 (Variant 01)
	RegisterProfile(
		TEXT("Build_Ramp_Diagonal_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Diagonal Ramp 8x4 (Variant 01)
	RegisterProfile(
		TEXT("Build_Ramp_Diagonal_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Diagonal Ramp 8x1 (Variant 02)
	RegisterProfile(
		TEXT("Build_Ramp_Diagonal_8x1_02_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_CornerRamp_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Diagonal Ramp 8x2 (Variant 02)
	RegisterProfile(
		TEXT("Build_Ramp_Diagonal_8x2_02_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Diagonal Ramp 8x4 (Variant 02)
	RegisterProfile(
		TEXT("Build_Ramp_Diagonal_8x4_02_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// DOUBLE RAMPS
	// ===================================
	
	// Double Ramp (basic/default)
	RegisterProfile(
		TEXT("Build_RampDouble_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m (double height)
		false,
		true,
		TEXT("Holo_RampDouble_C -> Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Double Ramp 8x1
	RegisterProfile(
		TEXT("Build_RampDouble_8x1_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m (double height)
		false,
		true,
		TEXT("Holo_RampDouble_C -> Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Double Ramp 8x2
	RegisterProfile(
		TEXT("Build_RampDouble_8x2_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m (double height)
		false,
		true,
		TEXT("Holo_RampDouble_C -> Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Double Ramp 8x4
	RegisterProfile(
		TEXT("Build_RampDouble_8x4_C"),
		FVector(800.0f, 800.0f, 800.0f),  // 8m x 8m x 8m (double height)
		false,
		true,
		TEXT("Holo_RampDouble_C -> Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Double Ramp 8x8x8 (alternative naming for 8m tall double ramp)
	RegisterProfile(
		TEXT("Build_Ramp_8x8x8_C"),
		FVector(800.0f, 800.0f, 800.0f),  // 8m x 8m x 8m (double height)
		false,
		true,
		TEXT("Holo_RampDouble_C -> Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// STAIRS (FICSIT SET)
	// ===================================
	// Note: Stairs use _FicsitSet_ pattern, variants use _Asphalt_, etc.
	// Special inheritance pattern required in variant matching
	
	// Stairs 8x1 (Ficsit)
	RegisterProfile(
		TEXT("Build_Stair_FicsitSet_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Stairs 8x2 (Ficsit)
	RegisterProfile(
		TEXT("Build_Stair_FicsitSet_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Stairs 8x4 (Ficsit)
	RegisterProfile(
		TEXT("Build_Stair_FicsitSet_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// ===================================
	// RAMP FRAMES
	// ===================================
	
	// Ramp Frame 01
	RegisterProfile(
		TEXT("Build_Ramp_Frame_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Ramp Frame Inverted 01
	RegisterProfile(
		TEXT("Build_Ramp_Frame_Inverted_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("Holo_Ramp_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
}
