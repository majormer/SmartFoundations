# Auto-Connect

Auto-Connect creates preview connections for belts, pipes, and power when Smart! can safely infer the intended layout.

Auto-Connect is conservative. If a connection pair, angle, distance, tier, or capacity does not look valid, Smart! should skip the connection rather than create a broken layout.

> Screenshot placeholder: row of splitters or machines with Smart belt previews visible before placement.

## Belt Auto-Connect

Belt Auto-Connect handles distributors, conveyor attachments, and stackable support workflows.

Current behavior:

- Distributor-to-distributor connections can form chains.
- Distributor-to-building connections fill compatible nearby inputs and outputs.
- Stackable conveyor poles can create horizontal belt previews between adjacent supports.
- Belt tier and routing settings come from Smart settings.
- Chain actor stabilization may run after build where topology changes need it.

> Screenshot placeholder: scaled splitter row preview with belts connecting into a row of constructors.

## Pipe Auto-Connect

Pipe Auto-Connect scans pipe junction holograms and compatible pipe connectors, then creates pipe previews. It also handles floor-hole pipe previews and support layouts where implemented.

Current behavior:

- Pipe tier and pipe indicator style come from Smart settings.
- Junction chains use connector pairing logic.
- Pipe networks may need rebuild/stabilization after built connections so fluid simulation sees the final topology.

> Screenshot placeholder: pipe junction grid with preview pipes connecting to nearby machines or buffers.

## Power Auto-Connect

Power Auto-Connect processes scaled power poles, connects poles to neighbor poles, and can wire powered buildings to available pole capacity.

Current behavior:

- Power-line previews are tracked by the power preview helper.
- Pole capacity and reserved connection slots are tracked to avoid overbooking.
- Cable cost is derived from line length.
- Power range and reserved-slot settings are exposed through Smart settings.

> Screenshot placeholder: row of power poles with cable previews and nearby machines connected.

## Settings

Auto-Connect settings are exposed in the Smart Settings Form and backed by config fields for:

- Belt Auto-Connect.
- Pipe Auto-Connect.
- Power Auto-Connect.
- Belt and pipe tier choices.
- Belt and pipe routing modes.
- Power range, reserved slots, and pole capacity.

See [Settings Reference](Settings-Reference).

## Verified From

- `docs/Features/AutoConnect/IMPL_AutoConnect_CurrentFlow.md`
- `Source/SmartFoundations/Public/Features/AutoConnect/SFAutoConnectService.h`
- `Source/SmartFoundations/Public/Features/PipeAutoConnect/SFPipeAutoConnectManager.h`
- `Source/SmartFoundations/Public/Features/PowerAutoConnect/SFPowerAutoConnectManager.h`
- `Source/SmartFoundations/Public/Config/Smart_ConfigStruct.h`

