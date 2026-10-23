// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/AutoConnect/Holograms/SFLiftRouteProxyHologram.h"
#include "SmartFoundations.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteHologram.h"
#include "Features/AutoConnect/Holograms/SFLiftRouteBeltHologram.h"
#include "Features/AutoConnect/Preview/SFLiftRouteNativePreview.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "FGConstructDisqualifier.h"
#include "FGFactoryConnectionComponent.h"
#include "UObject/UnrealType.h"

bool ASFLiftRouteProxyHologram::Capture(AFGHologram* Native, TSharedPtr<FSFLiftRouteNativePreview> InNativePreview,
    UFGFactoryConnectionComponent* From, UFGFactoryConnectionComponent* To, bool bLift)
{
    NativePreview = InNativePreview;
    if (NativePreview) NativePreview->RegisterProxy(this);
    RouteFrom = From;
    RouteTo = To;
    bHasRouteTo = To != nullptr;
    if (From) { RouteFromPose = From->GetComponentTransform(); FromDirection = static_cast<int32>(From->GetDirection()); }
    if (To) { RouteToPose = To->GetComponentTransform(); ToDirection = static_cast<int32>(To->GetDirection()); }
    bIsLift = bLift;
    NativeCost = Native->GetCost(false);
    NativeMultiplier = Native->GetBaseCostMultiplier();
    if (auto* Lift = Cast<ASFLiftRouteHologram>(Native)) Top = Lift->GetTopTransform();
    if (auto* Belt = Cast<ASFLiftRouteBeltHologram>(Native)) Spline = Belt->GetSplineData();
    TInlineComponentArray<UFGFactoryConnectionComponent*> Sources, Targets;
    Native->GetComponents(Sources);
    GetComponents(Targets);
    for (auto* Source : Sources) for (auto* Target : Targets)
        if (Source->GetFName() == Target->GetFName())
        {
            Target->SetWorldTransform(Source->GetComponentTransform());
            Target->SetDirection(Source->GetDirection());
        }
    if (!bIsLift) for (auto* Target : Targets)
    {
        const bool bInput = Target->GetFName() == TEXT("ConveyorAny0");
        auto* Endpoint = bInput ? From : To;
        if (Endpoint)
        {
            Target->SetWorldLocation(Endpoint->GetComponentLocation());
            Target->SetWorldRotation((-Endpoint->GetConnectorNormal()).Rotation());
            Target->SetDirection(bInput ? EFactoryConnectionDirection::FCD_INPUT : EFactoryConnectionDirection::FCD_OUTPUT);
        }
    }
    CopyMeshes(Native);
    bReady = Meshes.Num() > 0 && NativeCost.Num() > 0 && NativePreview.IsValid();
    SetPlacementMaterialState(EHologramMaterialState::HMS_OK);
    return bReady;
}

void ASFLiftRouteProxyHologram::CopyMeshes(AFGHologram* Native)
{
    TInlineComponentArray<UStaticMeshComponent*> Existing, Sources;
    GetComponents(Existing);
    Native->GetComponents(Sources);
    for (auto* Mesh : Existing) { Mesh->SetVisibility(false); Mesh->SetHiddenInGame(true); }
    for (auto* Source : Sources)
    {
        if (!Source->GetStaticMesh()) continue;
        auto* SourceSpline = Cast<USplineMeshComponent>(Source);
        bool bGeometry = !bIsLift && SourceSpline;
        if (bIsLift)
            for (const TCHAR* Name : {TEXT("mBottomMesh"), TEXT("mMidMesh"), TEXT("mHalfMidMesh"), TEXT("mTopMesh"), TEXT("mJointMesh"), TEXT("mPassthroughBottomMesh"), TEXT("mPassthroughTopMesh")})
                if (const auto* Property = FindFProperty<FObjectPropertyBase>(AFGConveyorLiftHologram::StaticClass(), Name))
                    bGeometry |= Property->GetObjectPropertyValue_InContainer(Native) == Source->GetStaticMesh();
        if (!bGeometry) continue;
        UStaticMeshComponent* Mesh = SourceSpline
            ? NewObject<USplineMeshComponent>(this) : NewObject<UStaticMeshComponent>(this);
        Mesh->SetupAttachment(GetRootComponent());
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetComponentTickEnabled(false);
        Mesh->ComponentTags.AddUnique(AFGHologram::HOLOGRAM_MESH_TAG);
        Mesh->SetStaticMesh(Source->GetStaticMesh());
        Mesh->SetRelativeTransform(Source->GetComponentTransform().GetRelativeTransform(Native->GetActorTransform()));
        if (auto* TargetSpline = Cast<USplineMeshComponent>(Mesh))
        {
            TargetSpline->SetForwardAxis(SourceSpline->GetForwardAxis(), false);
            TargetSpline->SetSplineUpDir(SourceSpline->GetSplineUpDir(), false);
            TargetSpline->SetStartAndEnd(SourceSpline->GetStartPosition(), SourceSpline->GetStartTangent(), SourceSpline->GetEndPosition(), SourceSpline->GetEndTangent(), false);
            TargetSpline->SetStartScale(SourceSpline->GetStartScale(), false);
            TargetSpline->SetEndScale(SourceSpline->GetEndScale(), false);
            TargetSpline->SetStartRoll(SourceSpline->GetStartRoll(), false);
            TargetSpline->SetEndRoll(SourceSpline->GetEndRoll(), false);
            TargetSpline->SetStartOffset(SourceSpline->GetStartOffset(), false);
            TargetSpline->SetEndOffset(SourceSpline->GetEndOffset(), false);
            TargetSpline->UpdateMesh();
        }
        for (int32 Index = 0; Index < Source->GetNumMaterials(); ++Index) Mesh->SetMaterial(Index, Source->GetMaterial(Index));
        AddInstanceComponent(Mesh);
        Mesh->RegisterComponent();
        Mesh->SetVisibility(Source->IsVisible());
        Mesh->SetHiddenInGame(Source->bHiddenInGame);
        Meshes.Add(Mesh);
    }
}

