# Smart Upgrade

Smart Upgrade helps replace older infrastructure after you unlock better tiers.

Use it when you want to upgrade many belts, lifts, pipes, or power poles without replacing each one by hand.

## What It Can Upgrade

| Type | Supported |
|------|-----------|
| Conveyor belts | Yes, Mk1-Mk6 |
| Conveyor lifts | Yes, Mk1-Mk6 |
| Pipes | Yes, Mk1-Mk2 |
| Power poles | Yes, Mk1-Mk3 |
| Wall outlets | Yes, Mk1-Mk3 |
| Miners | Upgrade only; retain the native resource node |
| Power towers | Scan/traversal context only |

## Basic Flow

1. Open **K → Upgrade** with a supported building held. Belts, pipes and wire/power lines can open this task directly.
2. Choose a scan mode.
3. Scan nearby items or a connected network.
4. Pick the family and target tier — and, on a network scan, optionally a single source tier to upgrade just that tier.
5. Review the material cost.
6. Run the upgrade.

Power poles and wall outlets still open the normal Smart Panel when held so they can be scaled. To upgrade them, open Smart Upgrade from a wire/power line or use scan modes.

## Scan Modes

Smart Upgrade can scan:

- A radius around you.
- The whole map by using radius `0`.
- A connected network starting from an anchor buildable.

Network traversal is useful when you want to upgrade one belt line, pipe line, or power network without touching unrelated nearby items. For **Network**, press **Select Target**, then click the intended buildable in the world outside the Panel. The Panel itself does not pass clicks through. One target click finishes selection; Escape cancels it. An invalid hit keeps the previous target. Selecting a new target clears the old scan and quote, so scan again before upgrading. The held tier does not choose the target network. Traversal follows pipes through cross and T junctions and refreshes after an upgrade.

## Tier-to-Tier Targeting

A network scan lists the tiers it found in the connected run, and lets you choose which to upgrade:

- Click a specific tier — for example **Mk.2** — to upgrade **only** that tier to your target. Bump your Mk.2 belts to Mk.3 and leave the Mk.4s untouched.
- Pick **All tiers** to bring every differing belt, lift, or pipe tier to the selected target. This can upgrade some tiers and downgrade others in the same run.

This matches the tier control the radius scan already had.

## Downgrading Logistics

Choose a lower unlocked target tier to downgrade belts, conveyor lifts, or pipes. A specific source-tier row changes only that tier; **All tiers** changes every differing logistics tier in the selected network.

Power poles and wall outlets remain upgrade-only, including in an All tiers selection. Downgrades use the same replacement-cost and refund accounting below.

## Costs And Refunds

Smart Upgrade calculates the cost of the new item and subtracts the refund from the old item. It uses your inventory and central storage according to your normal preferences.

If there is overflow from refunds, Smart! can spawn a dismantle crate near you.

## Keep dismantle groups

**Preserve dismantle groups** is on by default. Supported replacements retain their original Smart group, including groups with only one building. Native Designer ownership is retained inside a Blueprint Designer. Turning the option off skips Smart's membership transfer; it does not strip membership already preserved by native replacement.

Miners and power/capacity families are upgrade-only. Logistics downgrades remain available for belts, lifts and pipes. A rejected replacement or insufficient resources leaves the original in place.

## Conveyor Warning

Large conveyor upgrades are complicated because Satisfactory groups belts and lifts into conveyor chain actors internally. Smart! includes stabilization and diagnostics, but very large upgrades may still need a save/reload if the game reports unsettled chain state afterward.

## In-game examples

![A selected Radius result shows the target tier and material settlement before Upgrade.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/upgrade-materials.png)

A selected Radius result shows the target tier and material settlement before Upgrade.

![Network Upgrade has a selected existing belt and populated scan results. Scanning spends no materials.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/upgrade-network.png)

Network Upgrade has a selected existing belt and populated scan results. Scanning spends no materials.

---

_Last updated: 2026-10-20 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
