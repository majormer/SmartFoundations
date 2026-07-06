# Troubleshooting

For broader common questions from Discord and release history, see the [FAQ](FAQ).

## Smart! Controls Do Nothing

Check:

- You are holding the build gun.
- You have an active buildable hologram.
- Smart! controls are still bound under Options > Controls > Mods.
- You are not typing in a search box or another UI.
- The Smart! mod is enabled.

Default panel key: `K`.

> Screenshot placeholder: Smart! controls in the Satisfactory controls menu.

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

When this happens, the Smart! HUD shows a short summary while you aim — for example, *"2 belt connection(s) skipped: too steep"* — so you can see that something didn't connect and why. The rest of the grid still builds; only the connection it couldn't make is left out.

Common reasons:

- Auto-Connect is disabled.
- The tier setting is not right.
- The connector is **too far** away.
- The angle is **too steep** for a valid belt or pipe.
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

## Reporting Issues

When reporting an issue, include:

- What you were trying to build.
- The buildable name.
- The grid size.
- Which Smart feature was active.
- Whether Auto-Connect or Extend was involved.
- Single-player or multiplayer.
- Host or client, if multiplayer.
