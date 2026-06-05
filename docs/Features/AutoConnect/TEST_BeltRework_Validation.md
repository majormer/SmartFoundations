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
| 2.1 | Build compiles | `BUILD SUCCESSFUL` | ⬜ |
| 2.2 | Stacked pole, 2 tall, 3 stacks (the repro) | `STACK-PROBE … 0 null-chain belt(s) (OK)`; items flow whole stack | ⬜ |
| 2.3 | Tall stack (5+ levels), long run | one chain per run; no zombies | ⬜ |
| 2.4 | Stacked belts with **Curve** routing mode | belts curved AND correctly chained (shape + chain both correct) | ⬜ |
| 2.5 | Triage → Detect after 2.2–2.4 | 0 `SPLIT_CHAIN`, 0 `NO_SEGMENTS` | ⬜ |
| 2.6 | **Save + reload** after building stacks | chains valid on reload (the original-bug gate) | ⬜ |
| 2.7 | Dismantle a stacked run | clean teardown, no orphan chains | ⬜ |

## Step 3 — Converge other features onto the shared builder
| # | Test | Expectation | Status |
|---|---|---|---|
| 3.1 | Extend a belt run + a manifold | chains contiguous, items flow (no regression) | ⬜ |
| 3.2 | Scaled Extend, dense (200+ belts) | no `SPLIT_CHAIN` / zombies, in-game + reload | ⬜ |
| 3.3 | Conveyor lift auto-connect | lift chains correctly | ⬜ |
| 3.4 | AutoConnect bounce site review (`SFSubsystem_AutoConnect.cpp:793`) | converted or justified; no regression | ⬜ |

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
