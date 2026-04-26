// Copyright Epic Games, Inc. All Rights Reserved.

#include "Features/AutoConnect/SFAutoConnectOrchestrator.h"
#include "Features/AutoConnect/SFAutoConnectService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Data/SFBuildableSizeRegistry.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableFactory.h"
#include "Buildables/FGBuildableGenerator.h"
#include "Buildables/FGBuildableConveyorAttachment.h"

void USFAutoConnectOrchestrator::Initialize(AFGHologram* InParentHologram, USFAutoConnectService* InAutoConnectService)
{
	ParentHologram = InParentHologram;
	AutoConnectService = InAutoConnectService;
	ReservedInputs.Empty();
	
	// Reset context-aware spacing state for new placement
	bContextSpacingApplied = false;
	LastTargetBuildingClass.Reset();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Auto-Connect Orchestrator initialized for %s"), 
		*InParentHologram->GetName());
}

void USFAutoConnectOrchestrator::EvaluateGrid(bool bForceRecreate)
{
	if (!ParentHologram.IsValid() || !AutoConnectService)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🎯 Orchestrator: Cannot evaluate - invalid parent or service"));
		return;
	}
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (USFSubsystem* Subsystem = AutoConnectService->GetSubsystem())
	{
		if (Subsystem->IsSmartDisabledForCurrentAction())
		{
			ClearAllPreviews();
			UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: EvaluateGrid skipped - Smart disabled for current action"));
			return;
		}
	}

	// Prevent recursive evaluation (belt preview updates can trigger movement events)
	if (bIsEvaluatingBelts)
	{
		// Issue #269: Don't silently drop - flag for re-evaluation when cooldown clears
		bPendingBeltReevaluation = true;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Evaluation deferred (cooldown active, pending re-eval flagged)"));
		return;
	}

	// Set evaluation flag
	bIsEvaluatingBelts = true;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Evaluating grid (ForceRecreate=%s)"), 
		bForceRecreate ? TEXT("YES") : TEXT("NO"));

	// Clear all previews if forcing recreation
	if (bForceRecreate)
	{
		ClearAllPreviews();
	}

	// Clear reservation map for fresh evaluation
	ReservedInputs.Empty();

	// Evaluate connections for all distributors
	EvaluateConnections();

	// Clear evaluation flag after a delay to allow for async movement events
	if (ParentHologram.IsValid() && ParentHologram->GetWorld())
	{
		ParentHologram->GetWorld()->GetTimerManager().SetTimer(
			BeltCooldownTimer,
			this,
			&USFAutoConnectOrchestrator::ClearBeltEvaluationFlag,
			0.1f,  // 100ms cooldown
			false  // Don't loop
		);
	}
	else
	{
		// Fallback: Clear immediately if we can't set a timer
		bIsEvaluatingBelts = false;
	}
}

void USFAutoConnectOrchestrator::OnGridChanged()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Grid changed - scheduling re-evaluation"));
	
	// CRITICAL FIX: Clear evaluation flag for grid changes to prevent race condition
	// When children are spawned shortly after parent creation, the initial evaluation's
	// 100ms cooldown can block the child evaluation, leaving children without belt previews
	if (bIsEvaluatingBelts)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("   🔓 Clearing evaluation flag for grid change (was blocking)"));
		bIsEvaluatingBelts = false;
		
		// Also clear the cooldown timer since we're forcing a new evaluation anyway
		if (ParentHologram.IsValid() && ParentHologram->GetWorld())
		{
			ParentHologram->GetWorld()->GetTimerManager().ClearTimer(BeltCooldownTimer);
		}
	}
	
	// Grid changed (children added/removed) - force full recreation, debounced
	ScheduleEvaluation(true);
}

void USFAutoConnectOrchestrator::OnDistributorsMoved()
{
	// NOTE: We intentionally do NOT check bIsEvaluatingBelts here.
	// The cooldown period (100ms after evaluation) was blocking movement updates,
	// causing belt previews to not update during free movement of parent-only distributors.
	// ScheduleEvaluation already handles debouncing - if an evaluation is already scheduled,
	// it will just return early. The guard in EvaluateGrid() prevents actual recursive evaluation.

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Distributors moved - scheduling update"));
	
	// Distributors moved - update previews, debounced
	ScheduleEvaluation(false);
}

void USFAutoConnectOrchestrator::OnPipeJunctionsMoved()
{
	// NOTE: We intentionally do NOT check bIsEvaluatingPipes here (same fix as OnDistributorsMoved).
	// The cooldown period was blocking movement updates during free movement.
	// SchedulePipeEvaluation handles debouncing, and RunScheduledPipeEvaluation guards against recursion.

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Pipe junctions moved - scheduling update"));
	
	// REFACTOR: Use debounced scheduling like belts (prevents multiple updates per frame)
	SchedulePipeEvaluation(false);
}

void USFAutoConnectOrchestrator::OnPipeGridChanged()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Pipe grid changed - scheduling re-evaluation"));
	
	// CRITICAL FIX: Clear evaluation flag for grid changes (same as belt fix above)
	if (bIsEvaluatingPipes)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("   🔓 Clearing evaluation flag for pipe grid change (was blocking)"));
		bIsEvaluatingPipes = false;
		
		// Also clear the cooldown timer since we're forcing a new evaluation anyway
		if (ParentHologram.IsValid() && ParentHologram->GetWorld())
		{
			ParentHologram->GetWorld()->GetTimerManager().ClearTimer(PipeCooldownTimer);
		}
	}
	
	// ISSUE #235: Don't force recreate for grid changes - child holograms persist and update in place
	// Force recreate was needed for old FPipePreviewHelper system but causes flashing with child holograms
	SchedulePipeEvaluation(false);
}

void USFAutoConnectOrchestrator::OnPowerPolesMoved()
{
	// NOTE: We intentionally do NOT check bIsEvaluatingPower here (same fix as OnDistributorsMoved).
	// The cooldown period was blocking movement updates during free movement.
	// SchedulePowerEvaluation handles debouncing, and RunScheduledPowerEvaluation guards against recursion.

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Power poles moved - scheduling update"));
	
	// Use debounced scheduling
	SchedulePowerEvaluation(false);
}

void USFAutoConnectOrchestrator::OnPowerGridChanged()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Power grid changed - scheduling re-evaluation"));
	
	// CRITICAL FIX: Clear evaluation flag for grid changes (same as belt/pipe fix)
	if (bIsEvaluatingPower)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("   🔓 Clearing evaluation flag for power grid change (was blocking)"));
		bIsEvaluatingPower = false;
		
		// Also clear the cooldown timer since we're forcing a new evaluation anyway
		if (ParentHologram.IsValid() && ParentHologram->GetWorld())
		{
			ParentHologram->GetWorld()->GetTimerManager().ClearTimer(PowerCooldownTimer);
		}
	}
	
	// Use debounced scheduling with force recreate for grid changes
	SchedulePowerEvaluation(true);
}

void USFAutoConnectOrchestrator::ForceRefresh()
{
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Force refresh triggered from settings change"));
	
	// Trigger re-evaluation of all connection types with force recreate
	// This ensures previews are updated with new settings (tier, routing mode, etc.)
	OnGridChanged();
	OnPipeGridChanged();
	OnPowerGridChanged();
	OnStackableConveyorPolesChanged();
	OnStackablePipelineSupportsChanged();
	OnFloorHolePipesChanged();
}

void USFAutoConnectOrchestrator::Cleanup()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Cleaning up all previews"));
	
	ClearAllPreviews();
	ReservedInputs.Empty();
	
	// Clear timers if active
	if (ParentHologram.IsValid() && ParentHologram->GetWorld())
	{
		UWorld* World = ParentHologram->GetWorld();
		World->GetTimerManager().ClearTimer(BeltCooldownTimer);
		World->GetTimerManager().ClearTimer(PipeCooldownTimer);
		World->GetTimerManager().ClearTimer(PowerCooldownTimer);
		World->GetTimerManager().ClearTimer(EvalTimerHandle);
		World->GetTimerManager().ClearTimer(PipeEvalTimerHandle);
		World->GetTimerManager().ClearTimer(PowerEvalTimerHandle);
		World->GetTimerManager().ClearTimer(StackablePipeEvalTimerHandle);
		World->GetTimerManager().ClearTimer(StackableBeltEvalTimerHandle);
		World->GetTimerManager().ClearTimer(FloorHolePipeEvalTimerHandle);
	}
	
	bIsEvaluatingBelts = false;
	bIsEvaluatingPipes = false;
	bIsEvaluatingPower = false;
	bEvalScheduled = false;
	bPipeEvalScheduled = false;
	bPowerEvalScheduled = false;
	bStackablePipeEvalScheduled = false;
	bStackableBeltEvalScheduled = false;
	bFloorHolePipeEvalScheduled = false;
	bForceRecreatePending = false;
	bPipeForceRecreatePending = false;
	bPowerForceRecreatePending = false;
}

