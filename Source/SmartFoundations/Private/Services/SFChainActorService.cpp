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

void USFChainActorService::Initialize(USFSubsystem* InSubsystem)
{
	Subsystem = InSubsystem;
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

FConveyorTickGroup* USFChainActorService::FindOwningTickGroup(
	AFGBuildableSubsystem* BuildableSub,
	AFGConveyorChainActor* Chain) const
{
	if (!BuildableSub || !Chain) return nullptr;

	// Primary: TG->ChainActor == Chain
	for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
	{
		if (TG && TG->ChainActor == Chain)
		{
			return TG;
		}
	}

	// Fallback: a segment belt of Chain lives in a TG (orphan chain case).
	// Iterate segments and for each valid belt look for a containing group.
	for (const FConveyorChainSplineSegment& Seg : Chain->GetChainSegments())
	{
		AFGBuildableConveyorBase* Belt = Seg.ConveyorBase;
		if (!Belt || !IsValid(Belt)) continue;

		for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
		{
			if (TG && TG->Conveyors.Contains(Belt))
			{
				return TG;
			}
		}
	}

	return nullptr;
}

int32 USFChainActorService::InvalidateAndRebuildChains(
	const TSet<AFGConveyorChainActor*>& AffectedChains,
	const TSet<FConveyorTickGroup*>& ExplicitTickGroups)
{
	AFGBuildableSubsystem* BuildableSub = GetBuildableSubsystem();
	if (!BuildableSub)
	{
		UE_LOG(LogSmartFoundations, Warning, TEXT("ChainActorService: No BuildableSubsystem available — cannot invalidate chains"));
		return 0;
	}

	if (AffectedChains.Num() == 0 && ExplicitTickGroups.Num() == 0)
	{
		return 0;
	}

	auto DescribeConnectionForLog = [](UFGFactoryConnectionComponent* Conn) -> FString
	{
		if (!Conn) return TEXT("null");
		UFGFactoryConnectionComponent* Peer = Conn->GetConnection();
		if (!Peer) return TEXT("unconnected");

		AFGBuildable* PeerBuildable = Peer->GetOuterBuildable();
		const FString PeerName = PeerBuildable ? PeerBuildable->GetName() : TEXT("unknown");
		const FString PeerClass = (PeerBuildable && PeerBuildable->GetClass()) ? PeerBuildable->GetClass()->GetName() : TEXT("unknown");
		const bool bPeerIsConveyor = Cast<AFGBuildableConveyorBase>(PeerBuildable) != nullptr;
		return FString::Printf(TEXT("%s/%s conveyor=%s"),
			*PeerName,
			*PeerClass,
			bPeerIsConveyor ? TEXT("true") : TEXT("false"));
	};

	auto DescribeTickGroupForLog = [BuildableSub, &DescribeConnectionForLog](const FConveyorTickGroup* TG) -> FString
	{
		if (!TG) return TEXT("TG=null");

		FString Description;
		const int32 TickGroupIndex = BuildableSub->mConveyorTickGroup.IndexOfByKey(const_cast<FConveyorTickGroup*>(TG));
		Description += FString::Printf(TEXT("TG[%d]=%p chain=%s raw=%d"),
			TickGroupIndex,
			TG,
			TG->ChainActor ? *TG->ChainActor->GetName() : TEXT("null"),
			TG->Conveyors.Num());

		for (int32 Index = 0; Index < TG->Conveyors.Num(); ++Index)
		{
			AFGBuildableConveyorBase* Belt = TG->Conveyors[Index];
			if (!IsValid(Belt))
			{
				Description += FString::Printf(TEXT(" | [%d] invalid"), Index);
				continue;
			}

			Description += FString::Printf(
				TEXT(" | [%d] %s class=%s lift=%s speed=%.1f bucket=%d level=%d backptr=%s c0={%s} c1={%s}"),
				Index,
				*Belt->GetName(),
				Belt->GetClass() ? *Belt->GetClass()->GetName() : TEXT("unknown"),
				Cast<AFGBuildableConveyorLift>(Belt) ? TEXT("true") : TEXT("false"),
				Belt->GetSpeed(),
				Belt->GetConveyorBucketID(),
				Belt->GetLevel(),
				Belt->GetConveyorChainActor() ? *Belt->GetConveyorChainActor()->GetName() : TEXT("null"),
				*DescribeConnectionForLog(Belt->GetConnection0()),
				*DescribeConnectionForLog(Belt->GetConnection1()));
		}

		return Description;
	};

	// Phase 1: resolve each affected chain to its owning FConveyorTickGroup, deduped.
	// Chains with no owning group are fully orphan — their segment belts have already been
	// reassigned by competing migration. They are inert and GC naturally; no action.
	TSet<FConveyorTickGroup*> GroupsToRemove;
	int32 OrphanCount = 0;
	int32 InvalidChainCount = 0;
	int32 ExplicitGroupCount = 0;

	for (AFGConveyorChainActor* Chain : AffectedChains)
	{
		if (!IsValid(Chain))
		{
			++InvalidChainCount;
			continue;
		}

		if (FConveyorTickGroup* OwnerGroup = FindOwningTickGroup(BuildableSub, Chain))
		{
			GroupsToRemove.Add(OwnerGroup);
		}
		else
		{
			++OrphanCount;
		}
	}

	for (FConveyorTickGroup* TG : ExplicitTickGroups)
	{
		if (!TG) continue;
		++ExplicitGroupCount;
		if (TG->ChainActor)
		{
			GroupsToRemove.Add(TG);
		}
	}

	// Phase 2: clear every identified tick group. RemoveChainActorFromConveyorGroup
	// nulls TG->ChainActor AND every belt's mConveyorChainActor back-pointer. It does
	// NOT call Destroy() on the chain actor — the old chain becomes inert (no belts
	// agree back) and GCs naturally in ~1-2 minutes of game time.
	for (FConveyorTickGroup* TG : GroupsToRemove)
	{
		if (TG)
		{
			BuildableSub->RemoveChainActorFromConveyorGroup(TG);
		}
	}

	// Phase 2.5 + Phase 3: Drain pending groups, union-find merge adjacent isolated groups,
	// then migrate all surviving groups synchronously on the game thread.
	//
	// Root cause of SPLIT_CHAIN in mass upgrade: vanilla AddConveyor() creates a new isolated
	// FConveyorTickGroup for every registered belt. When 100s of belts are upgraded in a single
	// batch each new belt has its own 1-belt group. Without this phase, Phase 3 would call
	// MigrateConveyorGroupToChainActor on each isolated group independently, producing N separate
	// 1-belt chains where one contiguous N-belt chain should exist.
	//
	// This phase spans GroupsToRemove (cleared in Phase 2) PLUS any pre-existing pending groups
	// drained from mConveyorGroupsPendingChainActors. Adjacent groups (output belt of one group
	// feeds into a belt in another group) are absorbed via iterative union-find before migration.
	//
	// Belts and lifts both participate in conveyor chains. Cross-level connections are
	// preserved as endpoints, but lift edges must be allowed to merge: vanilla can and
	// does build one chain actor across belt -> lift -> belt runs.
	TSet<FConveyorTickGroup*> GroupsToMigrate = GroupsToRemove;
	for (FConveyorTickGroup* TG : ExplicitTickGroups)
	{
		if (TG) GroupsToMigrate.Add(TG);
	}

	// Drain pending groups into the migration set (moved from former Phase 3 header).
	for (FConveyorTickGroup* TG : BuildableSub->mConveyorGroupsPendingChainActors)
	{
		if (TG) GroupsToMigrate.Add(TG);
	}
	BuildableSub->mConveyorGroupsPendingChainActors.Empty();

	// Build belt → tick-group map across all groups to migrate.
	TMap<AFGBuildableConveyorBase*, FConveyorTickGroup*> BeltToGroup;
	for (FConveyorTickGroup* TG : GroupsToMigrate)
	{
		if (!TG) continue;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (IsValid(Belt)) BeltToGroup.Add(Belt, TG);
		}
	}

	// Union-find merge: repeatedly absorb adjacent groups until stable.
	TSet<FConveyorTickGroup*> MergedAwayGroups;
	bool bAnyMerge = true;
	while (bAnyMerge)
	{
		bAnyMerge = false;
		for (FConveyorTickGroup* TG : GroupsToMigrate)
		{
			if (!TG || MergedAwayGroups.Contains(TG)) continue;

			// Snapshot Conveyors to avoid iterator invalidation when we absorb.
			const TArray<AFGBuildableConveyorBase*> BeltSnapshot = TG->Conveyors;
			for (AFGBuildableConveyorBase* Belt : BeltSnapshot)
			{
				if (!IsValid(Belt)) continue;
				UFGFactoryConnectionComponent* OutConn = Belt->GetConnection1();
				if (!OutConn || !OutConn->IsConnected()) continue;
				UFGFactoryConnectionComponent* PeerConn = OutConn->GetConnection();
				if (!PeerConn) continue;

				AFGBuildableConveyorBase* Downstream =
					Cast<AFGBuildableConveyorBase>(PeerConn->GetOuterBuildable());
				if (!IsValid(Downstream)) continue;
				// Cross-level connection — streaming boundary, vanilla permanent split.
				if (Belt->GetLevel() != Downstream->GetLevel()) continue;

				FConveyorTickGroup** OtherTGPtr = BeltToGroup.Find(Downstream);
				if (!OtherTGPtr) continue;
				FConveyorTickGroup* OtherTG = *OtherTGPtr;
				if (OtherTG == TG || MergedAwayGroups.Contains(OtherTG)) continue;

				// Absorb OtherTG into TG.
				// Item flow is TG → OtherTG (OtherTG is downstream: TG's output-end
				// belt's Connection1 feeds into a belt in OtherTG).
				//
				// Vanilla stores FConveyorTickGroup::Conveyors in reverse item-flow order:
				//   [output-end at index 0, ..., input-end at last index]
				// A healthy pre-upgrade belt -> lift -> belt run confirms this ordering.
				// MigrateConveyorGroupToChainActor then passes Last as the traversal
				// start and [0] as the terminus target. If we build the array in
				// input-to-output order, vanilla logs "Last input != mLastConv output"
				// and creates a 0-segment zombie.
				//
				// Therefore the downstream group must come first in the merged array:
				// [OtherTG output..input] + [TG output..input].
				TArray<AFGBuildableConveyorBase*> MergedConveyors = OtherTG->Conveyors;
				MergedConveyors.Append(TG->Conveyors);
				TG->Conveyors = MoveTemp(MergedConveyors);

				// Update each absorbed belt's bucket ID to TG's canonical index.
				// mConveyorTickGroup is a TArray indexed by bucket ID; moved belts still
				// carry OtherTG's index which (a) would make vanilla's BuildChain-path
				// GetConnectedConveyorBelt return nullptr during its Connection1 walk
				// (it filters out belts whose bucket index is stale/invalid), producing
				// the "Last X != mLastConv Y" NO_SEGMENTS zombie, and (b) would crash
				// any later RemoveConveyor call via the
				// mConveyorTickGroup[bucketID]->Conveyors.Contains(belt) assertion.
				const int32 TGBucketIndex = BuildableSub->mConveyorTickGroup.IndexOfByKey(TG);
				if (TGBucketIndex != INDEX_NONE)
				{
					for (AFGBuildableConveyorBase* B : OtherTG->Conveyors)
					{
						if (IsValid(B)) B->SetConveyorBucketID(TGBucketIndex);
					}
				}

				for (AFGBuildableConveyorBase* B : OtherTG->Conveyors)
				{
					BeltToGroup.Add(B, TG);
				}
				OtherTG->Conveyors.Empty();
				MergedAwayGroups.Add(OtherTG);
				// DO NOT call mConveyorTickGroup.Remove(OtherTG) — it is a TArray, and
				// Remove() shifts indices of all subsequent buckets, invalidating every
				// belt whose mConveyorBucketID points past this slot. Leaving OtherTG
				// in place as an empty TG is benign (ticks are no-ops; GC is vanilla's
				// problem and it handles empty buckets fine).
				bAnyMerge = true;
				break;  // TG->Conveyors changed; restart from the outer for-loop.
			}
			if (bAnyMerge) break;
		}
	}

	// Drop absorbed groups from the migration set before Phase 3.
	for (FConveyorTickGroup* TG : MergedAwayGroups)
	{
		GroupsToMigrate.Remove(TG);
	}

	if (MergedAwayGroups.Num() > 0)
	{
		UE_LOG(LogSmartFoundations, Log,
			TEXT("ChainActorService: Phase 2.5 merged %d adjacent isolated tick group(s) — prevents SPLIT_CHAIN"),
			MergedAwayGroups.Num());
	}

	// Phase 3: Migrate all surviving groups to fresh chain actors.
	// Skip groups with zero or only-invalid belts — migrating those produces
	// NO_SEGMENTS zombie chain actors. Also log before/after so we can tell
	// whether vanilla's migrate call actually populated segments.
	int32 MigrateCount = 0;
	int32 SkippedEmptyGroups = 0;
	int32 PostMigrateZombies = 0;
	int32 FailedRecoveryGroups = 0;
	int32 DeferredDirtyGroups = 0;
	int32 DeferredDirtyConveyors = 0;

	auto IsDetachedOrDestroyingConveyor = [](AFGBuildableConveyorBase* Belt) -> bool
	{
		if (!IsValid(Belt) || Belt->IsActorBeingDestroyed()) return true;
		UFGFactoryConnectionComponent* Connection0 = Belt->GetConnection0();
		UFGFactoryConnectionComponent* Connection1 = Belt->GetConnection1();
		const bool bConnection0Connected = Connection0 && Connection0->IsConnected();
		const bool bConnection1Connected = Connection1 && Connection1->IsConnected();
		return !bConnection0Connected && !bConnection1Connected;
	};

	for (FConveyorTickGroup* TG : GroupsToMigrate)
	{
		if (!TG) continue;

		int32 DirtyConveyorsInGroup = 0;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (IsDetachedOrDestroyingConveyor(Belt))
			{
				++DirtyConveyorsInGroup;
			}
		}

		if (DirtyConveyorsInGroup > 0)
		{
			++DeferredDirtyGroups;
			DeferredDirtyConveyors += DirtyConveyorsInGroup;
			BuildableSub->mConveyorGroupsPendingChainActors.AddUnique(TG);
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("ChainActorService: Deferring TG migration because it still contains %d detached/destroying conveyor entrie(s). Details=%s"),
				DirtyConveyorsInGroup,
				*DescribeTickGroupForLog(TG));
			continue;
		}

		// Count valid belts in this group.
		int32 ValidBeltCount = 0;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (IsValid(Belt)) ++ValidBeltCount;
		}

		if (ValidBeltCount == 0)
		{
			++SkippedEmptyGroups;
			UE_LOG(LogSmartFoundations, Log,
				TEXT("ChainActorService: Phase 3 skipping empty TG (%d raw entries, 0 valid) — would create NO_SEGMENTS zombie"),
				TG->Conveyors.Num());
			continue;
		}

		// Pre-migrate backpointer capture disabled for release — re-enable to write ChainDiag_*.json
		// diagnostic dumps when investigating chain zombie failures (40+ files written per upgrade run).

		// PRE-MIGRATION: Sort TG->Conveyors into vanilla tick-group order.
		//
		// MigrateConveyorGroupToChainActor uses SetStartAndEndConveyors(Conveyors.Last(), Conveyors[0]).
		// Healthy vanilla tick groups store belts in reverse item-flow order, so Conveyors[0]
		// must be the OUTPUT-END belt (Connection1 → non-belt) and Conveyors.Last() the
		// INPUT-END belt (Connection0 → non-belt).
		//
		// During mass upgrade, our merge code can assemble groups in arbitrary order. Build a
		// physical input-to-output walk first, then reverse it before migration so vanilla sees
		// the output-to-input array it expects.
		//
		// If we pass input-to-output order, vanilla logs "Last input != mLastConv output" and
		// produces a 0-segment zombie; this was the April 2026 mass-upgrade failure mode.
		if (ValidBeltCount > 1)
		{
			// Find the input-end belt: Connection0 feeds from a non-belt building (or is unconnected).
			AFGBuildableConveyorBase* SortInputEnd = nullptr;
			for (AFGBuildableConveyorBase* B : TG->Conveyors)
			{
				if (!IsValid(B)) continue;
				UFGFactoryConnectionComponent* C0 = B->GetConnection0();
				if (C0 && (!C0->IsConnected() ||
					!Cast<AFGBuildableConveyorBase>(C0->GetConnection()->GetOuterBuildable())))
				{
					SortInputEnd = B;
					break;
				}
			}

			if (SortInputEnd)
			{
				TSet<AFGBuildableConveyorBase*> BeltsInTG;
				for (AFGBuildableConveyorBase* B : TG->Conveyors)
				{
					if (IsValid(B)) BeltsInTG.Add(B);
				}

				TArray<AFGBuildableConveyorBase*> InputToOutput;
				InputToOutput.Reserve(TG->Conveyors.Num());

				AFGBuildableConveyorBase* Current = SortInputEnd;
				while (IsValid(Current) && BeltsInTG.Contains(Current))
				{
					InputToOutput.Add(Current);
					BeltsInTG.Remove(Current);
					UFGFactoryConnectionComponent* Out1 = Current->GetConnection1();
					if (!Out1 || !Out1->IsConnected()) break;
					Current = Cast<AFGBuildableConveyorBase>(Out1->GetConnection()->GetOuterBuildable());
				}
				// Append any belts not reached by the topology walk (disconnected segments).
				for (AFGBuildableConveyorBase* B : BeltsInTG)
				{
					InputToOutput.Add(B);
				}

				if (InputToOutput.Num() == TG->Conveyors.Num())
				{
					TArray<AFGBuildableConveyorBase*> OutputToInput;
					OutputToInput.Reserve(InputToOutput.Num());
					for (int32 Index = InputToOutput.Num() - 1; Index >= 0; --Index)
					{
						OutputToInput.Add(InputToOutput[Index]);
					}
					TG->Conveyors = MoveTemp(OutputToInput);
				}
			}
		}

		// PRE-MIGRATION: Clear live zombie back-pointers on TG belts.
		//
		// After a failed migration cycle, vanilla assigns mConveyorChainActor on the starting
		// belt (Conveyors[0]) even when BuildChain produces 0 segments.  If the zombie is still
		// alive (IsValid == true, GetNumChainSegments() == 0), that belt appears "owned" and
		// subsequent BuildChain traversals skip it.  RemoveChainActorFromConveyorGroup does not
		// clear these back-pointers when the chain has 0 segments (the segment list is empty,
		// so there are no belts to iterate).  Stale pointers to already-destroyed zombies
		// (IsValid == false) are handled by vanilla's IsValid checks internally; we only need
		// to explicitly clear live 0-seg zombies here.
		for (AFGBuildableConveyorBase* B : TG->Conveyors)
		{
			if (!IsValid(B)) continue;
			AFGConveyorChainActor* OldChain = B->GetConveyorChainActor();
			if (IsValid(OldChain) && OldChain->GetNumChainSegments() == 0)
			{
				// Notify via the public setter so the chain-side bookkeeping is updated.
				// The zombie is inert (0 segs) and will be purged by PurgeZombieChainActors.
				B->SetConveyorChainActor(nullptr);
			}
		}

		BuildableSub->MigrateConveyorGroupToChainActor(TG);

		// Post-migrate check: did we produce a valid chain?
		if (TG->ChainActor)
		{
			const int32 NewSegs = TG->ChainActor->GetNumChainSegments();
			if (NewSegs == 0)
			{
				++PostMigrateZombies;

				// DumpFailingTickGroupToJson disabled — writes 40+ ChainDiag_*.json files per upgrade run.
				// Re-enable the call and the pre-migrate capture block above for offline zombie analysis.

				// Root cause: vanilla's MigrateConveyorGroupToChainActor fires
				//   "Last Conveyors[0] != mLastConv Conveyors.Last()"
				// for ANY TG where Conveyors.Num() > 1 (Conveyors[0] != Conveyors.Last()),
				// then early-returns with 0 segments. The chain ACTOR is still created.
				// We recover by identifying the true input-end and output-end belts and
				// calling SetStartAndEndConveyors + BuildChain directly — bypassing the
				// bad Conveyors-ordering assumption in MigrateConveyorGroupToChainActor.

				AFGBuildableConveyorBase* ChainInputEnd  = nullptr; // Connection0 → non-belt building
				AFGBuildableConveyorBase* ChainOutputEnd = nullptr; // Connection1 → non-belt building

				for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
				{
					if (!IsValid(Belt)) continue;
					UFGFactoryConnectionComponent* In0 = Belt->GetConnection0();
					if (In0 && (!In0->IsConnected() ||
						!Cast<AFGBuildableConveyorBase>(In0->GetConnection()->GetOuterBuildable())))
					{
						ChainInputEnd = Belt;
					}
					UFGFactoryConnectionComponent* Out1 = Belt->GetConnection1();
					if (Out1 && (!Out1->IsConnected() ||
						!Cast<AFGBuildableConveyorBase>(Out1->GetConnection()->GetOuterBuildable())))
					{
						ChainOutputEnd = Belt;
					}
				}

				// Migrate's internal BuildChain call (executed before its Conveyors[0] != Conveyors.Last()
				// check) walks Connection1 from mFirstConveyor and immediately hits the destination
				// building for the output-end belt. Before exiting it sets
				//   mFirstConveyor.mConveyorChainActor = zombie
				// (the starting belt always gets its back-pointer assigned even on failed traversals).
				// Subsequent BuildChain calls then find that belt "owned" and refuse to cross it.
				//
				// Fix: use RemoveChainActorFromConveyorGroup (which clears every belt's back-pointer
				// and nulls TG->ChainActor) then immediately restore TG->ChainActor to the zombie so
				// BuildChain's traversal sees all belts as free. mConveyorChainActor is protected so
				// we cannot clear it directly — but RemoveChainActorFromConveyorGroup does exactly that.
				AFGConveyorChainActor* ZombieToRecover = TG->ChainActor;
				BuildableSub->RemoveChainActorFromConveyorGroup(TG);  // clears belt back-pointers + nulls TG->ChainActor
				TG->ChainActor = ZombieToRecover;                     // restore so SetStartAndEnd + BuildChain can use it

				bool bRecovered = false;
				if (ChainInputEnd && ChainOutputEnd && ZombieToRecover)
				{
					// SetStartAndEndConveyors(arg1, arg2):
					//   arg1 -> traversal origin
					//   arg2 -> walk-terminus check target
					// BuildChain walks from mFirstConveyor via Connection1 (item-flow
					// direction) until it hits a non-belt building. That terminus is then
					// checked against mLastConveyor — they MUST match or vanilla emits
					// "Last X != mLastConv Y" and the chain ends up with 0 segments.
					//
					// Therefore:
					//   arg1 = input-end  (conn0 -> non-belt)  <- walk starts here
					//   arg2 = output-end (conn1 -> non-belt)  <- walk terminates here
					ZombieToRecover->SetStartAndEndConveyors(ChainInputEnd, ChainOutputEnd);
					ZombieToRecover->BuildChain();
					if (ZombieToRecover->GetNumChainSegments() > 0)
					{
						bRecovered = true;
						UE_LOG(LogSmartFoundations, Log,
							TEXT("ChainActorService: Recovered zombie via SetStartAndEnd+BuildChain — %d seg(s)"),
							ZombieToRecover->GetNumChainSegments());
					}
					else
					{
						UE_LOG(LogSmartFoundations, Warning,
							TEXT("ChainActorService: Zombie BuildChain still failed after belt-clear — InputEnd=%s OutputEnd=%s Belts=%d Details=%s"),
							*ChainInputEnd->GetName(),
							*ChainOutputEnd->GetName(),
							ValidBeltCount,
							*DescribeTickGroupForLog(TG));
					}
				}
				else
				{
					UE_LOG(LogSmartFoundations, Warning,
						TEXT("ChainActorService: Zombie endpoint detection incomplete — InputEnd=%s OutputEnd=%s Belts=%d Details=%s"),
						ChainInputEnd ? *ChainInputEnd->GetName() : TEXT("null"),
						ChainOutputEnd ? *ChainOutputEnd->GetName() : TEXT("null"),
						ValidBeltCount,
						*DescribeTickGroupForLog(TG));
				}

				if (!bRecovered)
				{
					++FailedRecoveryGroups;
					// Fallback: detach zombie from the subsystem and re-queue the tick group
					// for vanilla to retry on the next frame. Do NOT call Destroy() here —
					// doing so races AFGConveyorChainActor::Factory_Tick on a ParallelFor
					// worker thread (EXCEPTION_ACCESS_VIOLATION; see header "THINGS THAT
					// LOOK LIKE SHORTCUTS"). The zombie already has 0 segments (cleared by
					// RemoveChainActorFromConveyorGroup above) and will be swept by the
					// deferred PurgeZombieChainActors call 3 s after the upgrade, which
					// runs safely in TG_PrePhysics before TickFactoryActors (TG_PostPhysics).
					UE_LOG(LogSmartFoundations, Warning,
						TEXT("ChainActorService: Recovery failed for zombie TG=%p ForceShared=%s Belts=%d — detaching and re-queuing. Details=%s"),
						TG,
						TG->ForceIntoSharedByBuildable ? *TG->ForceIntoSharedByBuildable->GetName() : TEXT("null"),
						ValidBeltCount,
						*DescribeTickGroupForLog(TG));
					BuildableSub->RemoveConveyorChainActor(ZombieToRecover);
					BuildableSub->mConveyorGroupsPendingChainActors.AddUnique(TG);
				}
			}
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("ChainActorService: Phase 3 migrate call left TG->ChainActor null (TG had %d valid belt(s))"),
				ValidBeltCount);
		}
		++MigrateCount;
	}

	// Phase 4: Detach the original affected chain actors from the subsystem.
	//
	// Phase 2 already called RemoveChainActorFromConveyorGroup on every owning group,
	// which nulled TG->ChainActor AND cleared every belt's mConveyorChainActor back-pointer
	// AND zeroed the chain actor's own segment list (GetNumChainSegments() == 0 after Phase 2).
	// That makes the old chains inert 0-segment zombies that TickFactoryActors will never
	// pick up again (they are not in any TG->ChainActor pointer).
	//
	// We do NOT call Chain->Destroy() here. Doing so races AFGConveyorChainActor::Factory_Tick
	// on a ParallelFor worker thread and causes EXCEPTION_ACCESS_VIOLATION (see header comment
	// "THINGS THAT LOOK LIKE SHORTCUTS"). The precondition that "upgrade runs outside the
	// ParallelFor window" is NOT reliably satisfied when StartUpgrade is invoked from Slate
	// input processing, which can overlap with a non-blocking TickFactoryActors dispatch.
	//
	// Instead, the deferred PurgeZombieChainActors call (scheduled 3 s after the upgrade via
	// ScheduleDeferredZombiePurge) finds these 0-segment actors via TActorIterator and
	// destroys them safely. The deferred timer fires in TG_PrePhysics, which completes before
	// AFGBuildableSubsystem::Tick (TG_PostPhysics) dispatches the next TickFactoryActors —
	// guaranteed no parallel worker is touching the actor when Destroy() runs.
	int32 DetachedCount = 0;
	int32 DefensiveTGsCleared = 0;
	for (AFGConveyorChainActor* Chain : AffectedChains)
	{
		if (!IsValid(Chain)) continue;

		// Defensive re-scan: null TG->ChainActor on any group still pointing at this chain.
		// Phase 2 should have handled the owning group, but multiple TGs pointing at one
		// chain is unusual yet possible. Cheap to iterate; cannot hurt.
		for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
		{
			if (TG && TG->ChainActor == Chain)
			{
				BuildableSub->RemoveChainActorFromConveyorGroup(TG);
				++DefensiveTGsCleared;
			}
		}

		// Remove from subsystem's actor list so TickFactoryActors cannot reach it.
		// The actor remains in the world as a 0-segment zombie; the deferred purge
		// calls Destroy() on it in a safe timer context 3 seconds from now.
		BuildableSub->RemoveConveyorChainActor(Chain);
		++DetachedCount;
	}

	// Phase 5: Verify every affected group is internally consistent before returning.
	// The April 2026 steel-output repro reached this point with a fully connected
	// belt -> belt -> lift -> belt run sitting in TG->ChainActor == null. Re-queueing
	// the group was not enough; if this happens again, make it visible immediately in
	// FactoryGame.log with the exact topology instead of requiring external inspection.
	int32 PostRebuildUnassignedGroups = 0;
	int32 PostRebuildZeroSegmentGroups = 0;
	int32 PostRebuildBackPointerMismatchGroups = 0;
	int32 PostRebuildLoggedGroups = 0;
	const int32 MaxPostRebuildDetailLogs = 12;

	for (FConveyorTickGroup* TG : GroupsToMigrate)
	{
		if (!TG) continue;

		int32 LiveConveyors = 0;
		bool bBackPointerMismatch = false;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (!IsValid(Belt) || Belt->IsActorBeingDestroyed()) continue;
			++LiveConveyors;

			if (TG->ChainActor && Belt->GetConveyorChainActor() != TG->ChainActor)
			{
				bBackPointerMismatch = true;
			}
		}

		if (LiveConveyors == 0) continue;

		const bool bUnassigned = (TG->ChainActor == nullptr);
		const bool bZeroSegments = TG->ChainActor && TG->ChainActor->GetNumChainSegments() == 0;

		if (bUnassigned) ++PostRebuildUnassignedGroups;
		if (bZeroSegments) ++PostRebuildZeroSegmentGroups;
		if (bBackPointerMismatch) ++PostRebuildBackPointerMismatchGroups;

		if ((bUnassigned || bZeroSegments || bBackPointerMismatch) && PostRebuildLoggedGroups < MaxPostRebuildDetailLogs)
		{
			++PostRebuildLoggedGroups;
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("ChainActorService: Post-rebuild invariant failure — unassigned=%s zero_segments=%s backptr_mismatch=%s live_conveyors=%d Details=%s"),
				bUnassigned ? TEXT("true") : TEXT("false"),
				bZeroSegments ? TEXT("true") : TEXT("false"),
				bBackPointerMismatch ? TEXT("true") : TEXT("false"),
				LiveConveyors,
				*DescribeTickGroupForLog(TG));
		}
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("ChainActorService: chains=%d invalid=%d explicit_tgs=%d groups_cleared=%d groups_merged=%d groups_migrated=%d skipped_empty=%d deferred_dirty_groups=%d deferred_dirty_conveyors=%d zombies=%d failed_recovery=%d orphan_skipped=%d detached=%d defensive_tgs_cleared=%d post_unassigned=%d post_zero_segments=%d post_backptr_mismatch=%d"),
		AffectedChains.Num(), InvalidChainCount, ExplicitGroupCount, GroupsToRemove.Num(), MergedAwayGroups.Num(), MigrateCount, SkippedEmptyGroups, DeferredDirtyGroups, DeferredDirtyConveyors, PostMigrateZombies, FailedRecoveryGroups, OrphanCount, DetachedCount, DefensiveTGsCleared, PostRebuildUnassignedGroups, PostRebuildZeroSegmentGroups, PostRebuildBackPointerMismatchGroups);

	return MigrateCount;
}

