// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#pragma once

#include "CoreMinimal.h"

enum class ESFDistributorTopologyKind : uint8
{
	Unknown,
	PipeCross,
	PipeT,
	BeltSplitter,
	BeltMerger
};

/** Stable named-port relationships for a recognized distributor orientation. */
struct SMARTFOUNDATIONS_API FSFDistributorPortTopology
{
	ESFDistributorTopologyKind Kind = ESFDistributorTopologyKind::Unknown;
	FName FactoryPort = NAME_None;
	FName OppositeFactoryPort = NAME_None;
	FName LanePortA = NAME_None;
	FName LanePortB = NAME_None;
	FName LaneInputPort = NAME_None;
	FName LaneOutputPort = NAME_None;
	bool bRecognized = false;
	bool bValidManifold = false;
};

/**
 * Resolves vanilla distributor topology from the buildable class and the stable component name
 * used by the factory branch. Unknown classes are intentionally left unresolved so callers can
 * apply a separately validated fallback without weakening the vanilla contract.
 */
class SMARTFOUNDATIONS_API FSFDistributorTopologyResolver
{
public:
	static FSFDistributorPortTopology Resolve(const UClass* BuildClass, FName FactoryPort);
	static FSFDistributorPortTopology Resolve(const FString& BuildClassName, FName FactoryPort);

	static bool IsPipe(ESFDistributorTopologyKind Kind);
	static bool IsBelt(ESFDistributorTopologyKind Kind);
};
