#include "Services/SFHudService.h"
// NOTE: SFDeferredCostService removed - child holograms automatically aggregate costs via GetCost()
#include "Subsystem/SFSubsystem.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Restore/SFRestoreService.h"
#include "SmartFoundations.h"
#include "HUD/SFHUDTypes.h"
#include "HUD/SFHudWidget.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Hologram/FGHologram.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/HUD.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "Engine/Texture2D.h"
#include "Blueprint/UserWidget.h"
#include "Config/Smart_ConfigStruct.h"
#include "FGItemDescriptor.h"
#include "ItemAmount.h"
#include "FGCentralStorageSubsystem.h"
#include "FGPlayerController.h"
#include "FGCharacterPlayer.h"
#include "FGInventoryComponent.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

void USFHudService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
}

void USFHudService::UpdateWidgetDisplay(const FString& FirstLine, const FString& SecondLine)
{
    // Ensure HUD binding is active (lazy init)
    EnsureHUDBinding();

    // Store text for HUD canvas drawing
    if (!FirstLine.IsEmpty() || !SecondLine.IsEmpty())
    {
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[Counter Display] Line1: '%s' Line2: '%s'"), *FirstLine, *SecondLine);
        CurrentCounterText = FirstLine;
        if (!SecondLine.IsEmpty())
        {
            CurrentCounterText += FString::Printf(TEXT("\n%s"), *SecondLine);
        }

        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("HUD(UpdateWidgetDisplay): '%s'"), *CurrentCounterText.Replace(TEXT("\n"), TEXT(" | ")));
    }
    else
    {
        CurrentCounterText.Empty();
    }
}

void USFHudService::DrawCounterToHUD(AHUD* HUD, UCanvas* Canvas)
{
    if (!Subsystem.IsValid()) return;

    // Pull config every draw (keeps code simple and up to date)
    FSmart_ConfigStruct CurrentConfig = FSmart_ConfigStruct::GetActiveConfig(Subsystem.Get());
    HUDScaleMultiplier = CurrentConfig.HUDScale;
    CachedConfig.bShowHUD = CurrentConfig.bShowHUD;

    // Determine if the widget should be visible
    const bool bShouldShow = CachedConfig.bShowHUD
        && !bHUDSuppressed
        && Subsystem->GetActiveHologram() != nullptr;

    // Ensure widget exists
    if (!HudWidget)
    {
        CreateHudWidget();
    }

    if (!HudWidget) return;

    if (!bShouldShow)
    {
        if (HudWidget->IsInViewport())
        {
            HudWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    // Show widget if hidden
    if (!HudWidget->IsInViewport())
    {
        HudWidget->AddToViewport(0);
    }
    HudWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // Configure scale and theme (only applies if changed)
    HudWidget->SetHudScale(HUDScaleMultiplier);
    HudWidget->SetTheme(CurrentConfig.HUDTheme);

    // Position via viewport coordinates
    const float ClampedPosX = FMath::Clamp(CurrentConfig.HUDPositionX, 0.0f, 0.85f);
    const float ClampedPosY = FMath::Clamp(CurrentConfig.HUDPositionY, 0.0f, 0.85f);
    FVector2D ViewportSize;
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->GetViewportSize(ViewportSize);
    }
    HudWidget->SetPositionInViewport(FVector2D(ViewportSize.X * ClampedPosX, ViewportSize.Y * ClampedPosY));

    // Rebuild display lines every frame to capture dynamic values (lift height, etc.)
    TPair<FString, FString> DisplayLines = BuildCounterDisplayLines();
    CurrentCounterText = DisplayLines.Key;
    if (!DisplayLines.Value.IsEmpty())
    {
        CurrentCounterText += FString::Printf(TEXT("\n%s"), *DisplayLines.Value);
    }

    if (CurrentCounterText.IsEmpty())
    {
        HudWidget->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // Parse multi-line text
    TArray<FString> Lines;
    CurrentCounterText.ParseIntoArray(Lines, TEXT("\n"), true);

    // Get recipe icon if applicable
    UTexture2D* RecipeIcon = nullptr;
    TSubclassOf<UFGRecipe> ActiveRecipe = Subsystem->GetActiveRecipe();
    if (ActiveRecipe)
    {
        RecipeIcon = Subsystem->GetRecipePrimaryProductIcon(ActiveRecipe);
    }

    // Update the UMG widget with current content
    HudWidget->UpdateContent(Lines, RecipeIcon);
}

// ========================================================================
// UMG WIDGET LIFECYCLE
// ========================================================================

void USFHudService::CreateHudWidget()
{
    if (HudWidget) return;
    if (!Subsystem.IsValid()) return;

    UWorld* World = Subsystem->GetWorld();
    if (!World) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC) return;

    HudWidget = CreateWidget<USFHudWidget>(PC);
    if (HudWidget)
    {
        HudWidget->SetVisibility(ESlateVisibility::Collapsed);
        UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFHudService: Created UMG HUD widget"));
    }
}

void USFHudService::DestroyHudWidget()
{
    if (HudWidget)
    {
        if (HudWidget->IsInViewport())
        {
            HudWidget->RemoveFromParent();
        }
        HudWidget = nullptr;
        UE_LOG(LogSmartFoundations, Log, TEXT("SFHudService: Destroyed UMG HUD widget"));
    }
}

void USFHudService::InitializeWidgets()
{
	// HUD binding will happen lazily when first needed
	if (Subsystem.IsValid())
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("USFHudService: InitializeWidgets - HUD will bind on first use"));
	}
}

