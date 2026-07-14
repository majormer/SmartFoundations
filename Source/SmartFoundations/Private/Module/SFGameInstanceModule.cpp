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
#include "Services/SFRecipeManagementService.h"
#include "Services/SFChainActorService.h"  // [CHAIN-FIX] post-construct chain-hygiene sweep
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/Extend/SFExtendService.h"

// MP Slice 0 (Phase 1): client construct chunk guard
#include "Equipment/FGBuildGunBuild.h"        // UFGBuildGunStateBuild::InternalConstructHologram / GetHologram
#include "Equipment/FGBuildGun.h"             // [#368] AFGBuildGun::GetBuildGunStateFor / EBuildGunState
#include "FGCharacterPlayer.h"                // [#368] AFGCharacterPlayer::GetBuildGun
#include "Buildables/FGBuildableManufacturer.h" // [#368] UFGManufacturerClipboardSettings
#include "FGRecipe.h"                         // [#368] UFGRecipe
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

		// [#368/#279] Wire the orphaned holster cleanup to the real build-gun unequip event.
		RegisterBuildGunUnequipHook();

		// [#489] Let vanilla's clipboard decision govern implicit recipe/shard/Somersloop capture.
		RegisterBuildGunClipboardSampleHook();

		// [#162/#429] Consume wheel rotation at the build-gun chokepoint while Smart! owns the
		// moment (fixes the InfiniteNudge rotate-while-scaling conflict). See the header comment.
		RegisterBuildGunScrollSuppressionHook();

		// #342: capture Hold directly; polling the transient unlocked state is aim/tick-order dependent.
		RegisterExtendHologramLockHook();

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

	UE_LOG(LogSmartFoundations, Verbose, TEXT("⛓️ #341 %s HOOK: in-frame rebuilt %d/%d belt-support belt(s) on %s"),
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

// [#368] Sync the player's vanilla build-gun clipboard recipe so vanilla's PasteSettings applies the
// SAME recipe Smart's spec-construction does. See the header for the full rationale. Friend access to
// UFGBuildGunStateBuild::mSampledClipboardSettings comes from Config/AccessTransformers.ini.
void USFGameInstanceModule::SetBuildStateClipboardRecipe(AFGCharacterPlayer* Player, TSubclassOf<UFGRecipe> Recipe)
{
	if (!Player)
	{
		return;
	}
	AFGBuildGun* Gun = Player->GetBuildGun();
	if (!Gun)
	{
		return;
	}
	// Resolve the BUILD state object directly (not GetCurrentState) so this works even when the gun
	// has already left build state on holster - the clear path needs that.
	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(Gun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		return;
	}

	if (!Recipe)
	{
		// Clear: restore recipe-less placement. Only drop a manufacturer clipboard Smart may have set;
		// never stomp a non-manufacturer sampled clipboard the player legitimately holds.
		if (Cast<UFGManufacturerClipboardSettings>(BuildState->mSampledClipboardSettings))
		{
			BuildState->mSampledClipboardSettings = nullptr;
		}
		return;
	}

	// Read-modify-write: change only the recipe, preserving any sampled overclock/Somersloop on an
	// existing manufacturer clipboard. Create a neutral clipboard if there isn't one yet.
	UFGManufacturerClipboardSettings* Settings = Cast<UFGManufacturerClipboardSettings>(BuildState->mSampledClipboardSettings);
	if (!Settings)
	{
		Settings = NewObject<UFGManufacturerClipboardSettings>(BuildState);
		Settings->mTargetPotential = 1.0f;
		Settings->mTargetProductionBoost = 0.0f;
		Settings->mReachablePotential = 1.0f;
		Settings->mReachableProductionBoost = 0.0f;
		Settings->mOverclockingShardDescriptor = nullptr;
		Settings->mProductionBoostShardDescriptor = nullptr;
		BuildState->mSampledClipboardSettings = Settings;
	}
	Settings->mRecipe = Recipe;
}

void USFGameInstanceModule::RegisterBuildGunUnequipHook()
{
	// [#368/#279] AFGBuildGun::UnEquip fires on a true holster (weapon in hand / different equipment),
	// unlike just opening the build menu. We hold the gun here, so the build state (and its clipboard)
	// is reachable even though the player is leaving build mode. This is the live trigger the orphaned
	// USFSubsystem::OnBuildGunUnequipped() always needed.
	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGBuildGun::UnEquip,
		GetMutableDefault<AFGBuildGun>(),
		[](AFGBuildGun* self)
		{
			if (!self)
			{
				return;
			}

			AFGCharacterPlayer* Instigator = self->GetInstigatorCharacter();

			// Confirmed 2026-06-20: the instigator is ALWAYS detached by the time this AFTER hook runs
			// (logged instigator=None / locallyControlled=NO on every holster in a live SP test) - which
			// is why the original "bail if !Player" gate never cleared the recipe. The local-build-gun
			// fallback below is the real working path. Verbose: kept for future diagnosis, quiet by default.
			UE_LOG(LogSmartFoundations, Verbose,
				TEXT("[#392] AFGBuildGun::UnEquip: gun=%s instigator=%s locallyControlled=%s"),
				*GetNameSafe(self), *GetNameSafe(Instigator),
				(Instigator && Instigator->IsLocallyControlled()) ? TEXT("YES") : TEXT("NO"));

			// Local player only - never react to another player's replicated unequip on a listen host.
			// The instigator can be detached mid-UnEquip (our AFTER hook runs after the body), so a hard
			// "bail if instigator null" would silently skip the holster clear in exactly the case we care
			// about. When the instigator is gone, match this gun against the LOCAL player's build gun
			// instead: that stays correct on a listen host (a remote player's gun won't match) and on a
			// dedicated server (no local controller -> never matches), while still firing in SP/client.
			bool bIsLocal = false;
			if (Instigator)
			{
				bIsLocal = Instigator->IsLocallyControlled();
			}
			else if (UWorld* World = self->GetWorld())
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					if (PC->IsLocalController())
					{
						AFGCharacterPlayer* LocalChar = Cast<AFGCharacterPlayer>(PC->GetPawn());
						bIsLocal = (LocalChar && LocalChar->GetBuildGun() == self);
					}
				}
			}
			if (!bIsLocal)
			{
				return;
			}

			// Clear the LOCAL build-gun clipboard recipe directly via this gun (reliable mid-unequip).
			if (UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(self->GetBuildGunStateFor(EBuildGunState::BGS_BUILD)))
			{
				if (Cast<UFGManufacturerClipboardSettings>(BuildState->mSampledClipboardSettings))
				{
					BuildState->mSampledClipboardSettings = nullptr;
				}
			}

			// Run the holster cleanup: clears Smart's remembered recipe (HUD resets) and fires the
			// per-player RCO to clear the SERVER's clipboard copy, plus the existing context cleanup.
			if (USFSubsystem* Subsystem = USFSubsystem::Get(self->GetWorld()))
			{
				Subsystem->OnBuildGunUnequipped();
			}
		});

	UE_LOG(LogSmartFoundations, Verbose, TEXT("[#392] AFGBuildGun::UnEquip hook registered (holster recipe + clipboard clear)"));
}

