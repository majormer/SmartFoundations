# Code-Simplicity Audit (refactor tracking)

Charter: [`Simplification-GOAL.md`](Simplification-GOAL.md). This is the living tracker — files, classification, and status — for the per-file simplicity audit. Do not refactor without rechecking call sites; build-validate every change.

## Baseline

- Branch: `refactor/simplification-audit`
- Rollback anchor (known-good, pre-refactor): tag `refactor-baseline` at commit `9b15ecc` (the charter commit).
- Survey grounding (verified line counts): `SFSubsystem.cpp` 9,227; `SFExtendService.cpp` 9,519; `SFManifoldJSON.cpp` 3,694; `SFConveyorBeltHologram.cpp` 2,220; `SmartSettingsFormWidget.cpp` 3,746; `SmartUpgradePanel.cpp` 2,155; `SFPipelineHologram.cpp` 1,481; `SFWiringManifest.cpp` 1,317; `SFExtendTopologyService.cpp` 1,265; 14 `SFBuildableSizeRegistry_*.cpp` (~5,500); 13 files use `GetFirstPlayerController`; 1,594 `VeryVerbose` lines; 5 files hardcode the PowerLine path.

## Classification key

`SAFE-NOW` = mechanical, no behavior change. `NEEDS-CARE` = extraction/DI/serialization change → build-validate + in-game smoke. Impact/Effort = H/M/L.

## Epics

| ID | Theme | Impact/Effort | Lane | Status |
|----|-------|---------------|------|--------|
| T1 | Decompose god-objects (`SFSubsystem`, `SFExtendService`) | H/H | NEEDS-CARE | not started |
| T2 | Collapse Extend topology/serialization (`SFManifoldJSON` etc.) | H/M | NEEDS-CARE | not started |
| T3 | Table-drive the 14-file size registry | M/L | NEEDS-CARE (data-accuracy) | not started |
| T4 | Centralize duplicated lookups/asset paths/PC helpers | H/L | SAFE-NOW | in progress |
| T5 | Thin the UI widgets (model/presenter/binder) | M/M | NEEDS-CARE | not started |
| T6 | Inject a service context; explicit init phases | M/H | NEEDS-CARE | not started |
| T7 | Named log categories; prune VeryVerbose | M/M | SAFE-NOW (mostly) | not started |
| T8 | Split Smart-vs-vanilla hologram logic | M/M | NEEDS-CARE | not started |

Also tracked (beyond split/dedupe): SmartMCP smoke-test harness; hologram-adapter registry; consolidate power-connection state into `SFPowerAutoConnectManager`; const-correctness (hot-path `const_cast`); committed `ARCHITECTURE.md`/`CONTRIBUTING.md`.

## Work log

- 2026-05-29: Charter committed (`9b15ecc`). Audit tracker created; `refactor-baseline` tag set. Starting T4 (SAFE-NOW): centralize hardcoded asset paths into `SFAssetPaths.h`, then the player-controller/subsystem lookup helpers.
- 2026-05-29: T4.1 landed — added `SFAssetPaths.h` and routed the 4 power-feature PowerLine call sites (`SFWireHologram`, `PowerLinePreviewHelper`, `SFPowerAutoConnectManager`) through `SFAssetPaths::PowerLineBuildClass`. Package build SUCCESSFUL (0 errors). Deferred the 3 PowerLine sites in the two giant Extend files (`SFExtendService` 9.5k, `SFManifoldJSON` 3.7k) to fold into their T1/T2 rewrites rather than touch those files twice.
- 2026-05-29: Wrote `docs/ARCHITECTURE.md` — the 10-minute architecture map. Satisfies one of the charter success criteria; build-free contributor/LLM onboarding deliverable.
- 2026-05-29: Added per-area file classification (review-every-file pass, area level) and `CONTRIBUTING.md` (onboarding seam: build/run, conventions, PR guidelines). Autonomous SAFE-NOW + doc foundation now in place; remaining epics (T1/T2/T3/T5/T6/T8 and the bulk of T7) are NEEDS-CARE and require in-game SmartMCP smokes + maintainer review per the guardrails.

## Per-area classification (review-every-file pass)

Each Source area mapped to its primary theme(s), risk lane, and status. Per-exact-file refinement continues under each epic; the giant files are the headline targets.

| Area | Largest files (lines) | Theme(s) | Lane | Status |
|------|-----------------------|----------|------|--------|
| `Subsystem/` | `SFSubsystem.cpp` 9,227 (`.h` 1,483) | T1, T4, T6 | NEEDS-CARE | god-object; central decomposition target. Asset-path/PC dedup folds into the T1 split (don't touch twice). |
| `Features/Extend/` | `SFExtendService.cpp` 9,519; `SFManifoldJSON.cpp` 3,694; `SFWiringManifest.cpp` 1,317; `SFExtendTopologyService.cpp` 1,265 | T1, T2 | NEEDS-CARE | largest feature; serialization collapse (T2) + service split (T1). Deferred PowerLine-path dedup lives here. |
| `Features/PowerAutoConnect/` | `SFPowerAutoConnectManager.cpp` | T4 (done), power-state | partly done | T4.1 power-path dedup landed; consolidate power-connection state here (beyond-scope item). |
| `Features/AutoConnect/`, `PipeAutoConnect/` | `SFAutoConnectService.cpp` | T4 | mixed | shared lookup/asset/PC patterns. |
| `Features/Upgrade/` | `SFUpgradeExecutionService.cpp` | T4.2, review | mixed | recipe-path arrays duplicated with `SmartUpgradePanel` → shared table (T4.2). |
| `Features/Restore/` | `SFRestoreService.cpp` | T6-adjacent | NEEDS-CARE | couples to Extend; review with T6. |
| `Holograms/` | `SFConveyorBeltHologram.cpp` 2,220; `SFPipelineHologram.cpp` 1,481 | T8 | NEEDS-CARE | Smart-vs-vanilla split; child-hologram patterns shareable. |
| `UI/`, `HUD/` | `SmartSettingsFormWidget.cpp` 3,746; `SmartUpgradePanel.cpp` 2,155 | T5, T4 | NEEDS-CARE | widget model/presenter/binder split; `SFFontLibrary` done. |
| `Data/` | 14× `SFBuildableSizeRegistry_*` (~5,500); `SFHologramDataRegistry` | T3 | NEEDS-CARE (data) | registry → DataTable/CSV. |
| `Services/` | `SFChainActorService`, `SFHudService`, `SFRecipeManagementService` | review | mixed | mostly cohesive; classify individually. |
| `Constants/` | `SFAssetPaths.h` (new) | T4 | SAFE-NOW | grow as paths are centralized. |
| `Module/`, `SFRCO` | `SFGameInstanceModule`, `SFRCO` | MP-safety | defer | multiplayer-relevant; flag, don't refactor pre-1.2. |

## Per-slice ledger

| Slice | Files | Lane | Build | Commit |
|-------|-------|------|-------|--------|
| T4.1 PowerLine path → `SFAssetPaths::PowerLineBuildClass` | `SFAssetPaths.h` (new) + `SFWireHologram`, `PowerLinePreviewHelper`, `SFPowerAutoConnectManager` (4 sites) | SAFE-NOW | pass | committed |
| T4.1b PowerLine path in Extend giants (deferred) | `SFExtendService` (×2), `SFManifoldJSON` (×1) | SAFE-NOW | — | fold into T1/T2 |
