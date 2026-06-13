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
#include "Hologram/FGPoleHologram.h"              // [MP-SPEC] multi-step gate: pole height step
#include "Hologram/FGFloodlightHologram.h"        // [MP-SPEC] multi-step gate: floodlight angle step
#include "Hologram/FGStandaloneSignHologram.h"    // [MP-SPEC] multi-step gate: sign height step
#include "Holograms/Logistics/SFConveyorBeltHologram.h"  // #341: DrainStackBuiltConveyors
#include "FGConstructDisqualifier.h"
#include "FGCentralStorageSubsystem.h"
#include "FGGameState.h"
#include "Core/SF_ATAnchor.h"

// SML hooking for cost aggregation and blueprint construct
#include "Patching/NativeHookManager.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFChainActorService.h"  // [CHAIN-FIX] post-construct chain-hygiene sweep
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/Extend/SFExtendService.h"

// MP Slice 0 (Phase 1): client construct chunk guard
#include "Equipment/FGBuildGunBuild.h"        // UFGBuildGunStateBuild::InternalConstructHologram / GetHologram
#include "Core/Net/SFNetworkHelper.h"     // FSFNetworkHelper::IsClient
#include "Engine/Engine.h"                     // GEngine on-screen message

// MP spec-based construction: class-agnostic hooks + RCO spec staging
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Data/SFHologramDataRegistry.h"   // [EXTEND-MP] JsonCloneId wiring anchors post-construct
#include "Subsystem/SFHologramHelperService.h"
#include "Core/Net/SFRCO.h"
#include "FGPlayerController.h"
#include "FGBlueprintProxy.h"   // server-side Smart Dismantle group for spec-built grids

// For chain actor rebuilding
#include "Buildables/FGBuildableFactory.h"   // [EXTEND-MP] commit wiring pass anchor
#include "TimerManager.h"                    // [EXTEND-MP] next-tick commit wiring pass
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "Hologram/FGWallAttachmentHologram.h"        // [#364-MP] wall-support server re-validation
#include "Buildables/FGBuildableBlueprintDesigner.h"  // [#365-MP] designer containment re-derive
#include "EngineUtils.h"                              // [#365-MP] TActorIterator over designers

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


// [MP-SPEC] The blueprint proxy for the spec-construct currently executing on the server.
// Set by the Construct hook around scope() (construction is synchronous + single-threaded);
// the ConfigureActor hook assigns it to every buildable configured inside that window. The
// assignment MUST happen pre-BeginPlay: lightweight-eligible buildables (foundations etc.)
// convert to lightweight instances in BeginPlay and only then does vanilla transfer the proxy
// membership to replicated lightweight indices (RegisterLightweightInstance) - a proxy assigned
// after Construct ends up holding soon-destroyed temp actors and self-destructs (live finding
// 2026-06-09: "no grouping persists" on spec-built foundation grids).
static TWeakObjectPtr<AFGBlueprintProxy> GSFActiveSpecGroupProxy;

