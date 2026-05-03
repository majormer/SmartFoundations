#include "Holograms/Logistics/SFPipelineHologram.h"
#include "SmartFoundations.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Components/SplineMeshComponent.h"
#include "Hologram/FGSplineHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeSubsystem.h"
#include "FGPipeNetwork.h"
#include "Data/SFHologramDataRegistry.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "FGBuildable.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "FGRecipeManager.h"
#include "FGRecipe.h"
#include "EngineUtils.h"

ASFPipelineHologram::ASFPipelineHologram()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void ASFPipelineHologram::BeginPlay()
{
	Super::BeginPlay();
	
	// Check if base hologram created mesh components
	TArray<USplineMeshComponent*> MeshComps;
	GetComponents<USplineMeshComponent>(MeshComps);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   PIPE BeginPlay: Found %d SplineMeshComponents after Super::BeginPlay()"), MeshComps.Num());
	
	// Check mesh and material status
	for (int32 i = 0; i < MeshComps.Num(); i++)
	{
		USplineMeshComponent* MeshComp = MeshComps[i];
		if (MeshComp)
		{
			UStaticMesh* Mesh = MeshComp->GetStaticMesh();
			UMaterialInterface* Material = MeshComp->GetMaterial(0);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   PIPE BeginPlay: SplineMesh[%d] - Mesh=%s, Material=%s, Visible=%s"),
				i,
				Mesh ? *Mesh->GetName() : TEXT("NULL"),
				Material ? *Material->GetName() : TEXT("NULL"),
				MeshComp->IsVisible() ? TEXT("YES") : TEXT("NO"));
		}
	}
	
	if (mSplineComponent)
	{
		const int32 PointCount = mSplineComponent->GetNumberOfSplinePoints();
		const float SplineLength = mSplineComponent->GetSplineLength();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   PIPE BeginPlay: mSplineComponent has %d points, %.1f cm length"), PointCount, SplineLength);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("   PIPE BeginPlay: mSplineComponent is NULL!"));
	}
}

void ASFPipelineHologram::CheckValidPlacement()
{
	if (GetParentHologram())
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe child preview - skipping placement validation"));
		return;
	}

	Super::CheckValidPlacement();
}

