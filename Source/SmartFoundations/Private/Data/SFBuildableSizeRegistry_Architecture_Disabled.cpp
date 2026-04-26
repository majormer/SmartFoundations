// SFBuildableSizeRegistry_Architecture_Disabled.cpp
// Architecture buildables that are currently disabled for scaling
// Kept here for future revisiting when special hologram behaviors can be supported
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterArchitectureDisabled()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Architecture_Disabled.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Architecture (Disabled) (SFBuildableSizeRegistry_Architecture_Disabled.cpp)"));
	
	// ===================================
	// ROOFS (DISABLED - foundation snapping required)
	// ===================================
	// NOTE: Roofs must snap to foundations/structures
	// Validation logic prevents scaling even with valid children
	
	// Roof Orange 01
	RegisterProfile(
		TEXT("Build_Roof_Orange_01_C"),
		FVector(800.0f, 800.0f, 50.0f),  // 8m x 8m x 0.5m
		false,
		false,
		TEXT("Holo_Roof_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Roof Orange 02
	RegisterProfile(
		TEXT("Build_Roof_Orange_02_C"),
		FVector(800.0f, 800.0f, 100.0f),  // 8m x 8m x 1m
		false,
		false,
		TEXT("Holo_Roof_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Roof Orange 03
	RegisterProfile(
		TEXT("Build_Roof_Orange_03_C"),
		FVector(800.0f, 800.0f, 200.0f),  // 8m x 8m x 2m
		false,
		false,
		TEXT("Holo_Roof_C -> FGRampHologram -> FGFoundationHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Roof Orange 04
	RegisterProfile(
		TEXT("Build_Roof_Orange_04_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		false,
		TEXT("Holo_Roof_C -> FGRampHologram -> FGFoundationHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Roof Orange Out Corner 01
	RegisterProfile(
		TEXT("Build_Roof_Orange_OutCorner_01_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Roof Orange Out Corner 02
	RegisterProfile(
		TEXT("Build_Roof_Orange_OutCorner_02_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Roof Orange Out Corner 03
	RegisterProfile(
		TEXT("Build_Roof_Orange_OutCorner_03_C"),
		FVector(800.0f, 800.0f, 500.0f),  // 8m x 8m x 5m
		false,
		false,
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Roof Orange In Corner 01
	RegisterProfile(
		TEXT("Build_Roof_Orange_InCorner_01_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Roof Orange In Corner 02
	RegisterProfile(
		TEXT("Build_Roof_Orange_InCorner_02_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Roof Orange In Corner 03
	RegisterProfile(
		TEXT("Build_Roof_Orange_InCorner_03_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_CornerWall_C -> FGCornerWallHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// BEAMS (DISABLED - click-and-drag buildables)
	// ===================================
	// NOTE: Beams are click-and-drag buildables like conveyors, not grid-placeable
	// Progressive diagonal offset occurs because FGBeamHologram expects start/end point placement
	
	// Beam
	RegisterProfile(
		TEXT("Build_Beam_C"),
		FVector(50.0f, 100.0f, 400.0f),  // 0.5m x 1m x 4m
		false,
		false,
		TEXT("Holo_Beam_C -> FGBeamHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Beam Painted
	RegisterProfile(
		TEXT("Build_Beam_Painted_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Beam_C -> FGBeamHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam H
	RegisterProfile(
		TEXT("Build_Beam_H_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Beam_C -> FGBeamHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam Shelf
	RegisterProfile(
		TEXT("Build_Beam_Shelf_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Beam_C -> FGBeamHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam Concrete
	RegisterProfile(
		TEXT("Build_Beam_Concrete_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Beam_C -> FGBeamHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam Cable
	RegisterProfile(
		TEXT("Build_Beam_Cable_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Beam_C -> FGBeamHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam Cable Cluster
	RegisterProfile(
		TEXT("Build_Beam_Cable_Cluster_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Beam_C -> FGBeamHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam Connector
	RegisterProfile(
		TEXT("Build_Beam_Connector_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_BeamConnector_C -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam Connector Double
	RegisterProfile(
		TEXT("Build_Beam_Connector_Double_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_BeamConnector_C -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Beam Support
	RegisterProfile(
		TEXT("Build_Beam_Support_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Beam_Support_C -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// PILLARS (DISABLED - wall snapping required)
	// ===================================
	// NOTE: Pillars have special logic for snapping to walls/surfaces
	// Needs revisit for proper snap-to-surface support
	
	// Pillar Base
	RegisterProfile(
		TEXT("Build_PillarBase_C"),
		FVector(800.0f, 800.0f, 400.0f),  // 8m x 8m x 4m
		false,
		false,
		TEXT("Holo_PillarBase_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Pillar Middle
	RegisterProfile(
		TEXT("Build_PillarMiddle_C"),
		FVector(400.0f, 400.0f, 400.0f),  // 4m x 4m x 4m
		false,
		false,
		TEXT("Holo_Pillar_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Pillar Middle Concrete
	RegisterProfile(
		TEXT("Build_PillarMiddle_Concrete_C"),
		FVector(400.0f, 400.0f, 400.0f),  // 4m x 4m x 4m
		false,
		false,
		TEXT("Holo_Pillar_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Pillar Middle Frame
	RegisterProfile(
		TEXT("Build_PillarMiddle_Frame_C"),
		FVector(400.0f, 400.0f, 400.0f),  // 4m x 4m x 4m
		false,
		false,
		TEXT("Holo_Pillar_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Pillar Base Small
	RegisterProfile(
		TEXT("Build_PillarBase_Small_C"),
		FVector(400.0f, 400.0f, 200.0f),  // 4m x 4m x 2m
		false,
		false,
		TEXT("Holo_PillarBase_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Pillar Small Metal
	RegisterProfile(
		TEXT("Build_Pillar_Small_Metal_C"),
		FVector(200.0f, 200.0f, 400.0f),  // 2m x 2m x 4m
		false,
		false,
		TEXT("Holo_Pillar_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Pillar Small Concrete
	RegisterProfile(
		TEXT("Build_Pillar_Small_Concrete_C"),
		FVector(200.0f, 200.0f, 400.0f),  // 2m x 2m x 4m
		false,
		false,
		TEXT("Holo_Pillar_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// Pillar Small Frame
	RegisterProfile(
		TEXT("Build_Pillar_Small_Frame_C"),
		FVector(200.0f, 200.0f, 400.0f),  // 2m x 2m x 4m
		false,
		false,
		TEXT("Holo_Pillar_C -> FGPillarHologram -> FGBuildableHologram -> FGHologram"),
		true
	);
	
	// ===================================
	// LADDERS (DISABLED - click-and-drag buildables)
	// ===================================
	// NOTE: Ladders are click-and-drag buildables like beams, not grid-placeable
	
	// Ladder
	RegisterProfile(
		TEXT("Build_Ladder_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_Ladder_C -> FGLadderHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// LARGE VENTS (DISABLED - not validated)
	// ===================================
	
	// Large Fan
	RegisterProfile(
		TEXT("Build_LargeFan_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_LargeVent_C -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Large Vent
	RegisterProfile(
		TEXT("Build_LargeVent_C"),
		FVector(2000.0f, 2000.0f, 2000.0f),  // 20m x 20m x 20m
		false,
		false,
		TEXT("Holo_LargeVent_C -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
}
