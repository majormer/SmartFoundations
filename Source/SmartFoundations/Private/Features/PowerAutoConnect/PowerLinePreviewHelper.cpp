#include "Features/PowerAutoConnect/PowerLinePreviewHelper.h"
#include "Holograms/Power/SFWireHologram.h"
#include "Hologram/FGHologram.h"
#include "FGPowerConnectionComponent.h"
#include "Engine/World.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
#include "SmartFoundations.h"

static int32 GSFPowerWireChildCounter = 0;

FPowerLinePreviewHelper::FPowerLinePreviewHelper(UWorld* InWorld, AFGHologram* ParentPole)
	: World(InWorld)
	, ParentPole(ParentPole)
{
}

FPowerLinePreviewHelper::~FPowerLinePreviewHelper()
{
	DestroyPreview();
}

bool FPowerLinePreviewHelper::UpdatePreview(UFGPowerConnectionComponent* InStartConnection, UFGPowerConnectionComponent* InEndConnection)
{
	if (!InStartConnection || !InEndConnection)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ UpdatePreview: Invalid connections - start=%s, end=%s"),
			InStartConnection ? *InStartConnection->GetFullName() : TEXT("null"),
			InEndConnection ? *InEndConnection->GetFullName() : TEXT("null"));
		return false;
	}

	// Check maximum distance (100m = 10,000cm)
	FVector StartPos = InStartConnection->GetComponentLocation();
	FVector EndPos = InEndConnection->GetComponentLocation();
	float Distance = FVector::Dist(StartPos, EndPos);
	
	if (Distance > MAX_CONNECTION_DISTANCE)
	{
		return false;
	}

	// Store connections
	this->StartConnection = InStartConnection;
	this->EndConnection = InEndConnection;

	// Ensure hologram is spawned at the start connector position (matches belt/pipe previews)
	EnsureSpawned(StartPos, InStartConnection, InEndConnection);

	if (!PowerLineHologram.IsValid())
	{
		return false;
	}

	// Update endpoints
	UpdateLineEndpoints(InStartConnection, InEndConnection);

	return true;
}

