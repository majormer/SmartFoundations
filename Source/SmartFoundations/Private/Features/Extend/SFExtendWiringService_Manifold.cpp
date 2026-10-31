// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * SFExtendWiringService - Manifold connections (slice E2 unit G).
 * Methods moved verbatim from SFExtendService; operate on its shared state via ExtendService->.
 */

#include "Features/Extend/SFExtendWiringServiceImpl.h"
#include "Core/Helpers/SFBuildEffectHelper.h"

void USFExtendWiringService::WireManifoldPipe(AFGBuildablePipeline* BuiltPipe, UFGPipeConnectionComponentBase* SourceConnector, int32 CloneChainId)
{
    if (!BuiltPipe || !SourceConnector)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Invalid parameters"));
        return;
    }

    // Get the pipe's two connectors
    UFGPipeConnectionComponentBase* PipeConn0 = BuiltPipe->GetPipeConnection0();
    UFGPipeConnectionComponentBase* PipeConn1 = BuiltPipe->GetPipeConnection1();

    if (!PipeConn0 || !PipeConn1)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Pipe %s missing connectors"), *BuiltPipe->GetName());
        return;
    }

    // Find which pipe connector is closer to source junction
    float Dist0ToSource = FVector::Dist(PipeConn0->GetComponentLocation(), SourceConnector->GetComponentLocation());
    float Dist1ToSource = FVector::Dist(PipeConn1->GetComponentLocation(), SourceConnector->GetComponentLocation());

    UFGPipeConnectionComponentBase* PipeToSource = (Dist0ToSource < Dist1ToSource) ? PipeConn0 : PipeConn1;
    UFGPipeConnectionComponentBase* PipeToClone = (Dist0ToSource < Dist1ToSource) ? PipeConn1 : PipeConn0;

    // Wire pipe to source junction
    if (!PipeToSource->IsConnected() && !SourceConnector->IsConnected())
    {
        PipeToSource->SetConnection(SourceConnector);
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Connected %s.%s → source %s (verified: PipeConnected=%d, SourceConnected=%d)"),
            *BuiltPipe->GetName(), *PipeToSource->GetName(), *SourceConnector->GetName(),
            PipeToSource->IsConnected(), SourceConnector->IsConnected());
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Source connection failed (PipeConnected=%d, SourceConnected=%d)"),
            PipeToSource->IsConnected(), SourceConnector->IsConnected());
    }

    // Find clone junction from ExtendService->BuiltJunctionsByChain
    AFGBuildable** CloneJunctionPtr = ExtendService->BuiltJunctionsByChain.Find(CloneChainId);
    if (!CloneJunctionPtr || !*CloneJunctionPtr)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone junction not found for chain %d"), CloneChainId);
        return;
    }

    AFGBuildable* CloneJunction = *CloneJunctionPtr;

    // Get clone junction's connectors
    TArray<UFGPipeConnectionComponentBase*> CloneConnectors;
    CloneJunction->GetComponents<UFGPipeConnectionComponentBase>(CloneConnectors);

    // ============================================================
    // OPPOSING CONNECTOR APPROACH FOR PIPE MANIFOLD WIRING
    // ============================================================
    // The manifold pipe should connect OPPOSING connectors on source and clone.
    // Find the clone connector that faces TOWARD the source junction.
    // This matches the spawning logic where we selected the source connector
    // facing toward the clone, and used the same relative position on clone.
    // ============================================================

    FVector CloneLocation = CloneJunction->GetActorLocation();
    FVector SourceLocation = SourceConnector->GetOwner()->GetActorLocation();
    FVector DirectionToSource = (SourceLocation - CloneLocation).GetSafeNormal();

    UFGPipeConnectionComponentBase* BestCloneConnector = nullptr;
    float BestAlignment = -FLT_MAX;

    for (UFGPipeConnectionComponentBase* CloneConn : CloneConnectors)
    {
        if (!CloneConn || CloneConn->IsConnected()) continue;

        // Find the connector whose position (relative to junction center) best faces toward source
        FVector ConnPos = CloneConn->GetComponentLocation();
        FVector DirFromCenter = (ConnPos - CloneLocation).GetSafeNormal();
        float Alignment = FVector::DotProduct(DirFromCenter, DirectionToSource);

        if (Alignment > BestAlignment)
        {
            BestAlignment = Alignment;
            BestCloneConnector = CloneConn;
        }
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone connector selection - BestAlignment=%.2f"),
        BestAlignment);

    if (BestCloneConnector && !PipeToClone->IsConnected())
    {
        PipeToClone->SetConnection(BestCloneConnector);
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Connected %s.%s → clone %s.%s (alignment=%.2f, verified: PipeConnected=%d, CloneConnected=%d)"),
            *BuiltPipe->GetName(), *PipeToClone->GetName(),
            *CloneJunction->GetName(), *BestCloneConnector->GetName(), BestAlignment,
            PipeToClone->IsConnected(), BestCloneConnector->IsConnected());
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔧 EXTEND Manifold Pipe Wire: Clone connection failed (NoCloneConnector=%d, PipeConnected=%d)"),
            BestCloneConnector == nullptr, PipeToClone->IsConnected());
    }

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: ✅ Wired manifold pipe %s between source and clone junctions"),
        *BuiltPipe->GetName());

    // Merge pipe networks to ensure fluid can flow
    // Cast to UFGPipeConnectionComponent to access GetPipeNetworkID()
    UFGPipeConnectionComponent* NetworkConn0 = Cast<UFGPipeConnectionComponent>(PipeConn0);
    UFGPipeConnectionComponent* NetworkConn1 = Cast<UFGPipeConnectionComponent>(PipeConn1);
    if (NetworkConn0 && NetworkConn1)
    {
        UWorld* World = BuiltPipe->GetWorld();
        AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
        if (PipeSubsystem)
        {
            int32 Network0 = NetworkConn0->GetPipeNetworkID();
            int32 Network1 = NetworkConn1->GetPipeNetworkID();
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Network IDs - Conn0=%d, Conn1=%d"),
                Network0, Network1);

            if (Network0 != Network1 && Network0 != INDEX_NONE && Network1 != INDEX_NONE)
            {
                AFGPipeNetwork* Net0 = PipeSubsystem->FindPipeNetwork(Network0);
                AFGPipeNetwork* Net1 = PipeSubsystem->FindPipeNetwork(Network1);
                if (Net0 && Net1)
                {
                    Net0->MergeNetworks(Net1);
                    Net0->MarkForFullRebuild();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Merged networks %d and %d, marked for rebuild"),
                        Network0, Network1);
                }
            }
            else if (Network0 != INDEX_NONE)
            {
                AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network0);
                if (Net)
                {
                    Net->MarkForFullRebuild();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Marked network %d for rebuild"),
                        Network0);
                }
            }
            else if (Network1 != INDEX_NONE)
            {
                AFGPipeNetwork* Net = PipeSubsystem->FindPipeNetwork(Network1);
                if (Net)
                {
                    Net->MarkForFullRebuild();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Manifold Pipe Wire: Marked network %d for rebuild"),
                        Network1);
                }
            }
        }
    }
}



