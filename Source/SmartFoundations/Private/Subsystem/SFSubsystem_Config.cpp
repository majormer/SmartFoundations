// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USFSubsystem - RPC handlers + configuration system + recipe copying + restore-enhanced + pipe-tier + auto-connect setters.
 * Part of the SFSubsystem implementation split (see SFSubsystem.cpp). No behavior change.
 */

#include "Subsystem/SFSubsystemImpl.h"
#include "Features/Walk/SFWalkService.h"


// ========================================
// RPC Handler Methods (Called by SFRCO)
// ========================================

void USFSubsystem::ApplyScalingFromRPC(AFGHologram* HologramActor, uint8 Axis, int32 Delta, int32 NewCounter)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ApplyScalingFromRPC: Axis=%d, Delta=%d, Counter=%d, Hologram=%s"),
		Axis, Delta, NewCounter, *GetNameSafe(HologramActor));

	// Validate hologram matches our active hologram
	if (!HologramActor || HologramActor != ActiveHologram.Get())
	{
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[RPC] ApplyScalingFromRPC: Hologram mismatch or invalid"));
		return;
	}

	// Validate axis
	if (Axis > 2)
	{
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[RPC] ApplyScalingFromRPC: Invalid axis %d"), Axis);
		return;
	}

	// Calculate scaling delta from counter value
	// Counter represents cumulative steps; Delta is the increment direction
	const float StepSize = ScaleStepSize * Delta;
	FVector ScalingDelta = FVector::ZeroVector;

	switch (Axis)
	{
		case 0: // X axis
			ScalingDelta.X = StepSize;
			break;
		case 1: // Y axis
			ScalingDelta.Y = StepSize;
			break;
		case 2: // Z axis
			ScalingDelta.Z = StepSize;
			break;
	}

	// Apply the scaling
	ApplyScalingToHologram(ScalingDelta);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ApplyScalingFromRPC: Applied delta %s, TotalOffset now %s"),
		*ScalingDelta.ToString(), *CurrentScalingOffset.ToString());

	// TODO Task #21: Replicate to other clients via multicast or property replication
}

void USFSubsystem::ResetScalingFromRPC(AFGHologram* HologramActor)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ResetScalingFromRPC: Hologram=%s"), *GetNameSafe(HologramActor));

	// Validate hologram matches our active hologram
	if (!HologramActor || HologramActor != ActiveHologram.Get())
	{
		UE_LOG(LogSmartFoundations, Verbose,
			TEXT("[RPC] ResetScalingFromRPC: Hologram mismatch or invalid"));
		return;
	}

	// Reset scaling offset
	CurrentScalingOffset = FVector::ZeroVector;

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] ResetScalingFromRPC: Scaling offset cleared"));

	// TODO Task #21: Replicate to other clients
}

void USFSubsystem::SetSpacingModeFromRPC(ESFSpacingMode NewMode)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] SetSpacingModeFromRPC: NewMode=%d"), static_cast<uint8>(NewMode));

	// Update spacing mode in CounterState
	CounterState.SpacingMode = NewMode;

	FString ModeName = FSFSpacingModule::GetSpacingModeName(CounterState.SpacingMode);
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[RPC] SetSpacingModeFromRPC: Mode set to %s"), *ModeName);

	// Sync via centralized update
	UpdateCounterState(CounterState);

	// TODO Task #21: Replicate to other clients
}

void USFSubsystem::SetArrowVisibilityFromRPC(bool bVisible)
{
#if SMART_ARROWS_ENABLED
    UE_LOG(LogSmartFoundations, VeryVerbose,
        TEXT("[RPC] SetArrowVisibilityFromRPC: Visible=%d"), bVisible);

    // Update arrow runtime visibility state - Tick() will handle drawing
    bArrowsRuntimeVisible = bVisible;

    UE_LOG(LogSmartFoundations, VeryVerbose,
        TEXT("[RPC] SetArrowVisibilityFromRPC: Arrows visibility set to %s"),
        bVisible ? TEXT("VISIBLE") : TEXT("HIDDEN"));
#else
    UE_LOG(LogSmartFoundations, Verbose,
        TEXT("[RPC] SetArrowVisibilityFromRPC ignored - arrows feature disabled"));
#endif

    // TODO Task #21: Replicate to other clients
}

FString USFSubsystem::GetRecipeDisplayName(TSubclassOf<UFGRecipe> Recipe) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetRecipeDisplayName(Recipe);
	}
	return TEXT("None");
}

UTexture2D* USFSubsystem::GetRecipePrimaryProductIcon(TSubclassOf<UFGRecipe> Recipe) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetRecipePrimaryProductIcon(Recipe);
	}
	return nullptr;
}

FString USFSubsystem::GetRecipeWithInputsOutputs(TSubclassOf<UFGRecipe> Recipe) const
{
    // Delegate to recipe management service
    if (RecipeManagementService)
    {
        return RecipeManagementService->GetRecipeWithInputsOutputs(Recipe);
    }
    return TEXT("None");
}

void USFSubsystem::GetRecipeDisplayInfo(int32& OutCurrentIndex, int32& OutTotalRecipes) const
{
    if (RecipeManagementService)
    {
        RecipeManagementService->GetRecipeDisplayInfo(OutCurrentIndex, OutTotalRecipes);
    }
    else
    {
        OutCurrentIndex = 0;
        OutTotalRecipes = 0;
    }
}

TSubclassOf<UFGRecipe> USFSubsystem::GetActiveRecipe() const
{
    return RecipeManagementService ? RecipeManagementService->GetActiveRecipe() : nullptr;
}

FString USFSubsystem::GetRecipeWithIngredient(TSubclassOf<UFGRecipe> Recipe) const
{
    // Delegate to recipe management service
    if (RecipeManagementService)
    {
        return RecipeManagementService->GetRecipeWithIngredient(Recipe);
	}
	return TEXT("None");
}

void USFSubsystem::UpdateCounterDisplay()
{
    // Build formatted strings (Phase B): route through HUD service
    TPair<FString, FString> DisplayLines = HudService
        ? HudService->BuildCounterDisplayLines()
        : TPair<FString, FString>(FString(), FString());
    const FString& FirstLine = DisplayLines.Key;
    const FString& SecondLine = DisplayLines.Value;

    // Log for debugging
    if (FirstLine.Len() > 0 || SecondLine.Len() > 0)
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Counter Display] Line1: '%s' Line2: '%s'"),
            *FirstLine, *SecondLine);
    }

    // Update HUD via service
    if (HudService)
    {
        HudService->UpdateWidgetDisplay(FirstLine, SecondLine);
    }
}

// ========================================
// Configuration System (Task 69)
// ========================================

void USFSubsystem::LoadConfiguration()
{
	// Try to load configuration from SML config system
	// Note: ConfigManager might not be available during early initialization
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("⚠️ LoadConfiguration: No world context, using defaults"));
		// Use defaults - config will be loaded later if needed
		bArrowsRuntimeVisible = true;
		return;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!GameInstance)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("⚠️ LoadConfiguration: No game instance, using defaults"));
		bArrowsRuntimeVisible = true;
		return;
	}

	UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>();
	if (!ConfigManager)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("⚠️ LoadConfiguration: ConfigManager not available yet, using defaults"));
		bArrowsRuntimeVisible = true;
		return;
	}

	// ConfigManager is available, load config
	CachedConfig = FSmart_ConfigStruct::GetActiveConfig(this);

	// Initialize runtime arrow visibility from config (Issue #146)
	// This can be toggled during runtime with Num1, but starts with config value
	bArrowsRuntimeVisible = CachedConfig.bShowArrows;

	// Initialize arrow orbit and label toggles from config (Issue #179)
	if (ArrowModule)
	{
		ArrowModule->SetOrbitEnabled(CachedConfig.bShowArrowOrbit);
		ArrowModule->SetLabelsVisible(CachedConfig.bShowArrowLabels);
	}

	// Issue #257: Initialize Extend enabled state from config
	bExtendEnabledByConfig = CachedConfig.bExtendEnabled;

	bConfigLoaded = true;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Smart! Configuration loaded:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   bShowHUD: %s"), CachedConfig.bShowHUD ? TEXT("true") : TEXT("false"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   bShowArrows: %s (runtime: %s, orbit: %s, labels: %s)"),
		CachedConfig.bShowArrows ? TEXT("true") : TEXT("false"),
		bArrowsRuntimeVisible ? TEXT("true") : TEXT("false"),
		CachedConfig.bShowArrowOrbit ? TEXT("true") : TEXT("false"),
		CachedConfig.bShowArrowLabels ? TEXT("true") : TEXT("false"));
}

void USFSubsystem::CleanupStateForWorldTransition()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CleanupStateForWorldTransition: Resetting recipe inheritance state"));

	// Clear stored recipe and cache
	StoredProductionRecipe = nullptr;
	StoredRecipeDisplayName = TEXT("");
	bHasStoredProductionRecipe = false;

	// Clear hologram registry to prevent stale entries
	USFHologramDataRegistry::ClearRegistry();

	// Update HUD to reflect cleared state
	UpdateCounterDisplay();

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("CleanupStateForWorldTransition: Recipe inheritance state reset complete"));
}

