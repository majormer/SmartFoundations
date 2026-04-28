// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
#include "SFGameInstanceModule.h"
#include "SmartFoundations.h"

// Force UHT to parse these classes so AccessTransformers apply
#include "FactoryGame/Public/Hologram/FGConveyorBeltHologram.h"
#include "FactoryGame/Public/Hologram/FGConveyorAttachmentHologram.h"
#include "FactoryGame/Public/Hologram/FGSplineHologram.h"
#include "FactoryGame/Public/Hologram/FGBuildableHologram.h"
#include "FGBlueprintHologram.h"
#include "FGConstructDisqualifier.h"
#include "FGCentralStorageSubsystem.h"
#include "FGGameState.h"
#include "SF_ATAnchor.h"

// SML hooking for cost aggregation and blueprint construct
#include "Patching/NativeHookManager.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/Extend/SFExtendService.h"

// For chain actor rebuilding
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"

// Tiny linker anchor to ensure StaticClass() is referenced
static void SF_ForceUHT_SeeFGHolograms()
{
    (void)AFGConveyorBeltHologram::StaticClass();
    (void)AFGSplineHologram::StaticClass();
}

// Force UHT to include the anchor class in reflection graph
static void SF_ForceUHT_SeeAnchor()
{
    (void)USF_ATAnchor::StaticClass();
}

USFGameInstanceModule::USFGameInstanceModule()
{
	// Blueprint will set bRootModule = true
	// C++ class is Abstract and should not set root module flag
}

void USFGameInstanceModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	Super::DispatchLifecycleEvent(Phase);

	if (Phase == ELifecyclePhase::POST_INITIALIZATION)
	{
		UE_LOG(LogSmartFoundations, Display, TEXT("Smart! GameInstanceModule: POST_INITIALIZATION - Registering Smart! configuration"));

		// Register Smart! Configuration with SML for in-game menu access
		// Use the SmartConfigClass property set in the blueprint
		if (SmartConfigClass)
		{
			ModConfigurations.Add(SmartConfigClass);
			UE_LOG(LogSmartFoundations, Display, TEXT("✅ Smart! Configuration registered successfully - will appear in Mods menu"));
			UE_LOG(LogSmartFoundations, Display, TEXT("   Config Class: %s"), *SmartConfigClass->GetName());
			UE_LOG(LogSmartFoundations, Display, TEXT("   Config Path: %s"), *SmartConfigClass->GetPathName());
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("❌ SmartConfigClass not set in blueprint Class Defaults"));
			UE_LOG(LogSmartFoundations, Warning, TEXT("Config menu will not appear. Set SmartConfigClass to Smart_Config blueprint in SFGameInstanceModule_BP."));
		}

		// Register SML hook for cost aggregation (belt preview costs)
		RegisterCostAggregationHook();

		// Register SML hook for blueprint construct (chain actor rebuilding like AutoLink)
		RegisterBlueprintConstructHook();

		// Widget hooks will be registered here once Blueprint widget is created
	}
}

void USFGameInstanceModule::RegisterWidgetHooks()
{
	UE_LOG(LogSmartFoundations, Display, TEXT("Smart! GameInstanceModule: RegisterWidgetHooks called"));

	// Widget Blueprint Hooks require:
	// 1. A Blueprint widget asset in Content/SmartFoundations/UI/
	// 2. Target widget path (e.g., /Game/FactoryGame/Interface/UI/InGame/FGGameUI.FGGameUI_C)
	// 3. Named slot in target widget to inject into

	// Example implementation (once Blueprint widget exists):
	/*
	if (CounterWidgetClass)
	{
		UWidgetBlueprintLibrary::RegisterWidgetBlueprintHook(
			FSoftClassPath("/Game/FactoryGame/Interface/UI/InGame/FGGameUI.FGGameUI_C"),
			FName("CounterOverlaySlot"),
			CounterWidgetClass
		);
		UE_LOG(LogSmartFoundations, Display, TEXT("Registered counter widget hook into game HUD"));
	}
	*/

	UE_LOG(LogSmartFoundations, Warning,
		TEXT("Widget Blueprint Hooks require Blueprint asset - placeholder ready for future implementation"));
}

