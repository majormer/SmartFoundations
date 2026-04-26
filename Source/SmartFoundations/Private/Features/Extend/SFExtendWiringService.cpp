// Copyright Coffee Stain Studios. All Rights Reserved.

#include "Features/Extend/SFExtendWiringService.h"
#include "Features/Extend/SFExtendWiringService.h"
#include "SmartFoundations.h"  // For LogSmartFoundations

USFExtendWiringService::USFExtendWiringService()
{
}

void USFExtendWiringService::Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService)
{
    Subsystem = InSubsystem;
    ExtendService = InExtendService;
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFExtendWiringService initialized"));
}

void USFExtendWiringService::Shutdown()
{
    ExtendService = nullptr;
    Subsystem.Reset();
    UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("SFExtendWiringService shutdown"));
}
