// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Input/SFPanelInput.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"

bool SFPanelInput::IsToggleKey(APlayerController* Controller, const FKeyEvent& Event)
{
    if (Event.IsRepeat()) return false;
    if (FSlateApplication::IsInitialized())
    {
        const TSharedPtr<SWidget> Focus = FSlateApplication::Get().GetUserFocusedWidget(Event.GetUserIndex());
        if (Focus && Focus->GetTypeAsString().Contains(TEXT("EditableText"))) return false;
    }
    const ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
    const auto* Input = Player ? Player->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    const auto* Action = LoadObject<UInputAction>(nullptr,
        TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_ToggleSettingsForm.IA_Smart_ToggleSettingsForm"));
    if (Input && Action) return Input->QueryKeysMappedToAction(Action).Contains(Event.GetKey());
    return Event.GetKey() == EKeys::K;
}
