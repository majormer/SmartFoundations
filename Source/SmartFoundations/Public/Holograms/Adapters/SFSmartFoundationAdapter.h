// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Smart Foundation Hologram Adapter

#pragma once

#include "CoreMinimal.h"
#include "SFSmartBuildableAdapter.h"
#include "Holograms/Core/SFFoundationHologram.h"

/**
 * Adapter for Smart foundation holograms (ASFFoundationHologram)
 * 
 * Extends base buildable adapter with grid validation functionality.
 * Enables foundation-specific grid snapping and placement validation.
 */
class SMARTFOUNDATIONS_API FSFSmartFoundationAdapter : public FSFSmartBuildableAdapter
{
public:
	explicit FSFSmartFoundationAdapter(ASFFoundationHologram* InHologram);
	virtual ~FSFSmartFoundationAdapter() = default;

	// Grid validation interface (Phase 1.4 stubs for now)
	virtual bool ValidateFoundationGrid() const;
	virtual void ApplyGridSnapping();

	virtual FString GetAdapterTypeName() const override;

protected:
	/** Typed pointer to Smart foundation hologram */
	ASFFoundationHologram* SmartFoundationHologram = nullptr;
};
