# Settings Reference

This page explains what the Smart! settings are for. Names may vary slightly between versions.

> Screenshot placeholder: Smart Settings Form showing the major settings sections.

## Belt Auto-Connect

Use these settings to control belt previews.

- Enable or disable belt Auto-Connect.
- Include distributor-style logistics such as splitters and mergers.
- Choose main belt tier.
- Choose belt tier for machine connections.
- Choose routing style: default, curve, or straight.
- Enable stackable belt support behavior.

## Pipe Auto-Connect

Use these settings to control pipe previews.

- Enable or disable pipe Auto-Connect.
- Choose main pipe tier.
- Choose pipe tier for machine connections.
- Choose routing style: auto, 2D auto, straight, curve, noodle, or horizontal-to-vertical.
- Choose pipe indicator style where available.

## Power Auto-Connect

Use these settings to control power cable previews.

- Enable or disable power Auto-Connect.
- Choose connection mode.
- Set connection range.
- Reserve pole connections so Smart! does not fill every slot.

## Extend

- Enable or disable Extend.
- Enable or disable power copying during Extend where supported.
- Daisy-chain power building-to-building along the lane (once Upgraded Power Connectors is unlocked).
- Daisy-chain power on pole-less sources when starting a fresh manifold.

## Scaling

- Auto-Hold on grid change locks the hologram after you modify the grid, so a large preview doesn't move accidentally. **On by default** (as of 32.1.2) — turn it off here if you'd rather the hologram stay free to reposition, and the vanilla Hold key releases any individual lock.
- **Player Relative Controls** — makes building follow the direction you're looking instead of fixed compass axes: the mouse wheel grows the build toward wherever you're facing, and the numpad becomes a compass (away/toward, right/left, up/down). Applies to scaling, spacing, steps, stagger, and rotation. **Off by default** — your classic controls are unchanged until you turn it on, and the Smart Panel always stays on fixed X/Y/Z. See [Controls](Controls) for the full breakdown. (Added in 34.1.0.)

## Scroll Increments

Found under **Building Behavior** in the mod settings. How much each mouse-wheel notch changes a grid transform. These four increments are shared by Grid scaling, Extend, Restore, and Smart Walking, so tuning them here changes the feel everywhere you scroll to build. (Added in 33.5.3.)

- **Spacing Increment** (m) — meters of spacing added per scroll notch (also Extend spacing and a Walk segment's advance).
- **Steps Increment** (m) — meters of stepping per notch (also a Walk segment's rise).
- **Stagger Increment** (m) — meters of stagger per notch (also a Walk segment's shift).
- **Rotation Increment** (deg) — degrees of rotation per notch (also a Walk segment's turn).

Defaults are 0.5 m and 5°. The distance increments accept 0.1–8 m; rotation accepts 0.5–90°.

## Smart Panel

- Apply Immediately controls whether panel changes take effect as you edit or wait for the Apply button.

## HUD

- Show or hide the HUD.
- Change HUD scale.
- Change HUD position.
- Choose a HUD theme.

## Arrows

- Show or hide direction arrows.
- Enable orbit animation.
- Show or hide X/Y/Z labels.
