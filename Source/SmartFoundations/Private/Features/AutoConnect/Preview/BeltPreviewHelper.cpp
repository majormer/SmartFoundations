#include "Features/AutoConnect/Preview/BeltPreviewHelper.h"
#include "Engine/World.h"
#include "Components/SplineMeshComponent.h"
#include "FGRecipe.h"
#include "Subsystem/SFSubsystem.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "SmartFoundations.h"

FBeltPreviewHelper::FBeltPreviewHelper(UWorld* InWorld, int32 BeltTier, AFGHologram* ParentHolo)
	: Super(InWorld, FMath::Clamp(BeltTier, 1, 6), ParentHolo)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("FBeltPreviewHelper created with Mk%d belt tier"), GetTier());
}

// ========================================
// Virtual Hook Implementations
// ========================================

bool FBeltPreviewHelper::ShouldEnableTick() const
{
	// Belts need tick enabled for origin-snap detection and correction
	return true;
}

FString FBeltPreviewHelper::GetConnectorType() const
{
	return TEXT("Belt");
}

TSubclassOf<AFGSplineHologram> FBeltPreviewHelper::GetHologramClass() const
{
	return ASFConveyorBeltHologram::StaticClass();
}

TSubclassOf<AFGBuildable> FBeltPreviewHelper::GetBuildClass(USFSubsystem* Subsystem) const
{
	if (!Subsystem)
	{
		return nullptr;
	}

	return Subsystem->GetBeltClassForTier(Tier);
}

void FBeltPreviewHelper::ConfigureHologram(AFGSplineHologram* SpawnedHologram, USFSubsystem* Subsystem)
{
	ASFConveyorBeltHologram* BeltHologram = Cast<ASFConveyorBeltHologram>(SpawnedHologram);
	if (!BeltHologram || !Subsystem)
	{
		return;
	}

	// Get belt buildable class
	TSubclassOf<AFGBuildable> BeltBuildClass = GetBuildClass(Subsystem);
	if (!BeltBuildClass)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("Failed to get Mk%d belt buildable class"), Tier);
		return;
	}

	// Set build class
	BeltHologram->SetBuildClass(BeltBuildClass);

	// Get and set recipe for cost aggregation
	TSubclassOf<UFGRecipe> BeltRecipe = Subsystem->GetBeltRecipeForTier(Tier);
	if (BeltRecipe)
	{
		BeltHologram->SetRecipe(BeltRecipe);
		UE_LOG(LogSmartFoundations, Log, TEXT("✅ Set recipe on belt child for Mk%d"), Tier);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚠️ No recipe found for Mk%d belt - cost aggregation may fail"), Tier);
	}

	// Tag as auto-connect child
	BeltHologram->Tags.AddUnique(FName(TEXT("SF_BeltAutoConnectChild")));
}

void FBeltPreviewHelper::SetupSplineRouting(AFGSplineHologram* SpawnedHologram)
{
	ASFConveyorBeltHologram* BeltHologram = Cast<ASFConveyorBeltHologram>(SpawnedHologram);
	if (!BeltHologram)
	{
		return;
	}

	UFGFactoryConnectionComponent* Output = GetStartConnector();
	UFGFactoryConnectionComponent* Input = GetEndConnector();

	if (!Output || !Input)
	{
		return;
	}

	// Set snapped connections to prevent child pole spawning
	BeltHologram->SetSnappedConnections(Output, Input);

	// Get connector positions and normals for spline routing
	FVector StartPos = Output->GetComponentLocation();
	FVector EndPos = Input->GetComponentLocation();
	FVector StartNormal = Output->GetConnectorNormal();
	FVector EndNormal = Input->GetConnectorNormal();

	// Set routing mode from settings
	if (USFSubsystem* Subsystem = USFSubsystem::Get(World.Get()))
	{
		const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
		BeltHologram->SetRoutingMode(Settings.BeltRoutingMode);
	}

	// Try using build mode for automatic spline routing, fallback to custom routing
	if (!BeltHologram->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		// Build mode not available, use custom routing
		BeltHologram->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
	}
}

void FBeltPreviewHelper::FinalizeSpawn(AFGSplineHologram* SpawnedHologram)
{
	ASFConveyorBeltHologram* BeltHologram = Cast<ASFConveyorBeltHologram>(SpawnedHologram);
	if (!BeltHologram)
	{
		return;
	}

	// Configure visibility and collision
	BeltHologram->SetActorHiddenInGame(false);
	BeltHologram->SetActorEnableCollision(false);
	BeltHologram->RegisterAllComponents();
	BeltHologram->SetPlacementMaterialState(EHologramMaterialState::HMS_OK);

	// Generate mesh AFTER AddChild (which was already called by base class)
	BeltHologram->TriggerMeshGeneration();
	BeltHologram->ForceApplyHologramMaterial();

	// Force render state on spline meshes to ensure immediate visibility
	TArray<USplineMeshComponent*> SplineMeshes;
	BeltHologram->GetComponents<USplineMeshComponent>(SplineMeshes);
	for (USplineMeshComponent* SplineMesh : SplineMeshes)
	{
		if (SplineMesh)
		{
			SplineMesh->SetVisibility(true, true);
			SplineMesh->MarkRenderStateDirty();
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("✅ Belt hologram finalized: %s (Mk%d)"),
		*BeltHologram->GetName(), Tier);
}

void FBeltPreviewHelper::UpdateSplineEndpoints(UFGFactoryConnectionComponent* Output, UFGFactoryConnectionComponent* Input)
{
	ASFConveyorBeltHologram* BeltHologram = GetTypedHologram();
	if (!BeltHologram || !Output || !Input)
	{
		return;
	}

	// Get connector positions and normals
	FVector StartPos = Output->GetComponentLocation();
	FVector EndPos = Input->GetComponentLocation();
	FVector StartNormal = Output->GetConnectorNormal();
	FVector EndNormal = Input->GetConnectorNormal();

	// Set routing mode from settings
	if (USFSubsystem* Subsystem = USFSubsystem::Get(World.Get()))
	{
		const auto& Settings = Subsystem->GetAutoConnectRuntimeSettings();
		BeltHologram->SetRoutingMode(Settings.BeltRoutingMode);
	}

	// Update spline routing
	if (!BeltHologram->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		BeltHologram->AutoRouteSplineWithNormals(StartPos, StartNormal, EndPos, EndNormal);
	}

	// Force mesh update
	BeltHologram->TriggerMeshGeneration();
}
