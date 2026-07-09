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
  prompts relabel (e.g. Stagger's Num0 becomes **"Toggle Z Stagger"**). Labels stay absolute
  X/Y/Z/ZX/ZY — only the highlight + prompt logic branches. (`SFHudService` highlight is today
  `bIsActive = (mode && cycledAxis == thisAxis)`; under PR it compares the **facing-mapped** axis.)

## 5. Target selection — one operation, three inputs

"Select which target the wheel drives" is a single operation with three equivalent entry points:

1. **Numpad direction** — jump straight to a target (10-key shortcut).
2. **Cycle Axis** (Num0 / rebindable) — advance to the next target.
3. **Double-tap the mode key** — advance to the next, numpad-free (also speeds up classic mode).

The **wheel** drives the current target; **facing** maps a PR slot → concrete axis at input time; the
**HUD** highlights it. **Wheel-only players rely on (2)/(3)** — so Cycle Axis stays live in PR as the
slot-cycler (it is *not* a no-op), and every target must be reachable without a numpad.

### Per-transform targets

| Transform | Classic (absolute) | PR (facing slots) |
|---|---|---|
| Spacing | X → Y → Z | primary → lateral → vertical |
| Steps / Rotation | X → Y | primary → lateral |
| Stagger | ZX → ZY → X → Y | (Vert) primary → (Vert) lateral → (Horiz) primary → (Horiz) lateral |

**Stagger** is the special case: 4 modes = (progression axis) × (perpendicular horizontal drift).
Under PR the two families (Vertical ZX/ZY, Horizontal X/Y) fold into the target cycle; default family
= **Vertical** (matches the ZX-first cycle used for distributor grids). Facing maps primary/lateral to
the concrete Stagger axis inside the active family.

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
latch; stagger two-family, default Vertical; Cycle Axis = slot-cycler in PR (not a no-op — wheel-only
accessibility forces this).

**Open:** double-tap-mode-key interaction (hold-vs-toggle — the mode keys are hold-to-activate today,
so a double-tap needs an Enhanced Input double-tap trigger or a toggle rework); whether double-tap
lands with the core PR pass or as a fast-follow.

See the [scope doc](209-controls-simplification-scope.md) for the live decision log.
