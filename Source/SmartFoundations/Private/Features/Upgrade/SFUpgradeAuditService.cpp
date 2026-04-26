// Copyright Coffee Stain Studios. All Rights Reserved.

#include "Features/Upgrade/SFUpgradeAuditService.h"
#include "Features/Upgrade/SFUpgradeTraversalService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"

#include "FGPlayerController.h"
#include "Buildables/FGBuildableConveyorBelt.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "Buildables/FGBuildablePipeline.h"
#include "Buildables/FGBuildablePipelinePump.h"
#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "SFRCO.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

// ========================================
// FSFUpgradeFamilyResult Implementation
// ========================================

FString FSFUpgradeFamilyResult::GetFamilyDisplayName() const
{
	return USFUpgradeAuditService::GetFamilyDisplayName(Family);
}

// ========================================
// FSFUpgradeAuditResult Implementation
// ========================================

const FSFUpgradeFamilyResult* FSFUpgradeAuditResult::GetFamilyResult(ESFUpgradeFamily Family) const
{
	for (const FSFUpgradeFamilyResult& Result : FamilyResults)
	{
		if (Result.Family == Family)
		{
			return &Result;
		}
	}
	return nullptr;
}

// ========================================
// USFUpgradeAuditService Implementation
// ========================================

void USFUpgradeAuditService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
	UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: Initialized"));
}

void USFUpgradeAuditService::Cleanup()
{
	CancelAudit();
	ClearResults();
	Subsystem = nullptr;
	UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: Cleaned up"));
}

void USFUpgradeAuditService::Tick(float DeltaTime)
{
	if (bAuditInProgress)
	{
		ProcessScanBatch();
	}
}

// ========================================
// Audit Control
// ========================================

bool USFUpgradeAuditService::StartAudit(const FSFUpgradeAuditParams& Params)
{
	if (bAuditInProgress)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("USFUpgradeAuditService: Audit already in progress, canceling previous"));
		CancelAudit();
	}

	CurrentParams = Params;
	
	// Initialize working result
	WorkingResult = FSFUpgradeAuditResult();
	WorkingResult.bInProgress = true;
	WorkingResult.ScanOrigin = Params.Origin;
	WorkingResult.ScanRadius = Params.Radius;
	WorkingResult.StartTime = FDateTime::Now();

	// Initialize family results for all families we're scanning
	for (uint8 i = 1; i < static_cast<uint8>(ESFUpgradeFamily::MAX); ++i)
	{
		ESFUpgradeFamily Family = static_cast<ESFUpgradeFamily>(i);
		if (ShouldIncludeFamily(Family))
		{
			FSFUpgradeFamilyResult FamilyResult;
			FamilyResult.Family = Family;
			WorkingResult.FamilyResults.Add(FamilyResult);
		}
	}

	// Gather buildables to scan
	GatherBuildablesToScan();

	if (PendingBuildables.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: No buildables to scan"));
		WorkingResult.bSuccess = true;
		WorkingResult.bInProgress = false;
		WorkingResult.CompletionTime = FDateTime::Now();
		LastResult = WorkingResult;
		OnAuditCompleted.Broadcast(LastResult);
		return true;
	}

	bAuditInProgress = true;
	CurrentScanIndex = 0;

	UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: Started audit - %d buildables to scan (Radius: %.0f)"),
		PendingBuildables.Num(), Params.Radius);

	return true;
}

bool USFUpgradeAuditService::StartQuickAudit(float Radius)
{
	FSFUpgradeAuditParams Params;
	
	// Get player location as origin
	if (Subsystem)
	{
		AFGPlayerController* PC = Subsystem->GetLastController();
		if (PC && PC->GetPawn())
		{
			Params.Origin = PC->GetPawn()->GetActorLocation();
		}
	}
	
	Params.Radius = Radius;
	Params.bStoreEntries = true;
	
	return StartAudit(Params);
}

void USFUpgradeAuditService::CancelAudit()
{
	if (!bAuditInProgress)
	{
		return;
	}

	bAuditInProgress = false;
	PendingBuildables.Empty();
	CurrentScanIndex = 0;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: Audit canceled"));
}

void USFUpgradeAuditService::ClearResults()
{
	LastResult = FSFUpgradeAuditResult();
}

