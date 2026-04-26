// Copyright Epic Games, Inc. All Rights Reserved.
// Smart! Mod - Arrow Visualization Module Implementation (Redesigned for Task #17)

#include "Features/Arrows/SFArrowModule.h"
#include "SmartFoundations.h"
#include "DrawDebugHelpers.h"

FSFArrowModule::FSFArrowModule()
	: ColorScheme()
	, Config()
	, CurrentLastAxis(ELastAxisInput::None)
	, bLeftShiftPressed(false)
	, bLeftCtrlPressed(false)
	, bCurrentlyVisible(false)
	, LastWorld(nullptr)
{
	UE_LOG(LogSmartFoundations, Log, TEXT("FSFArrowModule: Initialized with DrawDebugDirectionalArrow rendering"));
}

// ========================================
// Primary API Implementation
// ========================================

void FSFArrowModule::UpdateArrows(
	UWorld* World,
	const FTransform& HologramTransform,
	ELastAxisInput LastAxis,
	bool bVisible)
{
	if (!bVisible)
	{
		bCurrentlyVisible = false;
		
		// Flush persistent debug lines to immediately clear arrows
		// This prevents the 1-second persistence causing visible arrows after toggle-off
		if (World)
		{
			FlushPersistentDebugLines(World);
			UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("FSFArrowModule: Flushed persistent debug lines on visibility=false"));
		}
		return;
	}

	if (!World)
	{
		UE_LOG(LogSmartFoundations, Error, 
			TEXT("FSFArrowModule::UpdateArrows: No world context provided"));
		return;
	}

	bCurrentlyVisible = true;
	CurrentLastAxis = LastAxis;

	// Store world reference for DrawArrows and legacy API compatibility
	LastWorld = World;

	FVector Location = HologramTransform.GetLocation();
	FRotator Rotation = HologramTransform.Rotator();

	// Debug log (verbose)
	static int32 UpdateCounter = 0;
	if (++UpdateCounter >= 60)
	{
		UpdateCounter = 0;
		UE_LOG(LogSmartFoundations, Verbose, 
			TEXT("FSFArrowModule::UpdateArrows: Drawing at %s, LastAxis=%d"), 
			*Location.ToString(), static_cast<int32>(LastAxis));
	}

	// Draw arrows at the hologram transform
	DrawArrows(World, Location, Rotation);
}

void FSFArrowModule::SetHighlightedAxis(ELastAxisInput Axis)
{
	CurrentLastAxis = Axis;
	UE_LOG(LogSmartFoundations, VeryVerbose, 
		TEXT("FSFArrowModule: Highlighted axis set to %d"), static_cast<uint8>(Axis));
}

void FSFArrowModule::DrawArrows(
	UWorld* World,
	const FVector& Location,
	const FRotator& Rotation)
{
	if (!World)
	{
		return;
	}

	// Store world reference for future calls
	LastWorld = World;

	// Calculate effective highlighted axis (considering modifiers)
	const ELastAxisInput HighlightedAxis = CalculateHighlightedAxis();

	// Calculate arrow origin (location + Z offset)
	const FVector ArrowOrigin = Location + FVector(0, 0, Config.ZOffset);

	// Arrow vectors (hologram-relative for X and Y, world-up for Z)
	// X (Red): Hologram forward
	const FVector XVector = Rotation.RotateVector(FVector(Config.VectorLength, 0, 0));
	// Y (Green): Hologram right
	const FVector YVector = Rotation.RotateVector(FVector(0, Config.VectorLength, 0));
	// Z (Blue): World up (no rotation)
	const FVector ZVector = FVector(0, 0, Config.VectorLength);

	// Draw each arrow (or only highlighted if ArrowCount=1)
	const bool bShowOnlyHighlighted = (Config.ArrowCount == 1 && HighlightedAxis != ELastAxisInput::None);

	if (!bShowOnlyHighlighted || HighlightedAxis == ELastAxisInput::X)
	{
		const float ThicknessX = GetThicknessForAxis(ELastAxisInput::X, HighlightedAxis);
		DrawSingleArrow(World, ArrowOrigin, ArrowOrigin + XVector, ColorScheme.ColorX, ThicknessX);
	}

	if (!bShowOnlyHighlighted || HighlightedAxis == ELastAxisInput::Y)
	{
		const float ThicknessY = GetThicknessForAxis(ELastAxisInput::Y, HighlightedAxis);
		DrawSingleArrow(World, ArrowOrigin, ArrowOrigin + YVector, ColorScheme.ColorY, ThicknessY);
	}

	if (!bShowOnlyHighlighted || HighlightedAxis == ELastAxisInput::Z)
	{
		const float ThicknessZ = GetThicknessForAxis(ELastAxisInput::Z, HighlightedAxis);
		DrawSingleArrow(World, ArrowOrigin, ArrowOrigin + ZVector, ColorScheme.ColorZ, ThicknessZ);
	}
}

