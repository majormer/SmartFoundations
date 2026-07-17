// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Holograms/Logistics/SFPipelineHologram.h"
#include "SmartFoundations.h"
#include "SFLogMacros.h"                       // [#168] LogSmartAutoConnect for seam-pipe wiring
#include "Hologram/FGBlueprintHologram.h"      // [#168] blueprint-seam parent discriminator
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipeBase.h"   // #405: common base of fluid pipe + hypertube — read the spline mesh by base cast
#include "Components/SplineMeshComponent.h"
#include "Hologram/FGSplineHologram.h"
#include "FGPipeConnectionComponent.h"
#include "FGPipeSubsystem.h"
#include "FGPipeNetwork.h"
#include "Data/SFHologramDataRegistry.h"
#include "Features/Extend/SFExtendService.h"
#include "Subsystem/SFSubsystem.h"
#include "FGConstructDisqualifier.h"
#include "Kismet/GameplayStatics.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Hologram/HologramHelpers.h"                // [#383] FHologramAStarNode for Noodle (PathFindingRouteSpline)
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
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   PIPE BeginPlay: Found %d SplineMeshComponents after Super::BeginPlay()"), MeshComps.Num());
	
	// Check mesh and material status
	for (int32 i = 0; i < MeshComps.Num(); i++)
	{
		USplineMeshComponent* MeshComp = MeshComps[i];
		if (MeshComp)
		{
			UStaticMesh* Mesh = MeshComp->GetStaticMesh();
			UMaterialInterface* Material = MeshComp->GetMaterial(0);
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   PIPE BeginPlay: SplineMesh[%d] - Mesh=%s, Material=%s, Visible=%s"),
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
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   PIPE BeginPlay: mSplineComponent has %d points, %.1f cm length"), PointCount, SplineLength);
	}
	else
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("   PIPE BeginPlay: mSplineComponent is NULL!"));
	}
}

