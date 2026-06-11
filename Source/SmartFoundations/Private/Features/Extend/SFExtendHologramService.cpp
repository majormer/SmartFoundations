// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Features/Extend/SFExtendHologramService.h"
#include "Features/Extend/SFExtendTypes.h"
#include "Features/Extend/SFExtendService.h"
#include "Features/Extend/SFExtendCloneTopology.h"
#include "SmartFoundations.h"  // For LogSmartExtend
#include "Holograms/Core/SFFactoryHologram.h"
#include "Holograms/Logistics/SFConveyorBeltHologram.h"
#include "Holograms/Logistics/SFConveyorLiftHologram.h"
#include "Holograms/Logistics/SFPipelineHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Hologram/FGHologram.h"
#include "Hologram/FGFactoryHologram.h"
#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunBuild.h"
#include "FGCharacterPlayer.h"
#include "Kismet/GameplayStatics.h"

USFExtendHologramService::USFExtendHologramService()
{
}

void USFExtendHologramService::Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService)
{
    Subsystem = InSubsystem;
    ExtendService = InExtendService;
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("SFExtendHologramService initialized"));
}

void USFExtendHologramService::Shutdown()
{
    ClearBeltPreviews();
    ClearTracking();
    StoredCloneTopology.Reset();
    JsonSpawnedHolograms.Empty();
    ExtendService = nullptr;
    Subsystem.Reset();
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("SFExtendHologramService shutdown"));
}

// ==================== Child Hologram Tracking ====================

void USFExtendHologramService::TrackChildHologram(AFGHologram* Child, const FVector& IntendedPosition, const FRotator& IntendedRotation)
{
    if (!IsValid(Child))
    {
        return;
    }

    TrackedChildren.AddUnique(Child);
    ChildIntendedPositions.Add(Child, IntendedPosition);
    ChildIntendedRotations.Add(Child, IntendedRotation);
}

FVector* USFExtendHologramService::GetIntendedPosition(AFGHologram* Child)
{
    return ChildIntendedPositions.Find(Child);
}

FRotator* USFExtendHologramService::GetIntendedRotation(AFGHologram* Child)
{
    return ChildIntendedRotations.Find(Child);
}

void USFExtendHologramService::ClearTracking()
{
    TrackedChildren.Empty();
    ChildIntendedPositions.Empty();
    ChildIntendedRotations.Empty();
}

// ==================== Preview Management ====================

