// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * FSFPipeAutoConnectManager - connection evaluation / manifold-connector / pipe-child spawning /
 * floor-hole pipes. Split out of SFPipeAutoConnectManager.cpp (slice PM, pure impl-split, one
 * class across .cpp) to keep each file <2k. No behavior change.
 */

#include "Features/PipeAutoConnect/SFPipeAutoConnectManager.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "FGPlayerController.h"  // AFGPlayerController (don't rely on transitive unity-build includes)
#include "Subsystem/SFHologramHelperService.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "Features/PipeAutoConnect/SFPipeConnectorFinder.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableStorage.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePassthrough.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGPipelineHologram.h"
#include "FGPipeConnectionComponent.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Subsystem/SFHologramDataService.h"

void FSFPipeAutoConnectManager::EvaluatePipeConnections(AFGHologram* ParentJunctionHologram)
{
	if (!ParentJunctionHologram || !Subsystem || !AutoConnectService)
	{
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("PipeAutoConnectManager::EvaluatePipeConnections - Missing context"));
		return;
	}

	// Phase 2: Manifold connections (junction-to-junction chaining)
	// Group junctions by which building pipe INPUT INDEX they connect to (not specific connector instance)
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔗 Pipe Manifolds: Evaluating junction-to-junction chains"));
	
	// Track which junctions will be valid sources in this evaluation
	// Any existing manifold pipe whose source is NOT in this set should be cleaned up
	TSet<AFGHologram*> ValidManifoldSources;
	
	// Build map: Building Input Index → List of Junctions connected to that input
	TMap<int32, TArray<AFGHologram*>> JunctionsByInputIndex;
	
	for (const auto& Pair : ReservedConnectors)
	{
		UFGPipeConnectionComponent* BuildingConnector = Pair.Key;
		AFGHologram* Junction = Pair.Value;
		
		if (!BuildingConnector || !Junction)
		{
			continue;
		}
		
		// Get the building that owns this connector
		AActor* BuildingActor = BuildingConnector->GetOwner();
		if (!BuildingActor)
		{
			continue;
		}
		
		// Find which input/output index this connector is on the building
		TArray<UFGPipeConnectionComponent*> BuildingPipeConnectors;
		BuildingActor->GetComponents<UFGPipeConnectionComponent>(BuildingPipeConnectors);
		
		int32 ConnectorIndex = BuildingPipeConnectors.IndexOfByKey(BuildingConnector);
		if (ConnectorIndex == INDEX_NONE)
		{
			continue;
		}
		
		// Add junction to this input index's group (e.g., all Input0 junctions together)
		JunctionsByInputIndex.FindOrAdd(ConnectorIndex).Add(Junction);
		
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Junction %s → Building %s Input[%d]"),
			*Junction->GetName(), *BuildingActor->GetName(), ConnectorIndex);
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔗 Pipe Manifolds: Found %d input index groups"),
		JunctionsByInputIndex.Num());
	
	// For each input index group, chain the junctions together
	for (const auto& Group : JunctionsByInputIndex)
	{
		int32 InputIndex = Group.Key;
		const TArray<AFGHologram*>& JunctionsInGroup = Group.Value;
		
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   📋 Input[%d]: %d junction(s)"),
			InputIndex, JunctionsInGroup.Num());
		
		if (JunctionsInGroup.Num() < 2)
		{
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("      ⏭️ Skipping (need at least 2 junctions for manifold)"));
			
			// Clean up any existing manifold children for junctions in this group (no longer part of chain)
			for (AFGHologram* Junction : JunctionsInGroup)
			{
				TWeakObjectPtr<ASFPipelineHologram>* ExistingManifoldChild = ManifoldPipeChildren.Find(Junction);
				if (ExistingManifoldChild && ExistingManifoldChild->IsValid())
				{
					UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Removing manifold child (group too small) for junction %s"),
						*Junction->GetName());
					RemovePipeChild(Junction, ExistingManifoldChild->Get());
					ManifoldPipeChildren.Remove(Junction);
					ManifoldTargetJunctions.Remove(Junction);
				}
			}
			continue; // Need at least 2 junctions to chain
		}
		
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("   🔗 Input[%d]: Chaining %d junctions in manifold"),
			InputIndex, JunctionsInGroup.Num());
		
		// #423: Sort junctions along the manifold's DOMINANT run axis (X vs Y), not always world-X, so a
		// manifold running along Y (or a rotated / restore-preview lane) chains in order instead of
		// scrambling and wrapping. Mirrors the belt path's dominant-axis pick.
		TArray<AFGHologram*> SortedJunctions = JunctionsInGroup;
		{
			double MinX = TNumericLimits<double>::Max(), MaxX = TNumericLimits<double>::Lowest();
			double MinY = MinX, MaxY = MaxX;
			for (const AFGHologram* J : SortedJunctions)
			{
				if (!J) continue;
				const FVector P = J->GetActorLocation();
				MinX = FMath::Min(MinX, P.X); MaxX = FMath::Max(MaxX, P.X);
				MinY = FMath::Min(MinY, P.Y); MaxY = FMath::Max(MaxY, P.Y);
			}
			const bool bSortByY = (MaxY - MinY) > (MaxX - MinX);
			SortedJunctions.Sort([bSortByY](const AFGHologram& A, const AFGHologram& B)
			{
				const FVector PA = A.GetActorLocation();
				const FVector PB = B.GetActorLocation();
				return bSortByY ? (PA.Y < PB.Y) : (PA.X < PB.X);
			});
		}
		
		// Chain junctions ONWARD in the lane - not necessarily to the immediate next member.
		// [#464] Mirrors the belt manifold lookahead: a staggered two-level lane interleaves BOTH
		// levels along the run, so the consecutive pair is cross-level (too steep / not facing) and
		// the old i->i+1-only pairing dropped the manifold link entirely. Each junction now considers
		// the next few lane members as continuation candidates, preferring its OWN level
		// (LevelAffinityPenalty) then proximity: a zigzag lane resolves into two flat interleaved
		// manifolds, while flat and gently-stepped lanes keep chaining consecutively (their best
		// candidate IS i+1).
		constexpr int32 PipeManifoldLookahead = 3;
		for (int32 i = 0; i < SortedJunctions.Num() - 1; i++)
		{
			AFGHologram* SourceJunction = SortedJunctions[i];
			if (!SourceJunction)
			{
				continue;
			}
			const FVector SourceJunctionPos = SourceJunction->GetActorLocation();

			// Candidate continuation targets: same level first, then nearest
			TArray<int32> CandidateIndices;
			for (int32 j = i + 1; j < SortedJunctions.Num() && j <= i + PipeManifoldLookahead; j++)
			{
				if (SortedJunctions[j])
				{
					CandidateIndices.Add(j);
				}
			}
			CandidateIndices.Sort([&SortedJunctions, &SourceJunctionPos](int32 IdxA, int32 IdxB)
			{
				const FVector PA = SortedJunctions[IdxA]->GetActorLocation();
				const FVector PB = SortedJunctions[IdxB]->GetActorLocation();
				const float KeyA = USFAutoConnectService::LevelAffinityPenalty(SourceJunctionPos, PA) + FVector::Dist(SourceJunctionPos, PA);
				const float KeyB = USFAutoConnectService::LevelAffinityPenalty(SourceJunctionPos, PB) + FVector::Dist(SourceJunctionPos, PB);
				return KeyA < KeyB;
			});

			AFGHologram* TargetJunction = nullptr;
			UFGPipeConnectionComponent* SourceConnector = nullptr;
			UFGPipeConnectionComponent* TargetConnector = nullptr;
			for (int32 CandidateIdx : CandidateIndices)
			{
				AFGHologram* Candidate = SortedJunctions[CandidateIdx];
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("      Chaining %s → %s (lookahead +%d)"),
					*SourceJunction->GetName(), *Candidate->GetName(), CandidateIdx - i);

				// Find best facing connector pair between the two junctions
				FindBestManifoldConnectorPair(SourceJunction, Candidate, SourceConnector, TargetConnector);
				if (SourceConnector && TargetConnector)
				{
					TargetJunction = Candidate;
					break;
				}
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("         ❌ No valid connector pair - trying next candidate"));
			}

			if (!TargetJunction || !SourceConnector || !TargetConnector)
			{
				UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("         ❌ No valid manifold continuation within lookahead"));
				continue;  // Don't add to ValidManifoldSources - will be cleaned up at end
			}
			
			// This source junction has a valid manifold connection - track it
			ValidManifoldSources.Add(SourceJunction);
			
			// Check if we already have a child hologram for this manifold
			TWeakObjectPtr<ASFPipelineHologram>* ExistingChild = ManifoldPipeChildren.Find(SourceJunction);
			
			if (ExistingChild && ExistingChild->IsValid())
			{
				// UPDATE existing child hologram with new spline data (matches stackable pipe pattern)
				ASFPipelineHologram* PipeChild = ExistingChild->Get();
				
				// CHECK: If pipe tier/style settings changed, destroy and recreate with new tier
				const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
				int32 ConfigTier = RuntimeSettings.PipeTierMain;  // Manifold connections use PipeTierMain
				bool bWithIndicator = RuntimeSettings.bPipeIndicator;
				
				AFGPlayerController* PlayerController = Cast<AFGPlayerController>(PipeChild->GetWorld()->GetFirstPlayerController());
				UClass* ExpectedBuildClass = Subsystem->GetPipeClassFromConfig(ConfigTier, bWithIndicator, PlayerController);
				UClass* CurrentBuildClass = PipeChild->GetBuildClass();
				
				if (ExpectedBuildClass && CurrentBuildClass != ExpectedBuildClass)
				{
					// Tier/style changed - destroy existing and spawn new with correct tier
					UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE MANIFOLD: Tier changed - recreating pipe for junction %s (old=%s, new=%s)"),
						*SourceJunction->GetName(),
						CurrentBuildClass ? *CurrentBuildClass->GetName() : TEXT("null"),
						*ExpectedBuildClass->GetName());
					
					RemovePipeChild(SourceJunction, PipeChild);
					ManifoldPipeChildren.Remove(SourceJunction);
					ManifoldTargetJunctions.Remove(SourceJunction);
					
					// Spawn new child with correct tier
					ASFPipelineHologram* NewChild = SpawnPipeChild(
						SourceJunction,
						SourceConnector,
						TargetConnector,
						true);  // bIsManifold = true (junction→junction)
					
					if (NewChild)
					{
						ManifoldPipeChildren.Add(SourceJunction, NewChild);
						ManifoldTargetJunctions.Add(SourceJunction, TargetJunction);
						UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE MANIFOLD: Recreated with new tier for junction %s"),
							*SourceJunction->GetName());
					}
					continue;  // Skip the update logic below since we just spawned a new child
				}
				
				// CRITICAL: Temporarily unlock child before updating (from HologramHelper pattern)
				// Parent lock blocks child transform updates - must unlock, update, then re-lock
				bool bParentLocked = SourceJunction->IsHologramLocked();
				bool bChildWasLocked = PipeChild->IsHologramLocked();
				if (bChildWasLocked)
				{
					PipeChild->LockHologramPosition(false);  // Unlock for update
				}
				
				// CRITICAL: Calculate connector positions using actor location + relative offset
				// This matches the stackable pipe pattern and works correctly when parent is locked
				FVector SourcePos = SourceJunction->GetActorLocation();
				FRotator SourceRot = SourceJunction->GetActorRotation();
				FVector SourceRelative = SourceConnector->GetRelativeLocation();
				FVector StartPos = SourcePos + SourceRot.RotateVector(SourceRelative);
				
				FVector TargetPos = TargetJunction->GetActorLocation();
				FRotator TargetRot = TargetJunction->GetActorRotation();
				FVector TargetRelative = TargetConnector->GetRelativeLocation();
				FVector EndPos = TargetPos + TargetRot.RotateVector(TargetRelative);
				
				// Calculate normals based on junction rotation
				FVector StartNormal = SourceRot.RotateVector(SourceConnector->GetRelativeRotation().Vector());
				if (StartNormal.IsNearlyZero()) StartNormal = SourceConnector->GetConnectorNormal();
				FVector EndNormal = TargetRot.RotateVector(TargetConnector->GetRelativeRotation().Vector());
				if (EndNormal.IsNearlyZero()) EndNormal = TargetConnector->GetConnectorNormal();
				
				// Update position and spline data
				PipeChild->SetActorLocation(StartPos);
				PipeChild->SetRoutingMode(RuntimeSettings.PipeRoutingMode);
				if (!PipeChild->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
				{
					// Fallback: Generate spline data manually
					const float Distance = FVector::Dist(StartPos, EndPos);
					const float SmallTangent = 50.0f;
					const float LargeTangent = Distance * 0.435f;
					const float FlatSectionLength = Distance * 0.047f;
					const float TransitionOffset = Distance * 0.070f;
					FVector Direction = (EndPos - StartPos).GetSafeNormal();
					
					TArray<FSplinePointData> SplinePoints;
					
					FSplinePointData Point0;
					Point0.Location = StartPos;
					Point0.ArriveTangent = StartNormal * SmallTangent;
					Point0.LeaveTangent = StartNormal * SmallTangent;
					SplinePoints.Add(Point0);
					
					FSplinePointData Point1;
					Point1.Location = StartPos + StartNormal * FlatSectionLength;
					Point1.ArriveTangent = StartNormal * SmallTangent;
					Point1.LeaveTangent = StartNormal * (SmallTangent * 0.99f);
					SplinePoints.Add(Point1);
					
					FSplinePointData Point2;
					Point2.Location = StartPos + StartNormal * TransitionOffset;
					Point2.ArriveTangent = StartNormal * SmallTangent;
					Point2.LeaveTangent = Direction * LargeTangent;
					SplinePoints.Add(Point2);
					
					FSplinePointData Point3;
					Point3.Location = EndPos + EndNormal * TransitionOffset;
					Point3.ArriveTangent = Direction * LargeTangent;
					Point3.LeaveTangent = -EndNormal * SmallTangent;
					SplinePoints.Add(Point3);
					
					FSplinePointData Point4;
					Point4.Location = EndPos + EndNormal * FlatSectionLength;
					Point4.ArriveTangent = -EndNormal * (SmallTangent * 0.99f);
					Point4.LeaveTangent = -EndNormal * SmallTangent;
					SplinePoints.Add(Point4);
					
					FSplinePointData Point5;
					Point5.Location = EndPos;
					Point5.ArriveTangent = -EndNormal * SmallTangent;
					Point5.LeaveTangent = -EndNormal * SmallTangent;
					SplinePoints.Add(Point5);
					
					PipeChild->SetSplineDataAndUpdate(SplinePoints);
				}
				
				// Regenerate mesh and apply hologram material
				PipeChild->TriggerMeshGeneration();
				PipeChild->ForceApplyHologramMaterial();
				
				// CRITICAL: Re-lock child if parent is locked (from HologramHelper pattern)
				// Children must match parent's lock state for visibility
				if (bParentLocked)
				{
					PipeChild->LockHologramPosition(true);
				}
				PipeChild->SetActorHiddenInGame(false);
				PipeChild->SetPlacementMaterialState(SourceJunction->GetHologramMaterialState());
				
				// Update target junction tracking (in case target changed)
				ManifoldTargetJunctions.Add(SourceJunction, TargetJunction);
				
				UE_LOG(LogSmartAutoConnect, Verbose, TEXT("         🔄 Updated existing manifold pipe child (parentLocked=%d, childRelocked=%d)"),
					bParentLocked ? 1 : 0, bParentLocked ? 1 : 0);
			}
			else
			{
				// Spawn new child hologram for manifold using stackable pipe pattern
				ASFPipelineHologram* NewChild = SpawnPipeChild(
					SourceJunction,
					SourceConnector,
					TargetConnector,
					true);  // bIsManifold = true (junction→junction)
				
				if (NewChild)
				{
					ManifoldPipeChildren.Add(SourceJunction, NewChild);
					ManifoldTargetJunctions.Add(SourceJunction, TargetJunction);  // Track target for cleanup
					UE_LOG(LogSmartAutoConnect, Verbose, TEXT("         ✅ Created manifold pipe child (now have %d manifold children)"),
						ManifoldPipeChildren.Num());
				}
			}
			
			// DEPRECATED: Keep old preview system for transition (will be removed)
			TSharedPtr<FPipePreviewHelper>* ExistingPreview = ManifoldPipePreviews.Find(SourceJunction);
			if (ExistingPreview && ExistingPreview->IsValid())
			{
				(*ExistingPreview)->UpdatePreview(SourceConnector, TargetConnector);
			}
		}
	}
	
	// CRITICAL: Clean up any manifold pipes whose source is no longer a valid source in the current chain
	// This handles the case where the grid configuration changed and a junction that was previously
	// a source is now a target (or not in a chain at all)
	TArray<AFGHologram*> StaleManifoldSources;
	for (const auto& Pair : ManifoldPipeChildren)
	{
		AFGHologram* SourceJunction = Pair.Key;
		if (!ValidManifoldSources.Contains(SourceJunction))
		{
			StaleManifoldSources.Add(SourceJunction);
		}
	}
	
	for (AFGHologram* StaleSource : StaleManifoldSources)
	{
		TWeakObjectPtr<ASFPipelineHologram>* ManifoldChild = ManifoldPipeChildren.Find(StaleSource);
		if (ManifoldChild && ManifoldChild->IsValid())
		{
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Cleaning up stale manifold pipe for %s (no longer valid source in chain)"),
				*GetNameSafe(StaleSource));
			RemovePipeChild(StaleSource, ManifoldChild->Get());
		}
		ManifoldPipeChildren.Remove(StaleSource);
		ManifoldTargetJunctions.Remove(StaleSource);
	}
	
	if (StaleManifoldSources.Num() > 0)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE: Cleaned up %d stale manifold pipe(s), now have %d manifold children"),
			StaleManifoldSources.Num(), ManifoldPipeChildren.Num());
	}
}

