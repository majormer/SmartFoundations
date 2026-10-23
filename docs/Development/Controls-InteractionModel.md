# Smart! Controls — Interaction Model (and the Player Relative fork)

**Durable architecture reference.** This is the one place that states *how a player drives Smart!*
across every surface, and the rule the Player Relative (PR) work must obey. Read it before touching
any control, HUD-navigation, or transform-input code.

- Running decision log / status for the PR effort: [`209-controls-simplification-scope.md`](209-controls-simplification-scope.md)
- Config plumbing: [`Config-Settings-Guide.md`](Config-Settings-Guide.md)

---

## The core rule

> **Every player-facing change to controls must keep the three interaction surfaces and the runtime
> HUD aligned to the *current PR mode*. PR forks behavior — but you never fork one surface without the
> others, and the HUD must always report the state the input surfaces are actually in.**

---

## 1. The three surfaces (+ the settings layer)

| # | Surface | Context | Input | Forks under PR? |
|---|---|---|---|---|
| 1 | **Smart Panel** | Frozen UI; player is frozen while it's up | Direct value entry, absolute **X/Y/Z** | **No — always absolute.** The Panel is the absolute reference/island. |
| 2 | **Mouse Wheel + modifiers** | World | Wheel = magnitude; modifiers/scroll pick the target | **Yes.** MUST work with **no numpad** (accessibility requirement). |
| 3 | **Keyboard / numpad + modifiers** | World | Mode-hold keys; numpad = direct target select; Cycle Axis | **Yes.** |
| + | **Runtime HUD settings** | World, outside the Panel | Live toggles (Cycle Axis, target selection, arrows…) | The HUD **display** forks (see §4). |

Whatever we do in PR has to be reasoned about across **all three** rows — a change that only makes
sense on the numpad (row 3) but strands the wheel-only player (row 2) is incomplete.

## 2. The shared state (PR does **not** change this)

Every transform is `(value, axis)`. State is stored **absolute** (`FSFCounterState`: `GridCounters`,
`Spacing*`, `Steps*`, `Stagger*`, `RotationZ`, and the per-mode `*Axis` selectors). **PR resolves
facing → a concrete absolute axis at INPUT time** and writes that. Extend is the deliberate
context exception: its X/Y storage has stable **Chain/Rows** meanings, resolved at input time
without exposing those storage axes in the runtime HUD. So:

- No schema change; **presets / Smart Restore / multiplayer replay identically**.
- The Panel keeps absolute **X/Y/Z** labels everywhere (one vocabulary, no translation).
- PR only changes *what your scroll/keys do*, never *what the HUD calls things*.

### Explicit Extend entry

Compatible factory and Water Extractor recipes expose **Extend** in the task strip. **Select Source** arms one world click outside the window; matching native targets receive a temporary outline. Escape cancels selection, panel clicks stay in the interface, and a miss retains the current preview. Source acceptance uses the same eligibility, topology capture and staged authority construction as automatic hover activation. The UI never constructs or charges materials.

The tab reuses the existing placement controls rather than introducing another set of counters. Merely opening it retains the current placement. Replacing a nontrivial draft or another source asks for confirmation first. Explicit source intent uses the existing manual-hold lifetime; build, cancel and recipe changes retain native teardown. The HUD identifies the source and resolves the actual rebound Panel key. Automatic matching-recipe hover remains available.

### Resuming Walking from existing logistics

Equip **Stackable Pipeline Support**, **Stackable Hypertube Support**, or **Stackable Conveyor Pole** for the corresponding run. Holding a pipe, tube or belt recipe alone does not enable this entry path. Close the Panel, aim at an exposed endpoint of the existing run with the stackable support equipped, then reopen the Smart Panel and select **Walking**. Do not build a new starting support first.

Smart discovers aligned, unoccupied horizontal endpoints in a regular grid, including manually placed buses, and restores their lane/level counts and spacing. Check the **Resume** summary before continuing. Existing lines and supports remain in place; normal material costs apply to new construction. Wall/floor attachments, occupied ends and ambiguous or irregular endpoint grids are rejected. A vertical continuation can be authored from a horizontal starting endpoint.