void USFHudService::EnsureHUDBinding()
{
	if (!Subsystem.IsValid()) return;

	// Already bound?
	if (CachedHUD.IsValid())
	{
		return;
	}

	// Try to bind to HUD's PostRender event for direct canvas drawing
	if (UWorld* World = Subsystem->GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (AHUD* HUD = PC->GetHUD())
			{
				CachedHUD = HUD;
				HUD->OnHUDPostRender.AddUObject(this, &USFHudService::DrawCounterToHUD);
				UE_LOG(LogSmartFoundations, Verbose, TEXT("USFHudService: ✅ Bound USFHudService::DrawCounterToHUD to HUD PostRender"));
			}
			else
			{
				UE_LOG(LogSmartFoundations, Warning, TEXT("USFHudService: ❌ PC->GetHUD() returned nullptr"));
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning, TEXT("USFHudService: ❌ GetPlayerController returned nullptr"));
		}
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("USFHudService: GetWorld() returned nullptr"));
	}
}

void USFHudService::CleanupWidgets()
{
	DestroyHudWidget();

	if (AHUD* HUD = CachedHUD.Get())
	{
		HUD->OnHUDPostRender.RemoveAll(this);
		UE_LOG(LogSmartFoundations, Verbose, TEXT("USFHudService: Unbound USFHudService::DrawCounterToHUD from HUD"));
	}
	CachedHUD.Reset();
}

