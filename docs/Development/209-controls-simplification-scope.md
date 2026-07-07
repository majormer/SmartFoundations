---
title: Controls Simplification — Scope & Design
type: SCOPE
date: 2026-07-07
status: Draft (pre-spike)
category: Development
issue: 209
tags: [input, controls, ux, player-relative, scaling, spacing, steps, stagger, rotation]
related: [../Features/AutoConnect/IMPL_SmartBlueprints_CurrentFlow.md]
---

# Controls Simplification (#209) — Scope & Design

**Status: exploratory.** This records the design direction agreed in evaluation. It is
deliberately spike-shaped — several decisions below can only be settled by *feel* in-game, and
are flagged as open. Nothing here is committed until a throwaway spike validates the core.

Issue #209 (ShadedPL, migrated from the old tracker) asks to cut Smart!'s keybind count and make
scaling directional/intuitive. The evaluation reframed it from "remove keybinds" into **two
separable levers** over the existing transform system, neither of which changes state, layout,
multiplayer, or presets.

---

## 1. The core model: every transform is `(value, axis)`

The state already decomposes every transform into a magnitude plus an axis. The axis enum is even
*named* in player terms today (`X = "Forward/Back"`, `Y = "Left/Right"`, `Z = "Up/Down"`) — the
original design wanted player-relative semantics and settled for fixed building axes.

| Transform | Value field(s) | Axis field |
|---|---|---|
| Scaling | `GridCounters` (signed `FIntVector` — negative = grow toward −axis) | modifier-chosen today |
| Spacing | `SpacingX/Y/Z` | `SpacingAxis` |
| Steps | `StepsX/Y` | `StepsAxis` |
| Stagger | `StaggerX/Y/ZX/ZY` | `StaggerAxis` |
| Rotation | `RotationZ` (deg) | `RotationAxis` (progression) |

Everything funnels through one call: `ApplyAxisScaling(ESFScaleAxis Axis, int32 signedDelta)`, which
pushes `GridCounters[Axis]` by a signed delta. **Direction (including negative) is already a
first-class layout concept** — this is what #168 leaned on for −X/−Y grids. So the redesign is an
*input-layer* remap of "which axis + sign does this input select," not a rewrite of scaling.

---

## 2. Lever A — Directional-pad axis selector

Replace axis **cycling** and the **increase/decrease** keys with a directional pad. While a mode is
held, a direction key picks the axis + sign directly; scroll (or repeated taps) sets magnitude.

| Held mode | Forward (e.g. Num8/2) | Lateral (Num4/6) | Vertical (Num9/3 · PgUp/Dn) |
|---|---|---|---|
| Scaling | grow array fwd/back | grow left/right | grow up/down |
| Spacing | fwd gap | lateral gap | vertical gap |
| Steps | staircase along fwd run | staircase along lateral run | (steps *are* vertical) |
| Stagger | offset rows fwd/back | offset rows left/right | vertical stagger |
| Rotation | progress angle per fwd-clone | per lateral-row | (n/a) |

This is what lets **Cycle Mode**, **Increase value**, and **Decrease value** be deleted.

**Open:** dedicated numpad vs. context-sensitive arrows (Nudge when no mode held, axis-select while a
mode is held). Arrows are Nudge today; reusing them is tempting but overloads a key.

---

## 3. Lever B — "Player Relative" (global boolean)

A single setting. When on, a directional input resolves against the **player's view** (quantized to
the nearest building cardinal) instead of the building's fixed axes. It is one shared resolver
consumed by every transform — it does not fork per-transform.

**Critical implementation call: resolve at INPUT time, not layout time.** When you press a direction,
map facing → a concrete building axis and write *that* into the state (`SpacingAxis = Y`, etc.). Never
rotate at layout time. Consequences:

- State stays building-relative → **no schema change; presets/Restore replay identically; MP untouched**
  (the spec already carries concrete axes).
- The "latch" is automatic — you set the axis on press; turning your head afterward doesn't scramble
  an in-progress build.
- The whole feature is a boolean over the input→axis-selection code. Nothing downstream changes.

**Z is invariant** — up is up regardless of facing; Player Relative only ever swaps forward↔lateral.

**Composition with building rotation:** resolve facing against the building's *local* axes (which
already rotate with the building), so the two layers don't double-rotate.

