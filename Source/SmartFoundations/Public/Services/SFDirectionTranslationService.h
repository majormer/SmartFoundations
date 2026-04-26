#pragma once

#include "CoreMinimal.h"
#include "SFDirectionTranslationService.generated.h"

/**
 * Player-relative direction for input interpretation
 */
UENUM(BlueprintType)
enum class EPlayerDirection : uint8
{
    Right,      // Player's right side
    Left,       // Player's left side
    Forward,    // Direction player is facing
    Back,       // Behind player
    Up,         // World up (Z+)
    Down        // World down (Z-)
};

/**
 * Mapping from player direction to hologram axis
 */
USTRUCT(BlueprintType)
struct FSFAxisMapping
{
    GENERATED_BODY()
    
    /** Which hologram axis this player direction maps to */
    UPROPERTY(BlueprintReadOnly)
    TEnumAsByte<EAxis::Type> Axis = EAxis::X;
    
    /** Direction along that axis (1 = positive, -1 = negative) */
    UPROPERTY(BlueprintReadOnly)
    int32 Sign = 1;
    
    FSFAxisMapping() = default;
    FSFAxisMapping(EAxis::Type InAxis, int32 InSign) : Axis(InAxis), Sign(InSign) {}
    
    /** Apply this mapping to a delta value, returning the axis-specific delta */
    FIntVector ApplyDelta(int32 Delta) const
    {
        FIntVector Result(0, 0, 0);
        switch (Axis)
        {
            case EAxis::X: Result.X = Delta * Sign; break;
            case EAxis::Y: Result.Y = Delta * Sign; break;
            case EAxis::Z: Result.Z = Delta * Sign; break;
            default: break;
        }
        return Result;
    }
};

/**
 * Complete mapping for all player directions at a given orientation
 */
USTRUCT(BlueprintType)
struct FSFDirectionMappingSet
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FSFAxisMapping Right;
    
    UPROPERTY(BlueprintReadOnly)
    FSFAxisMapping Left;
    
    UPROPERTY(BlueprintReadOnly)
    FSFAxisMapping Forward;
    
    UPROPERTY(BlueprintReadOnly)
    FSFAxisMapping Back;
    
    // Up/Down are always Z axis
    FSFAxisMapping Up = FSFAxisMapping(EAxis::Z, 1);
    FSFAxisMapping Down = FSFAxisMapping(EAxis::Z, -1);
    
    /** Get mapping for a specific direction */
    const FSFAxisMapping& GetMapping(EPlayerDirection Direction) const
    {
        switch (Direction)
        {
            case EPlayerDirection::Right:   return Right;
            case EPlayerDirection::Left:    return Left;
            case EPlayerDirection::Forward: return Forward;
            case EPlayerDirection::Back:    return Back;
            case EPlayerDirection::Up:      return Up;
            case EPlayerDirection::Down:    return Down;
            default:                        return Right;
        }
    }
};

/**
 * Service for translating player-relative directions to hologram axes.
 * 
 * When Player-Relative Controls are enabled, this service maps intuitive
 * directions (Right, Left, Forward, Back) to the hologram's local axes
 * based on the relative orientation between player and hologram.
 * 
 * The mapping snaps to the nearest 90° cardinal direction to provide
 * predictable, discrete axis selection.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFDirectionTranslationService : public UObject
{
    GENERATED_BODY()
    
public:
    /**
     * Calculate the complete direction mapping set for current orientations.
     * 
     * @param PlayerRotation   Current player camera/character rotation
     * @param HologramRotation Current hologram rotation
     * @return Complete mapping set for all directions
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Direction")
    static FSFDirectionMappingSet CalculateMappingSet(
        const FRotator& PlayerRotation,
        const FRotator& HologramRotation
    );
    
    /**
     * Get the hologram axis mapping for a specific player direction.
     * 
     * @param PlayerDirection  The player-relative direction (Right, Left, etc.)
     * @param PlayerRotation   Current player camera/character rotation
     * @param HologramRotation Current hologram rotation
     * @return The axis and sign this direction maps to
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Direction")
    static FSFAxisMapping GetAxisMapping(
        EPlayerDirection PlayerDirection,
        const FRotator& PlayerRotation,
        const FRotator& HologramRotation
    );
    
    /**
     * Translate a player-relative input delta to hologram-relative delta.
     * 
     * @param PlayerDirection  The player-relative direction of the input
     * @param Delta            The magnitude of the change
     * @param PlayerRotation   Current player camera/character rotation
     * @param HologramRotation Current hologram rotation
     * @return Delta vector in hologram space (X, Y, Z)
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Direction")
    static FIntVector TranslatePlayerDelta(
        EPlayerDirection PlayerDirection,
        int32 Delta,
        const FRotator& PlayerRotation,
        const FRotator& HologramRotation
    );
    
    /**
     * Get a human-readable description of the current mapping.
     * Useful for HUD display.
     * 
     * @param MappingSet The current mapping set
     * @return Multi-line string describing the mapping
     */
    UFUNCTION(BlueprintCallable, Category = "Smart|Direction")
    static FString GetMappingDescription(const FSFDirectionMappingSet& MappingSet);
    
    /**
     * Get the cardinal direction index (0-3) for a relative angle.
     * 0 = aligned, 1 = 90° CW, 2 = 180°, 3 = 90° CCW
     * 
     * @param RelativeYaw The yaw difference between player and hologram
     * @return Cardinal index (0-3)
     */
    static int32 GetCardinalIndex(float RelativeYaw);
    
private:
    /** Mapping tables for each cardinal orientation */
    static const FSFDirectionMappingSet CardinalMappings[4];
};
