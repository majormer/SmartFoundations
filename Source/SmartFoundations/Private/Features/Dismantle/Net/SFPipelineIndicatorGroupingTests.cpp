// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Dismantle/Net/SFPipelineIndicatorGrouping.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPipelineIndicatorGroupingTest,
	"SmartFoundations.Dismantle.PipelineIndicatorGrouping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFPipelineIndicatorGroupingTest::RunTest(const FString& Parameters)
{
	using namespace SFPipelineIndicatorGrouping;
	FContext LoadedChild;
	LoadedChild.bGameWorld = true;
	LoadedChild.bAuthority = true;
	LoadedChild.bNativeOwnerMatches = true;
	LoadedChild.bParentRegistered = true;
	TestTrue(TEXT("Native owned indicator can join its registered pipe's existing group"), CanRegister(LoadedChild));

	FContext Unrelated = LoadedChild;
	Unrelated.bNativeOwnerMatches = false;
	TestFalse(TEXT("Nearby indicator cannot be adopted without the exact native owner"), CanRegister(Unrelated));
	FContext StaleParent = LoadedChild;
	StaleParent.bParentRegistered = false;
	TestFalse(TEXT("A stale proxy pointer cannot establish a group"), CanRegister(StaleParent));
	FContext ForeignMember = LoadedChild;
	ForeignMember.bIndicatorHasGroup = true;
	TestFalse(TEXT("An assigned child is never re-registered or moved to another group"), CanRegister(ForeignMember));
	FContext Client = LoadedChild;
	Client.bAuthority = false;
	TestFalse(TEXT("Replication supplies client membership without a local append"), CanRegister(Client));
	FContext DesignerPipe = LoadedChild;
	DesignerPipe.bParentInDesigner = true;
	TestFalse(TEXT("Native Designer pipe ownership is preserved"), CanRegister(DesignerPipe));
	FContext DesignerChild = LoadedChild;
	DesignerChild.bIndicatorInDesigner = true;
	TestFalse(TEXT("Native Designer indicator ownership is preserved"), CanRegister(DesignerChild));
	FContext Preview = LoadedChild;
	Preview.bGameWorld = false;
	TestFalse(TEXT("Preview worlds are excluded"), CanRegister(Preview));
	return true;
}
#endif
