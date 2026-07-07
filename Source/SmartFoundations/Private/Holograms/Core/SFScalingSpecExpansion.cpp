// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "SmartFoundations.h"
#include "Hologram/FGHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Subsystem/SFHologramDataService.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Holograms/Power/SFWireHologram.h"
#include "FGPowerConnectionComponent.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableWire.h"
#include "Shared/Power/SFWireDesignerRegistration.h"  // [#421] designer containment for direct-spawned wires
#include "FGBlueprintProxy.h"
#include "Hologram/FGBlueprintHologram.h"       // [#168] Smart! Blueprints: staged blueprint grid cells
#include "Features/Scaling/SFGridCoordComponent.h"  // [#168-MP] cell-basis capture from live preview children
#include "Hologram/FGPoleHologram.h"            // #354: mPoleVariationIndex / mBuildStep
#include "Hologram/FGConveyorPoleHologram.h"
#include "Hologram/FGFloodlightHologram.h"      // #200: mFixtureAngle / mBuildStep
#include "Hologram/FGStandaloneSignHologram.h"  // #192: mBuildStep
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"   // [#418-MP] deferred time-sliced spec expansion
#include "HAL/IConsoleManager.h"
#include "FGDismantleInterface.h"
#include "Hologram/FGPipelinePoleHologram.h"
#include "Hologram/FGWaterPumpHologram.h"                  // #428: MP water-extractor crash fix
#include "Holograms/Logistics/SFWaterPumpChildHologram.h"  // #428
#include "Hologram/FGPassthroughHologram.h"                  // #458: MP floor-hole thickness/snap parity
#include "Holograms/Logistics/SFPassthroughChildHologram.h"  // #458
#include "UObject/UnrealType.h"                              // #458: FFloatProperty reflection

// MP spec-based construction. ON by default - the mod must be self-contained (no launch options /
// ini edits for players; Saved/Engine.ini is rewritten by the game's diff-config system anyway).
// The CVar exists ONLY as a developer escape hatch: in a dev/debug session it can be set to 0 to
// fall back to the legacy serialize-children path + oversized guard while isolating a regression.
// Players never touch it. (This branch does not ship until the complete MP solution is validated.)
static TAutoConsoleVariable<int32> CVarSFMPSpecConstruction(
	TEXT("sf.MP.SpecConstruction"),
	1,
	TEXT("Smart!: when 1 (default), scaling grids commit via a compact server-expanded spec (MP) ")
	TEXT("instead of serializing N child holograms. Set 0 to fall back to the legacy path + ")
	TEXT("oversized guard (developer debugging only)."),
	ECVF_Default);

namespace SFScalingSpecExpansion
{

bool AreAllWaterCellsValid(AFGHologram* Parent)
{
	// #428: water extractors get ASFWaterPumpChildHologram preview cells, each running its own water
	// validation. The MP spec path destroys these previews and fires a childless parent, so the
	// per-cell disqualifier never blocks the build; the fire hook calls this to refuse a fire whose
	// grid cells fall on land (matching SP). Non-water-pumps are not gated.
	if (!Parent || !Parent->IsA(AFGWaterPumpHologram::StaticClass()))
	{
		return true;
	}
	for (AFGHologram* Child : Parent->GetHologramChildren())
	{
		const ASFWaterPumpChildHologram* WaterChild = Cast<ASFWaterPumpChildHologram>(Child);
		if (WaterChild && !WaterChild->ValidateWaterPosition())
		{
			return false; // a grid cell is not over valid water
		}
	}
	return true;
}

bool IsSpecConstructionEnabled()
{
	const bool bEnabled = CVarSFMPSpecConstruction.GetValueOnAnyThread() != 0;

	// One-time visibility: make the gate state unambiguous in every session log.
	static bool bLoggedOnce = false;
	if (!bLoggedOnce)
	{
		bLoggedOnce = true;
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[MP-SPEC] Spec-based scaling construction is %s (sf.MP.SpecConstruction=%d)."),
			bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"),
			CVarSFMPSpecConstruction.GetValueOnAnyThread());
	}

	return bEnabled;
}

bool CaptureScalingSpec(AFGHologram* Hologram, FSFScalingSpec& OutSpec)
{
	if (!Hologram)
	{
		return false;
	}

	USFSubsystem* SS = USFSubsystem::Get(Hologram->GetWorld());
	if (!SS)
	{
		return false;
	}

	// NOTE: trivial 1x1x1 grids are captured too (since the #334 increment): the server-side
	// Construct hook is also the seam where auto-connect wiring is re-derived with authority, and
	// a SINGLE distributor with auto-connect belts needs that path as much as a grid does. The
	// expansion loop simply spawns zero children for a 1-cell spec, and the cost scale is x1.
	const FSFCounterState Counters = SS->GetCounterState();

	USFBuildableSizeRegistry::Initialize();
	const FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass());

	OutSpec.Counters = Counters;
	OutSpec.ItemSize = Profile.DefaultSize;
	OutSpec.AnchorOffset = Profile.AnchorOffset;
	// [#168-MP] Blueprint composites have NO registry profile - the fallback 8x8x4m would hand the
	// server a wrong grid pitch entirely. The subsystem's cached size IS what positioned the client
	// preview (the blueprint adapter's mLocalBounds footprint, cached at registration); carry it,
	// plus the measured clone content-convention delta the preview corrected every child by - the
	// captured conduit plan below is only valid against copies at those exact positions.
	if (Hologram->IsA<AFGBlueprintHologram>())
	{
		OutSpec.ItemSize = SS->GetCachedBuildingSize();
		OutSpec.AnchorOffset = FVector::ZeroVector;
		OutSpec.BlueprintContentDelta = SS->GetBlueprintChildContentDelta();

		// [#168-MP] Measure the preview grid's ACTUAL cell basis from the live children - the
		// server-side calculator provably disagrees with the client preview pitch for composites
		// (live 2026-07-07: one spacing unit of drift per grid step), and the conduit plan is only
		// valid at the preview's exact positions. Basis = (unit-cell child pos - parent pos) with
		// the per-child content delta removed, so the server can reconstruct cell (i,j,k) affinely.
		// Steps tilt a basis vector and are captured for free; STAGGER is parity-based (not affine)
		// - blueprints don't support it, but guard anyway by refusing the basis when set.
		const FRotator ParentRotForBasis = Hologram->GetActorRotation();
		const FVector RotatedDelta = ParentRotForBasis.RotateVector(OutSpec.BlueprintContentDelta);
		const FVector ParentLocForBasis = Hologram->GetActorLocation();
		const bool bStaggerActive = Counters.StaggerX != 0 || Counters.StaggerY != 0
			|| Counters.StaggerZX != 0 || Counters.StaggerZY != 0;
		// Rotation mode curves cell positions per index (arc) - also not affine. Both fall back
		// to the calculator path (positions may drift there; those modes are not in the blueprint
		// adapter's supported feature set).
		const bool bRotationActive = !FMath::IsNearlyZero(Counters.RotationZ);
		if (!bStaggerActive && !bRotationActive)
		{
			int32 AxesFound = 0;
			for (AFGHologram* Child : Hologram->GetHologramChildren())
			{
				FIntVector Cell;
				if (!Child || !Child->Tags.Contains(FName(TEXT("SF_GridChild")))
					|| !USFGridCoordComponent::TryGetCell(Child, Cell))
				{
					continue;
				}
				const FVector Basis = Child->GetActorLocation() - ParentLocForBasis - RotatedDelta;
				if (Cell == FIntVector(1, 0, 0)) { OutSpec.CellBasisX = Basis; ++AxesFound; }
				else if (Cell == FIntVector(0, 1, 0)) { OutSpec.CellBasisY = Basis; ++AxesFound; }
				else if (Cell == FIntVector(0, 0, 1)) { OutSpec.CellBasisZ = Basis; ++AxesFound; }
			}
			OutSpec.bHasCellBasis = AxesFound > 0;
			if (OutSpec.bHasCellBasis)
			{
				UE_LOG(LogSmartFoundations, Log,
					TEXT("[#168-MP] Captured blueprint cell basis: X=%s Y=%s Z=%s delta=%s (calculator would have used pitch from ItemSize=%s)"),
					*OutSpec.CellBasisX.ToCompactString(), *OutSpec.CellBasisY.ToCompactString(),
					*OutSpec.CellBasisZ.ToCompactString(), *OutSpec.BlueprintContentDelta.ToCompactString(),
					*OutSpec.ItemSize.ToCompactString());
			}
		}
	}
	OutSpec.BuildClass = Hologram->GetBuildClass();
	// [#368] Carry the player's remembered production recipe so the SERVER applies it to the
	// authoritative manufacturer build (recipe memory is client-side only; this is the sole crossing
	// for a fresh manual placement in MP). Null when nothing is remembered -> server applies nothing.
	// Non-manufacturer placements just carry whatever is remembered; the server-side apply ignores it
	// via its IsProductionBuilding gate, and the install is restored after the build.
	OutSpec.ProductionRecipe = SS->GetActiveRecipe();
	OutSpec.bValid = true;
	return true;
}

