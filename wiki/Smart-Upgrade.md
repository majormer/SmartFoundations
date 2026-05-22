# Smart Upgrade

Smart Upgrade scans and upgrades existing logistics and power infrastructure after better tiers are unlocked.

It is handled by a separate Smart Upgrade Panel, not by the main Smart Settings Form.

> Screenshot placeholder: Smart Upgrade Panel showing scan results for belts, lifts, pipes, and power poles.

## Supported Upgrade Families

| Family | Execution | Notes |
|--------|-----------|-------|
| Belt | Supported | Mk1-Mk6, with conveyor chain stabilization |
| Lift | Supported | Mk1-Mk6 |
| Pipe | Supported | Mk1-Mk2, preserves indicator/no-indicator style |
| Pump | Audit/traversal context only | Traversal can cross pumps; execution is not implemented |
| Power Pole | Supported | Mk1-Mk3 |
| Wall Outlet Single | Supported | Mk1-Mk3 |
| Wall Outlet Double | Supported | Mk1-Mk3 |
| Power Tower | Audit/traversal context only | Not upgraded |
| Wire/Power Line | Anchor only | Used as traversal entry into power networks |

## Scan Modes

Smart Upgrade supports:

- Radius scanning.
- Entire-map scan mode with radius value `0`.
- Connected-network traversal from an anchor buildable.

The panel can show counts by family and tier, target-tier choices, material cost preview, and execution controls.

> Screenshot placeholder: traversal scan started from a belt, showing connected network results.

## Execution Model

The current execution path processes replacements synchronously in one frame. Older notes described timer or throttled execution, but current source uses synchronous replacement because partial conveyor topology during async upgrades caused chain actor problems.

Smart Upgrade captures expected connections before destroying actors, performs replacements, repairs connections where possible, and delegates conveyor chain stabilization to the chain actor service.

## Cost and Refunds

For each upgrade target, Smart! computes the target cost minus the old buildable refund.

- If net cost is positive, it deducts from inventory and central storage according to player preference.
- If net cost is negative, it refunds.
- Overflow refunds are spawned in a dismantle crate near the player at completion.

## Chain Triage

The chain system is the highest-risk part of large conveyor upgrades. Current chain triage detects structural issues and can perform limited repair/stabilization actions, but in-game orphan bounce repair is diagnostic-only in the current implementation because tested variants were crash-prone.

For very large conveyor upgrades, save/reload may still be the safest recovery path if chain diagnostics report unsettled orphan state after the operation.

## Verified From

- `docs/Features/SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md`
- `Source/SmartFoundations/Public/Features/Upgrade/SFUpgradeAuditService.h`
- `Source/SmartFoundations/Public/Features/Upgrade/SFUpgradeExecutionService.h`
- `Source/SmartFoundations/Public/Features/Upgrade/SFUpgradeTraversalService.h`
- `Source/SmartFoundations/Public/Services/SFChainActorService.h`

