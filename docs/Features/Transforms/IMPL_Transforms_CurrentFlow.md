---
title: Smart Transforms Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [transforms, spacing, steps, stagger, rotation, placement]
related: [../Scaling/IMPL_Scaling_CurrentFlow.md, ../Extend/IMPL_Extend_CurrentFlow.md]
---

# Smart Transforms Current Flow

## Purpose

Transforms are the placement modifiers that alter where scaled child holograms appear relative to the active parent hologram. They are not a separate placement system; they are applied during Scaling and are reused by Scaled Extend where Extend asks the grid/positioning pipeline for clone offsets.

The current implemented transform set is Spacing, Steps, Stagger, and Z-axis Rotation.

## Current Status

| Transform | Status | Current behavior |
|-----------|--------|------------------|
| Spacing | Active | Adds extra gap on X, Y, and/or Z between grid cells. |
| Steps | Active | Adds progressive Z elevation from X and/or Y grid indices. |
| Stagger | Active | Adds lateral X/Y row offsets and vertical ZX/ZY layer offsets. |
| Rotation | Active, Z axis only | Places child holograms along a yaw arc/radial pattern. X/Y rotation axes are not implemented. |

## Primary Code Files

| File | Role |
|------|------|
| `Source/SmartFoundations/Public/Subsystem/SFPositionCalculator.h` | Position calculator interface and transform helper declarations |
| `Source/SmartFoundations/Private/Subsystem/SFPositionCalculator.cpp` | Combines base grid offset, spacing, steps, stagger, and rotation into child positions |
| `Source/SmartFoundations/Public/Services/SFGridStateService.h` | Counter mutation, mode routing, axis cycling, and adjustment increments |
| `Source/SmartFoundations/Public/HUD/SFHUDTypes.h` | `FSFCounterState` storage for grid and transform counters |
| `Source/SmartFoundations/Private/Input/SFInputRegistry.cpp` | Input bindings for transform modes, axis cycling, and value adjustment |
| `Source/SmartFoundations/Private/UI/SmartSettingsFormWidget.cpp` | Panel inputs for spacing, steps, stagger, and rotation values |

## Runtime Flow

1. Input toggles a transform mode or adjusts a value.
2. `USFSubsystem` routes the input to `USFGridStateService`.
3. The grid state service updates `FSFCounterState`.
4. `USFGridSpawnerService::UpdateChildPositions` iterates child holograms.
5. `FSFPositionCalculator::CalculateChildPosition` calculates each child location from grid index, item size, parent transform, and the current counter state.
6. The HUD and Smart Settings Form read the same counter state to show current values.

## Counter Model

| Counter group | Storage | Unit |
|---------------|---------|------|
| Spacing | `SpacingX`, `SpacingY`, `SpacingZ`, `SpacingAxis`, `SpacingMode` | Unreal units, displayed as meters in UI |
| Steps | `StepsX`, `StepsY`, `StepsAxis` | Unreal units |
| Stagger | `StaggerX`, `StaggerY`, `StaggerZX`, `StaggerZY`, `StaggerAxis` | Unreal units |
| Rotation | `RotationZ`, `RotationAxis` | Degrees |

Spacing, Steps, and Stagger adjustments use 50 Unreal-unit increments through the unified value adjustment path. Rotation uses 5 degree increments.

## Transform Details

### Spacing

Spacing increases separation between cells without changing the base buildable size. The default cell size still comes from `USFBuildableSizeRegistry`; spacing is an additional offset layered on top.

Spacing can be applied to individual axes or combined axes. The panel exposes per-axis numeric fields, while keybind mode cycling controls which axis receives wheel/value adjustments.

### Steps

Steps create stair-like elevation. X steps add height as X index changes. Y steps add height as Y index changes. This feature is intentionally tied to grid axes rather than world axes so it follows the active hologram orientation.

### Stagger

Stagger offsets rows and layers. X/Y stagger creates horizontal offset effects across the grid. ZX/ZY stagger introduces vertical-layer offsets used for leaning or angled multi-layer structures.

### Rotation

Rotation is currently Z-axis radial placement. Positive player-facing rotation is mapped into Unreal yaw math by the position calculator. X and Y rotation controls are not implemented in code and should not be documented as active features.

## Validation and Constraints

`USFValidationService` validates spacing constraints for special hologram types and handles floor-validation behavior for elevated children. The most important practical rule is that transforms only affect preview/build positions; they do not make unsupported buildables scaleable by themselves.

## Archived Inputs

The previous per-transform docs and radial-transform research were moved to `docs/Archive/2026/features-consolidation/superseded/Transforms/`. They remain useful for math history and failed/experimental radial ideas, but this document is the current source of truth.