void ASFPipelineHologram::CheckValidPlacement()
{
	if (GetParentHologram())
	{
		// [#437] A routed-invalid shape wins over the normal child preview mirroring: paint the
		// child invalid with vanilla's OWN disqualifier so the player sees the same "Invalid Pipe
		// Shape" a hand-built pipe of this shape produces, and firing is blocked until the routing
		// mode changes or the hologram moves somewhere the shape is valid.
		if (bRoutedShapeInvalid)
		{
			ResetConstructDisqualifiers();
			AddConstructDisqualifier(UFGCDPipeInvalidShape::StaticClass());
			SetPlacementMaterialState(EHologramMaterialState::HMS_ERROR);
			return;
		}

		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("Pipe child preview - skipping placement validation"));
		// The build gun derives preview red/cyan from construct disqualifiers, not from
		// SetPlacementMaterialState. Carry the parent's "unaffordable" disqualifier so the
		// pipe preview turns red; cleared when affordable so it returns to cyan.
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

AActor* ASFPipelineHologram::Construct(TArray<AActor*>& out_children, FNetConstructionID constructionID)
{
	// CRITICAL: Prevent double-construction
	// Construct() can be called by both vanilla child system AND manual build code in SFSubsystem.
	// If both call Construct(), we get duplicate pipes. This flag prevents the second call.
	// IMPORTANT: Return the previously built actor, NOT nullptr - vanilla crashes on nullptr!
	if (bHasBeenConstructed)
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE: Skipping duplicate Construct() for %s (already built, returning cached actor)"), *GetName());
		return ConstructedActor.Get();
	}
	bHasBeenConstructed = true;
	
	// Check if this is an EXTEND, STACKABLE, or PIPE AUTO-CONNECT child hologram
	bool bIsExtendChild = Tags.Contains(FName(TEXT("SF_ExtendChild")));
	bool bIsStackableChild = Tags.Contains(FName(TEXT("SF_StackableChild")));
	bool bIsPipeAutoConnectChild = Tags.Contains(FName(TEXT("SF_PipeAutoConnectChild")));
	
	if (bIsExtendChild || bIsStackableChild || bIsPipeAutoConnectChild)
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 %s: Pipe hologram %s Construct() called - building as child"), 
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
			
			FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this);
			
			// Log the mapping for debugging
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 %s: ✅ Pipe hologram %s → Buildable %s (ID: %u), discarded %d child actors"), 
				bIsStackableChild ? TEXT("STACKABLE") : (bIsPipeAutoConnectChild ? TEXT("PIPE AUTO-CONNECT") : TEXT("EXTEND")),
				*GetName(), *BuiltActor->GetName(), BuiltActor->GetUniqueID(), DiscardedChildren.Num());
			
			// Wire pipe AND hypertube spans: gate on the shared base AFGBuildablePipeBase so the hyper span
			// (AFGBuildablePipeHyper — a SIBLING of AFGBuildablePipeline, not a subclass) enters this branch.
			// Read connectors base-typed via GetConnection0/1() (NOT fluid-only GetPipeConnection0/1()); both
			// return UFGPipeConnectionComponentBase*. SetConnection + the snap-only finder live on the base; the
			// fluid AFGPipeNetwork merge tail self-skips for hyper (its Cast<UFGPipeConnectionComponent> is null). #405
			if (AFGBuildablePipeBase* Pipe = Cast<AFGBuildablePipeBase>(BuiltActor))
			{
				UFGPipeConnectionComponentBase* Conn0 = Pipe->GetConnection0();
				UFGPipeConnectionComponentBase* Conn1 = Pipe->GetConnection1();
				// #405: the fluid-only sub-features below (floor-hole/junction deferred wiring, Extend, manifold)
				// need the narrowed type. Null for a hyper span — which never trips their guards
				// (bIsPipeAutoConnectChild / ExtendChainId / bIsManifoldPipe are fluid-only).
				AFGBuildablePipeline* FluidPipe = Cast<AFGBuildablePipeline>(Pipe);
				UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND:   Conn0=%s @ %s, Conn1=%s @ %s"),
					Conn0 ? *Conn0->GetName() : TEXT("null"),
					Conn0 ? *Conn0->GetComponentLocation().ToString() : TEXT("N/A"),
					Conn1 ? *Conn1->GetName() : TEXT("null"),
					Conn1 ? *Conn1->GetComponentLocation().ToString() : TEXT("N/A"));
				
				// STACKABLE PIPE: Wire to neighbor pipes at pole snap points
				// The pole's SnapOnly0 is PCT_SNAP_ONLY (position only, not fluid connection).
				// We use its location to find and connect to OTHER PIPES at that location.
				if (bIsStackableChild && HoloData && HoloData->bIsStackablePipe)
				{
					// [#364] NEVER wire to a PCT_SNAP_ONLY connector: supports' SnapOnly0 is a snap
					// POINT, not a fluid connection - a pipe end wired to it is a dead joint that ALSO
					// blocks the player from snapping anything to that support. The hazard was always
					// latent in every support family (the finder returns ONE winner by tie-break; the
					// stackable/ground layouts happened to let the real pipe end win) - the wall layout
					// exposed it. Because the finder returns a single candidate, a snap-only winner must
					// be EXCLUDED and the search RETRIED, or a coincident real pipe end is never
					// considered (live find: reject-without-retry left run joints unwired for fluid).
					auto FindWireablePipeConn = [Pipe](UFGPipeConnectionComponentBase* OwnConn) -> UFGPipeConnectionComponentBase*
					{
						const FVector SearchLocation = OwnConn->GetComponentLocation();
						TSet<UFGPipeConnectionComponentBase*> Ignored;
						for (int32 Attempt = 0; Attempt < 4; ++Attempt)
						{
							UFGPipeConnectionComponentBase* Candidate = UFGPipeConnectionComponentBase::FindCompatibleOverlappingConnection(
								OwnConn, SearchLocation, Pipe, 50.0f, Ignored);
							if (!Candidate)
							{
								return nullptr;
							}
							const UFGPipeConnectionComponent* AsPipeConn = Cast<UFGPipeConnectionComponent>(Candidate);
							const bool bSnapOnly = AsPipeConn && AsPipeConn->GetPipeConnectionType() == EPipeConnectionType::PCT_SNAP_ONLY;
							if (bSnapOnly || Candidate->IsConnected() || Candidate->GetOwner() == Pipe)
							{
								Ignored.Add(Candidate);
								continue;
							}
							return Candidate;
						}
						return nullptr;
					};

					// At Conn0 location (start of this pipe), find another pipe's endpoint to connect to
					if (Conn0 && !Conn0->IsConnected())
					{
						if (UFGPipeConnectionComponentBase* NeighborConn = FindWireablePipeConn(Conn0))
						{
							Conn0->SetConnection(NeighborConn);
							UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" %s: Wired Conn0 to neighbor pipe %s.%s"),
								bIsStackableChild ? TEXT("STACKABLE") : (bIsPipeAutoConnectChild ? TEXT("PIPE AUTO-CONNECT") : TEXT("EXTEND")),
								*NeighborConn->GetOwner()->GetName(), *NeighborConn->GetName());
						}
					}

					// At Conn1 location (end of this pipe), find another pipe's endpoint to connect to
					if (Conn1 && !Conn1->IsConnected())
					{
						if (UFGPipeConnectionComponentBase* NeighborConn = FindWireablePipeConn(Conn1))
						{
							Conn1->SetConnection(NeighborConn);
							UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 STACKABLE: Wired Conn1 to neighbor pipe %s.%s"),
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
										UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 STACKABLE: Merged pipe networks %d and %d"), Network0, Network1);
									}
								}
								else if (Network0 != INDEX_NONE)
								{
									AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network0);
									if (Net)
									{
										Net->MarkForFullRebuild();
										UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 STACKABLE: Marked network %d for rebuild"), Network0);
									}
								}
								else if (Network1 != INDEX_NONE)
								{
									AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network1);
									if (Net)
									{
										Net->MarkForFullRebuild();
										UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 STACKABLE: Marked network %d for rebuild"), Network1);
									}
								}
							}
						}
					}
					
					// Finalize the pipe
					Pipe->OnBuildEffectFinished();
					UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 STACKABLE: Pipe %s finalized (index %d)"),
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
								
								UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 FLOOR HOLE PIPE: ✅ %s.Conn0 registered with %s via Set%sSnappedConnection (DistXY=%.1f)"),
									*Pipe->GetName(), *FloorHole->GetName(),
									bIsTopSide ? TEXT("Top") : TEXT("Bottom"), BestDist);
							}
							else
							{
								UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 FLOOR HOLE PIPE: Conn0 could not cast to UFGConnectionComponent"));
							}
						}
						else
						{
							UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 FLOOR HOLE PIPE: ❌ No floor hole found near Conn0 @ %s"),
								*PipeConn0Loc.ToString());
						}
					}
					else
					{
						// Junction/building pipes — use deferred proximity wiring (existing behavior)
						USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
						if (Subsystem)
						{
							// [#168] A blueprint SEAM pipe (parent is the blueprint hologram) wires
							// SYNCHRONOUSLY: our Construct hook builds every blueprint copy BEFORE the
							// seam conduits, so the neighbor connectors already exist this frame, and the
							// sync path also MERGES the two copies' pipe networks (the deferred junction
							// path only marks for rebuild — seam pipes built but fluid never crossed).
							if (Cast<AFGBlueprintHologram>(GetParentHologram()))
							{
								const int32 SeamWired = Subsystem->WireBlueprintSeamPipe(FluidPipe);
								UE_LOG(LogSmartAutoConnect, Verbose, TEXT("[#168] Seam pipe %s wired %d/2 endpoints synchronously"),
									*Pipe->GetName(), SeamWired);
							}
							else
							{
								Subsystem->RegisterPipeForDeferredWiring(FluidPipe);
							}
						}
						
						UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE AUTO-CONNECT: Pipe %s built, registered for deferred wiring (%s)"), 
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
							ExtendService->RegisterBuiltPipe(HoloData->ExtendChainId, HoloData->ExtendChainIndex, FluidPipe, HoloData->bIsInputChain);
							
							UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: Pipe %s registered in Construct() for chain %d, index %d (%s)"),
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
							ExtendService->WireManifoldPipe(FluidPipe, HoloData->ManifoldSourceConnector, HoloData->ManifoldCloneChainId);
						}
					}
				}
			}
		}
		else
		{
			SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Warning, TEXT("🔧 EXTEND: ❌ Pipe Construct returned nullptr!"));
		}
		
		return BuiltActor;
	}
	
	// For Auto-Connect pipes: Build normally (includes child poles if needed)
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 AUTO-CONNECT: Pipe hologram %s Construct() called - building pipe"), 
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
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE: Hologram %s SpawnChildren() - SKIPPING child pole spawning for child pipe"), 
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
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: Pipe hologram %s SetHologramLocationAndRotation() - SKIPPING vanilla processing"), 
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
			
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 EXTEND: Set snapped connections on %s: [0]=%s, [1]=%s"),
				*GetName(),
				Connection0 ? *Connection0->GetName() : TEXT("nullptr"),
				Connection1 ? *Connection1->GetName() : TEXT("nullptr"));
		}
	}
	else
	{
		SF_EXTEND_DIAGNOSTIC_LOG(LogSmartHologram, Warning, TEXT("🔧 EXTEND: Failed to find mSnappedConnectionComponents property on %s"), *GetName());
	}
}

