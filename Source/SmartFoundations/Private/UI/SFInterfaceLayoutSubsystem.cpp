// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFInterfaceLayoutSubsystem.h"
#include "SmartFoundations.h"
#include "HUD/SFHudLayoutEditorWidget.h"
#include "FGPlayerController.h"
#include "UI/FGGameUI.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EnhancedInputSubsystems.h"
#include "Input/FGEnhancedInputComponent.h"
#include "Input/FGInputMappingContext.h"
#include "InputAction.h"
#include "Widgets/SNullWidget.h"
#if SF_CAMERA_PROVIDER_AVAILABLE
#include "Features/IModularFeatures.h"
#include "Integration/SmartCameraLayoutExtension.h"
#include "Integration/SmartCameraViewportRegistry.h"

class FSFHudLayoutExtension final : public ISmartCameraLayoutExtension
{
public:
    explicit FSFHudLayoutExtension(USFInterfaceLayoutSubsystem* InOwner) : Owner(InOwner) {}
    virtual UObject* GetLayoutOwner() const override { return Owner.Get(); }
    virtual TSharedRef<SWidget> BeginSharedLayout(FSimpleDelegate Close) override { return Owner.IsValid() ? Owner->BeginShared(Close) : SNullWidget::NullWidget; }
    virtual void EndSharedLayout() override
    {
        if (Owner.IsValid()) Owner->EndEdit(false);
    }
    virtual bool FinishSharedLayout(bool bCommit) override { return !Owner.IsValid() || Owner->EndEdit(bCommit); }
    virtual bool HasBlockingInterface() const override { return Owner.IsValid() && Owner->HasBlockingPanel(); }
private:
    TWeakObjectPtr<USFInterfaceLayoutSubsystem> Owner;
};
#else
class FSFHudLayoutExtension {};
#endif

// SP/MP DIVERGENCE MAP: local player UI/config only; no server RPCs or world actors.
USFInterfaceLayoutSubsystem::USFInterfaceLayoutSubsystem() = default;
USFInterfaceLayoutSubsystem::~USFInterfaceLayoutSubsystem() = default;
void USFInterfaceLayoutSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if SF_CAMERA_PROVIDER_AVAILABLE
    Extension = MakeShared<FSFHudLayoutExtension>(this);
    IModularFeatures::Get().RegisterModularFeature(ISmartCameraLayoutExtension::GetFeatureName(), Extension.Get());
