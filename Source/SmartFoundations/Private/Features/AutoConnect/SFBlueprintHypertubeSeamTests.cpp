// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/SFBlueprintSeamService.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Hologram/FGBlueprintHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeConnectionComponentHyper.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	struct FSeamFixture
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::EditorPreview, false);
		AFGBlueprintHologram* Blueprint = World->SpawnActor<AFGBlueprintHologram>();
		FMapProperty* MapProperty = FindFProperty<FMapProperty>(AFGBlueprintHologram::StaticClass(), TEXT("mDuplicateConnectionToOriginalMap"));

		FSeamFixture()
		{
			if (!Blueprint->GetRootComponent())
			{
				USceneComponent* Root = NewObject<USceneComponent>(Blueprint, TEXT("FixtureRoot"));
				Blueprint->AddInstanceComponent(Root);
				Blueprint->SetRootComponent(Root);
				Root->RegisterComponent();
			}
		}
		~FSeamFixture() { World->DestroyWorld(false); }

		template<typename T>
		T* AddPort(const FName OriginalName, const FVector& Position, const FVector& Normal, bool bMapped = true,
			EPipeConnectionType ConnectionType = EPipeConnectionType::PCT_ANY)
		{
			AActor* Owner = World->SpawnActor<AActor>();
			T* Original = NewObject<T>(Owner, OriginalName);
			Original->SetPipeConnectionType(ConnectionType);
			Owner->AddInstanceComponent(Original);
			Owner->SetRootComponent(Original);
			Original->RegisterComponent();
			const FName DupName(*FString::Printf(TEXT("%s_%s"), *Owner->GetName(), *OriginalName.ToString()));
			T* Dup = NewObject<T>(Blueprint, DupName);
			Blueprint->AddInstanceComponent(Dup);
			Dup->SetupAttachment(Blueprint->GetRootComponent());
			Dup->SetRelativeLocationAndRotation(Position, Normal.Rotation());
			Dup->RegisterComponent();
			if (bMapped && MapProperty)
			{
				using FNativeSeamDuplicateMap = TMap<TObjectPtr<UFGConnectionComponent>, TObjectPtr<UFGConnectionComponent>>;
				MapProperty->ContainerPtrToValuePtr<FNativeSeamDuplicateMap>(Blueprint)->Add(Dup, Original);
			}
			return Dup;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintHypertubeKindsTest, "SmartFoundations.AutoConnect.BlueprintHypertubePortKinds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFBlueprintHypertubeKindsTest::RunTest(const FString& Parameters)
{
	FSeamFixture Fixture;
	if (!TestNotNull(TEXT("Native duplicate map"), Fixture.MapProperty)) return false;
	Fixture.AddPort<UFGPipeConnectionComponent>(TEXT("Fluid0"), FVector(1000, 0, 0), FVector::ForwardVector);
	Fixture.AddPort<UFGPipeConnectionComponent>(TEXT("Fluid1"), FVector(-1000, 0, 0), -FVector::ForwardVector);
	Fixture.AddPort<UFGPipeConnectionComponentHyper>(TEXT("PipeHyperConnection0"), FVector(1000, 0, 0), FVector::ForwardVector);
	Fixture.AddPort<UFGPipeConnectionComponentHyper>(TEXT("PipeHyperConnection1"), FVector(-1000, 0, 0), -FVector::ForwardVector);
	Fixture.AddPort<UFGPipeConnectionComponentHyper>(TEXT("SnapOnly0"), FVector(1000, 0, 100), FVector::ForwardVector, true, EPipeConnectionType::PCT_SNAP_ONLY);
	Fixture.AddPort<UFGPipeConnectionComponentHyper>(TEXT("SnapOnly0"), FVector(-1000, 0, 100), -FVector::ForwardVector, true, EPipeConnectionType::PCT_SNAP_ONLY);
	Fixture.AddPort<UFGPipeConnectionComponentHyper>(TEXT("Snap Only"), FVector(1000, 0, 200), FVector::ForwardVector, true, EPipeConnectionType::PCT_SNAP_ONLY);
	Fixture.AddPort<UFGPipeConnectionComponentHyper>(TEXT("Snap Only"), FVector(-1000, 0, 200), -FVector::ForwardVector, true, EPipeConnectionType::PCT_SNAP_ONLY);
	Fixture.AddPort<UFGPipeConnectionComponentHyper>(TEXT("Unmapped"), FVector(1000, 0, 200), FVector::ForwardVector, false);
	FSFBlueprintSeamTable Table;
	TestTrue(TEXT("Mixed native populations compute"), FSFBlueprintSeamService::BuildSeamTable(Fixture.Blueprint, Table));
	TestEqual(TEXT("Fluid enumeration remains isolated"), Table.PipeConnectorCount, 2);
	TestEqual(TEXT("Only mapped Hyper components get identity"), Table.HypertubeConnectorCount, 6);
	TestEqual(TEXT("One fluid seam and one Hyper seam; no support seam"), Table.Pairs.Num(), 2);
	int32 HyperPairs = 0;
	for (const FSFBlueprintSeamPair& Pair : Table.Pairs)
	{
		if (!Pair.bIsHypertube) continue;
		HyperPairs++;
		TestTrue(TEXT("Hyper uses conduit axes"), Pair.bIsPipe);
		TestNotNull(TEXT("Typed identity resolves"), FSFBlueprintSeamService::ResolveHypertubeConnector(Fixture.Blueprint, Pair.FromIndex, Pair.FromOriginalName));
		TestNull(TEXT("Mismatched original name rejects safely"), FSFBlueprintSeamService::ResolveHypertubeConnector(Fixture.Blueprint, Pair.FromIndex, TEXT("WrongPort")));
	}
	TestEqual(TEXT("Distinct Hyper pair"), HyperPairs, 1);
	TestNull(TEXT("Invalid index rejects safely"), FSFBlueprintSeamService::ResolveHypertubeConnector(Fixture.Blueprint, 99, TEXT("PipeHyperConnection0")));
	TestFalse(TEXT("Null parent cannot retain cached support"), FSFBlueprintSeamService::BuildSeamTable(nullptr, Table));
	TestEqual(TEXT("Null parent clears typed population"), Table.HypertubeConnectorCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFBlueprintHypertubeTransformsTest, "SmartFoundations.AutoConnect.BlueprintHypertubeTransforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFBlueprintHypertubeTransformsTest::RunTest(const FString& Parameters)
{
	FSeamFixture Fixture;
	if (!TestNotNull(TEXT("Native duplicate map"), Fixture.MapProperty)) return false;
	const FVector Axes[] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Fixture.AddPort<UFGPipeConnectionComponentHyper>(FName(*FString::Printf(TEXT("Hyper%dPositive"), Axis)), Axes[Axis] * 1000, Axes[Axis]);
		Fixture.AddPort<UFGPipeConnectionComponentHyper>(FName(*FString::Printf(TEXT("Hyper%dNegative"), Axis)), Axes[Axis] * -1000, -Axes[Axis]);
	}
	FSFBlueprintSeamTable Before, After;
	FSFBlueprintSeamService::BuildSeamTable(Fixture.Blueprint, Before);
	Fixture.Blueprint->SetActorTransform(FTransform(FRotator(0, 137, 0), FVector(10000, -9000, 7500)));
	FSFBlueprintSeamService::BuildSeamTable(Fixture.Blueprint, After);
	TestEqual(TEXT("XYZ native endpoint seams"), Before.Pairs.Num(), 3);
	TestEqual(TEXT("Rotated translated parent preserves pair count"), After.Pairs.Num(), Before.Pairs.Num());
	for (const FSFBlueprintSeamPair& Pair : Before.Pairs)
	{
		const FSFBlueprintSeamPair* Match = After.Pairs.FindByPredicate([&Pair](const FSFBlueprintSeamPair& Other)
		{
			return Other.Axis == Pair.Axis && Other.bIsHypertube && Other.FromIndex == Pair.FromIndex && Other.ToIndex == Pair.ToIndex;
		});
		TestNotNull(TEXT("Local pair identity survives native parent transform"), Match);
		TestNotNull(TEXT("Transformed endpoint resolves without proximity reassignment"), FSFBlueprintSeamService::ResolveHypertubeConnector(Fixture.Blueprint, Pair.FromIndex, Pair.FromOriginalName));
	}
	// Native SDK IsConnected is a stub: openness and network operation require Shipping acceptance.
	return true;
}
#endif