bool USFSubsystem::AbortRestoreSession(const TCHAR* Reason)
{
	const bool bHadRestoredTopology = ExtendService && ExtendService->IsRestoredCloneTopologyActive();
	const bool bHadRestoreSession = RestoreService && RestoreService->IsRestoreSessionActive();
	if (!bHadRestoredTopology && !bHadRestoreSession)
	{
		return false;
	}

	if (bHadRestoredTopology)
	{
		ExtendService->ClearRestoredCloneTopologySession(Reason);
	}

	if (bHadRestoreSession)
	{
		RestoreService->ClearActiveRestoreSession(Reason);
	}

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
		TEXT("[SmartRestore] Aborted restore session: reason=%s topology=%d session=%d"),
		Reason ? Reason : TEXT("Unknown"),
		bHadRestoredTopology ? 1 : 0,
		bHadRestoreSession ? 1 : 0);

	UpdateCounterDisplay();
	return true;
}

void USFSubsystem::InitializeHologramCleanup()
{
	if (UWorld* World = GetWorld())
	{
		// Bind to OnActorDestroyed delegate to automatically clean up hologram data
		// This ensures deterministic cleanup for any hologram destruction scenario
		World->AddOnActorDestroyedHandler(FOnActorDestroyed::FDelegate::CreateUObject(this, &USFSubsystem::OnActorDestroyed));
		UE_LOG(LogSmartFoundations, Verbose, TEXT("✅ Initialized Hologram Cleanup: Bound to OnActorDestroyed delegate."));
	}
	else
	{
		UE_LOG(LogSmartFoundations, Error, TEXT(" Failed to Initialize Hologram Cleanup: World is null."));
	}
}

void USFSubsystem::OnActorDestroyed(AActor* DestroyedActor)
{
	// Filter for holograms only (deterministic cleanup)
	if (AFGHologram* Hologram = Cast<AFGHologram>(DestroyedActor))
	{
		USFHologramDataRegistry::ClearData(Hologram);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("OnActorDestroyed: Auto-cleared hologram %s from registry"),
			*GetNameSafe(Hologram));

		// Clean up auto-connect previews when hologram is destroyed (covers Escape, right-click, etc.)
		if (ActiveHologram.IsValid() && ActiveHologram.Get() == Hologram)
		{
			// [#358] Esc/cancel destroys the active hologram WITHOUT passing through
			// UnregisterActiveHologram - the input context would stay stuck on, eating
			// vanilla keys (live find: scale + Esc left the Customizer's X dead).
			if (InputHandler)
			{
				InputHandler->SetSmartContextActive(false);
			}

			if (AutoConnectService)
			{
				// Only clear the appropriate preview type based on hologram type
				if (AutoConnectService->IsDistributorHologram(Hologram))
				{
					AutoConnectService->ClearBeltPreviewHelpers();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared belt previews for distributor (OnActorDestroyed)"));
				}
				else if (AutoConnectService->IsPipelineJunctionHologram(Hologram))
				{
					// CRITICAL FIX: Only clear previews for THIS specific junction, not all junctions
					// This prevents child hologram destruction from clearing parent previews (fixes flickering)
					AutoConnectService->ClearPipePreviews(Hologram);
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared pipe previews for junction %s (OnActorDestroyed)"),
						*GetNameSafe(Hologram));
				}
				else if (AutoConnectService->IsPowerPoleHologram(Hologram))
				{
					// Clear power line previews when power pole hologram is destroyed
					AutoConnectService->ClearAllPowerPreviews();
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" AUTO-CONNECT CLEANUP: Cleared power line previews for pole %s (OnActorDestroyed)"),
						*GetNameSafe(Hologram));
				}
			}

			// NOTE: Don't reset bProcessingGridPlacement here!
			// The hologram is destroyed both on successful build AND on cancel.
			// The belt building lambda will reset the flag after completion.
			// If user cancels, OnActorSpawned never runs, so flag is never set.
		}
	}
}

void USFSubsystem::StartPeriodicCleanup()
{
	// Start periodic cleanup timer during build mode
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PeriodicCleanupTimer,
			this,
			&USFSubsystem::OnPeriodicCleanup,
			10.0f,  // Every 10 seconds
			true    // Looping
		);

		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StartPeriodicCleanup: Started periodic hologram registry cleanup"));
	}
}

void USFSubsystem::StopPeriodicCleanup()
{
	// Stop periodic cleanup timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PeriodicCleanupTimer);
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("StopPeriodicCleanup: Stopped periodic hologram registry cleanup"));
	}
}

void USFSubsystem::OnPeriodicCleanup()
{
	// Perform deterministic cleanup of dead entries
	USFHologramDataRegistry::CleanupDeadEntries();
}

// ========================================
// Build HUD Widget Management
// ========================================

void USFSubsystem::InitializeWidgets()
{
	if (HudService)
	{
		HudService->InitializeWidgets();
	}
}

void USFSubsystem::EnsureHUDBinding()
{
	if (HudService)
	{
		HudService->EnsureHUDBinding();
	}
}

void USFSubsystem::CleanupWidgets()
{
    if (HudService)
    {
        HudService->CleanupWidgets();
    }

    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Widgets cleaned up"));
}

// ========================================
// Recipe Copying System Implementation
// ========================================

void USFSubsystem::StoreProductionRecipeFromBuilding(AFGBuildable* SourceBuilding)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->StoreProductionRecipeFromBuilding(SourceBuilding);
	}
}

void USFSubsystem::ApplyStoredProductionRecipeToBuilding(AFGBuildable* TargetBuilding)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyStoredProductionRecipeToBuilding(TargetBuilding);
	}
}

bool USFSubsystem::IsRecipeCompatibleWithHologram(TSubclassOf<UFGRecipe> Recipe, UClass* HologramBuildClass)
{
	if (RecipeManagementService)
	{
		return RecipeManagementService->IsRecipeCompatibleWithHologram(Recipe, HologramBuildClass);
	}
	return false;
}

bool USFSubsystem::IsProductionBuilding(AFGBuildable* Building) const
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->IsProductionBuilding(Building);
	}
	return false;
}

void USFSubsystem::ClearStoredProductionRecipe()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearStoredProductionRecipe();
	}
}

void USFSubsystem::OnBuildGunRecipeSampled(TSubclassOf<UFGRecipe> SampledRecipe)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->OnBuildGunRecipeSampled(SampledRecipe);
	}
}

// Recipe Selector System Implementation
// ========================================
void USFSubsystem::OnRecipeModeChanged(const FInputActionValue& Value)
{
	bool bPressed = Value.Get<bool>();

	// Context-aware mode switching: U button behaves differently based on hologram type
	// - Distributors: Auto-Connect Settings Mode (navigate belt settings)
	// - Pipe Junctions: Auto-Connect Settings Mode (navigate pipe settings)
	// - Power Poles: Auto-Connect Settings Mode (navigate power settings)
	// - Factories: Recipe Mode (select recipes)

	bool bIsAutoConnectHologram = false;
	bool bIsStackableHypertube = false;
	if (ActiveHologram.IsValid() && AutoConnectService)
	{
		bool bIsDistributor = AutoConnectService->IsDistributorHologram(ActiveHologram.Get());
		bool bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get());
		bool bIsStackablePipe = AutoConnectService->IsPipeSupportHologram(ActiveHologram.Get());
		bool bIsStackableBelt = USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get());
		bool bIsPowerPole = AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get());
		bool bIsPassthroughPipe = USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get());
		bIsStackableHypertube = USFAutoConnectService::IsStackableHypertubeSupportHologram(ActiveHologram.Get());

		bIsAutoConnectHologram = bIsDistributor || bIsPipeJunction || bIsStackablePipe || bIsStackableBelt || bIsPowerPole || bIsPassthroughPipe || bIsStackableHypertube;

		FString BuildClassName = ActiveHologram->GetBuildClass() ? ActiveHologram->GetBuildClass()->GetName() : TEXT("NULL");
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" OnRecipeModeChanged: Hologram=%s, BuildClass=%s, Distributor=%d, PipeJunction=%d, StackablePipe=%d, BeltSupport=%d, PowerPole=%d, PassthroughPipe=%d, IsAutoConnect=%d"),
			*ActiveHologram->GetClass()->GetName(), *BuildClassName, bIsDistributor, bIsPipeJunction, bIsStackablePipe, bIsStackableBelt, bIsPowerPole, bIsPassthroughPipe, bIsAutoConnectHologram);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT(" OnRecipeModeChanged: ActiveHologram=%d, AutoConnectService=%d"),
			ActiveHologram.IsValid(), AutoConnectService != nullptr);
	}

	if (bIsAutoConnectHologram)
	{
		// Auto-Connect Settings Mode for distributors, pipe junctions, and power poles
		bAutoConnectSettingsModeActive = bPressed;
		bRecipeModeActive = false;  // Ensure recipe mode is off

		if (bAutoConnectSettingsModeActive)
		{
			// Initialize runtime settings from config only if not already initialized
			// This preserves user modifications when re-entering settings mode
			if (bConfigLoaded && !AutoConnectRuntimeSettings.bInitialized)
			{
				AutoConnectRuntimeSettings.InitFromConfig(CachedConfig);
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚙️ Auto-Connect runtime settings initialized from config"));
			}

			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("⚙️ Auto-Connect Settings Mode: Active (U held on auto-connect hologram)"));
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Current Setting: %s"), *GetAutoConnectSettingDisplayString());

			// Try to acquire lock
			TryAcquireHologramLock();

			// Smart Walking: the enable toggle is moot during a walk, so don't show it as the entry setting — skip to
			// the first setting that actually shapes the run (belt Tier / pipe Tier).
			if (USFWalkService* WalkSvc = GetWalkService(); WalkSvc && WalkSvc->IsActive())
			{
				if (CurrentAutoConnectSetting == EAutoConnectSetting::StackableBeltEnabled)
				{
					CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltTier;
				}
				else if (CurrentAutoConnectSetting == EAutoConnectSetting::Enabled)
				{
					CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
				}
				else if (CurrentAutoConnectSetting == EAutoConnectSetting::HypertubeEnabled)
				{
					CurrentAutoConnectSetting = EAutoConnectSetting::HypertubeRoutingMode;
				}
			}

			// Hypertube poles expose only Enabled + Routing; if the cursor is on a non-hypertube setting
			// (left over from a belt/pipe pole), seed it onto a hypertube setting so the first U-hold shows the
			// right control instead of a stale one (parallels the Smart Walking seed above).
			if (bIsStackableHypertube
				&& CurrentAutoConnectSetting != EAutoConnectSetting::HypertubeEnabled
				&& CurrentAutoConnectSetting != EAutoConnectSetting::HypertubeRoutingMode)
			{
				// During a walk the enable is moot (the run always lays tube): seed onto Routing; otherwise Enabled.
				const USFWalkService* WalkSvcH = GetWalkService();
				CurrentAutoConnectSetting = (WalkSvcH && WalkSvcH->IsActive())
					? EAutoConnectSetting::HypertubeRoutingMode
					: EAutoConnectSetting::HypertubeEnabled;
			}

			// Update HUD to show current active setting
			UpdateCounterDisplay();
		}
		else
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("⚙️ Auto-Connect Settings Mode: Inactive (U released)"));

			// Try to release lock
			TryReleaseHologramLock();

			// Update HUD to refresh display (remove active indicator)
			UpdateCounterDisplay();
		}
	}
	else
	{
		// Recipe Mode for factory buildings
		bRecipeModeActive = bPressed;
		bAutoConnectSettingsModeActive = false;  // Ensure auto-connect settings mode is off

		if (bRecipeModeActive)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("🍽️ Recipe Mode: Active (U held on factory)"));

			// Try to acquire lock
			TryAcquireHologramLock();
		}
		else
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("🍽️ Recipe Mode: Inactive (U released)"));

			// Try to release lock
			TryReleaseHologramLock();
		}

		// Delegate to recipe management service (only for factories)
		if (RecipeManagementService)
		{
			RecipeManagementService->OnRecipeModeChanged(Value);
		}
	}
}

