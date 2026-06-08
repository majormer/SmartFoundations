# Conveyor Belts, Chain Actors, and Belt Placement in Satisfactory 1.2 — A Working Thesis

**Subject:** the complete model of how conveyor belts, conveyor chain actors, conveyor poles,
and Smart!'s auto-connect placement interact on Satisfactory 1.2 (game CL 491125, UE 5.6.1-CSS),
and why stacked-pole belt auto-connect is hard.

**Status:** living reference. Consolidates the investigation of 2026-06-05 (and the prior P0
chain-actor characterization), plus the **2026-06-08 escalation (§6.15)**. Supersedes scattered
notes; companion to `docs/Features/AutoConnect/DESIGN_StackablePole_FromScratch.md` (the chosen fix)
and `docs/Sprints/ChainActorMigrationPlan.md` (local; the P0 record).

> **Latest (2026-06-08):** the build-time chain coalesce (§10) is **RESUMED** on
> `feature/341-belt-run-chain-coalesce`. A controlled live retest (§6.15) escalated the stacked-run
> fragmentation from a reload **stall** to **crash-class** (a vanilla merge can leave a belt at
> `mChainSegmentIndex == -1` → assert on the next factory tick) and showed a **single reload does NOT
> coalesce** the run. See §6.15 and §10.

---

## 0. Provenance and epistemic caveats (read first)

Claims in this document are tagged by how we know them:

- **[H]** = verified from a 1.2 **header** (`Source/FactoryGame/Public/...`), with file:line.
- **[C]** = verified from **Smart mod C++** (`Mods/GameFeatures/SmartFoundations/Source/...`), file:line.
- **[E]** = **empirical**, observed in-game via instrumentation (log token + date).
- **[I]** = **inferred** — a model that fits [H]+[E] but is not directly proven.

> **The single most important caveat.** The FactoryGame `.cpp` bodies for the conveyor/chain
> subsystem are **auto-generated link stubs** — the implementation is in the shipped binary, not
> readable source. `AFGBuildableSubsystem::MigrateConveyorGroupToChainActor`,
> `RemoveChainActorFromConveyorGroup`, `ForceDestroyChainActor`, `RemoveConveyorFromBucket`, etc.
> all have **empty bodies** in the repo, and `FGConveyorChainActor.cpp` likewise. **[I/E]**
> Therefore the *semantics* of every chain/bucket primitive are known only from (a) header
> doc-comments, (b) tick wiring visible in headers, (c) Smart's crash-tested observations, and
> (d) deliberate in-game probes. **No primitive's behavior may be assumed from "reading the
> source"; it must be confirmed in-game.** This constraint shapes the entire thesis.

---

## 1. Abstract

A conveyor **belt** is the unit of item transport: a spline-shaped buildable with an input
connector (`Connection0`) and an output connector (`Connection1`). A run of connected belts is,
for performance and replication, coalesced by the engine into a single **`AFGConveyorChainActor`**
— one actor that ticks the whole run on a worker thread via `ParallelFor`. The chain actor is
*derived* state: the engine builds it from belt connectivity. The central, load-bearing invariant
of the whole system is:

> **A belt's connections must exist *before* it is registered into the conveyor subsystem
> (`AddConveyor`).** Register-then-connect produces a one-belt "solo" chain that the engine will
> **not** retroactively merge, and any attempt to fix it by mutating live bucket/chain state from
> outside a complete vanilla operation **crashes** on the next parallel factory tick. **(2026-06-08,
> §6.15: the crash is not only from *attempted fixes* — a fragmented run can crash from the player's
> own next action, when connecting a vanilla end-belt makes vanilla merge the solo chains and mis-assign
> a belt's segment index to -1.)**

Smart!'s belt auto-connect features (distributor→factory, Extend, stackable poles) all reduce to
the problem of honoring that invariant. Distributor and Extend belts satisfy it naturally because
their endpoints are **stable, pre-existing** buildables at construct time. **Stackable-pole belts
violate it structurally**: the belts in a placement are built *simultaneously*, so a belt's
neighbours don't exist (and its own connector geometry isn't even finalized) when it would need
to connect. Six in-game experiments (§6) converged on the shipped design: **the belt child holograms construct
real belts and wire their connectors by geometric coincidence at `Construct`, before registering into
the conveyor subsystem** — the same connect-before-register discipline Extend uses (§9), with cost left
to vanilla. (An earlier "preview-only belt holograms + fresh `BuildBelt`, cost via a `GetCost` override"
proposal was superseded before shipping; see §6.8–§6.9 and the §9 mis-conclusion note.) This document
records the model, the experiments, and the analysis in full.

> **Update 2026-06-08 (§6.15).** Two things sharpened since the original write-up. (1) The fragmented
> stacked run is **crash-class**, not merely a reload stall: a vanilla merge of the solo chains can
> leave a belt at `mChainSegmentIndex == -1`, asserting in `AFGConveyorChainActor::GetItemsForSegment()`
> on the next `ParallelFor` factory tick. (2) A single save+reload does **not** reliably coalesce the
> run. The required fix is therefore a **build-time coalesce** (§10, `feature/341-belt-run-chain-coalesce`),
> not reliance on reload. Note also that §9 revisits the **construction** approach summarized above
> (preview-only + fresh `BuildBelt`); read §8–§9 together before treating that summary as final.

---

## 2. The vanilla domain model (1.2 headers)

### 2.1 Belt = item-flow edge
- `AFGBuildableConveyorBelt : AFGBuildableConveyorBase`. **[H]**
- Connectors: `GetConnection0()` (input), `GetConnection1()` (output) — `FGBuildableConveyorBase.h:163-164`. Items flow `Connection0 → belt → Connection1`. **[H]**
- Geometry: `mSplineData : TArray<FSplinePointData>`; `GetSplinePointData()` / `GetMutableSplinePointData()` (`FGBuildableConveyorBelt.h:108-109`), `GetSplineComponent()` (`:112`), `GetMeshLength()` (`:110`), `GetLength()`/`mLength` on the base (`FGBuildableConveyorBase.h:159,374`). **[H]**
- Chain back-pointer: `GetConveyorChainActor()` → `mConveyorChainActor` (`FGBuildableConveyorBase.h:184`). A belt "knows" its chain via this pointer; it is null when unchained. **[H]**

### 2.2 The public belt-topology primitives (the mod-facing API)
Static factory operations on `AFGBuildableConveyorBelt` — the **sanctioned** way to change belt topology: **[H]**
- `Split(belt, offset, connectNewConveyors) → TArray<belt>` (`:92`)
- `Merge(TArray<belt>) → belt` (`:98`)
- `Respline(belt, newSplineData) → belt` (`:103`) — recreates the belt with new spline geometry; returns the (possibly new) belt.

These three are the only public belt factory entry points; there is **no** one-call "build a belt
between two connectors." **[H, verified by absence]**

### 2.3 Connectors and directions
`EFactoryConnectionDirection` (`FGFactoryConnectionComponent.h:28-34`): `FCD_INPUT`, `FCD_OUTPUT`,
`FCD_ANY`, **`FCD_SNAP_ONLY`** — the last documented verbatim as *"Special case for conveyor
poles, may need refactor later."* **[H]** Methods: `SetConnection(other)` (`:90`),
`ClearConnection()` (`:104`), `IsConnected()` (`:111`), `GetConnection()`→`mConnectedComponent`
(`:96`), `GetDirection()` (`:122`), `GetConnectorLocation()` (`:141`). **[H]**

### 2.4 Conveyor poles are NOT item-flow nodes
`AFGBuildablePoleConveyor`, `AFGBuildablePoleStackable` (`mStackHeight`), `AFGConveyorPoleHologram`
(`GetSnapConnection()`, `SnapToConnection(connection, parentConveyor)`). **[H]** Pole connectors are
`FCD_SNAP_ONLY` — **belts snap to poles for height/support; poles never carry items and never
belong to a chain.** **[H/I]** Consequence: a "run" of stacked-pole belts is an item graph of
**belt↔belt** connections; the poles are scaffolding.

`AFGConveyorMultiPoleHologram` exists but is an **empty stub** (header has no members; `.cpp` is
143 bytes — just the include). Vanilla provides **no** working stacked-pole-with-belts feature to
reuse. **[H, verified 2026-06-05]**

### 2.5 Chain actors — the coalesced run
`AFGConveyorChainActor` represents a contiguous run of belts as one ticking/replicating actor.
1.2 adds replication-size subclasses (`_RepSizeMedium/Large/Huge/NoCull`) — confirms the index is
CL-83+. **[I, from migration-plan archaeology]** Key observable members/methods:
- `GetNumChainSegments()` — `0` ⇒ a **zombie** (a chain actor that exists but transports nothing). **[H/E]**
- `SetStartAndEndConveyors(first, last)` and `BuildChain()` — build the chain by walking from
  `mFirstConveyor` along `Connection1` (item-flow direction) until a non-belt terminus, then
  checking the reached conveyor equals `mLastConveyor`. **[I, from `SFChainActorService.cpp:542-548`
  comments + the engine warning]**