void USFAutoConnectOrchestrator::ClearBeltEvaluationFlag()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Clearing belt evaluation flag after cooldown"));
	bIsEvaluatingBelts = false;
	
	// Issue #269: If evaluation was requested during cooldown, re-schedule now
	if (bPendingBeltReevaluation)
	{
		bPendingBeltReevaluation = false;
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Pending belt re-evaluation detected, scheduling now"));
		ScheduleEvaluation(false);
	}
}

void USFAutoConnectOrchestrator::ClearPipeEvaluationFlag()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Clearing pipe evaluation flag after cooldown"));
	bIsEvaluatingPipes = false;
}

void USFAutoConnectOrchestrator::ClearPowerEvaluationFlag()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Clearing power evaluation flag after cooldown"));
	bIsEvaluatingPower = false;
}

void USFAutoConnectOrchestrator::ScheduleEvaluation(bool bForceRecreate)
{
	bForceRecreatePending = bForceRecreatePending || bForceRecreate;
	if (bEvalScheduled)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Evaluation already scheduled (force=%d,pendingForce=%d)"), bForceRecreate ? 1 : 0, bForceRecreatePending ? 1 : 0);
		return;
	}

	if (!ParentHologram.IsValid() || !ParentHologram->GetWorld())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🎯 Orchestrator: Cannot schedule evaluation - no world"));
		// Fallback: run immediately
		RunScheduledEvaluation();
		return;
	}

	bEvalScheduled = true;
	UWorld* World = ParentHologram->GetWorld();
	FTimerDelegate D;
	D.BindUObject(this, &USFAutoConnectOrchestrator::RunScheduledEvaluation);
	// Use a short delay to ensure child BeginPlay/attachment complete before evaluation
	World->GetTimerManager().SetTimer(EvalTimerHandle, D, 0.02f, /*bLoop=*/false);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Evaluation scheduled in 0.02s (force pending=%d)"), bForceRecreatePending ? 1 : 0);
}

void USFAutoConnectOrchestrator::RunScheduledEvaluation()
{
	bEvalScheduled = false;
	const bool bForce = bForceRecreatePending;
	bForceRecreatePending = false;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Running scheduled evaluation (ForceRecreate=%d)"), bForce ? 1 : 0);
	EvaluateGrid(bForce);
}

void USFAutoConnectOrchestrator::SchedulePipeEvaluation(bool bForceRecreate)
{
	bPipeForceRecreatePending = bPipeForceRecreatePending || bForceRecreate;
	if (bPipeEvalScheduled)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Pipe evaluation already scheduled (force=%d,pendingForce=%d)"), 
			bForceRecreate ? 1 : 0, bPipeForceRecreatePending ? 1 : 0);
		return;
	}

	if (!ParentHologram.IsValid() || !ParentHologram->GetWorld())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🎯 Orchestrator: Cannot schedule pipe evaluation - no world"));
		// Fallback: run immediately
		RunScheduledPipeEvaluation();
		return;
	}

	bPipeEvalScheduled = true;
	UWorld* World = ParentHologram->GetWorld();
	FTimerDelegate D;
	D.BindUObject(this, &USFAutoConnectOrchestrator::RunScheduledPipeEvaluation);
	// Use same 20ms delay as belts to ensure child BeginPlay/attachment complete
	World->GetTimerManager().SetTimer(PipeEvalTimerHandle, D, 0.02f, /*bLoop=*/false);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Pipe evaluation scheduled in 0.02s (force pending=%d)"), 
		bPipeForceRecreatePending ? 1 : 0);
}

void USFAutoConnectOrchestrator::RunScheduledPipeEvaluation()
{
	bPipeEvalScheduled = false;
	const bool bForce = bPipeForceRecreatePending;
	bPipeForceRecreatePending = false;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Running scheduled pipe evaluation (ForceRecreate=%d)"), bForce ? 1 : 0);
	
	// Issue #198: Skip if Smart is disabled for current action (double-tap)
	if (AutoConnectService)
	{
		if (USFSubsystem* Subsystem = AutoConnectService->GetSubsystem())
		{
			if (Subsystem->IsSmartDisabledForCurrentAction())
			{
				AutoConnectService->ClearPipePreviews(ParentHologram.Get());
				UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Pipe evaluation skipped - Smart disabled for current action"));
				return;
			}
		}
	}
	
	// Set evaluation flag to prevent recursion
	bIsEvaluatingPipes = true;
	
	if (AutoConnectService && ParentHologram.IsValid())
	{
		// Check if parent is a pipe junction hologram
		if (USFAutoConnectService::IsPipelineJunctionHologram(ParentHologram.Get()))
		{
			// Clear all pipe previews if forcing recreation (e.g., settings changed)
			if (bForce)
			{
				AutoConnectService->ClearPipePreviews(ParentHologram.Get());
				UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Force cleared all pipe previews for settings change"));
			}
			
			// Update pipe previews (will create new previews with current settings)
			AutoConnectService->UpdatePipePreviews(ParentHologram.Get());
		}
	}
	
	// Clear evaluation flag after a delay to allow for async movement events
	if (ParentHologram.IsValid() && ParentHologram->GetWorld())
	{
		ParentHologram->GetWorld()->GetTimerManager().SetTimer(
			PipeCooldownTimer,
			this,
			&USFAutoConnectOrchestrator::ClearPipeEvaluationFlag,
			0.1f,  // 100ms cooldown
			false  // Don't loop
		);
	}
	else
	{
		// Fallback: Clear immediately if we can't set a timer
		bIsEvaluatingPipes = false;
	}
}

void USFAutoConnectOrchestrator::SchedulePowerEvaluation(bool bForceRecreate)
{
	bPowerForceRecreatePending = bPowerForceRecreatePending || bForceRecreate;
	if (bPowerEvalScheduled)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Power evaluation already scheduled (force=%d,pendingForce=%d)"), 
			bForceRecreate ? 1 : 0, bPowerForceRecreatePending ? 1 : 0);
		return;
	}

	if (!ParentHologram.IsValid() || !ParentHologram->GetWorld())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🎯 Orchestrator: Cannot schedule power evaluation - no world"));
		// Fallback: run immediately
		RunScheduledPowerEvaluation();
		return;
	}

	bPowerEvalScheduled = true;
	UWorld* World = ParentHologram->GetWorld();
	FTimerDelegate D;
	D.BindUObject(this, &USFAutoConnectOrchestrator::RunScheduledPowerEvaluation);
	// Use same 20ms delay as belts/pipes
	World->GetTimerManager().SetTimer(PowerEvalTimerHandle, D, 0.02f, /*bLoop=*/false);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Power evaluation scheduled in 0.02s (force pending=%d)"), 
		bPowerForceRecreatePending ? 1 : 0);
}

void USFAutoConnectOrchestrator::RunScheduledPowerEvaluation()
{
	bPowerEvalScheduled = false;
	const bool bForce = bPowerForceRecreatePending;
	bPowerForceRecreatePending = false;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Running scheduled power evaluation (ForceRecreate=%d)"), bForce ? 1 : 0);
	
	// Set evaluation flag to prevent recursion
	bIsEvaluatingPower = true;
	
	if (AutoConnectService && ParentHologram.IsValid())
	{
		// Check if parent is a power pole hologram
		if (USFAutoConnectService::IsPowerPoleHologram(ParentHologram.Get()))
		{
			// Clear all power previews if forcing recreation
			if (bForce)
			{
				AutoConnectService->ClearAllPowerPreviews();
				
				// CRITICAL: Flush the destroy queue immediately so wire holograms are actually removed
				// before we potentially re-create them (or skip re-creation if power is disabled)
				if (USFSubsystem* Subsystem = AutoConnectService->GetSubsystem())
				{
					if (FSFHologramHelperService* Helper = Subsystem->GetHologramHelper())
					{
						Helper->ForceDestroyPendingChildren();
					}
				}
				
				UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Force cleared all power previews for settings change"));
			}
			
			// Update power previews
			AutoConnectService->ProcessPowerPoles(ParentHologram.Get());
		}
	}
	
	// Clear evaluation flag after a delay to allow for async movement events
	if (ParentHologram.IsValid() && ParentHologram->GetWorld())
	{
		ParentHologram->GetWorld()->GetTimerManager().SetTimer(
			PowerCooldownTimer,
			this,
			&USFAutoConnectOrchestrator::ClearPowerEvaluationFlag,
			0.1f,  // 100ms cooldown
			false  // Don't loop
		);
	}
	else
	{
		// Fallback: Clear immediately if we can't set a timer
		bIsEvaluatingPower = false;
	}
}