TPair<FString, FString> USFHudService::BuildCounterDisplayLines() const
{
	if (!Subsystem.IsValid())
	{
		return TPair<FString, FString>(FString(), FString());
	}

	const FSFCounterState& State = Subsystem->GetCounterState();

	TArray<FString> Lines;

	// Header
	Lines.Add(LOCTEXT("HUD_Header", "SMART!").ToString());

	// Issue #160: Zoop warning display
	if (FSFHologramHelperService* Helper = Subsystem->GetHologramHelper())
	{
		if (Helper->IsZoopActive())
		{
			Lines.Add(LOCTEXT("HUD_ZoopActive", "*Zoop Active - Scaling Disabled").ToString());
		}
	}

	// Issue #198 + #257: Smart disable warning display
	if (Subsystem->IsSmartDisabledForCurrentAction() || Subsystem->IsExtendDisabled())
	{
		if (Subsystem->IsSmartDisabledForCurrentAction() && Subsystem->IsExtendDisabled())
		{
			Lines.Add(LOCTEXT("HUD_SmartDisabled", "*Smart Disabled (double-tap to re-enable)").ToString());
		}
		else if (Subsystem->IsSmartDisabledForCurrentAction())
		{
			Lines.Add(LOCTEXT("HUD_AutoConnectDisabled", "*Auto-Connect Disabled (double-tap to re-enable)").ToString());
		}
		else
		{
			Lines.Add(LOCTEXT("HUD_ExtendDisabled", "*Extend Disabled").ToString());
		}
	}

	if (Subsystem->IsRestoredExtendModeActive())
	{
		USFRestoreService* RestoreSvc = Subsystem->GetRestoreService();
		const FString PresetName = (RestoreSvc && RestoreSvc->IsRestoreSessionActive())
			? RestoreSvc->GetActiveRestorePresetName()
			: FString();
		Lines.Add(PresetName.IsEmpty()
			? LOCTEXT("HUD_RestoreActive", "*Restore Active").ToString()
			: FText::Format(LOCTEXT("HUD_RestorePreset", "*Restore: {0}"), FText::FromString(PresetName)).ToString());
	}

	// Lift height display (for conveyor lifts and pipe lifts)
	if (CachedLiftHeight != 0.0f)
	{
		const float AbsHeight = FMath::Abs(CachedLiftHeight) / 100.0f;
		const float WorldHeightMeters = CachedWorldHeight / 100.0f;
		Lines.Add(FText::Format(LOCTEXT("HUD_LiftHeight", "Lift: \u2195 {0}m  \u21D3 {1}m"),
			FText::FromString(FString::Printf(TEXT("%.1f"), AbsHeight)),
			FText::FromString(FString::Printf(TEXT("%.1f"), WorldHeightMeters))).ToString());
	}

	// Scaled Extend indicator (Issue #265)
	if (Subsystem->IsExtendModeActive())
	{
		USFExtendService* ExtendSvc = Subsystem->GetExtendService();
		if (ExtendSvc)
		{
			int32 CloneCount = ExtendSvc->GetExtendCloneCount();
			int32 RowCount = ExtendSvc->GetExtendRowCount();

			if (CloneCount > 0 || RowCount > 1)
			{
				// Show Scaled Extend mode with clone count
				FString ExtendLine = FText::Format(LOCTEXT("HUD_ExtendScaled", "*Extend: {0}x{1}"),
					FText::AsNumber(CloneCount + 1), FText::AsNumber(RowCount)).ToString();
				if (!ExtendSvc->IsScaledExtendValid())
				{
					FString Reason = ExtendSvc->GetScaledExtendInvalidReason();
					ExtendLine += Reason.IsEmpty()
					? FString(TEXT(" ")) + LOCTEXT("HUD_Invalid", "[INVALID]").ToString()
					: FString::Printf(TEXT(" [%s]"), *Reason);
				}
				Lines.Add(ExtendLine);
			}
			else
			{
				Lines.Add(LOCTEXT("HUD_ExtendActive", "*Extend Active").ToString());
			}
		}
		else
		{
			Lines.Add(LOCTEXT("HUD_ExtendActive", "*Extend Active").ToString());
		}
	}

	// Grid dimensions and count (suppress during Extend — the Extend indicator already shows clone×row)
	if (!Subsystem->IsExtendModeActive())
	{
		if (State.GridCounters.X != 1 || State.GridCounters.Y != 1 || State.GridCounters.Z != 1)
		{
			int32 ValidChildCount = 0;
			if (FSFHologramHelperService* Helper = Subsystem->GetHologramHelper())
			{
				const TArray<TWeakObjectPtr<AFGHologram>> CurrentChildren = Helper->GetSpawnedChildren();
				for (const TWeakObjectPtr<AFGHologram>& ChildPtr : CurrentChildren)
				{
					if (AFGHologram* Child = ChildPtr.Get())
					{
						if (IsValid(Child))
						{
							++ValidChildCount;
						}
					}
				}
			}

			const int32 TotalCount = 1 + ValidChildCount;
			const FString GridLine = FText::Format(LOCTEXT("HUD_Grid", "Grid: {0}x{1}x{2} ({3})"),
				FText::AsNumber(FMath::Abs(State.GridCounters.X)),
				FText::AsNumber(FMath::Abs(State.GridCounters.Y)),
				FText::AsNumber(FMath::Abs(State.GridCounters.Z)),
				FText::AsNumber(TotalCount)).ToString();
			Lines.Add(GridLine);
		}
		else
		{
			Lines.Add(LOCTEXT("HUD_GridDefault", "Grid: 1x1x1").ToString());
		}
	}

	// Recipe display
	{
		int32 CurrentIndex = 0;
		int32 TotalRecipes = 0;
		Subsystem->GetRecipeDisplayInfo(CurrentIndex, TotalRecipes);
		TSubclassOf<UFGRecipe> ActiveRecipe = Subsystem->GetActiveRecipe();

		// CRITICAL FIX: Only show recipe if it is compatible with the CURRENT hologram
		// This prevents stale recipes from persisting on the HUD when switching between incompatible buildings
		bool bIsCompatible = true;
		if (AFGHologram* ActiveHologram = Subsystem->GetActiveHologram())
		{
			if (UClass* HologramClass = ActiveHologram->GetBuildClass())
			{
				// Check compatibility via recipe service (exposed through subsystem proxy)
				bIsCompatible = Subsystem->IsRecipeCompatibleWithHologram(ActiveRecipe, HologramClass);
			}
		}

		if (ActiveRecipe && TotalRecipes > 0 && bIsCompatible)
		{
			Lines.Add(TEXT("[RECIPE_ICON]"));
			const FString RecipeWithDetails = Subsystem->GetRecipeWithInputsOutputs(ActiveRecipe);
			// Add '*' marker when recipe mode is active to render this line in yellow
			const bool bRecipeActive = Subsystem->IsRecipeModeActive();
			const FString RecipeLine = bRecipeActive
				? FText::Format(LOCTEXT("HUD_RecipeActive", "Recipe* {0}/{1}: {2}"),
					FText::AsNumber(CurrentIndex + 1), FText::AsNumber(TotalRecipes), FText::FromString(RecipeWithDetails)).ToString()
				: FText::Format(LOCTEXT("HUD_Recipe", "Recipe {0}/{1}: {2}"),
					FText::AsNumber(CurrentIndex + 1), FText::AsNumber(TotalRecipes), FText::FromString(RecipeWithDetails)).ToString();
			Lines.Add(RecipeLine);
		}
	}

	// Auto-Connect Settings display (distributors, pipe junctions, and power poles only)
	// Only show settings if the current hologram supports auto-connect
	{
		const bool bAutoConnectActive = Subsystem->IsAutoConnectSettingsModeActive();

		// Only fetch and display dirty settings if we're in settings mode OR
		// if the current hologram is an auto-connect type (distributor, pipe junction, power pole, stackable support)
		// This prevents stale settings from showing on non-auto-connect holograms
		const bool bIsAutoConnectHologram = Subsystem->IsCurrentHologramAutoConnectCapable();

		if (bIsAutoConnectHologram)
		{
			const TArray<FString> DirtySettings = Subsystem->GetDirtyAutoConnectSettings();

			if (bAutoConnectActive)
			{
				const FString CurrentSetting = Subsystem->GetAutoConnectSettingDisplayString();
				bool bRenderedCurrentInDirtyList = false;

				// First, render all dirty settings, highlighting the active one
				for (const FString& Setting : DirtySettings)
				{
					const bool bIsCurrent = (Setting == CurrentSetting);
					if (bIsCurrent)
					{
						bRenderedCurrentInDirtyList = true;
					}
					Lines.Add(FText::Format(LOCTEXT("HUD_SettingsActive", "Settings: {0}{1}"),
						FText::FromString(Setting), FText::FromString(bIsCurrent ? TEXT("*") : TEXT(""))).ToString());
				}

				// If the current setting is not dirty (matches config), still show it as active
				if (!bRenderedCurrentInDirtyList)
				{
					Lines.Add(FText::Format(LOCTEXT("HUD_SettingsCurrent", "Settings: {0}*"),
						FText::FromString(CurrentSetting)).ToString());
				}
			}
			else
			{
				// When not in settings mode, show all dirty settings without highlight
				for (const FString& Setting : DirtySettings)
				{
					Lines.Add(FText::Format(LOCTEXT("HUD_Settings", "Settings: {0}"),
						FText::FromString(Setting)).ToString());
				}
			}
		}
	}

	// Spacing lines
	const bool bSpacingActive = Subsystem->IsSpacingModeActive();
	const ESFScaleAxis SpacingAxis = Subsystem->GetCurrentSpacingAxis();
    // Show when value is non-zero (negatives allowed) or while held on active axis
    const bool bShowSpacingX = (State.SpacingX != 0) || (bSpacingActive && SpacingAxis == ESFScaleAxis::X);
    const bool bShowSpacingY = (State.SpacingY != 0) || (bSpacingActive && SpacingAxis == ESFScaleAxis::Y);
    const bool bIsExtendActive = Subsystem->IsExtendModeActive();
    const bool bShowSpacingZ = !bIsExtendActive && ((State.SpacingZ != 0) || (bSpacingActive && SpacingAxis == ESFScaleAxis::Z));
	bool bAnySpacingPrinted = false;
	if (bShowSpacingX)
	{
		const bool bIsActive = (bSpacingActive && SpacingAxis == ESFScaleAxis::X);
		Lines.Add(FText::Format(LOCTEXT("HUD_SpacingX", "Spacing [X]{0}: {1}m"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.SpacingX / 100.0f))).ToString());
		bAnySpacingPrinted = true;
	}
	if (bShowSpacingY)
	{
		const bool bIsActive = (bSpacingActive && SpacingAxis == ESFScaleAxis::Y);
		Lines.Add(FText::Format(LOCTEXT("HUD_SpacingY", "Spacing [Y]{0}: {1}m"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.SpacingY / 100.0f))).ToString());
		bAnySpacingPrinted = true;
	}
	if (bShowSpacingZ)
	{
		const bool bIsActive = (bSpacingActive && SpacingAxis == ESFScaleAxis::Z);
		Lines.Add(FText::Format(LOCTEXT("HUD_SpacingZ", "Spacing [Z]{0}: {1}m"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.SpacingZ / 100.0f))).ToString());
		bAnySpacingPrinted = true;
	}
	if (bSpacingActive && !bAnySpacingPrinted)
	{
		Lines.Add(LOCTEXT("HUD_SpacingDefault", "Spacing*: 0.0m").ToString());
	}

	// Steps lines
	const bool bStepsActive = Subsystem->IsStepsModeActive();
	const ESFScaleAxis StepsAxis = Subsystem->GetCurrentStepsAxis();
    // Show when value is non-zero (negatives allowed) or while held on active axis
    const bool bShowStepsX = (State.StepsX != 0) || (bStepsActive && StepsAxis == ESFScaleAxis::X);
    const bool bShowStepsY = (State.StepsY != 0) || (bStepsActive && StepsAxis == ESFScaleAxis::Y);
	bool bAnyStepsPrinted = false;
	if (bShowStepsX)
	{
		const bool bIsActive = (bStepsActive && StepsAxis == ESFScaleAxis::X);
		Lines.Add(FText::Format(LOCTEXT("HUD_StepsX", "Steps [X]{0}: {1}m columns"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.StepsX / 100.0f))).ToString());
		bAnyStepsPrinted = true;
	}
	if (bShowStepsY)
	{
		const bool bIsActive = (bStepsActive && StepsAxis == ESFScaleAxis::Y);
		Lines.Add(FText::Format(LOCTEXT("HUD_StepsY", "Steps [Y]{0}: {1}m rows"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.StepsY / 100.0f))).ToString());
		bAnyStepsPrinted = true;
	}
	if (bStepsActive && !bAnyStepsPrinted)
	{
		Lines.Add(LOCTEXT("HUD_StepsDefault", "Steps*: 0.0m").ToString());
	}

	// Stagger lines (suppress entirely during Extend — stagger is blocked)
	const bool bStaggerActive = !bIsExtendActive && Subsystem->IsStaggerModeActive();
	const ESFScaleAxis StaggerAxis = Subsystem->GetCurrentStaggerAxis();
    // Show when value is non-zero (negatives allowed) or while held on active axis
    const bool bShowStaggerX = !bIsExtendActive && ((State.StaggerX != 0) || (bStaggerActive && StaggerAxis == ESFScaleAxis::X));
    const bool bShowStaggerY = !bIsExtendActive && ((State.StaggerY != 0) || (bStaggerActive && StaggerAxis == ESFScaleAxis::Y));
    const bool bShowStaggerZX = !bIsExtendActive && ((State.StaggerZX != 0) || (bStaggerActive && StaggerAxis == ESFScaleAxis::ZX));
    const bool bShowStaggerZY = !bIsExtendActive && ((State.StaggerZY != 0) || (bStaggerActive && StaggerAxis == ESFScaleAxis::ZY));
	bool bAnyStaggerPrinted = false;
	if (bShowStaggerX)
	{
		const bool bIsActive = (bStaggerActive && StaggerAxis == ESFScaleAxis::X);
		Lines.Add(FText::Format(LOCTEXT("HUD_StaggerX", "Stagger [X]{0}: {1}m sideways"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.StaggerX / 100.0f))).ToString());
		bAnyStaggerPrinted = true;
	}
	if (bShowStaggerY)
	{
		const bool bIsActive = (bStaggerActive && StaggerAxis == ESFScaleAxis::Y);
		Lines.Add(FText::Format(LOCTEXT("HUD_StaggerY", "Stagger [Y]{0}: {1}m forward"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.StaggerY / 100.0f))).ToString());
		bAnyStaggerPrinted = true;
	}
	if (bShowStaggerZX)
	{
		const bool bIsActive = (bStaggerActive && StaggerAxis == ESFScaleAxis::ZX);
		Lines.Add(FText::Format(LOCTEXT("HUD_StaggerZX", "Stagger [ZX]{0}: {1}m shift forward"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.StaggerZX / 100.0f))).ToString());
		bAnyStaggerPrinted = true;
	}
	if (bShowStaggerZY)
	{
		const bool bIsActive = (bStaggerActive && StaggerAxis == ESFScaleAxis::ZY);
		Lines.Add(FText::Format(LOCTEXT("HUD_StaggerZY", "Stagger [ZY]{0}: {1}m shift sideways"),
			FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
			FText::FromString(FString::Printf(TEXT("%.1f"), State.StaggerZY / 100.0f))).ToString());
		bAnyStaggerPrinted = true;
	}
	if (bStaggerActive && !bAnyStaggerPrinted)
	{
		Lines.Add(LOCTEXT("HUD_StaggerDefault", "Stagger*: 0.0m").ToString());
	}

	// Rotation lines (radial/arc placement)
	const bool bRotationActive = Subsystem->IsRotationModeActive();
	const ESFScaleAxis RotationAxis = Subsystem->GetCurrentRotationAxis();
	const bool bShowRotationZ = !FMath::IsNearlyZero(State.RotationZ) || (bRotationActive && RotationAxis == ESFScaleAxis::Z);
	bool bAnyRotationPrinted = false;
	if (bShowRotationZ)
	{
		const bool bIsActive = (bRotationActive && RotationAxis == ESFScaleAxis::Z);

		// DESIGN DECISION: Show degrees + abbreviated calculated info
		// Calculate radius and buildings-per-circle for user reference
		float RotationStepRad = FMath::Abs(FMath::DegreesToRadians(State.RotationZ));
		float Radius = (RotationStepRad > KINDA_SMALL_NUMBER)
			? (State.SpacingX / RotationStepRad) / 100.0f  // Convert cm to m
			: 0.0f;
		int32 BuildingsPerCircle = (FMath::Abs(State.RotationZ) > KINDA_SMALL_NUMBER)
			? FMath::RoundToInt(360.0f / FMath::Abs(State.RotationZ))
			: 0;

		if (Radius > 0.0f && BuildingsPerCircle > 0)
		{
			Lines.Add(FText::Format(LOCTEXT("HUD_RotationZFull", "Rotation [Z]{0}: {1}\u00B0 (R={2}m, {3}/circle)"),
				FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
				FText::FromString(FString::Printf(TEXT("%.1f"), State.RotationZ)),
				FText::FromString(FString::Printf(TEXT("%.1f"), Radius)),
				FText::AsNumber(BuildingsPerCircle)).ToString());
		}
		else
		{
			Lines.Add(FText::Format(LOCTEXT("HUD_RotationZ", "Rotation [Z]{0}: {1}\u00B0"),
				FText::FromString(bIsActive ? TEXT("*") : TEXT("")),
				FText::FromString(FString::Printf(TEXT("%.1f"), State.RotationZ))).ToString());
		}
		bAnyRotationPrinted = true;
	}
	if (bRotationActive && !bAnyRotationPrinted)
	{
		Lines.Add(LOCTEXT("HUD_RotationDefault", "Rotation*: 0.0\u00B0").ToString());
	}

	const FString FirstLine = FString::Join(Lines, TEXT("\n"));
	const FString SecondLine = TEXT("");
	return TPair<FString, FString>(FirstLine, SecondLine);
}

