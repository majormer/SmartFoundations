// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFInterfaceSettingsWidget.generated.h"
/** Actions appended to the existing Mod Settings, without replacing its property controls. */
UCLASS()
class SMARTFOUNDATIONS_API USFInterfaceSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
};
