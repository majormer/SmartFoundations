// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * [#168] Smart! Blueprints — seam auto-connect EVALUATION (FR3–FR7).
 *
 * ProcessBlueprintSeams runs on the orchestrator's debounced grid-change/movement cadence for a
 * scaled BLUEPRINT parent. It resolves the cached pair table (FSFBlueprintSeamService — pairs by
 * index, computed once, held through transforms) against every adjacent clone pair along X and Y
 * and feeds the endpoints to the EXISTING conduit machinery:
 *   - belts: CreateOrUpdateBeltPreview (facing sanity + vanilla shape arbiter + skip flags)
 *   - pipes: FPipePreviewHelper + the #466 IsRoutedShapeInvalid arbiter (mirrors the junction path)
 * Previews are keyed (clone-pair, pair-index) for update-in-place; a pair whose geometry vanilla
 * declines is destroyed but STAYS in the table — it returns when transforms allow (FR3 dormancy).
 * Costs aggregate via AddChild (helpers parent to the blueprint parent hologram); construction is
 * the blueprint-Construct hook in SFGameInstanceModule_SpecHooks.cpp, which fires the SF-tagged
 * conduit children after the blueprint copies so they wire geometrically against built actors.
 */

#include "Features/AutoConnect/SFAutoConnectServiceImpl.h"
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Features/PipeAutoConnect/PipePreviewHelper.h"
#include "Features/Scaling/SFGridCoordComponent.h"
#include "Hologram/FGBlueprintHologram.h"

namespace
{
	/** [#168] Belts can't exist under vanilla's minimum length — at flush tiling (spacing 0) seam
	 *  ports touch and no conduit fits. Skipped silently (not a skip-HUD tally: nothing is wrong,
	 *  there is just no gap to bridge). Direct port-to-port wiring of flush seams is a known v1 gap. */
	constexpr float SEAM_MIN_CONDUIT_LENGTH = 50.0f;
}

const FSFBlueprintSeamTable* USFAutoConnectService::FindOrBuildSeamTable(AFGBlueprintHologram* ParentBlueprint)
{
	if (!ParentBlueprint)
	{
		return nullptr;
	}
	const FName BlueprintName = FName(*ParentBlueprint->mBlueprintDescName);

	if (const FSFBlueprintSeamTable* Existing = BlueprintSeamTables.Find(BlueprintName))
	{
		// Staleness check: same name but a different dup-connector population means the blueprint
		// was re-saved mid-session — recompute instead of wiring stale indices.
		TArray<UFGFactoryConnectionComponent*> BeltDups;
		TArray<UFGPipeConnectionComponent*> PipeDups;
		FSFBlueprintSeamService::GetDuplicatedBeltConnectors(ParentBlueprint, BeltDups);
		FSFBlueprintSeamService::GetDuplicatedPipeConnectors(ParentBlueprint, PipeDups);
		if (Existing->BeltConnectorCount == BeltDups.Num() && Existing->PipeConnectorCount == PipeDups.Num())
		{
			return Existing;
		}
		UE_LOG(LogSmartAutoConnect, Log, TEXT("[#168] Seam table for '%s' is stale (dups %d/%d -> %d/%d) — recomputing"),
			*BlueprintName.ToString(), Existing->BeltConnectorCount, Existing->PipeConnectorCount, BeltDups.Num(), PipeDups.Num());
	}

	FSFBlueprintSeamTable& Table = BlueprintSeamTables.FindOrAdd(BlueprintName);
	FSFBlueprintSeamService::BuildSeamTable(ParentBlueprint, Table);
	return Table.bComputed ? &Table : nullptr;
}

