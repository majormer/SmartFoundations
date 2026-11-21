// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "SFLogMacros.h"
#include "Core/Net/SFRCO.h"
#include "Core/Upgrade/SFConveyorGeometryPolicy.h"
#include "FGPlayerController.h"
#include "Engine/World.h"
#include "Subsystem/SFSubsystem.h"
#include "TimerManager.h"

namespace
{
// Entries contain only fixed-size values and a net object reference. Keep the
// worst-case page well below the 64 KiB bunch limit, even with object exports.
constexpr int32 AuditPageSize = 128;
constexpr float AuditIdleTimeout = 30.0f;

USFUpgradeAuditService* GetAuditService(USFRCO* RCO)
{
	if (USFSubsystem* Subsystem = USFSubsystem::Get(RCO))
	{
		return Subsystem->GetUpgradeAuditService();
	}
	return nullptr;
}
}

void USFRCO::RequestUpgradeAudit(FSFUpgradeAuditParams Params)
{
	CancelUpgradeAuditRequest();
	IncomingAuditId = FGuid::NewGuid();
	Params.RequestId = IncomingAuditId;
	// The world scan itself may take a long time on a large save. Time only
	// the page exchange, once the server has a completed snapshot.
	Server_StartUpgradeAudit(Params);
}

void USFRCO::CancelUpgradeAuditRequest()
{
	if (!IncomingAuditId.IsValid()) return;

	const FGuid RequestId = IncomingAuditId;
	IncomingAuditId.Invalidate();
	FinishIncomingAudit(false);
	Server_CancelUpgradeAudit(RequestId);
}

void USFRCO::Server_StartUpgradeAudit_Implementation(FSFUpgradeAuditParams Params)
{
	// Never use a supplied player reference for ownership or reply routing.
	AFGPlayerController* PC = GetTypedOuter<AFGPlayerController>();
	Params.RequestingPlayer = PC;
	ResetOutgoingAudit();
	if (USFUpgradeAuditService* Service = GetAuditService(this);
		PC && PC->HasAuthority() && Service && Service->StartAudit(Params))
	{
		return;
	}
	Client_BeginUpgradeAuditResult(Params.RequestId, FSFUpgradeAuditResult{}, 0);
}

bool USFRCO::Server_StartUpgradeAudit_Validate(FSFUpgradeAuditParams Params)
{
	float RadiusSq = 0.0f;
	return Params.RequestId.IsValid()
		&& SFConveyorGeometryPolicy::TryResolveRadiusSquared(Params.Origin, Params.Radius, RadiusSq);
}

void USFRCO::Server_CancelUpgradeAudit_Implementation(FGuid RequestId)
{
	if (!RequestId.IsValid()) return;
	if (OutgoingAuditId == RequestId) ResetOutgoingAudit();
	if (USFUpgradeAuditService* Service = GetAuditService(this))
	{
		Service->CancelRemoteAudit(GetTypedOuter<AFGPlayerController>(), RequestId);
	}
}

void USFRCO::SendUpgradeAuditResult(FGuid RequestId, const FSFUpgradeAuditResult& Result)
{
	ResetOutgoingAudit();
	FSFUpgradeAuditResult Header = Result;
	for (FSFUpgradeFamilyResult& Family : Header.FamilyResults)
	{
		for (FSFUpgradeTierBucket& Bucket : Family.TierBuckets)
		{
			OutgoingAuditEntries.Append(MoveTemp(Bucket.Entries));
			Bucket.Entries.Empty();
		}
	}

	const int32 TotalEntries = OutgoingAuditEntries.Num();
	if (TotalEntries > 0)
	{
		OutgoingAuditId = RequestId;
		GetWorld()->GetTimerManager().SetTimer(OutgoingAuditTimeout, this,
			&USFRCO::ResetOutgoingAudit, AuditIdleTimeout);
	}
	Client_BeginUpgradeAuditResult(RequestId, Header, TotalEntries);
	UE_LOG(LogSmartUpgrade, Log, TEXT("Audit reply: request=%s scanned=%d upgradeable=%d entries=%d pages=%d success=%d"),
		*RequestId.ToString(), Result.TotalScanned, Result.TotalUpgradeable, TotalEntries,
		FMath::DivideAndRoundUp(TotalEntries, AuditPageSize), Result.bSuccess);
}

