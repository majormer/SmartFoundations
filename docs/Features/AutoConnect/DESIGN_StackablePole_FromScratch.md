# Stackable-Pole Auto-Connect — Clean-Slate Design (Satisfactory 1.2 / SML 3.12)

> Written 2026-06-05, deliberately **ignoring the current implementation**. The goal is to
> describe how stacked-pole belt auto-connect *should* be built from first principles on the
> 1.2 headers, then use it to judge/replace what exists. Diagnosis of why the current code
> fails is in `docs/Sprints/ChainActorMigrationPlan.md`; the one-line summary is: **the current
> code registers each belt into the conveyor system before anything it can connect to is real,
> then tries to merge the resulting solo chains — which vanilla refuses to do.**

---

## 1. What the feature actually has to produce

Player places a row of stackable conveyor poles (each pole is N levels tall). The feature must,
in one build action, create conveyor belts spanning **consecutive poles at each level**, such
that every resulting belt run is:

- a single, correctly-formed **chain actor** (not split, not a zombie),
- **transporting items** end-to-end,
- **save/reload-stable** (no one-frame null-chain window for a save to capture),
- created without crashes in the parallel factory tick.

## 2. Domain model from the 1.2 headers (the facts the design must respect)

| Concept | 1.2 API (header) | What it means for us |
|---|---|---|
| Belt = item-flow edge | `AFGBuildableConveyorBase::GetConnection0/1()` (`FGBuildableConveyorBase.h:163-164`) | `Conn0` = **input**, `Conn1` = **output**. Items flow `Conn0 → belt → Conn1`. |
| Belt geometry | `AFGBuildableConveyorBelt::Respline(belt, TArray<FSplinePointData>)` (`FGBuildableConveyorBelt.h:103`) | The canonical way to give a spawned belt its shape. Returns the (possibly re-created) belt. |
| Connection | `UFGFactoryConnectionComponent::SetConnection/ClearConnection/IsConnected` (`FGFactoryConnectionComponent.h:90,104,111`) | Wire two connectors. **Directions:** `FCD_INPUT/OUTPUT/ANY/SNAP_ONLY` (`:28-34`). |
| **Pole = snap support, NOT item node** | `FCD_SNAP_ONLY` — *"Special case for conveyor poles"* (`FGFactoryConnectionComponent.h:33`); `AFGBuildablePoleConveyor`, `AFGBuildablePoleStackable` | **Poles do not carry items.** A belt snaps to a pole for *height/support* only; the item connection is **belt↔belt** (or belt↔machine). Poles never belong to a chain. |
| Registration / chaining | `AFGBuildableSubsystem::AddConveyor(belt)` (`FGBuildableSubsystem.h:275`) | Registers a belt **and** assigns/extends its chain **from its current connections**. Internally drives `MigrateConveyorGroupToChainActor`. |
| Vanilla stacked poles | `AFGConveyorMultiPoleHologram` (`Hologram/FGConveyorMultiPoleHologram.h`) | Vanilla already has a multi-pole hologram — **investigate reuse before reimplementing** (see §7). |

### The one invariant that dictates the whole design
From `BuildBeltFromPreview` (`SFAutoConnectService_Belt.cpp:1169-1175`) and Extend's
`CreateManifoldBelt`, both of which work today, and stated in vanilla's own comment:

> **Set a belt's connections BEFORE calling `AddConveyor`.** `AddConveyor` reads the
> connections to decide which chain the belt joins. A belt registered while unconnected
> becomes its own solo chain that **cannot** be merged afterward.

Everything below is just "honor that invariant for the stacked case."

## 3. Core principle — **connect-then-register, in dependency order**

Build the structure in an order where, at the moment each belt is registered, **every
connector it must attach to already exists as a real buildable**. Then each belt is born into
the correct chain and nothing ever needs merging, tearing down, or a deferred fix-up tick.

```
BuildStack(poleSpecs, levels):
    # Phase A — supports first (snap-only, independent, never chained)
    builtPoles = [ Construct(pole) for pole in poleSpecs ]      # real buildables now exist

    # Phase B — belts, per level, in run order
    for level in levels:
        prevBelt = ResolveRunEntryConnector(level)   # machine/existing belt, or null
        for i in range(len(builtPoles) - 1):
            a = builtPoles[i].SnapPointAtLevel(level)            # FCD_SNAP_ONLY transforms
            b = builtPoles[i+1].SnapPointAtLevel(level)
            spline = ShapedSpline(a, b, settings.BeltRoutingMode)   # honor Default/Curve/Straight — see §8

            belt = World.SpawnActor(BeltClass, a.Location)
            belt = AFGBuildableConveyorBelt::Respline(belt, spline)   # geometry
            belt->OnBuildEffectFinished()

            # connect-then-register (the invariant)
            if prevBelt: belt->GetConnection0()->SetConnection(prevBelt->GetConnection1())
            # (run-end machine/world connectors wired here too, when present)
            BuildableSubsystem->AddConveyor(belt)                # registers INTO the right chain

            SnapBeltToPole(belt, a); SnapBeltToPole(belt, b)     # support only (FCD_SNAP_ONLY)
            prevBelt = belt
```

