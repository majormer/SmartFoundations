#pragma once

#include "CoreMinimal.h"

/**
 * Size profile for a buildable, used to determine default spacing when scaling
 * 
 * All sizes are in Unreal Units (centimeters)
 * Rotation-aware: bSwapXYOnRotation handles asymmetric buildings that change dimensions when rotated
 */
struct FSFBuildableSizeProfile
{
	/** Buildable class name (e.g., "Build_Foundation_8x4_01_C") */
	FString BuildableClassName;
	
	/** Default size at 0° rotation: (Width=X, Depth=Y, Height=Z) in centimeters */
	FVector DefaultSize;
	
	/** If true, swap X and Y when rotated 90° (for asymmetric buildings) */
	bool bSwapXYOnRotation;

	/** Origin offset if needed (usually zero) */
	FVector AnchorOffset;

	/** Whether Smart! scaling is permitted for this buildable */
	bool bSupportsScaling;

	/** True if manually validated in-game for visual accuracy */
	bool bIsValidated;

	/** Optional: Hologram inheritance chain for reference */
	FString HologramInheritance;

	/** Source file where this profile was registered (for debugging) */
	FString SourceFile;
	
	FSFBuildableSizeProfile()
		: BuildableClassName(TEXT(""))
		, DefaultSize(FVector::ZeroVector)
		, bSwapXYOnRotation(false)
		, AnchorOffset(FVector::ZeroVector)
		, bSupportsScaling(true)
		, bIsValidated(false)
		, HologramInheritance(TEXT(""))
		, SourceFile(TEXT(""))
	{
	}
	
	FSFBuildableSizeProfile(
		const FString& InClassName,
		const FVector& InSize,
		bool bInSwapOnRotation = false,
		const FVector& InAnchorOffset = FVector::ZeroVector,
		bool bInSupportsScaling = true,
		bool bInValidated = false,
		const FString& InInheritance = TEXT(""),
		const FString& InSourceFile = TEXT("")
	)
		: BuildableClassName(InClassName)
		, DefaultSize(InSize)
		, bSwapXYOnRotation(bInSwapOnRotation)
		, AnchorOffset(InAnchorOffset)
		, bSupportsScaling(bInSupportsScaling)
		, bIsValidated(bInValidated)
		, HologramInheritance(InInheritance)
		, SourceFile(InSourceFile)
	{
	}
};
