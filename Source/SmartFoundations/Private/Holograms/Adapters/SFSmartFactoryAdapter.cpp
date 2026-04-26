// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Factory Hologram Adapter Implementation

#include "SFSmartFactoryAdapter.h"
#include "SmartFoundations.h"
#include "FGRecipe.h"

FSFSmartFactoryAdapter::FSFSmartFactoryAdapter(ASFFactoryHologram* InHologram)
	: FSFSmartBuildableAdapter(InHologram)
	, SmartFactoryHologram(InHologram)
{
	UE_LOG(LogSmartFoundations, Verbose, TEXT("FSFSmartFactoryAdapter created for: %s"), 
		InHologram ? *InHologram->GetName() : TEXT("NULL"));
}

bool FSFSmartFactoryAdapter::SupportsRecipeCopying() const
{
	// All factory holograms support recipe copying
	return true;
}

void FSFSmartFactoryAdapter::ApplyStoredRecipe(TSubclassOf<UFGRecipe> Recipe)
{
	if (SmartFactoryHologram)
	{
		// Delegate to custom hologram method (Phase 1.2 stub for now)
		// SmartFactoryHologram->ApplyStoredRecipe(Recipe);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("ApplyStoredRecipe called (Phase 1.2 stub)"));
	}
}

TSubclassOf<UFGRecipe> FSFSmartFactoryAdapter::GetStoredRecipe() const
{
	if (SmartFactoryHologram)
	{
		// Delegate to custom hologram method (Phase 1.2 stub for now)
		// return SmartFactoryHologram->GetStoredRecipe();
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetStoredRecipe called (Phase 1.2 stub)"));
	}
	return nullptr;
}

void FSFSmartFactoryAdapter::CopyRecipeFromBuilding(AFGBuildable* Building)
{
	if (SmartFactoryHologram)
	{
		// Delegate to custom hologram method (Phase 1.2 stub for now)
		// SmartFactoryHologram->CopyRecipeFromBuilding(Building);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("CopyRecipeFromBuilding called (Phase 1.2 stub)"));
	}
}

FString FSFSmartFactoryAdapter::GetAdapterTypeName() const
{
	return TEXT("SmartFactory");
}
