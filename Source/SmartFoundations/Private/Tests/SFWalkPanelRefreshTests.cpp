// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFWalkPanelWidget.h"
#include "Components/SpinBox.h"
#include "Blueprint/WidgetTree.h"
#include "Features/Walk/SFWalkService.h"
#include "Subsystem/SFSubsystem.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Hologram/FGRailroadTrackHologram.h"
#include "Hologram/FGVehiclePathSegmentHologram.h"
#include "Components/SplineComponent.h"
#include "UObject/UnrealType.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkPanelRefreshTest,
    "SmartFoundations.Walking.PanelRefreshPreservesEdits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFWalkPanelRefreshTest::RunTest(const FString& Parameters)
{
    auto* Panel = NewObject<USFWalkPanelWidget>();
    Panel->bApplyImmediately = false;
    // Use real UMG/Slate controls: attaching a lazily built spin box synchronizes
    // its value and broadcasts OnValueChanged even though the player did not edit it.
    auto* Advance = Panel->MakeEditCell(0, 0, 43.f, 1.f, 100.f, 1.f);
    Advance->TakeWidget();
    TestTrue(TEXT("Initial Slate synchronization cannot stage an edit"), Panel->PendingEdits.IsEmpty());
    TestEqual(TEXT("Initial rail advance is retained"), Advance->GetValue(), 43.f);

    Panel->bSuppressLiveEdit = true;
    auto* Turn = Panel->MakeEditCell(1, 1, 90.f, -270.f, 270.f, 5.f);
    TestTrue(TEXT("Nested cell initialization preserves the table rebuild guard"), Panel->bSuppressLiveEdit);
    Turn->OnValueChanged.Broadcast(0.f);
    Panel->CellBindings.Last()->OnCommitted(0.f, ETextCommit::OnCleared);
    TestTrue(TEXT("Rebuild callbacks cannot stage straight/default route values"), Panel->PendingEdits.IsEmpty());
    Panel->bSuppressLiveEdit = false;

    Turn->SetValue(45.f);
    const float* Draft = Panel->PendingEdits.Find(FIntPoint(1, 1));
    TestTrue(TEXT("A real current-cell edit still stages its turn"), Draft && *Draft == 45.f);
    auto* Retired = Panel->CellBindings.Last().Get();
    Panel->CellBindings.Reset();
    Retired->OnChanged(0.f);
    Retired->OnCommitted(0.f, ETextCommit::OnUserMovedFocus);
    Draft = Panel->PendingEdits.Find(FIntPoint(1, 1));
    TestTrue(TEXT("Retired cells cannot overwrite retained edits after Apply rebuilds the table"), Draft && *Draft == 45.f);

    // Exercise actual Apply -> reroute -> Refresh, including native Slate construction,
    // for every shared Walking family. Native SDK routing bodies are stubs: this
    // verifies the authored model and controls, not Shipping spline construction.
    for (const auto Type : {ESFWalkConveyanceType::Belt, ESFWalkConveyanceType::Pipe,
        ESFWalkConveyanceType::Hypertube, ESFWalkConveyanceType::Railroad, ESFWalkConveyanceType::VehiclePath})
    {
        auto* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
        GEngine->CreateNewWorldContext(EWorldType::EditorPreview).SetCurrentWorld(World);
        auto* Subsystem = NewObject<USFSubsystem>(World);
        auto* Walk = NewObject<USFWalkService>(Subsystem);
        Subsystem->WalkService = Walk;
        Walk->bActive = true;
        Walk->ConveyanceType = Type;
        Walk->ActiveIndex = 7;
        for (int32 Index = 0; Index < 8; ++Index)
        {
            FSFWalkSegment Segment;
            Segment.Advance = 2400.f + Index * 100.f;
            Segment.TurnDegrees = Index % 2 ? 15.f : -10.f;
            Segment.Rise = Index * 50.f;
            Segment.Shift = -Index * 25.f;
            AFGHologram* NativeSpan = nullptr;
            if (Type == ESFWalkConveyanceType::Railroad)
            {
                auto* Track = World->SpawnActor<AFGRailroadTrackHologram>();
                auto* SplineProperty = FindFProperty<FObjectPropertyBase>(Track->GetClass(), TEXT("mSplineComponent"));
                SplineProperty->SetObjectPropertyValue_InContainer(Track, NewObject<USplineComponent>(Track));
                FindFProperty<FFloatProperty>(Track->GetClass(), TEXT("mMaxLength"))->SetPropertyValue_InContainer(Track, 10000.f);
                NativeSpan = Track;
            }
            else if (Type == ESFWalkConveyanceType::VehiclePath)
            {
                auto* Path = World->SpawnActor<AFGVehiclePathSegmentHologram>();
                FindFProperty<FFloatProperty>(Path->GetClass(), TEXT("mMaxPathLength"))->SetPropertyValue_InContainer(Path, 10000.f);
                NativeSpan = Path;
            }
            if (NativeSpan)
            {
                Segment.Spans.Add(NativeSpan);
                if (Index == 0) Walk->SeedHologram = NativeSpan;
            }
            Walk->Segments.Add(Segment);
        }
        auto* RoutePanel = NewObject<USFWalkPanelWidget>(World);
        RoutePanel->CachedSubsystem = Subsystem;
        RoutePanel->WidgetTree = NewObject<UWidgetTree>(RoutePanel);
        RoutePanel->SegmentListBox = NewObject<UVerticalBox>(RoutePanel);
        RoutePanel->WidgetTree->RootWidget = RoutePanel->SegmentListBox;
        const auto Before = Walk->GetSegmentViews();
        for (int32 Pass = 0; Pass < 2; ++Pass)
        {
            RoutePanel->OnApply();
            RoutePanel->SegmentListBox->TakeWidget();
            const auto After = Walk->GetSegmentViews();
            TestEqual(TEXT("Apply retains all eight segments"), After.Num(), Before.Num());
            TestEqual(TEXT("Apply rebuilds four editors per segment"), RoutePanel->CellBindings.Num(), 32);
            for (int32 Index = 0; Index < Before.Num(); ++Index)
            {
                TestEqual(TEXT("Apply retains authored advance"), After[Index].Advance, Before[Index].Advance);
                TestEqual(TEXT("Apply retains authored turn"), After[Index].TurnDegrees, Before[Index].TurnDegrees);
                TestEqual(TEXT("Apply retains authored rise"), After[Index].Rise, Before[Index].Rise);
                TestEqual(TEXT("Apply retains authored shift"), After[Index].Shift, Before[Index].Shift);
                TestEqual(TEXT("Apply retains derived exit heading"), After[Index].ExitHeadingDeg, Before[Index].ExitHeadingDeg);
            }
        }
        RoutePanel->bApplyImmediately = false;
        RoutePanel->ApplyCellEditLive(3, 1, 40.f);
        RoutePanel->OnApply();
        TestEqual(TEXT("Staged Apply commits the chosen turn"), Walk->GetSegmentViews()[3].TurnDegrees, 40.f);
        TestTrue(TEXT("Staged Apply consumes pending edits"), RoutePanel->PendingEdits.IsEmpty());
        TestEqual(TEXT("Staged Apply preserves another segment"), Walk->GetSegmentViews()[4].TurnDegrees, Before[4].TurnDegrees);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    }
    return true;
}
#endif