int32 USFChainActorService::PurgeZombieChainActors()
{
	AFGBuildableSubsystem* BuildableSub = GetBuildableSubsystem();
	if (!BuildableSub) return 0;

	UWorld* World = BuildableSub->GetWorld();
	if (!World) return 0;

	// Collect all 0-segment chain actors. These are inert zombies left behind when
	// RemoveChainActorFromConveyorGroup clears belt back-pointers but the actor is not
	// destroyed. If serialised to a save they cause an assertion crash on reload:
	// Factory_Tick:614 → ForceDestroyChainActor fires from a ParallelFor worker thread
	// (assertion: GTestRegisterComponentTickFunctions == 0 fails on worker threads).
	TArray<AFGConveyorChainActor*> Zombies;
	for (TActorIterator<AFGConveyorChainActor> It(World); It; ++It)
	{
		AFGConveyorChainActor* Chain = *It;
		if (!Chain || !IsValid(Chain)) continue;
		if (Chain->GetNumChainSegments() == 0)
			Zombies.Add(Chain);
	}

	if (Zombies.Num() == 0) return 0;

	int32 PurgeCount = 0;
	for (AFGConveyorChainActor* Chain : Zombies)
	{
		if (!IsValid(Chain)) continue;

		// Step 1: Null out TG->ChainActor for any tick group still pointing at this zombie.
		// RemoveChainActorFromConveyorGroup is the only API that nulls TG->ChainActor;
		// RemoveConveyorChainActor alone does NOT. Multiple groups pointing to the same
		// zombie is unusual but we iterate all to be safe.
		for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
		{
			if (TG && TG->ChainActor == Chain)
			{
				BuildableSub->RemoveChainActorFromConveyorGroup(TG);
			}
		}

		// Step 2: Remove from the subsystem's actor list.
		BuildableSub->RemoveConveyorChainActor(Chain);

		// Step 3: Destroy the actor. Safe because:
		//   - TG->ChainActor is now null so no ParallelFor worker can reach this actor.
		//   - This runs on the game thread, between frames, outside the ParallelFor window.
		//   - Destruction removes the actor from the save, preventing the load-time crash.
		Chain->Destroy();

		++PurgeCount;
	}

	return PurgeCount;
}

