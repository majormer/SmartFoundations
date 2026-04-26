// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Centralized Logging Registry Implementation

#include "SFLogRegistry.h"
#include "Misc/ConfigCacheIni.h"

// Define static members
std::atomic<uint8> FSFLogRegistry::CategoryVerbosity[static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES)];

// Default verbosity levels (conservative - reduce spam in normal operation)
const ESFLogVerbosity FSFLogRegistry::DefaultVerbosity[static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES)] = {
	// Core systems - reduce spam
	ESFLogVerbosity::None,      // AdapterSizing - VERY noisy, off by default
	ESFLogVerbosity::Critical,  // InputEvents - only show errors
	ESFLogVerbosity::Normal,    // ModeChanges - important state changes
	ESFLogVerbosity::Critical,  // CounterUpdates - only show errors
	ESFLogVerbosity::Critical,  // HUDUpdates - only show errors
	
	// Grid/Array features
	ESFLogVerbosity::Normal,    // GridRegeneration - important for debugging grids
	ESFLogVerbosity::None,      // GridPositioning - very noisy, off by default
	ESFLogVerbosity::Normal,    // ChildLifecycle - important lifecycle events
	
	// Build gun integration
	ESFLogVerbosity::Normal,    // BuildGunState - important state tracking
	ESFLogVerbosity::Normal,    // HologramRegistration - important lifecycle
	
	// Feature modules
	ESFLogVerbosity::Critical,  // Arrows - only errors
	ESFLogVerbosity::Normal,    // Scaling - important feature
	ESFLogVerbosity::Normal,    // Spacing - important feature
	ESFLogVerbosity::Normal,    // Steps - important feature
	ESFLogVerbosity::Normal,    // Stagger - important feature
	
	// Performance & diagnostics
	ESFLogVerbosity::Verbose,   // Performance - detailed when needed
	ESFLogVerbosity::Normal     // NetworkSync - multiplayer issues
};

void FSFLogRegistry::SetCategoryVerbosity(ESFLogCategory Category, ESFLogVerbosity Level)
{
	const uint8 CategoryIndex = static_cast<uint8>(Category);
	if (CategoryIndex >= static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES))
	{
		UE_LOG(LogTemp, Warning, TEXT("FSFLogRegistry::SetCategoryVerbosity: Invalid category index %d"), CategoryIndex);
		return;
	}
	
	CategoryVerbosity[CategoryIndex].store(static_cast<uint8>(Level), std::memory_order_relaxed);
}

ESFLogVerbosity FSFLogRegistry::GetCategoryVerbosity(ESFLogCategory Category)
{
	const uint8 CategoryIndex = static_cast<uint8>(Category);
	if (CategoryIndex >= static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES))
	{
		return ESFLogVerbosity::None;
	}
	
	return static_cast<ESFLogVerbosity>(CategoryVerbosity[CategoryIndex].load(std::memory_order_relaxed));
}

void FSFLogRegistry::LoadFromConfig()
{
	// Reset to defaults first
	ResetToDefaults();
	
	// Try to load from config file
	if (!GConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("FSFLogRegistry::LoadFromConfig: GConfig not available, using defaults"));
		return;
	}
	
	const FString ConfigFile = GGameIni; // or GEngineIni depending on preference
	
	// Load each category from config
	for (uint8 i = 0; i < static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES); ++i)
	{
		const ESFLogCategory Cat = static_cast<ESFLogCategory>(i);
		const FString CategoryName = GetCategoryName(Cat);
		const FString Key = FString::Printf(TEXT("Category_%s"), *CategoryName);
		
		FString VerbosityStr;
		if (GConfig->GetString(ConfigSection, *Key, VerbosityStr, ConfigFile))
		{
			ESFLogVerbosity ParsedVerbosity;
			if (ParseVerbosityName(VerbosityStr, ParsedVerbosity))
			{
				SetCategoryVerbosity(Cat, ParsedVerbosity);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("FSFLogRegistry: Invalid verbosity '%s' for category '%s', using default"),
					*VerbosityStr, *CategoryName);
			}
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("FSFLogRegistry: Loaded logging configuration from %s"), *ConfigFile);
}

void FSFLogRegistry::SaveToConfig()
{
	if (!GConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("FSFLogRegistry::SaveToConfig: GConfig not available"));
		return;
	}
	
	const FString ConfigFile = GGameIni;
	
	// Save each category to config
	for (uint8 i = 0; i < static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES); ++i)
	{
		const ESFLogCategory Cat = static_cast<ESFLogCategory>(i);
		const FString CategoryName = GetCategoryName(Cat);
		const FString Key = FString::Printf(TEXT("Category_%s"), *CategoryName);
		const ESFLogVerbosity CurrentLevel = GetCategoryVerbosity(Cat);
		const FString VerbosityStr = GetVerbosityName(CurrentLevel);
		
		GConfig->SetString(ConfigSection, *Key, *VerbosityStr, ConfigFile);
	}
	
	GConfig->Flush(false, ConfigFile);
	UE_LOG(LogTemp, Log, TEXT("FSFLogRegistry: Saved logging configuration to %s"), *ConfigFile);
}

void FSFLogRegistry::ResetToDefaults()
{
	for (uint8 i = 0; i < static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES); ++i)
	{
		CategoryVerbosity[i].store(static_cast<uint8>(DefaultVerbosity[i]), std::memory_order_relaxed);
	}
}

