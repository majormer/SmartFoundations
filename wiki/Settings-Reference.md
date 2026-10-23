# Settings Reference

This page explains what the Smart! settings are for. Names may vary slightly between versions.

Find them from the **Main Menu → Mods → Smart!**. Changes made here **persist between saves and between sessions**. (Keybinds are separate — see [Controls](Controls).)

## Belt Auto-Connect

Use these settings to control belt previews.

- Enable or disable belt Auto-Connect.
- Include distributor-style logistics such as splitters and mergers.
- Choose main belt tier.
- Choose belt tier for machine connections.
- Choose routing style: default, curve, or straight.
- Choose machine connection height handling: **Belts Only**, **Lift at Distributor**, or **Lift at Building**. Belts Only is the default.
- Enable stackable belt support behavior.

## Pipe Auto-Connect

Use these settings to control pipe previews.

- Enable or disable pipe Auto-Connect.
- Choose main pipe tier.
- Choose pipe tier for machine connections.
- Choose routing style: auto, 2D auto, straight, curve, noodle, or horizontal-to-vertical.
- Choose pipe indicator style where available.

Connection preferences are live: tier, style and routing changes refresh the preview immediately. The placement Apply option controls grid geometry separately.

## Hypertube Auto-Connect

A separate switch for hypertube previews, independent of Pipe Auto-Connect.

- **Hypertube Auto-Connect** — enable or disable auto-connecting hypertubes. **On by default.** (Added in 33.1.0.)
- Choose hypertube routing mode.

## Power Auto-Connect

Use these settings to control power cable previews.

- Enable or disable power Auto-Connect.
- Choose connection mode.
- Set connection range.
- Reserve pole connections so Smart! does not fill every slot.
- **Daisy-Chain Power When Scaling** — when Upgraded Power Connectors are unlocked, scaling a factory or generator along Smart's X axis wires each copy directly to the next one instead of running a pole to each, matching the common manifold layout without a separate Extend pass. The Smart Panel can override it for the current build, and the HUD reports the effective choice. It respects each connector's available capacity. **On by default.** (Added in 34.2.0.)

## Blueprint Auto-Connect

A separate switch for the seams between scaled copies of your **own** blueprints — independent of Belt and Pipe Auto-Connect above, so blueprint copies can keep wiring together even when regular auto-connect is off.

- **Blueprint Seam Auto-Connect** — enable or disable auto-connecting the belts, pipes, Hypertubes, railway tracks, directed Vehicle Paths, and eligible power connections between scaled blueprint copies. **On by default.** (Added in 34.2.0.)

Power seam connections additionally require **Power Auto-Connect**. Each cable uses a matching power-pole socket with available capacity; internal wires and the two faces of a wall outlet count separately.

## Blueprint Spacing Defaults

Under **Building Behavior**, **Blueprint Default X/Y/Z Spacing (m)** choose the starting gap for each axis of a new blueprint build session. Each defaults to **1 m** and accepts **0-100 m**. Zero allows flush tiling; very short gaps may leave no room for belt or pipe seams.

Panel spacing overrides last for the current blueprint session, including repeated placements. Selecting a different blueprint or holstering starts a fresh session with your defaults. Eligible unresolved lift tails can align stacked copies using one shared Z spacing and native-valid belt slopes. Without those tails, default Z spacing stays in use. See [Blueprints](Blueprints). Settings are read live; no world reload is needed.

## Auto-Connect Behavior

- **Nearby Logistics Range** — how far splitters, mergers, pipe junctions, and pipe floor holes reach toward factory ports when auto-connecting. Lower it to keep Auto-Connect from reaching into neighboring factory groups. **Defaults to 25 m** (the previous fixed behavior). (Added in 34.2.0.)

## Extend

- Enable or disable Extend.
- Enable or disable power copying during Extend where supported.
- Daisy-chain power building-to-building along the lane (once Upgraded Power Connectors is unlocked).
- Daisy-chain power on pole-less sources when starting a fresh manifold.

## Scaling