// ========================================
// Support Structure Auto-Connect (Issue #220)
// ========================================

void USFAutoConnectOrchestrator::OnStackableConveyorPolesChanged()
{
	// Use debounced scheduling to prevent excessive calls during rapid spacing changes
	ScheduleStackableBeltEvaluation();
}

void USFAutoConnectOrchestrator::ScheduleStackableBeltEvaluation()
{
	if (bStackableBeltEvalScheduled)
	{
		// Already scheduled - skip duplicate
		return;
	}

	if (!ParentHologram.IsValid() || !ParentHologram->GetWorld())
	{
		// Fallback: run immediately if no world
		RunScheduledStackableBeltEvaluation();
		return;
	}

	bStackableBeltEvalScheduled = true;
	UWorld* World = ParentHologram->GetWorld();
	FTimerDelegate D;
	D.BindUObject(this, &USFAutoConnectOrchestrator::RunScheduledStackableBeltEvaluation);
	// Use 50ms delay to coalesce rapid spacing changes
	World->GetTimerManager().SetTimer(StackableBeltEvalTimerHandle, D, 0.05f, /*bLoop=*/false);
}

void USFAutoConnectOrchestrator::RunScheduledStackableBeltEvaluation()
{
	bStackableBeltEvalScheduled = false;
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🚧 Orchestrator: Stackable conveyor poles changed - processing auto-connect"));
	
	if (!AutoConnectService || !ParentHologram.IsValid())
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("🚧 Orchestrator: Missing service or parent hologram"));
		return;
	}
	
	// Process stackable conveyor poles for belt auto-connect
	AutoConnectService->ProcessStackableConveyorPoles(ParentHologram.Get());
	
	// Cache belt preview data for building (Issue #220)
	// This must be called while hologram is valid, so OnActorSpawned can use cached data
	if (USFSubsystem* Subsystem = AutoConnectService->GetSubsystem())
	{
		Subsystem->CacheStackableBeltPreviewsForBuild();
	}
}

void USFAutoConnectOrchestrator::OnStackablePipelineSupportsChanged()
{
	// Use debounced scheduling to prevent excessive calls during rapid spacing changes
	ScheduleStackablePipeEvaluation();
}

void USFAutoConnectOrchestrator::ScheduleStackablePipeEvaluation()
{
	if (!ParentHologram.IsValid() || !ParentHologram->GetWorld())
	{
		// Fallback: run immediately if no world
		RunScheduledStackablePipeEvaluation();
		return;
	}

	UWorld* World = ParentHologram->GetWorld();
	
	// ALWAYS clear and reschedule the timer on each grid change
	// This ensures only the LAST grid change triggers pipe creation
	// (coalesces rapid scaling into a single evaluation)
	World->GetTimerManager().ClearTimer(StackablePipeEvalTimerHandle);
	
	bStackablePipeEvalScheduled = true;
	FTimerDelegate D;
	D.BindUObject(this, &USFAutoConnectOrchestrator::RunScheduledStackablePipeEvaluation);
	// Use 100ms delay to coalesce rapid spacing changes (increased from 50ms)
	World->GetTimerManager().SetTimer(StackablePipeEvalTimerHandle, D, 0.10f, /*bLoop=*/false);
}

void USFAutoConnectOrchestrator::RunScheduledStackablePipeEvaluation()
{
	bStackablePipeEvalScheduled = false;
	
	if (!AutoConnectService || !ParentHologram.IsValid())
	{
		return;
	}
	
	// Process stackable pipeline supports for pipe auto-connect
	AutoConnectService->ProcessStackablePipelineSupports(ParentHologram.Get());
}

// ========================================
// Issue #187: Floor Hole Pipe Auto-Connect
// ========================================

void USFAutoConnectOrchestrator::OnFloorHolePipesChanged()
{
	ScheduleFloorHolePipeEvaluation();
}

void USFAutoConnectOrchestrator::ScheduleFloorHolePipeEvaluation()
{
	if (!ParentHologram.IsValid() || !ParentHologram->GetWorld())
	{
		RunScheduledFloorHolePipeEvaluation();
		return;
	}

	UWorld* World = ParentHologram->GetWorld();
	World->GetTimerManager().ClearTimer(FloorHolePipeEvalTimerHandle);
	
	bFloorHolePipeEvalScheduled = true;
	FTimerDelegate D;
	D.BindUObject(this, &USFAutoConnectOrchestrator::RunScheduledFloorHolePipeEvaluation);
	World->GetTimerManager().SetTimer(FloorHolePipeEvalTimerHandle, D, 0.10f, /*bLoop=*/false);
}

void USFAutoConnectOrchestrator::RunScheduledFloorHolePipeEvaluation()
{
	bFloorHolePipeEvalScheduled = false;
	
	if (!AutoConnectService || !ParentHologram.IsValid())
	{
		return;
	}
	
	AutoConnectService->ProcessFloorHolePipes(ParentHologram.Get());
}

void USFAutoConnectOrchestrator::CollectDistributors(TArray<AFGHologram*>& OutDistributors)
{
	OutDistributors.Empty();

	if (!ParentHologram.IsValid())
	{
		return;
	}

	// Recursive helper to collect all distributor descendants
	TFunction<void(AFGHologram*)> CollectRecursive = [&](AFGHologram* Hologram)
	{
		if (!Hologram)
		{
			return;
		}

		// Add this hologram if it's a distributor
		if (AutoConnectService && AutoConnectService->IsDistributorHologram(Hologram))
		{
			OutDistributors.Add(Hologram);
		}

		// Recurse into all children
		const TArray<AFGHologram*>& Children = Hologram->GetHologramChildren();
		for (AFGHologram* Child : Children)
		{
			CollectRecursive(Child);
		}
	};

	// Start recursive collection from parent
	CollectRecursive(ParentHologram.Get());

	// Only log at normal verbosity if we found distributors
	if (OutDistributors.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Collected %d distributors from entire grid"), 
			OutDistributors.Num());
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Collected 0 distributors from grid"));
	}
}

