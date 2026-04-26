#include "Services/SFDirectionTranslationService.h"

// Static mapping tables for each cardinal orientation
// Index 0: Player aligned with hologram (0° relative)
// Index 1: Player facing 90° clockwise from hologram
// Index 2: Player facing opposite direction (180°)
// Index 3: Player facing 90° counter-clockwise from hologram

const FSFDirectionMappingSet USFDirectionTranslationService::CardinalMappings[4] = {
    // 0°: Player aligned with hologram
    // Right = +X, Left = -X, Forward = +Y, Back = -Y
    {
        FSFAxisMapping(EAxis::X, +1),   // Right
        FSFAxisMapping(EAxis::X, -1),   // Left
        FSFAxisMapping(EAxis::Y, +1),   // Forward
        FSFAxisMapping(EAxis::Y, -1)    // Back
    },
    
    // 90°: Player facing hologram's right side
    // Right = +Y, Left = -Y, Forward = -X, Back = +X
    {
        FSFAxisMapping(EAxis::Y, +1),   // Right
        FSFAxisMapping(EAxis::Y, -1),   // Left
        FSFAxisMapping(EAxis::X, -1),   // Forward
        FSFAxisMapping(EAxis::X, +1)    // Back
    },
    
    // 180°: Player facing opposite direction
    // Right = -X, Left = +X, Forward = -Y, Back = +Y
    {
        FSFAxisMapping(EAxis::X, -1),   // Right
        FSFAxisMapping(EAxis::X, +1),   // Left
        FSFAxisMapping(EAxis::Y, -1),   // Forward
        FSFAxisMapping(EAxis::Y, +1)    // Back
    },
    
    // 270°: Player facing hologram's left side
    // Right = -Y, Left = +Y, Forward = +X, Back = -X
    {
        FSFAxisMapping(EAxis::Y, -1),   // Right
        FSFAxisMapping(EAxis::Y, +1),   // Left
        FSFAxisMapping(EAxis::X, +1),   // Forward
        FSFAxisMapping(EAxis::X, -1)    // Back
    }
};

int32 USFDirectionTranslationService::GetCardinalIndex(float RelativeYaw)
{
    // Normalize to 0-360
    RelativeYaw = FMath::Fmod(RelativeYaw + 360.0f, 360.0f);
    
    // Snap to nearest 90° and return index
    // 315-45° = 0 (aligned)
    // 45-135° = 1 (90° CW)
    // 135-225° = 2 (180°)
    // 225-315° = 3 (270° / 90° CCW)
    
    if (RelativeYaw < 45.0f || RelativeYaw >= 315.0f)
    {
        return 0;  // Aligned
    }
    else if (RelativeYaw < 135.0f)
    {
        return 1;  // 90° CW
    }
    else if (RelativeYaw < 225.0f)
    {
        return 2;  // 180°
    }
    else
    {
        return 3;  // 270° / 90° CCW
    }
}

FSFDirectionMappingSet USFDirectionTranslationService::CalculateMappingSet(
    const FRotator& PlayerRotation,
    const FRotator& HologramRotation)
{
    // Calculate relative yaw (horizontal rotation difference)
    const float PlayerYaw = FMath::UnwindDegrees(PlayerRotation.Yaw);
    const float HologramYaw = FMath::UnwindDegrees(HologramRotation.Yaw);
    const float RelativeYaw = FMath::UnwindDegrees(PlayerYaw - HologramYaw);
    
    // Get cardinal index and return corresponding mapping
    const int32 CardinalIdx = GetCardinalIndex(RelativeYaw);
    return CardinalMappings[CardinalIdx];
}

FSFAxisMapping USFDirectionTranslationService::GetAxisMapping(
    EPlayerDirection PlayerDirection,
    const FRotator& PlayerRotation,
    const FRotator& HologramRotation)
{
    // Up/Down are always Z axis, regardless of orientation
    if (PlayerDirection == EPlayerDirection::Up)
    {
        return FSFAxisMapping(EAxis::Z, +1);
    }
    if (PlayerDirection == EPlayerDirection::Down)
    {
        return FSFAxisMapping(EAxis::Z, -1);
    }
    
    // Calculate mapping set and return the specific direction
    const FSFDirectionMappingSet MappingSet = CalculateMappingSet(PlayerRotation, HologramRotation);
    return MappingSet.GetMapping(PlayerDirection);
}

FIntVector USFDirectionTranslationService::TranslatePlayerDelta(
    EPlayerDirection PlayerDirection,
    int32 Delta,
    const FRotator& PlayerRotation,
    const FRotator& HologramRotation)
{
    const FSFAxisMapping Mapping = GetAxisMapping(PlayerDirection, PlayerRotation, HologramRotation);
    return Mapping.ApplyDelta(Delta);
}

FString USFDirectionTranslationService::GetMappingDescription(const FSFDirectionMappingSet& MappingSet)
{
    auto AxisToString = [](const FSFAxisMapping& Mapping) -> FString
    {
        FString AxisName;
        switch (Mapping.Axis)
        {
            case EAxis::X: AxisName = TEXT("X"); break;
            case EAxis::Y: AxisName = TEXT("Y"); break;
            case EAxis::Z: AxisName = TEXT("Z"); break;
            default: AxisName = TEXT("?"); break;
        }
        return FString::Printf(TEXT("%s%s"), Mapping.Sign > 0 ? TEXT("+") : TEXT("-"), *AxisName);
    };
    
    return FString::Printf(
        TEXT("→ Right = %s\n← Left = %s\n↑ Forward = %s\n↓ Back = %s"),
        *AxisToString(MappingSet.Right),
        *AxisToString(MappingSet.Left),
        *AxisToString(MappingSet.Forward),
        *AxisToString(MappingSet.Back)
    );
}
