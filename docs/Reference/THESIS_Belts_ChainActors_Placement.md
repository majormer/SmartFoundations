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

**The structural defect:** all belts (and poles) in a stacked placement are built **simultaneously**.
At any belt's construct, (a) its sibling belts don't exist yet or aren't geometrically ready, and
(b) its own connectors aren't positioned (§4.1). So the belt **cannot** connect-then-register; it
register-then-(maybe-later-)connects → the invariant of §3 is violated by construction.

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
The single contract for **all** Smart belt creation: *compute shaped spline → `SpawnActor` →
`Respline` → `SetConnection` to **real** targets → `AddConveyor` last; build multi-belt runs in
order.* Audit:
- Distributor (flat), Extend, normal belts, lifts → already connect-then-register (stable targets). ✓
- Stackable poles → the lone structural violator (simultaneous build). Fix = Option B. ✗→✓
- `SFSubsystem_AutoConnect.cpp:793` `RemoveConveyor`/`AddConveyor` "bounce" → review (same hazard class). ⚠
- Pipes → parallel system, own routing modes, **no chain actors** (simpler); same "shape-aware,
  connect-before-register" discipline applies.

---

## 10. Open questions / future work
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
