// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#include "Features/Extend/Net/SFExtendAuthorityScope.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "Features/Restore/SFRestoreService.h"
#include "Buildables/FGBuildableFactory.h"
#include "FGCharacterPlayer.h"
#include "FGPlayerController.h"
#include "GameFramework/Pawn.h"
#include "UObject/StrongObjectPtr.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace
{
    thread_local USFSubsystem* AuthoritySubsystem = nullptr;
    thread_local FSFExtendAuthorityScope* AuthorityScope = nullptr;
}

struct FSFExtendAuthorityScope::FImpl
{
    USFSubsystem* Subsystem;
    USFSubsystem* PreviousAuthority;
    TStrongObjectPtr<USFExtendService> HostExtend;
    TStrongObjectPtr<USFRecipeManagementService> HostRecipes;
    TStrongObjectPtr<USFGridStateService> HostGrid;
    TStrongObjectPtr<USFRestoreService> HostRestore;
    FSFCounterState HostCounters;
    FIntVector HostGridMirror;
    USFSubsystem::FAutoConnectRuntimeSettings HostSettings;
    TWeakObjectPtr<AFGHologram> HostHologram;
    TWeakObjectPtr<AFGPlayerController> HostController;
    TSubclassOf<UFGRecipe> HostRecipeMirror;
    FString HostRecipeName;
    bool bHostHasRecipe;
    TMap<TWeakObjectPtr<AFGHologram>, TSet<TWeakObjectPtr<AFGHologram>>> OriginalChildren;

    explicit FImpl(USFSubsystem* InSubsystem)
        : Subsystem(InSubsystem), PreviousAuthority(AuthoritySubsystem),
          HostExtend(InSubsystem->ExtendService.Get()), HostRecipes(InSubsystem->RecipeManagementService.Get()),
          HostGrid(InSubsystem->GridStateService.Get()), HostRestore(InSubsystem->RestoreService.Get()),
          HostCounters(InSubsystem->CounterState), HostGridMirror(InSubsystem->GridCounters),
          HostSettings(InSubsystem->AutoConnectRuntimeSettings), HostHologram(InSubsystem->ActiveHologram),
          HostController(InSubsystem->LastController), HostRecipeMirror(InSubsystem->StoredProductionRecipe),
          HostRecipeName(InSubsystem->StoredRecipeDisplayName), bHostHasRecipe(InSubsystem->bHasStoredProductionRecipe)
    {
        AuthoritySubsystem = Subsystem;
        // [MP-AUTH] The host's Walk/Restore session and tracked grid never own this transaction.
        Subsystem->ActiveHologram.Reset();
        Subsystem->LastController.Reset();
        Subsystem->RestoreService = nullptr;
        Subsystem->CounterState = FSFCounterState();
        Subsystem->GridCounters = Subsystem->CounterState.GridCounters;
        Subsystem->AutoConnectRuntimeSettings = USFSubsystem::FAutoConnectRuntimeSettings();
        Subsystem->AutoConnectRuntimeSettings.InitFromConfig(Subsystem->GetCachedConfig());
        Subsystem->GridStateService = NewObject<USFGridStateService>(Subsystem);
        Subsystem->GridStateService->Initialize(Subsystem);
        Subsystem->RecipeManagementService = NewObject<USFRecipeManagementService>(Subsystem);
        Subsystem->RecipeManagementService->Initialize(Subsystem);
        Subsystem->ExtendService = NewObject<USFExtendService>(Subsystem);
        Subsystem->ExtendService->Initialize(Subsystem);
    }

