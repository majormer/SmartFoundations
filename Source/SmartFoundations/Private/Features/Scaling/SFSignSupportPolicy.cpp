// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFSignSupportPolicy.h"

int32 SFSignSupportPolicy::GetSupportLayer(int32 SignedLayerCount, double LayerPitch, double PoleDirectionZ)
{
	const int32 LastLayer = FMath::Max(1, FMath::Abs(SignedLayerCount)) - 1;
	const double SignedSpan = LastLayer * (SignedLayerCount < 0 ? -1.0 : 1.0) * LayerPitch;
	return SignedSpan * PoleDirectionZ < 0.0 ? LastLayer : 0;
}

double SFSignSupportPolicy::GetSupportHeight(double NativeHeight, double ParentZ, double CellZ, double PoleDirectionZ)
{
	return NativeHeight + (CellZ - ParentZ) * PoleDirectionZ;
}
