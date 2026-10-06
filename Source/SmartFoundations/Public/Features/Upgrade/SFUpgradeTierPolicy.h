// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"
#include "Features/Upgrade/SFUpgradeAuditService.h"

namespace SFUpgradeTierPolicy
{
	inline bool AllowsDowngrade(ESFUpgradeFamily Family)
	{
		return Family == ESFUpgradeFamily::Belt || Family == ESFUpgradeFamily::Lift || Family == ESFUpgradeFamily::Pipe;
	}

	inline bool HasAlternativeTier(int32 CurrentTier, int32 MaxUnlockedTier, bool bAllowDowngrade = true)
	{
		return CurrentTier > 0 && MaxUnlockedTier > 0
			&& (bAllowDowngrade ? (CurrentTier > 1 || MaxUnlockedTier > 1) : MaxUnlockedTier > CurrentTier);
	}

	// A selected row replaces only that source tier. A network-wide selection normalizes
	// all differing tiers to the requested target, in either direction.
	inline bool Matches(int32 CurrentTier, int32 SourceTier, int32 TargetTier, bool bAllowDowngrade = true)
	{
		return CurrentTier > 0 && SourceTier >= 0 && TargetTier > 0
			&& (bAllowDowngrade ? CurrentTier != TargetTier : CurrentTier < TargetTier)
			&& (SourceTier == 0 || CurrentTier == SourceTier);
	}

	inline bool IsValidRequest(int32 SourceTier, int32 TargetTier, bool bNetworkSelection, bool bAllowDowngrade = true)
	{
		return TargetTier > 0 && SourceTier >= 0 && SourceTier != TargetTier
			&& (bNetworkSelection || SourceTier > 0)
			&& (bAllowDowngrade || TargetTier > SourceTier);
	}
}