void USFHudService::UpdateBeltCosts(const TArray<FItemAmount>& BeltCosts, const TArray<FItemAmount>& DistributorCosts, UFGInventoryComponent* PlayerInventory, AFGCentralStorageSubsystem* CentralStorage)
{
	// DEPRECATED: Child holograms now aggregate costs automatically via GetCost()
	// This method is kept for API compatibility but does nothing
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 HUD: UpdateBeltCosts called (deprecated - child holograms handle costs)"));
}

void USFHudService::ClearBeltCosts()
{
	// DEPRECATED: Child holograms now aggregate costs automatically via GetCost()
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 HUD: ClearBeltCosts called (deprecated - child holograms handle costs)"));
}

void USFHudService::UpdatePipeCosts(const TArray<FItemAmount>& PipeCosts, UFGInventoryComponent* PlayerInventory, AFGCentralStorageSubsystem* CentralStorage)
{
	// DEPRECATED: Child holograms now aggregate costs automatically via GetCost()
	// This method is kept for API compatibility but does nothing
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 HUD: UpdatePipeCosts called (deprecated - child holograms handle costs)"));
}

void USFHudService::ClearPipeCosts()
{
	// DEPRECATED: Child holograms now aggregate costs automatically via GetCost()
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 HUD: ClearPipeCosts called (deprecated - child holograms handle costs)"));
}