// [#401/#414] Route a manifold lane through the ENGINE router and return WORLD-space spline data
// ready for the built actor (Respline / GetMutableSplinePointData - both consume the same space
// the legacy hand-rolled points used). The manifold lanes were the last routed lane path still
// hand-rolling a fixed-tangent 2-point spline on the BUILT actor (no routing-mode support,
// length-independent x100 bow, slope ramp on non-flat manifolds). A transient routing hologram -
// the same classes every auto-connect preview uses, running the descriptor-correct engine path -
// computes the real route, is harvested, and destroyed. Normals are the connectors' OUTWARD
// facings (the router negates the end side internally, like every other lane path).
static bool RouteManifoldSpanViaEngine(UWorld* World, UClass* ConduitBuildClass, bool bIsPipe,
	const FVector& StartPos, const FVector& StartNormal, const FVector& EndPos, const FVector& EndNormal,
	TArray<FSplinePointData>& OutWorldSplineData)
{
	if (!World || !ConduitBuildClass)
	{
		return false;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	OutWorldSplineData.Reset();
	AFGHologram* ToDestroy = nullptr;
	const TArray<FSplinePointData>* LocalData = nullptr;
	FTransform SplineXf = FTransform::Identity;

	if (bIsPipe)
	{
		ASFPipelineHologram* Pipe = World->SpawnActor<ASFPipelineHologram>(ASFPipelineHologram::StaticClass(), StartPos, FRotator::ZeroRotator, Params);
		if (!Pipe)
		{
			return false;
		}
		Pipe->SetReplicates(false);
		Pipe->SetActorHiddenInGame(true);
		Pipe->SetActorEnableCollision(false);
		Pipe->SetBuildClass(ConduitBuildClass);
		Pipe->RoutePipeLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);
		LocalData = &Pipe->GetSplineData();
		SplineXf = Pipe->GetSplineComponent() ? Pipe->GetSplineComponent()->GetComponentTransform() : Pipe->GetActorTransform();
		ToDestroy = Pipe;
	}
	else
	{
		ASFConveyorBeltHologram* Belt = World->SpawnActor<ASFConveyorBeltHologram>(ASFConveyorBeltHologram::StaticClass(), StartPos, FRotator::ZeroRotator, Params);
		if (!Belt)
		{
			return false;
		}
		Belt->SetReplicates(false);
		Belt->SetActorHiddenInGame(true);
		Belt->SetActorEnableCollision(false);
		Belt->SetBuildClass(ConduitBuildClass);
		Belt->RouteLaneWithConfiguredMode(StartPos, StartNormal, EndPos, EndNormal);
		LocalData = &Belt->GetSplineData();
		SplineXf = Belt->GetSplineComponent() ? Belt->GetSplineComponent()->GetComponentTransform() : Belt->GetActorTransform();
		ToDestroy = Belt;
	}

	if (LocalData)
	{
		for (const FSplinePointData& Local : *LocalData)
		{
			FSplinePointData WorldPoint;
			WorldPoint.Location = SplineXf.TransformPosition(Local.Location);
			WorldPoint.ArriveTangent = SplineXf.TransformVectorNoScale(Local.ArriveTangent);
			WorldPoint.LeaveTangent = SplineXf.TransformVectorNoScale(Local.LeaveTangent);
			OutWorldSplineData.Add(WorldPoint);
		}
	}
	if (ToDestroy)
	{
		ToDestroy->Destroy();
	}

	if (OutWorldSplineData.Num() < 2)
	{
		SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
			TEXT("RouteManifoldSpanViaEngine: engine route produced %d points - caller falls back to the legacy 2-point spline"),
			OutWorldSplineData.Num());
		return false;
	}
	return true;
}