void USFAutoConnectOrchestrator::EvaluateConnections()
{
	if (!AutoConnectService)
	{
		return;
	}

	// Issue #246: Check if belt auto-connect is enabled in runtime settings
	// This respects the global config setting (bAutoConnectEnabled) which initializes runtime settings
	if (USFSubsystem* Subsystem = AutoConnectService->GetSubsystem())
	{
		if (!Subsystem->GetAutoConnectRuntimeSettings().bEnabled)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: EvaluateConnections skipped - Belt auto-connect disabled"));
			return;
		}
	}

	// Collect all distributors in the grid
	TArray<AFGHologram*> Distributors;
	CollectDistributors(Distributors);

	// Early exit if no distributors (already logged in CollectDistributors)
	if (Distributors.Num() == 0)
	{
		return;
	}

	const FTransform ParentTransform = ParentHologram.IsValid() ? ParentHologram->GetActorTransform() : FTransform::Identity;

	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 Orchestrator: Evaluating connections for %d distributors using GLOBAL SCORING"), 
		Distributors.Num());

	// ========================================
	// PHASE 1: Collect ALL potential connections with scores
	// ========================================
	TArray<FPotentialConnection> AllConnections;
	CollectPotentialConnections(Distributors, ParentTransform, AllConnections);
	
	if (AllConnections.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("   ⚠️ No potential connections found"));
		return;
	}

	// ========================================
	// PHASE 2: Sort by score (lower = better, lane-aligned first)
	// ========================================
	AllConnections.Sort();
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 GLOBAL SCORING: Sorted %d connections by score"), AllConnections.Num());
	
	// Log top candidates for debugging
	for (int32 i = 0; i < FMath::Min(10, AllConnections.Num()); i++)
	{
		const FPotentialConnection& Conn = AllConnections[i];
		UE_LOG(LogSmartFoundations, Log, TEXT("   [%d] Score=%.0f | %s (lane %d) -> %s (idx %d)"),
			i, Conn.Score, 
			*Conn.Distributor->GetName(), Conn.DistributorLaneIndex,
			*Conn.BuildingConnector->GetName(), Conn.BuildingInputIndex);
	}

	// ========================================
	// PHASE 3: Optimal assignment (best-scored first, skip conflicts)
	// ========================================
	TSet<UFGFactoryConnectionComponent*> AssignedDistributorConnectors;
	TSet<UFGFactoryConnectionComponent*> AssignedBuildingConnectors;
	TMap<AFGHologram*, TArray<FPotentialConnection>> AssignmentsByDistributor;
	
	int32 TotalAssignments = 0;
	for (const FPotentialConnection& Conn : AllConnections)
	{
		// Skip if distributor connector already assigned
		if (AssignedDistributorConnectors.Contains(Conn.DistributorConnector))
		{
			continue;
		}
		
		// Skip if building connector already assigned
		if (AssignedBuildingConnectors.Contains(Conn.BuildingConnector))
		{
			continue;
		}
		
		// Assign this connection
		AssignedDistributorConnectors.Add(Conn.DistributorConnector);
		AssignedBuildingConnectors.Add(Conn.BuildingConnector);
		ReservedInputs.Add(Conn.BuildingConnector, Conn.Distributor);
		
		// Track assignment by distributor for belt creation
		if (!AssignmentsByDistributor.Contains(Conn.Distributor))
		{
			AssignmentsByDistributor.Add(Conn.Distributor, TArray<FPotentialConnection>());
		}
		AssignmentsByDistributor[Conn.Distributor].Add(Conn);
		
		TotalAssignments++;
		
		UE_LOG(LogSmartFoundations, Log, TEXT("   ✅ ASSIGNED: %s (lane %d) -> %s (idx %d) | Score=%.0f"),
			*Conn.Distributor->GetName(), Conn.DistributorLaneIndex,
			*Conn.BuildingConnector->GetName(), Conn.BuildingInputIndex,
			Conn.Score);
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 GLOBAL SCORING: Made %d optimal assignments"), TotalAssignments);

	// ========================================
	// PHASE 4: Create belt previews for assigned connections
	// ========================================
	int32 TotalPreviewsCreated = 0;
	for (auto& Pair : AssignmentsByDistributor)
	{
		AFGHologram* Distributor = Pair.Key;
		TArray<FPotentialConnection>& Assignments = Pair.Value;
		
		bool bIsParent = (Distributor == ParentHologram.Get());
		
		// Get existing belt previews for this distributor to enable reuse
		TArray<TSharedPtr<FBeltPreviewHelper>>* ExistingPreviews = AutoConnectService->GetBeltPreviews(Distributor);
		TArray<TSharedPtr<FBeltPreviewHelper>> Previews;
		
		for (const FPotentialConnection& Conn : Assignments)
		{
			// Determine output/input based on distributor type (splitter vs merger)
			bool bIsSplitter = USFAutoConnectService::IsSplitterHologram(Distributor);
			
			UFGFactoryConnectionComponent* OutputConnector = bIsSplitter ? Conn.DistributorConnector : Conn.BuildingConnector;
			UFGFactoryConnectionComponent* InputConnector = bIsSplitter ? Conn.BuildingConnector : Conn.DistributorConnector;
			
			// DEDUPLICATION: Check if we already have a belt preview for this connector pair
			TSharedPtr<FBeltPreviewHelper> ExistingPreview;
			if (ExistingPreviews)
			{
				for (const TSharedPtr<FBeltPreviewHelper>& Existing : *ExistingPreviews)
				{
					if (Existing.IsValid() &&
						Existing->GetOutputConnector() == OutputConnector &&
						Existing->GetInputConnector() == InputConnector)
					{
						ExistingPreview = Existing;
						break;
					}
				}
			}
			
			if (ExistingPreview.IsValid())
			{
				// CRITICAL FIX: Update existing belt preview with new connector positions
				// Previously this just reused the preview without updating, causing stale positions
				// when parent moves without child distributors
				ExistingPreview->UpdatePreview(OutputConnector, InputConnector);
				
				Previews.Add(ExistingPreview);
				TotalPreviewsCreated++;
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   ♻️ UPDATED & REUSING BELT: %s -> %s (Dist: %s)"),
					*GetNameSafe(OutputConnector), *GetNameSafe(InputConnector), *Distributor->GetName());
				continue;
			}
			
			UE_LOG(LogSmartFoundations, Log, TEXT("   🔧 BUILDING BELT: %s -> %s (Dist: %s)"),
				*GetNameSafe(OutputConnector), *GetNameSafe(InputConnector), *Distributor->GetName());
			
			// Use the service to create the actual belt preview
			TSharedPtr<FBeltPreviewHelper> Preview;
			bool bSuccess = AutoConnectService->CreateOrUpdateBeltPreview(
				OutputConnector,
				InputConnector,
				Preview,
				30.0f,  // MaxAngleDegrees
				false,  // bSkipAngleValidation
				Distributor  // ParentDistributor
			);
			
			if (bSuccess && Preview.IsValid())
			{
				Previews.Add(Preview);
				TotalPreviewsCreated++;
				UE_LOG(LogSmartFoundations, Log, TEXT("      ✅ BUILDING BELT SUCCESS"));
			}
			else
			{
				UE_LOG(LogSmartFoundations, Log, TEXT("      ❌ BUILDING BELT FAILED: bSuccess=%d, Preview=%s"),
					bSuccess, Preview.IsValid() ? TEXT("Valid") : TEXT("Invalid"));
			}
		}
		
		// Store previews for this distributor
		AutoConnectService->StoreBeltPreviews(Distributor, Previews);
		
		if (Previews.Num() > 0)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("   ✅ [%s] Created %d belt(s)"), 
				*Distributor->GetName(), Previews.Num());
			
			// CONTEXT-AWARE SPACING: Apply for parent distributor
			if (bIsParent && Previews[0].IsValid() && Assignments.Num() > 0)
			{
				AFGBuildable* Building = Assignments[0].Building;
				if (Building)
				{
					UClass* CurrentBuildingClass = Building->GetClass();
					
					bool bShouldApply = false;
					if (!bContextSpacingApplied)
					{
						bShouldApply = true;
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🎯 CONTEXT-AWARE SPACING: First connection detected"));
					}
					else if (LastTargetBuildingClass.IsValid() && LastTargetBuildingClass.Get() != CurrentBuildingClass)
					{
						bShouldApply = true;
						UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🎯 CONTEXT-AWARE SPACING: Target building changed"));
					}
					
					if (bShouldApply)
					{
						USFBuildableSizeRegistry::Initialize();
						FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(CurrentBuildingClass);
						float BuildingWidth = Profile.DefaultSize.X - 400.0f;
						
						if (BuildingWidth > 0.0f)
						{
							if (USFSubsystem* Subsystem = AutoConnectService->GetSubsystem())
							{
								FSFCounterState NewState = Subsystem->GetCounterState();
								NewState.SpacingX = FMath::RoundToInt(BuildingWidth);
								NewState.SpacingY = FMath::RoundToInt(BuildingWidth);
								Subsystem->UpdateCounterState(NewState);
								
								bContextSpacingApplied = true;
								LastTargetBuildingClass = CurrentBuildingClass;
								
								UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   🎯 CONTEXT-AWARE SPACING: Auto-adjusted to %.1fm"),
									BuildingWidth / 100.0f);
							}
						}
					}
				}
			}
		}
		else
		{
			AutoConnectService->CleanupDistributorPreviews(Distributor);
		}
	}
	
	// Clean up distributors with no assignments
	for (AFGHologram* Distributor : Distributors)
	{
		if (!AssignmentsByDistributor.Contains(Distributor))
		{
			AutoConnectService->CleanupDistributorPreviews(Distributor);
			AutoConnectService->StoreBeltPreviews(Distributor, TArray<TSharedPtr<FBeltPreviewHelper>>());
		}
	}

	// Summary log
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 EVALUATION COMPLETE: %d belts for %d distributors | Reservations: %d"), 
		TotalPreviewsCreated, Distributors.Num(), ReservedInputs.Num());
	
	if (TotalPreviewsCreated == 0)
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("   ⚠️ No belts created - check logs above for rejection reasons"));
	}

	// ========================================
	// MANIFOLD CONNECTIONS: Distributor-to-Distributor
	// Check if distributor chaining is enabled
	if (USFSubsystem* Subsystem = AutoConnectService->GetSubsystem())
	{
		if (!Subsystem->GetAutoConnectRuntimeSettings().bChainDistributors)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Skipping manifold connections (bChainDistributors=false)"));
			return; // Skip manifold connections when disabled
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🔗 Manifold Detection: Evaluating %d distributors for chaining"), Distributors.Num());

	// Build lanes per input index from BUILDING reservations
	TMap<int32, TArray<AFGHologram*>> DistributorsByInputIndex;
	for (const auto& Pair : ReservedInputs)
	{
		UFGFactoryConnectionComponent* BuildingInput = Pair.Key;
		AFGHologram* Distributor = Pair.Value;

		// Extract input index from connector name (e.g., "Input0" → 0, "Input1" → 1)
		FString ConnectorName = BuildingInput->GetName();
		int32 InputIndex = -1;
		if (ConnectorName.Contains(TEXT("Input")))
		{
			FString IndexStr = ConnectorName.Replace(TEXT("Input"), TEXT(""));
			InputIndex = FCString::Atoi(*IndexStr);
		}

		if (InputIndex >= 0)
		{
			if (!DistributorsByInputIndex.Contains(InputIndex))
			{
				DistributorsByInputIndex.Add(InputIndex, TArray<AFGHologram*>());
			}
			if (!DistributorsByInputIndex[InputIndex].Contains(Distributor))
			{
				DistributorsByInputIndex[InputIndex].Add(Distributor);
			}
		}
	}
	
	// IMPORTANT: Use separate reservation maps for manifold so we don't pollute building pairings
	ManifoldReservedInputs.Empty();
	ManifoldReservedOutputs.Empty();
	
	UE_LOG(LogSmartFoundations, Log, TEXT("   📊 Found %d input index groups for manifold chaining"), DistributorsByInputIndex.Num());
	
	// Get parent transform for spatial sorting
	const FTransform SortTransform = ParentHologram.IsValid() ? ParentHologram->GetActorTransform() : FTransform::Identity;
	
	// For each input index group, SORT spatially then connect the splitters sequentially
	for (auto& IndexGroup : DistributorsByInputIndex)
	{
		int32 InputIndex = IndexGroup.Key;
		TArray<AFGHologram*>& SplittersInLane = IndexGroup.Value;
		
		if (SplittersInLane.Num() < 2)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("   ⏭️ Input%d lane: Only %d splitter(s) - skipping manifold"), 
				InputIndex, SplittersInLane.Num());
			continue; // Need at least 2 splitters to connect
		}
		
		// CRITICAL FIX: Sort distributors spatially within this lane to ensure stable chaining
		// Without this, the order changes between evaluations causing manifold belts to vanish
		{
			float MinX = TNumericLimits<float>::Max();
			float MaxX = TNumericLimits<float>::Lowest();
			float MinY = TNumericLimits<float>::Max();
			float MaxY = TNumericLimits<float>::Lowest();
			for (AFGHologram* D : SplittersInLane)
			{
				if (!D) continue;
				const FVector LocalPos = SortTransform.InverseTransformPosition(D->GetActorLocation());
				MinX = FMath::Min(MinX, LocalPos.X);
				MaxX = FMath::Max(MaxX, LocalPos.X);
				MinY = FMath::Min(MinY, LocalPos.Y);
				MaxY = FMath::Max(MaxY, LocalPos.Y);
			}
			const float RangeX = MaxX - MinX;
			const float RangeY = MaxY - MinY;
			const bool bUseX = RangeX >= RangeY;
			
			SplittersInLane.Sort([&SortTransform, bUseX](const AFGHologram& A, const AFGHologram& B)
			{
				const FVector LA = SortTransform.InverseTransformPosition(A.GetActorLocation());
				const FVector LB = SortTransform.InverseTransformPosition(B.GetActorLocation());
				const float KA = bUseX ? LA.X : LA.Y;
				const float KB = bUseX ? LB.X : LB.Y;
				if (FMath::Abs(KA - KB) > 1.0f)
				{
					return KA < KB;
				}
				return A.GetName() < B.GetName();
			});
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("   🔗 Input%d lane: Connecting %d splitters in manifold (sorted spatially)"), 
			InputIndex, SplittersInLane.Num());
		
		// Connect each splitter to the next one in the lane
		for (int32 i = 0; i < SplittersInLane.Num() - 1; i++)
		{
			AFGHologram* SourceDistributor = SplittersInLane[i];
			AFGHologram* TargetDistributor = SplittersInLane[i + 1];
			
			// Find middle outputs/inputs for both directions
			UFGFactoryConnectionComponent* A_MiddleOut = AutoConnectService->FindMiddleConnector(SourceDistributor);
			UFGFactoryConnectionComponent* B_MiddleOut = AutoConnectService->FindMiddleConnector(TargetDistributor);
			if (!A_MiddleOut || A_MiddleOut->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ❌ %s has no middle output"), *SourceDistributor->GetName());
				continue;
			}
			if (!B_MiddleOut || B_MiddleOut->GetDirection() != EFactoryConnectionDirection::FCD_OUTPUT)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ℹ️ %s has no middle output (may be merger)"), *TargetDistributor->GetName());
			}

			// Get BACK inputs for both distributors (inputs facing opposite actor forward)
			auto GetBackInput = [](AFGHologram* Distro) -> UFGFactoryConnectionComponent*
			{
				TArray<UFGFactoryConnectionComponent*> Connectors;
				Distro->GetComponents<UFGFactoryConnectionComponent>(Connectors);
				UFGFactoryConnectionComponent* BackInput = nullptr;
				const FVector Fwd = Distro->GetActorForwardVector();
				float BestDotLocal = 1.0f; // most negative
				for (UFGFactoryConnectionComponent* C : Connectors)
				{
					if (C && C->GetDirection() == EFactoryConnectionDirection::FCD_INPUT)
					{
						const float Dot = FVector::DotProduct(C->GetForwardVector(), Fwd);
						if (!BackInput || Dot < BestDotLocal)
						{
							BestDotLocal = Dot;
							BackInput = C;
						}
					}
				}
				return BackInput;
			};

			UFGFactoryConnectionComponent* A_BackIn = GetBackInput(SourceDistributor);
			UFGFactoryConnectionComponent* B_BackIn = GetBackInput(TargetDistributor);
			if (!B_BackIn)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ❌ Target %s has no back input"), *TargetDistributor->GetName());
				continue;
			}

			// Try both orientations: A→B and B→A. Choose the one with better facing.
			struct FManifoldOption { UFGFactoryConnectionComponent* Out; UFGFactoryConnectionComponent* In; AFGHologram* StoreOn; float Dot; };
			TArray<FManifoldOption> Options;
			// Option 1: Source → Target
			if (A_MiddleOut && B_BackIn)
			{
				const FVector SrcPos = A_MiddleOut->GetComponentLocation();
				const FVector TgtPos = B_BackIn->GetComponentLocation();
				const float Dot = FVector::DotProduct(A_MiddleOut->GetConnectorNormal(), (TgtPos - SrcPos).GetSafeNormal());
				Options.Add({A_MiddleOut, B_BackIn, SourceDistributor, Dot});
			}
			// Option 2: Target → Source (only if target has middle out and source has back in)
			if (B_MiddleOut && A_BackIn)
			{
				const FVector SrcPos = B_MiddleOut->GetComponentLocation();
				const FVector TgtPos = A_BackIn->GetComponentLocation();
				const float Dot = FVector::DotProduct(B_MiddleOut->GetConnectorNormal(), (TgtPos - SrcPos).GetSafeNormal());
				Options.Add({B_MiddleOut, A_BackIn, TargetDistributor, Dot});
			}

			// Pick the best facing option
			Options.Sort([](const FManifoldOption& L, const FManifoldOption& R){ return L.Dot > R.Dot; });
			bool bLinked = false;
			for (const FManifoldOption& Opt : Options)
			{
				if (Opt.Dot < 0.2f)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⏭️ Skipping option: poor facing alignment (dot=%.2f)"), Opt.Dot);
					continue;
				}
				if (ManifoldReservedInputs.Contains(Opt.In))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⏭️ Manifold target input already reserved - skip option"));
					continue;
				}
				if (ManifoldReservedOutputs.Contains(Opt.Out))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⏭️ Manifold source output already reserved - skip option"));
					continue;
				}

				UE_LOG(LogSmartFoundations, Log, TEXT("      🔗 MANIFOLD BELT: %s.%s → %s.%s [Input%d lane] (dot=%.2f)"),
					*Opt.StoreOn->GetName(),
					*Opt.Out->GetName(),
					*Cast<AFGHologram>(Opt.In->GetOwner())->GetName(),
					*Opt.In->GetName(),
					InputIndex,
					Opt.Dot);

				// Create connection with normal angle validation
				if (AutoConnectService->ConnectAnyConnectors(Opt.Out, Opt.In, Opt.StoreOn, false))
				{
					ManifoldReservedInputs.Add(Opt.In, Opt.StoreOn);
					ManifoldReservedOutputs.Add(Opt.Out, Opt.StoreOn);
					bLinked = true;
					break;
				}
			}

			if (!bLinked)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ℹ️ No valid manifold orientation found for this pair"));
			}
		}
	}
	
	// ========================================
	// MERGER MANIFOLD CONNECTIONS: Group mergers by building OUTPUT index
	// Mergers collect from buildings, so we group by which OUTPUT they're connected to
	TMap<int32, TArray<AFGHologram*>> MergersByOutputIndex;
	for (const auto& Pair : ReservedInputs)
	{
		UFGFactoryConnectionComponent* Connector = Pair.Key;
		AFGHologram* Distributor = Pair.Value;
		
		// Check if this is a merger (building output → merger input)
		if (!AutoConnectService->IsMergerHologram(Distributor))
			continue;
		
		// For mergers, the "reserved input" is actually a building OUTPUT
		// Extract output index from connector name (e.g., "Output0" → 0, "Output1" → 1)
		FString ConnectorName = Connector->GetName();
		int32 OutputIndex = -1;
		if (ConnectorName.Contains(TEXT("Output")))
		{
			FString IndexStr = ConnectorName.Replace(TEXT("Output"), TEXT(""));
			OutputIndex = FCString::Atoi(*IndexStr);
		}
		
		if (OutputIndex >= 0)
		{
			if (!MergersByOutputIndex.Contains(OutputIndex))
			{
				MergersByOutputIndex.Add(OutputIndex, TArray<AFGHologram*>());
			}
			if (!MergersByOutputIndex[OutputIndex].Contains(Distributor))
			{
				MergersByOutputIndex[OutputIndex].Add(Distributor);
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("   📊 Found %d merger output index groups for manifold chaining"), MergersByOutputIndex.Num());
	
	// For each output index group, SORT spatially then connect the mergers sequentially
	for (auto& IndexGroup : MergersByOutputIndex)
	{
		int32 OutputIndex = IndexGroup.Key;
		TArray<AFGHologram*>& MergersInLane = IndexGroup.Value;
		
		if (MergersInLane.Num() < 2)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("   ⏭️ Output%d lane: Only %d merger(s) - skipping manifold"), 
				OutputIndex, MergersInLane.Num());
			continue;
		}
		
		// CRITICAL FIX: Sort mergers spatially within this lane to ensure stable chaining
		{
			float MinX = TNumericLimits<float>::Max();
			float MaxX = TNumericLimits<float>::Lowest();
			float MinY = TNumericLimits<float>::Max();
			float MaxY = TNumericLimits<float>::Lowest();
			for (AFGHologram* D : MergersInLane)
			{
				if (!D) continue;
				const FVector LocalPos = SortTransform.InverseTransformPosition(D->GetActorLocation());
				MinX = FMath::Min(MinX, LocalPos.X);
				MaxX = FMath::Max(MaxX, LocalPos.X);
				MinY = FMath::Min(MinY, LocalPos.Y);
				MaxY = FMath::Max(MaxY, LocalPos.Y);
			}
			const float RangeX = MaxX - MinX;
			const float RangeY = MaxY - MinY;
			const bool bUseX = RangeX >= RangeY;
			
			MergersInLane.Sort([&SortTransform, bUseX](const AFGHologram& A, const AFGHologram& B)
			{
				const FVector LA = SortTransform.InverseTransformPosition(A.GetActorLocation());
				const FVector LB = SortTransform.InverseTransformPosition(B.GetActorLocation());
				const float KA = bUseX ? LA.X : LA.Y;
				const float KB = bUseX ? LB.X : LB.Y;
				if (FMath::Abs(KA - KB) > 1.0f)
				{
					return KA < KB;
				}
				return A.GetName() < B.GetName();
			});
		}
		
		UE_LOG(LogSmartFoundations, Log, TEXT("   🔗 Output%d lane: Connecting %d mergers in manifold (sorted spatially)"), 
			OutputIndex, MergersInLane.Num());
		
		// Connect each pair of adjacent mergers in the lane
		// SAME PATTERN AS SPLITTERS: Try both orientations, pick best facing
		for (int32 i = 0; i < MergersInLane.Num() - 1; i++)
		{
			AFGHologram* SourceMerger = MergersInLane[i];
			AFGHologram* TargetMerger = MergersInLane[i + 1];
			
			// Get connectors
			// For mergers: single output, back input
			// Note: Mergers only have 1 output, so just find it directly
			UFGFactoryConnectionComponent* A_MiddleOut = nullptr;
			UFGFactoryConnectionComponent* A_BackIn = nullptr;
			UFGFactoryConnectionComponent* B_MiddleOut = nullptr;
			UFGFactoryConnectionComponent* B_BackIn = nullptr;
			
			// Find outputs (mergers have only 1)
			TArray<UFGFactoryConnectionComponent*> SourceOutputs;
			SourceMerger->GetComponents<UFGFactoryConnectionComponent>(SourceOutputs);
			for (UFGFactoryConnectionComponent* C : SourceOutputs)
			{
				if (C && C->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
				{
					A_MiddleOut = C;
					break;
				}
			}
			
			TArray<UFGFactoryConnectionComponent*> TargetOutputs;
			TargetMerger->GetComponents<UFGFactoryConnectionComponent>(TargetOutputs);
			for (UFGFactoryConnectionComponent* C : TargetOutputs)
			{
				if (C && C->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT)
				{
					B_MiddleOut = C;
					break;
				}
			}
			
			// Find back inputs using FindMiddleConnector (for mergers, it returns the back input)
			A_BackIn = AutoConnectService->FindMiddleConnector(SourceMerger);
			B_BackIn = AutoConnectService->FindMiddleConnector(TargetMerger);
			
			if (!A_MiddleOut)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ❌ Source %s has no middle output"), *SourceMerger->GetName());
				continue;
			}
			if (!B_BackIn)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ❌ Target %s has no back input"), *TargetMerger->GetName());
				continue;
			}

			// Try both orientations: A→B and B→A. Choose the one with better facing (SAME AS SPLITTERS)
			struct FManifoldOption { UFGFactoryConnectionComponent* Out; UFGFactoryConnectionComponent* In; AFGHologram* StoreOn; float Dot; };
			TArray<FManifoldOption> Options;
			// Option 1: Source → Target
			if (A_MiddleOut && B_BackIn)
			{
				const FVector SrcPos = A_MiddleOut->GetComponentLocation();
				const FVector TgtPos = B_BackIn->GetComponentLocation();
				const float Dot = FVector::DotProduct(A_MiddleOut->GetConnectorNormal(), (TgtPos - SrcPos).GetSafeNormal());
				Options.Add({A_MiddleOut, B_BackIn, SourceMerger, Dot});
			}
			// Option 2: Target → Source (only if target has middle out and source has back in)
			if (B_MiddleOut && A_BackIn)
			{
				const FVector SrcPos = B_MiddleOut->GetComponentLocation();
				const FVector TgtPos = A_BackIn->GetComponentLocation();
				const float Dot = FVector::DotProduct(B_MiddleOut->GetConnectorNormal(), (TgtPos - SrcPos).GetSafeNormal());
				Options.Add({B_MiddleOut, A_BackIn, TargetMerger, Dot});
			}

			// Pick the best facing option (SAME AS SPLITTERS)
			Options.Sort([](const FManifoldOption& L, const FManifoldOption& R){ return L.Dot > R.Dot; });
			bool bLinked = false;
			for (const FManifoldOption& Opt : Options)
			{
				if (Opt.Dot < 0.2f)
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⏭️ Skipping option: poor facing alignment (dot=%.2f)"), Opt.Dot);
					continue;
				}
				if (ManifoldReservedInputs.Contains(Opt.In))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⏭️ Manifold target input already reserved - skip option"));
					continue;
				}
				if (ManifoldReservedOutputs.Contains(Opt.Out))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ⏭️ Manifold source output already reserved - skip option"));
					continue;
				}

				UE_LOG(LogSmartFoundations, Verbose, TEXT("      🔗 MERGER MANIFOLD: %s → %s [Output%d lane] (dot=%.2f)"),
					*Opt.StoreOn->GetName(),
					*Cast<AFGHologram>(Opt.In->GetOwner())->GetName(),
					OutputIndex,
					Opt.Dot);

				// Create connection with normal angle validation (SAME AS SPLITTERS)
				if (AutoConnectService->ConnectAnyConnectors(Opt.Out, Opt.In, Opt.StoreOn, false))
				{
					ManifoldReservedInputs.Add(Opt.In, Opt.StoreOn);
					ManifoldReservedOutputs.Add(Opt.Out, Opt.StoreOn);
					bLinked = true;
					break;
				}
			}

			if (!bLinked)
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("      ℹ️ No valid manifold orientation found for this pair"));
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Manifold evaluation complete"));

	// Build connection pairings map from BUILDING reservation data only
	ConnectionPairings.Empty();
	for (const auto& Pair : ReservedInputs)
	{
		UFGFactoryConnectionComponent* BuildingInput = Pair.Key;
		AFGHologram* Distributor = Pair.Value;
		
		if (!ConnectionPairings.Contains(Distributor))
		{
			ConnectionPairings.Add(Distributor, TArray<UFGFactoryConnectionComponent*>());
		}
		
		ConnectionPairings[Distributor].Add(BuildingInput);
	}
	
	// Log all connection pairings for debugging
	LogConnectionPairings();
	
	// CRITICAL: Finalize visibility and locking for all belt children (matches stackable pole pattern)
	// This ensures belt children remain visible when parent is locked and grid changes
	for (int32 i = 0; i < Distributors.Num(); i++)
	{
		AFGHologram* Distributor = Distributors[i];
		AutoConnectService->FinalizeBeltChildrenVisibility(Distributor);
	}
}

