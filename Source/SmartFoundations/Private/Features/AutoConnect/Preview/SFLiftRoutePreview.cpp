// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Preview/SFLiftRoutePreview.h"
#include "SmartFoundations.h"
#include "Features/AutoConnect/Preview/SFLiftRouteNativePreview.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteProxyHologram.h"
#include "Misc/ScopeExit.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteBeltHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Subsystem/SFHologramHelperService.h"
#include "Subsystem/SFHologramDataService.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"
#include "HAL/IConsoleManager.h"

// SP/MP DIVERGENCE MAP — lift-assisted factory connections
// [MP-SEAM] Client captures exact native routes; authority validates them before payment.
bool FSFLiftRoutePreview::IsEnabled(const AFGHologram* Parent)
{
    if (!Parent) return false;
    const auto* Subsystem = USFSubsystem::Get(Parent->GetWorld());
    return Subsystem && Subsystem->GetAutoConnectRuntimeSettings().LiftUsageMode != 0;
}

FSFLiftRoutePreview::~FSFLiftRoutePreview()
{
    for (auto Proxy : {Belt, Lift}) if (Proxy.IsValid())
    {
        if (auto* Subsystem = USFSubsystem::Get(Proxy->GetWorld()))
            if (auto* Helper = Subsystem->GetHologramHelper())
            { Helper->QueueChildForDestroy(Proxy.Get()); continue; }
        Proxy->Destroy();
    }
}

bool FSFLiftRoutePreview::Create(AFGHologram* Parent, UFGFactoryConnectionComponent* Distributor, UFGFactoryConnectionComponent* Building, int32 Tier)
{
    if (!IsEnabled(Parent) || !IsValid(Distributor) || !IsValid(Building)
        || Distributor->GetDirection() == Building->GetDirection()) return false;
    NativePreview = FSFLiftRouteNativePreview::Acquire(Parent, Tier);
    if (!NativePreview) return false;
    ON_SCOPE_EXIT { NativePreview->Hide(); };
    const bool bAtBuilding = USFSubsystem::Get(Parent->GetWorld())->GetAutoConnectRuntimeSettings().LiftUsageMode == 2;
    auto* Anchor = bAtBuilding ? Building : Distributor;
    auto* Other = bAtBuilding ? Distributor : Building;
    const FVector Normal = Anchor->GetConnectorNormal().GetSafeNormal2D();
    if (Normal.IsNearlyZero()) return false;
    FVector Base = Anchor->GetComponentLocation();
    FTransform Pose((-Normal).Rotation(), Base);
    const FTransform Top(FRotator(0, 180, 0), FVector(0, 0, Other->GetComponentLocation().Z - Base.Z));
    auto* NativeLift = NativePreview->ConfigureLift(Parent, Anchor, Pose, Top);
    if (!NativeLift || !NativeLift->GetMouth(0)) return false;
    Base += Normal * (Anchor->GetConnectorClearance() + NativeLift->GetMouth(0)->GetConnectorClearance());
    Pose.SetLocation(Base);
    NativeLift->SetRouteGeometry(Pose, Top);
    const bool bDirect = NativeLift->TrySetDirectPeer(Other);
    if (!NativeLift->IsNativePlacementValid()) return false;
    auto* LiftProxy = Cast<ASFLiftRouteProxyHologram>(ASFBuildableChildHologram::SpawnPreview(
        NativeLift, Pose, ESFChildHologramType::AutoConnect, NAME_None, ASFLiftRouteProxyHologram::StaticClass()));
    if (!LiftProxy) return false;
    LiftProxy->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
    Lift = LiftProxy;
    if (!LiftProxy->Capture(NativeLift, NativePreview, Anchor, bDirect ? Other : nullptr, true)) return false;
    static uint32 Serial = 0;
    Parent->AddChild(LiftProxy, FName(*FString::Printf(TEXT("AutoConnectLift_%u"), ++Serial)));
    USFHologramDataService::MarkAsChild(LiftProxy, Parent, ESFChildHologramType::AutoConnect);
    if (bDirect) return true;
    UFGFactoryConnectionComponent* Mouth = nullptr;
    TInlineComponentArray<UFGFactoryConnectionComponent*> Ports;
    LiftProxy->GetComponents(Ports);
    for (auto* Port : Ports) if (Port->GetFName() == NativeLift->GetMouth(1)->GetFName()) Mouth = Port;
    if (!Mouth || !FMath::IsNearlyEqual(Mouth->GetComponentLocation().Z, Other->GetComponentLocation().Z, 0.1)) return false;
    const bool bAnchorOutput = Anchor->GetDirection() == EFactoryConnectionDirection::FCD_OUTPUT;
    auto* From = bAnchorOutput ? Mouth : Other;
    auto* To = bAnchorOutput ? Other : Mouth;
    auto* NativeBelt = NativePreview->ConfigureBelt(Parent, From, To);
    if (!NativeBelt || !NativeBelt->IsNativePlacementValid()) return false;
    auto* BeltProxy = Cast<ASFLiftRouteProxyHologram>(ASFBuildableChildHologram::SpawnPreview(
        NativeBelt, NativeBelt->GetActorTransform(), ESFChildHologramType::AutoConnect, NAME_None, ASFLiftRouteProxyHologram::StaticClass()));
    if (!BeltProxy) return false;
    BeltProxy->SetInsideBlueprintDesigner(Parent->GetBlueprintDesigner());
    Belt = BeltProxy;
    if (!BeltProxy->Capture(NativeBelt, NativePreview, From, To, false)) return false;
    LiftProxy->AddChild(BeltProxy, FName(*FString::Printf(TEXT("AutoConnectLiftBelt_%u"), ++Serial)));
    USFHologramDataService::MarkAsChild(BeltProxy, LiftProxy, ESFChildHologramType::AutoConnect);
    return true;
}
