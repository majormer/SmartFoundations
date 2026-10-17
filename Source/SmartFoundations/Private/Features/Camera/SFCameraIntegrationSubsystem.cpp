#include "Features/Camera/SFCameraIntegrationSubsystem.h"
#include "SmartFoundations.h"
#include "Engine/World.h"
#include "Subsystem/SFSubsystem.h"

// SP/MP DIVERGENCE MAP — Camera integration
//  1. [MP-CLIENT] Local preview framing only; no dedicated-server provider or saved state.
#if SF_CAMERA_PROVIDER_AVAILABLE
#include "Features/IModularFeatures.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGConveyorLiftHologram.h"
#include "Integration/SmartCameraTargetProvider.h"
#include "Integration/SmartCameraViewportRegistry.h"

class FSFCameraTargetProvider final : public ISmartCameraTargetProvider
{
public:
    explicit FSFCameraTargetProvider(USFSubsystem* InOwner) : Owner(InOwner) {}
    virtual UObject* GetOwner() const override { return Owner.Get(); }
    virtual bool TryGetTargetFrame(AFGHologram* Hologram, FSmartCameraTargetFrame& OutFrame) const override
    {
        if (!Owner.IsValid() || !IsValid(Hologram) || Owner->GetActiveHologram() != Hologram)
        {
            return false;
        }
        OutFrame.Location = Owner->GetFurthestTopHologramPosition();
        OutFrame.Forward = Hologram->GetActorForwardVector();
        OutFrame.Right = Hologram->GetActorRightVector();
        OutFrame.Up = Hologram->GetActorUpVector();
        FTransform RunFrame;
        if (USFExtendService* Extend = Owner->GetExtendService(); Extend && Extend->TryGetCameraRunFrame(Hologram, RunFrame))
        {
            OutFrame.Location = RunFrame.GetLocation();
            OutFrame.Forward = RunFrame.GetRotation().GetForwardVector();
            OutFrame.Right = RunFrame.GetRotation().GetRightVector();
            OutFrame.Up = RunFrame.GetRotation().GetUpVector();
        }
        return true;
    }
    virtual bool TryAcquireZoomLock(AFGHologram* Hologram) override
    {
        const bool Acquired = Owner.IsValid() && Owner->GetActiveHologram() == Hologram && Owner->TryAcquireHologramLock();
        if (Acquired) { LockedHologram = Hologram; }
        return Acquired;
    }
    virtual void ReleaseZoomLock() override
    {
        if (Owner.IsValid() && LockedHologram.IsValid() && Owner->GetActiveHologram() == LockedHologram.Get())
        {
            Owner->TryReleaseHologramLock();
        }
        LockedHologram.Reset();
    }
private:
    TWeakObjectPtr<USFSubsystem> Owner;
    TWeakObjectPtr<AFGHologram> LockedHologram;
};
#else
class FSFCameraTargetProvider {};
#endif

USFCameraIntegrationSubsystem::USFCameraIntegrationSubsystem() = default;
USFCameraIntegrationSubsystem::~USFCameraIntegrationSubsystem() = default;

bool USFCameraIntegrationSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // [MP-CLIENT] A server never consumes a local camera viewport.
    const UWorld* World = Cast<UWorld>(Outer);
    return SF_CAMERA_PROVIDER_AVAILABLE && !IsRunningDedicatedServer() && World &&
        (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) &&
        World->GetNetMode() != NM_DedicatedServer && Super::ShouldCreateSubsystem(Outer);
}

void USFCameraIntegrationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if SF_CAMERA_PROVIDER_AVAILABLE
    USFSubsystem* Owner = Collection.InitializeDependency<USFSubsystem>();
    if (Owner)
    {
        Provider = MakeShared<FSFCameraTargetProvider>(Owner);
        IModularFeatures::Get().RegisterModularFeature(ISmartCameraTargetProvider::GetFeatureName(), Provider.Get());
    }
#endif
}

void USFCameraIntegrationSubsystem::Deinitialize()
{
#if SF_CAMERA_PROVIDER_AVAILABLE
    for (ISmartCameraViewportRegistry* Registry : IModularFeatures::Get().GetModularFeatureImplementations<ISmartCameraViewportRegistry>(ISmartCameraViewportRegistry::GetNamedFeatureName()))
        if (Registry->GetCameraWorld() == GetWorld()) Registry->UnregisterConsumer(TEXT("SmartFoundations"), this);
    if (Provider)
    {
        Provider->ReleaseZoomLock();
        IModularFeatures::Get().UnregisterModularFeature(ISmartCameraTargetProvider::GetFeatureName(), Provider.Get());
    }
#endif
    Provider.Reset();
    Super::Deinitialize();
}

TStatId USFCameraIntegrationSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(USFCameraIntegrationSubsystem, STATGROUP_Tickables);
}

void USFCameraIntegrationSubsystem::Tick(float DeltaTime)
{
#if SF_CAMERA_PROVIDER_AVAILABLE
    USFSubsystem* Smart = GetWorld()->GetSubsystem<USFSubsystem>();
    AFGHologram* Hologram = Smart ? Smart->GetActiveHologram() : nullptr;
    for (ISmartCameraViewportRegistry* Registry : IModularFeatures::Get().GetModularFeatureImplementations<ISmartCameraViewportRegistry>(ISmartCameraViewportRegistry::GetNamedFeatureName()))
    {
        if (Registry->GetCameraWorld() != GetWorld()) continue;
        if (!Provider) { Registry->UnregisterConsumer(TEXT("SmartFoundations"), this); continue; }
        if (!Registry->RegisterConsumer(TEXT("SmartFoundations"), this, NSLOCTEXT("Smart", "CameraConsumer", "Smart"))) continue;
        FSmartCameraViewDefinition View;
        View.DisplayName = NSLOCTEXT("Smart", "CameraView", "Build Preview");
        View.ExclusiveGroup = TEXT("build-preview");
        View.Priority = 10;
        // Preserve the existing player's Camera defaults, independently of the built-in enable switch.
        View.SuggestedSettings = Registry->GetBuildPreviewDefaults();
        View.bUsesBuildAngles = true;
        if (Cast<AFGConveyorLiftHologram>(Hologram)) View.SuggestedSettings.Mode = ESmartCameraMode::Periscope;
        const FSmartCameraViewId Id{TEXT("SmartFoundations"), TEXT("build-preview")};
        FSmartCameraTargetFrame Frame;
        if (Registry->RegisterView(Id, this, View))
        {
            if (IsValid(Hologram) && Provider->TryGetTargetFrame(Hologram, Frame)) Registry->SetViewFrame(Id, this, Frame, Hologram);
            else Registry->DeactivateView(Id, this);
        }
    }
#endif
}