- **Poles are built first and never enter a chain** — they're `FCD_SNAP_ONLY`. They exist only
  to position belts; whether they're built before or after belts is irrelevant to chains, but
  building them first lets belt geometry reference real snap points.
- **Belts are built in run order**, each connected to the already-registered previous belt
  *before* its own `AddConveyor`. Vanilla therefore extends one growing chain per run — exactly
  what happens when a player drags belts pole-to-pole by hand.
- **No `SFChainActorService`, no `InvalidateAndRebuildForBelts`, no `OnActorSpawned`
  `PendingStackableBelts` timer, no zombie purge.** Those exist only to repair the damage of
  the register-first ordering; remove the cause and the entire repair layer is unnecessary.

## 4. Why this is immune to every failure we observed

| Failure in current code | Why it can't happen here |
|---|---|
| Belt registered unconnected → solo 1-belt chain | Belt is connected to its real neighbor *before* `AddConveyor`. |
| Retroactive merge → `Last != mLastConv` zombie | No merge step exists; chains are formed correctly on first registration. |
| Connect target is a hologram (`STACK-WIRE` no-op) | Targets are **built buildables** by construction order, never holograms. |
| One-frame null-chain window captured by a save | Chains are valid at the end of the synchronous build; no deferred tick. |
| ParallelFor crash from bucket mutation on a live chain | We only ever `AddConveyor` fresh belts with connections set — the sanctioned path. |

## 5. Where it lives (lifecycle)

A single owning call — e.g. `USFStackBuildService::BuildStack(...)` — invoked **once** when the
player confirms the placement, running the dependency-ordered sequence above. This replaces the
current spread-out machinery:

- ❌ child `ASFConveyorBeltHologram` spawns that auto-register via the build-gun `Construct`
- ❌ `ConfigureComponents` proximity search / deferral
- ❌ `OnActorSpawned` `PendingStackableBelts` next-tick timer + proximity belt-to-belt wiring
- ❌ `SFChainActorService` merge/zombie-recovery in the hot path

…with one deterministic builder that owns the belts' full lifecycle (spawn → respline →
connect → register), exactly like `BuildBeltFromPreview` already does for the flat case. In
effect: **generalize the working flat-belt builder to N belts across M poles**, rather than
react to vanilla's hologram construction after the fact.

## 6. Edge cases & how the model handles them

- **Run ends meeting existing world infrastructure** (a machine or pre-existing belt): the end
  connector is a real connector → same rule, `SetConnection` before `AddConveyor`.
- **Multiple stack levels** = multiple independent runs; build each run fully and independently.
- **Belt tier / routing**: tier from runtime settings (as today); spline from the pole snap
  transforms; reuse `TryUseBuildModeRouting` for curve quality if desired.
- **Dismantle/undo**: chains are ordinary vanilla chains, so vanilla dismantle handles teardown
  — no Smart-side chain bookkeeping to unwind.
- **Multiplayer / dedicated server**: do the whole build on **authority**; `SpawnActor` +
  `AddConveyor` mirror vanilla construction, so the 1.2 `AFGConveyorChainActor_RepSize*`
  replication subclasses replicate the result normally. (No client-side chain surgery.)
- **TObjectPtr correctness (1.2/UHT)**: hold belts/poles in `TObjectPtr` only where they are
  `UPROPERTY` members; transient locals in the build loop stay raw.

## 7. Before building: check what vanilla already does

`AFGConveyorMultiPoleHologram` exists in 1.2. Two questions to answer first (cheap, high-value):

1. Does vanilla's multi-pole hologram already place stacked poles **and run belts between
   them** correctly (i.e., is the feature partly native now)? If so, Smart's role shrinks to
   driving/extending the vanilla path instead of hand-spawning belts.