### Panel apply and close behavior

**Apply placement immediately** controls grid geometry (counts, direction, spacing, steps, stagger and progressive rotation, including its progression axis). With it off, the displayed placement edits wait for Apply. X, Escape and the panel toggle discard unapplied grid edits; accepted task switches retain the draft. Normal construction is still a separate action.

Auto-Connect preferences, production recipes, native wall rotation and Sign support mode are **live settings**. Their previews update immediately and their choices remain when the panel closes, independently of the geometry checkbox. Connection changes do not spend construction materials.

Walking uses **Apply segment edits immediately** for Advance, Turn, Rise and Shift. With it off, the displayed segment drafts survive refreshes and reopening; Apply commits exactly those values. **Preview exit** describes the currently applied route. Connection tier, routing, style and belt flow remain live. Closing Walking keeps the walk and staged segment values; ending it through Scaling/Restore still requires confirmation.

### Rotatable conduit wall supports

Pipeline and Hypertube wall supports use their native full rotation as the grid frame. Local Y follows the conduit run; local Z is the other axis in the wall plane. Conveyor wall supports retain their existing frame.

The Panel keeps these local X/Y/Z labels and offers a separate **Wall rotation** selector in native 45-degree steps. This uses native build-gun rotation, including its placement restrictions. The existing **Rotation** field still controls progressive grid curvature; it is not the wall-support angle.

With Player Relative enabled, scaling and spacing choose whichever in-wall axis is closest to world up for the vertical target. At a 45-degree tie, local Z remains the vertical target. The other in-wall axis serves the horizontal target, with its direction resolved against the unrolled parent frame and player facing. Thus a vertical conduit run uses local Y for X+Z+wheel and Num9/3, while the side target uses local Z. Arrow highlights and signs use the same resolution. Walking, Extend and the steps/stagger/curvature target models keep their existing rules.

## 3. Three kinds of "settings" (don't conflate them)