void FPowerLinePreviewHelper::EnsureSpawned(const FVector& SpawnLocation, UFGPowerConnectionComponent* InStart, UFGPowerConnectionComponent* InEnd)
{
	// If hologram already exists and is valid, no need to respawn
	if (PowerLineHologram.IsValid())
	{
		return;
	}

	if (!World.IsValid())
	{
		return;
	}

	// Issue #248: Check if parent pole has vanilla WIRE children (vanilla "insert into wire" mode)
	// When vanilla is inserting a pole into an existing wire, it creates 2 wire child holograms.
	// If we add more wire children, vanilla's AFGPowerPoleHologram::Construct() asserts out_children.Num() == 2.
	// Skip adding Smart wire children only if parent has vanilla WIRE children (not pole children from scaling).
	if (ParentPole.IsValid())
	{
		const TArray<AFGHologram*>& ExistingChildren = ParentPole->GetHologramChildren();
		for (AFGHologram* Child : ExistingChildren)
		{
			if (!Child) continue;
			
			// Only skip if this is a vanilla WIRE hologram (not a Smart wire, not a pole)
			// Smart wire children have our tag, vanilla wire children don't
			FString ChildClassName = Child->GetClass()->GetName();
			bool bIsWireHologram = ChildClassName.Contains(TEXT("PowerLine")) || ChildClassName.Contains(TEXT("Wire"));
			bool bIsSmartChild = Child->Tags.Contains(FName(TEXT("SF_PowerAutoConnectChild")));
			
			if (bIsWireHologram && !bIsSmartChild)
			{
				// Vanilla wire child detected - pole is in "insert into existing wire" mode
				UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EnsureSpawned: Skipped - parent pole has vanilla wire child: %s (class: %s)"), 
					*Child->GetName(), *ChildClassName);
				return;
			}
		}
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EnsureSpawned: Creating SFWireHologram at %s"), *SpawnLocation.ToString());

	// Spawn our custom Smart wire hologram with deferred construction
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;
	
	ASFWireHologram* NewHologram = World->SpawnActor<ASFWireHologram>(
		ASFWireHologram::StaticClass(), 
		SpawnLocation, 
		FRotator::ZeroRotator, 
		SpawnParams);
	
	if (NewHologram)
	{
		// Set build class BEFORE finishing construction
		UClass* PowerLineClass = LoadObject<UClass>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
		if (PowerLineClass)
		{
			NewHologram->SetBuildClass(PowerLineClass);
		}
		
		// Set the wire recipe so vanilla can aggregate costs for children
		// This is critical for proper cost aggregation - matches stackable belt/pipe pattern
		TSubclassOf<UFGRecipe> WireRecipe = LoadClass<UFGRecipe>(nullptr, TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PowerLine.Recipe_PowerLine_C"));
		if (WireRecipe)
		{
			NewHologram->SetRecipe(WireRecipe);
		}
		
		// Finish spawning
		NewHologram->FinishSpawning(FTransform(FRotator::ZeroRotator, SpawnLocation), true);

		// Attach as real engine child hologram for vanilla cost aggregation and construction
		if (ParentPole.IsValid())
		{
			const FName ChildName(*FString::Printf(TEXT("SF_PowerWireChild_%d"), GSFPowerWireChildCounter++));
			NewHologram->Tags.AddUnique(FName(TEXT("SF_PowerAutoConnectChild")));
			ParentPole->AddChild(Cast<AFGHologram>(NewHologram), ChildName);
			
			// CRITICAL: Match parent's lock state after adding as child (from pipe/belt pattern)
			bool bParentLocked = ParentPole->IsHologramLocked();
			if (bParentLocked)
			{
				NewHologram->LockHologramPosition(true);
			}
		}
		
		// Setup the wire with proper catenary rendering
		if (InStart && InEnd)
		{
			NewHologram->SetupWirePreview(InStart, InEnd);
		}
		
		// Trigger mesh generation to ensure visual
		NewHologram->TriggerMeshGeneration();
		
		// Force visibility
		NewHologram->SetActorHiddenInGame(false);
		NewHologram->ForceVisibilityUpdate();
		
		// Disable tick to prevent position fighting
		NewHologram->SetActorTickEnabled(false);
		
		PowerLineHologram = NewHologram;
		
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ EnsureSpawned: SUCCESS - Created wire hologram %s (parentLocked=%d)"), 
			*NewHologram->GetName(), ParentPole.IsValid() && ParentPole->IsHologramLocked() ? 1 : 0);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("⚡ EnsureSpawned: FAILED to spawn SFWireHologram"));
	}
}

void FPowerLinePreviewHelper::UpdateLineEndpoints(UFGPowerConnectionComponent* InStart, UFGPowerConnectionComponent* InEnd)
{
	if (!PowerLineHologram.IsValid() || !InStart || !InEnd)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ UpdateLineEndpoints: Invalid state - hologram=%s, start=%s, end=%s"),
			PowerLineHologram.IsValid() ? *PowerLineHologram->GetName() : TEXT("null"),
			InStart ? *InStart->GetFullName() : TEXT("null"),
			InEnd ? *InEnd->GetFullName() : TEXT("null"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ UpdateLineEndpoints: Updating wire %s: %s → %s"),
		*PowerLineHologram->GetName(),
		*InStart->GetName(), *InEnd->GetName());

	// CRITICAL: Temporarily unlock child before updating (from pipe/belt pattern)
	// Parent lock blocks child transform updates - must unlock, update, then re-lock
	bool bParentLocked = ParentPole.IsValid() && ParentPole->IsHologramLocked();
	bool bChildWasLocked = PowerLineHologram->IsHologramLocked();
	if (bChildWasLocked)
	{
		PowerLineHologram->LockHologramPosition(false);  // Unlock for update
	}

	// Use SFWireHologram's proper setup method which handles catenary and materials
	PowerLineHologram->SetupWirePreview(InStart, InEnd);
	
	// Trigger mesh generation to ensure visual update
	PowerLineHologram->TriggerMeshGeneration();
	
	// CRITICAL: Re-lock child if parent is locked (from pipe/belt pattern)
	// Children must match parent's lock state for visibility
	if (bParentLocked)
	{
		PowerLineHologram->LockHologramPosition(true);
	}
	
	// Force visibility update and ensure not hidden
	PowerLineHologram->SetActorHiddenInGame(false);
	PowerLineHologram->ForceVisibilityUpdate();
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ UpdateLineEndpoints: Updated wire (parentLocked=%d, childRelocked=%d)"),
		bParentLocked ? 1 : 0, bParentLocked ? 1 : 0);
}