void USFSubsystem::AddRecipeToUnlocked(TSubclassOf<UFGRecipe> Recipe)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->AddRecipeToUnlocked(Recipe);
	}
}

TArray<TSubclassOf<UFGRecipe>> USFSubsystem::GetFilteredRecipesForCurrentHologram()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		return RecipeManagementService->GetFilteredRecipesForCurrentHologram();
	}
	return TArray<TSubclassOf<UFGRecipe>>();
}


void USFSubsystem::SetActiveRecipeByIndex(int32 Index)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->SetActiveRecipeByIndex(Index);
	}
}

// ========================================
// Smart Restore Enhanced — New Subsystem Methods
// ========================================

void USFSubsystem::SetAutoConnectRuntimeSettingsFromPreset(const FSFRestoreAutoConnectState& PresetState)
{
	AutoConnectRuntimeSettings.bEnabled = PresetState.bBeltEnabled;
	AutoConnectRuntimeSettings.BeltTierMain = PresetState.BeltTierMain;
	AutoConnectRuntimeSettings.BeltTierToBuilding = PresetState.BeltTierToBuilding;
	AutoConnectRuntimeSettings.bChainDistributors = PresetState.bChainDistributors;
	AutoConnectRuntimeSettings.BeltRoutingMode = PresetState.BeltRoutingMode;
	AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = PresetState.bPipeEnabled;
	AutoConnectRuntimeSettings.PipeTierMain = PresetState.PipeTierMain;
	AutoConnectRuntimeSettings.PipeTierToBuilding = PresetState.PipeTierToBuilding;
	AutoConnectRuntimeSettings.bPipeIndicator = PresetState.bPipeIndicator;
	AutoConnectRuntimeSettings.PipeRoutingMode = PresetState.PipeRoutingMode;
	AutoConnectRuntimeSettings.bConnectPower = PresetState.bPowerEnabled;
	AutoConnectRuntimeSettings.PowerGridAxis = PresetState.PowerGridAxis;
	AutoConnectRuntimeSettings.PowerReserved = PresetState.PowerReserved;
	AutoConnectRuntimeSettings.bInitialized = true;

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
		TEXT("[SmartRestore] Applied auto-connect settings from preset"));
}

bool USFSubsystem::SetBuildGunByRecipeName(const FString& RecipeClassName)
{
	if (RecipeClassName.IsEmpty())
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Find the recipe by class name in available recipes
	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No recipe manager"));
		return false;
	}

	TArray<TSubclassOf<UFGRecipe>> AllRecipes;
	RecipeManager->GetAllAvailableRecipes(AllRecipes);

	TSubclassOf<UFGRecipe> TargetRecipe = nullptr;
	for (const TSubclassOf<UFGRecipe>& Recipe : AllRecipes)
	{
		if (Recipe && Recipe->GetName() == RecipeClassName)
		{
			TargetRecipe = Recipe;
			break;
		}
	}

	if (!TargetRecipe)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: Recipe '%s' not found in available recipes"),
			*RecipeClassName);
		return false;
	}

	// Get the player's build gun and set the recipe
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (!Character)
	{
		return false;
	}

	// The build gun is equipment — get it from the character
	AFGBuildGun* BuildGun = Character->GetBuildGun();
	if (!BuildGun)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No build gun found"));
		return false;
	}

	// Set the active recipe on the build gun's build state
	UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(
		BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
	if (!BuildState)
	{
		SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
			TEXT("[SmartRestore] SetBuildGunByRecipeName: No build gun build state"));
		return false;
	}

	BuildState->SetActiveRecipe(TargetRecipe);

	SF_RESTORE_DIAGNOSTIC_LOG(LogSmartFoundations, Verbose,
		TEXT("[SmartRestore] Switched build gun to recipe '%s'"), *RecipeClassName);
	return true;
}

void USFSubsystem::ApplyRecipeToParentHologram()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ApplyRecipeToParentHologram();
	}
}

void USFSubsystem::CycleRecipeForward(int32 AccumulatedSteps)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->CycleRecipeForward(AccumulatedSteps);
	}
}

void USFSubsystem::CycleRecipeBackward(int32 AccumulatedSteps)
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->CycleRecipeBackward(AccumulatedSteps);
	}
}

void USFSubsystem::ClearAllRecipes()
{
	// Delegate to recipe management service
	if (RecipeManagementService)
	{
		RecipeManagementService->ClearAllRecipes();
	}
}

// ========================================
// Pipe Tier Configuration Helpers
// ========================================

UClass* USFSubsystem::GetPipeClassForTier(int32 Tier, bool bWithIndicator, AFGPlayerController* PlayerController)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 2)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassForTier: Invalid tier %d (must be 1-2)"), Tier);
		return nullptr;
	}

	// Build pipe class path based on tier and style
	FString PipePath;
	if (Tier == 1)
	{
		// Mk1 pipes
		if (bWithIndicator)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C");
		}
		else
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline_NoIndicator.Build_Pipeline_NoIndicator_C");
		}
	}
	else  // Tier == 2
	{
		// Mk2 pipes
		if (bWithIndicator)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk2/Build_PipelineMK2.Build_PipelineMK2_C");
		}
		else
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMK2/Build_PipelineMK2_NoIndicator.Build_PipelineMK2_NoIndicator_C");
		}
	}

	UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);

	if (!PipeClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassForTier: Failed to load pipe class for tier %d (indicator=%d)"),
			Tier, bWithIndicator);
		return nullptr;
	}

	// Check if player has unlocked this pipe tier
	if (PlayerController)
	{
		UWorld* World = PlayerController->GetWorld();
		if (World)
		{
			AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
			if (RecipeManager)
			{
				// Cast to AFGBuildable to check availability
				TSubclassOf<AFGBuildable> PipeBuildableClass = PipeClass;
				if (!RecipeManager->IsBuildingAvailable(PipeBuildableClass))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetPipeClassForTier: Pipe tier Mk%d (%s) not unlocked yet"),
					Tier, bWithIndicator ? TEXT("Normal") : TEXT("Clean"));
					return nullptr;  // Pipe tier not unlocked - prevents cheating
				}
			}
		}
	}

	return PipeClass;
}

TSubclassOf<UFGRecipe> USFSubsystem::GetBeltRecipeForTier(int32 Tier)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 6)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltRecipeForTier: Invalid tier %d (must be 1-6)"), Tier);
		return nullptr;
	}

	// Build belt recipe path based on tier
	FString RecipePath;
	switch (Tier)
	{
		case 1: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk1.Recipe_ConveyorBeltMk1_C"); break;
		case 2: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk2.Recipe_ConveyorBeltMk2_C"); break;
		case 3: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk3.Recipe_ConveyorBeltMk3_C"); break;
		case 4: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk4.Recipe_ConveyorBeltMk4_C"); break;
		case 5: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk5.Recipe_ConveyorBeltMk5_C"); break;
		case 6: RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_ConveyorBeltMk6.Recipe_ConveyorBeltMk6_C"); break;
		default: return nullptr;
	}

	UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipePath);

	if (!RecipeClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetBeltRecipeForTier: Failed to load recipe for Mk%d belt"), Tier);
		return nullptr;
	}

	return TSubclassOf<UFGRecipe>(RecipeClass);
}

