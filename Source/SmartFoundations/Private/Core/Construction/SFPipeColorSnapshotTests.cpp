// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Core/Construction/SFPipeColorSnapshot.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFPipeColorSnapshotTest, "SmartFoundations.Construction.PipeColor.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSFPipeColorSnapshotTest::RunTest(const FString& Parameters)
{
	FFactoryCustomizationData Original;
	Original.SwatchDesc = UFGFactoryCustomizationDescriptor_Swatch::StaticClass();
	Original.OverrideColorData.PrimaryColor = FLinearColor(0.125f, 0.25f, 0.5f);
	Original.OverrideColorData.SecondaryColor = FLinearColor(0.8f, 0.6f, 0.4f);
	Original.OverrideColorData.PaintFinish = UFGFactoryCustomizationDescriptor_PaintFinish::StaticClass();
	Original.PatternDesc = UFGFactoryCustomizationDescriptor_Pattern::StaticClass();
	Original.MaterialDesc = UFGFactoryCustomizationDescriptor_Material::StaticClass();
	Original.Data.Add(123.0f);
	auto Snapshot = FSFPipeColorSnapshot::Capture(Original);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FObjectAndNameAsStringProxyArchive Save(Writer, false);
	FSFPipeColorSnapshot::StaticStruct()->SerializeItem(Save, &Snapshot, nullptr);
	FSFPipeColorSnapshot Received;
	FMemoryReader Reader(Bytes);
	FObjectAndNameAsStringProxyArchive Load(Reader, true);
	FSFPipeColorSnapshot::StaticStruct()->SerializeItem(Load, &Received, nullptr);
	const auto Restored = Received.ToCustomization();
	TestTrue(TEXT("Paint intent survives reflected plan serialization"), Received.bCaptured);
	TestTrue(TEXT("Swatch preserved"), Restored.SwatchDesc == Original.SwatchDesc);
	TestTrue(TEXT("Both custom colors and finish preserved"), Restored.OverrideColorData == Original.OverrideColorData);
	TestNull(TEXT("Do not copy unrelated material"), Restored.MaterialDesc.Get());
	TestNull(TEXT("Do not copy paid pattern"), Restored.PatternDesc.Get());
	TestEqual(TEXT("No runtime shader state copied"), Restored.Data.Num(), 0);
	Original.OverrideColorData.PrimaryColor.R = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Invalid primary paint is not captured"), FSFPipeColorSnapshot::Capture(Original).bCaptured);
	Original.OverrideColorData.PrimaryColor.R = 0.5f;
	Original.OverrideColorData.SecondaryColor.A = std::numeric_limits<float>::infinity();
	TestFalse(TEXT("Invalid secondary paint is not captured"), FSFPipeColorSnapshot::Capture(Original).bCaptured);
	TestNull(TEXT("Uncaptured paint does not carry an unrelated swatch"), FSFPipeColorSnapshot::Capture(Original).ToCustomization().SwatchDesc.Get());
	return true;
}
#endif
