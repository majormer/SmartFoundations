// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFSignSupportPolicy.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSignSupportEdgeTest, "SmartFoundations.Scaling.SignSupportEdge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSignSupportEdgeTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Floor supports bottom of upward grid"), SFSignSupportPolicy::GetSupportLayer(3, 400, 1), 0);
	TestEqual(TEXT("Downward floor grid supports a child, not the parent"), SFSignSupportPolicy::GetSupportLayer(-3, 400, 1), 2);
	TestEqual(TEXT("Ceiling supports top of upward grid"), SFSignSupportPolicy::GetSupportLayer(3, 400, -1), 2);
	TestEqual(TEXT("Downward ceiling grid supports parent layer"), SFSignSupportPolicy::GetSupportLayer(-3, 400, -1), 0);
	TestEqual(TEXT("Negative effective pitch reverses floor edge"), SFSignSupportPolicy::GetSupportLayer(3, -100, 1), 2);
	TestEqual(TEXT("Direction and negative pitch compose"), SFSignSupportPolicy::GetSupportLayer(-3, -100, -1), 2);
	TestEqual(TEXT("Single sign retains its native pole"), SFSignSupportPolicy::GetSupportLayer(-1, 400, 1), 0);
	TestEqual(TEXT("Coincident layers do not multiply poles"), SFSignSupportPolicy::GetSupportLayer(3, 0, 1), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSignSupportHeightTest, "SmartFoundations.Scaling.SignSupportHeight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSignSupportHeightTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Downward floor child meets original plane with shorter native pole"), SFSignSupportPolicy::GetSupportHeight(650, 9299, 8899, 1), 250.0);
	TestEqual(TEXT("Upward ceiling child meets original ceiling with shorter pole"), SFSignSupportPolicy::GetSupportHeight(650, 7899, 8299, -1), 250.0);
	TestEqual(TEXT("Steps are priced from actual cell height"), SFSignSupportPolicy::GetSupportHeight(650, 9299, 9499, 1), 850.0);
	TestEqual(TEXT("Ceiling steps preserve the same ceiling plane"), SFSignSupportPolicy::GetSupportHeight(650, 7899, 7699, -1), 850.0);
	TestEqual(TEXT("A below-floor support remains invalid, not clamped"), SFSignSupportPolicy::GetSupportHeight(650, 9299, 8499, 1), -150.0);
	return true;
}
#endif