// Multi-step buildables carry the player's step-2 choice in PRIVATE hologram members which the
// CLIENT preview syncs parent->child every tick via reflection (#354 standard conveyor pole
// height, #200 floodlight fixture angle, #192 sign build step - see USFSubsystem::
// SyncMultiStepHologramProperties). Spec-expanded server children spawn with DEFAULTS, so without
// this they build at e.g. floor height while the captured belts route to top connectors (live
// finding 2026-06-10, standard poles). The parent's own values crossed the wire inside the
// construct message - copy the same properties to each child and fire the same OnRep refresh, so
// the child's connectors/mesh state match before ConfigureActor snapshots them into the buildable.
static void SF_SyncMultiStepPropertiesToChild(AFGHologram* Parent, AFGHologram* Child)
{
	// #354: STANDARD conveyor pole only - stackable/wall poles are also AFGConveyorPoleHologram
	// and must not be touched (same gate as the client sync).
	if (USFAutoConnectService::IsRegularConveyorPoleHologram(Parent) && Child->IsA<AFGConveyorPoleHologram>())
	{
		if (FIntProperty* VarProp = FindFProperty<FIntProperty>(AFGPoleHologram::StaticClass(), TEXT("mPoleVariationIndex")))
		{
			VarProp->SetPropertyValue_InContainer(Child, VarProp->GetPropertyValue_InContainer(Parent));
		}
		if (FProperty* StepProp = FindFProperty<FProperty>(AFGPoleHologram::StaticClass(), TEXT("mBuildStep")))
		{
			StepProp->CopyCompleteValue(
				StepProp->ContainerPtrToValuePtr<void>(Child),
				StepProp->ContainerPtrToValuePtr<void>(Parent));
		}
		if (UFunction* RepFunc = Child->FindFunction(TEXT("OnRep_PoleVariationIndex")))
		{
			Child->ProcessEvent(RepFunc, nullptr);
		}
		return;
	}

	// #364: STANDARD pipeline support - height (inherited pole machinery) + vertical angle
	// (public API). Same gate discipline as #354: stackable/wall supports untouched.
	if (USFAutoConnectService::IsRegularPipelinePoleHologram(Parent) && Child->IsA<AFGPipelinePoleHologram>())
	{
		if (FIntProperty* VarProp = FindFProperty<FIntProperty>(AFGPoleHologram::StaticClass(), TEXT("mPoleVariationIndex")))
		{
			VarProp->SetPropertyValue_InContainer(Child, VarProp->GetPropertyValue_InContainer(Parent));
		}
		if (FProperty* StepProp = FindFProperty<FProperty>(AFGPoleHologram::StaticClass(), TEXT("mBuildStep")))
		{
			StepProp->CopyCompleteValue(
				StepProp->ContainerPtrToValuePtr<void>(Child),
				StepProp->ContainerPtrToValuePtr<void>(Parent));
		}
		AFGPipelinePoleHologram* PipeParent = CastChecked<AFGPipelinePoleHologram>(Parent);
		AFGPipelinePoleHologram* PipeChild = CastChecked<AFGPipelinePoleHologram>(Child);
		PipeChild->SetVerticalAngle(PipeParent->GetVerticalAngle());
		if (UFunction* RepFunc = Child->FindFunction(TEXT("OnRep_PoleVariationIndex")))
		{
			Child->ProcessEvent(RepFunc, nullptr);
		}
		if (UFunction* AngleRep = Child->FindFunction(TEXT("OnRep_VerticalAngle")))
		{
			Child->ProcessEvent(AngleRep, nullptr);
		}
		return;
	}

	// #200: floodlight fixture angle.
	if (Parent->IsA<AFGFloodlightHologram>() && Child->IsA<AFGFloodlightHologram>())
	{
		if (FIntProperty* AngleProp = FindFProperty<FIntProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mFixtureAngle")))
		{
			AngleProp->SetPropertyValue_InContainer(Child, AngleProp->GetPropertyValue_InContainer(Parent));
		}
		if (FProperty* StepProp = FindFProperty<FProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mBuildStep")))
		{
			StepProp->CopyCompleteValue(
				StepProp->ContainerPtrToValuePtr<void>(Child),
				StepProp->ContainerPtrToValuePtr<void>(Parent));
		}
		if (UFunction* RepFunc = Child->FindFunction(TEXT("OnRep_FixtureAngle")))
		{
			Child->ProcessEvent(RepFunc, nullptr);
		}
		return;
	}

	// #192: standalone sign build step.
	if (Parent->IsA<AFGStandaloneSignHologram>() && Child->IsA<AFGStandaloneSignHologram>())
	{
		if (FProperty* StepProp = FindFProperty<FProperty>(AFGStandaloneSignHologram::StaticClass(), TEXT("mBuildStep")))
		{
			StepProp->CopyCompleteValue(
				StepProp->ContainerPtrToValuePtr<void>(Child),
				StepProp->ContainerPtrToValuePtr<void>(Parent));
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// [#418-MP] Deferred, time-sliced spec expansion.
//
// A large staged grid expanded + constructed INLINE at the Construct seam blocks the server game
// thread for the whole build. Live incident 2026-07-03: ~45K foundation cells = one 56.9-second
// server frame -> the net driver never ticked -> every client hit the 30s connection timeout and
// was dropped (the build itself completed and persisted - correctness was never the problem,
// only duration). Past the cell threshold the seam captures everything BY VALUE (the parent
// hologram dies when its Construct returns) and a world-timer loop constructs cells under a
// per-frame time budget, so the server keeps ticking and connections stay alive while the grid
// fills in over a few dozen seconds.
//
// KNOWN WINDOW: a save or server shutdown mid-job persists only the cells built so far; the
// remainder of the job is lost (the player paid at fire time). Accepted for now - the inline
// path's alternative was a guaranteed all-client disconnect.
namespace
{
	struct FSFDeferredSpecJob
	{
		TWeakObjectPtr<UWorld> World;
		FSFScalingSpec Spec;
		TSubclassOf<UFGRecipe> Recipe;
		FNetConstructionID ConstructionID;
		FVector ParentLoc = FVector::ZeroVector;
		FRotator ParentRot = FRotator::ZeroRotator;
		bool bWaterPumpCells = false;
		TWeakObjectPtr<AActor> HoloOwner;
		TWeakObjectPtr<APawn> Instigator;
		/** Carries the parent's multi-step choices (pole height / floodlight angle / sign step)
		 *  across frames - the parent hologram itself is gone. Hidden, never constructed. */
		TWeakObjectPtr<AFGHologram> TemplateHologram;

		// Grid geometry + resume cursor (XI innermost - same order as the inline loop).
		int32 NX = 1, NY = 1, NZ = 1, SgnX = 1, SgnY = 1, SgnZ = 1;
		int32 XI = 0, YI = 0, ZI = 0;
		int32 LinearIndex = 0;
		int32 CellsBuilt = 0;
		int32 CellsFailed = 0;
		int32 TotalCells = 0;
		int32 LastLoggedDecile = 0;
	};

	/** Inline expansion past this many CHILD cells starves the net driver; defer instead. */
	constexpr int32 SFSpecDeferThresholdCells = 1000;

	/** Per-frame wall-clock budget for deferred cell construction. 15ms leaves the rest of a
	 *  30fps server frame for gameplay + net; a 45K-cell grid drains in roughly a minute
	 *  WITHOUT ever starving the net driver. */
	constexpr double SFSpecDeferBudgetMs = 15.0;

	/** Advance the ZI/YI/XI cursor one cell (XI innermost). Returns false when past the end. */
	bool AdvanceSpecCursor(FSFDeferredSpecJob& Job)
	{
		++Job.XI;
		if (Job.XI >= Job.NX) { Job.XI = 0; ++Job.YI; }
		if (Job.YI >= Job.NY) { Job.YI = 0; ++Job.ZI; }
		return Job.ZI < Job.NZ;
	}

	/** Build the cell at the current cursor - mirrors the inline loop body of
	 *  ExpandScalingSpecIntoChildren 1:1, except the cell hologram is standalone (no parent to
	 *  attach to) and is Construct()ed + destroyed here instead of by the vanilla child loop. */
	bool BuildOneDeferredCell(FSFDeferredSpecJob& Job, UWorld* World)
	{
		const FSFCounterState& C = Job.Spec.Counters;
		const int32 GX = Job.XI * Job.SgnX;
		const int32 GY = Job.YI * Job.SgnY;
		const int32 GZ = Job.ZI * Job.SgnZ;

		// AnchorOffset deliberately NOT passed (ZeroVector) - same reasoning as the inline path:
		// direct actor transforms expect the FINAL world position.
		FSFPositionCalculator Calc;
		const FVector CellLoc = Calc.CalculateChildPosition(
			GX, GY, GZ, Job.ParentLoc, Job.ParentRot,
			Job.Spec.ItemSize, C, Job.LinearIndex, FVector::ZeroVector);
		++Job.LinearIndex;

		// [#363]/[#372] rotation-mode yaw - identical to the inline path.
		FRotator CellRot = Job.ParentRot;
		if (!FMath::IsNearlyZero(C.RotationZ))
		{
			const int32 RotProgress = (C.RotationAxis == ESFScaleAxis::Y) ? -GY : GX;
			CellRot.Yaw += RotProgress * C.RotationZ;
		}

		AFGHologram* Cell = nullptr;
		if (Job.bWaterPumpCells)
		{
			// #428: water extractors must use the Smart child class (vanilla pump hologram
			// asserts on null mSnappedExtractableResource in ConfigureActor). No explicit
			// SpawnParams.Name (#428: deterministic names FATAL on a repeat build attempt).
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = Job.HoloOwner.Get();
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnParams.bDeferConstruction = true;

			ASFWaterPumpChildHologram* Pump = World->SpawnActor<ASFWaterPumpChildHologram>(
				ASFWaterPumpChildHologram::StaticClass(), CellLoc, CellRot, SpawnParams);
			if (Pump)
			{
				Pump->SetChildBuildClass(Job.Spec.BuildClass);
				Pump->SetRecipe(Job.Recipe);
				Pump->SetConstructionInstigator(Job.Instigator.Get());
				Pump->FinishSpawning(FTransform(CellRot, CellLoc));
				Pump->Tags.AddUnique(FName(TEXT("SF_GridChild")));
			}
			Cell = Pump;
		}
		else
		{
			const FRotator RotForSpawn = CellRot;
			Cell = AFGHologram::SpawnHologramFromRecipe(
				Job.Recipe, Job.HoloOwner.Get(), CellLoc, Job.Instigator.Get(),
				[RotForSpawn](AFGHologram* NewCell)
				{
					if (NewCell)
					{
						NewCell->SetActorRotation(RotForSpawn);
						NewCell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
					}
				});
		}

		if (!Cell)
		{
			return false;
		}

		// Multi-step parents: copy the player's step-2 choice from the template (which holds the
		// dead parent's values) so the cell builds with it - same as the inline path's
		// parent-to-child sync.
		if (AFGHologram* Template = Job.TemplateHologram.Get())
		{
			SF_SyncMultiStepPropertiesToChild(Template, Cell);
		}

		// Exact grid transform; server validation passed at fire time (same as inline).
		Cell->SetActorLocationAndRotation(CellLoc, CellRot);

		TArray<AActor*> CellChildren;
		AActor* Built = Cell->Construct(CellChildren, Job.ConstructionID);
		Cell->Destroy();
		return Built != nullptr;
	}

	void TickDeferredSpecJob(TSharedPtr<FSFDeferredSpecJob> Job)
	{
		UWorld* World = Job->World.Get();
		if (!World)
		{
			// World teardown mid-job: drop (see KNOWN WINDOW above).
			return;
		}

		const double SliceStart = FPlatformTime::Seconds();
		const double Budget = SFSpecDeferBudgetMs / 1000.0;
		bool bMore = true;
		int32 CellsThisSlice = 0;
		while (bMore)
		{
			// (0,0,0) is the parent buildable itself (built at the seam), never a cell.
			if (!(Job->XI == 0 && Job->YI == 0 && Job->ZI == 0))
			{
				if (BuildOneDeferredCell(*Job, World))
				{
					++Job->CellsBuilt;
				}
				else
				{
					++Job->CellsFailed;
				}
				++CellsThisSlice;
			}
			bMore = AdvanceSpecCursor(*Job);

			if (bMore && CellsThisSlice > 0 && (CellsThisSlice & 7) == 0
				&& (FPlatformTime::Seconds() - SliceStart) >= Budget)
			{
				break;
			}
		}

		const int32 Done = Job->CellsBuilt + Job->CellsFailed;
		const int32 Decile = (Job->TotalCells > 0) ? (10 * Done) / Job->TotalCells : 10;
		if (Decile > Job->LastLoggedDecile)
		{
			Job->LastLoggedDecile = Decile;
			UE_LOG(LogSmartFoundations, Display,
				TEXT("[MP-SPEC] Deferred expansion progress: %d/%d cells (%d failed)."),
				Done, Job->TotalCells, Job->CellsFailed);
		}

		if (bMore)
		{
			World->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateLambda([Job]() { TickDeferredSpecJob(Job); }));
		}
		else
		{
			if (AFGHologram* Template = Job->TemplateHologram.Get())
			{
				Template->Destroy();
			}
			UE_LOG(LogSmartFoundations, Display,
				TEXT("[MP-SPEC] Deferred expansion COMPLETE: %d/%d cells built (%d failed)."),
				Job->CellsBuilt, Job->TotalCells, Job->CellsFailed);
		}
	}
}

bool ShouldDeferSpecExpansion(AFGHologram* Parent, const FSFScalingSpec& Spec)
{
	return Parent
		&& Spec.bValid
		&& Spec.ConduitPlan.Num() == 0
		&& Parent->GetBlueprintDesigner() == nullptr
		&& (Spec.CellCount() - 1) > SFSpecDeferThresholdCells;
}

void BeginDeferredSpecExpansion(AFGHologram* Parent, const FSFScalingSpec& Spec,
	TSubclassOf<UFGRecipe> Recipe, const FNetConstructionID& ConstructionID)
{
	UWorld* World = Parent ? Parent->GetWorld() : nullptr;
	if (!World || !Spec.bValid || !Recipe)
	{
		return;
	}

	TSharedPtr<FSFDeferredSpecJob> Job = MakeShared<FSFDeferredSpecJob>();
	Job->World = World;
	Job->Spec = Spec;
	Job->Recipe = Recipe;
	Job->ConstructionID = ConstructionID;
	Job->ParentLoc = Parent->GetActorLocation();
	Job->ParentRot = Parent->GetActorRotation();
	Job->bWaterPumpCells = Parent->IsA(AFGWaterPumpHologram::StaticClass());
	Job->HoloOwner = Parent->GetOwner();
	Job->Instigator = Parent->GetConstructionInstigator();

	const FSFCounterState& C = Spec.Counters;
	Job->NX = FMath::Max(1, FMath::Abs(C.GridCounters.X));
	Job->NY = FMath::Max(1, FMath::Abs(C.GridCounters.Y));
	Job->NZ = FMath::Max(1, FMath::Abs(C.GridCounters.Z));
	Job->SgnX = (C.GridCounters.X < 0) ? -1 : 1;
	Job->SgnY = (C.GridCounters.Y < 0) ? -1 : 1;
	Job->SgnZ = (C.GridCounters.Z < 0) ? -1 : 1;
	Job->TotalCells = Spec.CellCount() - 1;

	// One hidden template hologram carries the parent's multi-step choices across frames; it is
	// never constructed and dies with the job.
	if (AFGHologram* Template = AFGHologram::SpawnHologramFromRecipe(
		Recipe, Parent->GetOwner(), Job->ParentLoc, Parent->GetConstructionInstigator()))
	{
		SF_SyncMultiStepPropertiesToChild(Parent, Template);
		Template->SetActorHiddenInGame(true);
		Template->SetActorEnableCollision(false);
		Template->SetActorTickEnabled(false);
		Job->TemplateHologram = Template;
	}

	UE_LOG(LogSmartFoundations, Display,
		TEXT("[MP-SPEC] Deferring expansion of %d cells for %s (%.0fms/frame budget) - inline expansion past ~%d cells starves the net driver and disconnects clients."),
		Job->TotalCells, *Parent->GetName(), SFSpecDeferBudgetMs, SFSpecDeferThresholdCells);

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateLambda([Job]() { TickDeferredSpecJob(Job); }));
}

int32 ExpandScalingSpecIntoChildren(AFGHologram* Parent, const FSFScalingSpec& Spec,
	TSubclassOf<UFGRecipe> Recipe)
{
	if (!Parent || !Parent->GetWorld() || !Spec.bValid)
	{
		return 0;
	}
	if (!Recipe)
	{
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[MP-SPEC] ExpandScalingSpecIntoChildren: no recipe on parent hologram %s; cannot expand."),
			*Parent->GetName());
		return 0;
	}

	const FSFCounterState& C = Spec.Counters;
	const FVector ParentLoc = Parent->GetActorLocation();
	const FRotator ParentRot = Parent->GetActorRotation();

	const int32 NX = FMath::Max(1, FMath::Abs(C.GridCounters.X));
	const int32 NY = FMath::Max(1, FMath::Abs(C.GridCounters.Y));
	const int32 NZ = FMath::Max(1, FMath::Abs(C.GridCounters.Z));
	const int32 SgnX = (C.GridCounters.X < 0) ? -1 : 1;
	const int32 SgnY = (C.GridCounters.Y < 0) ? -1 : 1;
	const int32 SgnZ = (C.GridCounters.Z < 0) ? -1 : 1;

	FSFPositionCalculator Calc;
	AActor* HoloOwner = Parent->GetOwner();
	int32 SpawnedChildren = 0;
	int32 LinearIndex = 0;

	for (int32 ZI = 0; ZI < NZ; ++ZI)
	{
		for (int32 YI = 0; YI < NY; ++YI)
		{
			for (int32 XI = 0; XI < NX; ++XI)
			{
				// (0,0,0) is the parent buildable itself (built by Super::Construct), not a child.
				if (XI == 0 && YI == 0 && ZI == 0)
				{
					continue;
				}

				const int32 GX = XI * SgnX;
				const int32 GY = YI * SgnY;
				const int32 GZ = ZI * SgnZ;

				// AnchorOffset deliberately NOT passed (ZeroVector): like the legacy grid spawner
				// (SFGridSpawnerService.cpp "CRITICAL FIX: DO NOT pass AnchorOffset"), we place via
				// direct actor transform, which expects the FINAL world position. Passing the
				// registry anchor pre-lowers attachment types (splitters/mergers/pipe junctions,
				// AnchorOffset.Z ~ -100cm) by their compensation - live finding 2026-06-09: spec
				// grid children sank half-height while the parent sat correctly.
				FVector CellLoc = Calc.CalculateChildPosition(
					GX, GY, GZ, ParentLoc, ParentRot,
					Spec.ItemSize, C, LinearIndex, FVector::ZeroVector);
				++LinearIndex;

				// [#168-MP] Blueprint copies: RECONSTRUCT the client preview's exact cell position
				// instead of re-deriving it. The captured world-space basis (per-axis child offsets,
				// delta-removed) + the content delta reproduce the preview affinely: cell(i,j,k) =
				// parent + i*BX + j*BY + k*BZ + rotated delta. The calculator path provably drifts
				// one spacing unit per grid step vs the preview for composites (live 2026-07-07 dedi
				// build: every seam pipe missed its port by exactly that drift), and the conduit
				// plan is only valid at the preview's positions - #334's rule: positions are part of
				// the plan. Unsigned loop indices: direction is baked into the measured basis.
				if (Parent->IsA<AFGBlueprintHologram>())
				{
					if (Spec.bHasCellBasis)
					{
						CellLoc = ParentLoc
							+ XI * Spec.CellBasisX
							+ YI * Spec.CellBasisY
							+ ZI * Spec.CellBasisZ
							+ ParentRot.RotateVector(Spec.BlueprintContentDelta);
					}
					else if (!Spec.BlueprintContentDelta.IsZero())
					{
						CellLoc += ParentRot.RotateVector(Spec.BlueprintContentDelta);
					}
				}

				// [#363] Rotation mode: each cell rotates progressively along the arc, exactly
				// like SP's grid spawner. The calculator already curves the POSITIONS from the
				// spec's counters; the expansion was pinning every cell's FACING to the parent -
				// MP clients previewed curved runs that built with unrotated cells (Discord report,
				// day one of 32.0.0).
				// [#372] Yaw builds up along the SAME axis the arc progresses on (default X, or Y when
				// RotationAxis == Y - rows fan out), matching SFGridSpawnerService. GX/GY are signed.
				FRotator CellRot = ParentRot;
				if (!FMath::IsNearlyZero(C.RotationZ))
				{
					// Y-progression swaps forward/curve axes (reflection) -> negate so the turn matches the fan.
					const int32 RotProgress = (C.RotationAxis == ESFScaleAxis::Y) ? -GY : GX;
					CellRot.Yaw += RotProgress * C.RotationZ;
				}

				const FName ChildName(*FString::Printf(TEXT("SFSpecCell_%d_%d_%d"), GX, GY, GZ));

				// [#365] Designer context for designer-resident spec grids (MP has designers too)
				AFGBuildableBlueprintDesigner* CellDesigner = Parent->GetBlueprintDesigner();

				// #428: Water extractors must use the Smart child class. SpawnChildHologramFromRecipe
				// resolves Build_WaterPump_C to the VANILLA AFGWaterPumpHologram, whose ConfigureActor
				// asserts on a null mSnappedExtractableResource (grid children never snap a resource —
				// the parent owns the binding). ASFWaterPumpChildHologram overrides ConfigureActor to skip
				// that assert; without it an MP client placing a row of water extractors crashes the
				// authority. AFGWaterPumpHologram has a private ctor, so it can't come from recipe
				// resolution — spawn the Smart child explicitly (mirrors the SP grid spawner,
				// SFHologramHelperService.cpp:885-941).
				AFGHologram* Child = nullptr;
				UWorld* SpawnWorld = Parent->GetWorld();
				if (Parent->IsA(AFGWaterPumpHologram::StaticClass()) && SpawnWorld)
				{
					FActorSpawnParameters SpawnParams;
					// #428: do NOT set SpawnParams.Name to ChildName. The per-cell name (SFSpecCell_X_Y_Z)
					// is deterministic, so on a repeat build attempt an explicit name that is still taken by
					// a lingering actor FATALS ("Cannot generate unique name", LevelActor.cpp:585). Let UE
					// auto-name the actor (SpawnChildHologramFromRecipe does the same); AddChild still keys on ChildName.
					SpawnParams.Owner = HoloOwner;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFWaterPumpChildHologram* WaterPumpChild = SpawnWorld->SpawnActor<ASFWaterPumpChildHologram>(
						ASFWaterPumpChildHologram::StaticClass(), CellLoc, CellRot, SpawnParams);
					if (WaterPumpChild)
					{
						WaterPumpChild->SetChildBuildClass(Parent->GetBuildClass());
						WaterPumpChild->SetRecipe(Recipe);
						WaterPumpChild->FinishSpawning(FTransform(CellRot, CellLoc));
						Parent->AddChild(WaterPumpChild, ChildName);
						WaterPumpChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));
						if (CellDesigner)
						{
							WaterPumpChild->SetInsideBlueprintDesigner(CellDesigner);
						}
						Child = WaterPumpChild;
					}
				}
				else if (Parent->IsA(AFGPassthroughHologram::StaticClass()) && SpawnWorld)
				{
					// #458: Conveyor lift/pipe FLOOR HOLES (passthroughs). SpawnChildHologramFromRecipe
					// below resolves Build_FoundationPassthrough_*_C to the VANILLA AFGPassthroughHologram,
					// which (a) runs its own SetHologramLocationAndRotation snap during Construct and
					// (b) never receives the parent's snapped foundation thickness. On a dedicated server
					// the client's SP-style preview looks correct, but the authority builds these clones
					// mis-centered (half-step elevated) because they default to the shortest thickness and
					// re-snap freely. ASFPassthroughChildHologram no-ops the snap and carries the propagated
					// thickness, exactly as the SP grid spawner does (Issue #187,
					// SFHologramHelperService.cpp) - mirror that here so SP and MP agree.
					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = HoloOwner;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					SpawnParams.bDeferConstruction = true;

					ASFPassthroughChildHologram* PassthroughChild = SpawnWorld->SpawnActor<ASFPassthroughChildHologram>(
						ASFPassthroughChildHologram::StaticClass(), CellLoc, CellRot, SpawnParams);
					if (PassthroughChild)
					{
						PassthroughChild->SetBuildClass(Parent->GetBuildClass());
						PassthroughChild->SetRecipe(Recipe);
						PassthroughChild->FinishSpawning(FTransform(CellRot, CellLoc));
						Parent->AddChild(PassthroughChild, ChildName);
						PassthroughChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));
						if (CellDesigner)
						{
							PassthroughChild->SetInsideBlueprintDesigner(CellDesigner);
						}

						// Issue #187/#458: propagate the parent's snapped foundation thickness so the clone
						// seats at the foundation's vertical center. mSnappedBuildingThickness is protected;
						// on the authority we only need the value for ConfigureActor to hand to the buildable,
						// so write it directly via reflection (the client-only preview mesh rebuild that
						// SetSnappedThickness performs is unnecessary on the construction path).
						if (FFloatProperty* ThickProp = CastField<FFloatProperty>(
								Parent->GetClass()->FindPropertyByName(FName(TEXT("mSnappedBuildingThickness")))))
						{
							const float ParentThickness = ThickProp->GetPropertyValue_InContainer(Parent);
							ThickProp->SetPropertyValue_InContainer(PassthroughChild, ParentThickness);
						}

						Child = PassthroughChild;
					}
				}
				else
				{
					Child = AFGHologram::SpawnChildHologramFromRecipe(
						Parent, ChildName, Recipe, HoloOwner, CellLoc,
						[CellRot, CellDesigner](AFGHologram* NewChild)
						{
							if (NewChild)
							{
								NewChild->SetActorRotation(CellRot);
								NewChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));
								if (CellDesigner)
								{
									NewChild->SetInsideBlueprintDesigner(CellDesigner);
								}
							}
						});
				}

				if (Child)
				{
					// [#168] SMART! BLUEPRINTS - stage blueprint grid cells for CONSTRUCTION.
					// The spec-construction model strips the staged PREVIEW children and re-expands
					// here at fire time (authority path, SP included), so the recipe-spawned cell is
					// an EMPTY Holo_Blueprint_C: without the descriptor + a loaded blueprint world its
					// Construct places nothing - "only the parent builds". Stage it exactly like the
					// preview spawner does, and apply vanilla's own root-bounds alignment (the parent
					// received it through the interactive flow; unaligned clones render/build offset).
					// Stage BEFORE the exact grid transform below so the grid has the final word on
					// the root position.
					if (AFGBlueprintHologram* ParentBlueprintCell = Cast<AFGBlueprintHologram>(Parent))
					{
						if (AFGBlueprintHologram* BlueprintCell = Cast<AFGBlueprintHologram>(Child))
						{
							if (ParentBlueprintCell->mBlueprintDescriptor)
							{
								BlueprintCell->SetBlueprintDescriptor(ParentBlueprintCell->mBlueprintDescriptor);
								BlueprintCell->mBlueprintDescName = ParentBlueprintCell->mBlueprintDescName;  // [#168] proxy identity (see preview spawner)
								BlueprintCell->LoadBlueprintToOtherWorld();
								// No AlignBuildableRootWithBounds: LoadBlueprintToOtherWorld aligns
								// internally; a second call displaces the root off the grid (live
								// measurement 2026-07-06).
								UE_LOG(LogSmartFoundations, Log,
									TEXT("[#168] Staged blueprint spec cell %s from descriptor %s"),
									*Child->GetName(), *GetNameSafe(ParentBlueprintCell->mBlueprintDescriptor));
							}
							else
							{
								UE_LOG(LogSmartFoundations, Warning,
									TEXT("[#168] Blueprint parent %s has no descriptor - spec cell %s will construct EMPTY"),
									*Parent->GetName(), *Child->GetName());
							}
						}
					}

					// Exact grid transform. No placement/validation pass is needed: expansion runs
					// inside Construct, AFTER server validation has already passed on the parent -
					// fresh children are constructed directly and never validated. (Live-test finding
					// 2026-06-09: expanding BEFORE validation is unworkable - freshly spawned vanilla
					// holograms carry FGCDInitializing/FGCDInvalidFloor/FGCDInvalidAimLocation that
					// programmatic spawns cannot clear, and the whole construct gets rejected.)
					Child->SetActorLocationAndRotation(CellLoc, CellRot);

					// Multi-step parents (pole height / floodlight angle / sign step): copy the
					// player's step-2 choice from the parent so the child builds with it.
					SF_SyncMultiStepPropertiesToChild(Parent, Child);

					++SpawnedChildren;
				}
			}
		}
	}

	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("[MP-SPEC] ExpandScalingSpecIntoChildren: regenerated %d/%d grid children server-side ")
		TEXT("for %s (recipe=%s). Vanilla cost + Construct will now build the full grid."),
		SpawnedChildren, Spec.CellCount() - 1, *Parent->GetName(), *Recipe->GetName());

	return SpawnedChildren;
}