| Layer | Lives in | Lifetime | Examples |
|---|---|---|---|
| **Global config** | `Smart_Config` asset | Persists across sessions | Player Relative toggle, scroll increments, auto-connect modes |
| **Runtime HUD state** | In-memory, per session | Until build-gun clear / recipe change | current target/axis selection, arrows toggle |
| **Per-build Panel edits** | Panel session | One build, then revert to global | spacing/scale entered in the Panel (#371) |
| **Local interface layout** | Local player preferences | Across saves on this computer | Panel positions; HUD appearance uses the existing global HUD settings |

**Global config and local interface layout** persist between game sessions. Runtime and construction Panel state reset between builds — mirror that when
adding PR state (e.g. the stagger family selector is runtime, not global).

### Interface layout and edit mode

Panel organization follows shared placement controls, applicable family options and building-specific controls. This is presentation, not a settings-inheritance model. The placement matrix retains the original native spin boxes, direction buttons, ranges, focus/navigation and staged/live handlers. Panel axes keep their existing absolute meanings under Player Relative.

The task strip exposes Scaling, Walking, Upgrade and Restore according to the current buildable's capabilities and prerequisites. Railway tracks and Vehicle Paths open Walking by default and hide Scaling. A held belt or pipe can expose an unavailable Walking capability with the compatible-support prerequisite; the tab does not create a new native starting attachment. Upgrade keeps its Radius and Network methods, and Restore keeps Grid Presets and Modules.

Task navigation belongs in the strip rather than duplicate footer shortcuts. Scaling's footer owns Apply Immediately, Reset and Apply; Walking's owns Apply Immediately and Apply. Upgrade's header X closes the panel, while Cancel Scan is shown only for a pending cancellable Radius scan. Radius and Network retain independent source-row and target-tier selections across method switches, subject to the current snapshot and available native tiers. Changing a Network target invalidates that method's saved selection too.

Restore is a page inside the task window. Load to Panel loads placement settings, returns to Scaling for inspection and honors Apply Immediately. Load Module confirms the recipe/session change and starts a layout preview; construction and material charges still occur through the normal build action. Saved-entry actions are unavailable without a valid selection.

Changing tasks suspends their drafts without applying, constructing, charging or executing an upgrade. A transition that ends an unbuilt walk must first identify the lost path and staged segment edits and offer **Keep Walking / End Walk**. Keep Walking and Escape dismiss that warning without closing the panel or losing work. Hidden tasks do not own modal input, change arrow resolution or revert obsolete placement state into a different hologram.

Network Upgrade retains an explicitly selected target. **Select Target** arms one world click while the panel remains open: panel clicks stay UI clicks, and one left click outside attempts the exact native hit and leaves selection mode even on a miss. Escape cancels selection alone, retaining the previous target and draft. The selection click never reaches construction. Missing targets require deliberate reselection rather than a nearest-object guess. Changing targets clears the old network results, tier and materials state until an explicit Scan; delayed results for an earlier target cannot replace the current selection.

Smart Panel, Walking and Upgrade accept left-button dragging on their title bars and retain right-button dragging. Close buttons retain each panel's own close/cancel behavior. The configured Smart Panel key (default K) is also handled inside the modal window: Scaling, Restore, Extend and Upgrade close through their existing cancellation path; Walking hides its editor while retaining the route. Key repeats do not repeatedly toggle the window, and editable text keeps its ordinary typing behavior. An open confirmation is dismissed before leaving Scaling or Walking. Only task windows that are both visible and mounted suppress the ordinary HUD; suspended or closed drafts cannot retain suppression. HUD layout editing and the saved Show HUD preference remain independent. All tasks share one local window position across saves. The first opened task migrates its older saved position when a shared position is absent. Restore uses the same frame and has no independent dock or close button. **Reset Panel Positions** in Mod Settings restores default positions independently of the construction **Reset** button. Restored frames are clamped to the viewport.

Each task and the HUD/interface settings provide an actual small `?` button beside their context label or section title. The button uses no literal brackets and no dedicated row. Hover shows the guidance; click expands the details in the task body, and click again collapses them, removing their layout space. Topic titles remain inside the guidance. Scaling places the button beside the held building name and hides it while Restore is active; HUD appearance and Interface layout put it at the right of their title row. Walking, Upgrade and Restore pair it with the existing route summary or relevant section heading. Descriptions stay collapsed by default; control tooltips explain scope, units, restrictions and consequences. Help expansion changes neither drafts nor construction state. Grid Preset loading waits for its matching native building preview before refreshing contextual controls and returning to Scaling; Apply Immediately is honored after that handoff. Upgrade presents scope/scan, results/target tier and materials in order; execution remains explicit. Grid Presets store placement settings; Modules store captured Extend layouts. Their help distinguishes selecting, loading, saving and building. Pending library confirmations block task navigation until resolved.

When Camera hosts shared Edit Mode, its single Done / Lock saves the pending camera rectangles and Smart HUD appearance. Cancel and Escape discard pending previews; pressing the edit binding again commits and exits. Forced menu/world closure cancels remaining drafts. Participants finish synchronously before host removal so normal HUD visibility does not depend on delayed widget destruction. Failed saves keep the editor open with feedback; do not silently discard remaining drafts. HUD preferences remain global while cameras keep their selected all-saves/current-save scope. An explicit All saves camera edit patches only its authored fields across that camera's global and existing saved profiles, including the current save. This save edits only the exact current-save profile. Layout commits patch position/size; enabling, frame rate, brightness, angle and distance have independent edit intent. A global camera reset removes its overrides across saves; a this-save reset removes only the current override. Other cameras remain unchanged. Smart HUD appearance and Panel positions stay global-only, independent of the camera scope chooser. Done must leave the edited rectangles visibly applied.

The HUD is locked and click-through during normal play. Layout editing provides drag/resize handles, a native scale spin box, the six existing themes, temporary preview, **Cancel HUD changes**, **Done / Lock** and **Reset HUD Appearance**. Done writes the existing HUD position/scale/theme settings once; Cancel leaves them unchanged. Resizing retains the current multilingual font selection. A marked sample allows editing when no normal HUD is visible.

The compact HUD separates the current Classic/Player Relative mode, active label/value and colored grid. Its active spacing, steps, stagger or rotation hint resolves the current input mappings, including player remaps; it does not define another binding or infer a second axis model. Layout handles sit outside the HUD content.

With Smart Camera present, its rebindable Edit Mode action owns the shared edit session. Without Camera, **Edit HUD Layout** defaults to F8 and uses its own rebindable mod input action. The independent context is removed while Camera owns the session. Mod Settings also offers **Edit HUD layout after Resume**. Editing appearance does not alter the current construction, Player Relative mode, presets or Camera framing.

Recipe controls are available only for buildings that accept a production recipe (`AFGBuildableManufacturer`, including derived modded machines). Power daisy-chaining is an independent capability; it does not imply recipe support. Elevators, resource extractors, generators, storage and logistics buildings do not expose the production-recipe selector. Native dimensions such as the selected Personnel Elevator shaft height remain part of vanilla placement; grid counts and spacing describe repeated copies, not shaft height.

### Session-wide opt-out

**Toggle Smart (Session)** defaults to **Num Decimal** and is rebindable under Options > Controls > Mods.
It uses a separate, single-action `UFGInputMappingContext` that remains installed while the
hologram-scoped building context is removed. This preserves a re-enable route, including for a
keyboard without a numpad after rebinding. Classic and Player Relative modes share this action.

Disabling closes Smart panels, clears preview assistance, cancels the current native placement,
and shows a confirmation notification. Selecting a recipe then starts vanilla placement. The
opt-out survives recipe changes and holstering, but resets on world load; it changes no saved config.
The owning player's authority state rejects and clears staged Smart grid, Extend, and Walk plans;
it does not disable other players. Re-enabling does not restore an abandoned plan. A vanilla
blueprint can be placed while opted out without relaxing the safety limits on Smart payloads.

`SFSubsystem_Session.cpp` owns the transition and controller-keyed synchronization. The independent
session input context must never be removed by ordinary hologram cleanup. Regression coverage:
`SmartFoundations.Session.PlayerIsolation` checks stale-plan rejection and independent players.

## 4. The PR fork, per surface

- **Smart Panel** — *no fork.* Absolute X/Y/Z, player frozen, per-build edits.
- **Mouse Wheel** — the wheel drives the **current target**; by default the **primary** (line-of-sight)
  axis. Selecting a different target without a numpad is via **Cycle Axis / double-tap** (§5).
- **Keyboard / numpad** — numpad directions select the target **directly** (Num8/5 primary, Num6/4
  lateral, Num9/3 vertical); mode-hold keys; Cycle Axis / double-tap advance.
- **HUD** — the active-target **highlight follows facing**, and the **ACTIVE transform row is
  labeled by its ROLE**: `Spacing [Forward]*` / `[Lateral]` / `[Vertical]`; stagger shows
  family + role (`Stagger [Stack/Forward]*`). The role is what re-tap cycles and what the wheel
  drives — labeling it is the feedback that makes the cycle player-relative. **(2026-07-09 revision
  of the keep-absolute-labels rule: ACTIVE row only, PR only, no axis descriptor.)** Inactive value
  rows keep absolute X/Y/Z/ZX/ZY labels; the Panel is untouched.

## 5. The Unified Target Model

**One sentence:** every transform mode has a **current target**; every input either *adjusts* it,
*selects* it, or both; classic and PR run the **same machinery** and differ at exactly two
configuration points — the **resolver** (how a target maps to a concrete axis) and the **numpad
profile** (what the direction keys mean).

| Concept | Classic | Player Relative |
|---|---|---|
| **Target** | an absolute axis (the existing per-mode `*Axis`) | a facing slot: **fwd / side / vert** (stagger: family × drift) |
| **Resolver** | identity (target *is* the axis) | facing → (axis, sign) at input time |
| **Adjusters** | wheel = current target ± | same |
| **Selectors** | Num0 cycle · **re-tap mode key** (new) | Num0 cycle · re-tap · **numpad directions** (select + adjust) |
| **HUD highlight** | the current axis | the current target's **resolved** axis (follows facing) |

Because the machinery is shared, classic mode inherits the **re-tap gesture** for free. (Note: in
classic modal the directional numpad keys are *not* dead — they scale the grid while a transform key
is held, and that behavior is preserved. Numpad direct-select of transform targets is **PR-only**;
the sole classic exception is stagger's Num9/3 = family select, part of the deliberate stagger
navigation change.)

### 5.1 The re-tap gesture (resolves double-tap-on-a-hold-key)

Mode keys stay **hold-to-activate**. The gesture is *tap, then re-grip*: on a mode-key **press**, if
that key was released less than ~300 ms ago, advance the target (then keep holding and scroll). Each
quick re-grip advances one more.

- Implementation is timestamps in the existing mode handlers (`Started` checks time since last
  `Completed`) — **no IMC/trigger surgery**, nothing to rebind.
- Works identically in classic (the original "double-tap `;` to switch spacing axis" wish) and PR.
- No collision with **double-tap Num0** (temporary Smart disable) — different keys.
- A single stray tap just flickers the mode on/off, as it does today (harmless).
- **Stagger** has *two* orthogonal selectors (direction + family), so each gets its own gesture —
  **identical in both control modes** (classic feel-tested 2026-07-09, PR aligned the same day):
  **Num0 = family (Z on/off)**, **re-tap `Y` = direction within the family** (classic ZX↔ZY / X↔Y,
  PR fwd-drift↔side-drift). All four modes are reachable with no numpad. This makes stagger the one
  transform where PR's Num0 does *not* advance the target slot — the family is the bigger mental
  switch and gets the dedicated key; the drift slot rides re-tap and Num6/4.

**Default mode keys** (rebindable): `;` Spacing · `I` Steps · `Y` Stagger · `,` Rotation · `U` Recipe
· `X`/`Z` scale modifiers · Num0 Cycle Axis · Num8/5 increase/decrease · Num1 Arrows · `K` Panel.

### 5.2 Per-transform targets and numpad profiles

**PR profile** — numpad = compass (matches the feel-approved scaling spike: 8/5 = away/toward,
6/4 = right/left, 9/3 = up/down). Direction keys **select and adjust** — the wheel continues on
whatever you last picked. While a transform key is held, the direction keys drive **that transform**
(in classic they scale the grid during modals; under PR, release the mode key to scale — the compass
always means the same thing).

| Mode | Targets (Num0 / re-tap cycle) | Num8/5 | Num6/4 | Num9/3 |
|---|---|---|---|---|
| Scaling *(default mode)* | — (modifiers/facing select; no cycle) | fwd ± | side ± | vert ± |
| Spacing | fwd → side → vert | fwd ± | side ± | vert ± |
| Steps | fwd → side | fwd ± | side ± | *(no-op — steps are vertical)* |
| Rotation | fwd-prog → side-prog | fwd ± | side ± | *(no-op)* |
| Stagger | fwd-drift ↔ side-drift *(re-tap only — **Num0 = family Z-toggle**)* | fwd-drift ± *(current family)* | side-drift ± *(current family)* | **family select: 9 = Stack, 3 = Flat** |

In PR, **increase/decrease ≡ forward/backward** (Num8/5 are the increase/decrease bindings; PR
defines "increase" as "grow toward intent"). The **wheel is the generic adjuster** for the current
target — that plus Num0/re-tap is the complete wheel-only path.

**Stagger — the 2×2 factoring (BOTH modes, confirmed 2026-07-09):** the four stagger modes are
(progression × horizontal drift), and both classic and PR navigate them as **family × axis** instead
of one 4-way cycle:

- **Family = the build context** — **Stack** (the vertical pile leans: classic ZX/ZY) vs **Flat** (a
  horizontal run drifts: classic X/Y). **Num0** toggles it (Z on/off) and **Num9/3** jump direct
  (9 = Stack, 3 = Flat) — both preserve your direction pick, in both control modes. Default
  **Stack** — with axis X, that's exactly today's ZX-first default.
- **Direction within the family** (classic X↔Y / PR fwd-drift↔side-drift) — **re-tap `Y`** in both
  modes; PR additionally direct-selects the side member with **Num6/4**.
- **The stored state does not change.** `StaggerAxis` keeps its four values {ZX, ZY, X, Y}; family
  and axis are *projections* of it — the family toggle rewrites ZX↔X / ZY↔Y, the axis toggle
  rewrites ZX↔ZY / X↔Y. All four counters still compound simultaneously; presets/Restore/MP capture
  and replay exactly what they do today.

This deletes stagger's special-case status: it becomes a 2-slot transform like steps/rotation, plus
one orthogonal toggle, identical in shape across classic and PR.

**Classic profile** — unchanged where it has behavior today, extended where it's dead, with **one
deliberate change** (stagger navigation — needs a changelog note):

| Input | Classic behavior |
|---|---|
| Wheel / Num8/5 | current axis ± — **unchanged** (Num8 on a cycled-to-Z spacing still adjusts Z) |
| Num0 | cycle axis — unchanged for spacing/steps/rotation; **stagger: family toggle, Z on/off** (ZX↔X / ZY↔Y, was the 4-way cycle — the deliberate classic change) |
| **Re-tap mode key** | cycle axis — **new**; stagger: **direction within the family** (ZX↔ZY / X↔Y) |
| Num6/4 in modal | grid Y scaling — **unchanged** (never dead: the numpad scales the grid while a transform key is held) |
| Num9/3 in modal | grid Z scaling — unchanged for spacing/steps/rotation; **stagger: family select (9 = Stack, 3 = Flat)** — part of the stagger change |

### 5.3 Wheel sign policy — per ROLE (maintainer spec 2026-07-09)

The resolver returns `(axis, sign)`; the sign policy is **per role**, not per transform:

| Role | Wheel-up means | Sign |
|---|---|---|
| **Forward** (all transforms) | grow/rise/curl/lean the run **in the direction you face** — standing at the anchor it stretches away; from the far end looking back, up **pulls it toward you** (spacing contracts) | facing × expansion direction (`PrimSign × sign(grid counter)`) |
| **Lateral / Vertical** — spacing | more gap | +1 (plain magnitude — the perpendicular facing isn't represented) |
| **Lateral** — steps / rotation | more rise / curl, toward the run's own far end | counter sign only (view-independent) |
| **Lateral drift** — stagger | leans to **your right** | `LatSign × sign(progression counter)` — the one signed lateral; a lean's *direction* is its meaning |

Scaling keeps its shipped scheme (facing-signed grow, feel-confirmed). Rotation's resolver sign
composes with the classic Y-negation — **feel-verify** remains the gate for steps/stagger/rotation.

### 5.4 State and lifetime

- PR's **view-dependent** targets (the per-transform fwd/side/vert slot) live in a **new runtime
  struct** on the subsystem — **not** in `FSFCounterState`, so presets/Restore/MP never see them (a
  view-dependent slot is meaningless to save).
- The **stagger family is NOT view-dependent** (Stack vs Flat is build context) — it lives in the
  existing stored `StaggerAxis` 4-state as a projection (§5.2), shared by classic and PR, persisting
  and preset-captured exactly as that field does today. PR resolves its drift slot *within* the
  stored family and writes the concrete 4-state back at input time.
- PR slots reset to defaults (all **fwd**) on build-gun clear / recipe change — the same lifetime as
  other runtime HUD state (§3).
- Classic per-mode axes stay in counter state exactly as today.
- HUD highlight must **refresh while a transform mode is held** in PR (facing changes don't touch
  counter state, so the existing change-driven redraw won't follow the player's turn — tick-gated
  refresh, only while a mode key is down).

### 5.5 Context-specific controls

- **Native pillars** use their full accepted parent rotation for grid X/Y/Z and spacing. The Panel
  retains absolute axis fields; their directions are the pillar's local axes. In Player Relative,
  the most vertical pillar axis serves Up/Down, and the remaining two axes serve Forward/Lateral
  according to view yaw. A diagonal tie keeps local Z as vertical. The HUD highlights the resolved
  axis and the arrows follow the same tilted frame. Profile dimensions remain local, avoiding a
  second tilt projection. Steps retains its world-vertical rise. This does not enable conduit-wall
  rotation controls for pillars or change externally tilted Blueprint behavior.

- **Extend** direction cycling stays building-relative (manifold alignment) — PR never forks it.
  While Extend is active, horizontal runtime transform targets are **Chain/Rows**, backed by signed X/Y state.
  The Left/Right selector owns the Chain sign, and every live/Restore placement path consumes that
  same signed state. Spacing cycles Chain → Rows → Vertical; Steps and Rotation cycle only Chain ↔ Rows.
  Grid Z repeats layers in world vertical, reachable with both scale modifiers plus wheel or Num9/3.
  The Smart Panel remains the absolute X/Y/Z editor. The horizontal exception is implemented by **#478**;
  layer scaling and vertical spacing are added by **#540**. Stagger remains unavailable in live Extend.
- **Smart Walking** is segment-relative, and that isolation is **enforced in code** (2026-07-09):
  `SF_ComputePlayerRelativeAxes` returns classic axes whenever a walk is active — one choke point
  disabling PR resolution wholesale (scaling, modal targets, HUD highlight, tick refresh) — plus
  walk gates on the Num9/3 stagger-family/vertical branches, PR slot cycling, and stagger re-tap,
  so every key keeps its walk meaning (Scale-X = advance, Y = lanes, Z = stack). Lesson: a scope
  statement about feature isolation needs an enforcement point, not an input-routing assumption.
- **Auto-Connect settings mode** (own Num0 meaning) — untouched.

## 6. Alignment invariant (the checklist)

When adding or changing a control, verify:

- [ ] **Panel** still absolute X/Y/Z (untouched by the change).
- [ ] **Wheel** path reaches the same target the **keyboard** path does — and reaches *every* target
      without a numpad.
- [ ] **HUD** highlight + prompts reflect the actual current target and PR mode.
- [ ] PR on/off flips **all** of {wheel semantics, keyboard/numpad meaning, HUD highlight/prompts}
      together — never one in isolation.
- [ ] New PR state is **runtime** (resets per build), not global, unless it's a deliberate setting.
- [ ] Stored state stays absolute (facing resolved at input time) → presets/Restore/MP unaffected.

## 7. Status & open questions

**Locked:** PR is opt-in (global default off); Panel stays absolute; HUD keeps absolute labels; no
latch; **stagger 2×2 factoring in BOTH modes** (family = build context Stack/Flat as a projection of
the stored 4-state, default Stack; Num0 = axis toggle within family; Num9/3 = family direct select)
— the **one deliberate classic behavior change** of the effort (Num0 was the 4-way cycle; needs a
changelog note); Cycle Axis = target-cycler in PR (not a no-op — wheel-only accessibility forces
this); **re-tap** (tap, re-grip within ~300 ms) = advance target, replacing the double-tap-on-hold
question — hold-to-activate stays, no toggle rework, lands with the core pass; PR numpad = compass
profile with select-and-adjust (PR-only — classic modal numpad keeps its grid-scaling behavior);
classic keeps Num8/5 = current-axis ± untouched and gains re-tap; **stagger gestures identical in
both modes (classic feel-tested + PR aligned 2026-07-09): Num0 = family (Z on/off), re-tap `Y` =
direction within the family**.

**Feel-verify list (first build):** steps facing-sign (rise-away), stagger drift signs, rotation
curl under resolved progression (highest risk), re-tap threshold (~300 ms), wheel-continues-on-last-
selected-target, HUD highlight following facing while a mode is held, classic stagger retrain (HUD
prompt clarity).

See the [scope doc](209-controls-simplification-scope.md) for the live decision log.
