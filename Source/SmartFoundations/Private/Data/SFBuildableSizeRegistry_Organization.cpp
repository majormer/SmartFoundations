// SFBuildableSizeRegistry_Organization.cpp
// Organization buildables: lighting, signs, control panels, and exploration structures
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterOrganization()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Organization.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Organization (SFBuildableSizeRegistry_Organization.cpp)"));
	
	// ===================================
	// LIGHTING
	// ===================================
	
	// Street Light (Scalable)
	// Measured: X=50.000 Y=800.000 Z=1100.000 (via spacing test: X-3000, Y+0, Z+0)
	// Note: Very narrow pole - cached size was incorrect
	RegisterProfile(
		TEXT("Build_StreetLight_C"),
		FVector(50.0f, 800.0f, 1100.0f),  // 0.5m x 8m x 11m (narrow pole)
		false,
		true,  // Scaling enabled
		TEXT("Holo_StreetLight_C -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Ceiling Light (Scalable)
	// Measured: X=1100.000 Y=1200.000 Z=100.000 (via spacing test: X+900, Y+1000, Z+0)
	RegisterProfile(
		TEXT("Build_CeilingLight_C"),
		FVector(1100.0f, 1200.0f, 100.0f),  // 11m x 12m x 1m (ceiling-mounted panel)
		false,
		true,  // Scaling enabled
		TEXT("Holo_CeilingLight_C -> FGCeilingLightHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Floodlight Pole (Scalable)
	// Measured: X=800.000 Y=800.000 Z=3700.000 (via spacing test: X+700, Y+700, Z+3600)
	RegisterProfile(
		TEXT("Build_FloodlightPole_C"),
		FVector(800.0f, 800.0f, 3700.0f),  // 8m x 8m x 37m (tall pole)
		false,
		true,  // Scaling enabled
		TEXT("Holo_FloodLightPole_C -> FGFloodlightHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Floodlight Wall (Scalable)
	// Measured: X=500.000 Y=750.000 Z=200.000 (via spacing test: X+300, Y+550, Z+0)
	RegisterProfile(
		TEXT("Build_FloodlightWall_C"),
		FVector(500.0f, 750.0f, 200.0f),  // 5m x 7.5m x 2m (wall-mounted floodlight)
		false,
		true,  // Scaling enabled
		TEXT("Holo_FloodLightWall_C -> FGFloodlightHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// CONTROL PANELS
	// ===================================
	
	// Lights Control Panel (Scalable)
	// Measured: X=200.000 Y=100.000 Z=300.000 (via spacing test: X+100, Y+0, Z+200)
	// Note: Uses Holo_BuildingGradualBase - same as MAM, uses registry-based adapter
	RegisterProfile(
		TEXT("Build_LightsControlPanel_C"),
		FVector(200.0f, 100.0f, 300.0f),  // 2m x 1m x 3m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_BuildingGradualBase_NoClearanceMesh_C -> Holo_BuildingGradualBase_C -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// SIGNS
	// ===================================
	
	// Label Sign (2m) — FGStandaloneSignHologram
	// In-game: 2m x 0.5m face. Estimate includes pole height.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Small_C"),
		FVector(200.0f, 20.0f, 50.0f),  // Spacing test: Z reduced by 150cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("Holo_StandaloneSign_Small_C -> FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Label Sign (3m) — FGStandaloneSignHologram
	// In-game: 3m x 0.5m face. Estimate includes pole height.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_SmallWide_C"),
		FVector(300.0f, 20.0f, 50.0f),  // Spacing test: Z reduced by 150cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Label Sign (4m) — FGStandaloneSignHologram
	// In-game: 4m x 0.5m face. Estimate includes pole height.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_SmallVeryWide_C"),
		FVector(400.0f, 20.0f, 50.0f),  // Spacing test: Z reduced by 150cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Square Sign (0.5m) — FGStandaloneSignHologram
	// In-game: 0.5m x 0.5m face.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Square_Tiny_C"),
		FVector(50.0f, 20.0f, 50.0f),  // Spacing test: Z reduced by 150cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Square Sign (1m) — FGStandaloneSignHologram
	// In-game: 1m x 1m face.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Square_Small_C"),
		FVector(100.0f, 20.0f, 100.0f),  // Spacing test: Z reduced by 100cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Square Sign (2m) / Display Sign — FGStandaloneSignHologram
	// In-game: 2m x 2m face.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Square_C"),
		FVector(200.0f, 20.0f, 200.0f),  // Spacing test: Z reduced by 100cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Display Sign (2m x 1m) — FGStandaloneSignHologram
	// In-game: 2m x 1m face.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Medium_C"),
		FVector(200.0f, 20.0f, 100.0f),  // Spacing test: Z reduced by 150cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("Holo_StandaloneSign_Medium_C -> FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Portrait Sign (2m x 3m) — FGStandaloneSignHologram
	// In-game: 2m x 3m face.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Portrait_C"),
		FVector(200.0f, 20.0f, 300.0f),  // Spacing test: Z reduced by 100cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("Holo_StandaloneSign_Portrait_C -> FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Small Billboard (8m x 4m) — FGStandaloneSignHologram
	// In-game: 8m x 4m face.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Large_C"),
		FVector(800.0f, 20.0f, 400.0f),  // Spacing test: Z reduced by 200cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("Holo_StandaloneSign_Large_C -> FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Large Billboard (16m x 8m) — FGStandaloneSignHologram
	// In-game: 16m x 8m face.
	RegisterProfile(
		TEXT("Build_StandaloneWidgetSign_Huge_C"),
		FVector(1600.0f, 20.0f, 800.0f),  // Spacing test: Z reduced by 200cm
		false,
		true,  // Scaling enabled (Issue #192)
		TEXT("Holo_StandaloneSign_Huge_C -> FGStandaloneSignHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// EXPLORATION
	// ===================================
	
	// Radar Tower (Scalable)
	// Measured: X=1000.000 Y=1000.000 Z=11400.000 (via spacing test: X+200, Y+200, Z+11000)
	RegisterProfile(
		TEXT("Build_RadarTower_C"),
		FVector(1000.0f, 1000.0f, 11400.0f),  // 10m x 10m x 114m (very tall tower!)
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Lookout Tower (Scalable)
	// Measured: X=800.000 Y=800.000 Z=2400.000 (via spacing test: X+700, Y+700, Z+2300)
	RegisterProfile(
		TEXT("Build_LookoutTower_C"),
		FVector(800.0f, 800.0f, 2400.0f),  // 8m x 8m x 24m (tall tower)
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
}
