---
title: Controls Simplification — Scope & Design
type: SCOPE
date: 2026-07-07
status: Design locked (pre-spike)
category: Development
issue: 209
tags: [input, controls, ux, player-relative, scaling, spacing, steps, stagger, rotation]
related: [../Features/AutoConnect/IMPL_SmartBlueprints_CurrentFlow.md]
---

# Controls Simplification (#209) — Scope & Design

> **Durable companion:** [`Controls-InteractionModel.md`](Controls-InteractionModel.md) — the
> three-surfaces model (Smart Panel · Mouse Wheel · Keyboard/numpad + runtime HUD) and the rule that
> PR must keep all three aligned. That doc is the framework; this doc is the #209 decision log.

Issue #209 (ShadedPL, migrated) asks to cut Smart!'s keybind count and make scaling
directional/intuitive. The evaluation reframed it from "remove keybinds" into **two separable
levers over the existing transform system**, neither of which changes stored state, layout,
multiplayer, or presets. The design below is settled; the remaining unknowns are *feel* (latch
timing, hysteresis), which only a spike can answer.

---

## Decisions (locked 2026-07-07)

| Decision | Call | Notes |
|---|---|---|
| Player Relative rollout | **Opt-in** (default OFF + toggle) | Existing muscle memory untouched; flip default later if it proves out |
| Direction selection | **Numpad cluster + facing + scroll, all live** | Fwd/back · left/right · up/down; numpad keys **rebindable** (no-10-key users) |
| Value input | **Keep scroll AND +/− keys** | Increase/Decrease stay; more conservative than the reporter's max |
| Radial menu | **Declined** | Smart Panel (K) already does it; that part is *done* |
| HUD axis colors | **KEPT** (reversed 2026-07-08) | Stay as the Smart Panel's absolute-axis reference; not worth changing |
| Keybind removal (Scale X/Y mods, Cycle Mode) | **OUT of scope** (dropped 2026-07-08) | Additive is final; players can clear bindings they don't want |
| PR extends to spacing/steps/stagger/rotation | **YES** (2026-07-09) | Unified Target Model — see [Controls-InteractionModel §5](Controls-InteractionModel.md) |
| Cycle Axis under PR | **Target-cycler, NOT no-op** (2026-07-09) | Wheel-only (no numpad) players need a numpad-free selector |
| Stagger — BOTH modes | **2×2 factoring confirmed** (2026-07-09): family (Stack/Flat, default Stack) × direction; stored 4-state unchanged (projections); Num9/3 = family direct | 4-way cycle was the artificial part; classic and PR become structurally identical |
| Double-tap mode key | **Re-tap gesture** (tap, re-grip < ~300 ms = advance target), core pass | Solves double-tap-on-hold with timestamps only; also speeds classic |
| Stagger gestures (BOTH modes) | **Num0 = family toggle (Z on/off), re-tap `Y` = direction within family** — classic feel-tested 2026-07-09, PR aligned same day | Family = the bigger mental switch, deserves the dedicated key; cross-mode consistency beats within-mode Num0 uniformity; full no-numpad reach |
| HUD active-row label under PR | **ROLE-labeled** (`[Forward]/[Lateral]/[Vertical]`; stagger `[Stack/Forward]`), NO axis descriptor; inactive rows stay absolute (2026-07-09) | The role is what re-tap cycles — labeling it is what makes the cycle player-relative; revises keep-absolute-labels for the active row only |
| Forward wheel sign | **Expansion-aware**: facing × grid-counter sign — up grows the run you face; from the far end, up pulls it toward you (2026-07-09, two-screenshot spec) | "Behind me is where it would expand" |
| Lateral/Vertical wheel sign | Plain magnitude (up = more); steps/rotation lateral = counter-signed (view-independent); **stagger lateral stays view-signed (up = leans right)** | Perpendicular facing isn't represented; a lean's direction IS its meaning |
| Numpad in PR modal | **Compass profile, select-and-adjust** (8/5 fwd, 6/4 side, 9/3 vert) | Matches feel-approved scaling spike; wheel continues on last pick |
| Classic mode | **Num8/5 = current-axis ± UNCHANGED**; gains re-tap. Modal Num6/4/9/3 keep their existing grid-scaling behavior (they were never dead — direct-select is PR-only). **Deliberate stagger changes: Num0 = axis-within-family (was 4-way cycle), Num9/3 = family select, re-tap = family flip** — changelog note | Zero regression outside stagger |
| Facing-sign per transform | Scaling YES · Spacing NO · Steps/Stagger YES · Rotation compose | Signed cell-index math (`Steps*X`, `Stagger*X`); feel-verify list in model doc §7 |
| HUD axis labels | **Keep absolute X/Y/Z** (no Forward/Lateral relabel) | See "feedback model" — the preview is the feedback, not the label |
| Arrow graphic | **Kept** | Orientation cue; needed by the Panel context anyway |
| Scope boundary | **Player Relative is world-context only** | The Panel is frozen → no facing → stays absolute X/Y/Z |

