// Copyright Coffee Stain Studios. All Rights Reserved.

/**
 * SFExtendWiringService - Post-Build Connection Wiring Interface
 * 
 * Provides a service interface for post-build connection wiring in EXTEND feature.
 * 
 * Current Implementation (Dec 2025):
 * - This service acts as a facade/interface for wiring operations
 * - Actual wiring logic remains in SFExtendService due to tight coupling with:
 *   - Built element tracking maps (populated during hologram Construct())
 *   - Topology data access
 *   - JSON-based wiring manifest
 * 
 * Future Migration:
 * - Move tracking maps to this service
 * - Move wiring implementation here
 * - SFExtendService would then delegate to this service
 * 
 * Part of EXTEND feature refactor (Dec 2025).
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SFExtendWiringService.generated.h"

class AFGBuildable;
class AFGBuildableFactory;
class AFGBuildableConveyorBase;
class AFGBuildablePipeline;
class AFGBuildableConveyorBelt;
class AFGHologram;
class UFGPipeConnectionComponentBase;
class UFGFactoryConnectionComponent;
class USFSubsystem;
class USFExtendService;

/**
 * Service interface for post-build connection wiring in EXTEND feature.
 * Currently delegates to SFExtendService; future versions will own the implementation.
 */
UCLASS()
class SMARTFOUNDATIONS_API USFExtendWiringService : public UObject
{
    GENERATED_BODY()

public:
    USFExtendWiringService();

    /** Initialize with owning subsystem and extend service references */
    void Initialize(USFSubsystem* InSubsystem, USFExtendService* InExtendService);

    /** Shutdown service */
    void Shutdown();

    // ==================== Service Status ====================

    /** Check if service is ready for wiring operations */
    UFUNCTION(BlueprintCallable, Category = "Smart|Extend|Wiring")
    bool IsReady() const { return ExtendService != nullptr; }

    // ==================== E-chain wiring (slice E2a) ====================
    // Moved from USFExtendService; operate on its shared registry maps via ExtendService->.

    /** Clear all connection wiring tracking maps */
    void ClearConnectionWiringMaps();

    /** Connect all chain elements (belts/lifts) after they've been built (deferred post-build) */
    void ConnectAllChainElements(AFGBuildableFactory* NewFactory);

    // ==================== Manifold connections (slice E2 unit G) ====================
    void WireManifoldConnections(AFGBuildableFactory* SourceFactory, AFGBuildableFactory* CloneFactory);
    void WireManifoldPipe(AFGBuildablePipeline* BuiltPipe, UFGPipeConnectionComponentBase* SourceConnector, int32 CloneChainId);
    void WireManifoldBelt(AFGBuildableConveyorBelt* BuiltBelt, UFGFactoryConnectionComponent* SourceConnector, int32 CloneChainId);
    bool CreateManifoldBelt(UFGFactoryConnectionComponent* FromConnector, UFGFactoryConnectionComponent* ToConnector);
    bool CreateManifoldPipe(UFGPipeConnectionComponentBase* FromConnector, UFGPipeConnectionComponentBase* ToConnector);
    AFGBuildableFactory* GetSourceFactory() const;

private:
    /** Wire up pipe hologram connections after all holograms in a chain are spawned */
    void WirePipeChainConnections(int32 ChainId, AFGHologram* ParentHologram, bool bIsInputChain);

    /** Wire up belt hologram connections after all holograms in a chain are spawned */
    void WireBeltChainConnections(int32 ChainId, AFGHologram* ParentHologram, bool bIsInputChain);

    /** Find a pipe connection component on a hologram by index (0 or 1) */
    UFGPipeConnectionComponentBase* FindPipeConnectionByIndex(AFGHologram* Hologram, int32 Index) const;

    /** Find a factory connection component on a hologram by index (0 or 1) */
    UFGFactoryConnectionComponent* FindFactoryConnectionByIndex(AFGHologram* Hologram, int32 Index) const;

    /** Owning subsystem */
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Parent extend service (owns wiring implementation for now) */
    UPROPERTY()
    USFExtendService* ExtendService = nullptr;
};