    ~FImpl()
    {
        // Native failure paths may leave the server root alive. Detach only children
        // created by this reconstruction, after vanilla has queried their final cost.
        // Existing native companion children and every local preview remain untouched.
        FArrayProperty* ChildrenProperty = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
        FMapProperty* NamesProperty = FindFProperty<FMapProperty>(AFGHologram::StaticClass(), TEXT("mChildrenNameLookupMap"));
        for (const auto& Entry : OriginalChildren)
        {
            AFGHologram* Root = Entry.Key.Get();
            if (!Root || !ChildrenProperty) continue;
            auto* Children = ChildrenProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<AFGHologram>>>(Root);
            for (int32 Index = Children->Num() - 1; Index >= 0; --Index)
            {
                AFGHologram* Child = (*Children)[Index];
                if (Entry.Value.Contains(Child)) continue;
                Children->RemoveAt(Index, 1, EAllowShrinking::No);
                if (NamesProperty)
                {
                    auto* Names = NamesProperty->ContainerPtrToValuePtr<TMap<FName, TObjectPtr<AFGHologram>>>(Root);
                    for (auto It = Names->CreateIterator(); It; ++It)
                        if (It.Value() == Child) It.RemoveCurrent();
                }
                if (IsValid(Child)) Child->Destroy();
            }
        }
        USFRecipeManagementService* RequestRecipes = Subsystem->RecipeManagementService;
        // Deferred application retains only actor/player/snapshot, never this transaction's
        // sampled state. The original service owns its readiness timer after native return.
        if (RequestRecipes && HostRecipes.IsValid())
        {
            if (UWorld* World = Subsystem->GetWorld())
                World->GetTimerManager().ClearTimer(RequestRecipes->FactorySettingsApplyTimer);
            for (const FSFPendingFactorySettingsApplication& Pending : RequestRecipes->PendingFactorySettingsApplications)
                HostRecipes->QueueFactorySettingsApplication(Pending.Factory.Get(), Pending.Player.Get(), Pending.Snapshot);
            RequestRecipes->PendingFactorySettingsApplications.Empty();
        }
        Subsystem->ExtendService = HostExtend.Get();
        Subsystem->RecipeManagementService = HostRecipes.Get();
        Subsystem->GridStateService = HostGrid.Get();
        Subsystem->RestoreService = HostRestore.Get();
        Subsystem->CounterState = HostCounters;
        Subsystem->GridCounters = HostGridMirror;
        Subsystem->AutoConnectRuntimeSettings = HostSettings;
        Subsystem->ActiveHologram = HostHologram;
        Subsystem->LastController = HostController;
        Subsystem->StoredProductionRecipe = HostRecipeMirror;
        Subsystem->StoredRecipeDisplayName = HostRecipeName;
        Subsystem->bHasStoredProductionRecipe = bHostHasRecipe;
        AuthoritySubsystem = PreviousAuthority;
    }
};

FSFExtendAuthorityScope::FSFExtendAuthorityScope(USFSubsystem* Subsystem, AFGHologram* Root)
{
    if (Subsystem)
    {
        Previous = AuthorityScope;
        Impl = MakeUnique<FImpl>(Subsystem);
        AuthorityScope = this;
        if (Root && Root->GetConstructionInstigator())
            Subsystem->LastController = Cast<AFGPlayerController>(Root->GetConstructionInstigator()->GetController());
    }
}
FSFExtendAuthorityScope::~FSFExtendAuthorityScope()
{
    if (Impl)
    {
        Impl.Reset();
        AuthorityScope = Previous;
    }
}

bool FSFExtendAuthorityScope::IsActive(const USFSubsystem* Subsystem)
{
    return Subsystem && AuthoritySubsystem == Subsystem;
}

bool FSFExtendAuthorityScope::ShouldIsolate(USFSubsystem* Subsystem, APawn* Instigator, UClass* BuildClass)
{
    if (!Subsystem || !Instigator || !BuildClass || Instigator->IsLocallyControlled()) return false;
    APlayerController* PC = Cast<APlayerController>(Instigator->GetController());
    if (!PC || !Subsystem->IsSmartEnabledForPlayer(PC)) return false;
    const FSFExtendCommitSpec* Spec = Subsystem->StagedExtendCommits.Find(PC);
    const double* Time = Subsystem->StagedExtendCommitTimes.Find(PC);
    const FSFWalkCommitSpec* Walk = Subsystem->StagedWalkCommits.Find(PC);
    const double* WalkTime = Subsystem->StagedWalkCommitTimes.Find(PC);
    const bool bCurrentWalk = Walk && Walk->bValid && Spec && Walk->BuildClass == Spec->BuildClass
        && WalkTime && FPlatformTime::Seconds() - *WalkTime <= 10.0;
    return Spec && Spec->bValid && Spec->BuildClass.Get() == BuildClass
        && Time && FPlatformTime::Seconds() - *Time <= 10.0
        && !bCurrentWalk;
}

void FSFExtendAuthorityScope::BeginReconstruction(USFSubsystem* Subsystem, AFGHologram* Root)
{
    if (!IsActive(Subsystem) || !AuthorityScope || !Root || Root->GetParentHologram()) return;
    auto& Original = AuthorityScope->Impl->OriginalChildren;
    if (Original.Contains(Root)) return;
    TSet<TWeakObjectPtr<AFGHologram>>& Children = Original.Add(Root);
    for (AFGHologram* Child : Root->GetHologramChildren()) Children.Add(Child);
}
