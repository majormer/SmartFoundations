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

## Per-slice ledger

| Slice | Files | Lane | Build | Commit |
|-------|-------|------|-------|--------|
| T4.1 asset-path constants (`SFAssetPaths.h`) | TBD | SAFE-NOW | pending | pending |
