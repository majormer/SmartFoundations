#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterFoundations()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Foundations.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Foundations (SFBuildableSizeRegistry_Foundations.cpp)"));
	
	// ===================================
	// FOUNDATIONS
	// ===================================
	
	// Foundation 8x4 (Standard)
	// Inheritance: Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=800.000 Y=800.000 Z=400.000
	RegisterProfile(
		TEXT("Build_Foundation_8x4_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,  // Symmetric - no rotation swap needed
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Manually validated
	);
	
	// Foundation 8x2 (2m height)
	// Inheritance: Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=800.000 Y=800.000 Z=200.000
	RegisterProfile(
		TEXT("Build_Foundation_8x2_01_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,  // Symmetric - no rotation swap needed
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Manually validated
	);
	
	// Foundation 8x1 (1m height)
	// Inheritance: Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram -> Actor -> UObject
	// Measured: X=800.000 Y=800.000 Z=100.000
	RegisterProfile(
		TEXT("Build_Foundation_8x1_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,  // Symmetric - no rotation swap needed
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Manually validated
	);
	
	// ===================================
	// FOUNDATION STYLE VARIANTS
	// ===================================
	// Style variants (Asphalt, Concrete, ConcretePolished, Metal, etc.) automatically
	// inherit dimensions from base foundations via variant inheritance fallback.
	// No explicit registration needed - registry will pattern-match and inherit from
	// Build_Foundation_8x4_01_C, Build_Foundation_8x2_01_C, Build_Foundation_8x1_01_C
	
	// ===================================
	// SPECIAL FOUNDATION TYPES
	// ===================================
	
	// Foundation Glass 01 (Architecture set - transparent foundation)
	RegisterProfile(
		TEXT("Build_FoundationGlass_01_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Flat Frame 01 (Architecture set - thin frame foundation/floor)
	// Spacing test: Z-350 (very thin frame)
	RegisterProfile(
		TEXT("Build_Flat_Frame_01_C"),
		FVector(800.0f, 800.0f, 50.0f),  // 8m x 8m x 0.5m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);

	// Frame Foundation (Architecture set - thick frame)
	RegisterProfile(
		TEXT("Build_Foundation_Frame_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// QUARTER PIPES
	// ===================================
	
	// Quarter Pipe Middle (Ficsit 8x1)
	// Spacing test: Y+400 (base 400 + 400 spacing = 800 total Y)
	// Asymmetric piece - X=4m, Y=8m, Z=1m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddle_Ficsit_8x1_C"),
		FVector(400.0f, 800.0f, 100.0f),  // 4m x 8m x 1m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle (Ficsit 8x2)
	// Spacing test: Y+400, Z+100 (same XY footprint as 8x1, double height)
	// Asymmetric piece - X=4m, Y=8m, Z=2m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddle_Ficsit_8x2_C"),
		FVector(400.0f, 800.0f, 200.0f),  // 4m x 8m x 2m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle (Ficsit 8x4)
	// Spacing test: Y+400, Z+300 (same XY footprint as 8x1/8x2, quad height)
	// Asymmetric piece - X=4m, Y=8m, Z=4m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddle_Ficsit_8x4_C"),
		FVector(400.0f, 800.0f, 400.0f),  // 4m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe (Standard)
	// Spacing test: X+350, Y+400, Z+300
	// Asymmetric corner piece - 7.5m x 8m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipe_C"),
		FVector(750.0f, 800.0f, 400.0f),  // 7.5m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_FoundationSideZoop_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe 02 (Variant)
	// Spacing test: X+400, Y+400, Z+300
	// Square footprint corner piece - 8m x 8m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipe_02_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_FoundationSideZoop_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Corner 01
	// Spacing test: X+400, Y+400, Z+300
	// Square footprint corner piece - 8m x 8m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipeCorner_01_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Corner 02
	// Spacing test: X+400, Y+400, Z+300
	// Square footprint corner piece - 8m x 8m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipeCorner_02_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Corner 03
	// Spacing test: X+400, Y+400, Z+300
	// Square footprint corner piece - 8m x 8m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipeCorner_03_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Corner 04
	// Spacing test: X+400, Y+400, Z+300
	// Square footprint corner piece - 8m x 8m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipeCorner_04_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle In Corner (Ficsit 8x1)
	// Spacing test: X+400, Y+400, Z+0
	// Square footprint inner corner - 8m x 8m x 1m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddleInCorner_Ficsit_8x1_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle In Corner (Ficsit 8x2)
	// Square footprint inner corner - 8m x 8m x 2m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddleInCorner_Ficsit_8x2_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle In Corner (Ficsit 8x4)
	// Square footprint inner corner - 8m x 8m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddleInCorner_Ficsit_8x4_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle Out Corner (Ficsit 4x1)
	// No spacing adjustment needed - outer corner piece
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddleOutCorner_Ficsit_4x1_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle Out Corner (Ficsit 4x2)
	// Outer corner piece - 4m x 4m x 2m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddleOutCorner_Ficsit_4x2_C"),
		FVector(400.0f, 400.0f, 200.0f),  // 4m x 4m x 2m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Quarter Pipe Middle Out Corner (Ficsit 4x4)
	// Outer corner piece - 4m x 4m x 4m
	RegisterProfile(
		TEXT("Build_QuarterPipeMiddleOutCorner_Ficsit_4x4_C"),
		FVector(400.0f, 400.0f, 400.0f),  // 4m x 4m x 4m
		false,  // No rotation swap (directional piece)
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// OTHER QUARTER PIPE VARIANTS
	// ===================================
	// Base types for non-Middle quarter pipes
	// These appear to be 8x4 only (no other sizes found)
	
	// Quarter Pipe (standard, non-Middle)
	RegisterProfile(
		TEXT("Build_QuarterPipe_Ficsit_8x4_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Down Quarter Pipe
	RegisterProfile(
		TEXT("Build_DownQuarterPipe_Ficsit_8x4_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Quarter Pipe In Corner
	RegisterProfile(
		TEXT("Build_QuarterPipeInCorner_Ficsit_8x4_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Down Quarter Pipe In Corner
	RegisterProfile(
		TEXT("Build_DownQuarterPipeInCorner_Ficsit_8x4_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Quarter Pipe Out Corner
	RegisterProfile(
		TEXT("Build_QuarterPipeOutCorner_Ficsit_8x4_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
	
	// Down Quarter Pipe Out Corner
	RegisterProfile(
		TEXT("Build_DownQuarterPipeOutCorner_Ficsit_8x4_C"),
		FVector(400.0f, 400.0f, 100.0f),  // 4m x 4m x 1m
		false,
		true,
		TEXT("Holo_Foundation_C -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated
	);
}
