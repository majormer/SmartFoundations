# Transforms

Transforms change the shape of a scaled grid. They are what turn a plain row or rectangle into something with gaps, height changes, offsets, or arcs.

> Screenshot placeholder: one comparison image showing spacing, steps, stagger, and rotation.

## Spacing

Spacing adds extra distance between each copy.

Use it for:

- Machine rows that need belt space.
- Foundations with deliberate gaps.
- Cleaner production layouts.

Controls:

- Hold `;`.
- Use `Num 8`, `Num 5`, or mouse wheel.
- Press `Num 0` to switch between X, Y, and Z spacing.

## Steps

Steps raise or lower each row or column.

Use it for:

- Stair-step foundation layouts.
- Terraced factories.
- Ramps and decorative structure work.

Controls:

- Hold `I`.
- Use `Num 8`, `Num 5`, or mouse wheel.
- Press `Num 0` to switch between X and Y stepping.

## Stagger

Stagger offsets rows or layers.

Use it for:

- Diagonal-looking patterns.
- Offset machine rows.
- Leaning or angled multi-layer layouts.

Controls:

- Hold `Y`.
- Use `Num 8`, `Num 5`, or mouse wheel.
- Press `Num 0` to cycle X, Y, ZX, and ZY.

## Rotation

Rotation places copies around a horizontal arc — each copy is yawed a little more than the last.

Use it for:

- Curved walls.
- Circular foundation patterns.
- Decorative arcs.

Controls:

- Hold `,`.
- Use `Num 8`, `Num 5`, or mouse wheel to set the per-copy angle.
- Press `Num 0` (or use the **X/Y selector on the Rotation row of the Smart! Panel**) to choose which grid axis the rotation builds up along:
  - **X** (default) — the rotation accumulates along the columns, so the **run** curves as it extends.
  - **Y** — the rotation accumulates along the rows, so the **rows** fan out around the vertical instead.

Either way it stays a flat, upright arc — nothing ever tilts or flips. (Smart! rotates around the vertical/yaw axis only; vertical arch rotation is not currently an active feature.)
