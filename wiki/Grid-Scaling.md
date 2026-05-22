# Grid Scaling

Grid Scaling is Smart!'s core placement feature. It turns the active build-gun hologram into the parent of an X/Y/Z grid and creates child holograms for the additional buildables.

The preview is shown before placement. When you build, Satisfactory constructs the parent and child holograms through the normal build path, including normal material costs.

> Screenshot placeholder: `5 x 3 x 1` foundation grid preview, with HUD counters visible.

## Axes

| Axis | Meaning |
|------|---------|
| X | Width/count along the active hologram's local X axis |
| Y | Depth/count along the active hologram's local Y axis |
| Z | Vertical layer count |

Grid counters can use negative values to place in the opposite direction. The mutation path skips invalid no-copy states so the grid does not collapse into zero-size placement.

## Supported Buildables

Smart! uses the buildable size registry to decide dimensions and scaling eligibility. The registry stores:

- Default buildable size.
- Whether dimensions swap on 90 degree rotation.
- Placement anchor offset.
- Whether scaling is supported.
- Whether the profile was manually validated.

Unknown or modded buildables may fall back to conservative behavior, but the safest documentation claim is that verified support comes from the registry and active hologram adapters.

## How It Works

1. Smart! detects the active build-gun hologram.
2. It resolves the buildable's size and support profile.
3. Scaling input changes X/Y/Z counters.
4. Smart! regenerates child holograms to match the requested grid.
5. The transform pipeline positions every child.
6. Vanilla construction builds the parent and children.

## Caveats

- Scaling is active for supported single-click buildables.
- Multi-step, drag, spline, or specialized vanilla holograms may be unsupported or handled by specialized Smart code.
- Very large grids can be limited by performance, validation, and material availability.
- Transforms change positions; they do not make an unsupported buildable scalable by themselves.

## Verified From

- `docs/Features/Scaling/IMPL_Scaling_CurrentFlow.md`
- `Source/SmartFoundations/Public/Services/SFGridStateService.h`
- `Source/SmartFoundations/Public/Services/SFGridSpawnerService.h`
- `Source/SmartFoundations/Public/Data/SFBuildableSizeRegistry.h`
- `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`