void USFExtendHologramService::CreateBeltPreviews(AFGHologram* ParentHologram)
{
    if (!ParentHologram || !ExtendService)
    {
        return;
    }

    CurrentParentHologram = ParentHologram;

    const FSFExtendTopology& Topology = ExtendService->GetCurrentTopology();
    if (!Topology.bIsValid || !Topology.SourceBuilding.IsValid())
    {
        return;
    }

    // Clear any existing previews
    ClearBeltPreviews();

    UWorld* World = ParentHologram->GetWorld();
    if (!World)
    {
        return;
    }

    // Calculate the offset we're using for the main building
    FVector SourceBuildingLocation = Topology.SourceBuilding->GetActorLocation();
    FVector NewBuildingLocation = ParentHologram->GetActorLocation();
    FVector CloneOffset = NewBuildingLocation - SourceBuildingLocation;

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 2: Cloning infrastructure with offset (%.1f, %.1f, %.1f)"),
        CloneOffset.X, CloneOffset.Y, CloneOffset.Z);
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND Phase 2: %d input chains, %d output chains to clone"),
        Topology.InputChains.Num(), Topology.OutputChains.Num());

    FSFSourceTopology SourceJSON = FSFSourceTopology::CaptureFromTopology(Topology);
    FSFCloneTopology CloneJSON = FSFCloneTopology::FromSource(SourceJSON, CloneOffset);

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("[SmartRestore][ExtendHologram] Clone topology generated: sourceChains=%d cloneHolograms=%d offset=(%.1f, %.1f, %.1f) parent=%s source=%s"),
        SourceJSON.BeltInputChains.Num() + SourceJSON.BeltOutputChains.Num() +
        SourceJSON.PipeInputChains.Num() + SourceJSON.PipeOutputChains.Num(),
        CloneJSON.ChildHolograms.Num(),
        CloneOffset.X,
        CloneOffset.Y,
        CloneOffset.Z,
        *GetNameSafe(ParentHologram),
        *GetNameSafe(Topology.SourceBuilding.Get()));

    // Spawn holograms from the clone topology.
    TMap<FString, AFGHologram*> SpawnedHolograms;
    int32 SpawnedCount = CloneJSON.SpawnChildHolograms(ParentHologram, ExtendService, SpawnedHolograms);

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][ExtendHologram] SpawnChildHolograms result: spawned=%d cloneChildren=%d"),
        SpawnedCount,
        CloneJSON.ChildHolograms.Num());

    // Wire connections between spawned holograms (lifts only - belts/pipes skip due to spline issues)
    int32 WiredCount = CloneJSON.WireChildHologramConnections(SpawnedHolograms, ParentHologram);
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND JSON SPAWN: Wired %d lift connections (belts/pipes deferred to post-build)"), WiredCount);

    // Store for post-build wiring
    StoredCloneTopology = MakeShared<FSFCloneTopology>(CloneJSON);
    JsonSpawnedHolograms.Reset();
    for (const TPair<FString, AFGHologram*>& Pair : SpawnedHolograms)
    {
        JsonSpawnedHolograms.Add(Pair.Key, Pair.Value);
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][ExtendHologram] Stored clone topology: children=%d spawnedMap=%d"),
        StoredCloneTopology->ChildHolograms.Num(),
        JsonSpawnedHolograms.Num());

    // Track spawned holograms for position refresh
    for (auto& Pair : SpawnedHolograms)
    {
        AFGHologram* Hologram = Pair.Value;
        if (IsValid(Hologram))
        {
            TrackChildHologram(Hologram, Hologram->GetActorLocation(), Hologram->GetActorRotation());
        }
    }

    RefreshChildPositions();

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
        TEXT("[SmartRestore][ExtendHologram] Tracked clone holograms: tracked=%d"),
        TrackedChildren.Num());
}

void USFExtendHologramService::ClearBeltPreviews()
{
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: ClearBeltPreviews - Destroying %d tracked children"),
        TrackedChildren.Num());

    // Clean up any Smart! children from parent's mChildren array BEFORE destroying them
    if (CurrentParentHologram.IsValid())
    {
        AFGHologram* Parent = CurrentParentHologram.Get();
        if (FArrayProperty* ChildrenProp = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren")))
        {
            TArray<AFGHologram*>* ChildrenArray = ChildrenProp->ContainerPtrToValuePtr<TArray<AFGHologram*>>(Parent);
            if (ChildrenArray)
            {
                // Remove any children that are Smart! owned
                for (int32 i = ChildrenArray->Num() - 1; i >= 0; --i)
                {
                    AFGHologram* Child = (*ChildrenArray)[i];
                    if (Child)
                    {
                        FString ChildName = Child->GetFName().ToString();
                        bool bIsSmartChild = ChildName.StartsWith(TEXT("Extend")) ||
                                             ChildName.StartsWith(TEXT("Json")) ||
                                             Child->Tags.Contains(FName(TEXT("SF_ExtendChild")));
                        if (bIsSmartChild)
                        {
                            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔧 EXTEND: Removing child %s from parent mChildren"), *Child->GetName());
                            ChildrenArray->RemoveAt(i);
                        }
                    }
                }
            }
        }
    }

    // Destroy tracked children
    for (AFGHologram* Child : TrackedChildren)
    {
        if (IsValid(Child))
        {
            UE_LOG(LogSmartExtend, Verbose, TEXT("🔧 EXTEND: Destroying child %s"), *Child->GetName());
            Child->SetActorHiddenInGame(true);
            Child->Destroy();
        }
    }

    // Clear tracking
    ClearTracking();

    // Clear JSON spawned holograms map
    JsonSpawnedHolograms.Empty();
}

