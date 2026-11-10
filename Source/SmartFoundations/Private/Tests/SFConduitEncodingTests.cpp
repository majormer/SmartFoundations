// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFScalingSpec.h"
#include "Features/Scaling/Net/SFConduitSchema.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HAL/IConsoleManager.h"
#include "Engine/PackageMapClient.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/CoreNet.h"

namespace
{
TArray<uint8> Canonical(FSFConduitPlanEntry& Entry)
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes);
    FObjectAndNameAsStringProxyArchive Ar(Writer, false);
    Ar.SetWantBinaryPropertySerialization(true);
    FSFConduitPlanEntry::StaticStruct()->SerializeBin(Ar, &Entry);
    return Bytes;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConduitSchemaIdentityTest,
    "SmartFoundations.Network.ConduitSchemaIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConduitSchemaIdentityTest::RunTest(const FString& Parameters)
{
    using SFConduitEncoding::HashSchemaToken;
    const auto Fingerprint = [](const TArray<FString>& Tokens)
    {
        uint32 Hash = 0;
        for (const FString& Token : Tokens) Hash = HashSchemaToken(Token, Hash);
        return Hash;
    };
    // Actual Shipping-process spellings captured from the failing Windows/Linux pair.
    const TArray<FString> Client = { TEXT("recipe"), TEXT("TSubclassOf<UFGRecipe>"), TEXT("1"), TEXT("amount"), TEXT("int32"), TEXT("1") };
    const TArray<FString> Server = { TEXT("Recipe"), TEXT("TSubclassOf<UFGRecipe>"), TEXT("1"), TEXT("Amount"), TEXT("int32"), TEXT("1") };
    TestEqual(TEXT("Equivalent reflected identity is independent of process name casing"), Fingerprint(Client), Fingerprint(Server));
    TestEqual(TEXT("Reflected names inside CPP types are normalized"), HashSchemaToken(TEXT("TSubclassOf<UFGRecipe>"), 0), HashSchemaToken(TEXT("TSubclassOf<UFGRECIPE>"), 0));
    for (const auto& Change : TArray<TPair<int32, FString>>{ {0, TEXT("OtherRecipe")}, {4, TEXT("float")}, {5, TEXT("2")} })
    {
        auto Changed = Server;
        Changed[Change.Key] = Change.Value;
        TestNotEqual(TEXT("Different field, type or dimension still changes schema"), Fingerprint(Client), Fingerprint(Changed));
    }
    auto Reordered = Server;
    Reordered.Swap(0, 3);
    TestNotEqual(TEXT("Field order remains significant"), Fingerprint(Client), Fingerprint(Reordered));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFConduitEncodingTest,
    "SmartFoundations.Network.CompactConduitLossless", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFConduitEncodingTest::RunTest(const FString& Parameters)
{
    // Truncation is deliberate. Expect the bounded-reader error without firing an
    // engine ensure; restore the caller's setting when this test finishes.
    auto* EnsureOnOverflow = IConsoleManager::Get().FindConsoleVariable(TEXT("net.BitReader.EnsureOnOverflow"));
    const int32 PreviousEnsure = EnsureOnOverflow->GetInt();
    EnsureOnOverflow->Set(0, ECVF_SetByCode);
    ON_SCOPE_EXIT { EnsureOnOverflow->Set(PreviousEnsure, ECVF_SetByCode); };
    AddExpectedError(TEXT("FBitReader::SetOverflowed"), EAutomationExpectedErrorFlags::Contains, 5);
    UPackageMapClient* Map = NewObject<UPackageMapClient>();
    for (int32 Kind = 0; Kind <= static_cast<int32>(ESFConduitPlanKind::Wire); ++Kind)
    {
        FSFConduitPlanEntry Source;
        Source.Kind = static_cast<ESFConduitPlanKind>(Kind);
        Source.Location = FVector(240800.00048828125, -0.0, 1000.0170016304633);
        Source.Rotation = FRotator(1.234567891, 67.0000000001, -15.000000001);
        Source.SplinePoints.Emplace(FVector(1e-12, -127.234567891, 450.123456789), FVector(0, 1, 2), FVector(3, 4, 5));
        Source.SplinePoints.Emplace(FVector(900, 420, -1e-12), FVector(400, 600, 800), FVector(-400, -600, -800));
        Source.bExactConveyorEndpoints = true;
        Source.ConveyorFrom.bPresent = true;
        Source.ConveyorFrom.PortName = TEXT("Output1");
        Source.ConveyorFrom.Direction = 1;
        Source.ConveyorFrom.OwnerPose = FTransform(Source.Rotation, Source.Location, FVector(1, 1, 1));
        Source.ConveyorFrom.PortPose = FTransform(FQuat::Identity, FVector(1e-12, 100, -0.0));
        Source.ConveyorFrom.OriginalOwnerPose = FTransform(FRotator(0, 90, 0), FVector(77, 88, 99));
        Source.ConveyorTo = Source.ConveyorFrom;
        Source.ConveyorTo.PortName = TEXT("Input1");
        Source.ConveyorTo.bBlueprintMember = true;
        Source.ConveyorTo.CopyCell = FIntVector(-3, 4, 2);
        Source.Cost.SetNum(2); Source.Cost[0].Amount = 3; Source.Cost[1].Amount = 100000;
        Source.bExactPowerEndpoints = true;
        Source.PowerStart.ComponentName = TEXT("PowerConnection1");
        Source.PowerStart.OwnerLocation = Source.Location;
        Source.WireStart = FVector(7, 8, 9); Source.WireEnd = FVector(9, 8, 7);
        Source.bFloorHolePipe = true;
        Source.PipeColor.bCaptured = true;
        Source.PipeColor.Colors.PrimaryColor = FLinearColor(.1234567f, .7654321f, .0000001f, .5f);
        Source.PipeColor.Colors.SecondaryColor = FLinearColor(.8f, .3f, .4f, .7f);
        Source.StackIndex = 123;
        FNetBitWriter Writer(Map, 0);
        bool Success = false;
        TestTrue(TEXT("All references mapped"), Source.NetSerialize(Writer, Map, Success));
        TestTrue(TEXT("Encoded"), Success && !Writer.IsError());
        FNetBitReader Reader(Map, Writer.GetData(), Writer.GetNumBits());
        FSFConduitPlanEntry Target;
        TestTrue(TEXT("Decoded mapping"), Target.NetSerialize(Reader, Map, Success));
        TestTrue(TEXT("Decoded completely"), Success && !Reader.IsError() && Reader.GetPosBits() == Writer.GetNumBits());
        TestTrue(TEXT("All reflected values identical including numeric low bits"), Canonical(Source) == Canonical(Target));
        FSFConduitPlanEntry Empty;
        FNetBitWriter EmptyWriter(Map, 0);
        Empty.NetSerialize(EmptyWriter, Map, Success);
        FNetBitReader EmptyReader(Map, EmptyWriter.GetData(), EmptyWriter.GetNumBits());
        Target.NetSerialize(EmptyReader, Map, Success);
        TestTrue(TEXT("Omitted fields reset a reused destination"), Success && Canonical(Empty) == Canonical(Target));
        TestTrue(TEXT("Defaults are compact"), EmptyWriter.GetNumBits() < 128);
        // Reject truncated payload without publishing partially decoded fields.
        FNetBitReader Truncated(Map, Writer.GetData(), Writer.GetNumBits() - 9);
        const auto Before = Canonical(Target);
        Target.NetSerialize(Truncated, Map, Success);
        TestTrue(TEXT("Truncation refused"), !Success && Truncated.IsError());
        TestTrue(TEXT("Truncation leaves destination unchanged"), Before == Canonical(Target));
    }
    FSFConduitPlanEntry Oversized;
    Oversized.SplinePoints.SetNum(65);
    FNetBitWriter Writer(Map, 0); bool Success = true;
    Oversized.NetSerialize(Writer, Map, Success);
    TestTrue(TEXT("Oversized array refused"), !Success && Writer.IsError());
    uint8 WrongVersion = 255;
    uint32 Schema = 0;
    FNetBitWriter Bad(Map, 0); Bad << WrongVersion << Schema;
    FNetBitReader BadReader(Map, Bad.GetData(), Bad.GetNumBits());
    FSFConduitPlanEntry Target;
    Target.NetSerialize(BadReader, Map, Success);
    TestTrue(TEXT("Unknown wire format refused"), !Success && BadReader.IsError());
    return true;
}
#endif
