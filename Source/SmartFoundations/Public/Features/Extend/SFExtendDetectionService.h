// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendDetectionService - Target Detection and Direction Management
 * 
 * Handles:
 * - Detecting valid EXTEND targets (factory buildings of matching type)
 * - Direction selection (Left/Right perpendicular to belt flow)
 * - Collision checking for valid placement directions
 * 
 * Part of EXTEND feature refactor (Dec 2025).
 * Extracted from SFExtendService for separation of concerns.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SFExtendDetectionService.generated.h"

class AFGBuildable;
class AFGBuildableFactory;
class AFGHologram;
class AFGBuildGun;
class UFGBuildGunStateBuild;
class ASFFactoryHologram;
class USFSubsystem;

/**
 * Direction for EXTEND cloning (perpendicular to belt/pipe direction)
 * 
 * Original Smart only supported Left/Right to maintain manifold alignment.
 * Forward/Backward would block input/output connectors.
 */
UENUM(BlueprintType)
enum class ESFExtendDirection : uint8
{
    Right       UMETA(DisplayName = "Right"),
    Left        UMETA(DisplayName = "Left")
};

/**
 * Service for detecting valid EXTEND targets and managing direction selection.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendDetectionService : public UObject
{
    GENERATED_BODY()

public:
    USFExtendDetectionService();

    /** Initialize with owning subsystem reference */
    void Initialize(USFSubsystem* InSubsystem);

    /** Shutdown service */
    void Shutdown();

    // ==================== Target Detection ====================

    /** Check if a building is a valid factory for extension */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Detection")
    bool IsValidExtendTarget(AFGBuildable* Building) const;

    /** Check if building type matches hologram build class */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Detection")
    bool DoesTypeMatch(AFGBuildable* Building, AFGHologram* Hologram) const;

    // ==================== Direction Management ====================

    /** Get current extend direction */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    ESFExtendDirection GetExtendDirection() const { return CurrentDirection; }

    /** Set extend direction directly */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    void SetExtendDirection(ESFExtendDirection NewDirection) { CurrentDirection = NewDirection; }

    /** Cycle to next/previous direction (called from mouse wheel) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    void CycleExtendDirection(int32 Delta);

    /** Get offset vector for current direction based on building size */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    FVector GetDirectionOffset(const FVector& BuildingSize, const FRotator& BuildingRotation) const;

    /** Get offset vector for a specific direction */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    FVector GetDirectionOffsetForDirection(ESFExtendDirection Direction, const FVector& BuildingSize, const FRotator& BuildingRotation) const;

    /** Check if a specific direction is valid (no building blocking the target position) */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    bool IsDirectionValid(ESFExtendDirection Direction, AFGBuildable* TargetBuilding) const;

    /** Get the list of valid directions for a target building */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    TArray<ESFExtendDirection> GetValidDirections(AFGBuildable* TargetBuilding) const;

    /** Auto-select a valid direction if current is blocked */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Direction")
    bool AutoSelectValidDirection(AFGBuildable* TargetBuilding);

    // ==================== Build Gun Access ====================

    /** Get the player's build gun */
    AFGBuildGun* GetPlayerBuildGun() const;

    /** Get the build gun's build state */
    UFGBuildGunStateBuild* GetBuildGunBuildState(AFGBuildGun* BuildGun) const;

private:
    /** Owning subsystem */
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Current extension direction (Right or Left - perpendicular to belt flow) */
    UPROPERTY()
    ESFExtendDirection CurrentDirection = ESFExtendDirection::Right;
};
