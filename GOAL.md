# Pre-1.2 Cleanup Audit Goal

Prepare Smart! for the Satisfactory 1.2 migration by auditing likely dead code, orphaned systems, stale docs, duplicated logic, and fragile assumptions. This branch is audit-only: discover broadly, verify carefully, deduplicate often, and leave cleanup/fixes for a later approved pass.

## Hard Rules

- Do not build or package the mod.
- Do not modify SML, FactoryGame, or other repos.
- Do not make code, feature-doc, or behavior fixes unless explicitly approved later.
- Treat Unreal reflection, Blueprint/content references, delegates, timers, config fields, saved data, and AccessTransformers as potentially live until proven otherwise.
- Prefer evidence over guesses. If uncertain, classify as `needs verification`.

## Primary Output

Maintain and curate:

`docs/Audits/Pre1.2CleanupAudit.md`

The audit should not just grow. After each major pass, review for duplicated findings, merge related entries, retire false positives, update classifications, and keep the recommendation summary coherent. Continue discovery loops until at least one full pass produces no new actionable findings.

## Finding Format

Each finding should include:

- Area/file
- Finding summary
- Evidence checked
- Risk/impact
- Classification
- Recommended next action

Classifications:

- `remove now`: clearly unused and low-risk to delete in a later cleanup pass
- `doc mismatch`: docs/comments/workflows do not match current behavior
- `needs verification`: possibly live through reflection/content/config/runtime paths
- `defer to 1.2`: recheck after 1.2 compile/runtime/API triage
- `do not touch`: intentional, live, or too risky to disturb

## Discovery Loop

Run repeated passes. A pass may add findings, refine evidence, merge duplicates, or close false positives. If it finds nothing new, record that clean result.

1. Source inventory: unused helpers, services, adapters, feature stubs, dead branches, duplicate logic, misleading public APIs.
2. Reflection/content pass: `UPROPERTY`, `UFUNCTION`, config, delegates, timers, AccessTransformers, Blueprint/content references where discoverable.
3. Docs pass: `docs/`, `CHANGELOG.md`, `.windsurf/rules/`, workflows, and feature docs describing old behavior, wrong ownership, removed APIs, or outdated limits.
4. 1.2 risk pass: hologram construction, child spawning, direct `SpawnActor`, `Construct()`, lock state, costs, recipes/unlocks, pipe/belt networks, build gun ownership, RCO/authority assumptions.
5. Dependency/build surface pass: `Build.cs`, module includes, reflected classes, content-facing APIs, config structs, and generated-code-sensitive declarations.
6. Dedup/review pass: consolidate duplicates, group related findings, trim stale wording, reprioritize, and make false-positive notes explicit.
7. Recommendation pass: separate safe 1.1 cleanup candidates from items that should wait for Satisfactory 1.2 headers, compile triage, or runtime testing.

## Definition of Done

- Audit document exists and is organized by classification/area.
- Multiple discovery passes have been run and logged.
- Duplicate or stale entries have been consolidated during the session.
- At least one full review pass finds no new actionable findings.
- Uncertain items include verification steps.
- 1.1-safe cleanup candidates are separated from 1.2-deferred items.
- Only audit materials changed unless the maintainer explicitly expands scope.
