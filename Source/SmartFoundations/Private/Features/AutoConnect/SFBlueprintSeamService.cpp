// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * [#168] Smart! Blueprints — seam auto-connect pair search (FR1/FR2).
 * See SFBlueprintSeamService.h for the model. This file owns every access to
 * AFGBlueprintHologram::mDuplicateConnectionToOriginalMap (AccessTransformers Friend).
 */

#include "Features/AutoConnect/SFBlueprintSeamService.h"

#include "Hologram/FGBlueprintHologram.h"
#include "FGFactoryConnectionComponent.h"
#include "FGPipeConnectionComponent.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "SFLogMacros.h"

/** Working record for one open original connector during the pair search. */
struct FSFBlueprintSeamService::FSeamConnector
{
	int32 Index = INDEX_NONE;      // index into the kind-filtered dup list (the transform-invariant identity)
	FVector Pos = FVector::ZeroVector;     // ORIGINAL connector position (blueprint-world space)
	FVector Normal = FVector::ZeroVector;  // ORIGINAL connector outward normal
	bool bCanOutput = false;
	bool bCanInput = false;
	EPipeConnectionType PipeType = EPipeConnectionType::PCT_ANY;
	FName OriginalName;
	bool bClaimed = false;         // consumed by a pair on its axis (one physical port, one seam)
};

namespace
{
	/** Quantize to 1cm buckets — deterministic tie-breaking (the #464 lesson: never tolerance-compare in sorts). */
	FORCEINLINE int64 QuantizeCm(double V)
	{
		return static_cast<int64>(FMath::RoundToDouble(V));
	}

	FORCEINLINE double AxisValue(const FVector& V, ESFSeamAxis Axis)
	{
		switch (Axis)
		{
		case ESFSeamAxis::X: return V.X;
		case ESFSeamAxis::Y: return V.Y;
		default:             return V.Z;
		}
	}

	/** The two lane coordinates for an axis, in fixed order (determinism). */
	FORCEINLINE void LaneValues(const FVector& V, ESFSeamAxis Axis, double& OutLane1, double& OutLane2)
	{
		switch (Axis)
		{
		case ESFSeamAxis::X: OutLane1 = V.Y; OutLane2 = V.Z; break;
		case ESFSeamAxis::Y: OutLane1 = V.X; OutLane2 = V.Z; break;
		default:             OutLane1 = V.X; OutLane2 = V.Y; break;
		}
	}

	bool ArePipeTypesCompatible(EPipeConnectionType A, EPipeConnectionType B)
	{
		if (A == EPipeConnectionType::PCT_SNAP_ONLY || B == EPipeConnectionType::PCT_SNAP_ONLY)
		{
			return false;
		}
		if (A == EPipeConnectionType::PCT_ANY || B == EPipeConnectionType::PCT_ANY)
		{
			return true;
		}
		return A != B; // consumer <-> producer
	}

	const TCHAR* AxisName(ESFSeamAxis Axis)
	{
		switch (Axis)
		{
		case ESFSeamAxis::X: return TEXT("X");
		case ESFSeamAxis::Y: return TEXT("Y");
		default:             return TEXT("Z");
		}
	}
}

void FSFBlueprintSeamService::GetDuplicatedBeltConnectors(AFGBlueprintHologram* Blueprint, TArray<UFGFactoryConnectionComponent*>& OutConnectors)
{
	OutConnectors.Reset();
	if (!Blueprint)
	{
		return;
	}
	TInlineComponentArray<UFGFactoryConnectionComponent*> Components(Blueprint);
	for (UFGFactoryConnectionComponent* Component : Components)
	{
		if (Component && Blueprint->mDuplicateConnectionToOriginalMap.Contains(Component))
		{
			OutConnectors.Add(Component);
		}
	}
}

