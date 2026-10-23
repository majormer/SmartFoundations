// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "UI/FGInteractWidget.h"
#include "UI/SFInterfacePreferences.h"
#include "SFHudLayoutEditorWidget.generated.h"
class USFInterfaceLayoutSubsystem;
class USFHudWidget;
class USFHudService;
class SCanvas;

UCLASS()
class SMARTFOUNDATIONS_API USFHudLayoutEditorWidget : public UFGInteractWidget
{
    GENERATED_BODY()
public:
    USFHudLayoutEditorWidget(const FObjectInitializer& Initializer);
    void Setup(USFInterfaceLayoutSubsystem* Owner, FSimpleDelegate CloseHost);
    bool Finish(bool bCommit);
    void Close(bool bCommit);
    const SFInterface::FHudAppearance& GetAppearance() const { return Draft; }
    void PreviewAppearance(const SFInterface::FHudAppearance& Value);
    virtual void OnEscapePressed_Implementation() override;
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
    virtual void NativeDestruct() override;
private:
    UPROPERTY(Transient) TObjectPtr<USFInterfaceLayoutSubsystem> LayoutOwner;
    UPROPERTY(Transient) TObjectPtr<USFHudWidget> Preview;
    TWeakObjectPtr<USFHudService> HiddenHud;
    SFInterface::FHudAppearance Draft;
    FSimpleDelegate CloseHost;
    TArray<TSharedPtr<FText>> Themes;
    TSharedPtr<SCanvas> Canvas;
    FVector2D DragStartPosition, DragStartSize;
    float DragStartScale = 1;
    float RefreshTime = 0;
    bool bFinished = false;
    FText Error;
    FVector2D ScreenSize() const;
    FVector2D FrameSize() const;
    FVector2D FramePosition() const;
};
