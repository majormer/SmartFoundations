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
#include "Hologram/FGConveyorPoleHologram.h"      // #341: belt-support parent hologram (covers stackable/wall/ceiling)
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

// MP Slice 0 (Phase 1): client construct chunk guard
#include "Equipment/FGBuildGunBuild.h"        // UFGBuildGunStateBuild::InternalConstructHologram / GetHologram
#include "Core/Helpers/SFNetworkHelper.h"     // FSFNetworkHelper::IsClient
#include "Engine/Engine.h"                     // GEngine on-screen message

// MP spec-based construction: class-agnostic hooks + RCO spec staging
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Subsystem/SFHologramHelperService.h"
#include "SFRCO.h"
#include "FGPlayerController.h"
#include "FGBlueprintProxy.h"   // server-side Smart Dismantle group for spec-built grids

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

		// #341: Register SML hook on the belt-support parent Construct (in-frame chain registration;
		// one hook covers stackable / wall / ceiling - see RegisterBeltSupportConstructHook).
		RegisterBeltSupportConstructHook();

		// MP Slice 0 (Phase 1): guard a client against committing an oversized scaled grid in one
		// construct RPC (the all-or-nothing drop + orphaned-preview bug). Backstop for the chunker below.
		RegisterClientConstructChunkGuardHook();

		// MP Slice 0 chunking: shrink an oversized client grid to a fit-in-one-RPC chunk at the fire handler,
		// before vanilla serializes (Increment 1 = single-chunk proof). See the method comment.
		RegisterClientGridChunkFireHook();

		// MP spec-based scaling construction (class-agnostic hook path - covers ALL scalable
		// buildables including BP hologram wrappers, no hologram swap). See the method comment.
		RegisterSpecConstructionHooks();

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

// #341: shared body for the belt-support parent Construct hooks (stackable / wall / ceiling).
// Runs at the parent hologram's Construct-AFTER: synchronous, all child belts built + wired, and BEFORE
// the factory tick - the timing Extend relies on. Registers the run's belts in-frame so vanilla builds one
// chain per series-run. The SAME RemoveConveyor+AddConveyor off a timer crashes Factory_Tick (THESIS 6.16);
// only the in-frame/pre-tick timing makes it safe.
static void SF_RegisterStackBuiltRunInFrame(AFGBuildableHologram* hologram, const TCHAR* HookLabel)
{
	if (!hologram)
	{
		return;
	}

	// Only the grid PARENT carries children; child poles built inside Super::Construct have none, so this
	// fires once per placement (not once per child pole/support).
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

	// Drain the belts this placement built + wired. Empty => not a belt-support placement.
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

	UE_LOG(LogSmartFoundations, Log, TEXT("⛓️ #341 %s HOOK: in-frame rebuilt %d/%d belt-support belt(s) on %s"),
		HookLabel, Rebuilt, StackBelts.Num(), *hologram->GetName());
}

void USFGameInstanceModule::RegisterBeltSupportConstructHook()
{
	// SML Hook: AFGConveyorPoleHologram::Construct (AFTER) - covers ALL belt-support pole grids:
	// stackable poles, wall poles, and ceiling mounts.  [#341]
	//
	// IMPORTANT (verified live 2026-06-08): AFGConveyorPoleHologram does NOT override Construct, so the hooked
	// method resolves to the base AFGBuildableHologram::Construct, and SML's vtable patch is broad enough that
	// this single hook fires for sibling belt-support hologram classes too - confirmed by the log firing on
	// Holo_ConveyorWallAttachment_C and Holo_ConveyorCeilingAttachment_C, not just stackable poles. So one hook
	// suffices; a separate AFGWallAttachmentHologram hook was redundant and removed. The handler is typed
	// AFGBuildableHologram* to match the base method. The GetHologramChildren()>0 + DrainStackBuiltConveyors()
	// guards make it a cheap no-op for any other hologram the broad patch may also fire for.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGConveyorPoleHologram::Construct,
		GetMutableDefault<AFGConveyorPoleHologram>(),
		[](AActor* returnValue, AFGBuildableHologram* hologram, TArray<AActor*>& out_children, FNetConstructionID NetConstructionID)
		{
			SF_RegisterStackBuiltRunInFrame(hologram, TEXT("BELT-SUPPORT"));
		}
	);

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ AFGConveyorPoleHologram::Construct hook registered (#341 belt-support chain registration: stackable/wall/ceiling)"));
}

