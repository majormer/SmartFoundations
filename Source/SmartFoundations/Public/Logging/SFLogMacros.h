// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Centralized Logging Macros

#pragma once

#include "SFLogRegistry.h"
#include "SmartFoundations.h"

/**
 * SF_LOG Macro System
 * 
 * These macros provide category-based filtering with zero-cost when disabled.
 * Arguments are only evaluated if the category verbosity check passes.
 * 
 * Usage:
 *   SF_LOG_INPUT(Verbose, TEXT("Scale value: %f"), Value);
 *   SF_LOG_ADAPTER(VeryVerbose, TEXT("Size: %s"), *Size.ToString());
 * 
 * Verbosity Levels:
 *   - Critical: Errors/warnings (always shown at default config)
 *   - Normal: Important state changes
 *   - Verbose: Detailed diagnostics
 *   - VeryVerbose: Extremely detailed (development only)
 */

// Base macro that maps verbosity to UE_LOG severity
#define SF_LOG_BASE(Category, Verbosity, UEVerbosity, Format, ...) \
	do { \
		if (FSFLogRegistry::ShouldLog(ESFLogCategory::Category, ESFLogVerbosity::Verbosity)) \
		{ \
			UE_LOG(LogSmartFoundations, UEVerbosity, Format, ##__VA_ARGS__); \
		} \
	} while(0)

// ============================================================================
// Core Systems
// ============================================================================

/** Adapter size detection logs (VERY NOISY - off by default) */
#define SF_LOG_ADAPTER(Verbosity, Format, ...) \
	SF_LOG_BASE(AdapterSizing, Verbosity, Log, Format, ##__VA_ARGS__)

/** Input event logs (keypresses, axis values) */
#define SF_LOG_INPUT(Verbosity, Format, ...) \
	SF_LOG_BASE(InputEvents, Verbosity, Log, Format, ##__VA_ARGS__)

/** Mode toggle logs (Spacing/Steps/Stagger activation) */
#define SF_LOG_MODE(Verbosity, Format, ...) \
	SF_LOG_BASE(ModeChanges, Verbosity, Log, Format, ##__VA_ARGS__)

/** Counter increment/decrement logs */
#define SF_LOG_COUNTER(Verbosity, Format, ...) \
	SF_LOG_BASE(CounterUpdates, Verbosity, Log, Format, ##__VA_ARGS__)

/** HUD/Widget update logs */
#define SF_LOG_HUD(Verbosity, Format, ...) \
	SF_LOG_BASE(HUDUpdates, Verbosity, Log, Format, ##__VA_ARGS__)

// ============================================================================
// Grid/Array Features
// ============================================================================

/** Grid regeneration logs (child spawn/destroy) */
#define SF_LOG_GRID(Verbosity, Format, ...) \
	SF_LOG_BASE(GridRegeneration, Verbosity, Log, Format, ##__VA_ARGS__)

/** Grid positioning logs (transform calculations) */
#define SF_LOG_GRIDPOS(Verbosity, Format, ...) \
	SF_LOG_BASE(GridPositioning, Verbosity, Log, Format, ##__VA_ARGS__)

/** Child hologram lifecycle logs */
#define SF_LOG_CHILD(Verbosity, Format, ...) \
	SF_LOG_BASE(ChildLifecycle, Verbosity, Log, Format, ##__VA_ARGS__)

// ============================================================================
// Build Gun Integration
// ============================================================================

/** Build gun state logs */
#define SF_LOG_BUILDGUN(Verbosity, Format, ...) \
	SF_LOG_BASE(BuildGunState, Verbosity, Log, Format, ##__VA_ARGS__)

/** Hologram registration logs */
#define SF_LOG_HOLOGRAM(Verbosity, Format, ...) \
	SF_LOG_BASE(HologramRegistration, Verbosity, Log, Format, ##__VA_ARGS__)

// ============================================================================
// Feature Modules
// ============================================================================

/** Arrow system logs */
#define SF_LOG_ARROWS(Verbosity, Format, ...) \
	SF_LOG_BASE(Arrows, Verbosity, Log, Format, ##__VA_ARGS__)

/** Scaling feature logs */
#define SF_LOG_SCALING(Verbosity, Format, ...) \
	SF_LOG_BASE(Scaling, Verbosity, Log, Format, ##__VA_ARGS__)

/** Spacing feature logs */
#define SF_LOG_SPACING(Verbosity, Format, ...) \
	SF_LOG_BASE(Spacing, Verbosity, Log, Format, ##__VA_ARGS__)

/** Steps feature logs */
#define SF_LOG_STEPS(Verbosity, Format, ...) \
	SF_LOG_BASE(Steps, Verbosity, Log, Format, ##__VA_ARGS__)

/** Stagger feature logs (lateral grid offset) */
#define SF_LOG_STAGGER(Verbosity, Format, ...) \
	SF_LOG_BASE(Stagger, Verbosity, Log, Format, ##__VA_ARGS__)

// ============================================================================
// Performance & Diagnostics
// ============================================================================

/** Performance measurement logs */
#define SF_LOG_PERF(Verbosity, Format, ...) \
	SF_LOG_BASE(Performance, Verbosity, Log, Format, ##__VA_ARGS__)

/** Network synchronization logs */
#define SF_LOG_NET(Verbosity, Format, ...) \
	SF_LOG_BASE(NetworkSync, Verbosity, Log, Format, ##__VA_ARGS__)

// ============================================================================
// Special Macros (Severity Mapping)
// ============================================================================

/** Critical error log (always shown, maps to UE Warning) */
#define SF_LOG_ERROR(Category, Format, ...) \
	SF_LOG_BASE(Category, Critical, Warning, TEXT("❌ ") Format, ##__VA_ARGS__)

/** Critical warning log (always shown, maps to UE Warning) */
#define SF_LOG_WARNING(Category, Format, ...) \
	SF_LOG_BASE(Category, Critical, Warning, TEXT("⚠️ ") Format, ##__VA_ARGS__)
