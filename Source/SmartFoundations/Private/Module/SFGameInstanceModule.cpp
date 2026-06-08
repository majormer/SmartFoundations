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
#include "Containers/Ticker.h"                 // FTSTicker for deferred one-per-frame chunk fires

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

// MP Slice 0 chunking. A Smart grid above this many total cells will not fit one 64KB Server_ConstructHologram.
static constexpr int32 SF_MP_OVERSIZED_CELLS = 130; // trigger chunking above this (total cells incl. parent)
static constexpr int32 SF_MP_CHUNK_CHILDREN  = 100; // grid children to build per chunk (parent adds 1 cell)
static constexpr int32 SF_MP_CHUNK_WAIT_FRAMES = 6; // frames to let the build gun settle between chunk fires

// Re-entrancy guard: the deferred chunk fires re-invoke InternalExecuteDuBuildStepInput; those nested calls
// must pass straight through to vanilla without re-chunking.
static bool GSFChunkingInProgress = false;

// Deferred-chunk queue. chunk 0 builds on the player's natural fire; chunks 1+ are fired one-per-frame from a
// ticker, AFTER the build gun settles, so each is a fresh fire instead of a synchronous re-entry (which the
// build-gun state machine refuses). Each pending chunk repositions the active hologram to its anchor (a cell
// the parent builds) and re-homes the chunk's other child holograms onto it.
struct FSFPendingChunk
{
	FTransform AnchorXform;
	TArray<TWeakObjectPtr<AFGHologram>> Children;
};
static TArray<FSFPendingChunk> GSFPendingChunks;
static TWeakObjectPtr<UFGBuildGunStateBuild> GSFPendingGun;
static int32 GSFChunkWait = 0;
static FTSTicker::FDelegateHandle GSFChunkTicker;

bool USFGameInstanceModule::TickPendingGridChunks(float /*Dt*/)
{
	if (GSFPendingChunks.Num() == 0)
	{
		GSFChunkTicker.Reset();
		return false; // unregister
	}
	if (--GSFChunkWait > 0)
	{
		return true; // still letting the gun settle
	}

	UFGBuildGunStateBuild* Gun = GSFPendingGun.Get();
	AFGHologram* Holo = Gun ? Gun->GetHologram() : nullptr;
	if (!Gun || !Holo)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("[MP-CHUNK] deferred: gun/hologram gone; aborting %d remaining chunk(s)."), GSFPendingChunks.Num());
		GSFPendingChunks.Reset();
		GSFChunkTicker.Reset();
		return false;
	}

	FSFPendingChunk Chunk = GSFPendingChunks[0];
	GSFPendingChunks.RemoveAt(0);

	TArray<AFGHologram*> Kids;
	for (const TWeakObjectPtr<AFGHologram>& W : Chunk.Children)
	{
		if (AFGHologram* C = W.Get()) { Kids.Add(C); }
	}

	Holo->SetActorTransform(Chunk.AnchorXform);
	USFGameInstanceModule::SetActiveHologramChildren(Holo, Kids);

	UE_LOG(LogSmartFoundations, Display,
		TEXT("[MP-CHUNK] deferred: firing chunk (%d children) at anchor; %d chunk(s) left after this."),
		Kids.Num(), GSFPendingChunks.Num());

	GSFChunkingInProgress = true;
	Gun->InternalExecuteDuBuildStepInput(false);
	GSFChunkingInProgress = false;

	USFGameInstanceModule::SetActiveHologramChildren(Holo, TArray<AFGHologram*>());
	GSFChunkWait = SF_MP_CHUNK_WAIT_FRAMES;
	return true;
}

// Set the active hologram's child list to exactly the given actors. Static member of USFGameInstanceModule so
// it inherits the friend access to AFGHologram::mChildren / mChildrenNameLookupMap (AccessTransformers.ini).
void USFGameInstanceModule::SetActiveHologramChildren(AFGHologram* Holo, const TArray<AFGHologram*>& NewChildren)
{
	if (!Holo) { return; }
	Holo->mChildren.Reset();
	Holo->mChildrenNameLookupMap.Reset();
	for (AFGHologram* C : NewChildren)
	{
		if (!C) { continue; }
		Holo->mChildren.Add(C);
		Holo->mChildrenNameLookupMap.Add(C->GetNameWithinParentHologram(), C);
	}
}