- `RevertChainActor_Unsafe()` — documented *"not safe to call directly; use
  `AFGBuildableSubsystem::ForceDestroyChainActor` instead."* **[H, migration plan §A]**

**The chain-build invariant (the "Last != mLastConv" rule).** When `BuildChain` walks from
`mFirstConveyor` and the conveyor it terminates on does not equal `mLastConveyor`, the engine
emits `LogConveyorChain: Warning: Last <X> != mLastConv <Y>` and **produces a 0-segment chain
(zombie)**. **[E, observed repeatedly 2026-06-05]** This is the failure surface for any attempt to
form a chain over belts whose endpoint bookkeeping is inconsistent with their actual connectivity.

### 2.6 Tick groups, buckets, and the ParallelFor hazard
The subsystem holds private arrays (Smart's `Friend=(AFGBuildableSubsystem, USFChainActorService)`
grant exists specifically to read them): **[H/C]**
- `mConveyorTickGroup : TArray<FConveyorTickGroup*>` — the "buckets"; each `FConveyorTickGroup`
  owns a `Conveyors` array and a `ChainActor`.
- `mConveyorGroupsPendingChainActors` — groups awaiting chain construction.

Belts are ticked by `AFGBuildableSubsystem::TickFactoryActors` which dispatches
`AFGConveyorChainActor::Factory_Tick()` across worker threads via **`ParallelFor`**
(`FGBuildableSubsystem.cpp:802`, `FGConveyorChainActor.cpp:270`, `ParallelFor.h`). **[E, from crash
callstacks]**

> **The ParallelFor hazard (the central safety constraint).** Mutating bucket/chain membership of
> a **live** belt from outside a complete vanilla operation leaves a chain actor in an
> inconsistent state that the **next** `Factory_Tick` ParallelFor pass dereferences →
> `EXCEPTION_ACCESS_VIOLATION`. The crash is **deferred to the next tick**, which is what makes it
> so dangerous: the mutating call itself returns "successfully." **[E, proven twice — see §6.]**

### 2.7 The subsystem chain/bucket API (semantics from header doc-comments)
From `FGBuildableSubsystem.h` (bodies are stubs — semantics are doc-comment + empirical): **[H/I]**
| Primitive | Doc-comment essence | Empirical safety |
|---|---|---|
| `AddConveyor(belt)` (`:275`) | adds a belt to the buckets; assigns/extends its chain **from its current connections** | safe **iff** connections are set first; double-add crashes |
| `RemoveConveyor(belt)` (`:293`) | removes a belt from the subsystem | **UNSAFE on a live registered belt** — leaves an orphaned chain in the tick path → ParallelFor crash **[E]** |
| `MigrateConveyorGroupToChainActor(tg)` (`:265`) | builds a chain actor for a tick group via `SetStartAndEndConveyors`+`BuildChain` | produces a **zombie** if endpoints/ownership are inconsistent **[E]** |
| `RemoveChainActorFromConveyorGroup(tg)` (`:272`) | nulls `TG->ChainActor` + belt back-pointers, then a full rebuild; doc calls "remove + full rebuild" the **sanctioned** pattern | safe (chain-level); the basis of Smart's working machinery **[E]** |
| `RemoveConveyorChainActor(chain)` (`:287`) | removes a chain actor | safe from timers (chain-level), but does **not** rebuild correctly for the stacked case **[E]** |
| `RemoveConveyorFromBucket` (`:306`) | remove a belt from its bucket | **UNSAFE standalone** on a live chain → next-tick ParallelFor crash **[E, P0]** |
| `RearrangeConveyorBuckets` (`:309`) | compacts the bucket array safely | the safe version of the `mConveyorTickGroup.Remove` that Smart forbids itself |
| `RemoveAndSplitConveyorBucket` (`:315`) | splits a bucket in two | precondition: consistent bucket state |
| `SplitConveyorGroupFromAttachment` (`:299`) | splits a bucket when an attachment is placed on a chain | placement-time op |
| **`ForceDestroyChainActor(chain)`** (`:296`, **NEW in 1.2**) | *"forcefully destroys… removes it from the tick buckets and transfers items back to belts… not particularly fast"* | **safe (no crash)** but **destructive**: items lost, belt transport breaks even with rebuild **[E, P0]** |

### 2.8 Cost model
- `AFGHologram::GetBaseCost()` (`:350`), `GetBaseCostMultiplier()` (`:353`), `GetCost(bool includeChildren)` (`:360`) — a parent hologram's `GetCost(true)` **aggregates child hologram costs**. **[H]**
- `AFGBuildable::GetCostMultiplierForLength(totalLength, costSegmentLength)` (`:498`) — belt cost scales with length. **[H]**
- Affordability/invalidation: `AddConstructDisqualifier(UFGCDUnaffordable::StaticClass())` blocks the build; respect `AFGGameState::GetCheatNoCost()`. Availability = personal inventory + `AFGCentralStorageSubsystem` (dimensional depot). **[C, `SFExtendService.cpp:879` `CanAffordExtendCost`; `SFAutoConnectService.cpp:1128-1171`]**

---

## 3. The fundamental invariant: connect-then-register

Stated three ways (they are equivalent):
1. Set a belt's connections, **then** call `AddConveyor`. **[C, `BuildBeltFromPreview` and
   `CreateManifoldBelt` both comment this verbatim]**
2. A belt registered while **unconnected** becomes its own **solo 1-segment chain** that the engine
   will **not** later merge with neighbours (`BuildChain` "Last != mLastConv" → zombie). **[E]**
3. You may never "fix it afterward" by mutating live bucket/chain state — that crashes the
   ParallelFor tick. **[E]**

Corollary — **build fresh, never touch live belts.** Belts created via
`World->SpawnActor<AFGBuildableConveyorBelt>` do **not** auto-register until an explicit
`AddConveyor`; this is the only way to guarantee connect-then-register and never disturb a live
chain. **[C/E]** The distributor's `BuildBeltFromPreview` (now legacy) and Extend's
`CreateManifoldBelt` both use exactly this sequence: `SpawnActor → Respline → SetConnection →
AddConveyor`.

---

## 4. Smart's placement architecture

### 4.1 Hologram construct lifecycle
Build-gun placement drives an `AFGHologram` through: `ConfigureActor(buildable)` →
`ConfigureComponents(buildable)` → `PostHologramPlacement`. Child holograms are registered on a
parent via `AddChild(child, name)` (into the private `mChildren` array) and built automatically by
vanilla's `Construct()`. **[C]** Smart tags auto-built children `SF_StackableChild` and special-cases
them in `PostHologramPlacement` (skip vanilla post-placement). **[C, `SFConveyorBeltHologram.cpp:269,1285`]**

> **Geometry-timing finding [E, STACK-ORDER, 2026-06-05].** A belt's connector world-positions are
> **not finalized at `ConfigureComponents`** — the spline/mesh that places `Connection0/1` is
> generated later (cf. the guard "mSplineComponent null, not yet initialized",
> `SFConveyorBeltHologram.cpp:319-323`). Probe result: at each stacked belt's `ConfigureComponents`,
> **0** sibling belts were within 200 cm of its endpoints, **even for the last-built belt whose
> siblings already existed.** ⇒ **no connector-based wiring is possible at construct time** (this is
> why the long-standing proximity search always logged `connections=NO`).

### 4.2 The clone-ID registry (general, not Extend-specific)
`USFExtendService::RegisterJsonBuiltActor(cloneId, builtActor)` populates a string→buildable map;
`GetBuiltActorByCloneId(cloneId)` resolves it. **[C, `SFExtendService.cpp:1821`]** Registration in
`ASFConveyorBeltHologram` construct is gated **only** on `!HoloData->JsonCloneId.IsEmpty()` (NOT on
EXTEND membership) — `SFConveyorBeltHologram.cpp:448-456`. **[C, verified 2026-06-05]** So any belt
with a `JsonCloneId` participates. EXTEND/distributor belts wire at construct via
`Conn0/Conn1TargetCloneId → GetBuiltActorByCloneId → SetConnection → AddConveyor`
(`SFConveyorBeltHologram.cpp:1466+`). **Caveat:** this still requires the target's geometry to be
resolvable at construct — which §4.1 shows it is **not** for siblings built in the same operation.

