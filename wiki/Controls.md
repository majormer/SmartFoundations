# Controls

Smart! uses Satisfactory's Enhanced Input system and registers controls under the mod controls menu. These defaults were verified from the current `MC_Smart_BuildGunBuild` input mapping context in Unreal Editor.

Keybinds can be changed in Satisfactory's controls menu under Mods.

> Screenshot placeholder: Satisfactory Options > Controls > Mods section showing the Smart! keybinds.

## Default Controls

| Key | Action |
|-----|--------|
| `K` | Toggle Smart Settings Form |
| `Num 8` | Increase active value |
| `Num 5` | Decrease active value |
| `Num 6` | Increase Y grid count |
| `Num 4` | Decrease Y grid count |
| `Num 9` | Increase Z grid count |
| `Num 3` | Decrease Z grid count |
| `Mouse Wheel` | Adjust the active Smart mode; with no Smart mode active it is left to vanilla unless a Smart modifier is held |
| `X` | Hold X modifier |
| `Z` | Hold Y modifier |
| `X + Z` | Hold both modifiers to adjust Z through the unified scale path |
| `;` | Hold Spacing mode |
| `I` | Hold Steps mode |
| `Y` | Hold Stagger mode |
| `,` | Hold Rotation mode |
| `Num 0` | Cycle the active mode's axis; double-tap with no mode active to toggle Smart Auto-Connect and Extend for the session |
| `U` | Recipe mode / clear active recipe selection |
| `Num 1` | Toggle directional arrows |

## Scaling Behavior

With no modal mode active:

- `Num 8` / `Num 5` adjust X scaling.
- `Num 6` / `Num 4` adjust Y scaling.
- `Num 9` / `Num 3` adjust Z scaling.
- Holding `X` makes the unified increase/decrease or mouse wheel adjust X.
- Holding `Z` makes the unified increase/decrease or mouse wheel adjust Y.
- Holding `X + Z` makes the unified increase/decrease or mouse wheel adjust Z.

## Modal Modes

Modal modes temporarily route `Num 8`, `Num 5`, mouse wheel, and `Num 0` to a feature instead of default scaling.

| Mode key | Mode | `Num 8` / `Num 5` / wheel | `Num 0` |
|----------|------|----------------------------|---------|
| `;` | Spacing | Adjust selected spacing axis | Cycle X, Y, Z |
| `I` | Steps | Adjust selected step axis | Toggle X/Y |
| `Y` | Stagger | Adjust selected stagger axis | Cycle X, Y, ZX, ZY |
| `,` | Rotation | Adjust Z rotation | Reserved for future axes; current code is Z only |
| `U` | Recipe | Cycle recipe selection | Clear recipe selection |

## Verified From

- Unreal Editor asset: `/SmartFoundations/SmartFoundations/Input/Contexts/MC_Smart_BuildGunBuild`
- Unreal Editor assets under `/SmartFoundations/SmartFoundations/Input/Actions/`
- `Source/SmartFoundations/Private/Input/SFInputRegistry.cpp`
- `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`

