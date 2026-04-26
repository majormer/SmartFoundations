#include "Holograms/Core/SFBuildableHologram.h"
#include "FGBuildable.h"
#include "Logging/SFLogMacros.h"
#include "Data/SFBuildableSizeRegistry.h"

ASFBuildableHologram::ASFBuildableHologram()
{
    // Initialize buildable-specific defaults
}

void ASFBuildableHologram::BeginPlay()
{
    Super::BeginPlay();
    LogSmartActivity(TEXT("Buildable hologram initialized"));
}

void ASFBuildableHologram::ConfigureActor(AFGBuildable* InBuildable) const
{
    Super::ConfigureActor(InBuildable);
    
    if (InBuildable)
    {
        RegisterConstructedBuilding(InBuildable);
        SetBuildingMetadata(InBuildable);
    }
}

void ASFBuildableHologram::RegisterConstructedBuilding(AActor* Building) const
{
    // Building registration logic will be implemented in derived classes
    LogSmartActivity(FString::Printf(TEXT("Registered building: %s"), *Building->GetClass()->GetName()));
}

void ASFBuildableHologram::SetBuildingMetadata(AActor* Building) const
{
    // Set metadata on the constructed building
    LogSmartActivity(FString::Printf(TEXT("Set metadata for building: %s"), *Building->GetClass()->GetName()));
}

void ASFBuildableHologram::LogSmartActivity(const FString& Activity) const
{
    SF_LOG_ADAPTER(Verbose, TEXT("Smart Buildable Hologram: %s"), *Activity);
}

void ASFBuildableHologram::SetSmartMetadata(int32 GroupIndex, int32 ChildIndex)
{
    PlacementGroupIndex = GroupIndex;
    PlacementChildIndex = ChildIndex;
}

// ========================================
// Smart Feature Support Methods (Phase 1)
// ========================================

FBoxSphereBounds ASFBuildableHologram::GetSmartBuildingBounds() const
{
    // Use SFBuildableSizeRegistry for accurate bounds
    UClass* BuildClass = GetBuildClass();
    if (BuildClass && USFBuildableSizeRegistry::HasProfile(BuildClass))
    {
        FSFBuildableSizeProfile Profile = USFBuildableSizeRegistry::GetProfile(BuildClass);
        FVector Size = Profile.DefaultSize;
        
        // Create bounds centered at origin (Z-up from ground)
        FBox BoundingBox(
            FVector(-Size.X * 0.5f, -Size.Y * 0.5f, 0.0f),
            FVector(Size.X * 0.5f, Size.Y * 0.5f, Size.Z)
        );
        
        return FBoxSphereBounds(BoundingBox);
    }
    
    // Fallback to actor bounds
    FVector Origin;
    FVector BoxExtent;
    GetActorBounds(false, Origin, BoxExtent);
    return FBoxSphereBounds(Origin, BoxExtent, BoxExtent.Size());
}

bool ASFBuildableHologram::SupportsSmartFeature(ESFFeature Feature) const
{
    // All buildable holograms support all Smart features by default
    // Specific hologram types can override this
    return true;
}

void ASFBuildableHologram::ApplySmartTransformOffset(const FVector& Offset)
{
    // Apply offset to hologram location
    SetActorLocation(GetActorLocation() + Offset);
}
