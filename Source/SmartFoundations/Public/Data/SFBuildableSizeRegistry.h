#pragma once

#include "CoreMinimal.h"
#include "SFBuildableSizeProfile.h"

// Forward declarations
class AFGBuildableHologram;
class AFGBuildable;

/**
 * Central registry for buildable size profiles
 * 
 * Provides default dimensions for vanilla buildables and fallback sizing for modded content.
 * All sizes are stored in Unreal Units (centimeters) for performance and precision.
 * 
 * Usage:
 *   FVector ItemSize = USFBuildableSizeRegistry::GetSizeForHologram(Hologram);
 */
class SMARTFOUNDATIONS_API USFBuildableSizeRegistry
{
public:
	/** Initialize the registry with known buildable profiles (call once at startup) */
	static void Initialize();
	
	/**
	 * Get size profile for a buildable class
	 * Returns fallback profile if buildable is unknown
	 */
	static FSFBuildableSizeProfile GetProfile(const UClass* BuildableClass);
	
	/**
	 * Get size for a hologram, accounting for rotation
	 * This is the primary method to use for spacing calculations
	 */
	static FVector GetSizeForHologram(const AFGBuildableHologram* Hologram);
	
	/**
	 * Check if we have a validated profile for this buildable
	 * Returns false for unknown/modded buildables using fallback
	 */
	static bool HasProfile(const UClass* BuildableClass);
	
	/**
	 * Get profile by class name string
	 * Useful when you only have the class name
	 */
	static FSFBuildableSizeProfile GetProfileByName(const FString& ClassName);
	
	/**
	 * Get the unit height for ramp stepping (handles single and double ramps)
	 * Returns 0.0f for non-ramp buildables
	 *
	 * For single ramps: returns full Z height (e.g., 400cm for 8x1)
	 * For double ramps: returns half Z height (e.g., 400cm for 8x2 double which is 800cm total)
	 */
	static float GetRampUnitHeight(const FSFBuildableSizeProfile& Profile);

	// ========================================================================
	// CDO Query Functions (for unknown/modded buildables)
	// ========================================================================

	/**
	 * Try to get buildable size from clearance box (CDO query)
	 * This is what Satisfactory uses for snapping visualization.
	 *
	 * @param BuildClass - The buildable class to query
	 * @param OutSize - Receives the size if clearance data is valid
	 * @return true if clearance box was successfully queried
	 */
	static bool TryGetSizeFromClearanceBox(UClass* BuildClass, FVector& OutSize);

	/**
	 * Try to get mesh bounds from the buildable's visual representation (CDO query)
	 *
	 * @param BuildClass - The buildable class to query
	 * @param OutSize - Receives the size if mesh bounds are valid
	 * @return true if mesh bounds were successfully queried
	 */
	static bool TryGetSizeFromMeshBounds(UClass* BuildClass, FVector& OutSize);

	/**
	 * Get buildable size with automatic fallback strategy:
	 * 1. Try registry profiles (most accurate)
	 * 2. Try clearance box + mesh bounds (use maximum to prevent visual overlaps)
	 * 3. Use default fallback (800x800x400)
	 *
	 * @param BuildClass - The UClass of the buildable
	 * @param OutSize - Receives the size
	 * @param OutSource - Receives description of which method succeeded
	 * @return true if size was determined from registry or CDO queries
	 */
	static bool GetSizeWithFallback(UClass* BuildClass, FVector& OutSize, FString& OutSource);

	/**
	 * Get default fallback size for unknown buildables
	 * @return 800x800x400 (standard 8m foundation)
	 */
	static FVector GetDefaultSize();

private:
	/** Helper to register a profile with readability */
	static void RegisterProfile(
		const FString& ClassName,
		const FVector& Size,
		bool bSwapOnRotation = false,
		bool bSupportsScaling = true,
		const FString& Inheritance = TEXT(""),
		bool bValidated = true,
		const FVector& AnchorOffset = FVector::ZeroVector
	);
	
	/** Populate registry with all known vanilla buildable profiles */
	static void RegisterDefaultProfiles();
	
	/** Category-specific registration functions */
	static void RegisterFoundations();  // Implemented in SFBuildableSizeRegistry_Foundations.cpp
	static void RegisterRamps();  // Implemented in SFBuildableSizeRegistry_Ramps.cpp
	static void RegisterWalls();  // Implemented in SFBuildableSizeRegistry_Walls.cpp
	static void RegisterWalkways();  // Implemented in SFBuildableSizeRegistry_Walkways.cpp
	static void RegisterBarriers();  // Implemented in SFBuildableSizeRegistry_Barriers.cpp
	static void RegisterArchitectureDisabled();  // Implemented in SFBuildableSizeRegistry_Architecture_Disabled.cpp
	static void RegisterStorage();  // Implemented in SFBuildableSizeRegistry_Storage.cpp
	static void RegisterOrganization();  // Implemented in SFBuildableSizeRegistry_Organization.cpp
	static void RegisterProduction();  // Implemented in SFBuildableSizeRegistry_Production.cpp
	static void RegisterExtractors();  // Implemented in SFBuildableSizeRegistry_Extractors.cpp
	static void RegisterSpecial();  // Implemented in SFBuildableSizeRegistry_Special.cpp
	static void RegisterPower();  // Implemented in SFBuildableSizeRegistry_Power.cpp
	static void RegisterLogistics();  // Implemented in SFBuildableSizeRegistry_Logistics.cpp
	static void RegisterTransport();  // Implemented in SFBuildableSizeRegistry_Transport.cpp
	static void RegisterArchitecture();
	static void RegisterStructures();
	static void RegisterFactory();
	static void RegisterExtraction();
	
	/** Check if hologram is rotated 90° or 270° (for X/Y swapping) */
	static bool IsRotated90Degrees(const FRotator& Rotation);

	/**
	 * Resolve style variant class name to its base profile name.
	 * Handles patterns like:
	 * - "Build_Foundation_Asphalt_8x4_C" → "Build_Foundation_8x4_01_C"
	 * - "Build_SteelWall_8x4_Window_01_C" → "Build_Wall_Window_8x4_01_C"
	 * - "Build_Stair_Concrete_8x1_C" → "Build_Stair_FicsitSet_8x1_01_C"
	 *
	 * @param ClassName - The variant class name to resolve
	 * @param OutBaseName - Receives the base profile name if found
	 * @return true if a base name was resolved
	 */
	static bool ResolveVariantBaseName(const FString& ClassName, FString& OutBaseName);

	/** Style names used for variant inheritance fallback */
	static const TArray<FString> StyleNames;

	/** Known buildable profiles */
	static TMap<FString, FSFBuildableSizeProfile> KnownProfiles;
	
	/** Fallback profile for unknown/modded buildables (conservative 800x800x400cm foundation size) */
	static FSFBuildableSizeProfile FallbackProfile;
	
	/** Initialization flag */
	static bool bIsInitialized;
};
