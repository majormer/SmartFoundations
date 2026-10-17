// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"

/** World-vertical support selection for each sign-grid column. */
namespace SFSignSupportPolicy
{
	/** Unsigned layer index at the floor/ceiling end of the actual signed vertical span. */
	int32 GetSupportLayer(int32 SignedLayerCount, double LayerPitch, double PoleDirectionZ);
	/** Native selected height plus the cell's actual world-vertical displacement. */
	double GetSupportHeight(double NativeHeight, double ParentZ, double CellZ, double PoleDirectionZ);
}
