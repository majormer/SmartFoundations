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

private:
    /** Owning subsystem */
    UPROPERTY()
    TWeakObjectPtr<USFSubsystem> Subsystem;

    /** Parent extend service (owns wiring implementation for now) */
    UPROPERTY()
    USFExtendService* ExtendService = nullptr;
};
