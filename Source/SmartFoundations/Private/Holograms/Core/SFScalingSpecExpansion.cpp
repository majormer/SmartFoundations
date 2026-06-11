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
#include "FGBlueprintProxy.h"
#include "Hologram/FGPoleHologram.h"            // #354: mPoleVariationIndex / mBuildStep
#include "Hologram/FGConveyorPoleHologram.h"
#include "Hologram/FGFloodlightHologram.h"      // #200: mFixtureAngle / mBuildStep
#include "Hologram/FGStandaloneSignHologram.h"  // #192: mBuildStep
#include "Engine/World.h"
#include "EngineUtils.h"
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
	OutSpec.BuildClass = Hologram->GetBuildClass();
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
			UE_LOG(LogSmartFoundations, Warning,
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
					UE_LOG(LogSmartFoundations, Warning,
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
			UE_LOG(LogSmartFoundations, Warning,
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
			UE_LOG(LogSmartFoundations, Warning,
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
			UE_LOG(LogSmartFoundations, Warning,
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
		AFGBuildableWire* NewWire = World->SpawnActor<AFGBuildableWire>(
			*Entry.BuildClass, Entry.WireStart, FRotator::ZeroRotator, SpawnParams);
		if (!NewWire)
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[MP-334] SpawnWirePlanPostConstruct: wire entry %d failed to spawn."), EntryIndex);
			continue;
		}
		if (!NewWire->Connect(C0, C1))
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("[MP-334] SpawnWirePlanPostConstruct: wire entry %d Connect() failed (%s <-> %s) - destroyed."),
				EntryIndex, *GetNameSafe(C0->GetOwner()), *GetNameSafe(C1->GetOwner()));
			NewWire->Destroy();
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
