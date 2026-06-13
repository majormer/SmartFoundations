# Grid Scaling

Grid Scaling is the main Smart! feature. It lets one build-gun preview become many buildables.

Use it for:

- Big foundation pads.
- Rows of machines.
- Storage walls.
- Repeated walls, ramps, barriers, and other simple buildables.

> Screenshot placeholder: `5 x 3 x 1` foundation grid preview with HUD counters visible.

## How To Use It

1. Equip a buildable.
2. Aim where you want the first buildable to go.
3. Increase X, Y, or Z until the preview matches the layout you want.
4. Click once to build the whole grid.

Smart! shows the full preview before you commit. Every copied buildable costs materials.

## Understanding X, Y, And Z

| Axis | In plain language |
|------|-------------------|
| X | Forward/back from the starting buildable |
| Y | Left/right from the starting buildable |
| Z | Up/down layers |

The exact direction follows the buildable's rotation. If you rotate the hologram, the grid rotates with it.

## Negative Values

Smart! can grow a grid in the opposite direction by using negative values. This is useful when the starting point is on the far side of a layout and you want the grid to grow back toward you.

## Supported Buildables

Smart! works best with standard buildables that have a known size and normal placement behavior. Some special Satisfactory holograms are more complicated and may need dedicated Smart! support.

Pole-style buildables scale too — Conveyor Poles, Pipeline Supports, and Pipeline Wall Supports — and Smart! carries their height (and a pipeline support's top angle) across the whole line. When you scale a line of supports, Smart! can also build the belt or pipe run between them; see [Auto-Connect](Auto-Connect).

If a buildable does not scale correctly, try a smaller test first and report the buildable name in an issue.