void USFAutoConnectOrchestrator::LogConnectionPairings() const
{
	if (ConnectionPairings.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 CONNECTION PAIRINGS (BUILDINGS): None (no connections established)"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 ========== CONNECTION PAIRINGS (BUILDINGS) =========="));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Total Distributors Connected: %d"), ConnectionPairings.Num());
	
	int32 TotalConnections = 0;
	for (const auto& Pair : ConnectionPairings)
	{
		AFGHologram* Distributor = Pair.Key;
		const TArray<UFGFactoryConnectionComponent*>& BuildingInputs = Pair.Value;
		
		bool bIsParent = (Distributor == ParentHologram.Get());
		FString DistributorType = bIsParent ? TEXT("PARENT") : TEXT("CHILD");
		
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 [%s] %s → %d building(s)"), 
			*DistributorType, *Distributor->GetName(), BuildingInputs.Num());
		
		for (int32 i = 0; i < BuildingInputs.Num(); i++)
		{
			UFGFactoryConnectionComponent* BuildingInput = BuildingInputs[i];
			AActor* BuildingOwner = BuildingInput ? BuildingInput->GetOwner() : nullptr;
			FString BuildingName = BuildingOwner ? BuildingOwner->GetName() : TEXT("Unknown");
			
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯    → Building %d: %s (Input: %s)"), 
				i + 1, *BuildingName, *BuildingInput->GetName());
			
			TotalConnections++;
		}
	}
	
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Total Connections: %d"), TotalConnections);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 =========================================="));
}

