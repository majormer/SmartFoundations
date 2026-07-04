// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

/**
 * USmartUpgradePanel - row selection + cost display/calc + tabs + triage + traversal. Split out
 * of SmartUpgradePanel.cpp (slice U2, pure impl-split: only .cpp bodies move, the .h is byte-
 * identical, so the Smart_UpgradePanel_Widget BP contract is unchanged). No behavior change.
 */

#include "UI/SmartUpgradePanelImpl.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

void USmartUpgradePanel::OnRowSelected(ESFUpgradeFamily Family, int32 Tier)
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Row selected - Family=%d Tier=%d"), static_cast<int32>(Family), Tier);

	// Get player location
	APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->GetPawn())
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_NoPlayerLoc", "Cannot find player location"));
		}
		return;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Find closest instance of this family/tier from cached results
	float ClosestDistSq = FLT_MAX;
	FVector ClosestLocation = FVector::ZeroVector;
	bool bFoundAny = false;

	for (const FSFUpgradeFamilyResult& FamilyResult : CachedAuditResult.FamilyResults)
	{
		if (FamilyResult.Family != Family && !(IsConveyorUpgradeFamily(Family) && IsConveyorUpgradeFamily(FamilyResult.Family)))
		{
			continue;
		}

		for (const FSFUpgradeTierBucket& Bucket : FamilyResult.TierBuckets)
		{
			if (Bucket.Tier != Tier)
			{
				continue;
			}

			for (const FSFUpgradeAuditEntry& Entry : Bucket.Entries)
			{
				float DistSq = FVector::DistSquared(PlayerLocation, Entry.Location);
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					ClosestLocation = Entry.Location;
					bFoundAny = true;
				}
			}
		}
	}

	if (!bFoundAny)
	{
		if (StatusText)
		{
			StatusText->SetText(LOCTEXT("Upgrade_NoInstances", "No instances found (entries not stored)"));
		}
		return;
	}

	// Calculate distance and direction
	float DistanceMeters = FMath::Sqrt(ClosestDistSq) / 100.0f;  // Convert cm to meters
	FString Direction = GetCardinalDirection(PlayerLocation, ClosestLocation);

	// Update status text
	FString FamilyName = GetPanelFamilyDisplayName(Family);
	FString DisplayName;
	if (Family == ESFUpgradeFamily::WallOutletSingle)
	{
		DisplayName = FText::Format(LOCTEXT("UpgradeRow_WallOutlet", "Wall Outlet Mk.{0}"), FText::AsNumber(Tier)).ToString();
	}
	else if (Family == ESFUpgradeFamily::WallOutletDouble)
	{
		DisplayName = FText::Format(LOCTEXT("UpgradeRow_DoubleWallOutlet", "Double Wall Outlet Mk.{0}"), FText::AsNumber(Tier)).ToString();
	}
	else
	{
		DisplayName = FString::Printf(TEXT("%s Mk.%d"), *FamilyName, Tier);
	}

	if (StatusText)
	{
		StatusText->SetText(FText::Format(LOCTEXT("Upgrade_Nearest", "Nearest {0}: {1}m {2}"), FText::FromString(DisplayName), FText::FromString(FString::Printf(TEXT("%.0f"), DistanceMeters)), FText::FromString(Direction)));
	}

	// Store selection and update row highlighting
	SelectedFamily = Family;
	SelectedTier = Tier;

	// Highlight selected row, dim others (translucent Smart! accent)
	const FLinearColor SelectedBgColor(SFPanelStyle::Accent.R, SFPanelStyle::Accent.G, SFPanelStyle::Accent.B, 0.3f);
	const FLinearColor DefaultBgColor(0.0f, 0.0f, 0.0f, 0.0f);         // Transparent

	for (const auto& Pair : RowDataMap)
	{
		if (UBorder* RowBorder = Cast<UBorder>(Pair.Key))
		{
			const FRowData& Data = Pair.Value;
			bool bIsSelected = (Data.Family == Family && Data.Tier == Tier);
			RowBorder->SetBrushColor(bIsSelected ? SelectedBgColor : DefaultBgColor);
		}
	}

	// Populate target tier dropdown based on selected family
	PopulateTargetTierDropdown();

	// Update cost display
	UpdateCostDisplay();

	// Enable upgrade button now that a row is selected
	if (SharedUpgradeButton)
	{
		SharedUpgradeButton->SetIsEnabled(true);
	}

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Nearest %s is %.0fm %s at %s"),
		*DisplayName, DistanceMeters, *Direction, *ClosestLocation.ToString());
}

FString USmartUpgradePanel::GetCardinalDirection(const FVector& FromLocation, const FVector& ToLocation)
{
	// Calculate direction vector (ignoring Z)
	FVector2D Direction2D(ToLocation.X - FromLocation.X, ToLocation.Y - FromLocation.Y);
	Direction2D.Normalize();

	// Get angle in degrees (0 = East, 90 = North in UE coordinates)
	// UE uses X = forward (North), Y = right (East)
	float AngleRad = FMath::Atan2(Direction2D.Y, Direction2D.X);
	float AngleDeg = FMath::RadiansToDegrees(AngleRad);

	// Convert to compass bearing (0 = North, 90 = East)
	// In UE: X+ is North, Y+ is East
	// Atan2(Y, X) gives angle from X axis
	// So 0 degrees = East, 90 = North, 180 = West, -90 = South
	// We want: 0 = North, 90 = East, 180 = South, 270 = West
	float CompassAngle = 90.0f - AngleDeg;  // Rotate so 0 = North
	if (CompassAngle < 0) CompassAngle += 360.0f;
	if (CompassAngle >= 360.0f) CompassAngle -= 360.0f;

	// 16-point compass (22.5 degrees per segment)
	// N = 0, NNE = 22.5, NE = 45, ENE = 67.5, E = 90, etc.
	static const TCHAR* Directions[] = {
		TEXT("N"), TEXT("NNE"), TEXT("NE"), TEXT("ENE"),
		TEXT("E"), TEXT("ESE"), TEXT("SE"), TEXT("SSE"),
		TEXT("S"), TEXT("SSW"), TEXT("SW"), TEXT("WSW"),
		TEXT("W"), TEXT("WNW"), TEXT("NW"), TEXT("NNW")
	};

	// Each segment is 22.5 degrees, offset by 11.25 to center on direction
	int32 Index = FMath::RoundToInt((CompassAngle + 11.25f) / 22.5f) % 16;

	return Directions[Index];
}

