// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
// Smart! Mod - Water Pump Child Hologram Implementation
// Issue #197: Water extractor scaling with proper water validation

#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildable.h"
#include "FGWaterVolume.h"
#include "FGConstructDisqualifier.h"
#include "Kismet/GameplayStatics.h"
#include "Data/SFBuildableSizeRegistry.h"

ASFWaterPumpChildHologram::ASFWaterPumpChildHologram()
{
}

void ASFWaterPumpChildHologram::CheckValidPlacement()
{
	// Issue #197: Replace vanilla CheckValidPlacement() entirely for children.
	//
	// Why not call Super: Vanilla CheckMinimumDepth() fails for children positioned
	// via SetActorLocation even when they ARE over valid water. The vanilla code likely
	// depends on internal placement state set during SetHologramLocationAndRotation()
	// which children don't go through.
	//
	// What we do instead: Our own water volume check using EncompassesPoint().
	// This validates that the child is actually over water without relying on
	// vanilla's broken depth trace for children.
	//
	// IMPORTANT: We MUST validate water. An unvalidated extractor over land would
	// generate free water resources, breaking game balance.
	
	if (!ValidateWaterPosition())
	{
		AddConstructDisqualifier(UFGCDNeedsWaterVolume::StaticClass());
	}
}

void ASFWaterPumpChildHologram::ConfigureActor(AFGBuildable* inBuildable) const
{
	// Skip AFGResourceExtractorHologram::ConfigureActor which asserts mSnappedExtractableResource.
	// Children don't snap to resources — parent handles resource binding.
	// Call grandparent directly to get basic factory hologram configuration.
	AFGFactoryHologram::ConfigureActor(inBuildable);
}

AActor* ASFWaterPumpChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	UE_LOG(LogSmartFoundations, Display, TEXT("WaterPump Construct: %s at %s"), 
		*GetName(), *GetActorLocation().ToString());
	return Super::Construct(out_children, constructionID);
}

