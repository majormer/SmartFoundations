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
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

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

bool IsSpecConstructionEnabled()
{
	const bool bEnabled = CVarSFMPSpecConstruction.GetValueOnAnyThread() != 0;

	// One-time visibility: make the gate state unambiguous in every session log.
	static bool bLoggedOnce = false;
	if (!bLoggedOnce)
	{
		bLoggedOnce = true;
		UE_LOG(LogSmartFoundations, Display,
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
	OutSpec.BuildClass = Hologram->GetBuildClass();
	OutSpec.bValid = true;
	return true;
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
		UE_LOG(LogSmartFoundations, Warning,
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
				const FVector CellLoc = Calc.CalculateChildPosition(
					GX, GY, GZ, ParentLoc, ParentRot,
					Spec.ItemSize, C, LinearIndex, FVector::ZeroVector);
				++LinearIndex;

				const FName ChildName(*FString::Printf(TEXT("SFSpecCell_%d_%d_%d"), GX, GY, GZ));

				AFGHologram* Child = AFGHologram::SpawnChildHologramFromRecipe(
					Parent, ChildName, Recipe, HoloOwner, CellLoc,
					[ParentRot](AFGHologram* NewChild)
					{
						if (NewChild)
						{
							NewChild->SetActorRotation(ParentRot);
							NewChild->Tags.AddUnique(FName(TEXT("SF_GridChild")));
						}
					});

				if (Child)
				{
					// Exact grid transform. No placement/validation pass is needed: expansion runs
					// inside Construct, AFTER server validation has already passed on the parent -
					// fresh children are constructed directly and never validated. (Live-test finding
					// 2026-06-09: expanding BEFORE validation is unworkable - freshly spawned vanilla
					// holograms carry FGCDInitializing/FGCDInvalidFloor/FGCDInvalidAimLocation that
					// programmatic spawns cannot clear, and the whole construct gets rejected.)
					Child->SetActorLocationAndRotation(CellLoc, ParentRot);
					++SpawnedChildren;
				}
			}
		}
	}

	UE_LOG(LogSmartFoundations, Display,
		TEXT("[MP-SPEC] ExpandScalingSpecIntoChildren: regenerated %d/%d grid children server-side ")
		TEXT("for %s (recipe=%s). Vanilla cost + Construct will now build the full grid."),
		SpawnedChildren, Spec.CellCount() - 1, *Parent->GetName(), *Recipe->GetName());

	return SpawnedChildren;
}

void CaptureBeltPlan(AFGHologram* Hologram, FSFScalingSpec& InOutSpec)
{
	if (!Hologram)
	{
		return;
	}
	USFSubsystem* SS = USFSubsystem::Get(Hologram->GetWorld());
	USFAutoConnectService* AutoConnect = SS ? SS->GetAutoConnectService() : nullptr;
	if (!AutoConnect)
	{
		return;
	}

	// The service stores previews keyed per distributor hologram: the parent itself and/or any of
	// its grid children (manifold lanes are stored on their SOURCE distributor, so walking each
	// distributor once visits every belt exactly once).
	TArray<AFGHologram*> Sources;
	Sources.Add(Hologram);
	for (AFGHologram* Child : Hologram->GetHologramChildren())
	{
		if (Child)
		{
			Sources.Add(Child);
		}
	}

	for (AFGHologram* Source : Sources)
	{
		const TArray<TSharedPtr<FBeltPreviewHelper>>* Previews = AutoConnect->GetBeltPreviews(Source);
		if (!Previews)
		{
			continue;
		}
		for (const TSharedPtr<FBeltPreviewHelper>& Helper : *Previews)
		{
			if (!Helper.IsValid())
			{
				continue;
			}
			ASFConveyorBeltHologram* BeltHolo = Helper->GetTypedHologram();
			if (!IsValid(BeltHolo) || BeltHolo->GetSplineData().Num() < 2)
			{
				continue;
			}

			FSFBeltPlanEntry Entry;
			Entry.BeltClass = BeltHolo->GetBuildClass();
			Entry.Recipe = BeltHolo->GetRecipe();
			Entry.Location = BeltHolo->GetActorLocation();
			Entry.Rotation = BeltHolo->GetActorRotation();
			Entry.SplinePoints = BeltHolo->GetSplineData();
			InOutSpec.BeltPlan.Add(MoveTemp(Entry));

			// Exact vanilla length-based cost of this preview, merged per item class. Charged by
			// the server's GetCost hook alongside the cell-scaled grid cost.
			for (const FItemAmount& Item : BeltHolo->GetCost(false))
			{
				bool bMerged = false;
				for (FItemAmount& Existing : InOutSpec.BeltPlanCost)
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
					InOutSpec.BeltPlanCost.Add(Item);
				}
			}
		}
	}

	if (InOutSpec.BeltPlan.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, Display,
			TEXT("[MP-334] Client fire: captured belt plan with %d belt(s) (%d cost item type(s)) from live previews."),
			InOutSpec.BeltPlan.Num(), InOutSpec.BeltPlanCost.Num());
	}
}

