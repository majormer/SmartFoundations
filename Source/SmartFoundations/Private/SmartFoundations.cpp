// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartFoundations.h"
#include "Input/SFInputRegistry.h"
#include "Logging/SFLogRegistry.h"
#include "SFRCO.h"
#include "Engine/Engine.h"
#include "Registry/RemoteCallObjectRegistry.h"

// Smart! Log Category - Already declared in header
DEFINE_LOG_CATEGORY(LogSmartFoundations);

void FSmartFoundationsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogSmartFoundations, Warning, TEXT("SmartFoundations module started - Phase 1: Core Infrastructure"));
	
	// Initialize centralized logging system
	FSFLogRegistry::LoadFromConfig();
	UE_LOG(LogSmartFoundations, Log, TEXT("✅ Logging registry initialized from Config/SmartFoundationsLogging.ini"));
	
	// Unregister any existing console commands (handles hot reload/stale state)
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("SF.Log.List"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("SF.Log.SetVerbosity"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("SF.Log.ResetToConfig"));
	
	// Register console commands for logging control
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SF.Log.List"),
		TEXT("List all logging categories and their current verbosity levels"),
		FConsoleCommandDelegate::CreateStatic(&FSmartFoundationsModule::ConsoleCommand_ListLogCategories),
		ECVF_Default
	);
	
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SF.Log.SetVerbosity"),
		TEXT("Set logging verbosity for a category. Usage: SF.Log.SetVerbosity <Category> <Verbosity>"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FSmartFoundationsModule::ConsoleCommand_SetLogVerbosity),
		ECVF_Default
	);
	
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SF.Log.ResetToConfig"),
		TEXT("Reset all logging categories to values from SmartFoundationsLogging.ini"),
		FConsoleCommandDelegate::CreateStatic(&FSmartFoundationsModule::ConsoleCommand_ResetLogConfig),
		ECVF_Default
	);
	
	UE_LOG(LogSmartFoundations, Log, TEXT("✅ Logging console commands registered (SF.Log.List, SF.Log.SetVerbosity, SF.Log.ResetToConfig)"));
	
	// Initialize Smart! Enhanced Input system using SML 3.11.x approach
	USFInputRegistry::InitializeSmartInputSystem();

	// Register Smart! Remote Call Object for multiplayer networking
	if (GEngine)
	{
		if (UGameInstance* GameInstance = GEngine->GetWorldContexts()[0].OwningGameInstance)
		{
			if (URemoteCallObjectRegistry* RCORegistry = GameInstance->GetSubsystem<URemoteCallObjectRegistry>())
			{
				RCORegistry->RegisterRemoteCallObject(USFRCO::StaticClass());
				UE_LOG(LogSmartFoundations, Log, TEXT("✅ USFRCO registered with SML RemoteCallObjectRegistry"));
			}
		}
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("SmartFoundations initialization complete"));
}

void FSmartFoundationsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	// Unregister console commands
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("SF.Log.List"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("SF.Log.SetVerbosity"));
	IConsoleManager::Get().UnregisterConsoleObject(TEXT("SF.Log.ResetToConfig"));
	
	UE_LOG(LogSmartFoundations, Warning, TEXT("SmartFoundations module shutdown"));
}

// ============================================================================
// Console Command Implementations
// ============================================================================

void FSmartFoundationsModule::ConsoleCommand_ListLogCategories()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogSmartFoundations, Log, TEXT("Smart! Logging Categories (Current Settings)"));
	UE_LOG(LogSmartFoundations, Log, TEXT("═══════════════════════════════════════════════════════════"));
	
	TMap<FString, ESFLogVerbosity> Categories = FSFLogRegistry::GetAllCategoryLevels();
	
	// Sort categories alphabetically
	TArray<FString> SortedNames;
	Categories.GetKeys(SortedNames);
	SortedNames.Sort();
	
	for (const FString& CategoryName : SortedNames)
	{
		const ESFLogVerbosity Level = Categories[CategoryName];
		const FString LevelName = FSFLogRegistry::GetVerbosityName(Level);
		
		// Add visual indicator for level
		FString Indicator;
		switch (Level)
		{
			case ESFLogVerbosity::None:        Indicator = TEXT("🚫"); break;
			case ESFLogVerbosity::Critical:    Indicator = TEXT("⚠️"); break;
			case ESFLogVerbosity::Normal:      Indicator = TEXT("📋"); break;
			case ESFLogVerbosity::Verbose:     Indicator = TEXT("📝"); break;
			case ESFLogVerbosity::VeryVerbose: Indicator = TEXT("🔍"); break;
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("  %s %-25s : %s"), *Indicator, *CategoryName, *LevelName);
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("═══════════════════════════════════════════════════════════"));
	UE_LOG(LogSmartFoundations, Log, TEXT("Use 'SF.Log.SetVerbosity <Category> <Level>' to change"));
	UE_LOG(LogSmartFoundations, Log, TEXT("Levels: None | Critical | Normal | Verbose | VeryVerbose"));
}

void FSmartFoundationsModule::ConsoleCommand_SetLogVerbosity(const TArray<FString>& Args)
{
	if (Args.Num() != 2)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Usage: SF.Log.SetVerbosity <Category> <Verbosity>"));
		UE_LOG(LogSmartFoundations, Warning, TEXT("Example: SF.Log.SetVerbosity InputEvents VeryVerbose"));
		UE_LOG(LogSmartFoundations, Warning, TEXT(""));
		UE_LOG(LogSmartFoundations, Warning, TEXT("Available verbosity levels:"));
		UE_LOG(LogSmartFoundations, Warning, TEXT("  None, Critical, Normal, Verbose, VeryVerbose"));
		UE_LOG(LogSmartFoundations, Warning, TEXT(""));
		UE_LOG(LogSmartFoundations, Warning, TEXT("Use 'SF.Log.List' to see all categories"));
		return;
	}
	
	const FString CategoryName = Args[0];
	const FString VerbosityName = Args[1];
	
	// Parse category
	ESFLogCategory Category;
	if (!FSFLogRegistry::ParseCategoryName(CategoryName, Category))
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Unknown category: '%s'"), *CategoryName);
		UE_LOG(LogSmartFoundations, Error, TEXT("Use 'SF.Log.List' to see available categories"));
		return;
	}
	
	// Parse verbosity
	ESFLogVerbosity Verbosity;
	if (!FSFLogRegistry::ParseVerbosityName(VerbosityName, Verbosity))
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("❌ Unknown verbosity: '%s'"), *VerbosityName);
		UE_LOG(LogSmartFoundations, Error, TEXT("Valid levels: None, Critical, Normal, Verbose, VeryVerbose"));
		return;
	}
	
	// Apply change
	FSFLogRegistry::SetCategoryVerbosity(Category, Verbosity);
	
	UE_LOG(LogSmartFoundations, Log, TEXT("✅ Set %s = %s"), *CategoryName, *VerbosityName);
	
	// Warn if enabling very verbose logging
	if (Verbosity == ESFLogVerbosity::VeryVerbose)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ VeryVerbose can generate thousands of log lines - use sparingly!"));
	}
}

void FSmartFoundationsModule::ConsoleCommand_ResetLogConfig()
{
	FSFLogRegistry::LoadFromConfig();
	UE_LOG(LogSmartFoundations, Log, TEXT("✅ Logging configuration reloaded from Config/SmartFoundationsLogging.ini"));
	UE_LOG(LogSmartFoundations, Log, TEXT("Use 'SF.Log.List' to see current settings"));
}
	
IMPLEMENT_MODULE(FSmartFoundationsModule, SmartFoundations)
