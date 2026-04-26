// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Module/GameInstanceModule.h"
#include "Configuration/ModConfiguration.h"
#include "SFGameInstanceModule.generated.h"

/**
 * Smart! Foundations Game Instance Module
 * Registers widget blueprint hooks and mod-wide initialization
 */
UCLASS(Abstract)
class SMARTFOUNDATIONS_API USFGameInstanceModule : public UGameInstanceModule
{
	GENERATED_BODY()

public:
	USFGameInstanceModule();

	/** Override lifecycle event to register widget hooks */
	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;

protected:
	/** Register widget blueprint hooks for HUD overlay */
	void RegisterWidgetHooks();
	
	/** Register SML hook for cost aggregation (belt preview costs in distributor tooltip) */
	void RegisterCostAggregationHook();
	
	/** Register SML hook for blueprint construct to handle chain actor rebuilding (like AutoLink) */
	void RegisterBlueprintConstructHook();

	/** Widget class to inject into game HUD */
	UPROPERTY(EditDefaultsOnly, Category = "Smart! UI")
	TSubclassOf<UUserWidget> CounterWidgetClass;

	/** Smart! Configuration blueprint - registered with SML for in-game menu access */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Smart! Configuration")
	TSubclassOf<class UModConfiguration> SmartConfigClass;
};
