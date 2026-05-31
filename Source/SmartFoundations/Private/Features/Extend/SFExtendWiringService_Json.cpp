// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendWiringService - JSON-based post-build wiring (slice E2 unit I).
 * GenerateAndExecuteWiring + JSON built-actor registry moved verbatim from SFExtendService;
 * operate on its shared state via ExtendService->. Subsystem uses this service own back-ref.
 */

#include "Features/Extend/SFExtendWiringServiceImpl.h"

int32 USFExtendWiringService::GenerateAndExecuteWiring(AFGBuildableFactory* NewFactory)
{
    ExtendService->bRestoredScaledWiringDeferred = false;

    if (!NewFactory)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 EXTEND Phase 5/6: GenerateAndExecuteWiring called with null factory"));
        return 0;
    }

    // Check if we have stored clone topology from JSON spawning
    if (!ExtendService->StoredCloneTopology.IsValid() || ExtendService->StoredCloneTopology->ChildHolograms.Num() == 0)
    {
        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: No stored clone topology - skipping JSON wiring"));
        return 0;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Generating wiring manifest from %d child holograms, %d registered built actors"),
        ExtendService->StoredCloneTopology->ChildHolograms.Num(), ExtendService->JsonBuiltActors.Num());

    // Build clone_id -> buildable mapping from ExtendService->JsonBuiltActors (populated during Construct())
    // Holograms are destroyed after Construct(), so we can't use ExtendService->JsonSpawnedHolograms here
    TMap<FString, AActor*> CloneIdToBuildable;

    // Add parent factory
    CloneIdToBuildable.Add(TEXT("parent"), NewFactory);

    // Copy all registered built actors
    for (const auto& Pair : ExtendService->JsonBuiltActors)
    {
        const FString& CloneId = Pair.Key;
        AActor* BuiltActor = Pair.Value;

        if (IsValid(BuiltActor))
        {
            CloneIdToBuildable.Add(CloneId, BuiltActor);
            UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Using registered actor %s -> %s"),
                *CloneId, *BuiltActor->GetName());
        }
        else
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 EXTEND Phase 5/6: Registered actor for %s is no longer valid"), *CloneId);
        }
    }

    // Pre-resolve "source:" targets from topology into CloneIdToBuildable.
    // Clone 1's lane segments have "source:ActorName" targets pointing to the SOURCE
    // building's distributors (real world actors, not clones). Without ExtendService, Generate()
    // can't find them in CloneIdToBuildable and skips the connection.
    if (ExtendService->StoredCloneTopology.IsValid())
    {
        for (const FSFCloneHologram& Holo : ExtendService->StoredCloneTopology->ChildHolograms)
        {
            auto ResolveSourceTarget = [&](const FString& Target)
            {
                if (Target.StartsWith(TEXT("source:")) && !CloneIdToBuildable.Contains(Target))
                {
                    FString SourceActorName = Target.Mid(7);
                    AFGBuildable* SourceBuildable = GetSourceBuildableByName(SourceActorName);
                    if (SourceBuildable)
                    {
                        CloneIdToBuildable.Add(Target, SourceBuildable);
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Pre-resolved source target '%s' → %s"),
                            *Target, *SourceBuildable->GetName());
                    }
                }
            };

            ResolveSourceTarget(Holo.CloneConnections.ConveyorAny0.Target);
            ResolveSourceTarget(Holo.CloneConnections.ConveyorAny1.Target);
        }
    }

    // Restored scaled replay uses synthetic factory ids (rr_X_Y_factory) for
    // child factory targets. Those child factories are vanilla factory holograms,
    // so they can miss ExtendService->JsonBuiltActors self-registration. Resolve them here before
    // the manifest decides which belt/pipe endpoints are wireable.
    if (ExtendService->bRestoredCloneTopologyActive && ExtendService->StoredCloneTopology.IsValid())
    {
        auto TryParseRestoredScaledFactoryId = [](const FString& CloneId, int32& OutX, int32& OutY) -> bool
        {
            if (!CloneId.StartsWith(TEXT("rr_")) || !CloneId.EndsWith(TEXT("_factory")))
            {
                return false;
            }

            FString GridText = CloneId.LeftChop(8).Mid(3);
            TArray<FString> Parts;
            GridText.ParseIntoArray(Parts, TEXT("_"), true);
            if (Parts.Num() != 2)
            {
                return false;
            }

            OutX = FCString::Atoi(*Parts[0]);
            OutY = FCString::Atoi(*Parts[1]);
            return true;
        };

        auto IsRestoredScaledFactoryId = [&](const FString& CloneId) -> bool
        {
            int32 UnusedX = 0;
            int32 UnusedY = 0;
            return TryParseRestoredScaledFactoryId(CloneId, UnusedX, UnusedY);
        };

        TSet<FString> RequiredFactoryIds;
        for (const auto& SpawnedPair : ExtendService->JsonSpawnedHolograms)
        {
            if (IsRestoredScaledFactoryId(SpawnedPair.Key))
            {
                RequiredFactoryIds.Add(SpawnedPair.Key);
            }
        }
        for (const FSFCloneHologram& Holo : ExtendService->StoredCloneTopology->ChildHolograms)
        {
            if (IsRestoredScaledFactoryId(Holo.CloneConnections.ConveyorAny0.Target))
            {
                RequiredFactoryIds.Add(Holo.CloneConnections.ConveyorAny0.Target);
            }
            if (IsRestoredScaledFactoryId(Holo.CloneConnections.ConveyorAny1.Target))
            {
                RequiredFactoryIds.Add(Holo.CloneConnections.ConveyorAny1.Target);
            }
        }

        constexpr float ExistingFactoryMatchRadiusCm = 3000.0f;
        constexpr float ExistingFactoryMatchRadiusSq = ExistingFactoryMatchRadiusCm * ExistingFactoryMatchRadiusCm;
        constexpr float FactoryMatchRadiusCm = 3000.0f;
        constexpr float FactoryMatchRadiusSq = FactoryMatchRadiusCm * FactoryMatchRadiusCm;
        int32 MissingRestoredFactoryCount = 0;
        int32 ResolvedRestoredFactoryCount = 0;

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
            TEXT("[SmartRestore][Extend] Restored scaled factory resolution: required=%d previewLocations=%d spawnedHolograms=%d builtActors=%d parentValid=%d"),
            RequiredFactoryIds.Num(),
            ExtendService->RestoredScaledFactoryPreviewLocations.Num(),
            ExtendService->JsonSpawnedHolograms.Num(),
            ExtendService->JsonBuiltActors.Num(),
            ExtendService->RestoredCloneParentHologram.IsValid() ? 1 : 0);

        auto ApplyRecipeToRestoredFactory = [&](AFGBuildableFactory* Factory)
        {
            if (!Factory || !Subsystem.IsValid())
            {
                return;
            }

            USFRecipeManagementService* RecipeSvc = Subsystem->GetRecipeManagementService();
            if (RecipeSvc && RecipeSvc->HasStoredProductionRecipe() && RecipeSvc->GetStoredProductionRecipe())
            {
                RecipeSvc->ApplyStoredProductionRecipeToBuilding(Factory);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("[SmartRestore][Extend] Applied stored recipe %s to restored scaled factory %s"),
                    *RecipeSvc->GetStoredProductionRecipe()->GetName(),
                    *Factory->GetName());
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("[SmartRestore][Extend] No stored recipe available for restored scaled factory %s (service=%d, subsystemHas=%d)"),
                    *Factory->GetName(),
                    RecipeSvc ? 1 : 0,
                    Subsystem->bHasStoredProductionRecipe ? 1 : 0);
            }
        };

        auto MeasureFactoryMatch = [](AFGBuildableFactory* Candidate, const FVector& ExpectedLocation, FString& OutBasis) -> float
        {
            if (!IsValid(Candidate))
            {
                OutBasis = TEXT("invalid");
                return FLT_MAX;
            }

            float BestDistSq = FVector::DistSquared(Candidate->GetActorLocation(), ExpectedLocation);
            OutBasis = TEXT("actor");

            TArray<UFGFactoryConnectionComponent*> FactoryConnectors;
            Candidate->GetComponents<UFGFactoryConnectionComponent>(FactoryConnectors);
            for (UFGFactoryConnectionComponent* Connector : FactoryConnectors)
            {
                if (!Connector)
                {
                    continue;
                }

                const float DistSq = FVector::DistSquared(Connector->GetComponentLocation(), ExpectedLocation);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    OutBasis = FString::Printf(TEXT("factory connector %s"), *Connector->GetName());
                }
            }

            TArray<UFGPipeConnectionComponentBase*> PipeConnectors;
            Candidate->GetComponents<UFGPipeConnectionComponentBase>(PipeConnectors);
            for (UFGPipeConnectionComponentBase* Connector : PipeConnectors)
            {
                if (!Connector)
                {
                    continue;
                }

                const float DistSq = FVector::DistSquared(Connector->GetComponentLocation(), ExpectedLocation);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    OutBasis = FString::Printf(TEXT("pipe connector %s"), *Connector->GetName());
                }
            }

            return BestDistSq;
        };

        const FString ExpectedRestoredFactoryClassName = ExtendService->StoredCloneTopology.IsValid()
            ? ExtendService->StoredCloneTopology->ParentBuildClass
            : FString();
        auto IsExpectedRestoredFactoryClass = [&](AFGBuildableFactory* Candidate) -> bool
        {
            if (!IsValid(Candidate))
            {
                return false;
            }
            if (!ExpectedRestoredFactoryClassName.IsEmpty())
            {
                return Candidate->GetClass() && Candidate->GetClass()->GetName() == ExpectedRestoredFactoryClassName;
            }
            return !NewFactory || Candidate->GetClass() == NewFactory->GetClass();
        };

        TSet<AActor*> UsedRestoredFactoryActors;
        if (AActor* const* ParentActor = CloneIdToBuildable.Find(TEXT("parent")))
        {
            AFGBuildableFactory* ParentFactory = Cast<AFGBuildableFactory>(*ParentActor);
            if (IsExpectedRestoredFactoryClass(ParentFactory) && ExtendService->StoredCloneTopology.IsValid())
            {
                FString ParentMatchBasis;
                const FVector ExpectedParentLocation = ExtendService->StoredCloneTopology->ParentTransform.Location.ToFVector();
                const float ParentMatchDistSq = MeasureFactoryMatch(ParentFactory, ExpectedParentLocation, ParentMatchBasis);
                if (ParentMatchDistSq <= FactoryMatchRadiusSq)
                {
                    UsedRestoredFactoryActors.Add(ParentFactory);
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                        TEXT("[SmartRestore][Extend] Reserved restored parent factory %s (dist=%.0fcm via %s)"),
                        *ParentFactory->GetName(),
                        FMath::Sqrt(ParentMatchDistSq),
                        *ParentMatchBasis);
                }
            }
            else if (IsExpectedRestoredFactoryClass(ParentFactory))
            {
                UsedRestoredFactoryActors.Add(*ParentActor);
            }
        }

        TArray<FString> SortedRequiredFactoryIds = RequiredFactoryIds.Array();
        SortedRequiredFactoryIds.Sort([](const FString& A, const FString& B)
        {
            return A < B;
        });

        auto TryCalculateFactoryLocationFromStoredTopology = [&](int32 GridX, int32 GridY, FVector& OutLocation) -> bool
        {
            if (!ExtendService->StoredCloneTopology.IsValid() || !Subsystem.IsValid())
            {
                return false;
            }

            USFBuildableSizeRegistry::Initialize();
            FVector BuildingSize(800.0f, 800.0f, 400.0f);
            if (NewFactory)
            {
                BuildingSize = USFBuildableSizeRegistry::GetProfile(NewFactory->GetClass()).DefaultSize;
            }
            else if (ExtendService->RestoredCloneParentHologram.IsValid() && ExtendService->RestoredCloneParentHologram->GetBuildClass())
            {
                BuildingSize = USFBuildableSizeRegistry::GetProfile(ExtendService->RestoredCloneParentHologram->GetBuildClass()).DefaultSize;
            }

            const FSFCounterState& State = Subsystem->GetCounterState();
            const FVector ParentLocation = ExtendService->StoredCloneTopology->ParentTransform.Location.ToFVector();
            const FRotator ParentRotation = ExtendService->StoredCloneTopology->ParentTransform.Rotation.ToFRotator();
            float XDirectionSign = State.GridCounters.X < 0 ? -1.0f : 1.0f;

            const FSFCloneTopology* TemplateTopology = ExtendService->RestoredCloneTopologyTemplate.IsValid()
                ? ExtendService->RestoredCloneTopologyTemplate.Get()
                : nullptr;
            if (TemplateTopology)
            {
                const FRotator OriginalParentRotation = TemplateTopology->ParentTransform.Rotation.ToFRotator();
                const FRotator RotationDelta = ParentRotation - OriginalParentRotation;
                const FVector CapturedStep = RotationDelta.RotateVector(TemplateTopology->WorldOffset.ToFVector());
                const FVector CapturedLocalStep = ParentRotation.UnrotateVector(CapturedStep);
                if (!FMath::IsNearlyZero(CapturedLocalStep.X))
                {
                    XDirectionSign *= FMath::Sign(CapturedLocalStep.X);
                }
            }

            const float StepDistance = FMath::Max(1.0f, BuildingSize.X + static_cast<float>(State.SpacingX));
            FVector LocalOffset = FVector::ZeroVector;

            if (!FMath::IsNearlyZero(State.RotationZ))
            {
                const float StepRadians = FMath::Abs(FMath::DegreesToRadians(State.RotationZ));
                const float Radius = (StepRadians > KINDA_SMALL_NUMBER) ? StepDistance / StepRadians : 0.0f;
                const float SignRotation = (State.RotationZ >= 0.0f) ? 1.0f : -1.0f;

                auto OffsetAtCloneIndex = [&](int32 CloneIndex) -> FVector
                {
                    const float AngleDeg = static_cast<float>(CloneIndex) * State.RotationZ;
                    const float AbsAngleRad = FMath::Abs(FMath::DegreesToRadians(AngleDeg));

                    FVector Offset;
                    Offset.X = XDirectionSign * Radius * FMath::Sin(AbsAngleRad);
                    Offset.Y = SignRotation * (Radius - Radius * FMath::Cos(AbsAngleRad));
                    Offset.Z = static_cast<float>(State.StepsX * CloneIndex);
                    return Offset;
                };

                const FVector ParentCloneOffset = OffsetAtCloneIndex(1);
                const FVector TargetCloneOffset = OffsetAtCloneIndex(GridX + 1);
                LocalOffset = TargetCloneOffset - ParentCloneOffset;
            }
            else
            {
                LocalOffset.X = XDirectionSign * StepDistance * static_cast<float>(GridX);
                LocalOffset.Z = static_cast<float>(State.StepsX * GridX);
            }

            if (GridY != 0)
            {
                const float YSign = State.GridCounters.Y < 0 ? -1.0f : 1.0f;
                const float RowDistance = FMath::Max(1.0f, BuildingSize.Y + static_cast<float>(State.SpacingY));
                LocalOffset.Y += RowDistance * static_cast<float>(GridY) * YSign;
                LocalOffset.Z += static_cast<float>(State.StepsY * GridY);
            }

            OutLocation = ParentLocation + ParentRotation.RotateVector(FVector(LocalOffset.X, LocalOffset.Y, 0.0f));
            OutLocation.Z += LocalOffset.Z;
            return true;
        };

        for (const FString& FactoryId : SortedRequiredFactoryIds)
        {
            AActor* ExistingMappedActor = nullptr;
            if (AActor** FoundMappedActor = CloneIdToBuildable.Find(FactoryId))
            {
                ExistingMappedActor = *FoundMappedActor;
            }

            FVector ExpectedLocation = FVector::ZeroVector;
            bool bHasExpectedLocation = false;
            if (const FVector* CachedPreviewLocation = ExtendService->RestoredScaledFactoryPreviewLocations.Find(FactoryId))
            {
                ExpectedLocation = *CachedPreviewLocation;
                bHasExpectedLocation = true;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("[SmartRestore][Extend] Using cached preview location for restored scaled factory %s: %s"),
                    *FactoryId,
                    *ExpectedLocation.ToString());
            }
            if (!bHasExpectedLocation)
            {
                if (AFGHologram* const* SpawnedFactoryHologram = ExtendService->JsonSpawnedHolograms.Find(FactoryId))
                {
                    if (IsValid(*SpawnedFactoryHologram))
                    {
                        ExpectedLocation = (*SpawnedFactoryHologram)->GetActorLocation();
                        bHasExpectedLocation = true;
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                            TEXT("[SmartRestore][Extend] Using live hologram location for restored scaled factory %s: %s"),
                            *FactoryId,
                            *ExpectedLocation.ToString());
                    }
                }
            }
            if (!bHasExpectedLocation && IsValid(ExistingMappedActor))
            {
                ExpectedLocation = ExistingMappedActor->GetActorLocation();
                bHasExpectedLocation = true;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("[SmartRestore][Extend] Using existing mapped actor location for restored scaled factory %s: %s"),
                    *FactoryId,
                    *ExpectedLocation.ToString());
            }
            if (!bHasExpectedLocation && ExtendService->RestoredCloneParentHologram.IsValid() && Subsystem.IsValid())
            {
                int32 GridX = 0;
                int32 GridY = 0;
                if (TryParseRestoredScaledFactoryId(FactoryId, GridX, GridY))
                {
                    const FSFCloneTopology* TemplateTopology = ExtendService->RestoredCloneTopologyTemplate.IsValid()
                        ? ExtendService->RestoredCloneTopologyTemplate.Get()
                        : nullptr;
                    const FRestoredScaledClonePlacement Placement = CalculateRestoredScaledClonePlacement(
                        ExtendService->RestoredCloneParentHologram.Get(),
                        TemplateTopology,
                        Subsystem->GetCounterState(),
                        GridX,
                        GridY);
                    ExpectedLocation = ExtendService->RestoredCloneParentHologram->GetActorLocation() + Placement.WorldOffset;
                    bHasExpectedLocation = true;
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                        TEXT("[SmartRestore][Extend] Recomputed restored scaled factory location for %s from live parent: %s"),
                        *FactoryId,
                        *ExpectedLocation.ToString());
                }
            }
            if (!bHasExpectedLocation && Subsystem.IsValid())
            {
                int32 GridX = 0;
                int32 GridY = 0;
                if (TryParseRestoredScaledFactoryId(FactoryId, GridX, GridY)
                    && TryCalculateFactoryLocationFromStoredTopology(GridX, GridY, ExpectedLocation))
                {
                    bHasExpectedLocation = true;
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                        TEXT("[SmartRestore][Extend] Recomputed restored scaled factory location for %s from stored topology: %s"),
                        *FactoryId,
                        *ExpectedLocation.ToString());
                }
            }

            const bool bExistingIsFactory = IsExpectedRestoredFactoryClass(Cast<AFGBuildableFactory>(ExistingMappedActor));
            FString ExistingMatchBasis;
            const float ExistingMatchDistSq = bExistingIsFactory && bHasExpectedLocation
                ? MeasureFactoryMatch(Cast<AFGBuildableFactory>(ExistingMappedActor), ExpectedLocation, ExistingMatchBasis)
                : 0.0f;
            if (bExistingIsFactory && (!bHasExpectedLocation || ExistingMatchDistSq <= ExistingFactoryMatchRadiusSq))
            {
                ApplyRecipeToRestoredFactory(Cast<AFGBuildableFactory>(ExistingMappedActor));
                UsedRestoredFactoryActors.Add(ExistingMappedActor);
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("[SmartRestore][Extend] Restored scaled factory %s already mapped to %s%s%s"),
                    *FactoryId,
                    *ExistingMappedActor->GetName(),
                    bHasExpectedLocation ? TEXT(" via ") : TEXT(""),
                    bHasExpectedLocation ? *ExistingMatchBasis : TEXT(""));
                continue;
            }

            if (!bHasExpectedLocation)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                    TEXT("[SmartRestore][Extend] Missing restored scaled factory mapping for %s and no location was available (stored=%d, previewLocations=%d, parentValid=%d, subsystem=%d)"),
                    *FactoryId,
                    ExtendService->StoredCloneTopology.IsValid() ? 1 : 0,
                    ExtendService->RestoredScaledFactoryPreviewLocations.Num(),
                    ExtendService->RestoredCloneParentHologram.IsValid() ? 1 : 0,
                    Subsystem.IsValid() ? 1 : 0);
                MissingRestoredFactoryCount++;
                continue;
            }

            AFGBuildableFactory* BestFactory = nullptr;
            FString BestMatchBasis;
            float BestDistSq = FactoryMatchRadiusSq;
            for (TActorIterator<AFGBuildableFactory> It(GetWorld()); It; ++It)
            {
                AFGBuildableFactory* Candidate = *It;
                if (!IsValid(Candidate))
                {
                    continue;
                }
                if (!IsExpectedRestoredFactoryClass(Candidate))
                {
                    continue;
                }
                if (UsedRestoredFactoryActors.Contains(Candidate))
                {
                    continue;
                }

                FString MatchBasis;
                const float DistSq = MeasureFactoryMatch(Candidate, ExpectedLocation, MatchBasis);
                if (DistSq < BestDistSq)
                {
                    BestDistSq = DistSq;
                    BestFactory = Candidate;
                    BestMatchBasis = MatchBasis;
                }
            }

            if (BestFactory)
            {
                CloneIdToBuildable.Add(FactoryId, BestFactory);
                ExtendService->JsonBuiltActors.Add(FactoryId, BestFactory);
                UsedRestoredFactoryActors.Add(BestFactory);
                ApplyRecipeToRestoredFactory(BestFactory);
                ResolvedRestoredFactoryCount++;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("[SmartRestore][Extend] Resolved restored scaled factory %s -> %s (dist=%.0fcm via %s)"),
                    *FactoryId,
                    *BestFactory->GetName(),
                    FMath::Sqrt(BestDistSq),
                    *BestMatchBasis);
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                    TEXT("[SmartRestore][Extend] Could not resolve restored scaled factory %s near %s"),
                    *FactoryId,
                    *ExpectedLocation.ToString());
                MissingRestoredFactoryCount++;
            }
        }

        if (MissingRestoredFactoryCount > 0)
        {
            ExtendService->bRestoredScaledWiringDeferred = true;
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                TEXT("[SmartRestore][Extend] Waiting for %d restored scaled factor%s before JSON wiring (resolved now=%d, required=%d)"),
                MissingRestoredFactoryCount,
                MissingRestoredFactoryCount == 1 ? TEXT("y") : TEXT("ies"),
                ResolvedRestoredFactoryCount,
                RequiredFactoryIds.Num());
            if (!ExtendService->bRestoredScaledWiringRetryScheduled && ExtendService->RestoredScaledWiringRetryAttempts < 5 && NewFactory && GetWorld())
            {
                ExtendService->bRestoredScaledWiringRetryScheduled = true;
                ExtendService->RestoredScaledWiringRetryAttempts++;
                TWeakObjectPtr<USFExtendService> WeakThis(ExtendService);
                TWeakObjectPtr<AFGBuildableFactory> WeakFactory(NewFactory);
                GetWorld()->GetTimerManager().SetTimerForNextTick([WeakThis, WeakFactory]()
                {
                    if (!WeakThis.IsValid())
                    {
                        return;
                    }

                    WeakThis->bRestoredScaledWiringRetryScheduled = false;
                    if (WeakFactory.IsValid() && WeakThis->HasPendingPostBuildWiring())
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                            TEXT("[SmartRestore][Extend] Retrying restored scaled JSON wiring after deferred factory wait"));
                        WeakThis->WireBuiltChildConnections(WeakFactory.Get());
                    }
                });
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                    TEXT("[SmartRestore][Extend] Restored scaled JSON wiring deferred without scheduling retry (scheduled=%d, attempts=%d, factory=%s, world=%d)"),
                    ExtendService->bRestoredScaledWiringRetryScheduled ? 1 : 0,
                    ExtendService->RestoredScaledWiringRetryAttempts,
                    *GetNameSafe(NewFactory),
                    GetWorld() ? 1 : 0);
            }
            return 0;
        }

        ExtendService->bRestoredScaledWiringDeferred = false;
        ExtendService->bRestoredScaledWiringRetryScheduled = false;
        ExtendService->RestoredScaledWiringRetryAttempts = 0;
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Mapped %d buildables (including parent + source targets)"), CloneIdToBuildable.Num());

    // Generate wiring manifest
    FSFWiringManifest WiringManifest = FSFWiringManifest::Generate(
        *ExtendService->StoredCloneTopology,
        CloneIdToBuildable,
        NewFactory);

    // Resolve source buildable targets for lane segments connecting to source junctions
    // These have bIsSourceBuildable=true and need resolution via GetSourceBuildableByName
    for (FSFWiringConnection& PipeConn : WiringManifest.PipeConnections)
    {
        if (PipeConn.Target.bIsSourceBuildable && !PipeConn.Target.ResolvedActor)
        {
            // Target.CloneId is "source:ActorName.ConnectorName" format
            FString SourceRef = PipeConn.Target.CloneId;
            if (SourceRef.StartsWith(TEXT("source:")))
            {
                FString SourceActorName = SourceRef.Mid(7);  // Remove "source:" prefix
                AFGBuildable* SourceBuildable = GetSourceBuildableByName(SourceActorName);
                if (SourceBuildable)
                {
                    PipeConn.Target.ResolvedActor = SourceBuildable;
                    PipeConn.Target.ActorName = SourceBuildable->GetName();
                    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Resolved source buildable '%s' -> %s"),
                        *SourceActorName, *SourceBuildable->GetName());
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔌 EXTEND Phase 5/6: Failed to resolve source buildable '%s'"),
                        *SourceActorName);
                }
            }
        }
    }

    // Save manifest for debugging
    FString LogDir = FPaths::ProjectLogDir();
    WiringManifest.SaveToFile(LogDir / TEXT("WiringManifest.json"));

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Generated manifest - %d belt connections, %d pipe connections"),
        WiringManifest.BeltConnections.Num(), WiringManifest.PipeConnections.Num());

    // Execute all wiring in single tick
    int32 WiredCount = WiringManifest.ExecuteWiring(GetWorld());

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔌 EXTEND Phase 5/6: Wiring complete - %d connections established"), WiredCount);

    // Create chain actors for wired belts (prevents crash in Factory_UpdateRadioactivity)
    // Pass ExtendService->JsonBuiltActors to include lane segments that were wired in ConfigureComponents
    int32 ChainsCreated = WiringManifest.CreateChainActors(GetWorld(), ExtendService->JsonBuiltActors);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Chain actors created - %d chains"), ChainsCreated);

    // Rebuild pipe networks to ensure fluid flow between source and clone manifolds
    // Pass ExtendService->JsonBuiltActors to include lane segment pipes that were wired in ConfigureComponents
    int32 NetworksRebuilt = WiringManifest.RebuildPipeNetworks(GetWorld(), ExtendService->JsonBuiltActors);

    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND Phase 5/6: Pipe networks rebuilt - %d networks"), NetworksRebuilt);

    // ==================== Lift ↔ Passthrough Linking (Issue #260) ====================
    // Built lifts need mSnappedPassthroughs set so they render as half-height
    // when connected to floor holes. Uses world search (TActorIterator) to find
    // passthroughs near each lift — no dependency on ExtendService->JsonBuiltActors registration.
    {
        // Step 1: Collect ALL built lifts from ExtendService->JsonBuiltActors
        TArray<AFGBuildableConveyorLift*> BuiltLifts;
        for (const auto& Pair : ExtendService->JsonBuiltActors)
        {
            if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(Pair.Value))
            {
                BuiltLifts.AddUnique(Lift);
            }
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Found %d built lifts in JsonBuiltActors (%d total entries)"),
            BuiltLifts.Num(), ExtendService->JsonBuiltActors.Num());

        if (BuiltLifts.Num() > 0)
        {
            // Step 2: Collect ALL passthroughs in the world near the build area (via TActorIterator)
            TArray<AFGBuildablePassthrough*> NearbyPassthroughs;
            FVector BuildCenter = NewFactory ? NewFactory->GetActorLocation() : FVector::ZeroVector;

            for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
            {
                AFGBuildablePassthrough* PT = *It;
                if (IsValid(PT) && FVector::Dist(PT->GetActorLocation(), BuildCenter) < 10000.0f) // 100m radius
                {
                    NearbyPassthroughs.Add(PT);
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Found %d passthroughs within 100m of factory at %s"),
                NearbyPassthroughs.Num(), *BuildCenter.ToString());

            // Step 3: Get reflection property
            FProperty* SnappedProp = AFGBuildableConveyorLift::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: mSnappedPassthroughs property %s"),
                SnappedProp ? TEXT("FOUND") : TEXT("NOT FOUND"));

            // Step 4: For each lift, find closest passthrough at bottom or top position
            int32 LinkedCount = 0;
            const float SnapDistance = 100.0f; // 1m tolerance

            for (AFGBuildableConveyorLift* Lift : BuiltLifts)
            {
                if (!IsValid(Lift) || !SnappedProp) continue;

                FVector LiftLoc = Lift->GetActorLocation();
                FTransform TopXform = Lift->GetTopTransform();
                FVector LiftTop = Lift->GetActorTransform().TransformPosition(TopXform.GetTranslation());

                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Lift %s bottom=(%s) top=(%s)"),
                    *Lift->GetName(), *LiftLoc.ToString(), *LiftTop.ToString());

                AFGBuildablePassthrough* BottomPT = nullptr;
                AFGBuildablePassthrough* TopPT = nullptr;
                float BestBottomDist = SnapDistance;
                float BestTopDist = SnapDistance;

                for (AFGBuildablePassthrough* PT : NearbyPassthroughs)
                {
                    FVector PTLoc = PT->GetActorLocation();

                    float DistBottom = FVector::Dist(PTLoc, LiftLoc);
                    float DistTop = FVector::Dist(PTLoc, LiftTop);

                    if (DistBottom < BestBottomDist)
                    {
                        BestBottomDist = DistBottom;
                        BottomPT = PT;
                    }
                    if (DistTop < BestTopDist)
                    {
                        BestTopDist = DistTop;
                        TopPT = PT;
                    }
                }

                if (BottomPT || TopPT)
                {
                    // Use reflection to access private mSnappedPassthroughs
                    TArray<AFGBuildablePassthrough*>* PassthroughArray =
                        SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(Lift);

                    if (PassthroughArray)
                    {
                        if (PassthroughArray->Num() < 2) PassthroughArray->SetNum(2);

                        if (BottomPT)
                        {
                            (*PassthroughArray)[0] = BottomPT;
                            BottomPT->SetTopSnappedConnection(Lift->GetConnection0());
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → bottom=%s (dist=%.1f)"),
                                *BottomPT->GetName(), BestBottomDist);
                        }
                        if (TopPT)
                        {
                            (*PassthroughArray)[1] = TopPT;
                            TopPT->SetBottomSnappedConnection(Lift->GetConnection1());
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → top=%s (dist=%.1f)"),
                                *TopPT->GetName(), BestTopDist);
                        }

                        // Trigger mesh rebuild via OnRep
                        UFunction* OnRepFunc = Lift->FindFunction(TEXT("OnRep_SnappedPassthroughs"));
                        if (OnRepFunc)
                        {
                            Lift->ProcessEvent(OnRepFunc, nullptr);
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → OnRep fired ✅"));
                        }
                        else
                        {
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → OnRep_SnappedPassthroughs NOT FOUND as UFunction"));
                        }

                        LinkedCount++;
                    }
                    else
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → ContainerPtrToValuePtr returned null!"));
                    }
                }
                else
                {
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗   → no passthrough within %.0fcm"), SnapDistance);
                }
            }

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🔗 PASSTHROUGH LINK: Linked %d/%d lifts to passthroughs"),
                LinkedCount, BuiltLifts.Num());
        }
    }

    // ==================== Pipe ↔ Passthrough Linking ====================
    // Built pipes through floor holes need SetTopSnappedConnection/SetBottomSnappedConnection
    // called on the passthrough with the pipe's connection. Same pattern as lift linking above.
    // AFGBuildablePassthrough does NOT own UFGPipeConnectionComponent components — it stores
    // references to connections from other actors that snap to it.
    {
        TArray<AFGBuildablePipeline*> BuiltPipes;
        for (const auto& Pair : ExtendService->JsonBuiltActors)
        {
            if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(Pair.Value))
            {
                BuiltPipes.AddUnique(Pipe);
            }
        }

        if (BuiltPipes.Num() > 0)
        {
            // Collect nearby pipe passthroughs
            TArray<AFGBuildablePassthrough*> NearbyPipePassthroughs;
            FVector PipeBuildCenter = NewFactory ? NewFactory->GetActorLocation() : FVector::ZeroVector;

            for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
            {
                AFGBuildablePassthrough* PT = *It;
                if (!IsValid(PT)) continue;

                // Only pipe floor holes (class name contains "Pipe")
                FString PTClassName = PT->GetClass()->GetFName().ToString();
                if (!PTClassName.Contains(TEXT("Pipe"))) continue;

                if (FVector::Dist(PT->GetActorLocation(), PipeBuildCenter) < 10000.0f)
                {
                    NearbyPipePassthroughs.Add(PT);
                }
            }

            int32 PipeLinkedCount = 0;
            const float PipeSnapDist = 100.0f; // 1m XY tolerance

            for (AFGBuildablePipeline* Pipe : BuiltPipes)
            {
                if (!IsValid(Pipe)) continue;

                // Check both endpoints of the pipe
                UFGPipeConnectionComponentBase* PipeConn0 = Pipe->GetPipeConnection0();
                UFGPipeConnectionComponentBase* PipeConn1 = Pipe->GetPipeConnection1();

                for (AFGBuildablePassthrough* PT : NearbyPipePassthroughs)
                {
                    FVector PTLoc = PT->GetActorLocation();
                    float PTZ = PTLoc.Z;

                    // Check Conn0 against ExtendService passthrough
                    if (PipeConn0)
                    {
                        FVector Conn0Loc = PipeConn0->GetComponentLocation();
                        float DistXY = FVector::Dist2D(Conn0Loc, PTLoc);
                        if (DistXY < PipeSnapDist)
                        {
                            bool bIsTop = (Conn0Loc.Z >= PTZ);
                            UFGConnectionComponent* ConnBase = Cast<UFGConnectionComponent>(PipeConn0);
                            if (ConnBase)
                            {
                                if (bIsTop)
                                    PT->SetTopSnappedConnection(ConnBase);
                                else
                                    PT->SetBottomSnappedConnection(ConnBase);
                                PipeLinkedCount++;
                                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔗 PIPE PASSTHROUGH LINK: %s.Conn0 → %s Set%sSnappedConnection"),
                                    *Pipe->GetName(), *PT->GetName(), bIsTop ? TEXT("Top") : TEXT("Bottom"));
                            }
                        }
                    }

                    // Check Conn1 against ExtendService passthrough
                    if (PipeConn1)
                    {
                        FVector Conn1Loc = PipeConn1->GetComponentLocation();
                        float DistXY = FVector::Dist2D(Conn1Loc, PTLoc);
                        if (DistXY < PipeSnapDist)
                        {
                            bool bIsTop = (Conn1Loc.Z >= PTZ);
                            UFGConnectionComponent* ConnBase = Cast<UFGConnectionComponent>(PipeConn1);
                            if (ConnBase)
                            {
                                if (bIsTop)
                                    PT->SetTopSnappedConnection(ConnBase);
                                else
                                    PT->SetBottomSnappedConnection(ConnBase);
                                PipeLinkedCount++;
                                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔗 PIPE PASSTHROUGH LINK: %s.Conn1 → %s Set%sSnappedConnection"),
                                    *Pipe->GetName(), *PT->GetName(), bIsTop ? TEXT("Top") : TEXT("Bottom"));
                            }
                        }
                    }
                }
            }

            if (PipeLinkedCount > 0)
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("🔗 PIPE PASSTHROUGH LINK: Linked %d pipe connections to %d pipe floor holes"),
                    PipeLinkedCount, NearbyPipePassthroughs.Num());
            }
        }
    }

    // VERIFICATION: Log chain actor status for all built conveyors
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⛓️ VERIFY CHAINS: Checking %d built actors for chain actor status"), ExtendService->JsonBuiltActors.Num());
    int32 ValidChains = 0;
    int32 NullChains = 0;
    for (const auto& Pair : ExtendService->JsonBuiltActors)
    {
        if (AFGBuildableConveyorBase* Conveyor = Cast<AFGBuildableConveyorBase>(Pair.Value))
        {
            AFGConveyorChainActor* ChainActor = Conveyor->GetConveyorChainActor();
            if (ChainActor)
            {
                ValidChains++;
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⛓️ VERIFY: ✅ %s -> Chain=%s (segments=%d)"),
                    *Conveyor->GetName(), *ChainActor->GetName(), ChainActor->GetNumChainSegments());
            }
            else
            {
                NullChains++;
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Error, TEXT("⛓️ VERIFY: ❌ %s -> ChainActor=NULL (CRASH RISK!)"),
                    *Conveyor->GetName());
            }
        }
    }
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⛓️ VERIFY CHAINS: %d valid, %d NULL (crash risk if NULL > 0)"), ValidChains, NullChains);

    // ==================== Power Pole Wiring (Issue #229) ====================
    // Wire built power poles: clone factory ↔ clone pole, and source pole ↔ clone pole
    int32 PowerWiredCount = 0;
    UClass* WireClass = LoadClass<AFGBuildableWire>(nullptr, SFAssetPaths::PowerLineBuildClass);

    for (const auto& WiringPair : ExtendService->PowerPoleWiringData)
    {
        const FString& CloneId = WiringPair.Key;
        const USFExtendService::FSFSourcePoleWiringData& SourceData = WiringPair.Value;

        // Find the built clone pole
        AActor* const* BuiltPoleActor = CloneIdToBuildable.Find(CloneId);
        if (!BuiltPoleActor || !IsValid(*BuiltPoleActor))
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND Power Wire: Clone pole %s not found in built actors"), *CloneId);
            continue;
        }

        AFGBuildablePowerPole* ClonePole = Cast<AFGBuildablePowerPole>(*BuiltPoleActor);
        if (!ClonePole)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND Power Wire: Built actor %s is not a power pole"), *CloneId);
            continue;
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ EXTEND Power Wire: Processing %s -> %s"), *CloneId, *ClonePole->GetName());

        // --- Wire 1: Clone Factory ↔ Clone Pole ---
        if (WireClass && IsValid(NewFactory))
        {
            // Get circuit connections — factory power connections are UFGCircuitConnectionComponent subclass
            // (belt/pipe connectors use separate class hierarchies, so only power conns returned)
            TArray<UFGCircuitConnectionComponent*> FactoryCircuitConns;
            NewFactory->GetComponents<UFGCircuitConnectionComponent>(FactoryCircuitConns);

            TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
            ClonePole->GetComponents<UFGCircuitConnectionComponent>(PoleCircuitConns);

            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ EXTEND Power Wire: Factory %s has %d circuit conns, Pole %s has %d circuit conns"),
                *NewFactory->GetName(), FactoryCircuitConns.Num(), *ClonePole->GetName(), PoleCircuitConns.Num());

            if (FactoryCircuitConns.Num() > 0 && PoleCircuitConns.Num() > 0)
            {
                UFGCircuitConnectionComponent* FactoryConn = FactoryCircuitConns[0];
                UFGCircuitConnectionComponent* PoleConn = PoleCircuitConns[0];

                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                    WireClass, ClonePole->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

                if (NewWire)
                {
                    bool bConnected = NewWire->Connect(FactoryConn, PoleConn);
                    if (bConnected)
                    {
                        PowerWiredCount++;
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ EXTEND Power Wire: Connected clone factory %s ↔ clone pole %s"),
                            *NewFactory->GetName(), *ClonePole->GetName());
                    }
                    else
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND Power Wire: Wire Connect() failed for factory ↔ pole - destroying wire"));
                        NewWire->Destroy();
                    }
                }
            }
            else
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND Power Wire: Missing connections - factory %s has %d circuit conns, pole has %d circuit conns"),
                    *NewFactory->GetClass()->GetName(), FactoryCircuitConns.Num(), PoleCircuitConns.Num());
            }
        }

        // --- Wire 2: Source Pole ↔ Clone Pole (only if source has free connections) ---
        if (WireClass && SourceData.SourcePole.IsValid() && SourceData.bSourceHasFreeConnections)
        {
            AFGBuildablePowerPole* SourcePole = SourceData.SourcePole.Get();

            // Get circuit connections on both poles
            TArray<UFGCircuitConnectionComponent*> SourceCircuitConns;
            SourcePole->GetComponents<UFGCircuitConnectionComponent>(SourceCircuitConns);

            TArray<UFGCircuitConnectionComponent*> CloneCircuitConns;
            ClonePole->GetComponents<UFGCircuitConnectionComponent>(CloneCircuitConns);

            if (SourceCircuitConns.Num() > 0 && CloneCircuitConns.Num() > 0)
            {
                UFGCircuitConnectionComponent* SourceConn = SourceCircuitConns[0];
                UFGCircuitConnectionComponent* CloneConn = CloneCircuitConns[0];

                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                    WireClass, SourcePole->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

                if (NewWire)
                {
                    bool bConnected = NewWire->Connect(SourceConn, CloneConn);
                    if (bConnected)
                    {
                        PowerWiredCount++;
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ EXTEND Power Wire: Connected source pole %s ↔ clone pole %s"),
                            *SourcePole->GetName(), *ClonePole->GetName());
                    }
                    else
                    {
                        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ EXTEND Power Wire: Wire Connect() failed for source ↔ clone pole - destroying wire"));
                        NewWire->Destroy();
                    }
                }
            }
        }
        else if (SourceData.SourcePole.IsValid() && !SourceData.bSourceHasFreeConnections)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ EXTEND Power Wire: Source pole %s has no free connections - skipping source↔clone wire (subsequent extends will chain)"),
                *SourceData.SourcePole->GetName());
        }
    }

    if (PowerWiredCount > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ EXTEND Power Wire: Complete - %d power connections established"), PowerWiredCount);
    }
    WiredCount += PowerWiredCount;

    // Restored Extend topology does not have live ExtendService->PowerPoleWiringData or ExtendService->ScaledExtendClones,
    // so wire its JSON-restored power poles directly from the expanded topology ids.
    if (ExtendService->bRestoredCloneTopologyActive && WireClass && ExtendService->PowerPoleWiringData.Num() == 0 && ExtendService->StoredCloneTopology.IsValid())
    {
        struct FRestoredPowerPoleEntry
        {
            FString CloneId;
            FString PoleKey;
            FString Prefix;
            FString SourcePoleId;
            int32 SortOrder = 0;
            AFGBuildablePowerPole* Pole = nullptr;
        };

        auto GetFirstCircuitConnection = [](AActor* Actor) -> UFGCircuitConnectionComponent*
        {
            if (!IsValid(Actor))
            {
                return nullptr;
            }

            TArray<UFGCircuitConnectionComponent*> CircuitConnections;
            Actor->GetComponents<UFGCircuitConnectionComponent>(CircuitConnections);
            return CircuitConnections.Num() > 0 ? CircuitConnections[0] : nullptr;
        };

        auto AreCircuitConnectionsLinked = [](UFGCircuitConnectionComponent* A, UFGCircuitConnectionComponent* B) -> bool
        {
            if (!A || !B)
            {
                return false;
            }

            TArray<UFGCircuitConnectionComponent*> ExistingConnections;
            A->GetConnections(ExistingConnections);
            return ExistingConnections.Contains(B);
        };

        auto ConnectPowerEndpoints = [&](UFGCircuitConnectionComponent* A, UFGCircuitConnectionComponent* B, const TCHAR* Context) -> bool
        {
            if (!A || !B || AreCircuitConnectionsLinked(A, B))
            {
                return false;
            }

            if (A->GetNumConnections() >= A->GetMaxNumConnections()
                || B->GetNumConnections() >= B->GetMaxNumConnections())
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                    TEXT("[SmartRestore][Extend] Power wiring skipped for %s: capacity A=%d/%d B=%d/%d"),
                    Context ? Context : TEXT("Unknown"),
                    A->GetNumConnections(), A->GetMaxNumConnections(),
                    B->GetNumConnections(), B->GetMaxNumConnections());
                return false;
            }

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            const FVector SpawnLocation = A->GetOwner() ? A->GetOwner()->GetActorLocation() : FVector::ZeroVector;
            AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                WireClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
            if (!NewWire)
            {
                return false;
            }

            if (NewWire->Connect(A, B))
            {
                return true;
            }

            NewWire->Destroy();
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning,
                TEXT("[SmartRestore][Extend] Power wiring failed for %s"),
                Context ? Context : TEXT("Unknown"));
            return false;
        };

        auto TryParseRestoredPrefixOrder = [](const FString& Prefix, int32& OutSortOrder) -> bool
        {
            if (Prefix.IsEmpty())
            {
                OutSortOrder = 0;
                return true;
            }

            if (!Prefix.StartsWith(TEXT("rr_")) || !Prefix.EndsWith(TEXT("_")))
            {
                return false;
            }

            FString GridText = Prefix.Mid(3, Prefix.Len() - 4);
            TArray<FString> Parts;
            GridText.ParseIntoArray(Parts, TEXT("_"), true);
            if (Parts.Num() != 2)
            {
                return false;
            }

            const int32 GridX = FCString::Atoi(*Parts[0]);
            const int32 GridY = FCString::Atoi(*Parts[1]);
            OutSortOrder = 1 + (GridY * 10000) + GridX;
            return true;
        };

        TMap<FString, TArray<FRestoredPowerPoleEntry>> RestoredPolesByKey;
        for (const FSFCloneHologram& Holo : ExtendService->StoredCloneTopology->ChildHolograms)
        {
            if (Holo.Role != TEXT("power_pole"))
            {
                continue;
            }

            const int32 PowerPoleMarkerIndex = Holo.HologramId.Find(TEXT("power_pole_"), ESearchCase::CaseSensitive);
            if (PowerPoleMarkerIndex == INDEX_NONE)
            {
                continue;
            }

            const FString Prefix = Holo.HologramId.Left(PowerPoleMarkerIndex);
            int32 SortOrder = 0;
            if (!TryParseRestoredPrefixOrder(Prefix, SortOrder))
            {
                continue;
            }

            AActor* const* BuiltPoleActor = CloneIdToBuildable.Find(Holo.HologramId);
            AFGBuildablePowerPole* BuiltPole = BuiltPoleActor ? Cast<AFGBuildablePowerPole>(*BuiltPoleActor) : nullptr;
            if (!BuiltPole)
            {
                continue;
            }

            FRestoredPowerPoleEntry Entry;
            Entry.CloneId = Holo.HologramId;
            Entry.PoleKey = Holo.HologramId.Mid(PowerPoleMarkerIndex);
            Entry.Prefix = Prefix;
            Entry.SourcePoleId = Holo.SourceId;
            Entry.SortOrder = SortOrder;
            Entry.Pole = BuiltPole;
            RestoredPolesByKey.FindOrAdd(Entry.PoleKey).Add(Entry);
        }

        int32 RestoredPowerWiredCount = 0;
        for (TPair<FString, TArray<FRestoredPowerPoleEntry>>& PoleGroup : RestoredPolesByKey)
        {
            PoleGroup.Value.Sort([](const FRestoredPowerPoleEntry& A, const FRestoredPowerPoleEntry& B)
            {
                return A.SortOrder < B.SortOrder;
            });

            for (const FRestoredPowerPoleEntry& Entry : PoleGroup.Value)
            {
                const FString FactoryId = Entry.Prefix.IsEmpty() ? TEXT("parent") : Entry.Prefix + TEXT("factory");
                AActor* FactoryActor = CloneIdToBuildable.FindRef(FactoryId);
                AFGBuildableFactory* Factory = Cast<AFGBuildableFactory>(FactoryActor);
                UFGCircuitConnectionComponent* FactoryConn = GetFirstCircuitConnection(Factory);
                UFGCircuitConnectionComponent* PoleConn = GetFirstCircuitConnection(Entry.Pole);
                if (ConnectPowerEndpoints(FactoryConn, PoleConn, TEXT("restored factory to cloned pole")))
                {
                    RestoredPowerWiredCount++;
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                        TEXT("[SmartRestore][Extend] Connected restored factory '%s' to power pole '%s'"),
                        *GetNameSafe(Factory),
                        *GetNameSafe(Entry.Pole));
                }
            }

            for (int32 Index = 0; Index < PoleGroup.Value.Num() - 1; ++Index)
            {
                UFGCircuitConnectionComponent* ConnA = GetFirstCircuitConnection(PoleGroup.Value[Index].Pole);
                UFGCircuitConnectionComponent* ConnB = GetFirstCircuitConnection(PoleGroup.Value[Index + 1].Pole);
                if (ConnectPowerEndpoints(ConnA, ConnB, TEXT("restored cloned pole chain")))
                {
                    RestoredPowerWiredCount++;
                    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log,
                        TEXT("[SmartRestore][Extend] Chained restored power poles '%s' to '%s'"),
                        *GetNameSafe(PoleGroup.Value[Index].Pole),
                        *GetNameSafe(PoleGroup.Value[Index + 1].Pole));
                }
            }
        }

        if (RestoredPowerWiredCount > 0)
        {
            WiredCount += RestoredPowerWiredCount;
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display,
                TEXT("[SmartRestore][Extend] Restored power wiring complete: %d connections across %d pole group(s)"),
                RestoredPowerWiredCount,
                RestoredPolesByKey.Num());
        }
    }

    // ==================== Scaled Extend: Additional Clone Wiring (Issue #265) ====================
    // Process wiring for each additional clone set beyond clone 1
    int32 ScaledExtendWiredCount = 0;
    for (int32 CloneIdx = 0; CloneIdx < ExtendService->ScaledExtendClones.Num(); CloneIdx++)
    {
        FSFScaledExtendClone& Clone = ExtendService->ScaledExtendClones[CloneIdx];
        if (!Clone.CloneTopology.IsValid() || Clone.CloneTopology->ChildHolograms.Num() == 0)
        {
            continue;
        }

        // Find ExtendService clone's factory building from built actors
        // The factory hologram was registered as "factory" in the clone's SpawnedHolograms
        // But we need to find the BUILT factory, not the hologram
        AFGBuildableFactory* CloneFactory = nullptr;

        // Search ExtendService->JsonBuiltActors for any factory building near the clone's expected position
        // Clone.WorldOffset is relative to SOURCE building, not clone 1
        FVector SourcePos = ExtendService->CurrentExtendTarget.IsValid() ? ExtendService->CurrentExtendTarget->GetActorLocation() : (NewFactory->GetActorLocation() - ExtendService->ScaledExtendClones[0].WorldOffset);
        FVector ExpectedPos = SourcePos + Clone.WorldOffset;
        float BestDist = MAX_FLT;
        for (const auto& BuiltPair : ExtendService->JsonBuiltActors)
        {
            if (AFGBuildableFactory* BuiltFactory = Cast<AFGBuildableFactory>(BuiltPair.Value))
            {
                float Dist = FVector::Dist(BuiltFactory->GetActorLocation(), ExpectedPos);
                if (Dist < 500.0f && Dist < BestDist)  // Within 5m of expected position
                {
                    BestDist = Dist;
                    CloneFactory = BuiltFactory;
                }
            }
        }

        if (!CloneFactory)
        {
            // Try finding from OnActorSpawned-registered factories
            for (TActorIterator<AFGBuildableFactory> It(GetWorld()); It; ++It)
            {
                float Dist = FVector::Dist(It->GetActorLocation(), ExpectedPos);
                if (Dist < 500.0f && Dist < BestDist)
                {
                    BestDist = Dist;
                    CloneFactory = *It;
                }
            }
        }

        if (!CloneFactory)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND Wire: Could not find built factory for Clone[%d] at expected position"), CloneIdx);
            continue;
        }

        // Build clone_id -> buildable mapping for ExtendService clone set
        TMap<FString, AActor*> CloneBuiltActors;
        CloneBuiltActors.Add(TEXT("parent"), CloneFactory);

        // Find all built actors with ExtendService clone's prefix
        FString ClonePrefix = FString::Printf(TEXT("sc%d_"), CloneIdx);
        for (const auto& Pair : ExtendService->JsonBuiltActors)
        {
            if (Pair.Key.StartsWith(ClonePrefix) && IsValid(Pair.Value))
            {
                // Strip prefix for topology matching
                FString OriginalId = Pair.Key.Mid(ClonePrefix.Len());
                CloneBuiltActors.Add(OriginalId, Pair.Value);
            }
        }

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND Wire: Clone[%d] - %d built actors mapped (factory=%s)"),
            CloneIdx, CloneBuiltActors.Num(), *CloneFactory->GetName());

        // Generate and execute wiring for ExtendService clone
        // Use the clone's topology but with stripped IDs (matching original topology structure)
        FSFCloneTopology StrippedTopology = *Clone.CloneTopology;
        for (FSFCloneHologram& Holo : StrippedTopology.ChildHolograms)
        {
            if (Holo.HologramId.StartsWith(ClonePrefix))
            {
                Holo.HologramId = Holo.HologramId.Mid(ClonePrefix.Len());
            }
            if (Holo.ConnectedPowerPoleHologramId.StartsWith(ClonePrefix))
            {
                Holo.ConnectedPowerPoleHologramId = Holo.ConnectedPowerPoleHologramId.Mid(ClonePrefix.Len());
            }
        }

        FSFWiringManifest CloneManifest = FSFWiringManifest::Generate(
            StrippedTopology, CloneBuiltActors, CloneFactory);

        int32 CloneWired = CloneManifest.ExecuteWiring(GetWorld());
        int32 CloneChains = CloneManifest.CreateChainActors(GetWorld(), CloneBuiltActors);
        int32 ClonePipes = CloneManifest.RebuildPipeNetworks(GetWorld(), CloneBuiltActors);

        // Lift ↔ Passthrough linking for scaled clone (Issue #260) — world search approach
        {
            TArray<AFGBuildableConveyorLift*> CloneLifts;
            for (const auto& BuiltPair : CloneBuiltActors)
            {
                if (AFGBuildableConveyorLift* Lift = Cast<AFGBuildableConveyorLift>(BuiltPair.Value))
                    CloneLifts.AddUnique(Lift);
            }
            if (CloneLifts.Num() > 0)
            {
                FProperty* SnappedProp = AFGBuildableConveyorLift::StaticClass()->FindPropertyByName(TEXT("mSnappedPassthroughs"));
                TArray<AFGBuildablePassthrough*> NearbyPTs;
                FVector Center = CloneFactory->GetActorLocation();
                for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
                {
                    if (IsValid(*It) && FVector::Dist(It->GetActorLocation(), Center) < 10000.0f)
                        NearbyPTs.Add(*It);
                }
                for (AFGBuildableConveyorLift* Lift : CloneLifts)
                {
                    if (!IsValid(Lift) || !SnappedProp) continue;
                    FVector LiftLoc = Lift->GetActorLocation();
                    FVector LiftTop = Lift->GetActorTransform().TransformPosition(Lift->GetTopTransform().GetTranslation());
                    AFGBuildablePassthrough* BottomPT = nullptr;
                    AFGBuildablePassthrough* TopPT = nullptr;
                    float BestBD = 100.0f, BestTD = 100.0f;
                    for (AFGBuildablePassthrough* PT : NearbyPTs)
                    {
                        float DB = FVector::Dist(PT->GetActorLocation(), LiftLoc);
                        float DT = FVector::Dist(PT->GetActorLocation(), LiftTop);
                        if (DB < BestBD) { BestBD = DB; BottomPT = PT; }
                        if (DT < BestTD) { BestTD = DT; TopPT = PT; }
                    }
                    if (BottomPT || TopPT)
                    {
                        TArray<AFGBuildablePassthrough*>* Arr = SnappedProp->ContainerPtrToValuePtr<TArray<AFGBuildablePassthrough*>>(Lift);
                        if (Arr)
                        {
                            if (Arr->Num() < 2) Arr->SetNum(2);
                            if (BottomPT) { (*Arr)[0] = BottomPT; BottomPT->SetTopSnappedConnection(Lift->GetConnection0()); }
                            if (TopPT) { (*Arr)[1] = TopPT; TopPT->SetBottomSnappedConnection(Lift->GetConnection1()); }
                            if (UFunction* Fn = Lift->FindFunction(TEXT("OnRep_SnappedPassthroughs"))) Lift->ProcessEvent(Fn, nullptr);
                        }
                    }
                }
            }
        }

        // Pipe ↔ Passthrough linking for scaled clone
        {
            TArray<AFGBuildablePipeline*> ClonePipeList;
            for (const auto& BuiltPair : CloneBuiltActors)
            {
                if (AFGBuildablePipeline* Pipe = Cast<AFGBuildablePipeline>(BuiltPair.Value))
                    ClonePipeList.AddUnique(Pipe);
            }
            if (ClonePipeList.Num() > 0)
            {
                TArray<AFGBuildablePassthrough*> NearbyPipePTs;
                FVector CenterLoc = CloneFactory->GetActorLocation();
                for (TActorIterator<AFGBuildablePassthrough> It(GetWorld()); It; ++It)
                {
                    AFGBuildablePassthrough* PT = *It;
                    if (!IsValid(PT)) continue;
                    FString PTClass = PT->GetClass()->GetFName().ToString();
                    if (!PTClass.Contains(TEXT("Pipe"))) continue;
                    if (FVector::Dist(PT->GetActorLocation(), CenterLoc) < 10000.0f)
                        NearbyPipePTs.Add(PT);
                }
                for (AFGBuildablePipeline* Pipe : ClonePipeList)
                {
                    if (!IsValid(Pipe)) continue;
                    UFGPipeConnectionComponentBase* PC0 = Pipe->GetPipeConnection0();
                    UFGPipeConnectionComponentBase* PC1 = Pipe->GetPipeConnection1();
                    for (AFGBuildablePassthrough* PT : NearbyPipePTs)
                    {
                        FVector PTLoc = PT->GetActorLocation();
                        float PTZ = PTLoc.Z;
                        if (PC0)
                        {
                            FVector C0Loc = PC0->GetComponentLocation();
                            if (FVector::Dist2D(C0Loc, PTLoc) < 100.0f)
                            {
                                UFGConnectionComponent* CB = Cast<UFGConnectionComponent>(PC0);
                                if (CB)
                                {
                                    if (C0Loc.Z >= PTZ) PT->SetTopSnappedConnection(CB);
                                    else PT->SetBottomSnappedConnection(CB);
                                }
                            }
                        }
                        if (PC1)
                        {
                            FVector C1Loc = PC1->GetComponentLocation();
                            if (FVector::Dist2D(C1Loc, PTLoc) < 100.0f)
                            {
                                UFGConnectionComponent* CB = Cast<UFGConnectionComponent>(PC1);
                                if (CB)
                                {
                                    if (C1Loc.Z >= PTZ) PT->SetTopSnappedConnection(CB);
                                    else PT->SetBottomSnappedConnection(CB);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Power pole wiring for ExtendService clone — connect built power poles to clone factory
        int32 ClonePowerWired = 0;
        if (WireClass)
        {
            for (const auto& BuiltPair : CloneBuiltActors)
            {
                if (!BuiltPair.Key.Contains(TEXT("power_pole"))) continue;

                AFGBuildablePowerPole* ClonePole = Cast<AFGBuildablePowerPole>(BuiltPair.Value);
                if (!ClonePole) continue;

                // Get circuit connections on both pole and factory
                // Factory power connections are UFGCircuitConnectionComponent subclass
                TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
                ClonePole->GetComponents<UFGCircuitConnectionComponent>(PoleCircuitConns);

                TArray<UFGCircuitConnectionComponent*> FactoryCircuitConns;
                CloneFactory->GetComponents<UFGCircuitConnectionComponent>(FactoryCircuitConns);

                if (PoleCircuitConns.Num() > 0 && FactoryCircuitConns.Num() > 0)
                {
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                        WireClass, ClonePole->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

                    if (NewWire)
                    {
                        bool bConnected = NewWire->Connect(FactoryCircuitConns[0], PoleCircuitConns[0]);
                        if (bConnected)
                        {
                            ClonePowerWired++;
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND Power: Connected %s ↔ %s"),
                                *CloneFactory->GetName(), *ClonePole->GetName());
                        }
                        else
                        {
                            NewWire->Destroy();
                        }
                    }
                }
            }
        }

        for (const FSFCloneHologram& Holo : StrippedTopology.ChildHolograms)
        {
            if (Holo.Role != TEXT("pipe_attachment")) continue;
            if (Holo.ConnectedPowerPoleHologramId.IsEmpty()) continue;
            if (!WireClass) continue;

            AActor* const* PumpActorPtr = CloneBuiltActors.Find(Holo.HologramId);
            AActor* const* PoleActorPtr = CloneBuiltActors.Find(Holo.ConnectedPowerPoleHologramId);
            if (!PumpActorPtr || !*PumpActorPtr || !PoleActorPtr || !*PoleActorPtr)
            {
                continue;
            }

            AFGBuildablePipelinePump* ClonePump = Cast<AFGBuildablePipelinePump>(*PumpActorPtr);
            AFGBuildablePowerPole* ClonePole = Cast<AFGBuildablePowerPole>(*PoleActorPtr);
            if (!ClonePump || !ClonePole) continue;

            UFGPowerConnectionComponent* PumpPowerConn = ClonePump->FindComponentByClass<UFGPowerConnectionComponent>();
            if (!PumpPowerConn) continue;

            TArray<UFGCircuitConnectionComponent*> PoleCircuitConns;
            ClonePole->GetComponents<UFGCircuitConnectionComponent>(PoleCircuitConns);
            if (PoleCircuitConns.Num() == 0) continue;

            UFGCircuitConnectionComponent* PoleConn = PoleCircuitConns[0];
            TArray<UFGCircuitConnectionComponent*> CurrentPumpPartners;
            PumpPowerConn->GetConnections(CurrentPumpPartners);
            if (CurrentPumpPartners.Contains(PoleConn))
            {
                continue;
            }

            if (PumpPowerConn->IsConnected())
            {
                TArray<AFGBuildableWire*> ExistingPumpWires;
                PumpPowerConn->GetWires(ExistingPumpWires);
                for (AFGBuildableWire* ExistingWire : ExistingPumpWires)
                {
                    if (!ExistingWire) continue;
                    UFGCircuitConnectionComponent* OtherConn = (ExistingWire->GetConnection(0) == PumpPowerConn)
                        ? ExistingWire->GetConnection(1)
                        : ExistingWire->GetConnection(0);
                    if (OtherConn != PoleConn)
                    {
                        UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⚡ SCALED EXTEND Pump Power: Clone[%d] replacing incorrect pump wire %s (%s → %s, expected %s)"),
                            CloneIdx, *ExistingWire->GetName(), *ClonePump->GetName(),
                            OtherConn && OtherConn->GetOwner() ? *OtherConn->GetOwner()->GetName() : TEXT("<unknown>"),
                            *ClonePole->GetName());
                        ExistingWire->Destroy();
                    }
                }
            }

            if (PoleConn->GetNumConnections() >= PoleConn->GetMaxNumConnections())
            {
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND Pump Power: Clone[%d] pole %s reached capacity (%d/%d) — skipping pump %s"),
                    CloneIdx, *ClonePole->GetName(), PoleConn->GetNumConnections(), PoleConn->GetMaxNumConnections(), *ClonePump->GetName());
                continue;
            }

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            AFGBuildableWire* NewWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                WireClass, ClonePump->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);
            if (!NewWire)
            {
                continue;
            }

            if (NewWire->Connect(PumpPowerConn, PoleConn))
            {
                ClonePowerWired++;
                UE_LOG(LogSmartExtend, VeryVerbose, TEXT("⚡ SCALED EXTEND Pump Power: Clone[%d] connected pump %s ↔ pole %s"),
                    CloneIdx, *ClonePump->GetName(), *ClonePole->GetName());
            }
            else
            {
                NewWire->Destroy();
                SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("⚡ SCALED EXTEND Pump Power: Clone[%d] Wire Connect() failed for pump %s ↔ pole %s"),
                    CloneIdx, *ClonePump->GetName(), *ClonePole->GetName());
            }
        }

        ScaledExtendWiredCount += CloneWired + ClonePowerWired;

        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ SCALED EXTEND Wire: Clone[%d] - %d belt/pipe, %d power, %d chains, %d pipe networks%s"),
            CloneIdx, CloneWired, ClonePowerWired, CloneChains, ClonePipes, Clone.bIsSeed ? TEXT(" [SEED]") : TEXT(""));
    }

    if (ScaledExtendWiredCount > 0)
    {
        SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("⚡ SCALED EXTEND Wire: Total %d additional connections across %d clones"),
            ScaledExtendWiredCount, ExtendService->ScaledExtendClones.Num());
        WiredCount += ScaledExtendWiredCount;
    }

    // ==================== Power Pole Chaining (Issue #229/#265) ====================
    // Chain power poles between consecutive clones: clone1_pole → clone2_pole → clone3_pole...
    // IDs: "power_pole_N" (clone 1), "sc0_power_pole_N" (clone 2), "sc1_power_pole_N" (clone 3), etc.
    if (WireClass)
    {
        // Discover all power pole indices from clone 1's topology
        TArray<int32> PoleIndices;
        for (const auto& Pair : CloneIdToBuildable)
        {
            FString Key = Pair.Key;
            if (Key.StartsWith(TEXT("power_pole_")))
            {
                FString IndexStr = Key.Mid(11); // after "power_pole_"
                int32 Idx = FCString::Atoi(*IndexStr);
                PoleIndices.AddUnique(Idx);
            }
        }

        int32 ChainWiredCount = 0;
        for (int32 PoleIdx : PoleIndices)
        {
            // Build ordered list: clone 1 pole, then each scaled clone's pole
            TArray<AFGBuildablePowerPole*> PoleChain;

            // Clone 1's pole
            FString Clone1PoleId = FString::Printf(TEXT("power_pole_%d"), PoleIdx);
            if (AActor* const* Actor = CloneIdToBuildable.Find(Clone1PoleId))
            {
                if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(*Actor))
                    PoleChain.Add(Pole);
            }

            // Each scaled clone's pole
            for (int32 CloneIdx = 0; CloneIdx < ExtendService->ScaledExtendClones.Num(); CloneIdx++)
            {
                FString ScPoleId = FString::Printf(TEXT("sc%d_power_pole_%d"), CloneIdx, PoleIdx);
                if (AActor* const* Actor = CloneIdToBuildable.Find(ScPoleId))
                {
                    if (AFGBuildablePowerPole* Pole = Cast<AFGBuildablePowerPole>(*Actor))
                        PoleChain.Add(Pole);
                }
            }

            // Wire consecutive poles in the chain
            for (int32 j = 0; j < PoleChain.Num() - 1; j++)
            {
                AFGBuildablePowerPole* PoleA = PoleChain[j];
                AFGBuildablePowerPole* PoleB = PoleChain[j + 1];

                TArray<UFGCircuitConnectionComponent*> ConnsA, ConnsB;
                PoleA->GetComponents<UFGCircuitConnectionComponent>(ConnsA);
                PoleB->GetComponents<UFGCircuitConnectionComponent>(ConnsB);

                if (ConnsA.Num() > 0 && ConnsB.Num() > 0)
                {
                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

                    AFGBuildableWire* ChainWire = GetWorld()->SpawnActor<AFGBuildableWire>(
                        WireClass, PoleA->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

                    if (ChainWire)
                    {
                        bool bConnected = ChainWire->Connect(ConnsA[0], ConnsB[0]);
                        if (bConnected)
                        {
                            ChainWiredCount++;
                            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Log, TEXT("⚡ POWER CHAIN: Connected %s ↔ %s (pole_%d, link %d)"),
                                *PoleA->GetName(), *PoleB->GetName(), PoleIdx, j);
                        }
                        else
                        {
                            ChainWire->Destroy();
                        }
                    }
                }
            }
        }

        if (ChainWiredCount > 0)
        {
            SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Display, TEXT("⚡ POWER CHAIN: %d pole-to-pole connections across %d pole indices"),
                ChainWiredCount, PoleIndices.Num());
            WiredCount += ChainWiredCount;
        }
    }

    // Clear stored topology and built actors after wiring
    ExtendService->StoredCloneTopology.Reset();
    ExtendService->JsonSpawnedHolograms.Empty();
    ExtendService->JsonBuiltActors.Empty();
    ExtendService->PowerPoleWiringData.Empty();

    // Clear scaled extend clones (their topologies were consumed by wiring)
    ExtendService->ScaledExtendClones.Empty();

    return WiredCount;
}

