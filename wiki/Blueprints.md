# Smart! Blueprints

Repeat a saved Blueprint in a Smart grid and connect compatible endpoints between neighboring copies. Every copy remains a separate native Blueprint; Smart adds the connections between them at normal construction cost.

## Design the connections you intend

Leave **open tails** at the edges you want to connect: belt, pipe, Hypertube or railway ends, or directed Vehicle Path endpoints. A machine or distributor port by itself is not a request to connect every matching port nearby. Preserve clear, compatible endpoints and check the preview.

1. Save the repeatable module in the native Blueprint Designer.
2. Select that Blueprint in the build gun.
3. Increase the grid and adjust spacing.
4. Inspect the seam previews and total material cost, then build.

## Supported seams

| Family | What to check |
|---|---|
| Belts | Compatible open tails and item flow; native span, curve and slope limits |
| Pipes | Open matching ends; native routing and length limits |
| Hypertubes | Compatible open ends; a continuous route and powered entrance for travel |
| Railway tracks | Free matching endpoints; native-valid curves with a full-spline 100 m limit |
| Vehicle Paths | Compatible directed free ends; preserve travel direction and native path validity |
| Power | Matching eligible sockets with capacity; enable Power Auto-Connect too |

Connections can join neighboring rows and columns. Vertical pipe/Hypertube endpoints can join compatible stacked copies. Existing internal connections are preserved; endpoints that are occupied, ambiguous or invalid are not permission to invent a route.

## Stack Blueprints using lift tails

Without eligible open conveyor-lift tails, stacked copies use the configured default **Z spacing**.

With lift tails, Smart looks for compatible unresolved lift endpoints facing in opposing directions with compatible conveyor flow. It attempts to align stacked copies using **one shared Z spacing for the entire grid**. A connecting belt can slope slightly when the native belt rules allow it; the endpoints do not have to form a perfectly level straight line.

This is not a separate Z offset for each lane, and it does not connect every machine port. Conflicting or ambiguous tails cannot define several different layer heights. Check the selected spacing and seam preview rather than assuming every tail can be joined. Lift placement, belt slope, length, unlocks and materials retain native restrictions.

## Spacing and settings

Under **Building Behavior**, set separate **Blueprint Default X/Y/Z Spacing (m)** values. Defaults are 1 m; 0 permits flush tiling. Current Panel spacing overrides last for the Blueprint session; changing Blueprint or holstering restores configured defaults. Lift-tail alignment can determine a shared Z interval for eligible stacked layouts as described above.

**Blueprint Seam Auto-Connect** is independent of ordinary nearby Belt/Pipe Auto-Connect. Power seams additionally require **Power Auto-Connect**. Internal wires count toward socket capacity, and each wall-outlet face has its own slots. Separate internal circuits remain separate.

## Ownership, limits and multiplayer

Each copy stays independently dismantlable. Within a native Designer, permitted Smart construction belongs to that Designer; Smart does not grant permission to place a building the Designer rejects.

The safe construction-payload limit also applies to Blueprint grids. A refused oversized placement keeps its preview: reduce the grid and build in sections. For a large vanilla Blueprint placement, disable [Smart Assistance](Smart-Assistance) and select it again.

Multiplayer uses authoritative validation and normal costs. A changed endpoint or insufficient resources can invalidate a previously valid preview. See [Auto-Connect](Auto-Connect) and [Compatibility and Multiplayer](Compatibility-and-Multiplayer).

## Stacked lift-tail example


![Two factory Blueprint floors, each containing a Constructor and exterior lifts. Smart! connects the lower floor’s output lift to the upper floor’s input lift.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/blueprint-factory-floors.png)

Two factory Blueprint floors, each containing a Constructor and exterior lifts. Smart! connects the lower floor’s output lift to the upper floor’s input lift.

Place the lifts outside the factory floor so their shafts and housings remain clear of the repeated buildings. Leave compatible input and output tails open at the floor boundaries. Smart! can use those tails to choose one shared floor interval and connect the copies. This image demonstrates the connection layout; production recipes and power must still be configured.

## Railway and Vehicle Path seams

![In the foreground, Smart! joins two Railway Blueprint modules with the middle track segment. Both outer ends and both joins are visible.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/blueprint-rail-seam.png)

In the foreground, Smart! joins two Railway Blueprint modules with the middle track segment. Both outer ends and both joins are visible.

Leave a free Railway endpoint at each intended boundary. The connecting track must meet the native curve and length limits. A visually aligned track is not enough: check that the preview actually joins both ends.

![Directed Vehicle Path spans join neighboring Blueprint copies. The arrows make the opposite travel directions of the two parallel lanes visible.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/blueprint-vehicle-seam.png)

Directed Vehicle Path spans join neighboring Blueprint copies. The arrows make the opposite travel directions of the two parallel lanes visible.

Equip a Vehicle Path to see its direction arrows. Join a departing end to a compatible arriving end; proximity alone does not make a directed route. The colors here visualize the path spans under the build tool, rather than a new Walking run. This closeup focuses on the joins between copies.

---

_Last updated: 2026-10-22 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
