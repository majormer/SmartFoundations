#pragma once

#include "CoreMinimal.h"
#include "FSFGridArrayTypes.generated.h"

/**
 * Integer 3D vector for grid counters and indices
 * 
 * Used throughout Smart! grid array system to represent:
 * - Grid counters (per-axis item counts with zero-skip semantics)
 * - Grid indices (integer positions in 3D grid)
 * - Grid directions (+1/-1 per axis)
 * 
 * Note: This is a simple POD struct, not a full vector type with math operations.
 * Use standard int32 operations for manipulation.
 */
USTRUCT(BlueprintType)
struct SMARTFOUNDATIONS_API FVector3i
{
	GENERATED_BODY()

	/** X component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 X;

	/** Y component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Y;

	/** Z component */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Z;

	/** Default constructor - initializes to zero */
	FVector3i()
		: X(0), Y(0), Z(0)
	{
	}

	/** Component constructor */
	FVector3i(int32 InX, int32 InY, int32 InZ)
		: X(InX), Y(InY), Z(InZ)
	{
	}

	/** Zero vector constant */
	static const FVector3i ZeroVector;

	/** One vector constant (1,1,1) */
	static const FVector3i OneVector;

	/** Equality operator */
	bool operator==(const FVector3i& Other) const
	{
		return X == Other.X && Y == Other.Y && Z == Other.Z;
	}

	/** Inequality operator */
	bool operator!=(const FVector3i& Other) const
	{
		return !(*this == Other);
	}

	/** Get component by index (0=X, 1=Y, 2=Z) */
	int32 operator[](int32 Index) const
	{
		check(Index >= 0 && Index < 3);
		return Index == 0 ? X : (Index == 1 ? Y : Z);
	}

	/** Set component by index (0=X, 1=Y, 2=Z) */
	int32& operator[](int32 Index)
	{
		check(Index >= 0 && Index < 3);
		return Index == 0 ? X : (Index == 1 ? Y : Z);
	}

	/** Convert to string for debugging */
	FString ToString() const
	{
		return FString::Printf(TEXT("(%d,%d,%d)"), X, Y, Z);
	}

	/** Get absolute value per component */
	FVector3i Abs() const
	{
		return FVector3i(FMath::Abs(X), FMath::Abs(Y), FMath::Abs(Z));
	}
};

/**
 * Axis enum for grid operations
 * Maps to FVector3i component indices
 */
UENUM(BlueprintType)
enum class ESFGridAxis : uint8
{
	/** X axis (index 0) */
	X = 0,

	/** Y axis (index 1) */
	Y = 1,

	/** Z axis (index 2) */
	Z = 2
};