void USmartUpgradePanel::OnUpgradeProgress(float ProgressPercent, int32 SuccessCount, int32 TotalCount)
{
	if (StatusText)
	{
		StatusText->SetText(FText::Format(LOCTEXT("Upgrade_ProgressCount", "Upgrading... {0}/{1} ({2}%)"), FText::AsNumber(SuccessCount), FText::AsNumber(TotalCount), FText::FromString(FString::Printf(TEXT("%.0f"), ProgressPercent * 100.0f))));
	}
}

void USmartUpgradePanel::OnUpgradeCompleted(const FSFUpgradeExecutionResult& Result)
{
	// Unsubscribe from delegates
	if (USFSubsystem* Subsystem = USFSubsystem::Get(this))
	{
		if (USFUpgradeExecutionService* ExecutionService = Subsystem->GetUpgradeExecutionService())
		{
			ExecutionService->OnUpgradeExecutionCompleted.RemoveDynamic(this, &USmartUpgradePanel::OnUpgradeCompleted);
			ExecutionService->OnUpgradeProgressUpdated.RemoveDynamic(this, &USmartUpgradePanel::OnUpgradeProgress);
		}
	}

	// Update status with results
	if (StatusText)
	{
		if (Result.bCompleted)
		{
			StatusText->SetText(FText::Format(LOCTEXT("Upgrade_Complete", "Upgrade complete: {0} success, {1} failed, {2} skipped"), FText::AsNumber(Result.SuccessCount), FText::AsNumber(Result.FailCount), FText::AsNumber(Result.SkipCount)));
		}
		else
		{
			StatusText->SetText(FText::Format(LOCTEXT("Upgrade_Cancelled", "Upgrade cancelled: {0}/{1} completed"), FText::AsNumber(Result.SuccessCount), FText::AsNumber(Result.TotalProcessed)));
		}
	}

	// Refresh so the results reflect the new state. Network mode re-walks the (now-upgraded)
	// network from a re-acquired anchor; radius mode re-runs the radius audit. [#456]
	if (ActiveTab == ESmartUpgradeTab::Traversal)
	{
		// On a network client the upgraded actors have not replicated yet when this result RPC
		// arrives, so an immediate re-acquire finds no seed and the refresh silently no-ops (MP
		// field report 2026-07-04). Defer + retry until replication catches up. Host/SP is
		// synchronous - the new actors already exist - so refresh straight away.
		if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
		{
			BeginDeferredTraversalRefresh();
		}
		else if (!RefreshTraversalScan() && SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_RescanAim", "Upgrade complete - aim at the network and Scan again"));
		}
	}
	else
	{
		RefreshAudit();
	}
}

void USmartUpgradePanel::OnTargetTierChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Target tier changed to: %s"), *SelectedItem);

	// Parse tier from selection (e.g., "Mk.4" -> 4)
	CachedTargetTier = 0;
	if (SelectedItem.StartsWith(TEXT("Mk.")))
	{
		FString TierStr = SelectedItem.RightChop(3);
		CachedTargetTier = FCString::Atoi(*TierStr);
	}

	// Update cost display
	UpdateCostDisplay();
}

void USmartUpgradePanel::PopulateTargetTierDropdown()
{
	// Use appropriate dropdown based on active tab
	UComboBoxString* ActiveComboBox = nullptr;

	if (ActiveTab == ESmartUpgradeTab::Radius)
	{
		ActiveComboBox = RadiusTargetTierComboBox;
	}
	else
	{
		// Traversal tab - use traversal dropdown
		ActiveComboBox = TraversalTargetTierComboBox;
	}

	if (!ActiveComboBox)
	{
		return;
	}

	// Clear existing options
	ActiveComboBox->ClearOptions();

	// In traversal mode, SelectedTier == 0 is the "All tiers" sweep row (multiple tiers in
	// network) - offer every target from Mk.2 up. [#456] A specific selected source tier
	// floors the target options above it, exactly like radius mode.
	bool bIsTraversalMode = (ActiveTab == ESmartUpgradeTab::Traversal);
	int32 MinSourceTier = (bIsTraversalMode && SelectedTier == 0) ? 1 : SelectedTier;

	if (SelectedFamily == ESFUpgradeFamily::None)
	{
		ActiveComboBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// For radius mode, require a selected tier
	if (!bIsTraversalMode && SelectedTier == 0)
	{
		ActiveComboBox->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	// Get max tier for this family
	int32 MaxTier = 6;  // Default for belts/lifts (Mk1-Mk6)
	if (SelectedFamily == ESFUpgradeFamily::Pipe || SelectedFamily == ESFUpgradeFamily::Pump)
	{
		MaxTier = 2;
	}
	else if (SelectedFamily == ESFUpgradeFamily::PowerPole ||
	         SelectedFamily == ESFUpgradeFamily::WallOutletSingle ||
	         SelectedFamily == ESFUpgradeFamily::WallOutletDouble)
	{
		MaxTier = 3;
	}

	// Get highest unlocked tier from subsystem
	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
	if (Subsystem && PC)
	{
		if (SelectedFamily == ESFUpgradeFamily::Belt || SelectedFamily == ESFUpgradeFamily::Lift)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedBeltTier(PC));
		}
		else if (SelectedFamily == ESFUpgradeFamily::Pipe)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedPipeTier(PC));
		}
		else if (SelectedFamily == ESFUpgradeFamily::PowerPole)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedPowerPoleTier(PC));
		}
		else if (SelectedFamily == ESFUpgradeFamily::WallOutletSingle)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedWallOutletTier(PC, /*bDouble*/ false));
		}
		else if (SelectedFamily == ESFUpgradeFamily::WallOutletDouble)
		{
			MaxTier = FMath::Min(MaxTier, Subsystem->GetHighestUnlockedWallOutletTier(PC, /*bDouble*/ true));
		}
	}

	// Add options for tiers above source tier up to max unlocked
	// In traversal mode, MinSourceTier is 1 (show all tiers from Mk.2 up)
	bool bHasOptions = false;
	for (int32 Tier = MinSourceTier + 1; Tier <= MaxTier; ++Tier)
	{
		FString Option = FString::Printf(TEXT("Mk.%d"), Tier);
		ActiveComboBox->AddOption(Option);
		bHasOptions = true;
	}

	if (bHasOptions)
	{
		// Default to max tier
		FString DefaultOption = FString::Printf(TEXT("Mk.%d"), MaxTier);
		ActiveComboBox->SetSelectedOption(DefaultOption);
		CachedTargetTier = MaxTier;
		ActiveComboBox->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ActiveComboBox->SetVisibility(ESlateVisibility::Collapsed);
	}

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Populated tier dropdown - Source=%d Max=%d Options=%d"),
		SelectedTier, MaxTier, bHasOptions ? (MaxTier - SelectedTier) : 0);
}

