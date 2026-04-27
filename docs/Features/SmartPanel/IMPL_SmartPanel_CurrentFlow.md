---
title: Smart Panel Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [smart_panel, settings_form, upgrade_panel, hud, ui]
related: [../SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md, ../AutoConnect/IMPL_AutoConnect_CurrentFlow.md, ../Transforms/IMPL_Transforms_CurrentFlow.md]
---

# Smart Panel Current Flow

## Purpose

This document describes the current Smart! UI surfaces commonly referred to as the panel. There is no single unified `SmartPanel` class in the codebase. The implementation is split across the Smart Settings Form, Smart Upgrade Panel, and HUD overlay.

## Current Status

| Surface | Status | Primary class |
|---------|--------|---------------|
| Smart Settings Form | Active | `USmartSettingsFormWidget` |
| Smart Upgrade Panel | Active | `USmartUpgradePanel` |
| HUD overlay | Active | `USFHudWidget` and `USFHudService` |
| Unified SmartPanel class | Not implemented | Not present in current code |

## Primary Code Files

| File | Role |
|------|------|
| `Source/SmartFoundations/Public/UI/SmartSettingsFormWidget.h` | Settings form widget bindings and state |
| `Source/SmartFoundations/Private/UI/SmartSettingsFormWidget.cpp` | Settings form behavior, apply/cancel, recipe details, AutoConnect controls |
| `Source/SmartFoundations/Public/UI/SmartUpgradePanel.h` | Upgrade panel widget bindings and state |
| `Source/SmartFoundations/Private/UI/SmartUpgradePanel.cpp` | Radius audit, network traversal, cost display, execution, chain triage UI |
| `Source/SmartFoundations/Public/HUD/SFHudWidget.h` | Programmatic HUD widget |
| `Source/SmartFoundations/Private/HUD/SFHudWidget.cpp` | HUD theme and text rendering |
| `Source/SmartFoundations/Public/Services/SFHudService.h` | HUD content manager |
| `Source/SmartFoundations/Private/Services/SFHudService.cpp` | Counter text generation and HUD suppression |
| `Source/SmartFoundations/Public/Config/Smart_ConfigStruct.h` | User-facing config fields |
| `Source/SmartFoundations/Private/Input/SFInputRegistry.cpp` | Input binding for toggling the settings form |

## Settings Form

The Smart Settings Form is the general build configuration surface. It is a `UFGInteractWidget` and reads/writes the active `FSFCounterState` through `USFSubsystem`.

Current responsibilities:

- Grid X/Y/Z controls and direction toggles.
- Spacing, Steps, Stagger, and Rotation inputs.
- Apply-immediately versus Apply-button workflow.
- Large-grid confirmation.
- Recipe selection and recipe detail display.
- Belt AutoConnect controls.
- Pipe AutoConnect controls.
- Power AutoConnect controls.

The form supports cancel/revert behavior by caching the last applied state and restoring it when the user closes without applying.

## Upgrade Panel

The Smart Upgrade Panel is a separate widget for SmartUpgrade. It is not the same UI surface as the settings form.

Current responsibilities:

- Radius audit scanning.
- Entire-map scan mode with radius value `0`.
- Network traversal scanning from an anchor buildable.
- Family/tier row selection.
- Target-tier dropdown population based on family and unlocks.
- Cost preview.
- Upgrade execution via `USFUpgradeExecutionService`.
- Chain triage detect/repair actions through `USFChainActorService`.

See [../SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md](../SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md).

## HUD Overlay

The HUD overlay is a programmatic UMG widget. It is not a Blueprint-only panel. `USFHudService` prepares the counter/mode text, while `USFHudWidget` renders it and handles theme/scale behavior.

Current HUD responsibilities:

- Show grid counters and active mode state while building.
- Show transform values and selected axes.
- Respect HUD suppression while modal/panel widgets are open.
- Render with native font support and selectable themes.

## Related Archived UI Docs

Older docs for GridCounters, HintBar, Interface, and Recipe were moved to `docs/Archive/2026/features-consolidation/needs-home/`. They are not deleted because they contain useful UI details, but their content overlaps the Settings Form, HUD, and input systems. A future structure could place them under `docs/UI/` or split them between `docs/Features/SmartPanel/` and `docs/Operations/Input/`.