AActor* ASFPipelineHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	// CRITICAL: Prevent double-construction
	// Construct() can be called by both vanilla child system AND manual build code in SFSubsystem.
	// If both call Construct(), we get duplicate pipes. This flag prevents the second call.
	// IMPORTANT: Return the previously built actor, NOT nullptr - vanilla crashes on nullptr!
	if (bHasBeenConstructed)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE: Skipping duplicate Construct() for %s (already built, returning cached actor)"), *GetName());
		return ConstructedActor.Get();
	}
	bHasBeenConstructed = true;
	
	// Check if this is an EXTEND, STACKABLE, or PIPE AUTO-CONNECT child hologram
	bool bIsExtendChild = Tags.Contains(FName(TEXT("SF_ExtendChild")));
	bool bIsStackableChild = Tags.Contains(FName(TEXT("SF_StackableChild")));
	bool bIsPipeAutoConnectChild = Tags.Contains(FName(TEXT("SF_PipeAutoConnectChild")));
	
	if (bIsExtendChild || bIsStackableChild || bIsPipeAutoConnectChild)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 %s: Pipe hologram %s Construct() called - building as child"), 
			bIsPipeAutoConnectChild ? TEXT("PIPE AUTO-CONNECT") : (bIsStackableChild ? TEXT("STACKABLE") : TEXT("EXTEND")), *GetName());
		
		// IMPORTANT: For EXTEND children, we need to build the pipe directly without
		// going through the full vanilla Construct() which might spawn child poles.
		// The vanilla AFGPipelineHologram::Construct() can spawn mChildPoleHologram
		// entries which we don't want for cloned pipes.
		
		// CRITICAL FIX: Use a dummy array for out_children to prevent any child actors
		// from being propagated back to the build gun. AFGSplineHologram::Construct() or
		// its parents may add child pole stubs to out_children, which then get built as
		// 200cm mini pipes. By using a dummy array, we discard any such children.
		TArray<AActor*> DiscardedChildren;
		AActor* BuiltActor = AFGSplineHologram::Construct(DiscardedChildren, constructionID);
		
		// Cache the built actor for duplicate Construct() calls
		ConstructedActor = BuiltActor;
		
		if (BuiltActor)
		{
			// CRITICAL: Transfer tags from hologram to built actor for deferred wiring detection
			// The deferred wiring system in SFSubsystem checks SpawnedActor->Tags
			if (bIsPipeAutoConnectChild)
			{
				BuiltActor->Tags.AddUnique(FName(TEXT("SF_PipeAutoConnectChild")));
			}
			if (bIsStackableChild)
			{
				BuiltActor->Tags.AddUnique(FName(TEXT("SF_StackableChild")));
			}
			if (bIsExtendChild)
			{
				BuiltActor->Tags.AddUnique(FName(TEXT("SF_ExtendChild")));
			}
			
			// Register hologram → buildable mapping for post-build wiring
			FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
			if (HoloData)
			{
				HoloData->bWasBuilt = true;
				HoloData->CreatedActor = Cast<AFGBuildable>(BuiltActor);
			}
			
			// Log the mapping for debugging
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 %s: ✅ Pipe hologram %s → Buildable %s (ID: %u), discarded %d child actors"), 
				bIsStackableChild ? TEXT("STACKABLE") : (bIsPipeAutoConnectChild ? TEXT("PIPE AUTO-CONNECT") : TEXT("EXTEND")),
				*GetName(), *BuiltActor->GetName(), BuiltActor->GetUniqueID(), DiscardedChildren.Num());
			
			// Log pipe connection info and register with SFExtendService
			if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(BuiltActor))
			{
				UFGPipeConnectionComponentBase* Conn0 = Pipe->GetPipeConnection0();
				UFGPipeConnectionComponentBase* Conn1 = Pipe->GetPipeConnection1();
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND:   Conn0=%s @ %s, Conn1=%s @ %s"),
					Conn0 ? *Conn0->GetName() : TEXT("null"),
					Conn0 ? *Conn0->GetComponentLocation().ToString() : TEXT("N/A"),
					Conn1 ? *Conn1->GetName() : TEXT("null"),
					Conn1 ? *Conn1->GetComponentLocation().ToString() : TEXT("N/A"));
				
				// STACKABLE PIPE: Wire to neighbor pipes at pole snap points
				// The pole's SnapOnly0 is PCT_SNAP_ONLY (position only, not fluid connection).
				// We use its location to find and connect to OTHER PIPES at that location.
				if (bIsStackableChild && HoloData && HoloData->bIsStackablePipe)
				{
					// At Conn0 location (start of this pipe), find another pipe's endpoint to connect to
					if (Conn0 && !Conn0->IsConnected())
					{
						FVector SearchLocation = Conn0->GetComponentLocation();
						UFGPipeConnectionComponentBase* NeighborConn = UFGPipeConnectionComponentBase::FindCompatibleOverlappingConnection(
							Conn0, SearchLocation, Pipe, 50.0f, {});
						
						if (NeighborConn && !NeighborConn->IsConnected() && NeighborConn->GetOwner() != Pipe)
						{
							Conn0->SetConnection(NeighborConn);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" %s: Wired Conn0 to neighbor pipe %s.%s"), 
								bIsStackableChild ? TEXT("STACKABLE") : (bIsPipeAutoConnectChild ? TEXT("PIPE AUTO-CONNECT") : TEXT("EXTEND")),
								*NeighborConn->GetOwner()->GetName(), *NeighborConn->GetName());
						}
					}
					
					// At Conn1 location (end of this pipe), find another pipe's endpoint to connect to
					if (Conn1 && !Conn1->IsConnected())
					{
						FVector SearchLocation = Conn1->GetComponentLocation();
						UFGPipeConnectionComponentBase* NeighborConn = UFGPipeConnectionComponentBase::FindCompatibleOverlappingConnection(
							Conn1, SearchLocation, Pipe, 50.0f, {});
						
						if (NeighborConn && !NeighborConn->IsConnected() && NeighborConn->GetOwner() != Pipe)
						{
							Conn1->SetConnection(NeighborConn);
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 STACKABLE: Wired Conn1 to neighbor pipe %s.%s"), 
								*NeighborConn->GetOwner()->GetName(), *NeighborConn->GetName());
						}
					}
					
					// Merge/rebuild pipe networks to ensure fluid can flow
					UWorld* World = GetWorld();
					if (World)
					{
						AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
						if (PipeSubsystem)
						{
							UFGPipeConnectionComponent* PipeConn0 = Cast<UFGPipeConnectionComponent>(Conn0);
							UFGPipeConnectionComponent* PipeConn1 = Cast<UFGPipeConnectionComponent>(Conn1);
							
							if (PipeConn0 && PipeConn1)
							{
								int32 Network0 = PipeConn0->GetPipeNetworkID();
								int32 Network1 = PipeConn1->GetPipeNetworkID();
								
								if (Network0 != Network1 && Network0 != INDEX_NONE && Network1 != INDEX_NONE)
								{
									AFGPipeNetwork* Net0 = PipeSubsystem->FindPipeNetwork(Network0);
									AFGPipeNetwork* Net1 = PipeSubsystem->FindPipeNetwork(Network1);
									if (Net0 && Net1)
									{
										Net0->MergeNetworks(Net1);
										Net0->MarkForFullRebuild();
										UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE: Merged pipe networks %d and %d"), Network0, Network1);
									}
								}
								else if (Network0 != INDEX_NONE)
								{
									AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network0);
									if (Net)
									{
										Net->MarkForFullRebuild();
										UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE: Marked network %d for rebuild"), Network0);
									}
								}
								else if (Network1 != INDEX_NONE)
								{
									AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network1);
									if (Net)
									{
										Net->MarkForFullRebuild();
										UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE: Marked network %d for rebuild"), Network1);
									}
								}
							}
						}
					}
					
					// Finalize the pipe
					Pipe->OnBuildEffectFinished();
					UE_LOG(LogSmartFoundations, Log, TEXT("🔧 STACKABLE: Pipe %s finalized (index %d)"), 
						*Pipe->GetName(), HoloData->StackablePipeIndex);
				}
				
				// PIPE AUTO-CONNECT: Wire connections after build
				if (bIsPipeAutoConnectChild && HoloData && HoloData->bIsPipeAutoConnectChild)
				{
					// Finalize the pipe
					Pipe->OnBuildEffectFinished();
					
					// Floor hole pipes (PipeAutoConnectConn0 == nullptr):
					// Conn1 (building side) is already wired by vanilla's AFGSplineHologram::Construct().
					// Conn0 (floor hole side) needs to be registered with the floor hole via
					// SetTopSnappedConnection/SetBottomSnappedConnection — the same pattern Extend uses
					// for lifts. AFGBuildablePassthrough does NOT own UFGPipeConnectionComponent components;
					// it stores references to connections from other actors that snap to it.
					if (!HoloData->PipeAutoConnectConn0)
					{
						// Find the floor hole that was just built by the parent hologram
						FVector PipeConn0Loc = Conn0 ? Conn0->GetComponentLocation() : FVector::ZeroVector;
						AFGBuildablePassthrough* FloorHole = nullptr;
						float BestDist = 100.0f; // 1m XY tolerance
						
						for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
						{
							AFGBuildablePassthrough* PT = *It;
							if (!PT) continue;
							
							float DistXY = FVector::Dist2D(PipeConn0Loc, PT->GetActorLocation());
							if (DistXY < BestDist)
							{
								BestDist = DistXY;
								FloorHole = PT;
							}
						}
						
						if (FloorHole && Conn0)
						{
							// Determine if pipe's Conn0 is at the top or bottom of the floor hole
							float FloorHoleZ = FloorHole->GetActorLocation().Z;
							bool bIsTopSide = (PipeConn0Loc.Z >= FloorHoleZ);
							
							UFGConnectionComponent* PipeConnAsBase = Cast<UFGConnectionComponent>(Conn0);
							if (PipeConnAsBase)
							{
								if (bIsTopSide)
								{
									FloorHole->SetTopSnappedConnection(PipeConnAsBase);
								}
								else
								{
									FloorHole->SetBottomSnappedConnection(PipeConnAsBase);
								}
								
								UE_LOG(LogSmartFoundations, Log, TEXT("🔧 FLOOR HOLE PIPE: ✅ %s.Conn0 registered with %s via Set%sSnappedConnection (DistXY=%.1f)"),
									*Pipe->GetName(), *FloorHole->GetName(),
									bIsTopSide ? TEXT("Top") : TEXT("Bottom"), BestDist);
							}
							else
							{
								UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 FLOOR HOLE PIPE: Conn0 could not cast to UFGConnectionComponent"));
							}
						}
						else
						{
							UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 FLOOR HOLE PIPE: ❌ No floor hole found near Conn0 @ %s"),
								*PipeConn0Loc.ToString());
						}
					}
					else
					{
						// Junction/building pipes — use deferred proximity wiring (existing behavior)
						USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
						if (Subsystem)
						{
							Subsystem->RegisterPipeForDeferredWiring(Pipe);
						}
						
						UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE AUTO-CONNECT: Pipe %s built, registered for deferred wiring (%s)"), 
							*Pipe->GetName(), HoloData->bIsPipeManifold ? TEXT("Manifold") : TEXT("Building"));
					}
				}
				
				// Register with SFExtendService for post-build wiring
				if (HoloData && HoloData->ExtendChainId >= 0 && HoloData->ExtendChainIndex >= 0)
				{
					USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
					if (Subsystem)
					{
						USFExtendService* ExtendService = Subsystem->GetExtendService();
						if (ExtendService)
						{
							ExtendService->RegisterBuiltPipe(HoloData->ExtendChainId, HoloData->ExtendChainIndex, Pipe, HoloData->bIsInputChain);
							
							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Pipe %s registered in Construct() for chain %d, index %d (%s)"),
								*Pipe->GetName(), HoloData->ExtendChainId, HoloData->ExtendChainIndex,
								HoloData->bIsInputChain ? TEXT("INPUT") : TEXT("OUTPUT"));
						}
					}
				}
				
				// Register with ExtendService for JSON-based post-build wiring
				if (HoloData && !HoloData->JsonCloneId.IsEmpty())
				{
					USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
					if (Subsystem)
					{
						USFExtendService* ExtendService = Subsystem->GetExtendService();
						if (ExtendService)
						{
							ExtendService->RegisterJsonBuiltActor(HoloData->JsonCloneId, BuiltActor);
						}
					}
				}
				
				// MANIFOLD PIPE WIRING: Wire directly after build
				if (HoloData && HoloData->bIsManifoldPipe && HoloData->ManifoldSourceConnector)
				{
					USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
					if (Subsystem)
					{
						USFExtendService* ExtendService = Subsystem->GetExtendService();
						if (ExtendService)
						{
							ExtendService->WireManifoldPipe(Pipe, HoloData->ManifoldSourceConnector, HoloData->ManifoldCloneChainId);
						}
					}
				}
			}
		}
		else
		{
			SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND: ❌ Pipe Construct returned nullptr!"));
		}
		
		return BuiltActor;
	}
	
	// For Auto-Connect pipes: Build normally (includes child poles if needed)
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 AUTO-CONNECT: Pipe hologram %s Construct() called - building pipe"), 
		*GetName());
	AActor* Result = Super::Construct(out_children, constructionID);
	
	// Cache the built actor for duplicate Construct() calls
	ConstructedActor = Result;
	
	return Result;
}

