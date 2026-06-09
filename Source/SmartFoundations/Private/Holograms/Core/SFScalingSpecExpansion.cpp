// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Core/SFScalingSpecExpansion.h"
#include "SmartFoundations.h"
#include "Hologram/FGHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFPositionCalculator.h"
#include "Data/SFBuildableSizeRegistry.h"
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

	const FSFCounterState Counters = SS->GetCounterState();
	const int32 NX = FMath::Max(1, FMath::Abs(Counters.GridCounters.X));
	const int32 NY = FMath::Max(1, FMath::Abs(Counters.GridCounters.Y));
	const int32 NZ = FMath::Max(1, FMath::Abs(Counters.GridCounters.Z));
	if (NX * NY * NZ <= 1)
	{
		return false; // trivial grid: nothing to expand server-side
	}

	USFBuildableSizeRegistry::Initialize();
	const FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass());

	OutSpec.Counters = Counters;
	OutSpec.ItemSize = Profile.DefaultSize;
	OutSpec.AnchorOffset = Profile.AnchorOffset;
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

				const FVector CellLoc = Calc.CalculateChildPosition(
					GX, GY, GZ, ParentLoc, ParentRot,
					Spec.ItemSize, C, LinearIndex, Spec.AnchorOffset);
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
					// Clear the never-aimed state. Freshly spawned holograms carry FGCDInitializing /
					// FGCDInvalidAimLocation until they receive a placement pass, and server-side
					// validation rejects the whole construct on them (live-test finding, 2026-06-09).
					// Run one placement pass with a synthetic hit at the cell, then restore the exact
					// grid transform (same pattern as the Extend lift spawner).
					FHitResult CellHit;
					CellHit.Location = CellLoc;
					CellHit.ImpactPoint = CellLoc;
					CellHit.ImpactNormal = FVector::UpVector;
					Child->SetHologramLocationAndRotation(CellHit);
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

} // namespace SFScalingSpecExpansion
