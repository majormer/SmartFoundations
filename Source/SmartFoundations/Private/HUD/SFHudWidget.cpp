// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
// Smart! Mod - UMG-based HUD Widget Implementation (Issue #179)

#include "HUD/SFHudWidget.h"
#include "SmartFoundations.h"
#include "UI/SFFontLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Styling/SlateTypes.h"
#include "Engine/Texture2D.h"

#define LOCTEXT_NAMESPACE "SmartFoundations"

void USFHudWidget::SetPlayerRelative(bool bEnabled)
{
	if (ModeLabel) ModeLabel->SetText(bEnabled ? LOCTEXT("HUD_Mode_PlayerRelative", "Controls: Player Relative") : LOCTEXT("HUD_Mode_BuildingRelative", "Controls: Building Relative"));
}

// ========================================================================
// HUD THEME PALETTES (Issue #179)
// 0=Default (FICSIT), 1=Dark, 2=Classic, 3=High Contrast, 4=Minimal, 5=Monochrome
// ========================================================================
const USFHudWidget::FHudTheme USFHudWidget::Themes[6] = {
	// 0: Default (FICSIT Orange)
	{
		FLinearColor(0.898f, 0.580f, 0.271f, 1.0f),   // Border: #E59445
		FLinearColor(0.168f, 0.168f, 0.168f, 0.92f),   // Background: #2B2B2B
		FLinearColor(0.898f, 0.580f, 0.271f, 1.0f),   // Header: FICSIT Orange
		FLinearColor(0.937f, 0.937f, 0.937f, 1.0f),   // Body: #EFEFEF
		FLinearColor(1.0f, 0.8f, 0.0f, 1.0f),          // Active: #FFCC00
		FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),          // Axis Red
		FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),          // Axis Green
		FLinearColor(0.0f, 0.0f, 1.0f, 1.0f),          // Axis Blue
	},
	// 1: Dark (muted, low-key)
	{
		FLinearColor(0.333f, 0.333f, 0.333f, 0.6f),   // Border: subtle gray
		FLinearColor(0.1f, 0.1f, 0.1f, 0.95f),         // Background: near-black
		FLinearColor(0.667f, 0.667f, 0.667f, 1.0f),   // Header: #AAA
		FLinearColor(0.533f, 0.533f, 0.533f, 1.0f),   // Body: #888
		FLinearColor(0.733f, 0.733f, 0.733f, 1.0f),   // Active: #BBB
		FLinearColor(0.7f, 0.2f, 0.2f, 1.0f),          // Axis Red (muted)
		FLinearColor(0.2f, 0.7f, 0.2f, 1.0f),          // Axis Green (muted)
		FLinearColor(0.3f, 0.3f, 0.8f, 1.0f),          // Axis Blue (muted)
	},
	// 2: Classic (green terminal)
	{
		FLinearColor(0.0f, 1.0f, 0.0f, 0.8f),          // Border: bright green
		FLinearColor(0.04f, 0.04f, 0.04f, 0.95f),      // Background: near-black
		FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),          // Header: green
		FLinearColor(0.0f, 0.8f, 0.0f, 1.0f),          // Body: #00CC00
		FLinearColor(1.0f, 1.0f, 0.0f, 1.0f),          // Active: yellow
		FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),          // Axis Red
		FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),          // Axis Green
		FLinearColor(0.3f, 0.3f, 1.0f, 1.0f),          // Axis Blue
	},
	// 3: High Contrast (bright white)
	{
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Border: white
		FLinearColor(0.0f, 0.0f, 0.0f, 0.98f),         // Background: pure black
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Header: white
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Body: white
		FLinearColor(1.0f, 1.0f, 0.0f, 1.0f),          // Active: yellow
		FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),          // Axis Red
		FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),          // Axis Green
		FLinearColor(0.3f, 0.3f, 1.0f, 1.0f),          // Axis Blue
	},
	// 4: Minimal (no border, semi-transparent)
	{
		FLinearColor(0.0f, 0.0f, 0.0f, 0.0f),          // Border: invisible
		FLinearColor(0.0f, 0.0f, 0.0f, 0.85f),         // Borderless, readable over bright scenery
		FLinearColor(0.937f, 0.937f, 0.937f, 1.0f),   // Header: light
		FLinearColor(0.8f, 0.8f, 0.8f, 1.0f),          // Body: #CCC
		FLinearColor(1.0f, 0.8f, 0.0f, 1.0f),          // Active: #FFCC00
		FLinearColor(1.0f, 0.0f, 0.0f, 1.0f),          // Axis Red
		FLinearColor(0.0f, 1.0f, 0.0f, 1.0f),          // Axis Green
		FLinearColor(0.0f, 0.0f, 1.0f, 1.0f),          // Axis Blue
	},
	// 5: Monochrome (pure white on black, no transparency)
	{
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Border: white
		FLinearColor(0.0f, 0.0f, 0.0f, 1.0f),          // Background: pure black
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Header: white
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Body: white
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Active: white
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Axis X: white (monochrome)
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Axis Y: white (monochrome)
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),          // Axis Z: white (monochrome)
	},
};