void USFGameInstanceModule::RegisterCostAggregationHook()
{
	UE_LOG(LogSmartFoundations, Display, TEXT("💰 Registering GetCost hook for belt preview cost aggregation"));

	// ========================================
	// SML Hook: GetCost for Conveyor Attachments
	// ========================================
	// We need an SML hook because the vanilla hologram Blueprint (Holo_ConveyorAttachment_C)
	// is used, not our custom C++ class. Hook intercepts vanilla GetCost() to add belt costs.
	// Vanilla ValidatePlacementAndCost() will then automatically handle affordability.

	// Hook AFGHologram::GetCost to inject belt preview costs
	// Signature must match SML's TCallScope pattern
	SUBSCRIBE_UOBJECT_METHOD(AFGHologram, GetCost, [](auto& scope, const AFGHologram* self, bool includeChildren)
	{
		// Call original GetCost implementation
		TArray<FItemAmount> BaseCost = scope(self, includeChildren);

		// Process conveyor attachment holograms (splitters/mergers)
		const AFGConveyorAttachmentHologram* Distributor = Cast<AFGConveyorAttachmentHologram>(self);
		if (Distributor)
		{
			// Try to get belt preview costs from auto-connect service
			if (UWorld* World = self->GetWorld())
			{
				if (USFSubsystem* Subsystem = USFSubsystem::Get(World))
				{
					if (USFAutoConnectService* AutoConnect = Subsystem->GetAutoConnectService())
					{
						TArray<FItemAmount> BeltCosts = AutoConnect->GetBeltPreviewsCost(Distributor);

						if (BeltCosts.Num() > 0)
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetCost hook: Adding %d belt cost item types"), BeltCosts.Num());

							// Merge belt costs using FactorySpawner's pattern
							for (const FItemAmount& BeltCost : BeltCosts)
							{
								if (!BeltCost.ItemClass) continue;

								FItemAmount* Existing = BaseCost.FindByPredicate([&](const FItemAmount& X) {
									return X.ItemClass == BeltCost.ItemClass;
								});
								if (Existing)
								{
									Existing->Amount += BeltCost.Amount;
								}
								else
								{
									BaseCost.Add(BeltCost);
								}
							}

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetCost hook: Returning %d item types total"), BaseCost.Num());
						}
					}
				}
			}
		}

		// Process power pole holograms
		else if (UWorld* World = self->GetWorld())
		{
			if (USFSubsystem* Subsystem = USFSubsystem::Get(World))
			{
				if (USFAutoConnectService* AutoConnect = Subsystem->GetAutoConnectService())
				{
					if (AutoConnect->IsPipelineJunctionHologram(self))
					{
						TArray<FItemAmount> PipeCosts = AutoConnect->GetPipePreviewsCost(self);

						if (PipeCosts.Num() > 0)
						{
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetCost hook: Adding %d pipe cost item types"), PipeCosts.Num());

							// Merge pipe costs using same pattern
							for (const FItemAmount& PipeCost : PipeCosts)
							{
								if (!PipeCost.ItemClass) continue;

								FItemAmount* Existing = BaseCost.FindByPredicate([&](const FItemAmount& X) {
									return X.ItemClass == PipeCost.ItemClass;
								});
								if (Existing)
								{
									Existing->Amount += PipeCost.Amount;
								}
								else
								{
									BaseCost.Add(PipeCost);
								}
							}

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 GetCost hook: Returning %d item types total (including pipes)"), BaseCost.Num());
						}
					}
				}
			}
		}

		return BaseCost;
	});

	// NOTE: ValidatePlacementAndCost hook removed in v24.2.0+
	// After switching to vanilla child hologram patterns, vanilla affordability checks handle everything correctly.
	// No custom cost validation needed - child holograms automatically aggregate costs via GetCost() override.

	UE_LOG(LogSmartFoundations, Display, TEXT("✅ GetCost hook registered - child hologram costs automatically aggregated by vanilla"));
}