int32 FSFPipeAutoConnectManager::GetConnectorIndex(AFGHologram* JunctionHologram, UFGPipeConnectionComponent* Connector)
{
	if (!JunctionHologram || !Connector)
	{
		return INDEX_NONE;
	}
	
	TArray<UFGPipeConnectionComponent*> JunctionConnectors;
	FSFPipeConnectorFinder::GetJunctionConnectors(JunctionHologram, JunctionConnectors);
	
	return JunctionConnectors.IndexOfByKey(Connector);
}

int32 FSFPipeAutoConnectManager::GetOppositeConnectorIndex(int32 Index, int32 TotalConnectors)
{
	// DEPRECATED: Hardcoded mapping doesn't match physical connector layout.
	// GetComponents() ordering is arbitrary — use GetOppositeConnectorByNormal() instead.
	// Keeping for API compatibility but prefer the normal-based version.
	if (TotalConnectors == 4)
	{
		if (Index == 0) return 2;
		if (Index == 1) return 3;
		if (Index == 2) return 0;
		if (Index == 3) return 1;
	}
	return Index;
}

int32 FSFPipeAutoConnectManager::GetOppositeConnectorByNormal(
	int32 SourceIndex,
	const TArray<UFGPipeConnectionComponent*>& Connectors)
{
	// Issue #206 fix: Find the connector whose normal is most opposite to the source connector's normal.
	// GetComponents() ordering is arbitrary and doesn't correspond to physical positions,
	// so we must use actual world-space normals to find the true physical opposite.
	if (SourceIndex < 0 || SourceIndex >= Connectors.Num() || !Connectors[SourceIndex])
	{
		return INDEX_NONE;
	}
	
	FVector SourceNormal = Connectors[SourceIndex]->GetConnectorNormal();
	int32 BestIdx = INDEX_NONE;
	float BestDot = 1.0f; // Want most negative dot product (most opposite direction)
	
	for (int32 i = 0; i < Connectors.Num(); i++)
	{
		if (i == SourceIndex || !Connectors[i]) continue;
		
		FVector ConnNormal = Connectors[i]->GetConnectorNormal();
		float Dot = FVector::DotProduct(SourceNormal, ConnNormal);
		if (Dot < BestDot)
		{
			BestDot = Dot;
			BestIdx = i;
		}
	}
	
	UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("   GetOppositeByNormal: Source[%d] Normal=(%.2f,%.2f,%.2f) → Opposite[%d] Dot=%.2f"),
		SourceIndex, SourceNormal.X, SourceNormal.Y, SourceNormal.Z, BestIdx, BestDot);
	
	return BestIdx;
}

