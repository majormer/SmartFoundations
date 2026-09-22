// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/PowerAutoConnect/SFBlueprintPowerService.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintPowerPlanTest, "SmartFoundations.Power.BlueprintNetworks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintPowerPlanTest::RunTest(const FString& Parameters)
{
	TArray<FSFBlueprintPowerNode> Nodes;
	for (int32 Z = 0; Z < 2; ++Z)
		for (int32 Y = 0; Y < 3; ++Y)
			for (int32 X = 0; X < 2; ++X)
			{
				const FIntVector Cell(X, Y, Z);
				const FVector Position(-X * 4000, Y * 4000, Z * 4000);
				Nodes.Add({Cell, TEXT("Face1"), TEXT("CircuitA"), Position, 2});
				Nodes.Add({Cell, TEXT("Face2"), TEXT("CircuitA"), Position + FVector(0, 140, 0), 1});
				Nodes.Add({Cell, TEXT("PoleB"), TEXT("CircuitB"), Position + FVector(500, 0, 0), 2});
			}
	const auto Edges = FSFBlueprintPowerService::PlanSpans(Nodes, 0, 10000);
	TestEqual(TEXT("One chain per separate internal circuit; no duplicate double-face chains"), Edges.Num(), 22);
	TMap<int32, int32> Used;
	for (const auto& Edge : Edges)
	{
		TestTrue(TEXT("Do not join separate circuits"), Nodes[Edge.Key].Network == Nodes[Edge.Value].Network);
		TestTrue(TEXT("Counterpart identity retained"), Nodes[Edge.Key].Identity == Nodes[Edge.Value].Identity);
		++Used.FindOrAdd(Edge.Key); ++Used.FindOrAdd(Edge.Value);
	}
	for (const auto& Entry : Used) TestTrue(TEXT("Internal wiring and each face's own free slots respected"), Entry.Value <= Nodes[Entry.Key].FreeSlots);
	TestEqual(TEXT("Overlong grid becomes dormant"), FSFBlueprintPowerService::PlanSpans(Nodes, 0, 3000).Num(), 0);
	TestEqual(TEXT("Restoring spacing restores edges"), FSFBlueprintPowerService::PlanSpans(Nodes, 0, 10000).Num(), 22);
	for (auto& Node : Nodes) Node.FreeSlots = 0;
	TestEqual(TEXT("Full poles do not plan unfunded-capacity wires"), FSFBlueprintPowerService::PlanSpans(Nodes, 0, 10000).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintSocketTransformTest, "SmartFoundations.Power.BlueprintSocketTransforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFBlueprintSocketTransformTest::RunTest(const FString& Parameters)
{
	const FTransform OriginalOwner(FRotator(0, 180, 0), FVector(1200, -500, 800));
	for (double Face : {-70.0, 70.0})
	{
		const FTransform LocalSocket(FRotator(15, 45, 0), FVector(Face, 0, 400));
		for (const FRotator Rotation : {FRotator::ZeroRotator, FRotator(0, 90, 0), FRotator(15, 180, 20)})
		{
			const FTransform BuiltOwner(Rotation, FVector(200000, -50000, 8000));
			const FTransform Resolved = FSFBlueprintPowerService::OwnerTransformFromSocket(OriginalOwner,
				LocalSocket * OriginalOwner, LocalSocket * BuiltOwner);
			TestTrue(TEXT("Exact built pivot independent of blueprint world convention and socket rotation"), Resolved.Equals(BuiltOwner, 0.001));
		}
	}
	return true;
}
#endif
