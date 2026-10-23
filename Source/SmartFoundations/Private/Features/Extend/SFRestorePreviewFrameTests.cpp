// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "Features/Extend/SFExtendRunFrame.h"
#include "Holograms/Core/SFFactoryHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFRestorePreviewFrameTest, "SmartFoundations.Camera.RestoreMaterializedFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFRestorePreviewFrameTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
    if (!TestNotNull(TEXT("Preview world"), World)) return false;
    ASFFactoryHologram* Parent = World->SpawnActor<ASFFactoryHologram>();
    ASFFactoryHologram* Previous = World->SpawnActor<ASFFactoryHologram>();
    ASFFactoryHologram* Last = World->SpawnActor<ASFFactoryHologram>();
    Parent->SetActorLocationAndRotation(FVector(175355, -100144, 120200), FRotator(0, 30, 0));
    Previous->SetActorLocation(FVector(176054, -99445, 120200));
    Last->SetActorLocation(FVector(176310, -98490, 120200));
    USFSubsystem* Subsystem = NewObject<USFSubsystem>(World);
    FSFCounterState State;
    State.GridCounters = FIntVector(-3, 1, 1);
    State.SpacingX = 500;
    State.RotationZ = 30;
    Subsystem->UpdateCounterState(State);
    USFExtendService* Service = NewObject<USFExtendService>(World);
    Service->Subsystem = Subsystem;
    Service->bRestoredCloneTopologyActive = true;
    Service->RestoredCloneParentHologram = Parent;
    Service->RestoredCloneTopologyTemplate = MakeShared<FSFCloneTopology>();
    Service->JsonSpawnedHolograms.Add(TEXT("rr_1_0_factory"), Previous);
    Service->JsonSpawnedHolograms.Add(TEXT("rr_2_0_factory"), Last);
    FTransform Frame;
    TestTrue(TEXT("Materialized Restore frame available"), Service->TryGetCameraRunFrame(Parent, Frame));
    TestTrue(TEXT("Focus is actual last preview, not recalculated from rotated parent"), Frame.GetLocation().Equals(Last->GetActorLocation(), .001));
    TestTrue(TEXT("Heading joins actual last two previews"), Frame.GetUnitAxis(EAxis::X).Equals((Last->GetActorLocation() - Previous->GetActorLocation()).GetSafeNormal(), .0001));
    Service->JsonSpawnedHolograms.Remove(TEXT("rr_1_0_factory"));
    TestFalse(TEXT("Missing predecessor retains caller fallback"), Service->TryGetCameraRunFrame(Parent, Frame));
    TArray<FSFScaledExtendClone> LiveClones;
    FSFScaledExtendClone First;
    First.GridX = 2; First.WorldOffset = FVector(2000, 0, 0);
    First.SpawnedHolograms.Add(TEXT("factory"), Previous);
    LiveClones.Add(First);
    FSFScaledExtendClone Final;
    Final.GridX = 3; Final.WorldOffset = FVector(3000, 0, 0);
    Final.SpawnedHolograms.Add(TEXT("factory"), Last);
    LiveClones.Add(Final);
    TestTrue(TEXT("Visible live Extend frame"), SFExtendRunFrame::TryFromClones(Parent->GetActorLocation(), Parent->GetActorLocation(), LiveClones, Frame));
    TestTrue(TEXT("Live Extend uses materialized focus over cached offset"), Frame.GetLocation().Equals(Last->GetActorLocation(), .001));
    TestTrue(TEXT("Live Extend follows actual predecessor"), Frame.GetUnitAxis(EAxis::X).Equals((Last->GetActorLocation() - Previous->GetActorLocation()).GetSafeNormal(), .0001));
    Service->JsonSpawnedHolograms.Empty();
    Service->RestoredCloneTopologyTemplate.Reset();
    World->DestroyWorld(false);
    return true;
}
#endif
