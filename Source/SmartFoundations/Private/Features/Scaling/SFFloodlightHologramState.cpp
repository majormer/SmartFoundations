// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "SmartFoundations.h"
#include "Features/Scaling/SFFloodlightHologramState.h"
#include "Features/Scaling/Holograms/SFFloodlightPreviewHologram.h"
#include "Holograms/Core/SFFloodlightChildHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "FGColoredInstanceMeshProxy.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

bool SFFloodlightHologramState::IsHandled(const AFGHologram* Hologram)
{
    if (!IsValid(Hologram) || !Hologram->IsA<AFGFloodlightHologram>() || !Hologram->GetBuildClass()) return false;
    const FName BuildClass = Hologram->GetBuildClass()->GetFName();
    return BuildClass == TEXT("Build_FloodlightPole_C") || BuildClass == TEXT("Build_FloodlightWall_C");
}

bool SFFloodlightHologramState::GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes)
{
    OutMeshes.Reset();
    if (!IsHandled(Hologram)) return false;
    // Blueprint-created body/fixture components are present on the live hologram,
    // not necessarily in the buildable CDO's component array. Native guide/ring
    // meshes use ordinary static-mesh components, so do not copy those helpers.
    TArray<UFGColoredInstanceMeshProxy*> NativeMeshes;
    Hologram->GetComponents(NativeMeshes);
    for (UFGColoredInstanceMeshProxy* Mesh : NativeMeshes)
        if (IsValid(Mesh) && Mesh->GetStaticMesh()) OutMeshes.Add(Mesh);
    const FObjectPropertyBase* FixtureProperty = FindFProperty<FObjectPropertyBase>(
        AFGFloodlightHologram::StaticClass(), TEXT("mLightFixtureComponent"));
    UStaticMeshComponent* Fixture = FixtureProperty
        ? Cast<UStaticMeshComponent>(FixtureProperty->GetObjectPropertyValue_InContainer(Hologram)) : nullptr;
    return OutMeshes.Num() == 2 && IsValid(Fixture) && OutMeshes.Contains(Fixture);
}

void SFFloodlightHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
    if (IsHandled(Parent) && IsValid(Child) && Parent->GetBuildClass() == Child->GetBuildClass())
        if (ASFFloodlightPreviewHologram* Preview = Cast<ASFFloodlightPreviewHologram>(Child)) Preview->SyncGeometry(Parent);
}

bool SFFloodlightHologramState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
    // [MP-AUTH] Remote previews do not establish local construction authority.
    if (!IsHandled(Parent) || Parent->GetParentHologram() || !Parent->GetWorld()
        || Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
    for (AFGHologram* Child : Parent->GetHologramChildren())
        if (IsValid(Child) && Child->IsA<ASFFloodlightPreviewHologram>() && !Child->IsDisabled()) return true;
    return false;
}

AFGHologram* SFFloodlightHologramState::SpawnConstructionCell(AFGHologram* Seed,
    AFGHologram* OwnerParent, FName ChildName, const FTransform& Pose)
{
    if (!IsHandled(Seed) || !IsValid(OwnerParent) || !Seed->GetWorld() || !Seed->GetRecipe()) return nullptr;
    FProperty* Angle = FindFProperty<FIntProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mFixtureAngle"));
    FProperty* Step = FindFProperty<FProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mBuildStep"));
    FProperty* Mount = FindFProperty<FProperty>(AFGFloodlightHologram::StaticClass(), TEXT("mMountingType"));
    if (!Angle || !Step || !Mount) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = Seed->GetOwner();
    Params.Instigator = Seed->GetInstigator();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction = true;
    ASFFloodlightChildHologram* Cell = Seed->GetWorld()->SpawnActor<ASFFloodlightChildHologram>(
        ASFFloodlightChildHologram::StaticClass(), Pose.GetLocation(), Pose.Rotator(), Params);
    if (!Cell) return nullptr;
    Cell->SetChildBuildClass(Seed->GetBuildClass());
    Cell->SetRecipe(Seed->GetRecipe());
    Cell->SetConstructionInstigator(Seed->GetConstructionInstigator());
    Mount->CopyCompleteValue(Mount->ContainerPtrToValuePtr<void>(Cell), Mount->ContainerPtrToValuePtr<void>(Seed));
    Cell->FinishSpawning(Pose);
    if (FProperty* Permission = AFGHologram::StaticClass()->FindPropertyByName(TEXT("mCanBePlacedInBlueprintDesigner")))
        Permission->CopyCompleteValue(Permission->ContainerPtrToValuePtr<void>(Cell), Permission->ContainerPtrToValuePtr<void>(Seed));
    OwnerParent->AddChild(Cell, ChildName);
    USFHologramDataService::MarkAsChild(Cell, Seed, ESFChildHologramType::ScalingGrid);
    Cell->Tags.AddUnique(FName(TEXT("SF_GridChild")));
    Angle->CopyCompleteValue(Angle->ContainerPtrToValuePtr<void>(Cell), Angle->ContainerPtrToValuePtr<void>(Seed));
    Step->CopyCompleteValue(Step->ContainerPtrToValuePtr<void>(Cell), Step->ContainerPtrToValuePtr<void>(Seed));
    if (UFunction* Rep = Cell->FindFunction(TEXT("OnRep_FixtureAngle"))) Cell->ProcessEvent(Rep, nullptr);
    Cell->SetActorTransform(Pose);
    if (Seed->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Seed->GetBlueprintDesigner());
    return Cell;
}