UFGPipeConnectionComponent* FSFPipeAutoConnectManager::FindAvailableManifoldConnector(AFGHologram* JunctionHologram)
{
	if (!JunctionHologram)
	{
		return nullptr;
	}
	
	// Get all connectors on this junction
	TArray<UFGPipeConnectionComponent*> JunctionConnectors;
	FSFPipeConnectorFinder::GetJunctionConnectors(JunctionHologram, JunctionConnectors);
	
	if (JunctionConnectors.Num() < 2)
	{
		// Issue #320: pipe junctions vary in port count (Cross = 4, T-Junction = 3). A manifold
		// needs at least a building port + one side port; the perpendicular-port search below is
		// geometry-driven (by connector normal), so any count >= 2 works.
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("Junction %s has %d pipe connectors (need >= 2)"),
			*JunctionHologram->GetName(), JunctionConnectors.Num());
		return JunctionConnectors.Num() > 0 ? JunctionConnectors[0] : nullptr;
	}
	
	// Find which connector this junction is using for its building connection (Phase 1)
	int32 BuildingConnectorIdx = INDEX_NONE;
	int32* StoredIdx = ParentConnectorIndices.Find(JunctionHologram);
	if (StoredIdx)
	{
		BuildingConnectorIdx = *StoredIdx;
	}
	
	if (BuildingConnectorIdx == INDEX_NONE || BuildingConnectorIdx >= JunctionConnectors.Num())
	{
		// No building connection yet, use first connector for manifold
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("            No building connection found, using connector 0 for manifold"));
		return JunctionConnectors[0];
	}
	
	// Get the building-connected port's direction
	UFGPipeConnectionComponent* BuildingPort = JunctionConnectors[BuildingConnectorIdx];
	FVector BuildingPortNormal = BuildingPort->GetConnectorNormal();
	
	// Find a connector that's PERPENDICULAR to the building port (side port, not opposite)
	// We want dot product ≈ 0 (perpendicular), not ≈ -1 (opposite)
	UFGPipeConnectionComponent* BestManifoldPort = nullptr;
	float BestPerpendicularScore = -1.0f; // Higher score = more perpendicular
	
	for (int32 i = 0; i < JunctionConnectors.Num(); i++)
	{
		if (i == BuildingConnectorIdx)
		{
			continue; // Skip the building-connected port itself
		}
		
		UFGPipeConnectionComponent* CandidatePort = JunctionConnectors[i];
		FVector CandidateNormal = CandidatePort->GetConnectorNormal();
		
		// Calculate how perpendicular this port is (dot product close to 0 is best)
		float DotProduct = FVector::DotProduct(BuildingPortNormal, CandidateNormal);
		float PerpendicularScore = 1.0f - FMath::Abs(DotProduct); // 1.0 = perfect perpendicular, 0.0 = parallel/opposite
		
		if (PerpendicularScore > BestPerpendicularScore)
		{
			BestPerpendicularScore = PerpendicularScore;
			BestManifoldPort = CandidatePort;
		}
	}
	
	if (BestManifoldPort)
	{
		int32 ManifoldIdx = JunctionConnectors.IndexOfByKey(BestManifoldPort);
		UE_LOG(LogSmartAutoConnect, VeryVerbose, TEXT("            Building on connector %d, using perpendicular connector %d for manifold (score=%.2f)"), 
			BuildingConnectorIdx, ManifoldIdx, BestPerpendicularScore);
		return BestManifoldPort;
	}
	
	// Fallback: return any available connector (should never reach here)
	return JunctionConnectors.Num() > 0 ? JunctionConnectors[0] : nullptr;
}

