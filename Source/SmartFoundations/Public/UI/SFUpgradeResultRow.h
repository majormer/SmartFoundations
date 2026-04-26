#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Features/Upgrade/SFUpgradeAuditService.h"
#include "SFUpgradeResultRow.generated.h"

/**
 * Single row in the Smart! Upgrade Panel results list
 * Represents a specific family and tier (e.g., "47 Conveyor Belt Mk.1")
 */
UCLASS(BlueprintType, Blueprintable)
class SMARTFOUNDATIONS_API USFUpgradeResultRow : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Initialize row with data */
	UFUNCTION(BlueprintCallable, Category = "Smart|Upgrade")
	void SetupRow(ESFUpgradeFamily Family, int32 Tier, int32 Count, int32 UpgradeableCount);

protected:
	/** Family name text */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* FamilyNameText;

	/** Count text */
	UPROPERTY(meta = (BindWidget))
	class UTextBlock* CountText;

	/** Upgradeable indicator text (optional) */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	class UTextBlock* UpgradeableText;

	/** Icon image (optional) */
	UPROPERTY(meta = (BindWidget, OptionalWidget = true))
	class UImage* FamilyIcon;
};
