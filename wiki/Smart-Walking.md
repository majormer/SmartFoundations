# Smart Walking

Walking creates a connected route one segment at a time. Use it for conveyor buses, pipes, Hypertubes, railway tracks and Vehicle Paths. It previews the route before building and charges normal materials for new construction.

![Three-lane pipe walk with a turn and rise on a clear platform](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/walking-bus.png)

## Start a fresh route

1. Equip a **Stackable Conveyor Pole**, **Stackable Pipeline Support** or **Stackable Hypertube Support** for that family. For rails or Vehicle Paths, select the corresponding native buildable.
2. Aim at a valid starting placement, then open **K → Walking**. Route-only buildables open this task directly.
3. Add and steer segments. Review the whole preview, then fire the build gun to construct it.

## Resume an existing bus

Holding a pipe or belt alone is not the entry point. **Equip the matching stackable support**, even when you want to continue from an existing pipe or belt.

1. Close the Panel and equip that support.
2. Aim at an **open endpoint** of the existing line, or its compatible stackable support at the endpoint.
3. Open **K → Walking** without placing a new starting support.
4. Check the **Resume** summary: lanes, levels and spacing should describe the bus you intended to continue.
5. Advance the new route and inspect the connections before building.

Smart can recognize a regular cross-section such as four side-by-side lines, a 3×3 bus, or a compatible manually placed grid. The start must be horizontal and the endpoints must be open and unambiguous. Wall/floor attachments are excluded. An occupied endpoint or irregular arrangement is not a valid resume target. The continuation can climb after the horizontal start.

Existing lines and supports remain in place; only new construction is charged.

## Add lanes and work backward

Increase the resumed bus's lane or level count to add more lines. For example, resume four lanes and grow to six: the original four stay intact, while Smart supplies the missing starting supports and new route pieces for the added lanes. Expansion is not limited to two additional lanes.

After building, resume from the open start of an added lane and walk backward toward its source. Conveyor flow is preserved per lane, including buses whose belts run in opposite directions. Inspect the flow arrows; reversing the direction you walk must not reverse an existing belt's transport direction.

## Steer and edit

| Control | Walking operation |
|---|---|
| Advance / Scale X | Add a segment; decrease to remove the last segment |
| Scale Y / Z | Adjust bus lanes / levels where supported |
| Spacing | Segment advance distance |
| Rotation | Turn |
| Steps | Rise or descend |
| Stagger | Shift sideways |

See [Controls](Controls) for defaults. Walking is **segment-relative**; Player Relative scaling does not change these route meanings. Smart Camera can follow the leading segment while you remain at the start.

The Walking tab lists each segment's **Advance, Turn, Rise, Shift** and **Preview exit** heading. Edit an earlier segment to reshape the rest of the preview. With **Apply segment edits immediately** off, the visible values remain staged until Apply. Connection options such as tier, routing and supported flow/style preferences are live and labeled separately.

![Walking segment table, preview heading and live connection options](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/walking-segments.png)

## Family rules

- Belts keep native flow, span and slope restrictions. Use lifts separately where a belt cannot make a climb.
- Pipes and Hypertubes follow their own native routing restrictions. They do not have conveyor-style item flow direction.
- Railway curves use native-style routing and the **100 m maximum applies to the full curved track**, not just the straight distance between endpoints. Shorten a turn or split it into more segments when needed. Separate lines retain separate buffer stops.
- Vehicle Paths are directed routes. Join the appropriate free endpoint and use the correct travel direction. To test a finished route with a vehicle, provide the native station connections, fuel, power and schedule it needs; a visible path alone is not a running transport service.

## Build or cancel

Fire to commit the new route. Missing materials, stale endpoints or a native-invalid segment prevent construction; correct the preview before trying again. Cancel or holster to discard an unbuilt route. Switching tasks warns when it would lose that route.

Walking previews locally and construction is checked by the authoritative game in multiplayer. Existing route objects remain normal Satisfactory buildings. See [Compatibility and Multiplayer](Compatibility-and-Multiplayer).

## Transport preview examples

![A Railway Walking curve and its approach on a clear platform.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/rail-walking.png)

A Railway Walking curve and its approach on a clear platform.

![An elevated side view separates the straight Vehicle Path segment from its curved continuation.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/vehicle-walking.png)

An elevated side view separates the straight Vehicle Path segment from its curved continuation.

---

_Last updated: 2026-10-20 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
