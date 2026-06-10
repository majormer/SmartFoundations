// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "SFRCO.h"
#include "SmartFoundations.h"
#include "Hologram/FGHologram.h"
#include "Subsystem/SFSubsystem.h"
#include "FGPlayerController.h"  // AFGPlayerController (don't rely on transitive unity-build includes)
#include "Net/UnrealNetwork.h"

// ========================================
// UFGRemoteCallObject Interface
// ========================================

bool USFRCO::ShouldRegisterRemoteCallObject(const AFGGameMode* GameMode) const
{
	// Always register Smart! RCO - no conditional registration needed
	return true;
}

void USFRCO::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Required for the RCO to replicate at all (SML rule): register at least one replicated property.
	DOREPLIFETIME(USFRCO, bDummyReplicated);
}

// ========================================
// Scaling RPCs
// ========================================

void USFRCO::Server_ApplyScaling_Implementation(
	AFGHologram* HologramActor,
	uint8 Axis,
	int32 Delta,
	int32 NewCounter
)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_ApplyScaling: Axis=%d, Delta=%d, Counter=%d, Hologram=%s"),
		Axis, Delta, NewCounter, *GetNameSafe(HologramActor));

	// Validate hologram exists
	if (!IsValid(HologramActor))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] Server_ApplyScaling: Invalid hologram actor"));
		return;
	}

	// Additional validation checks
	if (!ValidateScalingRequest(HologramActor, Axis, Delta, NewCounter))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] Server_ApplyScaling: Validation failed"));
		return;
	}

	// Get Smart! subsystem to apply the scaling
	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!IsValid(Subsystem))
	{
		UE_LOG(LogSmartFoundations, Error,
			TEXT("[SFRCO] Server_ApplyScaling: Failed to get SFSubsystem"));
		return;
	}

	// Forward to subsystem's scaling logic
	Subsystem->ApplyScalingFromRPC(HologramActor, Axis, Delta, NewCounter);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_ApplyScaling: SUCCESS - Forwarded to subsystem"));
}

bool USFRCO::Server_ApplyScaling_Validate(
	AFGHologram* HologramActor,
	uint8 Axis,
	int32 Delta,
	int32 NewCounter
)
{
	// Basic parameter validation
	if (Axis > 2) // X=0, Y=1, Z=2
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] Server_ApplyScaling_Validate: Invalid axis %d"), Axis);
		return false;
	}

	if (FMath::Abs(Delta) != 1)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] Server_ApplyScaling_Validate: Invalid delta %d (must be ±1)"), Delta);
		return false;
	}

	// Counter range check
	const int32 ClampedCounter = ClampCounter(NewCounter);
	if (ClampedCounter != NewCounter)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] Server_ApplyScaling_Validate: Counter %d out of range"), NewCounter);
		return false;
	}

	return true;
}

void USFRCO::Server_ResetScaling_Implementation(AFGHologram* HologramActor)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_ResetScaling: Hologram=%s"), *GetNameSafe(HologramActor));

	if (!IsValid(HologramActor))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] Server_ResetScaling: Invalid hologram actor"));
		return;
	}

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!IsValid(Subsystem))
	{
		UE_LOG(LogSmartFoundations, Error,
			TEXT("[SFRCO] Server_ResetScaling: Failed to get SFSubsystem"));
		return;
	}

	// Forward to subsystem
	Subsystem->ResetScalingFromRPC(HologramActor);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_ResetScaling: SUCCESS - Forwarded to subsystem"));
}

bool USFRCO::Server_ResetScaling_Validate(AFGHologram* HologramActor)
{
	// Just validate hologram pointer is not null
	return HologramActor != nullptr;
}

// ========================================
// Spacing RPCs
// ========================================

void USFRCO::Server_SetSpacingMode_Implementation(ESFSpacingMode NewMode)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_SetSpacingMode: NewMode=%d"), static_cast<uint8>(NewMode));

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!IsValid(Subsystem))
	{
		UE_LOG(LogSmartFoundations, Error,
			TEXT("[SFRCO] Server_SetSpacingMode: Failed to get SFSubsystem"));
		return;
	}

	// Forward to subsystem spacing logic
	Subsystem->SetSpacingModeFromRPC(NewMode);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_SetSpacingMode: SUCCESS - Forwarded to subsystem"));
}

bool USFRCO::Server_SetSpacingMode_Validate(ESFSpacingMode NewMode)
{
	// Validate enum is in range
	const uint8 ModeValue = static_cast<uint8>(NewMode);
	return ModeValue >= 0 && ModeValue <= 3; // None=0, X=1, XY=2, XYZ=3
}

// ========================================
// Arrow Visibility RPCs
// ========================================

void USFRCO::Server_ToggleArrows_Implementation(bool bVisible)
{
	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_ToggleArrows: Visible=%d"), bVisible);

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!IsValid(Subsystem))
	{
		UE_LOG(LogSmartFoundations, Error,
			TEXT("[SFRCO] Server_ToggleArrows: Failed to get SFSubsystem"));
		return;
	}

	// Forward to subsystem arrow manager
	Subsystem->SetArrowVisibilityFromRPC(bVisible);

	UE_LOG(LogSmartFoundations, VeryVerbose,
		TEXT("[SFRCO] Server_ToggleArrows: SUCCESS - Forwarded to subsystem"));
}

bool USFRCO::Server_ToggleArrows_Validate(bool bVisible)
{
	// Boolean always valid
	return true;
}

// ========================================
// MP spec-based scaling construction
// ========================================

