#include "Subsystem/SFValidationService.h"
#include "SmartFoundations.h"
#include "FGHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGPipelineJunctionHologram.h"
#include "Hologram/FGPipeAttachmentHologram.h"
#include "Hologram/FGPipeHyperAttachmentHologram.h"
#include "Hologram/FGPassthroughHologram.h"
#include "Hologram/FGPowerPoleHologram.h"
#include "Hologram/FGCeilingLightHologram.h"
#include "Hologram/FGFloodlightHologram.h"
#include "Hologram/FGWallAttachmentHologram.h"
#include "Engine/World.h"
#include "Kismet/KismetSystemLibrary.h"

// Known spacing constraints
static constexpr int32 MAX_SPACING_FOR_BELT = 5400; // Conveyor belt pole spacing cap

FSFValidationService::FSFValidationService()
{
}

FSFValidationService::~FSFValidationService()
{
}

FSFValidationService::FValidationResult FSFValidationService::ValidatePlacement(
	const AFGHologram* Hologram,
	const FVector& Position,
	const FRotator& Rotation,
	UWorld* World
) const
{
	// TODO: Extract placement validation logic from SFSubsystem
	
	if (!IsHologramValid(Hologram))
	{
		return FValidationResult::Failure(TEXT("Invalid hologram"));
	}
	
	if (!World)
	{
		return FValidationResult::Failure(TEXT("Invalid world context"));
	}
	
	// Check floor requirements if applicable
	if (RequiresFloorValidation(Hologram))
	{
		FValidationResult FloorResult = ValidateFloorRequirement(Hologram, Position, World);
		if (!FloorResult.bIsValid)
		{
			return FloorResult;
		}
	}
	
	return FValidationResult::Success();
}

FSFValidationService::FValidationResult FSFValidationService::ValidateGridSize(const FIntVector& GridCounters) const
{
	// Extracted from SFSubsystem::RegenerateChildHologramGrid (Phase 0 - Task #61.6)
	
	// Calculate total grid size
	int32 XCount = FMath::Abs(GridCounters.X);
	int32 YCount = FMath::Abs(GridCounters.Y);
	int32 ZCount = FMath::Abs(GridCounters.Z);
	int32 TotalSize = XCount * YCount * ZCount;
	
	if (TotalSize <= 0)
	{
		return FValidationResult::Failure(TEXT("Grid size must be positive"));
	}
	
	// CRITICAL: Hard cap to prevent UObject exhaustion (Satisfactory's limit is 2,162,688)
	// Each child + locked state creates multiple UObjects (widgets, components, delegates, etc.)
	// Capping at 1500 children leaves plenty of headroom for the rest of the game
	if (TotalSize > SMART_MAX_GRID_SIZE + 1)  // +1 for parent
	{
		FString ErrorMsg = FString::Printf(
			TEXT("Grid size %d exceeds maximum limit of %d (prevents UObject exhaustion)"),
			TotalSize,
			SMART_MAX_GRID_SIZE + 1
		);
		return FValidationResult::Failure(ErrorMsg);
	}
	
	return FValidationResult::Success();
}