void FSFBlueprintSeamService::GetDuplicatedPipeConnectors(AFGBlueprintHologram* Blueprint, TArray<UFGPipeConnectionComponent*>& OutConnectors)
{
	OutConnectors.Reset();
	if (!Blueprint)
	{
		return;
	}
	// UFGPipeConnectionComponent = fluid pipes only; hypertube connectors are the sibling
	// UFGPipeConnectionComponentHyper class and are deliberately not collected (v1 scope).
	TInlineComponentArray<UFGPipeConnectionComponent*> Components(Blueprint);
	for (UFGPipeConnectionComponent* Component : Components)
	{
		if (Component && Blueprint->mDuplicateConnectionToOriginalMap.Contains(Component))
		{
			OutConnectors.Add(Component);
		}
	}
}

bool FSFBlueprintSeamService::DupNameMatchesOriginal(const UObject* DupComponent, const FName& ExpectedOriginalName)
{
	// Dup names are "<blueprintWorldBuildableName>_<originalConnectorName>" (DuplicateConnectionComponent).
	// The buildable name embeds a per-world instance id that differs between clones; the trailing
	// original-connector name is content-fixed, so a suffix check catches order divergence.
	if (!DupComponent || ExpectedOriginalName.IsNone())
	{
		return false;
	}
	return DupComponent->GetName().EndsWith(ExpectedOriginalName.ToString());
}

UFGFactoryConnectionComponent* FSFBlueprintSeamService::ResolveBeltConnector(AFGBlueprintHologram* Clone, int32 Index, const FName& ExpectedOriginalName)
{
	TArray<UFGFactoryConnectionComponent*> Connectors;
	GetDuplicatedBeltConnectors(Clone, Connectors);
	if (!Connectors.IsValidIndex(Index))
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168] Seam resolve FAILED: belt index %d out of range (%d dups) on %s"),
			Index, Connectors.Num(), *GetNameSafe(Clone));
		return nullptr;
	}
	if (!DupNameMatchesOriginal(Connectors[Index], ExpectedOriginalName))
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168] Seam resolve MISMATCH: belt index %d on %s is %s, expected suffix %s — enumeration order diverged, pair skipped"),
			Index, *GetNameSafe(Clone), *GetNameSafe(Connectors[Index]), *ExpectedOriginalName.ToString());
		return nullptr;
	}
	return Connectors[Index];
}

UFGPipeConnectionComponent* FSFBlueprintSeamService::ResolvePipeConnector(AFGBlueprintHologram* Clone, int32 Index, const FName& ExpectedOriginalName)
{
	TArray<UFGPipeConnectionComponent*> Connectors;
	GetDuplicatedPipeConnectors(Clone, Connectors);
	if (!Connectors.IsValidIndex(Index))
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168] Seam resolve FAILED: pipe index %d out of range (%d dups) on %s"),
			Index, Connectors.Num(), *GetNameSafe(Clone));
		return nullptr;
	}
	if (!DupNameMatchesOriginal(Connectors[Index], ExpectedOriginalName))
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168] Seam resolve MISMATCH: pipe index %d on %s is %s, expected suffix %s — enumeration order diverged, pair skipped"),
			Index, *GetNameSafe(Clone), *GetNameSafe(Connectors[Index]), *ExpectedOriginalName.ToString());
		return nullptr;
	}
	return Connectors[Index];
}

