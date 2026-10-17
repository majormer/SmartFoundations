// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

namespace SFConveyanceLength
{
	/** Use the native hologram's configured ceiling, including its numerical allowance. */
	template <typename THologram>
	bool ExceedsNativeLimit(double LengthCm, const THologram& Hologram)
	{
		return LengthCm > Hologram.GetMaxSplineLength();
	}
}
