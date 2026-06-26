// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFHypertube::BuildOrUpdateSpan — hypertube auto-connect span construction (S2b PREVIEW, #405).
 *
 * The span between two adjacent stackable hypertube supports is an ASFPipelineHologram routed by the
 * GAME router (ApplyPipeBuildModeRouting), mirroring the Smart Walking pipe adapter
 * (USFWalkPipeConveyance::LinkOrUpdate). The SPAWN / tag / child / snapped-connection plumbing mirrors the
 * stackable-pipe auto-connect create path (USFAutoConnectService::UpdateOrCreatePipeForPolePair) — but NOT
 * its hand-rolled 6-point spline, which we deliberately replace with the game router.
 *
 * Hypertube connector = UFGPipeConnectionComponentHyper, a subclass of UFGPipeConnectionComponentBase, so
 * GetComponents<UFGPipeConnectionComponentBase>() catches it with no retype (same as the walk pipe adapter).
 */

#include "Features/HypertubeAutoConnect/SFHypertubeSpanBuilder.h"

#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"                 // DisableValidation / MarkAsChild / GetOrCreateData
#include "Features/AutoConnect/SFAutoConnectService.h"       // MAX_HYPERTUBE_LENGTH + ResolveSupportExitNormal
#include "Data/SFHologramData.h"                             // FSFHologramData + ESFChildHologramType
#include "Holograms/Logistics/SFPipelineHologram.h"          // ASFPipelineHologram (+ AFGPipelineHologram base)

#include "Hologram/FGHologram.h"
#include "Hologram/FGPipelineHologram.h"                     // AFGPipelineHologram::mSnappedConnectionComponents (reflection target)
#include "FGPipeConnectionComponent.h"                       // UFGPipeConnectionComponentBase (base catches the Hyper subclass); same header the AC/walk paths use
#include "FGPlayerController.h"
#include "FGRecipe.h"                                        // #405: UFGRecipe for SetRecipe (commit cost aggregation for the AddChild'd span)

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"                              // FProperty reflection for mSnappedConnectionComponents
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogSmartHyperSpan, Log, All);

namespace
{
	/**
	 * First pipe connector on a support hologram, or null. The hypertube support's connector is a
	 * UFGPipeConnectionComponentHyper; querying the BASE type catches it without a retype (identical to
	 * USFWalkPipeConveyance::FirstPipeConnector).
	 */
	UFGPipeConnectionComponentBase* FirstPipeConnector(AFGHologram* Support)
	{
		if (!IsValid(Support))
		{
			return nullptr;
		}
		TArray<UFGPipeConnectionComponentBase*> Connectors;
		Support->GetComponents<UFGPipeConnectionComponentBase>(Connectors);
		return Connectors.Num() > 0 ? Connectors[0] : nullptr;
	}
}

