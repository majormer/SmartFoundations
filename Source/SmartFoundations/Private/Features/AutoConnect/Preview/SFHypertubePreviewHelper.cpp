// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Preview/SFHypertubePreviewHelper.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Data/SFHologramData.h"
#include "FGPlayerController.h"
#include "FGRecipe.h"

namespace
{
	TSubclassOf<UFGRecipe> NativeTubeRecipe()
	{
		return LoadClass<UFGRecipe>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipeHyper.Recipe_PipeHyper_C"));
	}
}

FSFHypertubePreviewHelper::FSFHypertubePreviewHelper(UWorld* InWorld, AFGHologram* Parent)
	: Super(InWorld, 0, Parent) {}

FString FSFHypertubePreviewHelper::GetConnectorType() const { return TEXT("Hypertube"); }
TSubclassOf<AFGSplineHologram> FSFHypertubePreviewHelper::GetHologramClass() const { return ASFPipelineHologram::StaticClass(); }

TSubclassOf<AFGBuildable> FSFHypertubePreviewHelper::GetBuildClass(USFSubsystem* Subsystem) const
{
	AFGPlayerController* Player = World.IsValid() ? Cast<AFGPlayerController>(World->GetFirstPlayerController()) : nullptr;
	return Subsystem ? Subsystem->GetHypertubeClassFromConfig(Player) : nullptr;
}

void FSFHypertubePreviewHelper::UpdatePreview(UFGPipeConnectionComponentHyper* Start, UFGPipeConnectionComponentHyper* End)
{
	if (!World.IsValid() || !GetBuildClass(USFSubsystem::Get(World.Get())) || !NativeTubeRecipe())
	{
		DestroyPreview();
		return;
	}
	Super::UpdatePreview(Start, End);
}

void FSFHypertubePreviewHelper::ConfigureHologram(AFGSplineHologram* SpawnedHologram, USFSubsystem* Subsystem)
{
	ASFPipelineHologram* Tube = Cast<ASFPipelineHologram>(SpawnedHologram);
	if (!Tube || !Subsystem) return;
	Tube->SetBuildClass(GetBuildClass(Subsystem));
	Tube->SetRecipe(NativeTubeRecipe());
	// [MP-SEAM] The existing staged StackablePipe plan carries native Hyper build
	// class, recipe and spline. Its Construct path uses AFGBuildablePipeBase and
	// compatible native network ports, without entering fluid-only network logic.
	Tube->Tags.AddUnique(TEXT("SF_StackableChild"));
}

void FSFHypertubePreviewHelper::SetupSplineRouting(AFGSplineHologram* SpawnedHologram)
{
	ASFPipelineHologram* Tube = Cast<ASFPipelineHologram>(SpawnedHologram);
	UFGPipeConnectionComponentHyper* Start = GetStartConnector();
	UFGPipeConnectionComponentHyper* End = GetEndConnector();
	USFSubsystem* Subsystem = World.IsValid() ? USFSubsystem::Get(World.Get()) : nullptr;
	if (!Tube || !Start || !End || !Subsystem) return;
	Tube->SetSnappedConnections(Start, End);
	Tube->SetActorLocation(Start->GetConnectorLocation());
	Tube->ApplyPipeBuildModeRouting(Subsystem->GetAutoConnectRuntimeSettings().HypertubeRoutingMode,
		Start->GetConnectorLocation(), Start->GetConnectorNormal(), End->GetConnectorLocation(), End->GetConnectorNormal());
	if (FSFHologramData* Data = USFHologramDataService::GetOrCreateData(Tube))
	{
		Data->bIsStackablePipe = true;
		Data->StackablePipeConn0 = Start;
		Data->StackablePipeConn1 = End;
		Data->StackablePipeIndex = 0;
	}
	Tube->TriggerMeshGeneration();
	Tube->ForceApplyHologramMaterial();
}