FString FSFLogRegistry::GetCategoryName(ESFLogCategory Category)
{
	switch (Category)
	{
		// Core systems
		case ESFLogCategory::AdapterSizing:       return TEXT("AdapterSizing");
		case ESFLogCategory::InputEvents:         return TEXT("InputEvents");
		case ESFLogCategory::ModeChanges:         return TEXT("ModeChanges");
		case ESFLogCategory::CounterUpdates:      return TEXT("CounterUpdates");
		case ESFLogCategory::HUDUpdates:          return TEXT("HUDUpdates");
		
		// Grid/Array features
		case ESFLogCategory::GridRegeneration:    return TEXT("GridRegeneration");
		case ESFLogCategory::GridPositioning:     return TEXT("GridPositioning");
		case ESFLogCategory::ChildLifecycle:      return TEXT("ChildLifecycle");
		
		// Build gun integration
		case ESFLogCategory::BuildGunState:       return TEXT("BuildGunState");
		case ESFLogCategory::HologramRegistration:return TEXT("HologramRegistration");
		
		// Feature modules
		case ESFLogCategory::Arrows:              return TEXT("Arrows");
		case ESFLogCategory::Scaling:             return TEXT("Scaling");
		case ESFLogCategory::Spacing:             return TEXT("Spacing");
		case ESFLogCategory::Steps:               return TEXT("Steps");
		case ESFLogCategory::Stagger:             return TEXT("Stagger");
		
		// Performance & diagnostics
		case ESFLogCategory::Performance:         return TEXT("Performance");
		case ESFLogCategory::NetworkSync:         return TEXT("NetworkSync");
		
		case ESFLogCategory::MAX_CATEGORIES:
		default:
			return TEXT("Unknown");
	}
}

bool FSFLogRegistry::ParseCategoryName(const FString& Name, ESFLogCategory& OutCategory)
{
	// Case-insensitive comparison
	const FString LowerName = Name.ToLower();
	
	// Core systems
	if (LowerName == TEXT("adaptersizing"))       { OutCategory = ESFLogCategory::AdapterSizing; return true; }
	if (LowerName == TEXT("inputevents"))         { OutCategory = ESFLogCategory::InputEvents; return true; }
	if (LowerName == TEXT("modechanges"))         { OutCategory = ESFLogCategory::ModeChanges; return true; }
	if (LowerName == TEXT("counterupdates"))      { OutCategory = ESFLogCategory::CounterUpdates; return true; }
	if (LowerName == TEXT("hudupdates"))          { OutCategory = ESFLogCategory::HUDUpdates; return true; }
	
	// Grid/Array features
	if (LowerName == TEXT("gridregeneration"))    { OutCategory = ESFLogCategory::GridRegeneration; return true; }
	if (LowerName == TEXT("gridpositioning"))     { OutCategory = ESFLogCategory::GridPositioning; return true; }
	if (LowerName == TEXT("childlifecycle"))      { OutCategory = ESFLogCategory::ChildLifecycle; return true; }
	
	// Build gun integration
	if (LowerName == TEXT("buildgunstate"))       { OutCategory = ESFLogCategory::BuildGunState; return true; }
	if (LowerName == TEXT("hologramregistration")){ OutCategory = ESFLogCategory::HologramRegistration; return true; }
	
	// Feature modules
	if (LowerName == TEXT("arrows"))              { OutCategory = ESFLogCategory::Arrows; return true; }
	if (LowerName == TEXT("scaling"))             { OutCategory = ESFLogCategory::Scaling; return true; }
	if (LowerName == TEXT("spacing"))             { OutCategory = ESFLogCategory::Spacing; return true; }
	if (LowerName == TEXT("steps"))               { OutCategory = ESFLogCategory::Steps; return true; }
	if (LowerName == TEXT("stagger"))             { OutCategory = ESFLogCategory::Stagger; return true; }
	
	// Performance & diagnostics
	if (LowerName == TEXT("performance"))         { OutCategory = ESFLogCategory::Performance; return true; }
	if (LowerName == TEXT("networksync"))         { OutCategory = ESFLogCategory::NetworkSync; return true; }
	
	return false;
}

FString FSFLogRegistry::GetVerbosityName(ESFLogVerbosity Verbosity)
{
	switch (Verbosity)
	{
		case ESFLogVerbosity::None:        return TEXT("None");
		case ESFLogVerbosity::Critical:    return TEXT("Critical");
		case ESFLogVerbosity::Normal:      return TEXT("Normal");
		case ESFLogVerbosity::Verbose:     return TEXT("Verbose");
		case ESFLogVerbosity::VeryVerbose: return TEXT("VeryVerbose");
		default:                           return TEXT("Unknown");
	}
}

bool FSFLogRegistry::ParseVerbosityName(const FString& Name, ESFLogVerbosity& OutVerbosity)
{
	const FString LowerName = Name.ToLower();
	
	if (LowerName == TEXT("none"))        { OutVerbosity = ESFLogVerbosity::None; return true; }
	if (LowerName == TEXT("critical"))    { OutVerbosity = ESFLogVerbosity::Critical; return true; }
	if (LowerName == TEXT("normal"))      { OutVerbosity = ESFLogVerbosity::Normal; return true; }
	if (LowerName == TEXT("verbose"))     { OutVerbosity = ESFLogVerbosity::Verbose; return true; }
	if (LowerName == TEXT("veryverbose")) { OutVerbosity = ESFLogVerbosity::VeryVerbose; return true; }
	
	return false;
}

TMap<FString, ESFLogVerbosity> FSFLogRegistry::GetAllCategoryLevels()
{
	TMap<FString, ESFLogVerbosity> Result;
	
	for (uint8 i = 0; i < static_cast<uint8>(ESFLogCategory::MAX_CATEGORIES); ++i)
	{
		const ESFLogCategory Cat = static_cast<ESFLogCategory>(i);
		const FString CategoryName = GetCategoryName(Cat);
		const ESFLogVerbosity CurrentLevel = GetCategoryVerbosity(Cat);
		
		Result.Add(CategoryName, CurrentLevel);
	}
	
	return Result;
}
