// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFSnowCannonHologramState.h"
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFSnowCannonPreviewHologram.h"
#include "Hologram/FGBuildableSnowCannonHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"
#include "Features/Scaling/SFScalingFootprint.h"
#include "Data/SFBuildableSizeRegistry.h"

bool SFSnowCannonHologramState::IsHandled(const AFGHologram* Hologram)
{
    if (!IsValid(Hologram) || !Hologram->IsA<AFGBuildableSnowCannonHologram>() || !Hologram->GetBuildClass()) return false;
    const FName BuildClass = Hologram->GetBuildClass()->GetFName();
    return BuildClass == TEXT("Build_SnowCannon_C");
}

bool SFSnowCannonHologramState::GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes)
{
    OutMeshes.Reset();
    if (!IsHandled(Hologram)) return false;
    // Only construction meshes; omit the native placement aids.
    TArray<UStaticMeshComponent*> NativeMeshes;
    Hologram->GetComponents(NativeMeshes);
    for (UStaticMeshComponent* Mesh : NativeMeshes)
        if (IsValid(Mesh) && Mesh->GetStaticMesh()
            && Mesh->GetStaticMesh()->GetName().StartsWith(TEXT("SM_SnowCannon"))) OutMeshes.Add(Mesh);

    const FObjectPropertyBase* LauncherProperty = FindFProperty<FObjectPropertyBase>(
        AFGBuildableSnowCannonHologram::StaticClass(), TEXT("mCannonHeadMeshComponent"));
    UStaticMeshComponent* Launcher = LauncherProperty
        ? Cast<UStaticMeshComponent>(LauncherProperty->GetObjectPropertyValue_InContainer(Hologram)) : nullptr;
    return OutMeshes.Num() >= 2 && IsValid(Launcher) && OutMeshes.Contains(Launcher);
}

void SFSnowCannonHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
    if (IsHandled(Parent) && IsValid(Child) && Parent->GetBuildClass() == Child->GetBuildClass())
        if (ASFSnowCannonPreviewHologram* Preview = Cast<ASFSnowCannonPreviewHologram>(Child)) Preview->SyncGeometry(Parent);
}

bool SFSnowCannonHologramState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
    // [MP-AUTH] Remote previews do not establish local construction authority.
    if (!IsHandled(Parent) || Parent->GetParentHologram() || !Parent->GetWorld()
        || Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
    for (AFGHologram* Child : Parent->GetHologramChildren())
        if (IsValid(Child) && Child->IsA<ASFSnowCannonPreviewHologram>() && !Child->IsDisabled()) return true;
    return false;
}

AFGHologram* SFSnowCannonHologramState::SpawnConstructionCell(AFGHologram* Seed,
    AFGHologram* OwnerParent, FName ChildName, const FTransform& Pose)
{
    if (!IsHandled(Seed) || !IsValid(OwnerParent) || !Seed->GetWorld() || !Seed->GetRecipe()) return nullptr;
    FProperty* Angle = FindFProperty<FIntProperty>(AFGHologram::StaticClass(), TEXT("mScrollRotation"));
    FProperty* Step = FindFProperty<FProperty>(AFGBuildableSnowCannonHologram::StaticClass(), TEXT("mBuildStep"));
    if (!Angle || !Step) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = Seed->GetOwner();
    Params.Instigator = Seed->GetInstigator();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction = true;
    AFGBuildableSnowCannonHologram* Cell = Seed->GetWorld()->SpawnActor<AFGBuildableSnowCannonHologram>(
        Seed->GetClass(), Pose.GetLocation(), Pose.Rotator(), Params);
    if (!Cell) return nullptr;
    // Recipe assignment does not initialize the native build-class member.
    // BeginPlay queries its CDO, so establish it before FinishSpawning.
    FProperty* BuildClass = FindFProperty<FProperty>(AFGBuildableHologram::StaticClass(), TEXT("mBuildClass"));
    if (!BuildClass) { Cell->Destroy(); return nullptr; }
    BuildClass->CopyCompleteValue(BuildClass->ContainerPtrToValuePtr<void>(Cell), BuildClass->ContainerPtrToValuePtr<void>(Seed));
    Cell->SetRecipe(Seed->GetRecipe());
    Cell->SetConstructionInstigator(Seed->GetConstructionInstigator());
    Cell->FinishSpawning(Pose);
    if (FProperty* Permission = AFGHologram::StaticClass()->FindPropertyByName(TEXT("mCanBePlacedInBlueprintDesigner")))
        Permission->CopyCompleteValue(Permission->ContainerPtrToValuePtr<void>(Cell), Permission->ContainerPtrToValuePtr<void>(Seed));
    OwnerParent->AddChild(Cell, ChildName);
    USFHologramDataService::MarkAsChild(Cell, Seed, ESFChildHologramType::ScalingGrid);
    Cell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
    Angle->CopyCompleteValue(Angle->ContainerPtrToValuePtr<void>(Cell), Angle->ContainerPtrToValuePtr<void>(Seed));
    Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Cell), Step->ContainerPtrToValuePtr<void>(Seed));
    const FObjectPropertyBase* Head = FindFProperty<FObjectPropertyBase>(
        AFGBuildableSnowCannonHologram::StaticClass(), TEXT("mCannonHeadMeshComponent"));
    UStaticMeshComponent* SourceHead = Head ? Cast<UStaticMeshComponent>(Head->GetObjectPropertyValue_InContainer(Seed)) : nullptr;
    UStaticMeshComponent* TargetHead = Head ? Cast<UStaticMeshComponent>(Head->GetObjectPropertyValue_InContainer(Cell)) : nullptr;
    if (!SourceHead || !TargetHead) { Cell->Destroy(); return nullptr; }
    TargetHead->SetRelativeTransform(SourceHead->GetRelativeTransform());
    Cell->SetActorTransform(Pose);
    if (Seed->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Seed->GetBlueprintDesigner());
    return Cell;
}

bool SFSnowCannonHologramState::TryGetDimensions(AFGHologram* Hologram, FVector& OutDimensions)
{
    TArray<UStaticMeshComponent*> Meshes;
    if (!GetSelectedMeshes(Hologram, Meshes)) return false;
    FBox Bounds(ForceInit);
    for (UStaticMeshComponent* Mesh : Meshes)
        Bounds += Mesh->GetStaticMesh()->GetBoundingBox().TransformBy(
            Mesh->GetComponentTransform().GetRelativeTransform(Hologram->GetActorTransform()));
    if (!Bounds.IsValid) return false;
    FVector Size = USFBuildableSizeRegistry::GetProfile(Hologram->GetBuildClass()).DefaultSize;
    Size.Z = SFScalingFootprint::RoundHeightToNudge(Bounds.GetSize().Z, Hologram->GetNudgeDistance());
    OutDimensions = SFScalingFootprint::FromLocalDimensions(Size, Hologram->GetActorRotation());
    return true;
}
