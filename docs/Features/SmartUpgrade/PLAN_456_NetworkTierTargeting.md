---
title: "#456 — Network Scan Selective Tier-to-Tier Targeting (Implementation Plan)"
type: PLAN
date: 2026-07-03
status: Ready for implementation (handoff to Fable)
category: Features
tags: [upgrade, traversal, network-scan, tier, ui, mp]
related: [SFUpgradeExecutionService.cpp, SmartUpgradePanel.cpp, SmartUpgradePanel_Detail.cpp, IMPL_SmartUpgrade_CurrentFlow.md]
issue: 456
---

# #456 — Network Scan Selective Tier-to-Tier Targeting

**Homework by:** Opus (this pass). **Implementation by:** Fable.
**Companion reference:** `IMPL_SmartUpgrade_CurrentFlow.md` (the canonical Smart Upgrade flow — read it first).

---

## 1. What #456 asks for

> A network (traversal) scan can only pick a single **target** tier and then upgrades *everything below it*. Radius scan lets you pick a specific **source** tier. Make network scan match: select a specific source tier → target tier (e.g. "Mk2 → Mk3 only"), leaving other sub-target tiers alone.

Reporter: Infarctus, Discord `#discussion` (2026-07-02).

---

## 2. Root cause (verified against live code)

The upgrade **execution engine already supports tier-to-tier** — `FSFUpgradeExecutionParams` carries both `SourceTier` and `TargetTier`, and radius mode uses them. The gap is entirely in **(a) the traversal UI never lets you choose a source tier**, and **(b) two execution filter points ignore `SourceTier` on the traversal path.**

### 2a. UI gap — traversal rows are display-only

`SmartUpgradePanel_Detail.cpp :: UpdateTraversalUI` (~line 946) renders the tier breakdown as **plain `UTextBlock`s** ("`Mk2: 5`"). They are not `UBorder` rows, are never added to `RowDataMap`, and have no click path. So:

- There is no way to select a source tier in network mode.
- `SelectedTier` stays `0` in traversal (`UpdateTraversalUI` sets it to `0` explicitly, line ~994).
- `PopulateTargetTierDropdown` keys off `bIsTraversalMode ? 1 : SelectedTier` → in traversal it lists **all** tiers Mk.2..maxUnlocked regardless of what's present.
- `UpdateCostDisplay` (traversal branch, ~line 358) counts every `Entry.CurrentTier < CachedTargetTier` → "everything below target".

Contrast: radius mode's `UpdateAuditUI` (`SmartUpgradePanel.cpp` ~line 738) builds **clickable `UBorder` rows**, registers them in `RowDataMap`, and the hit-test in `NativeOnMouseButtonDown` (~line 911) calls `OnRowSelected(Family, Tier)` which sets `SelectedTier` = the source tier. **This is the exact pattern to mirror.**

### 2b. Execution gap — TWO filter points, both ignore SourceTier on the traversal path

This is the non-obvious part and the reason a one-line fix is wrong.

**Filter point #1 — `GatherUpgradeTargets`, `SpecificBuildables` branch** (`SFUpgradeExecutionService.cpp` ~line 257):

```cpp
// Add all buildables below the target tier (they will be upgraded)
int32 BuildableTier = USFUpgradeTraversalService::GetBuildableTier(Buildable);
if (BuildableTier < CurrentParams.TargetTier && BuildableTier > 0)
{
    PendingUpgrades.Add(Buildable);
}
```

Filters `tier < target`, ignoring `SourceTier`. This governs **pipes, poles, wall outlets** in traversal (they never go through cohort normalization).

**Filter point #2 — `NormalizeConveyorUpgradeTargets`** (`SFUpgradeExecutionService.cpp` ~line 465). For conveyors, `GatherUpgradeTargets` hands the seeds here, which **re-expands each seed to its full connected cohort** and re-derives eligibility:

```cpp
const int32 Tier = USFUpgradeTraversalService::GetBuildableTier(Conveyor);
if (Tier <= 0 || Tier >= CurrentParams.TargetTier) continue;   // below target

// Radius mode still honors SourceTier as the user's requested source tier.
if (!CurrentParams.HasSpecificBuildables() && CurrentParams.SourceTier > 0 && Tier != CurrentParams.SourceTier)
{
    continue;
}
```

