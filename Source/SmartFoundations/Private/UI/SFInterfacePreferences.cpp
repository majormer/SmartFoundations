// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "UI/SFInterfacePreferences.h"
#include "SmartFoundations.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Widget.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertyInteger.h"

namespace
{
    struct FPanelDefault
    {
        TWeakObjectPtr<UUserWidget> Owner;
        FVector2D FrameTranslation, OwnerTranslation, CanvasPosition;
        bool bCanvasSlot = false;
    };
    TMap<TWeakObjectPtr<UWidget>, FPanelDefault> PanelDefaults;
    TWeakObjectPtr<UUserWidget> ActiveTaskPanel;
    TOptional<FVector2D> TaskWindowPosition;
    FVector2D TaskWindowViewport = FVector2D::ZeroVector;
    UConfigPropertySection* HudSection(UObject* Context)
    {
        UWorld* World = Context ? Context->GetWorld() : nullptr;
        UConfigManager* Manager = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UConfigManager>() : nullptr;
        UConfigPropertySection* Root = Manager ? Manager->GetConfigurationRootSection({TEXT("SmartFoundations"), TEXT("")}) : nullptr;
        return Root ? Cast<UConfigPropertySection>(Root->SectionProperties.FindRef(TEXT("HUD"))) : nullptr;
    }
    bool Geometry(UUserWidget* Owner, UWidget* Frame, FVector2D& Position, FVector2D& Size, FVector2D& Screen)
    {
        if (!Owner || !Frame) return false;
        const FGeometry View = UWidgetLayoutLibrary::GetViewportWidgetGeometry(Owner);
        Screen = View.GetLocalSize();
        const FGeometry Rect = Frame->GetCachedGeometry();
        Position = View.AbsoluteToLocal(Rect.LocalToAbsolute(FVector2D::ZeroVector));
        Size = View.AbsoluteToLocal(Rect.LocalToAbsolute(Rect.GetLocalSize())) - Position;
        return Screen.X > 1 && Screen.Y > 1 && Size.X > 1 && Size.Y > 1;
    }
}

SFInterface::FHudAppearance SFInterface::ReadHud(UObject* Context, bool bDefaults)
{
    FHudAppearance Result;
    if (const auto* Section = HudSection(Context))
    {
        auto Number = [Section, bDefaults](const TCHAR* Key, float Fallback)
        {
            const auto* Property = Cast<UConfigPropertyFloat>(Section->SectionProperties.FindRef(Key));
            return Property ? (bDefaults ? Property->DefaultValue : Property->Value) : Fallback;
        };
        Result.Position = {Number(TEXT("HUDPositionX"), .02f), Number(TEXT("HUDPositionY"), .25f)};
        Result.Scale = Number(TEXT("HUDScale"), 1);
        if (const auto* Theme = Cast<UConfigPropertyInteger>(Section->SectionProperties.FindRef(TEXT("HUDTheme"))))
            Result.Theme = bDefaults ? Theme->DefaultValue : Theme->Value;
    }
    Result.Position.X = FMath::Clamp(Result.Position.X, 0., 1.);
    Result.Position.Y = FMath::Clamp(Result.Position.Y, 0., 1.);
    Result.Scale = FMath::Clamp(FMath::IsFinite(Result.Scale) ? Result.Scale : 1.f, .5f, 3.f);
    Result.Theme = FMath::Clamp(Result.Theme, 0, 5);
    return Result;
}

bool SFInterface::WriteHud(UObject* Context, const FHudAppearance& Value)
{
    if (!FMath::IsFinite(Value.Scale) || Value.Position.ContainsNaN() || Value.Scale < .5f || Value.Scale > 3 || Value.Theme < 0 || Value.Theme > 5) return false;
    auto* Section = HudSection(Context);
    if (!Section) return false;
    auto* X = Cast<UConfigPropertyFloat>(Section->SectionProperties.FindRef(TEXT("HUDPositionX")));
    auto* Y = Cast<UConfigPropertyFloat>(Section->SectionProperties.FindRef(TEXT("HUDPositionY")));
    auto* Scale = Cast<UConfigPropertyFloat>(Section->SectionProperties.FindRef(TEXT("HUDScale")));
    auto* Theme = Cast<UConfigPropertyInteger>(Section->SectionProperties.FindRef(TEXT("HUDTheme")));
    if (!X || !Y || !Scale || !Theme) return false;
    X->Value = FMath::Clamp(Value.Position.X, 0., 1.); Y->Value = FMath::Clamp(Value.Position.Y, 0., 1.);
    Scale->Value = Value.Scale; Theme->Value = Value.Theme;
    X->MarkDirty(); Y->MarkDirty(); Scale->MarkDirty(); Theme->MarkDirty();
    Context->GetWorld()->GetGameInstance()->GetSubsystem<UConfigManager>()->FlushPendingSaves();
    return true;
}

