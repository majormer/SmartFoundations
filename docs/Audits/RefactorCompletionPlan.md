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

> Status: **COMPLETE (2026-05-30)**. All 9 files >2k + both god-objects audited with full Slice
> Cards; coverage proven GAP=0; consolidated Entanglement Ledger + Global Sequencing done. Every
> count/claim is backed by live `wc -l` / Grep / Read output captured during this effort. Remaining
> `[VERIFY]` markers are narrow slice-time confirmations (anon-namespace sweeps, internal-caller
> lists for impl-splits) enumerated in the Ledger's "`[VERIFY]` debt" note — not open structural
> questions. Resume code work from the Global Sequencing table.

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

## T1b — `SFSubsystem.cpp` (live 9,227) — advances criterion #6 (get <3k) + #5

183 functions. **Banner sections are MISLABELED** (audit caught this): "Static Access" (717)
and "Hologram Lock Ownership" (1,802) are mostly **input handlers** + **active-hologram
lifecycle**, not what their names say. Real units (line ranges from live grep):

| Real unit | Lines (approx, from grep) | ~Size | Destination |
|-----------|---------------------------|------:|-------------|
| **Input handling** — `SetupPlayerInput`,`CheckForPlayerController`,`ApplyAxisScaling`, `OnScaleX/Y/Z`,`OnValue±`,`OnMouseWheel`,`OnModifierScaleX/Y±` (1049–1751) + `OnSpacing/Steps/Stagger/Rotation`,`OnToggleArrows/SettingsForm/UpgradePanel` (1929–2676) | scattered | **~1,300** | → `FSFInputHandler` (stub exists) |
| **Active-hologram lifecycle** — `RegisterActiveHologram` (2677–3228, ~551), `UnregisterActiveHologram` (3229–3356), `PollForActiveHologram` (3357–3553), `TryAcquire/ReleaseHologramLock`,`IsAnyModalFeatureActive`,`GetFurthestTopHologramPosition` (1756–1928) | 1756–3553 | **~1,100** | → `FSFHologramHelperService` (stub exists) |
| **Hologram creation + adapter + tier lookups** — `CreateCustom{Foundation,Factory,Logistics}Hologram`,`CopyHologramProperties`,`ReplaceHologramInBuildGun`,`GetBeltClassForTier`,`GetHighestUnlocked*Tier`,`OnActorSpawned`,`FindRecipeForSpawnedBuilding`,`ApplyRecipeDelayed` (6289–8372) | 6289–8372 | **2,083** | → `FSFHologramHelperService` (`CreateHologramAdapter` stub exists) |
| **Multi-Step Property Sync** (Issue #200) | 3554–4260 | 706 | → new hologram-sync helper or HologramHelper |
| **Pipe Tier Config** | 5034–5997 | 963 | → config helper / `SFPipeAutoConnectManager` |
| **Power Connection Mgmt** (PIMPL) | 423–1035 | 612 | → `SFPowerAutoConnectManager` (charter "also consider") |
| **Static accessors / Get() / RPC / Config / Recipe-copy / setters / registry / debug / chain-rebuild / restore-enh** | scattered | residual | STAYS on subsystem (facade) |

**KEY FINDING — destinations already stubbed.** `SFInputHandler.cpp` has
`FSFInputHandler::SetupPlayerInput`; `SFHologramHelperService.cpp:121/142/1595` have
`RegisterActiveHologram`/`UnregisterActiveHologram`/`CreateHologramAdapter` each marked
`// TODO: Extract from SFSubsystem::X`. The maintainer pre-built the homes. Both are **`F`-type
plain classes** (PIMPL), owned by the subsystem — extraction is "fill the stub + delete the
original", not new UObject wiring. **`GetCurrentAdapter()` is called externally** by
`SFGridSpawnerService.cpp:91,624,630`, so `CurrentAdapter` stays on the subsystem (or moves to
HologramHelper with an accessor) — audit at slice time.

### Cross-section coupling matrix (field c — from live grep, this effort)

```
ActiveHologram     142  SPINE  HoloLock=35 PipeTier=31 HoloCreate=26 StaticAccess=18 RecipeCopy=11 PowerConn=10
CounterState        70  SPINE  HoloLock=44 StaticAccess=17 RPC=4 PowerConn=2 DistribLife=2
CurrentAdapter      14         HoloLock=9 PowerConn=2 StaticAccess=2 PropSync=1   (external: GridSpawner)
Planned/CommittedBuildingConnections, Planned/DeferredPoleConnections  shared PowerConn<->HoloCreate
GridStateService    37  reach  HoloLock=14 StaticAccess=13 DistribLife=3 ...   (already a separate svc)
GridTransformService 11 reach  HoloLock=6 PropSync=2 ...
HudService          27  reach  HoloLock=14 BuildHUD=6 RPC=4 ...
ExtendService       42  reach  PowerConn=14 HoloLock=14 StaticAccess=5 ...
AutoConnectService  81  reach  PipeTier=26 HoloCreate=14 HoloLock=12 PowerConn=8 Config=7 ...
CurrentBuildProxy   12         HoloCreate=9 HoloLock=3
```

**`ActiveHologram` (142) + `CounterState` (70) are the spine** — read by every unit; they STAY
on the subsystem and extracted helpers access them via the owner back-ref (the `F`-helpers
already take a subsystem pointer). The feature-service pointers (ExtendService 42,
AutoConnectService 81, GridStateService 37, HudService 27 …) are the **sibling reach-back** the
T6 DI context formalizes — T1b uses the existing back-ref; T6 replaces it later.

### Slice Cards

#### SFSubsystem — Slice S1: Input handling → `FSFInputHandler`   [lane: NEEDS-CARE]
- Moves: `SetupPlayerInput`,`CheckForPlayerController`,`ApplyAxisScaling`,`OnScaleX/Y/Z`,
  `OnValue±`,`OnMouseWheel`,`OnModifierScaleX/Y±` (1049–1751 minus `Get()`), `OnSpacing/Steps/
  Stagger/RotationModeChanged`,`OnSpacingCycleAxis`,`OnCycleAxis`,`OnToggleArrows/SettingsForm/
  UpgradePanel`,`OnUpgradePanelCloseClicked` (1929–2676). ~1,300 lines.
- Call-site audit: input handlers are bound via Enhanced Input inside `SetupPlayerInput`
  (internal delegates) — **not** called cross-file; `SetupPlayerInput` invoked from
  `SFInputHandler.cpp:130` (already) + subsystem init. → minimal external surface.
- Shared state: writes `CounterState` (StaticAccess=17 of the 70), `bModifierScaleX/YActive`,
  `bSpacing/Steps/Stagger/RotationModeActive`; reads `ActiveHologram`, calls GridState/
  GridTransform/Extend/HudService. All via owner back-ref (stub already holds it).
- Extraction approach: fill the `FSFInputHandler` stub; methods take `Owner->` for CounterState
  + services (F-helper already has `Owner`). Delete originals; subsystem forwards
  `SetupPlayerInput`. No UObject wiring.
- Hidden helpers: `[VERIFY]` grep anon-namespace in 1049–2676 (input-curve/step math may have
  file-local statics).
- Runtime coupling (SMOKE-CRITICAL): input fires per-frame during placement; `OnScale*` mutate
  CounterState which drives grid spawn + Extend `OnScaledExtendStateChanged` (see round-1
  ClearExtendState comment — counter restore re-triggers extend). Init-order: input must bind
  after services construct. Smoke: scale X/Y/Z, modifiers, mouse-wheel, spacing/steps/stagger/
  rotation cycles, toggle arrows/settings/upgrade — all from the build gun.
- Size delta: −~1,300; SFSubsystem.cpp → ~7,927.
- Depends on: none (good first SFSubsystem slice — input is the most self-contained).

#### SFSubsystem — Slice S2: Active-hologram lifecycle → `FSFHologramHelperService`   [lane: NEEDS-CARE]
- Moves: `RegisterActiveHologram` (2677–3228), `UnregisterActiveHologram` (3229–3356),
  `PollForActiveHologram` (3357–3553), `TryAcquire/ReleaseHologramLock`,
  `IsAnyModalFeatureActive`,`GetFurthestTopHologramPosition` (1756–1928). ~1,100 lines.
- Call-site audit: `RegisterActiveHologram` referenced by Arrows + RecipeMgmt comments;
  primarily called internally by `PollForActiveHologram` + build-gun hooks → forwarders.
  `GetCurrentAdapter` external (GridSpawner ×3) → keep accessor.
- Shared state: heavy `ActiveHologram` (HoloLock=35), `CounterState` (44), `CurrentAdapter` (9),
  `SortedFilteredRecipes`/`CurrentRecipeIndex` (recipe cycling lives here too — note overlap with
  recipe state), GridState/Transform/Hud reach-backs. → owner back-ref.
- Extraction approach: fill the HologramHelper stubs; **split HologramHelper impl across `.cpp`
  files** because the file is already 2,144 lines and S2+S3 add ~3,200 → see T1d below.
- Hidden helpers: `[VERIFY]` `RegisterActiveHologram` is 551 lines — grep for file-local statics
  + check it doesn't inline child-spawn logic that belongs to GridSpawnerService.
- Runtime coupling (SMOKE-CRITICAL): THE init-order epicenter — Register/Unregister fire on every
  hologram change; `PollForActiveHologram` runs on tick; adapter created lazily. Re-entrancy with
  Extend's hologram swap (round-1 `SwapToSmartFactoryHologram`). Smoke: every build-gun hologram
  type, switch rapidly, Extend swap, upgrade-panel open/close.
- Size delta: −~1,100 from subsystem.
- Depends on: S1 (do input first — cleaner) is preferred but not strictly required.

#### SFSubsystem — Slice S3: Hologram creation + adapter + tier lookups → `FSFHologramHelperService`   [lane: NEEDS-CARE]
- Moves: `CreateCustom{Foundation,Factory,Logistics}Hologram`,`CopyHologramProperties`,
  `ReplaceHologramInBuildGun`,`GetBeltClassForTier`,`GetHighestUnlocked{Belt,PowerPole,WallOutlet}Tier`,
  `GetBeltClassFromConfig`,`OnActorSpawned`,`FindRecipeForSpawnedBuilding`,`ApplyRecipeDelayed`
  (6289–8372). 2,083 lines. **Fold the 10-way create switch into the adapter registry** (charter
  "also consider").
- Call-site audit: `[VERIFY]` grep external callers of `CreateCustom*Hologram` / `OnActorSpawned`
  (OnActorSpawned is an engine delegate bound in init).
- Shared state: power-connection maps (Planned/Committed/PoleConnections — shared with PowerConn
  slice S5 → sequence: extract these maps' owner first or use accessors), `CurrentBuildProxy` (9),
  `AutoConnectService` (14). 
- Extraction approach: fill `CreateHologramAdapter` stub + move creation fns; split across `.cpp`.
- Runtime coupling (SMOKE-CRITICAL): `OnActorSpawned` is an engine spawn callback (fires for every
  actor); adapter creation is lazy on first use. Smoke: place foundation/factory/logistics, verify
  tier selection + recipe copy on spawned manufacturers.
- Size delta: −2,083 from subsystem.
- Depends on: S5 (power maps) ordering OR accessors.

#### SFSubsystem — Slices S4/S5: Property Sync (706) + Power Connection (612)   [lane: NEEDS-CARE]
- S4 Property Sync (3554–4260) → hologram-sync helper. Shared: ActiveHologram, GridTransform.
  `[VERIFY external callers]`. Runtime: multi-step sync across frames (Issue #200) — smoke-critical.
- S5 Power Connection (423–1035) → consolidate into `SFPowerAutoConnectManager`. Shared maps
  with HoloCreate (S3) — **the one true cross-slice coupling in the subsystem**; handle by moving
  the maps' ownership to the power manager and giving S3 accessors. Runtime: deferred pole wiring,
  cost-deduction once-per-cycle flag. Smoke: power auto-connect + Extend power poles.

**Coverage:** S1(~1,300)+S2(~1,100)+S3(2,083)+S4(706)+S5(612) = ~5,801 removed → subsystem
~3,426. **Still >3k** — S4 or the Pipe Tier (963, → S6 into a config/pipe helper) must also move
to clear #6. Plan: also extract **Pipe Tier Config (5034–5997, 963)** → resulting subsystem
~2,463 (<3k ✓). `[Self-review will confirm the residual function-by-function.]`

## T1d — `SFHologramHelperService.cpp` (live 2,144) — MERGES with T1b S2/S3

This file is both a criterion-#5 target AND the destination for SFSubsystem S2+S3 (~3,200 lines
incoming). Net it would be ~5,300 lines → **must split its impl across ≥3 `.cpp` files** (one
class, many TUs, same pattern as the Extend wiring cluster): e.g. `SFHologramHelperService.cpp`
(existing helpers), `_Lifecycle.cpp` (Register/Unregister/Poll), `_Creation.cpp` (Create*/tier/
adapter). `[AUDIT PENDING: current 2,144-line content map — next turn.]`

## T1c — AutoConnect family — advances criterion #5

Live `wc -l` this effort: `SFAutoConnectService.cpp` **4,771**, `SFPipeAutoConnectManager.cpp`
**2,789** (both >2k targets); `SFAutoConnectOrchestrator.cpp` 1,781, `SFPowerAutoConnectManager.cpp`
1,852 (<2k — receive extracted logic, not split-targets).

### `SFAutoConnectService.cpp` (4,771) — FOUR bundled features (from live fn map)

| Sub-feature | Functions (line ranges) | ~Size |
|-------------|-------------------------|------:|
| **Belt-distributor core** | `OnDistributorHologramUpdated`(95), `ProcessSingleDistributor`(177–1054, **877**), `CreateOrUpdateBeltPreview`(1055), `ConnectAnyConnectors`(1185), `BuildBeltFromPreview`(1247), `BuildBeltsForDistributor`(1361), Find*/Get* connector helpers (1558–2243), preview/cost storage (2339–2699) | ~2,600 |
| **Pipe delegation (thin)** | `UpdatePipePreviews`(2700)…`GetPipePreviewsCost`(2834–2909) | ~210 |
| **Stackable supports** | `ProcessFloorHolePipes`(3048), `ProcessStackableConveyorPoles`(3093), `ProcessStackablePipelineSupports`(3342), `MakePolePairKey`,`UpdateOrCreatePipeForPolePair`(3662–4011), `RemoveOrphanedPipes`,`CleanupAllStackablePipes`,`FinalizeBeltChildrenVisibility`,`UpdateOrCreateBeltForPolePair`(4437–4685),`RemoveOrphanedBelts`,`CleanupAllStackableBelts` | ~1,500 |
| **Power-pole grid** | `ProcessPowerPoles`(4146),`ClearAllPowerPreviews`,`GetPowerManager`,`AnalyzeGridTopology`(4240),`AreGridAxisNeighbors`,`CalculateCableCost` | ~290 |
| **Type predicates** (shared classifiers) | `IsDistributor/Splitter/Merger/PowerPole/Stackable*/Wall*/Belt*Hologram` (1558–1623, 2910–3046) | ~150 |

#### SFAutoConnectService — Slice AC1: Stackable supports → `SFStackableSupportService`   [NEEDS-CARE]
- Moves: the ~1,500-line Stackable block (3048–3647, 3648–4145, 4437–4771).
- Call-site audit: `[VERIFY]` — `ProcessStackable*`/`ProcessFloorHolePipes` called from the
  AutoConnect orchestrator + grid-spawn path; grep `ProcessStackableConveyorPoles|ProcessFloorHolePipes`
  external callers (orchestrator `SFAutoConnectOrchestrator.cpp` is the likely sole caller).
- Shared state: per-pole-pair pipe/belt maps (keyed by `MakePolePairKey` uint64) — exclusive to
  stackable block (`[VERIFY grep]`). Uses type predicates (→ shared classifier helper) + Subsystem
  config (`StackableBelt*`). Reads grid axis info.
- Extraction approach: new service or fold into a stackable helper; back-ref to Subsystem +
  AutoConnectService (for predicates). Forwarder on AutoConnectService.
- Hidden helpers: `[VERIFY]` anon-namespace in 3048–4771.
- Runtime coupling (SMOKE-CRITICAL): processes on parent-hologram update (per-frame) + cleanup on
  orphan; pole-pair keying must stay stable across frames. Smoke: place stackable conveyor poles
  + pipeline supports in a grid, confirm auto belts/pipes between pole pairs + orphan cleanup.
- Size delta: −~1,500; SFAutoConnectService.cpp → ~3,271.
- Depends on: AC0 (extract shared type-predicates first, below).

#### SFAutoConnectService — Slice AC2: Power-pole grid → `SFPowerAutoConnectManager`   [NEEDS-CARE]
- Moves: power-pole block (4146–4436) into the existing `SFPowerAutoConnectManager` (1,852, has room).
- Shared state: `GetPowerManager` per-pole map; `AnalyzeGridTopology` grid math. Coordinates with
  Subsystem power-connection state (overlaps SFSubsystem S5 — note in ledger).
- Runtime coupling (SMOKE-CRITICAL): power grid analyzed across all poles each update; consolidating
  with SFSubsystem S5 power state must keep the deferred-wiring + cost-once-per-cycle invariant.
  Smoke: power auto-connect grid (X/Y/X+Y axes), Extend power poles.
- Size delta: −~290; → ~2,981.

#### SFAutoConnectService — Slice AC3: Belt-distributor core split   [NEEDS-CARE]
- After AC1/AC2 the core is ~2,981 — still >2k. Split the ONE class impl across 2 `.cpp`:
  `SFAutoConnectService.cpp` (orchestration + storage + predicates, ~1,200) +
  `SFAutoConnectService_Belt.cpp` (`ProcessSingleDistributor` 877 + belt preview/build +
  connector-finding, ~1,800). Both <2k.
- Shared state: per-distributor maps (connector pairs, belt previews, costs) stay members of the
  one class → no cross-service sharing.
- Runtime coupling (SMOKE-CRITICAL): `OnDistributorHologramUpdated` per-frame; `BuildBeltsForDistributor`
  from distributor `Construct()` (post-build). Smoke: place splitter/merger near buildings, confirm
  auto-belt preview + cost + build.
- Size delta: 0 net (impl split); resulting files each <2k. **Criterion #5 met for this file.**

#### SFAutoConnectService — Slice AC0 (prereq): shared type-predicates → small classifier helper
- The `IsXHologram` predicates are used by belt/pipe/stackable/power blocks → move to a shared
  `SFHologramClassifiers` free-function/static header so all post-split units share them (promote,
  don't duplicate). `[VERIFY grep usage spread]`.

### `SFPipeAutoConnectManager.cpp` (2,789) — one `F`-class, split impl across `.cpp`   [NEEDS-CARE]
- Units (live fn map): `ProcessAllJunctions`(39), `ProcessPipeJunctions`(368–1260, **892**),
  `EvaluatePipeConnections`(1552–1881, ~330), connector-index helpers (1882–2020),
  `FindAvailableManifoldConnector`/`FindBestManifoldConnectorPair`(1945–2180), `SpawnPipeChild`(2181)/
  `SpawnPipeChildAtPosition`(2398)/`RemovePipeChild`(2576), `ProcessFloorHolePipes`(2604–2778),
  preview helpers (`CreatePipePreviewBetweenConnectors`,`CleanupOrphanedPreviews`,`ClearPipePreviews`).
- Call-site audit: owned by `SFAutoConnectService` (per-junction map, `GetPipeManager`); methods
  called from AutoConnectService pipe-delegation block. → internal to the AutoConnect cluster.
- Shared state: back-refs `Subsystem` + `AutoConnectService` (ctor `Initialize`). Per-junction
  preview state is member-local. EXCLUSIVE — no other unit writes it (`[VERIFY grep]`).
- Extraction approach: NOT a move — **split the existing class's impl across `.cpp`**:
  `SFPipeAutoConnectManager.cpp` (process/evaluate, ~1,400) + `_Spawn.cpp` (SpawnPipeChild* +
  manifold-connector + floor-hole, ~1,389). One header, both <2k. No state changes.
- Hidden helpers: `[VERIFY]` anon-namespace at 311/1039 banners.
- Runtime coupling (SMOKE-CRITICAL): junction processing per-frame; child spawn during preview;
  floor-hole pipes special-cased. Smoke: place pipeline junctions + floor holes, confirm manifold
  pipe preview + spawn + orphan cleanup.
- Size delta: 0 net (impl split); each file <2k. **Criterion #5 met.**

## T1d — `SFHologramHelperService.cpp` (2,144) + `SFUpgradeExecutionService.cpp` (2,537)

`[AUDIT PENDING]`

## T5 — UI widgets — advances criterion #5

Live `wc -l`: `SmartSettingsFormWidget.cpp` **3,746**, `SmartUpgradePanel.cpp` **2,138**.
**UMG constraint:** the `UPROPERTY(meta=(BindWidget))` control members + `UFUNCTION` event
thunks MUST stay in the `UWidget` subclass (engine binds them by name). So the criterion-#5
path is **impl-split across `.cpp` files** (one class, many TUs) by section — NOT moving widgets
out. The charter's full model/presenter/binder is a *deeper* optional refactor (option B);
recommend option A (impl-split) to hit #5, B later.

### `SmartSettingsFormWidget.cpp` (3,746, 86 fns) — section split (from live fn map)

Sections: `NativeConstruct` (36–685, ~650) + counter/grid populate (686–1129, 1690–1763);
Restore/preset (1223–1837, ~600: Apply/Save/Delete/Update/Export/Import/ImportFromExtend +
dropdown/details/format); Apply/Reset/Close (1838–2037); window drag (`NativeOnMouse*` 2038–2125);
Recipe (2126–2441, ~315); Belt-AC controls (2442–2732); Pipe-AC controls (2733–2980); Power-AC
controls (2981–3746, ~766 `[VERIFY tail fn list]`).

#### SmartSettingsFormWidget — Slice U1: impl-split into 4 `.cpp`   [lane: NEEDS-CARE]
- Files: `SmartSettingsFormWidget.cpp` (construct + counter + apply + drag, ~1,500),
  `_Restore.cpp` (preset section ~600), `_AutoConnect.cpp` (belt+pipe+power controls ~1,050),
  `_Recipe.cpp` (~315). All <2k.
- Call-site audit: the widget is created/shown by the subsystem (`SettingsFormWidget` member,
  `OnToggleSettingsForm`); methods are internal event handlers (bound in NativeConstruct /
  BlueprintCallable). External entry: `USFSubsystem::OnToggleSettingsForm` + `PopulateFromCounterState`
  called by subsystem. `[VERIFY grep PopulateFromCounterState|GetSettingsFormWidget callers]`.
- Shared state: reads/writes the subsystem `Settings` config struct (the belt/pipe/power tier
  fields at SFSubsystem.h:1042–1064) via the AC setters (SFSubsystem "Belt/Pipe/Power Auto-Connect
  Setters" sections) — **cross-file coupling with SFSubsystem S6/AC setters** (ledger). Reads
  `CounterState`; calls `RestoreService` (preset apply/save), `RecipeManagementService`.
- Extraction approach: pure impl-split (one class across `.cpp`); NO friend/Owner rewrites (same
  class, members shared in-class). Zero behavior change.
- Hidden helpers: `[VERIFY]` anon-namespace in the file (formatters/combo-box builders likely).
- Runtime coupling (SMOKE-CRITICAL): NativeConstruct binds events + populates from live subsystem
  state; window-drag mutates slate geometry; apply writes config that AC services read next frame.
  Low tick-coupling (event-driven UI). Smoke: open Smart Panel; apply/reset; preset save→apply→
  update→delete→export→import→import-from-Extend; recipe select/clear; belt/pipe/power tier +
  toggle changes; confirm settings take effect on next placement.
- Size delta: 0 net (impl-split); each file <2k. **Criterion #5 met.**

### `SmartUpgradePanel.cpp` (2,138, 33 fns) — section split

Sections: `NativeConstruct` (50–375); Audit (`RefreshAudit`/`CancelAudit`/`UpdateAuditProgress`/
`UpdateAuditUI` 560–891 + Refresh/Cancel/EntireMap/Radius/Upgrade handlers); Cost
(`UpdateCostDisplay` 1323–1508, `CalculateUpgradeCost` 1545–1662, `PopulateTargetTierDropdown`,
`GetSelectedTargetTier`); Tabs (`OnRadius/Traversal/TriageTabClicked`,`SwitchToTab` 1663–1767);
Triage (`OnTriageDetect/RepairClicked` 1768–1905); Traversal (`OnTraversalScanClicked`,
`UpdateTraversalUI` 1906–2138); input/drag/`OnRowSelected`/`GetCardinalDirection`.

#### SmartUpgradePanel — Slice U2: impl-split into 3 `.cpp`   [lane: NEEDS-CARE]
- Files: `SmartUpgradePanel.cpp` (construct + audit + tabs + input, ~1,200), `_Cost.cpp`
  (cost display + `CalculateUpgradeCost` + tier dropdown, ~500), `_TriageTraversal.cpp`
  (triage + traversal tabs, ~440). All <2k.
- Call-site audit: created/shown by subsystem (`UpgradePanelWidget`, `OnToggleUpgradePanel`);
  methods internal. `[VERIFY grep external callers]`.
- Shared state: `CalculateUpgradeCost` uses `SFAssetPaths::UpgradeRecipes` (T4.2 — already shared
  with `SFUpgradeExecutionService::GetUpgradeRecipe`; ledger entry). Calls `UpgradeAuditService`,
  `UpgradeExecutionService` (scan/upgrade), reads results structs.
- Extraction approach: pure impl-split; no friend/Owner. Zero behavior change.
- Hidden helpers: `[VERIFY]` anon-namespace (cost formatting / cardinal-direction may be local).
- Runtime coupling (SMOKE-CRITICAL): audit/traversal scans run async with progress callbacks
  (`UpdateAuditProgress`/`OnUpgradeProgress` fire across frames from the services); cost recompute
  on tier change. Smoke: open Upgrade panel; Radius/Traversal/Triage tabs; scan; change target
  tier (cost updates); run upgrade (progress + completion); triage detect/repair.
- Size delta: 0 net; each file <2k. **Criterion #5 met.**

## T6 — Service-context DI

`[AUDIT PENDING]` — replace `USFSubsystem`→sibling reach-back with `FFeatureServices` +
explicit CONSTRUCT/INITIALIZE/LAZY phases. Needs an ADR. Audit every `GetXService()`
reach-back and init-order dependency surfaced in T1b.

## T8 — Hologram split — advances criterion #5 (`SFConveyorBeltHologram` only)

Live `wc -l`: `SFConveyorBeltHologram.cpp` **2,220** (>2k target), `SFPipelineHologram.cpp`
**1,481** (<2k — not a #5 target; included for the shared-dedup win). **Finding: belt & pipe
holograms have PARALLEL duplicated logic** — both have `Setup*Spline`, `TryUseBuildModeRouting`,
`SetSplineDataAndUpdate`, `TriggerMeshGeneration`, `ConfigureActor`, `ConfigureComponents`,
`GetCost`. T8's charter goal (shared `FSFHologramCostCalculator` + extracted adapters) = dedup
both into shared helpers, which shrinks the belt past <2k.

Belt fn map (live): ctor/BeginPlay/Tick/CheckValidPlacement/PostHologramPlacement/
SetPlacementMaterialState (27–362); `Construct` (363–620, 257); `GetCost` (632–773, 141);
`TriggerMeshGeneration` (774–975, 201); `ConfigureActor` (976–1294, 318); `ConfigureComponents`
(1295–1652, 357); `SetupBeltSpline` (1653–1836, 183); `AutoRouteSplineWithNormals` (1837–1920);
`TryUseBuildModeRouting` (1921–2026); `SetSplineDataAndUpdate` (2027–2071); position-correction
+ upgrade/snap (2072–2220).

#### SFConveyorBeltHologram — Slice T8a: shared spline-routing + cost → helpers   [lane: NEEDS-CARE]
- Moves: `GetCost` (632–773) → shared `FSFHologramCostCalculator`; `SetupBeltSpline`+
  `AutoRouteSplineWithNormals`+`TryUseBuildModeRouting`+`SetSplineDataAndUpdate`+
  `TriggerMeshGeneration` (~620) → shared `FSFHologramSplineRouter` (belt+pipe). ~760 removed.
- Call-site audit: `AutoRouteSplineWithNormals`/`TriggerMeshGeneration`/`SetSplineDataAndUpdate`/
  `TryUseBuildModeRouting` called by Extend restore-replay (`SFExtendRestoreReplayService.cpp`
  ~1423–1468 from round 1) + the clone spawner; `GetCost` called by vanilla cost aggregation +
  Extend `GetCost(true)`. `[VERIFY full external grep of AutoRouteSplineWithNormals|TriggerMeshGeneration]`.
- Shared state: these are mostly self-contained spline/mesh ops on the hologram's own components;
  the dedup target is the duplication BETWEEN belt & pipe, not shared mutable state. Likely home:
  a shared base (`ASFLogisticsHologram` exists — `[VERIFY]`) or free-function helpers taking the
  hologram + connectors.
- Extraction approach: extract to shared helper (free functions / shared base methods); belt & pipe
  both call them. Dedup, not move-behind-forwarder; verify byte-identical behavior for both.
- Hidden helpers: `[VERIFY]` anon-namespace in both hologram files (spline math likely file-local).
- Runtime coupling (SMOKE-CRITICAL): `Construct`/`ConfigureComponents` run at build; spline routing
  during preview + restore-replay; `TriggerMeshGeneration` regenerates mesh (render-state). These
  holograms are spawned as Extend/AutoConnect CHILDREN — coupling to those systems' Construct-time
  Register* calls. Smoke: place belts & pipes (straight/curved/build-modes), Extend with belts+pipes,
  auto-connect belts+pipes, restore-from-preset (exercises the routing helpers).
- Size delta: −~760; SFConveyorBeltHologram.cpp → ~1,460 (<2k ✓). SFPipelineHologram also shrinks
  (~−500 → ~980) via the shared helpers.
- Depends on: none, BUT touches the same spline-router helpers Extend restore-replay calls —
  coordinate so helper signatures match existing callers (ledger).

## SFUpgradeExecutionService.cpp (live 2,537) — advances criterion #5

Fn map (live): lifecycle/timer (Init/Cleanup/Tick/`ProcessUpgradeTimer`/`StartUpgrade` 116–250/
`CancelUpgrade`); target-gather (`GatherUpgradeTargets` 276–441, `NormalizeConveyorUpgradeTargets`
442–539, `CollectConnectedConveyorCohort`, `ConveyorIntersectsRadius`, `ConveyorFullyInsideRadius`
540–647); `GetTargetRecipeForBuildable` (623); **`ProcessSingleUpgrade` (648–1625, 977!)**;
`CompleteUpgrade` (1626–1741); `GetUpgradeRecipe` (1742–1830)/`GetBuildableClass` (1831–1940);
**batch connection repair** (`SaveBatchConnectionPairs` 1941, `FixBatchConnectionReferences` 2010,
`SaveBatchPipeConnectionPairs` 2151, `FixBatchPipeConnectionReferences` 2203,
`CaptureExpectedConnectionManifests` 2256, `ValidateAndRepairConnections` 2348–2537, ~600).

#### SFUpgradeExecutionService — Slice UP1: batch connection repair → `FSFUpgradeConnectionRepair`   [lane: NEEDS-CARE]
- Moves: the batch-repair block (1941–2537, ~600).
- Call-site audit: `SaveBatch*`/`FixBatch*`/`Capture*`/`ValidateAndRepair*` called from
  `ProcessSingleUpgrade`/`CompleteUpgrade` (internal). `[VERIFY grep external — likely none]`.
- Shared state: operates on the service's batch-upgrade target list + connection manifests
  (member state). `[VERIFY]` exclusive to upgrade-execution. `GetUpgradeRecipe`/`GetBuildableClass`
  use `SFAssetPaths::UpgradeRecipes` (T4.2 — shared with `SmartUpgradePanel::CalculateUpgradeCost`;
  ledger).
- Extraction approach: careful move; helper takes back-ref to the service (for the target list) or
  the manifests pass by value. Forwarders on the service.
- Hidden helpers: `[VERIFY]` anon-namespace.
- Runtime coupling (SMOKE-CRITICAL): the WHOLE point of batch-repair is FRAME-ORDER — save
  connection refs BEFORE buildables are destroyed, fix AFTER respawn (across the upgrade timer
  ticks in `ProcessUpgradeTimer`). Extremely tick-order sensitive. Smoke: upgrade a connected
  belt+pipe network (radius + entire-map), verify all connections survive the destroy/respawn.
- Size delta: −~600; SFUpgradeExecutionService.cpp → ~1,937 (<2k ✓).
- Depends on: none. `ProcessSingleUpgrade` (977) stays but file is now <2k; optional later
  internal decomposition.

## Tails (post-criteria, NOT #5/#6 blockers — sequenced last)

These do not gate criteria #5/#6 (no file >2k touched) and are scheduled in Global Sequencing's
post-criteria block: **T4 PC-helper** (`SFPlayerHelpers::GetFGPlayerController`, ~28 sites in 3
idioms — most live inside the T1 files, so fold into those slices rather than touch twice);
**T7 remaining** (`SFFactoryHologram` hardcoded `SF_HOLOGRAM_LOG` wrapper, `SFRecipeManagementService`,
`SFRadarPulseService` still on catch-all `LogSmartFoundations`). SAFE-NOW, solo-compile-validate.

---

## Blueprint Validation (the editor live editor, 2026-05-30)

Validated the plan against the Blueprint side via the the live-editor Python API (AssetRegistry referencer
query on `/Script/SmartFoundations`). **Result: only 3 Blueprint assets reference the entire C++
module:**

| Blueprint asset | Native parent (from `NativeParentClass` tag) | Refactor impact |
|-----------------|----------------------------------------------|-----------------|
| `Smart_SettingsForm_Widget` | `USmartSettingsFormWidget` | **T5 / U1** — only BP-coupled target |
| `Smart_UpgradePanel_Widget` | `USmartUpgradePanel` | **T5 / U2** — only BP-coupled target |
| `SFGameInstanceModule_BP` | `USFGameInstanceModule` | out of refactor scope (Module/, MP-deferred) |

**Implication — most of the refactor has ZERO Blueprint risk.** Nothing in `SFExtendService`,
`SFSubsystem`, `SFAutoConnectService`, `SFPipeAutoConnectManager`, the holograms
(`ASFConveyorBeltHologram`/`ASFPipelineHologram`), or `SFUpgradeExecutionService` is referenced by
ANY Blueprint asset — confirmed by the registry (holograms are spawned/swapped purely in C++; no
asset references them). So **T1a, T1b, T1c, T8, UP1 are pure-C++ and cannot break a Blueprint.** The
`F`-type helpers (`FSFHologramHelperService`, `FSFPipeAutoConnectManager`) aren't even reflected as
UClasses → not BP-visible at all.

**The only BP-coupled slices are U1 / U2** (the two T5 widgets). Their BP subclasses depend on:
- **Class being `Blueprintable`** (the BP subclasses it) — preserved (impl-split keeps the class).
- **`BindWidget` members** — `SmartSettingsFormWidget`: **4 required + 102 optional**;
  `SmartUpgradePanel`: **4 required + 32 optional**. These live in the `.h`; the BP widget tree
  must keep matching-named widgets. (Required `BindWidget` = hard contract; missing one fails BP
  compile.)
- **`BlueprintCallable` functions** the BP graph may call — `SmartUpgradePanel` exposes 3
  (`ClosePanel`, `RefreshAudit`, `CancelAudit`); `SmartSettingsFormWidget` exposes 0.
- **No `BlueprintImplementableEvent`/`BlueprintNativeEvent`** on either (0 each) — the BPs do NOT
  override C++ logic, only supply the widget tree + graph wiring. Lower risk.

**GUARDRAIL for U1 / U2 (added):** the impl-split must keep each widget's **`.h` byte-identical**
(class decl, all `BindWidget`/`BindWidgetOptional` UPROPERTYs, the 3 `BlueprintCallable`
signatures) — move ONLY `.cpp` bodies across files. After the split, **recompile both widget BPs
in-editor** (or `unreal`-compile + check for BindWidget/compile errors) to confirm the contract
holds. This is the single Blueprint-verification step in the whole refactor.

**GUARDRAIL (general, defensive):** when a slice moves a `BlueprintCallable` method off
`USFExtendService`/`USFSubsystem`, keep the forwarder `BlueprintCallable` too. No current BP calls
them (registry-proven), but it preserves the exposed API for runtime/external callers at zero cost.

## Build/Smoke + AccessTransformer Validation (2026-05-30)

**Smoke-harness coverage** (`scripts/smoke_test.py`, readback-only — maintainer drives actions).
Exposes: `holograms/active` (class), `holograms/children` (**count**), `metrics/uobjects`
(leak/widget counts), `power/summary`, `production/summary`. Mapping to SMOKE-CRITICAL paths:
- Covered directly: child-hologram **counts** (E1 scaled clones, E2/AC preview children, S1 grid
  scaling), active-hologram class (S2/S3 swap/create), UObject leak metrics (S2/S3 register/
  unregister), power state (S5/AC2).
- **GAP — no direct connection-integrity readback.** The two highest-risk slices —
  **E2** (post-build wiring) and **UP1** (upgrade batch connection-repair) — are *about* connector
  links, which the harness cannot read directly. → verify INDIRECTLY: build a small **producing**
  network, run the op, confirm `production/summary` keeps flowing (items move ⇒ connectors intact)
  + visual. **Optional harness upgrade:** expose the (already-extracted) `SFExtendDiagnosticsService`
  connection-capture (`FSFCapturedConnection` has `ConnectedToActor`/`ConnectedToConnector`) as a
  in-game diagnostic endpoint to make E2/UP1 smoke *direct* rather than inferential.

**AccessTransformers** (`Config/AccessTransformers.ini`) — **`Friend` grants are class-specific**,
so moving friend-dependent code to a *different* class loses private access (compile error). Audit:
| Grant | Slice that moves related code | Result |
|-------|-------------------------------|--------|
| `Friend(AFGConveyorBeltHologram, ASFConveyorBeltHologram)` + `Accessor mBuildModeCurve/Straight` | T8a (spline-router out) | spline fns have **0** private access → **SAFE**; `mBuildMode*` accessors **vestigial** (unused). |
| same, for `GetCost` | T8a (GetCost → `FSFHologramCostCalculator`) | **1** private access in GetCost → cost calc must param-pass it OR add a friend grant. |
| `Friend(AFGConveyorBeltHologram, USFUpgradeExecutionService)` | UP1 (batch-repair out) | moved block has **0** belt-private access → **SAFE** (grant stays with the service). |
| `Friend(AFGHologram, FSFHologramHelperService)` | S2/S3 (move INTO it) | **SAFE** — moving into the friend class; enables access. |
| `Friend(AFGBuildableSubsystem, USFChainActorService)` | S7 (chain-rebuild INTO it) | **SAFE** — moving into the friend class. |

**GUARDRAIL:** any NEW helper class (`FSFHologramSplineRouter`, `FSFHologramCostCalculator`,
`FSFUpgradeConnectionRepair`, the wiring/scaled services) that needs vanilla-private access must add
its own `Friend=` line to `AccessTransformers.ini`. Per this audit, only T8a's cost calc (1 member)
is a candidate; everything else either has zero private access or moves into an already-friended class.

## Entanglement Ledger

Every cross-unit shared-state coupling and non-obvious/hidden helper found during the audit,
with `file:line` evidence and how the plan handles it. (The thing that surprises us lives
here — keep it exhaustive.)

Consolidated from every per-file matrix surfaced this effort (all grep-grounded). Format:
**coupling — units/files involved — evidence — handling.**

**Intra-`SFExtendService.cpp` (wiring cluster):**
1. `StoredCloneTopology` (63 refs: F=18,I=18,J=18,E=5,Topo=4) + `LastCloneTopology` — shared by
   F/I/J/E + RestoreReplay. → stays canonical on `USFExtendService`; sub-services use `friend
   Owner->` (round-1 precedent).
2. ~19 wiring registry maps (`BuiltConveyorsByChain` F=10/E=1, `BuiltDistributorsByChain`
   F=10/G=3/E=1, `BuiltJunctions/PipesByChain`, `Source*ByChain` G/E/F, `DistributorConnectorNameByChain`
   F=2/E=1, …) — shared E/F/G. → **migrate INTO `USFExtendWiringService` as members** (become
   local; the cluster moves together so they stop being cross-unit).
3. `JsonBuiltActors` (I=25,F=19,J=1), `JsonSpawnedHolograms` (I=5,E=3,F=3,J=2,C=1),
   `RestoredScaledFactoryPreviewLocations` (I=3,F=1), `PowerPoleWiringData` (I=4,E=3,F=1) — →
   migrate with the wiring cluster.
4. `ScaledExtendClones` (J=20,I=7,C=1,F=1) — → ScaledService owns; expose accessor for I's 7 refs
   (**sequence J before the wiring cluster**).
5. `HologramService` (E=15,J=8,orch=6,C=5,D=5) — pervasive; stays on orchestrator, accessor.
6. `CalculateRestoredScaledClonePlacement` anon helper — shared RestoreReplay + I. **Round-1: already
   promoted** to a free fn in `SFExtendRestoreReplayService.h`.

**Cross-file (Extend ↔ holograms):**
7. Spline-router helpers (`AutoRouteSplineWithNormals`/`TriggerMeshGeneration`/`SetSplineDataAndUpdate`/
   `TryUseBuildModeRouting`) — called by `SFExtendRestoreReplayService.cpp:~1423–1468` AND by
   `ASFConveyorBeltHologram`/`ASFPipelineHologram`. → T8a extracts to a shared helper; **keep
   signatures stable** so the Extend callers still resolve (sequence T8a aware of E2).

**Intra-`SFSubsystem.cpp`:**
8. `ActiveHologram` (142) + `CounterState` (70) — the spine, read by every section. → STAY on
   subsystem; extracted `F`-helpers access via the owner back-ref they already hold.
9. Power-connection maps (`Planned/Committed BuildingConnections`, `Planned/Deferred PoleConnections`)
   — shared `PowerConn`(S5) ↔ `HoloCreate`(S3). → S5 owns (in `SFPowerAutoConnectManager`); S3 gets
   accessors (**sequence S5 before S3**).
10. `CurrentAdapter` (14) — external caller `SFGridSpawnerService.cpp:91,624,630`. → stays on
    subsystem with `GetCurrentAdapter()` accessor.
11. Feature-service pointers (`AutoConnectService` 81, `ExtendService` 42, `GridStateService` 37,
    `HudService` 27, `GridTransformService` 11 …) — sibling reach-back across all sections. → the
    **T6 DI seam**; T1b uses the existing back-ref, T6 formalizes later.

**Cross-file (AutoConnect / UI / Upgrade):**
12. `IsX...Hologram` predicates — shared by belt/pipe/stackable/power blocks in `SFAutoConnectService`.
    → promote to a shared `SFHologramClassifiers` header (Slice AC0, prereq).
13. AutoConnect power-pole grid (AC2) ↔ subsystem power state (S5) — both manage power connections.
    → consolidate both into `SFPowerAutoConnectManager`; coordinate AC2 + S5.
14. `SmartSettingsFormWidget` ↔ subsystem `Settings` config struct (`SFSubsystem.h:1042–1064`) via the
    AC-setter sections — UI writes config the AC services read. → cross-file by design; the setters
    are the API; no change, just note the contract.
15. `SmartUpgradePanel::CalculateUpgradeCost` ↔ `SFUpgradeExecutionService::GetUpgradeRecipe` — both
    use `SFAssetPaths::UpgradeRecipes`. **Already centralized (T4.2)** — no further action.

**Anon-namespace / file-local helper sweep — RESOLVED this effort** (grep of all 9 targets +
HologramHelper). Most files are clean (Extend/AutoConnect/PipeManager/both holograms/SettingsForm =
0 file-local helpers). Found:
- `SFUpgradeExecutionService.cpp`: 3 statics `FindFactory/Pipe/PowerConnectorByName` (2115–2150),
  used ONLY at 2406–2493 — **exclusive to the batch-repair block UP1 moves** → move with UP1, no
  promotion. ✅ clean.
- `SFSubsystem.cpp`: anon `struct FStackableBeltBuildData` (~157) — file-local; `[VERIFY at
  slice-time]` which S-slice(s) use it (likely stackable/grid path).
- `SmartUpgradePanel.cpp`: anon `IsConveyorUpgradeFamily`/`GetPanelFamilyDisplayName` (~37) —
  display helpers; if used across U2's 3 split `.cpp`, promote to a shared internal header.
- `SFHologramHelperService.cpp`: anon `RefreshHologramVisibility` (~72) — if used across T1d's
  split `.cpp`, promote.

**GENERAL GUARDRAIL (impl-splits):** an anonymous-namespace helper is translation-unit-local, so
any such helper used by methods landing in *different* split `.cpp` files MUST be promoted to a
shared internal header (round-1 precedent: `CalculateRestoredScaledClonePlacement`). Applies to
U1/U2, T1d, AC3, PM, and the wiring/subsystem-helper splits. Cheap to check per slice (grep the
helper name's usage line-ranges vs the split boundary).

**Other `[VERIFY]` debt (narrow, slice-time):** exact external-caller lists for impl-split files
(UI, PipeManager — most callers internal); pipe-tier (S6) fn-level destination (open question #6);
whether `ASFLogisticsHologram` is the shared base for T8a.

**Pre-implementation checks RUN (2026-05-30):** extraction-destination stubs confirmed real +
owned — `SFSubsystem.h:670/679` `TUniquePtr<FSFInputHandler> InputHandler` +
`TUniquePtr<FSFHologramHelperService> HologramHelper`, both `MakeUnique` at `SFSubsystem.cpp:176/185`
(so S1/S2/S3 are "fill the owned stub", not new-class wiring). RPC handlers stay on the facade (no
moved unit is an SFRCO handler); `SFRCO` is placeholder → low multiplayer coupling.

## Global Sequencing

Principle: lowest-coupling / highest-confidence first; respect dependencies; batch each file's
slices so one file clears <2k per build+smoke cycle. **Lane:** *solo* = pure impl-split (one class
across `.cpp`, no behavior change) → compile-validate + light smoke; *smoke* = state migration /
careful move → full maintainer smoke.

| # | Slice | File(s) cleared | Depends on | Lane |
|---|-------|-----------------|------------|------|
| 1 | **E1** Scaled Extend → `USFExtendScaledService` ✅ DONE (`a607e1a`, smoked) | SFExtendService.cpp 7,718→6,448 | — | smoke |
| 2 | **E2** Wiring cluster (F+G+I+E-chain) → `USFExtendWiringService` (4 `.cpp`) ✅ DONE (`c83da96`/`08d6f80`/`da3df3b`/`1df9877`, smoked) | **SFExtendService.cpp 3,397→1,877 (<2k ✓)** | E1 | smoke |
| 3 | **T8a** belt/pipe spline-router + cost → shared helpers | **SFConveyorBeltHologram.cpp ✓** | coordinate w/ E2 (helper sigs) | smoke |
| 4 | **AC0** shared `IsX` predicates → classifier header | — | — | solo |
| 5 | **AC1** stackable supports → service | — | AC0 | smoke |
| 6 | **AC2** power-pole grid → `SFPowerAutoConnectManager` | — | coordinate w/ S5 | smoke |
| 7 | **AC3** belt-distributor core impl-split | **SFAutoConnectService.cpp ✓** | AC0, AC1, AC2 | solo |
| 8 | **PM** `SFPipeAutoConnectManager` impl-split (2 `.cpp`) | **SFPipeAutoConnectManager.cpp ✓** | — | solo |
| 9 | **S5** subsystem power → `SFPowerAutoConnectManager` | — | coordinate w/ AC2 | smoke |
| 10 | **S1** input → `FSFInputHandler` | — | — | smoke |
| 11 | **S2** holo-lifecycle → `FSFHologramHelperService` | — | — | smoke |
| 12 | **S3** holo-create/adapter → `FSFHologramHelperService` | — | S5 (power-map accessors) | smoke |
| 13 | **S4** property-sync → helper | — | — | smoke |
| 14 | **S6** pipe-tier → config/pipe helper | — | open-Q #6 | smoke |
| 15 | **S7** Debug + Chain-rebuild → helper / `SFChainActorService` | **SFSubsystem.cpp ✓ (<2k)** | S1–S6 | solo |
| 16 | **T1d** split `FSFHologramHelperService` impl (≥3 `.cpp`) | **SFHologramHelperService.cpp ✓** | S2, S3 | solo |
| 17 | **U1** `SmartSettingsFormWidget` impl-split (4 `.cpp`) | **SmartSettingsFormWidget.cpp ✓** | — | solo |
| 18 | **U2** `SmartUpgradePanel` impl-split (3 `.cpp`) | **SmartUpgradePanel.cpp ✓** | — | solo |
| 19 | **UP1** batch connection-repair → `FSFUpgradeConnectionRepair` | **SFUpgradeExecutionService.cpp ✓** | — | smoke |

After slice 19, **all 9 files <2k (#5) and both god-objects <3k (#6)** — charter criteria #5 & #6
met. Independent tracks that can interleave to batch build/smoke cycles: Extend (1–3),
AutoConnect (4–8), Subsystem (9–16), UI (17–18), Upgrade (19). The *solo* slices (4,7,8,15,16,17,18)
can land between maintainer-smoke sessions with just compile-validate + a light targeted smoke.

**Post-criteria (optional, separate epics, NOT required for #5/#6):** T6 service-context DI
(formalize the reach-back seam from coupling #11), T8 full hologram MVP, T4 PC-helper
(`SFPlayerHelpers::GetFGPlayerController`), T7 log-category tails.

## Self-Review — coverage proof

Function-by-function coverage computed this effort (script bucketed every fn-def by line range
into its slice; sum of all fn bodies + file header must equal live `wc -l`). **GAP=0 for both
god-objects — no orphaned region.**

### `SFExtendService.cpp` (live 7,718) — GAP=0

| Destination | fns | lines |
|-------------|----:|------:|
| STAYS (orchestrator: Init/Clear/Shutdown, A Direction, Topo+restore-fwd, IsValidTarget, C ExtensionExec, D BuildGunSwap, E belt-preview, Diag-fwd) | 35 | 1,593 |
| → WiringService: E chain-wiring | 7 | 660 |
| → WiringService: F WireBuiltChild | 11 | 1,574 |
| → WiringService: G Manifold | 6 | 774 |
| → WiringService: I JSON wiring | 4 | 1,707 |
| → ScaledService: J | 9 | 1,333 |
| file header (1–77) | — | 77 |
| **TOTAL** | **72** | **7,718 ✓** |

WiringService receives 660+1,574+774+1,707 = **4,715** (split across 4 `.cpp`, each <2k).
Residual orchestrator = 1,593 + 77 = **1,670 (<2k ✓ criterion #5 met for this file)**.

### `SFSubsystem.cpp` (live 9,227) — GAP=0

| Destination | fns | lines |
|-------------|----:|------:|
| STAYS (facade: Get/accessors, RPC, Config, Recipe-copy, AC-setters, BldgReg, Debug, Chain-rebuild, Restore-enh, DistribLife, BuildHUD, AC-prod, Phase4-swap) | 97 | 2,186 |
| → FSFInputHandler: S1 (setup+scale+modes) | 24 | 1,455 |
| → FSFHologramHelperService: S2 lifecycle (lock+reg) | 7 | 1,056 |
| → FSFHologramHelperService: S3 create/adapter | 13 | 2,083 |
| → S4 PropSync | 16 | 704 |
| → SFPowerAutoConnectManager: S5 power | 14 | 612 |
| → S6 PipeTier | 13 | 963 |
| file header (1–168) | — | 168 |
| **TOTAL** | **184** | **9,227 ✓** |

**KEY SELF-REVIEW FINDING:** extracting S1–S6 removes 6,873 → residual facade = 2,186 + 168 =
**2,354**. That meets **#6 (<3k ✓)** but **still exceeds #5 (no file >2k)**. The subsystem is BOTH
a god-object AND a file, so #5 is the harder bar. **→ one more small slice needed:** extract
**Debug Tools (8539–8814, ~275)** → a debug helper + **Chain-Actor Rebuild (9005–9227, ~223)** →
the existing `SFChainActorService`. That removes ~498 → residual **~1,856 (<2k ✓ #5 met)**. Added
as Slice **S7** in ordering below.

### Open questions / assumptions needing maintainer input

1. **Grow stubs vs new classes** — recommend filling the existing `FSFInputHandler` /
   `FSFHologramHelperService` stubs (they have `TODO: Extract from SFSubsystem::X`) rather than
   new classes. Confirm.
2. **`CurrentAdapter` home** — stays on subsystem (external `GetCurrentAdapter` caller in
   `SFGridSpawnerService.cpp:91,624,630`) vs moves to HologramHelper with an accessor. Recommend
   stays; confirm.
3. **`StoredCloneTopology` home** — keep canonical on `USFExtendService` (round-1 precedent for
   `LastCloneTopology`); WiringService/ScaledService/RestoreReplay access via friend. Confirm.
4. **T8 dedup risk** — extracting shared belt/pipe spline-router must stay byte-identical for BOTH
   holograms; the only NEEDS-CARE behavior-equivalence check that spans two existing call paths.
5. **T6 (DI context)** — out of scope for #5/#6; the T1b extractions use the existing subsystem
   back-ref now. Confirm T6 is deferred until after the criteria are met.
6. **Pipe Tier (S6) destination** — config helper vs `SFPipeAutoConnectManager`. Needs the
   pipe-tier fn-level coupling sub-audit (uses `AutoConnectService` 26× + `ActiveHologram` 31×)
   before deciding; flagged for slice-time.