bool FSFBlueprintSeamService::BuildSeamTable(AFGBlueprintHologram* Blueprint, FSFBlueprintSeamTable& OutTable)
{
	OutTable.Pairs.Reset();
	OutTable.bComputed = false;
	if (!Blueprint)
	{
		return false;
	}
	OutTable.BlueprintName = FName(*Blueprint->mBlueprintDescName);

	// GEOMETRY FRAME: the search runs on the parent hologram's DUP connectors transformed into
	// HOLOGRAM-LOCAL space — that frame IS the grid frame (grid X/Y = parent local X/Y), which
	// the resolved endpoints are serviced in. The ORIGINALS' blueprint-world frame is NOT it:
	// live 2026-07-07 the originals read 180°-flipped vs the dups (same content-convention
	// mismatch as the clone content delta), which put every output on the wrong face and made
	// every resolved pair fail facing sanity. Originals still provide openness + flow direction
	// (connectivity is frame-free).
	const FTransform HologramTransform = Blueprint->GetActorTransform();

	// ---- Collect open BELT connectors (hologram-local dup geometry, dup-list indices) ----
	TArray<FSeamConnector> BeltConnectors;
	{
		TArray<UFGFactoryConnectionComponent*> Dups;
		GetDuplicatedBeltConnectors(Blueprint, Dups);
		OutTable.BeltConnectorCount = Dups.Num();
		for (int32 Index = 0; Index < Dups.Num(); ++Index)
		{
			UFGFactoryConnectionComponent* Original = Cast<UFGFactoryConnectionComponent>(
				Blueprint->mDuplicateConnectionToOriginalMap.FindRef(Dups[Index]));
			if (!Original || Original->IsConnected())
			{
				continue; // FR2: only OPEN content connectors participate
			}

			FSeamConnector Connector;
			Connector.Index = Index;
			Connector.Pos = HologramTransform.InverseTransformPosition(Dups[Index]->GetComponentLocation());
			Connector.Normal = HologramTransform.InverseTransformVectorNoScale(Dups[Index]->GetConnectorNormal());
			Connector.OriginalName = Original->GetFName();

			switch (Original->GetDirection())
			{
			case EFactoryConnectionDirection::FCD_OUTPUT:
				Connector.bCanOutput = true;
				break;
			case EFactoryConnectionDirection::FCD_INPUT:
				Connector.bCanInput = true;
				break;
			case EFactoryConnectionDirection::FCD_ANY:
				// Belt ENDS are FCD_ANY; flow is fixed by which end of the conveyor they are:
				// items enter at Connection0 and exit at Connection1.
				if (const AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Original->GetOwner()))
				{
					Connector.bCanOutput = (Original == Conveyor->GetConnection1());
					Connector.bCanInput = (Original == Conveyor->GetConnection0());
				}
				else
				{
					Connector.bCanOutput = true;
					Connector.bCanInput = true;
				}
				break;
			default:
				break; // FCD_SNAP_ONLY etc. never pair
			}
			BeltConnectors.Add(Connector);
		}
	}

	// ---- Collect open PIPE connectors ----
	TArray<FSeamConnector> PipeConnectors;
	{
		TArray<UFGPipeConnectionComponent*> Dups;
		GetDuplicatedPipeConnectors(Blueprint, Dups);
		OutTable.PipeConnectorCount = Dups.Num();
		for (int32 Index = 0; Index < Dups.Num(); ++Index)
		{
			UFGPipeConnectionComponent* Original = Cast<UFGPipeConnectionComponent>(
				Blueprint->mDuplicateConnectionToOriginalMap.FindRef(Dups[Index]));
			if (!Original || Original->IsConnected())
			{
				continue;
			}

			FSeamConnector Connector;
			Connector.Index = Index;
			Connector.Pos = HologramTransform.InverseTransformPosition(Dups[Index]->GetComponentLocation());
			Connector.Normal = HologramTransform.InverseTransformVectorNoScale(Dups[Index]->GetConnectorNormal());
			Connector.OriginalName = Original->GetFName();
			Connector.PipeType = Original->GetPipeConnectionType();
			if (Connector.PipeType == EPipeConnectionType::PCT_SNAP_ONLY)
			{
				continue;
			}
			// Pipes are undirected at the pairing level; compatibility is checked pairwise.
			Connector.bCanOutput = true;
			Connector.bCanInput = true;
			PipeConnectors.Add(Connector);
		}
	}

	// [#168] Connector dump — face classification is read straight off these lines when a
	// blueprint pairs unexpectedly (local pos + outward normal in the hologram/grid frame).
	for (const FSeamConnector& C : BeltConnectors)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168]   belt conn [%d]%s local=(%.0f,%.0f,%.0f) normal=(%.2f,%.2f,%.2f) out=%d in=%d"),
			C.Index, *C.OriginalName.ToString(), C.Pos.X, C.Pos.Y, C.Pos.Z, C.Normal.X, C.Normal.Y, C.Normal.Z,
			C.bCanOutput ? 1 : 0, C.bCanInput ? 1 : 0);
	}
	for (const FSeamConnector& C : PipeConnectors)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168]   pipe conn [%d]%s local=(%.0f,%.0f,%.0f) normal=(%.2f,%.2f,%.2f) type=%d"),
			C.Index, *C.OriginalName.ToString(), C.Pos.X, C.Pos.Y, C.Pos.Z, C.Normal.X, C.Normal.Y, C.Normal.Z,
			static_cast<int32>(C.PipeType));
	}

	// ---- Pair each axis independently (Z computed too; the v1 spawner ignores it) ----
	for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
	{
		const ESFSeamAxis Axis = static_cast<ESFSeamAxis>(AxisIndex);
		PairAxis(BeltConnectors, Axis, /*bPipes=*/false, OutTable.Pairs);
		PairAxis(PipeConnectors, Axis, /*bPipes=*/true, OutTable.Pairs);
		// Claims are per axis: a connector faces at most one axis (FACING_AXIS_MIN_DOT > cos45°),
		// so resetting here only lets the OTHER kind's list start clean for its own axes.
		for (FSeamConnector& Connector : BeltConnectors) { Connector.bClaimed = false; }
		for (FSeamConnector& Connector : PipeConnectors) { Connector.bClaimed = false; }
	}

	OutTable.bComputed = true;

	// ---- [#168] FR1 validation dump ----
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168] Seam table for '%s': %d pairs (X=%d Y=%d Z=%d) from %d open belt / %d open pipe connectors (%d/%d dups)"),
		*OutTable.BlueprintName.ToString(), OutTable.Pairs.Num(),
		OutTable.NumPairsForAxis(ESFSeamAxis::X), OutTable.NumPairsForAxis(ESFSeamAxis::Y), OutTable.NumPairsForAxis(ESFSeamAxis::Z),
		BeltConnectors.Num(), PipeConnectors.Num(), OutTable.BeltConnectorCount, OutTable.PipeConnectorCount);
	for (int32 PairIndex = 0; PairIndex < OutTable.Pairs.Num(); ++PairIndex)
	{
		const FSFBlueprintSeamPair& Pair = OutTable.Pairs[PairIndex];
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168]   pair %d: axis=%s %s from=[%d]%s to=[%d]%s flow=%s"),
			PairIndex, AxisName(Pair.Axis), Pair.bIsPipe ? TEXT("PIPE") : TEXT("BELT"),
			Pair.FromIndex, *Pair.FromOriginalName.ToString(),
			Pair.ToIndex, *Pair.ToOriginalName.ToString(),
			Pair.bFromOnPositiveFace ? TEXT("lower->upper") : TEXT("upper->lower"));
	}
	return true;
}