// ========================================
// Family Detection
// ========================================

ESFUpgradeFamily USFUpgradeAuditService::GetUpgradeFamily(AFGBuildable* Buildable)
{
	if (!Buildable)
	{
		return ESFUpgradeFamily::None;
	}

	FString ClassName = Buildable->GetClass()->GetName();

	// Conveyor belts
	if (Cast<AFGBuildableConveyorBelt>(Buildable))
	{
		return ESFUpgradeFamily::Belt;
	}

	// Conveyor lifts
	if (Cast<AFGBuildableConveyorLift>(Buildable))
	{
		return ESFUpgradeFamily::Lift;
	}

	// Pipelines (but not pumps or junctions)
	if (AFGBuildablePipeline* Pipeline = Cast<AFGBuildablePipeline>(Buildable))
	{
		// Check if it's a pump - EXCLUDED from upgrade consideration
		// Pumps are sized for head lift requirements; upgrading them has no benefit
		if (ClassName.Contains(TEXT("Pump")))
		{
			return ESFUpgradeFamily::None;
		}
		return ESFUpgradeFamily::Pipe;
	}

	// Pipeline pumps - EXCLUDED from upgrade consideration
	// Pumps are sized for head lift requirements; upgrading them has no benefit
	// as overkill head lift is wasted
	if (Cast<AFGBuildablePipelinePump>(Buildable) || ClassName.Contains(TEXT("PipelinePump")))
	{
		return ESFUpgradeFamily::None;
	}

	// Power poles
	if (AFGBuildablePowerPole* PowerPole = Cast<AFGBuildablePowerPole>(Buildable))
	{
		// Check for towers (audit-only)
		if (ClassName.Contains(TEXT("Tower")))
		{
			return ESFUpgradeFamily::Tower;
		}
		
		// Check for wall outlets - separate single vs double
		if (ClassName.Contains(TEXT("WallDouble")))
		{
			return ESFUpgradeFamily::WallOutletDouble;
		}
		if (ClassName.Contains(TEXT("Wall")) || ClassName.Contains(TEXT("Outlet")))
		{
			return ESFUpgradeFamily::WallOutletSingle;
		}
		
		return ESFUpgradeFamily::PowerPole;
	}

	// Additional class name checks for edge cases
	if (ClassName.Contains(TEXT("PowerTower")))
	{
		return ESFUpgradeFamily::Tower;
	}
	
	// Wall outlets - check double first (more specific)
	if (ClassName.Contains(TEXT("PowerPoleWallDouble")))
	{
		return ESFUpgradeFamily::WallOutletDouble;
	}
	if (ClassName.Contains(TEXT("PowerPoleWall")) || ClassName.Contains(TEXT("WallOutlet")))
	{
		return ESFUpgradeFamily::WallOutletSingle;
	}

	return ESFUpgradeFamily::None;
}

int32 USFUpgradeAuditService::GetBuildableTier(AFGBuildable* Buildable)
{
	if (!Buildable)
	{
		return 0;
	}

	FString ClassName = Buildable->GetClass()->GetName();

	// Pipeline tier detection (Issue #295/#296): pipe class names do NOT use "Mk1"/"Mk2" tokens.
	// Mk.1 = Build_Pipeline_C / Build_Pipeline_NoIndicator_C (no tier token)
	// Mk.2 = Build_PipelineMK2_C / Build_PipelineMK2_NoIndicator_C (contains "MK2", all caps)
	// Previously Mk1 NoIndicator fell through to the "return 1" fallback by coincidence; make it explicit.
	if (Cast<AFGBuildablePipeline>(Buildable))
	{
		if (ClassName.Contains(TEXT("MK2"), ESearchCase::CaseSensitive))
		{
			return 2;
		}
		return 1;
	}

	// Extract tier from class name patterns like "Mk1", "Mk2", etc.
	// Common patterns: ConveyorBeltMk1, ConveyorBeltMk2, PipelineMk1, PowerPoleMk1, etc.
	
	for (int32 Tier = 6; Tier >= 1; --Tier)
	{
		FString TierPattern = FString::Printf(TEXT("Mk%d"), Tier);
		if (ClassName.Contains(TierPattern))
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBuildableTier: %s matched pattern %s -> Tier %d"), *ClassName, *TierPattern, Tier);
			return Tier;
		}
	}
	
	UE_LOG(LogSmartFoundations, Warning, TEXT("GetBuildableTier: %s did not match any Mk pattern, falling back"), *ClassName);

	// Check for numeric suffixes (_01, _02, etc.)
	for (int32 Tier = 6; Tier >= 1; --Tier)
	{
		FString TierPattern = FString::Printf(TEXT("_%02d"), Tier);
		if (ClassName.Contains(TierPattern))
		{
			return Tier;
		}
	}

	// Default to tier 1 if we can't determine (base tier has no Mk suffix)
	// e.g. Build_PowerPoleWall_C, Build_PowerPoleWallDouble_C, Build_PowerPoleMk1_C
	return 1;
}