// Merge one preview's cost items into the plan's aggregated cost (per item class).
static void SF_MergePlanCost(FSFScalingSpec& Spec, const TArray<FItemAmount>& Items)
{
	for (const FItemAmount& Item : Items)
	{
		bool bMerged = false;
		for (FItemAmount& Existing : Spec.ConduitPlanCost)
		{
			if (Existing.ItemClass == Item.ItemClass)
			{
				Existing.Amount += Item.Amount;
				bMerged = true;
				break;
			}
		}
		if (!bMerged)
		{
			Spec.ConduitPlanCost.Add(Item);
		}
	}
}

void CaptureConduitPlan(AFGHologram* Hologram, FSFScalingSpec& InOutSpec)
{
	if (!Hologram)
	{
		return;
	}

	// All Smart auto-connect previews are TAGGED holograms somewhere in the parent's DESCENDANT
	// tree: distributor belts attach to the top parent, but grid-child junctions/poles attach
	// their pipe/wire previews to THEMSELVES (live finding 2026-06-10: a 5-cell junction grid
	// captured only the parent's 1 pipe with a direct-children-only walk). Walk recursively.
	static const FName BeltTag(TEXT("SF_BeltAutoConnectChild"));
	static const FName PipeTag(TEXT("SF_PipeAutoConnectChild"));
	static const FName PowerTag(TEXT("SF_PowerAutoConnectChild"));
	static const FName StackableTag(TEXT("SF_StackableChild"));

	TArray<AFGHologram*> Descendants;
	{
		TArray<AFGHologram*> Stack;
		Stack.Add(Hologram);
		while (Stack.Num() > 0)
		{
			AFGHologram* Current = Stack.Pop();
			for (AFGHologram* Child : Current->GetHologramChildren())
			{
				if (Child)
				{
					Descendants.Add(Child);
					Stack.Add(Child);
				}
			}
		}
	}

	int32 PerKind[5] = {0, 0, 0, 0, 0};
	for (AFGHologram* Child : Descendants)
	{

		FSFConduitPlanEntry Entry;
		if (ASFConveyorBeltHologram* BeltHolo = Cast<ASFConveyorBeltHologram>(Child))
		{
			if (Child->Tags.Contains(BeltTag))
			{
				Entry.Kind = ESFConduitPlanKind::Belt;
			}
			else if (Child->Tags.Contains(StackableTag))
			{
				Entry.Kind = ESFConduitPlanKind::StackableBelt;
			}
			else
			{
				continue;
			}
			if (BeltHolo->GetSplineData().Num() < 2)
			{
				continue;
			}
			Entry.SplinePoints = BeltHolo->GetSplineData();
		}
		else if (ASFPipelineHologram* PipeHolo = Cast<ASFPipelineHologram>(Child))
		{
			if (Child->Tags.Contains(PipeTag))
			{
				Entry.Kind = ESFConduitPlanKind::Pipe;
			}
			else if (Child->Tags.Contains(StackableTag))
			{
				Entry.Kind = ESFConduitPlanKind::StackablePipe;
			}
			else
			{
				continue;
			}
			if (PipeHolo->GetSplineData().Num() < 2)
			{
				continue;
			}
			Entry.SplinePoints = PipeHolo->GetSplineData();
		}
		else if (ASFWireHologram* WireHolo = Cast<ASFWireHologram>(Child))
		{
			if (!Child->Tags.Contains(PowerTag))
			{
				continue;
			}
			UFGCircuitConnectionComponent* C0 = WireHolo->GetConnection(0);
			UFGCircuitConnectionComponent* C1 = WireHolo->GetConnection(1);
			if (!C0 || !C1)
			{
				continue;
			}
			Entry.Kind = ESFConduitPlanKind::Wire;
			Entry.WireStart = C0->GetComponentLocation();
			Entry.WireEnd = C1->GetComponentLocation();
		}
		else
		{
			continue;
		}

		// Family-specific registry facts that must survive the wire (the registry is client-local).
		if (FSFHologramData* Data = USFHologramDataRegistry::GetData(Child))
		{
			if (Entry.Kind == ESFConduitPlanKind::Pipe)
			{
				Entry.bFloorHolePipe = (Data->PipeAutoConnectConn0 == nullptr);
			}
			else if (Entry.Kind == ESFConduitPlanKind::StackableBelt)
			{
				Entry.StackIndex = Data->StackableBeltIndex;
			}
			else if (Entry.Kind == ESFConduitPlanKind::StackablePipe)
			{
				Entry.StackIndex = Data->StackablePipeIndex;
			}
		}

		Entry.BuildClass = Child->GetBuildClass();
		Entry.Recipe = Child->GetRecipe();
		Entry.Location = Child->GetActorLocation();
		Entry.Rotation = Child->GetActorRotation();

		// Exact vanilla preview cost, merged per item class. Charged by the server's GetCost hook
		// alongside the cell-scaled grid cost.
		SF_MergePlanCost(InOutSpec, Child->GetCost(false));

		++PerKind[(int32)Entry.Kind];
		InOutSpec.ConduitPlan.Add(MoveTemp(Entry));
	}

	if (InOutSpec.ConduitPlan.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[MP-334] Client fire: captured conduit plan - %d belt(s), %d pipe(s), %d stackable belt(s), %d stackable pipe(s), %d wire(s) (%d cost item type(s))."),
			PerKind[0], PerKind[1], PerKind[2], PerKind[3], PerKind[4], InOutSpec.ConduitPlanCost.Num());
	}
}