bool ASFLiftRouteProxyHologram::ValidateCapturedRoute()
{
    const auto Matches = [](UFGFactoryConnectionComponent* Port, const FTransform& Pose, int32 Direction)
    {
        return IsValid(Port) && IsValid(Port->GetOwner()) && !Port->IsConnected()
            && static_cast<int32>(Port->GetDirection()) == Direction
            && Port->GetComponentLocation().Equals(Pose.GetLocation(), .1f)
            && Port->GetComponentQuat().Equals(Pose.GetRotation(), .0001f)
            && Port->GetComponentScale().Equals(Pose.GetScale3D(), .0001f);
    };
    if (!bReady || !NativePreview || !Matches(RouteFrom, RouteFromPose, FromDirection)
        || (bHasRouteTo && !Matches(RouteTo, RouteToPose, ToDirection))) return false;
    const bool Valid = NativePreview->Prepare(this) != nullptr;
    NativePreview->Hide();
    return Valid;
}

void ASFLiftRouteProxyHologram::CheckValidPlacement()
{
    ResetConstructDisqualifiers();
    if (!ValidateCapturedRoute()) AddConstructDisqualifier(UFGCDInvalidPlacement::StaticClass());
    if (NativePreview) NativePreview->Hide();
}

void ASFLiftRouteProxyHologram::SetPlacementMaterialState(EHologramMaterialState State)
{
    Super::SetPlacementMaterialState(State);
    for (UStaticMeshComponent* Mesh : Meshes) if (Mesh)
    {
        Mesh->SetRenderCustomDepth(true);
        Mesh->SetCustomDepthStencilValue(GetStencilForHologramMaterialState(State));
        Mesh->SetCustomDepthStencilWriteMask(ERendererStencilMask::ERSM_Default);
    }
}

TArray<FItemAmount> ASFLiftRouteProxyHologram::GetCost(bool IncludeChildren) const
{
    TArray<FItemAmount> Cost = NativeCost;
    if (IncludeChildren) for (auto* Child : GetHologramChildren()) if (IsValid(Child) && !Child->IsDisabled())
        for (const auto& Item : Child->GetCost(true))
        {
            auto* Existing = Cost.FindByPredicate([&](const FItemAmount& Value) { return Value.ItemClass == Item.ItemClass; });
            if (Existing) Existing->Amount += Item.Amount; else Cost.Add(Item);
        }
    return Cost;
}

AActor* ASFLiftRouteProxyHologram::Construct(TArray<AActor*>& OutChildren, FNetConstructionID ID)
{
    // Preview validators are reusable. Native construction gets a fresh cell so
    // per-construction state cannot leak into another route or future preview.
    auto Construction = bReady && NativePreview ? NativePreview->CreateConstructionPreview() : nullptr;
    AFGHologram* Native = Construction ? Construction->Prepare(this) : nullptr;
    if (!Native) { if (NativePreview) NativePreview->Hide(); return nullptr; }
    AActor* Result = Native->Construct(OutChildren, ID);
    Built = Cast<AFGBuildable>(Result);
    if (Result) for (auto* Child : GetHologramChildren()) if (IsValid(Child) && !Child->IsDisabled())
        if (auto* BuiltChild = Child->Construct(OutChildren, ID)) OutChildren.Add(BuiltChild);
    NativePreview->Hide();
    return Result;
}

UFGFactoryConnectionComponent* ASFLiftRouteProxyHologram::ResolveBuiltPort(FName Name) const
{
    if (!Built.IsValid()) return nullptr;
    // Native lifts reorder their built input/output indices when reversed.
    // Match the captured physical mouth and flow, not the preview's index.
    UFGFactoryConnectionComponent* Source = nullptr;
    TInlineComponentArray<UFGFactoryConnectionComponent*> PreviewPorts;
    GetComponents(PreviewPorts);
    for (auto* Port : PreviewPorts) if (Port->GetFName() == Name) Source = Port;
    if (!Source) return nullptr;
    TInlineComponentArray<UFGFactoryConnectionComponent*> Ports;
    Built->GetComponents(Ports);
    UFGFactoryConnectionComponent* Match = nullptr;
    for (auto* Port : Ports)
        if (Port->GetDirection() == Source->GetDirection()
            && Port->GetComponentLocation().Equals(Source->GetComponentLocation(), 0.1f))
        {
            if (Match) return nullptr;
            Match = Port;
        }
    return Match;
}

void ASFLiftRouteProxyHologram::Destroyed()
{
    if (NativePreview) NativePreview->ReleaseProxy(this);
    NativePreview.Reset();
    Super::Destroyed();
}
