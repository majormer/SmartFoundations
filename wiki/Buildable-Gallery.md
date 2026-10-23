# Buildable Gallery

These examples focus on buildables with distinct placement or selected-state behavior. Ordinary factories that share the same controls do not need a separate example for every machine or tier. See [Supported Buildings](Supported-Buildings) for scope and [Feature Gallery](Feature-Gallery) for complete workflows.

## Conveyor lifts

[Conveyor Lift Grids](Conveyor-Lift-Grids) repeat the chosen native height and direction.

![Conveyor lifts retain the selected height and direction in the grid.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/lift-grid.png)

Conveyor lifts retain the selected height and direction in the grid.

## Wall supports

Native Pipeline and Hypertube wall supports expose 45° rotation steps. Their grid Y follows the selected parent rotation, including vertical runs. Panel axes remain absolute X/Y/Z; Player Relative controls map the wheel and keys to those axes. See [Controls](Controls) and [Auto-Connect](Auto-Connect).

![Pipeline wall supports at 45°, with the connecting pipe following their orientation.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/pipe-wall-rotation.png)

Pipeline wall supports at 45°, with the connecting pipe following their orientation.

![Hypertube wall supports and their connection preview at 45°.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/hypertube-wall-rotation.png)

Hypertube wall supports and their connection preview at 45°.

## Beams, ladders and Jump Pads

Select the native length, height or launch angle first. Copies retain that chosen state.

![Selected beam length repeated across the grid.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/beam-length.png)

Selected beam length repeated across the grid.

![The selected ladder height is repeated, rather than reset for each copy.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/ladder-height.png)

The selected ladder height is repeated, rather than reset for each copy.

![Jump Pad copies retain the chosen launch angle; the trajectory preview remains visible.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/jump-pad-angle.png)

Jump Pad copies retain the chosen launch angle; the trajectory preview remains visible.

## Signs

The same X/Z array is viewed face-on in both images. The Sign supports setting changes the applicable native supports without changing the grid.

![The same planar X/Z sign grid with Sign supports set to Auto.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/sign-supports-auto.png)

The same planar X/Z sign grid with Sign supports set to Auto.

![The same sign grid and viewpoint with Sign supports set to None.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/sign-supports-none.png)

The same sign grid and viewpoint with Sign supports set to None.

## Native pillars

The grid follows the selected native pillar frame. Panel axes are the buildable frame; wheel/key feedback must agree with it. Externally tilted Blueprint contents are a separate case.

![Native pillar copies retain the selected compound frame. This is not a claim of a longitudinal slope.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/native-pillar-frame.png)

Native pillar copies retain the selected compound frame. This is not a claim of a longitudinal slope.

## Fans, Vents and roof corners

A raised viewpoint makes floor geometry and repetition visible. Native mounting restrictions remain in force.

![A raised view makes the four floor-mounted Fans visible.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/fan-grid.png)

A raised view makes the four floor-mounted Fans visible.

![Four native Vents seen from above beside the placement controls.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/vent-grid.png)

Four native Vents seen from above beside the placement controls.

![Inner roof corners repeated with their selected native shape.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/roof-corners.png)

Inner roof corners repeated with their selected native shape.

### Wall-mounted Fans and Vents

Select a valid wall mounting first, then adjust the grid and spacing in **K → Scaling** or with the wheel/numpad controls. The HUD reports the same counts and spacing.

![Two Large Fan previews mounted on a wall, with 1 m of added Y spacing. Smart! repeats the selected mounting orientation.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/fan-wall-grid.png)

Two Large Fan previews mounted on a wall, with 1 m of added Y spacing. Smart! repeats the selected mounting orientation.

![Two wall-mounted Large Vent previews with the same 1 m spacing. Native placement restrictions still apply.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/vent-wall-grid.png)

Two wall-mounted Large Vent previews with the same 1 m spacing. Native placement restrictions still apply.

## Floor holes

Place a Conveyor Lift Floor Hole on a valid foundation surface, then use **K → Scaling** or the normal grid controls to repeat it. Inspect the holes from above so the destinations are visible. This repeats the floor holes; it does not create a connected lift through them.

![Two Conveyor Lift Floor Hole previews repeat across the foundation surface. The HUD shows a 2×1×1 grid; the holes are not built yet.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/floor-hole-grid.png)

Two Conveyor Lift Floor Hole previews repeat across the foundation surface. The HUD shows a 2×1×1 grid; the holes are not built yet.

## Personnel Elevator

Smart! can repeat a new Elevator assembly using its selected height and floor-stop arrangement, so you do not have to set up each copy separately. When you later lengthen an existing Smart-built shaft using the game's normal controls, it stays in its original Smart Dismantle group, including after saving and loading.

Floor stops are part of the assembly; they cannot be scaled independently. Use the native controls to lengthen a shaft. Normal placement rules, material costs and power requirements apply.

![A 2×1×1 Personnel Elevator preview repeats the selected 5 m assembly. The Scaling Panel controls the grid and spacing.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/elevator-selected-height.png)

A 2×1×1 Personnel Elevator preview repeats the selected 5 m assembly. The Scaling Panel controls the grid and spacing.

## Water Extractors

See [Water Extractors](Water-Extractors) for horizontal scaling, Extend and Restore. Every child must pass the water-depth and terrain checks; copies preserve captured connected geometry.

![A horizontal two-extractor Smart preview beside its Scaling controls, with vertical repetition disabled.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/water-horizontal-grid.png)

This **2×1×1** preview repeats two Water Extractors horizontally in actual water. Both destinations passed placement validation; **Grid Z** and **Spacing Z** are disabled. The blue previews are unbuilt, and no pipe manifold is shown. Power daisy-chaining is enabled in this example.

![Two horizontally repeated Water Extractors with short Auto-Connect branches into one shared pipeline.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/water-connected-manifold.png)

Two horizontally repeated Water Extractors with short Auto-Connect branches into one shared pipeline.

## FICSMAS

These families require their normal native seasonal recipes. Smart does not make the event available all year. The examples show Candy Canes, Wreaths, Snowmen, Snow Dispensers, Giant Trees and Snow Cannons; selected native shapes, material costs and placement restrictions still apply.

![Seasonal Candy Cane previews.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/candy-canes.png)

Seasonal Candy Cane previews.

![Wreath previews seen from above so their ring geometry is visible.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/wreath-grid.png)

Wreath previews seen from above so their ring geometry is visible.

![Seasonal Snowman previews.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/snowmen.png)

Seasonal Snowman previews.

![A raised view of seasonal Snow Dispenser previews.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/snow-dispenser-grid.png)

A raised view of seasonal Snow Dispenser previews.

![The full native seasonal tree. Yellow is a native clearance warning, not a different tree variant.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/giant-tree.png)

The full native seasonal tree. Yellow is a native clearance warning, not a different tree variant.

![Seasonal Snow Cannon previews.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/snow-cannons.png)

Seasonal Snow Cannon previews.

---

_Last updated: 2026-10-22 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
