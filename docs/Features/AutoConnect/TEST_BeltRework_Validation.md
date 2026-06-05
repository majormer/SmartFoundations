# Belt Auto-Connect Rework — Validation Test Plan

Tracks the tests that validate the "belts done properly everywhere" rework
(see `DESIGN_StackablePole_FromScratch.md`). Each fix step lists the tests that must pass
**in-game** before it's considered done. Status: ⬜ not run · 🔄 in progress · ✅ pass · ❌ fail.

Validation tooling: `STACK-PROBE` / `STACK-PREPROBE` style `Display` logs in the new code, the
in-game **Triage → Detect** scan (expect 0 `SPLIT_CHAIN`, 0 `NO_SEGMENTS`), and **save+reload**
(the real test — the original bug was a broken state captured on save).

General pass criteria for ANY belt build: every belt ends with a **non-null chain actor**,
items **flow end-to-end**, Detect is **clean**, and the state **survives save/reload**.

---

## Step 1 — Shared `BuildBelt` helper (extract from `BuildBeltFromPreview`)
Refactor only; must be behavior-preserving for the distributor path.

| # | Test | Expectation | Status |
|---|---|---|---|
| 1.1 | Build compiles (PackagePlugin Shipping) | `BUILD SUCCESSFUL`, fresh DLL | ✅ 2026-06-05 (DLL 02:17) |
| 1.2 | **Distributor → factory auto-connect** (single belt) | belt builds, items flow, chain non-null — **unchanged from before** | ⬜ |
| 1.3 | Distributor manifold (splitter/merger row) | belts build, no `SPLIT_CHAIN` | ⬜ |
| 1.4 | Belt **spline shape** options (Default / Curve / Straight) | each routing mode still shapes the belt as before | ⬜ |
| 1.5 | Save + reload after 1.2–1.4 | no corruption, chains intact | ⬜ |

## Step 2 — `BuildStack` for stacked poles (connect-then-register, shape-aware)
The actual fix for the reported bug.

| # | Test | Expectation | Status |
|---|---|---|---|
| 2.1 | Build compiles | `BUILD SUCCESSFUL` | ✅ 2026-06-05 |
| 2.2 | Stacked pole, 2 tall, 3 stacks (the repro) | items flow whole stack; no null-chain belt | ✅ 2026-06-05 (items flowed end-to-end through the segment break) |
| 2.3 | Tall stack (5+ levels), long run | one chain per run; no zombies | ⬜ not yet run |
| 2.4 | Stacked belts with **Curve** routing mode | belts curved AND correctly chained (shape + chain both correct) | ⬜ not yet run |
| 2.5 | Triage → Detect after 2.2–2.4 (SmartMCP `/api/conveyor-chains`) | 0 zombie chains, all `hasValidLUT` | ✅ 2026-06-05 (`zombieCount:0`, 18 chains all valid) |
| 2.6 | **Save + reload** after building stacks | chains valid on reload (the original-bug gate) | ✅ 2026-06-05 (PASS — see resolution below) |
| 2.7 | Dismantle a stacked run | clean teardown, no orphan chains | ⬜ not yet run |

## Step 3 — Converge other features onto the shared builder
| # | Test | Expectation | Status |
|---|---|---|---|
| 3.1 | Extend a belt run + a manifold | chains contiguous, items flow (no regression) | ⬜ |
| 3.2 | Scaled Extend, dense (200+ belts) | no `SPLIT_CHAIN` / zombies, in-game + reload | ⬜ |
| 3.3 | Conveyor lift auto-connect | lift chains correctly | ⬜ |
| 3.4 | Non-Extend belt-site audit (connect-then-register invariant) | every live site compliant or justified | ✅ 2026-06-05 (see "Non-Extend belt-site audit" below) |

## Do-not-regress (run opportunistically)
- Belt tier respects runtime settings.
- Grid scaling child holograms still update in place.
- No crash in the parallel factory tick during/after any build.
- Multiplayer: build on authority replicates correctly (when MP work begins).

---

### Run log
- 2026-06-05, SF DLL 02:17 — **1.1 ✅** shared `BuildBelt` refactor compiles + deploys.
- 2026-06-05, SF DLL 02:44 — **Step 2 attempt A (post-construct re-register: `RemoveConveyor`→`AddConveyor` in the timer) → ❌ CRASH.** `EXCEPTION_ACCESS_VIOLATION` in `AFGConveyorChainActor::Factory_Tick` on a ParallelFor worker next tick — the documented bucket-mutation race (matches ChainActorMigrationPlan P0). `RemoveConveyor` on a **live registered** belt drops it from its bucket but leaves the now-empty solo chain actor in the tick path → next parallel tick dereferences it. **Re-register-on-live-belts is DISPROVEN.** Reverted to safe baseline + redeployed.

### Conclusion after attempt A — the only safe path is build-fresh
Every approach that touches **already-registered, live** stacked belts has now failed:
- merge (`InvalidateAndRebuildForBelts`) → zombie chains,
- re-register (`RemoveConveyor`/`AddConveyor`) → ParallelFor crash,
- baseline (`RemoveConveyorChainActor` + hope) → safe but split/no-merge (original bug).

The ONLY proven-safe + correct belt creation is **build fresh via `BuildBelt`** (`SpawnActor`→`Respline`→`SetConnection`→`AddConveyor`), exactly as the distributor/Extend paths do — because SpawnActor'd belts do **not** auto-register until the explicit `AddConveyor`, so they are never registered-while-unconnected and never tear down a live chain. This forces the **preview-only / build-on-confirm** architecture for stacked belts, which in turn requires **manual material cost handling** (SpawnActor'd belts aren't charged by vanilla). Cost reimplementation is therefore unavoidable for a correct fix.

