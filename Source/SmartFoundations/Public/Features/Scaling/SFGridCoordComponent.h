// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SFGridCoordComponent.generated.h"

/**
 * #418 coordinate keying: binds a scaling-grid child hologram to its UNSIGNED grid cell
 * (X,Y,Z >= 0; the parent occupies [0,0,0], which is never assigned to a child).
 *
 * Identity travels WITH the child, so:
 *  - resyncing SpawnedChildren from the parent's mChildren stays lossless;
 *  - growing/shrinking an axis is a pure cell set-difference (only genuinely-new cells spawn,
 *    only vanished cells despawn) instead of a full index->coordinate remap - the fix for
 *    "growing Y visibly refreshes every existing hologram";
 *  - the stackable AC neighbor map reads each child's cell directly instead of re-deriving
 *    cells from spawn order.
 *
 * Scale DIRECTION is deliberately NOT stored: positioning applies the live counter signs
 * (Cell * Dir), so flipping an axis repositions children correctly without changing identity.
 * Attached to every grid child regardless of tier - Smart classes and vanilla-delegate
 * stackables alike (which is why this is a component, not a base-class field).
 *
 * See docs/Features/Scaling/DESIGN_Scaling_ChildTypeSelection.md.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFGridCoordComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Unsigned grid cell (parent = [0,0,0], never assigned to a child). */
	FIntVector Cell = FIntVector::ZeroValue;

	/** Find-or-create the component on Child and set its cell. Returns nullptr for invalid Child. */
	static USFGridCoordComponent* AssignCell(AActor* Child, const FIntVector& InCell);

	/** Read Child's cell. False if the component is absent (child predates cell assignment). */
	static bool TryGetCell(const AActor* Child, FIntVector& OutCell);
};
