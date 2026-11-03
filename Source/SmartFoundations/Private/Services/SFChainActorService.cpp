// Copyright (c) 2025-present Finalomega. All rights reserved. See LICENSE.md.

#include "Services/SFChainActorService.h"
#include "SmartFoundations.h"
#include "Subsystem/SFSubsystem.h"
#include "FGBuildableSubsystem.h"
#include "FGConveyorChainActor.h"
#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableConveyorBase.h"
#include "Buildables/FGBuildableConveyorLift.h"
#include "FGFactoryConnectionComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "FGCircuitConnectionComponent.h"
#include "Buildables/FGBuildableWire.h"

void USFChainActorService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;

    // No load-time or post-build global conveyor repair. Construction operations
    // own exact affected cohorts; this service retains only wire guards and read-only diagnostics.

}

void USFChainActorService::Shutdown()
{
	Subsystem.Reset();
}

AFGBuildableSubsystem* USFChainActorService::GetBuildableSubsystem() const
{
	USFSubsystem* Sub = Subsystem.Get();
	if (!Sub) return nullptr;
	UWorld* World = Sub->GetWorld();
	if (!World) return nullptr;
	return AFGBuildableSubsystem::Get(World);
}

int32 USFChainActorService::ScrubNullWireEntries(AFGBuildable* Buildable)
{
	if (!Buildable)
	{
		return 0;
	}

	int32 Scrubbed = 0;
	TInlineComponentArray<UFGCircuitConnectionComponent*> CircuitConnections;
	Buildable->GetComponents(CircuitConnections);
	for (UFGCircuitConnectionComponent* Connection : CircuitConnections)
	{
		if (!Connection)
		{
			continue;
		}
		// mWires is protected (AccessTransformers friend grant). Remove entries that are null
		// or already torn down - vanilla's dismantle loop calls Execute_Dismantle on each one
		// unguarded and asserts on null.
		const int32 Removed = Connection->mWires.RemoveAll([](const TObjectPtr<AFGBuildableWire>& Wire)
		{
			return !IsValid(Wire.Get());
		});
		if (Removed > 0)
		{
			Scrubbed += Removed;
			Connection->mNumWiresConnected = static_cast<uint8>(Connection->mWires.Num());
		}
	}
	return Scrubbed;
}

int32 USFChainActorService::RemoveBuildableFromFactoryTickArrays(AFGBuildable* Buildable)
{
    // Retained ABI only. Native EndPlay/unregistration owns factory membership,
    // partition caches and dirty metadata; a raw array removal is not that transaction.
    return 0;
}

int32 USFChainActorService::ScrubFactoryTickArrays()
{
    // A sequential buildable callback is not authorization to repair arbitrary
    // subsystem containers or dereference potentially freed raw array entries.
    return 0;
}

