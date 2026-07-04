# Smart Upgrade

Smart Upgrade helps replace older infrastructure after you unlock better tiers.

Use it when you want to upgrade many belts, lifts, pipes, or power poles without replacing each one by hand.

> Screenshot placeholder: Smart Upgrade Panel showing scan results and target tier selection.

## What It Can Upgrade

| Type | Supported |
|------|-----------|
| Conveyor belts | Yes, Mk1-Mk6 |
| Conveyor lifts | Yes, Mk1-Mk6 |
| Pipes | Yes, Mk1-Mk2 |
| Power poles | Yes, Mk1-Mk3 |
| Wall outlets | Yes, Mk1-Mk3 |
| Pipeline pumps | No |
| Power towers | Scan/traversal context only |

## Basic Flow

1. Open the Smart Upgrade Panel with `K` while holding a belt, lift, pipe, or wire/power line.
2. Choose a scan mode.
3. Scan nearby items or a connected network.
4. Pick the family and target tier — and, on a network scan, optionally a single source tier to upgrade just that tier.
5. Review the material cost.
6. Run the upgrade.

Power poles and wall outlets still open the normal Smart Panel when held so they can be scaled. To upgrade them, open Smart Upgrade from a wire/power line or use scan modes.

> Screenshot placeholder: traversal scan from a belt showing connected belt/lift results.

## Scan Modes

Smart Upgrade can scan:

- A radius around you.
- The whole map by using radius `0`.
- A connected network starting from an anchor buildable.

Network traversal is useful when you want to upgrade one belt line, pipe line, or power network without touching unrelated nearby items. The scan anchors on whatever you aim at — whatever tier you happen to be holding — follows pipes through cross and T junctions, and refreshes its results automatically after an upgrade (including for players on a multiplayer server).

## Tier-to-Tier Targeting

A network scan lists the tiers it found in the connected run, and lets you choose which to upgrade:

- Click a specific tier — for example **Mk.2** — to upgrade **only** that tier to your target. Bump your Mk.2 belts to Mk.3 and leave the Mk.4s untouched.
- Pick **All tiers** to upgrade everything below the target at once (the original behavior).

This matches the tier control the radius scan already had.

## Costs And Refunds

Smart Upgrade calculates the cost of the new item and subtracts the refund from the old item. It uses your inventory and central storage according to your normal preferences.

If there is overflow from refunds, Smart! can spawn a dismantle crate near you.

## Conveyor Warning

Large conveyor upgrades are complicated because Satisfactory groups belts and lifts into conveyor chain actors internally. Smart! includes stabilization and diagnostics, but very large upgrades may still need a save/reload if the game reports unsettled chain state afterward.