bool USFExtendWiringService::CreateManifoldPipe(UFGPipeConnectionComponentBase* FromConnector, UFGPipeConnectionComponentBase* ToConnector)
{
    if (!FromConnector || !ToConnector)
    {
        return false;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    // Get pipe tier from auto-connect settings (use Mk2 by default for now)
    int32 PipeTier = 2;  // Default to Mk2

    // Load pipe class
    FString PipePath = FString::Printf(TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk%d/Build_PipelineMK%d.Build_PipelineMK%d_C"),
        PipeTier, PipeTier, PipeTier);
    UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);
    if (!PipeClass)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Failed to load pipe class: %s"), *PipePath);
        return false;
    }

    // Calculate spline points for the pipe
    FVector StartPos = FromConnector->GetComponentLocation();
    FVector EndPos = ToConnector->GetComponentLocation();
    FVector StartForward = FromConnector->GetForwardVector();
    FVector EndForward = -ToConnector->GetForwardVector();

    // [#401/#414] Engine-route the manifold lane (honors the player's pipe routing mode,
    // length-aware bows, and chord pitch on sloped manifolds). The router takes OUTWARD
    // connector normals. Falls back to the legacy fixed-tangent 2-point spline on failure.
    TArray<FSplinePointData> SplineData;
    if (!RouteManifoldSpanViaEngine(World, PipeClass, /*bIsPipe=*/true,
        StartPos, StartForward, EndPos, ToConnector->GetForwardVector(), SplineData))
    {
        SplineData.Reset();
        FSplinePointData StartPoint;
        StartPoint.Location = StartPos;
        StartPoint.ArriveTangent = StartForward * 100.0f;
        StartPoint.LeaveTangent = StartForward * 100.0f;
        SplineData.Add(StartPoint);

        FSplinePointData EndPoint;
        EndPoint.Location = EndPos;
        EndPoint.ArriveTangent = EndForward * 100.0f;
        EndPoint.LeaveTangent = EndForward * 100.0f;
        SplineData.Add(EndPoint);
    }

    // Spawn pipe with DEFERRED construction (same technique as BuildExtendPipeAndReturn)
    FActorSpawnParameters SpawnParams;
    SpawnParams.bDeferConstruction = true;

    AFGBuildablePipeline* Pipe = World->SpawnActor<AFGBuildablePipeline>(PipeClass, StartPos, FRotator::ZeroRotator, SpawnParams);
    if (!Pipe)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Failed to spawn manifold pipe"));
        return false;
    }

    // Apply spline data BEFORE FinishSpawning
    TArray<FSplinePointData>* MutableSpline = Pipe->GetMutableSplinePointData();
    if (!MutableSpline)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("   ⚠️ Cannot get mutable spline data for manifold pipe"));
        Pipe->Destroy();
        return false;
    }
    *MutableSpline = SplineData;

    // Finish spawning and signal build complete
    Pipe->FinishSpawning(FTransform(FRotator::ZeroRotator, StartPos));
    FSFBuildEffectHelper::Finish(Pipe);

    // Connect the pipe
    UFGPipeConnectionComponent* PipeConn0 = Pipe->GetPipeConnection0();
    UFGPipeConnectionComponent* PipeConn1 = Pipe->GetPipeConnection1();

    if (PipeConn0 && PipeConn1)
    {
        PipeConn0->SetConnection(Cast<UFGPipeConnectionComponent>(FromConnector));
        PipeConn1->SetConnection(Cast<UFGPipeConnectionComponent>(ToConnector));

        // Merge pipe networks
        AFGPipeSubsystem* PipeSubsystem = AFGPipeSubsystem::Get(World);
        if (PipeSubsystem)
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
                }
            }
        }
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("   🔧 Created manifold pipe Mk%d between junctions"), PipeTier);
    return true;
}

AFGBuildableFactory* USFExtendWiringService::GetSourceFactory() const
{
    if (ExtendService->GetCurrentTopology().SourceBuilding.IsValid())
    {
        return Cast<AFGBuildableFactory>(ExtendService->GetCurrentTopology().SourceBuilding.Get());
    }
    return nullptr;
}

// ==================== Diagnostic Capture (delegates to ExtendService->DiagnosticsService) ====================

