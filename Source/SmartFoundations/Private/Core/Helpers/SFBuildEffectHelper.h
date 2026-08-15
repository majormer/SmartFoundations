// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

class AFGBuildable;

/**
 * Narrow access seam for completing vanilla build effects on Smart-spawned buildables.
 * AFGBuildable keeps OnBuildEffectFinished protected; AccessTransformers.ini friends
 * only this helper so feature code does not depend directly on vanilla internals.
 */
class FSFBuildEffectHelper
{
public:
    static void Finish(AFGBuildable* Buildable);
};
