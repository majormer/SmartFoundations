#include "Holograms/Power/SFWireHologram.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildableWire.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Hologram/FGHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

ASFWireHologram::ASFWireHologram()
	: PreviewWireMesh(nullptr)
	, bWireConfigured(false)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ASFWireHologram::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ SFWireHologram::BeginPlay - %s"), *GetName());
}

TArray<FItemAmount> ASFWireHologram::GetCost(bool includeChildren) const
{
	TArray<FItemAmount> Cost;

	const float LengthCm = GetWireLength();
	if (LengthCm <= 0.0f)
	{
		return Cost;
	}

	const float LengthMeters = LengthCm / 100.0f;
	const int32 CablesNeeded = FMath::Max(1, FMath::CeilToInt(LengthMeters / 25.0f));

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

AActor* ASFWireHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ SFWireHologram::Construct - Building wire %s (Parent: %s)"),
		*GetName(), GetParentHologram() ? *GetParentHologram()->GetName() : TEXT("none"));
	
	// Issue #229: Extend wire children have no connections set — vanilla Construct()
	// would crash trying to build an unconnected wire. Spawn a raw wire actor instead;
	// post-build wiring in SFExtendService::GenerateAndExecuteWiring() connects it.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr,
			TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Build_PowerLine.Build_PowerLine_C"));
		if (WireClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			
			AFGBuildableWire* Wire = GetWorld()->SpawnActor<AFGBuildableWire>(
				WireClass, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
			
			if (Wire)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SFWireHologram::Construct - Extend wire spawned: %s (post-build wiring will connect)"),
				*Wire->GetName());
				return Wire;
			}
		}
		
		SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Error, TEXT(" SFWireHologram::Construct - Failed to spawn extend wire!"));
		// Fallback: must not return nullptr or vanilla crashes
	}
	
	// Normal path: power auto-connect wires with connections set
	return Super::Construct(out_children, constructionID);
}

void ASFWireHologram::CheckValidPlacement()
{
	// Skip validation for child holograms used as previews
	if (GetParentHologram())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Wire child preview - skipping placement validation"));
		return;
	}

	Super::CheckValidPlacement();
}

void ASFWireHologram::SetPlacementMaterialState(EHologramMaterialState materialState)
{
	// Let base class handle its components
	Super::SetPlacementMaterialState(materialState);
	
	// Apply materials to our preview wire mesh
	UMaterialInterface* Material = nullptr;
	switch (materialState)
	{
		case EHologramMaterialState::HMS_OK:
			Material = mValidPlacementMaterial;
			break;
		case EHologramMaterialState::HMS_WARNING:
			Material = mValidPlacementMaterial;
			break;
		case EHologramMaterialState::HMS_ERROR:
			Material = mInvalidPlacementMaterial;
			break;
	}
	
	if (Material)
	{
		ApplyHologramMaterial(Material);
	}
}

void ASFWireHologram::SetupWirePreview(UFGPowerConnectionComponent* StartConnection, UFGPowerConnectionComponent* EndConnection)
{
	if (!StartConnection || !EndConnection)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT(" SetupWirePreview: Invalid connections"));
		return;
	}

	// Set the connections using base class method (which triggers internal mesh updates)
	SetConnection(0, StartConnection);
	SetConnection(1, EndConnection);

	// Cache positions for our custom mesh
	CachedStartPos = StartConnection->GetComponentLocation();
	CachedEndPos = EndConnection->GetComponentLocation();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SetupWirePreview: %s"), *GetName());
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Start: %s"), *CachedStartPos.ToString());
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   End: %s"), *CachedEndPos.ToString());

	// Create our own wire mesh with proper catenary curve
	CreateWireMeshWithCatenary(CachedStartPos, CachedEndPos);

	bWireConfigured = true;

	// Position actor at midpoint
	FVector Midpoint = (CachedStartPos + CachedEndPos) * 0.5f;
	SetActorLocation(Midpoint);

	SetActorHiddenInGame(false);
}

