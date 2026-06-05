# Conveyor Belts, Chain Actors, and Belt Placement in Satisfactory 1.2 — A Working Thesis

**Subject:** the complete model of how conveyor belts, conveyor chain actors, conveyor poles,
and Smart!'s auto-connect placement interact on Satisfactory 1.2 (game CL 491125, UE 5.6.1-CSS),
and why stacked-pole belt auto-connect is hard.

**Status:** living reference. Consolidates the investigation of 2026-06-05 (and the prior P0
chain-actor characterization). Supersedes scattered notes; companion to
`docs/Features/AutoConnect/DESIGN_StackablePole_FromScratch.md` (the chosen fix) and
`docs/Sprints/ChainActorMigrationPlan.md` (local; the P0 record).

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
> outside a complete vanilla operation **crashes** on the next parallel factory tick.

Smart!'s belt auto-connect features (distributor→factory, Extend, stackable poles) all reduce to
the problem of honoring that invariant. Distributor and Extend belts satisfy it naturally because
their endpoints are **stable, pre-existing** buildables at construct time. **Stackable-pole belts
violate it structurally**: the belts in a placement are built *simultaneously*, so a belt's
neighbours don't exist (and its own connector geometry isn't even finalized) when it would need
to connect. Six in-game experiments narrowed the solution space to exactly one viable design:
**preview-only belt holograms + fresh `BuildBelt` construction on confirm**, with cost charged via
a hologram `GetCost` override. This document records the model, the experiments, and the analysis
in full.

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
This works for **existing** chains (mass-upgrade, Extend). It does **not** save the stacked case
(see §6) because those belts arrive as multiple **live solo chains** the merge cannot reconstitute.

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
- **SmartMCP validation:** `/api/connections` shows both ends wired to the correct run peers;
  `/api/conveyor-chains` shows `zombieCount:0`, all LUTs valid.
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
With the CTD gone (§6.10), reversed runs built without crashing but **mis-wired**. SmartMCP
`/api/connections` showed connected "peers" whose connectors were **~100 m apart** (e.g.
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

## 8. The surviving design (Option B) — and its cost story

**Preview-only belt holograms + fresh `BuildBelt` on confirm.** (Full plan:
`DESIGN_StackablePole_FromScratch.md` §9.8.)
1. Belt child holograms remain **visual previews + cost contributors** but are overridden to
   **construct no buildable** (`SF_StackableChild`). No auto-register → no crash, no zombie.
2. On confirm, `BuildStack` computes each span's shaped spline from real pole transforms and calls
   `BuildBelt` (`SpawnActor→Respline→SetConnection`(predecessor)`→AddConveyor`) in **run order**, so
   the engine grows one correct chain per run.
3. **Cost** (the one thing fresh-build loses, since `SpawnActor` doesn't bill): a `GetCost` override
   on the stackable-pole hologram adds spanned-belt cost (mirror `ASFConveyorAttachmentHologram::GetCost`),
   gated by `UFGCDUnaffordable` + `CanAffordExtendCost`-style availability (inventory + central
   storage), respecting `GetCheatNoCost`. This is **reuse of an existing pattern**, not new cost code.

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
- **Stackable poles** — the lone structural violator → **fixed** via connect-by-reference at Construct
  (§5.3 resolution, §6.9). ✗→✓
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
- **Dead-code removal (build-verified).** Remove the orphaned belt machinery surfaced by the §9
  audit: `QueueChainRebuild` (crash-class, never called), `BuildBeltFromPreview`,
  `BuildBeltsForDistributor`, and the write-only `CacheStackableBeltPreviewsForBuild` producer
  (+ `SFSubsystemStackableCache.h` + the `SFAutoConnectOrchestrator.cpp:531` call). Cross-file, so
  do it with a compile (the two `OnActorSpawned` `if(false)` consumers were already removed in
  `20b39fd`).
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
mLastConv …` (zombie BuildChain), `ChainActorService:` (service phase summary), `LogSmartMCP`
(unrelated dev bridge).

## Appendix C — Glossary
**Chain actor** — `AFGConveyorChainActor`, one actor ticking a run of belts. **Solo chain** — a
1-segment chain for a belt registered while unconnected. **Zombie** — a chain actor with
`GetNumChainSegments()==0` (exists, transports nothing). **Tick group / bucket** —
`FConveyorTickGroup` in `mConveyorTickGroup`. **SPLIT_CHAIN** — connected belts that ended up in
distinct chains instead of one. **Connect-then-register** — set connections before `AddConveyor`.
**Run** — a contiguous line of belts that should form one chain.