void USmartUpgradePanel::UpdateCostDisplay()
{
	// Use appropriate cost text based on active tab
	UTextBlock* ActiveCostText = nullptr;
	if (ActiveTab == ESmartUpgradeTab::Radius)
	{
		ActiveCostText = RadiusCostDetailsText;
	}
	else
	{
		ActiveCostText = TraversalCostDetailsText;
	}

	if (!ActiveCostText)
	{
		return;
	}

	// Traversal mode: use CachedTraversalResult
	if ((ActiveTab == ESmartUpgradeTab::Traversal) && CachedTraversalResult.IsValid())
	{
		if (CachedTargetTier == 0)
		{
			ActiveCostText->SetText(LOCTEXT("Upgrade_SelectTargetTier", "Select target tier"));
			return;
		}

		// Count upgradeable items by family
		TMap<ESFUpgradeFamily, int32> FamilyCounts;
		TMap<TSubclassOf<UFGItemDescriptor>, int32> TotalNetCost;
		int32 TotalUpgradeable = 0;

		for (const FSFUpgradeAuditEntry& Entry : CachedTraversalResult.Entries)
		{
			// [#456] SelectedTier > 0 = cost only the picked source tier; 0 = the legacy sweep.
			const bool bTierMatch = (SelectedTier > 0)
				? (Entry.CurrentTier == SelectedTier && Entry.CurrentTier < CachedTargetTier)
				: (Entry.CurrentTier < CachedTargetTier && Entry.CurrentTier > 0);
			if (bTierMatch)
			{
				ESFUpgradeFamily EntryFamily = USFUpgradeTraversalService::GetUpgradeFamily(Entry.Buildable.Get());
				FamilyCounts.FindOrAdd(EntryFamily)++;
				TotalUpgradeable++;

				// Calculate cost for this item
				TMap<TSubclassOf<UFGItemDescriptor>, int32> ItemCost;
				if (CalculateUpgradeCost(EntryFamily, Entry.CurrentTier, CachedTargetTier, 1, ItemCost))
				{
					for (const auto& Pair : ItemCost)
					{
						TotalNetCost.FindOrAdd(Pair.Key) += Pair.Value;
					}
				}
			}
		}

		if (TotalUpgradeable == 0)
		{
			ActiveCostText->SetText(LOCTEXT("Upgrade_NoItems", "No items to upgrade"));
			return;
		}

		// Build cost display string
		FString CostString = FText::Format(LOCTEXT("Upgrade_CostItems", "{0} items \u2192 Mk.{1}\n"), FText::AsNumber(TotalUpgradeable), FText::AsNumber(CachedTargetTier)).ToString();

		bool bHasCost = false;
		bool bHasRefund = false;

		for (const auto& Pair : TotalNetCost)
		{
			if (Pair.Value != 0 && Pair.Key)
			{
				FString ItemName = UFGItemDescriptor::GetItemName(Pair.Key).ToString();
				if (Pair.Value > 0)
				{
					CostString += FString::Printf(TEXT("-%d %s\n"), Pair.Value, *ItemName);
					bHasCost = true;
				}
				else
				{
					CostString += FString::Printf(TEXT("+%d %s\n"), -Pair.Value, *ItemName);
					bHasRefund = true;
				}
			}
		}

		if (!bHasCost && !bHasRefund)
		{
			CostString += LOCTEXT("Upgrade_NoNetCost", "(No net cost)").ToString();
		}

		ActiveCostText->SetText(FText::FromString(CostString));
		return;
	}

	// Radius mode: original logic
	if (SelectedFamily == ESFUpgradeFamily::None || SelectedTier == 0 || CachedTargetTier == 0)
	{
		ActiveCostText->SetText(LOCTEXT("Upgrade_SelectFamily", "Select a family to see costs"));
		return;
	}

	// Find how many items of this tier exist in the cached audit
	int32 ItemCount = 0;
	TMap<TSubclassOf<UFGItemDescriptor>, int32> NetCost;
	bool bCostAvailable = false;
	for (const FSFUpgradeFamilyResult& FamilyResult : CachedAuditResult.FamilyResults)
	{
		if (FamilyResult.Family == SelectedFamily || (IsConveyorUpgradeFamily(SelectedFamily) && IsConveyorUpgradeFamily(FamilyResult.Family)))
		{
			for (const FSFUpgradeTierBucket& Bucket : FamilyResult.TierBuckets)
			{
				if (Bucket.Tier == SelectedTier)
				{
					ItemCount += Bucket.Count;

					TMap<TSubclassOf<UFGItemDescriptor>, int32> FamilyNetCost;
					if (!CalculateUpgradeCost(FamilyResult.Family, SelectedTier, CachedTargetTier, Bucket.Count, FamilyNetCost))
					{
						bCostAvailable = false;
						break;
					}

					for (const auto& Pair : FamilyNetCost)
					{
						NetCost.FindOrAdd(Pair.Key) += Pair.Value;
					}
					bCostAvailable = true;
					break;
				}
			}

			if (!bCostAvailable && ItemCount > 0)
			{
				break;
			}

			if (!IsConveyorUpgradeFamily(SelectedFamily))
			{
				break;
			}
		}
	}

	if (ItemCount == 0)
	{
		ActiveCostText->SetText(LOCTEXT("Upgrade_NoItems", "No items to upgrade"));
		return;
	}

	FString FamilyName = GetPanelFamilyDisplayName(SelectedFamily);

	if (!bCostAvailable)
	{
		ActiveCostText->SetText(FText::Format(LOCTEXT("Upgrade_CostUnavailable", "{0} \u00d7 {1} Mk.{2} \u2192 Mk.{3}\n(Cost unavailable)"), FText::AsNumber(ItemCount), FText::FromString(FamilyName), FText::AsNumber(SelectedTier), FText::AsNumber(CachedTargetTier)));
		return;
	}

	// Build cost display string
	FString CostString = FText::Format(LOCTEXT("Upgrade_CostHeader", "{0} \u00d7 {1} Mk.{2} \u2192 Mk.{3}\n"), FText::AsNumber(ItemCount), FText::FromString(FamilyName), FText::AsNumber(SelectedTier), FText::AsNumber(CachedTargetTier)).ToString();

	bool bHasCost = false;
	bool bHasRefund = false;

	for (const auto& Pair : NetCost)
	{
		if (Pair.Value != 0 && Pair.Key)
		{
			FString ItemName = UFGItemDescriptor::GetItemName(Pair.Key).ToString();
			if (Pair.Value > 0)
			{
				CostString += FString::Printf(TEXT("-%d %s\n"), Pair.Value, *ItemName);
				bHasCost = true;
			}
			else
			{
				CostString += FString::Printf(TEXT("+%d %s\n"), -Pair.Value, *ItemName);
				bHasRefund = true;
			}
		}
	}

	if (!bHasCost && !bHasRefund)
	{
		CostString += LOCTEXT("Upgrade_NoNetCost", "(No net cost)").ToString();
	}

	ActiveCostText->SetText(FText::FromString(CostString));
}

