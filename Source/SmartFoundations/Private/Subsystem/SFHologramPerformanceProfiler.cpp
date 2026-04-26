#include "Subsystem/SFHologramPerformanceProfiler.h"
#include "SmartFoundations.h"
#include "SFSubsystem.h"
#include "Hologram/FGHologram.h"
#include "Components/ActorComponent.h"
#include "HAL/PlatformMemory.h"

FSFHologramPerformanceProfiler::FProfileData FSFHologramPerformanceProfiler::CurrentProfile;
bool FSFHologramPerformanceProfiler::bIsProfiling = false;

void FSFHologramPerformanceProfiler::BeginSpawnProfile(const FString& OperationName, int32 ChildCount)
{
	if (bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ BeginSpawnProfile called while already profiling!"));
		return;
	}
	
	bIsProfiling = true;
	CurrentProfile.OperationName = OperationName;
	CurrentProfile.ChildCount = ChildCount;
	CurrentProfile.UObjectsBefore = GUObjectArray.GetObjectArrayNum();
	CurrentProfile.TimeStart = FPlatformTime::Seconds();
	
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	CurrentProfile.MemoryBefore = MemStats.UsedPhysical;
	
	// Tester-friendly log: simple spawn count
	UE_LOG(LogSmartFoundations, Log, TEXT("📊 Spawning %d children..."), ChildCount);
	
	// Detailed profiling data at Verbose for performance analysis
	UE_LOG(LogSmartFoundations, Verbose, TEXT("📊 SPAWN PROFILE START: %s (spawning %d children)"), *OperationName, ChildCount);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   UObjects Before: %d"), CurrentProfile.UObjectsBefore);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   Memory Before: %.2f MB"), CurrentProfile.MemoryBefore / (1024.0 * 1024.0));
}

void FSFHologramPerformanceProfiler::EndSpawnProfile()
{
	if (!bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ EndSpawnProfile called without BeginSpawnProfile!"));
		return;
	}
	
	const int32 UObjectsAfter = GUObjectArray.GetObjectArrayNum();
	const int32 UObjectDelta = UObjectsAfter - CurrentProfile.UObjectsBefore;
	const double TimeElapsed = FPlatformTime::Seconds() - CurrentProfile.TimeStart;
	
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	const SIZE_T MemoryAfter = MemStats.UsedPhysical;
	const int64 MemoryDelta = static_cast<int64>(MemoryAfter) - static_cast<int64>(CurrentProfile.MemoryBefore);
	
	// Tester-friendly log: simple completion with time
	UE_LOG(LogSmartFoundations, Log, TEXT("✅ Spawned %d children in %.1f ms"), CurrentProfile.ChildCount, TimeElapsed * 1000.0);
	
	// Detailed profiling data at Verbose for performance analysis
	UE_LOG(LogSmartFoundations, Verbose, TEXT("📊 SPAWN PROFILE END: %s"), *CurrentProfile.OperationName);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   Children Spawned: %d"), CurrentProfile.ChildCount);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   UObjects After: %d (delta: %d)"), UObjectsAfter, UObjectDelta);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   UObjects Per Child: %.2f"), CurrentProfile.ChildCount > 0 ? static_cast<float>(UObjectDelta) / CurrentProfile.ChildCount : 0.0f);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   Memory After: %.2f MB (delta: %.2f MB)"), MemoryAfter / (1024.0 * 1024.0), MemoryDelta / (1024.0 * 1024.0));
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   Memory Per Child: %.2f KB"), CurrentProfile.ChildCount > 0 ? (MemoryDelta / 1024.0) / CurrentProfile.ChildCount : 0.0f);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   Time Elapsed: %.3f ms (%.3f ms per child)"), TimeElapsed * 1000.0, CurrentProfile.ChildCount > 0 ? (TimeElapsed * 1000.0) / CurrentProfile.ChildCount : 0.0f);
	
	bIsProfiling = false;
}