namespace SFHypertube
{
	AFGHologram* BuildOrUpdateSpan(
		USFSubsystem* Sub,
		AFGHologram* FromSupport,
		AFGHologram* ToSupport,
		AFGHologram* ExistingSpan,
		AFGHologram* ParentForChild)
	{
		if (!Sub || !IsValid(FromSupport) || !IsValid(ToSupport))
		{
			return ExistingSpan;
		}

		// Extract the pipe connector from each support (base type catches UFGPipeConnectionComponentHyper).
		UFGPipeConnectionComponentBase* FromConn = FirstPipeConnector(FromSupport);
		UFGPipeConnectionComponentBase* ToConn   = FirstPipeConnector(ToSupport);

		// Endpoints from the support's LIVE actor location + the connector's relative offset rotated by the support's
		// actor rotation — NOT GetComponentLocation(), which returns STALE component transforms while the grid supports
		// are LOCKED during incremental scaling (the multi-pole "spans bunched at the root" bug). Mirrors the working
		// stackable-pipe AC endpoint derivation (UpdateOrCreatePipeForPolePair). #405.
		const FVector StartPos = FromConn
			? FromSupport->GetActorLocation() + FromSupport->GetActorRotation().RotateVector(FromConn->GetRelativeLocation())
			: FromSupport->GetActorLocation();
		const FVector EndPos = ToConn
			? ToSupport->GetActorLocation() + ToSupport->GetActorRotation().RotateVector(ToConn->GetRelativeLocation())
			: ToSupport->GetActorLocation();

		// Length cap (#405): refuse an over-long pair — return null, no span. This is the SKIP policy of the
		// existing stackable auto-connect, NOT the walk's red+reason (S3). Caller drops any stale too-long span.
		const float ChordLen = FVector::Dist(StartPos, EndPos);
		if (ChordLen > USFAutoConnectService::MAX_HYPERTUBE_LENGTH)
		{
			UE_LOG(LogSmartHyperSpan, Verbose, TEXT("BuildOrUpdateSpan: span %.0f cm > %.0f cm cap — skipped (pair too long)"),
				ChordLen, USFAutoConnectService::MAX_HYPERTUBE_LENGTH);
			return nullptr;
		}

		// Exit normals via the SHARED resolver — the exact path the stackable-pipe AC uses (#400/#291):
		// each support's horizontal facing is oriented TOWARD the run, the chord's pitch is kept, and it falls back to
		// the straight chord when the facing is perpendicular or the run is vertical. Hypertube supports are non-wall
		// (forward facing, not RightVector). Replaces the old hard-coded -(facing) through-route, which on these
		// supports aimed the exit AWAY from the destination and ballooned the span into a vertical loop. #405.
		const FVector Direction = (EndPos - StartPos);
		const FVector ToTarget  = Direction.GetSafeNormal();
		const FVector ToSource  = (-Direction).GetSafeNormal();

		FVector StartNormal = USFAutoConnectService::ResolveSupportExitNormal(FromSupport, ToTarget, false);
		FVector EndNormal   = USFAutoConnectService::ResolveSupportExitNormal(ToSupport, ToSource, false);

		// FLATTEN the exit tangents: a hypertube must leave its connector HORIZONTALLY and let the router climb to the
		// next pole's height (the Horiz→Vert shape). ResolveSupportExitNormal bakes in the chord's pitch for pipes
		// (their #291 no-ramp rule) — for tubes that pitch is exactly the diagonal "straight at the next connector"
		// exit we don't want, so drop the Z and keep the horizontal heading toward the run. #405.
		StartNormal.Z = 0.0f; StartNormal = StartNormal.GetSafeNormal();
		EndNormal.Z   = 0.0f; EndNormal   = EndNormal.GetSafeNormal();

		if (StartNormal.IsNearlyZero()) { StartNormal = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal(); }
		if (EndNormal.IsNearlyZero())   { EndNormal   = FVector(ToSource.X, ToSource.Y, 0.0f).GetSafeNormal(); }

		const int32 RoutingMode = Sub->GetAutoConnectRuntimeSettings().HypertubeRoutingMode;

		UE_LOG(LogSmartHyperSpan, Verbose, TEXT("BuildOrUpdateSpan: Start=%s End=%s | StartN=%s EndN=%s | mode=%d len=%.1f existing=%s"),
			*StartPos.ToString(), *EndPos.ToString(), *StartNormal.ToString(), *EndNormal.ToString(),
			RoutingMode, ChordLen, *GetNameSafe(ExistingSpan));

		// UPDATE path: re-route an existing span to follow the (re-scaled) supports.
		if (ASFPipelineHologram* Existing = Cast<ASFPipelineHologram>(ExistingSpan))
		{
			// GRID SCALING PATTERN (mirrors the stackable-pipe UPDATE in UpdateOrCreatePipeForPolePair, #405):
			// a LOCKED hologram pins its actor, so SetActorLocation is a no-op while locked — the span freezes at
			// its CREATE origin and the game router then routes relative to that stale origin (multi-pole spans pile
			// up at the root; a nudge doesn't move them). Temporarily unlock, move + re-route, then restore the lock.
			const bool bParentLocked = IsValid(ParentForChild) && ParentForChild->IsHologramLocked();
			const bool bSpanWasLocked = Existing->IsHologramLocked();
			if (bParentLocked && bSpanWasLocked)
			{
				Existing->LockHologramPosition(false);
			}

			Existing->SetActorLocation(StartPos);
			Existing->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);
			Existing->TriggerMeshGeneration();
			Existing->ForceApplyHologramMaterial();

			if (bParentLocked)
			{
				Existing->LockHologramPosition(true);
			}
			UE_LOG(LogSmartHyperSpan, Verbose, TEXT("BuildOrUpdateSpan: UPDATE span=%s"), *GetNameSafe(Existing));
			return Existing;
		}

