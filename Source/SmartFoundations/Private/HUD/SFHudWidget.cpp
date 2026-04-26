// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
// Smart! Mod - UMG-based HUD Widget Implementation (Issue #179)

#include "HUD/SFHudWidget.h"
#include "SmartFoundations.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Styling/SlateTypes.h"
#include "Engine/Texture2D.h"

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
		FLinearColor(0.0f, 0.0f, 0.0f, 0.4f),          // Background: faint
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

void USFHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CurrentTheme = Themes[0];  // Default theme until config loads
	BuildWidgetTree();
}

void USFHudWidget::SetTheme(int32 ThemeIndex)
{
	ThemeIndex = FMath::Clamp(ThemeIndex, 0, 5);
	if (ThemeIndex == CachedThemeIndex) return;

	CachedThemeIndex = ThemeIndex;
	CurrentTheme = Themes[ThemeIndex];
	ApplyThemeColors();

	UE_LOG(LogSmartFoundations, Log, TEXT("SFHudWidget: Theme changed to %d"), ThemeIndex);
}

void USFHudWidget::ApplyThemeColors()
{
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
		// Update font sizes on existing widgets (don't rebuild tree — Slate representation
		// is finalized after AddToViewport/TakeWidget, new widgets would be orphaned)
		const int32 HeaderFontSize = FMath::RoundToInt(24.0f * CachedScale);
		const int32 BodyFontSize = FMath::RoundToInt(19.0f * CachedScale);

		for (int32 i = 0; i < TextPool.Num(); ++i)
		{
			if (TextPool[i])
			{
				FSlateFontInfo Font = TextPool[i]->GetFont();
				Font.Size = (i == 0) ? HeaderFontSize : BodyFontSize;
				TextPool[i]->SetFont(Font);
			}
		}
		for (UTextBlock* Seg : GridSegments)
		{
			if (Seg)
			{
				FSlateFontInfo Font = Seg->GetFont();
				Font.Size = BodyFontSize;
				Seg->SetFont(Font);
			}
		}
		if (RecipeIconWidget)
		{
			const float IconSize = 48.0f * CachedScale;
			RecipeIconWidget->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		}

		UE_LOG(LogSmartFoundations, Log, TEXT("SFHudWidget: Scale changed to %.1f (header=%d, body=%d)"),
			CachedScale, HeaderFontSize, BodyFontSize);
	}
}

// ========================================================================
// WIDGET TREE CONSTRUCTION
// ========================================================================

FSlateFontInfo USFHudWidget::MakeFont(const FString& TypefaceName, int32 Size) const
{
	// Use the default UMG TextBlock font (always available in cooked games).
	// Raw TTF paths like EngineContent/Slate/Fonts/Roboto-*.ttf are stripped in packaged builds.
	// We grab the default font and only change the size — don't touch TypefaceFontName
	// since the composite font's typeface entries may not match "Bold"/"Regular".
	FSlateFontInfo Font;
	if (UTextBlock* TempBlock = NewObject<UTextBlock>())
	{
		Font = TempBlock->GetFont();
		TempBlock->MarkAsGarbage();
	}
	Font.Size = Size;
	// Note: TypefaceName parameter reserved for future use if we find the correct
	// composite font entry names. For now, default typeface + size is sufficient.
	return Font;
}

