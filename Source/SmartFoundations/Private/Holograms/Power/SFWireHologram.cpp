// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Power/SFWireHologram.h"
#include "SmartFoundations.h"
#include "Constants/SFAssetPaths.h"
#include "Features/Extend/SFExtendService.h"  // ResolveChildPreviewMaterialState
#include "FGConstructDisqualifier.h"
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
	
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⚡ SFWireHologram::BeginPlay - %s"), *GetName());
}

TArray<FItemAmount> ASFWireHologram::GetBaseCost() const
{
	// #497: scale-daisy/auto-connect wire previews are spawned without a recipe (mRecipe == null); their
	// cost is length-based and computed in GetCost. Vanilla AFGHologram::GetBaseCost would call
	// UFGRecipe::GetIngredients(nullptr), logging "FGRecipe::GetIngredients: class was nullpeter" per wire
	// per frame — synchronous disk writes (UE log + Sentry breadcrumb) that stutter the game. Skip it.
	if (!GetRecipe())
	{
		return TArray<FItemAmount>();
	}
	return Super::GetBaseCost();
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
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⚡ SFWireHologram::Construct - Building wire %s (Parent: %s)"),
		*GetName(), GetParentHologram() ? *GetParentHologram()->GetName() : TEXT("none"));
	
	// Issue #229: Extend wire children have no connections set — vanilla Construct()
	// would crash trying to build an unconnected wire. Spawn a raw wire actor instead;
	// post-build wiring in SFExtendService::GenerateAndExecuteWiring() connects it.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);
		if (WireClass)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			
			AFGBuildableWire* Wire = GetWorld()->SpawnActor<AFGBuildableWire>(
				WireClass, GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
			
			if (Wire)
			{
				UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" SFWireHologram::Construct - Extend wire spawned: %s (post-build wiring will connect)"),
				*Wire->GetName());
				return Wire;
			}
		}
		
		SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Error, TEXT(" SFWireHologram::Construct - Failed to spawn extend wire!"));
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
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" Wire child preview - skipping placement validation"));
		// Build gun paints previews from construct disqualifiers; carry the parent's when unaffordable.
		const EHologramMaterialState ChildState = USFExtendService::ResolveChildPreviewMaterialState(this);
		ResetConstructDisqualifiers();
		if (ChildState == EHologramMaterialState::HMS_ERROR)
		{
			AddConstructDisqualifier(UFGCDUnaffordable::StaticClass());
		}
		SetPlacementMaterialState(ChildState);
		return;
	}

	Super::CheckValidPlacement();
}

void ASFWireHologram::SetPlacementMaterialState(EHologramMaterialState materialState)
{
	// Let base class handle its components (stencil/render depth on the base meshes)
	Super::SetPlacementMaterialState(materialState);

	// #346: tint the wire via the custom-depth stencil - the same way belts and vanilla holograms do -
	// instead of swapping the mesh material. The catenary is rendered by the realistic Powerline
	// material's world-position-offset; replacing it with a flat hologram material broke the curve's
	// mid-span shading and left a red/invalid-looking middle segment. Keeping the real material and
	// writing the hologram stencil lets the hologram post-process tint the whole wire uniformly (cyan
	// when valid, red when invalid). PreviewWireMesh is created dynamically, so the base class does not
	// know about it - we write its stencil ourselves (mirrors ASFConveyorBeltHologram).
	if (PreviewWireMesh)
	{
		const uint8 StencilValue = GetStencilForHologramMaterialState(materialState);
		PreviewWireMesh->SetRenderCustomDepth(true);
		PreviewWireMesh->SetCustomDepthStencilValue(StencilValue);
		PreviewWireMesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
		PreviewWireMesh->MarkRenderStateDirty();
	}
}

