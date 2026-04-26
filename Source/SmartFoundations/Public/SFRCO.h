#pragma once

#include "CoreMinimal.h"
#include "FGRemoteCallObject.h"
#include "Features/Spacing/SFSpacingTypes.h"
#include "Features/Upgrade/SFUpgradeExecutionService.h"
#include "SFRCO.generated.h"

/**
 * Smart! Remote Call Object
 * 
 * Handles client→server RPC calls for scaling, spacing, and arrow visibility.
 * Registered via SML's Remote Call Object Registry during module startup.
 * 
 * @see Task #12 in tasks/tasks.json
 * @see docs/SMART_ARCHITECTURE_PHILOSOPHY.md for architectural overview
 */
UCLASS(Within = FGPlayerController)
class SMARTFOUNDATIONS_API USFRCO : public UFGRemoteCallObject
{
	GENERATED_BODY()

public:
	//~ Begin UFGRemoteCallObject Interface
	virtual bool ShouldRegisterRemoteCallObject(const class AFGGameMode* GameMode) const override;
	//~ End UFGRemoteCallObject Interface

	// ========================================
	// Scaling RPCs
	// ========================================

	/**
	 * Client requests to apply scaling offset to their active hologram
	 * 
	 * @param HologramActor - The hologram being scaled (authority check target)
	 * @param Axis - Which axis to scale (X=0, Y=1, Z=2)
	 * @param Delta - Increment direction (+1 or -1)
	 * @param NewCounter - Client's proposed counter value (server validates)
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ApplyScaling(
		AFGHologram* HologramActor,
		uint8 Axis,
		int32 Delta,
		int32 NewCounter
	);

	/**
	 * Client requests to reset all scaling offsets to zero
	 * 
	 * @param HologramActor - The hologram to reset
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ResetScaling(AFGHologram* HologramActor);

	// ========================================
	// Spacing RPCs
	// ========================================

	/**
	 * Client requests to change spacing mode (None→X→XY→XYZ→None)
	 * 
	 * @param NewMode - Desired spacing mode
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_SetSpacingMode(ESFSpacingMode NewMode);

	// ========================================
	// Arrow Visibility RPCs
	// ========================================

	/**
	 * Client requests to toggle arrow visibility
	 * 
	 * @param bVisible - New visibility state
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_ToggleArrows(bool bVisible);

	// ========================================
	// Upgrade Audit RPCs
	// ========================================

	/**
	 * Client requests to start an upgrade audit
	 * 
	 * @param Params - Audit parameters (radius, families, etc.)
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_StartUpgradeAudit(FSFUpgradeAuditParams Params);

	/**
	 * Client requests to cancel an in-progress audit
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_CancelUpgradeAudit();

	/**
	 * Server sends audit result back to the client
	 * 
	 * @param Result - The complete audit result snapshot
	 */
	UFUNCTION(Client, Reliable)
	void Client_ReceiveAuditResult(FSFUpgradeAuditResult Result);

protected:
	// ========================================
	// Validation & Security
	// ========================================

	/**
	 * Validate scaling parameters and caller authority
	 * Returns true if request is valid, false otherwise
	 */
	bool ValidateScalingRequest(
		AFGHologram* HologramActor,
		uint8 Axis,
		int32 Delta,
		int32 NewCounter
	) const;

	/**
	 * Clamp counter to safe range [-100, 100]
	 * Prevents overflow and abuse
	 */
	int32 ClampCounter(int32 Counter) const;

	/**
	 * Check if caller has authority to modify the hologram
	 * (Placeholder for future ownership/permission checks)
	 */
	bool HasHologramAuthority(AFGHologram* HologramActor) const;

	/**
	 * Check rate limiting for spam prevention
	 * (Placeholder for throttling logic - Task #22)
	 */
	bool CheckRateLimit() const;

private:
	// Future: Rate limiting state per client
	// TMap<AFGPlayerController*, float> LastRequestTime;
};
