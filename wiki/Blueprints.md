# Smart! Blueprints

Smart! Blueprints lets you scale one of your **own** blueprints into a grid and connects belts, pipes, and eligible power sockets between the copies for you.

Design a blueprint whose conveyors or pipes reach its edge, then stamp out a 4x2, a 3x3, or any grid. Every seam between neighboring copies is wired in a single placement, and each copy stays a real, independent blueprint you can dismantle on its own.

> Screenshot placeholder: a single blueprint beside a 3x2 Smart! grid of it with the seams auto-connected.

## When To Use

Use Smart! Blueprints when you have a blueprint that is meant to tile - a production module, a manifold segment, a wall or floor section with belts or pipes running through it - and you want a whole field of it wired together, not a row of disconnected copies.

## Requirements

- The blueprint's belts or pipes must **reach the edge** of the blueprint on the sides you want to connect. Smart! joins copies where their edge connections line up.
- You are scaling your own blueprint through the vanilla Blueprint designer/build gun; Smart! adds the grid and the seam connections on top.

## Basic Use

1. Build and save a blueprint whose conveyors or pipes end at its edge.
2. Equip that blueprint in the build gun.
3. Use Smart!'s grid controls (or the Smart Panel) to scale it into the grid you want - for example 4x2 or 3x3.
4. Check the preview: each copy is placed on the grid and the belts/pipes between adjacent copies are previewed as connected.
5. Build once. Every seam is placed in that single action.

## What Gets Connected

- **Two-dimensional seams.** Copies connect both along a row and across rows - the true 2D connections the game's own blueprint auto-connect cannot make on its own.
- **Vertical pipe stacking.** Pipes that stack between stacked copies are joined as well.
- **Only where edges align.** A seam is made where two copies present matching edge connections. Edges that do not line up are left open, as you would expect.

## Power Between Copies

Enable **Blueprint Seam Auto-Connect** and **Power Auto-Connect** to link matching power-pole sockets between copies. Auto mode creates a continuous chain through the grid, including vertical layers. Existing internal cables count toward each socket's capacity; the two faces of a wall outlet keep their own slots. Cables beyond the normal game range are omitted.

Separate internal circuits stay separate. Check the preview for the intended connections and price before building.

## Default Spacing

Set separate X, Y, and Z blueprint-spacing defaults under **Building Behavior**. Each starts at 1 m; use 0 for flush tiling. Change spacing in the Smart Panel for the current blueprint session. Repeated placements retain that override; changing blueprints or holstering restores the configured defaults.

## Large Blueprints

Smart placements retain the safe construction-payload limit in single-player, listen-server, and dedicated-server sessions. A refused placement keeps its preview so you can reduce the grid. To place a large vanilla blueprint normally, disable [Smart Assistance](Smart-Assistance), then select the blueprint again.

## Each Copy Stays Independent

Every placed copy remains a normal, independent blueprint. You can dismantle one without touching its neighbors - Smart! only adds the connections between them, it does not fuse them into one object.

## Settings

The **Blueprint Auto-Connect** setting (its own settings section) controls whether Smart! wires the seams between scaled blueprint copies. It is independent of the regular Belt and Pipe Auto-Connect, so you can keep blueprint seams connecting even when nearby auto-connect is off. See [Settings Reference](Settings-Reference).

## Multiplayer

Smart! Blueprints works in single-player and in multiplayer on dedicated servers (Windows and Linux).

## Related

- [Grid Scaling](Grid-Scaling) - how Smart! scales anything into a grid.
- [Auto-Connect](Auto-Connect) - connecting belts, pipes, and power for non-blueprint builds.
- [Settings Reference](Settings-Reference) - the Blueprint Auto-Connect switch.

---

_Last updated: 2026-10-06 · Smart! v34.4.0_