TSubclassOf<UFGRecipe> USFSubsystem::GetPipeRecipeForTier(int32 Tier, bool bWithIndicator)
{
	// Clamp to valid range
	if (Tier < 1 || Tier > 2)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeRecipeForTier: Invalid tier %d (must be 1-2)"), Tier);
		return nullptr;
	}

	// Build pipe recipe path based on tier and style
	FString RecipePath;
	if (Tier == 1)
	{
		// Mk1 pipes
		if (bWithIndicator)
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline.Recipe_Pipeline_C");
		}
		else
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_Pipeline_NoIndicator.Recipe_Pipeline_NoIndicator_C");
		}
	}
	else  // Tier == 2
	{
		// Mk2 pipes
		if (bWithIndicator)
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2.Recipe_PipelineMK2_C");
		}
		else
		{
			RecipePath = TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_PipelineMK2_NoIndicator.Recipe_PipelineMK2_NoIndicator_C");
		}
	}

	UClass* RecipeClass = LoadObject<UClass>(nullptr, *RecipePath);

	if (!RecipeClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeRecipeForTier: Failed to load recipe for tier %d (indicator=%d)"),
			Tier, bWithIndicator);
		return nullptr;
	}

	return TSubclassOf<UFGRecipe>(RecipeClass);
}

int32 USFSubsystem::GetHighestUnlockedPipeTier(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedPipeTier: No player controller, defaulting to Mk1"));
		return 1;  // Default to Mk1 if no player context
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedPipeTier: No world context, defaulting to Mk1"));
		return 1;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedPipeTier: No recipe manager, defaulting to Mk1"));
		return 1;
	}

	// Check pipe tiers from highest (Mk2) to lowest (Mk1) to find highest unlocked
	// Check "Normal" variant (with indicators) as the canonical unlock check
	for (int32 Tier = 2; Tier >= 1; Tier--)
	{
		FString PipePath;
		if (Tier == 1)
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline.Build_Pipeline_C");
		}
		else  // Tier == 2
		{
			PipePath = TEXT("/Game/FactoryGame/Buildable/Factory/PipelineMk2/Build_PipelineMK2.Build_PipelineMK2_C");
		}

		UClass* PipeClass = LoadObject<UClass>(nullptr, *PipePath);
		if (PipeClass)
		{
			TSubclassOf<AFGBuildable> PipeBuildableClass = PipeClass;
			if (RecipeManager->IsBuildingAvailable(PipeBuildableClass))
			{
				UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetHighestUnlockedPipeTier: Highest unlocked tier is Mk%d"), Tier);
				return Tier;  // Found highest unlocked tier
			}
		}
	}

	// Fallback: If nothing is unlocked (shouldn't happen), return Mk1
	UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHighestUnlockedPipeTier: No pipes unlocked, defaulting to Mk1"));
	return 1;
}

bool USFSubsystem::AreCleanPipesUnlocked(AFGPlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return false;
	}

	UWorld* World = PlayerController->GetWorld();
	if (!World)
	{
		return false;
	}

	AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World);
	if (!RecipeManager)
	{
		return false;
	}

	// Check if Mk1 clean pipe is unlocked (NoIndicator variant)
	FString CleanPipePath = TEXT("/Game/FactoryGame/Buildable/Factory/Pipeline/Build_Pipeline_NoIndicator.Build_Pipeline_NoIndicator_C");
	UClass* CleanPipeClass = LoadObject<UClass>(nullptr, *CleanPipePath);

	if (CleanPipeClass)
	{
		TSubclassOf<AFGBuildable> CleanPipeBuildableClass = CleanPipeClass;
		if (RecipeManager->IsBuildingAvailable(CleanPipeBuildableClass))
		{
			return true;
		}
	}

	return false;
}

UClass* USFSubsystem::GetPipeClassFromConfig(int32 ConfigTier, bool bWithIndicator, AFGPlayerController* PlayerController)
{
	int32 ActualTier = ConfigTier;

	// Handle "Auto" mode (0 = use highest unlocked)
	if (ConfigTier == 0)
	{
		ActualTier = GetHighestUnlockedPipeTier(PlayerController);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassFromConfig: Auto mode selected highest tier Mk%d"), ActualTier);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassFromConfig: Using configured tier Mk%d"), ActualTier);
	}

	// Get pipe class for the determined tier and style
	UClass* PipeClass = GetPipeClassForTier(ActualTier, bWithIndicator, PlayerController);

	if (!PipeClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetPipeClassFromConfig: Pipe tier Mk%d (%s) unavailable or not unlocked - pipe category disabled"),
			ActualTier, bWithIndicator ? TEXT("Normal") : TEXT("Clean"));
	}

	return PipeClass;
}

UClass* USFSubsystem::GetHypertubeClassFromConfig(AFGPlayerController* PlayerController)
{
	// Hypertubes are a SINGLE buildable - no tier, no indicator/clean variant - so unlike GetPipeClassFromConfig
	// this loads the one class and gates it on the player's unlock (mirrors GetPipeClassForTier's unlock check).
	static const TCHAR* HyperPath = TEXT("/Game/FactoryGame/Buildable/Factory/PipeHyper/Build_PipeHyper.Build_PipeHyper_C");
	// Cache the resolved class: this runs from span-creation paths (per preview update) and the engine
	// buildable never changes; TWeakObjectPtr so a GC of the class just reloads instead of dangling.
	static TWeakObjectPtr<UClass> CachedHyperClass;
	UClass* HyperClass = CachedHyperClass.Get();
	if (!HyperClass)
	{
		HyperClass = LoadObject<UClass>(nullptr, HyperPath);
		CachedHyperClass = HyperClass;
	}
	if (!HyperClass)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("GetHypertubeClassFromConfig: failed to load Build_PipeHyper_C"));
		return nullptr;
	}

	if (PlayerController)
	{
		if (UWorld* World = PlayerController->GetWorld())
		{
			if (AFGRecipeManager* RecipeManager = AFGRecipeManager::Get(World))
			{
				TSubclassOf<AFGBuildable> HyperBuildableClass = HyperClass;
				if (!RecipeManager->IsBuildingAvailable(HyperBuildableClass))
				{
					UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("GetHypertubeClassFromConfig: hypertube not unlocked yet"));
					return nullptr;   // not researched - prevents building an unavailable hypertube
				}
			}
		}
	}

	return HyperClass;
}

