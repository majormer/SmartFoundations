// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Subsystem/SFSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Buildables/FGBuildable.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFSessionControlTest,
    "SmartFoundations.Session.PlayerIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFSessionControlTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated test world"), World)) return false;
    USFSubsystem* Subsystem = NewObject<USFSubsystem>(World);
    APlayerController* First = World->SpawnActor<APlayerController>();
    APlayerController* Second = World->SpawnActor<APlayerController>();
    APawn* FirstPawn = World->SpawnActor<APawn>();
    APawn* SecondPawn = World->SpawnActor<APawn>();
    if (!First || !Second || !FirstPawn || !SecondPawn)
    {
        AddError(TEXT("Could not create isolated players"));
        World->DestroyWorld(false);
        return false;
    }
    First->Possess(FirstPawn);
    Second->Possess(SecondPawn);

    FSFScalingSpec Spec;
    Spec.bValid = true;
    Spec.BuildClass = AFGBuildable::StaticClass();
    FSFScalingSpec Observed;
    FSFExtendCommitSpec Extend;
    Extend.bValid = true;
    Extend.BuildClass = Spec.BuildClass;
    FSFExtendCommitSpec ObservedExtend;
    FSFWalkCommitSpec Walk;
    Walk.bValid = true;
    Walk.BuildClass = Spec.BuildClass;
    FSFWalkCommitSpec ObservedWalk;
    Subsystem->StageExtendCommitForPlayer(First, Extend);
    Subsystem->StageExtendCommitForPlayer(Second, Extend);
    Subsystem->StageWalkCommitForPlayer(First, Walk);
    Subsystem->StageWalkCommitForPlayer(Second, Walk);
    Subsystem->StageScalingSpecForPlayer(First, Spec);
    Subsystem->StageScalingSpecForPlayer(Second, Spec);
    TestTrue(TEXT("First player's staged grid visible"), Subsystem->PeekScalingSpecForInstigator(FirstPawn, Spec.BuildClass, Observed));
    Subsystem->SetSmartEnabledForPlayer(First, false);
    TestFalse(TEXT("Opt-out recorded only for first player"), Subsystem->IsSmartEnabledForPlayer(First));
    TestTrue(TEXT("Other player remains enabled"), Subsystem->IsSmartEnabledForPlayer(Second));
    TestFalse(TEXT("Opt-out clears abandoned grid"), Subsystem->PeekScalingSpecForInstigator(FirstPawn, Spec.BuildClass, Observed));
    TestTrue(TEXT("Other player's staged grid survives"), Subsystem->PeekScalingSpecForInstigator(SecondPawn, Spec.BuildClass, Observed));
    TestFalse(TEXT("Opt-out clears abandoned Extend"), Subsystem->PeekExtendCommitForInstigator(FirstPawn, Spec.BuildClass, ObservedExtend));
    TestFalse(TEXT("Opt-out clears abandoned Walk"), Subsystem->PeekWalkCommitForInstigator(FirstPawn, Spec.BuildClass, ObservedWalk));
    TestTrue(TEXT("Other player's Extend survives"), Subsystem->PeekExtendCommitForInstigator(SecondPawn, Spec.BuildClass, ObservedExtend));
    TestTrue(TEXT("Other player's Walk survives"), Subsystem->PeekWalkCommitForInstigator(SecondPawn, Spec.BuildClass, ObservedWalk));
    Subsystem->StageExtendCommitForPlayer(First, Extend);
    Subsystem->StageWalkCommitForPlayer(First, Walk);
    TestFalse(TEXT("Late Extend cannot bypass opt-out"), Subsystem->PeekExtendCommitForInstigator(FirstPawn, Spec.BuildClass, ObservedExtend));
    TestFalse(TEXT("Late Walk cannot bypass opt-out"), Subsystem->PeekWalkCommitForInstigator(FirstPawn, Spec.BuildClass, ObservedWalk));
    Subsystem->StageScalingSpecForPlayer(First, Spec);
    TestFalse(TEXT("Late staging cannot bypass opt-out"), Subsystem->PeekScalingSpecForInstigator(FirstPawn, Spec.BuildClass, Observed));
    Subsystem->SetSmartEnabledForPlayer(First, true);
    TestFalse(TEXT("Re-enable does not resurrect old plan"), Subsystem->PeekScalingSpecForInstigator(FirstPawn, Spec.BuildClass, Observed));
    Subsystem->StageScalingSpecForPlayer(First, Spec);
    Subsystem->SetSmartEnabledForPlayer(First, true);
    TestTrue(TEXT("Repeated enabled-state synchronization preserves fresh plan"), Subsystem->PeekScalingSpecForInstigator(FirstPawn, Spec.BuildClass, Observed));
    TestTrue(TEXT("Fresh plan works after re-enable"), Subsystem->ConsumeScalingSpecForInstigator(FirstPawn, Spec.BuildClass, Observed));
    TestTrue(TEXT("Per-player authority state does not switch local session"), Subsystem->IsSmartEnabledForSession());
    World->DestroyWorld(false);
    return true;
}
#endif
