// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Features/Scaling/SFProxyGridConstructionCost.h"
#include "Engine/World.h"
#include "Hologram/FGHologram.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFGridPaymentRequestLifetimeTest,
    "SmartFoundations.Scaling.GridPaymentRequestLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFGridPaymentRequestLifetimeTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Isolated authority world"), World)) return false;
    AFGHologram* Parent = World->SpawnActor<AFGHologram>();
    AFGHologram* Other = World->SpawnActor<AFGHologram>();
    if (!Parent || !Other) { World->DestroyWorld(false); return false; }
    TArray<FItemAmount> Accepted;
    Accepted.AddDefaulted_GetRef().Amount = 2;
    TArray<FItemAmount> Observed;
    SFProxyGridConstructionCost::RetainForRequest(Parent, Accepted);
    TestFalse(TEXT("No quote can escape a native request"), SFProxyGridConstructionCost::GetAcceptedCost(Parent, Observed));
    {
        SFProxyGridConstructionCost::FRequestScope Request;
        SFProxyGridConstructionCost::RetainForRequest(Parent, Accepted);
        Accepted[0].Amount = 99;
        TestTrue(TEXT("Quote survives staged plan consumption"), SFProxyGridConstructionCost::GetAcceptedCost(Parent, Observed));
        TestEqual(TEXT("Accepted amount is copied"), Observed[0].Amount, 2);
        TestFalse(TEXT("Other roots cannot use this quote"), SFProxyGridConstructionCost::GetAcceptedCost(Other, Observed));
        {
            SFProxyGridConstructionCost::FRequestScope Nested;
            TestFalse(TEXT("Nested request cannot inherit an outer payment"), SFProxyGridConstructionCost::GetAcceptedCost(Parent, Observed));
            SFProxyGridConstructionCost::RetainForRequest(Other, Accepted);
            TestTrue(TEXT("Nested request owns its own quote"), SFProxyGridConstructionCost::GetAcceptedCost(Other, Observed));
        }
        TestFalse(TEXT("Nested quote is retired"), SFProxyGridConstructionCost::GetAcceptedCost(Other, Observed));
        TestTrue(TEXT("Outer quote remains until outer payment"), SFProxyGridConstructionCost::GetAcceptedCost(Parent, Observed));
        TestEqual(TEXT("Outer quote was not overwritten"), Observed[0].Amount, 2);
    }
    TestFalse(TEXT("Completed request cannot charge a subsequent build"), SFProxyGridConstructionCost::GetAcceptedCost(Parent, Observed));
    World->DestroyWorld(false);
    return true;
}
#endif
