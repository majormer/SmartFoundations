// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Scaling/SFJumpPadHologramState.h"
#include "SmartFoundations.h"
#include "Features/Scaling/Holograms/SFJumpPadPreviewHologram.h"
#include "Features/Scaling/Holograms/SFJumpPadConstructionHologram.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Subsystem/SFHologramDataService.h"
#include "UObject/UnrealType.h"

bool SFJumpPadHologramState::IsHandled(const AFGHologram* Hologram)
{
    if (!IsValid(Hologram) || !Hologram->IsA<AFGJumpPadLauncherHologram>() || !Hologram->GetBuildClass()) return false;
    const FName BuildClass = Hologram->GetBuildClass()->GetFName();
    return BuildClass == TEXT("Build_JumpPadAdjustable_C");
}

bool SFJumpPadHologramState::GetSelectedMeshes(AFGHologram* Hologram, TArray<UStaticMeshComponent*>& OutMeshes)
{
    OutMeshes.Reset();
    if (!IsHandled(Hologram)) return false;
    // Copy live native body, launcher and indicator meshes. The trajectory and
    // destination guides are placement aids, not construction geometry.
    TArray<UStaticMeshComponent*> NativeMeshes;
    Hologram->GetComponents(NativeMeshes);
    for (UStaticMeshComponent* Mesh : NativeMeshes)
    {
        if (!IsValid(Mesh) || !Mesh->GetStaticMesh()) continue;
        const FString MeshName = Mesh->GetStaticMesh()->GetName();
        if (MeshName.StartsWith(TEXT("SM_JumpPad")) || MeshName == TEXT("SM_ProductionLight_01"))
            OutMeshes.Add(Mesh);
    }
    const FObjectPropertyBase* LauncherProperty = FindFProperty<FObjectPropertyBase>(
        AFGJumpPadLauncherHologram::StaticClass(), TEXT("mLauncherMeshComponent"));
    UStaticMeshComponent* Launcher = LauncherProperty
        ? Cast<UStaticMeshComponent>(LauncherProperty->GetObjectPropertyValue_InContainer(Hologram)) : nullptr;
    return OutMeshes.Num() >= 2 && IsValid(Launcher) && OutMeshes.Contains(Launcher);
}

void SFJumpPadHologramState::Sync(AFGHologram* Parent, AFGHologram* Child)
{
    if (IsHandled(Parent) && IsValid(Child) && Parent->GetBuildClass() == Child->GetBuildClass())
        if (ASFJumpPadPreviewHologram* Preview = Cast<ASFJumpPadPreviewHologram>(Child)) Preview->SyncGeometry(Parent);
}

bool SFJumpPadHologramState::IsLocalProxyGridConstruction(AFGHologram* Parent)
{
    // [MP-AUTH] Remote previews do not establish local construction authority.
    if (!IsHandled(Parent) || Parent->GetParentHologram() || !Parent->GetWorld()
        || Parent->GetWorld()->GetNetMode() == NM_Client || !Parent->HasAuthority()) return false;
    for (AFGHologram* Child : Parent->GetHologramChildren())
        if (IsValid(Child) && Child->IsA<ASFJumpPadPreviewHologram>() && !Child->IsDisabled()) return true;
    return false;
}

AFGHologram* SFJumpPadHologramState::SpawnConstructionCell(AFGHologram* Seed,
    AFGHologram* OwnerParent, FName ChildName, const FTransform& Pose)
{
    if (!IsHandled(Seed) || !IsValid(OwnerParent) || !Seed->GetWorld() || !Seed->GetRecipe()) return nullptr;
    FProperty* Angle = FindFProperty<FIntProperty>(AFGJumpPadLauncherHologram::StaticClass(), TEXT("mLaunchAngle"));
    FProperty* Step = FindFProperty<FProperty>(AFGJumpPadLauncherHologram::StaticClass(), TEXT("mBuildStep"));
    if (!Angle || !Step) return nullptr;
    FActorSpawnParameters Params;
    Params.Owner = Seed->GetOwner();
    Params.Instigator = Seed->GetInstigator();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Params.bDeferConstruction = true;
    ASFJumpPadConstructionHologram* Cell = Seed->GetWorld()->SpawnActor<ASFJumpPadConstructionHologram>(
        ASFJumpPadConstructionHologram::StaticClass(), Pose.GetLocation(), Pose.Rotator(), Params);
    if (!Cell) return nullptr;
    Cell->SetChildBuildClass(Seed->GetBuildClass());
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
    if (UFunction* Rep = Cell->FindFunction(TEXT("OnRep_LaunchAngle"))) Cell->ProcessEvent(Rep, nullptr);
    Cell->SetActorTransform(Pose);
    if (Seed->GetBlueprintDesigner()) Cell->SetInsideBlueprintDesigner(Seed->GetBlueprintDesigner());
    return Cell;
}
