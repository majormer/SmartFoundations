// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Walk/SFWalkEndpointGrid.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFWalkEndpointGridTest,
    "SmartFoundations.Walking.ExistingEndpointGrid",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFWalkEndpointGridTest::RunTest(const FString& Parameters)
{
    const FVector Forward = FVector(1, 1, 0).GetSafeNormal();
    const FVector Lateral = FVector(-1, 1, 0).GetSafeNormal();
    const FVector Vertical = FVector::UpVector;
    const FVector Origin(11000, -23000, 120200);
    TArray<FSFWalkEndpointSample> Samples;
    // Deliberately shuffled, manual geometry; no groups, actor order or Walking metadata.
    for (int32 Identity : {8, 1, 5, 0, 7, 3, 2, 6, 4})
    {
        FSFWalkEndpointSample Sample;
        Sample.Identity = Identity;
        Sample.Position = Origin + Lateral * (300.0 * (Identity / 3)) + Vertical * (400.0 * (Identity % 3));
        Sample.Outward = Forward;
        Sample.bReverseFlow = Identity % 2 != 0;
        Samples.Add(Sample);
    }
    auto Grid = SFWalkEndpointGrid::Resolve(Samples, 4, Lateral, Vertical);
    TestTrue(TEXT("Interior selection resolves complete rotated 3x3"), Grid.Result == ESFWalkEndpointGridResult::Ready);
    TestEqual(TEXT("Three lanes"), Grid.Lanes, 3);
    TestEqual(TEXT("Three stacks"), Grid.Stacks, 3);
    TestEqual(TEXT("Selected interior lane retained"), Grid.SelectedCell, 4);
    TestTrue(TEXT("Actual lateral pitch, not default pitch"), FMath::IsNearlyEqual(Grid.LaneSpacing, 300.0f));
    TestTrue(TEXT("Actual vertical pitch"), FMath::IsNearlyEqual(Grid.StackSpacing, 400.0f));
    TestTrue(TEXT("Corner recovered independently of selected cell"), Grid.Origin.Equals(Origin, 0.01));
    for (int32 Cell = 0; Cell < Grid.Identities.Num(); ++Cell)
    {
        TestEqual(TEXT("Stable connector identity"), Grid.Identities[Cell], Cell);
        TestEqual(TEXT("Per-lane mixed direction retained"), Grid.ReverseFlow[Cell], Cell % 2 != 0);
    }
    Samples.Swap(0, 7);
    const auto Reordered = SFWalkEndpointGrid::Resolve(Samples, 4, Lateral, Vertical);
    TestTrue(TEXT("Enumeration order cannot change mapping"), Grid.Identities == Reordered.Identities);

    auto Invalid = Samples;
    Invalid.RemoveAt(0);
    TestTrue(TEXT("Missing corner cannot silently become smaller bus"),
        SFWalkEndpointGrid::Resolve(Invalid, 4, Lateral, Vertical).Result == ESFWalkEndpointGridResult::IncompleteRectangle);
    Invalid = Samples; Invalid[0].bOccupied = true;
    TestTrue(TEXT("Occupied lane blocks full cross-section"),
        SFWalkEndpointGrid::Resolve(Invalid, 4, Lateral, Vertical).Result == ESFWalkEndpointGridResult::IncompleteRectangle);
    Invalid = Samples; Invalid[0].bEligible = false;
    TestTrue(TEXT("Excluded support is not included through nearby geometry"),
        SFWalkEndpointGrid::Resolve(Invalid, 4, Lateral, Vertical).Result == ESFWalkEndpointGridResult::IncompleteRectangle);
    Invalid = Samples; Invalid[0].Position += Lateral * 30.0;
    TestTrue(TEXT("Irregular nearby row is ambiguous"),
        SFWalkEndpointGrid::Resolve(Invalid, 4, Lateral, Vertical).Result != ESFWalkEndpointGridResult::Ready);
    Invalid = Samples; Invalid.Add(Samples[0]);
    TestTrue(TEXT("Duplicate identity refused"),
        SFWalkEndpointGrid::Resolve(Invalid, 4, Lateral, Vertical).Result == ESFWalkEndpointGridResult::AmbiguousIdentity);
    Invalid = Samples; Invalid[0].Position = Invalid[1].Position;
    TestTrue(TEXT("Coincident distinct endpoints refused"),
        SFWalkEndpointGrid::Resolve(Invalid, 4, Lateral, Vertical).Result != ESFWalkEndpointGridResult::Ready);
    Invalid = Samples; Invalid[0].Position.X = std::numeric_limits<double>::quiet_NaN();
    TestTrue(TEXT("Nonfinite geometry refused"),
        SFWalkEndpointGrid::Resolve(Invalid, 4, Lateral, Vertical).Result == ESFWalkEndpointGridResult::AmbiguousGeometry);
    TestTrue(TEXT("Skew frame refused"),
        SFWalkEndpointGrid::Resolve(Samples, 4, Forward, Vertical).Result == ESFWalkEndpointGridResult::InvalidFrame);
    TestTrue(TEXT("Unknown selection refused"),
        SFWalkEndpointGrid::Resolve(Samples, 99, Lateral, Vertical).Result == ESFWalkEndpointGridResult::InvalidSelection);
    // Backward start from three added lanes beside four already connected originals.
    TArray<FSFWalkEndpointSample> Expanded;
    for (int32 Lane = 0; Lane < 7; ++Lane)
    {
        FSFWalkEndpointSample Sample;
        Sample.Identity = Lane; Sample.Position = Origin + Lateral * (200.0 * Lane);
        Sample.Outward = -Forward; Sample.bOccupied = Lane < 4;
        Sample.bReverseFlow = true; // input ends: preserve flow toward the existing run
        Expanded.Add(Sample);
    }
    auto Reverse = SFWalkEndpointGrid::Resolve(Expanded, 5, Lateral, Vertical);
    TestTrue(TEXT("Free added starting ends resume beside occupied originals"), Reverse.Result == ESFWalkEndpointGridResult::Ready);
    TestEqual(TEXT("Only three added lanes resume backward"), Reverse.Lanes, 3);
    TestTrue(TEXT("Connected originals excluded"), Reverse.Identities == TArray<int32>({4, 5, 6}));
    TestTrue(TEXT("Backward belt flow retained"), Reverse.ReverseFlow == TArray<bool>({true, true, true}));
    Expanded[5].bOccupied = true;
    TestTrue(TEXT("Interior occupied hole still fails"),
        SFWalkEndpointGrid::Resolve(Expanded, 4, Lateral, Vertical).Result == ESFWalkEndpointGridResult::IncompleteRectangle);
    Expanded[5].bOccupied = false;
    Expanded[0].Position = Expanded[4].Position;
    TestTrue(TEXT("Coincident occupied endpoint remains ambiguous"),
        SFWalkEndpointGrid::Resolve(Expanded, 4, Lateral, Vertical).Result == ESFWalkEndpointGridResult::AmbiguousGeometry);
    const TArray<FSFWalkEndpointSample> Single{Samples[0]};
    Grid = SFWalkEndpointGrid::Resolve(Single, Single[0].Identity, Lateral, Vertical);
    TestTrue(TEXT("Single exposed endpoint accepted"), Grid.Result == ESFWalkEndpointGridResult::Ready);
    TestEqual(TEXT("Single lane"), Grid.Lanes, 1);
    TestEqual(TEXT("Single stack"), Grid.Stacks, 1);
    return true;
}
#endif