// ========================================================================
// LIFECYCLE
// ========================================================================

USFHudWidget::FHudTheme USFHudWidget::ToLinearTheme(const FHudTheme& Theme)
{
	// The palette above specifies display/sRGB colors. Slate consumes linear
	// tints; interpreting those numbers as linear washes out the intended theme.
	auto Linear = [](FLinearColor Color)
	{
		auto Result = FLinearColor::FromSRGBColor(Color.ToFColor(false));
		Result.A = Color.A; return Result;
	};
	FHudTheme Result{Linear(Theme.Border),Linear(Theme.Background),Linear(Theme.HeaderText),Linear(Theme.BodyText),Linear(Theme.ActiveText),Linear(Theme.AxisRed),Linear(Theme.AxisGreen),Linear(Theme.AxisBlue)};
	// Keep the branded foreground colors by strengthening the dark backdrops first.
	if (Result.Background.A >= .9f) Result.Background.A = FMath::Max(Result.Background.A, .98f);
	// Bound contrast against white scenery behind the translucent background.
	// Lift only colors that need it, retaining each theme's hue and hierarchy.
	auto Luminance = [](FLinearColor Color) { return .2126f * Color.R + .7152f * Color.G + .0722f * Color.B; };
	const float Background = Luminance(Result.Background) * Result.Background.A + 1.f - Result.Background.A;
	const float MinimumText = 4.5f * (Background + .05f) - .05f;
	for (FLinearColor* Color : {&Result.HeaderText, &Result.BodyText, &Result.ActiveText, &Result.AxisRed, &Result.AxisGreen, &Result.AxisBlue})
	{
		const float Light = Luminance(*Color);
		if (Light < MinimumText)
			*Color = FMath::Lerp(*Color, FLinearColor::White, FMath::Clamp((MinimumText - Light) / (1.f - Light), 0.f, 1.f));
	}
	return Result;
}

void USFHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CurrentTheme = ToLinearTheme(Themes[0]);
	BuildWidgetTree();
}

void USFHudWidget::SetTheme(int32 ThemeIndex)
{
	ThemeIndex = FMath::Clamp(ThemeIndex, 0, 5);
	if (ThemeIndex == CachedThemeIndex) return;

	CachedThemeIndex = ThemeIndex;
	CurrentTheme = ToLinearTheme(Themes[ThemeIndex]);
	ApplyThemeColors();

	UE_LOG(LogSmartFoundations, Verbose, TEXT("SFHudWidget: Theme changed to %d"), ThemeIndex);
}