int32 USFUpgradeAuditService::GetMaxAvailableTier(ESFUpgradeFamily Family) const
{
	// Check actual unlocks via subsystem's unlock detection
	if (Subsystem)
	{
		AFGPlayerController* PC = Subsystem->GetLastController();
		
		switch (Family)
		{
			case ESFUpgradeFamily::Belt:
			case ESFUpgradeFamily::Lift:
				// Belts and lifts share the same tier unlocks
				return Subsystem->GetHighestUnlockedBeltTier(PC);
			case ESFUpgradeFamily::Pipe:
			case ESFUpgradeFamily::Pump:
				// Pipes and pumps share the same tier unlocks
				return Subsystem->GetHighestUnlockedPipeTier(PC);
			case ESFUpgradeFamily::PowerPole:
				return Subsystem->GetHighestUnlockedPowerPoleTier(PC);
			case ESFUpgradeFamily::WallOutletSingle:
				return Subsystem->GetHighestUnlockedWallOutletTier(PC, /*bDouble*/ false);
			case ESFUpgradeFamily::WallOutletDouble:
				return Subsystem->GetHighestUnlockedWallOutletTier(PC, /*bDouble*/ true);
			default:
				break;
		}
	}
	
	// Fallback for families without unlock tracking or if subsystem unavailable
	switch (Family)
	{
		case ESFUpgradeFamily::Belt:
		case ESFUpgradeFamily::Lift:
			return 1;  // Fallback to Mk1
		case ESFUpgradeFamily::Pipe:
		case ESFUpgradeFamily::Pump:
			return 1;  // Fallback to Mk1
		case ESFUpgradeFamily::PowerPole:
			return 3;  // Mk1-Mk3 (no unlock tracking yet)
		case ESFUpgradeFamily::WallOutletSingle:
		case ESFUpgradeFamily::WallOutletDouble:
			return 3;  // Mk1-Mk3 (some via MAM/FICSIT shop)
		case ESFUpgradeFamily::Tower:
			return 1;  // No upgrade tiers
		default:
			return 0;
	}
}

FString USFUpgradeAuditService::GetFamilyDisplayName(ESFUpgradeFamily Family)
{
	switch (Family)
	{
		case ESFUpgradeFamily::Belt:
			return LOCTEXT("Family_Belt", "Conveyor Belts").ToString();
		case ESFUpgradeFamily::Lift:
			return LOCTEXT("Family_Lift", "Conveyor Lifts").ToString();
		case ESFUpgradeFamily::Pipe:
			return LOCTEXT("Family_Pipe", "Pipelines").ToString();
		case ESFUpgradeFamily::Pump:
			return LOCTEXT("Family_Pump", "Pipeline Pumps").ToString();
		case ESFUpgradeFamily::PowerPole:
			return LOCTEXT("Family_PowerPole", "Power Poles").ToString();
		case ESFUpgradeFamily::WallOutletSingle:
			return LOCTEXT("Family_WallOutletSingle", "Wall Outlets (Single)").ToString();
		case ESFUpgradeFamily::WallOutletDouble:
			return LOCTEXT("Family_WallOutletDouble", "Wall Outlets (Double)").ToString();
		case ESFUpgradeFamily::Tower:
			return LOCTEXT("Family_Tower", "Power Towers").ToString();
		default:
			return LOCTEXT("Family_Unknown", "Unknown").ToString();
	}
}

// ========================================
// Internal Scanning
// ========================================