void USFExtendHologramService::RefreshChildPositions()
{
    const EHologramMaterialState ParentMaterialState = CurrentParentHologram.IsValid()
        ? CurrentParentHologram->GetHologramMaterialState()
        : EHologramMaterialState::HMS_OK;

    // Force child holograms back to their intended positions every frame
    for (AFGHologram* Child : TrackedChildren)
    {
        if (IsValid(Child))
        {
            if (FVector* IntendedPos = ChildIntendedPositions.Find(Child))
            {
                Child->SetActorLocation(*IntendedPos);
            }
            if (FRotator* IntendedRot = ChildIntendedRotations.Find(Child))
            {
                Child->SetActorRotation(*IntendedRot);
            }

            // Also update root component to ensure mesh moves
            if (USceneComponent* ChildRoot = Child->GetRootComponent())
            {
                if (FVector* IntendedPos = ChildIntendedPositions.Find(Child))
                {
                    ChildRoot->SetWorldLocation(*IntendedPos);
                }
                if (FRotator* IntendedRot = ChildIntendedRotations.Find(Child))
                {
                    ChildRoot->SetWorldRotation(*IntendedRot);
                }
                ChildRoot->MarkRenderStateDirty();
            }

            // Keep JSON-spawned children visually aligned with the parent result.
            // They do not run normal validation/cost checks while tick-disabled.
            Child->SetActorHiddenInGame(false);
            Child->SetPlacementMaterialState(ParentMaterialState);

            if (ASFConveyorLiftHologram* LiftChild = Cast<ASFConveyorLiftHologram>(Child))
            {
                LiftChild->ForceApplyHologramMaterial();
            }
            else if (ASFConveyorBeltHologram* BeltChild = Cast<ASFConveyorBeltHologram>(Child))
            {
                BeltChild->ForceApplyHologramMaterial();
            }
            else if (ASFPipelineHologram* PipeChild = Cast<ASFPipelineHologram>(Child))
            {
                PipeChild->ForceApplyHologramMaterial();
            }
        }
    }
}

// ==================== Hologram Swapping ====================

