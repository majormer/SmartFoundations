# Compatibility and Multiplayer

Smart! is designed to remain vanilla-save neutral. It places normal Satisfactory buildables, charges normal material costs, and avoids making save files depend on Smart-only buildable classes for the placed factory.

## Compatibility

Smart! works best with vanilla buildables and mods that use standard Satisfactory placement systems.

Practical rules:

- Verified support comes from current hologram adapters and the buildable size registry.
- Unknown or modded buildables may fall back to conservative behavior.
- Specialized vanilla holograms may require dedicated support.
- Transforms do not make unsupported buildables scalable by themselves.

## Save Safety

Removing Smart! should not prevent the save from loading because Smart! places standard Satisfactory buildables. Smart-only convenience behavior is unavailable without the mod, but placed buildings should remain normal game objects.

## Multiplayer

Smart! is primarily developed for single-player. Multiplayer is under active testing with partial success, but should not be described as fully supported.

Current planning guidance:

- Local input, HUD, arrows, and temporary previews should remain local unless shared preview behavior is deliberately designed.
- Build commits must be server-authoritative.
- Client-originated world mutations should go through validated server paths.
- Remote Call Objects are appropriate for client-to-server requests when an owned RPC object is needed.
- Multicast/shared state should live on replicated subsystems or appropriate replicated actors, not on RCOs.

## Verified From

- `README.md`
- `docs/Features/Multiplayer/PLAN_MultiplayerSupport_Matrix.md`
- `Source/SmartFoundations/Public/Core/Helpers/SFNetworkHelper.h`
- `Source/SmartFoundations/Public/SFRCO.h`