void FSFHologramPerformanceProfiler::BeginDestroyProfile(const FString& OperationName, int32 ChildCount)
{
	if (bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ BeginDestroyProfile called while already profiling!"));
		return;
	}
	
	bIsProfiling = true;
	CurrentProfile.OperationName = OperationName;
	CurrentProfile.ChildCount = ChildCount;
	CurrentProfile.UObjectsBefore = GUObjectArray.GetObjectArrayNum();
	CurrentProfile.TimeStart = FPlatformTime::Seconds();
	
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	CurrentProfile.MemoryBefore = MemStats.UsedPhysical;
	
	// Detailed profiling at Verbose - testers don't need this
	UE_LOG(LogSmartFoundations, Verbose, TEXT("📊 DESTROY PROFILE START: %s (destroying %d children)"), *OperationName, ChildCount);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   UObjects Before: %d"), CurrentProfile.UObjectsBefore);
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   Memory Before: %.2f MB"), CurrentProfile.MemoryBefore / (1024.0 * 1024.0));
}

void FSFHologramPerformanceProfiler::EndDestroyProfile()
{
	if (!bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ EndDestroyProfile called without BeginDestroyProfile!"));
		return;
	}
	
	const int32 UObjectsAfter = GUObjectArray.GetObjectArrayNum();
	const int32 UObjectDelta = UObjectsAfter - CurrentProfile.UObjectsBefore;
	const double TimeElapsed = FPlatformTime::Seconds() - CurrentProfile.TimeStart;
	
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	const SIZE_T MemoryAfter = MemStats.UsedPhysical;
	const int64 MemoryDelta = static_cast<int64>(MemoryAfter) - static_cast<int64>(CurrentProfile.MemoryBefore);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 DESTROY PROFILE END: %s"), *CurrentProfile.OperationName);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Children Destroyed: %d"), CurrentProfile.ChildCount);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   UObjects After: %d (delta: %d)"), UObjectsAfter, UObjectDelta);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   UObjects Freed Per Child: %.2f"), CurrentProfile.ChildCount > 0 ? static_cast<float>(-UObjectDelta) / CurrentProfile.ChildCount : 0.0f);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Memory After: %.2f MB (delta: %.2f MB)"), MemoryAfter / (1024.0 * 1024.0), MemoryDelta / (1024.0 * 1024.0));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Memory Freed Per Child: %.2f KB"), CurrentProfile.ChildCount > 0 ? (-MemoryDelta / 1024.0) / CurrentProfile.ChildCount : 0.0f);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Time Elapsed: %.3f ms (%.3f ms per child)"), TimeElapsed * 1000.0, CurrentProfile.ChildCount > 0 ? (TimeElapsed * 1000.0) / CurrentProfile.ChildCount : 0.0f);
	
	bIsProfiling = false;
}

void FSFHologramPerformanceProfiler::BeginRepositionProfile(const FString& OperationName, int32 ChildCount)
{
	if (bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ BeginRepositionProfile called while already profiling!"));
		return;
	}
	
	bIsProfiling = true;
	CurrentProfile.OperationName = OperationName;
	CurrentProfile.ChildCount = ChildCount;
	CurrentProfile.UObjectsBefore = GUObjectArray.GetObjectArrayNum();
	CurrentProfile.TimeStart = FPlatformTime::Seconds();
	
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	CurrentProfile.MemoryBefore = MemStats.UsedPhysical;
	
	UE_LOG(LogSmartFoundations, Warning, TEXT("📊 REPOSITION PROFILE START: %s (repositioning %d children)"), *OperationName, ChildCount);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   UObjects Before: %d"), CurrentProfile.UObjectsBefore);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Memory Before: %.2f MB"), CurrentProfile.MemoryBefore / (1024.0 * 1024.0));
}