void FSFPipeAutoConnectManager::FindBestManifoldConnectorPair(
	AFGHologram* SourceJunction,
	AFGHologram* TargetJunction,
	UFGPipeConnectionComponent*& OutSourceConnector,
	UFGPipeConnectionComponent*& OutTargetConnector)
{
	OutSourceConnector = nullptr;
	OutTargetConnector = nullptr;
	
	if (!SourceJunction || !TargetJunction)
	{
		return;
	}
	
	// Get all connectors on both junctions
	TArray<UFGPipeConnectionComponent*> SourceConnectors;
	TArray<UFGPipeConnectionComponent*> TargetConnectors;
	FSFPipeConnectorFinder::GetJunctionConnectors(SourceJunction, SourceConnectors);
	FSFPipeConnectorFinder::GetJunctionConnectors(TargetJunction, TargetConnectors);
	
	if (SourceConnectors.Num() < 2 || TargetConnectors.Num() < 2)
	{
		// Issue #320: pipe junctions vary in port count (Cross = 4, T-Junction = 3). The pairing
		// below is geometry-driven (facing/alignment by connector normal), so any count >= 2 is fine.
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("Manifold: a junction has too few pipe connectors (Source=%d, Target=%d, need >= 2)"),
			SourceConnectors.Num(), TargetConnectors.Num());
		return;
	}
	
	// Get which connectors are used for building connections
	int32 SourceBuildingIdx = INDEX_NONE;
	int32 TargetBuildingIdx = INDEX_NONE;
	
	int32* SourceStoredIdx = ParentConnectorIndices.Find(SourceJunction);
	if (SourceStoredIdx) SourceBuildingIdx = *SourceStoredIdx;
	
	int32* TargetStoredIdx = ParentConnectorIndices.Find(TargetJunction);
	if (TargetStoredIdx) TargetBuildingIdx = *TargetStoredIdx;
	
	// Calculate direction vector between junctions
	FVector SourcePos = SourceJunction->GetActorLocation();
	FVector TargetPos = TargetJunction->GetActorLocation();
	FVector DirectionToTarget = (TargetPos - SourcePos).GetSafeNormal();
	
	// Find best connector pair that faces toward each other
	// IMPORTANT: Use deterministic tie-breaking to prevent manifold crossover when junctions
	// are positioned symmetrically between connectors. When scores are equal, prefer:
	// 1. Lower source connector index (consistent ordering)
	// 2. Lower target connector index (consistent ordering)
	float BestScore = -1.0f;
	int32 BestSrcIdx = INDEX_NONE;
	int32 BestTgtIdx = INDEX_NONE;
	
	for (int32 SrcIdx = 0; SrcIdx < SourceConnectors.Num(); SrcIdx++)
	{
		// Skip if this is the building-connected port
		if (SrcIdx == SourceBuildingIdx)
		{
			continue;
		}
		
		UFGPipeConnectionComponent* SrcConn = SourceConnectors[SrcIdx];
		if (!SrcConn) continue;
		
		FVector SrcNormal = SrcConn->GetConnectorNormal();
		
		// How well does this source connector face toward the target?
		float SrcAlignment = FVector::DotProduct(SrcNormal, DirectionToTarget);
		
		// Skip if not generally facing target direction (dot < 0.3)
		if (SrcAlignment < 0.3f)
		{
			continue;
		}
		
		for (int32 TgtIdx = 0; TgtIdx < TargetConnectors.Num(); TgtIdx++)
		{
			// Skip if this is the building-connected port
			if (TgtIdx == TargetBuildingIdx)
			{
				continue;
			}
			
			UFGPipeConnectionComponent* TgtConn = TargetConnectors[TgtIdx];
			if (!TgtConn) continue;
			
			FVector TgtNormal = TgtConn->GetConnectorNormal();
			
			// How well does this target connector face back toward source?
			float TgtAlignment = FVector::DotProduct(TgtNormal, -DirectionToTarget);
			
			// Skip if not generally facing source direction (dot < 0.3)
			if (TgtAlignment < 0.3f)
			{
				continue;
			}
			
			// Check that connectors face toward each other (normals should be opposite)
			float FacingScore = -FVector::DotProduct(SrcNormal, TgtNormal);
			
			// Combined score: alignment + facing
			float TotalScore = (SrcAlignment + TgtAlignment + FacingScore) / 3.0f;
			
			// Calculate total distance for this connector pair (used for tie-breaking)
			FVector SrcConnWorld = SrcConn->GetComponentLocation();
			FVector TgtConnWorld = TgtConn->GetComponentLocation();
			float PairDistance = FVector::Dist(SrcConnWorld, TgtConnWorld);
			
			// Deterministic tie-breaking when scores are nearly equal:
			// 1. Prefer shorter distance (closer junctions claim connectors first)
			// 2. If still tied, prefer lower connector indices (stable ordering)
			const float ScoreEpsilon = 0.001f;
			const float DistanceEpsilon = 1.0f; // 1cm tolerance for distance comparison
			bool bIsBetterScore = TotalScore > BestScore + ScoreEpsilon;
			bool bIsTiedScore = FMath::Abs(TotalScore - BestScore) <= ScoreEpsilon;
			
			// For tie-breaking: shorter distance wins, then lower indices
			bool bWinsTieBreaker = false;
			if (bIsTiedScore && BestSrcIdx != INDEX_NONE)
			{
				// Calculate best pair's distance for comparison
				FVector BestSrcWorld = SourceConnectors[BestSrcIdx]->GetComponentLocation();
				FVector BestTgtWorld = TargetConnectors[BestTgtIdx]->GetComponentLocation();
				float BestDistance = FVector::Dist(BestSrcWorld, BestTgtWorld);
				
				bool bShorterDistance = PairDistance < BestDistance - DistanceEpsilon;
				bool bSameDistance = FMath::Abs(PairDistance - BestDistance) <= DistanceEpsilon;
				bool bLowerIndices = (SrcIdx < BestSrcIdx || (SrcIdx == BestSrcIdx && TgtIdx < BestTgtIdx));
				
				bWinsTieBreaker = bShorterDistance || (bSameDistance && bLowerIndices);
			}
			
			if (bIsBetterScore || bWinsTieBreaker)
			{
				BestScore = TotalScore;
				BestSrcIdx = SrcIdx;
				BestTgtIdx = TgtIdx;
				OutSourceConnector = SrcConn;
				OutTargetConnector = TgtConn;
				
				UE_LOG(LogSmartAutoConnect, VeryVerbose, 
					TEXT("            Better pair: Src[%d]→Tgt[%d] (SrcAlign=%.2f TgtAlign=%.2f Facing=%.2f Total=%.2f Dist=%.1f%s)"),
					SrcIdx, TgtIdx, SrcAlignment, TgtAlignment, FacingScore, TotalScore, PairDistance,
					bWinsTieBreaker ? TEXT(" [tie-break]") : TEXT(""));
			}
		}
	}
	
	if (OutSourceConnector && OutTargetConnector)
	{
		int32 FinalSrcIdx = SourceConnectors.IndexOfByKey(OutSourceConnector);
		int32 FinalTgtIdx = TargetConnectors.IndexOfByKey(OutTargetConnector);
		UE_LOG(LogSmartAutoConnect, Verbose,
			TEXT("         ✅ Selected manifold pair: Src[%d]→Tgt[%d] (score=%.2f)"),
			FinalSrcIdx, FinalTgtIdx, BestScore);
	}
}