void USFHudWidget::ApplyThemeColors()
{
	for (const TCHAR* Name : {TEXT("HeaderDivider"),TEXT("GridDivider"),TEXT("HintDivider")})
		if (auto* Rule = Cast<UBorder>(WidgetTree->FindWidget(Name))) Rule->SetBrushColor(CurrentTheme.BodyText.CopyWithNewOpacity(.2f));
	if (OuterBorder)
	{
		OuterBorder->SetBrushColor(CurrentTheme.Border);
	}
	if (InnerBorder)
	{
		InnerBorder->SetBrushColor(CurrentTheme.Background);
	}
	// Text colors are applied each frame in UpdateContent based on CurrentTheme
	// Grid segment colors need updating
	if (GridSegments.Num() >= 8)
	{
		const FLinearColor SegColors[] = {
			CurrentTheme.BodyText, CurrentTheme.AxisRed, CurrentTheme.BodyText,
			CurrentTheme.AxisGreen, CurrentTheme.BodyText, CurrentTheme.AxisBlue,
			CurrentTheme.BodyText, CurrentTheme.BodyText
		};
		for (int32 i = 0; i < 8; ++i)
		{
			if (GridSegments[i])
			{
				GridSegments[i]->SetColorAndOpacity(FSlateColor(SegColors[i]));
			}
		}
	}
}

void USFHudWidget::SetHudScale(float InScale)
{
	if (!FMath::IsNearlyEqual(CachedScale, InScale))
	{
		CachedScale = InScale;
		if (auto* Width = Cast<USizeBox>(WidgetTree->RootWidget)) Width->SetMinDesiredWidth(250 * CachedScale);
		// Update font sizes on existing widgets (don't rebuild tree — Slate representation
		// is finalized after AddToViewport/TakeWidget, new widgets would be orphaned)
		const int32 HeaderFontSize = FMath::RoundToInt(16.0f * CachedScale);
		const int32 BodyFontSize = FMath::RoundToInt(14.0f * CachedScale);

		for (int32 i = 0; i < TextPool.Num(); ++i)
		{
			if (TextPool[i])
			{
				TextPool[i]->SetFont(SFFont::Get((i == 0) ? HeaderFontSize : BodyFontSize));
			}
		}
		for (UTextBlock* Seg : GridSegments)
		{
			if (Seg)
			{
				Seg->SetFont(SFFont::Get(BodyFontSize));
			}
		}
		for (UTextBlock* Value : ValuePool) if (Value) Value->SetFont(SFFont::Get(BodyFontSize));
		if (ModeLabel) ModeLabel->SetFont(SFFont::Get(FMath::RoundToInt(11 * CachedScale)));
		for (auto* Hint : {HintLabel.Get(),HintKeys.Get()}) if (Hint) Hint->SetFont(SFFont::Get(FMath::RoundToInt(11 * CachedScale)));
		if (RecipeIconWidget)
		{
			const float IconSize = 48.0f * CachedScale;
			RecipeIconWidget->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		}

		UE_LOG(LogSmartFoundations, Verbose, TEXT("SFHudWidget: Scale changed to %.1f (header=%d, body=%d)"),
			CachedScale, HeaderFontSize, BodyFontSize);
	}
}

// ========================================================================
// WIDGET TREE CONSTRUCTION
// ========================================================================

FSlateFontInfo USFHudWidget::MakeFont(const FString& TypefaceName, int32 Size) const
{
	// Use the in-game multi-script FactoryFont (via SFFont) so the HUD renders every
	// supported language (Arabic/Persian/Thai/CJK), not just the Latin engine default.
	// TypefaceName is currently unused: FactoryFont is an offline single-face font.
	return SFFont::Get(Size);
}

UTextBlock* USFHudWidget::CreateTextBlock(const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	// Use the provided font (FactoryFont via SFFont::Get) directly. Do NOT fall back to the
	// block's engine-default font, or non-Latin scripts (Arabic/Persian/Thai) won't render.
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetShadowOffset(FVector2D(1.0f, 1.0f));
	Block->SetShadowColorAndOpacity(FLinearColor::Black);
	Block->SetVisibility(ESlateVisibility::Collapsed);
	return Block;
}