void USFAutoConnectOrchestrator::LogManifoldPairings() const
{
	if (ManifoldReservedInputs.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 MANIFOLD PAIRINGS: None"));
		return;
	}

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 ========== MANIFOLD PAIRINGS =========="));
	int32 Count = 0;
	for (const auto& Pair : ManifoldReservedInputs)
	{
		UFGFactoryConnectionComponent* TargetInput = Pair.Key;
		AFGHologram* SourceDistributor = Pair.Value;
		AFGHologram* TargetDistributor = TargetInput ? Cast<AFGHologram>(TargetInput->GetOwner()) : nullptr;
		FString TargetName = TargetDistributor ? TargetDistributor->GetName() : TEXT("UnknownTarget");
		FString SourceName = SourceDistributor ? SourceDistributor->GetName() : TEXT("UnknownSource");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯    %s → %s (Input: %s)"), *SourceName, *TargetName, *TargetInput->GetName());
		Count++;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Total Manifold Links: %d"), Count);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 =========================================="));
}

void USFAutoConnectOrchestrator::ClearAllPreviews()
{
	if (!AutoConnectService)
	{
		return;
	}

	// Collect all distributors
	TArray<AFGHologram*> Distributors;
	CollectDistributors(Distributors);

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Clearing belt previews for %d distributors"), 
		Distributors.Num());

	// Clean up belt previews for each distributor
	for (AFGHologram* Distributor : Distributors)
	{
		AutoConnectService->CleanupDistributorPreviews(Distributor);
	}
	
	// CRITICAL: Also clear pipe previews if parent is a pipeline junction
	// This ensures pipe previews are destroyed when build is cancelled/recipe changed
	if (ParentHologram.IsValid() && USFAutoConnectService::IsPipelineJunctionHologram(ParentHologram.Get()))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Clearing pipe previews for junction %s"), 
			*ParentHologram->GetName());
		AutoConnectService->ClearPipePreviews(ParentHologram.Get());
	}

	// Clear power previews if parent is a power pole
	if (ParentHologram.IsValid() && USFAutoConnectService::IsPowerPoleHologram(ParentHologram.Get()))
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🎯 Orchestrator: Clearing power previews for pole %s"), 
			*ParentHologram->GetName());
		AutoConnectService->ClearAllPowerPreviews();
	}
}

