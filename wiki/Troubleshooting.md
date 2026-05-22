# Troubleshooting

## Smart! Does Not Respond To Controls

Check:

- You are holding the build gun.
- A buildable hologram is active.
- Keybinds are still assigned under Satisfactory Options > Controls > Mods.
- You are not in a modal state that routes controls to a different feature.

Default Smart Panel key: `K`.

> Screenshot placeholder: Controls menu with Smart! mod controls visible.

## Mouse Wheel Rotates Instead Of Scaling

With no Smart mode active, mouse wheel is left to vanilla unless a Smart scale modifier is held.

To use mouse wheel for Smart scaling:

- Hold `X` to adjust X.
- Hold `Z` to adjust Y.
- Hold `X + Z` to adjust Z.

To use mouse wheel for transforms, hold the relevant modal key:

- `;` for Spacing.
- `I` for Steps.
- `Y` for Stagger.
- `,` for Rotation.

## Auto-Connect Did Not Create A Belt, Pipe, Or Wire

Auto-Connect is intentionally conservative. It may skip a connection if:

- The target connector is not compatible.
- The distance is invalid.
- The angle or route is invalid.
- A pole is at capacity or reserved slots would be exceeded.
- The selected tier or routing mode cannot make a valid connection.

Use the Smart Panel to verify Auto-Connect settings and tiers.

## Extend Does Not Activate

Check:

- Extend is enabled in settings.
- You are aiming at a compatible source building.
- The held hologram matches the source class or family.
- Smart! has not been disabled for the current session by double-tapping `Num 0`.

## Smart Upgrade Reports Chain Issues

Large conveyor upgrades can leave chain actors settling for a while. Current chain triage is intentionally cautious, and some orphan repair paths are diagnostic-only because in-game repair variants were crash-prone during investigation.

If a very large conveyor upgrade still reports chain issues after settling, save/reload may be safer than forcing in-game repair.

## Verified From

- `docs/Features/AutoConnect/IMPL_AutoConnect_CurrentFlow.md`
- `docs/Features/Extend/IMPL_Extend_CurrentFlow.md`
- `docs/Features/SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md`
- `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`

