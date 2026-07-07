# Smart! Blueprints

Smart! Blueprints lets you take one of **your own blueprints** and stamp out a whole grid of copies at once — and Smart! automatically runs the belts and pipes between the copies for you.

Design a blueprint whose conveyors or pipes reach its edge, then scale it like any other building. Every seam between neighbouring copies gets wired in a single placement.

> Screenshot placeholder: a 3x3 grid of a fluid blueprint with pipes connecting every copy.

## How to use it

1. Open your blueprint from the blueprint menu, the same as always.
2. Hold a Smart! modifier and scroll to scale it — **X, Y, and Z**, with **spacing** and **steps**, just like scaling a foundation.
3. As the grid grows, Smart! previews the connecting belts and pipes at every seam, and adds their cost to the build.
4. Click to place. The whole grid builds, wired together.

When you pick up a blueprint, Smart! sets **spacing to 1 m on every axis** automatically, so there is room for the connecting runs to exist. You can change it — set spacing back to 0 if you want the copies flush with no connections.

## What connects

- **Belts** connect side-to-side (along X and Y).
- **Pipes** connect side-to-side **and vertically** — a blueprint with pipe ports on its roof and floor stacks into a self-connected tower when you scale it up (Z).
- The connections only happen where your blueprint's belts or pipes actually **reach its edge**. Design the blueprint with its conveyors/pipes running up to the border on the sides you want to join.

Because Smart! wires the copies directly, an **interior copy connects on all four sides at once** — a two-dimensional grid of connected blueprints, which the game's built-in blueprint auto-connect can't do on its own.

> Screenshot placeholder: interior copy of a blueprint with belts leaving all four edges.

## Every copy is a real blueprint

Each copy in the grid is a genuine, independent blueprint instance — not a merged blob. Dismantle one and you remove just that copy; the rest of the grid, and the belts and pipes between them, stay in place. This works the same in single-player and on a server.

## If connections come out missing

If a grid builds but some connections are absent, the Smart! HUD tells you why while you are aiming — most often **"too close"**, which means the copies are packed too tightly for a belt or pipe to fit between the mating ports. **Widen the spacing a little** and they appear. Other reasons ("too steep", "invalid shape", "too far") mean the game itself wouldn't let you place that conduit by hand at that geometry — nothing is broken, that one connection is just skipped.

## Multiplayer

Smart! Blueprints works in multiplayer, including on dedicated servers. One thing to know: a single blueprint-grid placement on a server has a size ceiling (roughly a couple of thousand buildings, or a few hundred connecting belts/pipes, whichever comes first). If you scale past it, Smart! refuses the placement and the HUD tells you to **build in smaller sections** — the grid stays live so you can scale down and place it in parts. Single-player has no such limit.

## Current limits

- **Vertical belts between stacked copies are not connected yet** — that needs conveyor lifts, which Smart! doesn't place automatically. Vertical **pipes** do connect. Horizontal belts connect normally.
- **Flush grids (spacing 0)** don't get connections — there's no room for a conduit. Use a little spacing if you want the copies wired.
- **Rotation and stagger** on a blueprint grid don't drive the auto-connect (they still scale the grid).

## See also

- [Auto-Connect](Auto-Connect) — the belt/pipe/power auto-connect Smart! Blueprints builds on.
- [Grid Scaling](Grid-Scaling) and [Transforms](Transforms) — the scaling controls.
- [Compatibility and Multiplayer](Compatibility-and-Multiplayer).