// ========================================
// Global Connection Scoring Helpers
// ========================================

int32 USFAutoConnectOrchestrator::ExtractBuildingInputIndex(UFGFactoryConnectionComponent* Connector)
{
	if (!Connector)
	{
		return -1;
	}
	
	FString ConnectorName = Connector->GetName();
	
	// Handle "Input0", "Input1", etc.
	if (ConnectorName.Contains(TEXT("Input")))
	{
		FString IndexStr = ConnectorName.Replace(TEXT("Input"), TEXT(""));
		return FCString::Atoi(*IndexStr);
	}
	
	// Handle "Output0", "Output1", etc. (for mergers)
	if (ConnectorName.Contains(TEXT("Output")))
	{
		FString IndexStr = ConnectorName.Replace(TEXT("Output"), TEXT(""));
		return FCString::Atoi(*IndexStr);
	}
	
	return -1;
}

int32 USFAutoConnectOrchestrator::CalculateDistributorLaneIndex(
	AFGHologram* Distributor,
	const FTransform& ParentTransform,
	const TArray<AFGHologram*>& AllDistributors)
{
	if (!Distributor || AllDistributors.Num() == 0)
	{
		return 0;
	}
	
	// Transform distributor position to parent-local space
	FVector LocalPos = ParentTransform.InverseTransformPosition(Distributor->GetActorLocation());
	
	// Collect all unique X positions (lateral axis in local space)
	TArray<float> UniqueXPositions;
	for (AFGHologram* Dist : AllDistributors)
	{
		if (Dist)
		{
			FVector DistLocalPos = ParentTransform.InverseTransformPosition(Dist->GetActorLocation());
			
			// Check if this X position is already in the list (within tolerance)
			bool bFound = false;
			for (float ExistingX : UniqueXPositions)
			{
				if (FMath::Abs(DistLocalPos.X - ExistingX) < 50.0f) // 50cm tolerance
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				UniqueXPositions.Add(DistLocalPos.X);
			}
		}
	}
	
	// Sort X positions to establish lane indices
	UniqueXPositions.Sort();
	
	// Find which lane index this distributor belongs to
	for (int32 i = 0; i < UniqueXPositions.Num(); i++)
	{
		if (FMath::Abs(LocalPos.X - UniqueXPositions[i]) < 50.0f)
		{
			return i;
		}
	}
	
	return 0;
}