int32 USmartUpgradePanel::GetSelectedTargetTier() const
{
	// Use appropriate dropdown based on active tab
	const UComboBoxString* ActiveComboBox = nullptr;

	if (ActiveTab == ESmartUpgradeTab::Radius)
	{
		ActiveComboBox = RadiusTargetTierComboBox;
	}
	else
	{
		// Traversal tab - use traversal dropdown
		ActiveComboBox = TraversalTargetTierComboBox;
	}

	if (!ActiveComboBox)
	{
		return 0;
	}

	FString SelectedOption = ActiveComboBox->GetSelectedOption();
	if (SelectedOption.IsEmpty())
	{
		return 0;
	}

	// Parse tier from selection (e.g., "Mk.4" -> 4)
	if (SelectedOption.StartsWith(TEXT("Mk.")))
	{
		FString TierStr = SelectedOption.RightChop(3);
		return FCString::Atoi(*TierStr);
	}

	return 0;
}

bool USmartUpgradePanel::CalculateUpgradeCost(ESFUpgradeFamily Family, int32 SourceTier, int32 TargetTier, int32 ItemCount, TMap<TSubclassOf<UFGItemDescriptor>, int32>& OutNetCost) const
{
	USFSubsystem* Subsystem = USFSubsystem::Get(GetWorld());
	if (!Subsystem)
	{
		return false;
	}

	// Get recipes for source and target tiers
	TSubclassOf<UFGRecipe> SourceRecipe = nullptr;
	TSubclassOf<UFGRecipe> TargetRecipe = nullptr;

	switch (Family)
	{
		case ESFUpgradeFamily::Belt:
			SourceRecipe = Subsystem->GetBeltRecipeForTier(SourceTier);
			TargetRecipe = Subsystem->GetBeltRecipeForTier(TargetTier);
			break;
		case ESFUpgradeFamily::Lift:
		{
			using namespace SFAssetPaths::UpgradeRecipes;
			if (SourceTier >= 1 && SourceTier <= 6)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, ConveyorLift[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 6)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, ConveyorLift[TargetTier - 1]);
			}
			break;
		}
		case ESFUpgradeFamily::Pipe:
			SourceRecipe = Subsystem->GetPipeRecipeForTier(SourceTier, true);
			TargetRecipe = Subsystem->GetPipeRecipeForTier(TargetTier, true);
			break;
		case ESFUpgradeFamily::PowerPole:
		{
			using namespace SFAssetPaths::UpgradeRecipes;
			if (SourceTier >= 1 && SourceTier <= 3)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, PowerPole[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, PowerPole[TargetTier - 1]);
			}
			break;
		}
		case ESFUpgradeFamily::WallOutletSingle:
		{
			using namespace SFAssetPaths::UpgradeRecipes;
			if (SourceTier >= 1 && SourceTier <= 3)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletSingle[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletSingle[TargetTier - 1]);
			}
			break;
		}
		case ESFUpgradeFamily::WallOutletDouble:
		{
			using namespace SFAssetPaths::UpgradeRecipes;
			if (SourceTier >= 1 && SourceTier <= 3)
			{
				SourceRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletDouble[SourceTier - 1]);
			}
			if (TargetTier >= 1 && TargetTier <= 3)
			{
				TargetRecipe = LoadClass<UFGRecipe>(nullptr, WallOutletDouble[TargetTier - 1]);
			}
			break;
		}
		default:
			return false;
	}

	if (!SourceRecipe || !TargetRecipe)
	{
		return false;
	}

	const UFGRecipe* SourceCDO = SourceRecipe->GetDefaultObject<UFGRecipe>();
	const UFGRecipe* TargetCDO = TargetRecipe->GetDefaultObject<UFGRecipe>();
	if (!SourceCDO || !TargetCDO)
	{
		return false;
	}

	// Get ingredients for both recipes
	TArray<FItemAmount> SourceIngredients = SourceCDO->GetIngredients();
	TArray<FItemAmount> TargetIngredients = TargetCDO->GetIngredients();

	// Calculate net cost: target cost - source refund
	// For each item in target, add to cost
	for (const FItemAmount& Ingredient : TargetIngredients)
	{
		if (Ingredient.ItemClass)
		{
			int32& CurrentAmount = OutNetCost.FindOrAdd(Ingredient.ItemClass);
			CurrentAmount += Ingredient.Amount * ItemCount;
		}
	}

	// For each item in source, subtract from cost (refund)
	for (const FItemAmount& Ingredient : SourceIngredients)
	{
		if (Ingredient.ItemClass)
		{
			int32& CurrentAmount = OutNetCost.FindOrAdd(Ingredient.ItemClass);
			CurrentAmount -= Ingredient.Amount * ItemCount;
		}
	}

	return true;
}