void USFSubsystem::CycleAutoConnectSetting()
{
    // Check context (Belt vs Pipe Junction vs Stackable Pipe vs Stackable Belt vs Power)
    bool bIsPipeJunction = false;
    bool bIsStackablePipe = false;
    bool bIsStackableBelt = false;
    bool bIsPowerPole = false;
    bool bIsStackableHypertube = false;
    if (ActiveHologram.IsValid() && AutoConnectService)
    {
        bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get());
        bIsStackablePipe = AutoConnectService->IsPipeSupportHologram(ActiveHologram.Get());
        bIsStackableBelt = USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get());
        bIsPowerPole = AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get());
        bIsStackableHypertube = USFAutoConnectService::IsStackableHypertubeSupportHologram(ActiveHologram.Get());
    }

    if (bIsPowerPole)
    {
        // Power Pole cycle: Enabled -> Reserved -> Grid Axis -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::PowerEnabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerReserved;
            break;
        case EAutoConnectSetting::PowerReserved:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerGridAxis;
            break;
        case EAutoConnectSetting::PowerGridAxis:
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
            break;
        default:
            // If on unrelated setting, jump to first power setting
            CurrentAutoConnectSetting = EAutoConnectSetting::PowerEnabled;
            break;
        }
    }
    else if (bIsStackableBelt)
    {
        // Stackable Conveyor Pole cycle: Enabled -> Tier -> Routing Mode -> Direction -> Enabled
        // NOTE: Uses BeltTierMain for tier, StackableBeltDirection for direction
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::StackableBeltEnabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltTier;
            break;
        case EAutoConnectSetting::StackableBeltTier:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltRoutingMode;
            break;
        case EAutoConnectSetting::BeltRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltDirection;
            break;
        case EAutoConnectSetting::StackableBeltDirection:
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
            break;
        default:
            // If on unrelated setting, jump to first stackable belt setting
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltEnabled;
            break;
        }
    }
    else if (USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
    {
        // Floor Hole Pipe cycle: Enabled -> To Building Tier -> Indicator -> Routing Mode -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierToBuilding;
            break;
        case EAutoConnectSetting::PipeTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else if (bIsStackableHypertube)
    {
        // Stackable Hypertube cycle: Enabled -> Routing Mode -> wrap (independent enable flag).
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::HypertubeEnabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::HypertubeRoutingMode;
            break;
        case EAutoConnectSetting::HypertubeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::HypertubeEnabled;
            break;
        default:
            CurrentAutoConnectSetting = EAutoConnectSetting::HypertubeEnabled;
            break;
        }
    }
    else if (bIsStackablePipe)
    {
        // Stackable Pipe cycle: Enabled -> Main Tier -> Indicator -> Routing Mode -> Enabled
        // NOTE: No TierToBuilding - stackable pipes only connect to each other, not buildings
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
            break;
        case EAutoConnectSetting::PipeTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else if (bIsPipeJunction)
    {
        // Pipe Junction cycle: Enabled -> Main Tier -> Building Tier -> Indicator -> Routing Mode -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
            break;
        case EAutoConnectSetting::PipeTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierToBuilding;
            break;
        case EAutoConnectSetting::PipeTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeIndicator;
            break;
        case EAutoConnectSetting::PipeIndicator:
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeRoutingMode;
            break;
        case EAutoConnectSetting::PipeRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first pipe setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }
    else
    {
        // Belt cycle (Distributor): Enabled -> Main Tier -> Building Tier -> Routing Mode -> Chain -> Enabled
        switch (CurrentAutoConnectSetting)
        {
        case EAutoConnectSetting::Enabled:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltTierMain;
            break;
        case EAutoConnectSetting::BeltTierMain:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltTierToBuilding;
            break;
        case EAutoConnectSetting::BeltTierToBuilding:
            CurrentAutoConnectSetting = EAutoConnectSetting::BeltRoutingMode;
            break;
        case EAutoConnectSetting::BeltRoutingMode:
            CurrentAutoConnectSetting = EAutoConnectSetting::ChainDistributors;
            break;
        case EAutoConnectSetting::ChainDistributors:
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        default:
            // If on unrelated setting, jump to first belt setting
            CurrentAutoConnectSetting = EAutoConnectSetting::Enabled;
            break;
        }
    }

    // Smart Walking: the enable toggle (Auto-Connect ON/OFF) is moot during a walk — the run always lays its own
    // conveyance — so skip it; while walking the cycle only steps through settings that actually shape the run
    // (belt: Tier/Routing/Direction; pipe: Tier/Style/Routing).
    if (USFWalkService* WalkSvc = GetWalkService(); WalkSvc && WalkSvc->IsActive())
    {
        if (CurrentAutoConnectSetting == EAutoConnectSetting::StackableBeltEnabled)
        {
            CurrentAutoConnectSetting = EAutoConnectSetting::StackableBeltTier;
        }
        else if (CurrentAutoConnectSetting == EAutoConnectSetting::Enabled)
        {
            CurrentAutoConnectSetting = EAutoConnectSetting::PipeTierMain;
        }
        else if (CurrentAutoConnectSetting == EAutoConnectSetting::HypertubeEnabled)
        {
            CurrentAutoConnectSetting = EAutoConnectSetting::HypertubeRoutingMode;
        }
    }

    // [AC-HUD #403 diag] Log-level: shows whether the Num0 cycle actually reaches Pipe Routing Mode on a pipe support
    // (and which type branch was taken) vs belt - the static path is symmetric so this isolates the runtime divergence.
    UE_LOG(LogSmartFoundations, Log, TEXT("[AC-HUD] cycled -> %s (pipe=%d belt=%d junction=%d power=%d)"),
        *GetAutoConnectSettingDisplayString(), bIsStackablePipe, bIsStackableBelt, bIsPipeJunction, bIsPowerPole);
    UpdateCounterDisplay();
}

void USFSubsystem::AdjustAutoConnectSetting(int32 Delta)
{
    // Mark settings as modified (initialized) when user makes changes
    AutoConnectRuntimeSettings.bInitialized = true;

    switch (CurrentAutoConnectSetting)
    {
    case EAutoConnectSetting::Enabled:
        // Context-aware toggle: belts on distributor, pipes on junction/stackable supports
        if (ActiveHologram.IsValid() && AutoConnectService)
        {
            if (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()) ||
                AutoConnectService->IsPipeSupportHologram(ActiveHologram.Get()) ||
                USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
            {
                // Pipe hologram (junction, stackable support, or floor hole): toggle pipe auto-connect
                AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = !AutoConnectRuntimeSettings.bPipeAutoConnectEnabled;
            }
            else
            {
                // Distributor: toggle belt auto-connect
                AutoConnectRuntimeSettings.bEnabled = !AutoConnectRuntimeSettings.bEnabled;
            }
        }
        break;

    case EAutoConnectSetting::BeltTierMain:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierMain + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::BeltTierToBuilding:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierToBuilding = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierToBuilding + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::ChainDistributors:
        AutoConnectRuntimeSettings.bChainDistributors = !AutoConnectRuntimeSettings.bChainDistributors;
        break;


    case EAutoConnectSetting::PipeTierMain:
        // Cycle through 0 (Auto), 1 (Mk1), 2 (Mk2)
        AutoConnectRuntimeSettings.PipeTierMain = FMath::Clamp(AutoConnectRuntimeSettings.PipeTierMain + Delta, 0, 2);
        break;

    case EAutoConnectSetting::PipeTierToBuilding:
        // Cycle through 0 (Auto), 1 (Mk1), 2 (Mk2)
        AutoConnectRuntimeSettings.PipeTierToBuilding = FMath::Clamp(AutoConnectRuntimeSettings.PipeTierToBuilding + Delta, 0, 2);
        break;

    case EAutoConnectSetting::PipeIndicator:
        // Toggle between Normal (with indicators) and Clean (no indicators)
        AutoConnectRuntimeSettings.bPipeIndicator = !AutoConnectRuntimeSettings.bPipeIndicator;
        break;

    case EAutoConnectSetting::PipeRoutingMode:
        // Cycle through 0=Auto, 1=Auto2D, 2=Straight, 3=Curve, 4=Noodle, 5=HorizontalToVertical
        AutoConnectRuntimeSettings.PipeRoutingMode = (AutoConnectRuntimeSettings.PipeRoutingMode + Delta) % 6;
        if (AutoConnectRuntimeSettings.PipeRoutingMode < 0)
        {
            AutoConnectRuntimeSettings.PipeRoutingMode += 6;
        }
        break;

    case EAutoConnectSetting::HypertubeEnabled:
        AutoConnectRuntimeSettings.bHypertubeAutoConnectEnabled = !AutoConnectRuntimeSettings.bHypertubeAutoConnectEnabled;
        break;

    case EAutoConnectSetting::HypertubeRoutingMode:
        // Cycle through 0=Auto..5=HorizontalToVertical
        AutoConnectRuntimeSettings.HypertubeRoutingMode = (AutoConnectRuntimeSettings.HypertubeRoutingMode + Delta) % 6;
        if (AutoConnectRuntimeSettings.HypertubeRoutingMode < 0)
        {
            AutoConnectRuntimeSettings.HypertubeRoutingMode += 6;
        }
        break;

    case EAutoConnectSetting::StackableBeltEnabled:
        // Toggle stackable conveyor pole belt auto-connect
        AutoConnectRuntimeSettings.bStackableBeltEnabled = !AutoConnectRuntimeSettings.bStackableBeltEnabled;
        break;

    case EAutoConnectSetting::StackableBeltTier:
        {
            // Cycle through 0 (Auto), 1 (Mk1), ... up to highest unlocked tier - reuses BeltTierMain
            int32 MaxTier = GetHighestUnlockedBeltTier(LastController.Get());
            AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(AutoConnectRuntimeSettings.BeltTierMain + Delta, 0, MaxTier);
        }
        break;

    case EAutoConnectSetting::StackableBeltDirection:
        // Toggle between 0 (Forward) and 1 (Backward)
        AutoConnectRuntimeSettings.StackableBeltDirection = (AutoConnectRuntimeSettings.StackableBeltDirection == 0) ? 1 : 0;
        break;

    case EAutoConnectSetting::PowerEnabled:
        AutoConnectRuntimeSettings.bConnectPower = !AutoConnectRuntimeSettings.bConnectPower;
        break;

    case EAutoConnectSetting::PowerReserved:
        AutoConnectRuntimeSettings.PowerReserved = FMath::Clamp(AutoConnectRuntimeSettings.PowerReserved + Delta, 0, 5);
        break;

    case EAutoConnectSetting::PowerGridAxis:
        // Cycle through 0 (Auto), 1 (X), 2 (Y), 3 (X+Y)
        AutoConnectRuntimeSettings.PowerGridAxis = FMath::Clamp(AutoConnectRuntimeSettings.PowerGridAxis + Delta, 0, 3);
        break;

    case EAutoConnectSetting::BeltRoutingMode:
        // Cycle through 0=Default, 1=Curve, 2=Straight (matches vanilla belt build modes)
        AutoConnectRuntimeSettings.BeltRoutingMode = (AutoConnectRuntimeSettings.BeltRoutingMode + Delta) % 3;
        if (AutoConnectRuntimeSettings.BeltRoutingMode < 0)
        {
            AutoConnectRuntimeSettings.BeltRoutingMode += 3;
        }
        break;
    }

    // [AC-HUD #403 diag] Log-level so a pipe-vs-belt HUD routing test is traceable (the static path is symmetric;
    // this confirms at runtime whether the adjust fires for a pipe support + which value it set).
    UE_LOG(LogSmartFoundations, Log, TEXT("[AC-HUD] adjusted -> %s (beltMode=%d pipeMode=%d)"),
        *GetAutoConnectSettingDisplayString(), AutoConnectRuntimeSettings.BeltRoutingMode, AutoConnectRuntimeSettings.PipeRoutingMode);

    // Trigger immediate re-evaluation of previews with new settings
    if (ActiveHologram.IsValid() && AutoConnectService)
    {
        if (AutoConnectService->IsDistributorHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this distributor
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all belt previews with new settings
                Orchestrator->EvaluateGrid(true);
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated belt previews after settings change"));
            }
        }
        else if (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this pipeline junction
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // [#451] Force recreation of all pipe previews with new settings. HUD SETTINGS
                // change (tier/style/routing via Num0), so pass force=true - the old non-force call
                // claimed "Force recreated" in its log while actually running the in-place path,
                // which is why HUD pipe routing/style changes didn't fully apply.
                Orchestrator->OnPipeGridChanged(/*bForceRecreate=*/true);
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated pipe previews after settings change"));
            }
        }
        else if (AutoConnectService->IsPipeSupportHologram(ActiveHologram.Get()))
        {
            // Stackable pipe supports: trigger re-processing of pipe previews
            AutoConnectService->ProcessStackablePipelineSupports(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, Log, TEXT("[AC-HUD] reprocess -> stackable PIPE supports (pipeMode=%d)"), AutoConnectRuntimeSettings.PipeRoutingMode);
        }
        else if (USFAutoConnectService::IsStackableHypertubeSupportHologram(ActiveHologram.Get()))
        {
            // #405: Stackable hypertube supports — re-process preview tubes on settings change
            AutoConnectService->ProcessStackableHypertubeSupports(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, Log, TEXT("[AC-HUD] reprocess -> stackable HYPERTUBE supports (hyperMode=%d)"), AutoConnectRuntimeSettings.HypertubeRoutingMode);
        }
        else if (USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get()))
        {
            // Floor hole pipes: trigger re-processing of pipe previews
            AutoConnectService->ProcessFloorHolePipes(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Floor Hole Pipe: Force recreated pipe previews after settings change"));
        }
        else if (USFAutoConnectService::IsBeltSupportHologram(ActiveHologram.Get()))
        {
            // Belt support poles (stackable, ceiling, wall): trigger re-processing of belt previews
            AutoConnectService->ProcessStackableConveyorPoles(ActiveHologram.Get());
            UE_LOG(LogSmartFoundations, Log, TEXT("[AC-HUD] reprocess -> belt supports (beltMode=%d)"), AutoConnectRuntimeSettings.BeltRoutingMode);
        }
        else if (AutoConnectService->IsPowerPoleHologram(ActiveHologram.Get()))
        {
            // Get orchestrator for this power pole
            USFAutoConnectOrchestrator* Orchestrator = GetOrCreateOrchestrator(ActiveHologram.Get());
            if (Orchestrator)
            {
                // Force recreation of all power previews with new settings
                Orchestrator->OnPowerGridChanged();
                UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Orchestrator: Force recreated power previews after settings change"));
            }
        }
    }

    // Smart Walking (#356): a walk in progress reads these auto-connect settings (belt routing mode, tier) when it
    // routes its belts — re-route its preview belts immediately so changing routing to Curve/Straight is reflected
    // live, just like auto-connect refreshes its own previews above.
    if (WalkService && WalkService->IsActive())
    {
        // A TIER or pipe-STYLE change alters the conveyance CLASS, so the spans must be RE-CREATED — RerouteSpans
        // only re-routes the existing old-class spans (the live bug: a tier switch via the build HUD left the preview
        // on the old Mk class even though the committed build was correct; the walk PANEL works because it calls
        // RecreateSpans). A routing change is geometry-only, so the lighter reroute still suffices there.
        const bool bClassChanged =
            CurrentAutoConnectSetting == EAutoConnectSetting::BeltTierMain ||
            CurrentAutoConnectSetting == EAutoConnectSetting::StackableBeltTier ||
            CurrentAutoConnectSetting == EAutoConnectSetting::PipeTierMain ||
            CurrentAutoConnectSetting == EAutoConnectSetting::PipeIndicator;
        if (bClassChanged) { WalkService->RecreateSpans(); } else { WalkService->RerouteSpans(); }
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Walk: %s spans after auto-connect setting change"), bClassChanged ? TEXT("re-created") : TEXT("re-routed"));
    }

    UpdateCounterDisplay();
    AutoConnectRuntimeSettings.bInitialized = true;
}

