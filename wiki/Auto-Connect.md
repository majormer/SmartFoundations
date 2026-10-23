# Auto-Connect

Auto-Connect tries to create belts, pipes, hyper tubes, and power lines for a Smart! preview when the connection is obvious.

It is intentionally cautious. If Smart! is not sure the connection is valid, it skips it instead of building a messy or broken layout.

## Where to change connections

Open **K → Scaling** and use the applicable **Auto-Connect** section for the held buildable. On supported logistics buildables, `U` opens connection settings. Global defaults are in **Options → Mod Settings → Smart!**; the current preview's choices are in the Panel. Connection preferences update live, independently of **Apply placement immediately**.

Use the Panel, wheel or numpad to shape the grid; inspect the generated connections and direction arrows in the world before building. The HUD reports placement values rather than providing another connection-settings form. See [Controls](Controls) for default keys and Player Relative behavior.

## Belt Auto-Connect

Use Belt Auto-Connect when scaling:

- Splitters.
- Mergers.
- Conveyor supports.
- Machine rows with nearby inputs or outputs.

Smart! can preview belts between compatible distributors and from distributors to nearby machines.

See the step-by-step [Lift-assisted Auto-Connect](Lift-Assisted-Auto-Connect) guide.

### Lift-assisted machine connections

Choose **Belts Only** (default), **Lift at Distributor** or **Lift at Building** in the Panel or global Belt Auto-Connect settings. The lift options handle the height difference while keeping the flat belt level at the machine connection. Use stagger and Z scaling to place splitters or mergers at different heights and distances along a Manufacturer line.

Smart reuses the existing port-selection rules, preserves flow and charges for the new lifts and belts. Matching native direct docks need no extra belt and do not charge for one. A lift does not bypass unlocks, native attachment rules or an invalid route.

![Lift at Building: cyan lift and belt previews join the raised splitters to the existing Constructor inputs.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/lift-at-building-side.png)

Lift at Building: cyan lift and belt previews join the raised splitters to the existing Constructor inputs.

## Pipe Auto-Connect

Use Pipe Auto-Connect when scaling:

- Pipe junctions.
- Pipeline supports (ground and wall).
- Fluid machines.
- Fluid buffers.

Smart! can preview pipe connections where the pipe endpoints are compatible.

**Scaled pipeline supports build a pipe run.** When you scale a line of Pipeline Supports or Pipeline Wall Supports, Smart! plumbs a connected pipe run between the copies — the same way a scaled line of Conveyor Poles builds a belt run. Fluid flows through the whole run, and each support's own snap point stays free, so you can still connect a pipe to it by hand. The ground support's height and its top-piece angle carry across the whole line, so a sloped run stays consistent.

Generated pipes inherit paint from a connected source pipe. A fitting inherits paint only when its connected pipes agree; moving a preview to a connection with no paint source restores its original paint.

Connection settings are live: changing pipe tier, style or routing refreshes the preview immediately. **Apply placement immediately** controls grid geometry, not these preferences.

![A rotated Pipeline Wall Support grid with its connecting pipe preview.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/pipe-wall-rotation.png)

A rotated Pipeline Wall Support grid with its connecting pipe preview.

## Hyper Tube Auto-Connect

Use Hyper Tube Auto-Connect when scaling:

- Stackable Hypertube supports.
- Hypertube wall supports.

**Scaled hyper tube supports build a hyper tube run.** When you scale a line of Stackable Hyper Tube Supports, Smart! lays a connected hyper tube run between the copies — the same way a scaled line of Conveyor Poles builds a belt run or Pipeline Supports build a pipe run. The run follows the line's spacing and height and can climb steeply, and each support's own snap point stays free, so you can still connect a hyper tube to it by hand.

Hyper Tube Auto-Connect has its **own enable toggle and routing mode**, independent from pipes — set them from the Smart Panel, the in-build Auto-Connect settings (hold `U`), or the mod config. See [Settings Reference](Settings-Reference).

## Rotated wall-support runs

Pipeline and Hypertube wall-support children follow the parent's full rotation. The Panel's **Wall rotation** offers native **45° steps**, including a vertical run. This is separate from grid Rotation, which bends a sequence of copies.

In Building Relative controls, Y follows the rotated wall run. In Player Relative controls, vertical input follows the most vertical supported direction: for a vertical wall run, **X+Z + wheel** grows the run vertically while **Z + wheel** grows sideways. The Panel retains its X/Y/Z columns; use the arrows to see those axes. Conveyor wall connectors retain their native orientation restrictions and use lifts for vertical transport.

![Hypertube Wall Support copies retain the selected angle, with a connecting route preview.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/hypertube-wall-rotation.png)

Hypertube Wall Support copies retain the selected angle, with a connecting route preview.

## Power Auto-Connect

Use Power Auto-Connect when scaling:

- Power poles.
- Single- and double-sided wall outlets, Mk.1-Mk.3.
- Rows of powered machines.

Smart! can connect poles to nearby poles and, depending on settings and capacity, connect machines to poles. Auto mode joins a continuous chain across horizontal and vertical outlet grids. Double-sided outlets count each face's available slots separately and can supply nearby unconnected buildings from either face. Native cable range and unlock-dependent socket capacities still apply.

## If A Connection Is Missing

Check:

- Auto-Connect is enabled in the Smart Panel.
- The belt, pipe, or power tier is available.
- The distance is reasonable.
- The connectors face compatible directions.
- Power poles still have free connection slots.

Some layouts still need a manual connection afterward. That is normal.

---

_Last updated: 2026-10-22 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