UTextBlock* USFHudWidget::CreateTextBlock(const FSlateFontInfo& Font, const FLinearColor& Color)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	// Get the block's own default font (valid in widget tree context), then override just the size
	FSlateFontInfo ActualFont = Block->GetFont();
	ActualFont.Size = Font.Size;
	Block->SetFont(ActualFont);
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
		UE_LOG(LogSmartFoundations, Warning, TEXT("SFHudWidget::BuildWidgetTree: WidgetTree is null!"));
		return;
	}
	UE_LOG(LogSmartFoundations, Log, TEXT("SFHudWidget::BuildWidgetTree: Starting (scale=%.1f)"), CachedScale);

	// Clear existing content
	TextPool.Empty();
	GridSegments.Empty();
	OuterBorder = nullptr;
	InnerBorder = nullptr;
	ContentBox = nullptr;
	GridLineBox = nullptr;
	RecipeIconWidget = nullptr;

	const int32 HeaderFontSize = FMath::RoundToInt(24.0f * CachedScale);
	const int32 BodyFontSize = FMath::RoundToInt(19.0f * CachedScale);
	const FSlateFontInfo HeaderFont = MakeFont(TEXT("Bold"), HeaderFontSize);
	const FSlateFontInfo BodyFont = MakeFont(TEXT("Regular"), BodyFontSize);

	// Outer border (theme border color — acts as visible border via padding)
	OuterBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterBorder"));
	OuterBorder->SetBrushColor(CurrentTheme.Border);
	OuterBorder->SetPadding(FMargin(2.0f));  // Border thickness

	// Inner border (theme background with content padding)
	InnerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InnerBorder"));
	InnerBorder->SetBrushColor(CurrentTheme.Background);
	InnerBorder->SetPadding(FMargin(16.0f));

	// Content container
	ContentBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ContentBox"));

	// Build the hierarchy
	WidgetTree->RootWidget = OuterBorder;
	OuterBorder->AddChild(InnerBorder);
	InnerBorder->AddChild(ContentBox);
	UE_LOG(LogSmartFoundations, Log, TEXT("SFHudWidget::BuildWidgetTree: Root=%s, ContentBox=%s"),
		OuterBorder ? TEXT("OK") : TEXT("NULL"),
		ContentBox ? TEXT("OK") : TEXT("NULL"));

	// Pre-allocate text line pool
	for (int32 i = 0; i < MaxTextLines; ++i)
	{
		const FSlateFontInfo& Font = (i == 0) ? HeaderFont : BodyFont;
		const FLinearColor& Color = (i == 0) ? CurrentTheme.HeaderText : CurrentTheme.BodyText;
		UTextBlock* Block = CreateTextBlock(Font, Color);
		TextPool.Add(Block);

		UVerticalBoxSlot* Slot = ContentBox->AddChildToVerticalBox(Block);
		if (Slot)
		{
			Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		}
	}

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
		GridLineBox->AddChildToHorizontalBox(Seg);
	}

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

	UE_LOG(LogSmartFoundations, Log, TEXT("SFHudWidget: Built widget tree (scale=%.1f, headerFont=%d, bodyFont=%d)"),
		CachedScale, HeaderFontSize, BodyFontSize);
}

// ========================================================================
// CONTENT UPDATE (called each frame by SFHudService)
// ========================================================================

void USFHudWidget::UpdateContent(const TArray<FString>& Lines, UTexture2D* RecipeIcon)
{
	if (!ContentBox || TextPool.Num() == 0)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("SFHudWidget::UpdateContent: ContentBox=%s, TextPool=%d — early exit"),
			ContentBox ? TEXT("OK") : TEXT("NULL"), TextPool.Num());
		return;
	}

	static int32 LogCounter = 0;
	if (LogCounter++ % 120 == 0)  // Log every ~2 seconds at 60fps
	{
		UE_LOG(LogSmartFoundations, Log, TEXT("SFHudWidget::UpdateContent: %d lines, RecipeIcon=%s, TextPool=%d"),
			Lines.Num(), RecipeIcon ? TEXT("YES") : TEXT("NO"), TextPool.Num());
		for (int32 i = 0; i < FMath::Min(Lines.Num(), 3); ++i)
		{
			UE_LOG(LogSmartFoundations, Log, TEXT("  Line[%d]: '%s'"), i, *Lines[i]);
		}
	}

	int32 PoolIndex = 0;
	bool bGridLineUsed = false;
	bool bRecipeIconUsed = false;

	for (int32 i = 0; i < Lines.Num() && PoolIndex < MaxTextLines; ++i)
	{
		FString LineText = Lines[i];

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
			UE_LOG(LogSmartFoundations, Warning, TEXT("SFHudWidget: TextPool[%d] is NULL!"), PoolIndex);
			PoolIndex++;
			continue;
		}
		Block->SetText(FText::FromString(LineText));

		// Color logic
		const bool bIsHeader = (i == 0);
		if (bIsHeader)
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
	}

	// Hide grid line box if not used this frame
	if (!bGridLineUsed && GridLineBox)
	{
		GridLineBox->SetVisibility(ESlateVisibility::Collapsed);
	}

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