		// CREATE path: resolve the single, unlock-gated hypertube build class (Build_PipeHyper_C).
		UWorld* World = ToSupport->GetWorld();
		if (!World)
		{
			return nullptr;
		}
		AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());

		UClass* HyperBuildClass = Sub->GetHypertubeClassFromConfig(PC);
		if (!HyperBuildClass)
		{
			UE_LOG(LogSmartHyperSpan, Verbose, TEXT("BuildOrUpdateSpan: no hypertube build class (not unlocked?) — skipped"));
			return nullptr;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = ToSupport->GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;

		ASFPipelineHologram* Span = World->SpawnActor<ASFPipelineHologram>(
			ASFPipelineHologram::StaticClass(), StartPos, FRotator::ZeroRotator, SpawnParams);
		if (!Span)
		{
			return nullptr;
		}

		// MP: preview holograms never replicate; the S3 commit re-tags + AddChild's server-side (walk pattern).
		Span->SetReplicates(false);
		Span->SetReplicateMovement(false);
		Span->SetBuildClass(HyperBuildClass);
		// MESH comes from the build-class CDO (ASFPipelineHologram::TriggerMeshGeneration base-casts to
		// AFGBuildablePipeBase, #405). RECIPE is set so vanilla aggregates this child's COST on commit — the span is
		// AddChild'd below (pipe-parity), so it builds on the fire; the stackable-pipe AC sets its recipe for the same reason.
		if (UClass* HyperRecipeClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeHyper.Recipe_PipeHyper_C")))
		{
			Span->SetRecipe(TSubclassOf<UFGRecipe>(HyperRecipeClass));
		}
		Span->Tags.AddUnique(FName(TEXT("SF_StackableChild")));

		USFHologramDataService::DisableValidation(Span);
		// Reuse the WalkSegment child type — the span IS a routed pole-conveyance segment, same lifecycle as the
		// walk pipe child. See OPEN QUESTION in the handoff: add a dedicated ESFChildHologramType::HypertubeSegment
		// only if S3 commit needs to distinguish hypertube children from walk children at the construct seam.
		USFHologramDataService::MarkAsChild(Span, ToSupport, ESFChildHologramType::WalkSegment);

		if (FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(Span))
		{
			// Reuse the stackable-pipe connector fields — the hypertube connector IS a UFGPipeConnectionComponentBase,
			// so StackablePipeConn0/1 hold it with no new field. See OPEN QUESTION in the handoff.
			HoloData->bIsStackablePipe   = true;
			HoloData->StackablePipeConn0 = FromConn;
			HoloData->StackablePipeConn1 = ToConn;
			HoloData->StackablePipeIndex = 0;
		}

		Span->FinishSpawning(FTransform(StartPos));

		// Pre-wire the snapped connections by reflection. ASFPipelineHologram exposes SetSnappedConnections(), but
		// the stackable-pipe AC create path sets the vanilla mSnappedConnectionComponents[0/1] directly by reflection
		// (and the walk pipe adapter mirrors it) — we follow that proven pattern so the route honours both ends.
		if (FromConn || ToConn)
		{
			if (FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents")))
			{
				if (void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(Span))
				{
					UFGPipeConnectionComponentBase** SnappedArray = static_cast<UFGPipeConnectionComponentBase**>(PropAddr);
					if (SnappedArray)
					{
						SnappedArray[0] = FromConn;
						SnappedArray[1] = ToConn;
					}
				}
			}
		}

		// Route via the GAME router (locked decision) — NOT a hand-rolled spline.
		Span->ApplyPipeBuildModeRouting(RoutingMode, StartPos, StartNormal, EndPos, EndNormal);

		Span->SetActorHiddenInGame(false);
		Span->SetActorEnableCollision(false);
		Span->SetActorTickEnabled(false);
		Span->RegisterAllComponents();
		Span->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

		// Parent the span so vanilla mChildren owns its lifecycle (cascade-destroys on cancel/holster) AND the
		// vanilla construct cascade builds it on commit — exactly like the stackable-pipe AC create path. #405.
		static int32 SpanCounter = 0;
		const FName ChildName(*FString::Printf(TEXT("StackableHypertube_%d"), SpanCounter++));
		if (IsValid(ParentForChild))
		{
			ParentForChild->AddChild(Span, ChildName);
		}
		else
		{
			// Parent vanished (e.g. a post-cancel debounce tick firing after the parent began teardown) — never
			// leave an un-parented orphan; destroy the freshly-spawned span and report no span for this pair. #405.
			Span->Destroy();
			return nullptr;
		}

		Span->TriggerMeshGeneration();
		Span->ForceApplyHologramMaterial();

		UE_LOG(LogSmartHyperSpan, Verbose, TEXT("BuildOrUpdateSpan: CREATE span=%s actor.world=%s | class=%s"),
			*GetNameSafe(Span), *Span->GetActorLocation().ToString(), *GetNameSafe(HyperBuildClass));
		return Span;
	}
}