int32 USFChainActorService::RepairSplitChains()
{
	AFGBuildableSubsystem* BuildableSub = GetBuildableSubsystem();
	if (!BuildableSub) return 0;

	UWorld* World = BuildableSub->GetWorld();
	if (!World) return 0;

	// Build a conveyor -> chain map for all live chain actors.
	TMap<AFGBuildableConveyorBase*, AFGConveyorChainActor*> BeltToChain;
	for (TActorIterator<AFGConveyorChainActor> It(World); It; ++It)
	{
		AFGConveyorChainActor* Chain = *It;
		if (!Chain || !IsValid(Chain)) continue;
		for (const FConveyorChainSplineSegment& Seg : Chain->GetChainSegments())
		{
			if (Seg.ConveyorBase) BeltToChain.Add(Seg.ConveyorBase, Chain);
		}
	}

	// Detect SPLIT_CHAIN: conveyor A's output feeds conveyor B but they are in different chains.
	TSet<AFGConveyorChainActor*> ChainsToRepair;
	for (TActorIterator<AFGConveyorChainActor> It(World); It; ++It)
	{
		AFGConveyorChainActor* Chain = *It;
		if (!Chain || !IsValid(Chain) || Chain->GetNumChainSegments() == 0) continue;

		for (const FConveyorChainSplineSegment& Seg : Chain->GetChainSegments())
		{
			AFGBuildableConveyorBase* Belt = Seg.ConveyorBase;
			if (!Belt || !IsValid(Belt)) continue;
			UFGFactoryConnectionComponent* OutConn = Belt->GetConnection1();
			if (!OutConn || !OutConn->IsConnected()) continue;
			UFGFactoryConnectionComponent* PeerConn = OutConn->GetConnection();
			if (!PeerConn) continue;

			AFGBuildableConveyorBase* Downstream =
				Cast<AFGBuildableConveyorBase>(PeerConn->GetOuterBuildable());
			if (!IsValid(Downstream)) continue;
			if (Belt->GetLevel() != Downstream->GetLevel()) continue;

			AFGConveyorChainActor* const* DownstreamChainPtr = BeltToChain.Find(Downstream);
			if (!DownstreamChainPtr) continue;
			AFGConveyorChainActor* DownstreamChain = *DownstreamChainPtr;
			if (DownstreamChain == Chain) continue;

			// Downstream conveyor belongs to a different chain — this is a SPLIT_CHAIN.
			ChainsToRepair.Add(Chain);
			ChainsToRepair.Add(DownstreamChain);
		}
	}

	if (ChainsToRepair.Num() == 0) return 0;

	UE_LOG(LogSmartFoundations, Log,
		TEXT("ChainActorService: RepairSplitChains found %d chain(s) involved in SPLIT_CHAIN — rebuilding"),
		ChainsToRepair.Num());

	// InvalidateAndRebuildChains now includes Phase 2.5, which merges the split groups.
	return InvalidateAndRebuildChains(ChainsToRepair);
}