bool ASFWaterPumpChildHologram::ValidateWaterPosition() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	
	const FVector ChildLocation = GetActorLocation();
	
	// ========================================================================
	// THREE-PHASE WATER VALIDATION
	// ========================================================================
	//
	// Phase 1: Sky Access — Can the water surface be reached from above?
	//   Trace from high above the child straight down. If terrain is hit above
	//   the child's Z position, terrain blocks access (cliff overhang, shoreline).
	//   This prevents false positives where a water volume extends laterally
	//   under terrain that a player could never aim through to place a building.
	//
	// Phase 2: Water Volume — Is the child position inside a water volume?
	//   Uses EncompassesPoint() since AFGWaterVolume is a PhysicsVolume (overlap
	//   trigger) that doesn't participate in line traces.
	//
	// Phase 3: Depth — Is the water deep enough at this position?
	//   Trace downward to terrain, verify depth ≥ 50cm and terrain is inside
	//   the water volume (not at a bank/cliff edge).
	// ========================================================================
	
	static constexpr float MinimumWaterDepth = 50.0f;     // Matches AFGBuildableWaterPump::mMinimumDepthForPlacement
	static constexpr float DepthTraceDistance = 5000.0f;   // 50m max trace down
	static constexpr float SkyAccessTolerance = 50.0f;     // Allow terrain within 50cm of top (waterline tolerance)
	
	FCollisionQueryParams BaseTraceParams(SCENE_QUERY_STAT(WaterPumpValidation), false);
	BaseTraceParams.AddIgnoredActor(this);
	
	// ---- Phase 1: Sky Access Check (5-point bounding box) ----
	// Trace from the top of the building's bounding box at 5 points (center + 4 corners)
	// straight down. If ANY point has terrain above the building top, this position is
	// under a cliff/overhang and the building wouldn't physically fit here.
	// Uses registry dimensions rather than a fixed height to work in covered areas.
	{
		// Get building half-extents from registry
		FVector BuildingSize = FVector(2000.0f, 1800.0f, 2400.0f);  // Default water extractor size
		if (mBuildClass)
		{
			FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(mBuildClass);
			if (!Profile.DefaultSize.IsNearlyZero())
			{
				BuildingSize = Profile.DefaultSize;
			}
		}
		
		const float HalfX = BuildingSize.X * 0.5f;
		const float HalfY = BuildingSize.Y * 0.5f;
		const float TopZ = BuildingSize.Z * 0.5f;  // Half-height above child base
		
		// 5 trace points relative to child (local space)
		const TArray<FVector2D> TraceOffsets = {
			FVector2D(0.0f, 0.0f),          // Center
			FVector2D(-HalfX, -HalfY),      // Front-Left
			FVector2D(-HalfX, +HalfY),      // Front-Right
			FVector2D(+HalfX, -HalfY),      // Back-Left
			FVector2D(+HalfX, +HalfY),      // Back-Right
		};
		
		// Rotate offsets by hologram rotation to get world-space positions
		const FRotator ActorRotation = GetActorRotation();
		
		for (const FVector2D& Offset : TraceOffsets)
		{
			// Rotate the local XY offset into world space
			const FVector LocalOffset(Offset.X, Offset.Y, 0.0f);
			const FVector WorldOffset = ActorRotation.RotateVector(LocalOffset);
			
			// Trace from top of bounding box down to well below the child
			const FVector TraceStart = ChildLocation + WorldOffset + FVector(0.0f, 0.0f, TopZ);
			const FVector TraceEnd = ChildLocation + WorldOffset - FVector(0.0f, 0.0f, DepthTraceDistance);
			
			FHitResult SkyHit;
			if (World->LineTraceSingleByChannel(SkyHit, TraceStart, TraceEnd, ECC_WorldStatic, BaseTraceParams))
			{
				// Terrain hit — check if it's ABOVE the child's base position (waterline).
				// The trace goes from building top downward. If terrain is hit above the
				// child's Z (where it sits at the water surface), there's land between
				// the sky and the water at this point — a player couldn't aim here.
				if (SkyHit.ImpactPoint.Z > ChildLocation.Z + SkyAccessTolerance)
				{
					UE_LOG(LogSmartFoundations, Log, TEXT("WaterPump %s: Sky access blocked at offset (%.0f,%.0f) — terrain Z=%.1f above waterline Z=%.1f"),
						*GetName(), Offset.X, Offset.Y, SkyHit.ImpactPoint.Z, ChildLocation.Z);
					return false;
				}
			}
			// No terrain hit at this point = clear sky = good
		}
	}
	
	// ---- Phase 2: Water Volume Check ----
	TArray<AActor*> WaterVolumes;
	UGameplayStatics::GetAllActorsOfClass(World, AFGWaterVolume::StaticClass(), WaterVolumes);
	
	AFGWaterVolume* ContainingVolume = nullptr;
	for (AActor* VolumeActor : WaterVolumes)
	{
		AFGWaterVolume* WaterVolume = Cast<AFGWaterVolume>(VolumeActor);
		if (WaterVolume && WaterVolume->EncompassesPoint(ChildLocation))
		{
			ContainingVolume = WaterVolume;
			break;
		}
	}
	
	if (!ContainingVolume)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("WaterPump %s: Position %s is NOT in any water volume"),
			*GetName(), *ChildLocation.ToString());
		return false;
	}
	
	// ---- Phase 3: Depth Check ----
	// Trace downward from child to find river bed / terrain below.
	// Must be deep enough AND terrain must be inside the water volume.
	FCollisionQueryParams DepthTraceParams = BaseTraceParams;
	for (AActor* VolumeActor : WaterVolumes)
	{
		DepthTraceParams.AddIgnoredActor(VolumeActor);
	}
	
	const FVector DepthStart = ChildLocation;
	const FVector DepthEnd = ChildLocation - FVector(0.0f, 0.0f, DepthTraceDistance);
	
	FHitResult DepthHit;
	if (World->LineTraceSingleByChannel(DepthHit, DepthStart, DepthEnd, ECC_WorldStatic, DepthTraceParams))
	{
		const FVector TerrainPoint = DepthHit.ImpactPoint;
		const float DepthBelowChild = ChildLocation.Z - TerrainPoint.Z;
		
		if (DepthBelowChild < MinimumWaterDepth)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("WaterPump %s: Too shallow %.1f cm at %s"),
				*GetName(), DepthBelowChild, *ChildLocation.ToString());
			return false;
		}
		
		// Terrain hit must be inside the water volume — if outside, we're at a bank/cliff edge
		if (!ContainingVolume->EncompassesPoint(TerrainPoint))
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("WaterPump %s: Terrain hit outside water volume at %s — bank/cliff edge"),
				*GetName(), *ChildLocation.ToString());
			return false;
		}
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("WaterPump %s: All 3 phases passed — depth %.1f cm, sky clear, in water volume"),
			*GetName(), DepthBelowChild);
	}
	// No terrain hit within trace distance = very deep water = valid
	
	return true;
}
