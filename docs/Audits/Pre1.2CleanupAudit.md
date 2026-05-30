# Pre-1.2 Cleanup Audit

Status: baseline audit complete for pre-1.2 cleanup planning, not a cleanup plan. Do not remove code from this list without rechecking reflection/content/config/runtime references. Current findings are grouped to avoid one-entry-per-TODO growth.

## Pre-Fix Baseline

- Branch: `codex/pre-1-2-cleanup`
- Pre-remediation rollback anchor: this committed audit baseline (`HEAD` before cleanup/remediation commits begin)
- Anchor summary: `docs: add pre-1.2 cleanup audit baseline`
- Scope note: this anchor represents the committed audit state before cleanup fixes begin. It includes `GOAL.md`, this audit document, and all current `Remediation Status:` fields.
- Code-only baseline before audit artifacts: `352f547915abf9c722b8c92afc35eab826d286b8`

## Audit Log

- 2026-05-29: Recorded this committed audit baseline as the pre-remediation rollback/compare anchor before starting cleanup implementation batches. Use this commit if a cleanup introduces build, editor, or runtime issues. The previous code-only anchor was `352f547915abf9c722b8c92afc35eab826d286b8`.
- 2026-05-29: Cleanup batch 1 removed native-only stubs/inert artifacts for F001/F002/F009/F010/F026/F033/F047. Added static and manual validation notes to each remediated entry. Maintainer build and smoke tests passed for foundations, constructors, splitters, auto-connect, pipe/power checks, Blueprint proxy grouping, HUD sanity, and Smart Upgrade triage.
- 2026-05-29: Cleanup batch 2 removed source-only reflected/content-verified cleanup items for F018/F019/F021/F023. Maintainer build and smoke tests passed for Smart input, arrows, mod configuration registration, and HUD display. Player-relative controls were confirmed as not implemented/active, matching the removed detached service evidence. Left binary/content and larger behavior-sensitive cleanup items for later batches.
- 2026-05-29: Cleanup batch 3 removed dead cost/build-result helpers for F028/F037/F045. Current authoritative cost behavior remains child-hologram/vanilla cost aggregation for placement and inline per-item Smart Upgrade affordability/deduction.
- 2026-05-28: Source/config/docs discovery pass using `rg` over `Source`, `Config`, `docs`, `.windsurf`, and `CHANGELOG.md`. Created initial deduplicated findings.
- 2026-05-28: Targeted follow-up pass for tracked backup files, misleading disabled names, pure helper modules, and SmartRestore doc status. Added F010-F013 and expanded F008 instead of duplicating missing-doc findings.
- 2026-05-28: Migration-risk marker pass over TODO/deprecated/legacy/RPC terms. Added F014-F015; grouped adapter stubs together and kept multiplayer replication separate from RCO validation placeholders.
- 2026-05-28: Build/config/reflection-sensitive pass over Build.cs, plugin/config files, AccessTransformers, input assets, and docs indexes. Added F016-F018 and avoided duplicating SmartRestore/archive doc findings already covered by F008/F013.
- 2026-05-28: Service ownership pass over subsystem-created services and reflected helpers. Added F019; confirmed hologram data service/registry are live and did not add duplicate service findings.
- 2026-05-28: Public/reflected API follow-up over arrows and Smart Upgrade UI helpers. Added F020-F022; kept reflected/content-facing items verification-gated.
- 2026-05-28: Docs/migration-risk pass over module startup hooks, AccessTransformer anchors, and chain rebuild comments. Added F023-F025; grouped chain actor behavior with existing migration-risk notes rather than duplicating F005.
- 2026-05-28: Source inventory pass over placeholder helpers, disabled deferred build paths, and native utility classes. Added F026-F029; separated true no-op placeholders from 1.2/multiplayer-sensitive helpers.
- 2026-05-28: Config/content-facing text pass over localization, content asset names, and stale helper references. Added F030 and refined F029 evidence to note an unused include rather than no references at all.
- 2026-05-28: Workflow/process-doc pass over Windsurf release, issue, and ADA workflows. Added F031-F032; kept missing reference-file details grouped with F008 where possible.
- 2026-05-28: Feature-doc/source cross-check pass over Smart Dismantle, Scaling, archive references, and Restore docs. Refined F012/F008 and recorded Smart Dismantle proxy grouping as a checked false positive rather than a new finding.
- 2026-05-28: Reflected/helper inventory pass over deprecated, legacy, and Blueprint-callable symbols. Added F033 for a native-only duplicated proxy flag; left reflected Blueprint compatibility fields verification-gated.
- 2026-05-28: Build/config/access-transformer surface pass. Added F034 for broad Build.cs dependency ballast; did not duplicate existing AccessTransformer findings.
- 2026-05-28: 1.2 ownership-assumption sweep over build gun/player controller lookups. Added F035 for first-player-controller assumptions across feature services.
- 2026-05-28: Public docs/wiki pass over README, ficsit.app reference, and wiki. Added F036 for Smart Upgrade pump support drift; left wiki `DocsURL` alone because the repo contains a tracked `wiki/` tree.
- 2026-05-28: Targeted binary-content/reflection pass over Smart Upgrade widgets, input assets, and deprecated mirror fields. Refined F006/F017/F018/F021/F022 and added false-positive notes for live deprecated mirrors.
- 2026-05-28: Module Blueprint/config placeholder pass. Refined F023: `SmartConfigClass` is live in the module Blueprint, while `CounterWidgetClass` still looks unset and the widget hook remains uncalled.
- 2026-05-28: Smart Upgrade cost/construction risk pass. Added F037-F038; separated unused batch-level helpers from live duplicated cost/refund paths that should wait for 1.2 cost API triage.
- 2026-05-28: Hand-built construction-order pass over Extend, AutoConnect, PipeAutoConnect, and power wiring. Added F039 for scattered manual child-hologram/wire spawn ordering that should be audited against 1.2 APIs.
- 2026-05-28: Feature-doc/source consistency pass. Refined F013 with current Restore field/schema drift and added F040 for AutoConnect docs understating direct wire spawn/cost paths.
- 2026-05-28: Curation pass. Added recommendation summary by classification so cleanup can separate safe 1.1 work from verification-gated and 1.2-deferred items.
- 2026-05-28: Orphan/stale artifact pass over source/docs/config/content filenames, TODO/legacy markers, scripts, and header/source pairings. No new findings; refined F030 with localization script evidence and recorded header/source mismatches as a checked false-positive pattern.
- 2026-05-28: Documentation reference pass over Markdown links, source doc references, and Windsurf workflows. Added F041 for the ADA workflow's dependency on an absent private-repo prompt; kept generic missing links grouped under F008.
- 2026-05-28: Reflected class reachability pass over UCLASS/USTRUCT surfaces, static class loads, and content-facing class properties. Added F042 for trivial specialized hologram subclasses with no native selection path; kept it verification-gated because UCLASS assets may reference them.
- 2026-05-28: Recipe/unlock migration pass over `AFGRecipeManager`, tier helpers, SmartRestore validation, and recipe storage. Added F043 for duplicated/hard-coded recipe availability checks that should be re-audited with 1.2 APIs.
- 2026-05-28: Content/path reference pass over Smart-owned assets, config assets, hard-coded paths, and migration notes. Added F044 for the stale pre-publication migration plan; existing asset/content issues remained covered by F006/F017/F022/F023/F030/F043.
- 2026-05-28: Low-reference source symbol pass over private function definitions and helper APIs. Added F045 for unused build-result tracking helpers; other low-reference candidates were already covered by existing findings or live through subsystem/service initialization.
- 2026-05-28: Reflection/content-sensitive sweep over `UPROPERTY`/`UFUNCTION`, timers/delegates, hard-coded asset paths, and binary text in `Content/`. No new findings; refined F042/F045 evidence and left content-facing removals verification-gated.
- 2026-05-28: Documentation/current-behavior dedup pass over feature docs, workflows, README, changelog, and root migration notes. No new findings; stale candidates mapped to F013/F031/F032/F036/F041/F044 or existing 1.2 migration-risk buckets.
- 2026-05-28: 1.2 header spot-check against `L:\Personal\Repos\SatisfactoryModLoader-dev-1.2`. Added F046 for the new/visible conveyor-chain subsystem surface as a potential replacement or boundary for Smart's private buildable-subsystem chain workarounds; refined migration-risk context for F038/F039/F043.
- 2026-05-28: Large-service stale compatibility pass over TODO/legacy/fallback markers. Added F047 for the disabled orphan-bounce queue in `USFChainActorService`; other candidates mapped to existing chain, cost, construction, docs, and reflection findings.
- 2026-05-28: Markdown link/reference pass over `docs`, `.windsurf`, README, changelog, and root migration plan. No new findings; the remaining missing local link (`docs/Features/README.md` -> archive consolidation index) is already covered by F008's missing archive/reference-path bucket.
- 2026-05-28: Build/source inclusion pass over `Build.cs`, plugin metadata, module startup, helper modules, subsystem-created services, and low-reference reflected classes. No new findings; candidates mapped to F010/F014/F018/F019/F026/F027/F029/F030/F034/F042/F047 or were confirmed live through subsystem/module initialization.
- 2026-05-28: Final curation pass over finding IDs, required fields, recommendation buckets, false positives, and dedup notes. No new actionable findings; confirmed 47 unique findings and the summary split below.
- 2026-05-28: SMLMCP editor verification pass. `validate_mod` reported 36 Smart assets, 0 errors, and 0 warnings. Editor input summary confirmed 20 current `InputAction` assets and one `FGInputMappingContext`. Asset registry/CDO checks found no Smart content references for `USFInputActions`, axis/direction helper services, deprecated `bArrowsVisible`, `USFUpgradeResultRow`/`RowWidgetClass`, module `CounterWidgetClass`, or the trivial specialized hologram subclasses. Reclassified F018/F019/F021/F022/F023/F042 from `needs verification` to `remove now`.
- 2026-05-29: SmartCamera sibling-mod integration check. SmartCamera depends on SmartFoundations and consumes `USFSubsystem::Get`, `OnHologramCreated`, `OnHologramDestroyed`, `TryAcquireHologramLock`, `TryReleaseHologramLock`, `GetFurthestTopHologramPosition`, and the current `/SmartFoundations/.../IA_Smart_MouseWheel` asset. No `remove now` finding targets these integration surfaces; refined F017 caution to avoid deleting current input assets while cleaning stale config names.
- 2026-05-29: Cleanup batches 1-3 committed as `b1eca6f` (pre-1.2 cleanup: deletions + trims for F001/F002/F009/F010/F018/F019/F021/F023/F026/F028/F033/F037/F045/F047). The Extend affordability/preview fix committed separately as `60d1b05` (not an audit finding). Working tree is now clean except untracked `HANDOFF.md`.
- 2026-05-29: Cleanup batch 4 committed as `1a22d61` (F042 empty hologram subclasses + `Public/Holograms/README.md` rewrite; F022 `USFUpgradeResultRow`/`RowWidgetClass`). Re-verified safe against the editor side via SMLMCP before deletion: no widget or actor Blueprint derives from any removed class, and no Smart asset references them. This completes every `remove now` / safe-1.1 cleanup candidate on the code side. The only deferred remove-now sub-item is the orphaned `WBP_UpgradeResultRow.uasset` (editor-side content deletion).
- 2026-05-29: Merge-scope note. Several `doc mismatch` / workflow findings target files that are gitignored and therefore never merge to `main`: F031/F041 (and the `.windsurf/` parts of F032), `MIGRATION_PLAN.md` (F044), and `AGENTS.md`. These do not block the merge; fix them in the working copies as convenient. Tracked documentation fixes that do affect the merge: F005, F007, F008, F013, F036, F040 (plus any non-`.windsurf` portions of F032).
- 2026-05-29: Tracked doc-mismatch fixes committed as `20051fc` (F005, F007, F008, F013, F036, F040). F036 dropped pumps from the Smart Upgrade entry list; F007 corrected the grid-size cap comment; F008 fixed two runtime input-warning logs + a wrong doc path + dead `@see` refs; F013 marked SmartRestore implemented; F040 documented Power AutoConnect direct wire spawn; F005 corrected the RepairOrphanedBelts "diagnostic-only" claim to "explicit triage that re-registers conveyors".
- 2026-05-29: F030 resolved on `font/factoryfont-ui`. The `ar`/`fa`/`th` decision was made (supported) and implemented: switched Smart UI to the runtime multi-script DescriptionText font (the offline FactoryFont could not shape Arabic/Thai), re-enabled `ar`/`fa`/`th` across all localization configs + sync + compile, corrected building-term translations against the shipped game's `AllStringTables`, and recompiled all `.locres`. Committed in `4740ae6`/`1e837a8`.
- 2026-05-29: F006 resolved on `font/factoryfont-ui`. SMLMCP widget-tree inspection (by object path) of `Smart_UpgradePanel_Widget` confirmed the legacy `AuditResultsContainer`/`RadiusSpinBox`/`UpgradeButton`/`TargetTierComboBox`/`CostDetailsText` widgets are ABSENT (stale serialized names), while `CloseButton` is a live header control. Removed the five dead `BindWidgetOptional` fields and their fallback config/usage from `SmartUpgradePanel.h/.cpp`; kept `CloseButton`. Package build validated (0 errors).
- 2026-05-29: Verification-gated investigation pass. Removed (committed `e3c3f45`) after caller analysis + SMLMCP: F012 (`FSFScalingModule`, `FSFGridArray`; kept live `FSFSpacingModule`), F014 (`FSFSmartFactoryAdapter`, `FSFSmartFoundationAdapter`; kept live `FSFSmartLogisticsAdapter`/`FSFSmartBuildableAdapter`), F020 (`FSFArrowModule` DrawDebug), F027 (`FSFConveyorConnectionHelper`), plus the stale includes they left in `SFSubsystem.cpp`. F004 (`USFValidationService`) confirmed LIVE (active callers) and kept. `FSFGridArrayTypes.h` is now unreferenced but left in place (harmless standalone reflected header). Still gated pending human/runtime/compile evidence: F006, F016, F017, F030, F034.

