# Extend

Extend copies an existing layout from a source building into a new adjacent placement. Use it when one factory module already works and you want Smart! to continue it.

Extend is automatic: Smart! looks for a compatible source while you aim the current hologram at an existing building.

> Screenshot placeholder: one completed constructor module beside a preview clone created by Extend.

## What Extend Can Capture

Current implementation can capture topology around a source and clone supported parts of it, including:

- The source building.
- Connected distributors.
- Belts and lifts.
- Pipes and junctions.
- Supported attachments.
- Recipes where code has explicit support.
- Power wiring where source topology and capacity allow it.

## Basic Flow

1. Aim at a compatible source building while holding a matching hologram.
2. Smart! validates the source and available direction.
3. Smart! captures topology around the source.
4. It generates a clone topology offset from the source.
5. Child holograms preview the clone.
6. Vanilla construction builds the parent and children.
7. Smart! performs post-build wiring and stabilization.

## Scaled Extend

Scaled Extend layers [Grid Scaling](Grid-Scaling) on top of Extend. It uses the same transform state for clone and row offsets.

Implemented transform inputs for Scaled Extend include:

- Spacing.
- Steps.
- Stagger where allowed by current mode checks.
- Z rotation.

X/Y rotation axes are not implemented in the current transform pipeline.

## Direction Cycling

When Extend is active and no modal mode or modifier is active, mouse wheel cycles Extend direction.

## Caveats

- The held hologram must match the intended source class or family.
- Post-build wiring is deferred because many vanilla components are not ready at actor-spawn time.
- Large or dense belt clones share the same chain actor fragility as other conveyor-heavy operations.
- Floor holes, pumps, valves, and passthrough behavior should be trusted from current topology code, not old tracker notes.

## Verified From

- `docs/Features/Extend/IMPL_Extend_CurrentFlow.md`
- `Source/SmartFoundations/Public/Features/Extend/SFExtendService.h`
- `Source/SmartFoundations/Public/Features/Extend/SFExtendTopologyService.h`
- `Source/SmartFoundations/Public/Features/Extend/SFManifoldJSON.h`
- `Source/SmartFoundations/Public/Features/Extend/SFWiringManifest.h`
- `Source/SmartFoundations/Private/Subsystem/SFSubsystem.cpp`