void USmartUpgradePanel::OnRadiusTabClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Radius tab clicked"));
	SwitchToTab(ESmartUpgradeTab::Radius);
}

void USmartUpgradePanel::OnTraversalTabClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Traversal tab clicked"));
	SwitchToTab(ESmartUpgradeTab::Traversal);
}

// [Track E] OnTriageTabClicked removed.

void USmartUpgradePanel::SwitchToTab(ESmartUpgradeTab NewTab)
{
	ActiveTab = NewTab;

	const bool bRadiusTab = (NewTab == ESmartUpgradeTab::Radius);
	const bool bTraversalTab = (NewTab == ESmartUpgradeTab::Traversal);

	// Toggle content visibility
	if (RadiusContent)
	{
		RadiusContent->SetVisibility(bRadiusTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TraversalContent)
	{
		TraversalContent->SetVisibility(bTraversalTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Update tab button appearance - shared Smart! scheme (mirrors Restore's SetActiveRestoreTab):
	// active tab = orange accent fill with a near-black label, idle = dark fill with a light label.
	if (RadiusTabButton)
	{
		RadiusTabButton->SetStyle(SFPanelStyle::MakeButtonStyle(bRadiusTab));
	}
	if (TraversalTabButton)
	{
		TraversalTabButton->SetStyle(SFPanelStyle::MakeButtonStyle(bTraversalTab));
	}
	if (UTextBlock* RadiusText = RadiusTabButton ? Cast<UTextBlock>(RadiusTabButton->GetChildAt(0)) : nullptr)
	{
		RadiusText->SetColorAndOpacity(FSlateColor(bRadiusTab ? SFPanelStyle::NearBlackText : SFPanelStyle::LightText));
	}
	if (UTextBlock* TraversalText = TraversalTabButton ? Cast<UTextBlock>(TraversalTabButton->GetChildAt(0)) : nullptr)
	{
		TraversalText->SetColorAndOpacity(FSlateColor(bTraversalTab ? SFPanelStyle::NearBlackText : SFPanelStyle::LightText));
	}

	// Update shared status text based on tab
	if (SharedStatusText)
	{
		if (bRadiusTab)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_SetRadiusScan", "Set radius and press Scan to find items"));
		}
		else if (bTraversalTab)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_AimAndScan", "Aim at a buildable and press Scan Network"));
		}
	}

	// Reset selection state when switching tabs
	SelectedFamily = ESFUpgradeFamily::None;
	SelectedTier = 0;
	CachedTargetTier = 0;

	// Disable upgrade button until new selection is made
	if (SharedUpgradeButton)
	{
		SharedUpgradeButton->SetIsEnabled(false);
	}

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Switched to %s tab"),
		bRadiusTab ? TEXT("Radius") : TEXT("Network"));
}

// [Track E] OnTriageDetectClicked / OnTriageRepairClicked removed. They were the only callers of the
// chain-repair triage tooling (DetectChainActorIssues / RepairAllChainActorIssues), which is also removed.

void USmartUpgradePanel::OnTraversalScanClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Traversal Scan clicked"));

	// Get player controller and build gun
	AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_ErrNoPlayer", "Error: No player controller"));
		}
		return;
	}

	// Get the upgrade target from the current hologram - the game already knows what we're aiming at
	AFGBuildable* AnchorBuildable = nullptr;

	// The active build-gun hologram (if any). Captured so the fallback trace can IGNORE it -
	// otherwise, when holding a matching-tier belt, the snapped preview sits right where you aim
	// and the trace hits the hologram instead of the belt behind it (#456 field report).
	AFGHologram* ActiveHologram = nullptr;
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (Character)
	{
		AFGBuildGun* BuildGun = Character->GetBuildGun();
		if (BuildGun)
		{
			UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetCurrentState());
			ActiveHologram = BuildState ? BuildState->GetHologram() : nullptr;
			if (ActiveHologram)
			{
				// GetUpgradedActor returns the buildable being targeted for upgrade, but ONLY when
				// the held gun would replace it (a different tier - upgrade OR downgrade). Holding
				// the SAME tier leaves the gun in plain build mode and this returns null.
				AActor* UpgradeTarget = ActiveHologram->GetUpgradedActor();
				AnchorBuildable = Cast<AFGBuildable>(UpgradeTarget);

				if (AnchorBuildable)
				{
					UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Got upgrade target from hologram: %s"),
						*AnchorBuildable->GetClass()->GetName());
				}
			}
		}
	}

	// Fallback: the hologram's GetUpgradedActor() only yields a target when the held build gun would
	// REPLACE what you're aiming at - i.e. a DIFFERENT belt tier. The Upgrade Panel only opens while
	// you hold a conduit (belt/pipe/power line, IsUpgradeCapableContext), so in practice the failing
	// case is aiming at a MATCHING-tier belt, which returned nothing (#456 field report). Resolve the
	// aim independently of the held tier, mirroring the Extend detection trace (SFSubsystem): player
	// view point + ECC_WorldStatic (hits buildings, not just visibility) + ignore the pawn AND the
	// active hologram (its snapped preview otherwise blocks the trace in the matching-tier case).
	if (!AnchorBuildable)
	{
		FVector ViewLoc;
		FRotator ViewRot;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		const FVector StartLocation = ViewLoc;
		const FVector EndLocation = StartLocation + ViewRot.Vector() * 5000.0f;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PC->GetPawn());
		if (ActiveHologram)
		{
			QueryParams.AddIgnoredActor(ActiveHologram);
		}

		FHitResult HitResult;
		if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_WorldStatic, QueryParams))
		{
			// Real-actor buildings (machines, poles) report themselves directly.
			AnchorBuildable = Cast<AFGBuildable>(HitResult.GetActor());
			if (AnchorBuildable && USFUpgradeTraversalService::GetUpgradeFamily(AnchorBuildable) == ESFUpgradeFamily::None)
			{
				AnchorBuildable = nullptr;
			}

			// Belts/pipes are AbstractInstance-rendered: the trace reports the shared instance
			// manager as the hit actor, so resolve the specific instance to its owning buildable
			// (the same mechanism the game's dismantle aim uses - exact, no distance guessing).
			if (!AnchorBuildable)
			{
				if (AAbstractInstanceManager* InstanceManager = AAbstractInstanceManager::GetInstanceManager(this))
				{
					FInstanceHandle Handle;
					if (InstanceManager->ResolveHit(HitResult, Handle))
					{
						if (AFGBuildable* ResolvedBuildable = Cast<AFGBuildable>(AAbstractInstanceManager::GetOwnerByHandle(Handle)))
						{
							if (USFUpgradeTraversalService::GetUpgradeFamily(ResolvedBuildable) != ESFUpgradeFamily::None)
							{
								AnchorBuildable = ResolvedBuildable;
								UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Resolved aim to buildable via instance manager: %s"),
									*AnchorBuildable->GetClass()->GetName());
							}
						}
					}
				}
			}

			// Final safety net: nearest upgradeable buildable to the impact point.
			if (!AnchorBuildable)
			{
				FVector SearchPoint = HitResult.ImpactPoint;
				float SearchRadius = 300.0f;
				float ClosestDist = FLT_MAX;

				for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
				{
					AFGBuildable* Buildable = *It;
					if (!Buildable) continue;

					float Dist = FVector::Dist(SearchPoint, Buildable->GetActorLocation());
					if (Dist < SearchRadius)
					{
						// Only consider upgradeable types
						ESFUpgradeFamily Family = USFUpgradeTraversalService::GetUpgradeFamily(Buildable);
						if (Family != ESFUpgradeFamily::None && Dist < ClosestDist)
						{
							ClosestDist = Dist;
							AnchorBuildable = Buildable;
						}
					}
				}

				if (AnchorBuildable)
				{
					UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Found buildable near aim point: %s (dist: %.1f)"),
						*AnchorBuildable->GetClass()->GetName(), ClosestDist);
				}
			}
		}
	}

	if (!AnchorBuildable)
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_AimAtTarget", "Aim at a belt, pipe, or power pole to scan its network"));
		}
		if (TraversalAnchorText)
		{
			TraversalAnchorText->SetText(LOCTEXT("Upgrade_NoTarget", "No target - aim at a buildable"));
		}
		return;
	}

	// Check if it's an upgradeable type
	ESFUpgradeFamily Family = USFUpgradeTraversalService::GetUpgradeFamily(AnchorBuildable);
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: GetUpgradeFamily for %s returned %d"),
		*AnchorBuildable->GetClass()->GetName(), (int32)Family);

	if (Family == ESFUpgradeFamily::None)
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_NotUpgradeable", "Target is not upgradeable - aim at a belt, pipe, or power pole"));
		}
		if (TraversalAnchorText)
		{
			TraversalAnchorText->SetText(FText::Format(LOCTEXT("Upgrade_Invalid", "Invalid: {0}"), FText::FromString(AnchorBuildable->GetClass()->GetName())));
		}
		return;
	}

	RunTraversalScanFromAnchor(AnchorBuildable);
}

