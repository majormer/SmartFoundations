---
title: Smart Scaling Current Flow
type: IMPL
date: 2026-04-24
status: Active
category: Features
tags: [scaling, grid, child_holograms, buildable_size_registry]
related: [./DESIGN_Scaling_ChildTypeSelection.md, ../Transforms/IMPL_Transforms_CurrentFlow.md, ../SmartPanel/IMPL_SmartPanel_CurrentFlow.md]
---

# Smart Scaling Current Flow

## Purpose

Scaling is Smart!'s grid placement system. It lets the active build-gun hologram become the parent of an X/Y/Z child-hologram grid, then uses the transform pipeline to position every child before vanilla construction builds the parent and children together.

## Current Status

Scaling is active for supported single-click buildables. It uses vanilla holograms and child holograms rather than swapping the active hologram class. This avoids registration loops and lets vanilla cost aggregation work through `AddChild`.

## Primary Code Files

| File | Role |
|------|------|
| `Source/SmartFoundations/Public/Subsystem/SFSubsystem.h` | Feature facade, scaling input callbacks, grid state accessors |
| `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp` | Active hologram registration, scale handlers, panel opening, child update triggers |
| `Source/SmartFoundations/Public/Services/SFGridStateService.h` | Grid counter mutation and forbidden-value skipping |
| `Source/SmartFoundations/Public/Services/SFGridSpawnerService.h` | Grid regeneration and child-positioning facade |
| `Source/SmartFoundations/Private/Services/SFGridSpawnerService.cpp` | Regenerates children and updates positions |
| `Source/SmartFoundations/Public/Subsystem/SFHologramHelperService.h` | Child hologram lifecycle and parent-child registration |
| `Source/SmartFoundations/Private/Subsystem/SFHologramHelperService.cpp` | Spawns, destroys, and tracks child holograms |
| `Source/SmartFoundations/Public/Data/SFBuildableSizeRegistry.h` | Source of buildable dimensions and scaling eligibility |
| `Source/SmartFoundations/Public/Features/Scaling/FSFGridArray.h` | Pure grid array helper types |
| `Source/SmartFoundations/Public/Features/Scaling/SFScalingTypes.h` | Axis and bounds types |

## Runtime Flow

1. `USFSubsystem::PollForActiveHologram` detects the current build-gun hologram.
2. `RegisterActiveHologram` initializes counter state, resolves buildable size, and prepares helper services.
3. Scale input calls `OnScaleXChanged`, `OnScaleYChanged`, or `OnScaleZChanged`.
4. `ApplyAxisScaling` asks `USFGridStateService` to mutate `GridCounters`.
5. `USFGridSpawnerService::RegenerateChildHologramGrid` adds or removes child holograms to match the requested grid.
6. `USFGridSpawnerService::UpdateChildPositions` uses `FSFPositionCalculator` and the current transform counters to place every child.
7. Vanilla construction builds the parent and its registered children, with costs flowing through the normal hologram child list.

## Copied Factory Settings and Item Conservation

Scaling, Extend, Scaled Extend, and Restore share `FSFFactorySettingsSnapshot` and
`USFRecipeManagementService::QueueFactorySettingsApplication`. Application waits for
the built factory and required inventories to become ready; recipe and inventory
mutation runs only on the construction authority.

Each factory must independently budget Power Shards and Somersloops at application
time. `GetAffordableShardTarget` caps the total installed target to matching items
already installed plus the placing player's current inventory, never above the
requested count. A batch-wide inventory snapshot would let later copies reuse funds
spent by earlier copies. A satisfied or unfunded target skips native inventory mutation.
Insufficient supply permits a partially equipped machine; this is not an all-or-nothing
placement gate and does not add Dimensional Depot support.

