// Copyright (c) 2024 SmartFoundations Mod. All Rights Reserved.
// Smart! Mod - UMG-based HUD Widget (Issue #179)
// Replaces Canvas-based DrawCounterToHUD for crisp text at any scale.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SFHudWidget.generated.h"

class UBorder;
class UVerticalBox;
class UHorizontalBox;
class UTextBlock;
class UImage;
class UTexture2D;

/**
 * UMG-based HUD overlay for Smart! grid counters and status display.
 * 
 * Created programmatically (no Blueprint/editor asset needed).
 * Uses FSlateFontInfo at native resolution — crisp text at any scale.
 * Replaces the old Canvas-based DrawCounterToHUD which pixelated at 2-3x.
 *
 * Widget tree (built in NativeConstruct):
 *   UBorder (FICSIT Orange border)
 *     └─ UBorder (dark background)
 *          └─ UVerticalBox
 *               ├─ UTextBlock (header)
 *               ├─ UTextBlock (line 1)
 *               ├─ UHorizontalBox (grid line with colored segments)
 *               ├─ UImage (recipe icon)
 *               └─ ... (pooled text blocks, shown/hidden as needed)
 */
UCLASS()
class SMARTFOUNDATIONS_API USFHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Configure the HUD scale (triggers widget rebuild if scale changed) */
	void SetHudScale(float InScale);

	/** Apply a color theme (0=Default, 1=Dark, 2=Classic, 3=High Contrast, 4=Minimal, 5=Monochrome) */
	void SetTheme(int32 ThemeIndex);

	/** Update all display content from parsed lines. Called each frame by SFHudService. */
	void UpdateContent(const TArray<FString>& Lines, UTexture2D* RecipeIcon);

protected:
	virtual void NativeOnInitialized() override;

private:
	// ========================================================================
	// COLOR THEME SYSTEM (Issue #179)
	// ========================================================================
	struct FHudTheme
	{
		FLinearColor Border;
		FLinearColor Background;
		FLinearColor HeaderText;
		FLinearColor BodyText;
		FLinearColor ActiveText;
		FLinearColor AxisRed;
		FLinearColor AxisGreen;
		FLinearColor AxisBlue;
	};

	static const FHudTheme Themes[6];
	FHudTheme CurrentTheme;
	int32 CachedThemeIndex = -1;  // Force first apply

	/** Apply current theme colors to all existing widgets */
	void ApplyThemeColors();

	// Widget tree
	UPROPERTY()
	UBorder* OuterBorder = nullptr;

	UPROPERTY()
	UBorder* InnerBorder = nullptr;

	UPROPERTY()
	UVerticalBox* ContentBox = nullptr;

	// Pre-allocated text line pool
	static constexpr int32 MaxTextLines = 24;

	UPROPERTY()
	TArray<UTextBlock*> TextPool;

	// Colored grid line (reused when grid line is present)
	UPROPERTY()
	UHorizontalBox* GridLineBox = nullptr;

	UPROPERTY()
	TArray<UTextBlock*> GridSegments;  // 8 segments: "Grid: " X "x" Y "x" Z " (N)"

	// Recipe icon
	UPROPERTY()
	UImage* RecipeIconWidget = nullptr;

	// Build helpers
	void BuildWidgetTree();
	UTextBlock* CreateTextBlock(const FSlateFontInfo& Font, const FLinearColor& Color);
	FSlateFontInfo MakeFont(const FString& TypefaceName, int32 Size) const;

	/** Parse a grid line like "Grid: 2x3x1 (6)" into colored segments */
	bool TryUpdateGridLine(const FString& LineText);

	// Tracking
	int32 ActiveLineCount = 0;
	float CachedScale = 1.5f;
};
