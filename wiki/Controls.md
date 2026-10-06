# Controls

Smart! controls are available in **Options > Keybindings > "Smart! Mod Controls"**, so you can rebind them. (Smart! Camera's keys are in the adjacent **"Smart! Camera Controls"** section.)

> Screenshot placeholder: Satisfactory Options > Keybindings > "Smart! Mod Controls" showing Smart! keybinds.

## Default Keys

| Key | What it does |
|-----|--------------|
| `K` | Open or close the Smart Panel |
| `Num Decimal` | Toggle Smart building assistance for this session |
| `Num 8` | Increase the active value |
| `Num 5` | Decrease the active value |
| `Num 6` | Increase Y grid count |
| `Num 4` | Decrease Y grid count |
| `Num 9` | Increase Z grid count |
| `Num 3` | Decrease Z grid count |
| `X` | Hold to adjust X with the shared controls or mouse wheel |
| `Z` | Hold to adjust Y with the shared controls or mouse wheel |
| `X + Z` | Hold both to adjust Z with the shared controls or mouse wheel |
| `;` | Hold Spacing mode |
| `I` | Hold Steps mode |
| `Y` | Hold Stagger mode |
| `,` | Hold Rotation mode |
| `Num 0` | Cycle the active mode's axis |
| `U` | Recipe mode on a factory (clears the selected recipe); Auto-Connect settings on other buildables |
| `Num 1` | Toggle direction arrows |
| `Mouse Wheel` | Adjust the active Smart mode, or vanilla rotate when no Smart mode is active |

## Temporarily Disable Smart

Press **Num Decimal** to toggle Smart building assistance for your current session. Disabling cancels the active Smart preview. Select a recipe again to place normally, including vanilla blueprints too large for a Smart placement. Press the same key to re-enable assistance.

Your saved mod settings stay intact. The choice survives recipe changes and holstering, and each multiplayer player controls their own assistance. Rebind **Toggle Smart (Session)** in Options if your keyboard has no numpad. This is separate from the double-tap Num 0 shortcut below.

See [Smart Assistance](Smart-Assistance) for the full workflow.

## Basic Scaling

When you are just scaling a grid:

- `Num 8` / `Num 5` changes X.
- `Num 6` / `Num 4` changes Y.
- `Num 9` / `Num 3` changes Z.

For mouse wheel scaling, hold `X`, `Z`, or both.

## Mode Keys

Some keys are held to temporarily change what the increase/decrease controls do.

| Hold | Mode | Use it for |
|------|------|------------|
| `;` | Spacing | Adding or removing gaps |
| `I` | Steps | Raising each row or column |
| `Y` | Stagger | Offsetting rows or layers |
| `,` | Rotation | Making a horizontal arc |
| `U` | Recipe | Choosing or clearing a recipe |

While holding one of these modes, use `Num 8`, `Num 5`, or mouse wheel to adjust the value. Press `Num 0` — or quickly **re-tap the mode key you're holding** — to switch which axis the mode is editing, without reaching for the numpad.

## Stagger: Stack and Flat

Stagger has four patterns, grouped into two families:

- **Stack** — a vertical pile that leans as it rises.
- **Flat** — a row that drifts sideways as it runs.

`Num 0` toggles between Stack and Flat, re-tapping the Stagger key (`Y`) switches the direction within the family, and `Num 9` / `Num 3` jump straight to Stack or Flat.

## Player Relative Controls (optional)

Turn on **Player Relative Controls** in the mod settings (Building Behavior) and building follows the direction you're looking instead of fixed compass axes:

- The **mouse wheel** grows the build toward wherever you're facing.
- The numpad becomes a **compass**: `Num 8` / `Num 5` away from and toward you, `Num 6` / `Num 4` to your right and left, `Num 9` / `Num 3` up and down.
- While you hold a transform mode, the HUD names what the wheel is driving — **Forward**, **Lateral**, or **Vertical** — and a quick re-tap of the mode key switches between them.

It's off by default, so your classic controls are unchanged until you switch it on. The Smart Panel always stays on fixed X/Y/Z.

## Repeat a recent restore

Hold `U` and press `Num 9` to step through the Smart Restore presets you've applied this session (newest first), or `Num 3` to step back. Each one arms just like pressing **Apply** in the Smart Panel — so placing the same restore over and over is just aim, click, `U` + `Num 9`, aim, click.

## Double-Tap Num 0

Double-tapping `Num 0` with no mode key held toggles Smart Auto-Connect and Extend for the current session. This is useful when Smart! is trying to help but you want one normal placement.

---

_Last updated: 2026-10-06 · Smart! v34.4.0_