// MP Slice 0 (Phase 1) - construct chunk guard.
// CONFIRMED seam (live test 2026-06-08): the client builds the construct message and calls
// Server_ConstructHologram DIRECTLY (the earlier InternalConstructHologram hook never fired). The live
// failure is "LogNet: Error: Can't send function 'Server_ConstructHologram' ...: Failed to serialize
// properties" - the oversized FConstructHologramMessage.SerializedHologramData blob (one TArray<uint8>
// holding the whole hologram tree) is too large to marshal, so the reliable RPC is silently dropped ->
// all-or-nothing + orphaned previews (no Client_OnBuildableFailedConstruction because the server never
// processed it). We hook Server_ConstructHologram on the client and read the ACTUAL serialized byte size,
// so the guard is robust across building types (no per-type child-count guessing). Empirically ~137
// foundations / ~135 constructors fit; that corresponds to ~64KB of SerializedHologramData. We cancel a
// Smart grid construct whose blob exceeds a margin below that. Phase 2 will chunk instead of refusing.
static constexpr int32 SF_MP_CONSTRUCT_MAX_BYTES = 60000; // cancel a Smart-grid construct above this
static constexpr int32 SF_MP_CONSTRUCT_LOG_BYTES = 20000; // log any Smart-grid construct above this (capture real sizes)

void USFGameInstanceModule::RegisterClientConstructChunkGuardHook()
{
	SUBSCRIBE_METHOD(
		UFGBuildGunStateBuild::Server_ConstructHologram,
		[](auto& scope, UFGBuildGunStateBuild* self, FNetConstructionID clientNetConstructID, FConstructHologramMessage data)
		{
			if (!self)
			{
				return;
			}

			// Only a true network client (NM_Client) marshals the construct over the wire and can hit the
			// serialize-too-large failure. Host / dedicated-server authority / single-player construct locally.
			UWorld* World = self->GetWorld();
			if (!World || !FSFNetworkHelper::IsClient(World))
			{
				return;
			}

			// Scope strictly to Smart scaled grids: only act when the active hologram has Smart grid children
			// (tagged SF_GridChild). This leaves vanilla single placements AND blueprints completely untouched.
			AFGHologram* Holo = self->GetHologram();
			if (!Holo)
			{
				return;
			}
			static const FName GridChildTag(TEXT("SF_GridChild"));
			bool bIsSmartGrid = false;
			for (const AFGHologram* Child : Holo->GetHologramChildren())
			{
				if (Child && Child->Tags.Contains(GridChildTag))
				{
					bIsSmartGrid = true;
					break;
				}
			}
			if (!bIsSmartGrid)
			{
				return; // not a Smart scaled grid -> vanilla path (incl. blueprints)
			}

			const int32 Bytes = data.SerializedHologramData.Num();

			// Diagnostic: capture the real serialized size near/over the ceiling (confirms the byte limit).
			if (Bytes >= SF_MP_CONSTRUCT_LOG_BYTES)
			{
				UE_LOG(LogSmartFoundations, Display,
					TEXT("[MP-CHUNK] Smart-grid client construct: SerializedHologramData=%d bytes (NumBits=%lld), cancel threshold=%d."),
					Bytes, (long long)data.NumBits, SF_MP_CONSTRUCT_MAX_BYTES);
			}

			if (Bytes <= SF_MP_CONSTRUCT_MAX_BYTES)
			{
				return; // fits one RPC -> let it send (works in MP)
			}

			// Oversized: the RPC would fail to serialize and be dropped (all-or-nothing) + orphan the previews.
			// Cancel the send. The active hologram + preview children stay live so the player can scale down.
			UE_LOG(LogSmartFoundations, Display,
				TEXT("[MP-CHUNK] Blocked oversized client construct: %d bytes (> %d). Single-RPC construct would be dropped (Failed to serialize properties). Cancelled before send; preview kept."),
				Bytes, SF_MP_CONSTRUCT_MAX_BYTES);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Orange,
					FString::Printf(TEXT("Smart!: placement too large for multiplayer (%d KB). Build in smaller sections."),
						Bytes / 1024));
			}

			scope.Cancel(); // suppress the doomed Server_ConstructHologram send.
		}
	);

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ Client construct chunk-guard hook registered (MP Slice 0 Phase 1, Server_ConstructHologram)"));
}

