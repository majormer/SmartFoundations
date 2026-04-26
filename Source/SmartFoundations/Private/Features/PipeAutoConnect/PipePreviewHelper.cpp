#include "Features/PipeAutoConnect/PipePreviewHelper.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramDataService.h"
#include "Data/SFHologramData.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Components/SplineComponent.h"
#include "DrawDebugHelpers.h"
#include "FGPlayerController.h"
#include "FGRecipe.h"

DEFINE_LOG_CATEGORY_STATIC(LogPipePreview, Log, All);

namespace
{
int32 ResolvePipeTier(int32 ConfigTier, USFSubsystem* Subsystem, UWorld* World)
{
	int32 ActualTier = ConfigTier;
	if (ActualTier == 0)
	{
		AFGPlayerController* PlayerController = World ? Cast<AFGPlayerController>(World->GetFirstPlayerController()) : nullptr;
		ActualTier = Subsystem ? Subsystem->GetHighestUnlockedPipeTier(PlayerController) : 1;
		if (ActualTier == 0)
		{
			ActualTier = 1;
		}
	}

	return FMath::Clamp(ActualTier, 1, 2);
}
}

FPipePreviewHelper::FPipePreviewHelper(UWorld* InWorld, int32 PipeTier, bool bWithIndicator, AFGHologram* ParentJunction)
	: Super(InWorld, FMath::Clamp(PipeTier, 0, 2), ParentJunction)
	, bPipeWithIndicator(bWithIndicator)
{
	const TCHAR* TierDisplay = (GetTier() == 0) ? TEXT("Auto") : *FString::Printf(TEXT("Mk%d"), GetTier());
	UE_LOG(LogPipePreview, VeryVerbose, TEXT("FPipePreviewHelper created with %s pipe tier (%s)"),
		TierDisplay, bPipeWithIndicator ? TEXT("Normal") : TEXT("Clean"));
}

void FPipePreviewHelper::UpdatePipeTier(int32 NewTier, bool bWithIndicator)
{
	int32 ClampedTier = FMath::Clamp(NewTier, 0, 2);
	if (ClampedTier != GetTier() || bWithIndicator != bPipeWithIndicator)
	{
		Tier = ClampedTier;
		bPipeWithIndicator = bWithIndicator;
		const TCHAR* TierDisplay = (GetTier() == 0) ? TEXT("Auto") : *FString::Printf(TEXT("Mk%d"), GetTier());
		UE_LOG(LogPipePreview, VeryVerbose, TEXT("Pipe tier/style updated to %s (%s) - destroying existing hologram"),
			TierDisplay, bPipeWithIndicator ? TEXT("Normal") : TEXT("Clean"));

		DestroyPreview();
	}
}

bool FPipePreviewHelper::ShouldEnableTick() const
{
	// Pipes rely on lock state management; no tick needed for origin correction
	return false;
}

FString FPipePreviewHelper::GetConnectorType() const
{
	return TEXT("Pipe");
}

TSubclassOf<AFGSplineHologram> FPipePreviewHelper::GetHologramClass() const
{
	return ASFPipelineHologram::StaticClass();
}

TSubclassOf<AFGBuildable> FPipePreviewHelper::GetBuildClass(USFSubsystem* Subsystem) const
{
	if (!Subsystem)
	{
		return nullptr;
	}

	AFGPlayerController* PlayerController = World.IsValid() ? Cast<AFGPlayerController>(World->GetFirstPlayerController()) : nullptr;
	return Subsystem->GetPipeClassFromConfig(GetTier(), bPipeWithIndicator, PlayerController);
}

void FPipePreviewHelper::ConfigureHologram(AFGSplineHologram* SpawnedHologram, USFSubsystem* Subsystem)
{
	ASFPipelineHologram* PipeHologram = Cast<ASFPipelineHologram>(SpawnedHologram);
	if (!PipeHologram || !Subsystem)
	{
		return;
	}

	UClass* PipeBuildClass = GetBuildClass(Subsystem);
	if (!PipeBuildClass)
	{
		UE_LOG(LogPipePreview, Error, TEXT("Failed to get pipe class for tier=%d (%s)"),
			GetTier(), bPipeWithIndicator ? TEXT("Normal") : TEXT("Clean"));
		return;
	}

	PipeHologram->SetBuildClass(PipeBuildClass);
	PipeHologram->Tags.AddUnique(FName(TEXT("SF_PipeAutoConnectChild")));

	// Set recipe for cost aggregation (tier must be resolved from Auto if configured)
	int32 ActualTier = ResolvePipeTier(GetTier(), Subsystem, World.Get());
	TSubclassOf<UFGRecipe> PipeRecipe = Subsystem->GetPipeRecipeForTier(ActualTier, bPipeWithIndicator);
	if (PipeRecipe)
	{
		PipeHologram->SetRecipe(PipeRecipe);
	}
	else
	{
		UE_LOG(LogPipePreview, Warning, TEXT("No recipe found for Mk%d pipe (%s)"),
			ActualTier, bPipeWithIndicator ? TEXT("Normal") : TEXT("Clean"));
	}
}

