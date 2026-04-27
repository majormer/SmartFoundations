---
title: Smart Scaling Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [scaling, grid, child_holograms, buildable_size_registry]
related: [../Transforms/IMPL_Transforms_CurrentFlow.md, ../SmartPanel/IMPL_SmartPanel_CurrentFlow.md]
---

# Smart Scaling Current Flow

## Purpose

Scaling is Smart!'s grid placement system. It lets the active build-gun hologram become the parent of an X/Y/Z child-hologram grid, then uses the transform pipeline to position every child before vanilla construction builds the parent and children together.

## Current Status

Scaling is active for supported single-click buildables. It uses vanilla holograms and child holograms rather than swapping the active hologram class. This avoids registration loops and lets vanilla cost aggregation work through `AddChild`.

## Primary Code Files

| File | Role |
|------|------|
| `Source/SmartFoundations/Public/Subsystem/SFSubsystem.h` | Feature facade, scaling input callbacks, grid state accessors |
| `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp` | Active hologram registration, scale handlers, panel opening, child update triggers |
| `Source/SmartFoundations/Public/Services/SFGridStateService.h` | Grid counter mutation and forbidden-value skipping |
| `Source/SmartFoundations/Public/Services/SFGridSpawnerService.h` | Grid regeneration and child-positioning facade |
| `Source/SmartFoundations/Private/Services/SFGridSpawnerService.cpp` | Regenerates children and updates positions |
| `Source/SmartFoundations/Public/Subsystem/SFHologramHelperService.h` | Child hologram lifecycle and parent-child registration |
| `Source/SmartFoundations/Private/Subsystem/SFHologramHelperService.cpp` | Spawns, destroys, and tracks child holograms |
| `Source/SmartFoundations/Public/Data/SFBuildableSizeRegistry.h` | Source of buildable dimensions and scaling eligibility |
| `Source/SmartFoundations/Public/Features/Scaling/FSFGridArray.h` | Pure grid array helper types |
| `Source/SmartFoundations/Public/Features/Scaling/SFScalingTypes.h` | Axis and bounds types |

## Runtime Flow

1. `USFSubsystem::PollForActiveHologram` detects the current build-gun hologram.
2. `RegisterActiveHologram` initializes counter state, resolves buildable size, and prepares helper services.
3. Scale input calls `OnScaleXChanged`, `OnScaleYChanged`, or `OnScaleZChanged`.
4. `ApplyAxisScaling` asks `USFGridStateService` to mutate `GridCounters`.
5. `USFGridSpawnerService::RegenerateChildHologramGrid` adds or removes child holograms to match the requested grid.
6. `USFGridSpawnerService::UpdateChildPositions` uses `FSFPositionCalculator` and the current transform counters to place every child.
7. Vanilla construction builds the parent and its registered children, with costs flowing through the normal hologram child list.

## Counter Model

Scaling dimensions are stored in `FSFCounterState::GridCounters` as an `FIntVector`.

| Axis | Meaning |
|------|---------|
| X | Width/count along the active hologram's local X axis |
| Y | Depth/count along the active hologram's local Y axis |
| Z | Vertical layer count |

Grid counters support negative values to represent direction. The mutation path skips forbidden values `0` and `-1` so the grid never collapses into invalid no-copy states.

## Buildable Size Registry

The size registry is the current source of truth for supported dimensions and scaling eligibility.

| Registry field | Meaning |
|----------------|---------|
| `DefaultSize` | Cell size used for base grid spacing |
| `bSwapXYOnRotation` | Whether dimensions swap on 90 degree rotation |
| `AnchorOffset` | Offset from actor origin to intended placement anchor |
| `bSupportsScaling` | Whether Smart! allows scaling for this buildable |
| `bIsValidated` | Whether the profile was manually verified |

Unknown/modded buildables can fall back to a conservative default profile, but docs and user-facing claims should describe only verified support unless a test has confirmed the case.

## Transform Integration

Scaling is the owner of the transform pipeline. Spacing, Steps, Stagger, and Rotation do not spawn children themselves; they alter the locations that scaling computes for children.

See [../Transforms/IMPL_Transforms_CurrentFlow.md](../Transforms/IMPL_Transforms_CurrentFlow.md).

## Important Caveats

- Scaling operates on the active vanilla hologram and supported child holograms.
- Grid size is practically constrained by performance and validation, even where no strict hard cap is documented.
- Multi-step, drag, or highly specialized vanilla holograms may be unsupported even if their built actor appears in the size registry.
- Older Scaling docs that mention historical monolithic helper files are stale for the current service-based implementation.

## Archived Inputs

Previous Scaling audits and wall-power-pole research were moved to `docs/Archive/2026/features-consolidation/superseded/Scaling/`. The old transform subfolder was consolidated into the Transforms doc and archived separately.