// ========================================
// CHILD HOLOGRAM SPAWNING (Issue #235)
// ========================================

ASFPipelineHologram* FSFPipeAutoConnectManager::SpawnPipeChild(
	AFGHologram* ParentJunction,
	UFGPipeConnectionComponent* JunctionConnector,
	UFGPipeConnectionComponent* TargetConnector,
	bool bIsManifold)
{
	if (!ParentJunction || !JunctionConnector || !TargetConnector || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: SpawnPipeChild called with invalid parameters"));
		return nullptr;
	}
	
	UWorld* World = ParentJunction->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: No world context"));
		return nullptr;
	}
	
	// Get configured pipe tier and style
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	int32 ConfigTier = bIsManifold ? RuntimeSettings.PipeTierMain : RuntimeSettings.PipeTierToBuilding;
	bool bWithIndicator = RuntimeSettings.bPipeIndicator;
	
	// Get player controller for tier resolution (Auto mode)
	AFGPlayerController* PlayerController = Cast<AFGPlayerController>(World->GetFirstPlayerController());
	
	// Resolve "Auto" tier (0) to actual tier - CRITICAL for recipe lookup
	// Same pattern as stackable pipe pole auto-connect
	int32 ActualTier = ConfigTier;
	if (ConfigTier == 0)
	{
		ActualTier = Subsystem->GetHighestUnlockedPipeTier(PlayerController);
		if (ActualTier == 0) ActualTier = 1; // Default to Mk1 if detection fails
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Auto mode resolved to Mk%d"), ActualTier);
	}
	
	// Get pipe build class
	UClass* PipeBuildClass = Subsystem->GetPipeClassFromConfig(ConfigTier, bWithIndicator, PlayerController);
	if (!PipeBuildClass)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: No pipe build class for tier %d"), ActualTier);
		return nullptr;
	}
	
	// Get pipe recipe for cost aggregation - use resolved ActualTier, not ConfigTier!
	TSubclassOf<UFGRecipe> PipeRecipe = Subsystem->GetPipeRecipeForTier(ActualTier, bWithIndicator);
	
	// Get connector positions and normals
	FVector StartPos = JunctionConnector->GetComponentLocation();
	FVector EndPos = TargetConnector->GetComponentLocation();
	FVector StartNormal = JunctionConnector->GetConnectorNormal();
	FVector EndNormal = TargetConnector->GetConnectorNormal();
	
	// Generate unique child name
	FName ChildName(*FString::Printf(TEXT("PipeAutoConnect_%d"), PipeChildCounter++));
	
	// STACKABLE PIPE PATTERN: Use manual SpawnActor<ASFPipelineHologram> with deferred construction
	// This matches the belt auto-connect and stackable pipe pole patterns exactly.
	// SpawnChildHologramFromRecipe() spawns vanilla holograms which don't have our spline routing methods.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = ChildName;
	SpawnParams.Owner = ParentJunction->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;
	
	ASFPipelineHologram* PipeChild = World->SpawnActor<ASFPipelineHologram>(
		ASFPipelineHologram::StaticClass(),
		StartPos,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!PipeChild)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: SpawnActor returned null"));
		return nullptr;
	}
	
	// Configure BEFORE FinishSpawning (matches stackable pipe pattern)
	PipeChild->SetReplicates(false);
	PipeChild->SetReplicateMovement(false);
	PipeChild->SetBuildClass(PipeBuildClass);
	PipeChild->Tags.AddUnique(FName(TEXT("SF_PipeAutoConnectChild")));
	
	// Set recipe for vanilla cost aggregation - CRITICAL!
	if (PipeRecipe)
	{
		PipeChild->SetRecipe(PipeRecipe);
	}
	
	// Mark as child hologram via data service
	USFHologramDataService::DisableValidation(PipeChild);
	USFHologramDataService::MarkAsChild(PipeChild, ParentJunction, ESFChildHologramType::AutoConnect);
	
	// Store connector references for post-build wiring
	FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(PipeChild);
	if (HoloData)
	{
		HoloData->bIsPipeAutoConnectChild = true;
		HoloData->PipeAutoConnectConn0 = JunctionConnector;
		HoloData->PipeAutoConnectConn1 = TargetConnector;
		HoloData->bIsPipeManifold = bIsManifold;
	}
	
	// Finish spawning (matches stackable pipe pattern)
	PipeChild->FinishSpawning(FTransform(StartPos));
	
	// Set snapped connections via reflection (like stackable pipes)
	FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
	if (SnappedProp)
	{
		void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(PipeChild);
		UFGPipeConnectionComponentBase** SnappedArray = static_cast<UFGPipeConnectionComponentBase**>(PropAddr);
		if (SnappedArray)
		{
			SnappedArray[0] = JunctionConnector;
			SnappedArray[1] = TargetConnector;
		}
	}
	
	// Apply routing mode from settings
	PipeChild->SetRoutingMode(RuntimeSettings.PipeRoutingMode);
	
	// Try build mode routing first (same pattern as stackable pipe poles)
	if (!PipeChild->TryUseBuildModeRouting(StartPos, StartNormal, EndPos, EndNormal))
	{
		// Fallback: Generate spline data manually and use SetSplineDataAndUpdate
		// This ensures the spline component is properly initialized
		const float Distance = FVector::Dist(StartPos, EndPos);
		const float SmallTangent = 50.0f;
		const float LargeTangent = Distance * 0.435f;
		const float FlatSectionLength = Distance * 0.047f;
		const float TransitionOffset = Distance * 0.070f;
		FVector Direction = (EndPos - StartPos).GetSafeNormal();
		
		TArray<FSplinePointData> SplinePoints;
		
		// Point 0: Start connector
		FSplinePointData Point0;
		Point0.Location = StartPos;
		Point0.ArriveTangent = StartNormal * SmallTangent;
		Point0.LeaveTangent = StartNormal * SmallTangent;
		SplinePoints.Add(Point0);
		
		// Point 1: Flat section near start
		FSplinePointData Point1;
		Point1.Location = StartPos + StartNormal * FlatSectionLength;
		Point1.ArriveTangent = StartNormal * SmallTangent;
		Point1.LeaveTangent = StartNormal * (SmallTangent * 0.99f);
		SplinePoints.Add(Point1);
		
		// Point 2: Transition point
		FSplinePointData Point2;
		Point2.Location = StartPos + StartNormal * TransitionOffset;
		Point2.ArriveTangent = StartNormal * SmallTangent;
		Point2.LeaveTangent = Direction * LargeTangent;
		SplinePoints.Add(Point2);
		
		// Point 3: Middle curve point
		FSplinePointData Point3;
		Point3.Location = EndPos + EndNormal * TransitionOffset;
		Point3.ArriveTangent = Direction * LargeTangent;
		Point3.LeaveTangent = -EndNormal * SmallTangent;
		SplinePoints.Add(Point3);
		
		// Point 4: Flat section near end
		FSplinePointData Point4;
		Point4.Location = EndPos + EndNormal * FlatSectionLength;
		Point4.ArriveTangent = -EndNormal * (SmallTangent * 0.99f);
		Point4.LeaveTangent = -EndNormal * SmallTangent;
		SplinePoints.Add(Point4);
		
		// Point 5: End connector
		FSplinePointData Point5;
		Point5.Location = EndPos;
		Point5.ArriveTangent = -EndNormal * SmallTangent;
		Point5.LeaveTangent = -EndNormal * SmallTangent;
		SplinePoints.Add(Point5);
		
		PipeChild->SetSplineDataAndUpdate(SplinePoints);
	}
	
	// Configure visibility and state (matches stackable pipe pattern exactly)
	PipeChild->SetActorHiddenInGame(false);
	PipeChild->SetActorEnableCollision(false);
	PipeChild->SetActorTickEnabled(false);
	PipeChild->RegisterAllComponents();
	PipeChild->SetPlacementMaterialState(ParentJunction->GetHologramMaterialState());
	
	// CRITICAL: Sync lock state with parent at spawn time (from stackable pipe pattern)
	// Children must match parent's lock state for proper visibility and position updates
	bool bParentLocked = ParentJunction->IsHologramLocked();
	if (bParentLocked)
	{
		PipeChild->LockHologramPosition(true);
	}
	
	// Add as child to parent for vanilla cost aggregation (CRITICAL - matches stackable pattern)
	ParentJunction->AddChild(PipeChild, ChildName);
	
	// Trigger mesh generation AFTER AddChild (matches stackable pipe pattern exactly)
	PipeChild->TriggerMeshGeneration();
	PipeChild->ForceApplyHologramMaterial();

	// [#466] THE GAME IS THE SHAPE ARBITER (belt/pipe parity). TryUseBuildModeRouting above set
	// bRoutedShapeInvalid if the routed spline's min bend radius dropped below vanilla's limit -
	// the SAME "Invalid Pipe Shape!" a hand-built pipe of this shape produces. The old fixed 30°
	// chord gate (now relaxed to FACING_SANITY_ANGLE) used to pre-reject steep-but-buildable runs;
	// now we route first and decline only what the game itself won't build. Mirrors the belt
	// CreateOrUpdateBeltPreview arbiter so the two paths don't diverge.
	if (PipeChild->IsRoutedShapeInvalid())
	{
		// Tally building connections only (manifold declines just vanish, like belt lanes)
		if (!bIsManifold && Subsystem && Subsystem->GetAutoConnectService())
		{
			Subsystem->GetAutoConnectService()->GetSkipSummary().PipesInvalidShape++;
		}
		UE_LOG(LogSmartAutoConnect, Verbose,
			TEXT("🔧 PIPE CHILD: DECLINED (invalid routed shape) %s [%s]"),
			*ParentJunction->GetName(), bIsManifold ? TEXT("Manifold") : TEXT("Building"));
		RemovePipeChild(ParentJunction, PipeChild);
		return nullptr;
	}

	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Created %s for %s → %s (%s, Mk%d %s, Recipe=%s)"),
		*ChildName.ToString(),
		*ParentJunction->GetName(),
		bIsManifold ? TEXT("Junction") : TEXT("Building"),
		bIsManifold ? TEXT("Manifold") : TEXT("Building"),
		ActualTier,
		bWithIndicator ? TEXT("Normal") : TEXT("Clean"),
		PipeRecipe ? *PipeRecipe->GetName() : TEXT("NULL"));

	return PipeChild;
}

