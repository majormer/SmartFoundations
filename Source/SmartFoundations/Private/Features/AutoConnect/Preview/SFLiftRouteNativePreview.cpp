// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Preview/SFLiftRouteNativePreview.h"
#include "SmartFoundations.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteBeltHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteProxyHologram.h"
#include "Subsystem/SFHologramDataService.h"
#include "Subsystem/SFSubsystem.h"
#include "FGRecipe.h"
#include "FGRecipeManager.h"

namespace
{
    TMap<TWeakObjectPtr<AFGHologram>, TWeakPtr<FSFLiftRouteNativePreview>> Pools;

    template<class T> T* SpawnNative(AFGHologram* Context, int32 Tier, const TCHAR* Kind)
    {
        const FString Family = FString::Printf(TEXT("Conveyor%sMk%d"), Kind, Tier);
        auto* Recipe = LoadClass<UFGRecipe>(nullptr, *FString::Printf(
            TEXT("/Game/FactoryGame/Recipes/Buildings/Recipe_%s.Recipe_%s_C"), *Family, *Family));
        auto* BuildClass = LoadClass<AFGBuildable>(nullptr, *FString::Printf(
            TEXT("/Game/FactoryGame/Buildable/Factory/%s/Build_%s.Build_%s_C"), *Family, *Family, *Family));
        auto* Manager = AFGRecipeManager::Get(Context->GetWorld());
        if (!Recipe || !BuildClass || !Manager || !Manager->IsRecipeAvailable(Recipe)) return nullptr;
        FActorSpawnParameters Params;
        Params.Owner = Context->GetOwner();
        Params.bDeferConstruction = true;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // Spline meshes initialize their local frame during FinishSpawning. A
        // distributor's yaw must not become the belt's initial local frame.
        const FTransform InitialPose(Context->GetActorLocation());
        auto* Native = Context->GetWorld()->SpawnActor<T>(T::StaticClass(), InitialPose, Params);
        if (!Native) return nullptr;
        Native->SetBuildClass(BuildClass);
        Native->SetRecipe(Recipe);
        Native->SetConstructionInstigator(Context->GetConstructionInstigator());
        Native->SetActorEnableCollision(false);
        USFHologramDataService::MarkAsChild(Native, Context, ESFChildHologramType::AutoConnect);
        Native->FinishSpawning(InitialPose);
        Native->SetActorTickEnabled(false);
        Native->Tags.AddUnique(TEXT("SF_LiftRouteValidator"));
        return Native;
    }
}

TSharedPtr<FSFLiftRouteNativePreview> FSFLiftRouteNativePreview::Acquire(AFGHologram* Parent, int32 InTier)
{
    if (!IsValid(Parent)) return nullptr;
    while (Parent->GetParentHologram()) Parent = Parent->GetParentHologram();
    for (auto It = Pools.CreateIterator(); It; ++It)
        if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
    auto Existing = Pools.FindRef(Parent).Pin();
    if (Existing && Existing->Tier == InTier) return Existing;
    auto Result = MakeShared<FSFLiftRouteNativePreview>();
    Result->Tier = InTier;
    Pools.Add(Parent, Result);
    return Result;
}

FSFLiftRouteNativePreview::~FSFLiftRouteNativePreview()
{
    if (Lift.IsValid()) Lift->Destroy();
    if (Belt.IsValid()) Belt->Destroy();
}

ASFLiftRouteHologram* FSFLiftRouteNativePreview::ConfigureLift(AFGHologram* Context,
    UFGFactoryConnectionComponent* Anchor, const FTransform& Pose, const FTransform& Top)
{
    if (!IsValid(Context) || !IsValid(Anchor) || Anchor->IsConnected()) return nullptr;
    if (!Lift.IsValid()) Lift = SpawnNative<ASFLiftRouteHologram>(Context, Tier, TEXT("Lift"));
    if (!Lift.IsValid()) return nullptr;
    Lift->SetInsideBlueprintDesigner(Context->GetBlueprintDesigner());
    Lift->SetAnchor(Anchor);
    Lift->SetDirectPeer(nullptr);
    Lift->SetRouteGeometry(Pose, Top);
    Lift->SetActorHiddenInGame(false);
    return Lift.Get();
}

