// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Safe Arrow Asset Manager (Task #58 - Production Fix)

#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

/**
 * FSFArrowAssetManager - Safe async asset loading for arrow visualization
 * 
 * PROBLEM SOLVED (Task #58):
 * - Synchronous LoadObject() returns meshes with uninitialized RenderData
 * - Raw pointers don't track asset lifecycle (GC, streaming, level transitions)
 * - Calling SetStaticMesh() on uninitialized mesh triggers Nanite PSO crashes
 * - Pointer corruption (0x0000000600000097) passes null checks but crashes on dereference
 * 
 * SOLUTION:
 * - Async loading via FStreamableManager with completion callbacks
 * - TSoftObjectPtr<> for proper lifecycle tracking
 * - Comprehensive readiness validation (mesh RenderData + all materials)
 * - Deferred attachment with timeout and graceful fallback
 * 
 * USAGE:
 * ```cpp
 * FSFArrowAssetManager AssetManager;
 * AssetManager.LoadAssetsAsync(
 *     [this](bool bSuccess, UStaticMesh* Mesh, UMaterialInterface* Mat) {
 *         if (bSuccess && IsStaticMeshFullyReady(Mesh)) {
 *             ArrowComponent->SetStaticMesh(Mesh);
 *         }
 *     }
 * );
 * ```
 */
class SMARTFOUNDATIONS_API FSFArrowAssetManager
{
public:
	/** Constructor */
	FSFArrowAssetManager();
	
	/** Destructor - cancels pending loads */
	~FSFArrowAssetManager();

	/**
	 * Load arrow assets asynchronously
	 * 
	 * @param OnCompleted Callback when load completes (success, mesh, material)
	 * @return true if load started successfully
	 */
	bool LoadAssetsAsync(TFunction<void(bool, UStaticMesh*, UStaticMesh*, UMaterialInterface*)> OnCompleted);

	/**
	 * Check if static mesh and all materials are fully loaded and ready
	 * 
	 * Validates:
	 * - Mesh pointer valid and IsValid()
	 * - RenderData exists and IsInitialized()
	 * - All materials exist with valid render proxies
	 * - No pointer corruption (suspicious address ranges)
	 * 
	 * @param Mesh Mesh to validate
	 * @return true if mesh is completely ready for SetStaticMesh()
	 */
	static bool IsStaticMeshFullyReady(UStaticMesh* Mesh);

	/**
	 * Check if material is fully loaded and ready
	 * 
	 * @param Material Material to validate
	 * @return true if material is ready
	 */
	static bool IsMaterialFullyReady(UMaterialInterface* Material);

	/**
	 * Cancel any pending async loads
	 */
	void CancelPendingLoads();

	/**
	 * Check if assets are currently loading
	 */
	bool IsLoading() const { return StreamableHandle.IsValid() && StreamableHandle->IsLoadingInProgress(); }

	/**
	 * Check if assets have been loaded
	 */
	bool IsLoaded() const { return bAssetsLoaded; }

	/**
	 * Get loaded arrow head mesh (Cone - only valid after successful load completion)
	 */
	UStaticMesh* GetLoadedMesh() const;

	/**
	 * Get loaded shaft mesh (Cylinder - only valid after successful load completion)
	 */
	UStaticMesh* GetLoadedShaftMesh() const;

	/**
	 * Get loaded material (only valid after successful load completion)
	 */
	UMaterialInterface* GetLoadedMaterial() const;

private:
	/** Soft references to assets */
	TSoftObjectPtr<UStaticMesh> ArrowMeshSoft;
	TSoftObjectPtr<UStaticMesh> ShaftMeshSoft;
	TSoftObjectPtr<UMaterialInterface> ArrowMaterialSoft;

	/** Streamable handle for async loading */
	TSharedPtr<FStreamableHandle> StreamableHandle;

	/** Load completion state */
	bool bAssetsLoaded;

	/** Validate pointer is not corrupted before dereferencing */
	static bool IsPointerSafe(const void* Ptr);
};