void ASFWireHologram::TriggerMeshGeneration()
{
	if (!bWireConfigured)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT(" TriggerMeshGeneration called but wire not configured"));
		return;
	}

	// Recreate mesh with current positions
	CreateWireMeshWithCatenary(CachedStartPos, CachedEndPos);

	// Force visibility
	ForceVisibilityUpdate();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" TriggerMeshGeneration: Wire mesh updated"));
}

void ASFWireHologram::ForceVisibilityUpdate()
{
	SetActorHiddenInGame(false);
	SetActorEnableCollision(false);

	// Force all components visible
	TArray<USceneComponent*> SceneComps;
	GetComponents<USceneComponent>(SceneComps);
	for (USceneComponent* Comp : SceneComps)
	{
		if (Comp)
		{
			Comp->SetVisibility(true, true);
		}
	}

	// Apply valid placement material by default
	if (mValidPlacementMaterial)
	{
		ApplyHologramMaterial(mValidPlacementMaterial);
	}
}

float ASFWireHologram::GetWireLength() const
{
	return FVector::Dist(CachedStartPos, CachedEndPos);
}

void ASFWireHologram::SetWireEndpoints(const FVector& Start, const FVector& End)
{
	CachedStartPos = Start;
	CachedEndPos = End;
}

void ASFWireHologram::ConfigureActor(AFGBuildable* inBuildable) const
{
	// Issue #244: DEFERRED CONNECTION APPROACH
	// 
	// Problem: During grid build, poles are constructed sequentially. When ConfigureActor
	// is called for a wire, the target pole may not be built yet (still a hologram).
	// 
	// Solution: Skip ALL wire connections here. The deferred connection system in
	// SFSubsystem::ProcessDeferredPowerPoleConnections() and OnPowerPoleBuilt() handles
	// creating wires AFTER all poles are built, when we have actual buildables to connect.
	//
	// For pole-to-building wires, the target (building) already exists, so those connections
	// are handled by the existing deferred system as well.
	
	AFGBuildableWire* Wire = Cast<AFGBuildableWire>(inBuildable);
	if (!Wire)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT(" SFWireHologram::ConfigureActor - inBuildable is not a wire!"));
		return;
	}
	
	// Check if this is a pole-to-building wire (target is already a buildable, not a hologram)
	UFGCircuitConnectionComponent* StoredConn1 = GetConnection(1);
	if (StoredConn1 && StoredConn1->GetOwner())
	{
		AActor* Conn1Owner = StoredConn1->GetOwner();
		
		// If target is a buildable (not a hologram), we can connect now
		if (!Conn1Owner->IsA(AFGHologram::StaticClass()))
		{
			// This is a pole-to-building wire - target building already exists
			// Find the built source pole and connect
			UFGCircuitConnectionComponent* ActualConn0 = GetConnection(0);
			
			if (GetParentHologram())
			{
				FVector ParentLocation = GetParentHologram()->GetActorLocation();
				
				TArray<AActor*> FoundActors;
				UGameplayStatics::GetAllActorsOfClass(GetWorld(), AFGBuildablePowerPole::StaticClass(), FoundActors);
				
				for (AActor* Actor : FoundActors)
				{
					AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(Actor);
					if (Pole && FVector::Dist(Pole->GetActorLocation(), ParentLocation) < 100.0f)
					{
						TArray<UFGCircuitConnectionComponent*> CircuitConns;
						Pole->GetComponents<UFGCircuitConnectionComponent>(CircuitConns);
						if (CircuitConns.Num() > 0)
						{
							ActualConn0 = CircuitConns[0];
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SFWireHologram::ConfigureActor - Pole-to-building: Found source pole %s"),
					*Pole->GetName());
							break;
						}
					}
				}
			}
			
			if (ActualConn0 && StoredConn1)
			{
				bool bConnected = Wire->Connect(ActualConn0, StoredConn1);
				if (bConnected)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" SFWireHologram::ConfigureActor - Pole-to-building connected: %s to %s"),
					ActualConn0->GetOwner() ? *ActualConn0->GetOwner()->GetName() : TEXT("null"),
					StoredConn1->GetOwner() ? *StoredConn1->GetOwner()->GetName() : TEXT("null"));
				}
			}
			return;
		}
	}
	
	// Pole-to-pole wire: Target is still a hologram, skip connection here
	// The deferred system will handle this after all poles are built
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ SFWireHologram::ConfigureActor - Pole-to-pole wire: Deferring connection (target is hologram)"));
}

