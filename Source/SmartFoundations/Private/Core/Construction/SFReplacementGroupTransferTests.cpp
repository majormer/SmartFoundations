// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFReplacementGroupTransfer.h"
#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "FGBlueprintProxy.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFReplacementGroupTransferTest, "SmartFoundations.Upgrade.GroupTransfer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFReplacementGroupTransferTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("New requests preserve membership by default"), FSFUpgradeExecutionParams().bPreserveDismantleGroupMembership);
	UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
	if (!TestNotNull(TEXT("Isolated preview world"), World)) return false;
	GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
	ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
	AFGBlueprintProxy* Group = World->SpawnActor<AFGBlueprintProxy>();
	AFGBlueprintProxy* Other = World->SpawnActor<AFGBlueprintProxy>();
	AFGBuildableConveyorBelt* Old = World->SpawnActor<AFGBuildableConveyorBelt>();
	AFGBuildableConveyorBelt* New = World->SpawnActor<AFGBuildableConveyorBelt>();
	if (!TestNotNull(TEXT("Original group"), Group) || !TestNotNull(TEXT("Other group"), Other)
		|| !TestNotNull(TEXT("Original belt"), Old) || !TestNotNull(TEXT("Replacement belt"), New))
	{
		return false;
	}

	Old->SetBlueprintProxy(Group);
	New->SetBlueprintProxy(Other);
	{
		FSFReplacementGroupTransfer Disabled(Old, false);
		TestFalse(TEXT("Off does not hold the group"), FSFReplacementGroupTransfer::IsGroupHeld(Group));
		Disabled.Apply(New);
		TestTrue(TEXT("Off retains native replacement membership"), New->GetBlueprintProxy() == Other);
	}
	TestTrue(TEXT("Off retains original membership"), Old->GetBlueprintProxy() == Group);
	{
		FSFReplacementGroupTransfer Failed(Old, true);
		TestTrue(TEXT("Original group held before construction"), FSFReplacementGroupTransfer::IsGroupHeld(Group));
		TestFalse(TEXT("Unrelated group not held"), FSFReplacementGroupTransfer::IsGroupHeld(Other));
		Old->SetBlueprintProxy(nullptr); // Model a native callback clearing the association.
		Failed.Apply(nullptr);
	}
	TestTrue(TEXT("Failed construction restores surviving original"), Old->GetBlueprintProxy() == Group);
	TestFalse(TEXT("Failure releases lifetime hold"), FSFReplacementGroupTransfer::IsGroupHeld(Group));
	{
		FSFReplacementGroupTransfer Transfer(Old, true);
		Transfer.Apply(New);
		TestTrue(TEXT("Replacement inherits exact original group"), New->GetBlueprintProxy() == Group);
		New->SetBlueprintProxy(Other); // Model Upgrade_Implementation changing it afterward.
		Transfer.Apply(New);
		TestTrue(TEXT("Post-upgrade reconciliation keeps original group"), New->GetBlueprintProxy() == Group);
		{
			FSFReplacementGroupTransfer Nested(Old, true);
			TestTrue(TEXT("Nested replacement shares hold"), FSFReplacementGroupTransfer::IsGroupHeld(Group));
		}
		TestTrue(TEXT("Inner scope cannot release outer hold"), FSFReplacementGroupTransfer::IsGroupHeld(Group));
	}
	TestFalse(TEXT("Success releases lifetime hold"), FSFReplacementGroupTransfer::IsGroupHeld(Group));
	TestNull(TEXT("Lifespan-retired original no longer belongs to group"), Old->GetBlueprintProxy());
	Old->SetBlueprintProxy(nullptr);
	New->SetBlueprintProxy(nullptr);
	{
		FSFReplacementGroupTransfer Ungrouped(Old, true);
		Ungrouped.Apply(New);
	}
	TestNull(TEXT("Ungrouped replacement stays ungrouped"), New->GetBlueprintProxy());
	Old->SetBlueprintProxy(Group);
	AFGBuildableConveyorBelt* FailedReplacement = World->SpawnActor<AFGBuildableConveyorBelt>();
	{
		FSFReplacementGroupTransfer Transfer(Old, true);
		Transfer.Apply(FailedReplacement);
		Old->SetBlueprintProxy(nullptr);
		FailedReplacement->Destroy();
	}
	TestTrue(TEXT("Replacement lost during callbacks restores the surviving original"), Old->GetBlueprintProxy() == Group);
	TestFalse(TEXT("Replacement failure releases lifetime hold"), FSFReplacementGroupTransfer::IsGroupHeld(Group));
	// The editor SDK stubs native proxy registration/self-destruction. These assertions cover
	// scoped holds and persisted actor association; reciprocal membership/lifetime need Shipping.
	return true;
}
#endif