int32 USFChainActorService::RepairOrphanedBelts(FSFChainRepairResult* OutResult)
{
	// EXPLICIT TRIAGE MODE (2026-04-25).
	//
	// Earlier bounce-based repair crashed the game four different ways:
	//   1. Auto-bounce after upgrade          → Factory_Tick dangling belt
	//   2. Batched manual bounce (68 at once) → Factory_Tick dangling belt
	//   3. Throttled bounce (18/68 @ 0.1s)    → TickFactoryActors:794 null deref
	//   4. Throttled bounce + TG prune (3/68) → RemoveConveyor:1886 assertion
	//      (our RemoveAll on mConveyorTickGroup invalidated vanilla's cached
	//       per-belt bucketID, which RemoveConveyor uses as a direct index.)
	//
	// Do not restore those variants. This path now uses the validated vanilla
	// re-registration sequence (RemoveConveyor/AddConveyor on live conveyors)
	// only when the player explicitly presses Repair after the save has loaded
	// and the world is stable. Load-time and deferred post-upgrade automation
	// must remain diagnostic/zombie-purge only.

	AFGBuildableSubsystem* BuildableSub = GetBuildableSubsystem();
	if (!BuildableSub) return 0;

	int32 OrphanedTGs = 0;
	int32 EmptyTGs = 0;
	int32 BeltsAcrossTGs = 0;
	int32 CandidateBelts = 0;
	TArray<FString> CandidateNames;

	auto EscapeJsonString = [](const FString& In) -> FString
	{
		FString Out = In;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Out.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Out.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		return Out;
	};

	auto DescribeConnection = [&EscapeJsonString](UFGFactoryConnectionComponent* Conn) -> FString
	{
		if (!Conn) return TEXT("null");
		UFGFactoryConnectionComponent* Peer = Conn->GetConnection();
		if (!Peer) return TEXT("\"unconnected\"");

		AFGBuildable* PeerBuildable = Peer->GetOuterBuildable();
		const FString PeerName = PeerBuildable ? PeerBuildable->GetName() : TEXT("unknown");
		const bool bPeerIsConveyor = Cast<AFGBuildableConveyorBase>(PeerBuildable) != nullptr;
		return FString::Printf(TEXT("{\"peer\":\"%s\",\"peer_is_conveyor\":%s}"),
			*EscapeJsonString(PeerName),
			bPeerIsConveyor ? TEXT("true") : TEXT("false"));
	};

	FString OrphanReport;
	OrphanReport.Reserve(8192);
	const FDateTime ReportTime = FDateTime::Now();
	OrphanReport += TEXT("{\n");
	OrphanReport += FString::Printf(TEXT("  \"timestamp\": \"%s\",\n"), *ReportTime.ToIso8601());
	OrphanReport += TEXT("  \"guidance\": \"Explicit triage repair re-registers live conveyors from orphaned tick groups after the world is loaded and stable. Save after repair, reload, and re-run Detect.\",\n");
	OrphanReport += TEXT("  \"orphaned_tick_groups\": [\n");
	bool bFirstOrphanGroup = true;
	TArray<AFGBuildableConveyorBase*> BeltsToReRegister;
	TSet<AFGBuildableConveyorBase*> SeenBeltsToReRegister;

	for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
	{
		if (!TG || TG->ChainActor) continue;
		++OrphanedTGs;
		if (TG->Conveyors.Num() == 0) { ++EmptyTGs; continue; }

		bool bFoundCandidate = false;
		FString RepresentativeBeltName;
		int32 LiveBeltsInGroup = 0;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (!IsValid(Belt) || Belt->IsActorBeingDestroyed()) continue;
			++LiveBeltsInGroup;
			++BeltsAcrossTGs;
			if (!SeenBeltsToReRegister.Contains(Belt))
			{
				SeenBeltsToReRegister.Add(Belt);
				BeltsToReRegister.Add(Belt);
			}
			if (Cast<AFGBuildableConveyorLift>(Belt)) continue;

			const bool bConnected =
				(Belt->GetConnection0() && Belt->GetConnection0()->IsConnected()) ||
				(Belt->GetConnection1() && Belt->GetConnection1()->IsConnected());
			if (bConnected && !bFoundCandidate)
			{
				++CandidateBelts;
				RepresentativeBeltName = Belt->GetName();
				CandidateNames.Add(RepresentativeBeltName);
				bFoundCandidate = true;
			}
		}

		if (bFoundCandidate)
		{
			if (!bFirstOrphanGroup)
			{
				OrphanReport += TEXT(",\n");
			}
			bFirstOrphanGroup = false;

			const int32 TickGroupIndex = BuildableSub->mConveyorTickGroup.IndexOfByKey(TG);
			OrphanReport += TEXT("    {\n");
			OrphanReport += FString::Printf(TEXT("      \"tick_group_index\": %d,\n"), TickGroupIndex);
			OrphanReport += FString::Printf(TEXT("      \"tick_group_ptr\": \"0x%p\",\n"), TG);
			OrphanReport += FString::Printf(TEXT("      \"live_belts\": %d,\n"), LiveBeltsInGroup);
			OrphanReport += FString::Printf(TEXT("      \"representative_belt\": \"%s\",\n"), *EscapeJsonString(RepresentativeBeltName));
			OrphanReport += TEXT("      \"conveyors\": [\n");

			bool bFirstBelt = true;
			for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
			{
				if (!IsValid(Belt) || Belt->IsActorBeingDestroyed()) continue;
				if (!bFirstBelt)
				{
					OrphanReport += TEXT(",\n");
				}
				bFirstBelt = false;

				OrphanReport += TEXT("        {\n");
				OrphanReport += FString::Printf(TEXT("          \"name\": \"%s\",\n"), *EscapeJsonString(Belt->GetName()));
				OrphanReport += FString::Printf(TEXT("          \"is_lift\": %s,\n"), Cast<AFGBuildableConveyorLift>(Belt) ? TEXT("true") : TEXT("false"));
				OrphanReport += FString::Printf(TEXT("          \"bucket_id\": %d,\n"), Belt->GetConveyorBucketID());
				OrphanReport += FString::Printf(TEXT("          \"level\": %d,\n"), Belt->GetLevel());
				OrphanReport += FString::Printf(TEXT("          \"chain_actor_backptr\": \"%s\",\n"),
					Belt->GetConveyorChainActor() ? *EscapeJsonString(Belt->GetConveyorChainActor()->GetName()) : TEXT("null"));
				OrphanReport += FString::Printf(TEXT("          \"conn0\": %s,\n"), *DescribeConnection(Belt->GetConnection0()));
				OrphanReport += FString::Printf(TEXT("          \"conn1\": %s\n"), *DescribeConnection(Belt->GetConnection1()));
				OrphanReport += TEXT("        }");
			}

			OrphanReport += TEXT("\n      ]\n");
			OrphanReport += TEXT("    }");
		}
	}
	OrphanReport += TEXT("\n  ]\n");
	OrphanReport += TEXT("}\n");

	FString ReportPath;
	if (CandidateBelts > 0)
	{
		const FString Stamp = ReportTime.ToString(TEXT("%Y%m%d_%H%M%S")) + FString::Printf(TEXT("_%03d"), ReportTime.GetMillisecond());
		ReportPath = FPaths::ProjectLogDir() / FString::Printf(TEXT("SmartUpgrade_OrphanTickGroups_%s.json"), *Stamp);
		if (FFileHelper::SaveStringToFile(OrphanReport, *ReportPath))
		{
			UE_LOG(LogSmartFoundations, Display,
				TEXT("ChainActorService: orphan tick group report written -> %s"), *ReportPath);
		}
		else
		{
			UE_LOG(LogSmartFoundations, Warning,
				TEXT("ChainActorService: failed to write orphan tick group report to %s"), *ReportPath);
			ReportPath.Reset();
		}
	}

	int32 ReRegisteredBelts = 0;
	int32 QueuedGroups = 0;
	if (BeltsToReRegister.Num() > 0)
	{
		ReRegisteredBelts = BeltsToReRegister.Num();
		QueuedGroups = ReRegisterAndQueueVanillaRebuildForBelts(BeltsToReRegister, {});
	}

	UE_LOG(LogSmartFoundations, Display,
		TEXT("ChainActorService: RepairOrphanedBelts TRIAGE — orphaned_tgs=%d empty_tgs=%d live_belts=%d candidate_belts=%d reregistered_belts=%d queued_groups=%d report=%s. Save/reload and re-run Detect after repair."),
		OrphanedTGs, EmptyTGs, BeltsAcrossTGs, CandidateBelts, ReRegisteredBelts, QueuedGroups, ReportPath.IsEmpty() ? TEXT("none") : *ReportPath);

	if (OutResult)
	{
		OutResult->OrphanedTickGroupCount = OrphanedTGs;
		OutResult->EmptyOrphanedTickGroupCount = EmptyTGs;
		OutResult->LiveBeltsInOrphanedTickGroups = BeltsAcrossTGs;
		OutResult->OrphanedBeltCandidates = CandidateBelts;
		OutResult->OrphanedBeltsRequeued = ReRegisteredBelts;
		OutResult->OrphanedBeltCandidateNames = CandidateNames;
		OutResult->OrphanedBeltReportPath = ReportPath;
	}

	return ReRegisteredBelts;
}

