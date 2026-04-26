// SFBuildableSizeRegistry_Logistics.cpp
// Logistics buildable size registrations (conveyors, pipelines, attachments)
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterLogistics()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Logistics.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Logistics (SFBuildableSizeRegistry_Logistics.cpp)"));
	
	// ===================================
	// CONVEYOR BELTS (NO SCALING)
	// ===================================
	
	// Conveyor Belt Mk.1 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_ConveyorBeltMk1_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_ConveyorBelt_C -> FGConveyorBeltHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Belt Mk.2 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_ConveyorBeltMk2_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_ConveyorBelt_C -> FGConveyorBeltHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Belt Mk.3 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_ConveyorBeltMk3_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_ConveyorBelt_C -> FGConveyorBeltHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Belt Mk.4 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_ConveyorBeltMk4_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_ConveyorBelt_C -> FGConveyorBeltHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Belt Mk.5 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_ConveyorBeltMk5_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_ConveyorBelt_C -> FGConveyorBeltHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Belt Mk.6 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_ConveyorBeltMk6_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_ConveyorBelt_C -> FGConveyorBeltHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// CONVEYOR LIFTS (NO SCALING)
	// ===================================
	
	// Conveyor Lift Mk.1 (Scaling disabled - vertical transport)
	RegisterProfile(
		TEXT("Build_ConveyorLiftMk1_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vertical logistics
		TEXT("Holo_ConveyorLift_C -> FGConveyorLiftHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Lift Mk.2 (Scaling disabled - vertical transport)
	RegisterProfile(
		TEXT("Build_ConveyorLiftMk2_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vertical logistics
		TEXT("Holo_ConveyorLift_C -> FGConveyorLiftHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Lift Mk.3 (Scaling disabled - vertical transport)
	RegisterProfile(
		TEXT("Build_ConveyorLiftMk3_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vertical logistics
		TEXT("Holo_ConveyorLift_C -> FGConveyorLiftHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Lift Mk.4 (Scaling disabled - vertical transport)
	RegisterProfile(
		TEXT("Build_ConveyorLiftMk4_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vertical logistics
		TEXT("Holo_ConveyorLift_C -> FGConveyorLiftHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Lift Mk.5 (Scaling disabled - vertical transport)
	RegisterProfile(
		TEXT("Build_ConveyorLiftMk5_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vertical logistics
		TEXT("Holo_ConveyorLift_C -> FGConveyorLiftHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Conveyor Lift Mk.6 (Scaling disabled - vertical transport)
	RegisterProfile(
		TEXT("Build_ConveyorLiftMk6_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vertical logistics
		TEXT("Holo_ConveyorLift_C -> FGConveyorLiftHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// CONVEYOR POLES (SCALABLE)
	// ===================================
	
	// Conveyor Pole (Scalable support structure)
	// Measured: X=100.000 Y=150.000 Z=100.000 (via spacing test: X-700, Y-650, Z-300)
	RegisterProfile(
		TEXT("Build_ConveyorPole_C"),
		FVector(100.0f, 150.0f, 100.0f),  // 1m x 1.5m x 1m
		false,
		true,  // Scaling enabled
		TEXT("Holo_ConveyorPole_C -> FGConveyorPoleHologram -> FGPoleHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Conveyor Pole Stackable (Scalable support structure)
	// Measured: X=100.000 Y=250.000 Z=200.000 (via spacing test: X+0, Y+100, Z+100)
	// Note: Uses FGStackablePoleHologram - registry entry enables scaling via registry-based adapter
	RegisterProfile(
		TEXT("Build_ConveyorPoleStackable_C"),
		FVector(100.0f, 200.0f, 200.0f),  // 1m x 2m x 2m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_ConveyorStackable_C -> FGStackablePoleHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// CONVEYOR ATTACHMENTS
	// ===================================
	
	// Conveyor Wall Attachment (Issue #268: Scaling enabled - wall-mounted belt support)
	// Placeholder dimensions matching stackable conveyor pole — validate via spacing test
	RegisterProfile(
		TEXT("Build_ConveyorPoleWall_C"),
		FVector(200.0f, 200.0f, 200.0f),  // Placeholder: 2m x 2m x 2m (validate via spacing test)
		false,
		true,  // Scaling enabled - Issue #268: wall-mounted belt support auto-connect
		TEXT("Holo_ConveyorWallAttachment_C -> FGWallAttachmentHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false  // Not yet validated via spacing test
	);
	
	// Conveyor Ceiling Attachment (Issue #268: Scaling enabled - ceiling-mounted belt support)
	// Placeholder dimensions matching stackable conveyor pole — validate via spacing test
	RegisterProfile(
		TEXT("Build_ConveyorCeilingAttachment_C"),
		FVector(100.0f, 200.0f, 200.0f),  // Placeholder: 1m x 2m x 2m (validate via spacing test)
		false,
		true,  // Scaling enabled - Issue #268: ceiling-mounted belt support auto-connect
		TEXT("Holo_ConveyorCeilingAttachment_C -> FGWallAttachmentHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false  // Not yet validated via spacing test
	);
	
	// Conveyor Wall Hole (Scaling disabled - wall passthrough)
	RegisterProfile(
		TEXT("Build_ConveyorWallHole_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall passthrough
		TEXT("Holo_ConveyorWallHole_C -> FGWallAttachmentHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Foundation Passthrough - Lift (Issue #187: Scaling enabled for grid placement)
	RegisterProfile(
		TEXT("Build_FoundationPassthrough_Lift_C"),
		FVector(800.0f, 800.0f, 400.0f),  // Foundation-sized: 8m x 8m x 4m
		false,
		true,  // Scaling enabled (Issue #187)
		TEXT("FGPassthroughHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// PIPELINES (NO SCALING)
	// ===================================
	
	// Pipeline Mk.1 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_Pipeline_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_Pipeline_C -> FGPipelineHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Pipeline Mk.1 No Indicator (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_Pipeline_NoIndicator_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_Pipeline_C -> FGPipelineHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Pipeline Mk.2 (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_PipelineMK2_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_Pipeline_C -> FGPipelineHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Pipeline Mk.2 No Indicator (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_PipelineMK2_NoIndicator_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based logistics
		TEXT("Holo_Pipeline_C -> FGPipelineHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// PIPELINE ATTACHMENTS
	// ===================================
	
	// Pipeline Junction Cross (Scalable)
	// Measured: X=300.000 Y=300.000 Z=200.000 (adjusted from 250x250 - better match)
	// Note: 300x300 footprint matches actual visual mesh better than 250x250
	// ATTACHMENT TYPE: Elevated pivot at 87.5% height (migrated from hardcoded type-check)
	RegisterProfile(
		TEXT("Build_PipelineJunction_Cross_C"),
		FVector(300.0f, 300.0f, 200.0f),  // 3m x 3m x 2m
		false,
		true,  // Scaling enabled
		TEXT("FGPipelineJunctionHologram -> FGPipelineAttachmentHologram -> FGPipeAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,  // Needs revalidation with new dimensions
		FVector(0.0f, 0.0f, -175.0f)  // 7/8 height pivot compensation (migrated from SFSubsystem hardcoded type-check)
	);
	
	// Pipeline Pump Mk.1 (Issue #245: Scaling enabled - can be placed on ground/walls without pipes)
	// ATTACHMENT TYPE: Elevated pivot like junction (same hierarchy)
	RegisterProfile(
		TEXT("Build_PipelinePump_C"),
		FVector(400.0f, 250.0f, 250.0f),  // Estimated from visual comparison with junction
		false,
		true,  // Scaling enabled - pumps can be placed on ground/walls (Issue #245)
		TEXT("Holo_PipelinePump_C -> FGPipelinePumpHologram -> FGPipelineAttachmentHologram -> FGPipeAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,
		FVector(0.0f, 0.0f, -175.0f)  // Same 7/8 height pivot compensation as junction (same hierarchy)
	);
	
	// Pipeline Pump Mk.2 (Issue #245: Scaling enabled - can be placed on ground/walls without pipes)
	// ATTACHMENT TYPE: Elevated pivot like junction (same hierarchy)
	RegisterProfile(
		TEXT("Build_PipelinePumpMk2_C"),
		FVector(400.0f, 250.0f, 250.0f),  // Estimated from visual comparison with junction
		false,
		true,  // Scaling enabled - pumps can be placed on ground/walls (Issue #245)
		TEXT("Holo_PipelinePump_C -> FGPipelinePumpHologram -> FGPipelineAttachmentHologram -> FGPipeAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,
		FVector(0.0f, 0.0f, -175.0f)  // Same 7/8 height pivot compensation as junction (same hierarchy)
	);
	
	// Pipeline Valve (Scaling disabled - functional attachment)
	RegisterProfile(
		TEXT("Build_Valve_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - functional pipeline attachment
		TEXT("FGPipelineAttachmentHologram -> FGPipeAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// MONITORS
	// ===================================
	
	// Conveyor Monitor (Scaling disabled - snaps to conveyor belts)
	RegisterProfile(
		TEXT("Build_ConveyorMonitor_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - snaps to conveyor belts
		TEXT("Holo_ConveyorMonitor_C -> FGSplineBuildableSnapHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// PIPELINE SUPPORTS
	// ===================================
	
	// Pipeline Support (Scaling disabled - support structure)
	RegisterProfile(
		TEXT("Build_PipelineSupport_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - pipeline support
		TEXT("Holo_PipelineSupport_C -> FGPipelinePoleHologram -> FGPoleHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Pipeline Support Stackable (Scalable support structure)
	// Measured: X=100.000 Y=200.000 Z=200.000 (via spacing test: X+0, Y+100, Z+100)
	// Note: Uses FGStackablePoleHologram - registry entry enables scaling via registry-based adapter
	RegisterProfile(
		TEXT("Build_PipeSupportStackable_C"),
		FVector(100.0f, 200.0f, 200.0f),  // 1m x 2m x 2m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_PipelineStackable_C -> FGStackablePoleHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Pipeline Support Wall (Scaling disabled - wall-mounted)
	RegisterProfile(
		TEXT("Build_PipelineSupportWall_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall-mounted
		TEXT("Holo_PipelineSupportWall_C -> FGWallAttachmentHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Pipeline Support Wall Hole (Scaling disabled - wall passthrough)
	RegisterProfile(
		TEXT("Build_PipelineSupportWallHole_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall passthrough
		TEXT("Holo_PipelineSupportWallHole_C -> FGWallAttachmentHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Foundation Passthrough - Pipe (Issue #187: Scaling enabled for grid placement)
	RegisterProfile(
		TEXT("Build_FoundationPassthrough_Pipe_C"),
		FVector(800.0f, 800.0f, 400.0f),  // Foundation-sized: 8m x 8m x 4m
		false,
		true,  // Scaling enabled (Issue #187)
		TEXT("FGPassthroughHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// CONVEYOR ATTACHMENTS (SCALABLE)
	// ===================================
	
	// Conveyor Merger (Scalable)
	// ATTACHMENT TYPE: Pivot at center height (50% vs pipe/tube 87.5%)
	// TODO: Validate X/Y dimensions via spacing test
	RegisterProfile(
		TEXT("Build_ConveyorAttachmentMerger_C"),
		FVector(400.0f, 400.0f, 200.0f),  // 4m x 4m x 2m (Z validated, X/Y estimated)
		false,
		true,  // Scaling enabled
		TEXT("Holo_ConveyorAttachment_C -> FGConveyorAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,  // Not validated - dimensions are estimates
		FVector(0.0f, 0.0f, -100.0f)  // Pivot compensation (migrated from SFSubsystem hardcoded type-check)
	);
	
	// Conveyor Merger (Priority) (Scalable)
	// ATTACHMENT TYPE: Pivot at center height (50% vs pipe/tube 87.5%)
	// TODO: Validate X/Y dimensions via spacing test
	RegisterProfile(
		TEXT("Build_ConveyorAttachmentMergerPriority_C"),
		FVector(400.0f, 400.0f, 200.0f),  // 4m x 4m x 2m (Z validated, X/Y estimated)
		false,
		true,  // Scaling enabled
		TEXT("Holo_ConveyorAttachment_C -> FGConveyorAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,  // Not validated - dimensions are estimates
		FVector(0.0f, 0.0f, -100.0f)  // Pivot compensation (migrated from SFSubsystem hardcoded type-check)
	);
	
	// Conveyor Splitter (Scalable)
	// ATTACHMENT TYPE: Pivot at center height (50% vs pipe/tube 87.5%)
	// TODO: Validate X/Y dimensions via spacing test
	RegisterProfile(
		TEXT("Build_ConveyorAttachmentSplitter_C"),
		FVector(400.0f, 400.0f, 200.0f),  // 4m x 4m x 2m (Z validated, X/Y estimated)
		false,
		true,  // Scaling enabled
		TEXT("Holo_ConveyorAttachment_C -> FGConveyorAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,  // Not validated - dimensions are estimates
		FVector(0.0f, 0.0f, -100.0f)  // Pivot compensation (migrated from SFSubsystem hardcoded type-check)
	);
	
	// Conveyor Splitter (Smart) (Scalable)
	// ATTACHMENT TYPE: Pivot at center height (50% vs pipe/tube 87.5%)
	// TODO: Validate X/Y dimensions via spacing test
	RegisterProfile(
		TEXT("Build_ConveyorAttachmentSplitterSmart_C"),
		FVector(400.0f, 400.0f, 200.0f),  // 4m x 4m x 2m (Z validated, X/Y estimated)
		false,
		true,  // Scaling enabled
		TEXT("Holo_ConveyorAttachment_C -> FGConveyorAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,  // Not validated - dimensions are estimates
		FVector(0.0f, 0.0f, -100.0f)  // Pivot compensation (migrated from SFSubsystem hardcoded type-check)
	);
	
	// Conveyor Splitter (Programmable) (Scalable)
	// ATTACHMENT TYPE: Pivot at center height (50% vs pipe/tube 87.5%)
	// TODO: Validate X/Y dimensions via spacing test
	RegisterProfile(
		TEXT("Build_ConveyorAttachmentSplitterProgrammable_C"),
		FVector(400.0f, 400.0f, 200.0f),  // 4m x 4m x 2m (Z validated, X/Y estimated)
		false,
		true,  // Scaling enabled
		TEXT("Holo_ConveyorAttachment_C -> FGConveyorAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,  // Not validated - dimensions are estimates
		FVector(0.0f, 0.0f, -100.0f)  // Pivot compensation (migrated from SFSubsystem hardcoded type-check)
	);
}