---

## 1. The core model: every transform is `(value, axis)`

State already decomposes every transform into a magnitude + an axis. Everything funnels through one
call: `ApplyAxisScaling(ESFScaleAxis Axis, int32 signedDelta)`, pushing `GridCounters[Axis]` by a
signed delta. **`GridCounters` is a signed `FIntVector`** — negative components already mean "grow
toward −axis" (what #168 leaned on). So direction is a first-class *layout* concept, and the redesign
is an *input-layer* remap of "which axis + sign does this input pick," not a scaling rewrite.

| Transform | Value field(s) | Axis field |
|---|---|---|
| Scaling | `GridCounters` (signed) | modifier-chosen today → numpad/facing |
| Spacing | `SpacingX/Y/Z` | `SpacingAxis` |
| Steps | `StepsX/Y` | `StepsAxis` |
| Stagger | `StaggerX/Y/ZX/ZY` | `StaggerAxis` |
| Rotation | `RotationZ` (deg) | `RotationAxis` (progression) |

The axis enum is even named in player terms already (`X = "Forward/Back"`, `Y = "Left/Right"`,
`Z = "Up/Down"`) — the original design wanted player-relative and settled for fixed axes.

---

## 2. Two input surfaces, one source of truth

The critical architecture, clarified by the Smart Panel interaction:

| Surface | Context | Axis model |
|---|---|---|
| **World scaling** (numpad + facing + scroll) | You can look / move | **Player Relative** — input resolves to an absolute axis via facing |
| **Smart Panel (K)** | Player frozen, UI open | **Absolute X/Y/Z, always** — typed into X/Y/Z fields directly |

Both write the **same absolute state** (`GridCounters.X/Y/Z`, `SpacingX/Y/Z`, …). Player Relative is
an input-time convenience in the world context *only*; the Panel writes absolute axes directly and
needs **zero** player-relative work. This is why the absolute axes and the arrow graphic survive —
they're ground truth (the Panel edits them; the arrows orient them). Player Relative removes the
*need to consciously track* axes while scaling in the world, not their existence.

---

## 3. Lever A — Directional selection (numpad + facing + scroll)

While a transform mode is held, three inputs cooperate:
- **Facing** sets the primary axis (look where you want it to grow).
- **Numpad cluster** explicitly picks any axis+sign: fwd/back · left/right · up/down. **Rebindable**
  so no-10-key users can remap to arrows + two keys.
- **Scroll** (and the retained **+/− keys**) set magnitude.

| Held mode | Fwd (Num8/2) | Lateral (Num4/6) | Vertical (Num9/3 · PgUp/Dn) |
|---|---|---|---|
| Scaling | grow array fwd/back | grow left/right | grow up/down |
| Spacing | fwd gap | lateral gap | vertical gap |
| Steps | staircase along fwd run | staircase along lateral run | (steps *are* vertical) |
| Stagger | offset rows fwd/back | offset rows left/right | vertical stagger |
| Rotation | progress angle per fwd-clone | per lateral-row | (n/a) |

Because the numpad picks the axis directly, **Scale X/Y modifiers and Cycle Mode are removed.** The
**+/− keys stay** (value input decision).

---

## 4. Lever B — "Player Relative" (opt-in global boolean, world-context only)

A single setting, default OFF. When on, a directional input resolves against the **player's view**
(quantized to the nearest building cardinal) instead of the building's fixed axes.

**Resolve at INPUT time, not layout time.** On press, map facing → a concrete building axis and write
*that* into the state. Never rotate at layout time. Consequences:
- State stays absolute → **no schema change; presets/Restore replay identically; MP untouched**.
- The latch is automatic — you set the axis on press; turning afterward doesn't scramble a build.
- The whole feature is a boolean over the input→axis-selection code in the world context.