void ASFPipelineHologram::SpawnChildren(AActor* hologramOwner, FVector spawnLocation, APawn* hologramInstigator)
{
	// CRITICAL: For EXTEND/STACKABLE/PIPE AUTO-CONNECT child pipes, do NOT spawn child pole holograms.
	// The vanilla AFGPipelineHologram::SpawnChildren() spawns mChildPoleHologram entries
	// which would then be constructed as pipes (wrong!) when the build gun builds the parent.
	// EXTEND pipes are cloned from existing pipes that already have their own support poles.
	// STACKABLE pipes connect between stackable pole supports - no additional poles needed.
	// PIPE AUTO-CONNECT pipes connect junctions to buildings - no additional poles needed.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))) || 
	    Tags.Contains(FName(TEXT("SF_StackableChild"))) ||
	    Tags.Contains(FName(TEXT("SF_PipeAutoConnectChild"))))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE: Hologram %s SpawnChildren() - SKIPPING child pole spawning for child pipe"), 
			*GetName());
		// Do NOT call Super::SpawnChildren() - this prevents vanilla from spawning child poles
		return;
	}
	
	// For Auto-Connect pipes: Let vanilla spawn child poles normally
	Super::SpawnChildren(hologramOwner, spawnLocation, hologramInstigator);
}

void ASFPipelineHologram::SetHologramLocationAndRotation(const FHitResult& hitResult)
{
	// CRITICAL: For EXTEND child pipes, skip vanilla's SetHologramLocationAndRotation entirely.
	// The vanilla implementation can spawn child pole holograms and do other processing
	// that we don't want for cloned EXTEND pipes.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		// For EXTEND children, just set location directly without any vanilla processing
		// The position is managed by SFExtendService via ChildIntendedPositions
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Pipe hologram %s SetHologramLocationAndRotation() - SKIPPING vanilla processing"), 
			*GetName());
		return;
	}
	
	// For Auto-Connect pipes: Let vanilla handle location/rotation normally
	Super::SetHologramLocationAndRotation(hitResult);
}

void ASFPipelineHologram::PostHologramPlacement(const FHitResult& hitResult, bool callForChildren)
{
	// CRITICAL: For EXTEND, STACKABLE, and PIPE AUTO-CONNECT child pipes, skip vanilla's PostHologramPlacement entirely.
	// The vanilla implementation calls GenerateAndUpdateSpline() -> UpdateSplineComponent()
	// which tries to access spline mesh components that may have been destroyed/replaced
	// by our TriggerMeshGeneration(), causing a crash.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))) || 
	    Tags.Contains(FName(TEXT("SF_StackableChild"))) ||
	    Tags.Contains(FName(TEXT("SF_PipeAutoConnectChild"))))
	{
		// For child pipes, we've already set up the spline via TriggerMeshGeneration()
		// Just call the base AFGBuildableHologram version to skip the pipe-specific spline update
		// but still do any essential base class work
		AFGBuildableHologram::PostHologramPlacement(hitResult, callForChildren);
		return;
	}
	
	// For non-child pipes: Let vanilla handle post-placement normally
	Super::PostHologramPlacement(hitResult, callForChildren);
}

void ASFPipelineHologram::SetSnappedConnections(UFGPipeConnectionComponentBase* Connection0, UFGPipeConnectionComponentBase* Connection1)
{
	// Use reflection to access the private mSnappedConnectionComponents array
	// This tells the vanilla system that the pipe is already connected, preventing child pole spawning
	
	FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
	if (SnappedProp)
	{
		// mSnappedConnectionComponents is a C-style array of 2 UFGPipeConnectionComponentBase*
		// We need to get the raw pointer to the array
		void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(this);
		UFGPipeConnectionComponentBase** SnappedArray = static_cast<UFGPipeConnectionComponentBase**>(PropAddr);
		
		if (SnappedArray)
		{
			SnappedArray[0] = Connection0;
			SnappedArray[1] = Connection1;
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 EXTEND: Set snapped connections on %s: [0]=%s, [1]=%s"),
				*GetName(),
				Connection0 ? *Connection0->GetName() : TEXT("nullptr"),
				Connection1 ? *Connection1->GetName() : TEXT("nullptr"));
		}
	}
	else
	{
		SF_EXTEND_DIAGNOSTIC_LOG(LogSmartFoundations, Warning, TEXT("🔧 EXTEND: Failed to find mSnappedConnectionComponents property on %s"), *GetName());
	}
}

bool ASFPipelineHologram::TryUseBuildModeRouting(
	const FVector& StartPos,
	const FVector& StartNormal,
	const FVector& EndPos,
	const FVector& EndNormal)
{
	if (!mSplineComponent)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🔍 PIPE TryUseBuildModeRouting FAILED: No mSplineComponent on %s"), *GetName());
		return false;
	}

	// Select routing mode (set by HUD auto-connect settings).
	// 0=Auto, 1=2D, 2=Straight, 3=Curve
	switch (RoutingMode)
	{
	case 1:
		Auto2DRouteSpline(StartPos, StartNormal, EndPos, EndNormal);
		break;
	case 2:
		AutoRouteStraightSpline(StartPos, StartNormal, EndPos, EndNormal);
		break;
	case 3:
		AutoRouteCurveSpline(StartPos, StartNormal, EndPos, EndNormal);
		break;
	case 0:
	default:
		AutoRouteSpline(StartPos, StartNormal, EndPos, EndNormal);
		break;
	}

	// Ensure spline component is updated from mSplineData.
	// We explicitly call the base spline hologram impl, same pattern used elsewhere in Smart.
	AFGSplineHologram::UpdateSplineComponent();

	const int32 NewSplinePoints = mSplineData.Num();
	const float NewSplineLength = mSplineComponent->GetSplineLength();
	const float ExpectedDistance = FVector::Distance(StartPos, EndPos);

	// Accept 2-point straights (engine can legitimately return these).
	// Only fail on clearly invalid placeholders (less than 50cm minimum pipe length).
	if (NewSplinePoints < 2 || NewSplineLength < 50.0f)
	{
		UE_LOG(LogSmartFoundations, Log,
			TEXT("🔍 PIPE TryUseBuildModeRouting FAILED: Stub spline after AutoRouteSpline on %s | Points=%d Len=%.1f Expected=%.1f"),
			*GetName(),
			NewSplinePoints,
			NewSplineLength,
			ExpectedDistance);
		return false;
	}

	// Use the same routed spline for build.
	mBuildSplineData = mSplineData;

	// Log at normal level only for low-point-count results so we can verify engine routing.
	if (NewSplinePoints <= 3)
	{
		UE_LOG(LogSmartFoundations, Log,
			TEXT("🔍 PIPE TryUseBuildModeRouting OK: Engine AutoRouteSpline produced %d points (Len=%.1f Expected=%.1f) on %s"),
			NewSplinePoints,
			NewSplineLength,
			ExpectedDistance,
			*GetName());
	}

	return true;
}

