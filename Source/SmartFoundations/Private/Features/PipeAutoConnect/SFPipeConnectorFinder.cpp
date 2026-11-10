// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Engine/OverlapResult.h"

#include "SmartFoundations.h"
#include "Buildables/FGBuildable.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGPipelineJunctionHologram.h"
#include "Hologram/FGPipeAttachmentHologram.h"
#include "FGPipeConnectionComponent.h"
#include "UObject/UnrealType.h"

bool FSFPipeConnectorFinder::IsPipelineJunctionHologram(AFGHologram* Hologram)
{
	if (!Hologram)
	{
		return false;
	}

	// Prefer class check over name matching where possible.
	if (Hologram->IsA(AFGPipelineJunctionHologram::StaticClass()))
	{
		return true;
	}

	// Fallback: defensive name-based check for modded/custom junctions.
	if (UClass* BuildClass = Hologram->GetBuildClass())
	{
		const FString ClassName = BuildClass->GetName();
		return ClassName.Contains(TEXT("PipelineJunction"));
	}

	return false;
}

void FSFPipeConnectorFinder::FindNearbyPipeBuildings(
	AFGHologram* JunctionHologram,
	float SearchRadius,
	TArray<AFGBuildable*>& OutBuildings)
{
	OutBuildings.Empty();

	if (!JunctionHologram)
	{
		return;
	}

	UWorld* World = JunctionHologram->GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = JunctionHologram->GetActorLocation();
	const float Radius = FMath::Max(0.0f, SearchRadius);

	// Simple sphere overlap; filtering to production/power/buffer types happens later.
	TArray<FOverlapResult> Overlaps;
	
	// CRITICAL FIX: Satisfactory buildings are on WorldStatic channel, not WorldDynamic
	// We need to query both channels to catch all buildings
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldStatic);
	ObjectParams.AddObjectTypesToQuery(ECollisionChannel::ECC_WorldDynamic);
	
	FCollisionShape Sphere = FCollisionShape::MakeSphere(Radius);

	if (!World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, ObjectParams, Sphere))
	{
		return;
	}

	for (const FOverlapResult& Result : Overlaps)
	{
		AFGBuildable* Buildable = Cast<AFGBuildable>(Result.GetActor());
		if (Buildable)
		{
			OutBuildings.AddUnique(Buildable);
		}
	}
}

void FSFPipeConnectorFinder::GetPipeConnectors(
	AFGBuildable* Building,
	TArray<UFGPipeConnectionComponent*>& OutConnectors)
{
	OutConnectors.Empty();

	if (!Building)
	{
		return;
	}

	TArray<UActorComponent*> Components;
	Building->GetComponents(UFGPipeConnectionComponent::StaticClass(), Components);

	for (UActorComponent* Comp : Components)
	{
		if (UFGPipeConnectionComponent* PipeConn = Cast<UFGPipeConnectionComponent>(Comp))
		{
			if (!PipeConn->IsConnected())
			{
				OutConnectors.Add(PipeConn);
			}
		}
	}
}

TArray<FName> FSFPipeConnectorFinder::GetNativeSnapReservations(AFGHologram* JunctionHologram)
{
	TArray<FName> Reserved;
	auto* Attachment = Cast<AFGPipeAttachmentHologram>(JunctionHologram);
	if (!Attachment) return Reserved;

	// Hologram ports are not physically connected yet. Native insertion will split
	// the existing pipeline and consume its named inline pair during construction.
	static const auto* Pipe = FindFProperty<FObjectPropertyBase>(AFGPipeAttachmentHologram::StaticClass(), TEXT("mSnappedPipe"));
	static const auto* Endpoint = FindFProperty<FObjectPropertyBase>(AFGPipeAttachmentHologram::StaticClass(), TEXT("mSnappedConnectionComponent"));
	if (Pipe && IsValid(Pipe->GetObjectPropertyValue_InContainer(Attachment)))
	{
		Reserved.Add(TEXT("Connection0"));
		Reserved.Add(TEXT("Connection1"));
	}
	else if (Endpoint && IsValid(Endpoint->GetObjectPropertyValue_InContainer(Attachment)))
	{
		// This index belongs to the native cached connection array, NOT GetComponents
		// enumeration. Resolve it to the actual component's stable name.
		static const auto* Index = FindFProperty<FIntProperty>(AFGPipeAttachmentHologram::StaticClass(), TEXT("mSnapConnectionIndex"));
		static const auto* Ports = FindFProperty<FArrayProperty>(AFGPipeAttachmentHologram::StaticClass(), TEXT("mPipeConnectionComponents"));
		if (Index && Ports)
		{
			FScriptArrayHelper Array(Ports, Ports->ContainerPtrToValuePtr<void>(Attachment));
			const int32 Selected = Index->GetPropertyValue_InContainer(Attachment);
			const auto* Inner = CastField<FObjectPropertyBase>(Ports->Inner);
			if (Inner && Array.IsValidIndex(Selected))
			{
				if (const auto* Port = Cast<UFGPipeConnectionComponentBase>(Inner->GetObjectPropertyValue(Array.GetRawPtr(Selected))))
					Reserved.Add(Port->GetFName());
			}
		}
	}
	return Reserved;
}

void FSFPipeConnectorFinder::GetJunctionConnectors(
	AFGHologram* JunctionHologram,
	TArray<UFGPipeConnectionComponent*>& OutConnectors)
{
	OutConnectors.Empty();

	if (!JunctionHologram)
	{
		return;
	}

	TArray<UActorComponent*> Components;
	JunctionHologram->GetComponents(UFGPipeConnectionComponent::StaticClass(), Components);
	const TArray<FName> Reserved = GetNativeSnapReservations(JunctionHologram);

	for (UActorComponent* Comp : Components)
	{
		if (UFGPipeConnectionComponent* PipeConn = Cast<UFGPipeConnectionComponent>(Comp))
		{
			if (!Reserved.Contains(PipeConn->GetFName())) OutConnectors.Add(PipeConn);
		}
	}
}

bool FSFPipeConnectorFinder::IsConnectionAngleValid(
	UFGPipeConnectionComponent* Connector,
	const FVector& ConnectionDirection,
	float MaxAngleDegrees)
{
	if (!Connector)
	{
		return false;
	}

	// Get connector's forward direction
	FVector ConnectorForward = Connector->GetComponentTransform().GetUnitAxis(EAxis::X);
	
	// Normalize connection direction
	FVector NormalizedDirection = ConnectionDirection.GetSafeNormal();
	
	// Calculate angle between connector forward and connection direction
	float DotProduct = FVector::DotProduct(ConnectorForward, NormalizedDirection);
	float AngleRadians = FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f));
	float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
	
	// Also check reverse direction (for input connectors facing opposite way)
	float ReverseDot = FVector::DotProduct(-ConnectorForward, NormalizedDirection);
	float ReverseAngleRadians = FMath::Acos(FMath::Clamp(ReverseDot, -1.0f, 1.0f));
	float ReverseAngleDegrees = FMath::RadiansToDegrees(ReverseAngleRadians);
	
	// Accept if either forward or reverse is within threshold
	bool bAngleValid = (AngleDegrees <= MaxAngleDegrees) || (ReverseAngleDegrees <= MaxAngleDegrees);
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, 
		TEXT("   Angle check: Connector=%s Forward=%.1f° Reverse=%.1f° Valid=%d"),
		*Connector->GetName(), AngleDegrees, ReverseAngleDegrees, bAngleValid);
	
	return bAngleValid;
}
