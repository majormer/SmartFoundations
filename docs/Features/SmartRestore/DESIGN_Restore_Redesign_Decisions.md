---
title: Smart Restore Redesign — Locked Decisions
type: DESIGN
date: 2026-07-03
status: Active (design; implementation not started)
category: Features
tags: [smart_restore, presets, modules, extend, tabs, migration, ui_widgets, "issue_427"]
related: [../../../CHANGELOG.md, ./IMPL_SmartRestore_CurrentFlow.md]
---

# Smart Restore Redesign — Locked Decisions

Running record of design decisions for the Smart Restore redesign (GitHub #427), made in the
2026-07-03 design session. **Implementation has NOT started** — this is the decision log so nothing
is lost. Supersedes the terse recommendations in the #427 body where they conflict.

## 1. Structural decision — two decoupled TABS, one discriminated model

The Extend variant and the settings-preset variant are **decoupled into two tabs on the same
Smart Restore widget** (stronger than the #427-body "segmented filter"). They have opposite
authoring flows, which is the real justification:
- **Grid Preset** = a snapshot of panel *values*, created *from the panel*, reusable anywhere.
- **Module** = a captured *topology* (a manifold: factory + connections up to distributors/
  junctions), created *from a live Smart Extend*, replays one specific layout.

**Data model: ONE discriminated struct, not two.** Keep a single `FSFRestorePreset` with an
`ESFRestorePresetKind { Standard/GridPreset, Extend/Module }` discriminant + the `NormalizeKind()`
chokepoint (derived from topology presence on every load/capture/import). Tabs are a UI-layer
filter over `Kind`. Rationale: one import/export format, simple migration, a future third kind is
just an enum value + tab.

## 2. Naming (working; open to final confirmation)

- **Extend variant → "Modules."** Rides the community's existing "modular factory" vocabulary;
  self-explanatory to new users; the Extend tutorial video already anchors the term. **Do NOT put
  "Based on Extend" in the name** (that's provenance, not identity; leaks jargon). Surface the
  Extend link in the *capture button* ("Capture from Last Extend") and the *empty state* ("No
  Modules yet — build a Smart Extend, then capture it here"). **Avoid "Blueprint"** (collides with
  vanilla Blueprint Designer).
- **Settings variant → "Grid Presets"** (short: "Presets"). Alternative "Grid Patterns" considered,
  not chosen. Avoid "Layout" (overloads with Module).
- The deliberate non-parallelism ("Grid Presets" = a format vs "Modules" = a thing) reinforces the
  split. Final naming confirmation still pending.

## 3. Grid Preset tab — LOCKED functional model

The key finding: **restore is already implemented** — `PopulateSmartPanelFromPreset` ->
`BuildPendingCounterStateFromPreset` -> `PopulateCounterInputsFromState` already does "build a
counter-state from the preset, push to panel inputs, refresh." The redesign is mostly cleanup +
decoupling + migration, NOT net-new logic.

- **Full snapshot** (decided) — a Grid Preset captures the *entire* panel state, not a selective
  subset. Selective capture (the current 7-checkbox `CaptureFlags`) is demoted to a possible
  *advanced* option, not the default UI. New v3 presets have all `CaptureFlags` effectively true;
  flags only still matter to honor *old* partial presets on restore.
- **Scope of the snapshot** (decided) — grid counters + transforms (spacing/steps/stagger/rotation)
  + the axis/mode selectors + the **build-gun recipe** + the **production recipe** + auto-connect
  settings. Full panel snapshot.
- **Two recipes, distinct roles** (decided):
  - **Build-gun recipe** = which building. Restore switches the build gun to the SAME building,
    then applies the settings (e.g. a foundation preset switches to that foundation and applies its
    grid/transforms).
  - **Production recipe** = what a machine crafts. **"No recipe" must be a first-class, restorable
    value** (foundations have none; a machine may have none selected). Encode it EXPLICITLY —
    recommended a `"none"` token (fits the SR2 string-token philosophy), NOT empty-string (today
    empty conflates "deliberately none" with "don't touch"). Restore *actively clears* the
    production recipe on "none" instead of skipping. (Token vs bool is the one small
    representation detail still to finalize.)
- **Capture = flush-then-capture** (decided) — fixes the stale-capture bug. Today
  `CaptureCurrentState` reads committed `GetCounterState()`, which lags the on-screen (apply-on-
  commit) panel. Flush pending panel edits to counter-state first (`ApplyCurrentValues()` / a new
  non-applying `CommitPanelToCounterState()`), then snapshot. "Capture what we see."
- **Restore = populate panel, then honor `bApplyImmediately`** (decided) — restore writes the
  values into the panel and runs the SAME path a manual edit runs. `bApplyImmediately` already
  exists (config: "Apply Smart Panel changes instantly instead of clicking Apply", default off). If
  ON -> the grid refreshes like pressing Apply; if OFF -> values stage for fine-tuning. Zero
  special apply-logic; a restore is indistinguishable from typing the values.
- **Restore is an EXPLICIT action, not auto-load-on-select** — deleting the auto-load
  (`SmartSettingsFormWidget_Presets.cpp:319`) kills the "did selecting already apply it?" ambiguity.
- **Persistence** — because restore == set panel, a restored Grid Preset is subject to the #371
  one-shot (reverts on next hologram), exactly like a manual edit. Session-default persistence is a
  SEPARATE opt-in (see Open Decisions / Q1); it is a Grid-Preset-only concept (Modules are
  inherently one-shot replay).

## 4. Migration & versioning — decision A (accepted)

Adding the axis/mode fields is a schema change: bump `SF_RESTORE_PRESET_VERSION` 2 -> 3.

- **Old presets are NOT invalidated.** Build a `MigratePresetJson(v2->v3)` chain (does NOT exist
  today — net-new). Derive-on-load + **rewrite-lazily** (write back as v3 only on next save/update);
  NO bulk migration pass.
- **CRITICAL: missing fields default to LEGACY-IMPLICIT values, not struct-zero.** Especially
  `RotationAxis` (#419/#372): a pre-field v2 preset must default to the pre-#372 implicit axis (X),
  or old rotation-fan presets restore wrong.
- **SR1 codes readable forever** — keep the legacy read path (decode -> migrate up -> restore).
- **Known gap (accepted, not a regression):** new v3 codes will NOT import on not-yet-updated v2
  clients — today `JsonToPreset` hard-rejects `Version > current` (SFRestoreService.cpp:1152).
  Cross-version sharing already dead-ends on mismatch; the bump just adds a trigger. **SR2 (Phase 3)**
  is the real fix (a self-describing envelope with `minReaderVersion` so additive changes stay
  loadable by older readers). Most users update together via the mod manager, so usually invisible.
- **Do the cheap import-robustness fixes now** (free, support graceful handling): `TrimStartAndEnd`
  on import; flip the capture-flags parse from `GetBoolField` -> `TryGetBoolField` (the one field
  that currently does not degrade gracefully).

## 5. Open decisions (still needed)

- **Q1 — #371 session-default persistence (PIVOTAL).** Add an opt-in "keep these settings this
  session" that writes a Grid Preset's values into global `FSmart_ConfigStruct` (crosses the
  deliberate #371 one-shot boundary)? Recommendation: yes, but explicit + HUD-visible + one-click
  revert (precedent: #454 routing-mode HUD persistence). Without it, the redesign cleans the UI but
  does not *fix* the "presets evaporate after one build" complaint. Standard/Grid-Preset-tab only.
- **Q8 — dual draggable panels.** Merge the separate Restore side panel into the main Smart Panel,
  or keep it separate? Decide before the BP pass (each `Smart_Settings_Form_Widget` edit is a
  hazard event).
- **Final name confirmation** ("Grid Presets" / "Modules").
- **"No recipe" encoding** — `"none"` token (recommended) vs a `bRecipeSelected` bool.
- **The entire Module tab** — not yet designed; the majority of remaining work per the maintainer.

## 6. Phasing (from #427, updated)

- The tab restructure is essentially #427 Phase 1 + Phase 2 combined. Because tabs are a big
  `Smart_Settings_Form_Widget` change and BP edits are hazardous (nulls section subobjects, cooked-
  size gate), **do the tab layout + Phase-1 cleanups + `ESFRestorePresetKind` in ONE BP pass**
  (this is #427 Q3, recommended).
- **SR2 serialization envelope stays Phase 3** (separable). Reader-first rollout + legacy-SR1-export
  option during adoption (#427 Q2, recommended).
- Resolved-by-tab-split: #427 Q6 (Extend capture checklist) dissolves — the Module tab has no
  panel-capture checklist.

## 7. Status of the #427 open questions

| Q | Status |
|---|--------|
| Q1 #371 persistence | OPEN — pivotal; rec: opt-in, explicit, Grid-Preset-only |
| Q2 SR2 reader-first | Rec: yes, Phase 3 |
| Q3 fold Phase 1+2 BP pass | Rec: yes (reinforced by tabs) |
| Q4 compression | Rec: uncompressed default |
| Q5 lossless axis/mode | DECIDED: include (drives the v3 bump) |
| Q6 Extend checklist | DISSOLVED by tab split |
| Q7 list form factor | Rec: ComboBox + inline marker for now |
| Q8 dual panels | OPEN — decide before BP pass |