ASFPipelineHologram* FSFPipeAutoConnectManager::SpawnPipeChildAtPosition(
	AFGHologram* ParentHologram,
	const FVector& StartPos,
	const FVector& StartNormal,
	UFGPipeConnectionComponent* TargetConnector)
{
	if (!ParentHologram || !TargetConnector || !Subsystem)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: SpawnPipeChildAtPosition called with invalid parameters"));
		return nullptr;
	}
	
	UWorld* World = ParentHologram->GetWorld();
	if (!World)
	{
		return nullptr;
	}
	
	// Get configured pipe tier and style (building-side tier, not manifold)
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	int32 ConfigTier = RuntimeSettings.PipeTierToBuilding;
	bool bWithIndicator = RuntimeSettings.bPipeIndicator;
	
	AFGPlayerController* PlayerController = Cast<AFGPlayerController>(World->GetFirstPlayerController());
	
	int32 ActualTier = ConfigTier;
	if (ConfigTier == 0)
	{
		ActualTier = Subsystem->GetHighestUnlockedPipeTier(PlayerController);
		if (ActualTier == 0) ActualTier = 1;
	}
	
	UClass* PipeBuildClass = Subsystem->GetPipeClassFromConfig(ConfigTier, bWithIndicator, PlayerController);
	if (!PipeBuildClass)
	{
		return nullptr;
	}
	
	TSubclassOf<UFGRecipe> PipeRecipe = Subsystem->GetPipeRecipeForTier(ActualTier, bWithIndicator);
	
	FVector EndPos = TargetConnector->GetComponentLocation();
	FVector EndNormal = TargetConnector->GetConnectorNormal();
	
	FName ChildName(*FString::Printf(TEXT("FloorHolePipe_%d"), PipeChildCounter++));
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = ChildName;
	SpawnParams.Owner = ParentHologram->GetOwner();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.bDeferConstruction = true;
	
	ASFPipelineHologram* PipeChild = World->SpawnActor<ASFPipelineHologram>(
		ASFPipelineHologram::StaticClass(),
		StartPos,
		FRotator::ZeroRotator,
		SpawnParams
	);
	
	if (!PipeChild)
	{
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: SpawnActor returned null for floor hole pipe"));
		return nullptr;
	}
	
	PipeChild->SetReplicates(false);
	PipeChild->SetReplicateMovement(false);
	PipeChild->SetBuildClass(PipeBuildClass);
	PipeChild->Tags.AddUnique(FName(TEXT("SF_PipeAutoConnectChild")));
	
	if (PipeRecipe)
	{
		PipeChild->SetRecipe(PipeRecipe);
	}
	
	USFHologramDataService::DisableValidation(PipeChild);
	USFHologramDataService::MarkAsChild(PipeChild, ParentHologram, ESFChildHologramType::AutoConnect);
	
	FSFHologramData* HoloData = USFHologramDataService::GetOrCreateData(PipeChild);
	if (HoloData)
	{
		HoloData->bIsPipeAutoConnectChild = true;
		HoloData->PipeAutoConnectConn0 = nullptr; // Floor hole has no connector on hologram
		HoloData->PipeAutoConnectConn1 = TargetConnector;
		HoloData->bIsPipeManifold = false;
	}
	
	PipeChild->FinishSpawning(FTransform(StartPos));
	
	// Set snapped connections — only target side (index 1) has a real connector
	FProperty* SnappedProp = AFGPipelineHologram::StaticClass()->FindPropertyByName(TEXT("mSnappedConnectionComponents"));
	if (SnappedProp)
	{
		void* PropAddr = SnappedProp->ContainerPtrToValuePtr<void>(PipeChild);
		UFGPipeConnectionComponentBase** SnappedArray = static_cast<UFGPipeConnectionComponentBase**>(PropAddr);
		if (SnappedArray)
		{
			SnappedArray[0] = nullptr;
			SnappedArray[1] = TargetConnector;
		}
	}
	
	PipeChild->SetRoutingMode(RuntimeSettings.PipeRoutingMode);

	// [#437] Floor-hole pipes route from the hole face with the exit vector (straight up from the
	// top / straight down from the bottom) seeding the router's start tangent - the correct
	// vanilla routers build their own straight riser out of the face (round 2: no forced stub,
	// per live comparison against a hand-built route). The helper also validates the routed shape
	// against vanilla's minimum bend radius and flags the child invalid (vanilla's own "Invalid
	// Pipe Shape" disqualifier via CheckValidPlacement) instead of force-rendering a shape vanilla
	// would reject.
	if (!PipeChild->RouteWithStraightExit(0.0f, StartPos, StartNormal, EndPos, EndNormal))
	{
		const float Distance = FVector::Dist(StartPos, EndPos);
		const float SmallTangent = 50.0f;
		const float LargeTangent = Distance * 0.435f;
		const float FlatSectionLength = Distance * 0.047f;
		const float TransitionOffset = Distance * 0.070f;
		FVector Direction = (EndPos - StartPos).GetSafeNormal();
		
		TArray<FSplinePointData> SplinePoints;
		
		FSplinePointData Point0;
		Point0.Location = StartPos;
		Point0.ArriveTangent = StartNormal * SmallTangent;
		Point0.LeaveTangent = StartNormal * SmallTangent;
		SplinePoints.Add(Point0);
		
		FSplinePointData Point1;
		Point1.Location = StartPos + StartNormal * FlatSectionLength;
		Point1.ArriveTangent = StartNormal * SmallTangent;
		Point1.LeaveTangent = StartNormal * (SmallTangent * 0.99f);
		SplinePoints.Add(Point1);
		
		FSplinePointData Point2;
		Point2.Location = StartPos + StartNormal * TransitionOffset;
		Point2.ArriveTangent = StartNormal * SmallTangent;
		Point2.LeaveTangent = Direction * LargeTangent;
		SplinePoints.Add(Point2);
		
		FSplinePointData Point3;
		Point3.Location = EndPos + EndNormal * TransitionOffset;
		Point3.ArriveTangent = Direction * LargeTangent;
		Point3.LeaveTangent = -EndNormal * SmallTangent;
		SplinePoints.Add(Point3);
		
		FSplinePointData Point4;
		Point4.Location = EndPos + EndNormal * FlatSectionLength;
		Point4.ArriveTangent = -EndNormal * (SmallTangent * 0.99f);
		Point4.LeaveTangent = -EndNormal * SmallTangent;
		SplinePoints.Add(Point4);
		
		FSplinePointData Point5;
		Point5.Location = EndPos;
		Point5.ArriveTangent = -EndNormal * SmallTangent;
		Point5.LeaveTangent = -EndNormal * SmallTangent;
		SplinePoints.Add(Point5);
		
		PipeChild->SetSplineDataAndUpdate(SplinePoints);
	}
	
	PipeChild->SetActorHiddenInGame(false);
	PipeChild->SetActorEnableCollision(false);
	PipeChild->SetActorTickEnabled(false);
	PipeChild->RegisterAllComponents();
	PipeChild->SetPlacementMaterialState(ParentHologram->GetHologramMaterialState());
	
	bool bParentLocked = ParentHologram->IsHologramLocked();
	if (bParentLocked)
	{
		PipeChild->LockHologramPosition(true);
	}
	
	ParentHologram->AddChild(PipeChild, ChildName);
	PipeChild->TriggerMeshGeneration();
	PipeChild->ForceApplyHologramMaterial();
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Created floor hole pipe %s for %s (Mk%d %s)"),
		*ChildName.ToString(),
		*ParentHologram->GetName(),
		ActualTier,
		bWithIndicator ? TEXT("Normal") : TEXT("Clean"));
	
	return PipeChild;
}

