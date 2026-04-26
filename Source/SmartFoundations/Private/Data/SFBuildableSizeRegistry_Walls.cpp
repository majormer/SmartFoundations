// SFBuildableSizeRegistry_Walls.cpp
// Wall-related buildable size registrations
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterWalls()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Walls.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Walls (SFBuildableSizeRegistry_Walls.cpp)"));
	
	// ===================================
	// STANDARD WALLS (Base types for style variants)
	// ===================================
	
	// Wall 8x1 (1m height - base for variants)
	RegisterProfile(
		TEXT("Build_Wall_8x1_01_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall 8x2 (2m height - base for variants)
	RegisterProfile(
		TEXT("Build_Wall_8x2_01_C"),
		FVector(50.0f, 800.0f, 200.0f),  // 0.5m x 8m x 2m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall 8x4 (4m height - base for variants)
	RegisterProfile(
		TEXT("Build_Wall_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall 8x4 02 (Alternate base for variants)
	RegisterProfile(
		TEXT("Build_Wall_8x4_02_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall 8x8 (8m height - base for variants)
	RegisterProfile(
		TEXT("Build_Wall_8x8_01_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// ORANGE WALLS (Architecture set)
	// ===================================
	
	// Wall Orange 8x1
	RegisterProfile(
		TEXT("Build_Wall_Orange_8x1_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange Tris 8x1
	RegisterProfile(
		TEXT("Build_Wall_Orange_Tris_8x1_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange Tris 8x2
	RegisterProfile(
		TEXT("Build_Wall_Orange_Tris_8x2_C"),
		FVector(50.0f, 800.0f, 200.0f),  // 0.5m x 8m x 2m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange Tris 8x4
	RegisterProfile(
		TEXT("Build_Wall_Orange_Tris_8x4_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange Tris 8x8
	RegisterProfile(
		TEXT("Build_Wall_Orange_Tris_8x8_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange FlipTris 8x1
	RegisterProfile(
		TEXT("Build_Wall_Orange_FlipTris_8x1_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange FlipTris 8x2
	RegisterProfile(
		TEXT("Build_Wall_Orange_FlipTris_8x2_C"),
		FVector(50.0f, 800.0f, 200.0f),  // 0.5m x 8m x 2m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange FlipTris 8x4
	RegisterProfile(
		TEXT("Build_Wall_Orange_FlipTris_8x4_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange FlipTris 8x8
	RegisterProfile(
		TEXT("Build_Wall_Orange_FlipTris_8x8_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange Angular 8x4
	RegisterProfile(
		TEXT("Build_Wall_Orange_Angular_8x4_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Orange Angular 8x8
	RegisterProfile(
		TEXT("Build_Wall_Orange_Angular_8x8_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// STANDARD ANGULAR WALLS (Base types for style variants)
	// ===================================
	
	// Wall Angular 8x4 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_Angular_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Angular 8x8 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_Angular_8x8_01_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// STANDARD TRIS/FLIPTRIS WALLS (Base types for style variants)
	// ===================================
	
	// Wall Tris 8x1 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_Tris_8x1_01_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Tris 8x2 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_Tris_8x2_01_C"),
		FVector(50.0f, 800.0f, 200.0f),  // 0.5m x 8m x 2m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Tris 8x4 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_Tris_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Tris 8x8 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_Tris_8x8_01_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall FlipTris 8x1 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_FlipTris_8x1_01_C"),
		FVector(50.0f, 800.0f, 100.0f),  // 0.5m x 8m x 1m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall FlipTris 8x2 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_FlipTris_8x2_01_C"),
		FVector(50.0f, 800.0f, 200.0f),  // 0.5m x 8m x 2m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall FlipTris 8x4 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_FlipTris_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall FlipTris 8x8 (Base for Concrete/Steel variants)
	RegisterProfile(
		TEXT("Build_Wall_FlipTris_8x8_01_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// CONVEYOR WALLS (Base types)
	// ===================================
	
	// Wall Conveyor 8x4 01
	RegisterProfile(
		TEXT("Build_Wall_Conveyor_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Conveyor 8x4 02
	RegisterProfile(
		TEXT("Build_Wall_Conveyor_8x4_02_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Conveyor 8x4 03
	RegisterProfile(
		TEXT("Build_Wall_Conveyor_8x4_03_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// WINDOW WALLS (Base types)
	// ===================================
	
	// Wall Window 8x4 01
	RegisterProfile(
		TEXT("Build_Wall_Window_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window 8x4 02
	RegisterProfile(
		TEXT("Build_Wall_Window_8x4_02_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window 8x4 03
	RegisterProfile(
		TEXT("Build_Wall_Window_8x4_03_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window 8x4 04
	RegisterProfile(
		TEXT("Build_Wall_Window_8x4_04_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window 8x4 05
	RegisterProfile(
		TEXT("Build_Wall_Window_8x4_05_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window 8x4 06
	RegisterProfile(
		TEXT("Build_Wall_Window_8x4_06_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window 8x4 07
	RegisterProfile(
		TEXT("Build_Wall_Window_8x4_07_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window Thin 8x4 01
	RegisterProfile(
		TEXT("Build_Wall_Window_Thin_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Window Thin 8x4 02
	RegisterProfile(
		TEXT("Build_Wall_Window_Thin_8x4_02_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// DOOR WALLS (Base types)
	// ===================================
	
	// Wall Door 8x4 01
	RegisterProfile(
		TEXT("Build_Wall_Door_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Wall Door 8x4 03
	RegisterProfile(
		TEXT("Build_Wall_Door_8x4_03_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// GATE WALLS (Base types)
	// ===================================
	
	// Wall Gate 8x4 01
	RegisterProfile(
		TEXT("Build_Wall_Gate_8x4_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Gate Automated 8x4
	RegisterProfile(
		TEXT("Build_Gate_Automated_8x4_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Big Garage Door 16x8
	RegisterProfile(
		TEXT("Build_BigGarageDoor_16x8_C"),
		FVector(50.0f, 1600.0f, 800.0f),  // 0.5m x 16m x 8m
		false,
		true,
		TEXT("FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// FRAME WALLS
	// ===================================
	
	// Wall Frame 01 (Issue #247: Fixed Y dimension from 750 to 800)
	RegisterProfile(
		TEXT("Build_Wall_Frame_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		true,
		TEXT("Holo_Wall_C -> FGWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// CORNER WALLS (DISABLED - foundation snapping)
	// ===================================
	
	// Wall Orange 8x4 Corner 01
	RegisterProfile(
		TEXT("Build_Wall_Orange_8x4_Corner_01_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		false,  // Scaling DISABLED
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Wall Orange 8x8 Corner 01
	RegisterProfile(
		TEXT("Build_Wall_Orange_8x8_Corner_01_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		false,  // Scaling DISABLED
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Wall Orange 8x4 Corner 02
	RegisterProfile(
		TEXT("Build_Wall_Orange_8x4_Corner_02_C"),
		FVector(50.0f, 800.0f, 400.0f),  // 0.5m x 8m x 4m
		false,
		false,  // Scaling DISABLED
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Wall Orange 8x8 Corner 02
	RegisterProfile(
		TEXT("Build_Wall_Orange_8x8_Corner_02_C"),
		FVector(50.0f, 800.0f, 800.0f),  // 0.5m x 8m x 8m
		false,
		false,  // Scaling DISABLED
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
}
