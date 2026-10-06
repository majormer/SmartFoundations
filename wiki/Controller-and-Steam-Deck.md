# Controller & Steam Deck (Experimental)

> ⚠️ **Experimental / community-contributed.** Smart! was built for keyboard + mouse and has no
> official controller *bindings* — you map its keys with **Steam Input** (Steam's per-game
> controller layout editor). As of **34.2.0** the mod does ship one setting made for controllers:
> **Tap to Toggle Transform Modes** (see below), which makes a Steam Input radial menu drive
> Smart!'s modes natively. Feedback and better layouts are very welcome in the
> [Discord](https://discord.gg/SgXY4CwXYw) #help channel.

## Why this works with no conflicts

Base-game Satisfactory build mode only uses the **face buttons, triggers, D-pad, and sticks**. It
leaves the Steam Deck's most useful surfaces **completely free**: both **trackpads**, all four
**back paddles** (L4/L5/R4/R5), and **gyro**. Put every Smart! bind on those unused surfaces and
leave the native buttons alone, and Smart! adds **zero conflict** with normal building and movement.

> If you ever see native controller actions "break" after setting this up, it's almost always
> because a *native* button got remapped in Steam Input — not the mod. Keep ABXY / triggers / D-pad
> on their defaults and only add Smart! binds to the trackpads, paddles, and gyro.

## The single most important binding: the Smart Panel

The highest-leverage thing you can do is bind **one button to the Smart Panel** (default key `K`).
The Panel is a full on-screen form you navigate with the stick + `A`, and it exposes **every** Smart!
feature — all scaling axes, spacing, steps, stagger, rotation, recipe, restore, and toggles — with
**no key-holding at all**. If you do nothing else, this alone makes Smart! usable on a controller.

## Recommended Steam Deck layout

| Surface | Smart! binding |
|---|---|
| **R5** (back paddle) | **Smart Panel** — keyboard `K` (full feature access) |
| **L4** (hold) | X-axis modifier — hold keyboard `X` |
| **L5** (hold) | Y-axis modifier — hold keyboard `Z` |
| **L4 + L5** (hold both) | Z-axis — `X`+`Z` together |
| **R4** (hold) | Bulk dismantle — hold `Left Ctrl` |
| **Right trackpad — swipe** | Mouse-wheel emulation → adjusts the held axis / active mode (and vanilla rotate when no mode is active) |
| **Left trackpad — radial** | The numpad: `Num 8`/`Num 5` (X ±), `Num 6`/`Num 4` (Y ±), `Num 9`/`Num 3` (Z ±), `Num 0` (cycle axis), `Num 1` (toggle arrows) |
| **Right trackpad — click** | "Smart Modes" radial: `;` `I` `Y` `,` `U` — turn on **Tap to Toggle Transform Modes** and each pick latches its mode (see below) |
| **Gyro** *(optional)* | Mouse aim for fine build-gun pointing |

With just the paddles + two trackpads, you get: all discrete scaling (left-pad radial), continuous
scaling (paddle + right-pad wheel), axis cycling, arrow toggle, bulk dismantle, and — via the Panel
on R5 — literally everything else. Native movement/build controls stay on the normal buttons.

## Step-by-step setup (Steam Input)

1. In Steam, select Satisfactory → **⚙ (Controller Settings)** → **Edit Layout**.
2. Start from the built-in **Gamepad** template so native build controls are preserved.
3. Turn on **Back Grip Buttons** for the build action set.
4. Assign the paddles: **R5** → `K`; **L4** → `X` (hold); **L5** → `Z` (hold); **R4** → `Left Ctrl`
   (hold). (L4 + L5 pressed together naturally sends `X`+`Z` = the Z axis.)
5. **Left trackpad** → add a **Radial Menu**, 8 items, bound to `Num 8`, `Num 5`, `Num 6`, `Num 4`,
   `Num 9`, `Num 3`, `Num 0`, `Num 1`. Label them X+, X−, Y+, Y−, Z+, Z−, Cycle Axis, Arrows.
6. **Right trackpad** → set the **touch** behavior to **Mouse Wheel** (this drives live scaling and
   value changes). Optionally set the **click** behavior to a second radial for modes (see below).
7. *(Optional)* set **Gyro** to "As Mouse", gated behind a button or trackpad touch, for fine aim.
8. **Save**, and if you like, **Export/Publish** it as a community layout so others can grab it.

## Transform modes on a controller: Tap to Toggle

Smart!'s transform **modes** — Spacing (`;`), Steps (`I`), Stagger (`Y`), Rotation (`,`),
Recipe (`U` on a factory) — are normally **hold-to-activate**: you hold the key while scrolling.
A Steam Input radial selection is a momentary *tap*, so a radial can't hold a key for you. That's
what the **Tap to Toggle Transform Modes** setting is for *(Mods → Smart! →
Building Behavior, off by default; added in 34.2.0 —
[issue #482](https://github.com/majormer/SmartFoundations/issues/482))*.

With the setting **on**:

- **Tap a mode key once** → the mode switches **on** and stays on. Scroll the right trackpad
  (mouse wheel) to adjust; the numpad radial's `Num 0` still cycles the mode's axis/target.
- **Tap the same key again** → the mode switches **off** and the hologram is released.
- **Tap a different mode key** → it **switches** modes directly — exactly what a "Smart Modes"
  radial wants. One radial with `;` `I` `Y` `,` `U` gives you the whole modal surface, tap by tap.
- The HUD shows the active mode the same way it does for a held key, so you always see what a
  scroll will change.

Things that always release a latched mode (so you can't get stuck in one): putting the build gun
away, leaving build mode, switching to a **different building**, or opening any Smart! panel.
Placing a building and continuing with the *same* building keeps the mode on — just like holding
the key through the placement would.

A few boundaries to know:

- `U` latches only on **factories** (Recipe mode). On splitters/mergers, junctions, supports, and
  power poles, `U` opens the Auto-Connect settings walker and remains hold-to-use — bind it to a
  paddle you can hold if you use that surface a lot (or use the Smart Panel, which covers it all).
- With the setting **off**, everything behaves exactly as it always has — holds, plus the
  quick re-tap-of-a-held-key gesture that cycles the target. (In tap-to-toggle, re-tapping exits
  the mode instead; use `Num 0` on the left-pad radial to cycle targets.)
- **If anything ever feels wrong, just switch the setting off** — it's fully self-contained, and
  the classic hold behavior returns instantly. Then please tell us in #help what happened.

The **Smart Panel** (R5 → `K`) remains the zero-setup alternative: every mode and value in one
form, no holding, no latching, works regardless of this setting.

## Steam Controller (original) variant

The original Steam Controller has only **two** grip paddles, one stick, and two trackpads (the left
one clickable), so it leans harder on the Panel:

- **Left grip** → `X` (axis modifier)  •  **Right grip** → `K` (Smart Panel)
- **Left trackpad (click radial)** → the numpad numbers radial (as above)
- **Right trackpad** → mouse wheel (live adjust) — it's already the "mouse" pad
- Use a **Mode Shift** (hold left grip) to expose a second radial for the mode keys + `Left Ctrl`
  dismantle

## Known quirks

- **Input prompts may flicker** between controller and keyboard/mouse icons: Smart! binds are
  keyboard keys sent by Steam Input, so Satisfactory sees "mixed" input. Cosmetic only.
- **Trackpad scroll can feel jumpy** (up/down flips) depending on your Mouse Wheel settings —
  try lowering the wheel sensitivity or switching the pad to a scroll-wheel *region* rather than
  swipe. A directional-swipe alternative is being considered.
- Found something else? Tell us in #help — this page evolves with your reports.

## Credits

The community pioneered this — big thanks to **BartekS**, whose `Smart! combos` Steam layout and
experimentation shaped the mapping above, and to **The teacher** for pushing on big-factory Deck
play. If you build a better layout, please share it in #help.

---

_Last updated: 2026-07-14 · Smart! v34.2.1_