bool ASFPipelineHologram::ApplyPipeBuildModeRouting(int32 PipeRoutingMode,
	const FVector& StartPos, const FVector& StartNormal, const FVector& EndPos, const FVector& EndNormal)
{
	// [#383] Pipes route each mode through a DISTINCT vanilla function (AutoRouteCurveSpline,
	// AutoRouteStraightSpline, Auto2DRouteSpline, HorizontalAndVerticalRouteSpline, ...) - unlike belts,
	// whose single AutoRouteSpline is itself mode-aware. So the belt-style "set a build-mode descriptor
	// then call AutoRouteSpline" always produced the Auto route (near-straight, diagnostic: len≈straight).
	// Instead, set the mode and dispatch through TryUseBuildModeRouting, which calls the matching vanilla
	// routing function per mode (so Curve bows via AutoRouteCurveSpline, etc.).
	SetRoutingMode(PipeRoutingMode);
	return TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal);
}

void ASFPipelineHologram::RoutePipeLaneWithConfiguredMode(const FVector& StartPos, const FVector& StartNormal,
	const FVector& EndPos, const FVector& EndNormal)
{
	InvalidateCostCache();  // #497: spline (and therefore length-based cost) is changing
	int32 PipeRoutingMode = 0;
	if (USFSubsystem* SmartSubsystem = USFSubsystem::Get(GetWorld()))
	{
		PipeRoutingMode = SmartSubsystem->GetAutoConnectRuntimeSettings().PipeRoutingMode;
	}
	ApplyPipeBuildModeRouting(PipeRoutingMode, StartPos, StartNormal, EndPos, EndNormal);
}

