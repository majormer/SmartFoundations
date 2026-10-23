# Grid Scaling

Grid Scaling is the main Smart! feature. It lets one build-gun preview become many buildables.

Use it for:

- Big foundation pads.
- Rows of machines.
- Storage walls.
- Repeated walls, ramps, barriers, and other simple buildables.

![Grid counts and their corresponding foundation preview](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/scaling.png)

## How To Use It

1. Equip a buildable.
2. Aim where you want the first buildable to go.
3. Increase X, Y, or Z until the preview matches the layout you want.
4. Click once to build the whole grid.

Smart! shows the full preview before you commit. Every copied buildable costs materials.

## Panel, wheel and keyboard controls

- **Panel:** open **K → Scaling**, enter the X/Y/Z **Grid** counts and use each axis's direction button. Press **Apply** if immediate placement edits are off.
- **Mouse wheel:** hold `X` for X, `Z` for Y, or `X + Z` for Z, then scroll.
- **Numpad:** use `8/5` for X, `6/4` for Y and `9/3` for Z.

The runtime HUD reports the grid and active adjustment. Player Relative changes how world controls choose direction; the Panel retains grid X/Y/Z. See [Controls](Controls) and [Transforms](Transforms).

## Understanding X, Y, And Z

| Axis | In plain language |
|------|-------------------|
| X | Forward/back from the starting buildable |
| Y | Left/right from the starting buildable |
| Z | Up/down layers |

The exact direction follows the buildable's rotation. If you rotate the hologram, the grid rotates with it.

## Negative Values

Use the Panel's per-axis +/− direction buttons, or the corresponding directional controls, to grow a grid in the opposite direction. This is useful when the starting point is on the far side of a layout and you want the grid to grow back toward you.

For the full lift placement workflow, see [Conveyor Lift Grids](Conveyor-Lift-Grids).

## Selected shapes and special axes

Conveyor Lift Mk.1–Mk.6 can be repeated after selecting the first end, height and top direction. Copies retain the lift's shape and flow; each needs its own suitable native attachment. Native pillars use their accepted tilt for copies and spacing. Pipeline and Hypertube wall supports follow the selected parent rotation, including vertical runs. The arrows and Player Relative controls use the same frame; the Panel keeps X/Y/Z fields.

Beams retain selected length and mode, ladders retain native segment spacing, and Jump Pads retain their selected launch angle. Sign grids offer **Auto / None** supports; use ordinary nudge to position them. Native mounting and placement permissions still apply.

## Large Grids

Smart! is built to scale far. Large previews use lighter representations where supported, and growing a grid adds the new part — everything you already placed stays put instead of the whole preview redrawing. Jumping straight to a very large grid fills in progressively over a moment or two rather than freezing while everything appears at once. (Extremely large grids are still bounded by how much the game engine can hold at one time; Smart! will warn you as you approach that limit. If a placement is too large for the game to accept in one click, Smart! now refuses it with a "Placement too large" message instead of failing silently — your preview stays up, so just scale it down a step or two, place, and continue the grid from there.)

## Supported Buildables

Smart! works best with standard buildables that have a known size and normal placement behavior. Some special Satisfactory holograms are more complicated and may need dedicated Smart! support.

Pole-style buildables scale too — Conveyor Poles, Pipeline Supports, and Pipeline Wall Supports — and Smart! carries their height (and a pipeline support's top angle) across the whole line. When you scale a line of supports, Smart! can also build the belt or pipe run between them; see [Auto-Connect](Auto-Connect).

Single- and double-sided Wall Outlets Mk.1-Mk.3 support grid scaling with a 1 m default interval. Their power previews respect each outlet face's connection capacity.

If a buildable does not scale correctly, try a smaller test first and report the buildable name in an issue.

---

_Last updated: 2026-10-20 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