int32 SpawnBeltPlanChildren(AFGHologram* Parent, const FSFScalingSpec& Spec)
{
	UWorld* World = Parent ? Parent->GetWorld() : nullptr;
	if (!World || Spec.BeltPlan.Num() == 0)
	{
		return 0;
	}

	int32 Spawned = 0;
	int32 BeltIndex = 0;
	for (const FSFBeltPlanEntry& Entry : Spec.BeltPlan)
	{
		++BeltIndex;
		if (!Entry.BeltClass || Entry.SplinePoints.Num() < 2)
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[MP-334] SpawnBeltPlanChildren: plan entry %d invalid (class=%s, points=%d) - skipped."),
				BeltIndex, *GetNameSafe(*Entry.BeltClass), Entry.SplinePoints.Num());
			continue;
		}

		// Mirror the proven Extend clone-spawner recipe (SFExtendCloneSpawner belt_segment path),
		// minus the client-only visual finalization (the server doesn't render previews).
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Parent->GetOwner();
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;

		ASFConveyorBeltHologram* Belt = World->SpawnActor<ASFConveyorBeltHologram>(
			ASFConveyorBeltHologram::StaticClass(), Entry.Location, Entry.Rotation, SpawnParams);
		if (!Belt)
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[MP-334] SpawnBeltPlanChildren: belt %d failed to spawn."), BeltIndex);
			continue;
		}

		Belt->SetReplicates(false);
		Belt->SetReplicateMovement(false);
		Belt->SetBuildClass(Entry.BeltClass);
		Belt->SetRecipe(Entry.Recipe);

		// The SF_BeltAutoConnectChild tag routes this hologram's Construct through the auto-connect
		// path: Super::Construct builds the belt, then it self-wires each free end to the nearest
		// free, direction-compatible connector within 50cm among BUILT actors only - by then the
		// parent buildable and all grid-cell distributors exist (belts are appended LAST in
		// mChildren), and pre-existing world targets always did. Same mechanism SP uses.
		Belt->Tags.AddUnique(FName(TEXT("SF_BeltAutoConnectChild")));
		USFHologramDataService::DisableValidation(Belt);

		Belt->FinishSpawning(FTransform(Entry.Rotation, Entry.Location));
		Belt->SetActorEnableCollision(false);
		Belt->SetSplineDataAndUpdate(Entry.SplinePoints);

		Parent->AddChild(Belt, Belt->GetFName());

		// Defensive re-apply AFTER AddChild (Extend-proven: vanilla can reset spline data on
		// child registration; an empty mSplineData crashes OnRep_SplineData/UpdateSplineComponent).
		Belt->SetSplineDataAndUpdate(Entry.SplinePoints);

		++Spawned;
	}

	UE_LOG(LogSmartFoundations, Display,
		TEXT("[MP-334] SpawnBeltPlanChildren: %d/%d staged belt(s) attached to %s; vanilla construct will build + wire them."),
		Spawned, Spec.BeltPlan.Num(), *Parent->GetName());

	return Spawned;
}

} // namespace SFScalingSpecExpansion