void USFHudService::UpdatePowerCosts(const TArray<FItemAmount>& PowerCosts, UFGInventoryComponent* PlayerInventory, AFGCentralStorageSubsystem* CentralStorage)
{
	// DEPRECATED: Child holograms now aggregate costs automatically via GetCost()
	// This method is kept for API compatibility but does nothing
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 HUD: UpdatePowerCosts called (deprecated - child holograms handle costs)"));
}

void USFHudService::ClearPowerCosts()
{
	// DEPRECATED: Child holograms now aggregate costs automatically via GetCost()
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("💰 HUD: ClearPowerCosts called (deprecated - child holograms handle costs)"));
}

void USFHudService::UpdateLiftHeight(float LiftHeight, float WorldHeight)
{
	CachedLiftHeight = LiftHeight;
	CachedWorldHeight = WorldHeight;
}

void USFHudService::ClearLiftHeight()
{
	CachedLiftHeight = 0.0f;
	CachedWorldHeight = 0.0f;
}

void USFHudService::ResetState()
{
	// Clear all cached display text
	CurrentCounterText.Empty();
	CurrentBeltCostText.Empty();
	CurrentPipeCostText.Empty();
	CurrentPowerCostText.Empty();

	// Clear all cached cost arrays
	CachedBeltAvailability.Empty();
	CachedPipeCosts.Empty();
	CachedPipeAvailability.Empty();
	CachedPowerCosts.Empty();
	CachedPowerAvailability.Empty();

	// Clear lift height
	CachedLiftHeight = 0.0f;
	CachedWorldHeight = 0.0f;

	UE_LOG(LogSmartFoundations, Log, TEXT("HUD state reset for new hologram"));
}

#undef LOCTEXT_NAMESPACE