bool ASFPipelineHologram::TryUseBuildModeRouting(
	const FVector& StartPos,
	const FVector& StartNormal,
	const FVector& EndPos,
	const FVector& EndNormal)
{
	InvalidateCostCache();  // #497: spline (and therefore length-based cost) is changing
	if (!mSplineComponent)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("[PipeRoute] FALLBACK — no spline component %s"), *GetName());
		return false;
	}

	bRoutedShapeInvalid = false;

	// [#437] Vanilla mode state: C++-spawned holograms have NULL BP-default descriptors AND no
	// active build mode, and vanilla's routing internals degrade badly in that state (live-proven:
	// degenerate 2-point routes). Lazily copy the descriptors from the REAL pipeline hologram's
	// CDO (the belt #380 pattern) so every route below runs with legitimate mode state.
	if (!mBuildModeAuto || !mBuildModeAuto2D || !mBuildModeStraight || !mBuildModeCurve || !mBuildModeNoodle || !mBuildModeHorzToVert)
	{
		if (const TSubclassOf<AActor> BuildClass = GetBuildClass())
		{
			if (const AFGBuildable* BuildableCDO = Cast<AFGBuildable>(BuildClass->GetDefaultObject()))
			{
				if (BuildableCDO->mHologramClass)
				{
					if (const AFGPipelineHologram* HoloCDO = Cast<AFGPipelineHologram>(BuildableCDO->mHologramClass->GetDefaultObject()))
					{
						if (!mBuildModeAuto)       { mBuildModeAuto = HoloCDO->mBuildModeAuto; }
						if (!mBuildModeAuto2D)     { mBuildModeAuto2D = HoloCDO->mBuildModeAuto2D; }
						if (!mBuildModeStraight)   { mBuildModeStraight = HoloCDO->mBuildModeStraight; }
						if (!mBuildModeCurve)      { mBuildModeCurve = HoloCDO->mBuildModeCurve; }
						if (!mBuildModeNoodle)     { mBuildModeNoodle = HoloCDO->mBuildModeNoodle; }
						if (!mBuildModeHorzToVert) { mBuildModeHorzToVert = HoloCDO->mBuildModeHorzToVert; }
						// [#414] Bend-radius fidelity: this C++-spawned instance carries the C++
						// class defaults; the real hologram BP may tune the radii (esp. hypertubes).
						if (HoloCDO->mMinBendRadius > 0.0f) { mMinBendRadius = HoloCDO->mMinBendRadius; }
						if (HoloCDO->mBendRadius > 0.0f)    { mBendRadius = HoloCDO->mBendRadius; }
					}
				}
			}
		}
	}

	TSubclassOf<UFGHologramBuildModeDescriptor> ModeDesc = nullptr;
	switch (RoutingMode)
	{
	case 0: ModeDesc = mBuildModeAuto; break;
	case 1: ModeDesc = mBuildModeAuto2D; break;
	case 2: ModeDesc = mBuildModeStraight; break;
	case 3: ModeDesc = mBuildModeCurve; break;
	case 4: ModeDesc = mBuildModeNoodle; break;
	case 5: ModeDesc = mBuildModeHorzToVert; break;
	default: break;
	}

	// [#437 round 4 - live-validated finding] Setting the descriptor MATTERS but is not enough on
	// its own. With the descriptor set, routing all six modes through the top-level
	// AutoRouteSpline produced IDENTICAL output (points=6 len=1526 for every mode, logged live)
	// - pipes' AutoRouteSpline does NOT dispatch on the build mode; the per-mode dispatch lives
	// in the build-gun-driven SetHologramLocationAndRotation, which we cannot call without a
	// build gun. The original [#383] finding stands (owned: round 3 re-litigated it and lost).
	// HOWEVER: the descriptor state is what fixed the previously-degenerate output - vanilla's
	// route internals misbehave on a hologram with NO build mode set, which a C++-spawned
	// hologram never had. So the correct architecture = vanilla's mode state (descriptor) + the
	// SAME per-mode leaf dispatch vanilla's own SetHologramLocationAndRotation performs. The
	// leaf functions ARE the real vanilla routing; we only replicate the one switch above them.
	if (ModeDesc)
	{
		SetBuildModeOverride(ModeDesc);
	}
	UE_LOG(LogSmartHologram, Verbose,
		TEXT("[PipeRoute] mode=%d desc=%s -> leaf dispatch %s"),
		RoutingMode, ModeDesc ? *ModeDesc->GetName() : TEXT("NONE"), *GetName());

	// Vanilla's own per-mode route functions, running with legitimate mode state.
	// 0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical
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
	case 4:
		{
			// [#383] Noodle: A* path-find route (FHologramAStarNode now available via HologramHelpers.h).
			TArray<FHologramAStarNode> PathNodes;
			PathFindingRouteSpline(PathNodes, StartPos, StartNormal, EndPos, EndNormal);
		}
		break;
	case 5:
		// [#383] Horizontal->Vertical transition routing. The bool picks WHICH END gets the
		// horizontal leg (true = horizontal out of START, vertical rise near END). It was
		// hardcoded true, which is only correct when the start connector is the horizontal one -
		// a floor-hole pipe starts at the hole's VERTICAL connector with the building's
		// horizontal connector at the end, so the route left the hole sideways and rammed the
		// machine connector at a diagonal instead of exiting it straight (#404 follow-up).
		// Derive the leg order from the endpoint normals: a vertical start with a horizontal
		// end flips to vertical-first; every other combination keeps the old horizontal-first.
		//
		// [#437 2026-07-02] This is a genuine, narrowly-scoped bug fix (independent of the
		// tangent-repair block below) and is NOT the primary cause of the floor-hole "twisted
		// pipe" reports - see the #437 issue thread for the actual root cause: hand-building the
		// IDENTICAL floor-hole-to-building span in Auto 2D mode was rejected by vanilla itself
		// ("Invalid Pipe Shape!"). Smart's floor-hole child spawns with
		// USFHologramDataService::DisableValidation(), so it renders the shape anyway instead of
		// respecting that rejection. The real fix is structural (detect the invalid-shape case
		// and either synthesize a compliant intermediate bend point or decline the connection),
		// not yet implemented. This leg-order fix and the tangent repair below are safety nets
		// that reduce the severity of the rendered artifact; they do not address the underlying
		// invalid-shape condition.
		{
			const bool bStartVertical = FMath::Abs(StartNormal.Z) > 0.7f;
			const bool bEndVertical = FMath::Abs(EndNormal.Z) > 0.7f;
			const bool bHorizontalFirst = !(bStartVertical && !bEndVertical);
			// [#437 round 2] Use the NEW vanilla H2V router. The old HorizontalAndVerticalRouteSpline
			// emits a degenerate route (one 10cm point then a beeline); the build gun's own H2V route,
			// captured live from a hand-built hole-to-machine span, is the multi-point riser/elbow/
			// level-run/connector-stub shape that only ...New produces. Ground truth: 6 points -
			// 100cm straight riser out of the hole (tangents 10), elbow to connector height (163),
			// level run (496), 100cm connector stub (50/1) - all C1-continuous.
			HorizontalAndVerticalRouteSplineNew(bHorizontalFirst, StartPos, StartNormal, EndPos, EndNormal);
		}
		break;
	case 0:
	default:
		AutoRouteSpline(StartPos, StartNormal, EndPos, EndNormal);
		break;
	}

	// [#437] Endpoint-tangent repair. The vanilla routers seed the END tangents from the raw
	// UNIT connector normals we pass (observed live via /api/splines: |tangent| = 1 and 25 on a
	// 1316cm floor-hole span, where a healthy Hermite tangent is ~40% of the span). A straight
	// span renders straight regardless of tangent magnitude - which is why junction/stackable
	// runs never showed it - but on a BENT span (floor hole: vertical exit, horizontal arrival)
	// a near-zero tangent collapses the exit stiffness: the pipe leaves the connector for only a
	// few cm before beelining at the far endpoint, rendering as the reported self-clipping
	// twist. Repair only DEGENERATE endpoint tangents: rescale to a distance-proportional
	// magnitude preserving direction and all interior points; healthy router output is the
	// router's intended shape and is left untouched.
	//
	// [#437 2026-07-02 - IMPORTANT CONTEXT] This is a PARTIAL MITIGATION, not the root-cause
	// fix. Hand-building the identical floor-hole span in Auto 2D was rejected by vanilla itself
	// with "Invalid Pipe Shape!" - vanilla's own router does not consider a single-span
	// vertical-to-horizontal bend without an intermediate control point (e.g. a support pole)
	// buildable. Smart's floor-hole child disables placement validation, so it renders the
	// degenerate/invalid shape instead of refusing it. This repair only smooths the rendered
	// symptom (tangent magnitude) of that invalid state - it does not make the shape valid. A
	// real fix needs to either detect the invalid-shape condition and synthesize a compliant
	// intermediate bend point (mirroring what a vanilla support pole provides), or decline the
	// auto-connection outright for geometry this sharp. See the #437 issue thread for the full
	// investigation (live spline captures across routing modes + the vanilla manual-build
	// comparison that surfaced the "Invalid Pipe Shape!" rejection).
	if (mSplineData.Num() >= 2)
	{
		const FTransform ActorXf = GetActorTransform();

		// [#437 round 2] The repair threshold is PER-SEGMENT, not per-span. Ground truth from a
		// hand-built vanilla H2V route: vanilla legitimately uses SMALL endpoint tangents on its
		// short straight connector stubs (leave=10 on a 100cm riser) - those are healthy and must
		// not be inflated (a span-proportional rescale would balloon them to ~500 and wreck the
		// straight stub). A tangent is only degenerate relative to the segment it shapes: fire
		// only when it is under 5% of the distance to the NEIGHBORING point (the 1-25 tangents on
		// a 1300cm beeline segment that caused the original twist), and rescale proportional to
		// that same segment.
		auto RepairEndpointTangent = [this](FSplinePointData& Point, const FSplinePointData& Neighbor, const FVector& LocalFallbackDir, const TCHAR* Which)
		{
			const float SegLen = FVector::Dist(Point.Location, Neighbor.Location);
			if (SegLen < KINDA_SMALL_NUMBER)
			{
				return;
			}
			const float CurrentMag = FMath::Max(Point.ArriveTangent.Size(), Point.LeaveTangent.Size());
			if (CurrentMag >= SegLen * 0.05f)
			{
				return; // healthy relative to its own segment - keep the router's shape
			}
			const float DesiredTangent = FMath::Clamp(SegLen * 0.4f, 50.0f, 600.0f);
			FVector Dir = Point.LeaveTangent.GetSafeNormal();
			if (Dir.IsNearlyZero()) { Dir = Point.ArriveTangent.GetSafeNormal(); }
			if (Dir.IsNearlyZero()) { Dir = LocalFallbackDir; }
			if (Dir.IsNearlyZero())
			{
				return;
			}
			Point.ArriveTangent = Dir * DesiredTangent;
			Point.LeaveTangent = Dir * DesiredTangent;
			UE_LOG(LogSmartHologram, Verbose,
				TEXT("[PipeRoute] #437 repaired degenerate %s tangent (was %.1f, now %.1f, seg=%.0f) %s"),
				Which, CurrentMag, DesiredTangent, SegLen, *GetName());
		};

		// Tangents live in hologram-LOCAL space; the passed normals are WORLD. Convert for the
		// zero-tangent fallback (floor-hole children spawn unrotated, but extend lanes don't).
		const FVector LocalStartDir = ActorXf.InverseTransformVectorNoScale(StartNormal).GetSafeNormal();
		const FVector LocalEndDir = ActorXf.InverseTransformVectorNoScale(-EndNormal).GetSafeNormal();
		RepairEndpointTangent(mSplineData[0], mSplineData[1], LocalStartDir, TEXT("start"));
		RepairEndpointTangent(mSplineData.Last(), mSplineData[mSplineData.Num() - 2], LocalEndDir, TEXT("end"));
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
		UE_LOG(LogSmartHologram, Verbose,
			TEXT("[PipeRoute] FALLBACK — in-game routed a STUB (mode=%d points=%d len=%.0f expected=%.0f) %s"),
			RoutingMode, NewSplinePoints, NewSplineLength, ExpectedDistance, *GetName());
		return false;
	}

	// Use the same routed spline for build.
	mBuildSplineData = mSplineData;

	// [#437/#414] Honor vanilla's shape validity on EVERY routed pipe/hypertube span, not just
	// floor holes: vanilla invalidates a pipe whose bend radius drops below mMinBendRadius
	// ("Invalid Pipe Shape!" - reproduced live by hand-building a floor-hole span in Auto 2D).
	// CheckValidPlacement turns the flag into the SAME disqualifier on this child. Straight and
	// gently-curved spans sample as effectively-infinite radius and never flag.
	const float MinRadiusCm = ComputeMinRoutedBendRadiusCm();
	if (MinRadiusCm < mMinBendRadius)
	{
		bRoutedShapeInvalid = true;
		UE_LOG(LogSmartHologram, Verbose,
			TEXT("[PipeRoute] routed shape INVALID (min bend radius %.0f < %.0f, mode=%d) %s"),
			MinRadiusCm, mMinBendRadius, RoutingMode, *GetName());
	}

	UE_LOG(LogSmartHologram, Verbose,
		TEXT("[PipeRoute] IN-GAME used (mode=%d points=%d len=%.0f) %s"),
		RoutingMode, NewSplinePoints, NewSplineLength, *GetName());

	return true;
}