void FPipePreviewHelper::SetupSplineRouting(AFGSplineHologram* SpawnedHologram)
{
	ASFPipelineHologram* PipeHologram = Cast<ASFPipelineHologram>(SpawnedHologram);
	if (!PipeHologram)
	{
		return;
	}

	UFGPipeConnectionComponent* Start = GetStartConnector();
	UFGPipeConnectionComponent* End = GetEndConnector();
	if (!Start || !End)
	{
		return;
	}

	PipeHologram->SetSnappedConnections(Start, End);

	FVector StartPos = Start->GetConnectorLocation();
	FVector EndPos = End->GetConnectorLocation();
	FVector StartNormal = Start->GetConnectorNormal();
	FVector EndNormal = End->GetConnectorNormal();

	if (USFSubsystem* Subsystem = USFSubsystem::Get(World.Get()))
	{
		const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
		PipeHologram->SetRoutingMode(Settings.PipeRoutingMode);
	}

	if (!PipeHologram->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		PipeHologram->SetupPipeSpline(Start, End);
	}

	PipeHologram->TriggerMeshGeneration();

	if (FSFHologramData* Data = USFHologramDataService::GetOrCreateData(PipeHologram))
	{
		Data->bIsPipeAutoConnectChild = true;
		Data->PipeAutoConnectConn0 = Start;
		Data->PipeAutoConnectConn1 = End;
		Data->bIsPipeManifold = false;
	}
}

FVector FPipePreviewHelper::GetConnectorLocation(UFGPipeConnectionComponent* Connector) const
{
	return Connector ? Connector->GetConnectorLocation() : FVector::ZeroVector;
}

void FPipePreviewHelper::UpdateSplineEndpoints(UFGPipeConnectionComponent* Start, UFGPipeConnectionComponent* End)
{
	ASFPipelineHologram* PipeHologram = GetTypedHologram();
	if (!PipeHologram || !Start || !End)
	{
		return;
	}

	UE_LOG(LogPipePreview, VeryVerbose, TEXT("Updating pipe spline endpoints"));

	const FVector StartLoc = Start->GetConnectorLocation();
	const FVector EndLoc = End->GetConnectorLocation();
	const FVector StartNormal = Start->GetConnectorNormal();
	const FVector EndNormal = End->GetConnectorNormal();

	const float Distance = FVector::Dist(StartLoc, EndLoc);
	const FVector Direction = (EndLoc - StartLoc).GetSafeNormal();

	if (UWorld* LocalWorld = World.Get())
	{
		DrawDebugLine(LocalWorld, StartLoc, EndLoc, FColor::Cyan, false, 0.1f, 0, 8.0f);
	}

	PipeHologram->SetActorLocation(StartLoc);

	USplineComponent* SplineComp = PipeHologram->FindComponentByClass<USplineComponent>();
	if (!SplineComp)
	{
		UE_LOG(LogPipePreview, Warning, TEXT("No spline component found on pipe hologram"));
		return;
	}

	SplineComp->SetWorldLocation(StartLoc);
	SplineComp->ClearSplinePoints(false);

	const float SmallTangent = 50.0f;
	const float LargeTangent = Distance * 0.435f;
	const float FlatSectionLength = Distance * 0.047f;
	const float TransitionOffset = Distance * 0.070f;

	SplineComp->AddSplinePoint(StartLoc, ESplineCoordinateSpace::World, false);
	SplineComp->SetTangentsAtSplinePoint(0, StartNormal * SmallTangent, StartNormal * SmallTangent, ESplineCoordinateSpace::World, false);

	FVector FlatStart = StartLoc + StartNormal * FlatSectionLength;
	SplineComp->AddSplinePoint(FlatStart, ESplineCoordinateSpace::World, false);
	SplineComp->SetTangentsAtSplinePoint(1, StartNormal * SmallTangent, StartNormal * (SmallTangent * 0.99f), ESplineCoordinateSpace::World, false);

	FVector TransitionStart = StartLoc + StartNormal * TransitionOffset;
	SplineComp->AddSplinePoint(TransitionStart, ESplineCoordinateSpace::World, false);
	SplineComp->SetTangentsAtSplinePoint(2, StartNormal * SmallTangent, Direction * LargeTangent, ESplineCoordinateSpace::World, false);

	FVector TransitionEnd = EndLoc + EndNormal * TransitionOffset;
	SplineComp->AddSplinePoint(TransitionEnd, ESplineCoordinateSpace::World, false);
	SplineComp->SetTangentsAtSplinePoint(3, Direction * LargeTangent, -EndNormal * SmallTangent, ESplineCoordinateSpace::World, false);

	FVector FlatEnd = EndLoc + EndNormal * FlatSectionLength;
	SplineComp->AddSplinePoint(FlatEnd, ESplineCoordinateSpace::World, false);
	SplineComp->SetTangentsAtSplinePoint(4, -EndNormal * (SmallTangent * 0.99f), -EndNormal * SmallTangent, ESplineCoordinateSpace::World, false);

	SplineComp->AddSplinePoint(EndLoc, ESplineCoordinateSpace::World, false);
	SplineComp->SetTangentsAtSplinePoint(5, -EndNormal * SmallTangent, -EndNormal * SmallTangent, ESplineCoordinateSpace::World, false);

	SplineComp->UpdateSpline();

	UE_LOG(LogPipePreview, VeryVerbose, TEXT("Pipe spline updated: Distance=%.1fcm, Points=%d, Length=%.1fcm"),
		Distance, SplineComp->GetNumberOfSplinePoints(), SplineComp->GetSplineLength());
}