void USFRCO::Client_BeginUpgradeAuditResult_Implementation(FGuid RequestId, FSFUpgradeAuditResult Header, int32 TotalEntries)
{
	if (!IncomingAuditId.IsValid() || RequestId != IncomingAuditId || IncomingAuditTotal != INDEX_NONE) return;
	if (!Header.bSuccess || TotalEntries < 0 || TotalEntries > Header.TotalScanned)
	{
		FinishIncomingAudit(false);
		return;
	}

	IncomingAuditResult = MoveTemp(Header);
	IncomingAuditTotal = TotalEntries;
	if (TotalEntries == 0)
	{
		FinishIncomingAudit(true);
		return;
	}
	ArmIncomingAuditTimeout();
	Server_RequestUpgradeAuditPage(RequestId, 0);
}

void USFRCO::Server_RequestUpgradeAuditPage_Implementation(FGuid RequestId, int32 Offset)
{
	if (!OutgoingAuditId.IsValid() || RequestId != OutgoingAuditId || Offset != OutgoingAuditOffset
		|| Offset >= OutgoingAuditEntries.Num()) return;

	const int32 Count = FMath::Min(AuditPageSize, OutgoingAuditEntries.Num() - Offset);
	const TArray<FSFUpgradeAuditEntry> Page(OutgoingAuditEntries.GetData() + Offset, Count);
	OutgoingAuditOffset += Count;
	Client_ReceiveUpgradeAuditPage(RequestId, Offset, Page);
	if (OutgoingAuditOffset == OutgoingAuditEntries.Num())
	{
		ResetOutgoingAudit();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(OutgoingAuditTimeout, this,
			&USFRCO::ResetOutgoingAudit, AuditIdleTimeout);
	}
}

void USFRCO::Client_ReceiveUpgradeAuditPage_Implementation(FGuid RequestId, int32 Offset, const TArray<FSFUpgradeAuditEntry>& Entries)
{
	if (!IncomingAuditId.IsValid() || RequestId != IncomingAuditId || IncomingAuditTotal <= 0) return;
	if (Offset != IncomingAuditOffset || Entries.IsEmpty() || Entries.Num() > AuditPageSize
		|| Entries.Num() > IncomingAuditTotal - Offset)
	{
		FinishIncomingAudit(false);
		return;
	}
	for (const FSFUpgradeAuditEntry& Entry : Entries)
	{
		FSFUpgradeFamilyResult* Family = IncomingAuditResult.FamilyResults.FindByPredicate(
			[&Entry](const FSFUpgradeFamilyResult& Value) { return Value.Family == Entry.Family; });
		FSFUpgradeTierBucket* Bucket = Family ? Family->TierBuckets.FindByPredicate(
			[&Entry](const FSFUpgradeTierBucket& Value) { return Value.Tier == Entry.CurrentTier; }) : nullptr;
		if (!Bucket || Bucket->Entries.Num() >= Bucket->Count)
		{
			FinishIncomingAudit(false);
			return;
		}
		Bucket->Entries.Add(Entry);
	}
	IncomingAuditOffset += Entries.Num();
	if (IncomingAuditOffset == IncomingAuditTotal)
	{
		FinishIncomingAudit(true);
		return;
	}
	ArmIncomingAuditTimeout();
	Server_RequestUpgradeAuditPage(RequestId, IncomingAuditOffset);
}

void USFRCO::ArmIncomingAuditTimeout()
{
	GetWorld()->GetTimerManager().SetTimer(IncomingAuditTimeout,
		FTimerDelegate::CreateWeakLambda(this, [this]() { FinishIncomingAudit(false); }), AuditIdleTimeout, false);
}

void USFRCO::FinishIncomingAudit(bool bSuccess)
{
	const FGuid RequestId = IncomingAuditId;
	const int32 Received = IncomingAuditOffset;
	FSFUpgradeAuditResult Result = MoveTemp(IncomingAuditResult);
	IncomingAuditId.Invalidate();
	IncomingAuditResult = {};
	IncomingAuditTotal = INDEX_NONE;
	IncomingAuditOffset = 0;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(IncomingAuditTimeout);
	if (!RequestId.IsValid()) return; // Local cancellation already updated the panel.
	if (!bSuccess)
	{
		Result = {};
		Server_CancelUpgradeAudit(RequestId);
	}
	UE_LOG(LogSmartUpgrade, Log, TEXT("Audit reply received: request=%s scanned=%d upgradeable=%d entries=%d success=%d"),
		*RequestId.ToString(), Result.TotalScanned, Result.TotalUpgradeable, Received, bSuccess);
	if (USFUpgradeAuditService* Service = GetAuditService(this))
	{
		Service->InjectAuditResult(Result);
	}
}

void USFRCO::ResetOutgoingAudit()
{
	OutgoingAuditId.Invalidate();
	OutgoingAuditEntries.Empty();
	OutgoingAuditOffset = 0;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(OutgoingAuditTimeout);
}