> ⚠️ **Superseded** — the prediction above (preview-only + manual cost reimplementation) turned out to be **unnecessary**. The connect-then-register invariant can be satisfied *inside the real hologram build* by connecting **by reference** at Construct (the way Extend already does). See resolution.

---

## ✅ RESOLUTION (final, validated 2026-06-05)

**What actually worked — `STACK-CHAIN` handler in `ASFConveyorBeltHologram::Construct`:**
1. Each stacked run is tagged at placement with a `StackChainId` + per-belt `StackChainIndex`
   (`SFAutoConnectService_Stackable.cpp` → `SFHologramData`).
2. For a `bIsStackableBelt` child, `Construct` calls **`Super::Construct`** — so the belt is a
   **real buildable** (cost aggregates through the build gun, no null-return crash), then:
3. Resolves the already-built run neighbour(s) **by reference** via the conveyor registry
   (`GetBuiltConveyor(ChainId, Index±1)`), calls **`SetConnection`** on the built connectors, and
   **`RegisterBuiltConveyor`**. Order-agnostic (predecessor and/or successor — whichever is built).
4. SpawnActor'd belts never touch a live chain; vanilla re-unifies connected runs into proper
   multi-segment chains on load.

This is connect-then-register **without** preview-only or manual cost — because the connection is
made by *reference* at construct, not by geometry post-hoc. The earlier "cost reimplementation is
unavoidable" conclusion was the one wrong turn; Extend's two-phase model showed the way out.

**Validation (live game, via SmartMCP — replaces the planned `STACK-PROBE`/Detect tooling):**
- `/api/connections` — stacked belts wired to the correct run-neighbour peers on **both** ends.
- `/api/conveyor-chains` — `zombieCount: 0`, all 18 chains `hasValidLUT: true`, runs carry items.
- **Items flow end-to-end** through the former segment break (user-confirmed in-game).
- **Save + reload (the original-bug gate): PASS.** Post-reload `zombieCount: 0`, all LUTs valid,
  connections survived to correct peers, and vanilla had re-unified connected runs into healthy
  4-segment chains. The broken-state-captured-on-save signature does **not** occur.

---

## Non-Extend belt-site audit (Step 3.4)

Surveyed every site that creates or wires conveyor belts/chains outside the Extend feature,
against the invariant (set connections **before** the belt registers into the conveyor system;
never `AddConveyor`/`RemoveConveyor` on a **live** belt off a timer). Result: **no live non-Extend
path needs new treatment** — they are already compliant, now-fixed, or dead.

| Site | Creates/wires belts? | Invariant status | Notes |
|---|---|---|---|
| `ASFConveyorBeltHologram::Construct` — STACK-CHAIN (stacked poles) | yes (real hologram) | ✅ fixed this sprint | connect-by-reference at Construct, then register |
| `SFConveyorLiftHologram::ConfigureComponents` | yes (lifts) | ✅ compliant | `SetConnection` then conditional `AddConveyor`; Extend lifts skip `AddConveyor` (chain rebuild registers). Explicit "no double-add" guard. |
| Distributor **child holograms** (live auto-connect path) | yes (real child holograms) | ✅ compliant | belts build as build-gun children → connect at vanilla Construct; the `OnActorSpawned` distributor-child **manifold** timer only proximity-wires cross-link belts built *before* their target distributor (narrow scope, empirically zombie-free on reload) |
| `SFChainActorService` (mass-upgrade migration + connection repair) | re-registers existing belts | ✅ compliant (gated) | `RemoveConveyor`/`AddConveyor` only on player-initiated Repair in a stable world; union-find merge avoids per-belt solo `FConveyorTickGroup`s. Extensive crash-mode documentation in-file. |
| Extend family (`SFExtend*`, `SFWiringManifest`) | yes | ✅ compliant | two-phase chain model, connect-by-reference (`GetBuiltConveyor`/`RegisterBuiltConveyor`) |

### Dead code surfaced by the audit (remove in a build-verified follow-up)
None of these are reachable; removal is cleanup, not a fix. They span multiple files, so they were
**not** cut blind from the live-game session:
- `USFSubsystem::QueueChainRebuild` (+ its rebuild timer) — **crash-class** (`RemoveConveyor`/`AddConveyor` on live belts); **never called**. Highest-priority removal so nobody resurrects it.
- `USFAutoConnectService::BuildBeltFromPreview` — the textbook connect-then-register reference impl, but **never called**.
- `USFAutoConnectService::BuildBeltsForDistributor` — superseded by the child-hologram refactor (`SFConveyorAttachmentHologram.cpp:18`); **never called**.
- `USFSubsystem::CacheStackableBeltPreviewsForBuild` + `SFSubsystemStackableCache.h` globals + the call at `SFAutoConnectOrchestrator.cpp:531` — now a **write-only** producer (its only consumer was the deleted `OnActorSpawned` block).

### Run log
- 2026-06-05, SF DLL (stacked fix) — **2.1 ✅ / 2.2 ✅ / 2.5 ✅ / 2.6 ✅.** STACK-CHAIN handler:
  built real stacked-pole belts, items flowed end-to-end through the segment break, SmartMCP showed
  `zombieCount:0` + all LUTs valid, and the state **survived save/reload**. Fix committed `cd659cc`;
  diagnostics tidied + dead timer paths removed `20b39fd`.
- **3.4 ✅** non-Extend belt-site audit complete (table above): no live path needs new treatment;
  4 dead-code clusters flagged for a build-verified follow-up.
