// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SFGameInstanceModule.h"
#include "SmartFoundations.h"

// Force UHT to parse these classes so AccessTransformers apply
#include "Hologram/FGHologram.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Hologram/FGConveyorPoleHologram.h"      // #341: parent hologram of a stackable-pole grid
#include "Holograms/Logistics/SFConveyorBeltHologram.h"  // #341: DrainStackBuiltConveyors
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
    (void)AFGHologram::StaticClass();
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
		UE_LOG(LogSmartFoundations, Log, TEXT("Smart! GameInstanceModule: POST_INITIALIZATION - Registering Smart! configuration"));

		// Smart! configuration is declared declaratively in SFGameInstanceModule_BP's
		// ModConfigurations array (EditDefaultsOnly) and is registered with the ConfigManager
		// automatically by UGameInstanceModule::RegisterDefaultContent during the INITIALIZATION
		// lifecycle phase (which runs before this POST_INITIALIZATION). Do NOT re-add it here:
		// a runtime ModConfigurations.Add is redundant and duplicates the entry in the live module
		// instance. The blueprint's ModConfigurations array is the single source of truth.
		// (The empty config menu under 1.2 was a separate issue, fixed in the Smart_Config asset:
		// its RootSection had bHidden=true, which the 1.2 Mods menu uses to skip the property tree.)

		// #348: cost-aggregation GetCost hook removed - auto-connect belts/pipes are child
		// holograms, so vanilla GetCost(includeChildren) already counts them (no manual add).

		// Register SML hook for blueprint construct (chain actor rebuilding like AutoLink)
		RegisterBlueprintConstructHook();

		// #341: Register SML hook on the stackable-pole parent Construct (in-frame chain registration)
		RegisterStackablePoleConstructHook();

	}
}

// #348: The GetCost aggregation hook was removed. Smart's auto-connect belts and pipe junctions
// are now child holograms (tagged SF_BeltAutoConnectChild / SF_PipeAutoConnectChild and AddChild'd
// to the distributor/junction), so vanilla AFGHologram::GetCost(includeChildren) - which the build
// gun's affordability path uses - already counts them. The old hook also added
// GetBeltPreviewsCost/GetPipePreviewsCost on top of that base cost, double-counting the auto-connect
// belt and pipe cost in the build-gun preview (placement charged 2x the dismantle refund). With
// the children counted by vanilla, that manual addition is redundant, so the hook is gone.

void USFGameInstanceModule::RegisterBlueprintConstructHook()
{
	UE_LOG(LogSmartFoundations, Verbose, TEXT("⛓️ Registering AFGBlueprintHologram::Construct hook for chain actor rebuilding"));

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
			UE_LOG(LogSmartFoundations, Verbose, TEXT("⛓️ AFGBlueprintHologram::Construct AFTER: %s with %d children"),
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

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ AFGBlueprintHologram::Construct hook registered - chain actors will rebuild during construction"));
}

void USFGameInstanceModule::RegisterStackablePoleConstructHook()
{
	// ========================================
	// SML Hook: AFGConveyorPoleHologram::Construct (AFTER)  [#341]
	// ========================================
	// A Smart stackable-pole grid is built by the parent pole hologram: its Super::Construct builds ALL
	// child poles and child belts (each belt wires its connectors by coincidence in its own
	// ConfigureComponents), then returns. That AFTER point is synchronous, all-wired, and BEFORE the
	// factory tick - the same timing Extend's blueprint hook relies on. We register the run's belts here
	// (in-frame), so vanilla builds ONE chain per series-run. Doing this off a deferred timer instead
	// ticks into garbage and crashes Factory_Tick (THESIS 6.16); the in-frame timing makes it safe.
	// NOTE: AFGConveyorPoleHologram does not override Construct, so the hooked method resolves to the
	// base AFGBuildableHologram::Construct and the handler parameter must be typed AFGBuildableHologram*.
	// The GetMutableDefault<AFGConveyorPoleHologram>() instance still narrows the vtable patch to pole
	// holograms (and subclasses), so this only fires for conveyor-pole parents.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGConveyorPoleHologram::Construct,
		GetMutableDefault<AFGConveyorPoleHologram>(),
		[](AActor* returnValue, AFGBuildableHologram* hologram, TArray<AActor*>& out_children, FNetConstructionID NetConstructionID)
		{
			if (!hologram)
			{
				return;
			}

			// Only the grid PARENT pole carries children; child poles built inside Super::Construct have
			// none, so this fires once per placement (not once per child pole).
			if (hologram->GetHologramChildren().Num() == 0)
			{
				return;
			}

			UWorld* World = hologram->GetWorld();
			AFGBuildableSubsystem* BuildableSubsystem = World ? AFGBuildableSubsystem::Get(World) : nullptr;
			if (!BuildableSubsystem)
			{
				return;
			}

			// Drain the belts this placement built + wired. Empty => not a stackable-belt placement.
			TArray<AFGBuildableConveyorBase*> StackBelts;
			ASFConveyorBeltHologram::DrainStackBuiltConveyors(StackBelts);
			if (StackBelts.Num() == 0)
			{
				return;
			}

			// In-frame, pre-tick Remove->Add (Extend's AutoLink pattern). The belts are already wired
			// (connect-by-coincidence) and registered as solo chains; Remove->Add re-registers them with
			// connections in place so vanilla unifies each series-run into one multi-segment chain.
			int32 Rebuilt = 0;
			for (AFGBuildableConveyorBase* Belt : StackBelts)
			{
				if (!Belt || !Belt->IsValidLowLevel())
				{
					continue;
				}
				UFGFactoryConnectionComponent* Conn0 = Belt->GetConnection0();
				UFGFactoryConnectionComponent* Conn1 = Belt->GetConnection1();
				const bool bHasConnection = (Conn0 && Conn0->IsConnected()) || (Conn1 && Conn1->IsConnected());
				if (bHasConnection)
				{
					BuildableSubsystem->RemoveConveyor(Belt);
					BuildableSubsystem->AddConveyor(Belt);
					++Rebuilt;
				}
			}

			UE_LOG(LogSmartFoundations, Log, TEXT("⛓️ #341 STACK POLE HOOK: in-frame rebuilt %d/%d stacked belt(s) on %s"),
				Rebuilt, StackBelts.Num(), *hologram->GetName());
		}
	);

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ AFGConveyorPoleHologram::Construct hook registered (#341 stacked chain registration)"));
}