- Auto-Hold on grid change locks the hologram after you modify the grid, so a large preview doesn't move accidentally. **On by default** (as of 32.1.2) — turn it off here if you'd rather the hologram stay free to reposition, and the vanilla Hold key releases any individual lock.
- **Player Relative Controls** — makes building follow the direction you're looking instead of fixed compass axes: the mouse wheel grows the build toward wherever you're facing, and the numpad becomes a compass (away/toward, right/left, up/down). Applies to scaling, spacing, steps, stagger, and rotation. **Off by default** — your classic controls are unchanged until you turn it on, and the Smart Panel always stays on fixed X/Y/Z. See [Controls](Controls) for the full breakdown. (Added in 34.1.0.)
- **Tap to Toggle Transform Modes** — tap a transform key (Spacing, Steps, Stagger, Rotation, or Recipe on a factory) to switch that mode on; tap it again to switch it off; tap a different one to change modes. Made for controllers, the Steam Deck, and accessibility setups — Steam Input radial/touch menus send quick taps and cannot hold a key. Holstering, changing buildings, or opening a panel always releases the mode. **Off by default** — the classic hold-to-use behavior is untouched until you turn it on, and turning it back off restores it instantly. See [Controller & Steam Deck](Controller-and-Steam-Deck) for radial-menu setups. (Added in 34.2.0.)

## Scroll Increments

Found under **Building Behavior** in the mod settings. How much each mouse-wheel notch changes a grid transform. These four increments are shared by Grid scaling, Extend, Restore, and Smart Walking, so tuning them here changes the feel everywhere you scroll to build. (Added in 33.5.3.)

- **Spacing Increment** (m) — meters of spacing added per scroll notch (also Extend spacing and a Walk segment's advance).
- **Steps Increment** (m) — meters of stepping per notch (also a Walk segment's rise).
- **Stagger Increment** (m) — meters of stagger per notch (also a Walk segment's shift).
- **Rotation Increment** (deg) — degrees of rotation per notch (also a Walk segment's turn).

Defaults are 0.5 m and 5°. The distance increments accept 0.1–8 m; rotation accepts 0.5–90°.

## Smart Panel

- **Apply placement immediately** controls grid/transform geometry. When off, Apply updates the preview; closing discards unapplied placement edits.
- **Apply segment edits immediately** is the corresponding Walking geometry control.
- Connection preferences, recipes and applicable native building controls remain live and are labeled separately.
- A manually moved Panel remembers its position across saves; use the position reset to restore it.

## Smart Upgrade

- **Preserve dismantle groups** defaults to On. Supported replacements keep their original group; turning it off skips Smart's transfer without stripping native membership.

## HUD

- Show or hide the HUD.
- Change HUD scale.
- Change HUD position.
- Choose one of the six HUD themes.
- Enter **Edit HUD layout after Resume**, then move/resize, preview, save or cancel.
- **Reset HUD Appearance** restores defaults. HUD appearance and Panel positions apply to all saves locally; Camera scope does not change them.

## Smart Camera 2.0 scopes

In **Mods → Smart! Camera**, views appear automatically under their supplying mod and camera names, including inactive views. The built-in build-gun view has an independent enable control. Per-view position is normalized 0–1, resolution is 64–2048 pixels per axis, scale is 0.1–2, capture rate is 5–60/sec and brightness is −3 to +3 EV. Applicable building-camera distances are 5–50 m. Camera settings apply immediately; views hidden by menus update when you Resume. **All saves** patches only the edited field across global and existing saved profiles for that camera, including the current world. **This save** changes only the current native save profile. Unrelated fields and other cameras remain intact.

**Use mod suggestions** in All saves resets that camera across saves. **Use global settings** in This save removes only the current override. Framing/distance reset changes only those two fields. Layout edits preview until the shared **Done / Lock**; Cancel discards pending layout/HUD changes. See [Smart Camera](Smart-Camera).

## Arrows

- Show or hide direction arrows.
- Enable orbit animation.
- Show or hide X/Y/Z labels.

---

_Last updated: 2026-10-20 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