// Resolve the circuit connection component nearest to a captured wire-endpoint location among
// the BUILT actors of this construct (parent + out_children), falling back to the world (a wire
// endpoint may target an existing pole/machine outside the grid).
static UFGCircuitConnectionComponent* SF_ResolveCircuitConnectionAt(
	UWorld* World, AActor* BuiltParent, const TArray<AActor*>& OutChildren,
	const FVector& Location, float Tolerance)
{
	UFGCircuitConnectionComponent* Best = nullptr;
	float BestDistSq = FMath::Square(Tolerance);

	auto Consider = [&](AActor* Actor)
	{
		if (!Actor)
		{
			return;
		}
		TArray<UFGCircuitConnectionComponent*> Connections;
		Actor->GetComponents<UFGCircuitConnectionComponent>(Connections);
		for (UFGCircuitConnectionComponent* Conn : Connections)
		{
			if (!Conn)
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(Conn->GetComponentLocation(), Location);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Conn;
			}
		}
	};

	Consider(BuiltParent);
	for (AActor* Child : OutChildren)
	{
		Consider(Child);
	}
	if (!Best && World)
	{
		for (TActorIterator<AFGBuildable> It(World); It; ++It)
		{
			Consider(*It);
		}
	}
	return Best;
}

int32 SpawnConduitPlanChildren(AFGHologram* Parent, const FSFScalingSpec& Spec)
{
	UWorld* World = Parent ? Parent->GetWorld() : nullptr;
	if (!World || Spec.ConduitPlan.Num() == 0)
	{
		return 0;
	}

	// One synthetic stack-chain id per construct: the stack-chain Construct path only gates on
	// id/index >= 0 (wiring is geometric coincidence, not index pairing), but use a high offset
	// away from client/Extend id ranges anyway.
	static int32 GSFMPStackChainSeq = 1500000000;
	const int32 StackChainId = GSFMPStackChainSeq++;

	int32 Spawned = 0;
	int32 EntryIndex = 0;
	for (const FSFConduitPlanEntry& Entry : Spec.ConduitPlan)
	{
		++EntryIndex;
		const bool bIsWire = (Entry.Kind == ESFConduitPlanKind::Wire);
		if (!Entry.BuildClass || (!bIsWire && Entry.SplinePoints.Num() < 2))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("[MP-334] SpawnConduitPlanChildren: entry %d invalid (kind=%d, class=%s, points=%d) - skipped."),
				EntryIndex, (int32)Entry.Kind, *GetNameSafe(*Entry.BuildClass), Entry.SplinePoints.Num());
			continue;
		}

		// Mirror the proven client spawn recipes (belt/pipe/stackable spawners + the Extend clone
		// spawner), minus the client-only visual finalization (the server doesn't render previews).
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Parent->GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;

		AFGHologram* Conduit = nullptr;
		switch (Entry.Kind)
		{
		case ESFConduitPlanKind::Belt:
		case ESFConduitPlanKind::StackableBelt:
		{
			ASFConveyorBeltHologram* Belt = World->SpawnActor<ASFConveyorBeltHologram>(
				ASFConveyorBeltHologram::StaticClass(), Entry.Location, Entry.Rotation, SpawnParams);
			if (!Belt)
			{
				break;
			}
			Belt->SetReplicates(false);
			Belt->SetReplicateMovement(false);
			Belt->SetBuildClass(Entry.BuildClass);
			// [#331] Propagate designer context from the constructing parent so the built
			// belt registers with the designer (vanilla copies hologram->buildable).
			if (AFGBuildableBlueprintDesigner* Designer = Parent->GetBlueprintDesigner())
			{
				Belt->SetInsideBlueprintDesigner(Designer);
			}
			// Stackable belt previews carry NO recipe on the client (their spawner sets only the
			// build class), and vanilla SetRecipe check()s non-null - SetRecipe(null) was a live
			// server crash 2026-06-10. Mirror the client: only set when captured.
			if (Entry.Recipe)
			{
				Belt->SetRecipe(Entry.Recipe);
			}
			USFHologramDataService::DisableValidation(Belt);

			if (Entry.Kind == ESFConduitPlanKind::Belt)
			{
				// SF_BeltAutoConnectChild routes Construct through the auto-connect path:
				// Super::Construct builds the belt, then it self-wires each free end to the
				// nearest free, direction-compatible connector within 50cm among BUILT actors
				// only - by then the parent buildable and all grid-cell distributors exist
				// (conduits are appended LAST in mChildren). Same mechanism SP uses.
				Belt->Tags.AddUnique(FName(TEXT("SF_BeltAutoConnectChild")));
			}
			else
			{
				// SF_StackableChild + chain identity routes Construct through the stack-chain
				// path: build fresh, then wire by geometric coincidence against the other built
				// stacked belts and register for the #341 in-frame chain unification (which runs
				// in the belt-support parent's Construct hook, also live server-side).
				Belt->Tags.AddUnique(FName(TEXT("SF_StackableChild")));
				if (FSFHologramData* Data = USFHologramDataService::GetOrCreateData(Belt))
				{
					Data->bIsStackableBelt = true;
					Data->StackableBeltIndex = FMath::Max(0, Entry.StackIndex);
					Data->StackChainId = StackChainId;
					Data->StackChainIndex = FMath::Max(0, Entry.StackIndex);
				}
			}

			Belt->FinishSpawning(FTransform(Entry.Rotation, Entry.Location));
			Belt->SetActorEnableCollision(false);
			Belt->SetSplineDataAndUpdate(Entry.SplinePoints);
			Parent->AddChild(Belt, Belt->GetFName());
			// Defensive re-apply AFTER AddChild (Extend-proven: vanilla can reset spline data on
			// child registration; empty mSplineData crashes OnRep_SplineData/UpdateSplineComponent).
			Belt->SetSplineDataAndUpdate(Entry.SplinePoints);
			Conduit = Belt;
			break;
		}

		case ESFConduitPlanKind::Pipe:
		case ESFConduitPlanKind::StackablePipe:
		{
			ASFPipelineHologram* Pipe = World->SpawnActor<ASFPipelineHologram>(
				ASFPipelineHologram::StaticClass(), Entry.Location, Entry.Rotation, SpawnParams);
			if (!Pipe)
			{
				break;
			}
			Pipe->SetReplicates(false);
			Pipe->SetReplicateMovement(false);
			Pipe->SetBuildClass(Entry.BuildClass);
			// Same null guard as belts: stackable pipe previews may carry no recipe, and vanilla
			// SetRecipe check()s non-null.
			if (Entry.Recipe)
			{
				Pipe->SetRecipe(Entry.Recipe);
			}
			USFHologramDataService::DisableValidation(Pipe);
			USFHologramDataService::MarkAsChild(Pipe, Parent, ESFChildHologramType::AutoConnect);

			FSFHologramData* Data = USFHologramDataService::GetOrCreateData(Pipe);
			if (Entry.Kind == ESFConduitPlanKind::Pipe)
			{
				Pipe->Tags.AddUnique(FName(TEXT("SF_PipeAutoConnectChild")));
				if (Data)
				{
					Data->bIsPipeAutoConnectChild = true;
				}
			}
			else
			{
				Pipe->Tags.AddUnique(FName(TEXT("SF_StackableChild")));
				if (Data)
				{
					Data->bIsStackablePipe = true;
					Data->StackablePipeIndex = FMath::Max(0, Entry.StackIndex);
				}
			}

			Pipe->FinishSpawning(FTransform(Entry.Rotation, Entry.Location));
			Pipe->SetActorEnableCollision(false);

			// PipeAutoConnectConn0 is ONLY a branch discriminator at the construct seam: null =
			// floor-hole branch (passthrough snap registration by proximity), non-null = junction
			// branch (deferred geometric wiring). The component is never dereferenced there, so
			// any non-null connection works server-side. MUST be resolved AFTER FinishSpawning -
			// the deferred-spawned hologram has no components before it (live finding 2026-06-10).
			if (Entry.Kind == ESFConduitPlanKind::Pipe && Data && !Entry.bFloorHolePipe)
			{
				TArray<UFGPipeConnectionComponentBase*> PipeConns;
				Pipe->GetComponents<UFGPipeConnectionComponentBase>(PipeConns);
				Data->PipeAutoConnectConn0 = PipeConns.Num() > 0 ? PipeConns[0] : nullptr;
				if (PipeConns.Num() == 0)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose,
						TEXT("[MP-334] SpawnConduitPlanChildren: pipe entry %d has no connection component for the junction-branch discriminator; it will take the floor-hole branch."),
						EntryIndex);
				}
			}
			Pipe->SetSplineDataAndUpdate(Entry.SplinePoints);
			Parent->AddChild(Pipe, Pipe->GetFName());
			Pipe->SetSplineDataAndUpdate(Entry.SplinePoints);
			Conduit = Pipe;
			break;
		}

		case ESFConduitPlanKind::Wire:
			// Wires are NOT built from holograms - even in SP the wire child holograms exist only
			// for cost, and the persistent wire is direct-spawned post-build (unconnected wires
			// self-destruct; live finding 2026-06-10: hologram-replayed wires built as unconnected
			// zombies). Handled by SpawnWirePlanPostConstruct AFTER the grid constructs.
			continue;

		default:
			break;
		}

		if (Conduit)
		{
			++Spawned;
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("[MP-334] SpawnConduitPlanChildren: entry %d (kind=%d) failed to spawn."),
				EntryIndex, (int32)Entry.Kind);
		}
	}

	int32 WireEntries = 0;
	for (const FSFConduitPlanEntry& Entry : Spec.ConduitPlan)
	{
		if (Entry.Kind == ESFConduitPlanKind::Wire)
		{
			++WireEntries;
		}
	}
	UE_LOG(LogSmartFoundations, Verbose,
		TEXT("[MP-334] SpawnConduitPlanChildren: %d/%d staged conduit(s) attached to %s (%d wire(s) deferred to post-construct); vanilla construct will build + wire them."),
		Spawned, Spec.ConduitPlan.Num() - WireEntries, *Parent->GetName(), WireEntries);

	return Spawned;
}

