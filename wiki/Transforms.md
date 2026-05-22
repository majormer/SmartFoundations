# Transforms

Transforms modify where scaled child holograms appear. They are not separate placement systems; they layer on top of [Grid Scaling](Grid-Scaling) and are reused by Scaled Extend.

The current implemented transform set is:

- Spacing
- Steps
- Stagger
- Z-axis Rotation

> Screenshot placeholder: one comparison image with four small labeled examples: spacing gap, stepped row, staggered row, and rotation arc.

## Spacing

Spacing adds extra separation between cells without changing the base buildable size. The base cell size still comes from the buildable size registry; spacing is an additional offset.

Spacing can be adjusted on X, Y, and Z.

Default controls:

- Hold `;` for Spacing mode.
- Use `Num 8`, `Num 5`, or mouse wheel to adjust.
- Press `Num 0` to cycle X, Y, Z.

## Steps

Steps create stair-like elevation. X steps add height as X index changes. Y steps add height as Y index changes. Steps follow the active hologram orientation rather than a fixed world axis.

Default controls:

- Hold `I` for Steps mode.
- Use `Num 8`, `Num 5`, or mouse wheel to adjust.
- Press `Num 0` to toggle X/Y.

## Stagger

Stagger offsets rows and layers:

- X/Y stagger creates horizontal offsets across the grid.
- ZX/ZY stagger creates vertical-layer offsets for leaning or angled multi-layer layouts.

Default controls:

- Hold `Y` for Stagger mode.
- Use `Num 8`, `Num 5`, or mouse wheel to adjust.
- Press `Num 0` to cycle X, Y, ZX, ZY.

## Rotation

Rotation currently uses Z-axis yaw only. It places child holograms along a horizontal arc or radial pattern. X and Y rotation axes are not implemented in current source and should not be treated as active features.

Default controls:

- Hold `,` for Rotation mode.
- Use `Num 8`, `Num 5`, or mouse wheel to adjust.

## Adjustment Units

Current implementation notes:

- Spacing, Steps, and Stagger adjust in 50 Unreal-unit increments through the unified value adjustment path.
- Rotation adjusts in 5 degree increments.

## Verified From

- `docs/Features/Transforms/IMPL_Transforms_CurrentFlow.md`
- `Source/SmartFoundations/Private/Subsystem/SFPositionCalculator.cpp`
- `Source/SmartFoundations/Public/Services/SFGridStateService.h`
- `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`