void ASFPipelineHologram::SetPlacementMaterialState(EHologramMaterialState materialState)
{
	// Configure spline mesh components for hologram rendering
	// CRITICAL: Vanilla pipe holograms use the NATIVE pipe material (not hologram material)
	// The hologram effect comes from custom depth stencil settings, not material override
	TArray<USplineMeshComponent*> MeshComps;
	GetComponents<USplineMeshComponent>(MeshComps);
	
	// If no spline meshes are present, skip the rest to avoid log spam
	if (MeshComps.Num() == 0)
	{
		return;
	}
	
	for (USplineMeshComponent* Mesh : MeshComps)
	{
		if (Mesh)
		{
			// CRITICAL: Set custom depth stencil to match vanilla hologram rendering
			// Vanilla uses StencilValue=5, StencilWriteMask=0 (NOT 1 and 255)
			// This creates the hologram glow effect while keeping the native material
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(5);
			Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
			
			// Force render state update
			Mesh->MarkRenderStateDirty();
			Mesh->SetVisibility(true, true);
			Mesh->SetHiddenInGame(false);
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎨 PIPE SetPlacementMaterialState: State=%d, SplineMeshes=%d (using native material with StencilValue=5)"),
		(int32)materialState,
		MeshComps.Num());
}

void ASFPipelineHologram::SetupPipeSpline(UFGPipeConnectionComponentBase* StartConnector, UFGPipeConnectionComponentBase* EndConnector)
{
	if (!StartConnector || !EndConnector)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("SetupPipeSpline: Invalid connectors"));
		return;
	}

	const FVector StartPos = StartConnector->GetConnectorLocation();
	const FVector EndPos = EndConnector->GetConnectorLocation();
	const FVector StartNormal = StartConnector->GetConnectorNormal();
	const FVector EndNormal = EndConnector->GetConnectorNormal();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE SPLINE SETUP: %s"), *GetName());
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Start: %s (normal: %s)"), *StartPos.ToString(), *StartNormal.ToString());
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   End: %s (normal: %s)"), *EndPos.ToString(), *EndNormal.ToString());

	const float Distance = FVector::Dist(StartPos, EndPos);
	const float SmallTangent = 50.0f;
	const float LargeTangent = Distance * 0.435f;
	const float FlatSectionLength = Distance * 0.047f;
	const float TransitionOffset = Distance * 0.070f;
	FVector Direction = (EndPos - StartPos).GetSafeNormal();
	
	// ========================================
	// BUILD SPLINE: Perfect 6-point vanilla curved spline
	// ========================================
	mBuildSplineData.Empty();
	
	// Point 0: Start connector
	FSplinePointData BuildPoint0;
	BuildPoint0.Location = StartPos;
	BuildPoint0.ArriveTangent = StartNormal * SmallTangent;
	BuildPoint0.LeaveTangent = StartNormal * SmallTangent;
	mBuildSplineData.Add(BuildPoint0);
	
	// Point 1: Flat section near start
	FSplinePointData BuildPoint1;
	BuildPoint1.Location = StartPos + StartNormal * FlatSectionLength;
	BuildPoint1.ArriveTangent = StartNormal * SmallTangent;
	BuildPoint1.LeaveTangent = StartNormal * (SmallTangent * 0.99f);
	mBuildSplineData.Add(BuildPoint1);
	
	// Point 2: Transition point
	FSplinePointData BuildPoint2;
	BuildPoint2.Location = StartPos + StartNormal * TransitionOffset;
	BuildPoint2.ArriveTangent = StartNormal * SmallTangent;
	BuildPoint2.LeaveTangent = Direction * LargeTangent;
	mBuildSplineData.Add(BuildPoint2);
	
	// Point 3: Middle curve point
	FSplinePointData BuildPoint3;
	BuildPoint3.Location = EndPos + EndNormal * TransitionOffset;
	BuildPoint3.ArriveTangent = Direction * LargeTangent;
	BuildPoint3.LeaveTangent = -EndNormal * SmallTangent;
	mBuildSplineData.Add(BuildPoint3);
	
	// Point 4: Flat section near end
	FSplinePointData BuildPoint4;
	BuildPoint4.Location = EndPos + EndNormal * FlatSectionLength;
	BuildPoint4.ArriveTangent = -EndNormal * (SmallTangent * 0.99f);
	BuildPoint4.LeaveTangent = -EndNormal * SmallTangent;
	mBuildSplineData.Add(BuildPoint4);
	
	// Point 5: End connector
	FSplinePointData BuildPoint5;
	BuildPoint5.Location = EndPos;
	BuildPoint5.ArriveTangent = -EndNormal * SmallTangent;
	BuildPoint5.LeaveTangent = -EndNormal * SmallTangent;
	mBuildSplineData.Add(BuildPoint5);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ BUILD SPLINE: 6-point vanilla curve (distance=%.1f cm)"), Distance);
	
	// ========================================
	// PREVIEW SPLINE: Simple 2-point straight line
	// ========================================
	mSplineData.Empty();
	
	// Point 0: Start
	FSplinePointData PreviewStart;
	PreviewStart.Location = StartPos;
	PreviewStart.ArriveTangent = StartNormal * 100.0f;
	PreviewStart.LeaveTangent = StartNormal * 100.0f;
	mSplineData.Add(PreviewStart);
	
	// Point 1: End
	FSplinePointData PreviewEnd;
	PreviewEnd.Location = EndPos;
	PreviewEnd.ArriveTangent = -EndNormal * 100.0f;
	PreviewEnd.LeaveTangent = -EndNormal * 100.0f;
	mSplineData.Add(PreviewEnd);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ✅ PREVIEW SPLINE: 2-point straight line (distance=%.1f cm)"), Distance);

	// Position actor and convert preview spline to local space
	SetActorLocation(StartPos);

	if (mSplineComponent)
	{
		mSplineComponent->SetWorldLocation(StartPos);

		// Convert 2-point preview spline from world to local
		const FTransform CompTransform = mSplineComponent->GetComponentTransform();
		
		for (int32 i = 0; i < mSplineData.Num(); i++)
		{
			FVector WorldLocation = mSplineData[i].Location;
			FVector WorldArriveTangent = mSplineData[i].ArriveTangent;
			FVector WorldLeaveTangent = mSplineData[i].LeaveTangent;
			
			mSplineData[i].Location = CompTransform.InverseTransformPosition(WorldLocation);
			mSplineData[i].ArriveTangent = CompTransform.InverseTransformVector(WorldArriveTangent);
			mSplineData[i].LeaveTangent = CompTransform.InverseTransformVector(WorldLeaveTangent);
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Preview Point %d: Local=%s"), 
				i, *mSplineData[i].Location.ToString());
		}
		
		// Also convert 6-point build spline to local (for ConfigureComponents later)
		for (int32 i = 0; i < mBuildSplineData.Num(); i++)
		{
			FVector WorldLocation = mBuildSplineData[i].Location;
			FVector WorldArriveTangent = mBuildSplineData[i].ArriveTangent;
			FVector WorldLeaveTangent = mBuildSplineData[i].LeaveTangent;
			
			mBuildSplineData[i].Location = CompTransform.InverseTransformPosition(WorldLocation);
			mBuildSplineData[i].ArriveTangent = CompTransform.InverseTransformVector(WorldArriveTangent);
			mBuildSplineData[i].LeaveTangent = CompTransform.InverseTransformVector(WorldLeaveTangent);
		}

		mSplineComponent->SetVisibility(true, true);
	}

	// Push updated spline data via the base class (accessible protected method)
	AFGSplineHologram::UpdateSplineComponent();

	SetActorHiddenInGame(false);
}