void USFUpgradeAuditService::GatherBuildablesToScan()
{
	PendingBuildables.Empty();

	UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("USFUpgradeAuditService: No valid world for scanning"));
		return;
	}

	const float RadiusSquared = CurrentParams.Radius > 0.0f ? FMath::Square(CurrentParams.Radius) : 0.0f;
	const bool bRadiusLimited = RadiusSquared > 0.0f;

	// Iterate all buildables in the world
	for (TActorIterator<AFGBuildable> It(World); It; ++It)
	{
		AFGBuildable* Buildable = *It;
		if (!Buildable || Buildable->IsPendingKillPending())
		{
			continue;
		}

		// Check if this is an upgrade-capable family
		ESFUpgradeFamily Family = GetUpgradeFamily(Buildable);
		if (Family == ESFUpgradeFamily::None)
		{
			continue;
		}

		// Check if we should include this family
		if (!ShouldIncludeFamily(Family))
		{
			continue;
		}

		// Radius check if applicable
		if (bRadiusLimited)
		{
			float DistSquared = FVector::DistSquared(Buildable->GetActorLocation(), CurrentParams.Origin);
			if (DistSquared > RadiusSquared)
			{
				continue;
			}
		}

		PendingBuildables.Add(Buildable);
	}

	UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: Gathered %d buildables to scan"), PendingBuildables.Num());
}

void USFUpgradeAuditService::ProcessScanBatch()
{
	if (!bAuditInProgress)
	{
		return;
	}

	const int32 TotalCount = PendingBuildables.Num();
	const int32 EndIndex = FMath::Min(CurrentScanIndex + BatchSize, TotalCount);

	for (int32 i = CurrentScanIndex; i < EndIndex; ++i)
	{
		AFGBuildable* Buildable = PendingBuildables[i].Get();
		if (!Buildable)
		{
			continue;
		}

		// Build audit entry
		FSFUpgradeAuditEntry Entry;
		Entry.Buildable = Buildable;
		Entry.Family = GetUpgradeFamily(Buildable);
		Entry.CurrentTier = GetBuildableTier(Buildable);
		Entry.MaxAvailableTier = GetMaxAvailableTier(Entry.Family);
		Entry.Location = Buildable->GetActorLocation();
		Entry.DistanceFromOrigin = FVector::Dist(Entry.Location, CurrentParams.Origin);

		// Add to results
		AddEntryToResults(Entry);
		WorkingResult.TotalScanned++;

		if (Entry.IsUpgradeable())
		{
			WorkingResult.TotalUpgradeable++;
		}
	}

	CurrentScanIndex = EndIndex;

	// Update progress
	WorkingResult.ProgressPercent = TotalCount > 0 ? (static_cast<float>(CurrentScanIndex) / TotalCount) * 100.0f : 100.0f;
	OnAuditProgressUpdated.Broadcast(WorkingResult.ProgressPercent, WorkingResult.TotalScanned);

	// Check if complete
	if (CurrentScanIndex >= TotalCount)
	{
		FinalizeAudit();
	}
}

void USFUpgradeAuditService::InjectAuditResult(const FSFUpgradeAuditResult& Result)
{
	UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: Injected audit result from server - Scanned: %d"), Result.TotalScanned);
	
	// Stop any local scan if one is running
	if (bAuditInProgress)
	{
		CancelAudit();
	}

	LastResult = Result;
	OnAuditCompleted.Broadcast(LastResult);
}

