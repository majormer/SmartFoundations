// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFGameInstanceModule - server-side spec-based construction hooks (Net seam).
 * Split VERBATIM from SFGameInstanceModule.cpp (Wave 2): RegisterSpecConstructionHooks() and its
 * file-local static (GSFActiveSpecGroupProxy) - the authoritative consumers of the per-player
 * specs the client fire hook stages via USFRCO. Covers cost scaling (Hook A), grid/Extend
 * expansion + group bookkeeping (Hook B), placement leniency (Hook D), and pre-BeginPlay group
 * membership (Hook C), plus the chain-integrity guards. Same USFGameInstanceModule member,
 * registered identically from StartupModule - no behaviour change.
 */

#include "SFGameInstanceModule.h"
#include "Holograms/Logistics/SFWaterPumpChildHologram.h"
#include "SmartFoundations.h"
#include "SFSpecConstructionOwnership.h"
#include "Core/Construction/SFReplacementGroupTransfer.h"
#include "Features/Dismantle/Net/SFBlueprintGroupReplication.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGConveyorBeltHologram.h"
#include "Hologram/FGConveyorAttachmentHologram.h"
#include "Hologram/FGSplineHologram.h"
#include "Hologram/FGBuildableHologram.h"
#include "Hologram/FGBlueprintHologram.h"
#include "Features/AutoConnect/SFBlueprintLiftSeams.h"
#include "Hologram/FGConveyorPoleHologram.h"      // #341: belt-support parent hologram (covers stackable/wall/ceiling)
#include "Hologram/FGFactoryBuildingHologram.h"   // [#497] grid-child validation cancel at the override the poles dispatch to
#include "Hologram/FGPoleHologram.h"              // [MP-SPEC] multi-step gate: pole height step
#include "Hologram/FGFloodlightHologram.h"        // [MP-SPEC] multi-step gate: floodlight angle step
#include "Hologram/FGStandaloneSignHologram.h"    // [MP-SPEC] multi-step gate: sign height step
#include "Holograms/Core/SFSignHologramState.h"
#include "Holograms/Core/SFSignSupportMode.h"
#include "Features/Scaling/SFBeamHologramState.h"
#include "Features/Scaling/SFLadderHologramState.h"
#include "Features/Scaling/SFHypertubeSupportHologramState.h"
#include "Features/Scaling/SFFoundationPassthroughState.h"
#include "Features/Scaling/SFFloodlightHologramState.h"
#include "Features/Scaling/SFJumpPadHologramState.h"
#include "Features/Scaling/SFSnowCannonHologramState.h"
#include "Features/Scaling/SFElevatorHologramState.h"
#include "Features/Scaling/SFProxyGridConstructionCost.h"
#include "Features/Scaling/SFRailroadAttachmentHologramState.h"
#include "Features/Scaling/SFConveyorLiftHologramState.h"
#include "Features/Scaling/SFHypertubeEntranceState.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"  // #341: DrainStackBuiltConveyors
#include "FGConstructDisqualifier.h"
#include "FGRecipe.h"
#include "Resources/FGBuildingDescriptor.h"
#include "FGCentralStorageSubsystem.h"
#include "FGGameState.h"
#include "Core/SF_ATAnchor.h"
#include "Patching/NativeHookManager.h"
#include "Subsystem/SFSubsystem.h"
#include "Buildables/FGBuildableManufacturer.h"  // [#368] server-authoritative recipe apply
#include "Services/SFChainActorService.h"
#include "Services/SFRecipeManagementService.h"  // [CHAIN-FIX] post-construct chain-hygiene sweep
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/Net/SFLiftRoutePlan.h"
#include "Features/AutoConnect/SFBlueprintRailSeams.h"
#include "Features/AutoConnect/SFBlueprintVehicleSeams.h"
#include "Features/AutoConnect/SFBlueprintLiftSeams.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendBuiltActors.h"
#include "Features/Extend/Net/SFExtendCommitValidation.h"
#include "Features/Extend/Net/SFExtendAuthorityScope.h"
#include "Features/Extend/SFExtendWirePreviewScope.h"
#include "Features/Walk/SFWalkService.h"
#include "Features/Walk/SFWalkNativeSpanAdapter.h"   // Smart Walking (#356 Slice 3) server-side commit reconstruction
#include "Equipment/FGBuildGunBuild.h"        // UFGBuildGunStateBuild::InternalConstructHologram / GetHologram
#include "Core/Net/SFNetworkHelper.h"     // FSFNetworkHelper::IsClient
#include "Core/Net/SFRainOcclusionRemovalPolicy.h"  // #514: fail-closed CL 502094 rain ISM removals
#include "Core/Net/SFRainOcclusionAdditionPolicy.h" // #523: fail-closed duplicate-registration guard
#include "Engine/Engine.h"                     // GEngine on-screen message
#include "Misc/ScopeExit.h"
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Data/SFHologramDataRegistry.h"   // [EXTEND-MP] JsonCloneId wiring anchors post-construct
#include "Subsystem/SFHologramHelperService.h"
#include "Core/Net/SFRCO.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGBlueprintProxy.h"   // server-side Smart Dismantle group for spec-built grids
#include "Buildables/FGBuildableFactory.h"   // [EXTEND-MP] commit wiring pass anchor
#include "TimerManager.h"                    // [EXTEND-MP] next-tick commit wiring pass
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "Hologram/FGWallAttachmentHologram.h"        // [#364-MP] wall-support server re-validation
#include "Hologram/FGPillarHologram.h"                // [#522] pillar server re-validation (dedi floor snap)
#include "Hologram/FGPassthroughHologram.h"           // [#522] floor-hole server re-validation (dedi foundation snap)
#include "Buildables/FGBuildableBlueprintDesigner.h"  // [#365-MP] designer containment re-derive
#include "Buildables/FGBuildablePipeBase.h"
#include "EngineUtils.h"                              // [#365-MP] TActorIterator over designers
#include "FGRainOcclusionActor.h"               // #514: box/mesh rain-removal lifecycle guards
#include "Components/InstancedStaticMeshComponent.h" // #514: authoritative owner instance count

// [MP-SPEC] The blueprint proxy for the spec-construct currently executing on the server.
// Set by the Construct hook around scope() (construction is synchronous + single-threaded);
// the ConfigureActor hook assigns it to every buildable configured inside that window. The
// assignment MUST happen pre-BeginPlay: lightweight-eligible buildables (foundations etc.)
// convert to lightweight instances in BeginPlay and only then does vanilla transfer the proxy
// membership to replicated lightweight indices (RegisterLightweightInstance) - a proxy assigned
// after Construct ends up holding soon-destroyed temp actors and self-destructs (live finding
// 2026-06-09: "no grouping persists" on spec-built foundation grids).
static TWeakObjectPtr<AFGBlueprintProxy> GSFActiveSpecGroupProxy;

// [#497] True when this hologram is the ROOT of a large Smart grid (unparented, with many
// SF_GridChild children). Vanilla CheckClearance passes the hologram's ENTIRE child tree as
// ignored actors to the overlap query, and the physics filter checks every overlap candidate
// against that list linearly — O(candidates x children), quadratic in grid size. Counting diag
// round 4 proved the burner is exactly the root's 1-2 queries per frame (maxPassesPerFrame=2,
// 10-second frames at ~12K children). Above the threshold, skip the root's vanilla clearance:
// affordability red is computed separately (CheckCanAfford), and Smart grids place their
// members by design. Normal single-building behavior below the threshold is untouched.
static bool SFIsSmartGridMegaRoot(const AFGHologram* Holo)
{
	if (!Holo || Holo->GetParentHologram() != nullptr)
	{
		return false;
	}
	// Smart's subsystem tracks exactly which hologram it is actively scaling, and Smart's OWN
	// tracked-children list holds the grid. STATSv2 proved vanilla mChildren is nearly EMPTY
	// for a stackable mega-grid (rootKids=0-5 at thousands of tracked children) — the grid
	// delegates are never AddChild'd — so both earlier predicate legs (mChildren tag probe,
	// mChildren count threshold) were structurally unable to fire. It also means vanilla's
	// clearance query treats every collision-enabled grid child as a FOREIGN body: thousands
	// of hits to filter and process per query, the measured ~5 s.
	USFSubsystem* SmartSubsystem = USFSubsystem::Get(Holo->GetWorld());
	if (!SmartSubsystem || SmartSubsystem->GetActiveHologram() != Holo)
	{
		return false;
	}
	FSFHologramHelperService* HologramHelper = SmartSubsystem->GetHologramHelper();
	return HologramHelper && HologramHelper->GetSpawnedChildren().Num() >= 64;
}

// [#497] True when this hologram is a Smart-positioned grid child OR any descendant of one.
// The Tier-3 vanilla-delegate children carry the SF_GridChild tag, but vanilla stackable-pole
// holograms spawn their OWN stack-segment children under them (capture 8: the per-frame
// clearance burn survived the self-tag predicate because the burning holograms were these
// untagged grandchildren). Walk the parent chain — 2-3 hops, trivially cheaper than the
// vanilla validation/nudge work the callers cancel.
static bool SFIsSmartGridChildOrDescendant(const AFGHologram* Holo)
{
	static const FName SFGridChildTagName(TEXT("SF_GridChild"));
	if (!Holo || Holo->GetParentHologram() == nullptr)
	{
		return false;
	}
	for (const AFGHologram* H = Holo; H; H = H->GetParentHologram())
	{
		if (H->Tags.Contains(SFGridChildTagName))
		{
			return true;
		}
	}
	return false;
}