void ASFPipelineHologram::ConfigureComponents(AFGBuildable* inBuildable) const
{
	// CRITICAL: Swap in 6-point build spline before configuring!
	// Preview uses 2-point, but built pipe needs perfect 6-point curve
	if (mBuildSplineData.Num() > 0)
	{
		// Temporarily replace preview spline with build spline
		TArray<FSplinePointData> OriginalSplineData = mSplineData;
		const_cast<ASFPipelineHologram*>(this)->mSplineData = mBuildSplineData;
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🏗️ BUILDING PIPE: Swapped to 6-point build spline (%d points)"), mBuildSplineData.Num());
		
		// Call base class with 6-point spline
		Super::ConfigureComponents(inBuildable);
		
		// Restore preview spline (though hologram is about to be destroyed anyway)
		const_cast<ASFPipelineHologram*>(this)->mSplineData = OriginalSplineData;
	}
	else
	{
		// No build spline data (shouldn't happen, but fallback to base)
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" BUILDING PIPE: No build spline data! Using preview spline"));
		Super::ConfigureComponents(inBuildable);
	}
	
	// ========================================================================
	// PRE-TICK WIRING: Make pipe connections DURING construction
	// Similar to belt/lift ConfigureComponents, but for pipe connections
	// ========================================================================
	
	AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(inBuildable);
	if (!Pipeline)
	{
		return;
	}
	
	// Get hologram data for connection targets
	const FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
	if (!HoloData || (HoloData->Conn0TargetCloneId.IsEmpty() && HoloData->Conn1TargetCloneId.IsEmpty()))
	{
		return;
	}
	
	// Get services for looking up built actors
	USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld());
	USFExtendService* ExtendService = SmartSubsystem ? SmartSubsystem->GetExtendService() : nullptr;
	
	if (!ExtendService)
	{
		return;
	}
	
	// Get pipe connections
	TArray<UFGPipeConnectionComponentBase*> PipeConns;
	Pipeline->GetComponents<UFGPipeConnectionComponentBase>(PipeConns);
	
	UFGPipeConnectionComponentBase* Conn0 = PipeConns.Num() > 0 ? PipeConns[0] : nullptr;
	UFGPipeConnectionComponentBase* Conn1 = PipeConns.Num() > 1 ? PipeConns[1] : nullptr;
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: %s checking targets: Conn0→%s.%s, Conn1→%s.%s"),
		*Pipeline->GetName(),
		*HoloData->Conn0TargetCloneId, *HoloData->Conn0TargetConnectorName.ToString(),
		*HoloData->Conn1TargetCloneId, *HoloData->Conn1TargetConnectorName.ToString());
	
	// === CONN0 CONNECTION ===
	if (Conn0 && !Conn0->IsConnected() && !HoloData->Conn0TargetCloneId.IsEmpty())
	{
		AFGBuildable* TargetBuildable = nullptr;
		if (HoloData->Conn0TargetCloneId.StartsWith(TEXT("source:")))
		{
			FString SourceActorName = HoloData->Conn0TargetCloneId.Mid(7);
			TargetBuildable = ExtendService->GetSourceBuildableByName(SourceActorName);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
				*SourceActorName, TargetBuildable ? *TargetBuildable->GetName() : TEXT("NOT FOUND"));
		}
		else
		{
			TargetBuildable = ExtendService->GetBuiltActorByCloneId(HoloData->Conn0TargetCloneId);
		}
		
		if (TargetBuildable)
		{
			TArray<UFGPipeConnectionComponentBase*> TargetConns;
			TargetBuildable->GetComponents<UFGPipeConnectionComponentBase>(TargetConns);
			
			bool bFoundConnector = false;
			for (UFGPipeConnectionComponentBase* TargetConn : TargetConns)
			{
				if (TargetConn && TargetConn->GetFName() == HoloData->Conn0TargetConnectorName)
				{
					bFoundConnector = true;
					if (!TargetConn->IsConnected())
					{
						Conn0->SetConnection(TargetConn);
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ✅ Connected %s.Conn0 → %s.%s"),
							*Pipeline->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName());
					}
					else
					{
						// Target connector is already connected - this is a problem for lanes connecting to source
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ⚠️ %s.Conn0 target %s.%s is ALREADY CONNECTED to %s"),
							*Pipeline->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName(),
							TargetConn->GetConnection() ? *TargetConn->GetConnection()->GetOwner()->GetName() : TEXT("NULL"));
					}
					break;
				}
			}
			if (!bFoundConnector)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ⚠️ %s.Conn0 target connector '%s' NOT FOUND on %s"),
					*Pipeline->GetName(), *HoloData->Conn0TargetConnectorName.ToString(), *TargetBuildable->GetName());
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ⚠️ %s.Conn0 target buildable '%s' NOT FOUND"),
				*Pipeline->GetName(), *HoloData->Conn0TargetCloneId);
		}
	}
	
	// === CONN1 CONNECTION ===
	if (Conn1 && !Conn1->IsConnected() && !HoloData->Conn1TargetCloneId.IsEmpty())
	{
		AFGBuildable* TargetBuildable = nullptr;
		if (HoloData->Conn1TargetCloneId.StartsWith(TEXT("source:")))
		{
			FString SourceActorName = HoloData->Conn1TargetCloneId.Mid(7);
			TargetBuildable = ExtendService->GetSourceBuildableByName(SourceActorName);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
				*SourceActorName, TargetBuildable ? *TargetBuildable->GetName() : TEXT("NOT FOUND"));
		}
		else
		{
			TargetBuildable = ExtendService->GetBuiltActorByCloneId(HoloData->Conn1TargetCloneId);
		}
		
		if (TargetBuildable)
		{
			TArray<UFGPipeConnectionComponentBase*> TargetConns;
			TargetBuildable->GetComponents<UFGPipeConnectionComponentBase>(TargetConns);
			
			for (UFGPipeConnectionComponentBase* TargetConn : TargetConns)
			{
				if (TargetConn && TargetConn->GetFName() == HoloData->Conn1TargetConnectorName)
				{
					if (!TargetConn->IsConnected())
					{
						Conn1->SetConnection(TargetConn);
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ✅ Connected %s.Conn1 → %s.%s"),
							*Pipeline->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName());
					}
					break;
				}
			}
		}
	}
	
	// ========================================================================
	// PIPE NETWORK MERGE: Critical for fluid flow!
	// After establishing connections, merge the pipe networks so fluid can flow
	// between the source manifold and the clone manifold.
	// ========================================================================
	UWorld* World = GetWorld();
	if (World && Conn0 && Conn1)
	{
		AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
		if (PipeSubsystem)
		{
			// Cast to UFGPipeConnectionComponent to access GetPipeNetworkID()
			UFGPipeConnectionComponent* PipeConn0 = Cast<UFGPipeConnectionComponent>(Conn0);
			UFGPipeConnectionComponent* PipeConn1 = Cast<UFGPipeConnectionComponent>(Conn1);
			
			if (PipeConn0 && PipeConn1)
			{
				int32 Network0 = PipeConn0->GetPipeNetworkID();
				int32 Network1 = PipeConn1->GetPipeNetworkID();
				
				if (Network0 != Network1 && Network0 != INDEX_NONE && Network1 != INDEX_NONE)
				{
					AFGPipeNetwork* Net0 = PipeSubsystem->FindPipeNetwork(Network0);
					AFGPipeNetwork* Net1 = PipeSubsystem->FindPipeNetwork(Network1);
					
					if (Net0 && Net1 && Net0->IsValidLowLevel() && Net1->IsValidLowLevel())
					{
						Net0->MergeNetworks(Net1);
						Net0->MarkForFullRebuild();
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: Merged networks %d and %d for %s"),
							Network0, Network1, *Pipeline->GetName());
					}
				}
				else if (Network0 != INDEX_NONE || Network1 != INDEX_NONE)
				{
					// At least one network exists - mark it for rebuild
					int32 ValidNetworkID = (Network0 != INDEX_NONE) ? Network0 : Network1;
					AFGPipeNetwork* ValidNetwork = PipeSubsystem->FindPipeNetwork(ValidNetworkID);
					if (ValidNetwork && ValidNetwork->IsValidLowLevel())
					{
						ValidNetwork->MarkForFullRebuild();
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: Marked network %d for rebuild for %s"),
							ValidNetworkID, *Pipeline->GetName());
					}
				}
			}
		}
	}
}

