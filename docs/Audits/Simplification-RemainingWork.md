# Code-Simplicity Refactor — Remaining Work

Resume/handoff doc for the simplification effort. Branch: `refactor/simplification-audit`
(~43 commits ahead of `origin/main` at `15fe368`; tree clean; rollback tag `refactor-baseline` at
`9b15ecc`). HEAD is the branch tip — run `git log -1` for the exact commit. (Note: local `main` may
be stale; compare against `origin/main`.)
Charter: [`Simplification-GOAL.md`](Simplification-GOAL.md) · Tracker: [`SimplificationAudit.md`](SimplificationAudit.md)
· T3 design: [`ADR-T3-size-registry-format.md`](ADR-T3-size-registry-format.md).

> This is a status snapshot, not new work. The `/goal` hook is cleared; resume any time.

## Success criteria scorecard

| # | Criterion | Status |
|---|-----------|--------|
| 1 | 10-minute architecture map | ✅ done (`docs/ARCHITECTURE.md`) |
| 2 | basic smoke-test safety net | ✅ done (`scripts/smoke_test.py`) |
| 3 | one edit point for **asset paths** | ✅ done (`SFAssetPaths.h`) |
| 3b | one edit point for **building sizes** | ✅ done (`Content/Data/BuildableSizes.csv`) |
| 4 | per-feature log categories live | ✅ done (~1,900 sites; Extend/Subsystem fold into T1/T2) |
| 5 | no file >~2k lines | ⬜ needs T1/T5/T8 |
| 6 | no god-object >3k | ⬜ needs T1 |

**5 of 6 met.** The remaining open criteria (#5, #6) require the NEEDS-CARE god-object/UI/hologram epics.

## Current largest files (live `wc -l`, the targets for #5/#6)

| Lines | File | Epic |
|------:|------|------|
| 9227 | `Subsystem/SFSubsystem.cpp` | T1 |
| 6448 | `Features/Extend/SFExtendService.cpp` | T1 (round 1 H+B + slice E1 Scaled done; F/G/I remain) |
| 4771 | `Features/AutoConnect/SFAutoConnectService.cpp` | T1 (scope add) |
| 3746 | `UI/SmartSettingsFormWidget.cpp` | T5 |
| 2789 | `Features/PipeAutoConnect/SFPipeAutoConnectManager.cpp` | T1 (scope add) |
| 2537 | `Features/Upgrade/SFUpgradeExecutionService.cpp` | T1/review |
| 2220 | `Holograms/Logistics/SFConveyorBeltHologram.cpp` | T8 |
| 2144 | `Subsystem/SFHologramHelperService.cpp` | T1 (scope add) |
| 2138 | `UI/SmartUpgradePanel.cpp` | T5 |
| 1949 | `Services/SFChainActorService.cpp` | review |
| 1852 | `Features/PowerAutoConnect/SFPowerAutoConnectManager.cpp` | T1 (scope add) |
| 1805 | `Services/RadarPulse/SFRadarPulseService.cpp` | review |

(Counts refreshed 2026-05-30 after T1 round 1, from live `wc -l`. Still **9 `.cpp` files >2k lines**
— the nine rows at/above `SmartUpgradePanel.cpp` (2,138); `SFExtendService.cpp` dropped 9,515→7,718
after the H+B split but is still >2k, so the count is unchanged. The round-1 split files
`SFExtendDiagnosticsService.cpp` (~916) and `SFExtendRestoreReplayService.cpp` (~1,103) are below
the threshold, as are the T2 split files. The full decomposition plan for the remaining 9 files lives
in [`RefactorCompletionPlan.md`](RefactorCompletionPlan.md).)

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

### T2 — Clone topology split ✅ DONE (commit `9d58c9d`, smoke-passed 2026-05-30)
The original "hand-rolled JSON" premise was corrected in `ADR-T2-manifold-json.md`: the file was the
clone-topology engine plus debug dumps. Option A removed the debug dumps, renamed the file to
`SFExtendCloneTopology.*`, and split child spawning into `SFExtendCloneSpawner.cpp`. Shipping
`PackagePlugin` compile + cook clean. Smoke passed: Extend with pipes/belts/lifts/power all spawn +
wire; scaled buildings + auto-connect works; Restore from preset works (validates the
`SFRestoreTypes.h` include change).

### T1 — Decompose god-objects (the headline)
The only path to criteria #5 **and** #6. One cohesive slice per PR, pure-move first. **Full
per-slice plan (audited call sites + cross-unit coupling + runtime coupling) lives in
[`RefactorCompletionPlan.md`](RefactorCompletionPlan.md).**
- `SFExtendService.cpp` (live 7,718): **round 1 DONE + smoked** (`fd27261`) — Diagnostics →
  `SFExtendDiagnosticsService`, Restore-replay → `SFExtendRestoreReplayService`. Remaining
  units: post-build wiring (`WireBuiltChildConnections`), `GenerateAndExecuteWiring` + JSON
  registry, Scaled-Extend, Manifold — behind a thin orchestrator.
- `SFSubsystem.cpp` (live 9,227): not started — extract e.g. input binding, grid/scaling state,
  recipe state, child-hologram lifecycle, the 10-way hologram-adapter `CreateHologramAdapter()`
  switch (→ registry, see "also consider"), and power-connection state (→ `SFPowerAutoConnectManager`).
- Scope add (survey under-weighted): `SFAutoConnectService.cpp` (live 4,771),
  `SFPipeAutoConnectManager.cpp` (live 2,789), `SFHologramHelperService.cpp` (live 2,144) are
  also >2k and belong here.
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
  wrapper needs a body edit). Extend feature logs are now code-complete on `LogSmartExtend`; subsystem
  Extend-adjacent log sites remain with T1.
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
