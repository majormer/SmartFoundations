// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFGameInstanceModule - client-side multiplayer construct-message hooks (Net seam).
 * Split VERBATIM from SFGameInstanceModule.cpp (Wave 2): the client construct chunk-guard and the
 * client grid-chunk fire handler + their file-local helpers/statics (verified no shared state with
 * the module's other hooks). Same USFGameInstanceModule members, registered identically from
 * StartupModule - no behaviour change.
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

// [MP-SPEC] Multi-step holograms construct only on their FINAL step's input; earlier inputs merely
// advance the step (pole base->height, floodlight angle, sign height). Treating a step-advance as
// a fire captured + DESTROYED the previews mid-placement (live 2026-06-10: a standard conveyor
// pole had no belt previews during height adjust and then fired with an empty plan). The pole gate
// uses the exact-class check (Build_ConveyorPole_C): stackable/wall poles construct in one click
// and are live-validated - they must not be gated. Unknown layouts default to "constructs"
// (the pre-gate behavior, correct for every single-step hologram).
static bool SF_InputWillConstruct(AFGHologram* Holo)
{
	auto FinalStepReached = [&](UClass* Class, uint8 FinalStep) -> bool
	{
		FProperty* StepProp = FindFProperty<FProperty>(Class, TEXT("mBuildStep"));
		if (!StepProp)
		{
			return true;
		}
		uint8 Step = 0;
		StepProp->CopyCompleteValue(&Step, StepProp->ContainerPtrToValuePtr<void>(Holo));
		return Step == FinalStep;
	};

	// #364: the standard pipeline support shares AFGPoleHologram's step machinery with the conveyor pole
	if (USFAutoConnectService::IsRegularConveyorPoleHologram(Holo) || USFAutoConnectService::IsRegularPipelinePoleHologram(Holo))
	{
		return FinalStepReached(AFGPoleHologram::StaticClass(),
			static_cast<uint8>(EPoleHologramBuildStep::PHBS_AdjustHeight));
	}
	if (Holo->IsA<AFGFloodlightHologram>())
	{
		return FinalStepReached(AFGFloodlightHologram::StaticClass(),
			static_cast<uint8>(EFloodlightHologramBuildStep::FHBS_AdjustAngle));
	}
	if (Holo->IsA<AFGStandaloneSignHologram>())
	{
		return FinalStepReached(AFGStandaloneSignHologram::StaticClass(),
			static_cast<uint8>(ESignHologramBuildStep::ESHBS_AdjustHeight));
	}
	return true;
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

// [MP-SPEC] #334 stopgap: Smart auto-connect preview child tags stripped before a client fire
// (defined at file scope - a brace-initializer inside a SUBSCRIBE_METHOD macro argument splits
// the macro args on its commas, same gotcha as multi-capture lambda lists).
static const FName GSFAutoConnectChildTags[] = {
	FName(TEXT("SF_BeltAutoConnectChild")),
	FName(TEXT("SF_PipeAutoConnectChild")),
	FName(TEXT("SF_PowerAutoConnectChild")),
	FName(TEXT("SF_StackableChild")),
};
static constexpr int32 GSFAutoConnectChildTagCount = UE_ARRAY_COUNT(GSFAutoConnectChildTags);

// Alias: a TMap<A,B> inside a SUBSCRIBE macro argument splits the macro args on the template
// comma (the recurring gotcha - see the multi-capture and brace-initializer notes above).
using FSFSpawnedCloneMap = TMap<FString, AFGHologram*>;

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

			// A step-advance input (multi-step holograms) is NOT a fire: nothing serializes, so
			// nothing may be captured, staged, stripped, or destroyed here.
			if (!SF_InputWillConstruct(Holo))
			{
				return;
			}

			// [EXTEND-MP] Extend commit (slice 2): an active Extend session fires with
			// SF_ExtendChild clone children attached. They must NEVER serialize into the construct
			// message (the recipe-less belt/lift clones assert the CLIENT in
			// SerializeConstructMessage - live crash 2026-06-10). Instead, mirror the scaling
			// model: re-emit a FRESH clone topology at the FINAL fire position (the preview's
			// stored topology has stale transforms - children are re-positioned while aiming),
			// stage it via the RCO with the exact preview cost, destroy the preview children, and
			// fire the childless O(1) parent; the server's Construct hook reconstructs the clones.
			{
				static const FName ExtendChildTag(TEXT("SF_ExtendChild"));
				bool bExtendFire = false;
				for (AFGHologram* Child : Holo->GetHologramChildren())
				{
					if (Child && Child->Tags.Contains(ExtendChildTag))
					{
						bExtendFire = true;
						break;
					}
				}

				if (bExtendFire)
				{
					bool bCommitted = false;
					if (SFScalingSpecExpansion::IsSpecConstructionEnabled())
					{
						FSFExtendCommitSpec ExtendSpec;
						USFSubsystem* SS = USFSubsystem::Get(World);
						USFExtendService* Extend = SS ? SS->GetExtendService() : nullptr;
						if (Extend)
						{
							// Fresh clone topology at the FINAL fire position + cost + scaled
							// clone parameters (the same capture the throttled pre-stage uses).
							Extend->BuildCommitSpecForMP(Holo, ExtendSpec);
						}

						if (ExtendSpec.bValid)
						{
							if (APawn* InstigatorPawn = Holo->GetConstructionInstigator())
							{
								if (AFGPlayerController* PC = Cast<AFGPlayerController>(InstigatorPawn->GetController()))
								{
									if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
									{
										RCO->Server_StageExtendCommit(ExtendSpec);
										// Explicit scaling CLEAR: Extend hijacks the grid counters,
										// and a stale scaling spec must never expand a factory grid
										// on top of the Extend commit.
										RCO->Server_StageScalingSpec(FSFScalingSpec());
										bCommitted = true;
									}
								}
							}
						}

						if (bCommitted)
						{
							TArray<AFGHologram*> ToDestroy;
							for (AFGHologram* Child : Holo->mChildren)
							{
								if (Child && Child->Tags.Contains(ExtendChildTag))
								{
									ToDestroy.Add(Child);
								}
							}
							for (AFGHologram* Child : ToDestroy)
							{
								Holo->mChildren.Remove(Child);
								Child->Destroy();
							}
							UE_LOG(LogSmartFoundations, Verbose,
								TEXT("[EXTEND-MP] Client fire: staged Extend commit (offset %s, %d scaled clone(s)), destroyed %d previews; construct message will be O(1)."),
								*ExtendSpec.ParentOffset.ToCompactString(), ExtendSpec.ScaledClones.Num(), ToDestroy.Num());
							return; // fire proceeds with the childless parent
						}
					}

					// Could not stage (spec disabled / no topology / RCO unreachable): refuse the
					// fire - serializing the clone children would assert this client.
					UE_LOG(LogSmartFoundations, Verbose,
						TEXT("[EXTEND-MP] Refused client Extend fire on %s: could not stage the Extend commit."),
						*Holo->GetName());
					if (GEngine)
					{
						GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Orange,
							TEXT("Smart!: Extend commit could not be staged - try again."));
					}
					scope.Cancel();
					return;
				}
			}

			const bool bSpecEnabled = SFScalingSpecExpansion::IsSpecConstructionEnabled();

			// [MP-SPEC] Capture the spec AND the auto-connect belt plan (#334) BEFORE the strip
			// below destroys the preview holograms - the client's fire-time previews are the only
			// complete, real wiring plan that ever exists (server-side re-derivation and aim-time
			// preview reuse both failed live; see PLAN_MP_AutoConnect_334.md). Captured even for a
			// 1x1: a single distributor with belts needs the server Construct seam as much as a
			// grid does.
			FSFScalingSpec Spec; // bValid=false by default = explicit clear
			bool bScalable = false;
			if (bSpecEnabled)
			{
				USFBuildableSizeRegistry::Initialize();
				bScalable = USFBuildableSizeRegistry::GetProfile(Holo->GetBuildClass()).bSupportsScaling;
				if (bScalable)
				{
					SFScalingSpecExpansion::CaptureScalingSpec(Holo, Spec);
					SFScalingSpecExpansion::CaptureConduitPlan(Holo, Spec);

					// [MP-334] Reliable-RPC ceiling guard: a very large belt plan could overflow
					// Server_StageScalingSpec the same way the construct message itself once did
					// (a silently dropped staging RPC would leave the server constructing a bare
					// 1x1). Refuse the fire BEFORE the previews are destroyed - the grid stays
					// live so the player can place in smaller sections. Conservative estimate:
					// ~100B fixed + ~80B per spline point per belt (FVectors are doubles in UE5).
					int32 PlanBytesEstimate = 0;
					for (const FSFConduitPlanEntry& Entry : Spec.ConduitPlan)
					{
						PlanBytesEstimate += 100 + Entry.SplinePoints.Num() * 80;
					}
					if (PlanBytesEstimate > 45000)
					{
						UE_LOG(LogSmartFoundations, Verbose,
							TEXT("[MP-334] Refused client fire: conduit plan too large to stage reliably (%d conduits, ~%d bytes). Build in smaller sections."),
							Spec.ConduitPlan.Num(), PlanBytesEstimate);
						if (GEngine)
						{
							GEngine->AddOnScreenDebugMessage(-1, 6.0f, FColor::Orange,
								FString::Printf(TEXT("Smart!: too many auto-connect belts/pipes/wires for one multiplayer placement (%d). Build in smaller sections."),
									Spec.ConduitPlan.Num()));
						}
						scope.Cancel();
						return;
					}
				}
			}

			// [MP-SPEC] #334: Smart auto-connect preview children (belts / pipes / wires /
			// stackable poles) must NEVER cross the construct message - the server crashes
			// deserializing them (live 2026-06-09: assert in AFGConveyorBeltHologram::
			// UpdateSplineComponent via OnRep_SplineData inside Server_ConstructHologram, empty
			// spline array; a 1x1 merger fire with auto-connect belts killed the dedi). The belts
			// are rebuilt server-side from the staged plan above; pipes/power wiring is still
			// pending (#334 remaining increments). SP/listen-host are unaffected (this hook is
			// NM_Client-only). Applies to ALL client fires when the spec path is enabled, grid or
			// not (the crash case was a 1x1).
			if (bSpecEnabled)
			{
				TArray<AFGHologram*> AutoConnectToStrip;
				for (AFGHologram* Child : Holo->mChildren)
				{
					if (!Child)
					{
						continue;
					}
					for (int32 TagIdx = 0; TagIdx < GSFAutoConnectChildTagCount; ++TagIdx)
					{
						if (Child->Tags.Contains(GSFAutoConnectChildTags[TagIdx]))
						{
							AutoConnectToStrip.Add(Child);
							break;
						}
					}
				}
				if (AutoConnectToStrip.Num() > 0)
				{
					for (AFGHologram* Child : AutoConnectToStrip)
					{
						Holo->mChildren.Remove(Child);
						Child->Destroy();
					}
					UE_LOG(LogSmartFoundations, Verbose,
						TEXT("[MP-SPEC] Stripped %d auto-connect preview children before the fire ")
						TEXT("(server-side wiring pending, #334)."),
						AutoConnectToStrip.Num());
				}
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
			if (bSpecEnabled)
			{
				if (bScalable)
				{
					// Spec (+ belt plan) was captured ABOVE, before the strip destroyed the
					// previews. Even a 1x1 spec is staged: it routes the construct through the
					// server-side Construct hook, where the staged belt plan is replayed with
					// authority (#334) - a single distributor with belts needs that as much as a
					// grid does.

					// Stage (or clear) the spec server-side BEFORE the construct RPC goes out.
					// Also stage an Extend-commit CLEAR: a pre-staged commit from an earlier
					// (cancelled) Extend session must never be consumed by this plain fire.
					bool bStaged = false;
					if (APawn* InstigatorPawn = Holo->GetConstructionInstigator())
					{
						if (AFGPlayerController* PC = Cast<AFGPlayerController>(InstigatorPawn->GetController()))
						{
							if (USFRCO* RCO = PC->GetRemoteCallObjectOfClass<USFRCO>())
							{
								RCO->Server_StageScalingSpec(Spec);
								RCO->Server_StageExtendCommit(FSFExtendCommitSpec());
								bStaged = true;
							}
						}
					}

					if (Spec.bValid && bStaged)
					{
						UE_LOG(LogSmartFoundations, Verbose,
							TEXT("[MP-SPEC] Client fire: staged spec (%d cells of %s, %d planned conduit(s)), destroying %d preview children; construct message will be O(1)."),
							Spec.CellCount(), *GetNameSafe(*Spec.BuildClass), Spec.ConduitPlan.Num(), GridChildCount);

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