FString USFSubsystem::GetAutoConnectSettingDisplayString() const
{
    FString SettingName;
    FString SettingValue;

    switch (CurrentAutoConnectSetting)
    {
    case EAutoConnectSetting::Enabled:
        // Context-aware display: show belt or pipe auto-connect status
        if (ActiveHologram.IsValid() && AutoConnectService &&
            (AutoConnectService->IsPipelineJunctionHologram(ActiveHologram.Get()) ||
             AutoConnectService->IsPipeSupportHologram(ActiveHologram.Get()) ||
             USFAutoConnectService::IsPassthroughPipeHologram(ActiveHologram.Get())))
        {
            SettingName = TEXT("Pipe Auto-Connect");
            SettingValue = AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF");
        }
        else
        {
            SettingName = TEXT("Belt Auto-Connect");
            SettingValue = AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF");
        }
        break;

    case EAutoConnectSetting::BeltTierMain:
        SettingName = TEXT("Belt Tier (Main)");
        if (AutoConnectRuntimeSettings.BeltTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
        }
        break;

    case EAutoConnectSetting::BeltTierToBuilding:
        SettingName = TEXT("Belt Tier (To Building)");
        if (AutoConnectRuntimeSettings.BeltTierToBuilding == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
        }
        break;

    case EAutoConnectSetting::ChainDistributors:
        SettingName = TEXT("Chain Distributors");
        SettingValue = AutoConnectRuntimeSettings.bChainDistributors ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::BeltRoutingMode:
        SettingName = TEXT("Belt Routing");
        switch (AutoConnectRuntimeSettings.BeltRoutingMode)
        {
        case 0:
            SettingValue = TEXT("Default");
            break;
        case 1:
            SettingValue = TEXT("Curve");
            break;
        case 2:
            SettingValue = TEXT("Straight");
            break;
        default:
            SettingValue = TEXT("Default");
            break;
        }
        break;

    case EAutoConnectSetting::PipeTierMain:
        SettingName = TEXT("Pipe Tier (Main)");
        if (AutoConnectRuntimeSettings.PipeTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierMain);
        }
        break;

    case EAutoConnectSetting::PipeTierToBuilding:
        SettingName = TEXT("Pipe Tier (To Building)");
        if (AutoConnectRuntimeSettings.PipeTierToBuilding == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
        }
        break;

    case EAutoConnectSetting::PipeIndicator:
        SettingName = TEXT("Pipe Style");
        SettingValue = AutoConnectRuntimeSettings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean");
        break;

    case EAutoConnectSetting::PipeRoutingMode:
        SettingName = TEXT("Pipe Routing");
        switch (AutoConnectRuntimeSettings.PipeRoutingMode)
        {
        case 0:
            SettingValue = TEXT("Auto");
            break;
        case 1:
            SettingValue = TEXT("Auto 2D");
            break;
        case 2:
            SettingValue = TEXT("Straight");
            break;
        case 3:
            SettingValue = TEXT("Curve");
            break;
        case 4:
            SettingValue = TEXT("Noodle");
            break;
        case 5:
            SettingValue = TEXT("Horiz→Vert");
            break;
        default:
            SettingValue = TEXT("Auto");
            break;
        }
        break;

    case EAutoConnectSetting::HypertubeEnabled:
        SettingName = TEXT("Hypertube AC");
        SettingValue = AutoConnectRuntimeSettings.bHypertubeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::HypertubeRoutingMode:
        SettingName = TEXT("Hypertube Routing");
        switch (AutoConnectRuntimeSettings.HypertubeRoutingMode)
        {
        case 0: SettingValue = TEXT("Auto"); break;
        case 1: SettingValue = TEXT("Auto 2D"); break;
        case 2: SettingValue = TEXT("Straight"); break;
        case 3: SettingValue = TEXT("Curve"); break;
        case 4: SettingValue = TEXT("Noodle"); break;
        case 5: SettingValue = TEXT("Horiz→Vert"); break;
        default: SettingValue = TEXT("Auto"); break;
        }
        break;

    case EAutoConnectSetting::StackableBeltEnabled:
        SettingName = TEXT("Stackable Belt Auto-Connect");
        SettingValue = AutoConnectRuntimeSettings.bStackableBeltEnabled ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::StackableBeltTier:
        SettingName = TEXT("Stackable Belt Tier");
        if (AutoConnectRuntimeSettings.BeltTierMain == 0)
        {
            SettingValue = TEXT("Auto");
        }
        else
        {
            SettingValue = FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
        }
        break;

    case EAutoConnectSetting::StackableBeltDirection:
        SettingName = TEXT("Stackable Belt Direction");
        SettingValue = AutoConnectRuntimeSettings.StackableBeltDirection == 0 ? TEXT("Forward") : TEXT("Backward");
        break;

    case EAutoConnectSetting::PowerEnabled:
        SettingName = TEXT("Power Auto-Connect");
        SettingValue = AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF");
        break;

    case EAutoConnectSetting::PowerGridAxis:
        SettingName = TEXT("Grid Axis");
        switch (AutoConnectRuntimeSettings.PowerGridAxis)
        {
        case 0:
            SettingValue = TEXT("Auto");
            break;
        case 1:
            SettingValue = TEXT("X");
            break;
        case 2:
            SettingValue = TEXT("Y");
            break;
        case 3:
            SettingValue = TEXT("X+Y");
            break;
        default:
            SettingValue = TEXT("Auto");
            break;
        }
        break;

    case EAutoConnectSetting::PowerReserved:
        SettingName = TEXT("Reserved Slots");
        SettingValue = FString::Printf(TEXT("%d"), AutoConnectRuntimeSettings.PowerReserved);
        break;
    }

    return FString::Printf(TEXT("%s: %s"), *SettingName, *SettingValue);
}

