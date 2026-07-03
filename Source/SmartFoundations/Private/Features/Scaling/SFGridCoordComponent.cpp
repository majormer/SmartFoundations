// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Scaling/SFGridCoordComponent.h"

USFGridCoordComponent* USFGridCoordComponent::AssignCell(AActor* Child, const FIntVector& InCell)
{
	if (!IsValid(Child))
	{
		return nullptr;
	}

	USFGridCoordComponent* Comp = Child->FindComponentByClass<USFGridCoordComponent>();
	if (!Comp)
	{
		Comp = NewObject<USFGridCoordComponent>(Child);
		Comp->RegisterComponent();
	}
	Comp->Cell = InCell;
	return Comp;
}

bool USFGridCoordComponent::TryGetCell(const AActor* Child, FIntVector& OutCell)
{
	if (!IsValid(Child))
	{
		return false;
	}

	if (const USFGridCoordComponent* Comp = Child->FindComponentByClass<USFGridCoordComponent>())
	{
		OutCell = Comp->Cell;
		return true;
	}
	return false;
}