void USFGameInstanceModule::RegisterSpecConstructionHooks()
{
	static const FName GridChildTag(TEXT("SF_GridChild"));

	// [CHAIN-FIX] Stale tick-group entry guard. A conveyor whose cached mConveyorBucketID points
	// at the WRONG tick group makes vanilla RemoveConveyor silently fail in shipping (the assert
	// is compiled out) - the belt stays in some OTHER TG->Conveyors array, gets destroyed by the
	// dismantle, GC frees the memory ~1-2 min later, and TickFactoryActors' ParallelFor calls
	// through the freed pointer (EXCEPTION_ACCESS_VIOLATION at FGBuildableSubsystem.cpp:644 -
	// reproduced three times on 2026-06-10, each ~2 min after building/dismantling auto-connect
	// belts). AFTER vanilla RemoveConveyor runs, sweep every tick group and force-remove any
	// lingering entry for this belt. Plain SUBSCRIBE_METHOD is correct: RemoveConveyor is a
	// non-virtual member (the no-plain-subscribe rule applies to virtuals only).
	SUBSCRIBE_METHOD(
		AFGBuildableSubsystem::RemoveConveyor,
		[](auto& scope, AFGBuildableSubsystem* self, AFGBuildableConveyorBase* conveyor)
		{
			scope(self, conveyor);
			if (!self || !conveyor)
			{
				return;
			}
			// Sweep lives in the chain service (mConveyorTickGroup access grant).
			if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
			{
				if (USFChainActorService* ChainSvc = SS->GetChainActorService())
				{
					ChainSvc->RemoveConveyorFromAllTickGroups(conveyor);
				}
			}
		});

	// [CHAIN-FIX] Pre-tick integrity scrub (permanent safety net, ~2us for ~2k entries): validate
	// mFactoryBuildings (+ groups) immediately before vanilla's ParallelFor walks them. A corrupt
	// entry is removed (the server survives the tick) and logged with its index + valid neighbors.
	// Kept after the 2026-06-10 wild-free incident (GetCost hook Override-without-invoke, fixed):
	// this class of bug otherwise kills dedicated servers minutes after the corrupting write.
	SUBSCRIBE_METHOD(
		AFGBuildableSubsystem::TickFactory,
		[](auto& scope, AFGBuildableSubsystem* self, float dt, ELevelTick TickType)
		{
			if (self && self->HasAuthority())
			{
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
				{
					if (USFChainActorService* ChainSvc = SS->GetChainActorService())
					{
						ChainSvc->ScrubFactoryTickArrays();
					}
				}
			}
		});

	// [CHAIN-FIX] Destruction chokepoint guard, ALL buildables. cdb on the 5th freed-pointer tick
	// AV (2026-06-10) identified the crashing loop: TickFactoryActors' lambda walks
	// mFactoryBuildings (subsystem +0x3C0, confirmed via PDB) and virtual-calls each entry - the
	// freed entry sat at the array END (= most recently registered), and the conveyor-only guards
	// stayed silent, so a NON-conveyor factory buildable is destroyed by some path that skips
	// vanilla RemoveBuildable. EndPlay fires for every destruction path; running AFTER the
	// original (scope first) means vanilla's own cleanup has happened - anything still left in
	// mFactoryBuildings/groups or the conveyor tick groups is a true leak: force-removed and
	// logged with name + reason, naming the leaking path on the next repro. AFGBuildable
	// overrides EndPlay (primary-class virtual): hooking its body via the CDO fires for the
	// whole Super chain - every buildable, conveyors included.
	AFGBuildable* BuildableCDO = GetMutableDefault<AFGBuildable>();

	// [NULL-WIRE GUARD] Vanilla Dismantle_Implementation walks every circuit connection's wire
	// list and calls Execute_Dismantle on each entry UNGUARDED - a null entry (a wire destroyed
	// without disconnecting, or a SaveGame wire reference that failed to load) is an instant
	// "Assertion failed: O != 0" (live SP crash 2026-06-11: dismantling an Extended blueprint
	// buildable). Scrub null/dead entries BEFORE the original runs. Void return - no by-value
	// return-slot concerns; scope() invoked after the scrub.
	SUBSCRIBE_METHOD_VIRTUAL(AFGBuildable::Dismantle_Implementation, BuildableCDO,
		[](auto& scope, AFGBuildable* self)
		{
			if (self && self->HasAuthority())
			{
				const int32 Scrubbed = USFChainActorService::ScrubNullWireEntries(self);
				if (Scrubbed > 0)
				{
					UE_LOG(LogSmartFoundations, Warning,
						TEXT("[NULL-WIRE GUARD] %s (%s): scrubbed %d null/dead wire entr%s before dismantle (vanilla would have asserted)."),
						*self->GetName(), *self->GetClass()->GetName(), Scrubbed,
						Scrubbed == 1 ? TEXT("y") : TEXT("ies"));
				}
			}
			scope(self);
		});

	SUBSCRIBE_METHOD_VIRTUAL(AFGBuildable::EndPlay, BuildableCDO,
		[](auto& scope, AFGBuildable* self, const EEndPlayReason::Type endPlayReason)
		{
			scope(self, endPlayReason);
			if (!self || !self->HasAuthority())
			{
				return;
			}
			UWorld* World = self->GetWorld();
			USFSubsystem* SS = World ? USFSubsystem::Get(World) : nullptr;
			USFChainActorService* ChainSvc = SS ? SS->GetChainActorService() : nullptr;
			if (!ChainSvc)
			{
				return;
			}
			int32 Removed = ChainSvc->RemoveBuildableFromFactoryTickArrays(self);
			if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(self))
			{
				Removed += ChainSvc->RemoveConveyorFromAllTickGroups(Conveyor);
			}
			if (Removed > 0)
			{
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("[CHAIN-DIAG] EndPlay guard: %s (%s) died (reason=%d) while STILL in %d factory-tick entr%s - leak path identified, freed-pointer tick AV prevented."),
					*self->GetName(), *GetNameSafe(self->GetClass()), static_cast<int32>(endPlayReason),
					Removed, Removed == 1 ? TEXT("y") : TEXT("ies"));
			}
		});

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

	// [EXTEND-MP] Resolve the staged Extend commit for a constructing/costing hologram.
	auto FindStagedExtendCommit = [](const AFGHologram* Holo, FSFExtendCommitSpec& OutSpec, bool bConsume) -> bool
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
			? SS->ConsumeExtendCommitForInstigator(Instigator, BuildClass, OutSpec)
			: SS->PeekExtendCommitForInstigator(Instigator, BuildClass, OutSpec);
	};

	// ── [#364-MP] Wall-attachment re-validation for staged client fires. The server re-runs
	// CheckValidPlacement on the deserialized hologram inside Server_ConstructHologram, but
	// mSnappedBuilding does not survive the trip for these fires - the server-side hologram
	// reports FGCDMustSnapWall and the whole construct is refused (live 2026-06-11: every scaled
	// wall pipeline support fire on the Windows dedi). The CLIENT already validated the wall snap
	// (the build gun will not fire a red preview), and a staged spec exists ONLY for a Smart
	// client fire - the same trust model the spec expansion itself uses. Strip exactly that one
	// disqualifier; every other server-side check (overlap, clearance, affordability) stays.
	// Hooked at the AFGWallAttachmentHologram body so all wall-mount families are covered
	// (pipeline wall supports, conveyor wall mounts).
	AFGWallAttachmentHologram* WallAttachmentCDO = GetMutableDefault<AFGWallAttachmentHologram>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGWallAttachmentHologram::CheckValidPlacement, WallAttachmentCDO,
		[=](auto& scope, AFGWallAttachmentHologram* self)
		{
			scope(self);
			if (!self || !self->HasAuthority())
			{
				return;
			}
			FSFScalingSpec StagedSpec;
			if (!FindStagedSpec(self, StagedSpec, /*bConsume=*/false))
			{
				return; // not a staged Smart client fire - vanilla/SP validation untouched
			}
			TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
			self->GetConstructDisqualifiers(Disqualifiers);
			if (Disqualifiers.Remove(UFGCDMustSnapWall::StaticClass()) > 0)
			{
				self->ResetConstructDisqualifiers();
				for (const TSubclassOf<UFGConstructDisqualifier>& Disqualifier : Disqualifiers)
				{
					self->AddConstructDisqualifier(Disqualifier);
				}
				UE_LOG(LogSmartFoundations, Display,
					TEXT("[MP-SPEC] %s: cleared server-side FGCDMustSnapWall for a staged client fire (client validated the wall snap; %d other disqualifier(s) kept)."),
					*self->GetName(), Disqualifiers.Num());
			}
		});

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

			// [EXTEND-MP] A staged Extend commit overrides with the EXACT preview cost captured
			// client-side (parent + all clone children) - the childless server parent would
			// otherwise charge the bare factory only.
			//
			// THE 12-CRASH WILD-FREE BUG LIVED HERE (root-caused via hardware watchpoint,
			// 2026-06-10): scope.Override() WITHOUT a prior scope() invocation. For a hooked
			// method returning TArray BY VALUE, SML's invoker (ApplyCallUserTypeByValue)
			// move-ASSIGNS the override into the return slot - and if the original was never
			// invoked, that slot is UNINITIALIZED, so TArray::operator= frees whatever garbage
			// Data pointer the stack held. In CheckCanAfford's frame (dedi mirror build gun,
			// costs ON, every tick while aiming) that garbage was the AIMED-AT BUILDING's
			// address: FMemory::Free(liveActor) -> freelist scribble over its vtable -> the
			// freed-pointer factory-tick AV minutes later. RULE: NEVER Override a by-value
			// return without invoking scope() first to construct the slot (the scaling branch
			// below always did - which is why scaled costs never crashed).
			FSFExtendCommitSpec ExtendSpec;
			if (FindStagedExtendCommit(self, ExtendSpec, /*bConsume=*/false))
			{
				if (MutableSelf->GetHologramChildren().Num() == 0 && ExtendSpec.Cost.Num() > 0)
				{
					scope(self, includeChildren); // initialize the return slot FIRST
					scope.Override(ExtendSpec.Cost);
				}
				return;
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

			// [MP-334] Charge the staged auto-connect belts too: exact vanilla length-based
			// preview costs, summed client-side at capture. Without this the server-built plan
			// belts would be free (they join mChildren only inside Construct, after costing).
			for (const FItemAmount& BeltItem : Spec.ConduitPlanCost)
			{
				bool bMerged = false;
				for (FItemAmount& Item : Cost)
				{
					if (Item.ItemClass == BeltItem.ItemClass)
					{
						Item.Amount += BeltItem.Amount;
						bMerged = true;
						break;
					}
				}
				if (!bMerged)
				{
					Cost.Add(BeltItem);
				}
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
			// [#365] Universal designer propagation at the commit seam. The constructing
			// hologram reliably carries the Blueprint Designer context HERE (vanilla updates
			// it while aiming; activation-time stamping on clone spawn can run too early,
			// before the aim has tagged the hologram - live find: Extend's parent registered
			// with the designer but every clone built untracked). Vanilla's child-construct
			// loop does NOT propagate parent->child, so stamp every child hologram now - the
			// last moment before their buildables spawn. Covers Smart grids, Extend clones,
			// and conduit previews alike; runs regardless of the spec-construction toggle.
			if (self && self->HasAuthority())
			{
				// [#365-MP] Designer re-derivation for staged client fires. mBlueprintDesigner does
				// not reliably survive the construct message for these fires (live 2026-06-11:
				// client-scaled machines inside a designer built with NO designer context server-side
				// - "Attempt to connect between missmatched blueprint designers" on every conduit, and
				// nothing registered into the blueprint). The designer is an ordinary replicated actor
				// with an authoritative containment test, so re-derive it from the parent's location.
				// Gated to staged fires; a no-op when the reference crossed or outside any designer.
				if (self->GetBlueprintDesigner() == nullptr && SFScalingSpecExpansion::IsSpecConstructionEnabled())
				{
					FSFScalingSpec PeekSpec;
					FSFExtendCommitSpec PeekCommit;
					if (FindStagedSpec(self, PeekSpec, /*bConsume=*/false) ||
					    FindStagedExtendCommit(self, PeekCommit, /*bConsume=*/false))
					{
						const FVector ParentLocation = self->GetActorLocation();
						for (TActorIterator<AFGBuildableBlueprintDesigner> It(self->GetWorld()); It; ++It)
						{
							if (*It && It->IsLocationInsideDesigner(ParentLocation))
							{
								self->SetInsideBlueprintDesigner(*It);
								UE_LOG(LogSmartFoundations, Display,
									TEXT("[MP-SPEC] %s: re-derived Blueprint Designer context (%s) from containment for a staged client fire (reference did not cross the construct message)."),
									*self->GetName(), *It->GetName());
								break;
							}
						}
					}
				}

				if (AFGBuildableBlueprintDesigner* Designer = self->GetBlueprintDesigner())
				{
					for (AFGHologram* ChildHolo : self->GetHologramChildren())
					{
						if (ChildHolo && ChildHolo->GetBlueprintDesigner() == nullptr)
						{
							ChildHolo->SetInsideBlueprintDesigner(Designer);
						}
					}
				}
			}

			if (!self || !self->HasAuthority() || !SFScalingSpecExpansion::IsSpecConstructionEnabled())
			{
				return;
			}
			FSFScalingSpec Spec;
			const bool bHasScaling = FindStagedSpec(self, Spec, /*bConsume=*/true);
			FSFExtendCommitSpec ExtendSpec;
			const bool bHasExtend = !bHasScaling && FindStagedExtendCommit(self, ExtendSpec, /*bConsume=*/true);
			if (!bHasScaling && !bHasExtend)
			{
				return; // nothing staged for this instigator/class (e.g. a child in the loop)
			}

			// [#365-MP] One line per staged construct: the designer state at the seam is the
			// load-bearing fact for designer-resident client builds (see re-derivation above).
			UE_LOG(LogSmartFoundations, Display,
				TEXT("[MP-SPEC] Construct seam: %s (%s staged), designer=%s."),
				*self->GetName(), bHasScaling ? TEXT("scaling spec") : TEXT("extend commit"),
				self->GetBlueprintDesigner() ? *self->GetBlueprintDesigner()->GetName() : TEXT("none"));

			if (bHasScaling)
			{
				if (CountGridChildren(self) == 0)
				{
					SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(self, Spec, self->GetRecipe());
				}

				// [MP-334] Server-side auto-connect belt wiring: replay the CLIENT'S staged belt
				// plan. The client's fire-time previews are the only complete, real plan that ever
				// exists - server-side re-derivation (ProcessSingleDistributor) returned 0 at this
				// seam, and the server's own aim-time previews are empty here / not construct-grade
				// (three failed approaches, live-proven 2026-06-09; see PLAN_MP_AutoConnect_334.md).
				// The plan belts are appended AFTER the grid children, so the vanilla child-
				// construct loop below builds the distributors first and each belt's
				// SF_BeltAutoConnectChild Construct path wires it geometrically against BUILT
				// actors - the same mechanism SP uses.
				SFScalingSpecExpansion::SpawnConduitPlanChildren(self, Spec);
			}
			else
			{
				// [EXTEND-MP] Reconstruct the FULL commit server-side, pre-scope(): authoritative
				// graph walk -> CaptureFromTopology -> FromSource(ParentOffset) -> spawn parent
				// clone set + scaled clone sets. The topology MUST be derived here, never shipped
				// from the client - client-side capture poisons every segment connection because
				// GetConnection() is null on clients (live root cause 2026-06-10: empty wiring
				// manifests, every MP Extend built unwired). The children's Construct paths
				// populate the wiring registries; the synchronous wiring pass below finishes the
				// job with authority.
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
				{
					if (USFExtendService* Extend = SS->GetExtendService())
					{
						Extend->ReconstructCommitOnServer(self, ExtendSpec);
					}
				}
			}

			// Spawn the group proxy BEFORE construction and expose it for the window of this
			// construct: the ConfigureActor hook below assigns it to every buildable configured
			// inside scope() - i.e. pre-BeginPlay, the timing vanilla blueprints use, so
			// lightweight conversion transfers membership to replicated lightweight indices.
			// [#312] Never create a Smart group proxy for a construct inside the Blueprint
			// Designer: mBlueprintProxy is SaveGame, so a designer-resident buildable pointing
			// at a Smart WORLD proxy makes the blueprint save chase that reference out of the
			// designer ("saving a blueprint saves the whole world").
			AFGBlueprintProxy* GroupProxy = nullptr;
			if (UWorld* World = self->GetWorld(); World && self->GetBlueprintDesigner() == nullptr)
			{
				FActorSpawnParameters ProxyParams;
				ProxyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				GroupProxy = World->SpawnActor<AFGBlueprintProxy>(
					AFGBlueprintProxy::StaticClass(), self->GetActorTransform(), ProxyParams);
			}
			GSFActiveSpecGroupProxy = GroupProxy;

			AActor* BuiltParent = scope(self, out_children, constructionID);

			GSFActiveSpecGroupProxy.Reset();

			// [EXTEND-MP] Register the clone-id -> built-actor anchors for the wiring pass. In SP
			// this lives in ASFFactoryHologram::Construct (the swapped parent): it position-matches
			// every child hologram carrying a JsonCloneId (the scaled clone FACTORIES,
			// "sc{i}_factory" - vanilla holograms whose own Construct knows nothing of clone ids)
			// to its built actor. The server constructs through the VANILLA hologram, so those
			// anchors never registered and the deferred wiring resolved NOTHING - a scaled run
			// built geometrically perfect but fully unwired (live 2026-06-10). Same seam, same
			// position-match, with the self-registered skip (#288).
			if (bHasExtend)
			{
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
				{
					if (USFExtendService* Extend = SS->GetExtendService())
					{
						int32 RegisteredAnchors = 0;
						for (AFGHologram* ChildHolo : self->mChildren)
						{
							if (!ChildHolo)
							{
								continue;
							}
							FSFHologramData* ChildData = USFHologramDataRegistry::GetData(ChildHolo);
							if (!ChildData || ChildData->JsonCloneId.IsEmpty())
							{
								continue;
							}
							if (Extend->GetBuiltActorByCloneId(ChildData->JsonCloneId) != nullptr)
							{
								continue; // self-registered during its own Construct (exact match)
							}
							const FVector ChildPos = ChildHolo->GetActorLocation();
							AActor* BestMatch = nullptr;
							float BestDist = 200.0f;
							for (AActor* ChildActor : out_children)
							{
								if (!ChildActor)
								{
									continue;
								}
								const float Dist = FVector::Dist(ChildActor->GetActorLocation(), ChildPos);
								if (Dist < BestDist)
								{
									BestDist = Dist;
									BestMatch = ChildActor;
								}
							}
							if (BestMatch)
							{
								Extend->RegisterJsonBuiltActor(ChildData->JsonCloneId, BestMatch);
								++RegisteredAnchors;
							}
						}
						UE_LOG(LogSmartFoundations, Verbose,
							TEXT("[EXTEND-MP] Registered %d clone-id wiring anchor(s) post-construct for %s."),
							RegisteredAnchors, *GetNameSafe(BuiltParent));

						// [EXTEND-MP] Run the post-build wiring pass SYNCHRONOUSLY, here, with the
						// BUILT PARENT as the factory anchor. Deferral is structurally unreliable
						// at the commit seam: the legacy per-factory-spawn lambdas resolved
						// "parent"/"Factory" against whichever factory their trigger captured
						// (live: a junction -> every factory-end connection failed), and BOTH the
						// legacy and a next-tick pass race the post-construct hologram
						// re-registration cleanup that clears the pending-wiring state (live:
						// everything no-opped). This point is POST-construct - all children built,
						// configured, and registered - the same in-frame pre-tick timing the #341
						// chain registration proves safe.
						if (AFGBuildableFactory* BuiltFactory = Cast<AFGBuildableFactory>(BuiltParent))
						{
							UE_LOG(LogSmartFoundations, Verbose,
								TEXT("[EXTEND-MP] Commit wiring pass executing for %s."),
								*BuiltFactory->GetName());
							Extend->ConnectAllChainElements(BuiltFactory);
							Extend->WireBuiltChildConnections(BuiltFactory);
						}
					}
				}
			}

			// [MP-334] Post-construct sweep: spline buildables (the plan belts) never reach the
			// AFGBuildableHologram::ConfigureActor BASE body our pre-BeginPlay hook patches (the
			// vanilla spline-hologram chain configures them elsewhere - live finding 2026-06-09:
			// proxy registered the 4 mergers but none of the 7 belts). Registering them HERE is
			// safe because conveyors are real actors, never lightweight - the pre-BeginPlay window
			// only matters for lightweight conversion (foundations). The GetBlueprintProxy() guard
			// skips everything Hook C already registered.
			if (GroupProxy)
			{
				for (AActor* Child : out_children)
				{
					AFGBuildable* ChildBuildable = Cast<AFGBuildable>(Child);
					if (ChildBuildable && !ChildBuildable->GetBlueprintProxy())
					{
						ChildBuildable->SetBlueprintProxy(GroupProxy);
						GroupProxy->RegisterBuildable(ChildBuildable);
					}
				}
				if (AFGBuildable* ParentBuildable = Cast<AFGBuildable>(BuiltParent))
				{
					if (!ParentBuildable->GetBlueprintProxy())
					{
						ParentBuildable->SetBlueprintProxy(GroupProxy);
						GroupProxy->RegisterBuildable(ParentBuildable);
					}
				}
			}

			// [MP-334] Staged power wires build AFTER the grid, against BUILT actors - wires are
			// never built from holograms (even in SP the wire child holograms exist only for cost;
			// hologram-replayed wires came out as unconnected zombies, live 2026-06-10). Direct
			// AFGBuildableWire spawn + Connect, the proven OnPowerPoleBuilt primitive. Registers
			// the wires into the group proxy itself (they are not in out_children).
			if (bHasScaling)
			{
				SFScalingSpecExpansion::SpawnWirePlanPostConstruct(BuiltParent, out_children, Spec, GroupProxy);
			}

			if (GroupProxy)
			{
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[MP-SPEC] Spec group proxy %s: %d actor buildables registered (+ lightweight instances tracked by index); built parent=%s, out_children=%d."),
					*GroupProxy->GetName(), GroupProxy->GetBuildables().Num(),
					*GetNameSafe(BuiltParent), out_children.Num());
				// If nothing registered (neither actors nor lightweights), let it clean itself up.
				GroupProxy->ValidateExistanceOtherwiseSelfDestruct();
			}

			// [CHAIN-FIX] Run the chain-hygiene sweep shortly after EVERY server-side spec/extend
			// construct: it purges detached chains AND re-registers any connected-but-chainless
			// belt. The thesis factory-tick AV (FGBuildableSubsystem.cpp:644, virtual call into
			// freed/garbage memory minutes after a build) reproduced twice on 2026-06-10 in
			// sessions whose only Smart activity was 1x1 conduit-plan builds - the sweep only ran
			// at boot and post-upgrade, leaving the spec-build path uncovered. Timer is coalesced;
			// scheduling per construct is cheap.
			if (USFSubsystem* SweepSS = USFSubsystem::Get(self->GetWorld()))
			{
				if (USFChainActorService* ChainSvc = SweepSS->GetChainActorService())
				{
					ChainSvc->ScheduleDeferredZombiePurge(3.0f);
				}
			}

			scope.Override(BuiltParent);
		});

	// ── Hook D: server-side placement leniency for staged Extend commits. SP's swapped parent
	// (ASFFactoryHologram) SKIPS clearance checks during Extend - the clone is deliberately placed
	// adjacent to the source with logistics in between, which full vanilla clearance rejects. The
	// server constructs through the VANILLA hologram, so the same fire that previews valid on the
	// client was refused server-side (live 2026-06-10: FGCDEncroachingClearance from
	// ValidatePlacementAndCost). Mirror SP: when a staged Extend commit exists for this hologram,
	// clear the disqualifiers after vanilla evaluates them.
	// NOTE: hooked at the AFGBuildableHologram override, NOT the AFGHologram base -
	// &AFGHologram::CheckValidPlacement resolves to an unhookable import thunk (live dedi
	// startup FATAL 2026-06-10: "resulting function still points to a thunk"). The override
	// covers every buildable hologram, which is exactly the set that can carry a staged commit.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableHologram::CheckValidPlacement, BuildableHologramCDO,
		[=](AFGBuildableHologram* self)
		{
			if (!self || !self->HasAuthority() || !SFScalingSpecExpansion::IsSpecConstructionEnabled())
			{
				return;
			}
			FSFExtendCommitSpec ExtendSpec;
			if (FindStagedExtendCommit(self, ExtendSpec, /*bConsume=*/false))
			{
				self->ResetConstructDisqualifiers();
			}
		});

	// ── Hook C: group membership assignment, pre-BeginPlay. ConfigureActor is called by the
	// hologram on the deferred-spawned buildable BEFORE FinishSpawning/BeginPlay - the only window
	// where lightweight-eligible buildables (foundations etc.) can join a blueprint proxy, because
	// their BeginPlay converts them to lightweight instances and transfers proxy membership to
	// replicated indices. Assigns the active spec-construct's proxy to every buildable configured
	// inside the Construct window above. (Primary-class virtual; same hook InfiniteZoop uses.)
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableHologram::ConfigureActor, BuildableHologramCDO,
		[](const AFGBuildableHologram* self, AFGBuildable* inBuildable)
		{
			AFGBlueprintProxy* GroupProxy = GSFActiveSpecGroupProxy.Get();
			if (!GroupProxy || !inBuildable)
			{
				return;
			}
			// [#312] Designer-resident buildables must never reference a Smart world proxy
			// (SaveGame reference would poison blueprint saves).
			if (inBuildable->GetBlueprintDesigner() != nullptr)
			{
				return;
			}
			if (!inBuildable->GetBlueprintProxy())
			{
				inBuildable->SetBlueprintProxy(GroupProxy);
				GroupProxy->RegisterBuildable(inBuildable);
			}
		});

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ Spec-construction hooks registered (GetCost / Construct - class-agnostic; spec staged via USFRCO)"));
}
