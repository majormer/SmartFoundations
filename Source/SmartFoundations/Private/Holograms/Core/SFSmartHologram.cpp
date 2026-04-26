#include "Holograms/Core/SFSmartHologram.h"
#include "Logging/SFLogMacros.h"

ASFSmartHologram::ASFSmartHologram()
{
    // Initialize Smart-specific defaults
}

void ASFSmartHologram::BeginPlay()
{
    Super::BeginPlay();
    LogSmartActivity(TEXT("Smart hologram initialized"));
}

void ASFSmartHologram::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    LogSmartActivity(TEXT("Smart hologram destroyed"));
    Super::EndPlay(EndPlayReason);
}

void ASFSmartHologram::LogSmartActivity(const FString& Activity)
{
    SF_LOG_ADAPTER(Verbose, TEXT("Smart Hologram: %s"), *Activity);
}

void ASFSmartHologram::SetSmartMetadata(int32 GroupIndex, int32 ChildIndex)
{
    PlacementGroupIndex = GroupIndex;
    PlacementChildIndex = ChildIndex;
}

void ASFSmartHologram::AddChild(AFGHologram* InChild, FName HologramName)
{
    Super::AddChild(InChild, HologramName);
    LogSmartActivity(FString::Printf(TEXT("Added child hologram: %s (name: %s)"), 
        InChild ? *InChild->GetName() : TEXT("nullptr"), *HologramName.ToString()));
}

void ASFSmartHologram::ReplaceChildWithSmartHologram(AFGHologram* OriginalChild, AFGHologram* SmartChild)
{
    if (!OriginalChild || !SmartChild)
    {
        LogSmartActivity(TEXT("ReplaceChildWithSmartHologram: Invalid parameters"));
        return;
    }
    
    // Find and replace the original child in our children array
    for (int32 i = 0; i < mChildren.Num(); i++)
    {
        if (mChildren[i] == OriginalChild)
        {
            // Get the child's name before we replace it
            FName ChildName = OriginalChild->GetNameWithinParentHologram();
            
            // Replace with smart hologram
            mChildren[i] = SmartChild;
            
            // Update the name lookup map
            mChildrenNameLookupMap[ChildName] = SmartChild;
            
            // NOTE: We cannot set protected members on another instance
            // The child hologram will need to manage its own parent reference
            // This is a limitation of the UE4/5 protected member access rules
            
            LogSmartActivity(FString::Printf(TEXT("Replaced child %s with smart hologram %s"), 
                *OriginalChild->GetName(), *SmartChild->GetName()));
            return;
        }
    }
    
    LogSmartActivity(FString::Printf(TEXT("ReplaceChildWithSmartHologram: Child %s not found"), 
        *OriginalChild->GetName()));
}