void ASFPipelineHologram::SetSplineDataAndUpdate(const TArray<FSplinePointData>& InSplineData)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE SetSplineDataAndUpdate: Setting %d spline points on %s"), InSplineData.Num(), *GetName());
	
	// Copy spline data
	mSplineData = InSplineData;
	
	// Also copy to build spline data (for EXTEND, we use the same data for preview and build)
	mBuildSplineData = InSplineData;
	
	// CRITICAL: Store backup in registry so ConfigureActor can restore if vanilla resets it
	FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
	if (!HoloData)
	{
		HoloData = USFHologramDataRegistry::AttachData(this);
	}
	if (HoloData)
	{
		HoloData->bHasBackupSplineData = true;
		HoloData->BackupSplineData = InSplineData;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE SetSplineDataAndUpdate: Stored %d points in backup registry"), InSplineData.Num());
	}
	
	// Update spline component
	if (mSplineComponent)
	{
		AFGSplineHologram::UpdateSplineComponent();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE SetSplineDataAndUpdate: UpdateSplineComponent called, spline length=%.1f cm"), 
			mSplineComponent->GetSplineLength());
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE SetSplineDataAndUpdate: No spline component!"));
	}
}

void ASFPipelineHologram::ForceApplyHologramMaterial()
{
	// Configure spline mesh components for hologram rendering
	// CRITICAL: Vanilla pipe holograms use the NATIVE pipe material (not hologram material)
	// The hologram effect comes from custom depth stencil settings, not material override
	TArray<USplineMeshComponent*> SplineMeshes;
	GetComponents<USplineMeshComponent>(SplineMeshes);
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎨 PIPE ForceApplyHologramMaterial: Configuring %d spline meshes (using native material with StencilValue=5)"), 
		SplineMeshes.Num());
	
	for (int32 i = 0; i < SplineMeshes.Num(); i++)
	{
		USplineMeshComponent* SplineMesh = SplineMeshes[i];
		if (SplineMesh)
		{
			// CRITICAL: Set custom depth stencil to match vanilla hologram rendering
			// Vanilla uses StencilValue=5, StencilWriteMask=0 (NOT 1 and 255)
			// This creates the hologram glow effect while keeping the native material
			SplineMesh->SetRenderCustomDepth(true);
			SplineMesh->SetCustomDepthStencilValue(5);
			SplineMesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
			SplineMesh->MarkRenderStateDirty();
			
			UStaticMesh* Mesh = SplineMesh->GetStaticMesh();
			UMaterialInterface* Mat = SplineMesh->GetMaterial(0);
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   [%d] Mesh=%s, Material=%s, Visible=%d, Hidden=%d"),
				i,
				Mesh ? *Mesh->GetName() : TEXT("NULL"),
				Mat ? *Mat->GetName() : TEXT("NULL"),
				SplineMesh->IsVisible(),
				SplineMesh->bHiddenInGame);
		}
	}
}