void USFChainActorService::ProcessNextPendingBounce()
{
	// Retained for ABI stability; bounce queue is never populated in diagnostic mode.
	PendingBounceQueue.Reset();
	if (USFSubsystem* Sub = Subsystem.Get())
	{
		if (UWorld* World = Sub->GetWorld())
		{
			World->GetTimerManager().ClearTimer(BounceTimerHandle);
		}
	}
}

void USFChainActorService::RunPostLoadRepair()
{
	const FSFChainDiagnosticResult Result = DetectChainActorIssues();

	if (Result.HasIssues())
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("ChainActorService: Post-load chain diagnostic found issues but did not mutate conveyor state — zombies=%d split_chains=%d orphaned_tgs=%d tg_backptr_mismatches=%d. Use Smart Upgrade Triage or targeted tooling after the world is stable."),
			Result.ZombieChainCount,
			Result.SplitChainCount,
			Result.OrphanedTickGroupCount,
			Result.TickGroupBackPointerMismatchCount);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Log,
			TEXT("ChainActorService: Post-load chain diagnostic complete — no issues detected."));
	}
}

int32 USFChainActorService::InvalidateAndRebuildForBelts(
	const TArray<AFGBuildableConveyorBase*>& Belts,
	const TSet<AFGConveyorChainActor*>& ExtraChains)
{
	TSet<AFGConveyorChainActor*> AffectedChains;
	TSet<FConveyorTickGroup*> ExplicitTickGroups;
	AFGBuildableSubsystem* BuildableSub = GetBuildableSubsystem();

	for (AFGConveyorChainActor* Chain : ExtraChains)
	{
		if (Chain) AffectedChains.Add(Chain);
	}

	for (AFGBuildableConveyorBase* Belt : Belts)
	{
		if (!IsValid(Belt)) continue;

		if (BuildableSub)
		{
			FConveyorTickGroup* BeltGroup = nullptr;
			const int32 BucketID = Belt->GetConveyorBucketID();
			if (BuildableSub->mConveyorTickGroup.IsValidIndex(BucketID))
			{
				FConveyorTickGroup* CandidateGroup = BuildableSub->mConveyorTickGroup[BucketID];
				if (CandidateGroup && CandidateGroup->Conveyors.Contains(Belt))
				{
					BeltGroup = CandidateGroup;
				}
			}

			if (!BeltGroup)
			{
				for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
				{
					if (TG && TG->Conveyors.Contains(Belt))
					{
						BeltGroup = TG;
						break;
					}
				}
			}

			if (BeltGroup)
			{
				ExplicitTickGroups.Add(BeltGroup);
			}
		}

		if (AFGConveyorChainActor* Chain = Belt->GetConveyorChainActor())
		{
			AffectedChains.Add(Chain);
		}

		// Collect neighbours' chains too — they may reference stale topology after
		// the belt swap.
		for (UFGFactoryConnectionComponent* Conn : { Belt->GetConnection0(), Belt->GetConnection1() })
		{
			if (!Conn || !Conn->GetConnection()) continue;
			AFGBuildableConveyorBase* Neighbor = Cast<AFGBuildableConveyorBase>(Conn->GetConnection()->GetOwner());
			if (!Neighbor) continue;
			if (AFGConveyorChainActor* NChain = Neighbor->GetConveyorChainActor())
			{
				AffectedChains.Add(NChain);
			}
		}
	}

	return InvalidateAndRebuildChains(AffectedChains, ExplicitTickGroups);
}

