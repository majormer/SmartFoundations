#include "Shared/Conduits/ConduitPreviewHelper.h"
#include "Engine/World.h"
#include "Hologram/FGSplineHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Subsystem/SFHologramDataService.h"
#include "Components/SplineComponent.h"
#include "SmartFoundations.h"

FConduitPreviewHelper::FConduitPreviewHelper(UWorld* InWorld, int32 InTier, AFGHologram* InParent)
	: World(InWorld)
	, ParentHologram(InParent)
	, Tier(InTier)
{
	// Note: Cannot call GetConnectorType() here (pure virtual, not yet implemented in derived class)
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("FConduitPreviewHelper created: Tier=%d Parent=%s"),
		Tier, *GetNameSafe(InParent));
}

FConduitPreviewHelper::~FConduitPreviewHelper()
{
	DestroyPreview();
}

void FConduitPreviewHelper::EnsureSpawned(const FVector& SpawnLocation)
{
	if (!World.IsValid() || !ParentHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("Cannot spawn %s hologram - invalid world or parent"),
			*GetConnectorType());
		return;
	}

	if (Hologram.IsValid())
	{
		return; // Already spawned
	}

	// Get subsystem for build class retrieval
	USFSubsystem* Subsystem = USFSubsystem::Get(World.Get());
	if (!Subsystem)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Cannot spawn %s - no subsystem"), *GetConnectorType());
		return;
	}

	// Get hologram class from derived type
	TSubclassOf<AFGSplineHologram> HologramClass = GetHologramClass();
	if (!HologramClass)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Cannot spawn %s - no hologram class"), *GetConnectorType());
		return;
	}

	// Spawn with deferred construction
	FActorSpawnParameters SpawnParams;
	SpawnParams.bDeferConstruction = true;
	SpawnParams.Owner = ParentHologram->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AFGSplineHologram* SpawnedHologram = World->SpawnActor<AFGSplineHologram>(
		HologramClass,
		SpawnLocation,
		FRotator::ZeroRotator,
		SpawnParams);

	if (!SpawnedHologram)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Failed to spawn %s hologram actor"), *GetConnectorType());
		return;
	}

	// Configure hologram (type-specific)
	ConfigureHologram(SpawnedHologram, Subsystem);

	// Mark as child via data service
	USFHologramDataService::DisableValidation(SpawnedHologram);
	USFHologramDataService::MarkAsChild(SpawnedHologram, ParentHologram.Get(), ESFChildHologramType::AutoConnect);

	// Finish spawning
	SpawnedHologram->FinishSpawning(FTransform(SpawnLocation));

	// Setup spline routing (type-specific)
	SetupSplineRouting(SpawnedHologram);

	// Add as child to parent for vanilla cost aggregation
	static int32 ChildCounter = 0;
	const FName ChildName(*FString::Printf(TEXT("AutoConnect%s_%d"), *GetConnectorType(), ChildCounter++));
	ParentHologram->AddChild(SpawnedHologram, ChildName);

	// Configure visibility and tick
	ConfigureVisibility(SpawnedHologram);
	ConfigureTickBehavior(SpawnedHologram);

	// Finalize spawn (optional type-specific setup)
	FinalizeSpawn(SpawnedHologram);

	Hologram = SpawnedHologram;

	UE_LOG(LogSmartFoundations, Log, TEXT("✅ %s hologram spawned: %s (Tier %d)"),
		*GetConnectorType(), *SpawnedHologram->GetName(), Tier);
}

void FConduitPreviewHelper::ConfigureTickBehavior(AFGSplineHologram* SpawnedHologram)
{
	if (ShouldEnableTick())
	{
		SpawnedHologram->SetActorTickEnabled(true);
		SpawnedHologram->PrimaryActorTick.bCanEverTick = true;
		SpawnedHologram->PrimaryActorTick.SetTickFunctionEnable(true);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Tick enabled for %s hologram"), *GetConnectorType());
	}
}

void FConduitPreviewHelper::ConfigureVisibility(AFGSplineHologram* SpawnedHologram)
{
	SpawnedHologram->SetActorHiddenInGame(false);
	SpawnedHologram->SetActorEnableCollision(false);
	SpawnedHologram->SetReplicates(false);

	if (USceneComponent* Root = SpawnedHologram->GetRootComponent())
	{
		Root->SetMobility(EComponentMobility::Movable);
	}
}

void FConduitPreviewHelper::HidePreview()
{
	if (Hologram.IsValid())
	{
		Hologram->SetActorHiddenInGame(true);
	}
}

void FConduitPreviewHelper::DestroyPreview()
{
	if (Hologram.IsValid())
	{
		QueueForDestruction(Hologram.Get());
		Hologram.Reset();
	}
}

void FConduitPreviewHelper::QueueForDestruction(AFGSplineHologram* HologramToDestroy)
{
	if (!HologramToDestroy || !World.IsValid())
	{
		return;
	}

	// Use centralized helper service for safe queued destruction
	if (USFSubsystem* Subsystem = USFSubsystem::Get(World.Get()))
	{
		if (FSFHologramHelperService* Helper = Subsystem->GetHologramHelper())
		{
			Helper->QueueChildForDestroy(HologramToDestroy);
			return;
		}
	}

	// Fallback to immediate destruction
	HologramToDestroy->Destroy();
}

bool FConduitPreviewHelper::IsPreviewVisible() const
{
	return Hologram.IsValid() && !Hologram->IsHidden();
}

bool FConduitPreviewHelper::IsPreviewValid() const
{
	return Hologram.IsValid() && World.IsValid();
}

float FConduitPreviewHelper::GetLength() const
{
	if (!Hologram.IsValid())
	{
		return 0.0f;
	}

	if (USplineComponent* Spline = Hologram->FindComponentByClass<USplineComponent>())
	{
		return Spline->GetSplineLength();
	}

	return 0.0f;
}

TArray<FItemAmount> FConduitPreviewHelper::GetPreviewCost() const
{
	TArray<FItemAmount> Cost;

	if (!Hologram.IsValid() || !World.IsValid())
	{
		return Cost;
	}

	// Delegate to hologram's GetCost (which uses recipe and length)
	// This ensures consistency with vanilla cost calculation
	Cost = Hologram->GetCost(false);

	return Cost;
}