void USmartUpgradePanel::RunTraversalScanFromAnchor(AFGBuildable* AnchorBuildable)
{
	if (!AnchorBuildable)
	{
		return;
	}
	AFGPlayerController* PC = Cast<AFGPlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	// Remember the anchor + its location (the location survives the anchor being replaced by its
	// own upgrade, so the post-upgrade refresh can re-seed the walk - #456).
	TraversalAnchor = AnchorBuildable;
	TraversalAnchorLocation = AnchorBuildable->GetActorLocation();
	if (TraversalAnchorText)
	{
		int32 Tier = USFUpgradeTraversalService::GetBuildableTier(AnchorBuildable);
		TraversalAnchorText->SetText(FText::Format(LOCTEXT("Upgrade_Anchor", "Anchor: {0} (Mk{1})"), FText::FromString(AnchorBuildable->GetClass()->GetName()), FText::AsNumber(Tier)));
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(LOCTEXT("Upgrade_Scanning", "Scanning network..."));
	}

	// Read traversal config from checkboxes
	if (CrossSplittersCheckBox)
	{
		TraversalConfig.bCrossSplitters = CrossSplittersCheckBox->IsChecked();
	}
	if (CrossStorageCheckBox)
	{
		TraversalConfig.bCrossStorage = CrossStorageCheckBox->IsChecked();
	}
	if (CrossTrainPlatformsCheckBox)
	{
		TraversalConfig.bCrossTrainPlatforms = CrossTrainPlatformsCheckBox->IsChecked();
	}

	// [UPGRADE-MP] On a network client the walk must run on the SERVER: traversal follows
	// factory/pipe/circuit connection components and their connection values are server-only
	// (GetConnection() is null on clients - the same limitation as the Extend topology walk).
	// The result arrives via Client_ReceiveTraversalResult -> InjectTraversalResult -> the
	// static delegate below, and lands in the same UpdateTraversalUI path as the SP scan.
	if (GetWorld() && GetWorld()->GetNetMode() == NM_Client)
	{
		USFUpgradeTraversalService::OnClientTraversalResultReceived.RemoveAll(this);
		USFUpgradeTraversalService::OnClientTraversalResultReceived.AddUObject(
			this, &USmartUpgradePanel::OnClientTraversalResult);

		// RCOs are not actors — GetRemoteCallObjectOfClass, never GetAllActorsOfClass
		// (the old actor scan failed silently on every client; live finding 2026-06-10).
		if (AFGPlayerController* FGPC = Cast<AFGPlayerController>(PC))
		{
			if (USFRCO* RCO = FGPC->GetRemoteCallObjectOfClass<USFRCO>())
			{
				RCO->Server_StartUpgradeTraversal(AnchorBuildable, TraversalConfig);
				UE_LOG(LogSmartUI, Verbose, TEXT("Upgrade Panel: Sent traversal request via SFRCO"));
				return;
			}
		}
		UE_LOG(LogSmartUI, Verbose, TEXT("Upgrade Panel: Could not find SFRCO instance for traversal scan"));
		if (SharedStatusText)
		{
			SharedStatusText->SetText(LOCTEXT("Upgrade_ErrRCOScan", "Error: server connection unavailable"));
		}
		return;
	}

	// Create traversal service and run scan
	USFUpgradeTraversalService* TraversalService = NewObject<USFUpgradeTraversalService>();
	CachedTraversalResult = TraversalService->TraverseNetwork(AnchorBuildable, TraversalConfig, PC);

	// Update UI with results
	UpdateTraversalUI(CachedTraversalResult);
}