void USFUpgradeAuditService::FinalizeAudit()
{
	bAuditInProgress = false;
	WorkingResult.bSuccess = true;
	WorkingResult.bInProgress = false;
	WorkingResult.CompletionTime = FDateTime::Now();

	// Sort tier buckets within each family
	for (FSFUpgradeFamilyResult& FamilyResult : WorkingResult.FamilyResults)
	{
		FamilyResult.TierBuckets.Sort([](const FSFUpgradeTierBucket& A, const FSFUpgradeTierBucket& B)
		{
			return A.Tier < B.Tier;
		});

		// Sort entries within each bucket by distance
		if (CurrentParams.bStoreEntries)
		{
			for (FSFUpgradeTierBucket& Bucket : FamilyResult.TierBuckets)
			{
				Bucket.Entries.Sort([](const FSFUpgradeAuditEntry& A, const FSFUpgradeAuditEntry& B)
				{
					return A.DistanceFromOrigin < B.DistanceFromOrigin;
				});
			}
		}
	}

	// Store final result
	LastResult = WorkingResult;
	PendingBuildables.Empty();

	// Calculate scan duration
	FTimespan Duration = WorkingResult.CompletionTime - WorkingResult.StartTime;

	UE_LOG(LogSmartFoundations, Log, 
		TEXT("USFUpgradeAuditService: Audit complete - Scanned: %d, Upgradeable: %d, Duration: %.2fs"),
		LastResult.TotalScanned, LastResult.TotalUpgradeable, Duration.GetTotalSeconds());

	// Log family breakdown
	for (const FSFUpgradeFamilyResult& FamilyResult : LastResult.FamilyResults)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("  %s: %d total, %d upgradeable"),
			*FamilyResult.GetFamilyDisplayName(), FamilyResult.TotalCount, FamilyResult.UpgradeableCount);
	}

	// If we're on the server, send results back to the client via RCO
	UWorld* World = GetWorld();
	if (World && World->GetNetMode() < NM_Client)
	{
		if (AFGPlayerController* PC = CurrentParams.RequestingPlayer.Get())
		{
			// Find our RCO instance for this player
			// In SML, RCOs are spawned per player controller
			TArray<AActor*> RCOActors;
			UGameplayStatics::GetAllActorsOfClass(World, USFRCO::StaticClass(), RCOActors);
			
			for (AActor* Actor : RCOActors)
			{
				if (USFRCO* RCO = Cast<USFRCO>(Actor))
				{
					if (RCO->GetOuter() == PC)
					{
						RCO->Client_ReceiveAuditResult(LastResult);
						UE_LOG(LogSmartFoundations, Log, TEXT("USFUpgradeAuditService: Sent audit result to client via RCO"));
						break;
					}
				}
			}
		}
	}

	OnAuditCompleted.Broadcast(LastResult);
}

void USFUpgradeAuditService::AddEntryToResults(const FSFUpgradeAuditEntry& Entry)
{
	// Find or create family result
	FSFUpgradeFamilyResult* FamilyResult = nullptr;
	for (FSFUpgradeFamilyResult& Result : WorkingResult.FamilyResults)
	{
		if (Result.Family == Entry.Family)
		{
			FamilyResult = &Result;
			break;
		}
	}

	if (!FamilyResult)
	{
		// Shouldn't happen if we initialized correctly, but handle it
		FSFUpgradeFamilyResult NewResult;
		NewResult.Family = Entry.Family;
		WorkingResult.FamilyResults.Add(NewResult);
		FamilyResult = &WorkingResult.FamilyResults.Last();
	}

	// Update counts
	FamilyResult->TotalCount++;
	if (Entry.IsUpgradeable())
	{
		FamilyResult->UpgradeableCount++;
	}

	// Find or create tier bucket
	FSFUpgradeTierBucket* TierBucket = nullptr;
	for (FSFUpgradeTierBucket& Bucket : FamilyResult->TierBuckets)
	{
		if (Bucket.Tier == Entry.CurrentTier)
		{
			TierBucket = &Bucket;
			break;
		}
	}

	if (!TierBucket)
	{
		FSFUpgradeTierBucket NewBucket;
		NewBucket.Tier = Entry.CurrentTier;
		FamilyResult->TierBuckets.Add(NewBucket);
		TierBucket = &FamilyResult->TierBuckets.Last();
	}

	// Update bucket
	TierBucket->Count++;

	// Store entry if requested (with limit check)
	if (CurrentParams.bStoreEntries)
	{
		if (CurrentParams.MaxEntriesPerFamily == 0 || TierBucket->Entries.Num() < CurrentParams.MaxEntriesPerFamily)
		{
			TierBucket->Entries.Add(Entry);
		}
	}
}

bool USFUpgradeAuditService::ShouldIncludeFamily(ESFUpgradeFamily Family) const
{
	// If no specific families requested, include all (except None)
	if (CurrentParams.IncludeFamilies.Num() == 0)
	{
		return Family != ESFUpgradeFamily::None;
	}

	return CurrentParams.IncludeFamilies.Contains(Family);
}

#undef LOCTEXT_NAMESPACE
