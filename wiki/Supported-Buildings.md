# Supported Buildings

Smart! supports many buildables. Most are placed by **scaling them into a grid**; some multi-step builds — belt, pipe, and hypertube runs, and power — are **built for you through dedicated features** instead. Coverage keeps growing, so if something isn't handled yet, it may be added in a later release.

## Scaled directly into a grid

If a building places with **one click** in vanilla, Smart! can usually scale it into a grid:

- Foundations, ramps, walls, barriers, railings, walkways, and catwalks.
- Production buildings such as constructors, assemblers, manufacturers, smelters, foundries, refineries, packagers, blenders, converters, quantum encoders, and particle accelerators.
- Storage containers and fluid buffers.
- Power poles, power towers, switches, generators, and power storage.
- Splitters, mergers, smart splitters, programmable splitters, priority mergers, stackable supports, pipeline junctions, pumps, valves, wall holes, and floor holes.
- Many signs, billboards, lights, and factory organization pieces.
- Your own **blueprints**, scaled into a grid with the belt and pipe seams between copies auto-connected. See [Smart! Blueprints](Blueprints).

## Built through other features

These are multi-step or drag-based builds. You do not scale them as a direct grid item, but Smart! still builds them for you:

- **Belts, pipes, lifts, and power lines** — created by [Auto-Connect](Auto-Connect) between what you place, or copied along with a module by [Extend](Extend).
- **Belt, pipe, and hypertube runs** that turn corners, climb slopes, and route to a destination — laid as one connected run by [Smart Walking](Smart-Walking).

## Not currently supported

- Railways and train signals.

Smart!'s coverage grows over time, so this list can change between releases.

## Restricted buildings

A few buildables are intentionally restricted because their placement depends on world state that is not safe to clone like a normal factory building:

- **Resource extractors** (miners) are tied to fixed world resource nodes.
- **Water extractors** can scale in X/Y and spacing when the child placements still validate over water, but vertical scaling is disabled.
- Other special buildables may be restricted when placement depends on snap points, world state, or vanilla behaviors that cannot be safely repeated.

See the [FAQ](FAQ) for more on why miners and some special buildables are unsupported.

---

_Last updated: 2026-07-14 · Smart! v34.2.1_
