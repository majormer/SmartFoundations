# Code-Simplicity Refactor — Remaining Work

Resume/handoff doc for the simplification effort. Branch: `refactor/simplification-audit`
(18 commits ahead of `main`, HEAD `9056fc6`, tree clean, rollback tag `refactor-baseline`).
Charter: [`Simplification-GOAL.md`](Simplification-GOAL.md) · Tracker: [`SimplificationAudit.md`](SimplificationAudit.md)
· T3 design: [`ADR-T3-size-registry-format.md`](ADR-T3-size-registry-format.md).

> This is a status snapshot, not new work. The `/goal` hook is cleared; resume any time.

## Success criteria scorecard

| # | Criterion | Status |
|---|-----------|--------|
| 1 | 10-minute architecture map | ✅ done (`docs/ARCHITECTURE.md`) |
| 2 | basic smoke-test safety net | ✅ done (`scripts/smoke_test.py`) |
| 3 | one edit point for **asset paths** | ✅ done (`SFAssetPaths.h`) |
| 3b | one edit point for **building sizes** | ⬜ T3 (designed in ADR, not executed) |
| 4 | per-feature log categories live | ✅ done (~1,900 sites; Extend/Subsystem fold into T1/T2) |
| 5 | no file >~2k lines | ⬜ needs T1/T2/T5/T8 |
| 6 | no god-object >3k | ⬜ needs T1 |

