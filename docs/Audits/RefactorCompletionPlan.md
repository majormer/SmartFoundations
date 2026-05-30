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

## T1a — `SFExtendService.cpp` (7,718) — remaining units after round 1

Round 1 (committed `fd27261`) extracted **H** (Diagnostics → `SFExtendDiagnosticsService`)
and **B** (Restore-replay → `SFExtendRestoreReplayService`). Remaining cohesive units, from
the round-1 audit (line numbers are pre-round-1; **re-verify live before slicing**):

- **F** — built-child registration getters/setters + `WireBuiltChildConnections` (~1,570 lines;
  the single fn `WireBuiltChildConnections` was ~1,265).
- **I** — `GenerateAndExecuteWiring` + JSON built-actor registry (~1,705; the fn ~1,648).
- **J** — Scaled Extend (positions/previews/validate, ~1,330).
- **G** — Manifold connections (Wire/Create Manifold Belt/Pipe, ~770).
- **C** — Extension execution (TryExtend/Refresh/Cleanup/affordability, ~770).
- **E** — Belt previews + per-chain wiring (~770).
- **A** — Mode/Direction cycling (~280); **D** — build-gun hologram swapping (~185).

`[AUDIT PENDING]` — Slice Cards for F/I/J/G (the big ones) + a decision on whether A/C/D/E
collapse into a residual orchestrator under 2k.

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
