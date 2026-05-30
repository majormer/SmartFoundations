# Refactor Completion Plan — surprise-proof decomposition map

**Purpose.** A complete, audited plan for finishing the SmartFoundations code-simplicity
refactor (epics T1/T5/T6/T8 + T4/T7 tails) so that future code-change sessions hit **no
unaudited coupling**. This is the "measure twice" doc: every slice below is pre-audited for
call sites and cross-unit shared state *before* any code moves, because the costly surprises
so far (e.g. Slice B's restored-replay path writing `StoredCloneTopology` and reading the
`Json*` maps owned by other units) came from coupling that a surface-level read missed.

Charter: [`Simplification-GOAL.md`](Simplification-GOAL.md) · Tracker:
[`SimplificationAudit.md`](SimplificationAudit.md) · Status:
[`Simplification-RemainingWork.md`](Simplification-RemainingWork.md).

> Status: **IN PROGRESS (planning effort)**. Sections marked `[AUDIT PENDING]` are filled by
> the planning pass. Every count/claim must be backed by live `wc -l` / Grep / Read output
> captured during this effort — when a prior doc disagrees with live `wc -l`, the live count
> wins and the discrepancy is noted here.

## How to use this doc

Each target file gets a **Unit Map** (cohesive units with line ranges) and one **Slice
Card** per planned PR-sized extraction. A slice is only "plan-complete" when its card has all
fields filled with grounded evidence. The [Entanglement Ledger](#entanglement-ledger) and
[Global Sequencing](#global-sequencing) sections tie it together; the
[Self-Review](#self-review--coverage-proof) section proves no region is orphaned.

## Charter anchor — what "done" means

From [`Simplification-GOAL.md`](Simplification-GOAL.md). The refactor's 6 success criteria and
their live status (per [`Simplification-RemainingWork.md`](Simplification-RemainingWork.md)
scorecard, this session):

| # | Criterion | Status |
|---|-----------|--------|
| 1 | 10-minute architecture map | DONE (`docs/ARCHITECTURE.md`) |
| 2 | basic smoke-test safety net | DONE (`scripts/smoke_test.py`) |
| 3 | one edit point for asset paths | DONE (`SFAssetPaths.h`) |
| 3b | one edit point for building sizes | DONE (`Content/Data/BuildableSizes.csv`) |
| 4 | per-feature log categories live | DONE |
| **5** | **no file >~2k lines** | **OPEN — needs T1/T5/T8** |
| **6** | **no god-object >3k lines** | **OPEN — needs T1** |

**Only #5 and #6 remain, and both are pure decomposition.** That sharply scopes this plan:

- **MUST-DO (closes the criteria):** get every `.cpp` currently >2k under 2k, and both
  god-objects under 3k. That is exactly the 9 files >2k in the inventory below. This is the
  bar; everything else is optional.
- **SHOULD-DO (charter themes, not gating):** T6 service-context DI, the hologram-adapter
  registry, consolidating power state — improve coupling but are NOT required for #5/#6.
  Plan them, but sequence them after the criteria are met and flag them as design changes
  (need an ADR / maintainer intent), not mechanical moves.

Every Slice Card must state **which criterion it advances** (e.g. "drops SFSubsystem.cpp
9,227→<3k: criterion #6") so effort stays tied to finishing, not gold-plating. Guardrails
(from the charter) bind every slice: pure-move-first, audit-before-edit, build-validate +
NEEDS-CARE in-game smoke, no behavior change without a CHANGELOG note.

## Slice Card template (every slice must fill all fields)

```
### <Target file> — Slice <N>: <name>   [lane: SAFE-NOW | NEEDS-CARE]
- Moves: <functions + line ranges>  (verbatim/pure-move? or careful-move?)
- New unit: <new file(s) / destination>
- Call-site audit: <every external caller, file:line — from Grep>
- Shared-state / cross-unit coupling: <every member/helper this unit reads or WRITES that
  another unit also touches; name the other unit; file:line evidence>. "NONE — exclusive" only
  if proven by grep.
- Extraction approach: <pure move | friend back-ref | accessors | state migration>; what
  forwarders stay on the parent; what `this`→owner rewrites are needed.
- Hidden helpers: <anon-namespace / file-local statics this unit uses, and whether they are
  shared with other units (→ promote) or exclusive (→ move)>.
- Runtime coupling (SMOKE-CRITICAL): <init-order / lazy-init / frame-order / tick-order
  dependencies this unit touches that static grep CANNOT prove safe — e.g. a member read
  before its owner's Initialize(), placement material-state set across a frame boundary,
  order of GetXService() lazy construction. Name each, or "none identified — still
  smoke-verify". This is the class grep misses, so it is where Smart's real regressions live.>
- Risk + smoke: <in-game checks this slice needs; derive directly from the runtime-coupling
  field above>.
- Size delta: <expected -N lines; parent file resulting size>.
- Depends on: <other slices that must land first>.
```

---

## Live file inventory (verified `wc -l`, captured 2026-05-30, this effort)

These are the decomposition targets. Counts from `find Source -name '*.cpp' | xargs wc -l`,
run this session (authoritative; supersedes any figure in older docs).

| Lines | File | Epic | Status |
|------:|------|------|--------|
| 9227 | `Subsystem/SFSubsystem.cpp` | T1 god-object | [AUDIT PENDING] |
| 7718 | `Features/Extend/SFExtendService.cpp` | T1 god-object | round 1 done (H+B); units F/G/I/J + tails remain |
| 4771 | `Features/AutoConnect/SFAutoConnectService.cpp` | T1 scope-add | [AUDIT PENDING] |
| 3746 | `UI/SmartSettingsFormWidget.cpp` | T5 | [AUDIT PENDING] |
| 2789 | `Features/PipeAutoConnect/SFPipeAutoConnectManager.cpp` | T1 scope-add | [AUDIT PENDING] |
| 2537 | `Features/Upgrade/SFUpgradeExecutionService.cpp` | T1/review | [AUDIT PENDING] |
| 2220 | `Holograms/Logistics/SFConveyorBeltHologram.cpp` | T8 | [AUDIT PENDING] |
| 2144 | `Subsystem/SFHologramHelperService.cpp` | T1 scope-add | [AUDIT PENDING] |
| 2138 | `UI/SmartUpgradePanel.cpp` | T5 | [AUDIT PENDING] |
| 1949 | `Services/SFChainActorService.cpp` | review (borderline) | [AUDIT PENDING] |
| 1852 | `Features/PowerAutoConnect/SFPowerAutoConnectManager.cpp` | T1 scope-add (borderline) | [AUDIT PENDING] |
| 1805 | `Services/RadarPulse/SFRadarPulseService.cpp` | review (borderline) | [AUDIT PENDING] |
| 1793 | `Features/Restore/SFRestoreService.cpp` | T6-adjacent (borderline) | [AUDIT PENDING] |
| 1781 | `Features/AutoConnect/SFAutoConnectOrchestrator.cpp` | T1 (borderline) | [AUDIT PENDING] |
| 1481 | `Holograms/Logistics/SFPipelineHologram.cpp` | T8 | [AUDIT PENDING] |

(Files 1481–1949 are under the 2k criterion bar today but are in-scope for T6/T8 or as
risk-reduction; the plan states for each whether it needs a slice or is left as-is.)

---

## T1a — `SFExtendService.cpp` (live 7,718) — advances criterion #5 (get <2k)

Round 1 (`fd27261`) extracted H (Diagnostics) + B (Restore-replay). **Live unit map** (line
ranges from `grep -nE '^.*USFExtendService::'`, this effort):

| Unit | Lines | ~Size | Disposition |
|------|------:|------:|-------------|
| orchestration/lifecycle (Init/Clear/Shutdown) | 82–217 | 136 | STAYS (orchestrator) |
| A Direction cycling | 220–413 | 194 | STAYS or fold |
| Topology-walk + restore forwarders | 416–527 | 112 | STAYS (thin delegators) |
| IsValidExtendTarget | 528–570 | 43 | STAYS |
| C Extension execution (TryExtend/Refresh/Cleanup) | 573–1341 | 769 | STAYS (core preview path) |
| D Build-gun hologram swap | 1344–1529 | 186 | STAYS or fold |
| E Belt previews + per-chain wiring | 1532–2305 | 774 | SPLIT: previews stay, chain-wiring → Wiring |
| **F** Phase 3.8 `WireBuiltChildConnections` + Register*/Copy | 2308–3879 | **1,572** | → Wiring |
| **G** Manifold connections | 3882–4653 | **772** | → Wiring |
| Diagnostics forwarders | 4656–4676 | 21 | STAYS (thin) |
| **I** Phase 5/6 `GenerateAndExecuteWiring` + JSON registry | 4679–6383 | **1,705** | → Wiring |
| **J** Scaled Extend | 6386–7718 | **1,333** | → new ScaledService |

### Cross-unit shared-state matrix (field c — the entanglement, from live grep)

The post-build wiring registries are shared across **E/F/G**; the clone-topology + JSON +
scaled state across **F/I/J**. Counts = refs per unit (this effort):

```
BuiltConveyorsByChain        F=10 E=1            BuiltDistributorsByChain   F=10 G=3 E=1
BuiltJunctionsByChain        F=4 G=3 E=1         BuiltPipesByChain          F=9 E=1
BuiltChainIsInputMap         F=6 E=1 G=1         BuiltPipeChainIsInputMap   F=4 E=1 G=1
SourceDistributorsByChain    G=2 E=1 F=1         SourceJunctionsByChain     G=2 E=1 F=1
DistributorConnectorNameByChain F=2 E=1          (+ E-only: SourceToHologramMap, Pipe/Belt/Lift
                                                  ChainHologramMap, UnifiedConveyorChainMap,
                                                  BeltChainDistributorMap, ManifoldBeltHolograms,
                                                  ChainIsInputMap, BuiltChainElements)
StoredCloneTopology  F=18 I=18 J=18 E=5 Topo=4   (THE spine — 63 refs; also read by RestoreReplay)
JsonBuiltActors      I=25 F=19 J=1               JsonSpawnedHolograms  I=5 E=3 F=3 J=2 C=1
ScaledExtendClones   J=20 I=7 C=1 F=1            RestoredScaledFactoryPreviewLocations I=3 F=1
PowerPoleWiringData  I=4 E=3 F=1                 HologramService  E=15 J=8 orch=6 C=5 D=5
```

**Conclusion:** E/F/G/I cannot become *separate* services — they co-own ~19 registry maps +
`StoredCloneTopology`. But ~4,600 lines of wiring also cannot be one <2k file.

### Architecture decision — wiring cluster → ONE class, impl split across `.cpp` files

The existing **`USFExtendWiringService`** stub was built for exactly this — its header says
*"Actual wiring logic remains in SFExtendService due to tight coupling… Future Migration: Move
tracking maps to this service, move wiring implementation here."* It already holds
`TWeakObjectPtr<USFSubsystem> Subsystem` + `USFExtendService* ExtendService`. Plan:

- Migrate the **~19 registry maps** (Built*/Source*/chain-hologram/DistributorConnectorName)
  into `USFExtendWiringService` as members (state migration — they become *local*, so NO
  friend/Owner-> needed for them; this is cleaner than slice B because the maps are exclusive
  to the wiring cluster once E's chain-wiring moves with them).
- Move **F, G, I, and E's chain-wiring helpers** (`WirePipeChainConnections`,
  `WireBeltChainConnections`, `FindPipe/FactoryConnectionByIndex`, `ClearConnectionWiringMaps`,
  `ConnectAllChainElements`) into the class. To keep every `.cpp` <2k, **split the class
  implementation across multiple translation units** (UE supports one class, many `.cpp`):
  `SFExtendWiringService.cpp` (registry + chain wiring, ~1.0k), `SFExtendWiringService_BuiltChild.cpp`
  (F, ~1.6k), `SFExtendWiringService_Json.cpp` (I, ~1.7k), `SFExtendWiringService_Manifold.cpp`
  (G, ~0.8k). One header declares all; maps are members of the one class.
- **`StoredCloneTopology`** stays the canonical member on `USFExtendService` (read by Wiring +
  Scaled + RestoreReplay) — accessed via a `friend`/accessor, same as round 1's `LastCloneTopology`.
- **J Scaled Extend** → new `USFExtendScaledService` (owns `ScaledExtendClones`); I's 7
  `ScaledExtendClones` refs become an accessor on the scaled service (audit each before moving).

### Slice Cards

#### SFExtendService — Slice J: Scaled Extend → `USFExtendScaledService`   [lane: NEEDS-CARE]
- Moves: `GetExtendCloneCount`/`GetExtendRowCount`/`IsScaledExtendActive`/`OnScaledExtendStateChanged`
  (6386–6725), `CalculateScaledExtendPositions` (6726–6896), `SpawnScaledExtendPreviews`
  (6897–7477), `ClearScaledExtendClones` (7478–7565), `ValidateScaledExtendConstraints`
  (7566–7663), `ValidatePowerCapacity` (7664–7718). ~1,333 lines.
- Call-site audit: `OnScaledExtendStateChanged` ×5 external (Subsystem, HudService,
  SmartSettingsFormWidget — from grep); `GetExtendCloneCount`/`GetExtendRowCount`/
  `IsScaledExtendActive` called by UI/subsystem. → forwarders on orchestrator.
- Shared state: owns `ScaledExtendClones` (J=20) — but **I reads it 7×** and C 1×, F 1× →
  expose `GetScaledExtendClones()`/count accessor on the new service, audit those 9 sites.
  Reads `StoredCloneTopology` (J=18) → accessor on orchestrator. `HologramService` (J=8) →
  accessor. `Subsystem->GetCounterState()` for grid math.
- Extraction approach: careful move; friend `USFExtendScaledService` of `USFExtendService` (for
  StoredCloneTopology + HologramService + counter state); `this`→`Owner` where passed to
  `SpawnChildHologram*`. Forwarders for the ~5 public methods.
- Hidden helpers: `[VERIFY]` grep anon-namespace in SpawnScaledExtendPreviews region for
  file-local statics (round-1 anon ns was all restore-replay; confirm none here are shared).
- Runtime coupling (SMOKE-CRITICAL): `OnScaledExtendStateChanged` fires from
  `UpdateCounterState` mid-input; preview spawn happens during hologram tick. Order vs
  RefreshExtension matters (see round-1 ClearExtendState comment about counter restore
  triggering OnScaledExtendStateChanged). Smoke: scale X then Y, change spacing/rotation while
  previewing, look away + back, verify clone count + positions + power-capacity red state.
- Size delta: −1,333; SFExtendService.cpp → ~6,385.
- Depends on: none (most separable; **do first** of the remaining Extend units).

#### SFExtendService — Slices F+G+I+Echain: wiring cluster → `USFExtendWiringService`   [lane: NEEDS-CARE]
- Moves (one class, 4 `.cpp`): F `WireBuiltChildConnections`+Register*/getters+`CopyDistributorConfigurations`
  (2308–3879); G Manifold (3882–4653); I `GenerateAndExecuteWiring`+JSON registry (4679–6383);
  E chain-wiring (`WirePipeChainConnections` 1723–1899, `WireBeltChainConnections` 1947–2133,
  `Find*ConnectionByIndex`, `ClearConnectionWiringMaps`, `ConnectAllChainElements` 2157–2305).
- Call-site audit (external, from grep): `RegisterJsonBuiltActor` ×12, `GetBuiltActorByCloneId`
  ×9, `GenerateAndExecuteWiring` ×9, `WireBuiltChildConnections` ×7, `RegisterBuiltConveyor` ×5,
  `ConnectAllChainElements` ×3, `RegisterBuilt{Distributor,Junction,Pipe}` ×2 each,
  `WireManifold{Belt,Pipe}` ×2 each — callers are ~13 hologram `Construct()` files +
  `SFExtendChainHelper` + `SFSubsystem` + `SFHudService`. ALL preserved via orchestrator forwarders.
- Shared state: the ~19 registry maps migrate INTO the service (become local). Cross-refs that
  stay external: `StoredCloneTopology` (F=18,I=18 → Owner accessor), `HologramService`
  (→ Owner accessor), `ScaledExtendClones` (I=7 → ScaledService accessor — **sequence J first**),
  `PowerPoleWiringData` (I=4,E=3,F=1 → migrate with cluster), `Subsystem` (→ keep back-ref).
- Extraction approach: careful move + STATE MIGRATION of the maps; friend not needed for maps
  (now members) but needed for StoredCloneTopology/HologramService access; split impl across 4
  `.cpp` so each <2k. `this`→`Owner` where the wiring passes the service ptr to topology spawn.
- Hidden helpers: `[VERIFY]` grep anon-namespace + file-local statics in 1532–6383 for helpers
  shared with C/J before moving (the round-1 lesson — e.g. any belt/pipe routing helper).
- Runtime coupling (SMOKE-CRITICAL): the Register* methods run from hologram `Construct()` across
  frames; `GenerateAndExecuteWiring`/`WireBuiltChildConnections` run in a DEFERRED tick after
  build (post-build timer). Init-order: WiringService must exist before any child Construct().
  Frame-order: registries populated over multiple Construct() calls, consumed in the deferred
  wiring pass. This is the single highest-risk zone in Extend. Smoke: full Extend build with
  belts+lifts+pipes+power+distributors+junctions+manifold, scaled Extend, restore-from-preset.
- Size delta: −~4,600 from SFExtendService.cpp; resulting orchestrator ~1,786 (UNDER 2k ✓);
  WiringService split into 4 files each <2k.
- Depends on: **Slice J first** (so ScaledExtendClones has its accessor home before I's refs move).
- Open question `[MAINTAINER]`: confirm whether to grow the existing `USFExtendWiringService`
  stub vs a fresh class — recommend grow (purpose-built, already wired into Initialize).

After J + the wiring cluster, residual `SFExtendService.cpp` ≈ orchestration + A/C/D + belt-preview
shell + thin delegators ≈ **~1,786 lines (<2k, criterion #5 met for this file)**. A/C/D need no
further split. `[VERIFY in self-review: function-by-function coverage sums to 7,718.]`

## T1b — `SFSubsystem.cpp` (9,227) — the second god-object

`[AUDIT PENDING]` — Unit Map + Slice Cards. Known candidate units from the charter: input
binding, grid/scaling state, recipe state, child-hologram lifecycle, the 10-way
`CreateHologramAdapter()` switch (→ registry), power-connection state (→
`SFPowerAutoConnectManager`). Must audit the `GetExtendServiceSafe()` / sibling reach-back
pattern and the init-order coupling (feeds T6).

## T1c — AutoConnect family

`[AUDIT PENDING]` — `SFAutoConnectService.cpp` (4,771), `SFPipeAutoConnectManager.cpp`
(2,789), `SFAutoConnectOrchestrator.cpp` (1,781), `SFPowerAutoConnectManager.cpp` (1,852).

## T1d — `SFHologramHelperService.cpp` (2,144) + `SFUpgradeExecutionService.cpp` (2,537)

`[AUDIT PENDING]`

## T5 — UI widgets

`[AUDIT PENDING]` — `SmartSettingsFormWidget.cpp` (3,746) + `SmartUpgradePanel.cpp` (2,138)
→ model / presenter / event-binder.

## T6 — Service-context DI

`[AUDIT PENDING]` — replace `USFSubsystem`→sibling reach-back with `FFeatureServices` +
explicit CONSTRUCT/INITIALIZE/LAZY phases. Needs an ADR. Audit every `GetXService()`
reach-back and init-order dependency surfaced in T1b.

## T8 — Hologram split

`[AUDIT PENDING]` — `SFConveyorBeltHologram.cpp` (2,220) + `SFPipelineHologram.cpp` (1,481)
→ grid adapters / junction spawners / shared `FSFHologramCostCalculator`.

## Tails

`[AUDIT PENDING]` — T4 PC-helper (`SFPlayerHelpers::GetFGPlayerController`, ~28 sites);
T7 remaining (`SFFactoryHologram` `SF_HOLOGRAM_LOG`, `SFRecipeManagementService`,
`SFRadarPulseService`).

---

## Entanglement Ledger

Every cross-unit shared-state coupling and non-obvious/hidden helper found during the audit,
with `file:line` evidence and how the plan handles it. (The thing that surprises us lives
here — keep it exhaustive.)

- **[Extend] `StoredCloneTopology` + `LastCloneTopology`** — written by the normal-extend /
  JSON-wiring path AND the Restore-replay path; read by `GetLastCloneTopology`. Round-1
  handling: kept on `USFExtendService`, restore-replay sub-service accesses via `friend
  Owner->`. (Confirmed `SFExtendService.cpp` round-1 audit.)
- **[Extend] `CalculateRestoredScaledClonePlacement`** — anon-namespace helper shared between
  restore-replay and `GenerateAndExecuteWiring` (unit I). Round-1 handling: promoted to a free
  function in `SFExtendRestoreReplayService.h`. Lesson: **grep every anon-namespace/file-local
  helper across the whole file before assuming a unit owns it.**
- `[AUDIT PENDING]` — populate for every remaining target.

## Global Sequencing

`[AUDIT PENDING]` — recommended slice order across all epics; inter-slice dependencies;
which slices are solo-compile-validate vs need maintainer smoke; how slices batch into
build+smoke cycles.

## Self-Review — coverage proof

`[AUDIT PENDING]` — for each god-object, re-derive that the union of planned slices accounts
for ~100% of the file's current functions (no orphaned region), via a function-by-function
checklist mapped to slices. List every remaining open question / assumption needing maintainer
input.
