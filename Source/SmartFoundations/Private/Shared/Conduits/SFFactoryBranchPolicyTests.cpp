// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Shared/Conduits/SFFactoryBranchPolicy.h"
#include "Shared/Conduits/SFDistributorTopology.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFFactoryBranchPolicyTest, "SmartFoundations.AutoConnect.FactoryBranchSides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFFactoryBranchPolicyTest::RunTest(const FString& Parameters)
{
    using namespace SFFactoryBranchPolicy;
    // Live report: both Manufacturers lie at +825Y while splitter branches face +/-X.
    const FVector A(-1500, 825, 0), B(1500, 825, 0), N(0, -1, 0);
    for (double Yaw : {0.0, 45.0, 90.0, 180.0})
    {
        const FQuat Rotation(FVector::UpVector, FMath::DegreesToRadians(Yaw));
        const auto R = [&](const FVector& V) { return Rotation.RotateVector(V); };
        TestFalse(TEXT("Reported sideways port pair is rejected under every world rotation"),
            FacesOpposite(R(FVector::ForwardVector), R(N), 80, true));
        TestFalse(TEXT("Different buildings in the same row cannot consume both branches"),
            OpposingBanks(FVector::ZeroVector, R(A), R(N), R(B), R(N), true));
        TestTrue(TEXT("Opposing rows remain supported under every world rotation"),
            OpposingBanks(FVector::ZeroVector, R(A), R(N), R(-A), R(-N), true));
    }
    TestFalse(TEXT("Unused Output2 facing away from the factory row is not a skipped candidate"),
        IsForwardCandidate(FVector(0, -1, 0), FVector(0, 1125, 0), true));
    TestTrue(TEXT("Output3 facing the factory row is a candidate"),
        IsForwardCandidate(FVector(0, 1, 0), FVector(0, 925, 0), true));
    TestTrue(TEXT("Sideways factory-port rejection still reports a forward candidate"),
        IsForwardCandidate(FVector(1, 0, 0), FVector(1500, 825, 0), true));
    TestTrue(TEXT("Facing pair accepted"), FacesOpposite(-N, N, 80, true));
    TestFalse(TEXT("Same-facing normals rejected"), FacesOpposite(N, N, 80, true));
    TestFalse(TEXT("Vertical stacking alone does not create a second belt bank"),
        OpposingBanks(FVector::ZeroVector, A, N, A + FVector(0, 0, 2000), N, true));
    TestFalse(TEXT("A port on the separating plane is ambiguous"),
        OpposingBanks(FVector::ZeroVector, A, N, FVector(1500, 0, 0), -N, true));
    TestTrue(TEXT("Vertical pipe branches retain native three-dimensional facing"),
        FacesOpposite(FVector::UpVector, -FVector::UpVector, 80, false));
    TestTrue(TEXT("Vertical opposing pipe destinations are distinct banks"),
        OpposingBanks(FVector::ZeroVector, FVector(0, 0, 1000), -FVector::UpVector,
            FVector(0, 0, -1000), FVector::UpVector, false));
    for (const TCHAR* Class : {TEXT("Build_ConveyorAttachmentSplitter_C"),
        TEXT("Build_ConveyorAttachmentSplitterSmart_C"), TEXT("Build_ConveyorAttachmentSplitterProgrammable_C"),
        TEXT("Build_ConveyorAttachmentMerger_C"), TEXT("Build_ConveyorAttachmentMergerPriority_C")})
    {
        const bool bMerger = FString(Class).Contains(TEXT("Merger"));
        const auto Topology = FSFDistributorTopologyResolver::Resolve(FString(Class), FName(bMerger ? TEXT("Input2") : TEXT("Output2")));
        TestTrue(TEXT("Required distributor variant has recognized branch roles"), Topology.bRecognized && Topology.bValidManifold);
    }
    TestTrue(TEXT("Cross has an opposing factory branch"),
        !FSFDistributorTopologyResolver::Resolve(FString(TEXT("Build_PipelineJunction_Cross_C")), FName(TEXT("Connection2"))).OppositeFactoryPort.IsNone());
    TestTrue(TEXT("T junction must not invent an opposing branch"),
        FSFDistributorTopologyResolver::Resolve(FString(TEXT("Build_PipelineJunction_T_C")), FName(TEXT("Connection2"))).OppositeFactoryPort.IsNone());
    return true;
}
#endif
