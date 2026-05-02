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

- What to build: the building class (single structure or, in future tiers, an Extend module).
- How to build it: grid, spacing, steps, stagger, and rotation.
- How it behaves: recipe, auto-connect settings, and (in future tiers) overclocking and Somersloop state.

## Initial Data Model

```csharp
class SmartRestorePreset
{
    string Name;

    // Core identity — always captured
    string BuildingClass; // e.g. /Game/FactoryGame/Buildable/Factory/Constructor/Build_Constructor.Build_Constructor_C

    // Capture flags — user selects which fields to include when saving.
    // The save dialog defaults to all flags checked; the user unchecks
    // fields they do not want stored in this preset.
    bool CaptureGrid;
    bool CaptureSpacing;
    bool CaptureSteps;
    bool CaptureStagger;
    bool CaptureRotation;
    bool CaptureRecipe;
    bool CaptureAutoConnect;

    // Grid configuration (stored when CaptureGrid is true)
    int GridX;
    int GridY;
    int GridZ;

    // Transform configuration (each group stored when its flag is true)
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

    // Optional functional settings (stored when their flag is true)
    string Recipe; // Recipe class path or identifier

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

The save flow presents a **capture checklist** so the user selects which parameters to include in the preset. Not every field needs to be saved every time — for example, a user may want to save spacing and steps but not rotation or recipe.

Capture checklist:

| Field group | Always captured | User-selectable |
|-------------|-----------------|------------------|
| Building class | Yes | — |
| Grid (X, Y, Z) | — | Yes |
| Spacing | — | Yes |
| Steps | — | Yes |
| Stagger | — | Yes |
| Rotation | — | Yes |
| Recipe | — | Yes (see recipe rules below) |
| Auto-connect settings | — | Yes |

### Recipe Capture Rules

Recipe is only eligible for capture when:

- The active building is a production building that supports recipes.
- A recipe is currently set on the hologram.
- The recipe is compatible with the stored `BuildingClass`.

If the user selects recipe capture but the current building does not support recipes, the save flow should warn and skip the recipe field rather than blocking the save.

When a preset with a stored recipe is loaded, the recipe is only applied if the target building class is compatible. If the user later loads the preset while holding a different (incompatible) building, the recipe field is silently skipped.

Prompt for:

- Preset name.
- Overwrite confirmation when a duplicate name already exists.

## Load Preset Behavior

When the user selects and applies a preset:

1. **Auto-switch the build gun** to the stored `BuildingClass`. No confirmation prompt — the preset is deterministic and the user chose to apply it.
2. Apply grid settings (if `CaptureGrid` was true when saved).
3. Apply spacing, steps, stagger, and rotation (each only if its capture flag was true).
4. Apply recipe if the stored recipe is valid for the restored building (if `CaptureRecipe` was true).
5. Apply auto-connect overrides (if `CaptureAutoConnect` was true).
6. Fields that were not captured in the preset leave the current Smart Panel state untouched.
7. Refresh or respawn the hologram preview so the panel state and build gun preview agree.

## Smart Panel Integration

### Placement Options

The Smart Panel (`K`) currently has two separate widget surfaces: the **Settings Form** (`USmartSettingsFormWidget` — grid, transforms, recipe, auto-connect) and the **Upgrade Panel** (`USmartUpgradePanel`). Smart Restore must integrate with one of them.

| Option | Approach | Pros | Cons |
|--------|----------|------|------|
| **A: Section in Settings Form** | Add a Restore/Presets collapsible section to the existing Settings Form widget | Keeps all build-configuration in one place; preset save can read current form state directly; no new widget class needed | Settings Form is already dense with controls; may require scrolling or layout rework |
| **B: Dedicated Restore tab/panel** | Create a new `USmartRestorePanel` widget, opened by a sub-tab or toggle in the Smart Panel | Clean separation; room for future expansion (preset library, import/export, search); Upgrade Panel precedent shows a second widget works | More UE widget scaffolding; requires its own open/close keybind or tab switcher; must still read Settings Form state for capture |

This decision depends on the Smart Panel's UE Blueprint layout and available screen real estate. Both options use the same underlying preset service and data model. The choice should be made in the Unreal Editor where the widget hierarchy is visible.

### Controls

Regardless of placement, the Restore UI needs these controls:

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
- `Save Current`: opens the capture checklist dialog, then creates a new preset from the current hologram and Smart settings.
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

### Storage Strategy

Support both global config storage and world-specific SaveGame storage. Global config is the primary location for user-managed presets (easy to inspect, share, back up). World-specific storage can reference or import from global presets.

### Backward Compatibility

Presets from older saves or older Smart! versions should be importable into newer versions. The version field in each preset enables migration:

- On load, check the preset version against the current Smart! version.
- If the version is older, run migration logic (e.g., renamed class paths, changed parameter ranges, new fields with defaults).
- If a stored `BuildingClass` path changed between Satisfactory versions, attempt to map it to the current equivalent.
- If migration fails for a specific field, warn and skip that field rather than rejecting the entire preset.
- Recipes are the highest-risk field for cross-version breakage. A recipe valid in one Satisfactory version may not exist in another. Recipe restore should always validate availability before applying.

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

## Preset Scope: Single Structure vs. Extend Module

The initial implementation captures a **single structure** preset: one `BuildingClass` plus grid and transform parameters. This covers the majority of use cases (foundation grids, production rows, storage rows, etc.).

A natural future extension is capturing an **Extend module** preset — the full topology of a source building plus its connected belts, pipes, distributors, and recipes as captured by Scaled Extend. This would allow players to save a working factory module as a named preset and restore it later, combining Smart Restore with Extend's topology capture.

Extend module presets are not an initial implementation goal, but the data model and preset service should be designed so they can be added without breaking the single-structure preset format.

## Non-Goals

Initial implementation should not attempt:

- Full topology capture or Extend-level cloning (deferred to Tier 3 expansion).
- Saving connections or manifolds.
- Saving arbitrary world state.
- Multiplayer sync beyond normal local state behavior.

## Future Expansion

### Tier 2

Additional single-structure fields:

- Overclocking state (power shard count / clock speed).
- Somersloop state.
- Color swatches.
- Signs.

### Tier 3

Hybrid with Extend:

- Extend module presets: capture a source building's full topology (buildings, belts, pipes, distributors, junctions, recipes) as a named preset.
- Parametric blueprint-style presets where the topology is reusable across locations.

## Key Design Principles

1. Deterministic: the same preset should produce the same layout.
2. Decoupled from world state: presets should not depend on existing structures.
3. Composable: restore should work with Scaling, Transforms, AutoConnect, and future Extend flows.
4. Fail-safe: invalid fields should degrade gracefully without corrupting panel or hologram state.

## Resolved Design Decisions

| Question | Decision | Notes |
|----------|----------|-------|
| Should presets auto-switch the build gun? | **Yes, auto-switch.** | No confirmation prompt. The preset is deterministic and the user chose to apply it. |
| Should recipes be optional per preset? | **Yes, user-selectable via capture checklist.** | Recipe capture requires a compatible production building. The save UI presents a checklist of all capturable fields. |
| Should presets support hotkeys? | **Not initially.** | Selection via Smart Panel dropdown. Hotkey support can be added later. |
| Should presets be shared/exportable? | **Yes.** | Support explicit import/export. Consider a compact string encoding for easy sharing (e.g., clipboard-friendly encoded preset). |
| Should storage be global or world-specific? | **Both.** | Global config is primary (easy to share/back up). World-specific storage can reference global presets. Backward compatibility via version migration. |

## Compact Sharing Format

Presets should be shareable as a short, clipboard-friendly string. The encoding must be:

- **Compact**: short enough to paste in Discord, game chat, or a forum post.
- **Self-contained**: includes all captured fields plus version.
- **Version-safe**: includes a version prefix so the decoder can migrate or reject incompatible strings.

Recommended format: **Base64-encoded JSON with a version prefix**.

```text
SR1:<base64-encoded-json>
```

- `SR1` is the format version tag (Smart Restore v1). Future format changes increment this (SR2, SR3, etc.).
- The colon separates the version tag from the payload.
- The payload is the preset JSON (excluding metadata like `CreatedAt`/`UpdatedAt`) compressed and Base64-encoded.
- Compression (e.g., zlib/deflate before Base64) is optional for v1 but recommended if presets grow beyond ~200 bytes of JSON.

Example flow:

1. User clicks **Export** → Smart! serializes the preset to JSON, strips metadata, Base64-encodes it, prepends `SR1:`, copies to clipboard.
2. User pastes `SR1:eyJOYW1lIjoiVHJhaW5SYW1wIi...` in Discord.
3. Recipient clicks **Import** → Smart! strips prefix, decodes Base64, parses JSON, validates fields, adds to preset library.

If a future version needs a more compact binary encoding (e.g., for very large Extend module presets), the version prefix allows transparent migration.

## Open Questions

- Should Smart Restore be its own tab in the Smart Panel, or extend the main panel with a Restore/Presets section? (Depends on UE Blueprint layout — see Smart Panel Integration above for pros/cons of each option.)
- Should Tier 2 fields (overclocking, Somersloops) share the same capture checklist, or appear as a separate "advanced" capture group?

## Implementation Notes

- Hook into the Smart Panel state object rather than inventing a parallel state path.
- Serialize and deserialize panel state through a dedicated preset struct.
- Reapply settings through the same setters used by UI controls so validation, clamping, labels, and hologram refresh behavior remain consistent.
- Refresh the hologram after applying the preset.
- Store class and recipe identifiers as stable class paths where possible.
- Treat invalid fields as warnings and skip those fields rather than aborting the entire restore, except for an unavailable building class.

## Suggested Implementation Slices

1. Data model and JSON read/write service.
2. Capture current panel and hologram state into a preset (with capture checklist UI).
3. Apply grid and transform settings from a selected preset.
4. Restore building class with unlock validation and auto-switch build gun.
5. Add Smart Panel preset dropdown and buttons.
6. Add recipe restore validation.
7. Add auto-connect override support.
8. Add compact sharing format (SR1 Base64 export/import).
9. Add version migration hooks.
10. Tier 2: Add overclocking, Somersloop, color swatch, and sign capture.
11. Tier 3: Add Extend module preset capture.