**4 of 6 met.** The two open criteria (#5, #6) require the NEEDS-CARE god-object epics.

## Current largest files (live `wc -l`, the targets for #5/#6)

| Lines | File | Epic |
|------:|------|------|
| 9519 | `Features/Extend/SFExtendService.cpp` | T1 + T2 |
| 9227 | `Subsystem/SFSubsystem.cpp` | T1 |
| 4771 | `Features/AutoConnect/SFAutoConnectService.cpp` | T1 (scope add) |
| 3746 | `UI/SmartSettingsFormWidget.cpp` | T5 |
| 3694 | `Features/Extend/SFManifoldJSON.cpp` | T2 |
| 2788 | `Features/PipeAutoConnect/SFPipeAutoConnectManager.cpp` | T1 (scope add) |
| 2537 | `Features/Upgrade/SFUpgradeExecutionService.cpp` | T1/review |
| 2220 | `Holograms/Logistics/SFConveyorBeltHologram.cpp` | T8 |
| 2144 | `Subsystem/SFHologramHelperService.cpp` | T1 (scope add) |
| 2138 | `UI/SmartUpgradePanel.cpp` | T5 |
| 1949 | `Services/SFChainActorService.cpp` | review |
| 1852 | `Features/PowerAutoConnect/SFPowerAutoConnectManager.cpp` | T1 (scope add) |
| 1481 | `Holograms/Logistics/SFPipelineHologram.cpp` | T8 |

(Counts are live `wc -l` at HEAD `1e1ae90`. There are **12 files >2k lines** — that's the size of
criterion #5. `SFHologramHelperService.cpp` (2,144) is a newly-surfaced >2k file to fold into T1.)

## The hard constraint (why the rest is collaborative)

Every remaining epic is **NEEDS-CARE**, and the charter mandates an **in-game SmartMCP smoke per
slice** (foundation grid, auto-connect, extend manifold, upgrade cost). Two facts make these
turn-taking with the maintainer, not solo-completable:

1. The smoke needs the **game running** (maintainer at the keyboard).
2. The deploy-build needs the **game closed** (it locks `...SmartFoundations-Win64-Shipping.dll`;
   a deploy while the game runs fails with `UnauthorizedAccessException` → `ExitCode=1`).

So the loop per slice is: **game closed →** agent edits + `RunUAT PackagePlugin` compile-validate +
commit **→ game open →** maintainer launches, runs `scripts/smoke_test.py` + manual checks.
(`ExitCode=6` + `error C####` = real compile failure; `ExitCode=1 Error_Unknown` with compile/cook
at `ExitCode=0` = the DLL-lock deploy failure, not a code error.)

## Remaining epics (recommended order)

### T3 — Size registry → CSV (NEXT; lowest-risk NEEDS-CARE)
Fully designed in the ADR. Executes the "single edit point for building sizes" criterion.
- Steps: write `scripts/extract_size_registry.py` (parse the 14 `RegisterProfile(...)` files →
  `Config/BuildableSizes.csv`, round-trip count check) → implement CSV load in
  `RegisterDefaultProfiles()` → delete the 14 `SFBuildableSizeRegistry_*.cpp` + their decls →
  compile-validate → **smoke:** place foundation 8x4 / 8x1 / wall / ramp / a rotation-swap building,
  confirm spacing unchanged.
- Net: −~5,500 lines; satisfies criterion #3b. Risk: data-accuracy (mitigated by round-trip + smoke).

### T2 — Collapse Extend serialization (`SFManifoldJSON.cpp` 3,645)
~99% hand-rolled JSON parse/serialize. Replace with `FArchive`/struct serialization or walk-once-in-
memory. Best size-payoff per effort among the big files. Needs an ADR (serialization format) +
Extend/Restore smoke (capture preset → apply → build). Folds in the deferred Extend log migration.

### T1 — Decompose god-objects (the headline)
The only path to criteria #5 **and** #6. One cohesive slice per PR, pure-move first.
- `SFSubsystem.cpp` (7,991): extract e.g. input binding, grid/scaling state, recipe state,
  child-hologram lifecycle, the 10-way hologram-adapter `CreateHologramAdapter()` switch
  (→ registry, see "also consider"), and power-connection state (→ `SFPowerAutoConnectManager`).
- `SFExtendService.cpp` (8,357): topology walk / wiring orchestration / scaled-clone planning /
  diagnostics into focused units behind a thin orchestrator.
- Scope add (survey under-weighted): `SFAutoConnectService.cpp` (4,509),
  `SFPipeAutoConnectManager.cpp` (2,738) are also >2k and belong here.
- Each slice: compile-validate + full smoke (grid, auto-connect, extend, upgrade).

### T5 — Thin UI widgets
`SmartSettingsFormWidget.cpp` (3,242) + `SmartUpgradePanel.cpp` (2,155) → model / presenter /
event-binder so each `.cpp` drops under 2k. Smoke: open Smart Panel + Upgrade panel, exercise
apply/reset/scan/upgrade.

### T8 — Split Smart-vs-vanilla hologram logic
`SFConveyorBeltHologram.cpp` (2,220) + `SFPipelineHologram.cpp` (1,481) → extract grid adapters /
junction spawners / a shared `FSFHologramCostCalculator`; core hologram logic drops to ~600 lines.
Also remove the commented manifold-belt dead block (~lines 490–503 of the belt hologram). Smoke:
place/scale belts & pipes, confirm cost + connection.

### T6 — Inject a service context (architectural)
Replace `USFSubsystem`→sibling reach-back with a `FFeatureServices` context + explicit
CONSTRUCT/INITIALIZE/LAZY phases; ends init-order nullptr bugs. Best done alongside/after T1.
Needs an ADR (DI context). Enables isolation testing later.

## Smaller follow-ups (SAFE-NOW, can be solo with build-validate)

- **T4 tail — PC-lookup helper:** 28 `GetFirstPlayerController()`/`UGameplayStatics::GetPlayerController(World,0)`
  sites in 3 variants. Add `SFPlayerHelpers::GetFGPlayerController(World)`; most sites live inside
  Extend/Subsystem, so fold into T1 rather than touch those files twice.
- **T7 tail:** `SFFactoryHologram.cpp` (`SF_HOLOGRAM_LOG` hardcoded wrapper) + `SFRecipeManagementService`
  + `SFRadarPulseService` remain on the catch-all `LogSmartFoundations` (no fitting category, or
  wrapper needs a body edit). Extend/Subsystem log sites migrate with their T1/T2 rewrites.
- **Reusable tooling already in place:** `scripts/migrate_log_category.py` (guarded log-category
  swapper), `scripts/smoke_test.py`. `SmartFoundations.h` includes `SFLogMacros.h` so categories
  resolve module-wide.

## Beyond split/dedupe (charter "also consider")

- Hologram-adapter **registry** replacing the subsystem's 10-way switch (do within T1).
- Consolidate power-connection state wholly into `SFPowerAutoConnectManager` (within T1).
- **const-correctness:** remove hot-path `const_cast<AFGHologram*>` (e.g. `SFHologramDataRegistry::GetData`).
- **Multiplayer-safety baseline (flag, don't fix pre-1.2):** document every `GetFirstPlayerController`
  / client-side spawn / cost-charge as single-player-only vs server-authority before deep refactors.

## Process notes (lessons from the solo stretch)

- **Edit strictly sequentially, Read-before-Edit.** Large parallel batches mixing Read/Edit/build/commit
  caused mid-flight cancels + partial trees. One file: read → edit → verify counts → next.
- **Verify, don't guess** include/line context and macro forms (wrapper macros like
  `SF_HOLOGRAM_LOG` hardcode their category; `SF_*_DIAGNOSTIC_LOG` forward it).
- **Never build concurrently** with the maintainer's build/game.
- Per-epic: branch + `refactor-baseline`-style tag, small revertable PRs, CHANGELOG entry per merge.
- `.vscode/settings.json` stays blank in-repo (secrets backed up at `L:\SmartFoundations-vscode-settings.backup.json`).
