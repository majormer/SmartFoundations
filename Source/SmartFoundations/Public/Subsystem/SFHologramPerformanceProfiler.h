#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"
#include "Hologram/FGHologram.h"

/**
 * Performance profiler for hologram operations
 * Tracks UObject creation, memory, and timing to find bottlenecks
 */
class SMARTFOUNDATIONS_API FSFHologramPerformanceProfiler
{
public:
	/** Start profiling a spawn operation */
	static void BeginSpawnProfile(const FString& OperationName, int32 ChildCount);
	
	/** End profiling a spawn operation */
	static void EndSpawnProfile();
	
	/** Start profiling a destroy operation */
	static void BeginDestroyProfile(const FString& OperationName, int32 ChildCount);
	
	/** End profiling a destroy operation */
	static void EndDestroyProfile();
	
	/** Start profiling a reposition operation */
	static void BeginRepositionProfile(const FString& OperationName, int32 ChildCount);
	
	/** End profiling a reposition operation */
	static void EndRepositionProfile(int32 TransformCalls, int32 FloorValidationToggles);
	
	/** Start profiling a validation operation */
	static void BeginValidationProfile(const FString& OperationName);
	
	/** End profiling a validation operation */
	static void EndValidationProfile(EHologramMaterialState MaterialState);
	
	/** Log current UObject statistics */
	static void LogUObjectStats(const FString& Context);
	
	/** Log hologram component breakdown */
	static void LogHologramComponents(const AFGHologram* Hologram);
	
private:
	struct FProfileData
	{
		FString OperationName;
		int32 ChildCount = 0;
		int32 UObjectsBefore = 0;
		double TimeStart = 0.0;
		SIZE_T MemoryBefore = 0;
	};
	
	static FProfileData CurrentProfile;
	static bool bIsProfiling;
};
