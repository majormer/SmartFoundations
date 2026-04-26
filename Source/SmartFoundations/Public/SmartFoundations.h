// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSmartFoundations, Log, All);

class FSmartFoundationsModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** Console command: List all logging categories */
	static void ConsoleCommand_ListLogCategories();
	
	/** Console command: Set category verbosity */
	static void ConsoleCommand_SetLogVerbosity(const TArray<FString>& Args);
	
	/** Console command: Reset to config file values */
	static void ConsoleCommand_ResetLogConfig();
};
