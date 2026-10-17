// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - Water Pump Child Hologram Implementation
// Issue #197: Water extractor scaling with proper water validation

#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildable.h"
#include "FGWaterVolume.h"
#include "FGConstructDisqualifier.h"
#include "Kismet/GameplayStatics.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Buildables/FGBuildableWaterPump.h"
#include "Subsystem/SFHologramDataService.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendBuiltActors.h"
#include "Data/SFHologramDataRegistry.h"

ASFWaterPumpChildHologram::ASFWaterPumpChildHologram()
{
}

void ASFWaterPumpChildHologram::CheckValidPlacement()
{
    mSnappedExtractableResource = nullptr;
    mUpgradeTarget = nullptr;
    if (!ValidateWaterTree(this))
    {
        AddConstructDisqualifier(UFGCDNeedsWaterVolume::StaticClass());
        return;
    }

    // The native server root deserializes these inherited resource fields.
    // Resolve the destination volume, never copy the source extractor's binding.
    TArray<AActor*> Volumes;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFGWaterVolume::StaticClass(), Volumes);
    for (AActor* Actor : Volumes)
    {
        AFGWaterVolume* Volume = Cast<AFGWaterVolume>(Actor);
        if (Volume && Volume->EncompassesPoint(GetActorLocation()))
        {
            mSnappedExtractableResource.SetObject(Volume);
            mSnappedExtractableResource.SetInterface(Cast<IFGExtractableResourceInterface>(Volume));
            return;
        }
    }
    AddConstructDisqualifier(UFGCDNeedsWaterVolume::StaticClass());
}

void ASFWaterPumpChildHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// #418: Block vanilla parent propagation from repositioning this child — the same no-op every
	// other Smart grid child class uses. Smart! positions children via SetActorLocation. This was
	// the LAST Smart child class without the override: parent propagation dragged pump children
	// toward the hit result each frame, so the tick-driven water check sometimes ran against the
	// wrong (reset) location between intended-transform re-apply passes. ValidateWaterPosition()
	// keys entirely off GetActorLocation(), so blocking vanilla repositioning makes the water
	// validation strictly more correct.
}

void ASFWaterPumpChildHologram::ConfigureActor(AFGBuildable* inBuildable) const
{
	// Skip AFGResourceExtractorHologram::ConfigureActor which asserts mSnappedExtractableResource.
	// Owned copies can construct without native aim state; the buildable's native
	// missing-resource discovery binds water at its own validated destination.
	// Call grandparent directly to get basic factory hologram configuration.
	AFGFactoryHologram::ConfigureActor(inBuildable);
}

AActor* ASFWaterPumpChildHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("WaterPump Construct: %s at %s"), 
		*GetName(), *GetActorLocation().ToString());
    AActor* BuiltActor = Super::Construct(out_children, constructionID);
    if (BuiltActor && !GetParentHologram())
    {
        // A Water Extractor can also be the live Extend root. Publish its native
        // Construct result through the same wiring boundary as other factory roots.
        USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
        USFExtendService* Extend = Subsystem ? Subsystem->GetExtendService() : nullptr;
        if (Extend)
        {
            if (const FSFHologramData* Data = USFHologramDataRegistry::GetData(this))
            {
                if (!Data->JsonCloneId.IsEmpty()) Extend->RegisterJsonBuiltActor(Data->JsonCloneId, BuiltActor);
            }
            SFExtendBuiltActors::RegisterChildren(Extend, this, out_children);
            if (Extend->HasPendingPostBuildWiring()) Extend->RegisterJsonBuiltActor(TEXT("parent"), BuiltActor);
        }
    }
    return BuiltActor;
}

bool ASFWaterPumpChildHologram::ValidateWaterPosition() const
{
	return ValidateWaterPosition(this);
}

bool ASFWaterPumpChildHologram::IsVanillaWaterExtractor(const UClass* BuildClass)
{
	return BuildClass && BuildClass->IsChildOf(AFGBuildableWaterPump::StaticClass())
		&& BuildClass->GetName() == TEXT("Build_WaterPump_C")
		&& BuildClass->GetOutermost()->GetName().StartsWith(TEXT("/Game/FactoryGame/"));
}