void ASFPipelineHologram::TriggerMeshGeneration()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE TriggerMeshGeneration: %s - mSplineData has %d points"), *GetName(), mSplineData.Num());
	
	if (!mSplineComponent)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE TriggerMeshGeneration: %s - mSplineComponent is NULL!"), *GetName());
		return;
	}

	// Push mSplineData into spline component
	AFGSplineHologram::UpdateSplineComponent();

	// Log spline stats for debugging
	const int32 PointCount = mSplineComponent->GetNumberOfSplinePoints();
	const float SplineLength = mSplineComponent->GetSplineLength();
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE TriggerMeshGeneration: %s - %d spline points, %.1f cm length"), *GetName(), PointCount, SplineLength);
	
	// Get spline points for geometry update
	if (PointCount < 2)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("   PIPE TriggerMeshGeneration: Not enough spline points (%d)"), PointCount);
		return;
	}
	
	// Get pipe mesh, material, and mesh length from build class CDO - this ensures they match the actual tier
	UStaticMesh* PipeMesh = nullptr;
	UMaterialInterface* PipeMaterial = nullptr;
	float MeshLength = 200.0f; // Default fallback
	
	if (mBuildClass)
	{
		if (AFGBuildablePipeline* PipeCDO = Cast<AFGBuildablePipeline>(mBuildClass->GetDefaultObject()))
		{
			PipeMesh = PipeCDO->GetSplineMesh();
			PipeMaterial = PipeCDO->mSplineMeshMaterial;
			MeshLength = PipeCDO->GetMeshLength();
			UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE TriggerMeshGeneration: Got from CDO: Mesh=%s, Material=%s, MeshLength=%.1f"), 
				PipeMesh ? *PipeMesh->GetName() : TEXT("NULL"),
				PipeMaterial ? *PipeMaterial->GetName() : TEXT("NULL"),
				MeshLength);
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE TriggerMeshGeneration: mBuildClass=%s but CDO cast failed"), *mBuildClass->GetName());
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE TriggerMeshGeneration: mBuildClass is NULL!"));
	}
	
	// Fallback to hardcoded generic pipe mesh if CDO lookup failed
	if (!PipeMesh)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE TriggerMeshGeneration: CDO mesh lookup failed, using fallback"));
		PipeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Mesh/SM_Pipe.SM_Pipe"));
	}
	if (!PipeMesh)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("   PIPE TriggerMeshGeneration: Failed to load pipe mesh!"));
		return;
	}
	
	// Ensure mesh length is valid (use CDO value, not bounds calculation)
	if (MeshLength <= 0.0f)
	{
		MeshLength = 200.0f; // Default pipe mesh length
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE TriggerMeshGeneration: Mesh=%s, MeshLength=%.1f cm, SplineLength=%.1f cm"), 
		*PipeMesh->GetName(), MeshLength, SplineLength);
	
	// CRITICAL FIX: Calculate segments based on spline length / mesh length (like vanilla)
	// NOT based on spline point count - that causes severe stretching
	const int32 RequiredSegments = FMath::Max(1, FMath::CeilToInt(SplineLength / MeshLength));
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE TriggerMeshGeneration: Need %d segments (%.1f cm each) for %.1f cm spline"), 
		RequiredSegments, SplineLength / RequiredSegments, SplineLength);
	
	// Get existing mesh components - base class creates default ones that may not update properly
	TArray<USplineMeshComponent*> MeshComps;
	GetComponents<USplineMeshComponent>(MeshComps);
	
	// CRITICAL FIX: Destroy existing mesh components from base class - they don't update properly
	for (USplineMeshComponent* OldMesh : MeshComps)
	{
		if (OldMesh)
		{
			OldMesh->DestroyComponent();
		}
	}
	MeshComps.Empty();

	// Create fresh mesh components
	while (MeshComps.Num() < RequiredSegments)
	{
		USplineMeshComponent* NewMesh = NewObject<USplineMeshComponent>(this);
		if (NewMesh)
		{
			NewMesh->SetStaticMesh(PipeMesh);
			NewMesh->SetMobility(EComponentMobility::Movable);
			NewMesh->SetForwardAxis(ESplineMeshAxis::X);
			
			// Apply tier-specific material from CDO (e.g., MI_PipeMK2 for Mk2 pipes)
			if (PipeMaterial)
			{
				NewMesh->SetMaterial(0, PipeMaterial);
			}
			
			NewMesh->RegisterComponent();
			NewMesh->AttachToComponent(mSplineComponent, FAttachmentTransformRules::SnapToTargetIncludingScale);
			NewMesh->SetVisibility(true, true);
			
			MeshComps.Add(NewMesh);
		}
	}
	
	// Calculate segment length for even distribution
	const float SegmentLength = SplineLength / RequiredSegments;
	
	// Update each segment by sampling the spline at regular distance intervals
	for (int32 SegmentIdx = 0; SegmentIdx < RequiredSegments && SegmentIdx < MeshComps.Num(); SegmentIdx++)
	{
		USplineMeshComponent* MeshComp = MeshComps[SegmentIdx];
		if (MeshComp)
		{
			MeshComp->SetStaticMesh(PipeMesh);
			MeshComp->SetForwardAxis(ESplineMeshAxis::X);
			
			// Sample spline at distance intervals (not at spline points)
			const float StartDist = SegmentIdx * SegmentLength;
			const float EndDist = (SegmentIdx + 1) * SegmentLength;
			
			FVector StartPos = mSplineComponent->GetLocationAtDistanceAlongSpline(StartDist, ESplineCoordinateSpace::Local);
			FVector EndPos = mSplineComponent->GetLocationAtDistanceAlongSpline(EndDist, ESplineCoordinateSpace::Local);
			FVector StartTangent = mSplineComponent->GetTangentAtDistanceAlongSpline(StartDist, ESplineCoordinateSpace::Local);
			FVector EndTangent = mSplineComponent->GetTangentAtDistanceAlongSpline(EndDist, ESplineCoordinateSpace::Local);
			
			// Normalize tangents to segment length for proper mesh scaling
			StartTangent = StartTangent.GetSafeNormal() * SegmentLength;
			EndTangent = EndTangent.GetSafeNormal() * SegmentLength;
			
			MeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
			MeshComp->SetVisibility(true, true);
			MeshComp->MarkRenderStateDirty();
			
			if (SegmentIdx == 0)
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE Segment[0]: Start=%s End=%s (dist %.1f-%.1f)"), 
					*StartPos.ToString(), *EndPos.ToString(), StartDist, EndDist);
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔧 PIPE TriggerMeshGeneration: Created %d segments of %.1f cm each"), 
		MeshComps.Num(), SegmentLength);
	
	// CRITICAL: Apply hologram material to newly created mesh components
	// Without this, the meshes render with default black material
	SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
	
	// Check actor position
	FVector ActorLoc = GetActorLocation();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   PIPE: Actor location: %s"), *ActorLoc.ToString());
	
	// Check if components are actually attached and positioned
	for (int32 i = 0; i < MeshComps.Num() && i < 2; i++)
	{
		if (MeshComps[i])
		{
			FVector CompWorldLoc = MeshComps[i]->GetComponentLocation();
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      MeshComp[%d] World Location: %s"), i, *CompWorldLoc.ToString());
		}
	}
}

void ASFPipelineHologram::ConfigureActor(class AFGBuildable* inBuildable) const
{
	// CRITICAL: Same pattern as ASFConveyorBeltHologram::ConfigureActor
	// Vanilla can clear mSplineData during construction processing.
	// We detect this and restore from backup before passing to Super.
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Hologram=%s, Buildable=%s"), 
		*GetName(), inBuildable ? *inBuildable->GetName() : TEXT("NULL"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: mSplineData has %d points"), mSplineData.Num());
	
	// Log current spline data state
	for (int32 i = 0; i < mSplineData.Num() && i < 3; i++)
	{
		const FSplinePointData& Point = mSplineData[i];
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor:   Point[%d] Location=%s"), 
			i, *Point.Location.ToString());
	}
	
	// CRITICAL FIX: Check if we have backup data that should be restored
	// This handles the case where mSplineData was reset to default 2-point spline by vanilla
	if (const FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Registry entry found - bHasBackup=%d, BackupPoints=%d"),
			HoloData->bHasBackupSplineData ? 1 : 0, HoloData->BackupSplineData.Num());
		
		// If backup has more points than current data, OR if current data appears wrong, we need to restore
		// This detects the case where:
		// 1. 6+ point spline was reset to 2-point default
		// 2. 2-point manifold spline was zeroed out
		// 3. 2-point manifold spline was replaced with vanilla's 200cm default spline
		// 4. Spline direction changed (e.g., Y-axis pipe replaced with X-axis default)
		bool bNeedsRestore = false;
		if (HoloData->bHasBackupSplineData && HoloData->BackupSplineData.Num() > 0)
		{
			if (HoloData->BackupSplineData.Num() > mSplineData.Num())
			{
				// More points in backup - definitely reset
				bNeedsRestore = true;
			}
			else if (mSplineData.Num() >= 2 && HoloData->BackupSplineData.Num() >= 2)
			{
				// Same point count - check if current data differs significantly from backup
				// This handles:
				// - Zeroed spline (all points at origin)
				// - Vanilla default 200cm spline replacing our manifold spline
				// - Spline direction changed (backup goes along Y, current goes along X)
				const FVector& CurrentEnd = mSplineData[1].Location;
				const FVector& BackupEnd = HoloData->BackupSplineData[1].Location;
				
				float BackupLength = BackupEnd.Size();
				float CurrentLength = CurrentEnd.Size();
				
				// Check if endpoints differ significantly (catches direction changes)
				// Vanilla default is 200cm along X-axis, our pipes can be any direction
				if (!CurrentEnd.Equals(BackupEnd, 10.0f))
				{
					// Endpoints differ - check if this is a meaningful difference
					// Restore if: backup has any significant length AND current doesn't match
					if (BackupLength > 100.0f)
					{
						bNeedsRestore = true;
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Backup end=%s (%.1f cm), Current end=%s (%.1f cm) - differs, will restore"),
							*BackupEnd.ToString(), BackupLength, *CurrentEnd.ToString(), CurrentLength);
					}
				}
				
				if (!bNeedsRestore && CurrentEnd.IsNearlyZero(1.0f) && !BackupEnd.IsNearlyZero(1.0f))
				{
					// Current is zeroed but backup isn't
					bNeedsRestore = true;
				}
			}
		}
		
		if (bNeedsRestore)
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE ConfigureActor: mSplineData was reset! Restoring %d points from backup (had %d)"), 
				HoloData->BackupSplineData.Num(), mSplineData.Num());
			
			// Restore spline data (need to cast away const for this emergency restore)
			ASFPipelineHologram* MutableThis = const_cast<ASFPipelineHologram*>(this);
			MutableThis->mSplineData = HoloData->BackupSplineData;
			
			// CRITICAL FIX: Also update the spline component!
			// Super::ConfigureActor() uses the SplineComponent, not mSplineData directly.
			// Same pattern as ASFConveyorBeltHologram::ConfigureActor
			if (MutableThis->mSplineComponent)
			{
				MutableThis->AFGSplineHologram::UpdateSplineComponent();
			}
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Restored mSplineData now has %d points"), 
				mSplineData.Num());
			
			// Log the restored points
			for (int32 i = 0; i < mSplineData.Num() && i < 4; i++)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor:   RestoredPoint[%d] Location=%s"), 
					i, *mSplineData[i].Location.ToString());
			}
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: No registry entry found for this hologram"));
	}
	
	if (mSplineComponent)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: SplineComponent has %d points, length=%.1f cm"),
			mSplineComponent->GetNumberOfSplinePoints(), mSplineComponent->GetSplineLength());
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🔧 PIPE ConfigureActor: mSplineComponent is NULL!"));
	}
	
	// Call parent to configure the buildable
	Super::ConfigureActor(inBuildable);
	
	// Log after parent call to see if buildable got the spline data
	if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(inBuildable))
	{
		const TArray<FSplinePointData>& PipeSplineData = Pipeline->GetSplinePointData();
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: AFTER Super - Buildable has %d spline points"), 
			PipeSplineData.Num());
		for (int32 i = 0; i < PipeSplineData.Num() && i < 3; i++)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🔧 PIPE ConfigureActor:   BuildablePoint[%d] Location=%s"), 
				i, *PipeSplineData[i].Location.ToString());
		}
	}
}