void ASFWireHologram::CreateWireMeshWithCatenary(const FVector& StartPos, const FVector& EndPos)
{
	// Load the power line static mesh
	UStaticMesh* WireMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Mesh/PowerLine_static.PowerLine_static"));
	if (!WireMesh)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("⚡ Failed to load SM_PowerLine mesh"));
		return;
	}

	// Create or reuse our preview mesh component
	if (!PreviewWireMesh)
	{
		PreviewWireMesh = NewObject<UStaticMeshComponent>(this);
		if (PreviewWireMesh)
		{
			PreviewWireMesh->SetStaticMesh(WireMesh);
			PreviewWireMesh->SetMobility(EComponentMobility::Movable);
			PreviewWireMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			
			// Use vanilla production power line material (matches vanilla wire hologram)
			UMaterialInterface* PowerlineMaterial = LoadObject<UMaterialInterface>(nullptr, 
				TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Material/Powerline_Inst.Powerline_Inst"));
			if (PowerlineMaterial)
			{
				PreviewWireMesh->SetMaterial(0, PowerlineMaterial);
			}
			
			PreviewWireMesh->RegisterComponent();
			PreviewWireMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ Created PreviewWireMesh component with Powerline_Inst material"));
		}
	}

	if (!PreviewWireMesh)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("⚡ Failed to create PreviewWireMesh"));
		return;
	}

	// Use static helper from AFGBuildableWire to create a proper wire instance
	// This handles the catenary curve calculation
	FWireInstance WireInstance = AFGBuildableWire::CreateWireInstance(StartPos, EndPos, GetActorTransform());
	
	// Copy the mesh settings from the wire instance if it was configured
	// The CreateWireInstance function calculates proper catenary positions
	
	// Apply the wire mesh transform to show catenary curve
	// Power lines use a catenary mesh that gets scaled and positioned
	FVector Direction = EndPos - StartPos;
	float Length = Direction.Size();
	Direction.Normalize();

	// Position at midpoint
	FVector Midpoint = (StartPos + EndPos) * 0.5f;
	PreviewWireMesh->SetWorldLocation(Midpoint);

	// Rotate to face direction
	FRotator MeshRotation = Direction.Rotation();
	// Power line mesh is oriented along X axis, adjust rotation
	MeshRotation.Pitch = 0.0f;
	PreviewWireMesh->SetWorldRotation(MeshRotation);

	// Scale to length
	// SM_PowerLine is typically 100cm in length, scale accordingly
	float ScaleFactor = Length / 100.0f;
	PreviewWireMesh->SetWorldScale3D(FVector(ScaleFactor, 1.0f, 1.0f));

	// Now use the static UpdateWireInstanceMesh to apply proper catenary curve
	// This is the key function that makes wires look correct
	FWireInstance TempInstance;
	TempInstance.WireMesh = PreviewWireMesh;
	TempInstance.Locations[0] = StartPos;
	TempInstance.Locations[1] = EndPos;
	
	// Update the mesh with catenary calculations
	AFGBuildableWire::UpdateWireInstanceMesh(TempInstance);

	PreviewWireMesh->SetVisibility(true, true);
	PreviewWireMesh->MarkRenderStateDirty();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ Wire mesh configured: Length=%.1f cm, Scale=%.2f"), Length, ScaleFactor);
}

void ASFWireHologram::ApplyHologramMaterial(UMaterialInterface* Material)
{
	if (!Material)
	{
		return;
	}

	// Apply to our preview mesh
	if (PreviewWireMesh)
	{
		PreviewWireMesh->SetMaterial(0, Material);
	}

	// Also apply to any base class meshes
	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (MeshComp)
		{
			MeshComp->SetMaterial(0, Material);
		}
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚡ Applied hologram material: %s"), *Material->GetName());
}