bool USFSubsystem::IsCurrentHologramAutoConnectCapable() const
{
	if (!ActiveHologram.IsValid() || !AutoConnectService)
	{
		return false;
	}

	AFGHologram* Hologram = ActiveHologram.Get();
	return AutoConnectService->IsDistributorHologram(Hologram) ||
	       AutoConnectService->IsPipelineJunctionHologram(Hologram) ||
	       AutoConnectService->IsPowerPoleHologram(Hologram) ||
	       USFAutoConnectService::IsBeltSupportHologram(Hologram) ||
	       AutoConnectService->IsPipeSupportHologram(Hologram) ||   // #403: pipe supports (stackable/regular/wall) were
	                                                                //   missing here, so the HUD suppressed the whole
	                                                                //   auto-connect settings section for them (belt worked)
	       USFAutoConnectService::IsPassthroughPipeHologram(Hologram) ||
	       USFAutoConnectService::IsStackableHypertubeSupportHologram(Hologram);  // #405: hypertube supports were
	                                                                              //   missing here too, so the HUD
	                                                                              //   suppressed the settings overlay
}

bool USFSubsystem::IsCurrentHologramWalkable() const
{
	// Smart Walking seeds from a stackable belt, pipe, OR hypertube support — matches the walk's conveyance adapters
	// (belt/pipe/hypertube). The hypertube support keys off its own build class, not IsStackableSupportHologram, so OR
	// it in explicitly — else the Walk Path button never appears on a held tube pole. #405
	return AutoConnectService && ActiveHologram.IsValid()
		&& (AutoConnectService->IsStackableSupportHologram(ActiveHologram.Get())
			|| USFAutoConnectService::IsStackableHypertubeSupportHologram(ActiveHologram.Get()));
}