bool USmartUpgradePanel::RefreshTraversalScan()
{
	// Re-acquire a network seed. Prefer the original anchor if it survived; otherwise the upgrade
	// replaced it (and possibly the whole run) with new actors - fall back to any still-valid
	// scanned entry, then to the nearest upgradeable buildable to where the anchor was (the new
	// belts spawn in-place, so this lands on one of them).
	AFGBuildable* NewAnchor = TraversalAnchor.IsValid() ? TraversalAnchor.Get() : nullptr;

	if (!NewAnchor)
	{
		for (const FSFUpgradeAuditEntry& Entry : CachedTraversalResult.Entries)
		{
			if (Entry.Buildable.IsValid())
			{
				NewAnchor = Entry.Buildable.Get();
				break;
			}
		}
	}

	if (!NewAnchor && GetWorld())
	{
		// Cap the search so a fully-replaced network re-seeds on a nearby new belt rather than
		// grabbing something across the map if nothing survived close by.
		constexpr float MaxReacquireDistSq = 1000.0f * 1000.0f;  // 10m
		float ClosestDistSq = MaxReacquireDistSq;
		for (TActorIterator<AFGBuildable> It(GetWorld()); It; ++It)
		{
			AFGBuildable* Buildable = *It;
			if (!Buildable) continue;
			if (USFUpgradeTraversalService::GetUpgradeFamily(Buildable) == ESFUpgradeFamily::None) continue;

			const float DistSq = FVector::DistSquared(TraversalAnchorLocation, Buildable->GetActorLocation());
			if (DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				NewAnchor = Buildable;
			}
		}
	}

	if (NewAnchor)
	{
		RunTraversalScanFromAnchor(NewAnchor);
		return true;
	}
	return false;
}

void USmartUpgradePanel::BeginDeferredTraversalRefresh()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TraversalRefreshAttempts = 0;
	TWeakObjectPtr<USmartUpgradePanel> WeakThis(this);
	World->GetTimerManager().SetTimer(TraversalRefreshTimerHandle,
		FTimerDelegate::CreateLambda([WeakThis]()
		{
			USmartUpgradePanel* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			UWorld* TimerWorld = Self->GetWorld();
			if (!TimerWorld)
			{
				return;
			}

			Self->TraversalRefreshAttempts++;
			// Each attempt re-acquires a (now hopefully replicated) seed and kicks an async server
			// scan. A fresh result clears this timer in OnClientTraversalResult, so we normally stop
			// as soon as the server answers; the attempt cap is the backstop.
			Self->RefreshTraversalScan();

			if (Self->TraversalRefreshAttempts >= 5)
			{
				TimerWorld->GetTimerManager().ClearTimer(Self->TraversalRefreshTimerHandle);
				if (Self->SharedStatusText)
				{
					Self->SharedStatusText->SetText(LOCTEXT("Upgrade_RescanAim", "Upgrade complete - aim at the network and Scan again"));
				}
			}
		}),
		0.6f, /*bLoop*/ true, /*FirstDelay*/ 0.6f);
}

void USmartUpgradePanel::OnClientTraversalResult(const FSFTraversalResult& Result)
{
	// A fresh server result arrived - stop any deferred post-upgrade refresh retries (#456 MP).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TraversalRefreshTimerHandle);
	}

	CachedTraversalResult = Result;
	UpdateTraversalUI(CachedTraversalResult);
}

