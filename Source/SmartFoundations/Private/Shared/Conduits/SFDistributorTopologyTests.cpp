// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Shared/Conduits/SFDistributorTopology.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSFDistributorTopologyTest,
	"SmartFoundations.Shared.DistributorTopology",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFDistributorTopologyTest::RunTest(const FString& Parameters)
{
	const auto Expect = [this](const TCHAR* ClassName, const TCHAR* FactoryPort, bool bValid, const TCHAR* LaneA, const TCHAR* LaneB)
	{
		const FSFDistributorPortTopology Result = FSFDistributorTopologyResolver::Resolve(ClassName, FName(FactoryPort));
		TestTrue(FString::Printf(TEXT("%s is recognized"), ClassName), Result.bRecognized);
		TestEqual(FString::Printf(TEXT("%s.%s validity"), ClassName, FactoryPort), Result.bValidManifold, bValid);
		if (bValid)
		{
			TestEqual(TEXT("Lane A"), Result.LanePortA, FName(LaneA));
			TestEqual(TEXT("Lane B"), Result.LanePortB, FName(LaneB));
		}
	};

	Expect(TEXT("Build_PipelineJunction_Cross_C"), TEXT("Connection0"), true, TEXT("Connection2"), TEXT("Connection3"));
	Expect(TEXT("Build_PipelineJunction_Cross_C"), TEXT("Connection1"), true, TEXT("Connection2"), TEXT("Connection3"));
	Expect(TEXT("Build_PipelineJunction_Cross_C"), TEXT("Connection2"), true, TEXT("Connection0"), TEXT("Connection1"));
	Expect(TEXT("Build_PipelineJunction_Cross_C"), TEXT("Connection3"), true, TEXT("Connection0"), TEXT("Connection1"));
	Expect(TEXT("Build_PipelineJunction_T_C"), TEXT("Connection0"), false, TEXT(""), TEXT(""));
	Expect(TEXT("Build_PipelineJunction_T_C"), TEXT("Connection1"), false, TEXT(""), TEXT(""));
	Expect(TEXT("Build_PipelineJunction_T_C"), TEXT("Connection2"), true, TEXT("Connection0"), TEXT("Connection1"));

	for (const TCHAR* SplitterClass : {
		TEXT("Build_ConveyorAttachmentSplitter_C"),
		TEXT("Build_ConveyorAttachmentSplitterSmart_C"),
		TEXT("Build_ConveyorAttachmentSplitterProgrammable_C")})
	{
		Expect(SplitterClass, TEXT("Output2"), true, TEXT("Input1"), TEXT("Output1"));
		Expect(SplitterClass, TEXT("Output3"), true, TEXT("Input1"), TEXT("Output1"));
		Expect(SplitterClass, TEXT("Input1"), false, TEXT(""), TEXT(""));
		Expect(SplitterClass, TEXT("Output1"), false, TEXT(""), TEXT(""));
	}

	for (const TCHAR* MergerClass : {
		TEXT("Build_ConveyorAttachmentMerger_C"),
		TEXT("Build_ConveyorAttachmentMergerPriority_C")})
	{
		Expect(MergerClass, TEXT("Input2"), true, TEXT("Input1"), TEXT("Output1"));
		Expect(MergerClass, TEXT("Input3"), true, TEXT("Input1"), TEXT("Output1"));
		Expect(MergerClass, TEXT("Input1"), false, TEXT(""), TEXT(""));
		Expect(MergerClass, TEXT("Output1"), false, TEXT(""), TEXT(""));
	}

	const FSFDistributorPortTopology Unknown = FSFDistributorTopologyResolver::Resolve(TEXT("Build_ModdedDistributor_C"), FName(TEXT("Port0")));
	TestFalse(TEXT("Unknown classes remain unrecognized"), Unknown.bRecognized);
	return true;
}
#endif
