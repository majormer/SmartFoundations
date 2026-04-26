// SFBuildableSizeRegistry_Transport.cpp
// Transport buildable size registrations (stations, trains, hypertubes, vehicles)
// Separated from main registry for better organization

#include "SFBuildableSizeRegistry.h"
#include "Logging/SFLogMacros.h"

extern FString CurrentSourceFile;

void USFBuildableSizeRegistry::RegisterTransport()
{
	CurrentSourceFile = TEXT("SFBuildableSizeRegistry_Transport.cpp");
	SF_LOG_ADAPTER(Normal, TEXT("📂 Registering category: Transport (SFBuildableSizeRegistry_Transport.cpp)"));
	
	// ===================================
	// STATIONS
	// ===================================
	
	// Truck Station (Scalable)
	// Measured: X=1700.000 Y=1450.000 Z=1220.000 (via spacing test: X+0, Y+550, Z+600)
	RegisterProfile(
		TEXT("Build_TruckStation_C"),
		FVector(1700.0f, 1450.0f, 1220.0f),  // 17m x 14.5m x 12.2m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Drone Station (Scalable)
	// Measured: X=2400.000 Y=2400.000 Z=1500.000 (via spacing test: X+700, Y+400, Z+1150)
	RegisterProfile(
		TEXT("Build_DroneStation_C"),
		FVector(2400.0f, 2400.0f, 1500.0f),  // 24m x 24m x 15m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// TRAIN STATIONS & PLATFORMS (NO SCALING)
	// ===================================
	
	// Train Station (Scaling disabled - railroad track assignment conflict)
	// Note: Cannot scale - game only allows one platform per track segment
	RegisterProfile(
		TEXT("Build_TrainStation_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - railroad track assignment limitation
		TEXT("Holo_TrainStation_C -> FGTrainStationHologram -> FGTrainPlatformHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Freight Platform (Scaling disabled - railroad track assignment conflict)
	// Note: Cannot scale - same limitation as Train Station, inherits from FGTrainPlatformHologram
	RegisterProfile(
		TEXT("Build_TrainDockingStation_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - railroad track assignment limitation
		TEXT("Holo_TrainPlatformCargo_C -> FGTrainStationHologram -> FGTrainPlatformHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Fluid Freight Platform (Scaling disabled - railroad track assignment conflict)
	// Note: Cannot scale - same limitation as Train Station, inherits from FGTrainPlatformHologram
	RegisterProfile(
		TEXT("Build_TrainDockingStationLiquid_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - railroad track assignment limitation
		TEXT("Holo_TrainPlatformCargo_C -> FGTrainStationHologram -> FGTrainPlatformHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Railroad End Stop (Scaling disabled - railroad track attachment)
	RegisterProfile(
		TEXT("Build_RailroadEndStop_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - railroad track attachment
		TEXT("Holo_RailroadEndStop_C -> FGRailroadAttachmentHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Empty Train Platform (Scaling disabled - railroad track assignment conflict)
	// Note: Cannot scale - same limitation as Train Station, inherits from FGTrainPlatformHologram
	RegisterProfile(
		TEXT("Build_TrainPlatformEmpty_02_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - railroad track assignment limitation
		TEXT("Holo_TrainPlatformEmpty_C -> FGTrainStationHologram -> FGTrainPlatformHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// RAILROAD TRACKS & SIGNALS (NO SCALING)
	// ===================================
	
	// Railroad Track (Scaling disabled - spline-based track system)
	RegisterProfile(
		TEXT("Build_RailroadTrack_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based, drag-to-place
		TEXT("Holo_RailroadTrack_C -> FGRailroadTrackHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Railroad Block Signal (Scaling disabled - railroad signal system)
	RegisterProfile(
		TEXT("Build_RailroadBlockSignal_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - railroad signal
		TEXT("Holo_RailroadSignal_C -> FGRailroadSignalHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Railroad Path Signal (Scaling disabled - railroad signal system)
	RegisterProfile(
		TEXT("Build_RailroadPathSignal_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - railroad signal
		TEXT("Holo_RailroadSignal_C -> FGRailroadSignalHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// HYPERTUBES (SPLINE-BASED - NO SCALING)
	// ===================================
	
	// Hypertube (Scaling disabled - spline-based)
	RegisterProfile(
		TEXT("Build_PipeHyper_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - spline-based, drag-to-place
		TEXT("Holo_PipeHyper_C -> FGPipelineHologram -> FGSplineHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Hypertube Entrance (Scaling disabled - pipe part)
	RegisterProfile(
		TEXT("Build_PipeHyperStart_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - pipe part
		TEXT("Holo_PipeHyperStart_C -> FGPipePartHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Hypertube Wall Support (Scaling disabled - wall-mounted)
	RegisterProfile(
		TEXT("Build_HyperTubeWallSupport_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall-mounted
		TEXT("Holo_PipelineSupportWall_C -> FGWallAttachmentHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Hypertube Wall Hole (Scaling disabled - wall-mounted)
	RegisterProfile(
		TEXT("Build_HyperTubeWallHole_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - wall-mounted
		TEXT("Holo_PipelineSupportWallHole_C -> FGWallAttachmentHologram -> FGGenericBuildableHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Hypertube Floor Hole (Scaling disabled - foundation passthrough)
	RegisterProfile(
		TEXT("Build_FoundationPassthrough_Hypertube_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - passthrough requires foundation snapping
		TEXT("FGPassthroughPipeHyperHologram -> FGPassthroughPipeBaseHologram -> FGPassthroughHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Hypertube Support (Scaling disabled - adjustable height pole)
	RegisterProfile(
		TEXT("Build_PipeHyperSupport_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - pole has adjustable height for terrain
		TEXT("Holo_PipelineSupport_C -> FGPipelinePoleHologram -> FGPoleHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Hypertube Pole Stackable (Scalable support structure)
	// Measured: X=100.000 Y=200.000 Z=200.000 (via spacing test: X+0, Y+100, Z+100)
	// Note: Uses FGStackablePoleHologram - registry entry enables scaling via registry-based adapter
	RegisterProfile(
		TEXT("Build_HyperPoleStackable_C"),
		FVector(100.0f, 200.0f, 200.0f),  // 1m x 2m x 2m
		false,
		true,  // Scaling enabled - uses registry-based adapter
		TEXT("Holo_PipelineStackable_C -> FGStackablePoleHologram -> FGFactoryBuildingHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// HYPERTUBE JUNCTIONS (SCALABLE)
	// ===================================
	
	// Hypertube Junction (Scalable)
	// Spacing test: X+300→500, Y+150→500, Z+100 (but Z should NOT include spacing!)
	// Visual mesh dimensions: 500×500×200 (X/Y include spacing, Z is raw height)
	// ATTACHMENT TYPE: Pivot elevated significantly above visual bottom
	// AnchorOffset.Z = -175 compensates for elevated pivot point (87.5% of height, final tuning)
	RegisterProfile(
		TEXT("Build_HyperTubeJunction_C"),
		FVector(500.0f, 500.0f, 200.0f),  // 5m x 5m x 2m (visual mesh size)
		false,
		true,  // Scaling enabled - uses FSFHypertubeAttachmentAdapter
		TEXT("Holo_HyperTubeJunction_C -> FGPipeHyperAttachmentHologram -> FGPipeAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false,  // Needs revalidation with adjusted AnchorOffset
		FVector(0.0f, 0.0f, -175.0f)  // Lower children 175cm to align with parent visual bottom
	);
	
	// Hypertube T-Junction (Scalable)
	// Spacing test: X+400→500, Y+300, Z+100
	// Visual mesh dimensions: 500×400×200 (X includes spacing, Y/Z raw dimensions)
	// ATTACHMENT TYPE: Same pivot behavior as Hypertube Junction (87.5% height)
	RegisterProfile(
		TEXT("Build_HypertubeTJunction_C"),
		FVector(500.0f, 400.0f, 200.0f),  // 5m x 4m x 2m (visual mesh size)
		false,
		true,  // Scaling enabled - uses FSFHypertubeAttachmentAdapter
		TEXT("Holo_HypertubeTJunction_C -> Holo_HyperTubeJunction_C -> FGPipeHyperAttachmentHologram -> FGPipeAttachmentHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true,  // Validated via spacing test
		FVector(0.0f, 0.0f, -175.0f)  // Same pivot offset as Junction (7/8 × 200cm height)
	);
	
	// ===================================
	// PORTALS & JUMP PADS
	// ===================================
	
	// Portal (Scalable)
	// Measured: X=800.000 Y=800.000 Z=1200.000 (via spacing test: X+0, Y+0, Z+0)
	RegisterProfile(
		TEXT("Build_Portal_C"),
		FVector(800.0f, 800.0f, 1200.0f),  // 8m x 8m x 12m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Portal Satellite (Scalable)
	// Measured: X=800.000 Y=800.000 Z=1000.000 (via spacing test: X+0, Y+0, Z+0)
	RegisterProfile(
		TEXT("Build_PortalSatellite_C"),
		FVector(800.0f, 800.0f, 1000.0f),  // 8m x 8m x 10m
		false,
		true,  // Scaling enabled
		TEXT("FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// Jump Pad (Adjustable) (Scaling disabled - adjustable trajectory wheel)
	RegisterProfile(
		TEXT("Build_JumpPadAdjustable_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - trajectory wheel only applies to parent
		TEXT("Holo_JumpPadLauncher_C -> FGJumpPadLauncherHologram -> FGJumpPadHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Landing Pad (U-Jelly Landing Pad) (Scalable)
	// Measured: X=1000.000 Y=1000.000 Z=650.000 (via spacing test: X+0, Y+0, Z+150)
	RegisterProfile(
		TEXT("Build_LandingPad_C"),
		FVector(1000.0f, 1000.0f, 650.0f),  // 10m x 10m x 6.5m
		false,
		true,  // Scaling enabled
		TEXT("Holo_LandingPad_C -> FGJumpPadHologram -> FGFactoryHologram -> FGBuildableHologram -> FGHologram"),
		true  // Validated via spacing test
	);
	
	// ===================================
	// ELEVATORS (NO SCALING)
	// ===================================
	
	// Elevator (Scaling disabled - adjustable height system)
	RegisterProfile(
		TEXT("Build_Elevator_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - elevator has adjustable height
		TEXT("Holo_Elevator_C -> FGElevatorHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// Elevator Floor Stop (Scaling disabled - must align with elevator floors)
	RegisterProfile(
		TEXT("Build_ElevatorFloorStop_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - must align with specific elevator floors
		TEXT("Holo_ElevatorFloorStop_C -> FGElevatorFloorStopHologram -> FGBuildableHologram -> FGHologram"),
		false
	);
	
	// ===================================
	// VEHICLES (NO SCALING)
	// ===================================
	
	// Tractor (Scaling disabled - vehicle)
	RegisterProfile(
		TEXT("BP_Tractor_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vehicle
		TEXT("Holo_Tractor_C -> FGWheeledVehicleHologram -> FGVehicleHologram -> FGHologram"),
		false
	);
	
	// Truck (Scaling disabled - vehicle)
	RegisterProfile(
		TEXT("BP_Truck_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vehicle
		TEXT("Holo_Truck_C -> FGWheeledVehicleHologram -> FGVehicleHologram -> FGHologram"),
		false
	);
	
	// Explorer (Scaling disabled - vehicle)
	RegisterProfile(
		TEXT("BP_Explorer_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vehicle
		TEXT("Holo_Explorer_C -> FGWheeledVehicleHologram -> FGVehicleHologram -> FGHologram"),
		false
	);
	
	// Drone (Scaling disabled - vehicle)
	RegisterProfile(
		TEXT("BP_DroneTransport_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vehicle
		TEXT("Holo_DroneTransport_C -> FGBuildableDroneHologram -> FGVehicleHologram -> FGHologram"),
		false
	);
	
	// Locomotive (Scaling disabled - vehicle)
	RegisterProfile(
		TEXT("BP_Locomotive_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vehicle
		TEXT("Holo_Locomotive_C -> FGRailroadVehicleHologram -> FGVehicleHologram -> FGHologram"),
		false
	);
	
	// Freight Wagon (Scaling disabled - vehicle)
	RegisterProfile(
		TEXT("BP_FreightWagon_C"),
		FVector(100.0f, 100.0f, 100.0f),  // Reference only
		false,
		false,  // Scaling disabled - vehicle
		TEXT("FGRailroadVehicleHologram -> FGVehicleHologram -> FGHologram"),
		false
	);
}