void USFGameInstanceModule::RegisterSpecConstructionHooks()
{
	static const FName GridChildTag(TEXT("SF_GridChild"));
    FSFBlueprintRailSeams::InstallNativeHooks();

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
					UE_LOG(LogSmartFoundations, Verbose,
						TEXT("[NULL-WIRE GUARD] %s (%s): scrubbed %d null/dead wire entr%s before dismantle (vanilla would have asserted)."),
						*self->GetName(), *self->GetClass()->GetName(), Scrubbed,
						Scrubbed == 1 ? TEXT("y") : TEXT("ies"));
				}
			}
			scope(self);
		});

	// [#514 / CL 502094] Rain occlusion publicly stores one owner/index lookup per hash for
	// instance removal. RemoveShape dispatches to private box-sprite and custom-mesh siblings.
	// The report reaches a private RemoveBoxSprite bounds assertion with index 25 / array size 24.
	// The proprietary body does not expose which private array asserts or what created that state.
	// Guard both siblings only at the shared public contract against the owner's live instance count:
	// valid lookups retain the original path unchanged; missing/dead/out-of-range lookups discard
	// that unusable hash entry and do not forward the opaque private call. This is intentionally
	// independent of Smart Dismantle attribution: the reporter had a large mod set and Smart's
	// EndPlay hook appears in the stack because it invokes vanilla before its own cleanup.
	// [#523] Synchronous mirror of every rain-occlusion hash Smart has forwarded to the private
	// add siblings, per subsystem instance. The proprietary bodies defer their ISM/octree work
	// (ProcessAddsAndRemovals), so no vanilla structure is guaranteed to observe an add at call
	// time; this mirror is updated inside the add/remove hooks themselves and is therefore exact
	// at every hook entry regardless of vanilla's internal batching. FObjectKey keeps recycled
	// subsystem allocations from aliasing; dead worlds are purged when a new subsystem first adds.
	static TMap<FObjectKey, TSet<FRainHashKey>> RegisteredRainHashes;

	static const auto GuardRainOcclusionRemoval = [](
		URainOcclusionWorldSubsystem* self,
		const FRainHashKey Hash,
		const ESFRainOcclusionRemovalPath Path) -> bool
	{
		// #514: CL 502094 RemoveBoxSprite waits at RVA 0x968F23 before any
		// lookup access; RemoveMeshShape delegates to it. A pre-hook otherwise
		// reads/mutates the lookup before vanilla's worker-completion barrier.
		// Preserve that boundary even when the guard cancels the original call.
		if (self)
		{
			self->EnsureWTTaskComplete();
		}
		FLookupData* Lookup = self ? self->mLookupTable.Find(Hash) : nullptr;
		const bool bHasLookup = Lookup != nullptr;
		UInstancedStaticMeshComponent* Owner = bHasLookup ? Lookup->Owner : nullptr;
		const bool bHasOwner = IsValid(Owner);
		const int32 LookupIndex = Lookup ? Lookup->Index : INDEX_NONE;
		const int32 InstanceCount = bHasOwner ? Owner->GetInstanceCount() : 0;
		if (SFRainOcclusionRemovalPolicy::DecideForPath(
			Path, bHasLookup, bHasOwner, LookupIndex, InstanceCount)
			== ESFRainOcclusionRemovalDecision::Forward)
		{
			// Only a forwarded removal unregisters the shape. A fail-closed discard below erases
			// just the lookup entry: the shape itself stays in vanilla's octree, so the #523 mirror
			// must keep the hash. Erasing it there let a later rebuild at the same spot re-add the
			// hash and trip vanilla's bDEBUGIsAddedTwice assertion (#523 reports on 34.3.1 follow
			// dismantles, not zoop); keeping it makes that re-add a discarded duplicate instead.
			if (TSet<FRainHashKey>* Registered = RegisteredRainHashes.Find(FObjectKey(self)))
			{
				Registered->Remove(Hash);
			}
			return false;
		}

		if (self)
		{
			self->mLookupTable.Remove(Hash);
		}
		const TCHAR* PathName = Path == ESFRainOcclusionRemovalPath::BoxSprite
			? TEXT("box") : TEXT("mesh");
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[#514] Rejected inconsistent rain-occlusion lookup (path=%s hash=%llu buildable=%d lookup=%d owner=%d index=%d instances=%d); private removal was not forwarded."),
			PathName, static_cast<unsigned long long>(Hash.Hash), Hash.bIsFromBuildable ? 1 : 0,
			bHasLookup ? 1 : 0, bHasOwner ? 1 : 0, LookupIndex, InstanceCount);
		return true;
	};

	SUBSCRIBE_METHOD(URainOcclusionWorldSubsystem::RemoveBoxSprite,
		[](auto& scope, URainOcclusionWorldSubsystem* self, const FRainHashKey Hash)
		{
			if (GuardRainOcclusionRemoval(self, Hash, ESFRainOcclusionRemovalPath::BoxSprite))
			{
				scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD(URainOcclusionWorldSubsystem::RemoveMeshShape,
		[](auto& scope, URainOcclusionWorldSubsystem* self, const FRainHashKey Hash)
		{
			if (GuardRainOcclusionRemoval(self, Hash, ESFRainOcclusionRemovalPath::MeshShape))
			{
				scope.Cancel();
			}
		});

	// [#523 / CL 502094] The add-side counterpart of the #514 removal guard. Zooping ramp walls
	// can reach the private add siblings twice with the same FRainHashKey and trip vanilla's
	// bDEBUGIsAddedTwice assertion (FGRainOcclusionActor.cpp:502). A duplicate add that survived
	// would also clobber the hash's mLookupTable owner/index entry, which is exactly the
	// inconsistent state the #514 removal guard keeps rejecting, so the two guards close one loop:
	// nothing registers twice, and nothing inconsistent is removed. The class log line is the
	// attribution probe for the root cause hunt: it names which buildable produced the duplicate.
	static const auto GuardRainOcclusionAddition = [](
		URainOcclusionWorldSubsystem* self,
		const AFGBuildable* Buildable,
		const FRainHashKey Hash,
		const TCHAR* PathName) -> bool
	{
		if (!self)
		{
			return false;
		}
		TSet<FRainHashKey>* Registered = RegisteredRainHashes.Find(FObjectKey(self));
		if (!Registered)
		{
			for (auto It = RegisteredRainHashes.CreateIterator(); It; ++It)
			{
				if (!It.Key().ResolveObjectPtr())
				{
					It.RemoveCurrent();
				}
			}
			Registered = &RegisteredRainHashes.Add(FObjectKey(self));
		}
		if (SFRainOcclusionAdditionPolicy::Decide(Registered->Contains(Hash))
			== ESFRainOcclusionAdditionDecision::Forward)
		{
			Registered->Add(Hash);
			return false;
		}
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[#523] Rejected duplicate rain-occlusion registration (path=%s hash=%llu buildable=%d class=%s); private add was not forwarded."),
			PathName, static_cast<unsigned long long>(Hash.Hash), Hash.bIsFromBuildable ? 1 : 0,
			Buildable ? *Buildable->GetClass()->GetName() : TEXT("<null>"));
		return true;
	};

	SUBSCRIBE_METHOD(URainOcclusionWorldSubsystem::AddBoxSprite,
		[](auto& scope, URainOcclusionWorldSubsystem* self, const AFGBuildable* Buildable,
			const FTransform& ActorTransform, FRainHashKey Hash)
		{
			if (GuardRainOcclusionAddition(self, Buildable, Hash, TEXT("box")))
			{
				scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD(URainOcclusionWorldSubsystem::AddMeshShape,
		[](auto& scope, URainOcclusionWorldSubsystem* self, const AFGBuildable* Buildable,
			const FSimpleOcclusionData& InstanceData, FRainHashKey Hash)
		{
			if (GuardRainOcclusionAddition(self, Buildable, Hash, TEXT("mesh")))
			{
				scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(AFGBuildable::EndPlay, BuildableCDO,
		[](auto& scope, AFGBuildable* self, const EEndPlayReason::Type endPlayReason)
		{
			scope(self, endPlayReason);
			// [#381] Only a real in-game destruction (Destroyed) can leave a buildable registered in a
			// factory-tick array and cause the freed-pointer tick AV this guard exists to prevent. On
			// world teardown (Quit / LevelTransition / RemovedFromWorld - e.g. dedicated-server shutdown)
			// EVERY buildable is still registered by design and there is no subsequent tick, so the
			// force-remove + warning are meaningless and spam one line per buildable. Skip those.
			if (endPlayReason != EEndPlayReason::Destroyed)
			{
				return;
			}
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
				// [#388-perf] VeryVerbose, not Warning: this guard fires per-conveyor on every stale
				// tick-group removal (normal in a churning factory) - at Warning it floods the log
				// (~70k lines / session reported) and tanks FPS via synchronous file writes. The guard
				// still does its job (prevents the freed-pointer factory-tick AV); only the log is quiet.
				UE_LOG(LogSmartFoundations, VeryVerbose,
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
	SUBSCRIBE_METHOD_VIRTUAL(AFGHologram::UpdateHologramPlacement, HologramCDO,
		[](auto& scope, AFGHologram* self, const FHitResult& Hit)
		{
			SFSignSupportMode::BeforeNativePlacement(self);
			scope(self, Hit);
			SFSignSupportMode::AfterNativePlacement(self);
		});

	// Resolve the staged spec for a constructing/costing hologram (server side).
	auto FindStagedSpec = [](const AFGHologram* Holo, FSFScalingSpec& OutSpec, bool bConsume) -> bool
	{
		if (!SFSpecConstructionOwnership::CanOwnStagedRequest(Holo)) return false;
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
		if (!SFSpecConstructionOwnership::CanOwnStagedRequest(Holo)) return false;
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

    auto IsExtendConstructionRequest = [=](AFGHologram* Holo) -> bool
    {
        if (!IsValid(Holo) || !Holo->HasAuthority() || Holo->GetParentHologram()
            || !SFScalingSpecExpansion::IsSpecConstructionEnabled()) return false;
        const AFGBuildableHologram* Buildable = Cast<AFGBuildableHologram>(Holo);
        if (Buildable && Buildable->GetZoopInstanceTransforms().Num() > 0) return false;
        USFSubsystem* SS = USFSubsystem::Get(Holo->GetWorld());
        USFExtendService* Extend = SS ? SS->GetExtendService() : nullptr;
        if (!Extend) return false;
        FSFWalkCommitSpec WalkCommit;
        if (SS->PeekWalkCommitForInstigator(Holo->GetConstructionInstigator(), Holo->GetBuildClass(), WalkCommit)) return false;
        const bool Local = SS->GetActiveHologram() == Holo;
        if (Local && SS->GetWalkService() && SS->GetWalkService()->IsActive()) return false;
        FSFExtendCommitSpec Commit;
        return FindStagedExtendCommit(Holo, Commit, false)
            || (Local && (SS->IsExtendModeActive() || Extend->IsRestoredCloneTopologyActive()));
    };

    // [MP-AUTH] These envelopes distinguish an actual build request from aiming's
    // per-frame ValidatePlacementAndCost calls. Nested native calls share one plan.
    SUBSCRIBE_METHOD(UFGBuildGunStateBuild::InternalConstructHologram,
        [=](auto& scope, UFGBuildGunStateBuild* self, FNetConstructionID Id)
        {
            USFSubsystem* SS = USFSubsystem::Get(self->GetWorld());
            AFGHologram* Root = self->GetHologram();
            TUniquePtr<FSFExtendAuthorityScope> Authority;
            if (!FSFExtendAuthorityScope::IsActive(SS)
                && Root && !Root->GetParentHologram()
                && FSFExtendAuthorityScope::ShouldIsolate(SS, Root->GetConstructionInstigator(), Root->GetBuildClass()))
                Authority = MakeUnique<FSFExtendAuthorityScope>(SS, Root);
            SFExtendCommitValidation::FRequestScope Request;
            SFConveyorLiftHologramState::FRequestScope LiftGridRequest;
            SFRailroadAttachmentHologramState::FRequestScope RailroadAttachmentRequest;
            SFHypertubeEntranceState::FRequestScope EntranceRequest;
            AFGHologram* Holo = self->GetHologram();
            TArray<FSFBlueprintRailPlanEntry> BlueprintRails;
            TArray<FItemAmount> BlueprintRailCost;
            if (Holo && Holo->HasAuthority())
                FSFBlueprintRailSeams::Capture(Holo, BlueprintRails, BlueprintRailCost);
            TArray<FSFBlueprintVehiclePlanEntry> BlueprintVehicles;
            FSFBlueprintVehicleSeams::Capture(Holo, BlueprintVehicles);
            if ((!BlueprintRails.IsEmpty() && !FSFBlueprintRailSeams::IsPrepared(Holo))
                || (!BlueprintVehicles.IsEmpty() && !FSFBlueprintVehicleSeams::IsPrepared(Holo)))
            {
                // Local authority enters this request with the live preview. Run
                // the same whole-copy/native rail validation before construction.
                auto* Player = Cast<AFGCharacterPlayer>(Holo->GetConstructionInstigator());
                Holo->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
                if (!Holo->CanConstruct()) { scope.Cancel(); return; }
            }
            // [MP-AUTH] The local beam grid is built before native payment queries
            // its cost again. Materialization/cleanup must not change that quote.
            TUniquePtr<SFProxyGridConstructionCost::FScope> ProxyGridConstruction;
            if (SFBeamHologramState::IsLocalProxyGridConstruction(Holo)
                || SFLadderHologramState::IsLocalProxyGridConstruction(Holo)
                || SFHypertubeSupportHologramState::IsLocalProxyGridConstruction(Holo)
                || SFFoundationPassthroughState::IsLocalProxyGridConstruction(Holo)
                || SFElevatorHologramState::IsLocalProxyGridConstruction(Holo)
                || SFJumpPadHologramState::IsLocalProxyGridConstruction(Holo)
                || SFSnowCannonHologramState::IsLocalProxyGridConstruction(Holo)
                || SFFloodlightHologramState::IsLocalProxyGridConstruction(Holo)
                || SFConveyorLiftHologramState::HasProxyGrid(Holo)
                || SFRailroadAttachmentHologramState::HasProxyGrid(Holo)
                || SFHypertubeEntranceState::HasProxyGrid(Holo)
                // Native payment follows Construct; the rail session's preview
                // cleanup must not erase the validated bridge price before then.
                || FSFBlueprintRailSeams::IsPrepared(Holo) || FSFBlueprintVehicleSeams::IsPrepared(Holo))
                ProxyGridConstruction = MakeUnique<SFProxyGridConstructionCost::FScope>(Holo);
            if (SFConveyorLiftHologramState::HasProxyGrid(Holo))
            {
                if (!SFConveyorLiftHologramState::Prepare(Holo)) { scope.Cancel(); return; }
                AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Holo->GetConstructionInstigator());
                Holo->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
                if (!Holo->CanConstruct()) { scope.Cancel(); return; }
            }
            if (SFHypertubeEntranceState::HasProxyGrid(Holo))
            {
                if (!SFHypertubeEntranceState::Prepare(Holo)) { scope.Cancel(); return; }
                AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Holo->GetConstructionInstigator());
                Holo->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
                if (!Holo->CanConstruct()) { scope.Cancel(); return; }
            }
            if (SFRailroadAttachmentHologramState::HasProxyGrid(Holo))
            {
                if (!SFRailroadAttachmentHologramState::Prepare(Holo)) { scope.Cancel(); return; }
                AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Holo->GetConstructionInstigator());
                Holo->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
                if (!Holo->CanConstruct()) { scope.Cancel(); return; }
            }
            // Revalidate every captured lift endpoint before the parent can build or spend.
            if (Holo && !SFLiftRoutePlan::ValidateTree(Holo)) { scope.Cancel(); return; }
            // [MP-AUTH] Native local construction builds before paying. Preserve the
            // accepted sign quote across that call and suppress only its excluded support.
            TUniquePtr<SFSignHologramState::FLocalConstructionScope> SignConstruction;
            if (SFSignHologramState::IsLocalGridConstruction(Holo))
            {
                if (!SFSignHologramState::AreGridSupportsValid(Holo))
                {
                    scope.Cancel();
                    return;
                }
                SignConstruction = MakeUnique<SFSignHologramState::FLocalConstructionScope>(Holo);
            }
            USFWalkService* PreparedNativeWalk = nullptr;
            APawn* NativeSpanInstigator = nullptr;
            UClass* NativeSpanBuildClass = nullptr;
            ON_SCOPE_EXIT
            {
                if (PreparedNativeWalk)
                {
                    PreparedNativeWalk->DiscardNativeSpanCommit(Holo);
                    USFWalkService::DiscardExistingCommit(Holo);
                    FSFWalkCommitSpec FinishedRequest;
                    SS->ConsumeWalkCommitForInstigator(NativeSpanInstigator, NativeSpanBuildClass, FinishedRequest);
                }
            };
            FSFWalkCommitSpec NativeSpanSpec;
            if (SS && SFSpecConstructionOwnership::CanOwnStagedRequest(Holo)
                && SS->PeekWalkCommitForInstigator(Holo->GetConstructionInstigator(), Holo->GetBuildClass(), NativeSpanSpec)
                && (FSFWalkNativeSpanAdapter::IsSpanType(NativeSpanSpec.ConveyanceType) || !NativeSpanSpec.ExistingEndpoints.IsEmpty()))
            {
                PreparedNativeWalk = SS ? SS->GetWalkService() : nullptr;
                NativeSpanInstigator = Holo->GetConstructionInstigator();
                NativeSpanBuildClass = Holo->GetBuildClass();
                if (!PreparedNativeWalk || PreparedNativeWalk->ReconstructWalkCommitOnServer(Holo, NativeSpanSpec) < 0)
                {
                    scope.Cancel();
                    return;
                }
                AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Holo->GetConstructionInstigator());
                Holo->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
                if (!Holo->CanConstruct())
                {
                    TSet<FString> Reasons;
                    TArray<AFGHologram*> Candidates=Holo->GetHologramChildren();
                    Candidates.Add(Holo);
                    int32 InvalidActors=0;
                    for (AFGHologram* Candidate : Candidates)
                    {
                        if (!IsValid(Candidate) || Candidate->CanConstruct()) { continue; }
                        ++InvalidActors;
                        TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
                        Candidate->GetConstructDisqualifiers(Disqualifiers);
                        for (const auto& Disqualifier : Disqualifiers) { Reasons.Add(GetNameSafe(Disqualifier.Get())); }
                    }
                    UE_LOG(LogSmartFoundations, Warning, TEXT("Walking native-span construction rejected: invalidHolograms=%d reasons=%s; no construction or payment."),
                        InvalidActors, *FString::Join(Reasons.Array(), TEXT(",")));
                    scope.Cancel();
                    return;
                }
            }
            if (IsExtendConstructionRequest(Holo))
            {
                // CL 502094's InternalConstructHologram calls Construct before paying,
                // and never calls ValidatePlacementAndCost itself. The server RPC has
                // already validated; the local path needs an explicit request-time check.
                if (!SFExtendCommitValidation::FindPrepared(Holo))
                {
                    AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(Holo->GetConstructionInstigator());
                    Holo->ValidatePlacementAndCost(Player ? Player->GetInventory() : nullptr);
                }
                const bool* Prepared = SFExtendCommitValidation::FindPrepared(Holo);
                if (!Prepared || !*Prepared || !Holo->CanConstruct())
                {
                    scope.Cancel(); // no native Construct, material removal, or upgrade side effects
                    return;
                }
            }
            scope(self, Id);
        });
    UFGBuildGunStateBuild* BuildStateCDO = GetMutableDefault<UFGBuildGunStateBuild>();
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(UFGBuildGunStateBuild::Client_OnRecipeBuilt_Implementation, BuildStateCDO,
		[](UFGBuildGunStateBuild* self, TSubclassOf<UFGRecipe> Recipe, int32 Count)
		{
			SFSignSupportMode::OnRecipeBuilt(self, Recipe, Count);
		});
    SUBSCRIBE_METHOD_VIRTUAL(UFGBuildGunStateBuild::Server_ConstructHologram_Implementation, BuildStateCDO,
        [](auto& scope, UFGBuildGunStateBuild* self, FNetConstructionID Id, FConstructHologramMessage Data)
        {
            // [MP-AUTH] Vanilla deserializes the root inside this RPC. Isolate by
            // its owning pawn through validation, wiring and post-Construct payment.
            USFSubsystem* SS = USFSubsystem::Get(self->GetWorld());
            AFGBuildGun* Gun = self->GetBuildGun();
            UClass* BuildClass = nullptr;
            if (Data.Recipe)
            {
                const TArray<FItemAmount>& Products = Data.Recipe->GetDefaultObject<UFGRecipe>()->GetProducts();
                if (Products.Num() == 1 && Products[0].ItemClass
                    && Products[0].ItemClass->IsChildOf(UFGBuildingDescriptor::StaticClass()))
                    BuildClass = UFGBuildingDescriptor::GetBuildableClass(Products[0].ItemClass.Get());
            }
            TUniquePtr<FSFExtendAuthorityScope> Authority;
            // A recent staged Extend cannot classify a later different recipe's build.
            // Unknown message classes wait for the exact deserialized-root fallback.
            if (!FSFExtendAuthorityScope::IsActive(SS)
                && FSFExtendAuthorityScope::ShouldIsolate(SS, Gun ? Gun->GetInstigator() : nullptr, BuildClass))
                Authority = MakeUnique<FSFExtendAuthorityScope>(SS);
            SFExtendCommitValidation::FRequestScope Request;
            SFConveyorLiftHologramState::FRequestScope LiftGridRequest;
            SFRailroadAttachmentHologramState::FRequestScope RailroadAttachmentRequest;
            SFHypertubeEntranceState::FRequestScope EntranceRequest;
            ON_SCOPE_EXIT
            {
                APawn* Pawn = Gun ? Gun->GetInstigator() : nullptr;
                USFWalkService::DiscardExistingForInstigator(Pawn, BuildClass);
                FSFWalkCommitSpec Finished;
                if (SS && SS->PeekWalkCommitForInstigator(Pawn, BuildClass, Finished) && !Finished.ExistingEndpoints.IsEmpty())
                    SS->ConsumeWalkCommitForInstigator(Pawn, BuildClass, Finished);
            };
            scope(self, Id, Data);
        });
    SUBSCRIBE_METHOD(AFGHologram::ValidatePlacementAndCost,
        [=](auto& scope, AFGHologram* self, UFGInventoryComponent* Inventory)
        {
            ON_SCOPE_EXIT
            {
                // Some native support classes skip the buildable placement body.
                // Apply the whole-resume verdict after native validation/materials,
                // so the held support agrees with the HUD and the fire guard.
                USFSubsystem* WalkSS = self ? USFSubsystem::Get(self->GetWorld()) : nullptr;
                USFWalkService* Walk = WalkSS ? WalkSS->GetWalkService() : nullptr;
                if (Walk && Walk->IsActive() && Walk->IsResuming()
                    && Walk->GetSeedHologram() == self && !Walk->BuildCommitSpec().bValid)
                {
                    self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                    self->SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
                }
            };
            // Equipment can temporarily have no instigator during server deserialization.
            // The validated root supplies the exact player/class; retain its authority
            // scope on the outer request through construction and material payment.
            SFExtendCommitValidation::IsolateAuthority(USFSubsystem::Get(self->GetWorld()), self);
            if (SFExtendCommitValidation::IsRequestActive() && !self->GetParentHologram()
                && SFSpecConstructionOwnership::CanOwnStagedRequest(self))
            {
                USFSubsystem* WalkSubsystem = USFSubsystem::Get(self->GetWorld());
                FSFWalkCommitSpec Resume;
                if (WalkSubsystem && WalkSubsystem->PeekWalkCommitForInstigator(self->GetConstructionInstigator(), self->GetBuildClass(), Resume)
                    && !Resume.ExistingEndpoints.IsEmpty())
                {
                    USFWalkService* Walk = WalkSubsystem->GetWalkService();
                    const bool Valid = Walk && Walk->ReconstructWalkCommitOnServer(self, Resume) >= 0;
                    // Reconstruction happens after the build gun's normal reset. Newly
                    // spawned children still carry BeginPlay's Initializing marker.
                    // Repeat the native reset for the complete tree, then repopulate
                    // every placement and affordability result through native validation.
                    if (Valid) { self->ResetConstructDisqualifiers(); }
                    scope(self, Inventory);
                    if (!Valid || !self->CanConstruct())
                    {
                        TSet<FString> Reasons;
                        TArray<AFGHologram*> Candidates = self->GetHologramChildren();
                        Candidates.Add(self);
                        for (AFGHologram* Candidate : Candidates)
                        {
                            if (!IsValid(Candidate) || Candidate->CanConstruct()) { continue; }
                            TArray<TSubclassOf<UFGConstructDisqualifier>> Disqualifiers;
                            Candidate->GetConstructDisqualifiers(Disqualifiers);
                            for (const auto& Disqualifier : Disqualifiers)
                                Reasons.Add(GetNameSafe(Candidate->GetClass()) + TEXT(":") + GetNameSafe(Disqualifier.Get()));
                        }
                        UE_LOG(LogSmartFoundations, Warning, TEXT("Walking resume rejected before payment: prepared=%d reasons=%s."),
                            Valid ? 1 : 0, *FString::Join(Reasons.Array(), TEXT(",")));
                        USFWalkService::DiscardExistingCommit(self);
                        self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                    }
                    return;
                }
            }
            // [MP-SEAM] Validate the whole lift-tail layout and exact native price before payment.
            if (SFExtendCommitValidation::IsRequestActive() && SFSpecConstructionOwnership::CanOwnStagedRequest(self))
            {
                if (auto* Blueprint = Cast<AFGBlueprintHologram>(self))
                {
                    FSFScalingSpec LiftSpec;
                    const bool StagedLift = FindStagedSpec(self, LiftSpec, false);
                    if (!StagedLift)
                    {
                        auto* SS = USFSubsystem::Get(self->GetWorld());
                        auto* Player = self->GetConstructionInstigator();
                        if (SS && SS->GetActiveHologram() == self && Player && Player->IsLocallyControlled())
                        {
                            SFScalingSpecExpansion::CaptureScalingSpec(self, LiftSpec);
                            SFScalingSpecExpansion::CaptureConduitPlan(self, LiftSpec);
                        }
                    }
                    if (LiftSpec.bBlueprintLiftSeamsRequired)
                    {
                        const auto Before = self->mConstructDisqualifiers;
                        const bool Valid = FSFBlueprintLiftSeams::Prepare(Blueprint, LiftSpec);
                        if (Valid && StagedLift)
                        {
                            self->ResetConstructDisqualifiers();
                            for (const auto& D : Before) if (D != UFGCDInitializing::StaticClass()) self->AddConstructDisqualifier(D);
                        }
                        if (!Valid || LiftSpec.BlueprintRailSeams.IsEmpty())
                        {
                            auto Children = MoveTemp(self->mChildren);
                            scope(self, Inventory);
                            self->mChildren = MoveTemp(Children);
                            if (!Valid) self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                            return;
                        }
                    }
                }
            }
            // [MP-SEAM] Prepare exact Blueprint rail connections before native payment.
            if (SFExtendCommitValidation::IsRequestActive() && SFSpecConstructionOwnership::CanOwnStagedRequest(self))
            {
                FSFScalingSpec RailSpec;
                auto* Blueprint = Cast<AFGBlueprintHologram>(self);
                const bool StagedRail = Blueprint && FindStagedSpec(self, RailSpec, false);
                if (Blueprint && !StagedRail)
                {
                    // [MP-SEAM] Standalone/listen-host grids already own their native
                    // previews. Capture that same plan before native payment too.
                    auto* Local = USFSubsystem::Get(self->GetWorld());
                    auto* Instigator = self->GetConstructionInstigator();
                    if (Local && Local->GetActiveHologram() == self && Instigator && Instigator->IsLocallyControlled())
                    {
                        SFScalingSpecExpansion::CaptureScalingSpec(self, RailSpec);
                        SFScalingSpecExpansion::CaptureConduitPlan(self, RailSpec);
                    }
                }
                if (Blueprint && (!RailSpec.BlueprintRailSeams.IsEmpty() || !RailSpec.BlueprintVehicleSeams.IsEmpty()))
                {
                    // Fresh copied cells have no independent aiming step. Snapshot
                    // only the parent's own native restrictions, without restoring
                    // a copied cell's aiming verdict onto its parent.
                    const auto BeforePreparation = self->mConstructDisqualifiers;
                    FString Failure;
                    const bool Valid = (RailSpec.BlueprintRailSeams.IsEmpty() || FSFBlueprintRailSeams::Prepare(Blueprint, RailSpec, &Failure))
                        && (RailSpec.BlueprintVehicleSeams.IsEmpty() || FSFBlueprintVehicleSeams::Prepare(Blueprint, RailSpec, &Failure));
                    if (Valid && StagedRail)
                    {
                        self->ResetConstructDisqualifiers();
                        // Remove only initialization propagated by fresh grid cells;
                        // keep the initiating Blueprint's native placement restrictions.
                        for (const auto& Disqualifier : BeforePreparation)
                            if (Disqualifier != UFGCDInitializing::StaticClass()) self->AddConstructDisqualifier(Disqualifier);
                    }
                    // Keep the native TObjectPtr element type: a converting move
                    // into a raw-pointer array copies and leaves children attached.
                    decltype(self->mChildren) PreparedChildren;
                    if (StagedRail) PreparedChildren = MoveTemp(self->mChildren);
                    const auto RestoreRailChildren = ::ScopeExitSupport::FScopeGuardSyntaxSupport()
                        + [&]() { if (StagedRail) self->mChildren = MoveTemp(PreparedChildren); };
                    scope(self, Inventory);
                    if (Valid && !self->CanConstruct() && !self->ActorHasTag(TEXT("SF_BlueprintRailPlacementLogged")))
                    {
                        self->Tags.AddUnique(TEXT("SF_BlueprintRailPlacementLogged"));
                        TArray<FString> Reasons;
                        for (const auto& D : self->mConstructDisqualifiers) Reasons.Add(GetNameSafe(D.Get()));
                        UE_LOG(LogSmartFoundations, Warning, TEXT("Prepared Blueprint rail placement rejected by native parent validation: %s."), *FString::Join(Reasons, TEXT(",")));
                    }
                    if (!Valid)
                    {
                        self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                        if (!self->ActorHasTag(TEXT("SF_BlueprintRailRejectionLogged")))
                        {
                            self->Tags.AddUnique(TEXT("SF_BlueprintRailRejectionLogged"));
                            UE_LOG(LogSmartFoundations, Warning, TEXT("Blueprint rail construction rejected before payment: %s."), *Failure);
                        }
                    }
                    return;
                }
            }
            if (SFExtendCommitValidation::IsRequestActive() && SFSpecConstructionOwnership::CanOwnStagedRequest(self))
            {
                FSFScalingSpec LiftPlan;
                if (FindStagedSpec(self, LiftPlan, false) && !LiftPlan.LiftRoutes.IsEmpty())
                {
                    bool Valid = true;
                    if (!self->ActorHasTag(TEXT("SF_LiftPlanPrepared")))
                    {
                        if (CountGridChildren(self) == 0)
                            SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(self, LiftPlan, self->GetRecipe());
                        SFScalingSpecExpansion::SpawnConduitPlanChildren(self, LiftPlan);
                        Valid = SFLiftRoutePlan::Prepare(self, LiftPlan.LiftRoutes);
                    }
                    else Valid = SFLiftRoutePlan::ValidateTree(self);
                    // These children were expanded before payment so their exact cost and
                    // route endpoints could be checked. Ordinary Smart grid children do not
                    // have independent aiming state: native placement validates the player
                    // parent, while the prepared route checks above validate each lift route.
                    // Keep the complete prepared quote active while validating that parent.
                    // AddChild can propagate the fresh cells' Initializing marker after
                    // the build gun's reset. Repeat that native reset for the complete
                    // prepared tree, just as the Walking reconstruction path does above.
                    if (Valid) self->ResetConstructDisqualifiers();
                    auto PreparedChildren = MoveTemp(self->mChildren);
                    const auto RestoreLiftPlanChildren = ::ScopeExitSupport::FScopeGuardSyntaxSupport()
                        + [&]() { self->mChildren = MoveTemp(PreparedChildren); };
                    scope(self, Inventory);
                    if (!Valid) self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                    return;
                }
            }
            if (SFConveyorLiftHologramState::IsRequestActive() && SFConveyorLiftHologramState::IsHandled(self) && !self->GetParentHologram())
            {
                FSFScalingSpec LiftSpec;
                const bool Staged = FindStagedSpec(self, LiftSpec, false);
                if (SFConveyorLiftHologramState::HasProxyGrid(self) || (Staged && LiftSpec.CellCount() > 1))
                {
                    const bool Valid = SFConveyorLiftHologramState::Prepare(self, Staged ? &LiftSpec : nullptr);
                    scope(self, Inventory);
                    if (!Valid) self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                    return;
                }
            }
            if (SFHypertubeEntranceState::IsRequestActive() && SFHypertubeEntranceState::IsHandled(self)
                && !self->GetParentHologram())
            {
                FSFScalingSpec EntranceSpec;
                const bool Staged = FindStagedSpec(self, EntranceSpec, false);
                const bool Valid = SFHypertubeEntranceState::Prepare(self, Staged ? &EntranceSpec : nullptr);
                scope(self, Inventory);
                if (!Valid) self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                return;
            }
            if (SFRailroadAttachmentHologramState::IsRequestActive() && SFRailroadAttachmentHologramState::IsHandled(self)
                && !self->GetParentHologram())
            {
                FSFScalingSpec RailroadSpec;
                const bool Staged = FindStagedSpec(self, RailroadSpec, false);
                const bool Valid = SFRailroadAttachmentHologramState::Prepare(self, Staged ? &RailroadSpec : nullptr);
                scope(self, Inventory);
                if (!Valid) self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
                return;
            }
            if (!SFExtendCommitValidation::IsRequestActive() || !IsExtendConstructionRequest(self))
            {
                scope(self, Inventory);
                return;
            }
            USFSubsystem* SS = USFSubsystem::Get(self->GetWorld());
            USFExtendService* Extend = SS ? SS->GetExtendService() : nullptr;
            FSFExtendCommitSpec Commit;
            const bool Staged = FindStagedExtendCommit(self, Commit, false);
            const bool Local = SS && Extend && SS->GetActiveHologram() == self
                && (SS->IsExtendModeActive() || Extend->IsRestoredCloneTopologyActive());
            if (!Extend || (!Staged && !Local))
            {
                scope(self, Inventory);
                return;
            }

            FString Reason;
            if (!SFExtendCommitValidation::FindPrepared(self))
            {
                SFExtendCommitValidation::SetPrepared(self, false); // recursive validation cannot reconstruct twice
                bool Valid = true;
                if (Staged) Valid = Extend->ReconstructCommitOnServer(self, Commit) > 0 && Extend->IsScaledExtendValid();
                if (Valid && !ASFWaterPumpChildHologram::ValidateWaterTree(self))
                {
                    Valid = false;
                    Reason = NSLOCTEXT("SmartFoundations", "MP_Notice_WaterExtractorDeepWater", "Every water extractor must be placed on deep water.").ToString();
                }
                Valid = Valid && Extend->ValidatePowerPlanForConstruction(self, Reason);
                const TArray<FItemAmount> AuthorityCost = Valid && Staged ? self->GetCost(true) : TArray<FItemAmount>();
                if (Valid && Staged && !SFExtendCommitValidation::SameCost(Commit.Cost, AuthorityCost))
                {
                    Valid = false;
                    Reason = NSLOCTEXT("SmartFoundations", "Extend_SourceOrCostChanged", "The source layout or cable quote changed. Re-select the source before building.").ToString();
                    const auto DescribeCost = [](const TArray<FItemAmount>& Cost)
                    {
                        FString Result;
                        for (const FItemAmount& Item : Cost)
                            Result += FString::Printf(TEXT("%s%s=%d"), Result.IsEmpty() ? TEXT("") : TEXT(","), *GetNameSafe(Item.ItemClass.Get()), Item.Amount);
                        return Result;
                    };
                    UE_LOG(LogSmartFoundations, Warning,
                        TEXT("Extend quote mismatch: preview=[%s] authority=[%s] children=%d restore=%d"),
                        *DescribeCost(Commit.Cost), *DescribeCost(AuthorityCost), self->GetHologramChildren().Num(), Commit.bIsRestore);
                }
                SFExtendCommitValidation::SetPrepared(self, Valid);
                if (!Valid)
                {
                    UE_LOG(LogSmartFoundations, Warning, TEXT("Extend pre-construction validation refused %s: %s"),
                        *GetNameSafe(self), *Reason);
                    if (APawn* Pawn = self->GetConstructionInstigator())
                        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
                            PC->ClientMessage(Reason.IsEmpty() ? NSLOCTEXT("SmartFoundations", "Extend_CannotReconstruct", "Smart could not reconstruct this Extend layout.").ToString() : Reason);
                }
            }

            scope(self, Inventory);
            // Hook D clears native placement disqualifiers for Extend *inside* the
            // call above. Add our hard failure afterwards, before native construction.
            const bool* Valid = SFExtendCommitValidation::FindPrepared(self);
            if (!Valid || !*Valid) self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
        });

	// Smart Walking (#356 Slice 3): resolve the staged walk commit for a constructing/costing seed hologram.
	auto FindStagedWalkCommit = [](const AFGHologram* Holo, FSFWalkCommitSpec& OutSpec, bool bConsume) -> bool
	{
		if (!SFSpecConstructionOwnership::CanOwnStagedRequest(Holo)) return false;
		AFGHologram* MutableHolo = const_cast<AFGHologram*>(Holo);
		USFSubsystem* SS = USFSubsystem::Get(MutableHolo->GetWorld());
		if (!SS)
		{
			return false;
		}
		APawn* Instigator = MutableHolo->GetConstructionInstigator();
		UClass* BuildClass = MutableHolo->GetBuildClass();
		return bConsume
			? SS->ConsumeWalkCommitForInstigator(Instigator, BuildClass, OutSpec)
			: SS->PeekWalkCommitForInstigator(Instigator, BuildClass, OutSpec);
	};

	// [MP-AUTH] ActiveHologram also tracks the remote build gun on a dedicated server.
	// It is not proof of local ownership. Prepared Extend children must reach the staged
	// branch; otherwise a local power wrapper can leave the commit for a factory child.
	auto CanUseLocalPowerPlan = [=](AFGHologram* Holo, USFSubsystem* SS) -> bool
	{
		if (!SS || !SFSpecConstructionOwnership::CanOwnStagedRequest(Holo)) return false;
		const APawn* Instigator = Holo->GetConstructionInstigator();
		FSFScalingSpec Scaling;
		FSFExtendCommitSpec Extend;
		FSFWalkCommitSpec Walk;
		const bool HasStaged = FindStagedSpec(Holo, Scaling, false)
			|| FindStagedExtendCommit(Holo, Extend, false) || FindStagedWalkCommit(Holo, Walk, false)
			|| SFExtendCommitValidation::FindPrepared(Holo) != nullptr;
		return SFSpecConstructionOwnership::CanUseLocalPowerPlan(true,
			Instigator && Instigator->IsLocallyControlled(), SS->GetActiveHologram() == Holo,
			SS->ShouldSuppressNormalGridChildren(), HasStaged);
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
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[MP-SPEC] %s: cleared server-side FGCDMustSnapWall for a staged client fire (client validated the wall snap; %d other disqualifier(s) kept)."),
					*self->GetName(), Disqualifiers.Num());
			}
		});

	// ── [#522] Pillar re-validation for staged client fires - the same disease #364-MP fixed for
	// wall attachments, in a second family. A pillar snapped onto a foundation references the
	// CLIENT's temporary actor for that lightweight instance (the dedi log shows the server
	// failing to resolve exactly that GUID: "Unable to resolve default guid from client:
	// Build_Foundation_..."), so the deserialized server-side hologram loses its snap target and
	// the CheckValidFloor re-run reports FGCDInvalidFloor - every Smart-scaled pillar over a
	// foundation is refused, dedi only (SP resolves the actor trivially; against rock/terrain no
	// actor reference is needed, which is why those placements work). Same trust model as #364:
	// the client's build gun will not fire a red preview, and a staged spec exists ONLY for a
	// Smart client fire. Clear exactly FGCDInvalidFloor; every other server check stays.
	SUBSCRIBE_METHOD_VIRTUAL(AFGBuildableHologram::CheckValidPlacement, BuildableHologramCDO,
		[=](auto& scope, AFGBuildableHologram* self)
		{
			scope(self);
			if (!self || !self->HasAuthority() || !self->IsA<AFGPillarHologram>())
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
			if (Disqualifiers.Remove(UFGCDInvalidFloor::StaticClass()) > 0)
			{
				self->ResetConstructDisqualifiers();
				for (const TSubclassOf<UFGConstructDisqualifier>& Disqualifier : Disqualifiers)
				{
					self->AddConstructDisqualifier(Disqualifier);
				}
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[#522] %s: cleared server-side FGCDInvalidFloor for a staged client fire (client validated the floor snap; %d other disqualifier(s) kept)."),
					*self->GetName(), Disqualifiers.Num());
			}
		});

	// ── [#522] Passthrough (hypertube/pipe floor hole) re-validation for staged client fires.
	// The passthrough's entire placement contract is the foundation it pierces, and that
	// reference is the same client-side lightweight temporary that cannot resolve on the server
	// - the dedi log shows all three snap-shaped refusals (FGCDInvalidFloor / FGCDMustSnap /
	// FGCDEncroachingSoftClearance, the last because without the resolved foundation the
	// server clearance-tests the hole against the very slab it should be embedded in). Clear
	// those three for staged fires; overlap/affordability and every other check stays.
	AFGPassthroughHologram* PassthroughCDO = GetMutableDefault<AFGPassthroughHologram>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGPassthroughHologram::CheckValidPlacement, PassthroughCDO,
		[=](auto& scope, AFGPassthroughHologram* self)
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
			int32 Cleared = 0;
			Cleared += Disqualifiers.Remove(UFGCDInvalidFloor::StaticClass());
			Cleared += Disqualifiers.Remove(UFGCDMustSnap::StaticClass());
			Cleared += Disqualifiers.Remove(UFGCDEncroachingSoftClearance::StaticClass());
			if (Cleared > 0)
			{
				self->ResetConstructDisqualifiers();
				for (const TSubclassOf<UFGConstructDisqualifier>& Disqualifier : Disqualifiers)
				{
					self->AddConstructDisqualifier(Disqualifier);
				}
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[#522] %s: cleared %d server-side snap-shaped disqualifier(s) for a staged client fire (client validated the foundation snap; %d other disqualifier(s) kept)."),
					*self->GetName(), Cleared, Disqualifiers.Num());
			}
		});

	// ── Hook A: cost. Charged server-side BEFORE Construct, when the grid children do not exist
	// yet - scale the uniform per-cell cost by the staged cell count. Fires at the
	// AFGHologram::GetCost base body (primary virtual; the scalable parents' classes do not
	// override GetCost, and spline types that do never carry a staged spec).
	SUBSCRIBE_METHOD_VIRTUAL(AFGHologram::GetCost, HologramCDO,
		[=](auto& scope, const AFGHologram* self, bool includeChildren)
		{
			if (!self || !includeChildren)
			{
				return;
			}
            TArray<FItemAmount> PreparedRouteCost;
            if (SFLiftRoutePlan::GetPreparedCost(self, PreparedRouteCost))
            {
                scope(self, false); scope.Override(PreparedRouteCost); return;
            }
            if (FSFBlueprintRailSeams::GetPreparedCost(self, PreparedRouteCost))
            {
                scope(self, false); scope.Override(PreparedRouteCost); return;
            }
            if (self->ActorHasTag(TEXT("SF_LiftPlanPrepared"))) return;
			TArray<FItemAmount> LocalSignCost;
			if (USFWalkService::GetPreparedNativeSpanCost(self, LocalSignCost)
                || USFWalkService::GetPreparedExistingCost(self, LocalSignCost))
            {
                scope(self, false);
                scope.Override(LocalSignCost);
                return;
            }
			if (SFProxyGridConstructionCost::GetAcceptedCost(self, LocalSignCost))
			{
				scope(self, includeChildren);
				scope.Override(LocalSignCost);
				return;
			}
			if (SFSignHologramState::GetLocalConstructionCost(self, LocalSignCost))
			{
				scope(self, includeChildren);
				scope.Override(LocalSignCost);
				return;
			}
			// [#497] Child holograms can never be a staged-commit root: scaling/extend specs stage on
			// the held (root) hologram and walk seeds are standalone. The build gun's per-frame cost
			// walk calls GetCost on EVERY AddChild'd preview child (thousands at high Extend counts),
			// and each pass through this hook constructed an FSFExtendCommitSpec probe + ran the
			// staged-commit lookups below — measured as real per-frame overhead at 77×1. Bail first.
			if (self->GetParentHologram() != nullptr)
			{
				return;
			}
			AFGHologram* MutableSelf = const_cast<AFGHologram*>(self);
			// A negative grid can leave the native height-step parent above the support row.
			// Its pole stays enabled for native input, but must not appear in the priced preview.
			if (self->IsA<AFGStandaloneSignHologram>())
			{
				TArray<FItemAmount> PreviewCost;
				FSFScalingSpec SignSpec;
				FSFExtendCommitSpec SignExtend;
				FSFWalkCommitSpec SignWalk;
				USFSubsystem* SignSS = USFSubsystem::Get(self->GetWorld());
				USFWalkService* SignWalkService = SignSS ? SignSS->GetWalkService() : nullptr;
				if (!FindStagedSpec(self, SignSpec, /*bConsume=*/false)
					&& !FindStagedExtendCommit(self, SignExtend, /*bConsume=*/false)
					&& !FindStagedWalkCommit(self, SignWalk, /*bConsume=*/false)
					&& !(SignWalkService && SignWalkService->IsActive()))
				{
					PreviewCost = scope(self, includeChildren);
					if (SFSignHologramState::AdjustPreviewCost(self, PreviewCost)) scope.Override(PreviewCost);
					return;
				}
			}
			if (!SFScalingSpecExpansion::IsSpecConstructionEnabled()) return;
			if (!MutableSelf->HasAuthority())
			{
				// Extend/scaling preview children are AddChild'd, so vanilla GetCost aggregation already sums them on
				// the client. Walk previews are STANDALONE (invisible to that aggregation), so on a client the build-gun
				// overlay would otherwise show only the bare seed — fall through to the existing walk block below to
				// synthesize the full run cost while an ACTIVE walk is held; everything else still returns to vanilla.
				// (No new Override here — this only lets the client REACH the existing crash-safe walk block.)
				USFSubsystem* WalkSS = USFSubsystem::Get(self->GetWorld());
				USFWalkService* WalkSvc = WalkSS ? WalkSS->GetWalkService() : nullptr;
				if (!WalkSvc || !WalkSvc->IsActive())
				{
					return; // non-walk client cost comes from vanilla aggregation of the AddChild'd children
				}
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

			// Smart Walking (#356 Slice 3): a staged walk commit charges the seed's own cost PLUS every walk
			// pole/belt the server will build (WalkSpec.Cost is the extras only; the seed is the bare hologram
			// here). Same by-value safety rule: invoke scope() to construct the return slot BEFORE Override().
			// Smart Walking (#356 Slice 3): charge the seed's own cost PLUS every walk pole/belt the server will
			// build. At COMMIT the spec is staged (FindStagedWalkCommit); during the PREVIEW the build-gun cost
			// overlay updates while aiming but nothing is staged yet, so synthesize the cost from the LIVE walk
			// session - otherwise the overlay shows only the bare seed (walk holograms are standalone, so vanilla
			// GetCost aggregation can't see them, unlike auto-connect's AddChild'd grid children). Same by-value
			// safety rule: invoke scope() to construct the return slot BEFORE Override(). NO re-entry: BuildCommitSpec
			// sums GetCost(false), which this hook ignores (the includeChildren==false guard returns at the top).
			FSFWalkCommitSpec WalkSpec;
			bool bHaveWalkCost = FindStagedWalkCommit(self, WalkSpec, /*bConsume=*/false);
			if (!bHaveWalkCost)
			{
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
				{
					if (USFWalkService* Walk = SS->GetWalkService();
                        !FSFExtendAuthorityScope::IsActive(SS) && Walk && Walk->IsActive())
					{
						WalkSpec = Walk->BuildCommitSpec();
						bHaveWalkCost = WalkSpec.bValid;
					}
				}
			}
			if (bHaveWalkCost)
			{
                if (USFWalkService::IsExistingCommitPrepared(MutableSelf)
                    || (FSFWalkNativeSpanAdapter::IsSpanType(WalkSpec.ConveyanceType) && USFWalkService::IsNativeSpanCommitPrepared(MutableSelf))) { return; }
				const int32 NumChildren = MutableSelf->GetHologramChildren().Num();
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[Walk-cost] Hook A %s: incl=%d children=%d cost-types=%d -> %s"),
					*self->GetName(), includeChildren ? 1 : 0, NumChildren, WalkSpec.Cost.Num(),
					(NumChildren == 0 && WalkSpec.Cost.Num() > 0) ? TEXT("CHARGING walk extras") : TEXT("SKIP"));
				if ((NumChildren == 0 || FSFWalkNativeSpanAdapter::IsSpanType(WalkSpec.ConveyanceType)) && WalkSpec.Cost.Num() > 0)
				{
					TArray<FItemAmount> Total = scope(self, includeChildren); // seed's own cost (slot initialized)
					for (const FItemAmount& W : WalkSpec.Cost)
					{
						bool bMerged = false;
						for (FItemAmount& T : Total)
						{
							if (T.ItemClass == W.ItemClass) { T.Amount += W.Amount; bMerged = true; break; }
						}
						if (!bMerged) { Total.Add(W); }
					}
					scope.Override(Total);
				}
				return;
			}

			// #487 standalone/listen-host preview: normal factory scaling keeps the vanilla
			// FGFactoryHologram, so charge the subsystem-owned daisy-chain spans here rather than
			// relying on the Extend-only ASFFactoryHologram replacement.
			if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld());
				CanUseLocalPowerPlan(MutableSelf, SS))
			{
				const TArray<FItemAmount> DaisyCost = SFScalingSpecExpansion::GetScaleDaisyChainPowerCost(MutableSelf);
				if (!DaisyCost.IsEmpty())
				{
					TArray<FItemAmount> Total = scope(self, includeChildren);
					for (const FItemAmount& Extra : DaisyCost)
					{
						bool bMerged = false;
						for (FItemAmount& Existing : Total)
						{
							if (Existing.ItemClass == Extra.ItemClass)
							{
								Existing.Amount += Extra.Amount;
								bMerged = true;
								break;
							}
						}
						if (!bMerged)
						{
							Total.Add(Extra);
						}
					}
					scope.Override(Total);
					return;
				}
			}

			FSFScalingSpec Spec;
			if (!FindStagedSpec(self, Spec, /*bConsume=*/false) || CountGridChildren(MutableSelf) > 0)
			{
				return; // no staged spec (or children already present) -> vanilla cost
			}

			TArray<FItemAmount> Cost = scope(self, includeChildren);
			const int32 Cells = Spec.CellCount();
			if (!SFSignHologramState::GetGridCost(self, Spec, Cost))
			{
				for (FItemAmount& Item : Cost) Item.Amount *= Cells;
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

            // [MP-AUTH] A native shaft extension replaces its exact snapped owner and
            // reassigns existing stops. It is not a new assembly grid. Keep its original
            // module group across native retirement, including a one-member group.
            if (self && self->HasAuthority() && self->GetWorld()->GetNetMode() != NM_Client
                && !self->GetParentHologram())
            {
                if (AFGBuildable* Original = SFElevatorHologramState::GetSnappedElevator(self))
                {
                    USFSubsystem* SessionSS = USFSubsystem::Get(self->GetWorld());
                    AFGPlayerController* PC = self->GetConstructionInstigator()
                        ? Cast<AFGPlayerController>(self->GetConstructionInstigator()->GetController()) : nullptr;
                    if (!SessionSS || !PC || !(PC->IsLocalController()
                        ? SessionSS->IsSmartEnabledForSession() : SessionSS->IsSmartEnabledForPlayer(PC))) return;
                    if (AFGBuildableBlueprintDesigner* Designer = Original->GetBlueprintDesigner())
                        self->SetInsideBlueprintDesigner(Designer);
                    TWeakObjectPtr<AFGBlueprintProxy> OriginalGroup = Original->GetBlueprintProxy();
                    FSFScalingSpec Discarded;
                    FindStagedSpec(self, Discarded, /*bConsume=*/true);
                    FSFReplacementGroupTransfer Membership(Original, true);
                    AActor* Replacement = scope(self, out_children, constructionID);
                    Membership.Apply(Cast<AFGBuildable>(Replacement));
                    UE_LOG(LogSmartFoundations, Log,
                        TEXT("Elevator extension: native owner replacement=%s original group retained=%d"),
                        *GetNameSafe(Replacement),
                        OriginalGroup.IsValid() && IsValid(Cast<AFGBuildable>(Replacement))
                            && Cast<AFGBuildable>(Replacement)->GetBlueprintProxy() == OriginalGroup.Get());
                    scope.Override(Replacement);
                    return;
                }
            }
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
					FSFWalkCommitSpec PeekWalk;
					if (FindStagedSpec(self, PeekSpec, /*bConsume=*/false) ||
					    FindStagedExtendCommit(self, PeekCommit, /*bConsume=*/false) ||
					    FindStagedWalkCommit(self, PeekWalk, /*bConsume=*/false))
					{
						const FVector ParentLocation = self->GetActorLocation();
						for (TActorIterator<AFGBuildableBlueprintDesigner> It(self->GetWorld()); It; ++It)
						{
							if (*It && It->IsLocationInsideDesigner(ParentLocation))
							{
								self->SetInsideBlueprintDesigner(*It);
								UE_LOG(LogSmartFoundations, Verbose,
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

			// [#523] Zoop stand-down at the commit seam. The tick-time guard (#160) detects vanilla
			// zoop and destroys Smart's preview children, but it runs one subsystem tick behind
			// player input: a build click landing between the zoop drag and Smart's next tick
			// constructs BOTH vanilla's zoop instances AND Smart's grid children at overlapping
			// transforms - the "duplicate buildings at the same location" the #160 guard exists to
			// prevent, and the leading suspect for the duplicated rain-occlusion hash behind
			// #523/#514. The commit point cannot race input: with zoop live on the constructing
			// hologram, strip Smart's grid children here, before vanilla's child-construct loop.
			if (self && self->HasAuthority() && self->GetZoopInstanceTransforms().Num() > 0)
			{
				static const FName ZoopStripGridChildTag(TEXT("SF_GridChild"));
				TArray<AFGHologram*> GridChildrenToStrip;
				for (AFGHologram* Child : self->GetHologramChildren())
				{
					if (Child && Child->Tags.Contains(ZoopStripGridChildTag))
					{
						GridChildrenToStrip.Add(Child);
					}
				}
				if (GridChildrenToStrip.Num() > 0)
				{
					for (AFGHologram* Child : GridChildrenToStrip)
					{
						self->mChildren.Remove(Child);
						Child->Destroy();
					}
					UE_LOG(LogSmartFoundations, Warning,
						TEXT("[#523] Zoop live at construct on %s: stripped %d Smart grid children before the child-construct loop (tick-time stand-down raced the build click)."),
						*self->GetName(), GridChildrenToStrip.Num());
				}
			}

			// [MP-AUTH] Port-aware power uses the same post-construct plan in standalone/listen
			// and dedicated play. Local previews already contributed their cost; remove only those
			// wire children while vanilla builds the grid, avoiding transient unconnected wires.
			if (self && self->HasAuthority() && self->GetZoopInstanceTransforms().Num() == 0)
			{
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld());
					CanUseLocalPowerPlan(self, SS))
				{
					TArray<AFGHologram*> ExactWireChildren;
					for (AFGHologram* Child : self->GetHologramChildren())
						if (IsValid(Child) && Child->Tags.Contains(TEXT("SF_ExactPowerPlan"))) ExactWireChildren.Add(Child);
					if (!ExactWireChildren.IsEmpty())
					{
						FSFScalingSpec LocalPowerPlan;
						SFScalingSpecExpansion::CaptureConduitPlan(self, LocalPowerPlan);
						LocalPowerPlan.ConduitPlan.RemoveAll([](const FSFConduitPlanEntry& Entry) { return !Entry.bExactPowerEndpoints; });
						for (AFGHologram* Child : ExactWireChildren) self->mChildren.Remove(Child);
						AActor* BuiltParent = scope(self, out_children, constructionID);
						AFGBuildable* Buildable = Cast<AFGBuildable>(BuiltParent);
						SFScalingSpecExpansion::SpawnWirePlanPostConstruct(BuiltParent, out_children, LocalPowerPlan,
							Buildable ? Buildable->GetBlueprintProxy() : nullptr);
						for (AFGHologram* Child : ExactWireChildren)
							if (IsValid(Child)) self->mChildren.AddUnique(Child);
						scope.Override(BuiltParent);
						return;
					}
				}
			}

			// #487 standalone/listen-host construction has no staged network spec. Wrap the normal
			// vanilla parent construct while its Smart grid children and counters still exist, then
			// use the same post-construct primitive as the authoritative staged path below.
			if (self && self->HasAuthority() && self->GetZoopInstanceTransforms().Num() == 0)
			{
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld());
					CanUseLocalPowerPlan(self, SS))
				{
					// [#523] Gated on zoop being inactive: with zoop live the counters can be stale
					// (tick-time stand-down raced the click) and daisy-chain power for a grid that
					// zoop, not Smart, is placing would wire against buildings that don't exist.
					const FSFCounterState Counters = SS->GetCounterState();
					const bool bDaisyEnabled = SS->GetAutoConnectRuntimeSettings().bScaleDaisyChainPower;
					if (bDaisyEnabled && FMath::Abs(Counters.GridCounters.X) >= 2)
					{
						AActor* BuiltParent = scope(self, out_children, constructionID);
						SFScalingSpecExpansion::SpawnScaleDaisyChainPowerPostConstruct(
							BuiltParent, out_children, Counters, true, nullptr);
						scope.Override(BuiltParent);
						return;
					}
				}
			}


			if (!self || !self->HasAuthority() || !SFScalingSpecExpansion::IsSpecConstructionEnabled())
			{
				return;
			}
			// Consume ALL staged specs for this seed (so none leaks to a later construct), then pick by intent below.
			// A stackable conveyor pole is ALSO a (1x1) scaling grid, so on a network CLIENT a committed walk stages
			// BOTH a walk commit AND a scaling spec for the same seed — the Extend/scaling staging runs below the
			// IsClient gate in the fire hook (skipped on SP/listen-host, which is why this only broke on a dedicated
			// server). The old short-circuit consumed the scaling spec and never checked the walk commit, so the run
			// never reconstructed and only the seed built.
			FSFScalingSpec Spec;
			bool bHasScaling = FindStagedSpec(self, Spec, /*bConsume=*/true);
			FSFExtendCommitSpec ExtendSpec;
			bool bHasExtend = FindStagedExtendCommit(self, ExtendSpec, /*bConsume=*/true);
			FSFWalkCommitSpec WalkSpec;
			const bool bHasWalk = FindStagedWalkCommit(self, WalkSpec, /*bConsume=*/true);
			// A committed WALK supersedes the incidental scaling/Extend spec a stackable pole also stages.
			if (bHasWalk) { bHasScaling = false; bHasExtend = false; }
			// Native zoop owns this placement, including a stale Extend request captured
			// before the tick-time stand-down. It was consumed above and cannot leak.
			if (self->GetZoopInstanceTransforms().Num() > 0) bHasExtend = false;
			// [#523] With zoop live on the constructing hologram, zoop owns placement: a staged
			// scaling spec is a stale capture from before the client's tick-time stand-down could
			// reset the counters. Expanding it would build Smart's grid on top of vanilla's zoop
			// instances at overlapping transforms. The spec is already consumed above, so it
			// cannot leak into a later construct; discard it here.
			if (bHasScaling && self->GetZoopInstanceTransforms().Num() > 0)
			{
				UE_LOG(LogSmartFoundations, Warning,
					TEXT("[#523] Zoop live at construct seam on %s: discarded a stale %d-cell staged scaling spec (zoop owns this placement)."),
					*self->GetName(), Spec.CellCount());
				bHasScaling = false;
			}
			if (!bHasScaling && !bHasExtend && !bHasWalk)
			{
				return; // nothing staged for this instigator/class (e.g. a child in the loop)
			}

			// [#365-MP] One line per staged construct: the designer state at the seam is the
			// load-bearing fact for designer-resident client builds (see re-derivation above).
			UE_LOG(LogSmartFoundations, Verbose,
				TEXT("[MP-SPEC] Construct seam: %s (%s staged), designer=%s."),
				*self->GetName(), bHasScaling ? TEXT("scaling spec") : (bHasExtend ? TEXT("extend commit") : TEXT("walk commit")),
				self->GetBlueprintDesigner() ? *self->GetBlueprintDesigner()->GetName() : TEXT("none"));

			if (bHasScaling)
			{
				if (CountGridChildren(self) == 0)
				{
					// [#418-MP] Past the cell threshold, expansion + construction is DEFERRED and
					// time-sliced across server frames: inline it blocked one server frame ~57s at
					// ~45K cells and timed out every client connection (2026-07-03). Conduit-plan
					// (#334) and designer-resident grids keep the inline path - their correctness
					// depends on the vanilla child-construct ordering at this seam.
					if (SFScalingSpecExpansion::ShouldDeferSpecExpansion(self, Spec))
					{
						SFScalingSpecExpansion::BeginDeferredSpecExpansion(self, Spec, self->GetRecipe(), constructionID);
					}
					else
					{
						SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(self, Spec, self->GetRecipe());
					}
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
                if (!self->ActorHasTag(TEXT("SF_LiftPlanPrepared")))
                    SFScalingSpecExpansion::SpawnConduitPlanChildren(self, Spec);

				// Expansion (or the deferred template) has captured the native pole state now.
				SFSignHologramState::ApplyParentConstruction(self, Spec);
			}
			else if (bHasExtend)
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

                        if (const bool* Prepared = SFExtendCommitValidation::FindPrepared(self))
                        {
                            if (!*Prepared)
                            {
                                UE_LOG(LogSmartFoundations, Error, TEXT("Native construction reached an invalid Extend plan after validation."));
                                scope.Override(nullptr);
                                return;
                            }
                            // Reuse the exact children/cost validated earlier in this native request.
                        }
                        else Extend->ReconstructCommitOnServer(self, ExtendSpec);
					}
				}
			}
			else if (bHasWalk)
			{
				// Smart Walking (#356 Slice 3): reconstruct the walk's poles + belts server-side, pre-scope():
				// re-derive every frame from the per-segment deltas (AccumulateFrame), spawn the cross-section
				// poles + spanning belts AddChild'd to this seed + pre-wired. The scope() cascade below builds
				// them; the belts coincidence-wire via the existing ASFConveyorBeltHologram construct path. The
				// client only ever previewed standalone holograms (discarded on commit) - the authoritative
				// geometry is re-derived here from the parameters-only spec, exactly like the Extend commit.
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
				{
					if (USFWalkService* Walk = SS->GetWalkService())
					{
						if (Walk->ReconstructWalkCommitOnServer(self, WalkSpec) < 0)
                        {
                            scope.Override(nullptr);
                            return;
                        }
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

			int32 ExpectedFactories = 1;
			if (bHasExtend)
			{
				for (AFGHologram* Child : self->GetHologramChildren())
					if (IsValid(Child) && Child->GetBuildClass() == self->GetBuildClass()) ++ExpectedFactories;
			}
			AActor* BuiltParent = scope(self, out_children, constructionID);

			GSFActiveSpecGroupProxy.Reset();

			if (bHasExtend)
			{
				TSet<AActor*> BuiltFactories;
				if (IsValid(BuiltParent) && BuiltParent->GetClass() == self->GetBuildClass()) BuiltFactories.Add(BuiltParent);
				for (AActor* Child : out_children)
					if (IsValid(Child) && Child->GetClass() == self->GetBuildClass()) BuiltFactories.Add(Child);
				UE_LOG(LogSmartFoundations, Log,
					TEXT("Extend construction: root=%s prepared=%d expectedFactories=%d builtFactories=%d builtChildren=%d restore=%d"),
					*GetNameSafe(BuiltParent), SFExtendCommitValidation::FindPrepared(self) != nullptr,
					ExpectedFactories, BuiltFactories.Num(), out_children.Num(), ExtendSpec.bIsRestore);
				if (BuiltFactories.Num() != ExpectedFactories)
					UE_LOG(LogSmartFoundations, Error, TEXT("Extend construction result does not match its prepared factory count."));
			}

			// [#515-#517] Hand the exact staged value contract to every built factory. The
			// service-owned queue waits for BeginPlay/inventories and consumes from this instigator.
			if (bHasScaling || bHasExtend)
			{
				const FSFFactorySettingsSnapshot& Settings = bHasScaling
					? Spec.FactorySettings
					: ExtendSpec.FactorySettings;
				if (Settings.HasAnySettings())
				{
					if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
					{
						if (USFRecipeManagementService* RecipeService = SS->GetRecipeManagementService())
						{
							AFGCharacterPlayer* Player = Cast<AFGCharacterPlayer>(self->GetConstructionInstigator());
							RecipeService->QueueFactorySettingsApplication(Cast<AFGBuildable>(BuiltParent), Player, Settings);
							for (AActor* BuiltChild : out_children)
							{
								RecipeService->QueueFactorySettingsApplication(Cast<AFGBuildable>(BuiltChild), Player, Settings);
							}
						}
					}
				}
			}

			// [MP-AUTH] ConfigureActor registers exact clone identity. Native subclasses
            // that bypass that callback use the same unique class/transform match as SP,
            // restricted to this Construct's output. No existing world actor is a candidate.
			if (bHasExtend)
			{
				if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
				{
					if (USFExtendService* Extend = SS->GetExtendService())
					{
						const int32 RegisteredAnchors = SFExtendBuiltActors::RegisterChildren(Extend, self, out_children);
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
				SFScalingSpecExpansion::SpawnScaleDaisyChainPowerPostConstruct(
					BuiltParent, out_children, Spec.Counters, Spec.bScaleDaisyChainPower, GroupProxy);
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

    // The explicit plan is the sole wire materializer. The native child loop must
    // not spawn the old unconnected raw wire as well. Wrap scope() explicitly so
    // early-returning handlers still enter this wrapper before native construction.
    SUBSCRIBE_METHOD_VIRTUAL(AFGBuildableHologram::Construct, BuildableHologramCDO,
        [](auto& scope, AFGBuildableHologram* self, TArray<AActor*>& Children, FNetConstructionID Id)
        {
            FSFExtendWirePreviewScope PreviewScope(self->mChildren);
            AActor* Built = scope(self, Children, Id);
            scope.Override(Built);
        });

	// ── Hook D: server-side placement leniency for staged Extend commits. SP's swapped parent
	// (ASFFactoryHologram) SKIPS clearance checks during Extend - the clone is deliberately placed
	// adjacent to the source with logistics in between, which full vanilla clearance rejects. The
	// server constructs through the VANILLA hologram, so the same fire that previews valid on the
	// client was refused server-side (live 2026-06-10: FGCDEncroachingClearance from
	// ValidatePlacementAndCost). Mirror SP: when a staged Extend commit exists for this hologram,
	// clear the disqualifiers after vanilla evaluates them.
	// ── [#497] Vanilla-delegate grid children (Tier-3 stackable supports, SF_GridChild tag):
	// two vanilla per-frame paths scale O(children) and burned the game thread at a 3,600-pole
	// stackable grid (capture 6: 54% per-child CheckClearance Chaos overlaps, 21% reposition
	// tug-of-war paying UpdateOverlaps on collision-enabled children):
	//  1) ValidatePlacementAndCost recurses every child -> CheckValidPlacement -> CheckClearance
	//     overlap query per child per frame. Smart child classes no-op validation (#354); the
	//     vanilla-delegate classes can't be overridden, so cancel at the hook seam.
	//  2) The locked-parent nudge cascade (blocked on Smart classes in 78f3e06) still drags
	//     vanilla-delegate children every tick, and the Phase-4 refresh drags them back.
	//     Cancel the child-side nudge for tagged children; the parent's own nudge still moves
	//     children through Smart's transform follow.
	{
		SUBSCRIBE_METHOD_VIRTUAL(AFGBuildableHologram::CheckValidPlacement, BuildableHologramCDO,
			[](auto& scope, AFGBuildableHologram* self)
			{
				if (SFIsSmartGridChildOrDescendant(self) && !FSFBlueprintLiftSeams::IsNativeValidation(self))
				{
					return; // Smart-positioned child: skip vanilla validation entirely
				}
				if (SFIsSmartGridMegaRoot(self))
				{
					return; // mega-grid root: vanilla clearance is O(candidates x children)
				}
				scope(self);
			});
		// SML virtual hooks bind ONE function body, not the whole vtable slot: the stackable pole
		// children dispatch to the AFGFactoryBuildingHologram OVERRIDE and never enter the
		// AFGBuildableHologram body above (capture 7: clearance burn survived the first hook).
		// Hook the override too; all cancels share the ancestor-tag predicate.
		AFGFactoryBuildingHologram* FactoryBuildingHologramCDO = GetMutableDefault<AFGFactoryBuildingHologram>();
		SUBSCRIBE_METHOD_VIRTUAL(AFGFactoryBuildingHologram::CheckValidPlacement, FactoryBuildingHologramCDO,
			[](auto& scope, AFGFactoryBuildingHologram* self)
			{
				if (SFIsSmartGridChildOrDescendant(self) && !FSFBlueprintLiftSeams::IsNativeValidation(self))
				{
					return; // Smart-positioned child: skip vanilla validation entirely
				}
				if (SFIsSmartGridMegaRoot(self))
				{
					return; // mega-grid root: vanilla clearance is O(candidates x children)
				}
				scope(self);
			});
		SUBSCRIBE_METHOD_VIRTUAL(AFGHologram::SetHologramNudgeLocation, HologramCDO,
			[](auto& scope, AFGHologram* self)
			{
				if (SFIsSmartGridChildOrDescendant(self))
				{
					return; // block the locked-parent nudge cascade for Smart-positioned children
				}
				scope(self);
			});
	}

	// NOTE: hooked at the AFGBuildableHologram override, NOT the AFGHologram base -
	// &AFGHologram::CheckValidPlacement resolves to an unhookable import thunk (live dedi
	// startup FATAL 2026-06-10: "resulting function still points to a thunk"). The override
	// covers every buildable hologram, which is exactly the set that can carry a staged commit.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(AFGBuildableHologram::CheckValidPlacement, BuildableHologramCDO,
		[=](AFGBuildableHologram* self)
		{
			// Local sign grids retain native first-step height selection. At the final
			// step, show the same invalid-support decision used by the fire guard.
			if (SFSignHologramState::ShouldRejectGridPreview(self))
			{
				self->AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
			}
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

	// ── [#168] SMART! BLUEPRINTS: construct the scaled blueprint grid children.
	// AFGBlueprintHologram::Construct is a SELF-CONTAINED override: it builds the blueprint's
	// own contents and never enters the AFGBuildableHologram::Construct body (Hook B above) nor
	// the base child-construct loop - so the SF_GridChild blueprint children Smart scaling
	// attaches were silently discarded at fire time ("only the parent builds"; live-confirmed
	// 2026-07-06: no construct-seam log for a blueprint parent). Hook the blueprint body
	// directly: run the original, then construct each staged blueprint grid child with the same
	// construction id. Authority path (SP owns it outright); MP client construct-messaging for
	// blueprint grids is an explicit follow-up slice.
	AFGBlueprintHologram* BlueprintHologramCDO = GetMutableDefault<AFGBlueprintHologram>();
	SUBSCRIBE_METHOD_VIRTUAL(AFGBlueprintHologram::Construct, BlueprintHologramCDO,
		[=](auto& scope, AFGBlueprintHologram* self, TArray<AActor*>& out_children, FNetConstructionID constructionID)
		{
			static const FName BlueprintGridChildTag(TEXT("SF_GridChild"));
			static const FName SeamBeltTag(TEXT("SF_BeltAutoConnectChild"));
			static const FName SeamPipeTag(TEXT("SF_PipeAutoConnectChild"));
			static const FName SeamStackablePipeTag(TEXT("SF_StackableChild"));
			FSFScalingSpec BlueprintPowerPlan;
			if (self && self->HasAuthority())
			{
				SFScalingSpecExpansion::CaptureConduitPlan(self, BlueprintPowerPlan);
				BlueprintPowerPlan.ConduitPlan.RemoveAll([](const FSFConduitPlanEntry& Entry) { return Entry.Kind != ESFConduitPlanKind::Wire || !Entry.bExactPowerEndpoints; });
			}

			// ── [#168-MP] SERVER-SIDE RE-EXPANSION for blueprint parents. A network client's fire
			// strips the SF_GridChild copies + seam conduits and stages the spec (the standard
			// spec-construction model), but the generic re-expansion lives in Hook B's
			// AFGBuildableHologram::Construct body - which this SELF-CONTAINED override never
			// enters. Consume + expand HERE instead: grid copies first (the expansion's blueprint
			// cell staging loads each copy's blueprint world), then the client's captured conduit
			// plan (seam belts/pipes ride the same #334 plan as every other auto-connect family) -
			// appended after the copies so the construct loops below fire them against BUILT
			// actors. Native copies keep their own proxies; generated seam conduits receive
			// their own group after materialization, independent of local preview state.
			// SP is untouched: nothing stages a spec outside a network-client fire, and the
			// preview children (present in SP) suppress expansion via CountGridChildren.
			if (self && self->HasAuthority() && SFScalingSpecExpansion::IsSpecConstructionEnabled())
			{
				FSFScalingSpec BlueprintSpec;
				if (FindStagedSpec(self, BlueprintSpec, /*bConsume=*/true) && BlueprintSpec.bValid)
				{
					if (CountGridChildren(self) == 0)
					{
						const int32 Expanded = SFScalingSpecExpansion::ExpandScalingSpecIntoChildren(
							self, BlueprintSpec, self->GetRecipe());
						UE_LOG(LogSmartFoundations, Verbose,
							TEXT("[#168-MP] Blueprint construct seam %s: expanded %d grid cop%s server-side (delta=%s)."),
							*self->GetName(), Expanded, Expanded == 1 ? TEXT("y") : TEXT("ies"),
							*BlueprintSpec.BlueprintContentDelta.ToCompactString());
					}
					// [#168-MP] Shift the conduit plan by the MEASURED parent-convention difference:
					// the plan is routed in the client world (content anchored at the client
					// parent's convention, carried in the spec); the server world anchors at the
					// SERVER parent's convention, measured here. Zero when they agree - they
					// provably vary per blueprint and staging context (live 2026-07-07: a carried
					// constant fixed FluidGrid and broke TestBP by exactly itself). Cells align to
					// the same measured parent anchor inside the expansion, so world + plan agree.
					if (BlueprintSpec.ConduitPlan.Num() > 0)
					{
						const FVector ServerParentAnchor = SFScalingSpecExpansion::MeasureBlueprintContentAnchor(self);
						const FVector PlanShift = self->GetActorRotation().RotateVector(
							ServerParentAnchor - BlueprintSpec.ClientParentAnchorRel);
						if (!PlanShift.IsNearlyZero(0.5f))
						{
							for (FSFConduitPlanEntry& Entry : BlueprintSpec.ConduitPlan)
							{
								Entry.Location += PlanShift;
								Entry.WireStart += PlanShift;
								Entry.WireEnd += PlanShift;
								if (!Entry.PowerStart.bExisting) Entry.PowerStart.OwnerLocation += PlanShift;
								if (!Entry.PowerEnd.bExisting) Entry.PowerEnd.OwnerLocation += PlanShift;
							}
						}
						UE_LOG(LogSmartFoundations, Verbose,
							TEXT("[#168-MP] Blueprint construct seam %s: plan shift %s (serverAnchor=%s clientAnchor=%s)."),
							*self->GetName(), *PlanShift.ToCompactString(),
							*ServerParentAnchor.ToCompactString(), *BlueprintSpec.ClientParentAnchorRel.ToCompactString());
					}
                    const int32 Conduits = FSFBlueprintRailSeams::IsPrepared(self) ? 0
                        : SFScalingSpecExpansion::SpawnConduitPlanChildren(self, BlueprintSpec);
					BlueprintPowerPlan = BlueprintSpec;
					BlueprintPowerPlan.ConduitPlan.RemoveAll([](const FSFConduitPlanEntry& Entry) { return Entry.Kind != ESFConduitPlanKind::Wire || !Entry.bExactPowerEndpoints; });
					if (Conduits > 0)
					{
						UE_LOG(LogSmartFoundations, Verbose,
							TEXT("[#168-MP] Blueprint construct seam %s: staged %d seam conduit(s) from the client plan."),
							*self->GetName(), Conduits);
					}
				}
			}

			// Snapshot the staged blueprint children BEFORE the original runs - construction
			// tears hologram state down as it goes. [#168] Seam CONDUIT children (the belt/pipe
			// previews seam auto-connect AddChild'd to this parent) are snapshotted too: the
			// blueprint Construct override never runs the base child-construct loop, so without
			// this they'd be silently discarded at fire exactly like the grid children were.
			TArray<AFGBlueprintHologram*> BlueprintChildren;
			TArray<AFGHologram*> SeamConduitChildren;
			if (self && self->HasAuthority())
			{
				for (AFGHologram* Child : self->GetHologramChildren())
				{
					if (!Child)
					{
						continue;
					}
					if (Child->ActorHasTag(BlueprintGridChildTag))
					{
						if (AFGBlueprintHologram* BlueprintChild = Cast<AFGBlueprintHologram>(Child))
						{
							BlueprintChildren.Add(BlueprintChild);
						}
					}
					else if (Child->ActorHasTag(SeamBeltTag) || Child->ActorHasTag(SeamPipeTag)
						|| (Child->ActorHasTag(SeamStackablePipeTag) && Child->IsA<ASFPipelineHologram>()))
					{
                        if (Child->ActorHasTag(TEXT("SF_BlueprintLiftSeam"))) continue;
						SeamConduitChildren.Add(Child);
					}
				}
			}

			const int32 MembersBeforeNative = out_children.Num();
            AActor* BuiltBlueprint = FSFBlueprintRailSeams::ConstructCopy(self,
                [&]() { return scope(self, out_children, constructionID); });
            TArray<AActor*> CopyMembers;
            for (int32 I = MembersBeforeNative; I < out_children.Num(); ++I) CopyMembers.Add(out_children[I]);
            FSFBlueprintVehicleSeams::RecordCopy(self, BuiltBlueprint, CopyMembers);

			for (AFGBlueprintHologram* BlueprintChild : BlueprintChildren)
			{
				if (!IsValid(BlueprintChild))
				{
					continue;
				}
				TArray<AActor*> ChildBuilt;
				AActor* BuiltMain = BlueprintChild->Construct(ChildBuilt, constructionID);
				if (BuiltMain)
				{
					out_children.Add(BuiltMain);
				}
				out_children.Append(ChildBuilt);
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[#168] Constructed blueprint grid child %s -> %s (+%d actors)"),
					*BlueprintChild->GetName(), *GetNameSafe(BuiltMain), ChildBuilt.Num());
			}

			// [MP-AUTH] Power is also built only now, including each copy's internal cables.
            // Native rail managers remap only into actors from these exact copies.
            TArray<AActor*> BuiltVehicleSeams;
            FSFBlueprintVehicleSeams::Construct(self, constructionID, BuiltVehicleSeams);
            for (auto* Actor : BuiltVehicleSeams) out_children.AddUnique(Actor);
            FSFBlueprintGroupReplication::GroupConstructedSeams(BuiltVehicleSeams);
            TArray<AActor*> BuiltRailSeams;
            FSFBlueprintRailSeams::Construct(self, out_children, constructionID, BuiltRailSeams);
            for (auto* Actor : BuiltRailSeams) out_children.AddUnique(Actor);
            FSFBlueprintGroupReplication::GroupConstructedSeams(BuiltRailSeams);
            TArray<AActor*> BuiltLiftSeams;
            FSFBlueprintLiftSeams::Construct(self, out_children, constructionID, BuiltLiftSeams);
            for (auto* Actor : BuiltLiftSeams) out_children.AddUnique(Actor);
            FSFBlueprintGroupReplication::GroupConstructedSeams(BuiltLiftSeams);
			if (!BlueprintPowerPlan.ConduitPlan.IsEmpty())
				SFScalingSpecExpansion::SpawnWirePlanPostConstruct(BuiltBlueprint, out_children, BlueprintPowerPlan, nullptr);

			// [#168] Seam conduits fire AFTER every blueprint copy so their SF-tagged Construct
			// paths wire geometrically against just-BUILT actors (the same ordering contract the
			// scaling-spec expansion relies on).
			TArray<TWeakObjectPtr<AFGBuildablePipeBase>> BuiltSeamPipes;
			TArray<AActor*> BuiltSeamActors;
			for (AFGHologram* Conduit : SeamConduitChildren)
			{
				if (!IsValid(Conduit))
				{
					continue;
				}
				TArray<AActor*> ConduitBuilt;
				AActor* BuiltMain = Conduit->Construct(ConduitBuilt, constructionID);
				if (AFGBuildablePipeBase* Pipe = Cast<AFGBuildablePipeBase>(BuiltMain))
				{
					BuiltSeamPipes.AddUnique(Pipe);
				}
				if (BuiltMain)
				{
					out_children.Add(BuiltMain);
				}
				out_children.Append(ConduitBuilt);
				if (BuiltMain) BuiltSeamActors.AddUnique(BuiltMain);
				BuiltSeamActors.Append(ConduitBuilt);
				UE_LOG(LogSmartFoundations, Verbose,
					TEXT("[#168] Constructed blueprint seam conduit %s -> %s (+%d actors)"),
					*Conduit->GetName(), *GetNameSafe(BuiltMain), ConduitBuilt.Num());
			}

            // Native copies retain their own proxies. The early spawn observer may have
            // registered them in the seam group before those owners were assigned.
            // [MP-AUTH] Dedicated construction has no local grid observer to create this group.
            // Scope it to this operation's conduits, preserving every native copy's owner.
            FSFBlueprintGroupReplication::GroupConstructedSeams(BuiltSeamActors);
            FSFBlueprintGroupReplication::ReconcileConstructedGroups(out_children);

			// Native Blueprint construction tears down duplicated preview connectors. Resolve
			// the final network ports after that cleanup, without relying on child registry data
			// or GetParentHologram() surviving the native Construct call. This covers fluid and
			// Hypertube seams; only this operation's newly built conduits are revisited.
			if (!BuiltSeamPipes.IsEmpty() && self && self->GetWorld())
			{
				TWeakObjectPtr<USFSubsystem> WeakSubsystem = USFSubsystem::Get(self->GetWorld());
				self->GetWorld()->GetTimerManager().SetTimerForNextTick([WeakSubsystem, BuiltSeamPipes]()
				{
					if (USFSubsystem* Subsystem = WeakSubsystem.Get())
					{
						for (const TWeakObjectPtr<AFGBuildablePipeBase>& Pipe : BuiltSeamPipes)
						{
							if (Pipe.IsValid()) Subsystem->WireBlueprintSeamPipe(Pipe.Get());
						}
					}
				});
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
			// Exact hologram-to-buildable identity is available before native BeginPlay.
            // Registration is independent of group proxies, including Designer builds.
            if (self && IsValid(inBuildable) && inBuildable->HasAuthority())
            {
                const FSFHologramData* Data = USFHologramDataRegistry::GetData(self);
                if (Data && !Data->JsonCloneId.IsEmpty() && self->GetBuildClass() == inBuildable->GetClass())
                {
                    if (USFSubsystem* SS = USFSubsystem::Get(self->GetWorld()))
                        if (USFExtendService* Extend = SS->GetExtendService())
                            Extend->RegisterJsonBuiltActor(Data->JsonCloneId, inBuildable);
                }
            }
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