### 4.3 `USFChainActorService` — the proven chain machinery
The one code path empirically proven to rebuild chains cleanly (P0 "RebuildOnly"). `InvalidateAndRebuildForBelts(belts, extraChains)` → `InvalidateAndRebuildChains(affectedChains, explicitTGs)`: **[C]**
- **Phase 1** (`:143`) resolve affected chains → owning tick groups.
- **Phase 2** (`:183`) `RemoveChainActorFromConveyorGroup` on each (clears belt back-pointers + nulls `TG->ChainActor`).
- **Phase 2.5** (`:191-326`) union-find **merge** of adjacent isolated tick groups (prevents SPLIT_CHAIN).
- **Phase 3** (`:328+`) `MigrateConveyorGroupToChainActor` per surviving group; **pre-migration zombie-clear only nulls back-pointers for 0-segment chains** (`:471-481`); on a 0-seg result, a recovery attempt does `SetStartAndEndConveyors(inputEnd, outputEnd)` + `BuildChain` (`:536-554`).
- **Phase 4** (`:613+`) detach the original affected chain actors.
This works for **existing** chains (mass-upgrade, Extend). It was previously believed it could **not**
save the stacked case (see §6) because those belts arrive as multiple **live solo chains**. **Revised
2026-06-08 (§6.15/§10):** the §6.15 retest shows those solo chains are *correctly connected in series*
(items flowed end-to-end pre-crash) — they are merely in separate chain actors, which is exactly the
Phase 2.5 union-find merge's job. So this path is now the **chosen build-time coalesce for stacked runs**
and is being validated under #341; the old "merge cannot reconstitute" claim held only for the pre-fix
garbage-connectivity state (§6.7 row-1, before §6.11/§6.12).

---

## 5. Feature applications

### 5.1 Distributor → factory auto-connect — WORKS
Architecture (post "child-hologram refactor", `SFConveyorAttachmentHologram.cpp:18`): belt children
are added via `AddChild` and built by vanilla `Construct`. **[C]** Works because each belt's
endpoints are a **stable, pre-existing** distributor connector and machine connector — connections
resolve at construct → connect-then-register holds. Cost: `ASFConveyorAttachmentHologram::GetCost`
override injects belt-preview cost into the distributor total (`:26`); affordability via
`UFGCDUnaffordable`. **Note:** `BuildBeltFromPreview`/`BuildBeltsForDistributor` are now **dead
code** (no callers) — superseded by child holograms; they remain the cleanest *reference* for the
`SpawnActor→Respline→SetConnection→AddConveyor` sequence. **[C, verified 2026-06-05]**

### 5.1b How Extend actually forms chains (the model to adapt for stacked) [C, 2026-06-05]
Extend solves the **same simultaneous-build problem** stacked has, via a **two-phase** scheme —
and notably **without** returning `nullptr` and **without** position-based wiring:

1. **Real-constructing child holograms.** Extend belt children call `Super::Construct` (return a
   real actor) — so **no build-gun crash**, and **cost works via child `GetCost` aggregation**.
2. **Chain model.** `HoloData->ExtendChainId` / `ExtendChainIndex` / `ExtendChainLength` /
   `bIsInputChain` (`IsExtendChainMember` = ChainId≥0 && Index≥0). Assigned from the Extend
   topology/manifest (`TopoInfo`).
3. **Construct-time, connect-by-REFERENCE (best-effort).** `ResolveChainConnections`
   (`SFExtendChainHelper.cpp:50-57`): belt *i* resolves its already-built successor via
   `ExtendService->GetBuiltConveyor(ChainId, i+1)` and connects `Conn1 → successor.GetConnection0()`
   — connector **object**, not world position. Relies on a **reverse build order** (highest index
   built first) so the successor exists. **This is why geometry-not-final at Construct is irrelevant
   — it corrects the false conclusion from the `STACK-ORDER` probe (§4.1), which measured positions.**
