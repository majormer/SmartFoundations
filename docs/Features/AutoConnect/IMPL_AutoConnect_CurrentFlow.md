---
title: Smart AutoConnect Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [autoconnect, belts, pipes, power, previews, child_holograms]
related: [../Scaling/IMPL_Scaling_CurrentFlow.md, ../SmartPanel/IMPL_SmartPanel_CurrentFlow.md]
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

AutoConnect is coordinated through the active hologram and child-hologram system. It does not place free-standing built actors outside the normal build commit path.

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
- Stackable conveyor poles can create horizontal belt previews between adjacent supports.
- Preview creation respects belt tier settings and belt length limits.
- Chain actor stabilization is handled after build by the shared chain actor service where topology changes require it.

## Pipe AutoConnect

Pipe AutoConnect is handled by `FSFPipeAutoConnectManager`. It scans pipe junction holograms, looks for compatible pipe connectors, and creates `FPipePreviewHelper` previews. It also handles floor-hole pipe previews and can route around stackable/ceiling support layouts.

Important behavior:

- Pipe tier and indicator/no-indicator style come from runtime settings.
- Junction chains are evaluated with connector pairing logic rather than only nearest-distance matching.
- Pipe network rebuilds are required after built connections so fluid simulation sees the final topology.

## Power AutoConnect

Power AutoConnect is handled by `FSFPowerAutoConnectManager`. It processes scaled poles, connects poles to neighbor poles, and optionally wires powered buildings to available pole capacity.

Important behavior:

- Power-line previews are represented by `FPowerLinePreviewHelper`.
- Pole capacity and reserved slots are tracked so one preview path does not overbook a connection.
- Cable cost is derived from line length.
- Power grid axis and reserved connection settings are exposed through Smart Settings.

## SmartPanel Integration

The Smart Settings Form exposes AutoConnect controls for belts, pipes, and power. The settings are split by domain rather than having separate feature panels.

See [../SmartPanel/IMPL_SmartPanel_CurrentFlow.md](../SmartPanel/IMPL_SmartPanel_CurrentFlow.md).

## Archived Inputs

Older belt-only, pipe-only, power-only, orchestrator, child-hologram-refactor, and stackable-pole docs were moved to `docs/Archive/2026/features-consolidation/superseded/AutoConnect/` and `docs/Archive/2026/features-consolidation/superseded/PowerAutoConnect/`.

Those notes are kept for implementation history and test context. This document is the current canonical AutoConnect map.
