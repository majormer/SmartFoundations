// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Features/Extend/SFExtendTypes.h"

class USFSubsystem;
class AFGBuildable;

namespace SFExtendTopologyQuery
{
    /** Capture an authoritative aim reply without changing any player's live Extend cache. */
    FSFExtendTopology Capture(USFSubsystem* Subsystem, AFGBuildable* SourceBuilding);
}
