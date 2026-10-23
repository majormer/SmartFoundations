# Lift-assisted Auto-Connect

Use lifts when a splitter or merger sits above or below its machine connection. Smart can handle the height difference with a lift and keep the machine-connection belt level.

| Factory Connection option | Result |
|---|---|
| **Belts Only** | Ordinary belt routing; the default |
| **Lift at Distributor** | Lift beside the splitter/merger, with a flat belt to the machine |
| **Lift at Building** | Lift beside the building, with a flat belt from the distributor |

## Example: a Manufacturer input line

1. Place the Manufacturer row and select a compatible splitter.
2. Open **K → Scaling** and enable Belt Auto-Connect.
3. Under **Factory Connection**, choose the lift location.
4. Use grid counts, spacing, stagger and Z scaling to arrange the distributors at the desired input heights and distances.
5. Inspect each lift, flat belt, flow arrow and the complete material quote before building.

The same choices apply to compatible merger/output layouts. Existing port-selection rules still decide which ports are eligible; choosing a lift location does not request connections to every nearby machine port.

Matching native direct docks omit an unnecessary belt and its cost. Otherwise you pay for the new belts and lifts. Unlocks, attachment rules, native shape limits and available resources still apply. A route that cannot be constructed safely is not permission to bypass those rules.

Global defaults live under Belt Auto-Connect; the current build's connection options are live in the Panel. See [Auto-Connect](Auto-Connect), [Settings Reference](Settings-Reference) and [Conveyor Lift Grids](Conveyor-Lift-Grids).

## Controls and feedback

The lift-location choice is a Panel or mod-setting option, not a wheel mode. Use **K → Scaling → Factory Connection** for the current preview, or Belt Auto-Connect in the mod settings for the default. Arrange the distributor grid through the Panel or the wheel/numpad [scaling](Grid-Scaling) and [transform](Transforms) controls. `U` opens connection settings on compatible distributors.

The HUD reports grid and transform values; the world preview must show the proposed belts, lifts and flow arrows. The complete cost is shown before construction. Do not infer a valid lift route from floating splitter previews alone.

## Compare the lift locations

These matched side views show the same two Constructors and raised splitter grid. Cyan parts are the proposed connections; the machines are already built.

![Lift at Building: vertical lifts beside the Constructor inputs, with horizontal belts from the raised splitters.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/lift-at-building-side.png)

**Lift at Building:** the lifts sit beside the Constructor inputs. The horizontal belts run from the elevated splitters to the tops of the lifts.

![Lift at Distributor: vertical lifts beneath the splitters, with lower horizontal belts toward the Constructor inputs.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/lift-at-distributor-side.png)

**Lift at Distributor:** the lifts sit beneath the splitters. The horizontal machine connections run at the lower input height. These are unbuilt previews; check the complete material quote before construction.


---

_Last updated: 2026-10-20 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
