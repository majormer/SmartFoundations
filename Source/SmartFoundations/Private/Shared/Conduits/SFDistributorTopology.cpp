// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Shared/Conduits/SFDistributorTopology.h"

namespace
{
	const FName Connection0(TEXT("Connection0"));
	const FName Connection1(TEXT("Connection1"));
	const FName Connection2(TEXT("Connection2"));
	const FName Connection3(TEXT("Connection3"));
	const FName Input1(TEXT("Input1"));
	const FName Input2(TEXT("Input2"));
	const FName Input3(TEXT("Input3"));
	const FName Output1(TEXT("Output1"));
	const FName Output2(TEXT("Output2"));
	const FName Output3(TEXT("Output3"));

	FSFDistributorPortTopology MakeInvalid(ESFDistributorTopologyKind Kind, FName FactoryPort)
	{
		FSFDistributorPortTopology Result;
		Result.Kind = Kind;
		Result.FactoryPort = FactoryPort;
		Result.bRecognized = true;
		return Result;
	}

	FSFDistributorPortTopology MakePipe(
		ESFDistributorTopologyKind Kind,
		FName FactoryPort,
		FName OppositeFactoryPort,
		FName LanePortA,
		FName LanePortB)
	{
		FSFDistributorPortTopology Result = MakeInvalid(Kind, FactoryPort);
		Result.OppositeFactoryPort = OppositeFactoryPort;
		Result.LanePortA = LanePortA;
		Result.LanePortB = LanePortB;
		Result.bValidManifold = true;
		return Result;
	}

	FSFDistributorPortTopology MakeBelt(
		ESFDistributorTopologyKind Kind,
		FName FactoryPort,
		FName OppositeFactoryPort)
	{
		FSFDistributorPortTopology Result = MakeInvalid(Kind, FactoryPort);
		Result.OppositeFactoryPort = OppositeFactoryPort;
		Result.LanePortA = Input1;
		Result.LanePortB = Output1;
		Result.LaneInputPort = Input1;
		Result.LaneOutputPort = Output1;
		Result.bValidManifold = true;
		return Result;
	}
}

FSFDistributorPortTopology FSFDistributorTopologyResolver::Resolve(const UClass* BuildClass, FName FactoryPort)
{
	return Resolve(BuildClass ? BuildClass->GetName() : FString(), FactoryPort);
}

FSFDistributorPortTopology FSFDistributorTopologyResolver::Resolve(const FString& BuildClassName, FName FactoryPort)
{
	if (BuildClassName == TEXT("Build_PipelineJunction_Cross_C"))
	{
		if (FactoryPort == Connection0) return MakePipe(ESFDistributorTopologyKind::PipeCross, FactoryPort, Connection1, Connection2, Connection3);
		if (FactoryPort == Connection1) return MakePipe(ESFDistributorTopologyKind::PipeCross, FactoryPort, Connection0, Connection2, Connection3);
		if (FactoryPort == Connection2) return MakePipe(ESFDistributorTopologyKind::PipeCross, FactoryPort, Connection3, Connection0, Connection1);
		if (FactoryPort == Connection3) return MakePipe(ESFDistributorTopologyKind::PipeCross, FactoryPort, Connection2, Connection0, Connection1);
		return MakeInvalid(ESFDistributorTopologyKind::PipeCross, FactoryPort);
	}

	if (BuildClassName == TEXT("Build_PipelineJunction_T_C"))
	{
		// The T has no local +Y face. Only a factory branch on local -Y leaves the
		// complete Connection0/Connection1 pair available as a manifold lane.
		if (FactoryPort == Connection2) return MakePipe(ESFDistributorTopologyKind::PipeT, FactoryPort, NAME_None, Connection0, Connection1);
		return MakeInvalid(ESFDistributorTopologyKind::PipeT, FactoryPort);
	}

	const bool bSplitter =
		BuildClassName == TEXT("Build_ConveyorAttachmentSplitter_C") ||
		BuildClassName == TEXT("Build_ConveyorAttachmentSplitterSmart_C") ||
		BuildClassName == TEXT("Build_ConveyorAttachmentSplitterProgrammable_C");
	if (bSplitter)
	{
		if (FactoryPort == Output2) return MakeBelt(ESFDistributorTopologyKind::BeltSplitter, FactoryPort, Output3);
		if (FactoryPort == Output3) return MakeBelt(ESFDistributorTopologyKind::BeltSplitter, FactoryPort, Output2);
		return MakeInvalid(ESFDistributorTopologyKind::BeltSplitter, FactoryPort);
	}

	const bool bMerger =
		BuildClassName == TEXT("Build_ConveyorAttachmentMerger_C") ||
		BuildClassName == TEXT("Build_ConveyorAttachmentMergerPriority_C");
	if (bMerger)
	{
		if (FactoryPort == Input2) return MakeBelt(ESFDistributorTopologyKind::BeltMerger, FactoryPort, Input3);
		if (FactoryPort == Input3) return MakeBelt(ESFDistributorTopologyKind::BeltMerger, FactoryPort, Input2);
		return MakeInvalid(ESFDistributorTopologyKind::BeltMerger, FactoryPort);
	}

	return FSFDistributorPortTopology();
}

bool FSFDistributorTopologyResolver::IsPipe(ESFDistributorTopologyKind Kind)
{
	return Kind == ESFDistributorTopologyKind::PipeCross || Kind == ESFDistributorTopologyKind::PipeT;
}

bool FSFDistributorTopologyResolver::IsBelt(ESFDistributorTopologyKind Kind)
{
	return Kind == ESFDistributorTopologyKind::BeltSplitter || Kind == ESFDistributorTopologyKind::BeltMerger;
}
