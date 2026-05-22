# Smart Dismantle

Smart Dismantle is a compatibility layer around Satisfactory's vanilla Blueprint Dismantle grouping.

It is not a custom Smart dismantle panel and it is not a separate radius/network dismantle scanner.

> Screenshot placeholder: vanilla Blueprint Dismantle mode highlighting a Smart-placed multi-building group.

## What It Does

Smart! groups multi-building placements under a vanilla blueprint proxy so Satisfactory can dismantle the group through its existing Blueprint Dismantle path.

Grouping is active for:

- Scaled grid placements with more than one cell.
- Extend placements.

Single vanilla placements are intentionally not grouped.

## Why Use Vanilla Blueprint Proxy

Satisfactory already knows how to:

- Highlight buildables owned by a blueprint proxy.
- Calculate group dismantle refunds.
- Dismantle registered buildables through the vanilla path.
- Preserve proxy references on buildables.

Smart! uses that path instead of inventing a separate dismantle system.

## Caveats

- The proxy is dynamically spawned for the current build session.
- Save/load behavior should be retested whenever this path changes.
- Supporting actors spawned outside the active hologram/proxy window may not join the group.
- There is no Smart-specific refund UI or confirmation panel for dismantling.

## Verified From

- `docs/Features/SmartDismantle/IMPL_SmartDismantle_CurrentFlow.md`
- `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`
- `Reference/FactoryGame/Public/FGBlueprintProxy.h`
- `Reference/FactoryGame/Public/Buildables/FGBuildable.h`