void USFRCO::Server_StageScalingSpec_Implementation(FSFScalingSpec Spec)
{
	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	AFGPlayerController* OwnerPC = Cast<AFGPlayerController>(GetOuter());
	if (!IsValid(Subsystem) || !OwnerPC)
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] Server_StageScalingSpec: missing subsystem (%d) or owner PC (%d)"),
			IsValid(Subsystem) ? 1 : 0, OwnerPC ? 1 : 0);
		return;
	}

	Subsystem->StageScalingSpecForPlayer(OwnerPC, Spec);

	if (Spec.bValid)
	{
		UE_LOG(LogSmartFoundations, Display,
			TEXT("[MP-SPEC] Server staged scaling spec for %s: %d cells of %s, %d planned conduit(s)."),
			*GetNameSafe(OwnerPC), Spec.CellCount(), *GetNameSafe(*Spec.BuildClass), Spec.ConduitPlan.Num());
	}
}

bool USFRCO::Server_StageScalingSpec_Validate(FSFScalingSpec Spec)
{
	// Sanity-bound the grid (a forged/buggy spec cannot demand absurd expansion).
	const FIntVector& G = Spec.Counters.GridCounters;
	const int64 Cells = (int64)FMath::Max(1, FMath::Abs(G.X))
		* FMath::Max(1, FMath::Abs(G.Y))
		* FMath::Max(1, FMath::Abs(G.Z));
	if (FMath::Abs(G.X) > 2000 || FMath::Abs(G.Y) > 2000 || FMath::Abs(G.Z) > 2000
		|| Cells > 100000)
	{
		return false;
	}

	// Sanity-bound the conduit plan (#334): at most a few conduits per grid cell in practice; allow
	// generous headroom. Spline previews route with a handful of points; 64 is far beyond any real
	// auto-connect belt.
	if (Spec.ConduitPlan.Num() > 4096)
	{
		return false;
	}
	for (const FSFConduitPlanEntry& Entry : Spec.ConduitPlan)
	{
		if (Entry.SplinePoints.Num() > 64)
		{
			return false;
		}
	}
	return true;
}

// ========================================
// Upgrade Audit RPCs
// ========================================

void USFRCO::Server_StartUpgradeAudit_Implementation(FSFUpgradeAuditParams Params)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[SFRCO] Server_StartUpgradeAudit: Radius=%f"), Params.Radius);

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (!IsValid(Subsystem))
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("[SFRCO] Server_StartUpgradeAudit: Failed to get SFSubsystem"));
		return;
	}

	USFUpgradeAuditService* AuditService = Subsystem->GetUpgradeAuditService();
	if (!IsValid(AuditService))
	{
		UE_LOG(LogSmartFoundations, Error, TEXT("[SFRCO] Server_StartUpgradeAudit: Failed to get UpgradeAuditService"));
		return;
	}

	// Set the requesting player for result delivery back to client
	Params.RequestingPlayer = Cast<AFGPlayerController>(GetOuter());

	// Start the audit on the server
	AuditService->StartAudit(Params);
}

bool USFRCO::Server_StartUpgradeAudit_Validate(FSFUpgradeAuditParams Params)
{
	// Basic validation: radius must be non-negative
	return Params.Radius >= 0.0f;
}

void USFRCO::Server_CancelUpgradeAudit_Implementation()
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[SFRCO] Server_CancelUpgradeAudit"));

	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (IsValid(Subsystem))
	{
		if (USFUpgradeAuditService* AuditService = Subsystem->GetUpgradeAuditService())
		{
			AuditService->CancelAudit();
		}
	}
}

bool USFRCO::Server_CancelUpgradeAudit_Validate()
{
	return true;
}

void USFRCO::Client_ReceiveAuditResult_Implementation(FSFUpgradeAuditResult Result)
{
	UE_LOG(LogSmartFoundations, VeryVerbose, TEXT("[SFRCO] Client_ReceiveAuditResult: Success=%d, TotalScanned=%d"), Result.bSuccess, Result.TotalScanned);

	// Results are received on the client and injected into the local audit service
	USFSubsystem* Subsystem = USFSubsystem::Get(this);
	if (IsValid(Subsystem))
	{
		if (USFUpgradeAuditService* AuditService = Subsystem->GetUpgradeAuditService())
		{
			AuditService->InjectAuditResult(Result);
		}
	}
}

// ========================================
// Validation & Security Helpers
// ========================================

bool USFRCO::ValidateScalingRequest(
	AFGHologram* HologramActor,
	uint8 Axis,
	int32 Delta,
	int32 NewCounter
) const
{
	// Check hologram authority
	if (!HasHologramAuthority(HologramActor))
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] ValidateScalingRequest: Caller lacks hologram authority"));
		return false;
	}

	// Check rate limiting
	if (!CheckRateLimit())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("[SFRCO] ValidateScalingRequest: Rate limit exceeded"));
		return false;
	}

	// All validation passed
	return true;
}

int32 USFRCO::ClampCounter(int32 Counter) const
{
	// Clamp to safe range [-100, 100]
	// Each step is 50cm, so max offset is ±50m which is reasonable
	return FMath::Clamp(Counter, -100, 100);
}

bool USFRCO::HasHologramAuthority(AFGHologram* HologramActor) const
{
	// TODO Task #22: Implement proper ownership/authority checks
	// For now, just check if hologram is valid
	if (!IsValid(HologramActor))
	{
		return false;
	}

	// Future: Check if GetOwnerPlayerController() matches hologram owner
	// AFGPlayerController* OwningController = GetOwnerPlayerController();
	// return OwningController && HologramActor->GetInstigator() == OwningController->GetPawn();

	return true; // Placeholder - accept all valid holograms for now
}

bool USFRCO::CheckRateLimit() const
{
	// TODO Task #22: Implement proper rate limiting
	// Max 20 requests/second with exponential backoff

	// Placeholder: always allow for now
	return true;
}