void FSFPipeAutoConnectManager::RemovePipeChild(AFGHologram* ParentJunction, ASFPipelineHologram* PipeChild)
{
	if (!ParentJunction || !PipeChild)
	{
		return;
	}
	
	// Remove from parent's mChildren array
	FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
	if (ChildrenProp)
	{
		TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(ParentJunction);
		if (ChildrenArray)
		{
			ChildrenArray->Remove(PipeChild);
		}
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 PIPE CHILD: Removed %s from %s"),
		*PipeChild->GetName(), *ParentJunction->GetName());
	
	PipeChild->Destroy();
}

// ========================================
// Issue #187: Floor Hole Pipe Auto-Connect
// ========================================

void FSFPipeAutoConnectManager::ProcessFloorHolePipes(AFGHologram* ParentHologram)
{
	if (!ParentHologram || !Subsystem)
	{
		return;
	}
	
	// Collect all floor hole holograms (parent + children)
	// [#453] Only treat the parent as a floor hole if it actually IS one. This runs off
	// ForceRefresh (the Smart Panel's TriggerAutoConnectRefresh), which fires floor-hole
	// processing for ANY held hologram - so on a pipeline-junction grid the junction parent was
	// being processed as a floor hole (default thickness, endpoint below itself) and spawned a
	// spurious FloorHolePipe dropping out its bottom. Children were already filtered by
	// IsPassthroughPipeHologram; the parent was not. The HUD path never hit this because its
	// junction re-eval calls only OnPipeGridChanged, not the floor-hole path.
	TArray<AFGHologram*> AllFloorHoles;
	if (USFAutoConnectService::IsPassthroughPipeHologram(ParentHologram))
	{
		AllFloorHoles.Add(ParentHologram);
	}

	FSFHologramHelperService* HologramHelper = Subsystem->GetHologramHelper();
	if (HologramHelper)
	{
		TArray<TWeakObjectPtr<AFGHologram>> SpawnedChildren = HologramHelper->GetSpawnedChildren();
		for (const TWeakObjectPtr<AFGHologram>& ChildPtr : SpawnedChildren)
		{
			if (ChildPtr.IsValid() && USFAutoConnectService::IsPassthroughPipeHologram(ChildPtr.Get()))
			{
				AllFloorHoles.Add(ChildPtr.Get());
			}
		}
	}
	
	UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 FLOOR HOLE PIPE: Processing %d floor holes (parent + %d children)"),
		AllFloorHoles.Num(), AllFloorHoles.Num() - 1);
	
	// Get runtime settings for pipe tier and style
	const auto& RuntimeSettings = Subsystem->GetAutoConnectRuntimeSettings();
	
	// Track which floor holes we process this frame (for orphan cleanup)
	TSet<AFGHologram*> ProcessedHoles;
	
	// Connector reservation: each building connector can only be claimed by one floor hole
	TSet<UFGPipeConnectionComponent*> LocalReservedConnectors;
	
	for (AFGHologram* FloorHole : AllFloorHoles)
	{
		if (!FloorHole) continue;
		ProcessedHoles.Add(FloorHole);
		
		// Read foundation thickness from THIS floor hole (protected member, access via reflection)
		// Each floor hole can snap to different foundation heights (1m, 2m, 4m, etc.)
		float FoundationThickness = 400.0f; // Default 4m
		FFloatProperty* ThickProp = CastField<FFloatProperty>(
			FloorHole->GetClass()->FindPropertyByName(FName(TEXT("mSnappedBuildingThickness"))));
		if (ThickProp)
		{
			FoundationThickness = ThickProp->GetPropertyValue_InContainer(FloorHole);
		}
		
		// Floor hole holograms do NOT have pipe connection components.
		// Calculate top and bottom connection endpoints from hologram position + foundation thickness.
		// Hologram position = CENTER of the passthrough (middle of foundation thickness).
		FVector HoleLocation = FloorHole->GetActorLocation();
		float HalfThickness = FoundationThickness * 0.5f;
		FVector TopPos = HoleLocation + FVector(0.0f, 0.0f, HalfThickness);   // Top of foundation (where buildings sit)
		FVector BottomPos = HoleLocation - FVector(0.0f, 0.0f, HalfThickness); // Bottom of foundation
		
		// Find nearby buildings with unconnected pipe connectors
		TArray<AFGBuildable*> NearbyBuildings;
		FSFPipeConnectorFinder::FindNearbyPipeBuildings(FloorHole, 2500.0f, NearbyBuildings);
		
		// Search for best building connector (not already reserved by another floor hole)
		UFGPipeConnectionComponent* BestBuildingConn = nullptr;
		float BestDistance = TNumericLimits<float>::Max();
		bool bUseTopEndpoint = true;
		
		for (AFGBuildable* Building : NearbyBuildings)
		{
			if (!Building) continue;
			
			// Skip pipes, supports, storage, junctions, and other passthroughs
			if (Building->IsA(AFGBuildablePipeline::StaticClass())) continue;
			FString BClassName = Building->GetClass()->GetFName().ToString();
			if (BClassName.Contains(TEXT("PipelineSupport")) || BClassName.Contains(TEXT("PipeSupport"))) continue;
			if (Building->IsA(AFGBuildableStorage::StaticClass())) continue;
			if (Building->IsA(AFGBuildablePassthrough::StaticClass())) continue;
			if (BClassName.Contains(TEXT("PipelineJunction"))) continue;
			
			// Must be a factory building
			if (!Building->IsA(AFGBuildableFactory::StaticClass())) continue;
			
			TArray<UFGPipeConnectionComponent*> PipeConns;
			FSFPipeConnectorFinder::GetPipeConnectors(Building, PipeConns);
			
			for (UFGPipeConnectionComponent* BuildingConn : PipeConns)
			{
				if (!BuildingConn) continue;
				
				// Skip connectors already claimed by another floor hole
				if (LocalReservedConnectors.Contains(BuildingConn)) continue;
				
				FVector BuildingConnPos = BuildingConn->GetComponentLocation();

				// [#437] Face selection is by HEIGHT, not 3D distance: a connector above the
				// hole's top routes from the TOP face (exit straight up), one below the bottom
				// routes from the BOTTOM face (exit straight down) - matching how a passthrough
				// is used. A connector within the foundation's height band takes the vertically
				// nearer face. 3D distance still ranks candidate connectors against each other.
				const bool bTop = BuildingConnPos.Z >= (TopPos.Z + BottomPos.Z) * 0.5f;
				const FVector FacePos = bTop ? TopPos : BottomPos;
				const float Distance = FVector::Dist(FacePos, BuildingConnPos);

				if (Distance < BestDistance)
				{
					BestDistance = Distance;
					BestBuildingConn = BuildingConn;
					bUseTopEndpoint = bTop;
				}
			}
		}
		
		// Validate distance constraints
		constexpr float MaxConnectionDistance = 2500.0f;
		constexpr float MinConnectionDistance = 50.0f;
		
		if (!BestBuildingConn || BestDistance > MaxConnectionDistance || BestDistance < MinConnectionDistance)
		{
			// No valid connection found — clean up any existing child
			TWeakObjectPtr<ASFPipelineHologram>* Existing = FloorHolePipeChildren.Find(FloorHole);
			if (Existing && Existing->IsValid())
			{
				RemovePipeChild(FloorHole, Existing->Get());
				FloorHolePipeChildren.Remove(FloorHole);
			}
			continue;
		}
		
		// Reserve this connector so no other floor hole claims it
		LocalReservedConnectors.Add(BestBuildingConn);
		
		// Select the correct endpoint and normal
		FVector StartPos = bUseTopEndpoint ? TopPos : BottomPos;
		FVector StartNormal = bUseTopEndpoint ? FVector::UpVector : FVector::DownVector;
		
		UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 FLOOR HOLE PIPE: %s → %s (%.1fm, %s endpoint)"),
			*FloorHole->GetName(), *BestBuildingConn->GetName(), BestDistance / 100.0f,
			bUseTopEndpoint ? TEXT("TOP") : TEXT("BOTTOM"));
		
		// Clean up existing pipe child if any (recreate — pipe children don't support endpoint updates)
		TWeakObjectPtr<ASFPipelineHologram>* ExistingChild = FloorHolePipeChildren.Find(FloorHole);
		if (ExistingChild && ExistingChild->IsValid())
		{
			RemovePipeChild(FloorHole, ExistingChild->Get());
			FloorHolePipeChildren.Remove(FloorHole);
		}
		
		// Spawn pipe child from floor hole endpoint to building connector
		ASFPipelineHologram* NewChild = SpawnPipeChildAtPosition(FloorHole, StartPos, StartNormal, BestBuildingConn);
		if (NewChild)
		{
			FloorHolePipeChildren.Add(FloorHole, NewChild);
			UE_LOG(LogSmartAutoConnect, Verbose, TEXT("🔧 FLOOR HOLE PIPE: Created pipe child %s for floor hole %s"),
				*NewChild->GetName(), *FloorHole->GetName());
		}
	}
	
	// Clean up orphaned floor hole pipe children (floor holes that were removed from grid)
	TArray<AFGHologram*> OrphanedHoles;
	for (const auto& Pair : FloorHolePipeChildren)
	{
		if (!ProcessedHoles.Contains(Pair.Key))
		{
			OrphanedHoles.Add(Pair.Key);
		}
	}
	for (AFGHologram* Orphan : OrphanedHoles)
	{
		TWeakObjectPtr<ASFPipelineHologram>* Child = FloorHolePipeChildren.Find(Orphan);
		if (Child && Child->IsValid())
		{
			RemovePipeChild(Orphan, Child->Get());
		}
		FloorHolePipeChildren.Remove(Orphan);
	}
}

void FSFPipeAutoConnectManager::ClearFloorHolePipePreviews()
{
	for (auto& Pair : FloorHolePipeChildren)
	{
		if (Pair.Value.IsValid())
		{
			RemovePipeChild(Pair.Key, Pair.Value.Get());
		}
	}
	FloorHolePipeChildren.Empty();
}

