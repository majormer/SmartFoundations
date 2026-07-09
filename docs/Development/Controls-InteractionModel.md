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
facing → a concrete absolute axis at INPUT time** and writes that. So:

- No schema change; **presets / Smart Restore / multiplayer replay identically**.
- The Panel keeps absolute **X/Y/Z** labels everywhere (one vocabulary, no translation).
- PR only changes *what your scroll/keys do*, never *what the HUD calls things*.

## 3. Three kinds of "settings" (don't conflate them)

| Layer | Lives in | Lifetime | Examples |
|---|---|---|---|
| **Global config** | `Smart_Config` asset | Persists across sessions | Player Relative toggle, scroll increments, auto-connect modes |
| **Runtime HUD state** | In-memory, per session | Until build-gun clear / recipe change | current target/axis selection, arrows toggle |
| **Per-build Panel edits** | Panel session | One build, then revert to global | spacing/scale entered in the Panel (#371) |

Only **global config** persists between builds. Runtime and Panel state reset — mirror that when
adding PR state (e.g. the stagger family selector is runtime, not global).

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

### 5.5 Explicitly out of scope (align later, tracked)

- **Extend** direction cycling (has its own fwd/right/back/left semantics) — review against PR later.
- **Smart Walking** segment modals (segment-relative, not player-relative) — unaffected.
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