static USFRecipeManagementService* SF_GetLocalRecipeService(UFGBuildGunStateBuild* BuildState)
{
	AFGBuildGun* Gun = BuildState ? BuildState->GetBuildGun() : nullptr;
	AFGCharacterPlayer* Player = Gun ? Gun->GetInstigatorCharacter() : nullptr;
	if (!Player || !Player->IsLocallyControlled())
	{
		return nullptr;
	}

	USFSubsystem* Subsystem = USFSubsystem::Get(Gun->GetWorld());
	return Subsystem ? Subsystem->GetRecipeManagementService() : nullptr;
}

void USFGameInstanceModule::RegisterBuildGunClipboardSampleHook()
{
	// Reset before vanilla makes its copy decision. If the gameplay option is OFF,
	// SampleClipboardSettingsFromActor is never reached and the reset is the final state.
	SUBSCRIBE_METHOD_VIRTUAL(
		UFGBuildGunStateBuild::OnActorSampled_Implementation,
		GetMutableDefault<UFGBuildGunStateBuild>(),
		[](auto& scope, UFGBuildGunStateBuild* self, AActor* actor)
		{
			if (USFRecipeManagementService* RecipeService = SF_GetLocalRecipeService(self))
			{
				RecipeService->BeginImplicitSettingsSample();
			}
		});

	// This non-virtual function is vanilla's authoritative settings-copy seam. Its presence in the
	// current actor-sampling call means the gameplay preference allowed copying; the resulting
	// manufacturer clipboard verifies that the sampled actor supplied compatible factory settings.
	SUBSCRIBE_METHOD_AFTER(
		UFGBuildGunStateBuild::SampleClipboardSettingsFromActor,
		[](UFGBuildGunStateBuild* self, AActor* actor)
		{
			USFRecipeManagementService* RecipeService = SF_GetLocalRecipeService(self);
			AFGBuildableManufacturer* Manufacturer = Cast<AFGBuildableManufacturer>(actor);
			UFGManufacturerClipboardSettings* Clipboard = self
				? Cast<UFGManufacturerClipboardSettings>(self->mSampledClipboardSettings)
				: nullptr;
			if (RecipeService && Manufacturer && Clipboard)
			{
				RecipeService->CaptureVanillaSampledProductionSettings(Manufacturer);
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		UFGBuildGunStateBuild::OnActorSampled_Implementation,
		GetMutableDefault<UFGBuildGunStateBuild>(),
		[](UFGBuildGunStateBuild* self, AActor* actor)
		{
			if (USFRecipeManagementService* RecipeService = SF_GetLocalRecipeService(self))
			{
				RecipeService->FinishImplicitSettingsSample(actor);
			}
		});

	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("[#489] Build-gun clipboard sampling hooks registered (vanilla-authoritative MMB settings capture)"));
}

void USFGameInstanceModule::RegisterBuildGunScrollSuppressionHook()
{
	// [#162/#429] Scroll_Implementation is a virtual override -> must be SUBSCRIBE_METHOD_VIRTUAL
	// with a CDO sample (plain SUBSCRIBE_METHOD compiles but fatal-asserts at install). The lambda
	// runs BEFORE the original; scope.Cancel() swallows the delta so neither vanilla rotation nor
	// Server_Scroll nor AFGHologram::Scroll (InfiniteNudge's hook point) ever run. When we don't
	// cancel, the original auto-forwards and the wheel is fully vanilla.
	SUBSCRIBE_METHOD_VIRTUAL(UFGBuildGunStateBuild::Scroll_Implementation,
		GetMutableDefault<UFGBuildGunStateBuild>(),
		[](auto& scope, UFGBuildGunStateBuild* self, int32 delta)
		{
			if (!self)
			{
				return;
			}
			AFGBuildGun* Gun = self->GetBuildGun();
			UWorld* World = Gun ? Gun->GetWorld() : nullptr;
			if (!World)
			{
				return;
			}

			// Local player only - never police a remote player's build gun on a listen host.
			// (On a dedicated server ActiveHologram is never set, so the gate below stays false.)
			APlayerController* LocalPC = World->GetFirstPlayerController();
			APawn* LocalPawn = (LocalPC && LocalPC->IsLocalController()) ? LocalPC->GetPawn() : nullptr;
			APawn* InstigatorPawn = Gun->GetInstigator();
			if (LocalPawn && InstigatorPawn && InstigatorPawn != LocalPawn)
			{
				return;
			}

			USFSubsystem* Subsystem = USFSubsystem::Get(World);
			if (Subsystem && Subsystem->ShouldSuppressBuildGunScroll(self->GetHologram()))
			{
				scope.Cancel();
			}
		});

	UE_LOG(LogSmartFoundations, Verbose, TEXT("[#162] UFGBuildGunStateBuild::Scroll_Implementation hook registered (wheel-rotation suppression while Smart! owns the hologram)"));
}

void USFGameInstanceModule::RegisterExtendHologramLockHook()
{
	SUBSCRIBE_METHOD_AFTER(
		UFGBuildGunStateBuild::ToggleHologramPositionLock,
		[](UFGBuildGunStateBuild* self)
		{
			if (!self)
			{
				return;
			}

			AFGBuildGun* Gun = self->GetBuildGun();
			UWorld* World = Gun ? Gun->GetWorld() : nullptr;
			USFSubsystem* Subsystem = World ? USFSubsystem::Get(World) : nullptr;
			USFExtendService* ExtendService = Subsystem ? Subsystem->GetExtendService() : nullptr;
			if (ExtendService)
			{
				ExtendService->HandleHologramLockToggle(self->GetHologram());
			}
		});

	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("[#342] UFGBuildGunStateBuild::ToggleHologramPositionLock hook registered (deterministic Extend pin toggle)"));
}
