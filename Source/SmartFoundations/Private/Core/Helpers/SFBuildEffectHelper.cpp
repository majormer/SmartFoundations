// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Core/Helpers/SFBuildEffectHelper.h"

#include "Buildables/FGBuildable.h"

void FSFBuildEffectHelper::Finish(AFGBuildable* Buildable)
{
    Buildable->OnBuildEffectFinished();
}