bool ASFPipelineHologram::RouteWithStraightExit(float /*ExitStubCm - retired*/, const FVector& StartPos, const FVector& ExitNormal,
	const FVector& EndPos, const FVector& EndNormal)
{
	InvalidateCostCache();  // #497: spline (and therefore length-based cost) is changing
	// [#414] Thin shim, kept only for call-site stability: the forced exit stub was removed in
	// #437 round 2 (the real routers build their own riser/connector stubs), and the shape
	// validation moved INTO TryUseBuildModeRouting so all pipe/hypertube spans get it. The exit
	// vector seeds the router's start tangent, exactly like a hand-built pipe leaving a
	// passthrough.
	return TryUseBuildModeRouting(StartPos, ExitNormal, EndPos, EndNormal);
}

float ASFPipelineHologram::ComputeMinRoutedBendRadiusCm() const
{
	if (!mSplineComponent)
	{
		return TNumericLimits<float>::Max();
	}

	const float Len = mSplineComponent->GetSplineLength();
	constexpr float StepCm = 25.0f;
	if (Len < StepCm * 2.0f)
	{
		return TNumericLimits<float>::Max();
	}

	// Discrete curvature: circumradius of each triple of samples 25cm apart along the spline
	// (R = abc / (4 * Area)). The minimum over the run approximates the tightest bend, which is
	// what vanilla's mMinBendRadius shape check gates on.
	float MinRadius = TNumericLimits<float>::Max();
	for (float D = StepCm; D <= Len - StepCm; D += StepCm)
	{
		const FVector A = mSplineComponent->GetLocationAtDistanceAlongSpline(D - StepCm, ESplineCoordinateSpace::Local);
		const FVector B = mSplineComponent->GetLocationAtDistanceAlongSpline(D, ESplineCoordinateSpace::Local);
		const FVector C = mSplineComponent->GetLocationAtDistanceAlongSpline(D + StepCm, ESplineCoordinateSpace::Local);

		const float SideA = FVector::Dist(B, C);
		const float SideB = FVector::Dist(A, C);
		const float SideC = FVector::Dist(A, B);
		const float DoubleArea = FVector::CrossProduct(B - A, C - A).Size();
		if (DoubleArea < KINDA_SMALL_NUMBER)
		{
			continue; // locally straight
		}
		MinRadius = FMath::Min(MinRadius, (SideA * SideB * SideC) / (2.0f * DoubleArea));
	}
	return MinRadius;
}

bool ASFPipelineHologram::ValidateCurrentSpline(float MaxSplineLengthCm, bool& OutTooLong)
{
	OutTooLong = mSplineComponent && mSplineComponent->GetSplineLength() > MaxSplineLengthCm;
	bRoutedShapeInvalid = ComputeMinRoutedBendRadiusCm() < mMinBendRadius;
	return !OutTooLong && !bRoutedShapeInvalid;
}

void ASFPipelineHologram::SetPlacementMaterialState(EHologramMaterialState materialState)
{
	// #497 set-once: BOTH the vanilla Super sweep and our spline sweep dirty render proxies, so the
	// early-out must come BEFORE Super (capture 4: per-frame parent cascade with unchanged state ran
	// the unguarded Super on every child every frame — render-thread proxy churn at rest). Once a
	// state has been swept it holds. Meshes created later are painted by TriggerMeshGeneration's
	// trailing ApplySplineMeshMaterialState.
	if (bSplineMaterialStateApplied && materialState == LastAppliedSplineMaterialState)
	{
		return;
	}

	Super::SetPlacementMaterialState(materialState);

	ApplySplineMeshMaterialState(materialState);
}

void ASFPipelineHologram::ApplySplineMeshMaterialState(EHologramMaterialState materialState)
{
	TArray<USplineMeshComponent*> MeshComps;
	GetComponents<USplineMeshComponent>(MeshComps);
	const uint8 StencilValue = GetStencilForHologramMaterialState(materialState);

	// No meshes yet: leave the guard unarmed so the next call (or mesh generation) paints them.
	if (MeshComps.Num() == 0)
	{
		return;
	}

	for (USplineMeshComponent* Mesh : MeshComps)
	{
		if (Mesh)
		{
			Mesh->SetRenderCustomDepth(true);
			Mesh->SetCustomDepthStencilValue(StencilValue);
			Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);

			// Force render state update
			Mesh->MarkRenderStateDirty();
			Mesh->SetVisibility(true, true);
			Mesh->SetHiddenInGame(false);
		}
	}

	LastAppliedSplineMaterialState = materialState;
	bSplineMaterialStateApplied = true;

	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🎨 PIPE SetPlacementMaterialState: State=%d, SplineMeshes=%d, Stencil=%d"),
		(int32)materialState,
		MeshComps.Num(),
		(int32)StencilValue);
}

