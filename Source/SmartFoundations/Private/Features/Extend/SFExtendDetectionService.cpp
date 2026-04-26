// Copyright Coffee Stain Studios. All Rights Reserved.

#include "Features/Extend/SFExtendDetectionService.h"
#include "SmartFoundations.h"  // For LogSmartFoundations
#include "Subsystem/SFSubsystem.h"
#include "FGBuildable.h"
#include "FGBuildableFactory.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Hologram/FGHologram.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGCharacterPlayer.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableResourceExtractorBase.h"
#include "Buildables/FGBuildableWaterPump.h"
#include "Buildables/FGBuildableManufacturer.h"

USFExtendDetectionService::USFExtendDetectionService()
{
}

void USFExtendDetectionService::Initialize(USFSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFExtendDetectionService initialized"));
}

void USFExtendDetectionService::Shutdown()
{
    Subsystem.Reset();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFExtendDetectionService shutdown"));
}

// ==================== Target Detection ====================

bool USFExtendDetectionService::IsValidExtendTarget(AFGBuildable* Building) const
{
    if (!IsValid(Building))
    {
        return false;
    }

    // EXCEPTION: Allow FICSMAS Gift Trees (Build_TreeGiftProducer_C)
    // Check this FIRST to ensure it passes regardless of class hierarchy
    if (Building->GetName().Contains(TEXT("TreeGiftProducer")))
    {
         return true;
    }

    // Explicitly reject all resource extractors (miners, oil extractors, resource well extractors, water extractors)
    // NOTE: We may revisit re-enabling water extractors (AFGBuildableWaterPump) in the future.
    if (Building->IsA(AFGBuildableResourceExtractorBase::StaticClass()))
    {
        return false;
    }

    // Allow power generators/plants (these often have power connections, not belt/pipe connectors)
    if (Building->IsA(AFGBuildableGenerator::StaticClass()))
    {
        return true;
    }

    // Must be a factory building (production machine).
    // NOTE: Intentionally excludes logistics (splitters/mergers/belts/pipes) as EXTEND targets.
    return Building->IsA(AFGBuildableManufacturer::StaticClass());
}

bool USFExtendDetectionService::DoesTypeMatch(AFGBuildable* Building, AFGHologram* Hologram) const
{
    if (!IsValid(Building) || !IsValid(Hologram))
    {
        return false;
    }

    UClass* HologramBuildClass = Hologram->GetBuildClass();
    if (!HologramBuildClass)
    {
        return false;
    }

    return Building->IsA(HologramBuildClass);
}

// ==================== Direction Management ====================

void USFExtendDetectionService::CycleExtendDirection(int32 Delta)
{
    // Only two directions, so any non-zero delta toggles
    if (Delta != 0)
    {
        CurrentDirection = (CurrentDirection == ESFExtendDirection::Right) 
            ? ESFExtendDirection::Left 
            : ESFExtendDirection::Right;
        
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Direction cycled to %s"),
            CurrentDirection == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
    }
}

FVector USFExtendDetectionService::GetDirectionOffset(const FVector& BuildingSize, const FRotator& BuildingRotation) const
{
    return GetDirectionOffsetForDirection(CurrentDirection, BuildingSize, BuildingRotation);
}

FVector USFExtendDetectionService::GetDirectionOffsetForDirection(ESFExtendDirection Direction, const FVector& BuildingSize, const FRotator& BuildingRotation) const
{
    // Get the right vector of the building (perpendicular to forward)
    FVector RightVector = BuildingRotation.RotateVector(FVector::RightVector);
    
    // Use Y dimension (width) for perpendicular offset
    float OffsetDistance = BuildingSize.Y;
    
    // Apply direction
    if (Direction == ESFExtendDirection::Left)
    {
        return -RightVector * OffsetDistance;
    }
    else // Right
    {
        return RightVector * OffsetDistance;
    }
}

bool USFExtendDetectionService::IsDirectionValid(ESFExtendDirection Direction, AFGBuildable* TargetBuilding) const
{
    if (!IsValid(TargetBuilding))
    {
        return false;
    }

    // Get building size from registry
    USFBuildableSizeRegistry::Initialize();
    FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(TargetBuilding->GetClass());
    FVector BuildingSize = Profile.DefaultSize;

    // Calculate target position
    FVector SourceLocation = TargetBuilding->GetActorLocation();
    FRotator SourceRotation = TargetBuilding->GetActorRotation();
    FVector Offset = GetDirectionOffsetForDirection(Direction, BuildingSize, SourceRotation);
    FVector TargetLocation = SourceLocation + Offset;

    // Check for overlapping buildings at target location
    UWorld* World = TargetBuilding->GetWorld();
    if (!World)
    {
        return false;
    }

    // Use a box overlap check with building dimensions
    FVector HalfExtent = BuildingSize * 0.4f; // Slightly smaller to allow some tolerance
    
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(TargetBuilding);
    
    bool bHasOverlap = World->OverlapMultiByChannel(
        Overlaps,
        TargetLocation,
        SourceRotation.Quaternion(),
        ECC_WorldStatic,
        FCollisionShape::MakeBox(HalfExtent),
        QueryParams
    );

    if (bHasOverlap)
    {
        // Check if any overlaps are buildables (not just terrain)
        for (const FOverlapResult& Overlap : Overlaps)
        {
            if (AFGBuildable* OverlappingBuildable = Cast<AFGBuildable>(Overlap.GetActor()))
            {
                // Found a blocking buildable
                return false;
            }
        }
    }

    return true;
}

TArray<ESFExtendDirection> USFExtendDetectionService::GetValidDirections(AFGBuildable* TargetBuilding) const
{
    TArray<ESFExtendDirection> ValidDirections;

    if (IsDirectionValid(ESFExtendDirection::Right, TargetBuilding))
    {
        ValidDirections.Add(ESFExtendDirection::Right);
    }

    if (IsDirectionValid(ESFExtendDirection::Left, TargetBuilding))
    {
        ValidDirections.Add(ESFExtendDirection::Left);
    }

    return ValidDirections;
}

bool USFExtendDetectionService::AutoSelectValidDirection(AFGBuildable* TargetBuilding)
{
    TArray<ESFExtendDirection> ValidDirs = GetValidDirections(TargetBuilding);
    
    if (ValidDirs.Num() == 0)
    {
        return false;
    }

    // If current direction is valid, keep it
    if (ValidDirs.Contains(CurrentDirection))
    {
        return true;
    }

    // Otherwise, switch to first valid direction
    CurrentDirection = ValidDirs[0];
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔄 EXTEND: Auto-selected direction %s (other side blocked)"),
        CurrentDirection == ESFExtendDirection::Right ? TEXT("Right") : TEXT("Left"));
    
    return true;
}

// ==================== Build Gun Access ====================

AFGBuildGun* USFExtendDetectionService::GetPlayerBuildGun() const
{
    if (!Subsystem.IsValid())
    {
        return nullptr;
    }

    UWorld* World = Subsystem->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    // Get the local player controller
    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC)
    {
        return nullptr;
    }

    AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
    if (!Character)
    {
        return nullptr;
    }

    return Character->GetBuildGun();
}

UFGBuildGunStateBuild* USFExtendDetectionService::GetBuildGunBuildState(AFGBuildGun* BuildGun) const
{
    if (!BuildGun)
    {
        return nullptr;
    }

    return Cast<UFGBuildGunStateBuild>(BuildGun->GetCurrentState());
}