void FSFBlueprintSeamService::PairAxis(TArray<FSeamConnector>& Connectors, ESFSeamAxis Axis, bool bPipes, TArray<FSFBlueprintSeamPair>& OutPairs)
{
	// Split into +face / -face populations: outward-facing along the axis, within FACE_TOLERANCE
	// of the outermost same-facing connector (connector-derived face planes — no bounds-space
	// assumptions; the outermost port IS the face).
	TArray<FSeamConnector*> PlusFace;
	TArray<FSeamConnector*> MinusFace;
	double PlusExtreme = -TNumericLimits<double>::Max();
	double MinusExtreme = TNumericLimits<double>::Max();
	for (FSeamConnector& Connector : Connectors)
	{
		const double NormalDot = AxisValue(Connector.Normal, Axis);
		if (NormalDot >= FACING_AXIS_MIN_DOT)
		{
			PlusFace.Add(&Connector);
			PlusExtreme = FMath::Max(PlusExtreme, AxisValue(Connector.Pos, Axis));
		}
		else if (NormalDot <= -FACING_AXIS_MIN_DOT)
		{
			MinusFace.Add(&Connector);
			MinusExtreme = FMath::Min(MinusExtreme, AxisValue(Connector.Pos, Axis));
		}
	}
	PlusFace.RemoveAll([&](const FSeamConnector* C) { return PlusExtreme - AxisValue(C->Pos, Axis) > FACE_TOLERANCE; });
	MinusFace.RemoveAll([&](const FSeamConnector* C) { return AxisValue(C->Pos, Axis) - MinusExtreme > FACE_TOLERANCE; });
	if (PlusFace.Num() == 0 || MinusFace.Num() == 0)
	{
		return;
	}

	// Deterministic processing order: quantized lane coords, then index (#464 lesson).
	auto SortByLane = [Axis](TArray<FSeamConnector*>& List)
	{
		List.Sort([Axis](const FSeamConnector& A, const FSeamConnector& B)
		{
			double A1, A2, B1, B2;
			LaneValues(A.Pos, Axis, A1, A2);
			LaneValues(B.Pos, Axis, B1, B2);
			if (QuantizeCm(A1) != QuantizeCm(B1)) return QuantizeCm(A1) < QuantizeCm(B1);
			if (QuantizeCm(A2) != QuantizeCm(B2)) return QuantizeCm(A2) < QuantizeCm(B2);
			return A.Index < B.Index;
		});
	};
	SortByLane(PlusFace);
	SortByLane(MinusFace);

	// Greedy same-lane matching: for each From candidate, claim the nearest-in-lane To candidate
	// within LANE_MATCH_TOLERANCE. Quantized keys keep ties deterministic.
	auto MatchInto = [&](TArray<FSeamConnector*>& FromList, TArray<FSeamConnector*>& ToList, bool bFromOnPositiveFace)
	{
		for (FSeamConnector* From : FromList)
		{
			if (From->bClaimed || !From->bCanOutput)
			{
				continue;
			}
			double FromLane1, FromLane2;
			LaneValues(From->Pos, Axis, FromLane1, FromLane2);

			FSeamConnector* Best = nullptr;
			int64 BestDistSq = TNumericLimits<int64>::Max();
			for (FSeamConnector* To : ToList)
			{
				if (To->bClaimed || !To->bCanInput || To == From)
				{
					continue;
				}
				if (bPipes && !ArePipeTypesCompatible(From->PipeType, To->PipeType))
				{
					continue;
				}
				double ToLane1, ToLane2;
				LaneValues(To->Pos, Axis, ToLane1, ToLane2);
				const int64 DeltaLane1 = QuantizeCm(FromLane1) - QuantizeCm(ToLane1);
				const int64 DeltaLane2 = QuantizeCm(FromLane2) - QuantizeCm(ToLane2);
				const int64 DistSq = DeltaLane1 * DeltaLane1 + DeltaLane2 * DeltaLane2;
				const int64 ToleranceSq = static_cast<int64>(LANE_MATCH_TOLERANCE) * static_cast<int64>(LANE_MATCH_TOLERANCE);
				if (DistSq > ToleranceSq)
				{
					continue;
				}
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Best = To;
				}
				// Equal quantized distance: earlier sort position wins (ToList is lane-sorted) — keep first.
			}
			if (Best)
			{
				From->bClaimed = true;
				Best->bClaimed = true;
				FSFBlueprintSeamPair Pair;
				Pair.Axis = Axis;
				Pair.bIsPipe = bPipes;
				Pair.FromIndex = From->Index;
				Pair.ToIndex = Best->Index;
				Pair.bFromOnPositiveFace = bFromOnPositiveFace;
				Pair.FromOriginalName = From->OriginalName;
				Pair.ToOriginalName = Best->OriginalName;
				OutPairs.Add(Pair);
			}
		}
	};

	// Pass A — flow lower->upper: +face outputs feed the neighbor's -face inputs.
	MatchInto(PlusFace, MinusFace, /*bFromOnPositiveFace=*/true);
	if (!bPipes)
	{
		// Pass B — reverse flow: -face outputs feed the LOWER neighbor's +face inputs (a belt
		// crossing the blueprint the other way). Pipes are undirected — pass A covers them.
		MatchInto(MinusFace, PlusFace, /*bFromOnPositiveFace=*/false);
	}
}
