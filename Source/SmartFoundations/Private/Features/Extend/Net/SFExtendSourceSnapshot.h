// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "Features/Extend/SFExtendTypes.h"

namespace SFExtendSourceSnapshot
{
    /** Bound the complete value reply before it enters a reliable RPC. */
    bool FitsReply(const FSFSourceTopology& Source);
    /** Replace exact owner IDs throughout the value graph, preserving named sockets. */
    void RemapOwners(FSFSourceTopology& Source, const TMap<FString, FString>& Owners);
    /** Resolve server identities to client identities using replicated actor references. */
    bool ResolveReply(FSFExtendTopology& Reply);
}