void USFAutoConnectOrchestrator::CollectPotentialConnections(
	const TArray<AFGHologram*>& Distributors,
	const FTransform& ParentTransform,
	TArray<FPotentialConnection>& OutConnections)
{
	if (!AutoConnectService)
	{
		return;
	}
	
	USFSubsystem* Subsystem = AutoConnectService->GetSubsystem();
	if (!Subsystem)
	{
		return;
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 GLOBAL SCORING: Collecting potential connections from %d distributors"), Distributors.Num());
	
	int32 TotalBuildingsFound = 0;
	int32 TotalSideConnectorsFound = 0;
	int32 TotalBuildingConnectorsFound = 0;
	int32 TotalValidationsFailed = 0;
	
	// For each distributor, find all compatible building inputs and create candidate connections
	for (AFGHologram* Distributor : Distributors)
	{
		if (!Distributor)
		{
			continue;
		}
		
		// Calculate this distributor's lane index
		int32 DistributorLane = CalculateDistributorLaneIndex(Distributor, ParentTransform, Distributors);
		
		// Determine if this is a splitter or merger
		bool bIsSplitter = USFAutoConnectService::IsSplitterHologram(Distributor);
		bool bIsMerger = USFAutoConnectService::IsMergerHologram(Distributor);
		
		if (!bIsSplitter && !bIsMerger)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("   ⏭️ %s is neither splitter nor merger"), *Distributor->GetName());
			continue;
		}
		
		FVector DistributorPos = Distributor->GetActorLocation();
		
		// Find compatible buildings nearby
		TArray<AFGBuildable*> NearbyBuildings = Subsystem->FindNearbyBuildings(DistributorPos, 2500.0f);
		TotalBuildingsFound += NearbyBuildings.Num();
		
		// Get distributor's side connectors
		TArray<UFGFactoryConnectionComponent*> SideConnectors;
		AutoConnectService->GetAllSideConnectors(Distributor, SideConnectors);
		TotalSideConnectorsFound += SideConnectors.Num();
		
		if (SideConnectors.Num() == 0)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("   ⏭️ %s has 0 side connectors"), *Distributor->GetName());
			continue;
		}
		
		// For each building, find the BEST side connector (closest one that faces the building)
		for (AFGBuildable* Building : NearbyBuildings)
		{
			if (!Building)
			{
				continue;
			}
			
			// Only allow factory buildings and power plants.
			// This prevents connections to unrelated buildables (e.g., belts, lifts, poles, etc.).
			if (!Building->IsA(AFGBuildableFactory::StaticClass()) && !Building->IsA(AFGBuildableGenerator::StaticClass()))
			{
				continue;
			}
			
			// Exclude distributors themselves (splitters/mergers) - those are handled by manifold chaining, not building connections.
			if (Building->IsA(AFGBuildableConveyorAttachment::StaticClass()))
			{
				continue;
			}
			
			// Skip storage containers and other non-production buildings
			FString BuildingClassName = Building->GetClass()->GetName();
			if (BuildingClassName.Contains(TEXT("Storage")) || 
				BuildingClassName.Contains(TEXT("Container")))
			{
				continue;
			}
			
			// Get building connectors
			TArray<UFGFactoryConnectionComponent*> BuildingInputs;
			TArray<UFGFactoryConnectionComponent*> BuildingOutputs;
			AutoConnectService->GetBuildingConnectors(Building, BuildingInputs, BuildingOutputs);
			
			// Choose the right connectors based on distributor type
			TArray<UFGFactoryConnectionComponent*>& BuildingConnectors = bIsSplitter ? BuildingInputs : BuildingOutputs;
			TotalBuildingConnectorsFound += BuildingConnectors.Num();
			
			if (BuildingConnectors.Num() == 0)
			{
				continue;
			}
			
			// For each building connector, find the BEST side connector
			// (the one that faces most directly toward the building connector)
			for (UFGFactoryConnectionComponent* BuildingConnector : BuildingConnectors)
			{
				if (!BuildingConnector)
				{
					continue;
				}
				
				FVector BuildingConnectorPos = BuildingConnector->GetComponentLocation();
				FVector DistributorPos = Distributor->GetActorLocation();
				
				// Find the best side connector - the one that FACES the building connector most directly
				UFGFactoryConnectionComponent* BestSideConnector = nullptr;
				float BestAngle = 180.0f; // Start with worst possible angle
				float BestDistance = FLT_MAX;
				
				for (UFGFactoryConnectionComponent* DistConnector : SideConnectors)
				{
					if (!DistConnector)
					{
						continue;
					}
					
					FVector DistConnectorPos = DistConnector->GetComponentLocation();
					FVector DistConnectorNormal = DistConnector->GetConnectorNormal();
					
					float Distance = FVector::Dist(DistConnectorPos, BuildingConnectorPos);
					if (Distance > 2500.0f)
					{
						TotalValidationsFailed++;
						continue; // Too far
					}
					
					// Calculate the angle between connector normal and direction to building
					FVector ToBuilding = (BuildingConnectorPos - DistConnectorPos).GetSafeNormal();
					float AngleCosine = FVector::DotProduct(DistConnectorNormal, ToBuilding);
					float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(AngleCosine, -1.0f, 1.0f)));
					
					UE_LOG(LogSmartFoundations, Log, TEXT("      📐 %s: Normal=(%.2f,%.2f,%.2f) ToBuilding=(%.2f,%.2f,%.2f) Angle=%.1f° Dist=%.0f"),
						*DistConnector->GetName(),
						DistConnectorNormal.X, DistConnectorNormal.Y, DistConnectorNormal.Z,
						ToBuilding.X, ToBuilding.Y, ToBuilding.Z,
						AngleDegrees, Distance);
					
					// Skip connectors that exceed our maximum allowed belt connector angle.
					// This must match the MaxAngleDegrees used for CreateOrUpdateBeltPreview.
					if (AngleDegrees > 30.0f)
					{
						TotalValidationsFailed++;
						continue;
					}
					
					// Pick the connector with the SMALLEST angle (most direct path)
					// Tie-break by distance
					if (AngleDegrees < BestAngle || (AngleDegrees == BestAngle && Distance < BestDistance))
					{
						BestAngle = AngleDegrees;
						BestDistance = Distance;
						BestSideConnector = DistConnector;
					}
				}
				
				// If we found a valid side connector, create a potential connection
				if (BestSideConnector)
				{
					FVector DistConnectorPos = BestSideConnector->GetComponentLocation();
					FVector ToBuilding = (BuildingConnectorPos - DistConnectorPos).GetSafeNormal();
					float ActualDistance = FVector::Dist(DistConnectorPos, BuildingConnectorPos);
					
					// Calculate alignment for scoring
					FVector DistForward = Distributor->GetActorForwardVector();
					FVector DistRight = Distributor->GetActorRightVector();
					float ForwardDot = FMath::Abs(FVector::DotProduct(ToBuilding, DistForward));
					float RightDot = FMath::Abs(FVector::DotProduct(ToBuilding, DistRight));
					float MaxAlignment = FMath::Max(ForwardDot, RightDot);
					
					// === SCORING: Prioritize STRAIGHT connections (low angle) and SHORT distance ===
					int32 BuildingInputIndex = ExtractBuildingInputIndex(BuildingConnector);
					
					// Calculate the actual angle between connector normal and direction to building
					FVector ConnectorNormal = BestSideConnector->GetConnectorNormal();
					float AngleCosine = FVector::DotProduct(ConnectorNormal, ToBuilding);
					float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(AngleCosine, -1.0f, 1.0f)));
					
					// Score: Heavily weight angle (straighter = much better), then distance
					// A 0° connection at 2000cm should beat a 30° connection at 1000cm
					float Score = AngleDegrees * 100.0f + ActualDistance * 0.1f;
					
					// Log for debugging
					UE_LOG(LogSmartFoundations, VeryVerbose, 
						TEXT("   📐 %s -> %s: Angle=%.1f° Dist=%.0fcm Score=%.0f"),
						*BestSideConnector->GetName(), *BuildingConnector->GetName(),
						AngleDegrees, ActualDistance, Score);
					
					// Create the potential connection
					FPotentialConnection Connection;
					Connection.Distributor = Distributor;
					Connection.DistributorConnector = BestSideConnector;
					Connection.BuildingConnector = BuildingConnector;
					Connection.Building = Building;
					Connection.Score = Score;
					Connection.DistributorLaneIndex = DistributorLane;
					Connection.BuildingInputIndex = BuildingInputIndex;
					Connection.bIsValid = true;
					
					OutConnections.Add(Connection);
				}
			}
		}
	}
	
	UE_LOG(LogSmartFoundations, Log, TEXT("🎯 GLOBAL SCORING: Collected %d potential connections"), OutConnections.Num());
	UE_LOG(LogSmartFoundations, Log, TEXT("   📊 DIAGNOSTICS: Buildings=%d, SideConnectors=%d, BuildingConnectors=%d, ValidationsFailed=%d"),
		TotalBuildingsFound, TotalSideConnectorsFound, TotalBuildingConnectorsFound, TotalValidationsFailed);
}
