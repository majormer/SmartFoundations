// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "Features/Extend/SFExtendScaledService.h"
#include "Math/RotationMatrix.h"
#include "Hologram/FGHologram.h"

namespace SFExtendRunFrame
{
	inline bool TryMake(const FVector& Focus, const FVector& Previous, FTransform& OutFrame)
	{
		const FVector Forward = (Focus - Previous).GetSafeNormal();
		if (Focus.ContainsNaN() || Forward.ContainsNaN() || Forward.IsNearlyZero()) return false;
		OutFrame = FTransform(FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).ToQuat(), Focus);
		return true;
	}

	/** Preserve the established focus selection, independent of child-array ordering. */
	inline const FSFScaledExtendClone* FindFocus(const TArray<FSFScaledExtendClone>& Clones)
	{
		const FSFScaledExtendClone* Focus = nullptr;
		float BestDistanceSquared = -1.0f;
		for (const FSFScaledExtendClone& Clone : Clones)
		{
			const float DistanceSquared = Clone.WorldOffset.SizeSquared();
			if (DistanceSquared > BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				Focus = &Clone;
			}
		}
		return Focus;
	}

	inline bool TryFromClones(const FVector& Source, const FVector& HeldFirst,
		const TArray<FSFScaledExtendClone>& Clones, FTransform& OutFrame)
	{
		const FSFScaledExtendClone* Focus = FindFocus(Clones);
		if (!Focus) return false;
		auto Position = [&](const FSFScaledExtendClone& Clone)
		{
			const TWeakObjectPtr<AFGHologram>* Factory = Clone.SpawnedHolograms.Find(TEXT("factory"));
			return Factory && Factory->IsValid() ? Factory->Get()->GetActorLocation() : Source + Clone.WorldOffset;
		};
		const FVector Location = Position(*Focus);
		for (const FSFScaledExtendClone& Other : Clones)
		{
			if (Other.GridY != Focus->GridY || Other.GridZ != Focus->GridZ) continue;
			if (Other.GridX == Focus->GridX - 1)
				return TryMake(Location, Position(Other), OutFrame);
		}
		// The held first copy and original source are intentionally absent from the clone array.
		if (Focus->GridY == 0 && Focus->GridZ == 0)
		{
			if (Focus->GridX == 2) return TryMake(Location, HeldFirst, OutFrame);
			if (Focus->GridX == 1) return TryMake(Location, Source, OutFrame);
		}
		// A far side-row seed may be the established focus. Its same-row next copy defines heading.
		for (const FSFScaledExtendClone& Other : Clones)
		{
			if (Other.GridY == Focus->GridY && Other.GridZ == Focus->GridZ && Other.GridX == Focus->GridX + 1)
				return TryMake(Location, Location - (Position(Other) - Location), OutFrame);
		}
		return false;
	}

	template <typename LocationAt>
	bool TryFromRestoredGrid(const FIntVector& LastCell, LocationAt&& GetLocation, FTransform& OutFrame)
	{
		FIntVector Previous = LastCell;
		if (Previous.X > 0) --Previous.X;
		else if (Previous.Y > 0) --Previous.Y;
		else if (Previous.Z > 0) --Previous.Z;
		else return false;
		return TryMake(GetLocation(LastCell), GetLocation(Previous), OutFrame);
	}
}
