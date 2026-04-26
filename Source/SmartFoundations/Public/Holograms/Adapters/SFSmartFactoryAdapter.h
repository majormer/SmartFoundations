// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Factory Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "SFSmartBuildableAdapter.h"
#include "Holograms/Core/SFFactoryHologram.h"

class UFGRecipe;

/**
 * Adapter for Smart factory holograms (ASFFactoryHologram)
 * 
 * Extends base buildable adapter with recipe copying functionality.
 * Enables copying recipes from first building to subsequent placements in grids.
 */
class SMARTFOUNDATIONS_API FSFSmartFactoryAdapter : public FSFSmartBuildableAdapter
{
public:
	explicit FSFSmartFactoryAdapter(ASFFactoryHologram* InHologram);
	virtual ~FSFSmartFactoryAdapter() = default;

	// Recipe copying interface (Phase 5 implementation)
	virtual bool SupportsRecipeCopying() const;
	virtual void ApplyStoredRecipe(TSubclassOf<UFGRecipe> Recipe);
	virtual TSubclassOf<UFGRecipe> GetStoredRecipe() const;
	virtual void CopyRecipeFromBuilding(AFGBuildable* Building);

	virtual FString GetAdapterTypeName() const override;

protected:
	/** Typed pointer to Smart factory hologram */
	ASFFactoryHologram* SmartFactoryHologram = nullptr;
};
