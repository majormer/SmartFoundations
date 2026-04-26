#include "UI/SFUpgradeResultRow.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "SmartFoundations.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

void USFUpgradeResultRow::SetupRow(ESFUpgradeFamily Family, int32 Tier, int32 Count, int32 UpgradeableCount)
{
	if (FamilyNameText)
	{
		FString FamilyName = USFUpgradeAuditService::GetFamilyDisplayName(Family);
		FText DisplayText;
		
		// Wall outlets handle tier naming differently
		if (Family == ESFUpgradeFamily::WallOutletSingle)
		{
			DisplayText = FText::Format(LOCTEXT("UpgradeRow_WallOutlet", "Wall Outlet Mk.{0}"), FText::AsNumber(Tier));
		}
		else if (Family == ESFUpgradeFamily::WallOutletDouble)
		{
			DisplayText = FText::Format(LOCTEXT("UpgradeRow_DoubleWallOutlet", "Double Wall Outlet Mk.{0}"), FText::AsNumber(Tier));
		}
		else
		{
			DisplayText = FText::Format(LOCTEXT("UpgradeRow_FamilyTier", "{0} Mk.{1}"), FText::FromString(FamilyName), FText::AsNumber(Tier));
		}
		
		FamilyNameText->SetText(DisplayText);
	}

	if (CountText)
	{
		CountText->SetText(FText::AsNumber(Count));
	}

	if (UpgradeableText)
	{
		if (UpgradeableCount > 0)
		{
			UpgradeableText->SetText(FText::Format(LOCTEXT("UpgradeRow_Upgradeable", "({0} upgradeable)"), FText::AsNumber(UpgradeableCount)));
			UpgradeableText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			UpgradeableText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	UE_LOG(LogSmartFoundations, Verbose, TEXT("Upgrade Result Row: Setup for %s (Count: %d)"), *USFUpgradeAuditService::GetFamilyDisplayName(Family), Count);
}

#undef LOCTEXT_NAMESPACE