**Native contract evidence (#524):** in the CL 502094 Windows Shipping FactoryGame
binary, `AFGBuildableFactory::FillPotentialSlotsInternal` starts at RVA `0x4E5090`,
adds a stack to the machine at `0x4E54F6`, then calls player inventory `Remove` at
`0x4E552C` without first checking supply. Its reference count represents the total
installed target, including matching items already present, not an additional debit.
Calling it with an unfunded target creates items; adding another debit outside it
charges twice. Displaced items still use the existing inventory-return/world-drop
path. Do not re-test the unfunded native call on a real save.

`SmartFoundations.Construction.FactorySettings.ApplyPolicy` covers empty, partial,
existing, surplus, negative, and overflow budgets plus conservation across repeated
copies sharing finite supply. These policy tests do not replace live inventory and
multiplayer validation.

## Passthrough Blueprint Placement Permission

Scaled floor-hole previews and construction-time copies use a manually spawned
`ASFPassthroughChildHologram`. In addition to designer ownership, they must inherit
the spawn-initialized `mCanBePlacedInBlueprintDesigner` value from the same-build-class
parent. `CopyBlueprintPlacementPermissionFrom` copies it after `FinishSpawning` and
before `AddChild` in both `SFHologramHelperService` and `SFScalingSpecExpansion`.
False remains false; this is not permission to bypass designer bounds or connection rules.

Failure evidence (#526): a three-cell pipe-floor-hole preview had
`FGCDNotAllowedInBlueprint` on both scaled holes despite valid designer references;
all three attached pipes were valid. Reducing to one cell at unchanged coordinates
removed that rejection from the original parent. Source inheritance is corrected;
post-fix runtime validation remains outstanding.

## Performance Model (33.4.0, #418)

Three properties keep large grids usable; see the code for the details (each site is thoroughly commented).

- **Cell-based identity.** Every grid child carries a `USFGridCoordComponent` holding its unsigned grid cell (parent = `[0,0,0]`), rather than being identified by its slot in the spawn-order array. `RegenerateChildHologramGrid` runs a reconcile pass each regen (evict out-of-bounds cells, adopt unassigned, compute free cells), so growing/shrinking any axis is a cell set-difference that only touches genuinely-changed cells. The positioning batch and the stackable auto-connect neighbor maps read the cell directly. This is what stopped an inner-axis (Y) grow from re-positioning the whole grid.
- **Drift-proof children.** Grid children hold their own position (they no-op the vanilla parent-propagation `SetHologramLocationAndRotation`); see [DESIGN_Scaling_ChildTypeSelection.md](./DESIGN_Scaling_ChildTypeSelection.md) for which child class each type gets.
- **Time-sliced bulk work.** The preview spawn burst yields at a per-frame budget and continues on the next `USFSubsystem::Tick` (`bSpawnContinuationPending`). In multiplayer, large spec construction on the authority is deferred across server frames (`SFScalingSpecExpansion::ShouldDeferSpecExpansion` / `BeginDeferredSpecExpansion`) so the server keeps servicing the net driver instead of blocking one long frame and timing out clients.

## Counter Model

Scaling dimensions are stored in `FSFCounterState::GridCounters` as an `FIntVector`.

| Axis | Meaning |
|------|---------|
| X | Width/count along the active hologram's local X axis |
| Y | Depth/count along the active hologram's local Y axis |
| Z | Vertical layer count |

Grid counters support negative values to represent direction. The mutation path skips forbidden values `0` and `-1` so the grid never collapses into invalid no-copy states.

## Buildable Size Registry

The size registry is the current source of truth for supported dimensions and scaling eligibility.

| Registry field | Meaning |
|----------------|---------|
| `DefaultSize` | Cell size used for base grid spacing |
| `bSwapXYOnRotation` | Whether dimensions swap on 90 degree rotation |
| `AnchorOffset` | Offset from actor origin to intended placement anchor |
| `bSupportsScaling` | Whether Smart! allows scaling for this buildable |
| `bIsValidated` | Whether the profile was manually verified |

Unknown/modded buildables can fall back to a conservative default profile, but docs and user-facing claims should describe only verified support unless a test has confirmed the case.

## Wall outlets

The six vanilla wall-outlet profiles are explicitly scalable: `Build_PowerPoleWall_C`,
`Build_PowerPoleWall_Mk2_C`, `Build_PowerPoleWall_Mk3_C`, and the corresponding
`Build_PowerPoleWallDouble` variants. Their 100 cm cell size is a default placement
interval, not a claim about physical dimensions. `Validated` remains false until
visual spacing has been verified in-game. The canonical CSV and generated registry
must agree; regenerate with `scripts/gen_size_registry.py` after editing the CSV.

Both recipe holograms (`Holo_PowerSocket_C` and `Holo_PowerSocketDouble_C`) derive
from `AFGPowerPoleWallHologram` / `AFGWallAttachmentHologram`. The existing registry
adapter path enables their grid controls; generic children provide previews,
and the existing construction-spec path reconstructs recipe-native children on authority.
No new class-wide opt-in or snap-validation bypass is introduced by #541.

Wall outlets need an additional parent-side transform guard. In CL 502094 Windows
Shipping, `AFGPowerPoleWallHologram::PostHologramPlacement` directly calls
`SetActorLocation` on every child using the outlet's snap-connection position. This
bypasses the generic child's `SetHologramLocationAndRotation` no-op, even when the
post-placement recursion flag is false. Runtime evidence: the three copies in a
1x4x1 grid all occupied the parent's connector, 80 cm ahead of the parent pivot.

`FSFWallOutletPlacement` brackets that native call with a snapshot/restore of only
direct `SF_GridChild` transforms, matching the transform-preservation pattern used
by Extend belts. The native function still runs exactly once with its original
arguments. Untagged wire companions, parent snap/marker state, child membership,
validation, and construction are not modified. Restoration finishes inside the
placement call, not on a later tick; dead children are ignored.

`SmartFoundations.Scaling.WallOutletPlacement` reproduces the native connector
write against real actor transforms in an isolated editor world, checks signed
horizontal and vertical layouts, and retains an untagged companion as a control.
The editor SDK's native placement function is a stub, so this controlled regression
test does not replace Shipping gameplay and dedicated-server validation.

Power Auto-Connect's `IsPowerPoleHologram` predicate intentionally excludes wall outlets;
enabling their scaling does not enable automatic wall-outlet wiring. Wall-outlet upgrades
and the native wire-insertion workflow remain separate from ordinary grid placement.
The `SmartFoundations.Scaling.WallOutlets` automation checks all six real build classes,
both native hologram families, profile eligibility, grid pitch, and pivot policy. It does
not establish successful placement or per-cell wall attachment in SP/MP; those require
gameplay validation.

## Transform Integration

Scaling is the owner of the transform pipeline. Spacing, Steps, Stagger, and Rotation do not spawn children themselves; they alter the locations that scaling computes for children.

See [../Transforms/IMPL_Transforms_CurrentFlow.md](../Transforms/IMPL_Transforms_CurrentFlow.md).

## Important Caveats

- Scaling operates on the active vanilla hologram and supported child holograms.
- Grid size is practically constrained by performance and validation, even where no strict hard cap is documented.
- Multi-step, drag, or highly specialized vanilla holograms may be unsupported even if their built actor appears in the size registry.
- Older Scaling docs that mention historical monolithic helper files are stale for the current service-based implementation.

## Archived Inputs

Previous Scaling audits and wall-power-pole research were moved to `docs/Archive/2026/features-consolidation/superseded/Scaling/`. The old transform subfolder was consolidated into the Transforms doc and archived separately.
