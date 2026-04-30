---
title: Smart Restore Enhanced Feature Specification
type: PLAN
date: 2026-04-25
status: WIP
category: Features
tags: [restore, presets, smart-panel, configuration, json]
related:
  - ../SmartPanel/IMPL_SmartPanel_CurrentFlow.md
  - ../Scaling/IMPL_Scaling_CurrentFlow.md
  - ../Transforms/IMPL_Transforms_CurrentFlow.md
  - ../AutoConnect/IMPL_AutoConnect_CurrentFlow.md
  - ../../Reference/REF_Lexicon.md
  - ../../Reference/ISSUES_PRIORITIZED.md
---

# Smart Restore Enhanced

## Summary

Smart Restore Enhanced is a named preset system that captures and restores Smart Panel configuration, including building type, grid parameters, transform parameters, and optional functional settings such as recipes and auto-connect overrides.

This evolves the original Restore concept:

- Original Restore: parameter presets without building type.
- Restore Enhanced: full Smart build presets that combine type and parameters.

Short definition: Smart Restore Enhanced restores a saved building type together with Smart placement parameters and optional functional settings through the Smart Panel.

## Core Concept

A Restore preset represents a reusable Smart Panel state that can be applied to spawn a configured hologram preview quickly.

It captures:

- What to build: the building class.
- How to build it: grid, spacing, steps, stagger, and rotation.
- How it behaves: recipe and auto-connect settings when present and valid.

## Initial Data Model

```csharp
class SmartRestorePreset
{
    string Name;

    // Core identity
    string BuildingClass; // e.g. /Game/FactoryGame/Buildable/Factory/Constructor/Build_Constructor.Build_Constructor_C

    // Grid configuration
    int GridX;
    int GridY;
    int GridZ;

    // Transform configuration
    float SpacingX;
    float SpacingY;
    float SpacingZ;

    float StepX;
    float StepY;

    float StaggerX;
    float StaggerY;
    float StaggerZX;
    float StaggerZY;

    float RotationDegrees;

    // Optional functional settings
    string Recipe; // Recipe class path or identifier
    bool HasRecipeOverride;

    AutoConnectSettings AutoConnect;

    // Metadata
    int Version;
    DateTime CreatedAt;
    DateTime UpdatedAt;
}
```

## AutoConnect Settings Draft

```csharp
class AutoConnectSettings
{
    bool Enabled;

    // Belt
    int BeltTier; // or enum: Auto, Mk1-Mk6

    // Pipe
    int PipeTier;
    string PipeRoutingMode;

    // Power
    float PowerRange;
    int ReservedConnections;

    bool HasOverrides; // whether to apply on restore
}
```

## Save Preset Behavior

Triggered from the Smart Panel.

Capture current state:

- Active hologram building class.
- Grid dimensions: X, Y, and Z.
- Transform state: spacing, steps, stagger, and rotation.
- Recipe, when applicable.
- Auto-connect settings, when the preset elects to override them.

Prompt for:

- Preset name.
- Overwrite confirmation when a duplicate name already exists.

## Load Preset Behavior

When the user selects and applies a preset:

1. Set the active build gun to the stored `BuildingClass`.
2. Apply grid settings.
3. Apply transform settings and rotation.
4. Apply recipe override if the stored recipe is valid for the restored building.
5. Apply auto-connect overrides if `HasOverrides` is true.
6. Refresh or respawn the hologram preview so the panel state and build gun preview agree.

## Smart Panel Integration

Extend the Smart Panel (`K`) with a Restore / Presets section.

```text
[ Restore / Presets ]

Dropdown: [Select Preset]

Buttons:
[Apply] [Save Current] [Delete]

Optional:
[Update Existing Preset]
```

Expected control behavior:

- `Apply`: loads selected preset into the build gun and panel state.
- `Save Current`: creates a new preset from the current hologram and Smart settings.
- `Delete`: removes the selected preset after confirmation.
- `Update Existing Preset`: overwrites the selected preset without changing its name.

## Behavior Rules

### Building Type

Building type is always part of a preset.

If the building is not unlocked, the first implementation should block the restore with a warning. Fallback-to-current-hologram can remain a later option, but silent fallback risks surprising the player because the preset would no longer be deterministic.

### Recipe Application

Apply the recipe only if:

- The restored building supports recipes.
- The stored recipe is valid for that building.
- The recipe is available in the current game state.

Invalid recipes should degrade gracefully. The initial UI can warn and continue with no recipe override.

### Auto-Connect

Apply stored auto-connect settings only when `AutoConnect.HasOverrides` is true.

If `HasOverrides` is false, restore should leave the current Smart auto-connect settings untouched.

### Versioning

Every preset should include a version field. This allows migrations if Smart internals, panel state names, or stored class paths change.

## Storage

Initial approach: JSON-based storage.

Candidate location:

```text
/Smart/Presets/*.json
```

Open storage decision:

- Config folder: easier for users to inspect, share, and back up.
- SaveGame storage: closer to world-specific unlock and recipe context.

The first implementation should prefer a location that supports export/import without depending on arbitrary world state.

## Example Presets

### Train Ramp

```text
Building: FoundationRamp_4m
Grid: 100x3x1
Spacing: -10, 0, 0
Steps: +10
Rotation: +3 degrees
```

### Constructor Row

```text
Building: Constructor
Grid: 10x1x1
Recipe: Screws
AutoConnect: Enabled, belt tier auto
```

## Non-Goals

Initial implementation should not attempt:

- Full topology capture or Extend-level cloning.
- Saving connections or manifolds.
- Saving arbitrary world state.
- Multiplayer sync beyond normal local state behavior.

## Future Expansion

### Tier 2

Potentially save:

- Power shard state.
- Somersloop state.
- Color swatches.
- Signs.

### Tier 3

Potential hybrid with Extend:

- Template plus topology snapshot.
- Parametric blueprint-style presets.

## Key Design Principles

1. Deterministic: the same preset should produce the same layout.
2. Decoupled from world state: presets should not depend on existing structures.
3. Composable: restore should work with Scaling, Transforms, AutoConnect, and future Extend flows.
4. Fail-safe: invalid fields should degrade gracefully without corrupting panel or hologram state.

## Open Questions

- Should presets auto-switch the build gun, or require confirmation first?
- Should recipes be optional per preset or always stored?
- Should presets support hotkeys like the original Restore behavior?
- Should presets be shared/exportable through explicit import/export UI?
- Should storage be global config, world-specific SaveGame, or both?

## Implementation Notes

- Hook into the Smart Panel state object rather than inventing a parallel state path.
- Serialize and deserialize panel state through a dedicated preset struct.
- Reapply settings through the same setters used by UI controls so validation, clamping, labels, and hologram refresh behavior remain consistent.
- Refresh the hologram after applying the preset.
- Store class and recipe identifiers as stable class paths where possible.
- Treat invalid fields as warnings and skip those fields rather than aborting the entire restore, except for an unavailable building class.

## Suggested Implementation Slices

1. Data model and JSON read/write service.
2. Capture current panel and hologram state into a preset.
3. Apply grid and transform settings from a selected preset.
4. Restore building class with unlock validation.
5. Add Smart Panel preset dropdown and buttons.
6. Add recipe restore validation.
7. Add auto-connect override support.
8. Add version migration hooks and import/export polish.