void ASFWireHologram::SetupWirePreview(UFGPowerConnectionComponent* StartConnection, UFGPowerConnectionComponent* EndConnection)
{
	if (!StartConnection || !EndConnection)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT(" SetupWirePreview: Invalid connections"));
		return;
	}

	// Set the connections using base class method (which triggers internal mesh updates)
	SetConnection(0, StartConnection);
	SetConnection(1, EndConnection);

	// Cache positions for our custom mesh
	CachedStartPos = StartConnection->GetComponentLocation();
	CachedEndPos = EndConnection->GetComponentLocation();

	UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" SetupWirePreview: %s"), *GetName());
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Start: %s"), *CachedStartPos.ToString());
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   End: %s"), *CachedEndPos.ToString());

	// Create our own wire mesh with proper catenary curve (the vanilla AFGWireHologram does not render
	// its wire when this hologram is a child preview, so we draw it ourselves).
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
		UE_LOG(LogSmartHologram, Verbose, TEXT(" TriggerMeshGeneration called but wire not configured"));
		return;
	}

	// Recreate mesh with current positions
	CreateWireMeshWithCatenary(CachedStartPos, CachedEndPos);

	// Force visibility
	ForceVisibilityUpdate();

	UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" TriggerMeshGeneration: Wire mesh updated"));
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

	// #346: apply the valid-state hologram stencil (not a material swap) so the wire keeps its realistic
	// catenary material and is tinted as a valid hologram.
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
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

void ASFWireHologram::SetupWirePreviewFromPositions(const FVector& StartWorld, const FVector& EndWorld)
{
	// Issue #345: like SetupWirePreview, but from raw world positions (no connection components, since
	// the Extend clone poles don't exist yet). Endpoints also drive GetWireLength()/GetCost().
	CachedStartPos = StartWorld;
	CachedEndPos = EndWorld;

	// Extend repositions its child holograms every frame; keep the wire mesh in absolute world space so
	// it stays on the endpoints instead of being dragged to the child actor's transform.
	bUseAbsoluteMeshTransform = true;
	CreateWireMeshWithCatenary(StartWorld, EndWorld);
	bWireConfigured = true;

	SetActorLocation((StartWorld + EndWorld) * 0.5f);
	SetActorHiddenInGame(false);

	// Apply the valid-state hologram stencil so the wire is tinted as a hologram (no material swap).
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
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
		UE_LOG(LogSmartHologram, Verbose, TEXT(" SFWireHologram::ConfigureActor - inBuildable is not a wire!"));
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
							UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" SFWireHologram::ConfigureActor - Pole-to-building: Found source pole %s"),
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
					UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" SFWireHologram::ConfigureActor - Pole-to-building connected: %s to %s"),
					ActualConn0->GetOwner() ? *ActualConn0->GetOwner()->GetName() : TEXT("null"),
					StoredConn1->GetOwner() ? *StoredConn1->GetOwner()->GetName() : TEXT("null"));
				}
			}
			return;
		}
	}
	
	// Pole-to-pole wire: Target is still a hologram, skip connection here
	// The deferred system will handle this after all poles are built
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⚡ SFWireHologram::ConfigureActor - Pole-to-pole wire: Deferring connection (target is hologram)"));
}

void ASFWireHologram::CreateWireMeshWithCatenary(const FVector& StartPos, const FVector& EndPos)
{
	// Load the power line static mesh
	UStaticMesh* WireMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/PowerLine/Mesh/PowerLine_static.PowerLine_static"));
	if (!WireMesh)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("⚡ Failed to load SM_PowerLine mesh"));
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
			
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⚡ Created PreviewWireMesh component with Powerline_Inst material"));
		}
	}

	if (!PreviewWireMesh)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("⚡ Failed to create PreviewWireMesh"));
		return;
	}

	// Issue #345: decouple the mesh from the (Extend-repositioned) child actor so the world transform
	// applied below sticks. Must be set BEFORE the SetWorld* calls so they are interpreted as absolute.
	if (bUseAbsoluteMeshTransform)
	{
		PreviewWireMesh->SetUsingAbsoluteLocation(true);
		PreviewWireMesh->SetUsingAbsoluteRotation(true);
		PreviewWireMesh->SetUsingAbsoluteScale(true);
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

	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("⚡ Wire mesh configured: Length=%.1f cm, Scale=%.2f"), Length, ScaleFactor);
}