int32 USFChainActorService::InvalidateAndQueueVanillaRebuildForBelts(
	const TArray<AFGBuildableConveyorBase*>& Belts,
	const TSet<AFGConveyorChainActor*>& ExtraChains)
{
	AFGBuildableSubsystem* BuildableSub = GetBuildableSubsystem();
	if (!BuildableSub)
	{
		return 0;
	}

	TSet<FConveyorTickGroup*> GroupsToQueue;
	TSet<AFGConveyorChainActor*> AffectedChains;

	for (AFGConveyorChainActor* Chain : ExtraChains)
	{
		if (Chain)
		{
			AffectedChains.Add(Chain);
		}
	}

	for (AFGBuildableConveyorBase* Belt : Belts)
	{
		if (!IsValid(Belt)) continue;

		const int32 BucketID = Belt->GetConveyorBucketID();
		if (BuildableSub->mConveyorTickGroup.IsValidIndex(BucketID))
		{
			FConveyorTickGroup* CandidateGroup = BuildableSub->mConveyorTickGroup[BucketID];
			if (CandidateGroup && CandidateGroup->Conveyors.Contains(Belt))
			{
				GroupsToQueue.Add(CandidateGroup);
			}
		}

		if (AFGConveyorChainActor* Chain = Belt->GetConveyorChainActor())
		{
			AffectedChains.Add(Chain);
		}

		for (UFGFactoryConnectionComponent* Conn : { Belt->GetConnection0(), Belt->GetConnection1() })
		{
			if (!Conn || !Conn->IsConnected()) continue;
			UFGFactoryConnectionComponent* Other = Conn->GetConnection();
			if (!Other) continue;

			AFGBuildableConveyorBase* OtherBelt = Cast<AFGBuildableConveyorBase>(Other->GetOuterBuildable());
			if (!IsValid(OtherBelt)) continue;

			const int32 OtherBucketID = OtherBelt->GetConveyorBucketID();
			if (BuildableSub->mConveyorTickGroup.IsValidIndex(OtherBucketID))
			{
				FConveyorTickGroup* OtherGroup = BuildableSub->mConveyorTickGroup[OtherBucketID];
				if (OtherGroup && OtherGroup->Conveyors.Contains(OtherBelt))
				{
					GroupsToQueue.Add(OtherGroup);
				}
			}

			if (AFGConveyorChainActor* OtherChain = OtherBelt->GetConveyorChainActor())
			{
				AffectedChains.Add(OtherChain);
			}
		}
	}

	for (AFGConveyorChainActor* Chain : AffectedChains)
	{
		if (!Chain || !IsValid(Chain)) continue;
		if (FConveyorTickGroup* TG = FindOwningTickGroup(BuildableSub, Chain))
		{
			GroupsToQueue.Add(TG);
		}
	}

	for (FConveyorTickGroup* TG : BuildableSub->mConveyorGroupsPendingChainActors)
	{
		if (TG)
		{
			GroupsToQueue.Add(TG);
		}
	}

	int32 QueuedGroupBackPointerMismatches = 0;
	for (FConveyorTickGroup* TG : GroupsToQueue)
	{
		if (!TG) continue;

		if (TG->ChainActor)
		{
			AffectedChains.Add(TG->ChainActor);
		}

		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (!IsValid(Belt)) continue;

			if (AFGConveyorChainActor* BeltChain = Belt->GetConveyorChainActor())
			{
				AffectedChains.Add(BeltChain);
				if (TG->ChainActor && BeltChain != TG->ChainActor)
				{
					++QueuedGroupBackPointerMismatches;
				}
			}
		}
	}

	int32 ClearedGroups = 0;
	int32 ClearedBackPointers = 0;
	for (FConveyorTickGroup* TG : GroupsToQueue)
	{
		if (!TG) continue;
		if (TG->ChainActor)
		{
			BuildableSub->RemoveChainActorFromConveyorGroup(TG);
			++ClearedGroups;
		}

		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (!IsValid(Belt)) continue;
			if (Belt->GetConveyorChainActor())
			{
				Belt->SetConveyorChainActor(nullptr);
				++ClearedBackPointers;
			}
		}
	}

	int32 DetachedChains = 0;
	for (AFGConveyorChainActor* Chain : AffectedChains)
	{
		if (!IsValid(Chain)) continue;

		for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
		{
			if (TG && TG->ChainActor == Chain)
			{
				BuildableSub->RemoveChainActorFromConveyorGroup(TG);
				GroupsToQueue.Add(TG);
				BuildableSub->mConveyorGroupsPendingChainActors.AddUnique(TG);
				++ClearedGroups;
			}
		}

		BuildableSub->RemoveConveyorChainActor(Chain);
		++DetachedChains;
	}

	TMap<AFGBuildableConveyorBase*, FConveyorTickGroup*> BeltToGroup;
	for (FConveyorTickGroup* TG : GroupsToQueue)
	{
		if (!TG) continue;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (IsValid(Belt))
			{
				BeltToGroup.Add(Belt, TG);
			}
		}
	}

	auto GetConnectedConveyor = [](UFGFactoryConnectionComponent* Conn) -> AFGBuildableConveyorBase*
	{
		if (!Conn || !Conn->IsConnected()) return nullptr;
		UFGFactoryConnectionComponent* Other = Conn->GetConnection();
		return Other ? Cast<AFGBuildableConveyorBase>(Other->GetOuterBuildable()) : nullptr;
	};

	auto SortGroupOutputToInput = [&GetConnectedConveyor](FConveyorTickGroup* TG) -> bool
	{
		if (!TG || TG->Conveyors.Num() <= 1) return true;

		TSet<AFGBuildableConveyorBase*> BeltsInGroup;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (!IsValid(Belt)) return false;
			BeltsInGroup.Add(Belt);
		}

		AFGBuildableConveyorBase* InputEnd = nullptr;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			AFGBuildableConveyorBase* Upstream = GetConnectedConveyor(Belt->GetConnection0());
			if (!Upstream || !BeltsInGroup.Contains(Upstream))
			{
				InputEnd = Belt;
				break;
			}
		}

		if (!InputEnd) return false;

		TArray<AFGBuildableConveyorBase*> InputToOutput;
		InputToOutput.Reserve(TG->Conveyors.Num());

		AFGBuildableConveyorBase* Current = InputEnd;
		while (IsValid(Current) && BeltsInGroup.Contains(Current))
		{
			InputToOutput.Add(Current);
			BeltsInGroup.Remove(Current);

			AFGBuildableConveyorBase* Downstream = GetConnectedConveyor(Current->GetConnection1());
			Current = Downstream;
		}

		if (InputToOutput.Num() != TG->Conveyors.Num()) return false;

		TArray<AFGBuildableConveyorBase*> OutputToInput;
		OutputToInput.Reserve(InputToOutput.Num());
		for (int32 Index = InputToOutput.Num() - 1; Index >= 0; --Index)
		{
			OutputToInput.Add(InputToOutput[Index]);
		}

		TG->Conveyors = MoveTemp(OutputToInput);
		return true;
	};

	int32 CoalescedGroups = 0;
	int32 CoalescedBelts = 0;
	bool bMergedAnyGroup = true;
	while (bMergedAnyGroup)
	{
		bMergedAnyGroup = false;
		for (FConveyorTickGroup* TG : GroupsToQueue)
		{
			if (!TG || TG->Conveyors.Num() == 0) continue;

			const TArray<AFGBuildableConveyorBase*> BeltSnapshot = TG->Conveyors;
			for (AFGBuildableConveyorBase* Belt : BeltSnapshot)
			{
				if (!IsValid(Belt)) continue;

				AFGBuildableConveyorBase* Downstream = GetConnectedConveyor(Belt->GetConnection1());
				if (!IsValid(Downstream)) continue;
				if (Belt->GetLevel() != Downstream->GetLevel()) continue;

				FConveyorTickGroup** OtherGroupPtr = BeltToGroup.Find(Downstream);
				if (!OtherGroupPtr) continue;

				FConveyorTickGroup* OtherGroup = *OtherGroupPtr;
				if (!OtherGroup || OtherGroup == TG || OtherGroup->Conveyors.Num() == 0) continue;
				if (!GroupsToQueue.Contains(OtherGroup)) continue;

				const int32 TGBucketIndex = BuildableSub->mConveyorTickGroup.IndexOfByKey(TG);
				if (TGBucketIndex == INDEX_NONE) continue;

				const TArray<AFGBuildableConveyorBase*> OtherBelts = OtherGroup->Conveyors;
				TArray<AFGBuildableConveyorBase*> MergedConveyors = TG->Conveyors;
				MergedConveyors.Append(OtherGroup->Conveyors);
				TG->Conveyors = MoveTemp(MergedConveyors);

				for (AFGBuildableConveyorBase* MovedBelt : OtherBelts)
				{
					if (!IsValid(MovedBelt)) continue;
					MovedBelt->SetConveyorBucketID(TGBucketIndex);
					BeltToGroup.Add(MovedBelt, TG);
					++CoalescedBelts;
				}

				OtherGroup->Conveyors.Empty();
				OtherGroup->ChainActor = nullptr;
				BuildableSub->mConveyorGroupsPendingChainActors.Remove(OtherGroup);

				++CoalescedGroups;
				bMergedAnyGroup = true;
				break;
			}

			if (bMergedAnyGroup) break;
		}
	}

	int32 FinalQueuedGroups = 0;
	int32 MultiConveyorGroups = 0;
	int32 SortedGroups = 0;
	int32 UnsortedGroups = 0;
	int32 LiftBeltsInQueuedGroups = 0;
	int32 NormalizedBucketAssignments = 0;
	int32 DiagnosticSamplesLogged = 0;
	for (FConveyorTickGroup* TG : GroupsToQueue)
	{
		if (!TG || TG->Conveyors.Num() == 0) continue;
		const bool bMultiConveyorGroup = TG->Conveyors.Num() > 1;
		if (bMultiConveyorGroup)
		{
			++MultiConveyorGroups;
		}

		const bool bSorted = SortGroupOutputToInput(TG);
		if (bSorted)
		{
			++SortedGroups;
		}
		else
		{
			++UnsortedGroups;
		}

		const int32 TGBucketIndex = BuildableSub->mConveyorTickGroup.IndexOfByKey(TG);
		if (TGBucketIndex != INDEX_NONE)
		{
			for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
			{
				if (IsValid(Belt))
				{
					if (Belt->GetConveyorBucketID() != TGBucketIndex)
					{
						++NormalizedBucketAssignments;
					}
					Belt->SetConveyorBucketID(TGBucketIndex);
					if (Cast<AFGBuildableConveyorLift>(Belt))
					{
						++LiftBeltsInQueuedGroups;
					}
				}
			}
		}

		if (bMultiConveyorGroup && DiagnosticSamplesLogged < 12)
		{
			AFGBuildableConveyorBase* First = TG->Conveyors.Num() > 0 ? TG->Conveyors[0] : nullptr;
			AFGBuildableConveyorBase* Last = TG->Conveyors.Num() > 0 ? TG->Conveyors.Last() : nullptr;
			UE_LOG(LogSmartFoundations, Log,
				TEXT("ChainActorService: queued group sample - bucket=%d conveyors=%d sorted=%s first=%s first_bucket=%d last=%s last_bucket=%d"),
				TGBucketIndex,
				TG->Conveyors.Num(),
				bSorted ? TEXT("true") : TEXT("false"),
				IsValid(First) ? *First->GetName() : TEXT("null"),
				IsValid(First) ? First->GetConveyorBucketID() : INDEX_NONE,
				IsValid(Last) ? *Last->GetName() : TEXT("null"),
				IsValid(Last) ? Last->GetConveyorBucketID() : INDEX_NONE);
			++DiagnosticSamplesLogged;
		}

		BuildableSub->mConveyorGroupsPendingChainActors.AddUnique(TG);
		++FinalQueuedGroups;
	}

	UE_LOG(LogSmartFoundations, Display,
		TEXT("ChainActorService: queued vanilla conveyor rebuild - belts=%d chains=%d groups=%d final_groups=%d cleared=%d detached_chains=%d mismatched_backptrs=%d cleared_backptrs=%d coalesced_groups=%d coalesced_belts=%d multi_groups=%d sorted_groups=%d unsorted_groups=%d lift_belts=%d normalized_buckets=%d"),
		Belts.Num(), AffectedChains.Num(), GroupsToQueue.Num(), FinalQueuedGroups, ClearedGroups, DetachedChains, QueuedGroupBackPointerMismatches, ClearedBackPointers,
		CoalescedGroups, CoalescedBelts, MultiConveyorGroups, SortedGroups, UnsortedGroups, LiftBeltsInQueuedGroups, NormalizedBucketAssignments);

	return FinalQueuedGroups;
}

