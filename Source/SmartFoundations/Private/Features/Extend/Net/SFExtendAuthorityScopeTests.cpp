// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.
#if WITH_DEV_AUTOMATION_TESTS
#include "SmartFoundations.h"
#include "Features/Extend/Net/SFExtendAuthorityScope.h"
#include "Features/Extend/Net/SFExtendCommitValidation.h"
#include "Features/Extend/Net/SFExtendTopologyQuery.h"
#include "Holograms/Power/SFWireHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "Buildables/FGBuildableFactory.h"
#include "FGCustomizationRecipe.h"
#include "Resources/FGPowerShardDescriptor.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendAuthorityIsolationTest,
    "SmartFoundations.Extend.Commit.HostStateIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendAuthorityIsolationTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine for isolated authority context"), GEngine)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated authority world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    USFSubsystem* Subsystem = nullptr;
    ON_SCOPE_EXIT
    {
        if (Subsystem && Subsystem->GetRecipeManagementService())
            Subsystem->GetRecipeManagementService()->Cleanup();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    Subsystem = NewObject<USFSubsystem>(World);
    USFExtendService* HostExtend = NewObject<USFExtendService>(Subsystem);
    HostExtend->Initialize(Subsystem);
    Subsystem->ExtendService = HostExtend;
    USFRecipeManagementService* HostRecipes = Subsystem->GetRecipeManagementService();
    USFGridStateService* HostGrid = Subsystem->GridStateService;

    UClass* FactoryClass = LoadClass<AFGBuildableFactory>(nullptr,
        TEXT("/Game/FactoryGame/Buildable/Factory/SmelterMk1/Build_SmelterMk1.Build_SmelterMk1_C"));
    if (!TestNotNull(TEXT("Concrete factory fixture class"), FactoryClass))
    {
        return false;
    }
    // Deferred factories supply real actor identity/authority without native BeginPlay,
    // production simulation, or a fabricated construction result from the Editor SDK.
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Spawn.bDeferConstruction = true;
    AFGBuildableFactory* HostSource = World->SpawnActor<AFGBuildableFactory>(FactoryClass,
        FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    AFGBuildableFactory* RemoteFactory = World->SpawnActor<AFGBuildableFactory>(FactoryClass,
        FVector(1000, 0, 0), FRotator::ZeroRotator, Spawn);
    AFGHologram* HostPreview = World->SpawnActor<AFGHologram>();
    AFGHologram* RemoteRoot = World->SpawnActor<AFGHologram>();
    AFGHologram* NestedRoot = World->SpawnActor<AFGHologram>();
    if (!TestNotNull(TEXT("Host source"), HostSource)
        || !TestNotNull(TEXT("Remote factory"), RemoteFactory)
        || !TestNotNull(TEXT("Host preview"), HostPreview)
        || !TestNotNull(TEXT("Remote construction root"), RemoteRoot)
        || !TestNotNull(TEXT("Nested construction root"), NestedRoot))
    {
        return false;
    }

    FSFCounterState HostCounters;
    HostCounters.GridCounters = FIntVector(10, 2, 1);
    HostCounters.SpacingX = 125;
    HostCounters.StepsY = 50;
    HostCounters.RotationZ = 15.0f;
    HostGrid->UpdateCounterState(HostCounters);
    Subsystem->CounterState = HostCounters;
    Subsystem->GridCounters = HostCounters.GridCounters;
    Subsystem->AutoConnectRuntimeSettings.BeltRoutingMode = 2;
    Subsystem->AutoConnectRuntimeSettings.PipeRoutingMode = 4;
    Subsystem->AutoConnectRuntimeSettings.BeltTierMain = 6;
    Subsystem->AutoConnectRuntimeSettings.PipeTierMain = 2;
    Subsystem->AutoConnectRuntimeSettings.bPipeIndicator = false;
    Subsystem->AutoConnectRuntimeSettings.bExtendDaisyChain = false;
    Subsystem->AutoConnectRuntimeSettings.bExtendDaisyChainPoleless = false;
    const USFSubsystem::FAutoConnectRuntimeSettings HostSettings = Subsystem->AutoConnectRuntimeSettings;

    FSFFactorySettingsSnapshot HostFactorySettings;
    HostFactorySettings.bHasRecipe = true;
    HostFactorySettings.Recipe = UFGRecipe::StaticClass();
    HostFactorySettings.bHasPotential = true;
    HostFactorySettings.Potential = 2.0f;
    HostFactorySettings.OverclockShardClass = UFGPowerShardDescriptor::StaticClass();
    HostFactorySettings.OverclockShardCount = 2;
    HostFactorySettings.bHasProductionBoost = true;
    HostFactorySettings.ProductionBoost = 1.5f;
    HostFactorySettings.ProductionBoostShardClass = UFGPowerShardDescriptor::StaticClass();
    HostFactorySettings.ProductionBoostShardCount = 1;
    HostRecipes->InstallFactorySettingsSnapshot(HostFactorySettings, FactoryClass);
    const FString HostRecipeDisplayName = Subsystem->StoredRecipeDisplayName;

    FSFCloneTopology HostPlan;
    HostPlan.ParentBuildClass = FactoryClass->GetName();
    HostPlan.ChildHolograms.AddDefaulted_GetRef().HologramId = TEXT("host_plan_marker");
    HostExtend->SetStoredCloneTopologyForServerCommit(HostPlan);
    const TSharedPtr<FSFCloneTopology> HostPlanIdentity = HostExtend->StoredCloneTopology;
    HostExtend->SetServerCommitSourceBuilding(HostSource);
    HostExtend->CurrentExtendHologram = HostPreview;
    HostExtend->bHasValidTarget = true;
    HostExtend->bExtendCommitted = true;
    HostExtend->SetExtendManualHold(true);
    HostExtend->bNeedsFinalCleanup = true;
    HostExtend->JsonSpawnedHolograms.Add(TEXT("host_preview_marker"), HostPreview);
    HostExtend->JsonBuiltActors.Add(TEXT("host_actor_marker"), HostSource);
    FSFExtendTopology HostTopology;
    HostTopology.SourceBuilding = HostSource;
    HostTopology.bIsValid = true;
    HostTopology.InputChains.AddDefaulted();
    HostExtend->GetTopologyService()->InjectTopology(HostTopology);
    Subsystem->ActiveHologram = HostPreview;

    auto CheckSnapshot = [&](const TCHAR* Context, USFRecipeManagementService* Recipes,
        const FSFFactorySettingsSnapshot& Expected)
    {
        FSFFactorySettingsSnapshot Actual;
        Recipes->CaptureFactorySettingsSnapshot(Actual);
        TestTrue(*FString::Printf(TEXT("%s: recipe presence and identity"), Context),
            Actual.bHasRecipe == Expected.bHasRecipe && Actual.Recipe == Expected.Recipe);
        TestTrue(*FString::Printf(TEXT("%s: overclock settings and shard count"), Context),
            Actual.bHasPotential == Expected.bHasPotential
            && FMath::IsNearlyEqual(Actual.Potential, Expected.Potential)
            && Actual.OverclockShardClass == Expected.OverclockShardClass
            && Actual.OverclockShardCount == Expected.OverclockShardCount);
        TestTrue(*FString::Printf(TEXT("%s: Somersloop settings and count"), Context),
            Actual.bHasProductionBoost == Expected.bHasProductionBoost
            && FMath::IsNearlyEqual(Actual.ProductionBoost, Expected.ProductionBoost)
            && Actual.ProductionBoostShardClass == Expected.ProductionBoostShardClass
            && Actual.ProductionBoostShardCount == Expected.ProductionBoostShardCount);
    };
    auto CheckHost = [&]()
    {
        TestTrue(TEXT("Host Extend service identity survives"), Subsystem->GetExtendService() == HostExtend);
        TestTrue(TEXT("Host recipe service identity survives"), Subsystem->GetRecipeManagementService() == HostRecipes);
        TestTrue(TEXT("Host grid service identity survives"), Subsystem->GridStateService == HostGrid);
        TestTrue(TEXT("Host grid/spacing/steps/rotation survive"), Subsystem->GetCounterState().Equals(HostCounters));
        TestTrue(TEXT("Legacy grid mirror agrees with host"), Subsystem->GetGridCounters() == HostCounters.GridCounters);
        TestTrue(TEXT("Host routing, tiers, indicator and power options survive"),
            Subsystem->GetAutoConnectRuntimeSettings().EqualsConfigDerived(HostSettings));
        TestTrue(TEXT("Host active preview survives"), Subsystem->GetActiveHologram() == HostPreview);
        TestTrue(TEXT("Host pinned Extend target survives"), HostExtend->GetCurrentTarget() == HostSource
            && HostExtend->IsExtendModeActive() && HostExtend->IsExtendManualHoldActive()
            && HostExtend->bExtendCommitted && HostExtend->bNeedsFinalCleanup);
        TestTrue(TEXT("Rejected client build cannot invalidate host preview"), HostExtend->IsScaledExtendValid());
        TestTrue(TEXT("Host clone plan is preserved without replacement"), HostExtend->StoredCloneTopology == HostPlanIdentity);
        if (TestTrue(TEXT("Host plan remains valid"), HostExtend->StoredCloneTopology.IsValid()))
        {
            TestEqual(TEXT("Host plan contents survive"), HostExtend->StoredCloneTopology->ChildHolograms.Num(), 1);
            if (HostExtend->StoredCloneTopology->ChildHolograms.Num() == 1)
                TestEqual(TEXT("Host plan identity marker survives"),
                    HostExtend->StoredCloneTopology->ChildHolograms[0].HologramId, FString(TEXT("host_plan_marker")));
        }
        TestTrue(TEXT("Host topology cache survives"), HostExtend->GetCurrentTopology().bIsValid
            && HostExtend->GetCurrentTopology().SourceBuilding.Get() == HostSource);
        TestEqual(TEXT("Host topology records survive"), HostExtend->GetCurrentTopology().InputChains.Num(), 1);
        TestTrue(TEXT("Host hologram and actor registries survive"),
            HostExtend->JsonSpawnedHolograms.FindRef(TEXT("host_preview_marker")) == HostPreview
            && HostExtend->JsonBuiltActors.FindRef(TEXT("host_actor_marker")) == HostSource);
        CheckSnapshot(TEXT("Host sample"), HostRecipes, HostFactorySettings);
        TestTrue(TEXT("Host recipe mirror survives"), Subsystem->bHasStoredProductionRecipe
            && Subsystem->StoredProductionRecipe == HostFactorySettings.Recipe
            && Subsystem->StoredRecipeDisplayName == HostRecipeDisplayName);
    };

    FArrayProperty* PendingProperty = FindFProperty<FArrayProperty>(USFRecipeManagementService::StaticClass(),
        TEXT("PendingFactorySettingsApplications"));
    if (!TestNotNull(TEXT("Reflected delayed factory settings queue"), PendingProperty))
    {
        return false;
    }
    auto PendingFor = [PendingProperty](USFRecipeManagementService* Recipes)
        -> const TArray<FSFPendingFactorySettingsApplication>&
    {
        return *PendingProperty->ContainerPtrToValuePtr<TArray<FSFPendingFactorySettingsApplication>>(Recipes);
    };
    HostRecipes->QueueFactorySettingsApplication(HostSource, nullptr, HostFactorySettings);
    TestEqual(TEXT("Existing host delayed work is queued"), PendingFor(HostRecipes).Num(), 1);
    TestFalse(TEXT("Host starts outside authority reconstruction"), FSFExtendAuthorityScope::IsActive(Subsystem));

    FArrayProperty* ChildrenProperty = FindFProperty<FArrayProperty>(AFGHologram::StaticClass(), TEXT("mChildren"));
    FMapProperty* NamesProperty = FindFProperty<FMapProperty>(AFGHologram::StaticClass(), TEXT("mChildrenNameLookupMap"));
    FObjectPropertyBase* ParentProperty = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mParent"));
    if (!TestNotNull(TEXT("Native reflected child collection"), ChildrenProperty)
        || !TestNotNull(TEXT("Native reflected child name lookup"), NamesProperty)
        || !TestNotNull(TEXT("Native reflected parent relationship"), ParentProperty))
    {
        return false;
    }
    AFGHologram* HostChild = World->SpawnActor<AFGHologram>();
    AFGHologram* NativeCompanion = World->SpawnActor<AFGHologram>();
    AFGHologram* NestedCompanion = World->SpawnActor<AFGHologram>();
    AFGHologram* NestedAdded = World->SpawnActor<AFGHologram>();
    ASFWireHologram* AddedWire = World->SpawnActor<ASFWireHologram>();
    if (!TestNotNull(TEXT("Existing host child"), HostChild)
        || !TestNotNull(TEXT("Existing server companion"), NativeCompanion)
        || !TestNotNull(TEXT("Existing nested server companion"), NestedCompanion)
        || !TestNotNull(TEXT("Nested reconstruction child"), NestedAdded)
        || !TestNotNull(TEXT("Priced reconstruction wire child"), AddedWire))
    {
        return false;
    }
    auto InstallChild = [&](AFGHologram* Parent, AFGHologram* Child, FName Name)
    {
        // The distributed Editor SDK's native AddChild body is a stub. Populate its
        // reflected collection/map/parent contract for this ownership-only fixture.
        ChildrenProperty->ContainerPtrToValuePtr<TArray<TObjectPtr<AFGHologram>>>(Parent)->AddUnique(Child);
        NamesProperty->ContainerPtrToValuePtr<TMap<FName, TObjectPtr<AFGHologram>>>(Parent)->Add(Name, Child);
        ParentProperty->SetObjectPropertyValue_InContainer(Child, Parent);
    };
    InstallChild(HostPreview, HostChild, TEXT("host_child"));
    InstallChild(RemoteRoot, NativeCompanion, TEXT("native_companion"));
    InstallChild(NestedRoot, NestedCompanion, TEXT("nested_companion"));
    AddedWire->SetWireEndpoints(FVector::ZeroVector, FVector(1000, 0, 0));
    const TArray<FItemAmount> WireQuote = AddedWire->GetCost(false);
    TestTrue(TEXT("Fixture has a real cable quote"), WireQuote.Num() > 0);

    FSFExtendCommitSpec Remote;
    Remote.BuildClass = FactoryClass;
    Remote.CounterState.GridCounters = FIntVector(3, 1, 2);
    Remote.CounterState.SpacingX = 250;
    Remote.CounterState.RotationZ = -30.0f;
    Remote.BeltRoutingMode = 1;
    Remote.PipeRoutingMode = 3;
    Remote.BeltTierMain = 1;
    Remote.PipeTierMain = 1;
    Remote.bPipeIndicator = true;
    Remote.FactorySettings = HostFactorySettings;
    Remote.FactorySettings.Recipe = UFGCustomizationRecipe::StaticClass();
    Remote.FactorySettings.Potential = 2.5f;
    Remote.FactorySettings.OverclockShardCount = 3;
    Remote.FactorySettings.ProductionBoost = 2.0f;
    Remote.FactorySettings.ProductionBoostShardCount = 2;
    // Missing SourceBuilding deliberately fails AFTER the real reconstruction entry has
    // installed counters, planner settings, recipe/shards, and its source/clone plan.
    {
        FSFExtendAuthorityScope Outer(Subsystem, RemoteRoot);
        USFExtendService* RemoteExtend = Subsystem->GetExtendService();
        USFRecipeManagementService* RemoteRecipes = Subsystem->GetRecipeManagementService();
        USFGridStateService* RemoteGrid = Subsystem->GridStateService;
        TestTrue(TEXT("Remote request has independent service instances"), RemoteExtend != HostExtend
            && RemoteRecipes != HostRecipes && RemoteGrid != HostGrid);
        TestTrue(TEXT("Authority request is active through real subsystem"), FSFExtendAuthorityScope::IsActive(Subsystem));
        TestTrue(TEXT("Remote request cannot expose host active preview"), Subsystem->GetActiveHologram() != HostPreview);
        TestEqual(TEXT("Missing source rejects real Extend reconstruction"),
            RemoteExtend->ReconstructCommitOnServer(RemoteRoot, Remote), 0);
        InstallChild(RemoteRoot, AddedWire, TEXT("request_wire"));
        InstallChild(RemoteRoot, AddedWire, TEXT("request_wire_alias"));
        // Re-entrant preparation cannot redefine this newly added child as a native baseline.
        FSFExtendAuthorityScope::BeginReconstruction(Subsystem, RemoteRoot);
        TestFalse(TEXT("Rejected request carries its own invalid state"), RemoteExtend->IsScaledExtendValid());
        TestTrue(TEXT("Client counters installed on request grid"), Subsystem->GetCounterState().Equals(Remote.CounterState));
        TestTrue(TEXT("Client settings used by the reconstruction"),
            Subsystem->GetAutoConnectRuntimeSettings().BeltRoutingMode == Remote.BeltRoutingMode
            && Subsystem->GetAutoConnectRuntimeSettings().PipeRoutingMode == Remote.PipeRoutingMode
            && Subsystem->GetAutoConnectRuntimeSettings().BeltTierMain == Remote.BeltTierMain
            && Subsystem->GetAutoConnectRuntimeSettings().PipeTierMain == Remote.PipeTierMain
            && Subsystem->GetAutoConnectRuntimeSettings().bPipeIndicator == Remote.bPipeIndicator);
        CheckSnapshot(TEXT("Remote sample"), RemoteRecipes, Remote.FactorySettings);

        // The live host services remain unchanged even before the request finishes.
        TestTrue(TEXT("Host grid never temporarily becomes client grid"), HostGrid->GetCounterState().Equals(HostCounters));
        CheckSnapshot(TEXT("Host sample during remote build"), HostRecipes, HostFactorySettings);
        TestTrue(TEXT("Host pin remains attached to its own target during remote build"),
            HostExtend->GetCurrentTarget() == HostSource && HostExtend->IsExtendManualHoldActive());

        RemoteExtend->SetStoredCloneTopologyForServerCommit(HostPlan);
        RemoteExtend->JsonBuiltActors.Add(TEXT("request_actor_marker"), RemoteFactory);
        {
            FSFExtendAuthorityScope Inner(Subsystem, NestedRoot);
            USFExtendService* NestedExtend = Subsystem->GetExtendService();
            TestTrue(TEXT("Nested different root has independent services"), NestedExtend != RemoteExtend
                && NestedExtend != HostExtend && Subsystem->GetRecipeManagementService() != RemoteRecipes);
            TestEqual(TEXT("Nested request has no outer built actors"), NestedExtend->JsonBuiltActors.Num(), 0);
            FSFExtendCommitSpec Restore;
            Restore.bIsRestore = true;
            Restore.BuildClass = FactoryClass;
            Restore.RestoreCounterState.GridCounters = FIntVector(2, 3, 4);
            Restore.RestoreTemplate.ParentBuildClass = TEXT("DeliberatelyMismatchedBuildClass");
            TestEqual(TEXT("Mismatched Restore rejects after installing its settings"),
                NestedExtend->ReconstructCommitOnServer(NestedRoot, Restore), 0);
            InstallChild(NestedRoot, NestedAdded, TEXT("nested_request_child"));
            FSFExtendAuthorityScope::BeginReconstruction(Subsystem, NestedRoot);
            TestTrue(TEXT("Nested reconstruction child remains alive before native return"), IsValid(NestedAdded));
            TestTrue(TEXT("Restore counter state belongs to nested request"),
                Subsystem->GetCounterState().Equals(Restore.RestoreCounterState));
            CheckSnapshot(TEXT("Empty nested sample"), Subsystem->GetRecipeManagementService(), FSFFactorySettingsSnapshot());
        }
        TestTrue(TEXT("Nested return restores outer service instances"), Subsystem->GetExtendService() == RemoteExtend
            && Subsystem->GetRecipeManagementService() == RemoteRecipes && Subsystem->GridStateService == RemoteGrid);
        TestTrue(TEXT("Nested Restore cannot overwrite outer counters"), Subsystem->GetCounterState().Equals(Remote.CounterState));
        CheckSnapshot(TEXT("Outer sample after nested rejection"), RemoteRecipes, Remote.FactorySettings);
        TestTrue(TEXT("Nested return retains outer construction registry"),
            RemoteExtend->JsonBuiltActors.FindRef(TEXT("request_actor_marker")) == RemoteFactory);
        TestTrue(TEXT("Nested return destroys only its added child"),
            !IsValid(NestedAdded) || NestedAdded->IsActorBeingDestroyed());
        TestNull(TEXT("Nested return removes its generated child name"),
            NestedRoot->FindChildHologramByName(TEXT("nested_request_child")));
        TestTrue(TEXT("Nested native companion remains attached and alive"),
            IsValid(NestedCompanion) && NestedRoot->GetHologramChildren().Contains(NestedCompanion)
            && NestedRoot->FindChildHologramByName(TEXT("nested_companion")) == NestedCompanion);
        TestTrue(TEXT("Outer child remains alive until its own native return"),
            IsValid(AddedWire) && RemoteRoot->GetHologramChildren().Contains(AddedWire));
        TestTrue(TEXT("Final payment can still query the priced wire after nested return"),
            SFExtendCommitValidation::SameCost(WireQuote, AddedWire->GetCost(false)));
        RemoteRecipes->QueueFactorySettingsApplication(RemoteFactory, nullptr, Remote.FactorySettings);
        TestEqual(TEXT("Request delayed work queued independently"), PendingFor(RemoteRecipes).Num(), 1);
        TestEqual(TEXT("Host delayed queue still untouched during request"), PendingFor(HostRecipes).Num(), 1);
    }
    CheckHost();
    TestTrue(TEXT("Rejected request destroys its generated wire after payment window"),
        !IsValid(AddedWire) || AddedWire->IsActorBeingDestroyed());
    TestFalse(TEXT("Rejected request detaches its generated wire"), RemoteRoot->GetHologramChildren().Contains(AddedWire));
    TestNull(TEXT("Rejected request removes generated child lookup"), RemoteRoot->FindChildHologramByName(TEXT("request_wire")));
    TestNull(TEXT("Rejected request removes every generated child lookup alias"),
        RemoteRoot->FindChildHologramByName(TEXT("request_wire_alias")));
    TestTrue(TEXT("Rejected request preserves the original native companion"),
        IsValid(NativeCompanion) && RemoteRoot->GetHologramChildren().Contains(NativeCompanion)
        && RemoteRoot->FindChildHologramByName(TEXT("native_companion")) == NativeCompanion);
    TestTrue(TEXT("Remote cleanup preserves the host child and its lookup"),
        IsValid(HostChild) && HostPreview->GetHologramChildren().Contains(HostChild)
        && HostPreview->FindChildHologramByName(TEXT("host_child")) == HostChild);
    TestFalse(TEXT("Authority scope ends on native return"), FSFExtendAuthorityScope::IsActive(Subsystem));
    TestEqual(TEXT("Request delayed work survives alongside existing host work"), PendingFor(HostRecipes).Num(), 2);
    bool bFoundHostWork = false;
    bool bFoundRemoteWork = false;
    for (const FSFPendingFactorySettingsApplication& Pending : PendingFor(HostRecipes))
    {
        if (Pending.Factory.Get() == HostSource)
            bFoundHostWork = Pending.Snapshot.Recipe == HostFactorySettings.Recipe
                && Pending.Snapshot.OverclockShardCount == HostFactorySettings.OverclockShardCount;
        if (Pending.Factory.Get() == RemoteFactory)
            bFoundRemoteWork = Pending.Snapshot.Recipe == Remote.FactorySettings.Recipe
                && Pending.Snapshot.OverclockShardCount == Remote.FactorySettings.OverclockShardCount
                && Pending.Snapshot.ProductionBoostShardCount == Remote.FactorySettings.ProductionBoostShardCount;
    }
    TestTrue(TEXT("Host pending factory keeps immutable host settings"), bFoundHostWork);
    TestTrue(TEXT("Remote pending factory keeps immutable client settings after return"), bFoundRemoteWork);

    {
        FSFExtendAuthorityScope Next(Subsystem, RemoteRoot);
        USFExtendService* NextExtend = Subsystem->GetExtendService();
        TestNull(TEXT("Later request inherits no Extend target"), NextExtend->GetCurrentTarget());
        TestEqual(TEXT("Later request inherits no built actors"), NextExtend->JsonBuiltActors.Num(), 0);
        TestEqual(TEXT("Later request inherits no preview actors"), NextExtend->JsonSpawnedHolograms.Num(), 0);
        TestFalse(TEXT("Later request inherits no topology cache"), NextExtend->GetCurrentTopology().bIsValid);
        Remote.FactorySettings = FSFFactorySettingsSnapshot();
        TestEqual(TEXT("Empty later commit still executes the rejection boundary"),
            NextExtend->ReconstructCommitOnServer(RemoteRoot, Remote), 0);
        CheckSnapshot(TEXT("Later empty sample"), Subsystem->GetRecipeManagementService(), Remote.FactorySettings);
    }
    CheckHost();
    TestEqual(TEXT("Empty later request does not discard queued actor settings"), PendingFor(HostRecipes).Num(), 2);

    UClass* FoundationClass = LoadClass<AFGBuildable>(nullptr,
        TEXT("/Game/FactoryGame/Buildable/Building/Foundation/Build_Foundation_8x4_01.Build_Foundation_8x4_01_C"));
    if (TestNotNull(TEXT("Unsupported Extend topology fixture class"), FoundationClass))
    {
        AFGBuildable* UnsupportedSource = World->SpawnActor<AFGBuildable>(FoundationClass,
            FVector(2000, 0, 0), FRotator::ZeroRotator, Spawn);
        if (TestNotNull(TEXT("Unsupported Extend topology source"), UnsupportedSource))
        {
            // Use the same capture function as the RCO. Its unsuccessful authoritative
            // walk clears the worker cache, which previously erased the host's live cache.
            const FSFExtendTopology NegativeReply = SFExtendTopologyQuery::Capture(Subsystem, UnsupportedSource);
            TestFalse(TEXT("Unsupported topology query returns a negative reply"), NegativeReply.bIsValid);
            TestTrue(TEXT("Negative topology reply identifies the queried source"),
                NegativeReply.SourceBuilding.Get() == UnsupportedSource);
            TestEqual(TEXT("Negative reply cannot inherit the host topology"), NegativeReply.InputChains.Num(), 0);
            TestFalse(TEXT("Topology query does not leave a construction scope active"),
                FSFExtendAuthorityScope::IsActive(Subsystem));
            CheckHost();
            TestEqual(TEXT("Topology query preserves delayed construction work"), PendingFor(HostRecipes).Num(), 2);
        }
    }
    const FSFExtendTopology MissingReply = SFExtendTopologyQuery::Capture(Subsystem, nullptr);
    TestFalse(TEXT("Missing topology source returns a negative reply"), MissingReply.bIsValid);
    TestFalse(TEXT("Missing topology source remains absent in reply"), MissingReply.SourceBuilding.IsValid());
    CheckHost();

    // Native construction can destroy the root before the subsystem poll sees it.
    // A committed preview without that root must not silently suppress a new session.
    HostPreview->Destroy();
    TestFalse(TEXT("Destroyed committed root is no longer a live preview"), HostExtend->CurrentExtendHologram.IsValid());
    TestFalse(TEXT("Expired committed root cannot claim the next aim update"),
        HostExtend->TryExtendFromBuilding(nullptr, RemoteRoot));
    TestFalse(TEXT("Expired root clears the old target gate"), HostExtend->IsExtendModeActive());
    TestFalse(TEXT("Expired root releases committed and manual holds"),
        HostExtend->bExtendCommitted || HostExtend->IsExtendManualHoldActive());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSFExtendAuthorityFallbackTest,
    "SmartFoundations.Extend.Commit.NativeRequestFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSFExtendAuthorityFallbackTest::RunTest(const FString& Parameters)
{
    if (!TestNotNull(TEXT("Engine for isolated listen context"), GEngine)) return false;
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("Isolated listen authority world"), World)) return false;
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    // UWorld derives the pre-driver listen mode from its pending URL. No transport,
    // socket, map travel, native FactoryGame construction, or new module is required.
    World->NextURL = TEXT("/Game/FactoryGame/Maps/Menu/Menu?listen");
    USFSubsystem* Subsystem = nullptr;
    ON_SCOPE_EXIT
    {
        if (Subsystem && Subsystem->GetRecipeManagementService())
            Subsystem->GetRecipeManagementService()->Cleanup();
        World->NextURL.Reset();
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };
    if (!TestTrue(TEXT("Fixture uses the engine's actual listen-server mode"), World->GetNetMode() == NM_ListenServer))
        return false;
    Subsystem = NewObject<USFSubsystem>(World);
    APlayerController* Remote = World->SpawnActor<APlayerController>();
    APlayerController* Other = World->SpawnActor<APlayerController>();
    APlayerController* Local = World->SpawnActor<APlayerController>();
    APawn* RemotePawn = World->SpawnActor<APawn>();
    APawn* OtherPawn = World->SpawnActor<APawn>();
    APawn* LocalPawn = World->SpawnActor<APawn>();
    AFGHologram* Root = World->SpawnActor<AFGHologram>();
    if (!TestNotNull(TEXT("Remote controller"), Remote) || !TestNotNull(TEXT("Other controller"), Other)
        || !TestNotNull(TEXT("Local controller"), Local) || !TestNotNull(TEXT("Remote pawn"), RemotePawn)
        || !TestNotNull(TEXT("Other pawn"), OtherPawn) || !TestNotNull(TEXT("Local pawn"), LocalPawn)
        || !TestNotNull(TEXT("Fallback construction root"), Root)) return false;
    Local->SetAsLocalPlayerController();
    Remote->Possess(RemotePawn);
    Other->Possess(OtherPawn);
    Local->Possess(LocalPawn);
    if (!TestFalse(TEXT("Remote fixture is a real non-owning controller"), Remote->IsLocalController())
        || !TestFalse(TEXT("Remote pawn uses the real non-local ownership predicate"), RemotePawn->IsLocallyControlled()))
        return false;

    FSFExtendCommitSpec Commit;
    Commit.bValid = true;
    Commit.BuildClass = AFGBuildable::StaticClass();
    Commit.CounterState.GridCounters = FIntVector(3, 1, 2);
    TestFalse(TEXT("Unstaged remote player cannot enter isolation"), FSFExtendAuthorityScope::ShouldIsolate(Subsystem, RemotePawn, Commit.BuildClass));
    Subsystem->StageExtendCommitForPlayer(Remote, Commit);
    TestTrue(TEXT("Fresh remote player's Extend is eligible"), FSFExtendAuthorityScope::ShouldIsolate(Subsystem, RemotePawn, Commit.BuildClass));
    TestFalse(TEXT("Missing construction class cannot own staged Extend"),
        FSFExtendAuthorityScope::ShouldIsolate(Subsystem, RemotePawn, nullptr));
    TestFalse(TEXT("A different construction class cannot own staged Extend"),
        FSFExtendAuthorityScope::ShouldIsolate(Subsystem, RemotePawn, AFGBuildableFactory::StaticClass()));
    TestFalse(TEXT("Another remote player cannot inherit its staged intent"), FSFExtendAuthorityScope::ShouldIsolate(Subsystem, OtherPawn, Commit.BuildClass));
    Subsystem->StageExtendCommitForPlayer(Local, Commit);
    TestFalse(TEXT("Local player never receives a remote construction context"), FSFExtendAuthorityScope::ShouldIsolate(Subsystem, LocalPawn, Commit.BuildClass));

    FSFWalkCommitSpec Walk;
    Walk.bValid = true;
    Walk.BuildClass = Commit.BuildClass;
    Subsystem->StageWalkCommitForPlayer(Remote, Walk);
    TestFalse(TEXT("Current matching Walk owns its own construction path"), FSFExtendAuthorityScope::ShouldIsolate(Subsystem, RemotePawn, Commit.BuildClass));
    Walk.BuildClass = AFGBuildableFactory::StaticClass();
    Subsystem->StageWalkCommitForPlayer(Remote, Walk);
    TestTrue(TEXT("A different build class's Walk cannot suppress Extend isolation"),
        FSFExtendAuthorityScope::ShouldIsolate(Subsystem, RemotePawn, Commit.BuildClass));
    Walk.bValid = false;
    Subsystem->StageWalkCommitForPlayer(Remote, Walk);
    Subsystem->SetSmartEnabledForPlayer(Remote, false);
    Subsystem->StageExtendCommitForPlayer(Remote, Commit);
    TestFalse(TEXT("Opted-out player cannot install late staged intent"), FSFExtendAuthorityScope::ShouldIsolate(Subsystem, RemotePawn, Commit.BuildClass));
    Subsystem->SetSmartEnabledForPlayer(Remote, true);
    Subsystem->StageExtendCommitForPlayer(Remote, Commit);

    FObjectPropertyBase* BuildProperty = FindFProperty<FObjectPropertyBase>(AFGHologram::StaticClass(), TEXT("mBuildClass"));
    if (!TestNotNull(TEXT("Native reflected build class"), BuildProperty)) return false;
    BuildProperty->SetObjectPropertyValue_InContainer(Root, Commit.BuildClass.Get());
    Root->SetConstructionInstigator(RemotePawn);
    TestTrue(TEXT("Fallback fixture is the authority root for the staged build class"), Root->HasAuthority()
        && !Root->GetParentHologram() && Root->GetBuildClass() == Commit.BuildClass.Get());

    FSFCounterState HostCounters;
    HostCounters.GridCounters = FIntVector(10, 2, 1);
    Subsystem->UpdateCounterState(HostCounters);
    USFRecipeManagementService* HostRecipes = Subsystem->GetRecipeManagementService();
    Subsystem->AutoConnectRuntimeSettings.BeltTierMain = 6;
    SFExtendCommitValidation::IsolateAuthority(Subsystem, Root);
    TestFalse(TEXT("Aiming cannot create a construction context without a native request"),
        FSFExtendAuthorityScope::IsActive(Subsystem));
    {
        SFExtendCommitValidation::FRequestScope Outer;
        BuildProperty->SetObjectPropertyValue_InContainer(Root, AFGBuildableFactory::StaticClass());
        SFExtendCommitValidation::IsolateAuthority(Subsystem, Root);
        TestFalse(TEXT("Wrong-class native root cannot own another build's staged request"),
            FSFExtendAuthorityScope::IsActive(Subsystem));
        BuildProperty->SetObjectPropertyValue_InContainer(Root, Commit.BuildClass.Get());
        SFExtendCommitValidation::IsolateAuthority(Subsystem, Root);
        TestTrue(TEXT("Validation fallback creates authority context inside native request"),
            FSFExtendAuthorityScope::IsActive(Subsystem));
        USFExtendService* RequestExtend = Subsystem->GetExtendService();
        USFRecipeManagementService* RequestRecipes = Subsystem->GetRecipeManagementService();
        TestTrue(TEXT("Fallback uses request-owned services"), RequestExtend && RequestRecipes != HostRecipes);
        Subsystem->UpdateCounterState(Commit.CounterState);
        SFExtendCommitValidation::SetPrepared(Root, false);
        {
            SFExtendCommitValidation::FRequestScope Inner;
            SFExtendCommitValidation::IsolateAuthority(Subsystem, Root);
            TestTrue(TEXT("Nested native callback retains the original construction context"),
                Subsystem->GetExtendService() == RequestExtend && Subsystem->GetRecipeManagementService() == RequestRecipes);
            const bool* Prepared = SFExtendCommitValidation::FindPrepared(Root);
            TestTrue(TEXT("Nested callback retains the root's preparation result"), Prepared && !*Prepared);
            SFExtendCommitValidation::SetPrepared(Root, true);
        }
        TestTrue(TEXT("Nested native return cannot finish the outer authority context"),
            FSFExtendAuthorityScope::IsActive(Subsystem) && Subsystem->GetExtendService() == RequestExtend);
        TestTrue(TEXT("Request settings remain available through the outer payment window"),
            Subsystem->GetCounterState().Equals(Commit.CounterState));
        const bool* Prepared = SFExtendCommitValidation::FindPrepared(Root);
        TestTrue(TEXT("Nested preparation outcome remains available to outer construction"), Prepared && *Prepared);
    }
    TestFalse(TEXT("Native request return destroys the fallback authority context"), FSFExtendAuthorityScope::IsActive(Subsystem));
    TestFalse(TEXT("Native request return ends preparation tracking"), SFExtendCommitValidation::IsRequestActive());
    TestNull(TEXT("Native request return discards its preparation result"), SFExtendCommitValidation::FindPrepared(Root));
    TestTrue(TEXT("Fallback restores host grid, settings and persistent recipe service"),
        Subsystem->GetCounterState().Equals(HostCounters)
        && Subsystem->GetAutoConnectRuntimeSettings().BeltTierMain == 6
        && Subsystem->GetRecipeManagementService() == HostRecipes);
    {
        SFExtendCommitValidation::FRequestScope Next;
        TestNull(TEXT("Later native request cannot inherit preparation"), SFExtendCommitValidation::FindPrepared(Root));
        TestFalse(TEXT("Later native request must establish its own authority context"), FSFExtendAuthorityScope::IsActive(Subsystem));
    }
    return true;
}
#endif
