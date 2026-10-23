// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#pragma once
#include "CoreMinimal.h"
#include "Components/HorizontalBox.h"
#include "SFInterfaceTaskStrip.generated.h"

class USFSubsystem;

/** Shared local navigation; buttons never apply or construct a task's draft. */
UCLASS()
class SMARTFOUNDATIONS_API USFInterfaceTaskStrip : public UHorizontalBox
{
    GENERATED_BODY()
public:
    void Initialize(USFSubsystem* Subsystem, int32 ActiveTask);
    void SetActiveTask(int32 ActiveTask);
private:
    TWeakObjectPtr<USFSubsystem> Owner;
    UFUNCTION() void OnPanel();
    UFUNCTION() void OnWalking();
    UFUNCTION() void OnUpgrade();
    UFUNCTION() void OnRestore();
    UFUNCTION() void OnExtend();
    TArray<int32> TaskIds;
};