void USFHudWidget::BuildWidgetTree()
{
	if (!WidgetTree)
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("SFHudWidget::BuildWidgetTree: WidgetTree is null!"));
		return;
	}
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFHudWidget::BuildWidgetTree: Starting (scale=%.1f)"), CachedScale);

	// Clear existing content
	TextPool.Empty();
	ValuePool.Empty(); LineFrames.Empty();
	GridSegments.Empty();
	OuterBorder = nullptr;
	InnerBorder = nullptr;
	ContentBox = nullptr;
	GridLineBox = nullptr;
	RecipeIconWidget = nullptr;

	const int32 HeaderFontSize = FMath::RoundToInt(16.0f * CachedScale);
	const int32 BodyFontSize = FMath::RoundToInt(14.0f * CachedScale);
	const FSlateFontInfo HeaderFont = MakeFont(TEXT("Bold"), HeaderFontSize);
	const FSlateFontInfo BodyFont = MakeFont(TEXT("Regular"), BodyFontSize);

	// Outer border (theme border color — acts as visible border via padding)
	OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterBorder"));
	OuterBorder->SetBrushColor(CurrentTheme.Border);
	OuterBorder->SetPadding(FMargin(3.0f, 1.0f, 1.0f, 1.0f));

	// Inner border (theme background with content padding)
	InnerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InnerBorder"));
	InnerBorder->SetBrushColor(CurrentTheme.Background);
	InnerBorder->SetPadding(FMargin(12.0f, 10.0f));

	// Content container
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));

	// Build the hierarchy
	auto* Width = WidgetTree->ConstructWidget<USizeBox>(); Width->SetMinDesiredWidth(250 * CachedScale); Width->SetContent(OuterBorder);
	WidgetTree->RootWidget = Width;
	OuterBorder->AddChild(InnerBorder);
	InnerBorder->AddChild(ContentBox);
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFHudWidget::BuildWidgetTree: Root=%s, ContentBox=%s"),
		OuterBorder ? TEXT("OK") : TEXT("NULL"),
		ContentBox ? TEXT("OK") : TEXT("NULL"));

	auto Divider = [this](FName Name)
	{
		auto* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name); Rule->SetBrushColor(CurrentTheme.BodyText.CopyWithNewOpacity(.2f)); Rule->SetPadding(FMargin(0));
		auto* Height = WidgetTree->ConstructWidget<USizeBox>(); Height->SetHeightOverride(1); Rule->SetContent(Height);
		ContentBox->AddChildToVerticalBox(Rule)->SetPadding(FMargin(0,4));
	};
	// Pre-allocate text line pool
	for (int32 i = 0; i < MaxTextLines; ++i)
	{
		const FSlateFontInfo& Font = (i == 0) ? HeaderFont : BodyFont;
		const FLinearColor& Color = (i == 0) ? CurrentTheme.HeaderText : CurrentTheme.BodyText;
		UTextBlock* Block = CreateTextBlock(Font, Color);
		TextPool.Add(Block);

		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		auto* LabelSlot = Row->AddChildToHorizontalBox(Block);
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		auto* Value = CreateTextBlock(BodyFont, CurrentTheme.BodyText);
		ValuePool.Add(Value);
		auto* ValueSlot = Row->AddChildToHorizontalBox(Value); ValueSlot->SetPadding(FMargin(20,0,0,0)); ValueSlot->SetVerticalAlignment(VAlign_Center);
		auto* Frame = WidgetTree->ConstructWidget<UBorder>();
		Frame->SetBrushColor(FLinearColor::Transparent); Frame->SetPadding(FMargin(0,4)); Frame->SetContent(Row); Frame->SetVisibility(ESlateVisibility::Collapsed);
		LineFrames.Add(Frame); ContentBox->AddChildToVerticalBox(Frame);
		if (i == 0)
		{
			ModeLabel = CreateTextBlock(SFFont::Get(FMath::RoundToInt(11 * CachedScale)), CurrentTheme.BodyText);
			ModeLabel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            auto* ModeSlot = Row->AddChildToHorizontalBox(ModeLabel);
            ModeSlot->SetVerticalAlignment(VAlign_Center);
            ModeSlot->SetPadding(FMargin(16, 0, 0, 0));
			SetPlayerRelative(false);
			Frame->SetPadding(FMargin(0,0,0,10));
			Divider(TEXT("HeaderDivider"));
		}
	}

	Divider(TEXT("GridDivider"));
	// Grid line (colored segments in a horizontal box)
	GridLineBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("GridLineBox"));
	GridLineBox->SetVisibility(ESlateVisibility::Collapsed);
	UVerticalBoxSlot* GridSlot = ContentBox->AddChildToVerticalBox(GridLineBox);
	if (GridSlot)
	{
		GridSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
	}

	// 8 segments: "Grid: ", X, "x", Y, "x", Z, " ", "(N)"
	const FLinearColor SegColors[] = {
		CurrentTheme.BodyText, CurrentTheme.AxisRed, CurrentTheme.BodyText,
		CurrentTheme.AxisGreen, CurrentTheme.BodyText, CurrentTheme.AxisBlue,
		CurrentTheme.BodyText, CurrentTheme.BodyText
	};
	for (int32 i = 0; i < 8; ++i)
	{
		UTextBlock* Seg = CreateTextBlock(BodyFont, SegColors[i]);
		Seg->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		GridSegments.Add(Seg);
		auto* SegmentSlot = GridLineBox->AddChildToHorizontalBox(Seg);
		if (i == 0) {SegmentSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); SegmentSlot->SetPadding(FMargin(0,0,20,0));}
	}
	Divider(TEXT("HintDivider"));
	HintLineBox = WidgetTree->ConstructWidget<UHorizontalBox>();
	HintLabel = CreateTextBlock(SFFont::Get(FMath::RoundToInt(11 * CachedScale)), CurrentTheme.BodyText);
	HintKeys = CreateTextBlock(SFFont::Get(FMath::RoundToInt(11 * CachedScale)), CurrentTheme.BodyText);
	HintLineBox->AddChildToHorizontalBox(HintLabel)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	HintLineBox->AddChildToHorizontalBox(HintKeys)->SetPadding(FMargin(20,0,0,0));
	ContentBox->AddChildToVerticalBox(HintLineBox)->SetPadding(FMargin(0,6,0,0));

	// Recipe icon
	RecipeIconWidget = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RecipeIcon"));
	RecipeIconWidget->SetVisibility(ESlateVisibility::Collapsed);
	const float IconSize = 48.0f * CachedScale;
	RecipeIconWidget->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
	UVerticalBoxSlot* IconSlot = ContentBox->AddChildToVerticalBox(RecipeIconWidget);
	if (IconSlot)
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		IconSlot->SetHorizontalAlignment(HAlign_Left);
	}

	UE_LOG(LogSmartFoundations, Verbose, TEXT("SFHudWidget: Built widget tree (scale=%.1f, headerFont=%d, bodyFont=%d)"),
		CachedScale, HeaderFontSize, BodyFontSize);
}

