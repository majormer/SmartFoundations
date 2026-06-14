# Auto-Connect

Auto-Connect tries to create belts, pipes, and power lines for a Smart! preview when the connection is obvious.

It is intentionally cautious. If Smart! is not sure the connection is valid, it skips it instead of building a messy or broken layout.

> Screenshot placeholder: splitter row preview with belts connecting into machines.

## Belt Auto-Connect

Use Belt Auto-Connect when scaling:

- Splitters.
- Mergers.
- Conveyor supports.
- Machine rows with nearby inputs or outputs.

Smart! can preview belts between compatible distributors and from distributors to nearby machines.

> Screenshot placeholder: row of splitters feeding a row of constructors.

## Pipe Auto-Connect

Use Pipe Auto-Connect when scaling:

- Pipe junctions.
- Pipeline supports (ground and wall).
- Fluid machines.
- Fluid buffers.

Smart! can preview pipe connections where the pipe endpoints are compatible.

**Scaled pipeline supports build a pipe run.** When you scale a line of Pipeline Supports or Pipeline Wall Supports, Smart! plumbs a connected pipe run between the copies — the same way a scaled line of Conveyor Poles builds a belt run. Fluid flows through the whole run, and each support's own snap point stays free, so you can still connect a pipe to it by hand. The ground support's height and its top-piece angle carry across the whole line, so a sloped run stays consistent.

> Screenshot placeholder: pipe junction grid with preview pipes.

## Power Auto-Connect

Use Power Auto-Connect when scaling:

- Power poles.
- Rows of powered machines.

Smart! can connect poles to nearby poles and, depending on settings and capacity, connect machines to poles.

> Screenshot placeholder: power poles with cable previews connected to machines.

## If A Connection Is Missing

Check:

- Auto-Connect is enabled in the Smart Panel.
- The belt, pipe, or power tier is available.
- The distance is reasonable.
- The connectors face compatible directions.
- Power poles still have free connection slots.

Some layouts still need a manual connection afterward. That is normal.
