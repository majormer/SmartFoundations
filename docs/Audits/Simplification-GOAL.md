# Code-Simplicity Refactor — GOAL

## Purpose
Make SmartFoundations materially easier to work in — for the maintainer, an LLM, and outside contributors — by cutting file size, duplication, and coupling **without changing behavior**. This charter scopes a per-file audit; it is not the audit.

**Success:** no file >~2k lines; no god-object >3k; one edit point for building sizes & asset paths; per-feature log categories live; a 10-minute architecture map; a basic smoke-test safety net.

## Scope
Review **every file** under `Source/SmartFoundations/` (also `Config/`, `docs/`, `scripts/`); classify each, even no-action ones. **Non-goals:** no feature/Blueprint/asset behavior change; no 1.2 port (but flag multiplayer-safety as found).

## Themes (epics → PR-sized slices)
- **T1 Decompose god-objects** (H/H): `SFSubsystem.cpp` (9.2k) & `SFExtendService.cpp` (9.5k) → focused services + thin orchestrators; one cohesive slice per PR.
- **T2 Collapse Extend topology/serialization** (H/M): `SFManifoldJSON.cpp` (3.7k, ~99% hand-rolled JSON) + topology/manifest → walk-once / `FArchive`. Best payoff-to-effort of the big files.
- **T3 Table-drive size registry** (M/L): 14 `SFBuildableSizeRegistry_*.cpp` (~5.5k) → one DataTable/CSV. Easy early confidence-builder.
- **T4 Centralize duplicated lookups** (H/L): `SFAssetPaths.h`; `SFPlayerHelpers::GetFGPlayerController()` (13 files, 3 idioms); `GetExtendServiceSafe()`; shared clone-ID registration.
- **T5 Thin UI widgets** (M/M): split `SmartSettingsFormWidget` & `SmartUpgradePanel` into model / presenter / event-binder.
- **T6 Inject a service context** (M/H): replace `USFSubsystem`→sibling reach-back with a `FFeatureServices` context + explicit CONSTRUCT/INITIALIZE/LAZY phases; ends init-order nullptr bugs.
- **T7 Named log categories** (M/M): `SFLogMacros.h` (`LogSmartExtend`, …); honor the 16 categories already declared in `SmartFoundationsLogging.ini`; prune the 1,594 VeryVerbose lines.
- **T8 Split Smart-vs-vanilla hologram logic** (M/M): extract grid adapters / junction spawners / a shared `FSFHologramCostCalculator` from `SFConveyorBeltHologram` & `SFPipelineHologram`.

## Also consider (beyond split/dedupe)
Smoke-test harness via SmartMCP (zero automated tests today — the safety net that makes T1/T6 less risky); hologram-adapter **registry** replacing the subsystem's 10-way switch; consolidate power-connection state wholly into `SFPowerAutoConnectManager`; const-correctness policy (remove hot-path `const_cast`); committed `ARCHITECTURE.md` / expanded `CONTRIBUTING.md`.

## Classification (per item)
Impact (H/M/L) × Effort (H/M/L) × **lane**: `SAFE-NOW` (mechanical, no behavior change) vs `NEEDS-CARE` (extraction / DI / JSON removal → build-validate + in-game smoke). **Order:** SAFE-NOW + low-effort first (T3, T4) to build confidence, then NEEDS-CARE epics, one slice per PR.

## Guardrails (mandatory)
- No behavior change without an explicit CHANGELOG/PR note.
- Audit-before-edit: read all call sites before moving/renaming a symbol.
- Build-validate every change (Alpakit/UHT); NEEDS-CARE adds a SmartMCP in-game smoke (foundation grid, auto-connect preview, extend manifold, upgrade cost).
- Pure-move first: extractions move code verbatim; behavior tweaks are separate follow-up PRs.
- Baseline/rollback: tag a known-good commit per epic; one epic = one branch; small, revertable PRs.

## Tracking
Living table (file → classification → status → PR) in `docs/Audits/`; an ADR per major decision (JSON removal, DI context, registry format); CHANGELOG entry per merged slice.

## Prerequisite (security, not refactor scope)
`.vscode/settings.json` holds live Discord/GitHub/GitLab/OpenAI tokens (gitignored). **Rotate all four** and add a placeholder `settings.example.json` before this work begins.