void FSFHologramPerformanceProfiler::EndRepositionProfile(int32 TransformCalls, int32 FloorValidationToggles)
{
	if (!bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ EndRepositionProfile called without BeginRepositionProfile!"));
		return;
	}
	
	const int32 UObjectsAfter = GUObjectArray.GetObjectArrayNum();
	const int32 UObjectDelta = UObjectsAfter - CurrentProfile.UObjectsBefore;
	const double TimeElapsed = FPlatformTime::Seconds() - CurrentProfile.TimeStart;
	
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	const SIZE_T MemoryAfter = MemStats.UsedPhysical;
	const int64 MemoryDelta = static_cast<int64>(MemoryAfter) - static_cast<int64>(CurrentProfile.MemoryBefore);
	
	UE_LOG(LogSmartFoundations, Warning, TEXT("📊 REPOSITION PROFILE END: %s"), *CurrentProfile.OperationName);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Children Repositioned: %d"), CurrentProfile.ChildCount);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Transform Calls: %d"), TransformCalls);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Floor Validation Toggles: %d"), FloorValidationToggles);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   UObjects After: %d (delta: %d)"), UObjectsAfter, UObjectDelta);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Memory After: %.2f MB (delta: %.2f MB)"), MemoryAfter / (1024.0 * 1024.0), MemoryDelta / (1024.0 * 1024.0));
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Time Elapsed: %.3f ms (%.3f ms per child)"), TimeElapsed * 1000.0, CurrentProfile.ChildCount > 0 ? (TimeElapsed * 1000.0) / CurrentProfile.ChildCount : 0.0f);
	
	bIsProfiling = false;
}

void FSFHologramPerformanceProfiler::BeginValidationProfile(const FString& OperationName)
{
	if (bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ BeginValidationProfile called while already profiling!"));
		return;
	}
	
	bIsProfiling = true;
	CurrentProfile.OperationName = OperationName;
	CurrentProfile.TimeStart = FPlatformTime::Seconds();
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 VALIDATION PROFILE START: %s"), *OperationName);
}

void FSFHologramPerformanceProfiler::EndValidationProfile(EHologramMaterialState MaterialState)
{
	if (!bIsProfiling)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ EndValidationProfile called without BeginValidationProfile!"));
		return;
	}
	
	const double TimeElapsed = FPlatformTime::Seconds() - CurrentProfile.TimeStart;
	
	const TCHAR* StateStr = (MaterialState == EHologramMaterialState::HMS_OK) ? TEXT("HMS_OK") :
	                        (MaterialState == EHologramMaterialState::HMS_WARNING) ? TEXT("HMS_WARNING") :
	                        TEXT("HMS_ERROR");
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("📊 VALIDATION PROFILE END: %s"), *CurrentProfile.OperationName);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Time Elapsed: %.3f ms"), TimeElapsed * 1000.0);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Material State: %s"), StateStr);
	
	bIsProfiling = false;
}

void FSFHologramPerformanceProfiler::LogUObjectStats(const FString& Context)
{
	const int32 TotalObjects = GUObjectArray.GetObjectArrayNum();
	const int32 MaxObjects = GUObjectArray.GetObjectArrayNumMinusPermanent();
	
	UE_LOG(LogSmartFoundations, Warning, TEXT("📊 UOBJECT STATS [%s]"), *Context);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Total UObjects: %d / %d"), TotalObjects, MaxObjects);
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Utilization: %.2f%%"), MaxObjects > 0 ? (static_cast<float>(TotalObjects) / MaxObjects * 100.0f) : 0.0f);
	
	FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	UE_LOG(LogSmartFoundations, Warning, TEXT("   Physical Memory: %.2f MB used / %.2f MB total"), 
		MemStats.UsedPhysical / (1024.0 * 1024.0),
		MemStats.TotalPhysical / (1024.0 * 1024.0));
}

void FSFHologramPerformanceProfiler::LogHologramComponents(const AFGHologram* Hologram)
{
	if (!Hologram || !IsValid(Hologram))
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("⚠️ LogHologramComponents: Invalid hologram"));
		return;
	}
	
	TArray<UActorComponent*> Components;
	Hologram->GetComponents(Components);
	
	// Component details at Verbose - testers don't need this level of detail
	UE_LOG(LogSmartFoundations, Verbose, TEXT("📊 HOLOGRAM COMPONENTS: %s"), *Hologram->GetName());
	UE_LOG(LogSmartFoundations, Verbose, TEXT("   Total Components: %d"), Components.Num());
	
	// Count by type
	TMap<FString, int32> ComponentCounts;
	for (UActorComponent* Component : Components)
	{
		if (Component)
		{
			FString TypeName = Component->GetClass()->GetName();
			ComponentCounts.FindOrAdd(TypeName)++;
		}
	}
	
	// Log breakdown at Verbose
	for (const TPair<FString, int32>& Pair : ComponentCounts)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("      %s: %d"), *Pair.Key, Pair.Value);
	}
}
