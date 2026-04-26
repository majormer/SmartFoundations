// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Safe Arrow Asset Manager Implementation

#include "Features/Arrows/SFArrowAssetManager.h"
#include "SmartFoundations.h"
#include "Logging/SFLogMacros.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

FSFArrowAssetManager::FSFArrowAssetManager()
	: bAssetsLoaded(false)
{
	// Set up soft references to engine assets (Task #58: Full asset paths with .AssetName suffix)
	ArrowMeshSoft = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cone.Cone")));
	ShaftMeshSoft = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
	ArrowMaterialSoft = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
	
	// Version stamp to confirm new code is running (Task #58)
	UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 SFArrowAssetManager v3.0 constructed - FIXED ASSET PATHS!"));
}

FSFArrowAssetManager::~FSFArrowAssetManager()
{
	CancelPendingLoads();
}

bool FSFArrowAssetManager::LoadAssetsAsync(TFunction<void(bool, UStaticMesh*, UStaticMesh*, UMaterialInterface*)> OnCompleted)
{
	// Cancel any existing load
	CancelPendingLoads();

	// Build array of asset paths to load (Issue #213: added Cylinder for arrow shaft)
	TArray<FSoftObjectPath> AssetsToLoad;
	AssetsToLoad.Add(ArrowMeshSoft.ToSoftObjectPath());
	AssetsToLoad.Add(ShaftMeshSoft.ToSoftObjectPath());
	AssetsToLoad.Add(ArrowMaterialSoft.ToSoftObjectPath());

	SF_LOG_ARROWS(Normal, TEXT("📦 Asset Manager: Starting async load of %d assets"), AssetsToLoad.Num());
	SF_LOG_ARROWS(Verbose, TEXT("   Head Mesh: %s"), *ArrowMeshSoft.ToSoftObjectPath().ToString());
	SF_LOG_ARROWS(Verbose, TEXT("   Shaft Mesh: %s"), *ShaftMeshSoft.ToSoftObjectPath().ToString());
	SF_LOG_ARROWS(Verbose, TEXT("   Material: %s"), *ArrowMaterialSoft.ToSoftObjectPath().ToString());

	// Get streamable manager from asset manager
	FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();

	// Request async load
	StreamableHandle = StreamableManager.RequestAsyncLoad(
		AssetsToLoad,
		FStreamableDelegate::CreateLambda([this, OnCompleted]()
		{
			// This callback runs on game thread after assets are loaded
			UStaticMesh* LoadedMesh = ArrowMeshSoft.Get();
			UStaticMesh* LoadedShaftMesh = ShaftMeshSoft.Get();
			UMaterialInterface* LoadedMaterial = ArrowMaterialSoft.Get();

			// Validate assets are fully ready
			const bool bMeshReady = IsStaticMeshFullyReady(LoadedMesh);
			const bool bShaftReady = IsStaticMeshFullyReady(LoadedShaftMesh);
			const bool bMaterialReady = IsMaterialFullyReady(LoadedMaterial);
			const bool bSuccess = bMeshReady && bShaftReady && bMaterialReady;

			if (bSuccess)
			{
				bAssetsLoaded = true;
				SF_LOG_ARROWS(Normal, TEXT("✅ Asset Manager: Assets loaded and validated successfully (head + shaft + material)"));
			}
			else
			{
				SF_LOG_ERROR(Arrows, TEXT("Asset load completed but validation failed (Head=%s Shaft=%s Material=%s)"),
					bMeshReady ? TEXT("OK") : TEXT("FAILED"),
					bShaftReady ? TEXT("OK") : TEXT("FAILED"),
					bMaterialReady ? TEXT("OK") : TEXT("FAILED"));
			}

			// Invoke completion callback
			if (OnCompleted)
			{
				OnCompleted(bSuccess, LoadedMesh, LoadedShaftMesh, LoadedMaterial);
			}
		}),
		FStreamableManager::AsyncLoadHighPriority  // High priority for UI-critical assets
	);

	return StreamableHandle.IsValid();
}