void USFChainActorService::DumpFailingTickGroupToJson(
	const FConveyorTickGroup* TG,
	const AFGConveyorChainActor* Zombie,
	const FString& Phase,
	const TArray<TPair<FString, FString>>& PreMigrateBackptrs) const
{
	if (!TG) return;

	// One file per failure, timestamped with sub-second resolution so multiple dumps
	// in the same batch don't collide.
	const FDateTime Now = FDateTime::Now();
	const FString Stamp = Now.ToString(TEXT("%Y%m%d_%H%M%S")) + FString::Printf(TEXT("_%03d"), Now.GetMillisecond());
	const FString Path = FPaths::ProjectLogDir() / FString::Printf(TEXT("ChainDiag_%s_%s.json"), *Phase, *Stamp);

	FString J;
	J.Reserve(4096);
	J += TEXT("{\n");
	J += FString::Printf(TEXT("  \"phase\": \"%s\",\n"), *Phase);
	J += FString::Printf(TEXT("  \"timestamp\": \"%s\",\n"), *Now.ToIso8601());
	J += FString::Printf(TEXT("  \"tg_ptr\": \"0x%p\",\n"), TG);
	J += FString::Printf(TEXT("  \"tg_conveyor_count\": %d,\n"), TG->Conveyors.Num());
	J += FString::Printf(TEXT("  \"tg_chain_actor\": \"%s\",\n"),
		TG->ChainActor ? *TG->ChainActor->GetName() : TEXT("null"));

	// Vanilla reads Conveyors[0] as mFirstConveyor (output-end, "front of chain")
	// and Conveyors.Last() as mLastConveyor (input-end). These are the two names the
	// "Last X != mLastConv Y" warning prints — dumping them here makes correlation trivial.
	if (TG->Conveyors.Num() > 0)
	{
		AFGBuildableConveyorBase* First = TG->Conveyors[0];
		AFGBuildableConveyorBase* Last  = TG->Conveyors.Last();
		J += FString::Printf(TEXT("  \"tg_conveyors_first\": \"%s\",\n"),
			IsValid(First) ? *First->GetName() : TEXT("invalid"));
		J += FString::Printf(TEXT("  \"tg_conveyors_last\": \"%s\",\n"),
			IsValid(Last) ? *Last->GetName() : TEXT("invalid"));
	}

	if (Zombie)
	{
		J += FString::Printf(TEXT("  \"zombie_name\": \"%s\",\n"), *Zombie->GetName());
		J += FString::Printf(TEXT("  \"zombie_num_segments\": %d,\n"), Zombie->GetNumChainSegments());
	}

	// Pre-migrate back-pointer snapshot: shows what mConveyorChainActor each belt held
	// BEFORE vanilla's Migrate ran. If these are mixed (multiple distinct non-null values)
	// or non-null at all, vanilla's BuildChain traversal aborts on the first "owned" belt.
	if (PreMigrateBackptrs.Num() > 0)
	{
		J += TEXT("  \"pre_migrate_backpointers\": [\n");
		for (int32 i = 0; i < PreMigrateBackptrs.Num(); ++i)
		{
			const bool bLast = (i == PreMigrateBackptrs.Num() - 1);
			J += FString::Printf(TEXT("    {\"belt\":\"%s\",\"chain_actor_backptr\":\"%s\"}%s\n"),
				*PreMigrateBackptrs[i].Key, *PreMigrateBackptrs[i].Value,
				bLast ? TEXT("") : TEXT(","));
		}
		J += TEXT("  ],\n");
	}

	J += TEXT("  \"conveyors\": [\n");
	for (int32 i = 0; i < TG->Conveyors.Num(); ++i)
	{
		AFGBuildableConveyorBase* Belt = TG->Conveyors[i];
		const bool bLast = (i == TG->Conveyors.Num() - 1);

		J += TEXT("    {\n");
		if (!IsValid(Belt))
		{
			J += FString::Printf(TEXT("      \"index\": %d, \"name\": \"invalid\", \"valid\": false\n"), i);
			J += bLast ? TEXT("    }\n") : TEXT("    },\n");
			continue;
		}

		J += FString::Printf(TEXT("      \"index\": %d,\n"), i);
		J += FString::Printf(TEXT("      \"name\": \"%s\",\n"), *Belt->GetName());
		J += FString::Printf(TEXT("      \"class\": \"%s\",\n"),
			Belt->GetClass() ? *Belt->GetClass()->GetName() : TEXT("?"));
		J += FString::Printf(TEXT("      \"is_lift\": %s,\n"),
			Cast<AFGBuildableConveyorLift>(Belt) ? TEXT("true") : TEXT("false"));
		// Bucket ID + level: vanilla's BuildChain refuses to cross a level or bucket boundary.
		// If a merged TG contains belts from different buckets/levels this is why it zombies.
		J += FString::Printf(TEXT("      \"bucket_id\": %d,\n"), Belt->GetConveyorBucketID());
		J += FString::Printf(TEXT("      \"level\": %d,\n"), Belt->GetLevel());
		J += FString::Printf(TEXT("      \"chain_actor_backptr\": \"%s\",\n"),
			Belt->GetConveyorChainActor() ? *Belt->GetConveyorChainActor()->GetName() : TEXT("null"));

		auto DescribeConn = [](UFGFactoryConnectionComponent* Conn) -> FString
		{
			if (!Conn) return TEXT("\"null\"");
			UFGFactoryConnectionComponent* Peer = Conn->GetConnection();
			if (!Peer) return TEXT("\"unconnected\"");
			AActor* PeerOwner = Peer->GetOwner();
			const FString OwnerName = PeerOwner ? PeerOwner->GetName() : TEXT("?");
			const bool bPeerIsBelt = Cast<AFGBuildableConveyorBase>(PeerOwner) != nullptr;
			return FString::Printf(TEXT("{\"peer\":\"%s\",\"is_belt\":%s}"),
				*OwnerName, bPeerIsBelt ? TEXT("true") : TEXT("false"));
		};

		J += FString::Printf(TEXT("      \"conn0\": %s,\n"), *DescribeConn(Belt->GetConnection0()));
		J += FString::Printf(TEXT("      \"conn1\": %s\n"), *DescribeConn(Belt->GetConnection1()));
		J += bLast ? TEXT("    }\n") : TEXT("    },\n");
	}
	J += TEXT("  ]\n");
	J += TEXT("}\n");

	if (FFileHelper::SaveStringToFile(J, *Path))
	{
		UE_LOG(LogSmartUpgrade, VeryVerbose,
			TEXT("ChainActorService: topology dump written → %s"), *Path);
	}
	else
	{
		UE_LOG(LogSmartUpgrade, Verbose,
			TEXT("ChainActorService: failed to write topology dump to %s"), *Path);
	}
}

