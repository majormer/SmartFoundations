# Frequently Asked Questions

This FAQ is based on current Smart! behavior and common Discord questions from the rebuild era, starting with the October 21, 2025 public release for Satisfactory 1.1.

Smart! was rebuilt in phases. If you remember a feature being missing in late 2025, it may have returned since then.

## Is Smart! Back To The Old Smart?

Yes. The original Smart feature set is back, but it returned piece by piece during the rebuild. Some later releases also added new features that original Smart never had.

Those features were not simply copied forward. As each feature came back, it was reviewed, rebuilt for the current Satisfactory/Unreal version, and often enhanced. For example, Auto-Connect no longer just recreates the old straight-line belt behavior: Smart! now generates proper belt splines, and the Auto-Connect family has grown to include pipe and power auto-connect.

- October 21, 2025: Smart! returned with grid scaling, spacing, steps, stagger, HUD, arrows, and keybind support.
- November 18, 2025: Belt Auto-Connect returned.
- November 22-25, 2025: Pipe and Power Auto-Connect followed.
- December 13, 2025: Extend returned.
- February 21, 2026: Smart Upgrade and Smart Dismantle were added as new features.
- February 23, 2026: Scaled Extend was added as a new feature, combining scaling and Extend in a way original Smart did not support.
- May 3, 2026: Smart Restore Enhanced added shareable presets and Extend topology restore.

If an old comment says "Smart does not have Extend yet" or "pipes are not supported yet," check the date. That was true during part of the rebuild, but not for the current release.

## Is Anything From The Original Smart Still Missing?

No. As far as the original Smart feature set goes, the current Smart! has restored the major features players remember: scaling, transforms, auto-connect, Extend, Restore-style presets, and companion camera support.

Multiplayer on dedicated servers — the last big piece — shipped in 32.0.0.

## Is Smart! A Cheat Mod?

No. Smart! uses normal Satisfactory buildings and normal material costs.

The mod is meant to remove repetitive placement work, not bypass progression. It should only build things you could place manually, and shared presets are checked against your current unlocks before they can be imported or applied.

## Can I Remove Smart! From A Save?

Smart! is designed to leave vanilla buildables behind. Your factory should not depend on custom Smart-only buildings to load.

Do save before changing any mod setup. If you used Smart Camera, remove that companion mod separately.

## Does Smart! Work In Multiplayer?

Yes. Smart! works in multiplayer on dedicated servers (Windows and Linux). Every feature — scaling, Auto-Connect, Extend, Smart Upgrade, Smart Restore, and Smart Dismantle — works when you play as a client, with normal build costs, and everything Smart! places is a standard replicated building that other players can see, use, and dismantle.

Install the same Smart! version on the server and on every client; the Mod Manager keeps them matched. Multiplayer is newer than single-player, so if something behaves differently in a session than it does solo, please report it. See [Compatibility and Multiplayer](Compatibility-and-Multiplayer).

## Will Smart! Work With Satisfactory 1.2?

Yes. Smart! runs on Satisfactory 1.2 — it was rebuilt for the 1.2 engine and is the current supported version. If you are coming from a 1.1 build, just update Smart! through the Mod Manager.

## Why Doesn't Smart! Load On My Dedicated Server After Updating To 1.2?

As of Satisfactory 1.2, every mod except SML must live in `FactoryGame/Mods/GameFeatures/` instead of the flat `FactoryGame/Mods/` layout 1.1 used. The Satisfactory Mod Manager moves your existing mods into the new folder automatically when it updates them for 1.2, but if you deploy to your dedicated server by hand (copying files yourself instead of letting the Mod Manager manage the server), that move does not happen on its own. The server keeps looking in the old location, the mod fails to load, and any companion mod that depends on it (Smart Camera, for example) fails too.

Fix: move `Mods/SmartFoundations` into `Mods/GameFeatures/SmartFoundations` on the server yourself (create the `GameFeatures` folder if it is missing), or let the Mod Manager handle server deployment so this happens automatically on every update.

## Why Do My Controls Do Nothing?

Check the simple things first:

- You are holding the build gun.
- You have an active buildable hologram.
- The control is still bound under Options > Controls > Mods.
- You are not typing into a search box or another UI.
- The selected buildable is supported by the Smart feature you are trying to use.

Smart! also shows build gun keybind hints for many actions, and those hints follow your current keybinds.

## Why Does Mouse Wheel Rotate Instead Of Scaling?

That is normal. Smart! leaves mouse wheel to vanilla rotation unless a Smart mode or modifier is active.

Default mouse wheel modifiers:

- Hold `X` to adjust X.
- Hold `Z` to adjust Y.
- Hold `X + Z` to adjust Z.
- Hold `;` for Spacing.
- Hold `I` for Steps.
- Hold `Y` for Stagger.
- Hold `,` for Rotation.

You can also use the Smart Panel with `K` if you prefer visible controls.

## Why Does `K` Open The Upgrade Panel Instead Of The Smart Panel?

Smart! uses `K` for panel-style controls, but the panel depends on what you are holding.

When you are placing a normal Smart-supported building, `K` opens the Smart Panel. When you are holding a belt, lift, pipe, or wire/power line, `K` opens Smart Upgrade.

