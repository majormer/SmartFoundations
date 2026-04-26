// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Centralized Logging Registry

#pragma once

#include "CoreMinimal.h"
#include <atomic>

/**
 * Log categories for Smart! subsystems
 * Used to filter log output at runtime without recompilation
 */
enum class ESFLogCategory : uint8
{
	// Core systems
	AdapterSizing,       // Hologram adapter size detection and fallback logic
	InputEvents,         // Input action/axis events (keypresses, bindings)
	ModeChanges,         // Mode toggle state changes (Spacing/Steps/Stagger activation)
	CounterUpdates,      // Grid counter increments/decrements
	HUDUpdates,          // Widget text updates and display changes
	
	// Grid/Array features
	GridRegeneration,    // Child hologram spawn/destroy during grid resize
	GridPositioning,     // Child transform calculations and updates
	ChildLifecycle,      // Child hologram creation/destruction events
	
	// Build gun integration
	BuildGunState,       // Build gun state changes and hologram validation
	HologramRegistration,// Hologram register/unregister events
	
	// Feature modules
	Arrows,              // Arrow visualization system
	Scaling,             // Scaling feature logic
	Spacing,             // Spacing feature logic
	Steps,               // Steps/stairs feature logic
	Stagger,             // Stagger (lateral offset) feature logic
	
	// Performance & diagnostics
	Performance,         // Frame timing, optimization metrics
	NetworkSync,         // Multiplayer synchronization
	
	MAX_CATEGORIES       // Sentinel value for array sizing
};

/**
 * Verbosity levels for log filtering
 * Maps to both runtime filtering and UE_LOG severity
 */
enum class ESFLogVerbosity : uint8
{
	None = 0,          // Suppress all logs for this category
	Critical = 1,      // Errors, warnings, critical state changes (always show)
	Normal = 2,        // Important informational messages (default)
	Verbose = 3,       // Detailed diagnostic information
	VeryVerbose = 4    // Extremely detailed, high-volume logs (development only)
};

/**
 * FSFLogRegistry - Centralized logging control system
 * 
 * Provides per-category verbosity filtering to reduce log spam while preserving
 * diagnostic capability. Categories can be configured via config file or runtime
 * console commands.
 * 
 * Thread-safe: Uses atomic operations for lock-free reads in hot paths.
 * 
 * Usage:
 *   // In code - use SF_LOG_* macros (see SFLogMacros.h)
 *   SF_LOG_INPUT(Verbose, TEXT("Scale changed: %f"), Value);
 *   
 *   // At runtime - console commands
 *   SF.Log.SetVerbosity InputEvents VeryVerbose
 *   SF.Log.List
 */
class SMARTFOUNDATIONS_API FSFLogRegistry
{
public:
	/**
	 * Check if a log message should be emitted
	 * PERFORMANCE: Inline, lock-free, single atomic read
	 * 
	 * @param Category - Log category
	 * @param MinLevel - Minimum verbosity required to log
	 * @return true if current category verbosity >= MinLevel
	 */
	static inline bool ShouldLog(ESFLogCategory Category, ESFLogVerbosity MinLevel)
	{
		const uint8 CategoryIndex = static_cast<uint8>(Category);
		if (CategoryIndex >= static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES))
		{
			return false; // Invalid category
		}
		
		const uint8 CurrentLevel = CategoryVerbosity[CategoryIndex].load(std::memory_order_relaxed);
		return CurrentLevel >= static_cast<uint8>(MinLevel);
	}
	
	/**
	 * Set verbosity level for a category at runtime
	 * Thread-safe via atomic write
	 * 
	 * @param Category - Category to configure
	 * @param Level - New verbosity level
	 */
	static void SetCategoryVerbosity(ESFLogCategory Category, ESFLogVerbosity Level);
	
	/**
	 * Get current verbosity level for a category
	 * 
	 * @param Category - Category to query
	 * @return Current verbosity level
	 */
	static ESFLogVerbosity GetCategoryVerbosity(ESFLogCategory Category);
	
	/**
	 * Load category verbosity settings from Config/SmartFoundationsLogging.ini
	 * Called automatically during module startup
	 */
	static void LoadFromConfig();
	
	/**
	 * Save current category verbosity settings to config file
	 * Allows persisting runtime changes
	 */
	static void SaveToConfig();
	
	/**
	 * Reset all categories to default values
	 */
	static void ResetToDefaults();
	
	/**
	 * Get human-readable category name for UI/commands
	 * 
	 * @param Category - Category enum value
	 * @return Category name string
	 */
	static FString GetCategoryName(ESFLogCategory Category);
	
	/**
	 * Parse category name from string (case-insensitive)
	 * 
	 * @param Name - Category name string
	 * @param OutCategory - Parsed category (if successful)
	 * @return true if parse succeeded
	 */
	static bool ParseCategoryName(const FString& Name, ESFLogCategory& OutCategory);
	
	/**
	 * Get human-readable verbosity name
	 * 
	 * @param Verbosity - Verbosity enum value
	 * @return Verbosity name string
	 */
	static FString GetVerbosityName(ESFLogVerbosity Verbosity);
	
	/**
	 * Parse verbosity level from string (case-insensitive)
	 * 
	 * @param Name - Verbosity name string
	 * @param OutVerbosity - Parsed verbosity (if successful)
	 * @return true if parse succeeded
	 */
	static bool ParseVerbosityName(const FString& Name, ESFLogVerbosity& OutVerbosity);
	
	/**
	 * Get all category names and current verbosity levels
	 * Used by SF.Log.List console command
	 * 
	 * @return Map of category names to verbosity levels
	 */
	static TMap<FString, ESFLogVerbosity> GetAllCategoryLevels();

private:
	/** Per-category verbosity levels (atomic for lock-free reads) */
	static std::atomic<uint8> CategoryVerbosity[static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES)];
	
	/** Default verbosity per category (applied on reset or missing config) */
	static const ESFLogVerbosity DefaultVerbosity[static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES)];
	
	/** Config file section name */
	static constexpr const TCHAR* ConfigSection = TEXT("/Script/SmartFoundations.SFLogRegistry");
};