TArray<FItemAmount> ASFPipelineHologram::GetCost(bool includeChildren) const
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE GetCost() CALLED on %s (includeChildren=%d)"), *GetName(), includeChildren);
	
	// Get base cost from parent (should be empty for pipes, but call it anyway)
	TArray<FItemAmount> TotalCost = Super::GetCost(includeChildren);
	
	// Calculate pipe cost based on spline length
	if (mSplineComponent)
	{
		float PipeLengthCm = mSplineComponent->GetSplineLength();
		float LengthInMeters = PipeLengthCm / 100.0f;
		
		// If the spline hasn't been initialized yet, try to restore from backup
		if (LengthInMeters <= KINDA_SMALL_NUMBER)
		{
			// DEFENSIVE RESTORATION: Check if we have backup data that should be restored
			if (const FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this))
			{
				if (HoloData->bHasBackupSplineData && HoloData->BackupSplineData.Num() >= 2)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE: Spline length is zero but backup has %d points - RESTORING!"), 
						HoloData->BackupSplineData.Num());
					
					// Restore spline data
					ASFPipelineHologram* MutableThis = const_cast<ASFPipelineHologram*>(this);
					MutableThis->mSplineData = HoloData->BackupSplineData;
					MutableThis->AFGSplineHologram::UpdateSplineComponent();
					
					// Recalculate length after restoration
					PipeLengthCm = mSplineComponent->GetSplineLength();
					LengthInMeters = PipeLengthCm / 100.0f;
					
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE: After restoration, spline length = %.1f cm"), PipeLengthCm);
					MutableThis->TriggerMeshGeneration();
				}
			}
			
			// If still zero after restoration attempt, skip cost calculation
			if (LengthInMeters <= KINDA_SMALL_NUMBER)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE: Spline length is zero - skipping pipe cost calculation"));
				return TotalCost;
			}
		}
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE: Spline length = %.1f cm (%.1f m)"), PipeLengthCm, LengthInMeters);
		
		// Get pipe class and determine tier
		if (mBuildClass)
		{
			// Extract tier from class name (e.g., Build_PipelineMK2_C -> tier 2)
			FString ClassName = mBuildClass->GetName();
			int32 PipeTier = 1; // Default Mk1
			
			// Parse tier from class name
			if (ClassName.Contains(TEXT("MK1")) || ClassName.Contains(TEXT("Mk1")) || ClassName.Contains(TEXT("Pipeline_C")))
			{
				PipeTier = 1;
			}
			else if (ClassName.Contains(TEXT("MK2")) || ClassName.Contains(TEXT("Mk2")))
			{
				PipeTier = 2;
			}
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE: Class = %s, Tier = Mk%d"), *ClassName, PipeTier);
			
			// Get recipe for this pipe class
			UWorld* World = GetWorld();
			if (World)
			{
				AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
				if (RecipeManager)
				{
					UClass* BuildClass = mBuildClass;
					TSubclassOf<AFGBuildable> PipeBuildableClass;
					if (BuildClass && BuildClass->IsChildOf(AFGBuildable::StaticClass()))
					{
						PipeBuildableClass = TSubclassOf<AFGBuildable>(BuildClass);
					}
					
					if (PipeBuildableClass)
					{
						// Get all recipes that produce this pipe
						TArray<TSubclassOf<UFGRecipe>> AvailableRecipes;
						RecipeManager->GetAllAvailableRecipes(AvailableRecipes);
						
						TSubclassOf<UFGRecipe> PipeRecipe = nullptr;
						for (const TSubclassOf<UFGRecipe>& Recipe : AvailableRecipes)
						{
							if (Recipe)
							{
								for (const FItemAmount& Product : UFGRecipe::GetProducts(Recipe))
								{
									if (Product.ItemClass && Product.ItemClass->IsChildOf(AFGBuildable::StaticClass()))
									{
										TSubclassOf<AFGBuildable> ProductBuildable = TSubclassOf<AFGBuildable>(Product.ItemClass);
										if (ProductBuildable == PipeBuildableClass)
										{
											PipeRecipe = Recipe;
											break;
										}
									}
								}
								if (PipeRecipe) break;
							}
						}
						
						if (PipeRecipe)
						{
							// Get recipe ingredients (cost per meter) - iterate directly to avoid copy
							const TArray<FItemAmount> Ingredients = UFGRecipe::GetIngredients(PipeRecipe);

							UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE: Found recipe with %d ingredients"), Ingredients.Num());

							// Calculate total cost based on pipe length
							for (const FItemAmount& Ingredient : Ingredients)
							{
								if (Ingredient.ItemClass)
								{
									// Calculate amount needed for this pipe length
									int32 AmountNeeded = FMath::CeilToInt(Ingredient.Amount * LengthInMeters);
									
									UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE:   %s: %.1f per meter × %.1f meters = %d total"),
										*Ingredient.ItemClass->GetName(), Ingredient.Amount, LengthInMeters, AmountNeeded);
									
									// Add to total cost
									bool bFound = false;
									for (FItemAmount& ExistingCost : TotalCost)
									{
										if (ExistingCost.ItemClass == Ingredient.ItemClass)
										{
											ExistingCost.Amount += AmountNeeded;
											bFound = true;
											break;
										}
									}
									if (!bFound)
									{
										TotalCost.Add(FItemAmount(Ingredient.ItemClass, AmountNeeded));
									}
								}
							}
						}
						else
						{
							UE_LOG(LogSmartFoundations, Warning, TEXT("💰 PIPE: No recipe found for pipe class %s"), *ClassName);
						}
					}
				}
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 PIPE GetCost() RETURNING %d item types"), TotalCost.Num());
	return TotalCost;
}