void FSFArrowModule::SetArrowColors(const FArrowColorScheme& NewColorScheme)
{
	ColorScheme = NewColorScheme;
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("FSFArrowModule: Color scheme updated"));
}

void FSFArrowModule::SetArrowConfig(const FArrowConfig& NewConfig)
{
	Config = NewConfig;
	UE_LOG(LogSmartFoundations, VeryVerbose, 
		TEXT("FSFArrowModule: Config updated (ZOffset=%.1f, VectorLength=%.1f, ArrowCount=%d)"),
		Config.ZOffset, Config.VectorLength, Config.ArrowCount);
}

void FSFArrowModule::SetModifierKeys(bool bShift, bool bCtrl)
{
	bLeftShiftPressed = bShift;
	bLeftCtrlPressed = bCtrl;
}

ELastAxisInput FSFArrowModule::GetEffectiveHighlightedAxis() const
{
	return CalculateHighlightedAxis();
}

// ========================================
// Legacy API Compatibility (Deprecated)
// ========================================

void FSFArrowModule::SpawnArrows(
	UWorld* World,
	USceneComponent* ParentComponent,
	const FVector& BaseLocation,
	const FRotator& BaseRotation)
{
	// Legacy compatibility - just store world reference
	LastWorld = World;
	
	UE_LOG(LogSmartFoundations, Warning, 
		TEXT("FSFArrowModule::SpawnArrows is deprecated - use UpdateArrows() instead"));
}

void FSFArrowModule::SetVisibility(bool bVisible)
{
	bCurrentlyVisible = bVisible;
	
	UE_LOG(LogSmartFoundations, Warning, 
		TEXT("FSFArrowModule::SetVisibility is deprecated - use UpdateArrows() instead"));
}

// ========================================
// Private Helper Methods
// ========================================

void FSFArrowModule::DrawSingleArrow(
	UWorld* World,
	const FVector& Start,
	const FVector& End,
	const FColor& Color,
	float Thickness)
{
	if (!World)
	{
		return;
	}

	// Draw debug directional arrow with multiple fallbacks for packaged builds
	const bool bPersistent = true;
	const float Duration = 1.0f;
	
	// Primary: DrawDebugDirectionalArrow
	DrawDebugDirectionalArrow(
		World,
		Start,
		End,
		50.0f,
		Color,
		bPersistent,
		Duration,
		SDPG_World,
		Thickness
	);
	
	// Fallback: Simple line (more likely to work in packaged builds)
	DrawDebugLine(
		World,
		Start,
		End,
		Color,
		bPersistent,
		Duration,
		SDPG_World,
		Thickness * 2.0f  // Thicker line for visibility
	);
	
	// Fallback 2: Draw a sphere at the end point for visibility testing
	DrawDebugSphere(
		World,
		End,
		25.0f,              // 25cm radius sphere
		12,                 // Segments
		Color,
		bPersistent,
		Duration,
		SDPG_World,
		Thickness
	);
	
	// Debug: Log draw call every time (for troubleshooting)
	static int32 DrawCallCounter = 0;
	if (++DrawCallCounter >= 60)  // Log every second at 60fps
	{
		DrawCallCounter = 0;
		UE_LOG(LogSmartFoundations, Warning, 
			TEXT("🎯 ARROW DRAW: Start=%s End=%s Color=%s Thick=%.1f"), 
			*Start.ToString(), *End.ToString(), *Color.ToString(), Thickness);
	}
}

ELastAxisInput FSFArrowModule::CalculateHighlightedAxis() const
{
	// Priority 1: Modifier keys override LastAxis
	if (bLeftShiftPressed && bLeftCtrlPressed)
	{
		// Both modifiers = Z axis
		return ELastAxisInput::Z;
	}
	else if (bLeftShiftPressed)
	{
		// Shift only = X axis
		return ELastAxisInput::X;
	}
	else if (bLeftCtrlPressed)
	{
		// Ctrl only = Y axis
		return ELastAxisInput::Y;
	}

	// Priority 2: No modifiers, use LastAxis
	return CurrentLastAxis;
}

float FSFArrowModule::GetThicknessForAxis(ELastAxisInput Axis, ELastAxisInput HighlightedAxis) const
{
	if (Axis == HighlightedAxis)
	{
		// Highlighted axis gets scaled thickness
		return Config.BaseThickness * Config.HighlightScale;
	}
	else
	{
		// Non-highlighted axes get base thickness
		return Config.BaseThickness;
	}
}
