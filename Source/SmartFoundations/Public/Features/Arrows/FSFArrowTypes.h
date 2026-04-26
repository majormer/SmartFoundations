#pragma once

#include "CoreMinimal.h"
#include "FSFArrowTypes.generated.h"

/**
 * Last axis input state for arrow highlighting
 * Tracks which axis was most recently manipulated
 */
enum class ELastAxisInput : uint8
{
	/** No axis selected */
	None = 0,

	/** X axis last used */
	X = 1,

	/** Y axis last used */
	Y = 2,

	/** Z axis last used */
	Z = 3
};

/**
 * Arrow color scheme configuration
 * Defines RGB colors for each axis
 */
struct FArrowColorScheme
{
	/** X axis color (default: Red) */
	FColor ColorX;

	/** Y axis color (default: Green) */
	FColor ColorY;

	/** Z axis color (default: Blue) */
	FColor ColorZ;

	/** Default constructor - RGB color scheme */
	FArrowColorScheme()
		: ColorX(255, 0, 0, 255)    // Red
		, ColorY(0, 255, 0, 255)    // Green
		, ColorZ(0, 0, 255, 255)    // Blue
	{
	}

	/** Custom color constructor */
	FArrowColorScheme(const FColor& InX, const FColor& InY, const FColor& InZ)
		: ColorX(InX)
		, ColorY(InY)
		, ColorZ(InZ)
	{
	}

	/** Get color for specific axis */
	FColor GetColorForAxis(int32 Axis) const
	{
		switch (Axis)
		{
			case 0: return ColorX;
			case 1: return ColorY;
			case 2: return ColorZ;
			default: return FColor::White;
		}
	}
};

/**
 * Arrow configuration settings
 */
struct FArrowConfig
{
	/** Vertical offset from hologram origin (in cm, default: 300) */
	float ZOffset = 300.0f;

	/** Arrow vector length (in cm, 2 meters = 200cm) */
	float VectorLength = 200.0f;

	/** Base arrow thickness (default: 6.0) */
	float BaseThickness = 6.0f;

	/** Highlighted arrow thickness multiplier (default: 2.0 = 12.0 total) */
	float HighlightScale = 2.0f;

	/** Smart arrows enabled */
	bool bSmartArrows = true;

	/** Arrow count mode: 1 = show only highlighted, 3 = show all */
	int32 ArrowCount = 3;

	/** Default constructor */
	FArrowConfig() = default;
};

/**
 * Hologram bounds information for dynamic arrow positioning
 * Used to calculate arrow placement above hologram surfaces
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FHologramBounds
{
	GENERATED_BODY()

	/** Center point of the hologram or grid */
	UPROPERTY(BlueprintReadOnly, Category = "Smart! Bounds")
	FVector Center;

	/** Extents from center to edges (half-size in each direction) */
	UPROPERTY(BlueprintReadOnly, Category = "Smart! Bounds")
	FVector Extents;

	/** Z coordinate of the highest surface point */
	UPROPERTY(BlueprintReadOnly, Category = "Smart! Bounds")
	float TopZ;

	/** Whether bounds calculation was successful */
	UPROPERTY(BlueprintReadOnly, Category = "Smart! Bounds")
	bool bIsValid;

	/** Default constructor - invalid bounds */
	FHologramBounds()
		: Center(FVector::ZeroVector)
		, Extents(FVector::ZeroVector)
		, TopZ(0.0f)
		, bIsValid(false)
	{
	}

	/** Constructor with calculated bounds */
	FHologramBounds(const FVector& InCenter, const FVector& InExtents, float InTopZ)
		: Center(InCenter)
		, Extents(InExtents)
		, TopZ(InTopZ)
		, bIsValid(true)
	{
	}

	/** Get the size of the bounds (full dimensions) */
	FVector GetSize() const
	{
		return Extents * 2.0f;
	}

	/** Get the minimum point of the bounds */
	FVector GetMin() const
	{
		return Center - Extents;
	}

	/** Get the maximum point of the bounds */
	FVector GetMax() const
	{
		return Center + Extents;
	}

	/** Calculate recommended arrow Z offset (50cm above top surface) */
	float GetArrowZOffset() const
	{
		return TopZ + 50.0f;  // 50cm clearance above highest point
	}

	/** Calculate arrow scale factor based on hologram size */
	float GetArrowScaleFactor() const
	{
		if (!bIsValid) return 1.0f;
		
		// Use largest dimension for scaling
		float MaxDimension = FMath::Max3(Extents.X, Extents.Y, Extents.Z);
		
		// Scale between 0.5x (small) and 2.0x (large)
		// 400cm = 1.0x scale (medium foundation size)
		float ScaleFactor = FMath::Clamp(MaxDimension / 400.0f, 0.5f, 2.0f);
		return ScaleFactor;
	}
};
