# Compatibility and Multiplayer

Smart! is designed to be save-friendly. It places normal Satisfactory buildings and charges normal material costs.

## Supported Game Version

Smart! 35.1.0 targets **Satisfactory 1.2.4 / CL 502094 or newer** and **SML 3.12.x**. Update the game and mod loader before installing. Older game builds are unsupported.

Smart Camera 2.0 and Smart! 35.1 must be updated as a compatible pair when both are installed. Camera also works alone; Smart's building tools do not require Camera. Camera preferences and HUD appearance belong to each local player, not everyone on the server.

## Save Safety

If you remove Smart!, the buildings it placed should remain normal Satisfactory buildings. You lose Smart!'s convenience tools, but the factory should still be loadable.

## Other Mods

Smart! works best with:

- Vanilla buildables.
- Modded buildables that behave like normal Satisfactory buildables.
- Standard placement and hologram behavior.

Some modded or unusual buildables may not scale correctly until Smart! has explicit support for them.

## Multiplayer

Smart! works in multiplayer on dedicated servers. Every Smart! feature — Grid Scaling, Auto-Connect (belts, pipes, and power), Smart Blueprints, Extend, Smart Upgrade, Smart Restore, and Smart Dismantle — works when you play as a client, with normal build costs. Everything Smart! places is a standard, fully replicated game building: other players see it, can use it, and can dismantle it, just as if you had built it by hand.

- **Dedicated servers:** both Windows and Linux are supported.
- **Blueprint Placement Safety Limit** under Building Behavior is enabled by default. For a remote multiplayer client, it blocks a Blueprint grid estimated above 2,000 buildings, counting the original and all copies. You can disable this setting live to attempt a larger placement, with increased freeze, disconnect and failure risk. Normal material, placement and construction-payload checks still apply. Build in smaller sections if a placement is refused.
- **For server admins:** install the same Smart! version on the server and on every client. The Mod Manager keeps these matched when you update.
- **Smart Camera** (the companion mod) also works in multiplayer.

Multiplayer is newer than single-player, so if something behaves differently in a multiplayer session than it does in single-player, that is a bug — please report it on GitHub or Discord, and mention what you were building and whether you were the host or a client.

Each player's session assistance toggle affects only their own building tools. Remote Extend/Restore keeps the listen-server host's preview and settings separate from the client's operation.

## Known Reports Still Under Investigation

- Intermittent rain/dismantle crashes ([#514](https://github.com/majormer/SmartFoundations/issues/514)); protection is improved, but the underlying cause is unresolved.
Third-party building spacing profiles are not generally maintained. Compatible manual spacing is a workaround, not a promise of support for another mod's full building set.

When reporting these, include game/Smart!/SML versions, whether you are a host or client, the layout and settings, other installed mods, and a save or blueprint that reproduces the behavior when available.

## Blueprint Designer

Supported native placements retain Designer ownership so they are captured when the Blueprint is saved. Smart does not remove native Designer restrictions: a building invalid there in vanilla does not become valid through Smart. Consult [Supported Buildings](Supported-Buildings) for the applicable workflows.

---

_Last updated: 2026-11-02 · Smart! v35.1.0 / Smart Camera v2.0.0 (release draft)_