TArray<FString> USFSubsystem::GetDirtyAutoConnectSettings() const
{
	TArray<FString> DirtySettings;

	if (!bConfigLoaded)
	{
		return DirtySettings; // No config to compare against
	}

	// Determine hologram type to filter relevant settings
	bool bIsPipeJunction = false;
	bool bIsDistributor = false;
	bool bIsPowerPole = false;
	bool bIsStackableSupport = false;   // belt supports (stackable/ceiling/wall/regular conveyor pole)
	bool bIsPipeSupport = false;        // #403: pipe supports (stackable/regular/wall) → show PIPE settings
	bool bIsPassthroughPipe = false;
	bool bIsStackableHypertube = false; // #405: hypertube supports → show HYPERTUBE settings
	if (ActiveHologram.IsValid() && AutoConnectService)
	{
		AFGHologram* Hologram = ActiveHologram.Get();
		bIsPipeJunction = AutoConnectService->IsPipelineJunctionHologram(Hologram);
		bIsDistributor = AutoConnectService->IsDistributorHologram(Hologram);
		bIsPowerPole = AutoConnectService->IsPowerPoleHologram(Hologram);
		bIsStackableSupport = USFAutoConnectService::IsBeltSupportHologram(Hologram);
		bIsPipeSupport = AutoConnectService->IsPipeSupportHologram(Hologram);
		bIsPassthroughPipe = USFAutoConnectService::IsPassthroughPipeHologram(Hologram);
		bIsStackableHypertube = USFAutoConnectService::IsStackableHypertubeSupportHologram(Hologram);
	}

	// CRITICAL FIX: Only show settings relevant to current hologram type
	// Pipe junctions should only show pipe settings, distributors should only show belt settings

	if (bIsPipeJunction || bIsPassthroughPipe || bIsPipeSupport)
	{
		// Pipe junction, floor hole passthrough, or pipe support (#403): Show pipe-related settings
		if (AutoConnectRuntimeSettings.bPipeAutoConnectEnabled != CachedConfig.bPipeAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Pipe Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.PipeTierMain != CachedConfig.PipeLevelMain)
		{
			const FString TierText = (AutoConnectRuntimeSettings.PipeTierMain == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierMain);
			DirtySettings.Add(FString::Printf(TEXT("Pipe Tier (Main): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.PipeTierToBuilding != CachedConfig.PipeLevelToBuilding)
		{
			const FString TierText = (AutoConnectRuntimeSettings.PipeTierToBuilding == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
			DirtySettings.Add(FString::Printf(TEXT("Pipe Tier (To Building): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.bPipeIndicator != CachedConfig.PipeIndicator)
		{
			DirtySettings.Add(FString::Printf(TEXT("Pipe Style: %s"),
				AutoConnectRuntimeSettings.bPipeIndicator ? TEXT("Normal") : TEXT("Clean")));
		}

		// A non-default routing mode persists on the HUD like every other deviating setting.
		// Strings MUST match GetAutoConnectSettingDisplayString (case PipeRoutingMode) so the
		// active-setting '*' highlight lines up when cycling with Num0.
		if (AutoConnectRuntimeSettings.PipeRoutingMode != CachedConfig.PipeRoutingMode)
		{
			FString RouteText;
			switch (AutoConnectRuntimeSettings.PipeRoutingMode)
			{
			case 0: RouteText = TEXT("Auto"); break;
			case 1: RouteText = TEXT("Auto 2D"); break;
			case 2: RouteText = TEXT("Straight"); break;
			case 3: RouteText = TEXT("Curve"); break;
			case 4: RouteText = TEXT("Noodle"); break;
			case 5: RouteText = TEXT("Horiz→Vert"); break;
			default: RouteText = TEXT("Auto"); break;
			}
			DirtySettings.Add(FString::Printf(TEXT("Pipe Routing: %s"), *RouteText));
		}
	}
	else if (bIsDistributor || bIsStackableSupport)
	{
		// Distributor: Show only belt-related settings
		if (AutoConnectRuntimeSettings.bEnabled != CachedConfig.bAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Belt Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.BeltTierMain != CachedConfig.BeltLevelMain)
		{
			const FString TierText = (AutoConnectRuntimeSettings.BeltTierMain == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierMain);
			DirtySettings.Add(FString::Printf(TEXT("Belt Tier (Main): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.BeltTierToBuilding != CachedConfig.BeltLevelToBuilding)
		{
			const FString TierText = (AutoConnectRuntimeSettings.BeltTierToBuilding == 0) ?
				TEXT("Auto") : FString::Printf(TEXT("Mk%d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
			DirtySettings.Add(FString::Printf(TEXT("Belt Tier (To Building): %s"), *TierText));
		}

		if (AutoConnectRuntimeSettings.bChainDistributors != CachedConfig.bAutoConnectDistributors)
		{
			DirtySettings.Add(FString::Printf(TEXT("Chain Distributors: %s"),
				AutoConnectRuntimeSettings.bChainDistributors ? TEXT("ON") : TEXT("OFF")));
		}

		// A non-default routing mode persists on the HUD like every other deviating setting.
		// Strings MUST match GetAutoConnectSettingDisplayString (case BeltRoutingMode).
		if (AutoConnectRuntimeSettings.BeltRoutingMode != CachedConfig.BeltRoutingMode)
		{
			FString RouteText;
			switch (AutoConnectRuntimeSettings.BeltRoutingMode)
			{
			case 0: RouteText = TEXT("Default"); break;
			case 1: RouteText = TEXT("Curve"); break;
			case 2: RouteText = TEXT("Straight"); break;
			default: RouteText = TEXT("Default"); break;
			}
			DirtySettings.Add(FString::Printf(TEXT("Belt Routing: %s"), *RouteText));
		}
	}
	else if (bIsPowerPole)
	{
		// Power pole: Show only power-related settings
		if (AutoConnectRuntimeSettings.bConnectPower != CachedConfig.bPowerAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Power Auto-Connect: %s"),
				AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.PowerReserved != CachedConfig.PowerConnectReserved)
		{
			DirtySettings.Add(FString::Printf(TEXT("Reserved Slots: %d"), AutoConnectRuntimeSettings.PowerReserved));
		}

		if (AutoConnectRuntimeSettings.PowerGridAxis != CachedConfig.PowerConnectMode)
		{
			FString AxisText;
			switch (AutoConnectRuntimeSettings.PowerGridAxis)
			{
			case 0: AxisText = TEXT("Auto"); break;
			case 1: AxisText = TEXT("X"); break;
			case 2: AxisText = TEXT("Y"); break;
			case 3: AxisText = TEXT("X+Y"); break;
			default: AxisText = TEXT("Auto"); break;
			}
			DirtySettings.Add(FString::Printf(TEXT("Grid Axis: %s"), *AxisText));
		}

		// Building range is config-only, but we can show if runtime value differs
		// Convert both to meters for comparison (runtime is in cm, config is in meters)
		int32 RuntimeRangeMeters = FMath::RoundToInt(AutoConnectRuntimeSettings.PowerBuildingRange / 100.0f);
		if (RuntimeRangeMeters != CachedConfig.PowerConnectRange)
		{
			DirtySettings.Add(FString::Printf(TEXT("Building Range: %dm"), RuntimeRangeMeters));
		}
	}
	else if (bIsStackableHypertube)
	{
		// Hypertube supports (#405): show only hypertube settings (enable + routing).
		// Strings MUST match GetAutoConnectSettingDisplayString so the active-setting highlight lines up.
		if (AutoConnectRuntimeSettings.bHypertubeAutoConnectEnabled != CachedConfig.bHypertubeAutoConnectEnabled)
		{
			DirtySettings.Add(FString::Printf(TEXT("Hypertube AC: %s"),
				AutoConnectRuntimeSettings.bHypertubeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF")));
		}

		if (AutoConnectRuntimeSettings.HypertubeRoutingMode != CachedConfig.HypertubeRoutingMode)
		{
			FString RouteText;
			switch (AutoConnectRuntimeSettings.HypertubeRoutingMode)
			{
			case 0: RouteText = TEXT("Auto"); break;
			case 1: RouteText = TEXT("Auto 2D"); break;
			case 2: RouteText = TEXT("Straight"); break;
			case 3: RouteText = TEXT("Curve"); break;
			case 4: RouteText = TEXT("Noodle"); break;
			case 5: RouteText = TEXT("Horiz→Vert"); break;
			default: RouteText = TEXT("Auto"); break;
			}
			DirtySettings.Add(FString::Printf(TEXT("Hypertube Routing: %s"), *RouteText));
		}
	}

	return DirtySettings;
}

void USFSubsystem::ResetAutoConnectRuntimeSettings()
{
	// [#371] Called when the active hologram INSTANCE changes - i.e. on equip/recipe change and on the
	// fresh hologram the build gun spawns AFTER a construct, but NOT while aiming (the same instance is
	// just repositioned; see RegisterActiveHologram's CurrentHologram != ActiveHologram guard). Smart
	// Panel / hotkey edits are a TEMPORARY one-off override of the global-config defaults for a SINGLE
	// build: they survive aiming the current placement (the just-built building's auto-connect reads
	// them before the next hologram registers), but once that placement is built we re-read the global
	// config here so the override does NOT carry to the next build. Per the maintainer's decision on
	// issue #371, the Panel is a per-build override only - anything that should persist between builds
	// belongs in the global settings (Pause -> Mods -> Smart!), which already exist for that purpose.
	FSmart_ConfigStruct FreshConfig = FSmart_ConfigStruct::GetActiveConfig(this);
	AutoConnectRuntimeSettings.InitFromConfig(FreshConfig);  // clears bInitialized -> back to config-tracking
	CachedConfig = FreshConfig;                              // new baseline for future change detection

	// Issue #257: Refresh Extend enabled state from fresh config (independent of the override above)
	bExtendEnabledByConfig = FreshConfig.bExtendEnabled;

	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT(" Auto-Connect runtime settings reset from config:"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Belt Auto-Connect: %s"), AutoConnectRuntimeSettings.bEnabled ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Pipe Auto-Connect: %s"), AutoConnectRuntimeSettings.bPipeAutoConnectEnabled ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Power Auto-Connect: %s"), AutoConnectRuntimeSettings.bConnectPower ? TEXT("ON") : TEXT("OFF"));
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Power Range: %.0fm"), AutoConnectRuntimeSettings.PowerBuildingRange / 100.0f);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("   Extend Enabled: %s"), bExtendEnabledByConfig ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::ResetSmartDisableFlag()
{
	if (bDisableSmartForNextAction || bExtendDisabledForSession)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("▶️ Smart! re-enabled (Auto-Connect + Extend flags reset)"));
	}
	bDisableSmartForNextAction = false;
	bExtendDisabledForSession = false;  // Issue #257: reset Extend session flag too
	LastCycleAxisTapTime = 0.0;
}

// ========================================
// Belt Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectBeltEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bEnabled = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectBeltTierMain(int32 Tier)
{
	AutoConnectRuntimeSettings.BeltTierMain = FMath::Clamp(Tier, 0, 6);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt tier main set to: %d"), AutoConnectRuntimeSettings.BeltTierMain);
}

void USFSubsystem::SetAutoConnectBeltTierToBuilding(int32 Tier)
{
	AutoConnectRuntimeSettings.BeltTierToBuilding = FMath::Clamp(Tier, 0, 6);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt tier to building set to: %d"), AutoConnectRuntimeSettings.BeltTierToBuilding);
}

void USFSubsystem::SetAutoConnectBeltChain(bool bEnabled)
{
	AutoConnectRuntimeSettings.bChainDistributors = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt chain distributors set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectStackableBeltDirection(int32 Direction)
{
	AutoConnectRuntimeSettings.StackableBeltDirection = FMath::Clamp(Direction, 0, 1);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Stackable belt direction set to: %d (%s)"),
		AutoConnectRuntimeSettings.StackableBeltDirection,
		AutoConnectRuntimeSettings.StackableBeltDirection == 0 ? TEXT("Forward") : TEXT("Backward"));
}

void USFSubsystem::SetAutoConnectBeltRoutingMode(int32 Mode)
{
	AutoConnectRuntimeSettings.BeltRoutingMode = FMath::Clamp(Mode, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString ModeName;
	switch (AutoConnectRuntimeSettings.BeltRoutingMode)
	{
	case 0: ModeName = TEXT("Default"); break;
	case 1: ModeName = TEXT("Curve"); break;
	case 2: ModeName = TEXT("Straight"); break;
	default: ModeName = TEXT("Default"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Belt routing mode set to: %d (%s)"), AutoConnectRuntimeSettings.BeltRoutingMode, *ModeName);
}

// ========================================
// Pipe Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectPipeEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bPipeAutoConnectEnabled = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectPipeTierMain(int32 Tier)
{
	AutoConnectRuntimeSettings.PipeTierMain = FMath::Clamp(Tier, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe tier main set to: %d"), AutoConnectRuntimeSettings.PipeTierMain);
}

void USFSubsystem::SetAutoConnectPipeTierToBuilding(int32 Tier)
{
	AutoConnectRuntimeSettings.PipeTierToBuilding = FMath::Clamp(Tier, 0, 2);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe tier to building set to: %d"), AutoConnectRuntimeSettings.PipeTierToBuilding);
}

void USFSubsystem::SetAutoConnectPipeIndicator(bool bIndicator)
{
	AutoConnectRuntimeSettings.bPipeIndicator = bIndicator;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe indicator style set to: %s"), bIndicator ? TEXT("Normal") : TEXT("Clean"));
}

void USFSubsystem::SetAutoConnectPipeRoutingMode(int32 Mode)
{
	AutoConnectRuntimeSettings.PipeRoutingMode = FMath::Clamp(Mode, 0, 5);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString ModeName;
	switch (AutoConnectRuntimeSettings.PipeRoutingMode)
	{
	case 0: ModeName = TEXT("Auto"); break;
	case 1: ModeName = TEXT("Auto 2D"); break;
	case 2: ModeName = TEXT("Straight"); break;
	case 3: ModeName = TEXT("Curve"); break;
	case 4: ModeName = TEXT("Noodle"); break;
	case 5: ModeName = TEXT("Horiz→Vert"); break;
	default: ModeName = TEXT("Auto"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Pipe routing mode set to: %s (%d)"), *ModeName, AutoConnectRuntimeSettings.PipeRoutingMode);
}

void USFSubsystem::SetAutoConnectHypertubeEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bHypertubeAutoConnectEnabled = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Hypertube auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectHypertubeRoutingMode(int32 Mode)
{
	AutoConnectRuntimeSettings.HypertubeRoutingMode = FMath::Clamp(Mode, 0, 5);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString ModeName;
	switch (AutoConnectRuntimeSettings.HypertubeRoutingMode)
	{
	case 0: ModeName = TEXT("Auto"); break;
	case 1: ModeName = TEXT("Auto 2D"); break;
	case 2: ModeName = TEXT("Straight"); break;
	case 3: ModeName = TEXT("Curve"); break;
	case 4: ModeName = TEXT("Noodle"); break;
	case 5: ModeName = TEXT("Horiz→Vert"); break;
	default: ModeName = TEXT("Auto"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Hypertube routing mode set to: %s (%d)"), *ModeName, AutoConnectRuntimeSettings.HypertubeRoutingMode);
}

// ========================================
// Power Auto-Connect Setters (for Settings Form)
// ========================================

void USFSubsystem::SetAutoConnectPowerEnabled(bool bEnabled)
{
	AutoConnectRuntimeSettings.bConnectPower = bEnabled;
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power auto-connect enabled set to: %s"), bEnabled ? TEXT("ON") : TEXT("OFF"));
}

void USFSubsystem::SetAutoConnectPowerGridAxis(int32 Axis)
{
	AutoConnectRuntimeSettings.PowerGridAxis = FMath::Clamp(Axis, 0, 3);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();

	FString AxisName;
	switch (AutoConnectRuntimeSettings.PowerGridAxis)
	{
	case 0: AxisName = TEXT("Auto"); break;
	case 1: AxisName = TEXT("X"); break;
	case 2: AxisName = TEXT("Y"); break;
	case 3: AxisName = TEXT("X+Y"); break;
	default: AxisName = TEXT("Auto"); break;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power grid axis set to: %d (%s)"), AutoConnectRuntimeSettings.PowerGridAxis, *AxisName);
}

void USFSubsystem::SetAutoConnectPowerReserved(int32 Reserved)
{
	AutoConnectRuntimeSettings.PowerReserved = FMath::Clamp(Reserved, 0, 5);
	AutoConnectRuntimeSettings.bInitialized = true;
	UpdateCounterDisplay();
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Power reserved slots set to: %d"), AutoConnectRuntimeSettings.PowerReserved);
}

void USFSubsystem::TriggerAutoConnectRefresh()
{
	// Trigger auto-connect preview refresh by notifying all active orchestrators
	for (auto& Pair : AutoConnectOrchestrators)
	{
		if (Pair.Value)
		{
			Pair.Value->ForceRefresh();
		}
	}

	if (AutoConnectOrchestrators.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("Auto-connect refresh triggered from settings change (%d orchestrators)"), AutoConnectOrchestrators.Num());
	}
}
