# Troubleshooting

For broader common questions from Discord and release history, see the [FAQ](FAQ).

## Smart! Controls Do Nothing

Check:

- You are holding the build gun.
- You have an active buildable hologram.
- Smart! controls are still bound under Options > Keybindings > "Smart! Mod Controls".
- You are not typing in a search box or another UI.
- The Smart! mod is enabled.

Default panel key: `K`.

## Mouse Wheel Rotates Instead Of Scaling

That is expected when no Smart mode is active. Smart! leaves mouse wheel to vanilla rotation unless you are holding a Smart modifier or mode key.

To scale with mouse wheel:

- Hold `X` for X.
- Hold `Z` for Y.
- Hold `X + Z` for Z.

To adjust transforms with mouse wheel:

- Hold `;` for Spacing.
- Hold `I` for Steps.
- Hold `Y` for Stagger.
- Hold `,` for Rotation.

## Auto-Connect Skipped Something

Auto-Connect skips connections when it cannot safely make them.

When this happens, the Smart! HUD shows a short summary while you aim — for example, *"2 belt connection(s) skipped: too steep"* — so you can see that something didn't connect and why. Some Auto-Connect paths omit the invalid connection; workflows that require a complete validated route can block the whole placement. Read the current preview message and quote.

Belts and pipes connect anywhere the game itself would let you place one by hand, so "too steep" or "invalid shape" now only appears when the conveyor or pipe genuinely can't be routed — not just because the connectors are at an awkward angle. If a connection is skipped, it usually means the same manual belt or pipe wouldn't build either.

Common reasons:

- Auto-Connect is disabled.
- The tier setting is not right.
- The connector is **too far** away.
- The belt is **too steep** to build (the game itself would reject the conveyor).
- The belt shape is **invalid** (the routed curve can't be built).
- The connector direction is incompatible.
- A **lane** between distributors couldn't be completed.
- The power pole has no free connection slots.

If a connection is skipped as too steep or too far, moving the grid a little further from the building, or reducing the height offset between levels, usually brings it into range. Try placing a smaller test layout to see which part is confusing the preview.

## Extend Does Not Start

Check:

- Extend is enabled.
- You are aiming at an existing source building.
- You are holding a matching buildable.
- There is room in the direction Smart! is trying to extend.
- You did not double-tap `Num 0` to disable Smart Auto-Connect and Extend for the session.

## Smart Upgrade Feels Stuck Or Risky

For large belt and lift upgrades, wait a moment after the upgrade finishes. Conveyor networks can take time to settle.

If diagnostics still look bad after a very large upgrade, save and reload before trying more repair actions.

## Walking is unavailable while holding a pipe or belt

Close the Panel, equip the matching **stackable support**, aim at an open existing endpoint, then reopen **K → Walking**. Check the Resume summary. Do not place a new starting support first. Wall/floor attachments and ambiguous or occupied endpoints cannot start a resumed bus. See [Smart Walking](Smart-Walking).

## Camera Enabled or layout seems unchanged

Native menus hide camera pictures. Resume after changing Enabled. In Camera 2.0, an explicit **All saves** edit includes saved overrides for the current world; choose **This save** for a local exception. Menu settings save immediately, while dragged layout/HUD changes require **Done / Lock**. The shared Cancel action discards pending layout edits. No world reload is required for these settings.

## Reporting Issues

When reporting an issue, include:

- What you were trying to build.
- The buildable name.
- The grid size.
- Which Smart feature was active.
- Whether Auto-Connect or Extend was involved.
- Single-player or multiplayer.
- Host or client, if multiplayer.

---

_Last updated: 2026-10-20 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