2. If not, does a vanilla "build belt between two connectors" hologram path exist that we can
   invoke programmatically end-to-end (which would give us correct chains for free, the way a
   player's drag does)? `BuildBeltFromPreview` is our own minimal version of this; vanilla may
   expose a cleaner entry point in 1.2.

Either answer is a win: reuse the native path if it exists, otherwise ship the §3 builder,
which is a small, deterministic generalization of code that already works.

### Cheap-check result (2026-06-05) — no vanilla reuse; build the §3 builder
1. **`AFGConveyorMultiPoleHologram` is an empty placeholder** — header has no members/methods,
   `.cpp` is just the `#include` (143 bytes). Not a functional stacked-pole+belt feature
   (likely an unfinished CSS stub). **No reuse.**
2. **No high-level belt-builder API.** The only public belt factory primitives are
   `AFGBuildableConveyorBelt::Split` / `Merge` / `Respline` (`FGBuildableConveyorBelt.h:92,98,103`)
   — the sanctioned mod-facing topology set. There is **no** one-call "build belt between two
   connectors"; the vanilla belt hologram is the interactive build-gun path only.

**Conclusion:** `SpawnActor → Respline → SetConnection → AddConveyor` is the primitive sequence,
and `BuildBeltFromPreview` already proves it works. Ship the §3 builder; it stays entirely
within sanctioned vanilla primitives (`Respline`/`AddConveyor`, optionally `Split`/`Merge`).

## 8. Belt spline SHAPE — orthogonal to chaining, but the builder must compute it

We expose a `BeltRoutingMode` option (`Default / Curve / Straight`;
`SmartFoundationsModConfiguration.cpp:32-33`). It is critical to see that **shape and chaining
are independent axes**:

- **Shape** lives entirely in the `TArray<FSplinePointData>` you hand to
  `AFGBuildableConveyorBelt::Respline`. It does not touch connections or chains.
- **Chaining** is the `SetConnection`-before-`AddConveyor` ordering. It does not touch geometry.

So connect-then-register does **not** change, break, or constrain the shape options — *provided
the builder actually computes the shaped spline.* The routing mode becomes geometry via the
belt hologram's router: `SetRoutingMode(mode)` → `TryUseBuildModeRouting()` /
`AutoRouteSplineWithNormals()` → `GetSplinePointData()` (see `BeltPreviewHelper.cpp:111-174`).
That is exactly what the **flat distributor path already does**: a preview hologram produces the
shaped spline, the points are extracted, and `BuildBeltFromPreview` spawns + `Respline`s with
them. Shape preserved, connect-then-register honored.

**Design consequence (the trap):** a naive stacked builder that draws a straight line between
poles would silently **discard the Curve/Default option**. `ShapedSpline()` in §3 must route
through the same `SetRoutingMode` + `TryUseBuildModeRouting` path (i.e. reuse
`FBeltPreviewHelper`) to produce the spline points, then build via connect-then-register. Shape
is computed once, up front; registration is unchanged.

## 9. Apply the contract everywhere — one belt builder, per-feature audit

The real lesson of this investigation is bigger than stacked poles: **every belt Smart creates
should go through one shared, shape-aware, connect-then-register builder** instead of each
feature re-deriving belt creation. The single contract:

> compute shaped spline (routing mode) → `SpawnActor` → `Respline` → `SetConnection` to **real**
> neighbour/endpoint connectors → `AddConveyor` **last**; build multi-belt runs in order so
> each belt extends the previous belt's chain.

Audit of the current belt-registration sites (each `AddConveyor`) against that contract:

| Feature | Site | Pattern today | Verdict |
|---|---|---|---|
| **Distributor → factory (flat)** | `SFAutoConnectService_Belt.cpp:1179` (`BuildBeltFromPreview`) | preview→SplineData→spawn→Respline→SetConnection→AddConveyor | ✅ **Reference.** Shape-aware + connect-then-register. Already correct. |
| **Extend manifold** | `SFExtendWiringService_Manifold.cpp:657` | SetConnection→AddConveyor | ✅ correct |
| **Extend manifest / built-child** | `SFWiringManifest.cpp:1282` | service-routed | ✅ (Extend family; verify shape carried) |
| **Belt hologram, normal belt** | `SFConveyorBeltHologram.cpp:1637` | AddConveyor after connections made | ✅ correct |
| **Conveyor lift (normal)** | `SFConveyorLiftHologram.cpp:468` | AddConveyor only when `bMadeConnection` | ✅ correct; EXTEND lifts defer to chain rebuild |
| **Stacked-pole belt** | (no explicit AddConveyor — vanilla auto-registers via hologram `Construct`) | register-first → proximity wire → merge | ❌ **the outlier.** The only path that lets vanilla register the belt before connections exist. Fix per §3. |
| **AutoConnect bounce** | `SFSubsystem_AutoConnect.cpp:793` | `RemoveConveyor`+`AddConveyor` bounce | ⚠️ review — re-registration bounce, not the clean contract |
| **Startup migration** | `SFGameInstanceModule.cpp:270` | startup AddConveyor | ⚠️ low-risk; confirm it runs before chains tick |
| **Upgrade re-register** | `SFChainActorService.cpp:1572` | service-owned RemoveConveyor/AddConveyor | ✅ inside the compliant service boundary |

Takeaways:
- **Distributor→factory is already done right** — it is the template, not a problem.
- **Stacked poles are the lone structural outlier** (register-first). The §3 builder brings it
  onto the same contract.
- A **shared `BuildBelt(shapedSpline, conn0, conn1)` helper** (generalized from
  `BuildBeltFromPreview`) would make the contract impossible to violate per-feature and would
  collapse the duplicated belt-creation code — the highest-leverage cleanup once stacked is fixed.
- **Pipes** are a parallel system with their own routing modes (`Auto/2D/Straight/Curve/Noodle/
  H-to-V`, `SmartFoundationsModConfiguration.cpp:47-48`) and many `SetRoutingMode` sites in
  `SFPipeAutoConnectManager*`; the same "one shape-aware builder, connect before register"
  discipline applies there, though pipes have no chain actors (simpler).

## 9.5 Material cost — a solved pattern, not a reimplementation (investigated 2026-06-05)

Because `BuildBelt` (`SpawnActor`) doesn't go through the build-gun construct, vanilla doesn't
auto-charge for it — so preview-only belts need cost handled explicitly. **The codebase already
does exactly this for every other Smart-built logistics piece**, so we reuse, not reinvent:

- **Charging** — a hologram `GetCost(bool includeChildren)` override that adds the Smart-built
  piece's cost into the total vanilla deducts on placement. Templates: `ASFConveyorBeltHologram::
  GetCost` (`:634`), `ASFConveyorAttachmentHologram::GetCost` (`:26` — the distributor injects
  its belt costs this way), `ASFPipelineHologram::GetCost`, `ASFWireHologram::GetCost`. Belt cost
  itself scales via vanilla `AFGBuildable::GetCostMultiplierForLength(totalLength, segmentLength)`.
- **Affordability / invalidation** — `AddConstructDisqualifier(UFGCDUnaffordable::StaticClass())`
  on the parent hologram when materials are short (vanilla then blocks the build). Used by belt,
  lift, pipe, attachment, wire, power-pole, factory holograms. Reusable check: `CanAffordExtendCost`
  (`SFExtendService.cpp:879`) = compare `Hologram->GetCost(true)` against player inventory **+**
  `AFGCentralStorageSubsystem` (dimensional depot). Respect the `GetCheatNoCost()` cheat.

**So the stacked design charges materials correctly with existing machinery:** keep the belt
**previews contributing cost** (the stackable-pole hologram's `GetCost` includes the spanned
belts — via child aggregation if they stay children, or a `GetCost` override mirroring the
distributor), and add the standard `UFGCDUnaffordable` disqualifier when short. Vanilla deducts
the aggregated total on confirm; `BuildBelt` then creates the real belts for free (no double
charge, since `SpawnActor` doesn't bill). The distributor path already operates exactly this way
(preview cost + `BuildBeltFromPreview` build), so stacked is bringing one more feature onto the
proven model — not inventing cost code.

## 9.6 Why the timer approaches failed (record)
- `RemoveConveyor`/`AddConveyor` on **live registered** belts → `Factory_Tick` ParallelFor crash
  (orphaned solo chain left in the tick path). Bucket primitives are unsafe on live belts (P0).
- `InvalidateAndRebuildForBelts` merge of solo chains → zero-segment zombies.
- Only **build-fresh via `BuildBelt`** is safe: `SpawnActor`'d belts don't auto-register until the
  explicit `AddConveyor`, so they're never registered-while-unconnected and never disturb a live
  chain. This is the architectural reason stacked must move to preview + build-on-confirm.

## 10. Summary

The current design fights vanilla's chaining by repairing it after the fact. The clean design
**never creates the wrong state**: build supports, then build belts in run order, connecting
each to a real neighbor before registering it. That is precisely how manual player belts and
Smart's own working flat auto-connect already behave — the stacked case just needs the same
discipline applied across the whole structure in one owning build call.