bool FSFValidationService::ValidateAndAdjustGridSize(FIntVector& GridCounters, int32& OutChildrenNeeded) const
{
	// Extracted from SFSubsystem::RegenerateChildHologramGrid (Phase 0 - Task #61.6)
	
	// Calculate total items and children needed
	int32 XCount = FMath::Abs(GridCounters.X);
	int32 YCount = FMath::Abs(GridCounters.Y);
	int32 ZCount = FMath::Abs(GridCounters.Z);
	int32 TotalItems = XCount * YCount * ZCount;
	int32 ChildrenNeeded = FMath::Max(0, TotalItems - 1);  // -1 for parent
	
	// Check if we exceed the limit
	if (ChildrenNeeded <= SMART_MAX_GRID_SIZE)
	{
		OutChildrenNeeded = ChildrenNeeded;
		return false;  // No adjustment needed
	}
	
	// Adjustment needed - scale down proportionally
	const int32 OriginalChildren = ChildrenNeeded;
	const int32 OriginalTotal = TotalItems;
	ChildrenNeeded = SMART_MAX_GRID_SIZE;
	TotalItems = SMART_MAX_GRID_SIZE + 1;  // +1 for parent
	
	// Adjust grid counters proportionally to reflect the capped size
	const float ScaleFactor = FMath::Sqrt((float)(SMART_MAX_GRID_SIZE + 1) / (float)OriginalTotal);
	XCount = FMath::Max(1, FMath::RoundToInt(XCount * ScaleFactor));
	YCount = FMath::Max(1, FMath::RoundToInt(YCount * ScaleFactor));
	ZCount = FMath::Max(1, FMath::RoundToInt(ZCount * ScaleFactor));
	
	// Update the grid counters (preserve sign for direction)
	GridCounters.X = XCount * FMath::Sign(GridCounters.X);
	GridCounters.Y = YCount * FMath::Sign(GridCounters.Y);
	GridCounters.Z = ZCount * FMath::Sign(GridCounters.Z);
	
	// Recalculate actual children needed with adjusted dimensions
	const int32 AdjustedTotal = XCount * YCount * ZCount;
	OutChildrenNeeded = FMath::Max(0, AdjustedTotal - 1);
	
	// Log the adjustment
	UE_LOG(LogSmartFoundations, Error, 
		TEXT("⚠️ GRID SIZE LIMIT! Requested %dx%dx%d (%d items) exceeds max (%d). Adjusted to %dx%dx%d (%d items)."),
		FMath::Abs(GridCounters.X), FMath::Abs(GridCounters.Y), FMath::Abs(GridCounters.Z), OriginalTotal,
		SMART_MAX_GRID_SIZE + 1,
		XCount, YCount, ZCount, AdjustedTotal);
	
	return true;  // Adjustment was made
}

FSFValidationService::FValidationResult FSFValidationService::ValidateSpacing(
	int32 SpacingValue,
	const AFGHologram* Hologram
) const
{
	// TODO: Extract spacing validation logic
	// Some holograms have spacing constraints (e.g., conveyor belt pole cap)
	
	if (!IsHologramValid(Hologram))
	{
		return FValidationResult::Failure(TEXT("Invalid hologram"));
	}
	
	int32 MaxSpacing = GetMaxSpacingForHologram(Hologram);
	
	if (FMath::Abs(SpacingValue) > MaxSpacing)
	{
		FString ErrorMsg = FString::Printf(
			TEXT("Spacing value %d exceeds maximum %d for this hologram type"),
			SpacingValue,
			MaxSpacing
		);
		return FValidationResult::Failure(ErrorMsg);
	}
	
	return FValidationResult::Success();
}

FSFValidationService::FValidationResult FSFValidationService::ValidateFloorRequirement(
	const AFGHologram* Hologram,
	const FVector& Position,
	UWorld* World
) const
{
	// TODO: Extract floor validation logic
	
	if (!IsHologramValid(Hologram))
	{
		return FValidationResult::Failure(TEXT("Invalid hologram"));
	}
	
	if (!World)
	{
		return FValidationResult::Failure(TEXT("Invalid world context"));
	}
	
	FHitResult HitResult;
	if (!TraceForFloor(Position, World, HitResult))
	{
		return FValidationResult::Failure(TEXT("No valid floor found beneath hologram"));
	}
	
	return FValidationResult::Success();
}