int32 USFChainActorService::ReRegisterAndQueueVanillaRebuildForBelts(
	const TArray<AFGBuildableConveyorBase*>& Belts,
	const TSet<AFGConveyorChainActor*>& ExtraChains)
{
	USFSubsystem* Sub = Subsystem.Get();
	if (!Sub) return 0;
	UWorld* World = Sub->GetWorld();
	if (!World) return 0;

	AFGBuildableSubsystem* BuildableSub = AFGBuildableSubsystem::Get(World);
	if (!BuildableSub) return 0;

	TArray<AFGBuildableConveyorBase*> UniqueBelts;
	TSet<AFGBuildableConveyorBase*> SeenBelts;
	TSet<AFGConveyorChainActor*> AffectedChains = ExtraChains;
	TSet<FConveyorTickGroup*> GroupsToClear;

	auto GetConnectedConveyor = [](UFGFactoryConnectionComponent* Conn) -> AFGBuildableConveyorBase*
	{
		if (!Conn || !Conn->IsConnected()) return nullptr;
		UFGFactoryConnectionComponent* Other = Conn->GetConnection();
		return Other ? Cast<AFGBuildableConveyorBase>(Other->GetOuterBuildable()) : nullptr;
	};

	for (AFGBuildableConveyorBase* Belt : Belts)
	{
		if (!IsValid(Belt) || SeenBelts.Contains(Belt)) continue;
		SeenBelts.Add(Belt);
		UniqueBelts.Add(Belt);

		if (AFGConveyorChainActor* Chain = Belt->GetConveyorChainActor())
		{
			AffectedChains.Add(Chain);
		}

		const int32 BucketID = Belt->GetConveyorBucketID();
		if (BuildableSub->mConveyorTickGroup.IsValidIndex(BucketID))
		{
			if (FConveyorTickGroup* TG = BuildableSub->mConveyorTickGroup[BucketID])
			{
				GroupsToClear.Add(TG);
			}
		}

		if (AFGBuildableConveyorBase* Upstream = GetConnectedConveyor(Belt->GetConnection0()))
		{
			if (AFGConveyorChainActor* Chain = Upstream->GetConveyorChainActor())
			{
				AffectedChains.Add(Chain);
			}
		}

		if (AFGBuildableConveyorBase* Downstream = GetConnectedConveyor(Belt->GetConnection1()))
		{
			if (AFGConveyorChainActor* Chain = Downstream->GetConveyorChainActor())
			{
				AffectedChains.Add(Chain);
			}
		}
	}

	for (AFGConveyorChainActor* Chain : AffectedChains)
	{
		if (!IsValid(Chain)) continue;
		for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
		{
			if (TG && TG->ChainActor == Chain)
			{
				GroupsToClear.Add(TG);
			}
		}
	}

	int32 ClearedGroups = 0;
	for (FConveyorTickGroup* TG : GroupsToClear)
	{
		if (!TG) continue;
		if (TG->ChainActor)
		{
			BuildableSub->RemoveChainActorFromConveyorGroup(TG);
			++ClearedGroups;
		}
		else
		{
			for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
			{
				if (IsValid(Belt) && Belt->GetConveyorChainActor())
				{
					Belt->SetConveyorChainActor(nullptr);
				}
			}
		}
	}

	int32 DetachedChains = 0;
	for (AFGConveyorChainActor* Chain : AffectedChains)
	{
		if (!IsValid(Chain)) continue;
		BuildableSub->RemoveConveyorChainActor(Chain);
		++DetachedChains;
	}

	const int32 ClearedPendingGroups = BuildableSub->mConveyorGroupsPendingChainActors.Num();
	BuildableSub->mConveyorGroupsPendingChainActors.Empty();

	int32 RemovedBelts = 0;
	for (AFGBuildableConveyorBase* Belt : UniqueBelts)
	{
		if (!IsValid(Belt)) continue;
		BuildableSub->RemoveConveyor(Belt);
		++RemovedBelts;
	}

	int32 AddedBelts = 0;
	for (AFGBuildableConveyorBase* Belt : UniqueBelts)
	{
		if (!IsValid(Belt)) continue;
		BuildableSub->AddConveyor(Belt);
		++AddedBelts;
	}

	const int32 PendingGroups = BuildableSub->mConveyorGroupsPendingChainActors.Num();
	UE_LOG(LogSmartFoundations, Display,
		TEXT("ChainActorService: vanilla re-registration queued rebuild - belts=%d removed=%d added=%d groups_cleared=%d chains=%d detached_chains=%d pending_cleared=%d pending_groups=%d"),
		UniqueBelts.Num(), RemovedBelts, AddedBelts, ClearedGroups, AffectedChains.Num(), DetachedChains, ClearedPendingGroups, PendingGroups);

	return PendingGroups;
}

void USFChainActorService::ScheduleDeferredZombiePurge(float DelaySeconds)
{
	USFSubsystem* Sub = Subsystem.Get();
	if (!Sub) return;
	UWorld* World = Sub->GetWorld();
	if (!World) return;

	FTimerManager& TM = World->GetTimerManager();
	if (TM.IsTimerActive(DeferredPurgeTimerHandle))
	{
		// Already pending — coalesce rather than stack.
		return;
	}

	TWeakObjectPtr<USFChainActorService> WeakSelf(this);
	TM.SetTimer(DeferredPurgeTimerHandle, FTimerDelegate::CreateLambda([WeakSelf]()
	{
		USFChainActorService* Self = WeakSelf.Get();
		if (!Self) return;

		// IMPORTANT: Only purge zombies here. Do NOT call RepairOrphanedBelts()
		// or RepairSplitChains() automatically from the deferred post-upgrade timer.
		//
		// RepairOrphanedBelts is an explicit Triage action only; do not run it
		// automatically after large upgrades. RepairSplitChains is also unsafe as an
		// automatic post-upgrade pass: it can drive the old
		// synchronous recovery path into backptr=null groups and zero-segment chain
		// actors. Zombie purge alone is safe because it only unhooks dead chain actors
		// from tick groups and destroys them without resubscribing belts into
		// mConveyorTickGroup. Orphaned/split TGs are left for explicit Triage after
		// the save has loaded and the world is stable.
		const int32 Purged = Self->PurgeZombieChainActors();

		if (Purged > 0)
		{
			UE_LOG(LogSmartFoundations, Display,
				TEXT("ChainActorService: Deferred post-upgrade cleanup — %d zombie(s) purged. If belts still appear stalled, wait for settling, run Triage > Detect, then save/reload before any orphan recovery."),
				Purged);
		}
		else
		{
			UE_LOG(LogSmartFoundations, Log,
				TEXT("ChainActorService: Deferred post-upgrade cleanup — no zombies detected."));
		}
	}), DelaySeconds, /*bLoop*/ false);
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
		UE_LOG(LogSmartFoundations, Display,
			TEXT("ChainActorService: topology dump written → %s"), *Path);
	}
	else
	{
		UE_LOG(LogSmartFoundations, Warning,
			TEXT("ChainActorService: failed to write topology dump to %s"), *Path);
	}
}