void USFGameInstanceModule::RegisterClientGridChunkFireHook()
{
	SUBSCRIBE_METHOD(
		UFGBuildGunStateBuild::InternalExecuteDuBuildStepInput,
		[](auto& scope, UFGBuildGunStateBuild* self, bool isInputFromARelease)
		{
			if (!self || GSFChunkingInProgress)
			{
				return; // nested re-fire -> let vanilla run unmodified
			}

			UWorld* World = self->GetWorld();
			if (!World || !FSFNetworkHelper::IsClient(World))
			{
				return; // only a network client serializes over the wire
			}

			AFGHologram* Holo = self->GetHologram();
			if (!Holo)
			{
				return;
			}

			// Snapshot the Smart grid child holograms (tagged SF_GridChild), in order.
			static const FName GridChildTag(TEXT("SF_GridChild"));
			TArray<AFGHologram*> Kids;
			for (const TObjectPtr<AFGHologram>& C : Holo->mChildren)
			{
				if (C && C->Tags.Contains(GridChildTag))
				{
					Kids.Add(C.Get());
				}
			}
			if (Kids.Num() == 0)
			{
				return; // not a Smart scaled grid
			}

			const int32 TotalCells = Kids.Num() + 1; // + parent/origin cell
			if (TotalCells <= SF_MP_OVERSIZED_CELLS)
			{
				return; // fits one construct -> untouched vanilla path
			}

			// INCREMENT 2b (deferred full-auto): chunk 0 builds on THIS natural fire; the rest are queued and
			// fired one-per-frame from a ticker (after the build gun settles), because the build gun refuses a
			// synchronous second construct in one fire (proven in 2a).

			// Chunk 0: keep the first SF_MP_CHUNK_CHILDREN grid children on the parent. Vanilla builds the
			// parent's own origin cell + these on this fire (do NOT cancel).
			TArray<AFGHologram*> Chunk0;
			for (int32 i = 0; i < Kids.Num() && Chunk0.Num() < SF_MP_CHUNK_CHILDREN; ++i)
			{
				Chunk0.Add(Kids[i]);
			}
			USFGameInstanceModule::SetActiveHologramChildren(Holo, Chunk0);

			// Queue the remaining cells as deferred chunks. Each chunk's first leftover cell becomes the
			// repositioned-parent anchor (its preview is destroyed now; the repositioned parent builds that
			// cell in the ticker); the rest become the chunk's re-homed children.
			GSFPendingChunks.Reset();
			for (int32 Start = SF_MP_CHUNK_CHILDREN; Start < Kids.Num(); Start += SF_MP_CHUNK_CHILDREN)
			{
				AFGHologram* Anchor = Kids[Start];
				if (!Anchor) { continue; }
				FSFPendingChunk PC;
				PC.AnchorXform = Anchor->GetActorTransform();
				for (int32 j = Start + 1; j < Kids.Num() && (j - Start) < SF_MP_CHUNK_CHILDREN; ++j)
				{
					if (Kids[j]) { PC.Children.Add(Kids[j]); }
				}
				GSFPendingChunks.Add(MoveTemp(PC));
				Anchor->Destroy();
			}

			GSFPendingGun = self;
			GSFChunkWait = SF_MP_CHUNK_WAIT_FRAMES;
			if (!GSFChunkTicker.IsValid())
			{
				GSFChunkTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&USFGameInstanceModule::TickPendingGridChunks), 0.0f);
			}

			UE_LOG(LogSmartFoundations, Display,
				TEXT("[MP-CHUNK] Increment 2b (deferred): %d cells -> chunk 0 = parent + %d (building now); queued %d more chunk(s) for one-per-frame fire."),
				TotalCells, Chunk0.Num(), GSFPendingChunks.Num());

			// Do NOT cancel: vanilla builds chunk 0 now; the ticker fires the queued chunks.
		}
	);

	UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ Client grid chunk fire-hook registered (MP Slice 0 chunking, InternalExecuteDuBuildStepInput)"));
}