void USFAutoConnectService::ProcessBlueprintSeams(AFGHologram* ParentHologram)
{
	AFGBlueprintHologram* ParentBlueprint = Cast<AFGBlueprintHologram>(ParentHologram);
	if (!ParentBlueprint || !Subsystem)
	{
		return;
	}

	// Drop state for dead parents so long sessions don't accumulate stale entries
	for (auto It = BlueprintSeamStates.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	if (Subsystem->IsSmartDisabledForCurrentAction())
	{
		CleanupAllBlueprintSeams(ParentHologram);
		return;
	}
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	if (!RuntimeSettings.bEnabled)
	{
		CleanupAllBlueprintSeams(ParentHologram);
		return;
	}

	// ---- Collect the clone grid (parent at [0,0,0] + SF_GridChild blueprint clones by cell) ----
	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (!HologramHelper)
	{
		return;
	}
	const FSFCounterState& CounterState = Subsystem->GetCounterState();
	const int32 XCount = FMath::Abs(CounterState.GridCounters.X);
	const int32 YCount = FMath::Abs(CounterState.GridCounters.Y);
	const int32 ZCount = FMath::Abs(CounterState.GridCounters.Z);

	auto PackGridPos = [](int32 X, int32 Y, int32 Z) -> int64 {
		return ((int64)(X + 128) << 16) | ((int64)(Y + 128) << 8) | (int64)(Z + 128);
	};

	TMap<int64, AFGBlueprintHologram*> GridToClone;
	GridToClone.Add(PackGridPos(0, 0, 0), ParentBlueprint);
	for (const TWeakObjectPtr<AFGHologram>& ChildPtr : HologramHelper->GetSpawnedChildren())
	{
		FIntVector Cell;
		if (ChildPtr.IsValid()
			&& USFGridCoordComponent::TryGetCell(ChildPtr.Get(), Cell))
		{
			if (AFGBlueprintHologram* Clone = Cast<AFGBlueprintHologram>(ChildPtr.Get()))
			{
				GridToClone.Add(PackGridPos(Cell.X, Cell.Y, Cell.Z), Clone);
			}
		}
	}
	if (GridToClone.Num() < 2)
	{
		// Grid shrank to a single copy — every seam preview is orphaned (mirror #391/#397).
		CleanupAllBlueprintSeams(ParentHologram);
		return;
	}

	// ---- Pair table (FR1, cached) ----
	const FSFBlueprintSeamTable* Table = FindOrBuildSeamTable(ParentBlueprint);
	if (!Table || Table->Pairs.Num() == 0)
	{
		CleanupAllBlueprintSeams(ParentHologram);
		return;
	}

	// Seam skips re-tally from scratch each evaluation. A blueprint parent never runs the
	// distributor/junction evaluations concurrently (type-gated), so resetting here is safe.
	SkipSummary.ResetBeltBuilding();
	SkipSummary.ResetBeltManifold();
	SkipSummary.ResetPipes();

	// ---- Tier resolution (mirrors the stackable paths) ----
	AFGPlayerController* PlayerController = nullptr;
	if (ParentHologram->GetConstructionInstigator())
	{
		PlayerController = Cast<AFGPlayerController>(ParentHologram->GetConstructionInstigator()->GetController());
	}
	int32 BeltTier = RuntimeSettings.BeltTierMain;
	if (BeltTier == 0)
	{
		BeltTier = Subsystem->GetHighestUnlockedBeltTier(PlayerController);
	}
	int32 PipeTier = RuntimeSettings.PipeTierMain;
	if (PipeTier == 0)
	{
		PipeTier = Subsystem->GetHighestUnlockedPipeTier(PlayerController);
		if (PipeTier == 0) { PipeTier = 2; }
	}
	const bool bWithIndicator = RuntimeSettings.bPipeIndicator;
	const bool bPipesEnabled = RuntimeSettings.bPipeAutoConnectEnabled;

	FBlueprintSeamState& State = BlueprintSeamStates.FindOrAdd(ParentHologram);
	TSet<FSeamKey> ActiveKeys;
	int32 BeltsPlaced = 0;
	int32 PipesPlaced = 0;

	// Cell-index order does NOT imply spatial order: scaling toward -X/-Y places cell N+1 at the
	// MORE NEGATIVE parent-local coordinate (live 2026-07-07: cell[1] at local -3700 — resolving
	// "From" by index put every endpoint on the far side, 180° facing, all pairs "too steep").
	// "Lower" is the clone whose +Axis face abuts the seam — decide it from actual positions.
	const FTransform ParentTransform = ParentHologram->GetActorTransform();
	auto LocalAxisCoord = [&ParentTransform](const AFGBlueprintHologram* Clone, ESFSeamAxis Axis) -> double
	{
		const FVector Local = ParentTransform.InverseTransformPosition(Clone->GetActorLocation());
		return (Axis == ESFSeamAxis::X) ? Local.X : (Axis == ESFSeamAxis::Y) ? Local.Y : Local.Z;
	};

	// The conduit routers reposition their actor with SetActorLocation, which is a NO-OP on a
	// LOCKED hologram — and the finalize pass below locks every seam conduit while the parent is
	// locked (i.e. always, during scaling). Without this unlock, the first evaluation placed the
	// conduit and every later one (spacing/steps/stagger) updated only the spline data while the
	// actor stayed at the OLD seam (live 2026-07-07: 10m spacing left belts floating at the 2m
	// positions). Same unlock -> move/route -> relock contract as the stackable update-in-place.
	auto UnlockForUpdate = [](FConduitPreviewHelper* Helper)
	{
		if (Helper)
		{
			if (AFGSplineHologram* Conduit = Helper->GetHologram())
			{
				if (Conduit->IsHologramLocked())
				{
					Conduit->LockHologramPosition(false);
				}
			}
		}
	};

	// ---- Walk every adjacent clone pair along X and Y (Z deferred: pipes v1.5, belt-lifts v2) ----
	for (int32 AxisIndex = 0; AxisIndex < 2; ++AxisIndex)
	{
		const ESFSeamAxis Axis = static_cast<ESFSeamAxis>(AxisIndex);
		if (Table->NumPairsForAxis(Axis) == 0)
		{
			continue;
		}
		for (int32 Z = 0; Z < FMath::Max(ZCount, 1); ++Z)
		{
			for (int32 Y = 0; Y < FMath::Max(YCount, 1); ++Y)
			{
				for (int32 X = 0; X < FMath::Max(XCount, 1); ++X)
				{
					if (Axis == ESFSeamAxis::X && X >= XCount - 1) continue;
					if (Axis == ESFSeamAxis::Y && Y >= YCount - 1) continue;

					AFGBlueprintHologram* const* CellAPtr = GridToClone.Find(PackGridPos(X, Y, Z));
					AFGBlueprintHologram* const* CellBPtr = (Axis == ESFSeamAxis::X)
						? GridToClone.Find(PackGridPos(X + 1, Y, Z))
						: GridToClone.Find(PackGridPos(X, Y + 1, Z));
					if (!CellAPtr || !CellBPtr || !*CellAPtr || !*CellBPtr)
					{
						continue;
					}
					const bool bAIsLower = LocalAxisCoord(*CellAPtr, Axis) <= LocalAxisCoord(*CellBPtr, Axis);
					AFGBlueprintHologram* Lower = bAIsLower ? *CellAPtr : *CellBPtr;
					AFGBlueprintHologram* Upper = bAIsLower ? *CellBPtr : *CellAPtr;
					const uint64 ClonePairKey = MakePolePairKey(Lower, Upper);

					for (int32 PairIndex = 0; PairIndex < Table->Pairs.Num(); ++PairIndex)
					{
						const FSFBlueprintSeamPair& Pair = Table->Pairs[PairIndex];
						if (Pair.Axis != Axis)
						{
							continue;
						}
						const FSeamKey Key(ClonePairKey, PairIndex);

						// From (output) lives on the lower clone when flow crosses the +face;
						// otherwise on the upper clone flowing back across its -face.
						AFGBlueprintHologram* FromClone = Pair.bFromOnPositiveFace ? Lower : Upper;
						AFGBlueprintHologram* ToClone = Pair.bFromOnPositiveFace ? Upper : Lower;

						if (!Pair.bIsPipe)
						{
							UFGFactoryConnectionComponent* FromConnector = FSFBlueprintSeamService::ResolveBeltConnector(FromClone, Pair.FromIndex, Pair.FromOriginalName);
							UFGFactoryConnectionComponent* ToConnector = FSFBlueprintSeamService::ResolveBeltConnector(ToClone, Pair.ToIndex, Pair.ToOriginalName);
							if (!FromConnector || !ToConnector)
							{
								continue;
							}
							const float Gap = FVector::Dist(FromConnector->GetComponentLocation(), ToConnector->GetComponentLocation());
							if (Gap < SEAM_MIN_CONDUIT_LENGTH)
							{
								continue; // flush tiling — no conduit fits (known v1 gap, see header note)
							}

							TSharedPtr<FBeltPreviewHelper>& Helper = State.BeltsBySeamKey.FindOrAdd(Key);
							if (!Helper.IsValid())
							{
								Helper = MakeShared<FBeltPreviewHelper>(GetWorld(), BeltTier, ParentHologram);
							}
							UnlockForUpdate(Helper.Get());
							if (CreateOrUpdateBeltPreview(FromConnector, ToConnector, Helper, FACING_SANITY_ANGLE, false, ParentHologram))
							{
								// Update-in-place re-ROUTES but never re-MESHES: FinalizeSpawn runs
								// TriggerMeshGeneration on creation only, so a transformed seam kept
								// rendering its OLD geometry while actor+spline were already correct
								// (live 2026-07-07, SmartMCP spline dump). Same post-route regen as
								// the stackable update path.
								if (ASFConveyorBeltHologram* BeltHologram = Cast<ASFConveyorBeltHologram>(Helper->GetHologram()))
								{
									BeltHologram->TriggerMeshGeneration();
									BeltHologram->ForceApplyHologramMaterial();
								}
								ActiveKeys.Add(Key);
								BeltsPlaced++;
							}
							else
							{
								// Pair stays in the table (dormant, FR3); tally the decline for the HUD.
								if (WasLastBeltRejectInvalidShape())
								{
									SkipSummary.BeltsInvalidShape++;
								}
								else
								{
									SkipSummary.BeltsTooSteep++;
								}
								State.BeltsBySeamKey.Remove(Key);
							}
						}
						else if (bPipesEnabled)
						{
							UFGPipeConnectionComponent* FromConnector = FSFBlueprintSeamService::ResolvePipeConnector(FromClone, Pair.FromIndex, Pair.FromOriginalName);
							UFGPipeConnectionComponent* ToConnector = FSFBlueprintSeamService::ResolvePipeConnector(ToClone, Pair.ToIndex, Pair.ToOriginalName);
							if (!FromConnector || !ToConnector)
							{
								continue;
							}
							const FVector FromPos = FromConnector->GetComponentLocation();
							const FVector ToPos = ToConnector->GetComponentLocation();
							const float Gap = FVector::Dist(FromPos, ToPos);
							if (Gap < SEAM_MIN_CONDUIT_LENGTH)
							{
								continue; // flush tiling
							}

							auto DestroySeamPipe = [&State, &Key]()
							{
								if (TSharedPtr<FPipePreviewHelper>* Existing = State.PipesBySeamKey.Find(Key))
								{
									if (Existing->IsValid())
									{
										(*Existing)->DestroyPreview();
									}
									State.PipesBySeamKey.Remove(Key);
								}
							};

							if (Gap > MAX_PIPE_LENGTH)
							{
								SkipSummary.PipesTooFar++;
								DestroySeamPipe();
								continue;
							}
							// Facing SANITY only (#466) — shape validity is the game's verdict below.
							const FVector DirToEnd = (ToPos - FromPos).GetSafeNormal();
							const float AngleFrom = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
								FVector::DotProduct(FromConnector->GetConnectorNormal(), DirToEnd), -1.0f, 1.0f)));
							const float AngleTo = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
								FVector::DotProduct(ToConnector->GetConnectorNormal(), -DirToEnd), -1.0f, 1.0f)));
							if (AngleFrom > FACING_SANITY_ANGLE || AngleTo > FACING_SANITY_ANGLE)
							{
								SkipSummary.PipesInvalidShape++;
								DestroySeamPipe();
								continue;
							}

							TSharedPtr<FPipePreviewHelper>& Helper = State.PipesBySeamKey.FindOrAdd(Key);
							if (!Helper.IsValid())
							{
								Helper = MakeShared<FPipePreviewHelper>(GetWorld(), PipeTier, bWithIndicator, ParentHologram);
							}
							UnlockForUpdate(Helper.Get());
							Helper->UpdatePreview(FromConnector, ToConnector);
							ASFPipelineHologram* PipeHologram = Helper->GetTypedHologram();
							if (!PipeHologram || !Helper->IsPreviewValid())
							{
								Helper->DestroyPreview();
								State.PipesBySeamKey.Remove(Key);
								continue;
							}
							// [#466] Vanilla is the shape arbiter — decline what the player couldn't place.
							if (PipeHologram->IsRoutedShapeInvalid())
							{
								SkipSummary.PipesInvalidShape++;
								Helper->DestroyPreview();
								State.PipesBySeamKey.Remove(Key);
								continue;
							}
							// Post-route mesh regen — see the belt branch note (creation-only meshing).
							PipeHologram->TriggerMeshGeneration();
							PipeHologram->ForceApplyHologramMaterial();
							ActiveKeys.Add(Key);
							PipesPlaced++;
						}
					}
				}
			}
		}
	}

	// ---- Orphan removal: keys whose clone pair vanished (grid shrank / spacing collapsed) ----
	for (auto It = State.BeltsBySeamKey.CreateIterator(); It; ++It)
	{
		if (!ActiveKeys.Contains(It.Key()))
		{
			if (It.Value().IsValid())
			{
				It.Value()->DestroyPreview();
			}
			It.RemoveCurrent();
		}
	}
	for (auto It = State.PipesBySeamKey.CreateIterator(); It; ++It)
	{
		if (!ActiveKeys.Contains(It.Key()))
		{
			if (It.Value().IsValid())
			{
				It.Value()->DestroyPreview();
			}
			It.RemoveCurrent();
		}
	}

	// ---- Visibility / material / lock sync (mirrors the stackable finalize pass) ----
	const bool bParentLocked = ParentHologram->IsHologramLocked();
	const EHologramMaterialState ParentMaterialState = ParentHologram->GetHologramMaterialState();
	auto FinalizeConduit = [bParentLocked, ParentMaterialState](AFGSplineHologram* Conduit)
	{
		if (!Conduit)
		{
			return;
		}
		Conduit->SetActorHiddenInGame(false);
		Conduit->SetPlacementMaterialState(ParentMaterialState);
		if (bParentLocked && !Conduit->IsHologramLocked())
		{
			Conduit->LockHologramPosition(true);
		}
	};
	for (const auto& Entry : State.BeltsBySeamKey)
	{
		if (Entry.Value.IsValid()) { FinalizeConduit(Entry.Value->GetHologram()); }
	}
	for (const auto& Entry : State.PipesBySeamKey)
	{
		if (Entry.Value.IsValid()) { FinalizeConduit(Entry.Value->GetHologram()); }
	}

	UE_LOG(LogSmartAutoConnect, Log, TEXT("[#168] Seams evaluated for %s: grid[%d,%d,%d] clones=%d pairs=%d -> belts=%d pipes=%d (skips: steep=%d shape=%d pipeShape=%d pipeFar=%d)"),
		*ParentHologram->GetName(), XCount, YCount, ZCount, GridToClone.Num(), Table->Pairs.Num(),
		BeltsPlaced, PipesPlaced,
		SkipSummary.BeltsTooSteep, SkipSummary.BeltsInvalidShape,
		SkipSummary.PipesInvalidShape, SkipSummary.PipesTooFar);
}

