#pragma once

#include "CoreMinimal.h"
#include "Math/IntVector.h"

class AFGHologram;

// REMOVED: Artificial grid size limit
// Previous limit was 700 children to prevent UObject exhaustion
// Root cause: LockHologramPosition() creates massive widget overhead per child
// Solution: Need to make children lightweight (don't lock them, or minimal rendering)
// Removing limit to expose the real performance bottleneck for proper fix
#ifndef SMART_MAX_GRID_SIZE
#define SMART_MAX_GRID_SIZE INT_MAX  // No artificial limit
#endif

/**
 * Smart! Validation Service - Handles placement and grid validation
 * 
 * Extracted from SFSubsystem.cpp (Phase 0 Refactoring - Task #61.6)
 * 
 * Responsibilities:
 * - Validate grid size limits (prevent UObject exhaustion)
 * - Validate hologram placement positions
 * - Validate spacing constraints (e.g., conveyor belt pole cap)
 * - Validate floor requirements for foundations
 * - Check hologram validity and readiness
 * 
 * Dependencies:
 * - Queries hologram state
 * - Checks world collision/tracing
 */
class SMARTFOUNDATIONS_API FSFValidationService
{
public:
	FSFValidationService();
	~FSFValidationService();

	/** Result of a validation check with detailed error information */
	struct FValidationResult
	{
		bool bIsValid;
		FString ErrorMessage;
		
		FValidationResult() : bIsValid(true), ErrorMessage(TEXT("")) {}
		FValidationResult(bool bValid, const FString& Error = TEXT(""))
			: bIsValid(bValid), ErrorMessage(Error) {}
		
		static FValidationResult Success() { return FValidationResult(true); }
		static FValidationResult Failure(const FString& Reason) { return FValidationResult(false, Reason); }
	};

	/**
	 * Validate if hologram can be placed at given position
	 * 
	 * @param Hologram Hologram to validate
	 * @param Position World position to check
	 * @param Rotation World rotation
	 * @param World World context for tracing
	 * @return Validation result with error details if invalid
	 */
	FValidationResult ValidatePlacement(
		const AFGHologram* Hologram,
		const FVector& Position,
		const FRotator& Rotation,
		UWorld* World
	) const;

	/**
	 * Validate grid size against SMART_MAX_GRID_SIZE limit
	 * Prevents UObject exhaustion crashes
	 * 
	 * @param GridCounters Grid dimensions (X * Y * Z must be <= SMART_MAX_GRID_SIZE)
	 * @return Validation result with size limit details if invalid
	 */
	FValidationResult ValidateGridSize(const FIntVector& GridCounters) const;

	/**
	 * Validate and adjust grid size if it exceeds SMART_MAX_GRID_SIZE
	 * Scales grid proportionally to fit within limit
	 * 
	 * @param GridCounters Input/Output grid dimensions (will be modified if too large)
	 * @param OutChildrenNeeded Output number of children needed after adjustment
	 * @return true if adjustment was needed, false if size was valid
	 */
	bool ValidateAndAdjustGridSize(FIntVector& GridCounters, int32& OutChildrenNeeded) const;

	/**
	 * Validate spacing value for specific hologram type
	 * Some holograms (e.g., conveyor attachments) have spacing constraints
	 * 
	 * @param SpacingValue Spacing counter value
	 * @param Hologram Hologram to validate spacing for
	 * @return Validation result with constraint details if invalid
	 */
	FValidationResult ValidateSpacing(int32 SpacingValue, const AFGHologram* Hologram) const;

	/**
	 * Validate if hologram requires floor beneath and has valid floor
	 * Specific to foundations and similar buildables
	 * 
	 * @param Hologram Foundation hologram to check
	 * @param Position World position to check floor at
	 * @param World World context for tracing
	 * @return Validation result with floor requirement details
	 */
	FValidationResult ValidateFloorRequirement(
		const AFGHologram* Hologram,
		const FVector& Position,
		UWorld* World
	) const;

	/**
	 * Determine if floor validation should be enabled for a child hologram
	 * Coordinates with native nudge and Steps feature for elevated children
	 * 
	 * @param ParentHologram Parent hologram to check nudge state
	 * @param GridZ Z-index in grid (0 = ground level, >0 = elevated)
	 * @param bStepsActive Whether Steps mode is active
	 * @return true if floor validation should be enforced
	 */
	bool ShouldEnableFloorValidation(
		AFGHologram* ParentHologram,
		int32 GridZ,
		bool bStepsActive
	) const;

	/**
	 * Check if hologram is valid and ready for Smart! operations
	 * 
	 * @param Hologram Hologram to check
	 * @return true if hologram is valid, non-null, and not pending kill
	 */
	bool IsHologramValid(const AFGHologram* Hologram) const;

	/**
	 * Get the maximum spacing value for a specific hologram type
	 * Some buildables have spacing caps (e.g., conveyor belt pole limit)
	 * 
	 * @param Hologram Hologram to get limit for
	 * @return Maximum allowed spacing value, or INT_MAX if unlimited
	 */
	int32 GetMaxSpacingForHologram(const AFGHologram* Hologram) const;

private:
	/**
	 * Check if hologram type requires floor validation
	 * 
	 * @param Hologram Hologram to check
	 * @return true if floor validation should be enforced
	 */
	bool RequiresFloorValidation(const AFGHologram* Hologram) const;

	/**
	 * Trace downward from position to find floor
	 * 
	 * @param Position Starting position
	 * @param World World context
	 * @param OutHitResult Hit result if floor found
	 * @return true if valid floor detected
	 */
	bool TraceForFloor(const FVector& Position, UWorld* World, FHitResult& OutHitResult) const;
};