FSFChainDiagnosticResult USFChainActorService::DetectChainActorIssues() const
{
	FSFChainDiagnosticResult Result;

	AFGBuildableSubsystem* BuildableSub = GetBuildableSubsystem();
	if (!BuildableSub) return Result;

	UWorld* World = BuildableSub->GetWorld();
	if (!World) return Result;

	// --- Phase A: NO_SEGMENTS zombies ---
	// Any chain actor with zero segments is inert and would crash on load if serialised.
	for (TActorIterator<AFGConveyorChainActor> It(World); It; ++It)
	{
		AFGConveyorChainActor* Chain = *It;
		if (!Chain || !IsValid(Chain)) continue;
		if (Chain->GetNumChainSegments() == 0)
		{
			++Result.ZombieChainCount;
		}
	}

	// --- Phase B: SPLIT_CHAIN pairs ---
	// Build a belt → chain map for all live chain actors.
	TMap<AFGBuildableConveyorBase*, AFGConveyorChainActor*> BeltToChain;
	for (TActorIterator<AFGConveyorChainActor> It(World); It; ++It)
	{
		AFGConveyorChainActor* Chain = *It;
		if (!Chain || !IsValid(Chain)) continue;
		for (const FConveyorChainSplineSegment& Seg : Chain->GetChainSegments())
		{
			if (Seg.ConveyorBase) BeltToChain.Add(Seg.ConveyorBase, Chain);
		}
	}

	// Detect split boundaries: conveyor A feeds conveyor B but they are in different
	// chain actors. Lifts are conveyors here; only cross-level edges are excluded.
	TSet<AFGConveyorChainActor*> SplitChains;
	for (TActorIterator<AFGConveyorChainActor> It(World); It; ++It)
	{
		AFGConveyorChainActor* Chain = *It;
		if (!Chain || !IsValid(Chain) || Chain->GetNumChainSegments() == 0) continue;

		for (const FConveyorChainSplineSegment& Seg : Chain->GetChainSegments())
		{
			AFGBuildableConveyorBase* Belt = Seg.ConveyorBase;
			if (!Belt || !IsValid(Belt)) continue;
			UFGFactoryConnectionComponent* OutConn = Belt->GetConnection1();
			if (!OutConn || !OutConn->IsConnected()) continue;
			UFGFactoryConnectionComponent* PeerConn = OutConn->GetConnection();
			if (!PeerConn) continue;

			AFGBuildableConveyorBase* Downstream =
				Cast<AFGBuildableConveyorBase>(PeerConn->GetOuterBuildable());
			if (!IsValid(Downstream)) continue;
			if (Belt->GetLevel() != Downstream->GetLevel()) continue;

			AFGConveyorChainActor* const* DownstreamChainPtr = BeltToChain.Find(Downstream);
			if (!DownstreamChainPtr) continue;
			AFGConveyorChainActor* DownstreamChain = *DownstreamChainPtr;
			if (DownstreamChain == Chain) continue;

			SplitChains.Add(Chain);
			SplitChains.Add(DownstreamChain);
		}
	}
	Result.SplitChainCount = SplitChains.Num();

	// --- Phase C: chain=NONE orphaned belts ---
	// Any flat belt (non-lift) that is connected on at least one end but belongs to no chain
	// actor is an orphan. Vanilla assigns connected conveyor runs to a chain; chain=NONE on a
	// connected flat belt means it was evicted and never re-adopted (e.g. mass-upgrade remnant).
	// Lifts are excluded from this legacy flat-belt count; split detection above handles lift edges.
	for (TActorIterator<AFGBuildableConveyorBase> It(World); It; ++It)
	{
		AFGBuildableConveyorBase* Belt = *It;
		if (!Belt || !IsValid(Belt)) continue;
		if (Cast<AFGBuildableConveyorLift>(Belt)) continue;
		if (BeltToChain.Contains(Belt)) continue; // already in a chain actor — fine

		// Belt is chain=NONE. If either end is connected it should be in a chain.
		const bool bConnected =
			(Belt->GetConnection0() && Belt->GetConnection0()->IsConnected()) ||
			(Belt->GetConnection1() && Belt->GetConnection1()->IsConnected());

		if (bConnected)
		{
			++Result.OrphanedBeltCount;
		}
	}

	// --- Phase D: orphaned tick groups ---
	// Chain actor diagnostics can be clean while connected belts still sit in a tick group
	// where TG->ChainActor is null. This is the authoritative signal for the chain=NONE
	// flow failures seen after large mass upgrades.
	for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
	{
		if (!TG || TG->ChainActor) continue;

		++Result.OrphanedTickGroupCount;

		int32 LiveBeltsInGroup = 0;
		bool bFoundCandidate = false;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (!IsValid(Belt) || Belt->IsActorBeingDestroyed()) continue;

			++LiveBeltsInGroup;
			++Result.LiveBeltsInOrphanedTickGroups;

			if (bFoundCandidate || Cast<AFGBuildableConveyorLift>(Belt)) continue;

			const bool bConnected =
				(Belt->GetConnection0() && Belt->GetConnection0()->IsConnected()) ||
				(Belt->GetConnection1() && Belt->GetConnection1()->IsConnected());

			if (bConnected)
			{
				++Result.OrphanedBeltCandidates;
				bFoundCandidate = true;
			}
		}

		if (LiveBeltsInGroup == 0)
		{
			++Result.EmptyOrphanedTickGroupCount;
		}
	}

	// --- Phase E: tick-group/member chain back-pointer mismatch ---
	// TG->ChainActor can be non-null while one or more conveyors in TG->Conveyors
	// still point to null or to a different chain actor. This was the first April 2026
	// steel-output failure signature and is invisible to chain-actor-only scans.
	for (FConveyorTickGroup* TG : BuildableSub->mConveyorTickGroup)
	{
		if (!TG || !TG->ChainActor) continue;

		bool bMismatch = false;
		for (AFGBuildableConveyorBase* Belt : TG->Conveyors)
		{
			if (!IsValid(Belt) || Belt->IsActorBeingDestroyed()) continue;
			if (Belt->GetConveyorChainActor() != TG->ChainActor)
			{
				bMismatch = true;
				break;
			}
		}

		if (bMismatch)
		{
			++Result.TickGroupBackPointerMismatchCount;
		}
	}

	UE_LOG(LogSmartFoundations, Log,
		TEXT("ChainActorService: DetectChainActorIssues — zombies=%d split_chains=%d orphaned_belts=%d orphaned_tgs=%d empty_orphaned_tgs=%d live_orphaned_belts=%d orphaned_belt_candidates=%d tg_backptr_mismatches=%d"),
		Result.ZombieChainCount,
		Result.SplitChainCount,
		Result.OrphanedBeltCount,
		Result.OrphanedTickGroupCount,
		Result.EmptyOrphanedTickGroupCount,
		Result.LiveBeltsInOrphanedTickGroups,
		Result.OrphanedBeltCandidates,
		Result.TickGroupBackPointerMismatchCount);

	return Result;
}

FSFChainRepairResult USFChainActorService::RepairAllChainActorIssues()
{
	FSFChainRepairResult Result;

	// ORDER MATTERS:
	//
	// 1. PurgeZombieChainActors (pass 1) — clears zombie ChainActor pointers from every TG.
	//    RepairOrphanedBelts skips TGs where TG->ChainActor != null, so zombies must be
	//    purged first or the orphan scan silently skips all affected TGs.
	//
	// 2. RepairSplitChains — fixes any SPLIT_CHAIN pairs that emerged after the purge.
	//    Uses InvalidateAndRebuildChains (synchronous migration) so any 0-seg zombies it
	//    produces in Phase 3 are swept up by pass 2 below.
	//
	// 3. RepairOrphanedBelts — explicit triage recovery for settled bad saves. It
	//    re-registers live conveyors from orphaned tick groups through vanilla.
	//
	// 4. PurgeZombieChainActors (pass 2) — sweeps any zombies produced by step 2's
	//    synchronous migration.
	Result.ZombiesPurged         = PurgeZombieChainActors();
	Result.SplitGroupsRebuilt    = RepairSplitChains();
	RepairOrphanedBelts(&Result);
	Result.ZombiesPurged        += PurgeZombieChainActors();

	UE_LOG(LogSmartFoundations, Display,
		TEXT("ChainActorService: RepairAllChainActorIssues complete — zombies_purged=%d split_groups_rebuilt=%d orphaned_tgs=%d empty_orphaned_tgs=%d live_orphaned_belts=%d orphaned_belt_candidates=%d orphaned_belts_requeued=%d"),
		Result.ZombiesPurged,
		Result.SplitGroupsRebuilt,
		Result.OrphanedTickGroupCount,
		Result.EmptyOrphanedTickGroupCount,
		Result.LiveBeltsInOrphanedTickGroups,
		Result.OrphanedBeltCandidates,
		Result.OrphanedBeltsRequeued);

	return Result;
}