#endif
}
void USFInterfaceLayoutSubsystem::Deinitialize()
{
    EndEdit(false);
    if (auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(); Input && EditContext) Input->RemoveMappingContext(EditContext);
#if SF_CAMERA_PROVIDER_AVAILABLE
    if (Extension) IModularFeatures::Get().UnregisterModularFeature(ISmartCameraLayoutExtension::GetFeatureName(), Extension.Get());
#endif
    Extension.Reset();
    Super::Deinitialize();
}
TStatId USFInterfaceLayoutSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(USFInterfaceLayoutSubsystem, STATGROUP_Tickables); }
void USFInterfaceLayoutSubsystem::TrackPanel(UUserWidget* Panel)
{
    Panels.RemoveAll([](const auto& Value){return !Value.IsValid();});
    Panels.AddUnique(Panel);
}
bool USFInterfaceLayoutSubsystem::HasBlockingPanel() const
{
    for (const auto& Panel : Panels) if (Panel.IsValid() && Panel->GetWorld() == GetWorld() && Panel->IsInViewport() && Panel->IsVisible()) return true;
    return false;
}
bool USFInterfaceLayoutSubsystem::HasCamera() const
{
#if SF_CAMERA_PROVIDER_AVAILABLE
    for (auto* Registry : IModularFeatures::Get().GetModularFeatureImplementations<ISmartCameraViewportRegistry>(ISmartCameraViewportRegistry::GetNamedFeatureName()))
        if (Registry->GetCameraWorld() == GetWorld()) return true;
#endif
    return false;
}
void USFInterfaceLayoutSubsystem::Tick(float DeltaTime)
{
    if (IsTemplate() || !GetWorld() || !GetWorld()->IsGameWorld() || !GetLocalPlayer()) return;
    auto* PC = Cast<AFGPlayerController>(GetLocalPlayer()->GetPlayerController(GetWorld()));
    auto* Input = GetLocalPlayer()->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    auto* Enhanced = PC ? Cast<UFGEnhancedInputComponent>(PC->InputComponent) : nullptr;
    if (!PC || !Input || !Enhanced) return;
    if (!EditContext) EditContext = LoadObject<UFGInputMappingContext>(nullptr, TEXT("/SmartFoundations/SmartFoundations/Input/Contexts/MC_Smart_Layout.MC_Smart_Layout"));
    if (EditContext)
    {
        const bool Wanted = !HasCamera();
        if (Wanted && !Input->HasMappingContext(EditContext)) Input->AddMappingContext(EditContext, 100);
        if (!Wanted && Input->HasMappingContext(EditContext)) Input->RemoveMappingContext(EditContext);
        if (BoundInput.Get() != Enhanced)
        {
            if (auto* Action = LoadObject<UInputAction>(nullptr, TEXT("/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_EditLayout.IA_Smart_EditLayout")))
            {
                Enhanced->BindAction(Action, ETriggerEvent::Started, this, &USFInterfaceLayoutSubsystem::ToggleEdit);
                BoundInput = Enhanced;
            }
        }
    }
    auto* UI = PC->GetGameUI();
    if (Editor && !bEmbedded && (!UI || UI->IsPauseMenuOpen() || !UI->GetInteractWidgetStack().Contains(Editor))) EndEdit(false);
    if (!bRequested || Editor || !UI || UI->IsPauseMenuOpen()) return;
    for (auto* Widget : UI->GetInteractWidgetStack())
        if (IsValid(Widget) && Widget->GetClass()->GetPathName() != TEXT("/Game/FactoryGame/Interface/UI/InGame/BuildMenu/Widget_BuildMode.Widget_BuildMode_C")) return;
    bRequested = false;
    CreateEditor(false);
}
void USFInterfaceLayoutSubsystem::RequestEdit()
{
#if SF_CAMERA_PROVIDER_AVAILABLE
    // Optional reflected entry avoids a Camera DLL dependency. Its existing action/session owns input.
    if (HasCamera() && GetWorld() && GetWorld()->GetGameInstance())
        if (auto* Class = FindObject<UClass>(nullptr, TEXT("/Script/SmartCamera.SmartCameraSubsystem")))
            if (auto* Camera = GetWorld()->GetGameInstance()->GetSubsystemBase(Class))
                if (auto* Function = Camera->FindFunction(TEXT("RequestLayoutEdit")))
                {
                    struct { bool bCurrentSave = false; } Params;
                    Camera->ProcessEvent(Function, &Params); return;
                }
#endif
    bRequested = true;
}
void USFInterfaceLayoutSubsystem::ToggleEdit(const FInputActionValue& Value)
{
    if (Editor) EndEdit(true); else bRequested = !bRequested;
}
void USFInterfaceLayoutSubsystem::CreateEditor(bool bShared, FSimpleDelegate Close)
{
    if (Editor || !GetWorld()) return;
    auto* PC = Cast<AFGPlayerController>(GetLocalPlayer()->GetPlayerController(GetWorld()));
    if (!PC || !PC->GetGameUI()) return;
    Editor = CreateWidget<USFHudLayoutEditorWidget>(PC);
    if (!Editor) return;
    bEmbedded = bShared;
    Editor->Setup(this, Close);
    if (!bShared)
    {
        PC->GetGameUI()->AddInteractWidget(Editor);
        Editor->AddToViewport(200);
        Editor->SetInputMode(); Editor->UpdateIgnoreMoveInput(); Editor->UpdateIgnoreLookInput(); Editor->SetKeyboardFocus();
    }
}
TSharedRef<SWidget> USFInterfaceLayoutSubsystem::BeginShared(FSimpleDelegate Close)
{
    CreateEditor(true, Close);
    return Editor ? Editor->TakeWidget() : SNullWidget::NullWidget;
}
bool USFInterfaceLayoutSubsystem::EndEdit(bool bCommit)
{
    bRequested = false;
    auto* Widget = Editor.Get();
    if (!Widget) return true;
    if (!Widget->Finish(bCommit)) return false;
    Editor = nullptr;
    if (bEmbedded) { bEmbedded = false; return true; }
    if (auto* PC = Cast<AFGPlayerController>(Widget->GetOwningPlayer()))
    {
        if (auto* UI = PC->GetGameUI())
        {
            UI->RemoveInteractWidget(Widget); Widget->RemoveFromParent();
            const auto Stack = UI->GetInteractWidgetStack();
            if (!Stack.IsEmpty() && IsValid(Stack.Last()))
            {
                Stack.Last()->SetInputMode(); Stack.Last()->UpdateIgnoreMoveInput(); Stack.Last()->UpdateIgnoreLookInput();
            }
            else
            {
                PC->bShowMouseCursor = false; PC->SetInputMode(FInputModeGameOnly()); PC->ResetIgnoreMoveInput(); PC->ResetIgnoreLookInput();
            }
        }
    }
    return true;
}
