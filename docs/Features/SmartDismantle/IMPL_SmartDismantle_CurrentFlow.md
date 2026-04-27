---
title: Smart Dismantle Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [smart_dismantle, blueprint_proxy, dismantle, grouping]
related: [../Scaling/IMPL_Scaling_CurrentFlow.md, ../Extend/IMPL_Extend_CurrentFlow.md]
---

# Smart Dismantle Current Flow

## Purpose

Smart Dismantle groups buildings placed by Smart! multi-building operations so players can dismantle the group through Satisfactory's vanilla Blueprint Dismantle mode.

This is not a separate Smart dismantle panel or a custom batch dismantle execution service. The active implementation is a compatibility layer around vanilla `AFGBlueprintProxy` grouping.

## Current Status

Smart Dismantle is partially implemented and active for Smart! placements that create multiple buildings.

| Capability | Status | Notes |
|------------|--------|-------|
| Group scaled grid placements | Active | Multi-axis grid builds create/register a blueprint proxy. |
| Group Extend placements | Active | Extend sessions can group built actors under the current proxy. |
| Use vanilla Blueprint Dismantle | Active | The group is intended to be selected by vanilla Blueprint Dismantle mode. |
| Custom Smart Dismantle UI | Not implemented | Older docs proposed this, but no current service/panel exists. |
| Radius/network dismantle scanning | Not implemented | Older research only. |

## Primary Code Files

| File | Role |
|------|------|
| `Source/SmartFoundations/Public/Subsystem/SFSubsystem.h` | `CurrentBuildProxy`, `CurrentProxyOwner`, and blueprint-proxy state |
| `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp` | `OnActorSpawned` grouping logic and session cleanup |
| `Source/SmartFoundations/Private/Services/SFRecipeManagementService.cpp` | Blueprint-proxy spawn detection used to avoid recipe-application conflicts |
| `Reference/FactoryGame/Public/FGBlueprintProxy.h` | Vanilla proxy class used for grouping |
| `Reference/FactoryGame/Public/Buildables/FGBuildable.h` | `GetBlueprintProxy` and `SetBlueprintProxy` support on buildables |

## Runtime Flow

1. A buildable actor spawns.
2. `USFSubsystem::OnActorSpawned` receives the actor.
3. The subsystem checks whether there is an active Smart! hologram.
4. It reads `GridCounters` and checks whether the current operation is a multi-grid build or Extend is active.
5. If the spawned buildable is not already owned by a blueprint proxy, Smart! creates an `AFGBlueprintProxy` for the current build session.
6. The buildable receives `SetBlueprintProxy(CurrentBuildProxy.Get())`.
7. The proxy registers the buildable through `RegisterBuildable`.
8. When the hologram/build session ends, the subsystem clears the weak proxy references; the proxy actor remains as the vanilla grouping owner.

## Scope Rules

Smart Dismantle currently groups only when at least one of these is true:

- The active grid has more than one cell on X, Y, or Z.
- Extend mode is active.

Single vanilla placements are intentionally not grouped.

## Why Vanilla Blueprint Proxy

The implementation uses the vanilla grouping pattern because Satisfactory already knows how to:

- Highlight all buildables owned by a blueprint proxy.
- Calculate group dismantle refunds.
- Dismantle the registered buildables through the vanilla dismantle path.
- Preserve proxy references on buildables via their built-in blueprint-proxy field.

## Caveats and Open Risks

- The code dynamically spawns a proxy rather than placing a saved blueprint descriptor. Save/load behavior should be retested whenever this path changes.
- The implementation groups actors at spawn time. If a build flow spawns supporting actors outside the active hologram/proxy window, those actors may not join the group.
- Lightweight instances and special vanilla blueprint descriptor behavior were researched in older docs but are not represented as a separate Smart dismantle service.
- There is no Smart-specific refund UI or confirmation panel for dismantling.

## Archived Inputs

The older dismantle API research, blueprint grouping research, and WIP design overview were moved to `docs/Archive/2026/features-consolidation/superseded/SmartDismantle/`. Those files are useful for native API background, but the active implementation is the proxy grouping flow above.