bool FSFArrowAssetManager::IsStaticMeshFullyReady(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		SF_LOG_ARROWS(VeryVerbose, TEXT("🔍 Readiness Check: Mesh is null"));
		return false;
	}

	if (!IsValid(Mesh))
	{
		SF_LOG_WARNING(Arrows, TEXT("Mesh failed IsValid() check - marked for GC?"));
		return false;
	}

	// Check RenderData
	const FStaticMeshRenderData* RenderData = Mesh->GetRenderData();
	
	// Validate pointer is safe to dereference
	if (!IsPointerSafe(RenderData))
	{
		const uintptr_t PtrValue = reinterpret_cast<uintptr_t>(RenderData);
		SF_LOG_WARNING(Arrows, TEXT("RenderData pointer is CORRUPTED (0x%llX) - This would have crashed!"), PtrValue);
		return false;
	}

	if (!RenderData)
	{
		SF_LOG_ARROWS(VeryVerbose, TEXT("🔍 Readiness Check: RenderData is null"));
		return false;
	}

	if (!RenderData->IsInitialized())
	{
		SF_LOG_ARROWS(Verbose, TEXT("🔍 Readiness Check: RenderData exists but NOT initialized yet"));
		return false;
	}

	// Validate all materials
	const TArray<FStaticMaterial>& StaticMaterials = Mesh->GetStaticMaterials();
	SF_LOG_ARROWS(VeryVerbose, TEXT("🔍 Readiness Check: Validating %d materials"), StaticMaterials.Num());
	
	for (int32 i = 0; i < StaticMaterials.Num(); ++i)
	{
		UMaterialInterface* Material = StaticMaterials[i].MaterialInterface;
		if (!IsMaterialFullyReady(Material))
		{
			SF_LOG_ARROWS(Verbose, TEXT("🔍 Readiness Check: Material[%d] NOT ready"), i);
			return false;
		}
	}
	
	SF_LOG_ARROWS(VeryVerbose, TEXT("✅ Readiness Check: Mesh FULLY READY (RenderData initialized, all materials valid)"));

	return true;
}

bool FSFArrowAssetManager::IsMaterialFullyReady(UMaterialInterface* Material)
{
	if (!Material)
	{
		return false;
	}

	if (!IsValid(Material))
	{
		return false;
	}

	// Check if render proxy exists (indicates material is ready for rendering)
	// Note: GetRenderProxy() can be expensive, but necessary for safety
	FMaterialRenderProxy* RenderProxy = Material->GetRenderProxy();
	if (!IsPointerSafe(RenderProxy))
	{
		return false;
	}

	if (!RenderProxy)
	{
		return false;
	}

	return true;
}

void FSFArrowAssetManager::CancelPendingLoads()
{
	if (StreamableHandle.IsValid())
	{
		SF_LOG_ARROWS(Normal, TEXT("📦 Asset Manager: Cancelling pending asset load"));
		StreamableHandle->CancelHandle();
		StreamableHandle.Reset();
	}

	bAssetsLoaded = false;
}

UStaticMesh* FSFArrowAssetManager::GetLoadedMesh() const
{
	return bAssetsLoaded ? ArrowMeshSoft.Get() : nullptr;
}

UStaticMesh* FSFArrowAssetManager::GetLoadedShaftMesh() const
{
	return bAssetsLoaded ? ShaftMeshSoft.Get() : nullptr;
}

UMaterialInterface* FSFArrowAssetManager::GetLoadedMaterial() const
{
	return bAssetsLoaded ? ArrowMaterialSoft.Get() : nullptr;
}

bool FSFArrowAssetManager::IsPointerSafe(const void* Ptr)
{
	if (!Ptr)
	{
		return true;  // Null is safe (will be caught by other checks)
	}

	const uintptr_t PtrValue = reinterpret_cast<uintptr_t>(Ptr);

	// Detect invalid/corrupted pointers:
	// - High addresses (0xFFFF...) = freed memory markers
	// - Low addresses (< 0x10000) = OS reserved, typically invalid
	// - Suspicious range (< 16GB) = Likely corrupted on 64-bit systems
	//   Real UE object pointers are typically > 0x400000000 (beyond 32-bit range)
	//   Crash report: 0x0000000600000097 (~6GB) was corrupted
	const bool bIsHighPoison = (PtrValue >= 0xFFFFFFFFFFFFFFF0);
	const bool bIsLowInvalid = (PtrValue < 0x10000);
	const bool bIsSuspiciousRange = (PtrValue < 0x400000000);  // < 16GB

	return !(bIsHighPoison || bIsLowInvalid || bIsSuspiciousRange);
}