4. **Post-build, sorted wiring (the reliable pass).** `WireBuiltChildConnections` (run from the
   `OnActorSpawned` deferred timer) iterates **sorted chain indices** ("reverse for INPUT chains,
   forward for OUTPUT", `SFExtendWiringService_BuiltChild.cpp:1254,1522`) and wires connections /
   reconciles chains after everything is built — the safety net that makes Extend robust regardless
   of exact construct order.
5. **Registry (reusable, general):** `RegisterBuiltConveyor(ChainId, Index, belt, bIsInput)` +
   `GetBuiltConveyor(ChainId, Index)` (`SFExtendService.h:318,327`).

**Open trace item:** the *exact* enforcement of reverse build order at construct is not a single
sort/`GetConstructResults` — it emerges from the clone/manifest **spawn+AddChild order**, with the
**post-build sorted pass as the actual reliability guarantee**. So adapting Extend for stacked means
either reproducing the reverse spawn order *or* (more robustly) leaning on a post-build sorted wiring
pass like §5.1b.4 — **but** any post-build chain reconciliation must avoid the live-belt bucket
hazard (§2.6); Extend's post-build pass appears to wire connections + rely on vanilla/chain-service
rebuild, the safety of which on stacked solo-chains is the thing to verify before relying on it.

### 5.2 Extend (manifold / built-child / json) — WORKS
Child holograms + clone-ID construct wiring (`Conn0/Conn1TargetCloneId`) + the
`USFChainActorService` rebuild for any chain reconciliation. `CreateManifoldBelt`
(`SFExtendWiringService_Manifold.cpp:600-667`) is the canonical fresh-build: build spline → spawn →
`Respline` → `SetConnection` → `AddConveyor` **last**. Affordability via `CanAffordExtendCost`. **[C]**
Works for the same reason: extend targets are already-built (or built-earlier-in-batch, resolvable
by clone-ID) buildables.

### 5.3 Stackable conveyor poles — BROKEN (the subject)
- **Preview/build driver:** `ProcessStackableConveyorPoles` (`SFAutoConnectService_Stackable.cpp:55`)
  runs every preview tick; maps the pole **grid** (`Z→X→Y`), finds primary-axis neighbours (X, or Y
  for wall poles), and per pole-pair calls `UpdateOrCreateBeltForPolePair` (`:1399`) → a belt **child
  hologram** (`AddChild`, `SF_StackableChild`), tracked in `FStackableBeltState.BeltsByPolePair`. **[C]**
- The belt hologram captures its intended pole connectors into `HoloData->StackableBeltConn0/Conn1`
  (`:1606-1607`) **but these are never read** (the deterministic wiring was plumbed, never finished). **[C]**
- At construct, the belt's `ConfigureComponents` stackable branch proximity-searches for pole/belt
  connectors, finds none (`connections=NO`), and **defers** — never calling `AddConveyor`. **[C]**
- Vanilla nonetheless **auto-registers** each belt during its own construction → each becomes a
  **solo 1-segment chain** while unconnected. **[E]**
- A next-tick `OnActorSpawned` timer (`PendingStackableBelts`) then proximity-wires belt↔belt and
  historically called `RemoveConveyorChainActor` ("invalidate and hope vanilla rebuilds"). **[C]**

**The structural defect (as originally diagnosed):** all belts (and poles) in a stacked placement
are built **simultaneously**. At any belt's construct, (a) its sibling belts don't exist yet or
aren't geometrically ready, and (b) its own connectors aren't positioned (§4.1). So the belt
**cannot** connect-then-register; it register-then-(maybe-later-)connects → the invariant of §3 is
violated by construction.

> ✅ **RESOLVED (2026-06-05) — the "cannot connect-then-register" claim was too strong.** It conflated
> *geometric* readiness with *reference* readiness. The §3 invariant only needs the **neighbour's
> connector pointer**, not its final world position — and the `STACK-ORDER` probe (§6.6) confirmed
> sibling belts **are already constructed** at a given belt's `Construct`. So, exactly as Extend does
> (§5.1b), the fix connects **by reference**: tag each run with a `StackChainId`/`StackChainIndex`,
> and in `ASFConveyorBeltHologram::Construct` for a stackable child — call `Super::Construct` (real
> belt, cost charged, no null-return crash), resolve the run neighbour via the conveyor registry
> (`GetBuiltConveyor`), `SetConnection` on the built connectors, then `RegisterBuiltConveyor`. No
> preview-only architecture and **no manual cost reimplementation** were needed. See §6.9 + §8.

---

## 6. Experimental record (chronological, exhaustive)

All in-game, Shipping build, singleplayer, game CL 491125. Log tokens are grep-able in
`%LOCALAPPDATA%/FactoryGame/Saved/Logs/FactoryGame.log`.

### 6.1 P0 — vanilla primitive characterization (prior; `ChainProbe`) **[E]**
- `RemoveConveyorFromBucket` standalone on a live 1-segment chain → **`EXCEPTION_ACCESS_VIOLATION`
  in `AFGConveyorChainActor::Factory_Tick` on a ParallelFor worker, next tick.** The call itself
  returned cleanly. ⇒ low-level bucket primitives are unsafe on live belts.
- `ForceDestroyChainActor` → **no crash, but destructive**: chain torn down, belt stops, items lost
  (40→0). `AddConveyor` after did **not** re-chain.
- `ForceDestroyChainActor + AddConveyor + Migrate` ("ForceDestroyMigrate") → structurally-present
  chain but **transport dead** ("items into the ether").
- `RemoveChainActorFromConveyorGroup + Migrate` ("RebuildOnly") → **seamless**: new chain, items
  preserved, no disruption. ⇒ the **only** clean rebuild of an existing chain; the basis of
  `USFChainActorService`.

### 6.2 Site #1 — `InvalidateAndRebuildForBelts` merge in the stacked timer **[E, STACK-PROBE]**
Result: `STACK-PROBE: re-built N belt(s) → 0 unique chain(s), N null-chain belt(s) (BUG)`. Service
summary: `groups_cleared=N groups_merged=… groups_migrated=2 zombies=2 failed_recovery=2
post_zero_segments=2`. Engine: `Last <X> != mLastConv <Y>` (both orderings). Items still *flowed*
(bucket-level ticking) but **no valid chain**; **did not self-heal**. ⇒ the merge cannot
reconstitute solo chains formed unconnected.

### 6.3 STACK-PREPROBE — pre-rebuild state of the belts **[E]**
Each belt: `splinePts=2 splineLen=800–950 length=800–950`, connectors wired into clean linear runs,
and **each already owns its own distinct 1-segment chain** (`FGConveyorChainActor_…`). ⇒ geometry is
healthy; the problem is purely chain **topology** (N solo chains where 1 merged chain is wanted).

### 6.4 STACK-WIRE — connect at construct via the captured connectors **[E]**
The captured `StackableBeltConn0/Conn1` resolve to **pole *holograms*** (`Holo_ConveyorStackable_C`,
direction `ANY`), and `SetConnection` to them is a **no-op** (`conn0=N conn1=N` afterward). ⇒ the
real pole buildables don't exist at belt-construct time; there is no valid construct-time target.

### 6.5 Re-register — `RemoveConveyor` → `AddConveyor` in the timer **[E]**
**CRASH:** `EXCEPTION_ACCESS_VIOLATION` in `AFGConveyorChainActor::Factory_Tick`, ParallelFor worker
(`FGBuildableSubsystem.cpp:802` → `FGConveyorChainActor.cpp:270`) — the exact P0 §6.1 signature.
`RemoveConveyor` on a live registered belt orphans its solo chain in the tick path. ⇒ post-construct
bucket mutation is fatal.

### 6.6 STACK-ORDER — is geometry/neighbours available at construct? **[E]**
At each stacked belt's `ConfigureComponents`: `521–524 built sibling belts in world, **0** within
200 cm of an endpoint`, **including the last-built belt** (siblings already existed). ⇒ connector
geometry isn't finalized at construct; **all construct-time wiring (clone-ID or proximity) is
impossible.**

### 6.8 Build-gun crash — child `Construct` returning `nullptr` **[E, 2026-06-05]**
Implemented Option B with stacked belt children as **preview-only by returning `nullptr` from
`ASFConveyorBeltHologram::Construct`** (the mechanism EXTEND used "previously"). On **building**
(not tick): **`EXCEPTION_ACCESS_VIOLATION` reading 0x0 in
`UFGBuildGunStateBuild::InternalConstructHologram` (`FGBuildGunBuild.cpp:1862`)**, via
`Server_ConstructHologram` ← `PrimaryFire`. ⇒ **In 1.2 the build gun dereferences the actor a
child `Construct` returns; returning `nullptr` is fatal.** This is exactly why EXTEND abandoned
nullptr-suppression ("Previously returned nullptr… but now we want them to build"). The shaped
spline + cost preview worked (previews showed, cost displayed) — the crash was purely on construct.

> **HARD CONSTRAINT [E].** A child hologram (in the parent's `mChildren`) **must return a real
> constructed actor** from `Construct`. Therefore a stacked belt cannot be made "preview-only"
> *while remaining a child*: it either constructs a real belt (auto-registers unconnected → the
> bug) or it must **not be a child at all**. Preview-only requires **non-child** preview holograms
> (the legacy `FBeltPreviewHelper` style), or no belt preview.

### 6.9 STACK-CHAIN — real belt + connect-by-reference at construct **[E, 2026-06-05] ✅ THE FIX**
After 6.8 forced "child holograms must construct a real belt," the resolution stopped fighting that
and instead made the real belt connect **by reference** (not geometry/proximity — sidestepping 6.6's
limitation entirely). In `ASFConveyorBeltHologram::Construct` for a `SF_StackableChild`: tag from
`StackChainId`/`StackChainIndex` → `Super::Construct` (real belt, cost charged via the build gun, no
null-return crash) → resolve run neighbour by `StackRegistry->GetBuiltConveyor(ChainId, Index±1)` →
**explicit `SetConnection`** on the built connectors (`SetSnappedConnections` alone did **not** carry
the connection — items still stopped at the break until the explicit `SetConnection` was added) →
`RegisterBuiltConveyor`. Order-agnostic (predecessor and/or successor).
- **Result:** no crash; cost charged; items **flow end-to-end** through the former break.
- **Live chain/connection inspection:** belt-connection state shows both ends wired to the correct
  run peers; the chain audit shows `zombieCount:0`, all LUTs valid.
- **Save/reload (the original-bug gate): PASS** — post-reload still `zombieCount:0`, connections
  intact, runs re-unified by vanilla into healthy multi-segment chains. ⇒ **invariant satisfiable at
  construct via reference; the §5.3 "impossible" conclusion was about *proximity* wiring only.**

### 6.10 Reversed-belt CTD — stale raw-pointer in the by-reference registry **[E, 2026-06-05] ✅ FIXED**
After 6.9 shipped, building belts with the **reversed/backward** option over or near a prior stacked
run crashed on **build**: **`EXCEPTION_ACCESS_VIOLATION reading 0xffffffffffffffff` in
`UFGConnectionComponent::GetOuterBlueprintDesigner` ← `UFGFactoryConnectionComponent::CanConnectTo`
← vanilla `AFGConveyorBeltHologram::ConfigureComponents:365`**, reached via our `Construct` →
`Super::Construct` → `ConstructInstance` → our `ConfigureComponents` → `Super`. Vanilla's snap check
dereferenced a **garbage connector pointer** our STACK-CHAIN handler had passed to `SetSnappedConnections`.
- **Root cause:** 6.9 resolved neighbours through the **Extend** registry (`BuiltConveyorsByChain`),
  which stores **raw `AFGBuildableConveyorBase*`** and is only `Empty()`'d by the **Extend** chain-fix
  finalize — which never runs for stacked builds. Stacked entries therefore **persist across builds and
  dangle.** `StackChainId`/`StackChainIndex` are **direction-agnostic** (`SFAutoConnectService_Stackable.cpp:271-272`),
  so a reversed build over a prior run hits a **colliding key**; `GetBuiltConveyor(Index±1)` returns a
  **freed** belt; `IsValid()` can't detect a dangling raw pointer; `Pred->GetConnection1()` yields garbage.
- **Fix:** stacked belts use a **dedicated `TWeakObjectPtr`-backed registry** (`GStackBuiltConveyors` in
  `SFConveyorBeltHologram.cpp`), not the Extend raw registry. A destroyed belt resolves to **null**
  (`.Get()`) and is never dereferenced; stacked entries no longer pollute the Extend finalize; connector
  results are `IsValid()`-guarded before `SetSnappedConnections`/`SetConnection`.
- **Lesson [I]:** "connect by reference" is only as safe as the reference's **lifetime**. A raw pointer
  shared across build operations without a clear-owner is a latent dangling-deref; cross-op references
  must be **weak** (or epoch-scoped). Extend got away with raw pointers only because it clears per-build.

### 6.11 Reversed-belt cross-wiring — index pairing vs. swapped Connection0/1 **[E, 2026-06-05] ✅ FIXED**
With the CTD gone (§6.10), reversed runs built without crashing but **mis-wired**. Live connection
inspection showed connected "peers" whose connectors were **~100 m apart** (e.g.
`…459653::ConveyorAny1` @ x=337600 paired with `…459649::ConveyorAny0` @ x=327600), while the
connectors that physically meet at each junction were left split — one open, one wired to a third
belt. Player symptom: **cannot snap a new belt to the run ends** (the end connectors are "consumed"
by the bogus long-distance links).
- **Root cause:** the handler paired connectors **by index** — our `Connection0` ← predecessor's
  `Connection1`, our `Connection1` → successor's `Connection0`. That holds for forward belts (where
  `Connection0` sits at the low-index/pole_i end), but a **reversed belt has `Connection0`/`Connection1`
  swapped relative to position**, so the index pairing connects non-coincident connectors.
- **Fix:** wire by **geometric coincidence**, not index. Post-`Super::Construct` (when the built
  belt's connectors have real world positions — reliable, unlike at hologram `ConfigureComponents`,
  §6.6), connect each of our connectors to the **coincident** (≤1.5 m), free, `CanConnectTo`-compatible
  connector among **both** ends of each resolved run neighbour. Also pass `SetSnappedConnections(nullptr)`
  so vanilla can't pre-wire a wrong index pair. Forward runs are unaffected (their coincident connector
  is the same one the index used). Pre-existing mis-wired belts are not retro-fixed — dismantle+rebuild.
- **Lesson [I]:** connect by **geometry**, not by connector index, whenever build direction can vary —
  `Connection0`/`Connection1` are not a stable proxy for "this physical end."

### 6.12 Multi-segment gaps — placement-relative index can't identify neighbours **[E, 2026-06-05] ✅ FIXED**
With reversed wiring corrected (§6.11), a reversed run built in **multiple drags** still left a
junction unwired: at one shared point both abutting belts' connectors were **free + coincident** but
not connected to each other (e.g. z=5300 `…427911::Any1` and `…427915::Any0`, both open @ x=337600),
while the identical junction one level down (z=5100) *was* wired.
- **Root cause:** even the geometric §6.11 fix still found *candidate* neighbours via the registry
  keyed on `StackChainId`/`StackChainIndex`. Those are **placement-relative loop indices**
  (`SFAutoConnectService_Stackable.cpp` numbers from each drag's grid origin), so belts from separate
  build segments get **misaligned keys** — `Index±1` returns the wrong belt or none, and even
  `StackChainId` itself can differ for the same physical row across drags. The true neighbour was
  never offered as a candidate, so the junction stayed open.
- **Fix:** drop index/ChainId from neighbour-finding entirely. Keep a **flat session list** of built
  stacked belts (`GStackBuiltConveyors`, `TWeakObjectPtr`) and, post-`Super::Construct`, connect each
  of our connectors to the **coincident (≤1.5 m), free, `CanConnectTo`-compatible** connector on **any**
  belt in that list (`SF_WireStackConnectorByCoincidence`). Whichever belt builds second at a shared
  point wires it; run-ends stay open. Order-, direction-, and segment-independent. (Cost: a linear
  scan of stacked belts per built belt — fine at normal scale; a spatial index is the future option.)
- **Lesson [I]:** any *identity-derived* key for "are these two things adjacent?" is fragile when the
  identity is assigned per-operation. Adjacency is a property of **the world**, so resolve it from the
  world (coincident positions), not from bookkeeping indices.

### 6.13 Orphan chains — a degenerate class distinct from zombies **[E, 2026-06-05]**
Inspecting a run built before the §6.11/§6.12 fixes, three chain actors reported **`segments == 1`
but `totalLengthMeters == 0`**, two of them holding 42 phantom items. They were **not** flagged
zombie (zombie ≝ `GetNumChainSegments() == 0`) yet were clearly degenerate — chains whose belt(s)
contribute ~0 length, the by-product of the earlier cross-wired connectors (peers ~100 m apart).
- **Persistence:** they **survived dismantling** the run's belts (orphaned chain actors with no live
  belt), but were **cleared by save/reload** — vanilla rebuilds chain actors only for belts that
  still exist, so an orphan with no belt is simply not recreated, releasing its phantom items.
- **Tooling:** the live chain audit now flags `segments>=1 && totalLength≈0` as **`orphan`** (plus an
  orphan count), a class the `segments==0` zombie check missed.
- **Contrast:** belts built with the §6.12 fix dismantled **cleanly** (no orphan residue), so orphans
  here were legacy pre-fix damage, not a leak in the current path. Live purge if ever needed:
  `ForceDestroyChainActor` (1.2, safe-but-destructive — appropriate since there's no transport to keep).

### 6.14 Reload chain-stall on long, multi-segment / manually-joined runs **[E, 2026-06-05]**
**Symptom.** A very long stacked-pole run (several segments joined manually), built across a session,
flowed correctly while built. After **save + reload** it appeared dead: belts looked empty; the head
storage container read empty while coal visibly entered it (a pass-through that never buffered) and
the source backed up — yet *dismantling* a belt reported coal present. Recreating one output belt, or
a **second** save+reload, instantly restored flow.

**Investigation (live world-state inspection).**
- **No hoarder/origin sink.** A world-wide audit of every `AFGConveyorChainActor` (sorted by item
  count) showed ~558 chains, all with item counts proportional to length; no orphan, no zombie,
  nothing near world origin. The "infinite chain hoarding everything" hypothesis was disproven.
- **No destruction.** Total items across all chains *rose* over time (≈6,986 → 7,002 → 8,294) — items
  were **conserved**, not deleted.
- **Broken vs working belts were structurally identical.** The stalled upper output belt and the
  working lower one sat in equivalent 2-segment chains: same item count (54), same `bHasValidLUT`,
  same segment order, same connectivity. No static structural difference distinguished them.
- **The belt→chain back-pointer is unreliable across reload.** `AFGBuildableConveyorBase::GetConveyorChainActor()`
  returned null for belts that were genuine segments of live chains (and, post-launder, returned a
  *stale far-away* chain). Membership must be resolved from the chain side (`GetChainSegments()`),
  never the belt's back-pointer.
- **Error vs laundered state.** In the bad first-load the long run existed as **fragmented** chains
  (≈302 m / 251-item pieces) that were structurally valid but **not advancing items** (a tick/transport
  stall). After save+reload the same run **coalesced into single `RepSizeHuge` chains** (28 segments,
  ≈1,208 m, 1,008 items) that ticked and flowed.

**Mechanism [I].** Smart's stacked belts persist in a **fragmented, per-belt chain state** (connect-
after-register: each belt its own 1-segment chain at build, §6.9/§6.12). On the **first** load from
that save, vanilla's chain reconstruction yields chains that are structurally valid but in a
**non-ticking transport state** for the long/manually-joined run. The 2026-06-05 observation was that
**a rebuild** — recreating a belt, or a *second* save+reload — re-coalesces the run into a proper
multi-segment chain that ticks ("first reload stalls, second reload launders"). ⚠️ **Partially revised
2026-06-08 (§6.15):** a controlled single-reload retest was consistent with "first reload stalls" but
**did not coalesce** (the solo chains persisted with identical IDs). We did **not** retest a *second*
reload, so "second reload launders" is the 2026-06-05 observation, **not re-confirmed** — do not rely on
it. The fix does not depend on reload behavior either way (it coalesces at build time).

> ⚠️ **SEVERITY — risk of UNRECOVERABLE loss for game-limited items [maintainer, 2026-06-05].**
> In every observed case items were **conserved** (totals rose; dismantle reported the coal) — for
> **bulk** items the stall is recoverable (the workaround restores flow). **But finite, non-replaceable
> world items — Mercer Spheres, Somersloops — routed across such a run are at real risk of permanent
> loss:** any path that *destroys* rather than stalls an item (a degenerate chain dropping items, or an
> unlucky rebuild) would be **unrecoverable** — there is a fixed number of them on the map. This raises
> the preventive fix from a quality issue to a **data-integrity** one. **Do not route irreplaceable
> items over Smart stacked-pole runs until the build-time coalesce fix lands**; if you must, save+reload
> and verify counts before and after.
>
> **ESCALATION 2026-06-08 (§6.15): now crash-class, not only data-loss-risk.** The same fragmentation
> also **crashes** — connecting a vanilla end-belt makes vanilla merge the solo chains and leave one belt
> at `mChainSegmentIndex == -1`, asserting on the next factory tick once items flow. So a fresh stacked
> run is unsafe to *merge or flow*, not just to route irreplaceable items over. The "stall is recoverable"
> note above held for the bulk-item stall case; it does **not** cover the crash path.

**Workaround (revised 2026-06-08, §6.15).** The original guidance — "save + reload once and it coalesces
into a stable, ticking state" — is **not reliable**: a controlled single-reload retest did **not** coalesce
(the solo chains persisted with identical IDs). Do **not** treat one reload as a fix. Until the build-time
coalesce (§10) lands, the only safe guidance is: **do not connect/flow or route items over a freshly built
long stacked run** (a merge can crash it, §6.15), and verify with the live chain audit if in doubt.

### 6.15 Live re-characterization via SmartMCP chain audit + a second failure mode (CRASH) **[E, 2026-06-08]**
Controlled retest of §6.14 using the live `smartmcp_conveyor_chains` audit, isolating each step.

**Repro.** A **2-high × 4-segment** Stackable Conveyor Pole run, belt auto-connect ON (8 Mk6 belts).

**Build-time state — fragmentation confirmed directly.** Immediately after build the 8 belts registered as
**8 separate 1-segment chains** (one per belt, 40.5 m each) instead of the expected **2 four-segment
chains** (one per level). At rest every solo chain was internally *valid*: `zombie:0`, `orphan:0`,
`hasValidLUT:true`, each belt segment 0 of its own chain — so **no `-1` exists yet at build**.

**Reload does NOT self-heal (corrects §6.14 workaround).** Save-to-new-slot + reload once: still the
**same 8 solo chains, identical `AFGConveyorChainActor` IDs**, still `zombie/orphan:0`. A single reload
did not coalesce. So vanilla reconstruction is **not** a reliable laundering step — the build-time
coalesce is the mod's responsibility, not something reload can be trusted to fix.

**Second failure mode — CRASH, not just stall [E/I].** The §6.14 case stalled (items conserved). In this
retest the consequence was a **CTD**. The fragmented solo chains are *stable at rest*, but when vanilla
feeder/drain belts were connected to the run, vanilla **merged** the fragments into one ~15-segment chain
and left one belt at **`mChainSegmentIndex == INDEX_NONE` (-1)**. Once screws flowed, the chain tick
called `AFGConveyorChainActor::GetItemsForSegment()` → indexed **-1 into the 15-element segment array** →
`check(Index >= 0 && Index < ArrayNum)` assert → crash, on the `ParallelFor` `TickFactoryActors` path
(`FGConveyorChainActor.cpp:1527` ← `Factory_UpdateRadioactivity` ← `Factory_Tick`). So the same
fragmentation has **two** consequences depending on what touches it: **stall** (§6.14) if left alone, or
**crash** if a merge mis-assigns a segment index. This escalates the issue from data-integrity to
**crash-class**.

**Implication for the fix.** The build-time coalesce target is now measurable on this repro: after build,
the audit must show **2 chains × 4 segments** (not 8 × 1), `zombie/orphan:0`. ⚠️ **Iteration-1 inference
FALSIFIED (§6.16, 2026-06-08):** this section first claimed the §10 Option-2 merge precondition "holds"
and that `InvalidateAndRebuildForBelts → Migrate` "should yield 2 clean chains." A live test disproved it
— the union-find found the correct 2×4 groups *with correct connectivity*, but `MigrateConveyorGroupToChainActor`
/ `BuildChain` still produced 0-segment **zombies** (exactly what the §6.7 summary-table row 1 already
recorded). **Correct connectivity is necessary but not sufficient**; the post-build merge of freshly-built
stacked solo chains is a dead end. See §6.16 for the root cause and the corrected approach (defer
registration, mirror Extend). Tracked: #341.

**Fix (planned, see §10).** Coalesce each stacked run into a single proper chain **at build time** so
the saved state restores cleanly on first load — the long-standing "unify into one chain" item, now at
**data-integrity** priority because of the irreplaceable-item risk above.

### 6.7 Summary table
| # | Approach | Where | Result |
|---|---|---|---|
| P0a | `RemoveConveyorFromBucket` standalone | live belt | **crash** (next-tick ParallelFor) |
| P0b | `ForceDestroyChainActor` (+rebuild) | live chain | no crash, **transport destroyed** |
| P0c | `RebuildOnly` (RemoveChainActorFromConveyorGroup+Migrate) | existing chain | **works** |
| 1 | `InvalidateAndRebuildForBelts` merge | stacked timer | **zombies** |
| 2 | connect at construct via captured connectors | ConfigureComponents | **no-op** (hologram targets) |
| 3 | `RemoveConveyor`+`AddConveyor` re-register | stacked timer | **crash** |
| 4 | STACK-ORDER diagnostic | ConfigureComponents | geometry **not ready** at construct |
| 5 | preview-only child via `Construct`→`nullptr` | build gun | **crash** (`InternalConstructHologram:1862` null deref) |

### 6.16 Build-time coalesce — iteration 1 result, root cause, and the iteration-2 hypothesis **[E+I, 2026-06-08]**

**Iteration 1 (shipped to a dev build, live-tested): post-build merge → FAILED.** Hooked a debounced
post-build timer that fed the run's belts to `InvalidateAndRebuildForBelts` (`USFSubsystem::ProcessDeferredBeltRunCoalesce`).
Live log on the §6.15 repro (2-high × 4):
```
#341 BELT-RUN COALESCE: coalesced 8 belt(s) -> migrated 2 group(s)
ChainActorService: Zombie BuildChain still failed after belt-clear — InputEnd=…6655 OutputEnd=…6643 Belts=4 …
ChainActorService: Recovery failed for zombie TG … — detaching and re-queuing
```
The union-find was **correct** — 2 groups of 4 belts, correct head→tail (…6655→…6651→…6648→…6643),
correct input/output ends. But `MigrateConveyorGroupToChainActor` / `BuildChain` produced **0-segment
zombies**, recovery (`SetStartAndEndConveyors`+retry) also failed, and the zombie-purge then left the
8 belts with **no chain actor at all** (audit: run absent from the chain list; belts physically present).
This reproduces §6.7 summary-table **row 1** and falsifies §6.15's "correct connectivity ⇒ merge holds"
inference: connectivity was provably correct, yet `BuildChain` could not reconstitute the freshly-built
solo chains. **The post-build merge path is a confirmed dead end for stacked.**

**Root cause found [C].** `ASFConveyorBeltHologram::ConfigureComponents` *intends* to defer `AddConveyor`
for stacked belts, but the guard is keyed on the wrong string:
```cpp
// SFConveyorBeltHologram.cpp ~1764
bool bIsStackableBelt = BeltName.Contains(TEXT("StackableBelt_"));
```
`BeltName` here is the **built actor** name — `Build_ConveyorBeltMk6_C_<id>` — which never contains
`StackableBelt_` (that is the *hologram/preview* name). So the guard is always **false**, the belt falls
to the `else` and **`AddConveyor` runs immediately** (`:1778`). Because stacked belts are built
**simultaneously**, a belt registers before its neighbour exists → **register-then-connect → solo chain**
(the §1/§3 invariant violation). The deferral the code's own comment describes **never actually happened**;
none of the §6.7 rows tested true deferral-then-first-registration because of this bug.

**Iteration-2 hypothesis [I] — mirror Extend's deferral, don't merge after the fact.** Extend never
fragments because its belts (a) **defer `AddConveyor`** (`bIsExtendBelt` branch, `:1747-1757`) and
(b) register **with connections already in place** — `SFExtendWiringService_BuiltChild.cpp:1441-1510`
pre-sets `mSnappedConnectionComponents` before `FinishSpawningActor`, plus topology `SetConnection`, then
defers registration. Apply the same to stacked:
1. **Actually defer** `AddConveyor` for stacked built belts — detect via the `SF_StackableChild` **tag**
   (set in `Construct`), not the never-matching name.
2. **First-register when the run is fully wired** — in the debounced post-build pass, call `AddConveyor`
   on each belt (connections are by then established by `SF_WireStackConnectorByCoincidence`), so vanilla
   builds **one chain per series-run** the normal way — connect-*before*-register honoured. This is a
   *first* registration of never-added belts (NOT the §6.7 row-3 `RemoveConveyor`+`AddConveyor` re-register
   on live belts, which crashes), so it is the standard safe path.
   Replaces the iteration-1 `InvalidateAndRebuildForBelts`+zombie-purge (wrong tool: it merges *existing*
   chains; there are none to merge once deferral works).

**Prediction (the test that confirms/refutes).** After build on the §6.15 repro: audit shows **2 chains ×
4 segments**, `zombie/orphan:0`; connect feeder/drain + flow → **no crash**; save+reload → still 2 chains,
flows first load. **Risk:** if topology `SetConnection` (without snapped connections) is insufficient for
vanilla `AddConveyor` to unify the run, expect partial/solo chains again → iteration 3 adds snapped
connections (`mSnappedConnectionComponents`) to the stacked wiring as Extend does. Tracked: #341.

**Iteration 2 RESULT (live-tested 2026-06-08): CRASH on build — approach abandoned.** The deferral
fix worked (`#341 BELT-RUN REGISTER: first-registered 8 wired belt(s)` logged at frame 6), but the **next
factory tick crashed** (frame 7): `EXCEPTION_ACCESS_VIOLATION reading 0x000000020000009b` in
`AFGConveyorChainActor::Factory_Tick()` (`FGConveyorChainActor.cpp:270`), on the `ParallelFor`
`TickFactoryActors` path. Calling `AFGBuildableSubsystem::AddConveyor` **from a deferred timer** (a later
frame) is the §9-forbidden "bucket op off a timer" and matches §6.7 summary-table **row 3** (`re-register |
stacked timer | crash`) — even as a *first* registration. So both post-build strategies are now empirically
closed: **iteration 1 (chain-level merge) → zombies (row 1); iteration 2 (bucket-level register off timer)
→ crash (row 3).**

**Conclusion [I, 2026-06-08].** A freshly-built stacked run **cannot** be repaired after the construction
frame — not by merging the solo chains (BuildChain won't reconstitute them) nor by (re)registering off a
timer (ticks into garbage). The only path the evidence leaves open is Extend's: register each belt **within
the construction frame, before any factory tick, with connections already set** (Extend does this via the
`AFGBlueprintHologram::Construct`-AFTER SML hook). Stacked's simultaneous grid build has **no equivalent
synchronous "all belts built + wired, pre-tick" hook today** — providing one is the real (larger) prerequisite,
not another post-build tweak. Iteration-2 code reverted to the safe solo-chain status quo (stalls on reload,
but no crash on build); #341 returns to the parked state with this dead-end map recorded. Tracked: #341.

---

## 7. Analysis — why each window fails, and what survives

There are exactly three temporal windows in which one might wire a stacked belt:

1. **At construct (`ConfigureComponents`).** Neighbours not built; **own connectors not positioned**
   (§6.6). Wiring impossible regardless of mechanism. ✗
2. **Post-construct, by fixing the auto-registered live belts (timer).** Geometry & neighbours
   exist, but the belts are **live registered solo chains**: merging them → zombies (§6.2);
   `RemoveConveyor`/bucket-mutating them → ParallelFor crash (§6.5, P0). ✗
3. **Post-construct, by building belts FRESH.** `SpawnActor`'d belts don't auto-register; compute
   geometry from the now-built pole transforms, `Respline`, `SetConnection` to the run predecessor,
   `AddConveyor` last. Never touches a live chain; honors connect-then-register. ✓

Window 3 is the **only** survivor. It is not the "double-build" the distributor refactor removed
(that built belts *both* as children *and* post-build); stacked would build belts **only** fresh
(no constructing belt children).

---

## 8. The shipped stacked design — real belt + connect-by-coincidence at construct

> **Supersedes the earlier "Option B (preview-only + fresh `BuildBelt`)" proposal.** §6.8 showed a child
> hologram cannot be preview-only without ceasing to be a child; §6.9 ("THE FIX") therefore kept the
> **real** belt and solved the invariant by wiring it at construct. The preview-only +
> `GetCost`-reimplementation story was retired — see the "Earlier mis-conclusion" note in §9.

**What actually ships (per §6.9–§6.12 + code, `SFConveyorBeltHologram.cpp`).**
1. Belt child holograms **construct real belts** (tagged `SF_StackableChild`). Cost is charged normally
   by the build gun, and `ASFConveyorBeltHologram::GetCost` simply trusts vanilla `Super::GetCost`
   (`:751`, the #348 behaviour) — **no preview-only, no cost reimplementation**.
2. At `Construct`, each belt's connectors are wired to the correct run peers by **geometric coincidence**
   (`SF_WireStackConnectorByCoincidence`), *before* registration — **coincidence, not `StackChainId`/`Index`**:
   the indices are placement-relative and mis-paired reversed runs (§6.11/§6.12). Neighbour lookups use a
   weak-ptr registry (`GStackBuiltConveyors`) to avoid the §6.10 dangling-pointer CTD.
3. Chain creation is **deferred**, so a freshly built run is a set of **per-belt solo chains** until
   coalesced. Turning them into one multi-segment chain per run at build time is the **#341** work
   (§6.15, §10); without it the run stalls on reload and can **crash** on a later merge (§6.15).

**Spline shape is orthogonal.** `BeltRoutingMode` (Default/Curve/Straight) lives entirely in the
`SplineData` handed to `Respline`; it does not interact with chaining. The builder must *compute*
the shaped spline (via the routing-mode path, as the distributor preview does) — a naive straight
line would silently discard the option.

---

## 9. Generalization — "belts done properly everywhere"
The single contract for **all** Smart belt creation: *set connections to **real** targets — by
reference where geometry isn't ready — **before** the belt registers into the conveyor system
(`AddConveyor` / vanilla auto-register); never `AddConveyor`/`RemoveConveyor` on a **live** belt off
a timer; build multi-belt runs by reference, order-agnostic.*

**Full non-Extend belt-site audit (2026-06-05).** Every live site that creates or wires belts/chains
outside Extend was reviewed; **none needs new treatment** — all are compliant, now-fixed, or dead.
(Detailed table + dead-code list: `docs/Features/AutoConnect/TEST_BeltRework_Validation.md` §3.4.)
- **Stackable poles** — the lone structural violator → **fixed** by wiring the real belt at Construct
  (§5.3 resolution, §6.9), refined from by-reference to **by-coincidence** in §6.11/§6.12. ✗→✓
  (Build-time chain coalesce of the resulting solo chains is the open #341 work — §6.15/§10.)
- **Distributor → factory** (child holograms) — connect at vanilla child Construct; a narrow
  `OnActorSpawned` manifold timer only proximity-wires cross-link belts built before their target
  distributor (empirically zombie-free on reload). ✓
- **Conveyor lifts** (`SFConveyorLiftHologram`) — `SetConnection` then conditional `AddConveyor`;
  explicit "no double-add" guard; Extend lifts defer registration to chain rebuild. ✓
- **`SFChainActorService`** (mass-upgrade migration + connection repair) — bucket ops gated to
  player-initiated Repair in a stable world; union-find merge prevents solo tick-groups. ✓
- **Extend family** — two-phase chain model, connect-by-reference. ✓
- **Pipes** — parallel system, own routing modes, **no chain actors**; same connect-before-register
  discipline applies but no ParallelFor/chain hazard. ✓

**Dead code surfaced (remove in a build-verified follow-up; spans multiple files, not cut blind):**
`USFSubsystem::QueueChainRebuild` (crash-class, never called), `BuildBeltFromPreview` (correct but
unused), `BuildBeltsForDistributor` (superseded by child holograms), and the now-write-only
`CacheStackableBeltPreviewsForBuild` producer + `SFSubsystemStackableCache.h` + its
`SFAutoConnectOrchestrator.cpp:531` call.

**Earlier mis-conclusion (corrected):** §9 previously prescribed a `SpawnActor` preview-only
"Option B" with manual cost reimplementation as the *only* safe path. That was wrong — connecting
**by reference** inside the real hologram build (Extend's model) satisfies the invariant without
preview-only or cost reimplementation. The `SpawnActor`→`Respline`→`SetConnection`→`AddConveyor`
sequence remains valid (it's what `BuildBeltFromPreview`/Extend manifold use), but is **not** the
only option.

---

## 10. Open questions / future work
- **★ Build-time chain coalesce (DATA-INTEGRITY + CRASH priority) — RESUMED 2026-06-08; implementing on
  `feature/341-belt-run-chain-coalesce`.** Coalesce each stacked run into one proper multi-segment chain
  **at build time** so the saved state restores cleanly on first load and the reload tick-stall (§6.14)
  can't occur. Elevated to **data-integrity** because a finite, non-replaceable world item (Mercer Sphere,
  Somersloop) routed over a stall-prone run risks **permanent, unrecoverable loss** (§6.14 severity note).
  **Further escalated 2026-06-08 (§6.15): the same fragmentation also CRASHES (CTD)** when a vanilla merge
  mis-assigns a belt's `mChainSegmentIndex` to -1 and items flow — so this is now crash-class, not only a
  reload-stall. The §6.15 retest also showed a **single reload does NOT coalesce**, so the "reload once"
  workaround is unverified; until the fix lands, do not move irreplaceable items over Smart stacked runs,
  and avoid connecting/flowing a freshly built long run without verifying the chain audit.
  - **Scope (§6.15 + pole-type audit):** applies to **all grid belt-support runs** — Stackable, Wall, and
    Ceiling mounts share the same deferred-wiring path — and the **distributor auto-connect** path is a
    suspected (not yet confirmed) sibling. Extend is already correct (`CreateChainActors`).
  - **Connectivity confirmed, but merge still fails (§6.15 → §6.16, REVISED 2026-06-08):** the run's belts
    are correctly connected in series (items flowed end-to-end pre-crash); they are merely in separate
    chain actors. We *inferred* that made the Option-2 union-find + Migrate merge precondition hold — and
    **iteration 1 disproved it** (§6.16): with correct connectivity, `BuildChain` still produced 0-segment
    zombies. Correct connectivity is necessary, not sufficient. **The post-build merge is abandoned.**
  - **Chosen approach (§6.16): defer registration, mirror Extend — NOT post-build merge.** Root cause is a
    name-check bug (`bIsStackableBelt = BeltName.Contains("StackableBelt_")`, never true for the built
    actor) so stacked belts never deferred `AddConveyor` and always register-then-connected into solo
    chains. Fix: defer `AddConveyor` via the `SF_StackableChild` tag, then first-register each belt once the
    run is fully wired so vanilla builds one chain (connect-before-register honoured). The two candidate
    *post-build rebuild* primitives below are retained only as historical analysis — **both are superseded
    by the deferral approach.**
    - **Option 2: `InvalidateAndRebuildForBelts → InvalidateAndRebuildChains`** — the §6.7
      P0c "RebuildOnly" path (`RemoveChainActorFromConveyorGroup` + Phase 2.5 union-find merge +
      synchronous `Migrate`). §4.3 calls it the one path "empirically proven to rebuild chains cleanly" —
      **for *existing* chains (Upgrade/Extend), not freshly-built stacked solo chains: iteration 1 (§6.16)
      showed it zombies here even with correct connectivity.** Never touches `RemoveConveyor` on a live
      belt; worst case = a NO_SEGMENTS zombie.
    - **Option 1: `ReRegisterAndQueueVanillaRebuildForBelts`** — detaches chains, then `RemoveConveyor`
      +`AddConveyor` per belt and lets vanilla rebuild next frame. This is what Mass Upgrade uses today
      (it abandoned manual coalescing because it produced zombies at 100s-of-belts scale). **But** it does
      `RemoveConveyor` on live belts — the op §2.7/§6.5/§6.7-row3/§10 repeatedly flag as crash-class.
      Its survival in Upgrade likely rests on detach-chains-first ordering + a settled, player-initiated
      context. Worst case = **CTD**.
  - **Decision:** implement **Option 2** from a debounced post-build timer (settled, game thread) +
    `ScheduleDeferredZombiePurge` net. **Pass criterion (revised 2026-06-08, §6.15):** the live chain
    audit after build must show the run as **one multi-segment chain per series-run** (for the §6.15
    repro: **2 chains × 4 segments**, not 8 × 1), `zombie/orphan:0`. Do **not** use "survives first reload"
    as the criterion — §6.15 showed vanilla reload is unreliable; reload stability must be **re-tested
    post-fix**, not assumed. Rationale: for a *don't-lose-irreplaceable-items* fix, a recoverable zombie
    beats a crash. Only fall back to Option 1 if Option 2 still zombies at stacked-run scale — and treat
    that as a signal to first finish the Upgrade audit below.
- **Audit the Upgrade re-register path (`ReRegisterAndQueueVanillaRebuildForBelts`).** Upgrade is the one
  live site deliberately using `RemoveConveyor` on live belts (the thesis crash-class op). It mitigates
  via detach-chains-first + settled timing, but this needs an explicit "does the detach-first ordering
  truly close the ParallelFor race, or is it a latent CTD on unlucky timing?" review. See
  `SFUpgradeExecutionService.cpp:1622-1638` (the switch to re-register, made because manual coalescing
  produced NO_SEGMENTS zombies) and `SFChainActorService.cpp:1455` (`ReRegisterAndQueueVanillaRebuildForBelts`).
  **Both items tracked in the backlog: majormer/SmartFoundations#341.**
- **Dead-code removal — ✅ DONE (2026-06-05, build-verified).** Removed the orphaned belt machinery
  surfaced by the §9 audit: the `QueueChainRebuild` / `CollectChainBelts` / `ExecuteDeferredChainRebuild`
  cluster (crash-class `RemoveConveyor`/`AddConveyor` on live belts, never called) + its members
  (`PendingChainRebuilds`, `ChainRebuildTimerHandle`, and its `ClearTimer` in `SFSubsystem.cpp`);
  `BuildBeltFromPreview`; `BuildBeltsForDistributor`; and the write-only `CacheStackableBeltPreviewsForBuild`
  producer + its `SFAutoConnectOrchestrator` call + the `FStackableBeltBuildData` cache
  (`SFSubsystemStackableCache.h` retained only for `bProcessingGridPlacement`, still in use). Note:
  the audit task under-specified the cluster — it named only `QueueChainRebuild`; the two helpers and
  the `SFSubsystem.cpp` `ClearTimer` had to be found by re-grep. Compiled + linked clean (Win64 Shipping).
- **Stacked-belt edge cases not yet tested:** tall runs (5+ levels), Curve routing mode, dismantle
  teardown (TEST plan 2.3/2.4/2.7).
- **Construct-order control.** If vanilla could be made to build stacked belt children
  predecessor-first *and* finalize geometry before `ConfigureComponents`, window 1 might reopen —
  unverified and likely not worth it vs. window 3.
- **`ForceDestroyChainActor` for zombie purge.** 1.2-new, proven safe-but-destructive — appropriate
  *only* for NO_SEGMENTS zombies (nothing to preserve). Could replace the deferred zombie-purge.
- **Multiplayer.** All belt construction must run on **authority**; chain replication is vanilla's
  (`_RepSize*` subclasses). Smart's single-player assumptions need a separate audit.
- **Lifts & pipes** parallel the belt story; lifts are `AFGBuildableConveyorBase` (same rules),
  pipes have no chain actors.

---

## Appendix A — File:line index (verified this investigation)
- `FGFactoryConnectionComponent.h`: `:28-34` directions, `:90` SetConnection, `:96` GetConnection, `:111` IsConnected, `:122` GetDirection.
- `FGBuildableConveyorBelt.h`: `:92` Split, `:98` Merge, `:103` Respline, `:108-112` spline accessors.
- `FGBuildableConveyorBase.h`: `:159` GetLength, `:163-164` GetConnection0/1, `:184` GetConveyorChainActor.
- `FGBuildableSubsystem.h`: `:265` Migrate, `:272` RemoveChainActorFromConveyorGroup, `:275` AddConveyor, `:287` RemoveConveyorChainActor, `:293` RemoveConveyor, `:296` ForceDestroyChainActor (NEW), `:299` SplitConveyorGroupFromAttachment, `:306` RemoveConveyorFromBucket, `:309` RearrangeConveyorBuckets, `:315` RemoveAndSplitConveyorBucket.
- `FGBuildable.h`: `:498` GetCostMultiplierForLength.
- `FGHologram.h`: `:350` GetBaseCost, `:360` GetCost(includeChildren).
- `SFChainActorService.cpp`: `:143` Phase 1, `:183` Phase 2, `:191-326` Phase 2.5, `:328+` Phase 3, `:471-481` zombie-clear, `:536-554` recovery, `:1036` InvalidateAndRebuildForBelts.
- `SFConveyorBeltHologram.cpp`: `:269` PostHologramPlacement, `:448-456` clone-ID registration, `:634` GetCost override, `:1297` ConfigureComponents, `:1348` stackable branch, `:1466+` clone-ID wiring.
- `SFAutoConnectService_Stackable.cpp`: `:55` ProcessStackableConveyorPoles, `:1399` UpdateOrCreateBeltForPolePair, `:1606-1607` captured-but-unused connectors, `:1634` AddChild.
- `SFAutoConnectService_Belt.cpp`: `BuildBeltFromPreview`/`BuildBeltsForDistributor` (dead code).
- `SFConveyorAttachmentHologram.cpp`: `:18` child-hologram refactor note, `:26` GetCost override.
- `SFExtendService.cpp`: `:879` CanAffordExtendCost, `:1821` GetBuiltActorByCloneId.

## Appendix B — Log grep tokens
`STACK-PROBE` (post-build chain verdict), `STACK-PREPROBE` (pre-rebuild belt state), `STACK-WIRE`
(captured-connector wiring), `STACK-ORDER` (construct-timing), `LogConveyorChain: Warning: Last … !=
mLastConv …` (zombie BuildChain), `ChainActorService:` (service phase summary).

## Appendix C — Glossary
**Chain actor** — `AFGConveyorChainActor`, one actor ticking a run of belts. **Solo chain** — a
1-segment chain for a belt registered while unconnected. **Zombie** — a chain actor with
`GetNumChainSegments()==0` (exists, transports nothing). **Tick group / bucket** —
`FConveyorTickGroup` in `mConveyorTickGroup`. **SPLIT_CHAIN** — connected belts that ended up in
distinct chains instead of one. **Connect-then-register** — set connections before `AddConveyor`.
**Run** — a contiguous line of belts that should form one chain.