void ASFPipelineHologram::SetupPipeSpline(UFGPipeConnectionComponentBase* StartConnector, UFGPipeConnectionComponentBase* EndConnector)
{
	InvalidateCostCache();  // #497: spline (and therefore length-based cost) is changing
	if (!StartConnector || !EndConnector)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("SetupPipeSpline: Invalid connectors"));
		return;
	}

	const FVector StartPos = StartConnector->GetConnectorLocation();
	const FVector EndPos = EndConnector->GetConnectorLocation();
	const FVector StartNormal = StartConnector->GetConnectorNormal();
	const FVector EndNormal = EndConnector->GetConnectorNormal();

	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE SPLINE SETUP: %s"), *GetName());
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Start: %s (normal: %s)"), *StartPos.ToString(), *StartNormal.ToString());
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   End: %s (normal: %s)"), *EndPos.ToString(), *EndNormal.ToString());

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
	
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   ✅ BUILD SPLINE: 6-point vanilla curve (distance=%.1f cm)"), Distance);
	
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
	
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   ✅ PREVIEW SPLINE: 2-point straight line (distance=%.1f cm)"), Distance);

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
			
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   Preview Point %d: Local=%s"), 
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
		
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🏗️ BUILDING PIPE: Swapped to 6-point build spline (%d points)"), mBuildSplineData.Num());
		
		// Call base class with 6-point spline
		Super::ConfigureComponents(inBuildable);
		
		// Restore preview spline (though hologram is about to be destroyed anyway)
		const_cast<ASFPipelineHologram*>(this)->mSplineData = OriginalSplineData;
	}
	else
	{
		// No build spline data (shouldn't happen, but fallback to base)
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT(" BUILDING PIPE: No build spline data! Using preview spline"));
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
	
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: %s checking targets: Conn0→%s.%s, Conn1→%s.%s"),
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
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
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
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ✅ Connected %s.Conn0 → %s.%s"),
							*Pipeline->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName());
					}
					else
					{
						// Target connector is already connected - this is a problem for lanes connecting to source
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ⚠️ %s.Conn0 target %s.%s is ALREADY CONNECTED to %s"),
							*Pipeline->GetName(), *TargetBuildable->GetName(), *TargetConn->GetName(),
							TargetConn->GetConnection() ? *TargetConn->GetConnection()->GetOwner()->GetName() : TEXT("NULL"));
					}
					break;
				}
			}
			if (!bFoundConnector)
			{
				UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ⚠️ %s.Conn0 target connector '%s' NOT FOUND on %s"),
					*Pipeline->GetName(), *HoloData->Conn0TargetConnectorName.ToString(), *TargetBuildable->GetName());
			}
		}
		else
		{
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ⚠️ %s.Conn0 target buildable '%s' NOT FOUND"),
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
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🛤️ LANE ConfigureComponents: Looking up source buildable '%s' → %s"),
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
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: ✅ Connected %s.Conn1 → %s.%s"),
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
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: Merged networks %d and %d for %s"),
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
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureComponents: Marked network %d for rebuild for %s"),
							ValidNetworkID, *Pipeline->GetName());
					}
				}
			}
		}
	}
}

void ASFPipelineHologram::SetSplineDataAndUpdate(const TArray<FSplinePointData>& InSplineData)
{
	InvalidateCostCache();  // #497: spline (and therefore length-based cost) is changing
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE SetSplineDataAndUpdate: Setting %d spline points on %s"), InSplineData.Num(), *GetName());
	
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
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE SetSplineDataAndUpdate: Stored %d points in backup registry"), InSplineData.Num());
	}
	
	// Update spline component
	if (mSplineComponent)
	{
		AFGSplineHologram::UpdateSplineComponent();
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE SetSplineDataAndUpdate: UpdateSplineComponent called, spline length=%.1f cm"), 
			mSplineComponent->GetSplineLength());
	}
	else
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE SetSplineDataAndUpdate: No spline component!"));
	}
}

void ASFPipelineHologram::ForceApplyHologramMaterial()
{
	// "Force" per the contract: bypass the #497 set-once guard so an explicit caller (post
	// mesh-generation fixups) always gets a full sweep even when the state value is unchanged.
	ApplySplineMeshMaterialState(GetHologramMaterialState());
}