// ========================================================================
// CONTENT UPDATE (called each frame by SFHudService)
// ========================================================================

void USFHudWidget::UpdateContent(const TArray<FString>& Lines, UTexture2D* RecipeIcon)
{
	if (!ContentBox || TextPool.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFHudWidget::UpdateContent: ContentBox=%s, TextPool=%d — early exit"),
			ContentBox ? TEXT("OK") : TEXT("NULL"), TextPool.Num());
		return;
	}

	static int32 LogCounter = 0;
	if (LogCounter++ % 120 == 0)  // Log every ~2 seconds at 60fps
	{
		UE_LOG(LogSmartFoundations, Verbose, TEXT("SFHudWidget::UpdateContent: %d lines, RecipeIcon=%s, TextPool=%d"),
			Lines.Num(), RecipeIcon ? TEXT("YES") : TEXT("NO"), TextPool.Num());
		for (int32 i = 0; i < FMath::Min(Lines.Num(), 3); ++i)
		{
			UE_LOG(LogSmartFoundations, Verbose, TEXT("  Line[%d]: '%s'"), i, *Lines[i]);
		}
	}

	int32 PoolIndex = 0;
	bool bGridLineUsed = false;
	bool bRecipeIconUsed = false;
	bool bHintUsed = false;

	for (int32 i = 0; i < Lines.Num() && PoolIndex < MaxTextLines; ++i)
	{
		FString LineText = Lines[i];
		if (LineText.RemoveFromStart(SFHud::HintLinePrefix))
		{
			FString Label, Keys;
			if (LineText.Split(TEXT("\t"), &Label, &Keys))
			{
				HintLabel->SetText(FText::FromString(Label)); HintKeys->SetText(FText::FromString(Keys));
				HintLabel->SetColorAndOpacity(CurrentTheme.BodyText); HintKeys->SetColorAndOpacity(CurrentTheme.BodyText);
				HintLabel->SetVisibility(ESlateVisibility::SelfHitTestInvisible); HintKeys->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				bHintUsed = true;
			}
			continue;
		}

		// ---- Recipe Icon ----
		if (LineText.Equals(TEXT("[RECIPE_ICON]")))
		{
			if (RecipeIcon && RecipeIconWidget)
			{
				RecipeIconWidget->SetBrushFromTexture(RecipeIcon);
				RecipeIconWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				bRecipeIconUsed = true;
			}
			continue;
		}

		// ---- Warning marker (fixed red, theme-independent) ----
		// Auto-Connect skip summaries and guard notices carry SFHud::WarningLinePrefix so they
		// always read as red regardless of the active HUD theme. Checked before the asterisk
		// so these lines get warning-red instead of the theme's Active color.
		bool bIsWarning = LineText.StartsWith(SFHud::WarningLinePrefix);
		if (bIsWarning)
		{
			LineText.RemoveFromStart(SFHud::WarningLinePrefix);
		}

		// ---- Active marker (asterisk) ----
		bool bIsActive = LineText.Contains(TEXT("*"));
		if (bIsActive)
		{
			LineText = LineText.Replace(TEXT("*"), TEXT(""));
		}

		// ---- Colored Grid Line ----
		if (i > 0 && LineText.StartsWith(TEXT("Grid: ")))
		{
			if (TryUpdateGridLine(LineText))
			{
				bGridLineUsed = true;
				// Move grid line box to current position in the vertical box
				GridLineBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
				continue;
			}
			// Fall through to plain text if parsing fails
		}

		// ---- Standard Text Line ----
		UTextBlock* Block = TextPool[PoolIndex];
		if (!Block)
		{
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFHudWidget: TextPool[%d] is NULL!"), PoolIndex);
			PoolIndex++;
			continue;
		}
		FString LabelText = LineText, ValueText;
		// Only split the established active counter format. Warnings and contextual
		// messages remain intact, including localized text with different punctuation.
		int32 Colon = INDEX_NONE;
		if (i > 0 && !bIsWarning && LineText.FindLastChar(TEXT(':'), Colon))
		{
			LabelText = LineText.Left(Colon); ValueText = LineText.Mid(Colon + 1).TrimStartAndEnd();
		}
		Block->SetText(FText::FromString(LabelText));
		ValuePool[PoolIndex]->SetText(FText::FromString(ValueText));
		ValuePool[PoolIndex]->SetVisibility(ValueText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		ValuePool[PoolIndex]->SetColorAndOpacity(bIsActive ? CurrentTheme.ActiveText : CurrentTheme.BodyText);
		ValuePool[PoolIndex]->SetFont(SFFont::Get(FMath::RoundToInt((bIsActive ? 18 : 14) * CachedScale)));
		LineFrames[PoolIndex]->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (i == 0 && ModeLabel) ModeLabel->SetColorAndOpacity(CurrentTheme.BodyText);

		// Color logic
		const bool bIsHeader = (i == 0);
        if (auto* LineSlot = Cast<UVerticalBoxSlot>(Block->Slot))
            LineSlot->SetPadding(FMargin(0, bIsActive ? 3.f : 0.f, 0, bIsHeader ? 8.f : bIsActive ? 5.f : 3.f));
		if (bIsWarning)
		{
			// Fixed warning red, deliberately NOT from the theme palette - a skipped
			// connection must read the same (and unmistakably) under every theme.
			FLinearColor WarningRed(0.95f, 0.20f, 0.15f, 1.0f);
			const float Background = (.2126f * CurrentTheme.Background.R + .7152f * CurrentTheme.Background.G + .0722f * CurrentTheme.Background.B) * CurrentTheme.Background.A + 1.f - CurrentTheme.Background.A;
			const float Light = .2126f * WarningRed.R + .7152f * WarningRed.G + .0722f * WarningRed.B;
			const float MinimumText = 4.5f * (Background + .05f) - .05f;
			if (Light < MinimumText) WarningRed = FMath::Lerp(WarningRed, FLinearColor::White, FMath::Clamp((MinimumText - Light) / (1.f - Light), 0.f, 1.f));
			Block->SetColorAndOpacity(FSlateColor(WarningRed));
		}
		else if (bIsHeader)
		{
			Block->SetColorAndOpacity(FSlateColor(CurrentTheme.HeaderText));
		}
		else if (bIsActive)
		{
			Block->SetColorAndOpacity(FSlateColor(CurrentTheme.ActiveText));
		}
		else
		{
			Block->SetColorAndOpacity(FSlateColor(CurrentTheme.BodyText));
		}

		Block->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		PoolIndex++;
	}

	// Hide unused pool entries
	for (int32 i = PoolIndex; i < MaxTextLines; ++i)
	{
		TextPool[i]->SetVisibility(ESlateVisibility::Collapsed);
		LineFrames[i]->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Hide grid line box if not used this frame
	if (!bGridLineUsed && GridLineBox)
	{
		GridLineBox->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (auto* Rule = WidgetTree->FindWidget(TEXT("GridDivider"))) Rule->SetVisibility(bGridLineUsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	HintLineBox->SetVisibility(bHintUsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (auto* Rule = WidgetTree->FindWidget(TEXT("HintDivider"))) Rule->SetVisibility(bHintUsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	// Hide recipe icon if not used this frame
	if (!bRecipeIconUsed && RecipeIconWidget)
	{
		RecipeIconWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	ActiveLineCount = PoolIndex;
}

bool USFHudWidget::TryUpdateGridLine(const FString& LineText)
{
	if (!GridLineBox || GridSegments.Num() < 8) return false;

	// Parse "Grid: 2x3x1 (6)"
	const FString PrefixText = TEXT("Grid: ");
	const FString AfterPrefix = LineText.Mid(PrefixText.Len());

	TArray<FString> GridTokens;
	AfterPrefix.ParseIntoArray(GridTokens, TEXT(" "), true);
	if (GridTokens.Num() < 1) return false;

	TArray<FString> AxisParts;
	GridTokens[0].ParseIntoArray(AxisParts, TEXT("x"), true);
	if (AxisParts.Num() != 3) return false;

	// Tail text (e.g., "(6)")
	FString TailText;
	if (GridTokens.Num() > 1)
	{
		TailText = TEXT(" ");
		for (int32 t = 1; t < GridTokens.Num(); ++t)
		{
			if (t > 1) TailText += TEXT(" ");
			TailText += GridTokens[t];
		}
	}

	// Set segments: "Grid: " X "x" Y "x" Z " " "(N)"
	GridSegments[0]->SetText(FText::FromString(PrefixText));
	GridSegments[1]->SetText(FText::FromString(AxisParts[0]));  // X (red)
	GridSegments[2]->SetText(FText::FromString(TEXT("x")));
	GridSegments[3]->SetText(FText::FromString(AxisParts[1]));  // Y (green)
	GridSegments[4]->SetText(FText::FromString(TEXT("x")));
	GridSegments[5]->SetText(FText::FromString(AxisParts[2]));  // Z (blue)
	GridSegments[6]->SetText(FText::FromString(TEXT("")));
	GridSegments[7]->SetText(FText::FromString(TailText));

	return true;
}

#undef LOCTEXT_NAMESPACE