## Current Recommendation Summary

Current count: 47 findings. Classification split: 15 `remove now`, 11 `doc mismatch`, 10 `needs verification`, 10 `defer to 1.2`, 1 `do not touch`.

Safe 1.1 cleanup candidates — ALL REMEDIATED on the code side (committed in cleanup batches 1-4: `b1eca6f`, `1a22d61`):

- `F001`, `F009`, `F010`, `F018`, `F019`, `F021`, `F022`, `F023`, `F026`, `F028`, `F033`, `F037`, `F042`, `F045`, `F047`
- Only deferred sub-item: delete the orphaned `WBP_UpgradeResultRow.uasset` in the editor (F022 content tail).

Documentation-only fixes (no behavior change):

- Tracked + DONE (committed `20051fc`): `F002` (earlier), `F005`, `F007`, `F008`, `F013`, `F036`, `F040`.
- Gitignored files — never merge to `main`, fix in working copies as convenient: `F031`, `F041`, `MIGRATION_PLAN.md` (`F044`), and the `.windsurf/` parts of `F032`.

Verification-gated items:

- RESOLVED this pass (committed `e3c3f45`, code-caller + SMLMCP verified): `F012`, `F014`, `F020`, `F027` removed. `F004` confirmed LIVE and KEPT (`ShouldEnableFloorValidation` / `ValidateAndAdjustGridSize` have active callers).
- RESOLVED on `font/factoryfont-ui`: `F030` (`ar`/`fa`/`th` localization) — decided supported and fully re-enabled, term-corrected, and compiled (commits `4740ae6`/`1e837a8`). `F006` (upgrade-panel legacy bindings) — SMLMCP-verified that 5 of the 6 legacy widgets are absent from the tree; removed their dead fields/fallbacks and kept the live `CloseButton` (package build validated).
- STILL gated — need interactive-editor / in-game / SML / compile evidence, not safe to auto-resolve: `F016` (confirm SML 3.11/1.2 AccessTransformer mechanism before touching the C# file), `F017` (in-game Options > Controls > Mods action check), `F034` (Build.cs dependency trim needs compile validation).

Defer until Satisfactory 1.2/SML compile and runtime triage:

- `F003`, `F015`, `F024`, `F025`, `F029`, `F035`, `F038`, `F039`, `F043`, `F046`

Keep:

- `F011`

## Findings

### F001 - Hologram helper future feature stubs

- Area/file: `Source/SmartFoundations/Public/Subsystem/SFHologramHelperService.h`, `Source/SmartFoundations/Private/Subsystem/SFHologramHelperService.cpp`
- Finding summary: `CanAutoConnect`, `ApplyAutoConnect`, `CanExtend`, and `ApplyExtend` are still declared/implemented as future stubs even though Auto-Connect and Extend now have dedicated services.
- Evidence checked: `rg` found only declarations and definitions. Implementations return `false`/no-op and point to missing `docs/Future_Features_Analysis.md`.
- Risk/impact: Low direct behavior risk if truly uncalled, but public helper API may be retained accidentally.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 1; removed the helper declarations and definitions after confirming no callers.
- Test/verification note: Static check `rg "CanAutoConnect|ApplyAutoConnect|CanExtend|ApplyExtend" Source\SmartFoundations` should return no helper-stub hits. Manual smoke: retest scaled foundation placement, Auto-Connect belt placement, and Extend chain placement to confirm the dedicated services still drive behavior.
- Validation result: Maintainer build and smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed smoke tests before committing this cleanup batch.

### F002 - Missing referenced future-features document

- Area/file: `SFHologramHelperService.cpp`
- Finding summary: Stub comments reference `docs/Future_Features_Analysis.md`, but that file does not exist under `docs/`.
- Evidence checked: `Test-Path docs/Future_Features_Analysis.md` returned false; `rg --files docs | rg "Future|future|Analysis"` found no replacement.
- Risk/impact: Misleads future maintainers during cleanup or 1.2 port triage.
- Classification: `doc mismatch`
- Remediation Status: Remediated in cleanup batch 1 through F001 removal; the stale source references to `docs/Future_Features_Analysis.md` were removed with the stubs.
- Test/verification note: Static check `rg "Future_Features_Analysis" Source\SmartFoundations` should return no live source references.
- Validation result: Static/source cleanup covered by the passing cleanup batch 1 build on 2026-05-29.
- Recommended next action: No further action unless a new stale reference appears in a later docs pass.

### F003 - SFRCO authority and rate-limit checks are placeholders

- Area/file: `Source/SmartFoundations/Public/SFRCO.h`, `Source/SmartFoundations/Private/SFRCO.cpp`
- Finding summary: `HasHologramAuthority` accepts any valid hologram and `CheckRateLimit` always returns true, while comments still describe future ownership/rate-limit checks.
- Evidence checked: `ValidateScalingRequest` calls both helpers; helper bodies contain `TODO Task #22` and placeholder returns.
- Risk/impact: Important 1.2/multiplayer audit item; unsafe to remove because server RPC validation currently depends on these functions.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: Recheck SML/FactoryGame RCO ownership APIs during 1.2 port; document current behavior as placeholder if no code change is made.

### F004 - Validation service comments promise broader validation than implementation

- Area/file: `Source/SmartFoundations/Public/Subsystem/SFValidationService.h`, `Source/SmartFoundations/Private/Subsystem/SFValidationService.cpp`
- Finding summary: Header describes placement, spacing, and floor validation as service responsibilities, but several implementation paths remain TODO/prototype-level and use class-name heuristics or simple line traces.
- Evidence checked: `ValidatePlacement`, `ValidateSpacing`, `ValidateFloorRequirement`, `GetMaxSpacingForHologram`, `RequiresFloorValidation`, and `TraceForFloor` contain extraction TODOs. `ShouldEnableFloorValidation` appears actively used and more complete.
- Risk/impact: Medium. Could be mistaken for authoritative validation during 1.2 migration, but some functions may be live via `USFGridSpawnerService`.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: Map every caller before edits. Split findings into live-critical behavior vs unused/prototype helpers.

### F005 - Smart Upgrade orphan repair docs conflict with current code/UI

- Area/file: `docs/Features/SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md`, `Source/SmartFoundations/Public/Services/SFChainActorService.h`, `Source/SmartFoundations/Private/Services/SFChainActorService.cpp`, `Source/SmartFoundations/Private/UI/SmartUpgradePanel.cpp`
- Finding summary: Docs say `RepairOrphanedBelts` is diagnostic-only and does not mutate vanilla state, but current code and UI describe explicit triage repair re-registering live conveyors from orphaned tick groups.
- Evidence checked: Docs lines mention diagnostic-only/no mutation. `RepairOrphanedBelts` builds `BeltsToReRegister` and calls `ReRegisterAndQueueVanillaRebuildForBelts`; UI text says repair re-registers live conveyors.
- Risk/impact: High documentation risk for 1.2 chain-actor work; wrong guidance here can lead to preserving or removing the wrong workaround.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Update Smart Upgrade docs to distinguish automatic post-load diagnostics from explicit UI triage repair.

### F006 - Smart Upgrade legacy Blueprint bindings may be removable but are content-sensitive

- Area/file: `Source/SmartFoundations/Public/UI/SmartUpgradePanel.h`, `Source/SmartFoundations/Private/UI/SmartUpgradePanel.cpp`, `Content/SmartFoundations/UI/Smart_UpgradePanel_Widget.uasset`
- Finding summary: Multiple widgets are labeled legacy (`AuditResultsContainer`, `CloseButton`, `RadiusSpinBox`, `UpgradeButton`, `TargetTierComboBox`, `CostDetailsText`) while newer shared/radius/traversal widgets exist.
- Evidence checked: Header marks fields as legacy; `.cpp` still binds/configures them and uses them as fallbacks. Binary text scan of `Smart_UpgradePanel_Widget.uasset` found current names (`RadiusAuditResultsContainer`, `RadiusSliderSpinBox`, `RadiusTargetTierComboBox`, `RadiusCostDetailsText`, `SharedCloseButton`, `SharedUpgradeButton`) and legacy-compatible names such as `CloseButton`/`CloseButtonText`.
- Risk/impact: High if Blueprint assets still bind old names; the asset evidence means these fallbacks are not safe `remove now` candidates without editor-side widget-tree inspection.
- Classification: `needs verification`
- Remediation Status: RESOLVED on `font/factoryfont-ui`. SMLMCP widget-tree inspection of `Smart_UpgradePanel_Widget` showed `AuditResultsContainer`, `RadiusSpinBox`, `UpgradeButton`, `TargetTierComboBox`, and `CostDetailsText` are ABSENT from the tree (stale serialized names), while `CloseButton`/`CloseButtonText` DO exist (the live header close X, distinct from the bottom-row `SharedCloseButton`). Removed the five dead `BindWidgetOptional` fields plus their config/fallback usage — the new `Radius*`/`Shared*`/`Traversal*` widgets are always present and were already selected first, so the change is behavior-preserving. Kept `CloseButton` (wired to `OnCloseButtonClicked`). Package build validated (0 errors).
- Recommended next action: None. The only related tail is the orphaned `WBP_UpgradeResultRow.uasset` content (separate F022 sub-item).

### F007 - Grid size/UObject limit comments and constants disagree

- Area/file: `SFValidationService.h/.cpp`, `SFHologramHelperService.h/.cpp`
- Finding summary: `SMART_MAX_GRID_SIZE` is now `INT_MAX` with “No artificial limit,” while validation comments still describe capping at 1500. Hologram helper separately has `GRID_CHILDREN_HARD_CAP = 2000` and runtime UObject warnings.
- Evidence checked: `rg SMART_MAX_GRID_SIZE|GRID_CHILDREN_HARD_CAP` found mismatched comments/constants and two different limit mechanisms.
- Risk/impact: Medium; stale comments can confuse pre-1.2 performance/memory decisions.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Clarify the actual current limit policy: no validation cap, helper-level critical downscale at 2000 children, plus UObject warning thresholds.

### F008 - Source comments and logs reference missing docs

- Area/file: `Source/SmartFoundations/Public/Subsystem/SFSubsystem.h`, `Source/SmartFoundations/Public/Input/SFInputRegistry.h`, `Source/SmartFoundations/Private/Input/SFInputRegistry.cpp`, `Source/SmartFoundations/Private/Subsystem/SFInputHandler.cpp`, `Source/SmartFoundations/Public/SFRCO.h`, arrow/scaling headers, `Source/SmartFoundations/Public/Services/SFChainActorService.h`, `Source/SmartFoundations/Private/Features/Upgrade/SFUpgradeExecutionService.cpp`
- Finding summary: Several source comments/log messages point to docs that are not present in the current `docs/` tree.
- Evidence checked: `Test-Path` returned false for `docs/Input/SMART_INPUT_SYSTEM.md`, `docs/SMART_ARCHITECTURE_PHILOSOPHY.md`, `docs/Features/Scaling/ARROW_SYSTEM_ANALYSIS.md`, `docs/Features/Scaling/AXIS_ORIENTATION.md`, `docs/Features/Scaling/ARRAY_PLACEMENT_ARCHITECTURE.md`, `docs/Features/Upgrade/IMPL_SmartUpgrade_CurrentFlow.md`, and `docs/Open_Issues/_pending_issue_upgrade_incomplete_connections.md`. Docs also reference missing archive/reference paths such as `docs/Archive/2026/features-consolidation/`, `docs/Reference/REF_Lexicon.md`, and `docs/Reference/ISSUES_PRIORITIZED.md`. Actual Smart Upgrade doc lives under `docs/Features/SmartUpgrade/`.
- Risk/impact: Medium. Runtime logs can direct users/developers to nonexistent files, and source comments make migration triage slower.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: During docs cleanup, update paths to current docs or remove references to documents that are no longer maintained.

### F009 - HUD cost-display compatibility API appears dead

- Area/file: `Source/SmartFoundations/Public/Services/SFHudService.h`, `Source/SmartFoundations/Private/Services/SFHudService.cpp`
- Finding summary: Belt/pipe/power cost update/clear methods are implemented as deprecated no-ops, while cached cost fields are only cleared in `ResetState` and not read elsewhere.
- Evidence checked: `rg` found no callers for `UpdateBeltCosts`, `ClearBeltCosts`, `UpdatePipeCosts`, `ClearPipeCosts`, `UpdatePowerCosts`, or `ClearPowerCosts` outside definitions. Cached cost fields only appear in the header and `ResetState`.
- Risk/impact: Low if strictly native-only; higher if external modules or Blueprint-facing paths were expected, though the service methods are not marked `UFUNCTION`.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 1; removed the no-op HUD cost methods and unused cached cost fields.
- Test/verification note: Static check `rg "UpdateBeltCosts|ClearBeltCosts|UpdatePipeCosts|ClearPipeCosts|UpdatePowerCosts|ClearPowerCosts" Source\SmartFoundations` should only show unrelated live names such as `ClearBeltCostsForDistributor`. Manual smoke: place foundations, constructors, splitters, belt auto-connect, pipe auto-connect, and power auto-connect while confirming HUD counters/lift height still render and build costs still come from child hologram cost aggregation.
- Validation result: Maintainer build and smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed HUD and cost smoke tests before committing this cleanup batch.

### F010 - Tracked `.cpp.old` backup files under Source

- Area/file: `Source/SmartFoundations/Private/Features/AutoConnect/Preview/BeltPreviewHelper.cpp.old`, `Source/SmartFoundations/Private/Features/PipeAutoConnect/PipePreviewHelper.cpp.old`
- Finding summary: Two old implementation backup files are tracked under `Source`.
- Evidence checked: `git ls-files "*.old"` returns only these files. `rg` found no references to either `.cpp.old` file or `.cpp.old` generally.
- Risk/impact: Low. The files are not normal C++ build inputs and appear to be stale backups, but should receive one final content/history check before deletion.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 1; removed both tracked `.cpp.old` backup files.
- Test/verification note: Static check `rg --files Source\SmartFoundations | rg "\.cpp\.old$"` should return no files. No runtime smoke specific to this item is required because these were not build inputs.
- Validation result: Maintainer build passed on 2026-05-29.
- Recommended next action: No further action unless another tracked backup artifact appears.

### F011 - Architecture disabled registry file is live despite name

- Area/file: `Source/SmartFoundations/Private/Data/SFBuildableSizeRegistry_Architecture_Disabled.cpp`, `Source/SmartFoundations/Private/Data/SFBuildableSizeRegistry.cpp`, `Source/SmartFoundations/Public/Data/SFBuildableSizeRegistry.h`
- Finding summary: The `_Disabled` file name looks like dead/orphaned code, but it is called from the active default profile registration path.
- Evidence checked: `RegisterDefaultProfiles()` calls `RegisterArchitectureDisabled()`, the header declares it, and the implementation registers profiles with `bSupportsScaling=false`.
- Risk/impact: Medium if mistaken for removable dead code; deleting it could drop intentionally registered non-scaling architecture profiles.
- Classification: `do not touch`
- Remediation Status: No action needed; confirmed live/intentional guard entry.
- Recommended next action: Leave in place during cleanup. Later, consider renaming or adding a short comment that "disabled" means scaling-disabled, not source-disabled.

### F012 - Early scaling/spacing pure modules appear unused or partially unused

- Area/file: `Source/SmartFoundations/Public/Features/Scaling/SFScalingModule.h`, `Source/SmartFoundations/Private/Features/Scaling/SFScalingModule.cpp`, `Source/SmartFoundations/Public/Features/Scaling/FSFGridArray.h`, `Source/SmartFoundations/Private/Features/Scaling/FSFGridArray.cpp`, `Source/SmartFoundations/Public/Features/Spacing/SFSpacingModule.h`, `Source/SmartFoundations/Private/Features/Spacing/SFSpacingModule.cpp`
- Finding summary: `FSFScalingModule` and `FSFGridArray` appear to be uncalled native helper modules; `FSFSpacingModule` is only partly used for `GetSpacingModeName`. The Scaling current-flow doc still lists `FSFGridArray.h` as primary code even though the active placement flow uses `FSFPositionCalculator` through `USFGridSpawnerService`.
- Evidence checked: `rg` found `FSFScalingModule` function references only in its header/implementation plus an include in `SFSubsystem.cpp`. `FSFGridArray` references are internal to its own files. `FSFSpacingModule::GetSpacingModeName` is used by `SFSubsystem.cpp`, while other spacing helpers appear uncalled. `docs/Features/Scaling/IMPL_Scaling_CurrentFlow.md` lists `FSFGridArray.h` under key files, but its runtime flow routes child positioning through `USFGridSpawnerService::UpdateChildPositions` and `FSFPositionCalculator`.
- Risk/impact: Medium. These are native/static helpers, but docs/comments imply broader architectural use and they may have migration/history value.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: Before removal, verify no external plugin/docs/tests/content expectations and separate safe dead helpers from the one live spacing display helper.

### F013 - SmartRestore docs still describe planned/draft state while code is live

- Area/file: `docs/Features/README.md`, `docs/Features/SmartRestore/PLAN_SmartRestore_Enhanced.md`, `docs/Features/SmartRestore/IMPL_SmartRestore_ImplementationPlan.md`, `Source/SmartFoundations/Public/Features/Restore/`, `Source/SmartFoundations/Private/Features/Restore/`, `Source/SmartFoundations/Private/UI/SmartSettingsFormWidget.cpp`
- Finding summary: Feature index and SmartRestore docs frame Restore as WIP/draft planning, while the service is implemented, initialized, and wired to Settings Form UI controls.
- Evidence checked: Feature README labels SmartRestore as `WIP` and implementation as `Draft`; `SFSubsystem.cpp` creates and initializes `USFRestoreService`; `SmartSettingsFormWidget.cpp` binds apply/save/import/load preset actions and calls `CaptureCurrentState`, `SavePreset`, `ApplyPreset`, and `ImportFromLastExtend`. The implementation plan still says `SmartSettingsFormWidget` wiring is future/Phase 2 and uses `RecipeClassPath`, while live code uses `FSFRestorePreset::RecipeClassName` and serializes `recipeClassName`.
- Risk/impact: Medium/high documentation risk for 1.2 triage because Restore now intersects Extend topology, recipe switching, HUD, and UI state.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Replace or supplement planning docs with a current-flow doc, or at minimum update the feature index to show implemented-but-needs-audit status.

### F014 - Specialized Smart adapter classes have stub-only or unused feature APIs

- Area/file: `Source/SmartFoundations/Public/Holograms/Adapters/SFSmartLogisticsAdapter.h`, `Source/SmartFoundations/Private/Holograms/Adapters/SFSmartLogisticsAdapter.cpp`, `Source/SmartFoundations/Public/Holograms/Adapters/SFSmartFoundationAdapter.h`, `Source/SmartFoundations/Private/Holograms/Adapters/SFSmartFoundationAdapter.cpp`, `Source/SmartFoundations/Public/Holograms/Adapters/SFSmartFactoryAdapter.h`, `Source/SmartFoundations/Private/Holograms/Adapters/SFSmartFactoryAdapter.cpp`, `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`
- Finding summary: Smart logistics/foundation/factory specialized adapters expose phase-labeled feature methods, but several are stub-only and the factory/foundation variants do not appear to be selected by the active adapter factory.
- Evidence checked: `SFSubsystem::CreateHologramAdapter` returns `FSFSmartLogisticsAdapter` for `ASFLogisticsHologram`, but returns `FSFFactoryAdapter` for `ASFFactoryHologram` and `FSFGenericAdapter` for `ASFFoundationHologram`. `rg` found the specialized feature methods only in declarations/definitions. Stub bodies log “Phase ... stub” and do not delegate.
- Risk/impact: Medium. The classes are native-only cleanup candidates, but adapter selection is core placement infrastructure and exported APIs may have history.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: In cleanup phase, confirm no external/module references, then remove unused specialized adapter APIs or align adapter selection with real behavior.

### F015 - Server RPC handlers still lack propagation to other clients

- Area/file: `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`, `Source/SmartFoundations/Public/SFRCO.h`, `Source/SmartFoundations/Private/SFRCO.cpp`, `docs/Features/Multiplayer/PLAN_MultiplayerSupport_Matrix.md`
- Finding summary: Server-side scaling, reset, spacing-mode, and arrow-visibility RPC handlers mutate subsystem state but still contain TODOs to replicate changes to other clients.
- Evidence checked: `ApplyScalingFromRPC`, `ResetScalingFromRPC`, `SetSpacingModeFromRPC`, and `SetArrowVisibilityFromRPC` each end with `TODO Task #21: Replicate to other clients`. Multiplayer docs already say shared replicated state should live outside the RCO.
- Risk/impact: High for multiplayer/1.2 expectations, but not a 1.1 cleanup target if current supported behavior remains single-player/local.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During 1.2 multiplayer triage, define ownership and replicated state location before adding multicast/property replication.

### F016 - Legacy C# AccessTransformers file appears detached from active access transformer config

- Area/file: `AccessTransformers/SmartFoundations.AccessTransformers.cs`, `Config/AccessTransformers.ini`, `MIGRATION_PLAN.md`
- Finding summary: The repo contains both the active SML-style `Config/AccessTransformers.ini` and a C# attribute-based transformer file. The C# file grants different spline/routing access and appears referenced only by the old migration plan and itself.
- Evidence checked: `rg AccessTransformers` found `SmartFoundations.AccessTransformers.cs`, `Config/AccessTransformers.ini`, `MIGRATION_PLAN.md`, and changelog notes for the ini file. Current source uses members covered by the ini file and direct subclass access; no native code references the C# transformer file.
- Risk/impact: Medium. AccessTransformers are port-critical and should not be deleted casually, but a stale transformer file can mislead the 1.2 API audit or public export allowlist.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: Confirm which AccessTransformer mechanism SML 3.11/1.2 actually consumes. If only the ini is active, remove or archive the C# file and update `MIGRATION_PLAN.md`.

### F017 - Default input config lists old action/tag names while code binds newer assets

- Area/file: `Config/DefaultInput.ini`, `Config/DefaultGameplayTags.ini`, `Source/SmartFoundations/Private/Input/SFInputRegistry.cpp`, `Content/SmartFoundations/Input/Actions/`
- Finding summary: Config files register per-direction actions like `IA_Smart_ScaleXPositive` and tags like `Smart.Input.Scale.X.Positive`, but current content/code uses aggregate assets such as `IA_Smart_ScaleX`, `IA_Smart_MouseWheel`, `IA_Smart_IncreaseValue`, and mode-specific actions.
- Evidence checked: `rg` found only nine old `mInputActionTagBindings`/gameplay tags in config. `rg --files Content/SmartFoundations/Input` shows the current asset set does not include the old per-direction action asset names. `SFInputRegistry::BindInputActionsToSubsystem` loads the newer aggregate action assets directly. Binary text scan of `MC_Smart_BuildGunBuild.uasset` references the newer aggregate asset paths, not the old per-direction config paths. SmartCamera also loads the current `/SmartFoundations/SmartFoundations/Input/Actions/IA_Smart_MouseWheel.IA_Smart_MouseWheel` asset for zoom input, so the current aggregate mouse-wheel asset is an external integration point, not cleanup ballast.
- Risk/impact: High for controls-menu correctness and 1.2 input migration. These config entries may be stale, or they may still be required for Satisfactory's Mods controls menu in a way native binding alone does not show.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: In Unreal/SML, verify which actions appear in Options > Controls > Mods and whether `DefaultInput.ini` should be regenerated to match the current action assets. Preserve current aggregate assets, especially `IA_Smart_MouseWheel`, because SmartCamera consumes them.

### F018 - `USFInputActions` data asset class appears unused by current input path

- Area/file: `Source/SmartFoundations/Public/Input/SFInputActions.h`, `Source/SmartFoundations/Private/Input/SFInputActions.cpp`
- Finding summary: `USFInputActions` defines an older data-asset wrapper for input actions and gameplay tags, but current input binding loads action assets directly through `USFInputRegistry`.
- Evidence checked: `rg USFInputActions|SFInputActions|InputActionTagBindings` found only the class declaration/implementation and old config names; no subsystem/service code loads or references a `USFInputActions` asset. Targeted binary scan of `Content/` found current `InputAction`/`FGInputMappingContext` assets but no obvious `USFInputActions`/`SFInputActions` asset reference. SMLMCP editor asset inventory found 20 current `InputAction` assets and one `FGInputMappingContext`, and an asset-registry dependency scan found no Smart asset references to `USFInputActions`, `SFInputActions`, or `InputActions`.
- Risk/impact: Low/medium. The class is reflected, but current source and editor asset registry evidence both show no active user. Coordinate cleanup with F017 so stale config and the old wrapper do not leave confusing partial input paths.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 2; removed `USFInputActions` and its implementation.
- Test/verification note: Static check `rg "USFInputActions|SFInputActions|InputActionTagBindings" Source\SmartFoundations` should return no results. Manual smoke: verify Smart input still binds and appears in-game for scaling, spacing, nudge, and mouse wheel behavior; F017 still tracks stale config/action-name cleanup separately.
- Validation result: Maintainer build and Smart input smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed input smoke tests before committing this cleanup batch.

### F019 - Axis label and direction translation services appear detached from active UI/control flow

- Area/file: `Source/SmartFoundations/Public/Services/SFAxisLabelProvider.h`, `Source/SmartFoundations/Private/Services/SFAxisLabelProvider.cpp`, `Source/SmartFoundations/Public/Services/SFDirectionTranslationService.h`, `Source/SmartFoundations/Private/Services/SFDirectionTranslationService.cpp`, `AGENTS.md`
- Finding summary: `USFAxisLabelProvider` and `USFDirectionTranslationService` are documented as service-architecture members, but native search found no active callers outside their own definitions and generated/reflected declarations.
- Evidence checked: `rg` found `USFAxisLabelProvider`/`USFDirectionTranslationService` references only in their headers/implementations plus `AGENTS.md`. UI/HUD code formats axis labels directly and current control flow uses `ESFScaleAxis`/counter state rather than these services. SMLMCP asset-registry dependency scan over Smart content found no references to `SFAxisLabelProvider` or `SFDirectionTranslationService`.
- Risk/impact: Low. Both classes are reflected, but source and editor-visible content checks now agree that they are detached. Docs currently overstate their role in active service ownership.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 2; removed the detached axis label and direction translation service classes and updated agent architecture notes.
- Test/verification note: Static check `rg "USFAxisLabelProvider|SFAxisLabelProvider|USFDirectionTranslationService|SFDirectionTranslationService|EPlayerDirection|FSFAxisMapping|FSFDirectionMappingSet" Source\SmartFoundations AGENTS.md` should return no results. Manual smoke: toggle player-relative/hologram-relative controls if available and confirm HUD/control labels still reflect the current active formatting path.
- Validation result: Maintainer build passed on 2026-05-29. Player-relative controls were confirmed not implemented/active, so no runtime toggle path was required.
- Recommended next action: Run the listed HUD/control-label smoke tests before committing this cleanup batch.

### F020 - Legacy DrawDebug arrow module appears replaced by static mesh module

- Area/file: `Source/SmartFoundations/Public/Features/Arrows/SFArrowModule.h`, `Source/SmartFoundations/Private/Features/Arrows/SFArrowModule.cpp`, `Source/SmartFoundations/Public/Features/Arrows/SFArrowModule_StaticMesh.h`, `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`
- Finding summary: `FSFArrowModule` still implements DrawDebug-based arrows and deprecated compatibility methods, but active subsystem code owns and calls `FSFArrowModule_StaticMesh`.
- Evidence checked: `rg FSFArrowModule` found the DrawDebug module referenced only by its own header/implementation. `SFSubsystem` includes/creates `FSFArrowModule_StaticMesh`, exposes it through `GetArrowModule`, and calls its `Initialize`, `AttachToHologram`, `UpdateArrows`, `SetOrbitEnabled`, `SetLabelsVisible`, `DetachFromHologram`, and `Cleanup` paths. `SFGridSpawnerService` also updates the static mesh module. Binary text scan of `Content/` found no obvious `FSFArrowModule` string references.
- Risk/impact: Medium. The legacy module is native-only but exported and may have historical/debug value. Its header also points to missing arrow docs already grouped under F008.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: Confirm no external module/test/debug workflow depends on `FSFArrowModule`. If none, remove the DrawDebug module and stale compatibility API in cleanup; otherwise document it as an intentional fallback.

### F021 - Deprecated `bArrowsVisible` reflected flag is still synchronized

- Area/file: `Source/SmartFoundations/Public/Subsystem/SFSubsystem.h`, `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`, arrow module headers
- Finding summary: `bArrowsVisible` is marked deprecated in favor of `bArrowsRuntimeVisible`, but code still synchronizes it and arrow module usage examples still mention the old name.
- Evidence checked: `rg bArrowsVisible|bArrowsRuntimeVisible` found the deprecated reflected property, sync writes in `ToggleArrows` and `SetArrowVisibilityFromRPC`, config restore writes to `bArrowsRuntimeVisible`, and comments/examples in both arrow module headers. Native logic otherwise reads `bArrowsRuntimeVisible`. Binary text scan of `Content/` found no obvious references. SMLMCP asset-registry dependency scan found no Smart content references to `bArrowsVisible`.
- Risk/impact: Low/medium. Removing a reflected property can break external Blueprint users, but no repo-owned content references it and the runtime state has already moved to `bArrowsRuntimeVisible`.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 2; removed the deprecated reflected mirror/sync writes and updated arrow examples to `bArrowsRuntimeVisible`.
- Test/verification note: Static check `rg "bArrowsVisible" Source\SmartFoundations` should return no results. Manual smoke: toggle arrows on/off, trigger axis highlights, and verify arrows still follow `bArrowsRuntimeVisible`.
- Validation result: Maintainer build and arrow smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed arrow visibility smoke tests before committing this cleanup batch.

### F022 - Smart Upgrade result row widget class appears bypassed by current generated rows

- Area/file: `Source/SmartFoundations/Public/UI/SFUpgradeResultRow.h`, `Source/SmartFoundations/Private/UI/SFUpgradeResultRow.cpp`, `Source/SmartFoundations/Public/UI/SmartUpgradePanel.h`, `Source/SmartFoundations/Private/UI/SmartUpgradePanel.cpp`
- Finding summary: `USFUpgradeResultRow` and `RowWidgetClass` remain reflected/content-facing, but current audit results UI builds rows directly with `NewObject<UTextBlock>`, `UBorder`, and `UHorizontalBox` instead of creating row widgets.
- Evidence checked: `rg USFUpgradeResultRow|RowWidgetClass|SetupRow` found `RowWidgetClass` declared on the panel and `USFUpgradeResultRow::SetupRow`, but no native `CreateWidget<USFUpgradeResultRow>` or `SetupRow` caller. `UpdateAuditUI` populates `RadiusAuditResultsContainer`/`AuditResultsContainer` with generated Slate widget objects. `Content/SmartFoundations/UI/WBP_UpgradeResultRow.uasset` still exists. SMLMCP `get_asset` confirms `WBP_UpgradeResultRow` is parented to `/Script/UMG.UserWidget`, not `USFUpgradeResultRow`; `list_dependencies` reports no referencers; `Smart_UpgradePanel_Widget` CDO has `RowWidgetClass = None`; asset-registry scan found no Smart content references to `USFUpgradeResultRow` or `RowWidgetClass`.
- Risk/impact: Low. Both source and editor content evidence indicate the native row class/property and the row widget asset are detached from current UI generation.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 4 (commit `1a22d61`); removed `USFUpgradeResultRow` (`.h`/`.cpp`) and the unused `RowWidgetClass` property. The stale `WBP_UpgradeResultRow.uasset` is intentionally left for a separate editor-side content cleanup; it is a plain `UserWidget`, so it does not break when the C++ class is removed.
- Test/verification note: `rg "USFUpgradeResultRow|RowWidgetClass" Source\SmartFoundations` returns no results. SMLMCP re-verification (2026-05-29): `WBP_UpgradeResultRow` and `Smart_UpgradePanel_Widget` CDOs are `isUserWidget=True`/`isUSFUpgradeResultRow=False`; no Smart asset dependency references `UpgradeResultRow`.
- Recommended next action: Delete the orphaned `WBP_UpgradeResultRow.uasset` in the editor during a content-cleanup pass, then rerun a reference check.

### F023 - GameInstance widget hook placeholder appears inactive

- Area/file: `Source/SmartFoundations/Public/Module/SFGameInstanceModule.h`, `Source/SmartFoundations/Private/Module/SFGameInstanceModule.cpp`, `Content/SmartFoundations/Module/SFGameInstanceModule_BP.uasset`
- Finding summary: `RegisterWidgetHooks` and `CounterWidgetClass` remain in the module, but the hook registration body is commented placeholder code and the method is not called from lifecycle startup.
- Evidence checked: `rg RegisterWidgetHooks|CounterWidgetClass` found only the declaration, commented example body, property, and method definition. `DispatchLifecycleEvent` registers config, cost aggregation, and blueprint construct hooks but does not call `RegisterWidgetHooks`. Binary text scan of `SFGameInstanceModule_BP.uasset` shows the Blueprint is parented to `SFGameInstanceModule` and serializes `SmartConfigClass` to `Smart_Config_C`. SMLMCP `get_class_defaults` confirms `CounterWidgetClass = null`, `SmartConfigClass = Smart_Config_C`, `bRootModule = true`, and both `WidgetBlueprintHooks` and `BlueprintHooks` are empty.
- Risk/impact: Low. The active module Blueprint path is live for config registration, but the widget hook/property is editor-proven unset and inactive.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 2; removed the placeholder widget hook and unset `CounterWidgetClass` while preserving `SmartConfigClass` and lifecycle hook registration.
- Test/verification note: Static check `rg "RegisterWidgetHooks|CounterWidgetClass" Source\SmartFoundations` should return no results. Manual smoke: open the mod configuration menu and confirm Smart configuration still registers; confirm HUD still appears through `USFHudService`.
- Validation result: Maintainer build, mod configuration, and HUD smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed config/HUD smoke tests before committing this cleanup batch.

### F024 - AccessTransformer UHT anchor path is 1.2-sensitive and partly self-contradictory

- Area/file: `Source/SmartFoundations/Private/Module/SFGameInstanceModule.cpp`, `Source/SmartFoundations/Private/SF_ATAnchor.h`, `Config/AccessTransformers.ini`
- Finding summary: The module contains uncalled `SF_ForceUHT_SeeFGHolograms` and `SF_ForceUHT_SeeAnchor` helpers that claim to force UHT/reflection visibility for AccessTransformers, while the actual active transformer requirements live in `Config/AccessTransformers.ini` and include different classes/members than the anchor.
- Evidence checked: `rg SF_ForceUHT_See|USF_ATAnchor|AccessTransformers` found both static force functions only in their definitions. `SF_ATAnchor.h` references `AFGHologram`, `AFGConveyorBeltHologram`, and `AFGSplineHologram`; `Config/AccessTransformers.ini` also grants friend access for `AFGPipelineHologram`, `AFGBuildableSubsystem`, and `AFGHologram`.
- Risk/impact: High for 1.2 porting because access transformers are private-API-sensitive and build-system behavior may change. Do not remove blindly; the anchor may be cargo-cult, required by UHT, or obsolete depending on SML/UE behavior.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During 1.2 compile triage, verify which AccessTransformer declarations are actually consumed and whether the anchor is required. Then either document the anchor mechanism or remove uncalled/static dead code.

### F025 - Blueprint construct chain-rebuild hook scope and docs disagree

- Area/file: `Source/SmartFoundations/Private/Module/SFGameInstanceModule.cpp`, `Source/SmartFoundations/Private/Features/Extend/SFWiringManifest.cpp`, `Source/SmartFoundations/Public/Services/SFChainActorService.h`
- Finding summary: The Blueprint construct hook comments say it checks for a Smart Extend build, but the code rebuilds conveyor chains for every `AFGBlueprintHologram::Construct` with connected conveyor children. `SFWiringManifest` also has adjacent comments saying chain rebuild is handled by the module hook, then by `ConfigureComponents`, while later chain actor creation delegates to `USFChainActorService`.
- Evidence checked: `RegisterBlueprintConstructHook` obtains `USFExtendService* ExtendService` but never uses it as a filter before calling `RemoveConveyor`/`AddConveyor` on connected blueprint child conveyors. `SFWiringManifest::Execute` comments describe the hook-based approach, then immediately describe `ConfigureComponents` as the chain rebuild path. `CreateChainActors` later delegates to `USFChainActorService`.
- Risk/impact: High migration risk. Global blueprint hook behavior may be intentional, but the stale comments obscure which path is authoritative and whether the old `RemoveConveyor`/`AddConveyor` pattern should survive the 1.2 port.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: Re-audit blueprint construction timing with 1.2 headers/runtime. Decide whether the hook should be Smart-only, global-but-documented, or replaced by the centralized chain actor service path.

### F026 - Pipe chain resolver is placeholder-only and appears uncalled

- Area/file: `Source/SmartFoundations/Public/Features/PipeAutoConnect/SFPipeChainResolver.h`, `Source/SmartFoundations/Private/Features/PipeAutoConnect/SFPipeChainResolver.cpp`, `Source/SmartFoundations/Private/Features/PipeAutoConnect/SFPipeAutoConnectManager.cpp`
- Finding summary: `FSFPipeChainResolver` remains a Phase 2 placeholder for junction manifold evaluation, but no native code calls `EvaluateJunctionManifolds`.
- Evidence checked: `rg FSFPipeChainResolver|PipeChainResolver|EvaluateJunctionManifolds|Task 75` found the class declaration, implementation, and an include in `SFPipeAutoConnectManager.cpp`; no call sites. The implementation only logs readiness and TODOs for grouping/chaining.
- Risk/impact: Low. Native-only helper with no behavior. The include may also be stale if the manager no longer uses it.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 1; removed the placeholder resolver header/source and stale include.
- Test/verification note: Static check `rg "FSFPipeChainResolver|PipeChainResolver|EvaluateJunctionManifolds" Source\SmartFoundations` should return no results. Manual smoke: place pipe junctions with pipe auto-connect enabled and confirm previews/builds still route through the current `SFPipeAutoConnectManager` path.
- Validation result: Maintainer build and pipe auto-connect smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed pipe junction auto-connect smoke test before committing this cleanup batch.

### F027 - Conveyor connection helper appears superseded by current Extend/wiring paths

- Area/file: `Source/SmartFoundations/Public/Core/Helpers/SFConveyorConnectionHelper.h`, `Source/SmartFoundations/Private/Core/Helpers/SFConveyorConnectionHelper.cpp`, `Source/SmartFoundations/Private/Holograms/Logistics/SFConveyorBeltHologram.cpp`, `Source/SmartFoundations/Private/Holograms/Logistics/SFConveyorLiftHologram.cpp`, `Source/SmartFoundations/Private/Features/Extend/SFWiringManifest.cpp`
- Finding summary: `FSFConveyorConnectionHelper` contains standalone connection helpers from Extend development, but current belt/lift build paths use `FSFExtendChainHelper`, hologram data, and wiring manifest code instead.
- Evidence checked: `rg SFConveyorConnectionHelper|ConnectToPreviousConveyor|ConnectToDistributor|ConnectToFactory` found only declarations/definitions. `FSFExtendChainHelper` is actively called from belt/lift hologram code, and `SFWiringManifest` handles post-build wiring.
- Risk/impact: Medium. Native-only but exported; connection helper logic is sensitive because naive `SetConnection` timing can corrupt chain actors.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: Confirm no external/debug usage. If none, remove the helper rather than preserving an unused alternate conveyor connection pattern.

### F028 - Manual belt/pipe charging and disabled deferred pipe build path are dead cost logic

- Area/file: `Source/SmartFoundations/Public/Subsystem/SFSubsystem.h`, `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`, `Source/SmartFoundations/Private/Module/SFGameInstanceModule.cpp`
- Finding summary: `ChargePlayerForBelt` and `ChargePlayerForPipe` are large manual cost-deduction helpers with no native callers, and an old pipe deferred-build block is skipped by an early `return` after logging that vanilla child holograms now handle pipe construction.
- Evidence checked: `rg ChargePlayerForBelt|ChargePlayerForPipe` found only declarations and definitions. The pipe junction auto-connect block logs "using vanilla child hologram system" then returns before the legacy deferred construction code. Module startup now hooks `AFGHologram::GetCost` for preview cost aggregation.
- Risk/impact: Medium/high if accidentally reactivated, especially for 1.2 cost multipliers and central-storage rules. Low current runtime impact if truly uncalled/unreachable.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 3; removed manual belt/pipe charge helpers and the unreachable legacy pipe deferred-build tail after the active vanilla child-hologram branch.
- Test/verification note: Static check `rg "ChargePlayerForBelt|ChargePlayerForPipe|FPipeBuildData|PendingPipeData" Source\SmartFoundations` should return no results except any unrelated historical docs. Manual smoke: place belt and pipe auto-connect builds with normal costs and insufficient materials, confirming vanilla child hologram cost aggregation still charges/blocks correctly.
- Recommended next action: Run the listed belt/pipe auto-connect affordability smoke tests before committing this cleanup batch.

### F029 - Network helper exists without active callers

- Area/file: `Source/SmartFoundations/Public/Core/Helpers/SFNetworkHelper.h`, `Source/SmartFoundations/Private/Core/Helpers/SFNetworkHelper.cpp`, `docs/Features/Multiplayer/PLAN_MultiplayerSupport_Matrix.md`
- Finding summary: `FSFNetworkHelper` provides multiplayer mode/policy helpers, but current native search found no actual method callers. Multiplayer docs mention it exists and that usage appears limited.
- Evidence checked: `rg FSFNetworkHelper|IsMultiplayer|ShouldEnableFeature|LogNetworkState` found the helper declaration/implementation, one include in `SFArrowModule_StaticMesh.cpp`, and one multiplayer matrix note. No source outside the helper calls `IsMultiplayer`, `ShouldEnableFeature`, `LogNetworkState`, or related methods.
- Risk/impact: Medium. This may be intentional groundwork for multiplayer/1.2, but it should not be mistaken for active gating of Smart features today.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During multiplayer/1.2 triage, either wire this helper into real feature gates or remove/update the docs so unsupported multiplayer behavior is not implied to be guarded.

### F030 - Localization folders include unsupported/uncompiled cultures

- Area/file: `Config/PluginLocalization.ini`, `Config/Localization/SmartFoundations_Gather.ini`, `Config/Localization/SmartFoundations_Compile.ini`, `Content/Localization/SmartFoundations/ar/`, `Content/Localization/SmartFoundations/fa/`, `Content/Localization/SmartFoundations/th/`
- Finding summary: Arabic, Persian, and Thai localization folders are present with `.archive`/`.po` files, but the plugin localization and gather/compile configs list only 18 active cultures and omit `ar`, `fa`, and `th`.
- Evidence checked: `Get-ChildItem Content/Localization/SmartFoundations -Directory` found `ar`, `fa`, and `th` alongside supported cultures. `PluginLocalization.ini`, `SmartFoundations_Gather.ini`, and `SmartFoundations_Compile.ini` list `en`, `de`, `es`, `fr`, `it`, `ja`, `ko`, `pl`, `pt-BR`, `ru`, `tr`, `zh-Hans`, `zh-Hant`, `bg`, `hu`, `no`, `uk`, and `vi`, but not `ar`, `fa`, or `th`. The unsupported folders have no `.locres` in the file inventory. `scripts/sync_po_to_archive.py` omits those three languages from `LANGS`, while `scripts/compile_localization.ps1` explicitly removes `ar`, `fa`, and `th` `.locres` files after compilation.
- Risk/impact: Low/medium. These may be unfinished translations, intentionally parked work, or stale generated files. They can confuse release localization status and package contents.
- Classification: `needs verification`
- Remediation Status: RESOLVED on `font/factoryfont-ui` (commits `4740ae6`/`1e837a8`). Decision: `ar`/`fa`/`th` ARE supported. Re-enabled across `PluginLocalization.ini`, both gather/compile configs, `scripts/sync_po_to_archive.py`, and the `compile_localization.ps1` Phase-4 disable list; regenerated `.locres` for all three. The real blocker was the offline FactoryFont's inability to shape Arabic/Thai (tofu), fixed by switching Smart UI to the runtime DescriptionText font. Translations validated compile-safe (0 placeholder mismatches); ~76% complete with English fallback for the rest (16 Triage keys + ~35 empty per language).
- Recommended next action: Optional later translation pass to fill the remaining untranslated strings for `ar`/`fa`/`th`. No longer a cleanup blocker.

### F031 - Release workflow still assumes v29 while plugin is v30

- Area/file: `.windsurf/workflows/prepare-release.md`, `SmartFoundations.uplugin`, `docs/Reference/FicsitApp/ficsit.app.SmartFoundations.md`
- Finding summary: The release workflow hardcodes the `29.X.X` release series and v29 examples, while the plugin and ficsit.app reference are already at `30.0.0`.
- Evidence checked: `SmartFoundations.uplugin` has `"Version": 30`, `"VersionName": "30.0.0"`, and `"SemVersion": "30.0.0"`. `docs/Reference/FicsitApp/ficsit.app.SmartFoundations.md` says current release `v30.0.0`. `.windsurf/workflows/prepare-release.md` still says current format `29.X.X`, `Version` currently 29, `Current: 29.2.X`, tag format `v29.X.X`, and many v29 examples.
- Risk/impact: Medium. The workflow could cause wrong version bumps, tags, commit messages, or release notes during the pre-1.2/v30 cycle.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Update release workflow examples to be major-version-neutral or explicitly v30-aware. Keep build/package steps marked manual and consistent with agent no-build rules.

### F032 - Issue-tracking guidance is split between tracker repo and code repo

- Area/file: `.windsurf/workflows/create-branch.md`, `.windsurf/workflows/create-issue.md`, `.windsurf/workflows/get-open-issues.md`, `docs/Reference/PLAN_Satisfactory12_Response.md`, `docs/Reference/FicsitApp/ficsit.app.SmartFoundations.md`
- Finding summary: Workflow docs say new issues belong in `SmartFoundations/SmartIssueTracker`, while current 1.2 planning and public ficsit.app docs link to `majormer/SmartFoundations/issues`.
- Evidence checked: `create-issue.md` says issues are always created in `SmartFoundations/SmartIssueTracker`; `create-branch.md` sends users to that repo when no issue exists; `get-open-issues.md` snapshots the tracker. `PLAN_Satisfactory12_Response.md` links 1.2 tracking issues and milestone under `majormer/SmartFoundations`. The ficsit.app reference tells users to report bugs at `https://github.com/majormer/SmartFoundations/issues`.
- Risk/impact: Medium. During cleanup/1.2 migration, issue numbers, branch names, and public reporting can diverge if the repo-of-record is unclear.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Decide the authoritative issue repo after the public-source transition. Update workflows and public docs consistently, or document the split if both repos intentionally remain in use.

### F033 - Subsystem blueprint-proxy recent-spawn flag is duplicated and unused

- Area/file: `Source/SmartFoundations/Public/Subsystem/SFSubsystem.h`, `Source/SmartFoundations/Public/Services/SFRecipeManagementService.h`, `Source/SmartFoundations/Private/Services/SFRecipeManagementService.cpp`
- Finding summary: `USFSubsystem` still declares a native `bBlueprintProxyRecentlySpawned` flag, but all current blueprint-proxy spawn tracking lives in `USFRecipeManagementService`.
- Evidence checked: `rg bBlueprintProxyRecentlySpawned` found the subsystem field declaration only in `SFSubsystem.h`. The service initializes, sets, clears, reads, and exposes its own field through `IsBlueprintProxyRecentlySpawned`. `USFSubsystem::ClearBlueprintProxyFlag` delegates to `RecipeManagementService->ClearBlueprintProxyFlag()` and does not use the subsystem field.
- Risk/impact: Low. The subsystem field is not reflected and appears to be stale extraction residue, but the active service flag is behavior-sensitive for recipe application around blueprint proxy spawns.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 1; removed only the unused `USFSubsystem` member and left the active `USFRecipeManagementService` flag intact.
- Test/verification note: Static check `rg "bBlueprintProxyRecentlySpawned" Source\SmartFoundations` should show only `SFRecipeManagementService` references. Manual smoke: place a scaled grid that creates a Blueprint proxy, then verify recipe restoration/Smart Dismantle proxy grouping still works.
- Validation result: Maintainer build and Blueprint proxy smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed Blueprint proxy recipe/grouping smoke test before committing this cleanup batch. Continue to preserve the service-level flag.

### F034 - Build.cs still carries broad starter dependency ballast

- Area/file: `Source/SmartFoundations/SmartFoundations.Build.cs`
- Finding summary: The module dependency list still includes a broad "FactoryGame transitive dependencies" starter set. Several modules have no obvious source references and may no longer be needed by Smart's active code.
- Evidence checked: Direct source search found active references for `EnhancedInput`, `GameplayTags`, `Json`, `Slate`, and `UMG`. The same search found zero source hits outside Build.cs for `DeveloperSettings`, `ApplicationCore`, `PhysicsCore`, `GeometryCollectionEngine`, `AnimGraphRuntime`, `AssetRegistry`, `NavigationSystem`, `AIModule`, `GameplayTasks`, `RenderCore`, `CinematicCamera`, `Foliage`, `NetCore`, and `JsonUtilities`. Build.cs comments explicitly say not all listed dependencies are required.
- Risk/impact: Medium for 1.2 compile triage. Extra dependencies may hide missing direct dependencies, pull modules whose names/APIs changed, or slow diagnosis when UE/SML module boundaries shift. Removal still needs compile validation, which this audit branch must not perform.
- Classification: `needs verification`
- Remediation Status: Not started.
- Recommended next action: During a later cleanup/build-validation pass, remove suspect dependencies incrementally and let the maintainer compile/package. Keep dependencies with proven includes or link requirements.

### F035 - Feature services still rely on first/local player controller assumptions

- Area/file: `SFInputHandler.cpp`, `SFHudService.cpp`, `SFAutoConnectService.cpp`, `SFPipeAutoConnectManager.cpp`, `SFPowerAutoConnectManager.cpp`, `SFExtendService.cpp`, `SFExtendDetectionService.cpp`, `SFExtendHologramService.cpp`, `SFUpgradeExecutionService.cpp`, `SFRadarPulseService.cpp`, `SFArrowModule_StaticMesh.cpp`
- Finding summary: Many feature paths still derive player/build-gun context from `GetFirstPlayerController`, `UGameplayStatics::GetPlayerController(World, 0)`, or player index 0 instead of a request/owner controller.
- Evidence checked: `rg GetFirstPlayerController|UGameplayStatics::GetPlayerController|GetPlayerCharacter` found first-player lookups in input context toggling, HUD visibility, AutoConnect tier/cost logic, pipe/power managers, Extend build-gun helpers, Smart Upgrade execution, RadarPulse capture, and arrow camera math. Some nearby code already uses `LastController` or widget owning players, so the pattern is mixed rather than centralized.
- Risk/impact: High for multiplayer and possibly 1.2 input/controller behavior; low for current single-player testing. These assumptions can select the wrong player inventory, unlock state, build gun, camera, or HUD context once multiple local/remote controllers exist.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During 1.2/multiplayer triage, define an authoritative player-context flow per feature. Prefer request/owning controller plumbing for world mutations and UI-owned controllers for local presentation; leave camera-only first-player calls documented if they are intentionally local-only.

### F036 - Public Smart Upgrade docs overstate pump upgrade support

- Area/file: `README.md`, `docs/Reference/FicsitApp/ficsit.app.SmartFoundations.md`, `wiki/Smart-Upgrade.md`, `docs/Features/SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md`, `Source/SmartFoundations/Private/Features/Upgrade/SFUpgradeAuditService.cpp`, `Source/SmartFoundations/Private/Features/Upgrade/SFUpgradeExecutionService.cpp`
- Finding summary: Public README and ficsit.app copy say Smart Upgrade opens/works while holding a pump, but current implementation docs and code treat pumps as traversal context only or exclude them from upgrade consideration.
- Evidence checked: README and ficsit.app reference say "holding a belt, lift, pipe, pump, power line, or wall outlet" opens Smart Upgrade. `wiki/Smart-Upgrade.md` says pipeline pumps are not supported and the basic flow excludes pumps. Current-flow docs list Pump as "Audit/traversal context only" and "execution not implemented." `USFUpgradeAuditService::GetUpgradeFamily` returns `None` for `AFGBuildablePipelinePump`/`PipelinePump` classes, and `USFUpgradeExecutionService::GetUpgradeRecipe`/`GetBuildableClass` leave `Pump` as TODO.
- Risk/impact: Medium public support risk. Players may expect pump upgrades to work, and 1.2 pipe/pump changes could waste triage time if docs do not separate traversal-through-pumps from upgrading pumps.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Update public README/ficsit.app text to remove pumps from upgrade entry/upgradeable lists unless pump execution is implemented. Keep notes that pipe traversal can cross pumps if that behavior remains true.

### F037 - Unused Smart Upgrade batch cost helpers duplicate stale cost logic

- Area/file: `Source/SmartFoundations/Public/Features/Upgrade/SFUpgradeExecutionService.h`, `Source/SmartFoundations/Private/Features/Upgrade/SFUpgradeExecutionService.cpp`
- Finding summary: `CalculateTotalUpgradeCost`, `CanAffordUpgrade`, and `DeductUpgradeCosts` remain declared/defined, but the live execution path performs per-item family-specific affordability/deduction inline instead.
- Evidence checked: `rg CalculateTotalUpgradeCost|CanAffordUpgrade|DeductUpgradeCosts` found only declarations and definitions. The live belt/pipe/lift/pole upgrade branches calculate net cost inline from hologram/recipe cost plus dismantle refunds, then call inventory/central-storage APIs directly.
- Risk/impact: Low code risk if removed later; medium maintenance risk while kept because these helpers encode a different deduction order/refund model than the live path.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 3; removed unused Smart Upgrade batch cost helper declarations/definitions.
- Test/verification note: Static check `rg "CalculateTotalUpgradeCost|CanAffordUpgrade|DeductUpgradeCosts" Source\SmartFoundations` should return no results. Manual smoke: run Smart Upgrade audit, verify displayed/previewed costs, test an affordability failure if practical, then perform at least one successful upgrade.
- Recommended next action: Run the listed Smart Upgrade audit/cost/affordability smoke tests before committing this cleanup batch.

### F038 - Smart Upgrade cost/refund handling is duplicated across live family paths

- Area/file: `Source/SmartFoundations/Private/Features/Upgrade/SFUpgradeExecutionService.cpp`, `Source/SmartFoundations/Private/UI/SmartUpgradePanel.cpp`, `docs/Reference/PLAN_Satisfactory12_Response.md`
- Finding summary: Live Smart Upgrade execution repeats inventory/central-storage affordability, refund, and overflow handling separately for belts, pipes, lifts, and poles, while the panel has its own display-side `CalculateUpgradeCost`. The 1.2 plan already flags recipe cost multipliers as critical.
- Evidence checked: `SFUpgradeExecutionService.cpp` has repeated blocks using `GetBaseCost`/recipe ingredients, `GetDismantleRefundReturns`, `GetNoBuildCost`, `GetNumItemsFromCentralStorage`, `GrabItemsFromInventoryAndCentralStorage`, and `AddStack`/overflow tracking per family. `SmartUpgradePanel::CalculateUpgradeCost` separately computes display costs from hard-coded recipe paths. `PLAN_Satisfactory12_Response.md` lists cost/refund math for 1.2 Game Mode cost multipliers as critical. The 1.2 headers expose `AFGHologram::GetBaseCostMultiplier` overrides for belts, pipes, lifts, wires, and other length/cost-sensitive holograms, plus explicit Creative Mode/NoBuildCost surfaces that should be checked before preserving Smart's current family-local cost math.
- Risk/impact: High for 1.2: any FactoryGame/SML cost API, central-storage, no-build-cost, or recipe multiplier change must be fixed in multiple places. Behavior changes here could also affect vanilla-neutral resource accounting.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During 1.2 triage, verify the authoritative cost/refund/multiplier APIs first, then decide whether to centralize Smart Upgrade cost display and execution accounting behind one helper/service. Avoid refactoring this before 1.2 unless a current bug requires it.

### F039 - Manual child hologram and wire spawn ordering is scattered and 1.2-sensitive

- Area/file: `Source/SmartFoundations/Private/Features/Extend/SFManifoldJSON.cpp`, `Source/SmartFoundations/Private/Features/AutoConnect/SFAutoConnectService.cpp`, `Source/SmartFoundations/Private/Features/PipeAutoConnect/SFPipeAutoConnectManager.cpp`, `Source/SmartFoundations/Private/Features/PowerAutoConnect/SFPowerAutoConnectManager.cpp`, hologram child headers
- Finding summary: Extend and AutoConnect contain many manual `SpawnActor`/`SetBuildClass`/`SetRecipe`/`FinishSpawning`/`AddChild` sequences with comments stating exact ordering requirements; power auto-connect also spawns raw `AFGBuildableWire` actors directly.
- Evidence checked: `rg SpawnActor|FinishSpawning|AddChild|SetBuildClass|SetRecipe` found repeated construction sequences for belts, lifts, pipes, passthroughs, pipe attachments, wall holes, poles, wires, and pipe lanes. Comments say tags must be added before `FinishSpawning`, spline data and mesh generation happen after `FinishSpawning`/`AddChild`, and `SetBuildClass` must happen before `FinishSpawning` to avoid crashes. Multiplayer docs already call direct `SpawnActor` paths an authority audit target. The 1.2 headers expose pending-construction hologram APIs on `AFGBuildableSubsystem` (`SpawnPendingConstructionHologram`, `AddPendingConstructionHologram`, `mPendingConstructionHolograms`), so Smart's manual child/direct actor paths should be checked against that flow during porting.
- Risk/impact: High for 1.2/API migration. Any changed hologram initialization, child construction, wire construction, or `AddChild` semantics can break several hand-maintained order recipes. These paths are live, so this is not cleanup dead code.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During 1.2 compile/runtime triage, build an inventory of each manual construction sequence and verify it against the new FactoryGame/SML hologram APIs. Prefer centralizing the ordering behind helpers only after parity is restored.

### F040 - AutoConnect current-flow docs understate direct power wire construction

- Area/file: `docs/Features/AutoConnect/IMPL_AutoConnect_CurrentFlow.md`, `Source/SmartFoundations/Private/Features/PowerAutoConnect/SFPowerAutoConnectManager.cpp`
- Finding summary: AutoConnect docs say it does not place free-standing built actors outside the normal build commit path, but Power AutoConnect directly spawns `AFGBuildableWire` actors after pole/building construction in at least two live paths.
- Evidence checked: AutoConnect docs state the system is coordinated through active/child holograms and "does not place free-standing built actors outside the normal build commit path." `SFPowerAutoConnectManager.cpp` calls `SpawnActor<AFGBuildableWire>`, `FinishSpawning`, `OnBuildEffectFinished`, `Connect`, `Destroy`, and `DeductCableCost` around lines 1242 and 1461.
- Risk/impact: Medium/high documentation risk for 1.2 and multiplayer. Direct post-build wire actors affect authority, cost charging, and vanilla-neutral accounting, so docs should not imply everything flows through child hologram construction.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Update AutoConnect docs to distinguish belt/pipe child-hologram paths from live Power AutoConnect direct wire spawn paths, and cross-link the 1.2 construction-order audit in F039.

### F041 - ADA documentation workflow depends on absent private-repo prompt

- Area/file: `.windsurf/workflows/update-ada-docs.md`, `.windsurf/workflows/prepare-release.md`
- Finding summary: The optional ADA documentation workflow is still documented as depending on `ada-regeneration-prompt.md` from the old private `Mods\smart` repository, so the workflow is not reproducible from the current SmartFoundations repo state.
- Evidence checked: `update-ada-docs.md` lists `L:\Personal\Repos\SatisfactoryModLoader\Mods\smart\docs\Community\ADA\ada-regeneration-prompt.md` as a prerequisite and instructs agents to use that prompt. `Test-Path` for that file returned false. `prepare-release.md` still points maintainers to `/update-ada-docs` as the optional release follow-up.
- Risk/impact: Low/medium. This does not affect mod runtime, but it can break or confuse release-adjacent documentation updates after the public repo split.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Move the ADA regeneration prompt into the current repo or update the workflow to point to the authoritative prompt/source in the ADA service repo. Keep the actual ADA service update outside this cleanup branch.

### F042 - Trivial specialized hologram subclasses appear unselected by native code

- Area/file: `Source/SmartFoundations/Public/Holograms/Production/SFFactoryHologram_*.h`, `Source/SmartFoundations/Private/Holograms/Production/SFFactoryHologram_*.cpp`, `Source/SmartFoundations/Public/Holograms/Foundations/SFFoundationHologram_Standard.h`, `Source/SmartFoundations/Public/Holograms/{Storage,Power,Transport,Special}/`, `Source/SmartFoundations/Public/Holograms/README.md`
- Finding summary: Many building-specific Smart hologram subclasses are empty shells over the shared base classes and are not selected by the current native spawn/adapter paths, while the README presents them as the active building-specific hierarchy.
- Evidence checked: Targeted `rg` for all production variants, `SFFoundationHologram_Standard`, `SFStorageHologram`, `SFPowerHologram`, `SFTransportHologram`, and `SFSpecialHologram` found only class declarations, `.cpp` self-includes, and `Public/Holograms/README.md` entries. Current spawn paths use shared classes such as `ASFFactoryHologram`, `ASFFoundationHologram`, and `ASFLogisticsHologram`. Binary text scan of `Content/` found no obvious references. SMLMCP asset-registry scan over all 36 Smart assets found no dependencies on `ASFFactoryHologram_Assembler`, `ASFFactoryHologram_Blender`, `ASFFactoryHologram_Constructor`, `ASFFactoryHologram_Converter`, `ASFFactoryHologram_Foundry`, `ASFFactoryHologram_HadronCollider`, `ASFFactoryHologram_Manufacturer`, `ASFFactoryHologram_Packager`, `ASFFactoryHologram_QuantumEncoder`, `ASFFactoryHologram_Refinery`, `ASFFactoryHologram_Smelter`, `ASFFoundationHologram_Standard`, `ASFStorageHologram`, `ASFPowerHologram`, `ASFTransportHologram`, or `ASFSpecialHologram`.
- Risk/impact: Low/medium. These reflected classes now have both native and editor-content evidence against active use, but remove them as one focused cleanup so generated-code fallout is easy to review.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 4 (commit `1a22d61`); deleted all 32 empty subclass files (16 `.h`/`.cpp` pairs) and rewrote `Public/Holograms/README.md` to describe the current adapter-based structure.
- Test/verification note: `git grep` finds no source references to the removed classes outside the rewritten README. SMLMCP re-verification (2026-05-29): all 16 classes exposed, and 0/5 Smart content Blueprints derive from any of them.
- Recommended next action: None; rerun an asset/reference check after the maintainer rebuild to confirm no generated-code fallout.

### F043 - Recipe and unlock checks are duplicated and 1.2 API-sensitive

- Area/file: `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`, `Source/SmartFoundations/Private/Features/Restore/SFRestoreService.cpp`, `Source/SmartFoundations/Private/Services/SFRecipeManagementService.cpp`, `Source/SmartFoundations/Private/UI/SmartSettingsFormWidget.cpp`, `docs/Reference/PLAN_Satisfactory12_Response.md`
- Finding summary: Recipe/buildable availability is checked through several local patterns: hard-coded belt/pipe recipe paths, `IsBuildingAvailable` tier probes, `GetAllAvailableRecipes` name maps, `GetAvailableRecipesForProducer`, and UI-side highest-tier clamping. These are live and should not be cleaned up before the 1.2 recipe/unlock API surface is known.
- Evidence checked: `GetBeltRecipeForTier`/`GetPipeRecipeForTier` load hard-coded recipe paths; `GetHighestUnlockedBeltTier`, `GetHighestUnlockedPipeTier`, `GetHighestUnlockedPowerPoleTier`, `GetHighestUnlockedWallOutletTier`, and `AreCleanPipesUnlocked` probe `AFGRecipeManager::IsBuildingAvailable`; SmartRestore `ValidatePresetUnlocks` rebuilds available-recipe maps by class name and separately validates product buildables; `USFRecipeManagementService::GetFilteredRecipesForCurrentHologram` uses `GetAvailableRecipesForProducer`; the 1.2 response plan already tracks power unlocks, recipe gating, and cost multiplier migration.
- Risk/impact: High for the 1.2 port and vanilla-neutral scope. New unlocks, renamed recipes/buildables, altered recipe manager APIs, or changed availability semantics could make Smart offer unavailable tiers, reject valid presets, or miss new supported buildables.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During 1.2 triage, define one authoritative recipe/buildable availability helper for Smart settings, Restore, AutoConnect, and Upgrade. First verify FactoryGame/SML unlock APIs and new 1.2 content; then consolidate duplicated path/name checks only after parity is restored.

### F044 - Root migration plan still describes pre-publication export work

- Area/file: `MIGRATION_PLAN.md`, `README.md`, `CHANGELOG.md`, `scripts/`
- Finding summary: `MIGRATION_PLAN.md` is still a draft plan for creating the clean source-available/public repository, but the current repo already contains the public/source-available docs and release history. It also references optional helper scripts that are no longer present.
- Evidence checked: `MIGRATION_PLAN.md` says target repo `L:\Personal\Repos\SmartFoundations`, target GitHub repo private until release-ready, and later phases should create `README.md`, `LICENSE.md`, `CONTRIBUTING.md`, `GOVERNANCE.md`, `TRADEMARKS.md`, and `SECURITY.md`. `README.md`, `CHANGELOG.md`, and ficsit.app docs already state the repo is source-available/public. `Test-Path` returned false for `scripts/check_archive.py`, `scripts/create_new_lang_po.py`, and `L:\Personal\Repos\SmartFoundations`.
- Risk/impact: Low/medium. This is not runtime code, but it can mislead future cleanup/publication decisions and duplicates stale local-path/private-repo assumptions during the 1.2 prep window.
- Classification: `doc mismatch`
- Remediation Status: Not started.
- Recommended next action: Either archive `MIGRATION_PLAN.md` as historical publication-planning context or replace it with a short current-state maintenance note. Do not treat its allowlists or script references as authoritative without rechecking current files.

### F045 - Hologram build-result tracking appears write-only

- Area/file: `Source/SmartFoundations/Public/Data/SFHologramData.h`, `Source/SmartFoundations/Public/Data/SFHologramDataRegistry.h`, `Source/SmartFoundations/Private/Data/SFHologramDataRegistry.cpp`, logistics hologram build paths
- Finding summary: `bWasBuilt` and `CreatedActor` are set by belt, lift, and pipe hologram construct paths, but the only readers are `USFHologramDataRegistry::GetBuiltBuildable` and `WasBuilt`, which appear unused.
- Evidence checked: `rg` found `GetBuiltBuildable` and `WasBuilt` only in their declarations/definitions. `bWasBuilt` and `CreatedActor` are written in `SFConveyorBeltHologram.cpp`, `SFConveyorLiftHologram.cpp`, and `SFPipelineHologram.cpp`; no active caller reads those fields outside the unused registry helpers. Binary text scan of `Content/` found no obvious references to the field/helper names.
- Risk/impact: Low if caller checks confirm no plugin/content dependency, but the fields are `UPROPERTY` members inside a reflected struct, so cleanup should still be deliberate.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 3; removed unused registry readers and write-only build-result fields/writes from belt, lift, and pipe hologram construct paths.
- Test/verification note: Static check `rg "GetBuiltBuildable|WasBuilt|bWasBuilt|CreatedActor" Source\SmartFoundations` should return no results. Manual smoke: place belts, lifts, and pipes through normal, Extend, stackable, and auto-connect paths where practical; confirm post-build wiring still registers through active Extend/pipe metadata.
- Recommended next action: Run the listed logistics post-build wiring smoke tests before committing this cleanup batch.

### F046 - 1.2 conveyor-chain subsystem may replace private chain workarounds

- Area/file: `Source/SmartFoundations/Public/Services/SFChainActorService.h`, `Source/SmartFoundations/Private/Services/SFChainActorService.cpp`, `Config/AccessTransformers.ini`, `L:\Personal\Repos\SatisfactoryModLoader-dev-1.2\Source\FactoryGame\Public\FGConveyorChainSubsystem.h`, `FGGameState.h`, `FGBuildableSubsystem.h`
- Finding summary: Smart's chain repair/stabilization service still depends on `AFGBuildableSubsystem` friend access to tick groups and pending chain actors, while the 1.2 headers expose an `AFGConveyorChainSubsystem`, replication component, `AFGGameState::GetConveyorChainSubsystem`, and public chain add/remove/notify APIs.
- Evidence checked: Current Smart code reads `mConveyorTickGroup` and `mConveyorGroupsPendingChainActors`, and calls `RemoveChainActorFromConveyorGroup`, `MigrateConveyorGroupToChainActor`, and `RemoveConveyorChainActor` through `USFChainActorService`; `Config/AccessTransformers.ini` grants `USFChainActorService` friend access to `AFGBuildableSubsystem`. The 1.2 headers still contain those buildable-subsystem APIs, but also expose `AFGConveyorChainSubsystem::Get`, `AddConveyorChain`, `RemoveConveyorChain`, item/segment update notifications, and `AFGGameState::GetConveyorChainSubsystem`.
- Risk/impact: High for the 1.2 port. Preserving private tick-group manipulation may keep Smart on a fragile workaround even if 1.2 has a supported chain subsystem path; switching too early could also regress the hard-won chain-stability fixes if the new APIs do not cover rebuild semantics.
- Classification: `defer to 1.2`
- Remediation Status: Not started.
- Recommended next action: During 1.2 compile/runtime triage, audit `USFChainActorService` against `AFGConveyorChainSubsystem` before porting the current friend-access approach unchanged. Keep the service boundary, but determine which calls can become supported subsystem calls, which private access remains necessary, and which behavior should become an upstream/SML API request.

### F047 - Disabled orphan-bounce queue is inert compatibility ballast

- Area/file: `Source/SmartFoundations/Public/Services/SFChainActorService.h`, `Source/SmartFoundations/Private/Services/SFChainActorService.cpp`, `docs/Features/SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md`
- Finding summary: `PendingBounceQueue`, `BounceTimerHandle`, and `ProcessNextPendingBounce` are retained as a disabled orphan-bounce compatibility path, but the queue is never populated and the callback only clears it.
- Evidence checked: `SFChainActorService.h` labels the queue and timer callback disabled and says the queue is never populated. `rg PendingBounce|BounceTimerHandle|ProcessNextPendingBounce` found only the member declarations and callback implementation. `ProcessNextPendingBounce` resets the queue and clears its timer. Live orphan repair now uses `RepairOrphanedBelts` with `ReRegisterAndQueueVanillaRebuildForBelts`, and Smart Upgrade current-flow docs already list stale bounce queue cleanup as a maintainability item.
- Risk/impact: Low. Removing it later should not affect current repair behavior, but keep the historical crash comments around `RepairOrphanedBelts` so the unsafe bounce approach is not reintroduced.
- Classification: `remove now`
- Remediation Status: Remediated in cleanup batch 1; removed the disabled queue, timer handle, and callback while leaving the active triage repair path unchanged.
- Test/verification note: Static check `rg "PendingBounceQueue|BounceTimerHandle|ProcessNextPendingBounce" Source\SmartFoundations` should return no results. Manual smoke: use Smart Upgrade triage/repair on belts and confirm orphan repair still re-registers through `RepairOrphanedBelts` without reintroducing bounce behavior.
- Validation result: Maintainer build and Smart Upgrade triage smoke tests passed on 2026-05-29.
- Recommended next action: Run the listed Smart Upgrade triage/repair smoke test before committing this cleanup batch, and preserve the "do not restore bounce repair" warning near the active triage repair code.

## Checked False Positives

- Smart Dismantle proxy grouping: `docs/Features/SmartDismantle/IMPL_SmartDismantle_CurrentFlow.md` matches the live `USFSubsystem::OnActorSpawned` path for `AFGBlueprintProxy` creation, `SetBlueprintProxy`, and `RegisterBuildable`. No audit finding unless a later runtime test proves grouping/save-load behavior differs.
- Deprecated `GridCounters` and `CurrentScalingOffset` mirrors: despite comments calling them deprecated, both are still actively synchronized or used by helper/spawner paths. Do not treat these as dead fields without a deliberate compatibility refactor.
- `SFGameInstanceModule_BP` itself is not dead: it is parented to `SFGameInstanceModule`, marked as root module, and provides `SmartConfigClass`. F023 applies only to the inactive widget hook/property.
- Header/source pair mismatches: public header-only types/adapters such as `FSFArrowTypes`, `FSFGridArrayTypes`, `ISFHologramAdapter`, `SFJumpPadAdapter`, `SFRampAdapter`, and `SFUnsupportedAdapter` are either type-only or actively included/instantiated. Private registry shard `.cpp` files without matching headers are included by the registry implementation. No standalone finding unless a future pass proves an unmatched file has no active include/call path.
- Source-inclusion suspects checked as live or already grouped: `USFHintBarService`, `USFGridStateService`, `USFRecipeManagementService`, `FSFInputHandler`, `FSFExtendChainHelper`, `FConduitPreviewHelper`, `FSFSplineAnalyzer`, `FSFLogRegistry`, and `FSFUnsupportedAdapter` all have active construction/call paths or are covered by existing reflected/helper findings. Do not re-add them from low reference counts alone.
- SmartCamera integration surfaces are live external API, not cleanup candidates: sibling mod `SmartCamera` includes `Subsystem/SFSubsystem.h`, depends on the `SmartFoundations` module, subscribes to `OnHologramCreated`/`OnHologramDestroyed`, calls `USFSubsystem::Get`, `TryAcquireHologramLock`, `TryReleaseHologramLock`, and `GetFurthestTopHologramPosition`, and loads Smart's current `IA_Smart_MouseWheel` input asset. Do not remove or rename these without coordinating the companion mod.

## Deferred/Dedup Notes

- Smart Upgrade chain actor findings are grouped under F005 for now; do not add separate duplicates for each UI string unless they describe a different behavior.
- Legacy compatibility fields are grouped by subsystem/widget. Before adding a new “legacy” finding, check whether it belongs under F006 or needs its own area.
- Missing documentation references are grouped under F008; do not add one finding per missing path unless the missing document has distinct migration impact.
- Tracked backup/source-disabled-looking files are not automatically dead. F010 covers true backup files; F011 is the counterexample and should prevent accidental deletion.
- Pure helper/module findings are grouped under F012 unless a later pass proves a specific helper has distinct behavior risk.
- Adapter stub findings are grouped under F014 unless a later pass proves an adapter is actively selected and behavior-critical.
- Hologram subclass cleanup is separate from adapter cleanup: F014 covers selected adapter APIs, while F042 covers empty reflected hologram subclasses that appear unselected by native code.
- Multiplayer/RPC concerns are split: F003 covers RCO validation placeholders; F015 covers server-to-client propagation gaps.
- Input cleanup should stay grouped: F017 covers config/action mismatch and F018 covers the reflected data-asset class.
- AccessTransformer findings should distinguish required private API access from stale transformer mechanisms; F016 covers only the detached C# file.
- Reflected helper classes require content checks before removal. F018 and F019 have now passed SMLMCP asset-registry checks and are `remove now`; apply the same standard before reclassifying other reflected helpers.
- Arrow cleanup is split: F020 covers the legacy DrawDebug module, F021 covers the deprecated reflected visibility mirror, and F008 covers missing arrow doc paths.
- Smart Upgrade UI cleanup is split: F006 covers legacy bound panel widgets, while F022 covers the separate result row widget class/path.
- Smart Upgrade documentation findings are split: F005 covers internal chain triage/repair semantics, while F036 covers public pump upgrade support claims.
- Chain actor findings are split: F005 covers Smart Upgrade docs/UI repair mismatch, F024 covers AccessTransformer support risk, and F025 covers Blueprint construct hook scope/comment drift.
- Conveyor-chain 1.2 findings are split: F024 covers whether the friend-access mechanism still compiles, while F046 covers whether the new/visible chain subsystem can replace or narrow that private workaround.
- Chain-service cleanup is split: F047 covers inert disabled bounce-queue ballast, while F046 covers the 1.2 subsystem/API migration question.
- Pipe/connection cleanup is split: F026 covers the empty pipe-chain placeholder, F027 covers unused conveyor connection helpers, and F028 covers dead manual cost/deferred pipe build code.
- Cost cleanup is split: F028 covers dead Auto-Connect manual charging, F037 covers unused Smart Upgrade batch helpers, and F038 covers live Smart Upgrade cost duplication that should wait for 1.2 cost API triage.
- Build-result tracking cleanup is separate from construction-order risk: F045 covers write-only registry fields/helpers, while F039 covers live manual child hologram/wire spawn ordering.
- Recipe/unlock cleanup is split from cost cleanup: F043 covers availability/tier gating and hard-coded recipe/buildable lookup paths, while F038 covers material accounting.
- Construction-order findings are split: F025 covers Blueprint construct chain rebuild scope, while F039 covers live manual child hologram/wire spawn ordering across Extend and AutoConnect.
- AutoConnect docs findings are split: F040 covers direct power wire spawn docs drift, while F039 covers the underlying migration-risk construction ordering.
- Multiplayer findings are split: F015 covers missing replication, F029 covers the unused network policy helper.
- Player ownership findings are split: F003 covers RCO validation, F015 covers missing replicated propagation, and F035 covers first/local player lookup assumptions outside the RCO path.
- Localization findings should separate config/support mismatches from stale source text; F030 covers unsupported culture folders only.
- Build/config findings are split: F016/F024 cover AccessTransformer mechanisms, F017/F018 cover input config/assets, F030 covers localization folders, and F034 covers Build.cs dependency ballast.
- Workflow/process-doc findings are split: F031 covers release version drift, F032 covers issue-repo drift, F041 covers ADA workflow reproducibility, F044 covers stale root migration/publication planning, and F008 covers generic missing referenced docs.
- Blueprint proxy findings are split: Smart Dismantle proxy grouping is a checked false positive, while F033 covers only the unused subsystem-side flag left after recipe-service extraction.
- TODO/extraction comments are not findings by themselves; add only when current behavior or docs are misleading.
