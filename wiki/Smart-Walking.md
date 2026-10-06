# Smart Walking

Smart Walking lays one connected run — belts, pipes, or hyper tubes on stackable poles and supports — that turns corners, climbs slopes, and routes exactly where you want it. Instead of a straight, uniform grid, you steer the run forward one segment at a time and build it all in a single click.

> Screenshot placeholder: a belt run walking around a corner and up a slope, with the Smart Walking panel open.

## When To Use Smart Walking

Use it when a straight run will not do:

- Route a belt bus around a corner or a cliff.
- Climb a slope with a continuous pipe run.
- Lay a long conveyor line that bends to follow your factory's layout.
- Reach a far-off destination while you stay put and watch through the Smart! Camera.
- Run a hyper tube up a steep climb to a distant platform or outpost.

## How To Start

1. Hold a **stackable conveyor pole** (for belts), a **stackable pipeline support** (for pipes), or a **stackable hyper tube support** (for hyper tubes) in the build gun.
2. Press `K` to open the Smart! Panel.
3. Click **Smart Walking**. (The button only appears for buildables that can be walked.)

The first segment starts at the pole you are aiming, and the run grows forward from there.

## Steering The Run

Smart Walking reuses the controls you already know from grid scaling — they just apply to the **active segment** (the leading edge) instead of a whole grid. Everything behind the active segment stays locked.

| To do this | Use the control |
|---|---|
| Advance a segment / back up | Mouse wheel (up advances, down undoes the last segment), `Num 8` / `Num 5`, or your Scale-X keybind |
| Turn a corner | **Rotation** |
| Set the gap to the next pole | **Spacing** |
| Climb or descend a slope | **Steps** (rise) |
| Shift sideways | **Stagger** |

See [Controls](Controls) for the exact keys. You stay free to move the whole time — walk alongside the run, or stand still and steer it from a distance using the [Smart Camera](Smart-Camera) picture-in-picture, which follows the head of the run.

## The Smart Walking Panel

Press `K` while walking to open the Smart Walking panel — an editable list of every segment:

- Each row shows a segment's advance, turn, rise, and shift, plus its **exit heading** as a compass bearing.
- Edit any segment and the run rebuilds from there forward.
- Choose the run's **tier**, **routing**, belt **flow direction**, or pipe **style** for the whole path.

## Belts, Pipes, And Hyper Tubes

- **Belts** carry a tier, a routing mode, and a flow direction (forward or backward).
- **Pipes** carry a tier and a routing mode, and flow either way once connected, so they have no direction.
- **Hyper tubes** carry a routing mode and, like pipes, have no direction. Because a hyper tube doesn't sag, its run can climb far more steeply than a belt, and there is a single hyper tube buildable, so there is no tier to choose.

The whole run uses one tier and one routing — mixing belt tiers on a single line would just bottleneck it at the slowest belt. Add splitters, mergers, or valves to the finished run the normal way.

## Finishing The Run

- **Commit:** fire the build gun (left mouse) to build the entire run at once. You pay the normal material cost for every pole and belt or pipe.
- **Can't afford it?** The preview turns red and the run will not commit until you can.
- **Too long or too steep?** A segment that overshoots the maximum span, or (for belts) climbs too steeply, is flagged and blocks the commit — shorten it or split the climb.
- **Cancel:** press `Escape` or put the build gun away to discard the run.

## Multiplayer

Smart Walking works in single-player and as a client on a dedicated server (Windows and Linux). The whole run previews on your machine and builds in one server-checked action, charging the normal cost. See [Compatibility and Multiplayer](Compatibility-and-Multiplayer).

## Tips

- Lay the straight sections first, then turn — each turn pivots the heading, and the next advance follows the new direction.
- Back up removes segments from the end. There is no partial undo of a turn mid-run, so back up to the point you want and re-steer.
- Use the [Smart Camera](Smart-Camera) to route long runs without walking the whole way.

---

_Last updated: 2026-06-26 · Smart! v33.1.0_
