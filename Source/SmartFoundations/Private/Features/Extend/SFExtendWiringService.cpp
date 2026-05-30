// Copyright Coffee Stain Studios. All Rights Reserved.

#include "Features/Extend/SFExtendWiringService.h"
#include "Features/Extend/SFExtendWiringService.h"
#include "SmartFoundations.h"  // For LogSmartExtend

USFExtendWiringService::USFExtendWiringService()
{
}

void USFExtendWiringService::Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService)
{
    Subsystem = InSubsystem;
    ExtendService = InExtendService;
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("SFExtendWiringService initialized"));
}

void USFExtendWiringService::Shutdown()
{
    ExtendService = nullptr;
    Subsystem.Reset();
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("SFExtendWiringService shutdown"));
}