void USFGameInstanceModule::RegisterBlueprintConstructHook()
{
	UE_LOG(LogSmartFoundations, Display, TEXT("⛓️ Registering AFGBlueprintHologram::Construct hook for chain actor rebuilding"));

	// ========================================
	// SML Hook: AFGBlueprintHologram::Construct (AFTER)
	// ========================================
	// Like AutoLink, we hook AFTER blueprint construction completes.
	// At this point:
	// - All children (including conveyors) have been spawned
	// - Conveyors are NOT YET in the subsystem's tick arrays
	// - We can safely do Remove→Connect→Add to rebuild chains
	//
	// This is the SAME timing AutoLink uses, and it's safe because
	// factory tick hasn't started on these conveyors yet.

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBlueprintHologram::Construct,
		GetMutableDefault<AFGBlueprintHologram>(),
		[](AActor* returnValue, AFGBlueprintHologram* hologram, TArray<AActor*>& out_children, FNetConstructionID NetConstructionID)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("⛓️ AFGBlueprintHologram::Construct AFTER: %s with %d children"),
				*hologram->GetName(), out_children.Num());

			// Get the world and subsystems
			UWorld* World = hologram->GetWorld();
			if (!World)
			{
				return;
			}

			AFGBuildableSubsystem* BuildableSubsystem = AFGBuildableSubsystem::Get(World);
			if (!BuildableSubsystem)
			{
				return;
			}

			// Collect all conveyors from children
			TArray<AFGBuildableConveyorBase*> BuiltConveyors;
			for (AActor* Child : out_children)
			{
				if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Child))
				{
					BuiltConveyors.Add(Conveyor);
				}
			}

			if (BuiltConveyors.Num() == 0)
			{
				return;  // No conveyors to process
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ HOOK: Found %d conveyors in blueprint children"), BuiltConveyors.Num());

			// Check if this is a Smart EXTEND build by looking for our subsystem/service
			USFSubsystem* SmartSubsystem = USFSubsystem::Get(World);
			USFExtendService* ExtendService = SmartSubsystem ? SmartSubsystem->GetExtendService() : nullptr;

			// For each conveyor that has connections established, do Remove→Add to rebuild chains
			// This is the AutoLink pattern - done DURING construction, BEFORE factory tick
			int32 RebuildCount = 0;
			for (AFGBuildableConveyorBase* Conveyor : BuiltConveyors)
			{
				if (!Conveyor || !Conveyor->IsValidLowLevel())
				{
					continue;
				}

				// Check if this conveyor has any connections
				UFGFactoryConnectionComponent* Conn0 = Conveyor->GetConnection0();
				UFGFactoryConnectionComponent* Conn1 = Conveyor->GetConnection1();

				bool bHasConnection = (Conn0 && Conn0->IsConnected()) || (Conn1 && Conn1->IsConnected());

				if (bHasConnection)
				{
					// AutoLink pattern: Remove → (connections already made) → Add
					// This triggers chain actor rebuild with proper unification
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ HOOK: Rebuilding chain for %s (Conn0=%s, Conn1=%s)"),
						*Conveyor->GetName(),
						Conn0 && Conn0->IsConnected() ? TEXT("connected") : TEXT("open"),
						Conn1 && Conn1->IsConnected() ? TEXT("connected") : TEXT("open"));

					BuildableSubsystem->RemoveConveyor(Conveyor);
					BuildableSubsystem->AddConveyor(Conveyor);
					RebuildCount++;
				}
			}

			if (RebuildCount > 0)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ HOOK: Rebuilt chains for %d conveyors"), RebuildCount);

				// Log chain status after rebuild
				TSet<AFGConveyorChainActor*> UniqueChains;
				for (AFGBuildableConveyorBase* Conveyor : BuiltConveyors)
				{
					if (Conveyor && Conveyor->IsValidLowLevel())
					{
						AFGConveyorChainActor* Chain = Conveyor->GetConveyorChainActor();
						if (Chain)
						{
							UniqueChains.Add(Chain);
						}
					}
				}

				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ HOOK: %d conveyors now belong to %d unique chain actors"),
					BuiltConveyors.Num(), UniqueChains.Num());

				for (AFGConveyorChainActor* Chain : UniqueChains)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⛓️ HOOK:   Chain %s has %d segments"),
						*Chain->GetName(), Chain->GetNumChainSegments());
				}
			}
		}
	);

	UE_LOG(LogSmartFoundations, Display, TEXT("✅ AFGBlueprintHologram::Construct hook registered - chain actors will rebuild during construction"));
}
