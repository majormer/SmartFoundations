#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "SFCameraIntegrationSubsystem.generated.h"

class FSFCameraTargetProvider;

/** Owns optional camera integration without changing the core subsystem's layout. */
UCLASS()
class USFCameraIntegrationSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    USFCameraIntegrationSubsystem();
    virtual ~USFCameraIntegrationSubsystem() override;
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
private:
    TSharedPtr<FSFCameraTargetProvider> Provider;
};
