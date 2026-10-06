# Compatibility and Multiplayer

Smart! is designed to be save-friendly. It places normal Satisfactory buildings and charges normal material costs.

## Supported Game Version

Smart! 34.4.0 requires **Satisfactory 1.2.4 / CL 502094 or newer** and **SML 3.12.x**. Update the game and mod loader before installing. Older game builds are unsupported.

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
- **Smart Blueprints size ceiling:** a single blueprint-grid placement on a server is capped (roughly a couple of thousand buildings, or a few hundred connecting belts/pipes, whichever comes first). Scale past it and Smart! refuses the placement — the HUD tells you to build in smaller sections, and the grid stays live so you can. The safe construction-payload limit also applies in single-player and listen-server sessions. Disable [Smart Assistance](Smart-Assistance) and select the recipe again to place a large vanilla blueprint normally.
- **Large grids build progressively:** committing a very large scaled build constructs it in the background over a few moments, so the server stays responsive and no one is disconnected while it builds.
- **For server admins:** install the same Smart! version on the server and on every client. The Mod Manager keeps these matched when you update.
- **Smart Camera** (the companion mod) also works in multiplayer.

Multiplayer is newer than single-player, so if something behaves differently in a multiplayer session than it does in single-player, that is a bug — please report it on GitHub or Discord, and mention what you were building and whether you were the host or a client.

Each player's session assistance toggle affects only their own building tools. Remote Extend/Restore keeps the listen-server host's preview and settings separate from the client's operation.

## Known Reports Still Under Investigation

- Intermittent rain/dismantle crashes ([#514](https://github.com/majormer/SmartFoundations/issues/514)); protection is improved, but the underlying cause is unresolved.
- Overlapping/doubled pipes in the reporter layout ([#551](https://github.com/majormer/SmartFoundations/issues/551)); preview refresh fixes do not prove this case resolved.
- Custom Industrial Evolution placement/scaling ([#552](https://github.com/majormer/SmartFoundations/issues/552)).

When reporting these, include game/Smart!/SML versions, whether you are a host or client, the layout and settings, other installed mods, and a save or blueprint that reproduces the behavior when available.

## Blueprint Designer

Smart! works inside the Blueprint Designer just like it does in the open world: scale grids, Auto-Connect belts and pipes, and use Extend and Scaled Extend on buildings in the designer. Everything Smart! builds is captured into the saved blueprint, so your blueprints come out complete.

---

_Last updated: 2026-10-06 · Smart! v34.4.0_