bool FSFValidationService::ShouldEnableFloorValidation(
	AFGHologram* ParentHologram,
	int32 GridZ,
	bool bStepsActive
) const
{
	// Extracted from SFSubsystem.cpp lines 2086-2109 (Phase 1 - Task #61.6)
	// CRITICAL: Toggle floor validation based on elevation
	// Conveyor attachments and Pipeline Junctions require floor contact by default (mNeedsValidFloor=true)
	// When used with Smart!, they fail CheckValidFloor() with "Surface too uneven"
	// This is essential for auto-connect feature where attachments stack in grids
	
	if (!IsHologramValid(ParentHologram))
	{
		return true;  // Default to enabled if we can't check
	}
	
	// CRITICAL: ALWAYS disable floor validation for Conveyor Attachments, ALL Pipe Attachments, Passthroughs, and Power Poles
	// These hologram types have custom floor requirements that conflict with Smart! grid placement
	// Issue #245: AFGPipeAttachmentHologram covers junctions, pumps, and valves
	// Issue #187: AFGPassthroughHologram covers conveyor lift and pipe floor holes
	// Issue #203: AFGPowerPoleHologram - vanilla allows placement inside buildings and floating;
	//            children fail CheckValidFloor() with "Surface too uneven" on splitters/mergers
	const bool bIsConveyorAttachment = ParentHologram->IsA(AFGConveyorAttachmentHologram::StaticClass());
	const bool bIsPipeAttachment = ParentHologram->IsA(AFGPipeAttachmentHologram::StaticClass());
	const bool bIsPassthrough = ParentHologram->IsA(AFGPassthroughHologram::StaticClass());
	const bool bIsPowerPole = ParentHologram->IsA(AFGPowerPoleHologram::StaticClass());
	const bool bIsCeilingLight = ParentHologram->IsA(AFGCeilingLightHologram::StaticClass());
	const bool bIsFloodlight = ParentHologram->IsA(AFGFloodlightHologram::StaticClass());
	const bool bIsWallAttachment = ParentHologram->IsA(AFGWallAttachmentHologram::StaticClass());
	if (bIsConveyorAttachment || bIsPipeAttachment || bIsPassthrough || bIsPowerPole || bIsCeilingLight || bIsFloodlight || bIsWallAttachment)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, 
			TEXT("ValidationService: Floor validation disabled - Hologram type requires it (ConveyorAttachment=%d, PipeAttachment=%d, Passthrough=%d, PowerPole=%d, CeilingLight=%d, Floodlight=%d, WallAttachment=%d)"),
			bIsConveyorAttachment, bIsPipeAttachment, bIsPassthrough, bIsPowerPole, bIsCeilingLight, bIsFloodlight, bIsWallAttachment);
		return false;  // Always disable for these types
	}
	
	// For other hologram types, disable validation if:
	//   1. Child is at Z>0 in grid (vertical stacking)
	//   2. Parent is vertically nudged (all children elevated)
	//   3. Steps mode is active (ANY child can be elevated based on grid position)
	
	const bool bIsGridElevated = (GridZ != 0);
	const float ParentNudgeZ = ParentHologram->GetHologramNudgeOffset().Z;
	const bool bIsParentNudged = (FMath::Abs(ParentNudgeZ) > 0.1f);  // 0.1f threshold (PRD spec)
	const bool bNeedsFloorValidation = !bIsGridElevated && !bIsParentNudged && !bStepsActive;
	
	if (!bNeedsFloorValidation)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, 
			TEXT("ValidationService: Floor validation disabled - GridZ=%d, ParentNudge=%.1f, StepsActive=%d"),
			GridZ, ParentNudgeZ, bStepsActive);
	}
	
	return bNeedsFloorValidation;
}

bool FSFValidationService::IsHologramValid(const AFGHologram* Hologram) const
{
	return Hologram != nullptr && IsValid(Hologram);
}

int32 FSFValidationService::GetMaxSpacingForHologram(const AFGHologram* Hologram) const
{
	// TODO: Extract hologram type detection and constraint mapping
	
	if (!IsHologramValid(Hologram))
	{
		return INT_MAX;
	}
	
	// Check if hologram is a conveyor attachment (belt poles have spacing cap)
	// This will need proper type checking once we extract the logic
	FString HologramClassName = Hologram->GetClass()->GetName();
	if (HologramClassName.Contains(TEXT("Conveyor")))
	{
		return MAX_SPACING_FOR_BELT;
	}
	
	// Default: no spacing limit
	return INT_MAX;
}

bool FSFValidationService::RequiresFloorValidation(const AFGHologram* Hologram) const
{
	// TODO: Extract floor requirement detection logic
	// Foundations and similar buildables require floor validation
	
	if (!IsHologramValid(Hologram))
	{
		return false;
	}
	
	FString HologramClassName = Hologram->GetClass()->GetName();
	
	// Check if it's a foundation hologram
	if (HologramClassName.Contains(TEXT("Foundation")))
	{
		return true;
	}
	
	// Default: no floor validation required
	return false;
}

bool FSFValidationService::TraceForFloor(const FVector& Position, UWorld* World, FHitResult& OutHitResult) const
{
	// TODO: Extract floor tracing logic from SFSubsystem
	
	if (!World)
	{
		return false;
	}
	
	// Trace downward from position to find floor
	FVector TraceStart = Position;
	FVector TraceEnd = Position - FVector(0.0f, 0.0f, 1000.0f); // 10m downward trace
	
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = false;
	
	return World->LineTraceSingleByChannel(
		OutHitResult,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);
}
