---
title: Smart AutoConnect Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [autoconnect, belts, pipes, power, previews, child_holograms]
related: [../Scaling/IMPL_Scaling_CurrentFlow.md, ../SmartPanel/IMPL_SmartPanel_CurrentFlow.md, ../../Reference/REF_DistributorPortTopology.md]
---

# Smart AutoConnect Current Flow

## Purpose

AutoConnect creates preview and child holograms that connect scaled logistics and power layouts. This document consolidates belt, pipe, and power AutoConnect into one current implementation reference.

## Current Status

| Area | Status | Current behavior |
|------|--------|------------------|
| Belt AutoConnect | Active | Creates belt previews from distributors/supports to nearby compatible connectors and between chainable logistics elements. |
| Pipe AutoConnect | Active | Creates pipe previews for junctions, storage, pumps, floor holes, and compatible pipe connectors. |
| Power AutoConnect | Active | Creates power-line previews between poles and to powered buildings while respecting connection capacity. |

AutoConnect for belts and pipes is coordinated through the active hologram and child-hologram system. Power AutoConnect is the exception: it directly spawns `AFGBuildableWire` actors (with its own cable cost deduction) after pole/building construction, so not all AutoConnect output flows through child-hologram construction. See the construction-order migration note (audit F039) before changing these paths for 1.2.

## Configuration Boundaries

Two configuration sections establish separate boundaries:

- **Blueprint Auto-Connect** owns **Blueprint Seam Auto-Connect**, which independently enables the belt and pipe conduits Smart creates
  between scaled blueprint copies. The normal Belt and Pipe Auto-Connect master switches do not gate
  blueprint seams. Their tier, style, and routing choices still supply the conduit recipe and shape.
- **Auto-Connect Behavior** owns **Nearby Logistics Range**, a connector-to-connector candidate cap, in meters, for
  distributor-to-factory belts and pipe-junction/floor-hole-to-factory pipes. It defaults to 25 m
  and is clamped to 1-56 m.

Nearby Logistics Range does not redefine topology. It does not apply to blueprint seams, distributor
manifold lanes, stackable support runs, hypertubes, Extend, power poles, or factory daisy chains.
Power retains its separate connection-range setting. The configured range can reject a candidate,
but it can never authorize a belt or pipe that the vanilla spline rules consider too long or invalid.

Building discovery is a broad phase with center-to-port headroom for large factories; acceptance is
always measured between the actual logistics connectors (or a floor-hole face and the factory port).

## Primary Code Files

| File | Role |
|------|------|
| `Source/SmartFoundations/Public/Features/AutoConnect/SFAutoConnectService.h` | Main AutoConnect service interface |
| `Source/SmartFoundations/Private/Features/AutoConnect/SFAutoConnectService.cpp` | Belt/distributor processing and preview management |
| `Source/SmartFoundations/Public/Features/AutoConnect/SFAutoConnectOrchestrator.h` | Timing and coordination facade |
| `Source/SmartFoundations/Private/Features/AutoConnect/SFAutoConnectOrchestrator.cpp` | Scheduled AutoConnect refreshes and stackable workflows |
| `Source/SmartFoundations/Public/Features/AutoConnect/Preview/BeltPreviewHelper.h` | Belt preview helper |
| `Source/SmartFoundations/Public/Features/PipeAutoConnect/SFPipeAutoConnectManager.h` | Pipe AutoConnect manager |
| `Source/SmartFoundations/Private/Features/PipeAutoConnect/SFPipeAutoConnectManager.cpp` | Pipe connector discovery, routing, floor-hole handling, preview spawning |
| `Source/SmartFoundations/Public/Features/PipeAutoConnect/PipePreviewHelper.h` | Pipe preview helper |
| `Source/SmartFoundations/Public/Features/PowerAutoConnect/SFPowerAutoConnectManager.h` | Power AutoConnect manager |
| `Source/SmartFoundations/Private/Features/PowerAutoConnect/SFPowerAutoConnectManager.cpp` | Pole grid detection, building wiring, reservations, cable costs |
| `Source/SmartFoundations/Public/Features/PowerAutoConnect/PowerLinePreviewHelper.h` | Power-line preview helper |
| `Source/SmartFoundations/Private/Services/SFGridTransformService.cpp` | Triggers AutoConnect refresh when parent transform changes |

## Runtime Flow

1. Scaling or parent hologram movement changes the active grid.
2. `USFGridTransformService` detects movement and calls `OnDistributorHologramUpdated` where appropriate.
3. `USFAutoConnectService` processes belt/distributor previews for the active hologram and child distributors.
4. Pipe and power managers process their own hologram families through manager-specific entry points.
5. Preview helpers own temporary spline or wire holograms.
6. The parent/child build commit constructs the previews as normal child holograms where applicable, and post-build services repair chain, pipe, or power state when needed.