void ASFPipelineHologram::TriggerMeshGeneration()
{
	InvalidateCostCache();  // #497: spline (and therefore length-based cost) is changing
	UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: %s - mSplineData has %d points"), *GetName(), mSplineData.Num());
	
	if (!mSplineComponent)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: %s - mSplineComponent is NULL!"), *GetName());
		return;
	}

	// Push mSplineData into spline component
	AFGSplineHologram::UpdateSplineComponent();

	// Log spline stats for debugging
	const int32 PointCount = mSplineComponent->GetNumberOfSplinePoints();
	const float SplineLength = mSplineComponent->GetSplineLength();
	UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: %s - %d spline points, %.1f cm length"), *GetName(), PointCount, SplineLength);
	
	// Get spline points for geometry update
	if (PointCount < 2)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("   PIPE TriggerMeshGeneration: Not enough spline points (%d)"), PointCount);
		return;
	}
	
	// Get pipe mesh, material, and mesh length from build class CDO - this ensures they match the actual tier
	UStaticMesh* PipeMesh = nullptr;
	UMaterialInterface* PipeMaterial = nullptr;
	float MeshLength = 200.0f; // Default fallback
	
	if (mBuildClass)
	{
		// #405: cast to the COMMON base (AFGBuildablePipeBase), not AFGBuildablePipeline — a hypertube
		// (AFGBuildablePipeHyper) is a SIBLING of the fluid pipe, so the narrow cast failed and fell back to
		// the fluid SM_Pipe mesh. GetSplineMesh/GetMeshLength/mSplineMeshMaterial are virtual on the base, so
		// each subclass (fluid OR hyper) returns its own correct mesh.
		if (AFGBuildablePipeBase* PipeCDO = Cast<AFGBuildablePipeBase>(mBuildClass->GetDefaultObject()))
		{
			PipeMesh = PipeCDO->GetSplineMesh();
			PipeMaterial = PipeCDO->mSplineMeshMaterial;
			MeshLength = PipeCDO->GetMeshLength();
			UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: Got from CDO: Mesh=%s, Material=%s, MeshLength=%.1f"), 
				PipeMesh ? *PipeMesh->GetName() : TEXT("NULL"),
				PipeMaterial ? *PipeMaterial->GetName() : TEXT("NULL"),
				MeshLength);
		}
		else
		{
			UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: mBuildClass=%s but CDO cast failed"), *mBuildClass->GetName());
		}
	}
	else
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: mBuildClass is NULL!"));
	}
	
	// Fallback to hardcoded generic pipe mesh if CDO lookup failed
	if (!PipeMesh)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: CDO mesh lookup failed, using fallback"));
		PipeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Mesh/SM_Pipe.SM_Pipe"));
	}
	if (!PipeMesh)
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("   PIPE TriggerMeshGeneration: Failed to load pipe mesh!"));
		return;
	}
	
	// Ensure mesh length is valid (use CDO value, not bounds calculation)
	if (MeshLength <= 0.0f)
	{
		MeshLength = 200.0f; // Default pipe mesh length
	}
	
	UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: Mesh=%s, MeshLength=%.1f cm, SplineLength=%.1f cm"), 
		*PipeMesh->GetName(), MeshLength, SplineLength);
	
	// CRITICAL FIX: Calculate segments based on spline length / mesh length (like vanilla)
	// NOT based on spline point count - that causes severe stretching
	const int32 RequiredSegments = FMath::Max(1, FMath::CeilToInt(SplineLength / MeshLength));
	
	UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: Need %d segments (%.1f cm each) for %.1f cm spline"), 
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
			NewMesh->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
			
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

	if (FArrayProperty* SplineMeshesProp = FindFProperty<FArrayProperty>(AFGPipelineHologram::StaticClass(), TEXT("mSplineMeshes")))
	{
		if (TArray<USplineMeshComponent*>* NativeSplineMeshes = SplineMeshesProp->ContainerPtrToValuePtr<TArray<USplineMeshComponent*>>(this))
		{
			NativeSplineMeshes->Reset();
			for (USplineMeshComponent* MeshComp : MeshComps)
			{
				if (MeshComp)
				{
					NativeSplineMeshes->Add(MeshComp);
				}
			}
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

			// [#404 follow-up] SplineUpDir defaults to +Z; a VERTICAL segment (tangent parallel to
			// the up-dir) degenerates the roll frame and the mesh visibly twists/pinches. Floor-hole
			// pipes are the one AC span with a vertical section (straight up out of the hole), which
			// is why only they rendered "twisted at the hole" in every routing mode. Give vertical
			// segments a horizontal roll reference; keep the +Z default everywhere else (any
			// horizontal axis is a valid roll reference for a round pipe).
			//
			// [#437 2026-07-02] Real, narrowly-scoped fix, but downstream of the actual root
			// cause - see the tangent-repair comment in ApplyPipeBuildModeRouting (above in this
			// file) and the #437 issue thread. Vanilla itself rejects this span shape
			// ("Invalid Pipe Shape!" on a manual build); Smart renders it anyway because floor-hole
			// children disable placement validation. This fix only cleans up ONE symptom (mesh
			// roll) of that unvalidated, degenerate spline - it does not make the underlying shape
			// valid, and does not fully resolve the reported twist on its own.
			const FVector SegmentDir = (EndPos - StartPos).GetSafeNormal();
			if (FMath::Abs(SegmentDir.Z) > 0.95f)
			{
				MeshComp->SetSplineUpDir(FVector::ForwardVector, false);
			}
			else
			{
				MeshComp->SetSplineUpDir(FVector::UpVector, false);
			}

			MeshComp->SetStartAndEnd(StartPos, StartTangent, EndPos, EndTangent, true);
			MeshComp->SetVisibility(true, true);
			MeshComp->MarkRenderStateDirty();
			
			if (SegmentIdx == 0)
			{
				UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE Segment[0]: Start=%s End=%s (dist %.1f-%.1f)"), 
					*StartPos.ToString(), *EndPos.ToString(), StartDist, EndDist);
			}
		}
	}
	
	UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE TriggerMeshGeneration: Created %d segments of %.1f cm each"), 
		MeshComps.Num(), SegmentLength);
	
	// Apply the current hologram material state to newly created mesh components. Must bypass the
	// #497 set-once guard: the mesh SET just changed even though the state value may not have.
	ApplySplineMeshMaterialState(GetHologramMaterialState());
	
	// Check actor position
	FVector ActorLoc = GetActorLocation();
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("   PIPE: Actor location: %s"), *ActorLoc.ToString());
	
	// Check if components are actually attached and positioned
	for (int32 i = 0; i < MeshComps.Num() && i < 2; i++)
	{
		if (MeshComps[i])
		{
			FVector CompWorldLoc = MeshComps[i]->GetComponentLocation();
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("      MeshComp[%d] World Location: %s"), i, *CompWorldLoc.ToString());
		}
	}
}

void ASFPipelineHologram::ConfigureActor(class AFGBuildable* inBuildable) const
{
	// CRITICAL: Same pattern as ASFConveyorBeltHologram::ConfigureActor
	// Vanilla can clear mSplineData during construction processing.
	// We detect this and restore from backup before passing to Super.
	
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Hologram=%s, Buildable=%s"), 
		*GetName(), inBuildable ? *inBuildable->GetName() : TEXT("NULL"));
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: mSplineData has %d points"), mSplineData.Num());
	
	// Log current spline data state
	for (int32 i = 0; i < mSplineData.Num() && i < 3; i++)
	{
		const FSplinePointData& Point = mSplineData[i];
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor:   Point[%d] Location=%s"), 
			i, *Point.Location.ToString());
	}
	
	// CRITICAL FIX: Check if we have backup data that should be restored
	// This handles the case where mSplineData was reset to default 2-point spline by vanilla
	if (const FSFHologramData* HoloData = USFHologramDataRegistry::GetData(this))
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Registry entry found - bHasBackup=%d, BackupPoints=%d"),
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
						UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Backup end=%s (%.1f cm), Current end=%s (%.1f cm) - differs, will restore"),
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
			UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE ConfigureActor: mSplineData was reset! Restoring %d points from backup (had %d)"),
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
			
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: Restored mSplineData now has %d points"), 
				mSplineData.Num());
			
			// Log the restored points
			for (int32 i = 0; i < mSplineData.Num() && i < 4; i++)
			{
				UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor:   RestoredPoint[%d] Location=%s"), 
					i, *mSplineData[i].Location.ToString());
			}
		}
	}
	else
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: No registry entry found for this hologram"));
	}
	
	if (mSplineComponent)
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: SplineComponent has %d points, length=%.1f cm"),
			mSplineComponent->GetNumberOfSplinePoints(), mSplineComponent->GetSplineLength());
	}
	else
	{
		UE_LOG(LogSmartHologram, Verbose, TEXT("🔧 PIPE ConfigureActor: mSplineComponent is NULL!"));
	}
	
	// Call parent to configure the buildable
	Super::ConfigureActor(inBuildable);
	
	// Log after parent call to see if buildable got the spline data
	if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(inBuildable))
	{
		const TArray<FSplinePointData>& PipeSplineData = Pipeline->GetSplinePointData();
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor: AFTER Super - Buildable has %d spline points"), 
			PipeSplineData.Num());
		for (int32 i = 0; i < PipeSplineData.Num() && i < 3; i++)
		{
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("🔧 PIPE ConfigureActor:   BuildablePoint[%d] Location=%s"), 
				i, *PipeSplineData[i].Location.ToString());
		}
	}
}