**Z is invariant** (up is up). Composition with building rotation: resolve against the building's
*local* axes (which already rotate with it), so the layers don't double-rotate. Look-down / steep
pitch: fall back to last valid yaw or building forward; input-time latching hides most of it.

### Feedback model (why X/Y/Z labels never change)
The player's confirmation is **spatial**: the preview array visibly grows in the direction they
aimed. That is the primary feedback, not the HUD counter. So the HUD keeps absolute **X/Y/Z** labels
everywhere (world *and* Panel — one vocabulary, no translation), and Player Relative silently changes
*what your scroll does*, not *what the HUD calls things*. Relabeling to Forward/Lateral would only
create a two-vocabulary mismatch with the Panel for no feedback gain.

---

## 5. Handedness — one rule: "away is right"

| Scroll | Lateral (scale / spacing / stagger) | Rotation |
|---|---|---|
| **Away (+)** | grow / offset to **screen-right** | curl **out-right** (clockwise) |
| **In (−)** | to **screen-left** | curl **out-left** (counter-clockwise) |

This already matches the rotation code (`// Positive RotationZ = Clockwise`, `// Rotation > 0:
Curves right`), so rotation needs no flip — the lateral transforms adopt rotation's handedness.
**Spike must verify** the "away = positive `AxisValue`" mouse-wheel binding in the IMC — the whole
chain rests on it, and it's the kind of inversion that silently flips everything.

---

## 6. HUD

- **Remove per-axis colors** on the grid readout (the theme handles look → single-color values).
- **Keep absolute X/Y/Z labels** (see feedback model).
- **Keep the arrow graphic** as an orientation cue (Toggle Arrows key may stay).

---

## 7. Keybind delta

| Disposition | Control | Why |
|---|---|---|
| **Add** | Scaling-mode key | one hold-key for scaling |
| **Add** | Player Relative toggle (setting) | opt-in boolean |
| **Add** | Numpad directional cluster (rebindable) | explicit axis+sign selection |
| Keep | Spacing / Steps / Stagger / Rotation mode keys | unchanged |
| Keep | Increase / Decrease value keys | value-input decision |
| Keep | Arrow graphic / orientation | ground-truth + Panel context |
| Keep | Recipe / Auto-Connect | via the Smart Panel (radial declined) |
| **Remove** | Scale X / Y modifiers | numpad/facing pick the axis |
| **Remove** | Cycle Mode | numpad picks the axis directly |

More modest than the reporter's maximal cut, but coherent and low-risk (opt-in, familiar +/− kept).

---

## 8. What's left — the spike (feel, not logic)

The design is closed on paper. Only *feel* remains, and it's spike-shaped:
1. **Latch timing** — sample facing on mode-enter (recommended) vs first-scroll vs continuous.
2. **Hysteresis** — the 45° flip band so the axis doesn't twitch as you look around.
3. **Handedness sanity** — confirm "away = right" bottom-to-top (incl. the IMC wheel-sign link).

Wire a **facing → (axis, sign) resolver with input-time write** in front of the existing
`ApplyAxisScaling`, leave the old modifiers working alongside it, and *feel* it for ten minutes.
Downstream is untouched, so the spike is ~1–2 days and answers the only open questions better than
more analysis. Keep the two levers separable so each is judged on its own.

---

## 9. Risk & effort

| Area | Effort | Risk |
|---|---|---|
| facing → (axis, sign) resolver + input-time write | **M** | Med — 45° hysteresis, negative-axis sign (#168 cell-order family) |
| Numpad directional routing (replaces X/Y mods + Cycle) | **M** | Med — coupled input state; rebinding surface |
| IMC asset surgery + `PlayerMappableKeySettings` names | **M** | **High-fiddly** — breaks the settings screen if wrong (SmartCamera-brackets lesson) |
| HUD: drop axis colors (keep labels + arrows) | **S** | Low |
| Player Relative setting plumbing | **S** | Low — one boolean, world-input layer only |
| Loc + wiki (Controls.md, Settings-Reference.md) | **S** | Low |

**Verdict:** M–L overall (down from XL — opt-in kills the classic-mode maintenance burden, and the
radial is gone). The core is a contained input-layer spike; no downstream, state, MP, or Panel work.
