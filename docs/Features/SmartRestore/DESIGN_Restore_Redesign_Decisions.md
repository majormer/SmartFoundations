---
title: Smart Restore Redesign — Locked Decisions
type: DESIGN
date: 2026-07-03
status: Design COMPLETE; core implementation LANDED 2026-07-03 (see section 11) - in-game validation pending
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

## 2. Naming — LOCKED (2026-07-03)

Final terms. These will be taught in the Smart Restore tutorial video (#446), so they are now
user-facing-committed — keep them stable.

- **Extend variant → "Modules."** Rides the community's existing "modular factory" vocabulary;
  self-explanatory to new users; the Extend tutorial video already anchors the term. **Do NOT put
  "Based on Extend" in the name** (that's provenance, not identity; leaks jargon). Surface the
  Extend link in the *capture button* ("Capture from Last Extend") and the *empty state* ("No
  Modules yet — build a Smart Extend, then capture it here"). **Avoid "Blueprint"** (collides with
  vanilla Blueprint Designer).
- **Settings variant → "Grid Presets"** (short: "Presets"). Alternative "Grid Patterns" considered,
  not chosen. Avoid "Layout" (overloads with Module).
- The deliberate non-parallelism ("Grid Presets" = a format vs "Modules" = a thing) reinforces the
  split.

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
    value** (foundations have none; a machine may have none selected). **DECIDED (2026-07-03): reuse
    the EXISTING mechanism, don't invent a token.** The codebase already has first-class "no recipe"
    support: `USFRecipeManagementService::HasStoredProductionRecipe()`/`GetStoredProductionRecipe()`
    (the has/hasn't flag), a **Clear Recipe button** in the Smart Panel
    (`OnClearRecipeButtonClicked`, SmartSettingsFormWidget.cpp:300) that already shows "No recipe
    selected", and `SetRecipe(nullptr)` (SFRecipeManagementService.cpp:1360) as the clear path. So
    the preset mirrors the `bHasStoredProductionRecipe`-style flag (a has-recipe bool + the recipe
    field); restore with "no recipe" calls the existing clear-recipe path instead of skipping.
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
- **Persistence — DECIDED (2026-07-03): DEFER stickiness; mitigate with FAST RELOAD.** No
  session-default / global-config persistence — a restored Grid Preset stays subject to the #371
  one-shot (reverts on next hologram), exactly like a manual edit. We do NOT cross the #371 config
  boundary. The "presets evaporate after one build" complaint is answered instead by making
  re-applying a preset *quick and low-friction* (list visible, one click to re-apply, honors
  `bApplyImmediately`). **Design requirement that follows: reload must be genuinely fast** — the
  explicit-restore + no-auto-load design already supports this; keep the reload path one action.
  (Q1 resolved this way; persistence can be revisited later if quick-reload proves insufficient.)

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

**Grid Preset tab is fully locked; naming locked; Q8 resolved.** Remaining open work is the Module
tab design.

- **Q8 — dual draggable panels: RESOLVED (2026-07-03) → Option B, DOCK.** Do NOT fully merge into
  the main panel (would overstuff the already-dense panel + bigger restructure) and do NOT keep two
  independent floaters (the "juggling two panels" friction is likely the core of the "convoluted"
  complaint). Instead **dock the Restore panel to the main Smart Panel so the two move as ONE
  draggable unit**, with Restore as its own clearly-delineated tabbed region within that unit.
  Removes the independent `RestoreSidePanel` drag (`bIsDraggingRestorePanel`,
  SmartSettingsFormWidget_Presets.cpp) in favor of a single draggable container.
- **The entire Module tab — STILL OPEN.** Not yet designed; the majority of remaining #427 design
  work. **Design this BEFORE the BP pass (see §6 sequencing).**

## 6. Phasing + implementation sequencing

- The tab restructure is essentially #427 Phase 1 + Phase 2 combined. Because reshaping
  `Smart_Settings_Form_Widget` is hazardous and delicate, **do the docked layout + both tabs +
  Phase-1 cleanups + `ESFRestorePresetKind` in ONE BP pass** (#427 Q3).
- **CRITICAL SEQUENCING:** the single BP pass builds BOTH tabs and the docked container, so it must
  not start until the **Module tab is designed** (§5). Design Module → then one BP pass covers Grid
  Presets tab + Module tab + dock + Phase-1 cleanups. Starting the BP pass with only Grid Presets
  designed would force a second BP edit for the Module tab later — exactly the hazard we're avoiding.
- **Implementation ownership:** the widget/BP restructure is **editor Blueprint work — delicate**, to
  be driven by a stronger model (maintainer will provide one). Favor as few BP edits as
  possible. The C++ side (capture/restore/migration/`NormalizeKind()`/service logic) is separable
  from the BP work and can proceed independently.
- **SR2 serialization envelope stays Phase 3** (separable). Reader-first rollout + legacy-SR1-export
  option during adoption (#427 Q2, recommended).
- Resolved-by-tab-split: #427 Q6 (Extend capture checklist) dissolves — the Module tab has no
  panel-capture checklist.

## 7. Status of the #427 open questions

| Q | Status |
|---|--------|
| Q1 #371 persistence | RESOLVED — DEFER stickiness; mitigate with fast preset reload (no config-layer crossing) |
| Q2 SR2 reader-first | Rec: yes, Phase 3 |
| Q3 fold Phase 1+2 BP pass | Rec: yes (reinforced by tabs) |
| Q4 compression | Rec: uncompressed default |
| Q5 lossless axis/mode | DECIDED: include (drives the v3 bump) |
| Q6 Extend checklist | DISSOLVED by tab split |
| Q7 list form factor | Rec: ComboBox + inline marker for now |
| Q8 dual panels | RESOLVED — Option B: dock into one draggable unit (Restore = tabbed region within it) |

## 8. Module tab — Step 1 grounding (2026-07-03)

**HEADLINE: the entire Module capture -> replay -> MP pipeline already ships and works.** Like Grid
Presets (restore was already built), the Module tab is a **UI reorganization of existing machinery**,
NOT a mechanic build. The only genuinely-new design is the **session UX** (see below).

- **Data model** (`FSFCloneTopology`, SFExtendCloneTopology.h:410): `ParentBuildClass` (source
  factory type), `ParentTransform`, `WorldOffset`, `SourceFactoryId`, and `ChildHolograms[]` — each
  `FSFCloneHologram` carries Role ("distributor"/"belt_segment"/"pipe_segment"/"pipe_junction"/
  "lift_segment"), class, build class, transform, chain. Complete; has `SpawnChildHolograms()`.
- **Capture** (`GetLastCloneTopology` / `IsLastExtendAvailable` / `ImportFromLastExtend`): grabs the
  last Smart Extend's topology. Availability gate = valid source topology + source building + >=1
  child, AND NOT while a restored topology is already active. Complete.
- **Replay = SEED-RELATIVE STAMP + build-class match** (`ReplayRestoreCloneTopology` /
  `IsHologramCompatibleWithRestoredCloneTopology`): Apply enters a SESSION
  (`bRestoredCloneTopologyActive` + `RestoredCloneTopologyTemplate`). While active, holding a
  building whose class == the topology's `ParentBuildClass` previews the WHOLE manifold relative to
  the held building; placing it stamps the manifold. Complete.
- **Session lifecycle = REPEATABLE, persistent** (NOT one-shot): clears only on parent invalid,
  build-class mismatch (switch buildings), a normal Extend activation, or explicit clear
  (`ClearRestoredCloneTopologySession`). So you can stamp the same Module multiple times by holding
  the matching building until you switch away.
- **RESCALABLE at restore (corrected 2026-07-03 — earlier "frozen snapshot" note was WRONG):**
  a restored Module is NOT stuck at the saved size. `BuildRestoredCloneTopologyForCurrentState`
  reads the CURRENT grid counters (`State.GridCounters.X/Y`) and re-lays the clones via
  `CalculateRestoredScaledClonePlacement(... State, X, Y)`; `OnRestoredCloneTopologyStateChanged`
  rebuilds on scale change. So restore re-enters a scalable Extend session seeded from the saved
  UNIT — you scale X/Y to any size. **Module = a captured, RESCALABLE Extend unit, not a frozen
  layout.** (The Grid-Preset-vs-Module split still holds: Grid Preset = parametric settings for a
  grid of *buildings*; Module = a captured wired-*manifold* unit you re-scale.)
- **MP: handled** — server-derived topology, `ReceiveServerCloneTopology`,
  `SetStoredCloneTopologyForServerCommit`, `StageCommitClearForMP`, server/restored/stored/last
  fallback chain in `GetLastCloneTopology`. Replay goes through the Extend spec/commit machinery.

**What's NET-NEW (the actual Module design work):** the **session UX** — the stamp session is
currently INVISIBLE. Applying a Module silently enters the session with no on-screen indicator of
(a) that a Module is active, (b) which building to hold to stamp it (`ParentBuildClass`), (c) that
it is repeatable, (d) how to exit/clear. That is the one real design conversation; everything else
is tab layout + a parts-summary details pane + confirm-existing.

### 8b. Module tab — replay-session model (DECIDED 2026-07-03)

Applying a Module drops you into an invisible stamping mode. Surface it, and fix the "which
building?" problem at the root.

- **Apply auto-equips the source building (DECIDED: YES).** On Apply, switch the build gun to the
  Module's source building (`ParentBuildClass`) so the manifold previews IMMEDIATELY — no "guess
  what to hold." Fallback when that building isn't unlocked: the HUD shows a "Hold a [Constructor]"
  prompt.
- **Indicator = the EXISTING scaling HUD + one banner line.** Once the source building is equipped, a
  restored-Module session behaves like a normal Smart scale/extend (hold building, scroll to size,
  click to build) — so reuse that HUD and add a `Module: "<name>"` banner. Minimal new UI, maximal
  consistency. State-dependent copy: ready (`Module "<name>" — scroll to scale (5x3), click to
  stamp`) vs prompt (`Module "<name>" — hold a Constructor to place it`).
- **The session is REPEATABLE + RESCALABLE while active** (already true): stamp as many as you want;
  scroll scales X/Y (§8 rescalable correction). The HUD reflects the live size like normal scaling.
- **Exit = the EXISTING familiar Smart patterns, NOT a bespoke Done button** (DECIDED): fire to
  build/stamp (current Restore already builds on fire), switch recipe / switch building, or leave
  build-gun-build mode (holster) — exactly how scaling and every other Smart mode exit. Consistency
  over a new affordance. The HUD hints at these like other modes' HUDs do. (These already clear the
  session in code: build-class mismatch, normal Extend activation, etc.)

### 8a. Module tab — capture model (DECIDED 2026-07-03)

The "last Extend" is today an INVISIBLE, transient, in-memory clipboard — the user can't tell if
they have one, what it holds, or how long it lasts, and it conflates the *save source* with the
separate *load source* (the saved-Modules library). Fix = make it a visible staging slot.

- **Two clearly-separated zones on the Module tab:** (1) TOP = a visible **"Extend clipboard"
  staging slot** (the transient capture buffer, made visible, with a `Save as Module` action);
  (2) BELOW = the saved-**Modules library** (the permanent list you Apply). This makes save-vs-load
  unambiguous: you only save-from the top slot, only load/apply-from the list.
- **A live Extend PREVIEW counts, not just a committed build** (decided) — the user must NOT have
  to build an Extend for it to be captureable. The slot is fed by a current preview OR the last-
  built Extend. **IMPLEMENTATION VERIFY:** topology is cached at the preview/hologram-service layer
  (`SFExtendHologramService` StoredCloneTopology), so this may be close already, but
  `IsLastExtendAvailable` currently gates on the last *built* source topology
  (`Topology.bIsValid && SourceBuilding.IsValid()`); ensure it accepts a live-preview state.
- **The slot shows a described summary of its content** (liked) — grouped by role from
  `ChildHolograms`: e.g. "Constructor manifold — 1 splitter, 6 belts, 2 pipes." Empty state: "none
  yet — preview or build a Smart Extend and it'll appear here, ready to save."
- **Naming nuance:** since a live preview counts, "Last Extend" is slightly off (it may be the
  CURRENT one). Use a neutral header ("Ready to save" / "Extend clipboard") — final wording TBD.
- **Mental model this creates:** Extend clipboard = volatile scratch (in-memory, lost on restart);
  Module = permanent save. "Save it as a Module to keep it" becomes self-evident.
- **HUD nudge (IN):** a one-time post-Extend hint — "Extend ready — save it in Smart Restore →
  Modules" — for discoverability when the panel isn't open.
- **Lifecycle to honor:** populate on preview/build; replace on a new Extend; unavailable while a
  Module replay session is active; session-only (cleared on restart).
- **Capture point = ONE clipboard, latest-wins (DECIDED 2026-07-03).** NOT three separate slots.
  The single "most recent Extend" buffer is fed by all three sources: (a) a live preview — while
  previewing, the clipboard IS the live preview, so "Save as Module" just saves the current buffer
  (no separate "save current preview" action); (b) a cancelled preview — escaping the build gun must
  NOT clear the buffer (the topology is already cached in-memory, so mostly "don't clear on cancel"
  + surface it); (c) a committed build — refreshes the buffer to the built config. Rejected: three
  spots (forces "which am I saving?"). **Sub-decision RESOLVED (2026-07-03): LATEST-WINS** — a new
  preview immediately overwrites a retained un-saved clipboard; mitigation = promote-to-Module before
  extending again. The clipboard is explicitly transient.
- **Bonus from the rescalable correction:** because restore re-scales, capturing at just **1 clone**
  yields a fully useful Module — no need to scale to final size before saving. Lowers the capture bar.

### 8c. Module details pane — Step 4 (DECIDED 2026-07-03)

Read-only details for a selected Module (same read-only-pane pattern as Grid Presets):
- **Source building** — friendly name of `ParentBuildClass` (e.g. "Constructor"): what gets
  auto-equipped on Apply / what you hold.
- **Per-UNIT composition** — role-grouped counts from `ChildHolograms` for ONE clone (belts / pipes /
  junctions / distributors / lifts / poles), e.g. "a Constructor with 6 belts, 2 pipes." Per-unit,
  NOT a fixed total, because the total scales on restore.
- **NO fixed cost shown** — cost is size-dependent (rescalable), so it's meaningless as a static
  number. Cost shows LIVE in the normal build/scaling HUD while you stamp (existing mechanism).
- **Availability/unlock status** — reuse `ValidatePresetUnlocks`; if the source building or contents
  aren't unlocked, show "Requires <X>" and DISABLE Apply.
- **Description + Created/Updated** — read-only, like Grid Presets.

## 9. Cross-cutting — Step 5 (all CONFIRM-EXISTING, DECIDED 2026-07-03)

Nothing net-new here; the redesign must preserve these existing behaviors:
- **MP replay** — already handled (server topology, `StageCommitClearForMP`, spec/commit path).
  Decision: rely on it as-is; **regression-test that the tab restructure doesn't break MP replay**.
- **Cost/charging** — a Module stamps real buildables; vanilla cost aggregates live as you stamp
  (existing extend/scaling cost path). No special Module cost handling.
- **Unlock validation** — reuse `ValidatePresetUnlocks` to gate Apply (feeds §8c's availability
  status).
- **Import/export** — works today via the existing `SR1` format for BOTH kinds. Module codes carry
  the topology blob and are the LARGE ones — so they are exactly the case that motivates SR2's
  optional compression (Q4). Decision: keep existing export through Phase 1/2; **SR2 + compression
  (Phase 3) is where large Module codes get addressed** (reaffirms Q4 = uncompressed default, add
  compression only if Module codes prove too long to paste).
- **Empty states** — Modules library: "No Modules yet — build a Smart Extend and save it here."

## 10. Unified widget layout — Step 6 (DECIDED 2026-07-03)

One draggable docked unit (Q8) = the existing Smart Panel + a docked Restore region. The Restore
region is a tabbed panel; **default landing tab = Grid Presets** (more common).

- **Tab bar:** `[ Grid Presets ] [ Modules ]`. The tab itself conveys kind, so the per-row type
  marker (#427 Q7) is now redundant — keep the ComboBox list, drop the inline marker.
- **Grid Presets tab:** ComboBox list -> read-only details pane -> actions (`Load to Panel`,
  `Apply & Build`, `Export Code`) -> `Save from Panel` create card (name + description; full
  snapshot) -> `Import Code`.
- **Modules tab:** "Extend clipboard" staging slot at TOP (live-described + `Save as Module`) ->
  saved-Modules library list BELOW -> read-only details pane (§8c) -> actions (`Apply`, `Export
  Code`) -> `Import Code` -> library empty state.

This section + §1-§9 IS the consolidated spec the single BP pass builds from. #427 design is
now complete pending only implementation (BP pass after this spec + the separable C++ side).

## 11. Implementation record (2026-07-03, Fable session — compiled clean, validation pending)

**ZERO Blueprint edits were needed.** Instead of the planned BP pass, the tabbed Restore UI
is built PROGRAMMATICALLY at construct time (`BuildRestoreTabUI`,
`SmartSettingsFormWidget_RestoreTabs.cpp` — the same C++-built-UI pattern as the Walk panel):
the BP's `RestoreSidePanel` border is the single designer dependency; its content is replaced with
the two-tab tree and the still-relevant bound widgets (dropdown, name/description inputs, action
buttons) are REPARENTED into it. The hazardous `Smart_Settings_Form_Widget` asset is untouched.

What landed (all compiled, client + server Shipping):
- **Data model v3** (`SFRestoreTypes.h`): `ESFRestorePresetKind` + `NormalizeKind()` chokepoint;
  axis/mode selector fields; explicit `bHasProductionRecipe`; versioning-rules doc-comment.
- **Migration** (`SFRestoreService.cpp`): v1/v2→v3 in-memory on every load/import, rewrite-lazily;
  legacy-implicit defaults (RotationAxis=X); recipe-less v2 presets keep legacy skip semantics
  (bRecipe forced off). New v3 enum fields serialize as append-only STRING TOKENS.
- **Import robustness**: `TrimStartAndEnd`; capture-flags `GetBoolField`→`TryGetBoolField`.
- **Capture**: `CapturePanelState` (flush-then-capture — Save records on-screen values, side-effect
  free via the widget's new `ReadPanelCounterState`); full-snapshot flags; "No recipe" captured
  explicitly.
- **Apply**: `ApplyPreset(Preset, bIncludeCounterState)`; Grid tab passes false → restore = SET THE
  PANEL, honoring `bApplyImmediately` (on → commits like pressing Apply incl. large-grid confirm;
  off → staged for fine-tuning). Explicit "No recipe" actively clears via the Clear-Recipe path.
  Module apply keeps counters (seeds the rescalable session) and already auto-equips (the existing
  build-gun switch IS the auto-equip).
- **Widget**: two tabs over one `UWidgetSwitcher`; kind-filtered lists; read-only details panes
  (building/grid/transforms/recipe incl. "No recipe"/description/created + "Requires X" unlock
  status gating Apply); visible **Extend clipboard** staging slot with live role-grouped summary
  (0.75s refresh timer) + reasoned empty/replay-active states; "Save as Module" = capture+save
  only (NO apply; unit semantics 1x1x1); Module Apply = heavy confirm → full apply → panel closes
  into the stamp session; import routes to the right tab by kind; Update gained its confirm
  dialog; auto-load-on-select REMOVED (explicit "Load to Panel"); the overloaded description box
  fixed (authoring input never clobbered by selection); Q8 dock (one draggable unit, independent
  Restore drag removed).
- **HUD banner** (`SFHudService.cpp`): `*Module "<name>": scroll to scale, fire to stamp
  (repeatable)` vs `equip its source building to place it` (build-class compatibility check).

**Deferred (deliberate):** the one-time post-Extend HUD nudge (needs a capture-timestamp signal
through the Extend service; the visible clipboard slot carries discoverability). Localization:
all new strings are code LOCTEXT — English until the next gather; relabeled BP buttons
(Apply & Build / Save from Panel / Export Code / Import Code / Save as Module) use new keys.

### 11a. Feedback round 1 (2026-07-03 evening, maintainer's first in-game pass)

Validated working end-to-end: clipboard populated from a Refinery extend, Save as Module, library,
Apply -> stamp session with HUD banner + grid readout, manifolds placed correctly. Fixes applied
from the feedback:
1. **Dock was far off to the side** - docking used the canvas SLOT size (designer size, much wider
   than the ScaleBox'd rendered panel). Now docks to the RENDERED right edge via cached geometry
   (+ next-tick re-dock after first layout).
2. **Grey-on-grey buttons** - all Restore buttons (new + reparented BP ones) now use a dark
   Smart-Panel-aligned FButtonStyle with LIGHT labels; active tab = orange accent with near-black
   label; UpdateRestoreButtonTextColors flipped from black to light (black-on-dark was invisible)
   and re-runs on clipboard enable-flips.
3. **"Apply & Build" retired** (maintainer: the open panel blocks hologram interaction, so
   apply-while-open is meaningless). **Load to Panel is THE load action**: build-gun switch +
   recipe + auto-connect + staged panel values, honoring Apply Immediately; availability gates it;
   success/failure is acknowledged in the details pane.
4. **Silent empty-name save fixed** ("FoundationFun") - hint texts on the name/description inputs,
   an explicit "enter a name" message on empty, and a "Saved 'X'." acknowledgement + auto-select
   on success. (Diagnosed from disk: the file was never written; the name field was empty -
   exactly #427's documented silent-no-op complaint.)
5. **"Smelters" mystery solved - NOT a migration bug**: the June-2 preset is an empty husk
   (buildingClassName "", no recipe key, all-default values) - there was never anything to
   restore. Details pane now says "none captured" instead of "Unknown"; migration correctly
   preserves legacy skip semantics for its empty recipe.
6. **KNOWN ISSUE (follow-up, pre-existing):** belt/lane-segment previews don't render their spline
   meshes in the Module replay preview (pipes do; everything PLACES correctly). The replay spawn
   path (FSFCloneTopology::SpawnChildHolograms belt branch) predates #427 and was untouched;
   needs its own investigation - likely the same class of spline-mesh-generation gap the Extend
   preview solved separately. Also NOTE: SF_RESTORE_DIAGNOSTIC_LOG is invisible in Shipping logs.

**Validation checklist (maintainer):** panel opens with two tabs docked right of the main panel;
grid-preset save/load/apply round-trip incl. Apply-Immediately on/off; "No recipe" preset clears
recipe on apply; v2 presets on disk load + apply unchanged (esp. rotation-axis fans); SR1 codes
import; extend preview → clipboard populates live → Save as Module → Apply → confirm → stamp
session with HUD banner + rescale; module list/export/import/delete; drag = one unit; Escape/
close-reopen behavior; MP smoke (module apply on client).

### 11b. Feedback round 2 (2026-07-03 night)

1. **Backdrop didn't cover the full panel** - the BP border only PAINTS its canvas-slot rect
   (fixed designer height); our programmatic content is taller, so the lower half (save inputs,
   Save from Panel, Export/Import) floated against the world. Fix: the slot is now auto-sized so
   the border's brush tracks the content height, with a SizeBox pinning the designer WIDTH
   (auto-size would otherwise collapse to the children's desired width).
2. **Belts invisible in Module replay preview - ROOT CAUSE FOUND (live in-game diagnosis).**
   NOT at world origin: connector-belt holograms (JsonBelt_*) sat at CORRECT world positions with
   correct rotations, but their live mSplineData was 2 all-zero points (vanilla's "belt awaiting
   first click" state) and no SplineMeshComponents existed. The saved module JSON was VALID (4
   real local points, real tangents), lanes rendered (re-synthesized per scale), pipes rendered
   (stored data applied verbatim). Divergence: `ASFPipelineHologram::SetHologramLocationAndRotation`
   skips vanilla for `SF_ExtendChild` tags; the belt override called `Super::` unconditionally
   ("PHASE 2" comment). The Module replay seeds children under the build gun's RAW VANILLA factory
   hologram, whose per-tick propagation (and the replay's own synthetic-hit kick) reaches children
   with the PARENT's hit result - vanilla belt code then regenerates mSplineData from that hit,
   wiping the authored route. The normal Extend preview never hit this because ASFFactoryHologram
   blocks propagation while Extend is active; belts were the ONLY spline child class without the
   guard (same defect family as #418 vanilla-parent propagation). Fix: mirror the pipe's tag guard
   in `ASFConveyorBeltHologram::SetHologramLocationAndRotation`.
3. **Shipping-log ground rule (maintainer):** temp debugging MUST use Log/Warning verbosity
   (the SF_*_DIAGNOSTIC_LOG macros hardcode Verbose - invisible in Shipping), and ALL temp logs
   must be cleared before release. Temp logs were tagged `[SF-TEMP-427]`
   (SFExtendRestoreReplayService.cpp replay-apply + SFConveyorBeltHologram.cpp wipe-restore).
   **DONE (2026-07-03 night):** the belt fix's validation logging is confirmed no longer needed
   (maintainer confirmed belts render); both raw `UE_LOG(..., Log, ...)` calls removed - the
   replay-apply log deleted outright (its unused-after-removal `Components/SplineComponent.h`
   include also dropped), the wipe-restore log converted to `SF_EXTEND_DIAGNOSTIC_LOG` (Verbose,
   matches the surrounding diagnostic style, stays for future debugging without a Shipping-log
   violation). Verified via `git diff` that no other non-Verbose `UE_LOG` lines were introduced
   by this feature.

**Round-2 belt fix CORRECTED by the temp log:** the retest still showed zero splines, but the
`[SF-TEMP-427]` log proved the replay APPLIED valid 1599cm belts - so the wipe happens AFTER
apply, and the round-2 `SetHologramLocationAndRotation` guard was the wrong lever. The vanilla
parent's per-tick cascade is `PostHologramPlacement`; the belt's once-gate deliberately lets ONE
vanilla call through "for connection wiring" - under a vanilla parent that call arrives during
PREVIEW with the parent's hit result and vanilla resets the belt to the unstarted 2-zero-point
state (matches: wipe happened exactly once, zeros persisted). Real fix: keep the once-through
(construct semantics unchanged - stamping was already validated working) but restore the
Smart-authored route from the `SetSplineDataAndUpdate` backup (`HoloData->BackupSplineData`,
guaranteed present on replay belts) when vanilla zeroes the spline, then regenerate meshes.
The round-2 tag guard stays (harmless, pipe-consistent, closes the other cascade vector).

**Round-2b UI consistency (same feedback message):** name/description/Module-name inputs
restyled LIGHT fill + dark text (`StyleRestoreInputLight`, pushed to live BP widgets via
`SynchronizeProperties`) - the dark BP fill made hint text grey-on-dark-grey; clipboard pane
brown wash -> same muted dark pane as details panes; "Extend clipboard" header -> dim
section-label styling; Modules "Apply" -> accent primary mirroring "Load to Panel".