// MP Slice 0 SAFETY GUARD. A Smart grid above this many total cells will not fit one 64KB
// Server_ConstructHologram (empirical ceiling ~135 cells / 65536 bytes). Building such a grid on a CLIENT is
// not safely achievable today: re-firing the build gun never constructs (single-construct-per-fire state
// machine, proven), and hand-building the construct message CRASHES the dedicated server (proven - server
// fatal in UNetDriver::InternalTickDispatch). So we REFUSE an oversized client grid at the fire handler,
// BEFORE anything is serialized or sent: the grid stays live so the player can scale down and place in
// smaller sections. This is a temporary guard, NOT a multiplayer feature - large-grid MP placement is part
// of the future complete multiplayer solution (which cannot ship partially; see AGENTS.md).
static constexpr int32 SF_MP_OVERSIZED_CELLS = 130; // refuse a client grid above this (total cells incl. parent)

void USFGameInstanceModule::RegisterClientGridChunkFireHook()
{
	SUBSCRIBE_METHOD(
		UFGBuildGunStateBuild::InternalExecuteDuBuildStepInput,
		[](auto& scope, UFGBuildGunStateBuild* self, bool isInputFromARelease)
		{
			if (!self)
			{
				return;
			}

			UWorld* World = self->GetWorld();
			if (!World || !FSFNetworkHelper::IsClient(World))
			{
				return; // only a network client serializes the construct over the wire
			}

			AFGHologram* Holo = self->GetHologram();
			if (!Holo)
			{
				return;
			}

			// Count Smart grid child holograms (tagged SF_GridChild). +1 for the parent/origin cell.
			static const FName GridChildTag(TEXT("SF_GridChild"));
			int32 GridChildCount = 0;
			for (const AFGHologram* Child : Holo->GetHologramChildren())
			{
				if (Child && Child->Tags.Contains(GridChildTag))
				{
					++GridChildCount;
				}
			}

			// [MP-SPEC] Spec path (class-agnostic): when enabled and the buildable is Smart-scalable
			// (size registry), the CLIENT commits the grid as a compact spec:
			//   1. capture the spec from the live grid (invalid when no grid - an explicit CLEAR),
			//   2. stage it on the server via the USFRCO reliable RPC (overwrite semantics, so a
			//      stale spec from an earlier failed construct can never leak into a later fire),
			//   3. destroy the local preview children through the grid-spawner's own proven cleanup
			//      (no strip/restore around vanilla serialization - that timing caused the orphan
			//      bug and the interface-virtual hook crash; the sticky grid counters regenerate the
			//      preview on the next hologram automatically, matching legacy UX),
			//   4. let the fire proceed: it serializes a clean 1-cell hologram (O(1) message); the
			//      server's Construct hook consumes the staged spec and expands the grid.
			if (SFScalingSpecExpansion::IsSpecConstructionEnabled())
			{
				USFBuildableSizeRegistry::Initialize();
				if (USFBuildableSizeRegistry::GetProfile(Holo->GetBuildClass()).bSupportsScaling)
				{
					FSFScalingSpec Spec; // bValid=false by default = explicit clear
					if (GridChildCount > 0)
					{
						SFScalingSpecExpansion::CaptureScalingSpec(Holo, Spec);
					}

					// Stage (or clear) the spec server-side BEFORE the construct RPC goes out.
					bool bStaged = false;
					if (APawn* InstigatorPawn = Holo->GetConstructionInstigator())
					{
						if (AFGPlayerController* PC = Cast<AFGPlayerController>(InstigatorPawn->GetController()))
						{
							if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
							{
								RCO->Server_StageScalingSpec(Spec);
								bStaged = true;
							}
						}
					}

					if (Spec.bValid && bStaged)
					{
						UE_LOG(LogSmartFoundations, Display,
							TEXT("[MP-SPEC] Client fire: staged spec (%d cells of %s), destroying %d preview children; construct message will be O(1)."),
							Spec.CellCount(), *GetNameSafe(*Spec.BuildClass), GridChildCount);

						// Destroy the preview grid through the helper's own cleanup (tracking stays
						// consistent; the sticky counters regenerate the grid on the next hologram).
						if (USFSubsystem* SS = USFSubsystem::Get(World))
						{
							if (FSFHologramHelperService* Helper = SS->GetHologramHelper())
							{
								Helper->DestroyAllChildren();
							}
						}
					}
					else if (!bStaged && Spec.bValid)
					{
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("[MP-SPEC] Client fire: could not reach USFRCO to stage the spec - falling through to the legacy path/guard."));
					}

					if (Spec.bValid && bStaged)
					{
						return; // fire proceeds with the (now childless) hologram
					}
					// else: no grid (clear staged) or no RCO -> fall through to legacy guard logic
				}
			}

			if (GridChildCount == 0)
			{
				return; // not a Smart scaled grid
			}

			const int32 TotalCells = GridChildCount + 1;
			if (TotalCells <= SF_MP_OVERSIZED_CELLS)
			{
				return; // fits one construct -> untouched vanilla path (builds fine in MP)
			}

			// Oversized: refuse the fire BEFORE vanilla serializes/sends. The active hologram + preview stay
			// live (no teardown, no orphaned previews, no dropped RPC, no server crash). The player scales
			// down and places in smaller sections.
			UE_LOG(LogSmartFoundations, Display,
				TEXT("[MP-CHUNK] Refused oversized client grid: %d cells (> %d). One construct can't carry this many over the wire safely; build in smaller sections."),
				TotalCells, SF_MP_OVERSIZED_CELLS);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Orange,
					FString::Printf(TEXT("Smart!: grid too large for multiplayer (%d cells, max ~%d). Build in smaller sections."),
						TotalCells, SF_MP_OVERSIZED_CELLS));
			}

			scope.Cancel(); // suppress the fire -> nothing is serialized or sent; the grid stays live.
		}
	);

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ Client grid oversized-guard fire-hook registered (MP Slice 0 safety, InternalExecuteDuBuildStepInput)"));
}