void FPowerLinePreviewHelper::HidePreview()
{
	if (PowerLineHologram.IsValid())
	{
		PowerLineHologram->SetActorHiddenInGame(true);
	}
}

void FPowerLinePreviewHelper::DestroyPreview()
{
	if (PowerLineHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("⚡ DestroyPreview: Destroying wire hologram %s"),
			*PowerLineHologram->GetName());

		// Use centralized queued destruction (same pattern as FConduitPreviewHelper)
		QueueForDestruction(PowerLineHologram.Get());
		PowerLineHologram.Reset();
	}

	StartConnection.Reset();
	EndConnection.Reset();
}

void FPowerLinePreviewHelper::QueueForDestruction(AActor* ActorToDestroy)
{
	if (!ActorToDestroy || !World.IsValid())
	{
		return;
	}

	// Use centralized helper service for safe queued destruction (hologram path)
	if (AFGHologram* HologramToDestroy = Cast<AFGHologram>(ActorToDestroy))
	{
		if (USFSubsystem* Subsystem = USFSubsystem::Get(World.Get()))
		{
			if (FSFHologramHelperService* Helper = Subsystem->GetHologramHelper())
			{
				Helper->QueueChildForDestroy(HologramToDestroy);
				return;
			}
		}
	}

	// Fallback to immediate destruction
	ActorToDestroy->Destroy();
}

bool FPowerLinePreviewHelper::IsPreviewVisible() const
{
	return PowerLineHologram.IsValid() && !PowerLineHologram->IsHidden();
}

bool FPowerLinePreviewHelper::IsPreviewValid() const
{
	return PowerLineHologram.IsValid() && 
	       StartConnection.IsValid() && 
	       EndConnection.IsValid();
}

float FPowerLinePreviewHelper::GetLineLength() const
{
	if (!StartConnection.IsValid() || !EndConnection.IsValid())
	{
		return 0.0f;
	}

	FVector StartPos = StartConnection->GetComponentLocation();
	FVector EndPos = EndConnection->GetComponentLocation();
	return FVector::Dist(StartPos, EndPos);
}

TArray<FItemAmount> FPowerLinePreviewHelper::GetPreviewCost() const
{
	TArray<FItemAmount> Cost;
	
	float Distance = GetLineLength();
	if (Distance <= 0.0f)
	{
		return Cost;
	}

	int32 CablesNeeded = CalculateCableCost(Distance);
	
	// Resolve Cable item descriptor class
	TSubclassOf<UFGItemDescriptor> CableClass = LoadClass<UFGItemDescriptor>(
		nullptr,
		TEXT("/Game/FactoryGame/Resource/Parts/Cable/Desc_Cable.Desc_Cable_C"));
	if (!*CableClass)
	{
		return Cost;
	}
	
	FItemAmount CableCost;
	CableCost.ItemClass = CableClass;
	CableCost.Amount = CablesNeeded;
	Cost.Add(CableCost);
	
	return Cost;
}

int32 FPowerLinePreviewHelper::CalculateCableCost(float DistanceInCm)
{
	// Convert cm to meters
	float DistanceInMeters = DistanceInCm / 100.0f;
	
	// 1 Cable per 25 meters, rounded up
	int32 CablesNeeded = FMath::CeilToInt(DistanceInMeters / 25.0f);
	
	return FMath::Max(1, CablesNeeded); // Minimum 1 cable
}
