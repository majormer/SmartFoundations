// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"

class AActor;
class AFGBuildable;
class AFGBlueprintProxy;

/** Keeps replicated group membership authoritative while client actor references resolve. */
struct SMARTFOUNDATIONS_API FSFBlueprintGroupReplication
{
	static void RegisterHooks();

	/** Group only unowned, world-placed conduits produced by this Blueprint operation. */
	static AFGBlueprintProxy* GroupConstructedSeams(const TArray<AActor*>& SeamActors);

	/** Members demonstrably reassigned to another native group during construction. */
	static TArray<AFGBuildable*> FindReassignedMembers(const AFGBlueprintProxy* Proxy);

	/** Remove stale registrations only from this completed Blueprint operation's groups. */
	static void ReconcileConstructedGroups(const TArray<AActor*>& BuiltActors);
};