bool ASFWaterPumpChildHologram::ValidateWaterPosition(const AFGHologram* Hologram)
{
	UWorld* World = IsValid(Hologram) ? Hologram->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	
	const FVector ChildLocation = Hologram->GetActorLocation();
	
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
	BaseTraceParams.AddIgnoredActor(Hologram);
	
	// ---- Phase 1: Sky Access Check (5-point bounding box) ----
	// Trace from the top of the building's bounding box at 5 points (center + 4 corners)
	// straight down. If ANY point has terrain above the building top, this position is
	// under a cliff/overhang and the building wouldn't physically fit here.
	// Uses registry dimensions rather than a fixed height to work in covered areas.
	{
		// Get building half-extents from registry
		FVector BuildingSize = FVector(2000.0f, 1800.0f, 2400.0f);  // Default water extractor size
		if (Hologram->GetBuildClass())
		{
			FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass());
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
		const FRotator ActorRotation = Hologram->GetActorRotation();
		
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
					UE_LOG(LogSmartHologram, VeryVerbose, TEXT("WaterPump %s: Sky access blocked at offset (%.0f,%.0f) — terrain Z=%.1f above waterline Z=%.1f"),
						*Hologram->GetName(), Offset.X, Offset.Y, SkyHit.ImpactPoint.Z, ChildLocation.Z);
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
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("WaterPump %s: Position %s is NOT in any water volume"),
			*Hologram->GetName(), *ChildLocation.ToString());
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
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("WaterPump %s: Too shallow %.1f cm at %s"),
				*Hologram->GetName(), DepthBelowChild, *ChildLocation.ToString());
			return false;
		}
		
		// Terrain hit must be inside the water volume — if outside, we're at a bank/cliff edge
		if (!ContainingVolume->EncompassesPoint(TerrainPoint))
		{
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("WaterPump %s: Terrain hit outside water volume at %s — bank/cliff edge"),
				*Hologram->GetName(), *ChildLocation.ToString());
			return false;
		}
		
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("WaterPump %s: All 3 phases passed — depth %.1f cm, sky clear, in water volume"),
			*Hologram->GetName(), DepthBelowChild);
	}
	// No terrain hit within trace distance = very deep water = valid
	
	return true;
}

bool ASFWaterPumpChildHologram::ValidateWaterTree(AFGHologram* Parent)
{
	return ValidateWaterTree(Parent, [](const AFGHologram* Cell) { return ValidateWaterPosition(Cell); });
}

bool ASFWaterPumpChildHologram::ValidateWaterTree(AFGHologram* Parent, TFunctionRef<bool(const AFGHologram*)> ValidateCell)
{
	if (!IsValid(Parent)) return false;
	if (!IsVanillaWaterExtractor(Parent->GetBuildClass())) return true;
	TArray<AFGHologram*> Pending{Parent};
	TSet<AFGHologram*> Visited;
	while (!Pending.IsEmpty())
	{
		AFGHologram* Cell = Pending.Pop(EAllowShrinking::No);
		if (!IsValid(Cell) || Visited.Contains(Cell)) continue;
		Visited.Add(Cell);
		if (IsVanillaWaterExtractor(Cell->GetBuildClass()))
		{
			// Horizontal-only family: stale/imported vertical grids cannot bypass the controls.
			if (!FMath::IsNearlyEqual(Cell->GetActorLocation().Z, Parent->GetActorLocation().Z, 1.0)
				|| !Cell->GetActorUpVector().Equals(FVector::UpVector, 0.001)
				|| !ValidateCell(Cell)) return false;
		}
		Pending.Append(Cell->GetHologramChildren());
	}
	return true;
}

ASFWaterPumpChildHologram* ASFWaterPumpChildHologram::SpawnPreview(AFGHologram* Parent, FName Name, const FVector& Location)
{
	if (!IsValid(Parent) || !Parent->GetWorld() || !IsVanillaWaterExtractor(Parent->GetBuildClass())) return nullptr;
	FActorSpawnParameters Params;
	// Actor names must remain unique across repeated requests; AddChild owns the stable key.
	Params.Owner = Parent->GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	ASFWaterPumpChildHologram* Child = Parent->GetWorld()->SpawnActor<ASFWaterPumpChildHologram>(
		StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (!Child) return nullptr;
	Child->SetChildBuildClass(Parent->GetBuildClass());
	Child->SetRecipe(Parent->GetRecipe());
	Child->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
	Parent->AddChild(Child, Name);
	// Native holograms start hidden, which ValidatePlacementAndCost reports as an
	// invalid aim. This positioned child must be visible before that validation;
	// do not depend on a later Extend material-state change to reveal new cells.
	Child->SetActorHiddenInGame(false);
	Child->SetActorLocation(Location);
	Child->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
	USFHologramDataService::MarkAsChild(Child, Parent, ESFChildHologramType::ScalingGrid);
	USFHologramDataService::EnableValidation(Child);
	Child->SetActorEnableCollision(false);
	TArray<UPrimitiveComponent*> Primitives;
	Child->GetComponents(Primitives);
	for (UPrimitiveComponent* Primitive : Primitives) Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Child->Tags.AddUnique(TEXT("SF_GridChild"));
	return Child;
}

void ASFWaterPumpChildHologram::SetHologramNudgeLocation()
{
	// [#497] Vanilla's locked-parent placement path (UFGBuildGunStateBuild::TickState ->
	// AFGHologram::UpdateHologramPlacement (FGHologram.cpp:440) -> SetHologramNudgeLocation
	// (FGHologram.cpp:2120)) cascades through mChildren with a PLAIN SetActorLocation of
	// lock-location + nudge offset - bypassing the SetHologramLocationAndRotation no-op entirely.
	// Extend locks its parent, children never capture a lock location (ZeroVector), so the cascade
	// dragged every child to world origin every tick (caught by the #497 origin-trap stack dump).
	// Smart owns this child's transform; parent nudges are propagated by Smart's own
	// transform-change follow. No-op, mirroring the #418 drift contract.
}