void USFExtendWiringService::RegisterJsonBuiltActor(const FString& CloneId, AActor* BuiltActor)
{
    if (!BuiltActor || CloneId.IsEmpty())
    {
        return;
    }

    ExtendService->JsonBuiltActors.Add(CloneId, BuiltActor);
    UE_LOG(LogSmartExtend, VeryVerbose, TEXT("🔌 EXTEND: Registered built actor %s -> %s"),
        *CloneId, *BuiltActor->GetName());
}

AFGBuildable* USFExtendWiringService::GetBuiltActorByCloneId(const FString& CloneId) const
{
    if (CloneId.IsEmpty())
    {
        return nullptr;
    }

    // Check ExtendService->JsonBuiltActors map
    if (AActor* const* FoundActor = ExtendService->JsonBuiltActors.Find(CloneId))
    {
        return Cast<AFGBuildable>(*FoundActor);
    }

    return nullptr;
}

AFGBuildable* USFExtendWiringService::GetSourceBuildableByName(const FString& ActorName) const
{
    if (ActorName.IsEmpty())
    {
        return nullptr;
    }

    // Search world for buildable with matching name
    // This is used by lane segments to find the source distributor (existing buildable)
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<AFGBuildable> It(World); It; ++It)
    {
        AFGBuildable* Buildable = *It;
        if (Buildable && Buildable->GetName() == ActorName)
        {
            return Buildable;
        }
    }

    SF_EXTEND_DIAGNOSTIC_LOG(LogSmartExtend, Warning, TEXT("🛤️ LANE: Source buildable '%s' not found in world"), *ActorName);
    return nullptr;
}

// ==================== Scaled Extend (Issue #265) ====================

