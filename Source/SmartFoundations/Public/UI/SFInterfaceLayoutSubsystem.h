// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Tickable.h"
#include "SFInterfaceLayoutSubsystem.generated.h"
class USFHudLayoutEditorWidget;
class UFGInputMappingContext;
class UFGEnhancedInputComponent;
class APlayerController;
class FSFHudLayoutExtension;
struct FInputActionValue;

/** Local UI only. Shared Camera edit session when available; independent input otherwise. */
UCLASS()
class SMARTFOUNDATIONS_API USFInterfaceLayoutSubsystem : public ULocalPlayerSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    USFInterfaceLayoutSubsystem();
    virtual ~USFInterfaceLayoutSubsystem();
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickableInEditor() const override { return false; }
    virtual bool IsTickableWhenPaused() const override { return true; }
    void RequestEdit();
    void ToggleEdit(const FInputActionValue& Value);
    TSharedRef<class SWidget> BeginShared(FSimpleDelegate Close);
    bool EndEdit(bool bCommit);
    bool IsEditing() const { return Editor != nullptr; }
    USFHudLayoutEditorWidget* GetEditor() const { return Editor; }
    void TrackPanel(class UUserWidget* Panel);
    bool HasBlockingPanel() const;
private:
    UPROPERTY(Transient) TObjectPtr<USFHudLayoutEditorWidget> Editor;
    UPROPERTY(Transient) TObjectPtr<UFGInputMappingContext> EditContext;
    TWeakObjectPtr<UFGEnhancedInputComponent> BoundInput;
    TSharedPtr<FSFHudLayoutExtension> Extension;
    TArray<TWeakObjectPtr<class UUserWidget>> Panels;
    bool bRequested = false;
    bool bEmbedded = false;
    bool HasCamera() const;
    void CreateEditor(bool bShared, FSimpleDelegate Close = {});
};
