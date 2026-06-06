// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "Configuration/ModConfiguration.h"
#include "SFGameInstanceModule.generated.h"

/**
 * Smart! Foundations Game Instance Module
 * Registers mod-wide initialization hooks
 */
UCLASS(Abstract)
class SMARTFOUNDATIONS_API USFGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	USFGameInstanceModule();

	/** Override lifecycle event to register module hooks */
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

protected:
	/** Register SML hook for cost aggregation (belt preview costs in distributor tooltip) */
	void RegisterCostAggregationHook();
	
	/** Register SML hook for blueprint construct to handle chain actor rebuilding (like AutoLink) */
	void RegisterBlueprintConstructHook();

	/** Smart! Configuration blueprint - registered with SML for in-game menu access */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Smart! Configuration")
	TSubclassOf<class UModConfiguration> SmartConfigClass;
};