Power poles and wall outlets intentionally keep opening the Smart Panel so you can scale them. To upgrade power poles or wall outlets, open Smart Upgrade from a wire/power line or use the panel's scan modes.

## Can I Hold Or Nudge A Smart Grid?

Yes. Smart! works with Satisfactory's vanilla Hold and nudge. Hold the parent hologram (default `H`) and nudge it with the arrow keys, and the whole Smart grid moves with the parent — the copies follow along. Vanilla levitation while held works the same way. (Nudge and levitation are vanilla build-gun features, not Smart! features; Smart! just makes the grid follow the held parent.)

Smart! also has an **Auto-Hold** setting that locks the hologram automatically after you change the grid, so a large preview does not drift. You can release that lock with the vanilla Hold key.

## What Is The Infinite Nudge Issue?

**Fixed in v33.2.0.** Older versions of Smart! had a conflict with Infinite Nudge: holding a Smart! modifier (X/Z) to scale a grid could also rotate the hologram at the same time, because Smart!'s own temporary lock on the building was read by Infinite Nudge as "ready to scroll-rotate." As of v33.2.0, Smart! only claims the scroll wheel for the moments it's actually managing the building (a modifier held, or Auto-Hold keeping a scaled grid pinned) — a building you lock yourself still works with Infinite Nudge exactly as it always has.

If you are on v33.2.0 or later and still see odd wheel/rotation behavior with Infinite Nudge or another build-gun mod installed, please report it — that would be a new conflict, not the one described above.

## Why Did Auto-Connect Skip Belts, Pipes, Or Power?

Auto-Connect only builds connections it believes are valid.

Common reasons it skips a connection:

- Auto-Connect is disabled globally or for that connection type.
- The session disable was triggered with the double-tap `Num 0` behavior.
- The selected tier is locked, unavailable, or not what you expected.
- The connector is too far away.
- The angle is invalid for vanilla belt, pipe, or wire placement.
- A power pole is out of free connection slots.
- The buildable has a connector arrangement Smart! does not safely understand yet.

Try a small test layout with the same parts. If the small layout works, the issue is usually distance, angle, connector direction, or pole capacity.

## Why Does Extend Not Start?

Extend needs a valid source and a matching held buildable.

Check:

- Extend is enabled.
- You are holding the same type of building you are aiming at.
- The source is a production building or supported Extend target.
- The source layout has room in the extend direction.
- The connected belts, pipes, or power pieces form a layout Smart! can safely clone.
- You did not temporarily disable Auto-Connect and Extend with double-tap `Num 0`.

Some parts became supported later than Extend itself. For example, power poles, floor holes, wall holes, valves, and pumps were improved after the first Extend release.

## Can I Scale Extend?

Yes. Scaled Extend returned on February 23, 2026.

Activate Extend on a valid source, then use X/Y scaling to add copies and rows. Z scaling, Z spacing, and stagger are hidden while Scaled Extend is active because they do not apply to that topology.

## Why Are Miners Or Some Special Buildables Unsupported?

Smart! focuses on things that can be safely multiplied without changing the rules of the game.

Resource extractors such as miners are restricted because their placement depends on fixed world resource nodes.

Water extractors are a special case: Smart! can scale them in X/Y and spacing when the child placements still validate over water, but vertical scaling is disabled. Some other special buildables are restricted when their placement depends on snap points, world state, or vanilla behaviors that are not safe to clone like normal factory buildings.

## Is Smart Upgrade Safe?

Smart Upgrade is built for batch upgrades, but large conveyor networks are complicated.

For belts and lifts, current versions rebuild connected conveyor runs more carefully than early Smart Upgrade releases. Still, make a save before very large upgrades, give the game a moment after the upgrade finishes, and use the Triage panel only when you are intentionally checking or repairing conveyor chain state.

If you hit a crash or stalled belt issue, include the upgrade mode you used, the approximate network size, the Smart! version, and whether the issue survives save/reload.

## What Is Smart Dismantle?

Smart Dismantle groups Smart-built placements into vanilla blueprint proxies so you can remove them with Blueprint Dismantle.

Use the game's Blueprint Dismantle mode, not a separate Smart-only dismantle tool.

## Is Smart Camera Part Of Smart!?

Smart Camera is a companion mod.

It gives a picture-in-picture build camera for precision building. It currently lives separately, but it may eventually be merged into Smart! or published with its own source.

Default Smart Camera controls:

- `[` cycles camera mode.
- Hold `]` and use mouse wheel to zoom.

## What Should I Include In A Bug Report?

Include enough detail for someone else to recreate the setup:

- Smart! version.
- SML and game version.
- Single-player, host, client, or dedicated server.
- The exact buildable you were holding.
- The feature involved: Scaling, Auto-Connect, Extend, Smart Upgrade, Restore, or Smart Camera.
- Grid size and spacing if relevant.
- Screenshots or a short video.
- Crash report or log if there is one.
- Whether Infinite Nudge, Infinite Zoop, or another build-gun mod is installed.

For Extend or Auto-Connect issues, include a screenshot of the source layout before placing.