**Look-down / steep pitch** (wall/ceiling builds): yaw ill-defined looking straight down → fall back
to last valid yaw or the building's forward. Input-time latching hides most of this.

---

## 4. Handedness — one rule: "away is right"

Locked convention, consistent across every transform:

| Scroll | Lateral (scale / spacing / stagger) | Rotation |
|---|---|---|
| **Away (+)** | grow / offset to **screen-right** | curl **out-right** (clockwise) |
| **In (−)** | to **screen-left** | curl **out-left** (counter-clockwise) |

This *already matches* the rotation code: `// Positive RotationZ = Clockwise (user expectation)` /
`// Rotation > 0: Curves right`. So rotation needs no convention flip — the lateral transforms adopt
rotation's handedness, and Player Relative reads "right" as screen-right. The array progresses "out"
(the forward direction) and curls left/right off that run, so forward-growth, lateral-growth, and
rotation-curl all agree on "away" and "right" for a given view.

**Verify in the spike:** the "away = positive `AxisValue`" link is a mouse-wheel IMC binding, not a
code guarantee. The entire "away = right" chain rests on it — confirm it first, it's the kind of
inversion that silently flips everything.

---

## 5. Keybind delta (the payoff)

| Disposition | Control | Why |
|---|---|---|
| **Add** | Scaling mode; **Player Relative** toggle (setting) | one mode key + the boolean |
| Keep | Spacing / Steps / Stagger / Rotation mode; Recipe / Auto-Connect | unchanged |
| **Remove** | Scale X / Y modifiers | direction from facing (Lever B) |
| **Remove** | direction arrows (as scale-direction) | direction from facing |
| **Remove** | Cycle Mode | directional pad picks axis (Lever A) |
| **Remove** | Increase / Decrease value | directional pad / scroll sets value (Lever A) |
| **Remove** | Toggle Arrows | direction implicit → HUD can drop axis colors |

The HUD simplification the reporter asked for ("one-color values back") falls out of Lever B: with
axes view-relative, the HUD shows **Forward / Lateral / Vertical** (the enum's existing display
names) instead of colored X/Y/Z.

---

## 6. Open questions (need a decision or a spike)

1. **Default on or opt-in?** Player Relative changes the default feel. Precision builders who scale a
   fixed world axis regardless of gaze now must physically face it (input-time latch softens this).
   Do we ship it default-on, or default-off with the boolean, or keep a "classic controls" scheme?
   *A classic scheme roughly doubles input maintenance forever — decide deliberately.* **This is the
   one to get the reporter's and testers' read on.**
2. Directional pad: numpad vs. context-sensitive arrows.
3. Z control: modifier vs. PgUp/Dn (looking up/down to scale vertically is awkward).
4. Latch/hysteresis specifics (45° flip band) — feel, not logic.
5. Radial menu for Recipe / Auto-Connect — orthogonal; likely a later phase, uncertain value.

---

## 7. Risk & effort

| Area | Effort | Risk |
|---|---|---|
| facing → (axis, sign) resolver + input-time write | **M** | Med — 45° hysteresis, negative-axis sign (the #168 cell-order family) |
| Directional-pad routing (replaces cycle / inc-dec) | **M** | Med — coupled input state |
| IMC asset surgery + `PlayerMappableKeySettings` names | **M** | **High-fiddly** — breaks the settings screen if wrong (SmartCamera-brackets lesson) |
| HUD relabel + drop axis colors / arrows | **M** | Low |
| Player Relative setting plumbing | **S** | Low — one boolean, input layer only |
| Backward-compat / classic mode | **M–L** | **Highest — UX, not code** (see open Q1) |
| Loc + wiki (control labels, Controls.md, Settings-Reference.md) | **S** | Low |
| Radial menu (deferred) | **L** | Med |

**Verdict:** XL overall as a full redesign, but the *core* is a contained input-layer spike. Biggest
risk is the UX/compat decision, not the implementation.

---

## 8. Recommended first step — spike, don't spec further

Wire a **facing → (axis, sign) resolver with input-time write** in front of the existing
`ApplyAxisScaling`, leave the old modifiers working alongside it, and *feel* it for ten minutes.
Because downstream is untouched, the spike is ~1–2 days and answers the only questions that matter
(latch timing, hysteresis, the precision trade-off, handedness inversion) better than more analysis.
Keep the two levers separable in the spike so each can be judged on its own.