ASFLiftRouteBeltHologram* FSFLiftRouteNativePreview::ConfigureBelt(AFGHologram* Context,
    UFGFactoryConnectionComponent* From, UFGFactoryConnectionComponent* To)
{
    if (!IsValid(Context) || !IsValid(From) || !IsValid(To) || From->IsConnected() || To->IsConnected()) return nullptr;
    if (!Belt.IsValid()) Belt = SpawnNative<ASFLiftRouteBeltHologram>(Context, Tier, TEXT("Belt"));
    if (!Belt.IsValid()) return nullptr;
    Belt->SetInsideBlueprintDesigner(Context->GetBlueprintDesigner());
    Belt->SetActorHiddenInGame(false);
    Belt->SetActorLocation(From->GetComponentLocation());
    Belt->SetActorRotation(FRotator::ZeroRotator);
    Belt->SetRouteEndpoints(From, To);
    const auto& Settings = USFSubsystem::Get(Context->GetWorld())->GetAutoConnectRuntimeSettings();
    Belt->SetRoutingMode(Settings.BeltRoutingMode);
    if (!Belt->TryUseBuildModeRouting(From->GetComponentLocation(), From->GetConnectorNormal(), To->GetComponentLocation(), To->GetConnectorNormal()))
        Belt->AutoRouteSplineWithNormals(From->GetComponentLocation(), From->GetConnectorNormal(), To->GetComponentLocation(), To->GetConnectorNormal());
    Belt->TriggerMeshGeneration();
    return Belt.Get();
}

AFGHologram* FSFLiftRouteNativePreview::Prepare(ASFLiftRouteProxyHologram* Proxy)
{
    if (Proxy->IsLift())
    {
        auto* Native = ConfigureLift(Proxy, Proxy->GetRouteFrom(), Proxy->GetActorTransform(), Proxy->GetTop());
        if (Native && Proxy->GetRouteTo() && !Native->TrySetDirectPeer(Proxy->GetRouteTo())) return nullptr;
        return Native && Native->IsNativePlacementValid() ? Native : nullptr;
    }
    auto* Native = ConfigureBelt(Proxy, Proxy->GetRouteFrom(), Proxy->GetRouteTo());
    if (!Native) return nullptr;
    // Revalidate the exact previewed spline; do not silently choose another route at commit.
    Native->SetActorTransform(Proxy->GetActorTransform());
    Native->SetSplineDataAndUpdate(Proxy->GetRouteSpline());
    return Native->IsNativePlacementValid() ? Native : nullptr;
}

void FSFLiftRouteNativePreview::Hide()
{
    if (Lift.IsValid()) Lift->SetActorHiddenInGame(true);
    if (Belt.IsValid()) Belt->SetActorHiddenInGame(true);
}

TSharedPtr<FSFLiftRouteNativePreview> FSFLiftRouteNativePreview::CreateConstructionPreview() const
{
    auto Result = MakeShared<FSFLiftRouteNativePreview>();
    Result->Tier = Tier;
    return Result;
}

void FSFLiftRouteNativePreview::RegisterProxy(ASFLiftRouteProxyHologram* Proxy)
{
    Proxies.Add(Proxy);
}

void FSFLiftRouteNativePreview::ReleaseProxy(ASFLiftRouteProxyHologram* Proxy)
{
    Proxies.Remove(Proxy);
    for (auto It = Proxies.CreateIterator(); It; ++It)
        if (!It->IsValid()) It.RemoveCurrent();
    if (!Proxies.IsEmpty()) return;
    // The orchestrator may keep a cleared route object until its next update.
    // Scratch actors belong to live visual cells, not that cache's lifetime.
    if (Lift.IsValid()) Lift->Destroy();
    if (Belt.IsValid()) Belt->Destroy();
    Lift.Reset();
    Belt.Reset();
}
