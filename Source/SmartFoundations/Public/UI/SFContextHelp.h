// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once

#include "CoreMinimal.h"
#include "Components/TextBlock.h"
#include "SFContextHelp.generated.h"

class UButton;

/** Collapsible guidance body with a separate button placed beside its context label. */
UCLASS()
class SMARTFOUNDATIONS_API USFContextHelp : public UTextBlock
{
    GENERATED_BODY()
public:
    void InitializeHelp(FText Guidance);
    UButton* GetHelpButton() const { return HelpButton; }
    bool GetIsExpanded() const { return GetVisibility() != ESlateVisibility::Collapsed; }
    void SetIsExpanded(bool bExpanded);
private:
    UFUNCTION() void ToggleDetails();
    UPROPERTY(Transient) TObjectPtr<UButton> HelpButton;
};
