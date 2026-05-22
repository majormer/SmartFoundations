# Smart Panel and HUD

Smart! has three user-facing UI surfaces:

- Smart Settings Form
- Smart Upgrade Panel
- HUD overlay

There is not currently one single unified `SmartPanel` class in source. The user-facing panel behavior is split between widgets and services.

> Screenshot placeholder: Smart Settings Form open while aiming at a buildable hologram.

## Smart Settings Form

Open the Smart Settings Form with `K`.

Current responsibilities:

- Grid X/Y/Z controls.
- Spacing, Steps, Stagger, and Rotation controls.
- Apply-immediately versus Apply-button workflow.
- Large-grid confirmation.
- Recipe selection and recipe details.
- Belt Auto-Connect settings.
- Pipe Auto-Connect settings.
- Power Auto-Connect settings.

The settings form caches the last applied state so cancel/revert behavior can restore the previous state when closed without applying.

> Screenshot placeholder: Smart Settings Form with Auto-Connect sections visible.

## Smart Upgrade Panel

The Smart Upgrade Panel is separate from the Smart Settings Form. It handles upgrade scans, cost preview, target tier selection, execution, and chain triage actions.

See [Smart Upgrade](Smart-Upgrade).

> Screenshot placeholder: Smart Upgrade Panel after a radius scan, showing family rows and upgrade counts.

## HUD Overlay

The HUD overlay displays current Smart counters while building. It can show:

- Grid values.
- Active transform values.
- Selected axes.
- Smart mode state.

The HUD is rendered by code-backed UMG classes and is suppressed while modal/panel widgets are open.

> Screenshot placeholder: HUD overlay while spacing mode is active, with a multi-cell hologram preview visible.

## Verified From

- `docs/Features/SmartPanel/IMPL_SmartPanel_CurrentFlow.md`
- `Source/SmartFoundations/Public/UI/SmartSettingsFormWidget.h`
- `Source/SmartFoundations/Private/UI/SmartSettingsFormWidget.cpp`
- `Source/SmartFoundations/Public/UI/SmartUpgradePanel.h`
- `Source/SmartFoundations/Private/UI/SmartUpgradePanel.cpp`
- `Source/SmartFoundations/Public/HUD/SFHudWidget.h`
- `Source/SmartFoundations/Private/HUD/SFHudWidget.cpp`
- SMLMCP widget inspection: `Smart_SettingsForm_Widget`, `Smart_UpgradePanel_Widget`

