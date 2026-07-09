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
- **HUD** — the active-target **highlight follows facing** (not the cycled axis), and navigation
  prompts relabel ("Num0 / re-tap: next direction"; stagger adds "9/3: stack/flat"). Labels stay
  absolute X/Y/Z/ZX/ZY — only the highlight + prompt logic branches. (`SFHudService` highlight is
  today `bIsActive = (mode && cycledAxis == thisAxis)`; under PR it compares the **facing-mapped**
  axis of the current target.)

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

Because the machinery is shared, classic mode inherits two pure additions for free (no behavior
change to anything that works today): the **re-tap gesture** and, where noted below, numpad
direct-select in modal (currently dead there).

### 5.1 The re-tap gesture (resolves double-tap-on-a-hold-key)

Mode keys stay **hold-to-activate**. The gesture is *tap, then re-grip*: on a mode-key **press**, if
that key was released less than ~300 ms ago, advance the target (then keep holding and scroll). Each
quick re-grip advances one more.

- Implementation is timestamps in the existing mode handlers (`Started` checks time since last
  `Completed`) — **no IMC/trigger surgery**, nothing to rebind.
- Works identically in classic (the original "double-tap `;` to switch spacing axis" wish) and PR.
- No collision with **double-tap Num0** (temporary Smart disable) — different keys.
- A single stray tap just flickers the mode on/off, as it does today (harmless).

### 5.2 Per-transform targets and numpad profiles

**PR profile** — numpad = compass (matches the feel-approved scaling spike: 8/5 = away/toward,
6/4 = right/left, 9/3 = up/down). Direction keys **select and adjust** — the wheel continues on
whatever you last picked.

| Mode | Targets (Num0 / re-tap cycle) | Num8/5 | Num6/4 | Num9/3 |
|---|---|---|---|---|
| Scaling *(default mode)* | — (modifiers/facing select; no cycle) | fwd ± | side ± | vert ± |
| Spacing | fwd → side → vert | fwd ± | side ± | vert ± |
| Steps | fwd → side | fwd ± | side ± | *(no-op — steps are vertical)* |
| Rotation | fwd-prog → side-prog | fwd ± | side ± | *(no-op)* |
| Stagger | V·fwd → V·side → H·fwd → H·side | fwd-drift ± *(current family)* | side-drift ± *(current family)* | **family select: 9 = Stack, 3 = Flat** |

In PR, **increase/decrease ≡ forward/backward** (Num8/5 are the increase/decrease bindings; PR
defines "increase" as "grow toward intent"). The **wheel is the generic adjuster** for the current
target — that plus Num0/re-tap is the complete wheel-only path.

**Stagger, resolved:** the four modes are (progression × horizontal drift): V = the vertical stack
leans, H = a horizontal run drifts. The **family is the build context** (stack vs flat grid — default
**Vertical**, matching the classic ZX-first cycle); the **direction keys are the drift** (away/toward,
right/left), facing-resolved. Family select (Num9/3) preserves the drift sub-slot (V·side → `3` →
H·side). The 4-slot cycle crosses families, so wheel-only players reach all four.

**Classic profile** — unchanged where it has behavior today, extended where it's dead:

| Input | Classic behavior |
|---|---|
| Wheel / Num8/5 | current axis ± — **unchanged** (Num8 on a cycled-to-Z spacing still adjusts Z) |
| Num0 | cycle axis — unchanged |
| **Re-tap mode key** | cycle axis — **new** |
| **Num6/4, Num9/3 in modal** | select Y / select Z + adjust — **new** (dead today; pure addition). *Exception: stagger stays cycle-only (4 modes don't map to 3 keys).* |

### 5.3 Sign policy (the resolver returns axis **and** sign)

Facing quantization yields `(axis, sign)`. Whether the sign multiplies the delta depends on the
transform's value semantics — this is per-transform, not global:

| Transform | Value means | Facing-sign applied? |
|---|---|---|
| Scaling | grow direction | **Yes** (shipped, feel-confirmed) |
| Spacing | gap magnitude (symmetric) | **No** |
| Steps | rise per cell along the run (`StepsX * X` — **signed** cell index) | **Yes** — else facing the −axis way, "up" descends going away. *Feel-verify.* |
| Stagger | drift direction (`Stagger* * signed index`) | **Yes** — "leans right" must mean right regardless of facing. *Feel-verify.* |
| Rotation | curl direction (away = right; classic Y-negation already in) | **Compose with resolver sign — feel-verify** (highest-risk sign in the set) |

### 5.4 State and lifetime

- PR targets live in a **new runtime struct** on the subsystem (per-transform current slot + stagger
  family) — **not** in `FSFCounterState`, so presets/Restore/MP never see them (a view-dependent slot
  is meaningless to save).
- Reset to defaults (all **fwd**; stagger family **Vertical**) on build-gun clear / recipe change —
  the same lifetime as other runtime HUD state (§3).
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
latch; stagger two-family (family = build context, default Vertical), 4-slot cycle, Num9/3 = family
select; Cycle Axis = target-cycler in PR (not a no-op — wheel-only accessibility forces this);
**re-tap** (tap, re-grip within ~300 ms) = advance target, replacing the double-tap-on-hold question
— hold-to-activate stays, no toggle rework, lands with the core pass; PR numpad = compass profile
with select-and-adjust; classic keeps Num8/5 = current-axis ± untouched and gains re-tap + modal
Num6/4/9/3 (except stagger, which stays cycle-only).

**Feel-verify list (first build):** steps facing-sign (rise-away), stagger drift signs, rotation
curl under resolved progression (highest risk), re-tap threshold (~300 ms), wheel-continues-on-last-
selected-target, HUD highlight following facing while a mode is held.

See the [scope doc](209-controls-simplification-scope.md) for the live decision log.