int32 SpawnWirePlanPostConstruct(AActor* BuiltParent, const TArray<AActor*>& OutChildren,
	const FSFScalingSpec& Spec, AFGBlueprintProxy* GroupProxy)
{
	UWorld* World = BuiltParent ? BuiltParent->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}

	int32 Built = 0;
	int32 WireEntries = 0;
	int32 EntryIndex = 0;
	for (const FSFConduitPlanEntry& Entry : Spec.ConduitPlan)
	{
		++EntryIndex;
		if (Entry.Kind != ESFConduitPlanKind::Wire)
		{
			continue;
		}
		++WireEntries;

		if (!Entry.BuildClass || !Entry.BuildClass->IsChildOf(AFGBuildableWire::StaticClass()))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("[MP-334] SpawnWirePlanPostConstruct: wire entry %d has invalid wire class %s - skipped."),
				EntryIndex, *GetNameSafe(*Entry.BuildClass));
			continue;
		}

		UFGCircuitConnectionComponent* C0 = SF_ResolveCircuitConnectionAt(
			World, BuiltParent, OutChildren, Entry.WireStart, 100.0f);
		UFGCircuitConnectionComponent* C1 = SF_ResolveCircuitConnectionAt(
			World, BuiltParent, OutChildren, Entry.WireEnd, 100.0f);
		if (!C0 || !C1 || C0 == C1)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("[MP-334] SpawnWirePlanPostConstruct: wire entry %d endpoints unresolved (C0=%s, C1=%s) - skipped."),
				EntryIndex, *GetNameSafe(C0), *GetNameSafe(C1));
			continue;
		}

		// Dedupe: the power manager's OnPowerPoleBuilt also runs server-side during this construct
		// and may already have wired this pair (e.g. pole-to-existing-factory connections).
		bool bAlreadyConnected = false;
		TArray<AFGBuildableWire*> ExistingWires;
		C0->GetWires(ExistingWires);
		for (AFGBuildableWire* Wire : ExistingWires)
		{
			if (Wire && (Wire->GetConnection(0) == C1 || Wire->GetConnection(1) == C1))
			{
				bAlreadyConnected = true;
				break;
			}
		}
		if (bAlreadyConnected)
		{
			continue;
		}

		// The proven OnPowerPoleBuilt primitive: direct-spawn the wire and Connect the two built
		// connection components. Unconnected wires self-destruct, so Connect failure -> Destroy.
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFGBuildableWire* NewWire = SFWireDesigner::SpawnWireForEndpoints(  // [#421] designer-aware spawn
			World, *Entry.BuildClass, Entry.WireStart, C0, C1);
		if (!NewWire)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("[MP-334] SpawnWirePlanPostConstruct: wire entry %d failed to spawn."), EntryIndex);
			continue;
		}
		if (!NewWire->Connect(C0, C1))
		{
			UE_LOG(LogSmartFoundations, VeryVerbose,
				TEXT("[MP-334] SpawnWirePlanPostConstruct: wire entry %d Connect() failed (%s <-> %s) - destroyed."),
				EntryIndex, *GetNameSafe(C0->GetOwner()), *GetNameSafe(C1->GetOwner()));
			// [NULL-WIRE GUARD] Dismantle, not Destroy: a failed Connect may still have
				// registered one side; bare Destroy leaves a dead entry in that connection's
				// SaveGame'd wire list (asserts on the owner's next dismantle / after reload).
				IFGDismantleInterface::Execute_Dismantle(NewWire);
			continue;
		}

		// Wires built here are not in out_children, so the module's proxy sweep misses them -
		// register into the Smart Dismantle group directly (wires are real actors, never
		// lightweight, so post-construct registration is safe).
		if (GroupProxy && !NewWire->GetBlueprintProxy())
		{
			NewWire->SetBlueprintProxy(GroupProxy);
			GroupProxy->RegisterBuildable(NewWire);
		}

		++Built;
	}

	if (WireEntries > 0)
	{
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[MP-334] SpawnWirePlanPostConstruct: built %d/%d staged wire(s) for %s."),
			Built, WireEntries, *GetNameSafe(BuiltParent));
	}

	return Built;
}

} // namespace SFScalingSpecExpansion
