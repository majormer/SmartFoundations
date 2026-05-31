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

	// Highlight selected row, dim others
	const FLinearColor SelectedBgColor(0.886f, 0.498f, 0.118f, 0.3f);  // Translucent orange
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

	// Refresh audit to show updated counts
	RefreshAudit();
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

	// In traversal mode, SelectedTier may be 0 (multiple tiers in network)
	// In that case, populate all tiers above 1
	bool bIsTraversalMode = (ActiveTab == ESmartUpgradeTab::Traversal);
	int32 MinSourceTier = bIsTraversalMode ? 1 : SelectedTier;

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
			if (Entry.CurrentTier < CachedTargetTier && Entry.CurrentTier > 0)
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

void USmartUpgradePanel::OnTriageTabClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Triage tab clicked"));
	SwitchToTab(ESmartUpgradeTab::Triage);
}

void USmartUpgradePanel::SwitchToTab(ESmartUpgradeTab NewTab)
{
	ActiveTab = NewTab;

	const bool bRadiusTab = (NewTab == ESmartUpgradeTab::Radius);
	const bool bTraversalTab = (NewTab == ESmartUpgradeTab::Traversal);
	const bool bTriageTab = (NewTab == ESmartUpgradeTab::Triage);

	// Toggle content visibility
	if (RadiusContent)
	{
		RadiusContent->SetVisibility(bRadiusTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TraversalContent)
	{
		TraversalContent->SetVisibility(bTraversalTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (TriageContent)
	{
		TriageContent->SetVisibility(bTriageTab ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Update tab button appearance (visual feedback for active/inactive)
	const FLinearColor ActiveTabColor(0.886f, 0.498f, 0.118f, 1.0f);    // Satisfactory orange
	const FLinearColor InactiveTabColor(0.15f, 0.15f, 0.18f, 1.0f);     // Dark gray
	const FLinearColor ActiveTextColor(1.0f, 1.0f, 1.0f, 1.0f);         // White
	const FLinearColor InactiveTextColor(0.6f, 0.6f, 0.6f, 1.0f);       // Dim gray

	if (RadiusTabButton)
	{
		RadiusTabButton->SetBackgroundColor(bRadiusTab ? ActiveTabColor : InactiveTabColor);
	}
	if (TraversalTabButton)
	{
		TraversalTabButton->SetBackgroundColor(bTraversalTab ? ActiveTabColor : InactiveTabColor);
	}
	if (TriageTabButton)
	{
		TriageTabButton->SetBackgroundColor(bTriageTab ? ActiveTabColor : InactiveTabColor);
	}

	// Update tab text colors
	if (UTextBlock* RadiusText = RadiusTabButton ? Cast<UTextBlock>(RadiusTabButton->GetChildAt(0)) : nullptr)
	{
		RadiusText->SetColorAndOpacity(FSlateColor(bRadiusTab ? ActiveTextColor : InactiveTextColor));
	}
	if (UTextBlock* TraversalText = TraversalTabButton ? Cast<UTextBlock>(TraversalTabButton->GetChildAt(0)) : nullptr)
	{
		TraversalText->SetColorAndOpacity(FSlateColor(bTraversalTab ? ActiveTextColor : InactiveTextColor));
	}
	if (UTextBlock* TriageText = TriageTabButton ? Cast<UTextBlock>(TriageTabButton->GetChildAt(0)) : nullptr)
	{
		TriageText->SetColorAndOpacity(FSlateColor(bTriageTab ? ActiveTextColor : InactiveTextColor));
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
		else
		{
			SharedStatusText->SetText(LOCTEXT("Triage_Ready", "Use Detect to scan for chain issues"));
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
		bRadiusTab ? TEXT("Radius") : bTraversalTab ? TEXT("Traversal") : TEXT("Triage"));
}

void USmartUpgradePanel::OnTriageDetectClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Triage Detect clicked"));

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	USFChainActorService* ChainService = Subsystem->GetChainActorService();
	if (!ChainService)
	{
		return;
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(LOCTEXT("Triage_Detecting", "Scanning map for chain issues..."));
	}

	const FSFChainDiagnosticResult Result = ChainService->DetectChainActorIssues();

	if (TriageDetectResultText)
	{
		if (Result.HasIssues())
		{
			TriageDetectResultText->SetText(FText::Format(
				LOCTEXT("Triage_IssuesFound", "Found {0} zombie chain(s), {1} split chain group(s), {2} chain=NONE belt(s), and {3} orphaned tick group(s) ({4} empty, {5} live belt/lift entries, {6} connected candidate group(s)).\nRepair will purge zombies, rebuild split chains, and re-register live conveyors from orphaned tick groups. After repair, save, reload, and run Detect again."),
				FText::AsNumber(Result.ZombieChainCount),
				FText::AsNumber(Result.SplitChainCount),
				FText::AsNumber(Result.OrphanedBeltCount),
				FText::AsNumber(Result.OrphanedTickGroupCount),
				FText::AsNumber(Result.EmptyOrphanedTickGroupCount),
				FText::AsNumber(Result.LiveBeltsInOrphanedTickGroups),
				FText::AsNumber(Result.OrphanedBeltCandidates)));
		}
		else
		{
			TriageDetectResultText->SetText(LOCTEXT("Triage_NoIssues", "No issues detected. Your conveyor chains are healthy."));
		}
		TriageDetectResultText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Show repair button only if there are issues to fix
	if (TriageRepairButton)
	{
		TriageRepairButton->SetVisibility(Result.HasIssues() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Reset repair result when re-detecting
	if (TriageRepairResultText)
	{
		TriageRepairResultText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(Result.HasIssues()
			? LOCTEXT("Triage_IssuesSummary", "Issues found - Repair handles zombies, splits, and orphaned tick groups")
			: LOCTEXT("Triage_AllClear", "All clear — no chain issues found"));
	}
}

void USmartUpgradePanel::OnTriageRepairClicked()
{
	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Triage Repair clicked"));

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!Subsystem)
	{
		return;
	}

	USFChainActorService* ChainService = Subsystem->GetChainActorService();
	if (!ChainService)
	{
		return;
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(LOCTEXT("Triage_Repairing", "Repairing chain issues..."));
	}

	const FSFChainRepairResult Result = ChainService->RepairAllChainActorIssues();

	if (TriageRepairResultText)
	{
		if (Result.HasAnyResults())
		{
			if (Result.HasOrphanCandidates() && !Result.OrphanedBeltReportPath.IsEmpty())
			{
				TriageRepairResultText->SetText(FText::Format(
					LOCTEXT("Triage_RepairDoneWithReport", "Repair complete. Removed {0} zombie chain(s), rebuilt {1} split group(s), and re-registered {7} live conveyor(s) from {3} orphaned tick group(s), including {4} empty group(s) and {5} live belt(s). A diagnostic report was written to: {6}"),
					FText::AsNumber(Result.ZombiesPurged),
					FText::AsNumber(Result.SplitGroupsRebuilt),
					FText::AsNumber(Result.OrphanedBeltCandidates),
					FText::AsNumber(Result.OrphanedTickGroupCount),
					FText::AsNumber(Result.EmptyOrphanedTickGroupCount),
					FText::AsNumber(Result.LiveBeltsInOrphanedTickGroups),
					FText::FromString(Result.OrphanedBeltReportPath),
					FText::AsNumber(Result.OrphanedBeltsRequeued)));
			}
			else
			{
				TriageRepairResultText->SetText(FText::Format(
					LOCTEXT("Triage_RepairDone", "Repair complete. Removed {0} zombie chain(s), rebuilt {1} split group(s), and re-registered {6} live conveyor(s) from {3} orphaned tick group(s), including {4} empty group(s) and {5} live belt(s). Save, reload, and run Detect again."),
					FText::AsNumber(Result.ZombiesPurged),
					FText::AsNumber(Result.SplitGroupsRebuilt),
					FText::AsNumber(Result.OrphanedBeltCandidates),
					FText::AsNumber(Result.OrphanedTickGroupCount),
					FText::AsNumber(Result.EmptyOrphanedTickGroupCount),
					FText::AsNumber(Result.LiveBeltsInOrphanedTickGroups),
					FText::AsNumber(Result.OrphanedBeltsRequeued)));
			}
		}
		else
		{
			TriageRepairResultText->SetText(LOCTEXT("Triage_RepairNone", "Repair complete. Nothing needed fixing."));
		}
		TriageRepairResultText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	// Hide the repair button after running
	if (TriageRepairButton)
	{
		TriageRepairButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (SharedStatusText)
	{
		SharedStatusText->SetText(Result.HasOrphanCandidates()
			? LOCTEXT("Triage_RepairCompleteWithOrphans", "Chain repair complete — save, reload, and run Detect again")
			: LOCTEXT("Triage_RepairComplete", "Chain repair complete"));
	}
}

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

	FVector HologramLocation = FVector::ZeroVector;
	AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
	if (Character)
	{
		AFGBuildGun* BuildGun = Character->GetBuildGun();
		if (BuildGun)
		{
			UFGBuildGunStateBuild* BuildState = Cast<UFGBuildGunStateBuild>(BuildGun->GetCurrentState());
			AFGHologram* Hologram = BuildState ? BuildState->GetHologram() : nullptr;
			if (Hologram)
			{
				// Save hologram location for fallback search
				HologramLocation = Hologram->GetActorLocation();

				// GetUpgradedActor returns the buildable being targeted for upgrade (only when placement is valid)
				AActor* UpgradeTarget = Hologram->GetUpgradedActor();
				AnchorBuildable = Cast<AFGBuildable>(UpgradeTarget);

				if (AnchorBuildable)
				{
					UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Got upgrade target from hologram: %s"),
						*AnchorBuildable->GetClass()->GetName());
				}
			}
		}
	}

	// Fallback: If hologram didn't give us a target (e.g. same-tier),
	// use line trace to find hit point and search for nearest upgradeable buildable
	if (!AnchorBuildable)
	{
		FHitResult HitResult;
		FVector StartLocation = PC->PlayerCameraManager->GetCameraLocation();
		FVector EndLocation = StartLocation + PC->PlayerCameraManager->GetCameraRotation().Vector() * 5000.0f;

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(PC->GetPawn());

		// Line trace to get hit point (even if it hits instanced mesh manager)
		if (GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, QueryParams))
		{
			// Use actor iteration to find nearest upgradeable buildable to hit point
			// This works around instanced mesh rendering where physics queries return wrong actor
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

	// Update anchor text
	TraversalAnchor = AnchorBuildable;
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

	// Create traversal service and run scan
	USFUpgradeTraversalService* TraversalService = NewObject<USFUpgradeTraversalService>();
	CachedTraversalResult = TraversalService->TraverseNetwork(AnchorBuildable, TraversalConfig, PC);

	// Update UI with results
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

	// Clear existing results
	if (TraversalResultsContainer)
	{
		TraversalResultsContainer->ClearChildren();
	}

	// Build tier breakdown display
	if (TraversalResultsContainer)
	{
		// Create a text block for tier breakdown
		for (const auto& TierPair : Result.CountByTier)
		{
			int32 Tier = TierPair.Key;
			int32 Count = TierPair.Value;

			UTextBlock* TierText = NewObject<UTextBlock>(this);
			TierText->SetText(FText::FromString(FString::Printf(TEXT("  Mk%d: %d"), Tier, Count)));
			TierText->SetFont(SFFont::Get(12));
			TierText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

			TraversalResultsContainer->AddChild(TierText);
		}
	}

	// Set selected family from result
	SelectedFamily = Result.Family;
	SelectedTier = 0; // Will be set when user selects target tier

	// Populate target tier dropdown
	PopulateTargetTierDropdown();

	// Show the tier dropdown for traversal tab
	if (TraversalTargetTierComboBox)
	{
		TraversalTargetTierComboBox->SetVisibility(ESlateVisibility::Visible);
	}

	// Update cost display for traversal results
	UpdateCostDisplay();

	// Enable upgrade button if there are upgradeable items
	if (Result.UpgradeableCount > 0)
	{
		if (SharedUpgradeButton)
		{
			SharedUpgradeButton->SetIsEnabled(true);
		}
	}

	UE_LOG(LogSmartUI, VeryVerbose, TEXT("Upgrade Panel: Traversal scan complete - %d items, %d upgradeable"),
		Result.TotalCount, Result.UpgradeableCount);
}

#undef LOCTEXT_NAMESPACE

