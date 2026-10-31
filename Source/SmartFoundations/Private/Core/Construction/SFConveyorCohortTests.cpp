// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Core/Construction/SFConveyorCohort.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConveyorCohortStructureTest, "SmartFoundations.Construction.ConveyorCohort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFConveyorCohortStructureTest::RunTest(const FString& Parameters)
{
    using EStatus = ESFConveyorCohortStatus;
    TStrongObjectPtr<USceneComponent> Downstream(NewObject<USceneComponent>()), Upstream(NewObject<USceneComponent>());
    TStrongObjectPtr<USceneComponent> ChainOwner(NewObject<USceneComponent>()), Other(NewObject<USceneComponent>());
    const FObjectKey D(Downstream.Get()), U(Upstream.Get()), C(ChainOwner.Get()), X(Other.Get());
    FSFConveyorCohort Good;
    FSFConveyorCohort::FMember Head, Tail;
    Head.Identity = D; Head.Bucket = 7; Head.Chain = C; Head.Next = U; Head.SegmentIndex = 0;
    Head.InputNeighbor = U; Tail.OutputNeighbor = D;
    Tail.Identity = U; Tail.Bucket = 7; Tail.Chain = C; Tail.SegmentIndex = 1;
    Good.Members.Add(D, Head); Good.Members.Add(U, Tail);
    FSFConveyorCohort::FGroup Group;
    Group.Bucket = 7; Group.Chain = C; Group.Members = {D, U};
    Good.Groups.Add(Group);
    FSFConveyorCohort::FChain Chain;
    Chain.Identity = C; Chain.Segments = {D, U}; Chain.First = D; Chain.Last = U;
    Chain.NativeGroupOccurrences = 1;
    Good.Chains.Add(C, Chain);
    FString Reason;
    auto Status = [&Reason](const FSFConveyorCohort& Case) { return FSFConveyorCohortValidator::ValidateStructure(Case, Reason); };
    TestTrue(TEXT("Native segment identities and indices establish ownership without a renderer lookup map"), Status(Good) == EStatus::CoherentStructure);
    auto Case = Good;
    Case.Members.Remove(U);
    TestTrue(TEXT("Selected subset cannot stand in for full native ownership"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(U).Bucket = INDEX_NONE;
    TestTrue(TEXT("Unregistered differs from coherent registered member"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Groups[0].Members.Add(U);
    TestTrue(TEXT("Duplicate member is rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Groups.Add(Group);
    TestTrue(TEXT("Aliased native bucket is rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(U).Chain = X;
    TestTrue(TEXT("Alive actor with different chain is rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Swap(Case.Chains.FindChecked(C).Segments[0], Case.Chains.FindChecked(C).Segments[1]);
    TestTrue(TEXT("Physical travel order must not replace native traversal order"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Chains.FindChecked(C).Segments[1] = D;
    TestTrue(TEXT("Duplicate segment owner cannot replace complete native ownership"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(U).SegmentIndex = 0;
    TestTrue(TEXT("Replicated member index is part of ownership"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Chains.FindChecked(C).First = U;
    TestTrue(TEXT("Valid endpoint in wrong position is rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(D).Next.Reset();
    TestTrue(TEXT("Array membership without cached traversal is insufficient"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(D).InputNeighbor.Reset();
    TestTrue(TEXT("Reciprocal metadata cannot replace actual internal edge"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(U).OutputNeighbor = X;
    TestTrue(TEXT("Both physical ends must agree with cached traversal"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Chains.FindChecked(C).NativeGroupOccurrences = 2;
    TestTrue(TEXT("Chain duplicated outside captured connected closure is rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(U).Next = D;
    TestTrue(TEXT("An accelerated chain cannot retain a contradictory loop traversal"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Groups[0].bShared = true;
    TestTrue(TEXT("Shared scheduling affinity can coexist with coherent accelerated ownership"), Status(Case) == EStatus::CoherentStructure);
    Case.Members.FindChecked(D).Next.Reset();
    TestTrue(TEXT("A shared accelerated group still requires native chain traversal"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Groups[0].bShared = true;
    Case.Members.FindChecked(U).Next = D;
    TestTrue(TEXT("Shared scheduling does not permit an accelerated loop"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Groups[0].PendingOccurrences = 1;
    TestTrue(TEXT("Assigned and pending chain is contradictory"), Status(Case) == EStatus::Inconsistent);
    Case = Good;
    Case.Chains.Reset(); Case.Groups[0].Chain.Reset(); Case.Groups[0].PendingOccurrences = 1;
    for (auto& Pair : Case.Members) { Pair.Value.Chain.Reset(); Pair.Value.SegmentIndex = INDEX_NONE; }
    TestTrue(TEXT("Coherent native pending migration remains pending"), Status(Case) == EStatus::NativePending);
    Case.Members.FindChecked(D).SegmentIndex = 0;
    Case.Members.FindChecked(U).SegmentIndex = 1;
    TestTrue(TEXT("Native detached members may retain cached indices while migration remains pending"), Status(Case) == EStatus::NativePending);
    Case.Groups[0].PendingOccurrences = 2;
    TestTrue(TEXT("Duplicate pending migration is not merely pending"), Status(Case) == EStatus::Inconsistent);
    Case.Groups[0].PendingOccurrences = 0;
    TestTrue(TEXT("Unqueued local cargo is a valid native representation without synthetic migration"), Status(Case) == EStatus::CoherentStructure);
    Case.Members.FindChecked(U).Chain = C;
    TestTrue(TEXT("An obsolete index is harmless only without a contradictory chain association"), Status(Case) == EStatus::Inconsistent);
    Case.Members.FindChecked(U).Chain.Reset();
    Case.Members.FindChecked(U).Next = D;
    Case.Members.FindChecked(U).InputNeighbor = D;
    Case.Members.FindChecked(D).OutputNeighbor = U;
    TestTrue(TEXT("A fully reciprocal local loop preserves native local ownership"), Status(Case) == EStatus::CoherentStructure);
    Case.Members.FindChecked(U).InputNeighbor = X;
    TestTrue(TEXT("A local loop with a foreign closure is inconsistent"), Status(Case) == EStatus::Inconsistent);
    Case.Members.FindChecked(U).InputNeighbor = D;
    Case.Groups[0].bShared = true;
    Swap(Case.Groups[0].Members[0], Case.Groups[0].Members[1]);
    TestTrue(TEXT("Shared local scheduler order is not an accelerated chain traversal order"), Status(Case) == EStatus::CoherentStructure);
    FSFConveyorCohort Designer;
    Designer.Designer = X;
    Head = {}; Tail = {};
    Head.Identity = D; Tail.Identity = U;
    Head.Designer = X; Tail.Designer = X;
    Head.InputNeighbor = U; Tail.OutputNeighbor = D;
    Designer.Members.Add(D, Head); Designer.Members.Add(U, Tail);
    TestTrue(TEXT("Designer local reciprocal conveyors need no factory bucket"), Status(Designer) == EStatus::CoherentStructure);
    Case = Designer; Case.Designer.Reset();
    TestTrue(TEXT("Unregistered world conveyors are not admitted as templates"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Members.FindChecked(U).Designer = C;
    TestTrue(TEXT("Different Designer ownership is rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Members.FindChecked(U).Designer.Reset();
    TestTrue(TEXT("Mixed world and Designer members are rejected"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Groups.Add(Group);
    TestTrue(TEXT("Designer cannot carry a synthetic factory group"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Chains.Add(C, Chain);
    TestTrue(TEXT("Designer cannot carry an accelerated chain"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Members.FindChecked(U).Bucket = 7;
    TestTrue(TEXT("Designer cannot retain factory bucket membership"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Members.FindChecked(U).Chain = C;
    TestTrue(TEXT("Designer cannot retain a native chain backpointer"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Members.FindChecked(U).Next = D;
    TestTrue(TEXT("Designer cannot retain factory cached traversal"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Members.FindChecked(U).OutputNeighbor.Reset();
    TestTrue(TEXT("Designer physical edges must be reciprocal"), Status(Case) == EStatus::Inconsistent);
    Case = Designer; Case.Members.Remove(U);
    TestTrue(TEXT("Designer capture must include connected conveyor closure"), Status(Case) == EStatus::Inconsistent);
    Case = Good; Case.Members.FindChecked(U).Designer = X;
    TestTrue(TEXT("Ordinary groups cannot silently include Designer members"), Status(Case) == EStatus::Inconsistent);
    return true;
}
#endif