void USFAutoConnectService::CleanupAllBlueprintSeams(AFGHologram* ParentHologram)
{
	FBlueprintSeamState* State = BlueprintSeamStates.Find(ParentHologram);
	if (!State)
	{
		return;
	}
	int32 Destroyed = 0;
	for (auto& Entry : State->BeltsBySeamKey)
	{
		if (Entry.Value.IsValid())
		{
			Entry.Value->DestroyPreview();
			Destroyed++;
		}
	}
	for (auto& Entry : State->PipesBySeamKey)
	{
		if (Entry.Value.IsValid())
		{
			Entry.Value->DestroyPreview();
			Destroyed++;
		}
	}
	BlueprintSeamStates.Remove(ParentHologram);
	if (Destroyed > 0)
	{
		UE_LOG(LogSmartAutoConnect, Log, TEXT("[#168] Cleaned up %d seam conduit previews for %s"), Destroyed, *GetNameSafe(ParentHologram));
	}
}

void USFAutoConnectService::CleanupAllBlueprintSeamsAllParents()
{
	for (auto& StateEntry : BlueprintSeamStates)
	{
		for (auto& Entry : StateEntry.Value.BeltsBySeamKey)
		{
			if (Entry.Value.IsValid())
			{
				Entry.Value->DestroyPreview();
			}
		}
		for (auto& Entry : StateEntry.Value.PipesBySeamKey)
		{
			if (Entry.Value.IsValid())
			{
				Entry.Value->DestroyPreview();
			}
		}
	}
	BlueprintSeamStates.Empty();
	BlueprintSeamTables.Empty();
}