ASFFactoryHologram* USFExtendHologramService::SwapToSmartFactoryHologram(AFGHologram* VanillaHologram)
{
    if (!VanillaHologram || !VanillaHologram->IsValidLowLevel())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Invalid vanilla hologram"));
        return nullptr;
    }

    // Only swap factory holograms
    AFGFactoryHologram* FactoryHolo = Cast<AFGFactoryHologram>(VanillaHologram);
    if (!FactoryHolo)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Not a factory hologram - %s"),
            *VanillaHologram->GetClass()->GetName());
        return nullptr;
    }

    // Get the build gun and its build state
    AFGBuildGun* BuildGun = GetPlayerBuildGun();
    if (!BuildGun)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Could not get build gun"));
        return nullptr;
    }

    UFGBuildGunStateBuild* BuildState = GetBuildGunBuildState(BuildGun);
    if (!BuildState)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: Could not get build state"));
        return nullptr;
    }

    // Get world for spawning
    UWorld* World = VanillaHologram->GetWorld();
    if (!World)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔄 EXTEND SWAP: No world"));
        return nullptr;
    }

    // Verify the vanilla hologram has a build class
    if (!VanillaHologram->GetBuildClass())
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("🔄 EXTEND SWAP: Vanilla hologram has no BuildClass"));
        return nullptr;
    }

    // Use SpawnActorDeferred so we can initialize BEFORE BeginPlay is called
    ASFFactoryHologram* CustomHologram = World->SpawnActorDeferred<ASFFactoryHologram>(
        ASFFactoryHologram::StaticClass(),
        FTransform(VanillaHologram->GetActorRotation(), VanillaHologram->GetActorLocation()),
        BuildGun,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (!CustomHologram)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("🔄 EXTEND SWAP: Failed to spawn deferred custom hologram"));
        return nullptr;
    }

    // Initialize from the vanilla hologram BEFORE BeginPlay
    CustomHologram->InitializeFromHologram(VanillaHologram);

    // [#331] Carry the Blueprint Designer context across the swap: vanilla set it on the
    // hologram it created inside the designer volume, and copies it onto every buildable at
    // construct. A swap that drops it makes everything this hologram builds invisible to the
    // designer (untracked, uncaptured by blueprint saves).
    if (AFGBuildableBlueprintDesigner* Designer = VanillaHologram->GetBlueprintDesigner())
    {
        CustomHologram->SetInsideBlueprintDesigner(Designer);
    }

    // Finish spawning - this will call BeginPlay with mBuildClass properly set
    CustomHologram->FinishSpawning(FTransform(VanillaHologram->GetActorRotation(), VanillaHologram->GetActorLocation()));

    // Replace the build gun's hologram pointer with our custom one via reflection
    FProperty* HologramProp = BuildState->GetClass()->FindPropertyByName(TEXT("mHologram"));
    if (HologramProp)
    {
        void* ValuePtr = HologramProp->ContainerPtrToValuePtr<void>(BuildState);
        if (ValuePtr)
        {
            FObjectProperty* ObjProp = CastField<FObjectProperty>(HologramProp);
            if (ObjProp)
            {
                ObjProp->SetObjectPropertyValue(ValuePtr, CustomHologram);
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND SWAP: ✅ Set mHologram via reflection"));
            }
        }
    }
    else
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("🔄 EXTEND SWAP: Could not find mHologram property"));
        CustomHologram->Destroy();
        return nullptr;
    }

    // Destroy the vanilla hologram
    VanillaHologram->Destroy();

    // Track the swap
    SwappedHologram = CustomHologram;
    bHasSwappedHologram = true;

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND SWAP: ✅ Successfully swapped to ASFFactoryHologram"));

    return CustomHologram;
}

void USFExtendHologramService::RestoreOriginalHologram()
{
    if (!bHasSwappedHologram)
    {
        return;
    }

    if (SwappedHologram.IsValid())
    {
        SwappedHologram->LockHologramPosition(false);
    }

    SwappedHologram.Reset();
    bHasSwappedHologram = false;

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔄 EXTEND SWAP: Hologram swap state cleared"));
}

ASFFactoryHologram* USFExtendHologramService::GetSwappedHologram() const
{
    return SwappedHologram.Get();
}

// ==================== JSON Spawning Support ====================

void USFExtendHologramService::StoreJsonSpawnedHolograms(const TMap<FString, AFGHologram*>& SpawnedHolograms)
{
    JsonSpawnedHolograms.Reset();
    for (const TPair<FString, AFGHologram*>& Pair : SpawnedHolograms)
    {
        JsonSpawnedHolograms.Add(Pair.Key, Pair.Value);
    }
}

void USFExtendHologramService::StoreCloneTopology(TSharedPtr<FSFCloneTopology> CloneTopology)
{
    StoredCloneTopology = CloneTopology;
}

// ==================== Helper Methods ====================

AFGBuildGun* USFExtendHologramService::GetPlayerBuildGun() const
{
    if (!Subsystem.IsValid())
    {
        return nullptr;
    }

    UWorld* World = Subsystem->GetWorld();
    if (!World)
    {
        return nullptr;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC)
    {
        return nullptr;
    }

    AFGCharacterPlayer* Character = Cast<AFGCharacterPlayer>(PC->GetPawn());
    if (!Character)
    {
        return nullptr;
    }

    return Character->GetBuildGun();
}

UFGBuildGunStateBuild* USFExtendHologramService::GetBuildGunBuildState(AFGBuildGun* BuildGun) const
{
    if (!BuildGun)
    {
        return nullptr;
    }

    return Cast<UFGBuildGunStateBuild>(BuildGun->GetBuildGunStateFor(EBuildGunState::BGS_BUILD));
}