void USmartUpgradePanel::UpdateTraversalUI(const FSFTraversalResult& Result)
{
	if (!Result.IsValid())
	{
		if (SharedStatusText)
		{
			SharedStatusText->SetText(FText::Format(LOCTEXT("Upgrade_ScanFailed", "Scan failed: {0}"), FText::FromString(Result.ErrorMessage)));
		}
		return;
	}

	// Update status text
	if (SharedStatusText)
	{
		FText FoundText = FText::Format(LOCTEXT("Upgrade_FoundItems", "Found {0} items ({1} upgradeable)"), FText::AsNumber(Result.TotalCount), FText::AsNumber(Result.UpgradeableCount));
		if (Result.bHitMaxLimit)
		{
			FoundText = FText::Format(LOCTEXT("Upgrade_FoundItemsLimit", "{0} [LIMIT HIT]"), FoundText);
		}
		SharedStatusText->SetText(FoundText);
	}

	// [#456] Rebuild the tier breakdown as SELECTABLE source-tier rows (they were display-only
	// text, which is why network scans could only ever sweep "everything below target"). Row
	// shape mirrors the radius audit rows; RowDataMap drives the shared click hit-test.
	RowDataMap.Empty();
	if (TraversalResultsContainer)
	{
		TraversalResultsContainer->ClearChildren();

		const FSlateFontInfo RowFont = SFFont::Get(11);

		auto AddTraversalRow = [&](int32 Tier, int32 Count, const FString& TierLabel, const FString& Tooltip)
		{
			UBorder* RowBorder = NewObject<UBorder>(this);
			RowBorder->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			RowBorder->SetPadding(FMargin(2.0f, 1.0f, 2.0f, 1.0f));
			RowBorder->SetToolTipText(FText::FromString(Tooltip));

			UHorizontalBox* RowHBox = NewObject<UHorizontalBox>(this);

			UTextBlock* CountText = NewObject<UTextBlock>(this);
			CountText->SetText(FText::FromString(FString::Printf(TEXT("%d"), Count)));
			CountText->SetFont(RowFont);
			CountText->SetColorAndOpacity(FSlateColor(SFPanelStyle::LightText));
			CountText->SetJustification(ETextJustify::Right);

			USizeBox* CountSizeBox = NewObject<USizeBox>(this);
			CountSizeBox->SetWidthOverride(50.0f);
			CountSizeBox->AddChild(CountText);
			if (UHorizontalBoxSlot* CountSlot = RowHBox->AddChildToHorizontalBox(CountSizeBox))
			{
				CountSlot->SetVerticalAlignment(VAlign_Center);
			}

			UTextBlock* SepText = NewObject<UTextBlock>(this);
			SepText->SetText(LOCTEXT("Upgrade_Separator", " x "));
			SepText->SetFont(RowFont);
			SepText->SetColorAndOpacity(FSlateColor(SFPanelStyle::DimText));
			if (UHorizontalBoxSlot* SepSlot = RowHBox->AddChildToHorizontalBox(SepText))
			{
				SepSlot->SetVerticalAlignment(VAlign_Center);
			}

			UTextBlock* NameText = NewObject<UTextBlock>(this);
			NameText->SetText(FText::FromString(TierLabel));
			NameText->SetFont(RowFont);
			NameText->SetColorAndOpacity(FSlateColor(SFPanelStyle::LightText));
			if (UHorizontalBoxSlot* NameSlot = RowHBox->AddChildToHorizontalBox(NameText))
			{
				NameSlot->SetVerticalAlignment(VAlign_Center);
			}

			RowBorder->AddChild(RowHBox);
			TraversalResultsContainer->AddChildToVerticalBox(RowBorder);
			RowDataMap.Add(RowBorder, FRowData{Result.Family, Tier, TierLabel});
		};

		// Sweep row first (Tier 0 = legacy "everything below the chosen target"; stays the default).
		AddTraversalRow(0, Result.TotalCount,
			LOCTEXT("Upgrade_AllTiersRow", "All tiers").ToString(),
			LOCTEXT("Upgrade_AllTiersTooltip", "Upgrade every tier below the chosen target").ToString());

		// One selectable row per tier present in the network, ascending.
		TArray<int32> Tiers;
		Result.CountByTier.GetKeys(Tiers);
		Tiers.Sort();
		for (int32 Tier : Tiers)
		{
			const int32 Count = Result.CountByTier[Tier];
			if (Count <= 0)
			{
				continue;
			}
			const FString TierLabel = FString::Printf(TEXT("Mk.%d"), Tier);
			AddTraversalRow(Tier, Count, TierLabel,
				FText::Format(LOCTEXT("Upgrade_TierRowTooltip", "Upgrade only Mk.{0} in this network"), FText::AsNumber(Tier)).ToString());
		}
	}

	// Default to the sweep row so the panel works with zero clicks, exactly like before #456.
	// This sets SelectedFamily/SelectedTier, highlights the row, populates the target dropdown,
	// refreshes cost, and gates the upgrade button.
	OnTraversalRowSelected(0);

	// Show the tier dropdown for traversal tab
	if (TraversalTargetTierComboBox)
	{
		TraversalTargetTierComboBox->SetVisibility(ESlateVisibility::Visible);
	}

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Traversal scan complete - %d items, %d upgradeable"),
		Result.TotalCount, Result.UpgradeableCount);
}

void USmartUpgradePanel::OnTraversalRowSelected(int32 Tier)
{
	if (!CachedTraversalResult.IsValid())
	{
		return;
	}

	SelectedFamily = CachedTraversalResult.Family;
	SelectedTier = Tier;

	// Highlight the selected row (same treatment as the radius rows).
	const FLinearColor SelectedBgColor(SFPanelStyle::Accent.R, SFPanelStyle::Accent.G, SFPanelStyle::Accent.B, 0.3f);
	const FLinearColor DefaultBgColor(0.0f, 0.0f, 0.0f, 0.0f);
	for (const auto& Pair : RowDataMap)
	{
		if (UBorder* RowBorder = Cast<UBorder>(Pair.Key))
		{
			RowBorder->SetBrushColor(Pair.Value.Tier == Tier ? SelectedBgColor : DefaultBgColor);
		}
	}

	// Gate the upgrade button on the selection actually containing upgradeable members.
	bool bHasUpgradeable = false;
	for (const FSFUpgradeAuditEntry& Entry : CachedTraversalResult.Entries)
	{
		if (Entry.IsUpgradeable() && (Tier == 0 || Entry.CurrentTier == Tier))
		{
			bHasUpgradeable = true;
			break;
		}
	}
	if (SharedUpgradeButton)
	{
		SharedUpgradeButton->SetIsEnabled(bHasUpgradeable);
	}

	// Nearest-instance readout for a specific tier (mirrors the radius row behavior).
	if (Tier > 0 && StatusText)
	{
		APlayerController* PC = GetOwningPlayer();
		if (PC && PC->GetPawn())
		{
			const FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
			float ClosestDistSq = FLT_MAX;
			FVector ClosestLocation = FVector::ZeroVector;
			bool bFoundAny = false;
			for (const FSFUpgradeAuditEntry& Entry : CachedTraversalResult.Entries)
			{
				if (Entry.CurrentTier != Tier)
				{
					continue;
				}
				const float DistSq = FVector::DistSquared(PlayerLocation, Entry.Location);
				if (DistSq < ClosestDistSq)
				{
					ClosestDistSq = DistSq;
					ClosestLocation = Entry.Location;
					bFoundAny = true;
				}
			}
			if (bFoundAny)
			{
				StatusText->SetText(FText::Format(LOCTEXT("Upgrade_NearestTier", "Nearest Mk.{0}: {1}m {2}"),
					FText::AsNumber(Tier),
					FText::FromString(FString::Printf(TEXT("%.0f"), FMath::Sqrt(ClosestDistSq) / 100.0f)),
					FText::FromString(GetCardinalDirection(PlayerLocation, ClosestLocation))));
			}
		}
	}

	PopulateTargetTierDropdown();
	UpdateCostDisplay();

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Traversal row selected - Tier=%d upgradeable=%d"),
		Tier, bHasUpgradeable ? 1 : 0);
}

#undef LOCTEXT_NAMESPACE