void USFGameInstanceModule::RegisterSpecConstructionHooks()
{
	static const FName GridChildTag(TEXT("SF_GridChild"));

	// Count the Smart grid children currently attached to a hologram (the strip/expand targets).
	auto CountGridChildren = [](AFGHologram* Holo) -> int32
	{
		int32 Count = 0;
		for (AFGHologram* Child : Holo->mChildren)
		{
			if (Child && Child->Tags.Contains(GridChildTag))
			{
				++Count;
			}
		}
		return Count;
	};

	// CDOs for virtual-method body resolution. SUBSCRIBE_METHOD on a virtual resolves to a vcall
	// thunk and SML's hook install CRASHES at startup (live finding 2026-06-09: FatalError in
	// FNativeHookManagerInternal::RegisterHookFunction). SUBSCRIBE_METHOD_VIRTUAL resolves the
	// actual body from the CDO's vtable - and hooking the BASE body fires for the entire Super
	// chain (same mechanism the belt-support Construct hook above relies on).
	//
	// IMPORTANT: only PRIMARY-class virtuals may be hooked. SerializeConstructMessage (and the
	// Pre/Post message methods) come from IFGConstructionMessageInterface - a SECONDARY base - and
	// hooking them delivers an interface-adjusted `this` (live crash 2026-06-09: self off by the
	// interface offset, EXCEPTION_ACCESS_VIOLATION in the first member read). The spec therefore
	// crosses the wire via USFRCO::Server_StageScalingSpec (staged per player at fire time by the
	// fire hook above), NOT by injecting into the construct message.
	AFGHologram* HologramCDO = GetMutableDefault<AFGHologram>();
	AFGBuildableHologram* BuildableHologramCDO = GetMutableDefault<AFGBuildableHologram>();

	// Resolve the staged spec for a constructing/costing hologram (server side).
	auto FindStagedSpec = [](const AFGHologram* Holo, FSFScalingSpec& OutSpec, bool bConsume) -> bool
	{
		AFGHologram* MutableHolo = const_cast<AFGHologram*>(Holo);
		USFSubsystem* SS = USFSubsystem::Get(MutableHolo->GetWorld());
		if (!SS)
		{
			return false;
		}
		APawn* Instigator = MutableHolo->GetConstructionInstigator();
		UClass* BuildClass = MutableHolo->GetBuildClass();
		return bConsume
			? SS->ConsumeScalingSpecForInstigator(Instigator, BuildClass, OutSpec)
			: SS->PeekScalingSpecForInstigator(Instigator, BuildClass, OutSpec);
	};

	// ── Hook A: cost. Charged server-side BEFORE Construct, when the grid children do not exist
	// yet - scale the uniform per-cell cost by the staged cell count. Fires at the
	// AFGHologram::GetCost base body (primary virtual; the scalable parents' classes do not
	// override GetCost, and spline types that do never carry a staged spec).
	SUBSCRIBE_METHOD_VIRTUAL(AFGHologram::GetCost, HologramCDO,
		[=](auto& scope, const AFGHologram* self, bool includeChildren)
		{
			if (!self || !includeChildren || !SFScalingSpecExpansion::IsSpecConstructionEnabled())
			{
				return;
			}
			AFGHologram* MutableSelf = const_cast<AFGHologram*>(self);
			if (!MutableSelf->HasAuthority())
			{
				return; // client cost comes from its real preview children via vanilla aggregation
			}
			FSFScalingSpec Spec;
			if (!FindStagedSpec(self, Spec, /*bConsume=*/false) || CountGridChildren(MutableSelf) > 0)
			{
				return; // no staged spec (or children already present) -> vanilla cost
			}

			TArray<FItemAmount> Cost = scope(self, includeChildren);
			const int32 Cells = Spec.CellCount();
			for (FItemAmount& Item : Cost)
			{
				Item.Amount *= Cells;
			}
			scope.Override(Cost);
		});

	// ── Hook B: expansion + group bookkeeping. Fires at the AFGBuildableHologram::Construct body
	// (primary virtual) - inside the Super chain of every buildable hologram, before the base
	// child-construct loop - on the server, after Server_ConstructHologram validation has already
	// passed on the childless parent. Consumes the staged spec (matched by instigator + build
	// class), expands, runs the original, then registers ALL built actors into one
	// AFGBlueprintProxy so Smart Dismantle group-dismantle works on spec-built grids. The proxy is
	// created SERVER-side (its mBuildables array is replicated, so the client group UI sees it and
	// dismantle is server-authoritative - the legacy client-side proxy from OnActorSpawned cannot
	// cover server-built actors).
	SUBSCRIBE_METHOD_VIRTUAL(AFGBuildableHologram::Construct, BuildableHologramCDO,
		[=](auto& scope, AFGBuildableHologram* self, TArray<AActor*>& out_children, FNetConstructionID constructionID)
		{
			if (!self || !self->HasAuthority() || !SFScalingSpecExpansion::IsSpecConstructionEnabled())
			{
				return;
			}
			FSFScalingSpec Spec;
			if (!FindStagedSpec(self, Spec, /*bConsume=*/true))
			{
				return; // no staged spec for this instigator/class (e.g. a child in the loop)
			}
			if (CountGridChildren(self) == 0)
			{
				SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(self, Spec, self->GetRecipe());
			}

			// Run the original construct now (parent + expanded children) so we can group the
			// resulting actors. out_children is filled by the vanilla child-construct loop.
			AActor* BuiltParent = scope(self, out_children, constructionID);

			// Group everything that was built into one blueprint proxy (Smart Dismantle group).
			TArray<AFGBuildable*> BuiltBuildables;
			if (AFGBuildable* ParentBuildable = Cast<AFGBuildable>(BuiltParent))
			{
				BuiltBuildables.Add(ParentBuildable);
			}
			for (AActor* ChildActor : out_children)
			{
				if (AFGBuildable* ChildBuildable = Cast<AFGBuildable>(ChildActor))
				{
					BuiltBuildables.AddUnique(ChildBuildable);
				}
			}

			if (BuiltBuildables.Num() > 1)
			{
				UWorld* World = self->GetWorld();
				FActorSpawnParameters ProxyParams;
				ProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				AFGBlueprintProxy* GroupProxy = World ? World->SpawnActor<AFGBlueprintProxy>(
					AFGBlueprintProxy::StaticClass(),
					BuiltBuildables[0]->GetActorTransform(),
					ProxyParams) : nullptr;

				if (GroupProxy)
				{
					for (AFGBuildable* Buildable : BuiltBuildables)
					{
						if (!Buildable->GetBlueprintProxy())
						{
							Buildable->SetBlueprintProxy(GroupProxy);
							GroupProxy->RegisterBuildable(Buildable);
						}
					}
					UE_LOG(LogSmartFoundations, Display,
						TEXT("[MP-SPEC] Grouped %d spec-built buildables into blueprint proxy %s (Smart Dismantle group)."),
						BuiltBuildables.Num(), *GroupProxy->GetName());
				}
				else
				{
					UE_LOG(LogSmartFoundations, Warning,
						TEXT("[MP-SPEC] Could not spawn blueprint proxy for spec-built group (%d buildables)."),
						BuiltBuildables.Num());
				}
			}

			scope.Override(BuiltParent);
		});

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ Spec-construction hooks registered (GetCost / Construct - class-agnostic; spec staged via USFRCO)"));
}
