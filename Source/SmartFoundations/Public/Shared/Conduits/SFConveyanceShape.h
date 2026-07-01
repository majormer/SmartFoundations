// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - shared span SHAPE-validity rules for stackable conveyances (belt / pipe / hypertube).

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"

/**
 * Shared span SHAPE-validity rules for stackable conveyances. Factored out of the Smart Walking
 * validator (USFWalkService::GetSegmentShapeError) so the walk, the hypertube auto-connect, and
 * (later) belt/pipe auto-connect share ONE rule set instead of re-deriving "too long / too steep"
 * per type.
 *
 * The RULES live here; each feature keeps its own POLICY: the walk blocks the whole run on ANY
 * invalid span, while auto-connect just SKIPS the one over-length pair. Callers pass MaxLenCm
 * (belt/pipe use the vanilla ~56 m spline cap; hypertube passes its own ~95 m cap), so this header
 * carries no Feature dependency. The over-length message reports the caller's actual cap (#417).
 */
namespace SFConveyanceShape
{
	enum class EKind : uint8 { Belt, Pipe, Hypertube };

	/**
	 * @return empty if span A->B is a valid shape for Kind under MaxLenCm; otherwise a localized,
	 *         player-facing reason. Only BELTS get the 30-degree slope gate - pipes route at any
	 *         angle and hypertubes climb steeply by design.
	 */
	inline FString EvaluateSpan(const FVector& A, const FVector& B, EKind Kind, float MaxLenCm, int32 OneBasedIndex)
	{
		const float Dist = FVector::Dist(A, B);
		if (Dist > MaxLenCm)
		{
			// [#417] The cap is a format argument, NOT baked into the string: hypertube's cap (~95 m)
			// diverged from the belt/pipe ~56 m vanilla spline cap, and the old hard-coded "> 56m"
			// under-reported the hypertube limit. Source-string change = the 20 non-English cultures
			// need this one key re-translated (done alongside this change in Localization/).
			FFormatOrderedArguments Args;
			Args.Add(OneBasedIndex);
			Args.Add(FMath::RoundToInt(Dist / 100.0f));
			Args.Add(FMath::RoundToInt(MaxLenCm / 100.0f));
			return FText::Format(NSLOCTEXT("SmartFoundations", "Walk_Invalid_TooLong", "segment {0} too long ({1}m > {2}m)"), Args).ToString();
		}
		if (Kind == EKind::Belt)
		{
			const FVector Dir = (B - A).GetSafeNormal();
			const float SlopeDeg = FMath::RadiansToDegrees(FMath::Asin(FMath::Abs(Dir.Z)));
			if (SlopeDeg > 30.0f)
			{
				FFormatOrderedArguments Args;
				Args.Add(OneBasedIndex);
				Args.Add(FMath::RoundToInt(SlopeDeg));
				return FText::Format(NSLOCTEXT("SmartFoundations", "Walk_Invalid_TooSteep", "segment {0} too steep ({1}deg > 30deg)"), Args).ToString();
			}
		}
		return FString();
	}
}