## Belt AutoConnect

Belt AutoConnect is centered on distributors, conveyor attachments, and stackable support workflows. `USFAutoConnectService::OnDistributorHologramUpdated` is the main refresh entry point. It clears stale previews, finds compatible connection pairs, then creates or updates `FBeltPreviewHelper` instances.

Important behavior:

- Distributor-to-distributor connections are preferred where they form a chain.
- Distributor-to-building connections fill compatible nearby inputs/outputs.
- Distributor-to-building candidates must be within Nearby Logistics Range at their actual ports.
- Stackable conveyor poles can create horizontal belt previews between adjacent supports.
- Preview creation respects belt tier settings and belt length limits.
- Chain actor stabilization is handled after build by the shared chain actor service where topology changes require it.

## Pipe AutoConnect

Pipe AutoConnect is handled by `FSFPipeAutoConnectManager`. It scans pipe junction holograms, looks for compatible pipe connectors, and creates `FPipePreviewHelper` previews. It also handles floor-hole pipe previews and can route around stackable/ceiling support layouts.

The stable named-port layouts for pipe junctions, splitters, and mergers are defined in [Distributor Port Topology Reference](../../Reference/REF_DistributorPortTopology.md). Use that contract instead of component enumeration order when assigning distributor roles.

For recognized pipeline junctions, Pipe Auto-Connect stores the factory-side component name, resolves the exact opposite factory port and two manifold-lane ports from the shared topology contract, and only uses geometry to order separate junction actors along a row. A T-Junction orientation without both perpendicular lane ports is discarded before child previews, costs, or manifold wiring are created. Unknown junction classes may use the conservative geometric fallback, but cannot alter the named vanilla maps.

Important behavior:

- Pipe tier and indicator/no-indicator style come from runtime settings.
- Junction Side A, Side B, and floor-hole factory candidates use the same Nearby Logistics Range.
- Junction chains are evaluated with connector pairing logic rather than only nearest-distance matching.
- Pipe network rebuilds are required after built connections so fluid simulation sees the final topology.

The direct junction/floor-hole spawners in `SFPipeAutoConnectManager_Spawn.cpp` bypass
`FConduitPreviewHelper::EnsureSpawned`, including its Blueprint Designer stamp. They
must set the child's designer before `FinishSpawning`. `ASFPipelineHologram::Construct`
also refreshes recognized Smart children's designer from their immediate parent at
commit, including clearing a stale reference when the parent moves outside. Without
that context, a pipe can build without joining the designer's save/clear ownership
(#526). This concerns Blueprint Designer contents, not placed-blueprint dismantle
membership for tier-upgraded replacements (#533, deliberately unchanged).

## Power AutoConnect

Power AutoConnect is handled by `FSFPowerAutoConnectManager`. It processes scaled poles, connects poles to neighbor poles, and optionally wires powered buildings to available pole capacity.

Important behavior:

- Power-line previews are represented by `FPowerLinePreviewHelper`.
- Pole capacity and reserved slots are tracked so one preview path does not overbook a connection.
- Cable cost is derived from line length.
- Power grid axis and reserved connection settings are exposed through Smart Settings.

Wall outlets use a port-aware planner: Auto forms one continuous grid chain, including vertical
rows, while the two faces retain independent external-wire budgets. Exact named endpoints are
captured for authority-side construction rather than resolved by the nearest port. Native wall
post-placement must preserve the authored wire transforms. Ground poles retain their legacy
grid planner. See [Power Connector Construction Contracts](../../Reference/BuildableContracts/PowerConnectors.md).

Both planners filter unavailable or already-connected consumers before reserving slots. Ceiling
lights are ordinary power consumers here, regardless of their non-factory class hierarchy.
Preview and committed building-wire acceptance use socket positions, not building centers, and
native free-connection getters respect research-dependent capacity. This does not enable light
scale daisy-chaining or add a reverse light-placement-to-existing-pole feature.

## SmartPanel Integration

The Smart Settings Form exposes AutoConnect controls for belts, pipes, and power. The settings are split by domain rather than having separate feature panels.

See [../SmartPanel/IMPL_SmartPanel_CurrentFlow.md](../SmartPanel/IMPL_SmartPanel_CurrentFlow.md).

## Archived Inputs

Older belt-only, pipe-only, power-only, orchestrator, child-hologram-refactor, and stackable-pole docs were moved to `docs/Archive/2026/features-consolidation/superseded/AutoConnect/` and `docs/Archive/2026/features-consolidation/superseded/PowerAutoConnect/`.

Those notes are kept for implementation history and test context. This document is the current canonical AutoConnect map.