TArray<FItemAmount> ASFPipelineHologram::GetBaseCost() const
{
	// #497: Preview pipe children are commonly spawned without a recipe (mRecipe == null); their cost is
	// length-based and computed in GetCost. Vanilla AFGHologram::GetBaseCost would call
	// UFGRecipe::GetIngredients(nullptr), which logs "FGRecipe::GetIngredients: class was nullpeter"
	// once per child per frame (~89/frame in a conveyor blueprint) — each line a synchronous disk write
	// via the UE log + Sentry breadcrumb, the confirmed source of the Extend stutter/frame-loss.
	// A null recipe has no base cost, so skip vanilla entirely (which returns empty anyway, just noisily).
	if (!GetRecipe())
	{
		return TArray<FItemAmount>();
	}
	return Super::GetBaseCost();
}

TArray<FItemAmount> ASFPipelineHologram::GetCost(bool includeChildren) const
{
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE GetCost() CALLED on %s (includeChildren=%d)"), *GetName(), includeChildren);

	// #497 cost cache (see header). The health check stays IN FRONT of the cache: a vanilla-reset
	// (zero-length) spline must fall through to the full path so the #357 defensive restoration
	// below still repairs the preview. Gated to extend preview children — they have no hologram
	// children of their own, so the includeChildren flag cannot change the result.
	const bool bSplineHealthy = mSplineComponent && mSplineComponent->GetSplineLength() > KINDA_SMALL_NUMBER;
	if (bSelfCostCacheValid && bSplineHealthy && Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		return CachedSelfCost;
	}

	// Get base cost from parent. In Satisfactory 1.2 vanilla AFGPipelineHologram::GetCost already
	// returns the length-based pipe material cost.
	TArray<FItemAmount> TotalCost = Super::GetCost(includeChildren);

	// #348: Trust the vanilla pipe cost when present. This method used to assume Super returned nothing
	// for pipes and always added its own length-scaled cost on top, double-counting the pipe (auto-
	// connect preview charged 2x the dismantle refund). The manual length calculation below now only
	// runs as a fallback when vanilla returned nothing (e.g. the spline data was reset before the query).
	if (TotalCost.Num() > 0)
	{
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE GetCost: using vanilla cost (%d item types), skipping manual calc"), TotalCost.Num());
		if (bSplineHealthy)
		{
			CachedSelfCost = TotalCost;
			bSelfCostCacheValid = true;
		}
		return TotalCost;
	}

	// Calculate pipe cost based on spline length (fallback when Super returned no cost)
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
					UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE: Spline length is zero but backup has %d points - RESTORING!"), 
						HoloData->BackupSplineData.Num());
					
					// Restore spline data
					ASFPipelineHologram* MutableThis = const_cast<ASFPipelineHologram*>(this);
					MutableThis->mSplineData = HoloData->BackupSplineData;
					MutableThis->AFGSplineHologram::UpdateSplineComponent();
					
					// Recalculate length after restoration
					PipeLengthCm = mSplineComponent->GetSplineLength();
					LengthInMeters = PipeLengthCm / 100.0f;
					
					UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE: After restoration, spline length = %.1f cm"), PipeLengthCm);
					MutableThis->TriggerMeshGeneration();
				}
			}
			
			// If still zero after restoration attempt, skip cost calculation
			if (LengthInMeters <= KINDA_SMALL_NUMBER)
			{
				UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE: Spline length is zero - skipping pipe cost calculation"));
				return TotalCost;
			}
		}
		
		UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE: Spline length = %.1f cm (%.1f m)"), PipeLengthCm, LengthInMeters);
		
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
			
			UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE: Class = %s, Tier = Mk%d"), *ClassName, PipeTier);
			
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
							const TArray<FItemAmount> Ingredients = UFGRecipe::GetIngredients(this, PipeRecipe);

							UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE: Found recipe with %d ingredients"), Ingredients.Num());

							// Calculate total cost based on pipe length
							for (const FItemAmount& Ingredient : Ingredients)
							{
								if (Ingredient.ItemClass)
								{
									// Calculate amount needed for this pipe length
									int32 AmountNeeded = FMath::CeilToInt(Ingredient.Amount * LengthInMeters);
									
									UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE:   %s: %.1f per meter × %.1f meters = %d total"),
										*Ingredient.ItemClass->GetName(), (double)Ingredient.Amount, LengthInMeters, AmountNeeded);
									
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
							UE_LOG(LogSmartHologram, Verbose, TEXT("💰 PIPE: No recipe found for pipe class %s"), *ClassName);
						}
					}
				}
			}
		}
	}
	
	UE_LOG(LogSmartHologram, VeryVerbose, TEXT("💰 PIPE GetCost() RETURNING %d item types"), TotalCost.Num());

	// #497: cache the computed fallback cost while the spline is healthy (a zero-length spline
	// leaves the cache invalid so the restoration path gets another chance next call).
	if (mSplineComponent && mSplineComponent->GetSplineLength() > KINDA_SMALL_NUMBER)
	{
		CachedSelfCost = TotalCost;
		bSelfCostCacheValid = true;
	}
	return TotalCost;
}

void ASFPipelineHologram::SetHologramNudgeLocation()
{
	// [#497] Extend children: block vanilla's locked-parent nudge cascade, which bypasses the
	// SF_ExtendChild SetHologramLocationAndRotation guard via plain SetActorLocation and dragged
	// every extend child to world origin each tick (origin-trap stack: FGBuildGunBuild.cpp:320 ->
	// FGHologram.cpp:440 -> :2120). Non-extend instances keep vanilla behavior.
	if (Tags.Contains(FName(TEXT("SF_ExtendChild"))))
	{
		return;
	}
	Super::SetHologramNudgeLocation();
}
