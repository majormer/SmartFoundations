#include "Services/SFGridTransformService.h"
#include "Subsystem/SFSubsystem.h"
#include "Services/SFGridSpawnerService.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"
#include "FGHologram.h"
#include "SmartFoundations.h"

void USFGridTransformService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

bool USFGridTransformService::DetectAndPropagateTransformChange(AFGHologram* CurrentHologram)
{
	if (!Subsystem.IsValid() || !CurrentHologram)
	{
		return false;
	}

	USFSubsystem* SS = Subsystem.Get();
	const FTransform CurrentTransform = CurrentHologram->GetActorTransform();
	
	// Check for transform change (tolerance: 0.01 units)
	if (CurrentTransform.Equals(LastKnownTransform, 0.01f))
	{
		return false; // No change
	}

	// Cache new transform
	LastKnownTransform = CurrentTransform;

	// Trigger child updates via spawner service
	if (USFGridSpawnerService* Spawner = SS->GetGridSpawnerService())
	{
		Spawner->UpdateChildrenForCurrentTransform();
	}

	// Update auto-connect belt previews if this is a distributor
	if (USFAutoConnectService* AutoConnect = SS->GetAutoConnectService())
	{
		if (AutoConnect->IsDistributorHologram(CurrentHologram))
		{
			SS->OnDistributorHologramUpdated(CurrentHologram);
		}
	}
	
	// Update pipe previews if this is a junction
	FString ClassName = CurrentHologram->GetClass()->GetName();
	if (ClassName.Contains(TEXT("PipelineJunction")))
	{
		// Use orchestrator for pipe junction updates (replaces legacy OnPipeJunctionHologramUpdated)
		if (USFAutoConnectOrchestrator* Orchestrator = SS->GetOrCreateOrchestrator(CurrentHologram))
		{
			Orchestrator->OnPipeJunctionsMoved();
		}
	}

	return true; // Transform changed
}
