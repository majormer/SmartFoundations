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
#include "SmartFoundations.h"
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
#include "Patching/NativeHookManager.h"
#include "Subsystem/SFSubsystem.h"
#include "Buildables/FGBuildableManufacturer.h"  // [#368] server-authoritative recipe apply
#include "Services/SFChainActorService.h"  // [CHAIN-FIX] post-construct chain-hygiene sweep
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/Extend/SFExtendService.h"
#include "Equipment/FGBuildGunBuild.h"        // UFGBuildGunStateBuild::InternalConstructHologram / GetHologram
#include "Core/Net/SFNetworkHelper.h"     // FSFNetworkHelper::IsClient
#include "Engine/Engine.h"                     // GEngine on-screen message
#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Data/SFHologramDataRegistry.h"   // [EXTEND-MP] JsonCloneId wiring anchors post-construct
#include "Subsystem/SFHologramHelperService.h"
#include "Core/Net/SFRCO.h"
#include "FGPlayerController.h"
#include "FGBlueprintProxy.h"   // server-side Smart Dismantle group for spec-built grids
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
					UE_LOG(LogSmartFoundations, Verbose,
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
				UE_LOG(LogSmartFoundations, Verbose,
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
			UE_LOG(LogSmartFoundations, Verbose,
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

			// [#368] Apply the staged remembered production recipe to the built manufacturer(s) with
			// SERVER AUTHORITY. Manual recipe-memory is client-side only and never reached the
			// authoritative build on a dedicated server: normal manufacturer placement constructs
			// through VANILLA holograms server-side (Smart's ConfigureActor recipe-apply only runs on
			// the Extend-swapped ASFFactoryHologram, not here), and the OnActorSpawned apply path is
			// correctly gated off when no active hologram exists on the server (#368 gate). So this
			// construct seam is the ONLY place a manually-remembered recipe both EXISTS
			// (Spec.ProductionRecipe, shipped client->server on FSFScalingSpec) and can be set with
			// authority (mCurrentRecipe is server-authoritative and replicates to every client).
			// Applied to the parent and every spec-expanded grid child. Post-scope is a proven seam
			// for operating on freshly built actors (the Extend wiring pass below runs here too).
			// Scaling path only: Extend clones inherit the SOURCE building's recipe via the server
			// topology walk, and the Restore sub-path carries its own (RestoreProductionRecipe).
			if (bHasScaling && Spec.ProductionRecipe)
			{
				UWorld* const RecipeWorld = self->GetWorld();
				const TSubclassOf<UFGRecipe> RecipeToApply = Spec.ProductionRecipe;
				// NOTE: [=] (not [RecipeWorld, RecipeToApply]) - this lambda sits inside the
				// SUBSCRIBE_METHOD_VIRTUAL macro, and a bare multi-capture comma would be parsed as an
				// extra macro argument (C4002). [=] is comma-free and safe: called synchronously below.
				auto ApplyServerRecipe = [=](AActor* Built)
				{
					AFGBuildableManufacturer* Mfg = Cast<AFGBuildableManufacturer>(Built);
					if (!Mfg)
					{
						return;
					}
					if (Mfg->HasActorBegunPlay())
					{
						Mfg->SetRecipe(RecipeToApply);
					}
					else if (RecipeWorld)
					{
						// Built but not yet begun play: defer briefly so SetRecipe takes - the same
						// reason USFRecipeManagementService::ApplyRecipeDelayed retries on
						// !HasActorBegunPlay. Weak ptr so a dismantle in the gap can't dangle.
						TWeakObjectPtr<AFGBuildableManufacturer> WeakMfg(Mfg);
						FTimerHandle DeferHandle;
						RecipeWorld->GetTimerManager().SetTimer(DeferHandle,
							[WeakMfg, RecipeToApply]()
							{
								if (AFGBuildableManufacturer* M = WeakMfg.Get())
								{
									M->SetRecipe(RecipeToApply);
								}
							}, 0.1f, false);
					}
				};
				ApplyServerRecipe(BuiltParent);
				for (AActor* BuiltChild : out_children)
				{
					ApplyServerRecipe(BuiltChild);
				}
			}

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