The `SourceTier` filter is **explicitly gated OFF when `HasSpecificBuildables()`** (the traversal path). So even if the UI sets `SourceTier` in traversal, the cohort re-filter would drag every other-tier belt in the same connected chain back in. **This line must change too**, or "Mk2 only" still sweeps Mk3/Mk4 in the same conveyor run.

> **Why two points matter:** a connected conveyor network is one cohort. Filter #1 only decides *which cohorts get expanded* (a seed of the source tier pulls its whole cohort); filter #2 decides *which members of an expanded cohort are upgraded*. Conveyor source-tier correctness lives at #2; pipe/pole/outlet source-tier correctness lives at #1.

---

## 3. Design decision: keep the sweep, add the granular mode

Do **not** remove "upgrade everything below target" — it's useful and it's the current behavior. Model source tier so that:

- **`SourceTier == 0`** = "All tiers below target" (today's sweep). Preserves back-compat; every existing code path with `SourceTier == 0` behaves identically.
- **`SourceTier > 0`** = "only this exact tier → target" (the new #456 behavior).

This makes every execution change **inert when `SourceTier == 0`**, so radius mode and the existing MP path are untouched by construction.

---

## 4. Implementation

### 4a. Execution (the core enabler — 2 edits, both backward-compatible)

**Edit 1 — `GatherUpgradeTargets`, `SpecificBuildables` branch** (~line 257). Honor an explicit source tier; fall back to the sweep when 0:

```cpp
int32 BuildableTier = USFUpgradeTraversalService::GetBuildableTier(Buildable);
if (BuildableTier <= 0) continue;
const bool bMatch = (CurrentParams.SourceTier > 0)
    ? (BuildableTier == CurrentParams.SourceTier)          // #456: exact source tier
    : (BuildableTier < CurrentParams.TargetTier);          // legacy sweep
if (bMatch)
{
    PendingUpgrades.Add(Buildable);
}
```

**Edit 2 — `NormalizeConveyorUpgradeTargets`** (~line 473). Drop the `!HasSpecificBuildables()` gate so the source-tier filter applies on the traversal path too:

```cpp
// Honor SourceTier as the requested source tier in BOTH radius and network modes (#456).
// SourceTier == 0 keeps the "all tiers below target" sweep (filter is inert).
if (CurrentParams.SourceTier > 0 && Tier != CurrentParams.SourceTier)
{
    continue;
}
```

That's the whole engine change. `SourceTier` is already a `UPROPERTY` on `FSFUpgradeExecutionParams`, so it **already replicates over the RCO** — no new MP plumbing (`Server_StartUpgrade(Params)` carries it).

### 4b. UI — make traversal tier rows selectable (mirror radius)

All in `SmartUpgradePanel_Detail.cpp` / `SmartUpgradePanel.cpp`:

1. **`UpdateTraversalUI`** — replace the display-only `UTextBlock` loop with clickable `UBorder` rows, one per entry in `Result.CountByTier`, **plus a top "All tiers below target" row** (represents `Tier = 0`, preserves the sweep and stays the default). For each row:
   - Clear `RowDataMap` at the top of this function first (today only `UpdateAuditUI` clears it).
   - Build the same `UBorder` + `UHorizontalBox` row shape radius uses (copy the pattern from `UpdateAuditUI` lines ~774-844 — count column, "x", tier name).
   - `RowDataMap.Add(RowBorder, FRowData{ Result.Family, Tier, DisplayName });` (Tier 0 for the "All" row).
   - Do **not** set `SelectedTier` here; leave selection to the click (default to the "All" row selected so the button works with no click, matching today).

2. **Row click routing** — the hit-test in `NativeOnMouseButtonDown` currently always calls `OnRowSelected(Family, Tier)`. Branch by tab:
   - Radius → `OnRowSelected` (unchanged; it searches `CachedAuditResult` for nearest instance).
   - Traversal → a new lightweight `OnTraversalRowSelected(int32 Tier)` (see below). `OnRowSelected` can't be reused as-is because it searches `CachedAuditResult` (empty/stale in traversal).

3. **New `OnTraversalRowSelected(int32 Tier)`**:
   - `SelectedFamily = CachedTraversalResult.Family; SelectedTier = Tier;`
   - Re-highlight rows via the existing `RowDataMap` loop (copy from `OnRowSelected` lines ~104-112).
   - `PopulateTargetTierDropdown(); UpdateCostDisplay();` enable `SharedUpgradeButton`.
   - *Nice-to-have:* compute nearest instance from `CachedTraversalResult.Entries` filtered by `Tier` and show the same "Nearest Mk2: 34m NE" readout (reuse `GetCardinalDirection`). Skip for `Tier == 0`.

4. **`PopulateTargetTierDropdown`** (~line 244) — the traversal source floor is now the selected tier, not always 1:
   ```cpp
   int32 MinSourceTier = (bIsTraversalMode && SelectedTier == 0) ? 1 : SelectedTier;
   ```
   (When a specific traversal tier is picked, target options become "above that tier", exactly like radius.)

5. **`UpdateCostDisplay`** (traversal branch, ~line 358) — filter by the selected source tier when set:
   ```cpp
   const bool bTierMatch = (SelectedTier > 0)
       ? (Entry.CurrentTier == SelectedTier)
       : (Entry.CurrentTier < CachedTargetTier && Entry.CurrentTier > 0);
   if (bTierMatch) { /* count + cost */ }
   ```

6. **`OnUpgradeButtonClicked`** (`SmartUpgradePanel.cpp` ~line 459) — **already** sets `Params.SourceTier = SelectedTier`. Once `SelectedTier` can be non-zero in traversal, it "just works." The `bIsTraversalMode` validation (only requires a target) stays correct for the `Tier == 0` sweep.
   - *Optional optimization:* when `SelectedTier > 0`, pre-filter the `SpecificBuildables` loop (lines ~470-476) to `Entry.CurrentTier == SelectedTier` so the RCO payload on huge networks only carries the relevant actors. Not required for correctness (execution filters anyway) but meaningfully shrinks MP traffic. Call it out; let Fable decide.

### 4c. Blueprint work

**None required.** The two dropdowns (`RadiusTargetTierComboBox`, `TraversalTargetTierComboBox`), the `TraversalResultsContainer`, and `SharedUpgradeButton` already exist as `BindWidgetOptional`s. Traversal rows are built **programmatically** into `TraversalResultsContainer`, exactly like radius rows. This is a pure C++ change — no `Smart_Upgrade_Panel` BP edit, no cook-risk.

---

## 5. Edge cases & guardrails for Fable

- **Belts + lifts share a conveyor network.** `Result.Family` is the anchor's family (Belt or Lift), but `CountByTier` aggregates both. Selecting "Mk2" upgrades all Mk2 conveyors (belt + lift) in the cohort — correct, and matches how execution treats the cohort. The target-tier dropdown's `MaxTier` derives from `SelectedFamily`; belt vs lift unlocks usually match but not guaranteed — acceptable, note in code.
- **`RowDataMap` lifecycle across tabs.** `SwitchToTab` resets `SelectedTier/Family` but does **not** clear `RowDataMap`. The hit-test guards on `Widget->IsVisible()`, so collapsed-tab rows can't be clicked — stale entries are harmless but grow the map. Clear `RowDataMap` at the top of both `UpdateAuditUI` (already does) and `UpdateTraversalUI` (add it).
- **Default selection.** Keep the button usable with zero clicks: default the "All tiers below target" row selected after a scan (i.e. `SelectedTier = 0`), reproducing today's behavior. A user who wants granular picks a specific row.
- **`SourceTier == 0` must stay a valid, meaningful value** everywhere (it's the sweep). Don't treat 0 as "unset/error" in the new branches.
- **Pumps** have no execution (audit/traversal only) — unaffected.
- **MP:** verify on the dedi that `Server_StartUpgrade(Params)` still carries `SourceTier` (it will; it's a plain replicated `int32`). No RCO signature change.

---

## 6. Test matrix (SP + MP)

Build a mixed conveyor run: Mk1 + Mk2 + Mk3 belts in one connected network, plus a branch with a lift.

1. Network scan → **"All tiers below target" + target Mk6** → upgrades Mk1/Mk2/Mk3 (today's behavior; regression check).
2. Network scan → **source Mk2 → target Mk3** → only Mk2 belts become Mk3; Mk1 and Mk3 untouched. **(the #456 acceptance case)**
3. Network scan → **source Mk2 → target Mk6** in a run that also has Mk3/Mk4 → only Mk2 upgraded, Mk3/Mk4 left alone (proves the cohort re-filter at edit #2 works).
4. Mixed belt+lift network, source Mk2 → confirms both belt and lift Mk2 members upgrade and the chain stays intact.
5. Pipe network, source Mk1 → Mk2 → only Mk1 pipes (proves filter #1 for non-conveyor families).
6. Radius mode unchanged (regression): source-tier row select still works.
7. MP (dedi): repeat #2 and #3 as a client — result comes back via `Client_ReceiveUpgradeResult` and only the selected tier is upgraded.

---

## 7. Effort estimate

- Execution: 2 small edits, backward-compatible by design.
- UI: ~1 new handler + mirror the radius row-builder into `UpdateTraversalUI` + 3 small conditional tweaks (dropdown floor, cost filter, hit-test branch).
- BP: none.
- Risk: **low.** No new engine surface, no MP plumbing, no BP/cook risk; the sweep path is preserved as `SourceTier == 0`.

---

## 8. Bundled task: align the Upgrade panel to the shared Smart! style (`SFPanelStyle`)

Do this **in the same pass** as the UI work above — you're already rewriting these rows, and the new traversal rows should be built in the aligned style from the start rather than restyled later.

**Context:** the Smart Panel / Smart Restore dark-orange-light scheme was extracted into a shared header, `Private/UI/SFPanelStyle.h` (palette constants + `MakeButtonStyle(bAccent)` + `StyleInputLight(box)`, all header-only inline). Smart Restore already consumes it. The Upgrade panel currently **mixes two oranges** (`0.886,0.498,0.118` in headers/tabs/row-highlight vs `1.0,0.6,0.2` in the comboboxes), uses white/`0.5` greys instead of `0.9`/`0.65`, and leaves its buttons on the **BP-default style**. Everything is reachable programmatically — **zero BP edits**.

`#include "UI/SFPanelStyle.h"` in `SmartUpgradePanel.cpp` / `_Detail.cpp`, then:

1. **Oranges → `SFPanelStyle::Accent`.** Replace every `FLinearColor(0.886f, 0.498f, 0.118f, 1.0f)` (section headers ~`SmartUpgradePanel.cpp:729`, tab active color + selected-row highlight in `SwitchToTab`/`OnRowSelected`) with `SFPanelStyle::Accent`. The comboboxes at `SmartUpgradePanel.cpp:169/198` already use `1.0,0.6,0.2` — swap to `SFPanelStyle::Accent` too for provenance.
2. **Buttons → `SFPanelStyle::MakeButtonStyle`.** In `NativeConstruct`, `SetStyle(SFPanelStyle::MakeButtonStyle(false))` on the neutral buttons (`RadiusScanButton`, `EntireMapButton`, `TraversalScanButton`, `SharedCancelButton`, `SharedCloseButton`) and `MakeButtonStyle(true)` on the primary `SharedUpgradeButton`. For the tabs, replace the `SetBackgroundColor` approach in `SwitchToTab` with `SetStyle(MakeButtonStyle(bActiveTab))` + a `NearBlackText`/`LightText` label swap (mirror `SetActiveRestoreTab` in `SmartSettingsFormWidget_RestoreTabs.cpp`).
3. **Text greys → palette.** Row text white→`SFPanelStyle::LightText`, the `0.5`/`0.6` dims → `SFPanelStyle::DimText`. Keep the green "(N upgradeable)" tint as-is (semantic, not chrome).
4. **Backdrop (optional).** `BackgroundBorder` is a `BindWidgetOptional`; `BackgroundBorder->SetBrushColor(SFPanelStyle::MutedPanel)` if it needs to match — check in-game first, the BP border may already be fine.
5. **Inputs.** `RadiusSliderSpinBox` is a `USpinBox`, not a `UEditableTextBox`, so `StyleInputLight` doesn't apply directly; leave it unless it reads poorly in-game (a spinbox style is a separate, optional tweak).

Keep it a **pure recolor** — no layout/structure changes beyond the #456 rows. Verify in-game that the two tabs read as one consistent scheme (the round-2 Restore feedback was specifically about cross-tab consistency).

---

## 9. Implementation record (2026-07-03, Fable — awaiting in-game validation)

**Implemented exactly per §4 + §8, plus the flagged optionals. Both Shipping targets compiled clean first try; client + dedi DLLs deployed.**

- **Execution (§4a):** both filter points now honor `SourceTier` (`==0` sweep / `>0` exact), with `[#456]` comments explaining the cohort-re-expansion trap at the `NormalizeConveyorUpgradeTargets` site.
- **UI (§4b):** `UpdateTraversalUI` rebuilt — clickable `UBorder` rows via a shared `AddTraversalRow` lambda (count/x/name columns mirroring radius), **"All tiers" sweep row first** (Tier 0), per-tier rows sorted ascending, `RowDataMap` cleared at entry; ends with `OnTraversalRowSelected(0)` so the default reproduces pre-#456 behavior with zero clicks. New `OnTraversalRowSelected(int32)` (plain method, not UFUNCTION): sets selection, accent-highlights, **gates the upgrade button on the selection actually containing upgradeable members**, nearest-instance readout for a specific tier (the §4b nice-to-have — implemented), then dropdown + cost refresh. Hit-test branches by tab. Dropdown floor + cost filter per plan. `OnUpgradeButtonClicked`: traversal validation now also requires `TargetTier > SelectedTier` when a tier is picked; **the RCO-payload pre-filter optional was implemented** (uses `Entry.CurrentTier`, execution still re-filters — defense in depth); the "Upgrading X Mk.0 → Mk.N" sweep-mode status wart fixed with a separate LOCTEXT.
- **Styling (§8):** `SFPanelStyle.h` included via `SmartUpgradePanelImpl.h`. Neutral buttons (`RadiusScan/EntireMap/TraversalScan/SharedCancel/SharedClose`) + accent `SharedUpgradeButton` styled in `NativeConstruct` with **labels forced light** (guards against the Restore round-1 dark-label-on-dark-fill bug); `SwitchToTab` now uses `MakeButtonStyle(bActive)` + NearBlack/Light label swap (mirrors `SetActiveRestoreTab`); both hardcoded oranges, row/separator greys, and combobox item colors unified onto the palette. Header BP chrome (`RefreshButton`/`CancelButton`/`CloseButton` X) and `BackgroundBorder`/`RadiusSliderSpinBox` deliberately untouched per §8 items 4-5 — revisit only if they read poorly in-game.
- **Known UX edge (accepted):** after scanning tab A then tab B, tab A's old result rows remain visible but unclickable until re-scan (`RowDataMap` holds one tab's rows; `SwitchToTab` already resets selection + prompts a new scan — matches pre-existing flow).
- **NEXT:** run the §6 test matrix in-game, **case #3 first** (source Mk2 → target Mk6 in a run that also has Mk3/Mk4) — it's the one that proves the cohort-re-filter edit. New LOCTEXT keys (`Upgrade_AllTiersRow/AllTiersTooltip/TierRowTooltip/NearestTier/UpgradingSweep`) are English until the next loc re-gather.

### 9a. Follow-up fix: anchor detection independent of held item (2026-07-03, field report)

**Report:** "if I wasn't holding a higher-tier belt than the belt I was looking at, I couldn't scan the network."

**Root cause:** `OnTraversalScanClicked` resolved the scan anchor two ways — (1) `Hologram->GetUpgradedActor()`, which only returns a target when the HELD build gun would perform a *valid* upgrade on the aimed buildable (i.e. you're holding a higher-tier belt), and (2) a line-trace fallback that searched for the nearest `AFGBuildable` within 300cm of the impact point. Belts/pipes/poles are AbstractInstance-rendered, so the trace reports the shared instance *manager* as the hit actor (never the buildable), and the 300cm-from-actor-location search misses long belts (actor pivot far from the aim point). So in practice only path (1) worked → the "must hold a higher tier" symptom.

**Fix (round 1, incomplete):** in the fallback, after the line trace, resolve the hit to its owning buildable via `AAbstractInstanceManager::ResolveHit` + `GetOwnerByHandle` (the same mechanism the game's dismantle aim uses). Required enabling the `AbstractInstance` plugin/module dependency (`SmartFoundations.Build.cs` + `.uplugin` — base-game plugin, always present).

### 9b. Follow-up: post-upgrade refresh in network mode (2026-07-03)

**Report:** "After an upgrade, the scan needs to be refreshed." `OnUpgradeCompleted` only called `RefreshAudit()` (the *radius* scan), so the network tier rows/costs stayed stale after a network upgrade.

**Fix:** branch `OnUpgradeCompleted` by active tab — network mode calls a new `RefreshTraversalScan()`, radius keeps `RefreshAudit()`. The wrinkle is the anchor belt may have been **replaced by its own upgrade** (new actor, old pointer dead), so `RefreshTraversalScan` re-acquires a seed: original anchor if still valid → any still-valid scanned entry → nearest upgradeable buildable within 10m of the remembered `TraversalAnchorLocation` (new belts spawn in-place). Extracted the scan tail (config read + SP/MP dispatch) into a shared `RunTraversalScanFromAnchor` so the button and the refresh use one path; stamped `TraversalAnchorLocation` at scan time. Compiled clean both targets; deployed.

**MP follow-up (field-confirmed 2026-07-04): the auto-refresh didn't fire on a client** (manual re-scan worked) — exactly the replication race predicted in the pre-test MP review. On a client, `OnUpgradeCompleted` fires from the result RPC (`InjectUpgradeResult`) BEFORE the server's upgraded actors replicate down, so the immediate re-acquire finds no valid seed and silently no-ops. Fix: `RefreshTraversalScan()` now returns bool (kicked a scan?), and on a client `OnUpgradeCompleted` calls `BeginDeferredTraversalRefresh()` — a 0.6s looping retry (up to 5 attempts / ~3s) that re-acquires + re-scans until a fresh result lands. `OnClientTraversalResult` clears the timer the moment the server answers (so usually 1-2 tries); the attempt cap + "aim and Scan again" message is the backstop; `NativeDestruct` clears the timer. Host/SP still refreshes immediately (synchronous, actors already exist). **Purely client-side — no server/struct/RPC change, so it's protocol-compatible with the live dedi and needs no server restart.** Compiled clean both targets; client deployed (dedi unchanged this build).

### 9c. Follow-up: pipe traversal crosses junctions (2026-07-03)

**Report:** "Does network scan not traverse pipe junctions?" — correct, it didn't. `TraversePipelineNetwork` only recursed into `AFGBuildablePipeline` neighbors and stepped through pumps (`bCrossPumps`); a cross/T junction is `AFGBuildablePipelineJunction : AFGBuildablePipelineAttachment` (not a pipeline, not a pump), so the walk dead-ended at every junction and missed the rest of the manifold.

**Fix (maintainer: cross unconditionally):** generalized the pump-crossing into `ShouldCrossPipeAttachment` (junctions **always** cross — pure passthrough topology; pumps stay behind `bCrossPumps`) + `CrossPipeAttachment` which steps through the attachment to the pipes on its other connections. Crucially it recurses through **chained** attachments (junction→junction, junction→pump) via mutual recursion with `TraversePipelineNetwork`, so multi-junction manifolds are fully walked — a one-hop mirror of the pump block would still have under-counted. Attachments are crossed, not collected (never upgrade targets); the VisitedSet breaks cycles and skips the incoming pipe. Server-side walk, so SP + MP both fixed. Compiled clean both targets; deployed.

### 9a details

**Fix (round 2, complete — the field-corrected diagnosis):** round 1 still failed when holding a **matching-tier** belt. The real condition isn't "holding a higher tier" — it's holding a **different** tier (upgrade OR downgrade) so the build gun enters a replace context and `GetUpgradedActor()` returns the target. Matching tier = plain build mode → null, AND the snapped belt-preview hologram sits exactly where you aim, so the round-1 `ECC_Visibility` trace hit the *hologram*, not the belt. The maintainer pointed at Extend, which detects same-type targets reliably. Root cause found by reading Extend's detection trace (`SFSubsystem.cpp` ~1010-1033): it uses **`PC->GetPlayerViewPoint`** + **`ECC_WorldStatic`** (hits buildings, not just visibility) + **ignores the pawn AND the active hologram**. Round 2 mirrors that exact recipe, then layers: direct `Cast<AFGBuildable>(HitResult.GetActor())` for real-actor buildings (machines/poles) → `ResolveHit` for AbstractInstance belts/pipes → 300cm nearest-search safety net. The key miss in round 1 was **not ignoring the active hologram** + the wrong trace channel. Compiled clean both targets; deployed. Now anchors regardless of the held **tier** (matching or different). Note: the Upgrade Panel only opens while you hold a conduit (belt/pipe/power line, `IsUpgradeCapableContext`), so "empty hand" was never a real case — the fix removes the *matching-tier* failure within that gate, not the conduit-in-hand requirement itself.