FVector2D SFInterface::ClampPosition(FVector2D Position, FVector2D Size, FVector2D Viewport)
{
    if (Position.ContainsNaN()) Position = FVector2D::ZeroVector;
    return {FMath::Clamp(Position.X, 0., FMath::Max(0., Viewport.X - Size.X)),
            FMath::Clamp(Position.Y, 0., FMath::Max(0., Viewport.Y - Size.Y))};
}

void SFInterface::RestorePanel(UUserWidget* Owner, UWidget* Frame, FName Id, bool& bRestored)
{
    if (!Owner || !Owner->IsVisible()) return;
    if (ActiveTaskPanel.Get() != Owner) { bRestored = false; ActiveTaskPanel = Owner; }
    FVector2D Position, Size, Screen;
    if (bRestored || !Geometry(Owner, Frame, Position, Size, Screen)) return;
    // Capture native defaults before applying preferences. Walking can retain a
    // collapsed widget, so deleting the saved keys alone would not reset it.
    for (auto It = PanelDefaults.CreateIterator(); It; ++It) if (!It.Key().IsValid()) It.RemoveCurrent();
    if (!PanelDefaults.Contains(Frame))
    {
        FPanelDefault Default;
        Default.Owner = Owner; Default.FrameTranslation = Frame->GetRenderTransform().Translation;
        Default.OwnerTranslation = Owner->GetRenderTransform().Translation;
        if (auto* Slot = Cast<UCanvasPanelSlot>(Frame->Slot)) {Default.bCanvasSlot = true; Default.CanvasPosition = Slot->GetPosition();}
        PanelDefaults.Add(Frame, Default);
    }
    bRestored = true;
    float X = 0, Y = 0;
    if (!TaskWindowPosition.IsSet())
    {
        auto ReadPosition = [&](const FString& Key)
        {
            return GConfig && GConfig->GetFloat(TEXT("SmartInterface"), *(Key + TEXT("X")), X, GGameUserSettingsIni)
                && GConfig->GetFloat(TEXT("SmartInterface"), *(Key + TEXT("Y")), Y, GGameUserSettingsIni)
                && FMath::IsFinite(X) && FMath::IsFinite(Y);
        };
        // Migrate the first opened task's old preference. All tabs subsequently
        // share one normalized location, including across saves and resolutions.
        TaskWindowPosition = ReadPosition(TEXT("TaskWindow")) || ReadPosition(Id.ToString())
            ? FVector2D(X,Y) : ClampPosition(Position,Size,Screen) / Screen;
    }
    Frame->SetRenderTranslation(Frame->GetRenderTransform().Translation + ClampPosition(TaskWindowPosition.GetValue() * Screen,Size,Screen) - Position);
    TaskWindowViewport = Screen;
}

void SFInterface::ClampPanel(UUserWidget* Owner, UWidget* Frame)
{
    FVector2D Position, Size, Screen;
    if (Geometry(Owner, Frame, Position, Size, Screen))
    {
        if (ActiveTaskPanel.Get() == Owner && !TaskWindowViewport.Equals(Screen))
        {
            TaskWindowPosition = ClampPosition(Position, Size, Screen) / Screen;
            TaskWindowViewport = Screen;
        }
        Frame->SetRenderTranslation(Frame->GetRenderTransform().Translation + ClampPosition(Position, Size, Screen) - Position);
    }
}

void SFInterface::SavePanel(UUserWidget* Owner, UWidget* Frame, FName /*Id*/)
{
    FVector2D Position, Size, Screen;
    if (!GConfig || !Geometry(Owner, Frame, Position, Size, Screen)) return;
    Position = ClampPosition(Position, Size, Screen) / Screen;
    TaskWindowPosition = Position;
    const FString Key = TEXT("TaskWindow");
    GConfig->SetFloat(TEXT("SmartInterface"), *(Key + TEXT("X")), Position.X, GGameUserSettingsIni);
    GConfig->SetFloat(TEXT("SmartInterface"), *(Key + TEXT("Y")), Position.Y, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void SFInterface::ResetPanelPositions()
{
    TaskWindowPosition.Reset(); ActiveTaskPanel.Reset(); TaskWindowViewport = FVector2D::ZeroVector;
    for (const auto& Entry : PanelDefaults)
        if (UWidget* Frame = Entry.Key.Get())
        {
            Frame->SetRenderTranslation(Entry.Value.FrameTranslation);
            if (auto* Owner = Entry.Value.Owner.Get()) Owner->SetRenderTranslation(Entry.Value.OwnerTranslation);
            if (Entry.Value.bCanvasSlot)
                if (auto* Slot = Cast<UCanvasPanelSlot>(Frame->Slot)) Slot->SetPosition(Entry.Value.CanvasPosition);
        }
    if (!GConfig) return;
    GConfig->EmptySection(TEXT("SmartInterface"), GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

bool SFInterface::IsHeaderHit(UWidget* Header, const FPointerEvent& Event)
{
    return Header && Header->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition());
}
