# Frequently Asked Questions

This FAQ is based on current Smart! behavior and common Discord questions from the rebuild era, starting with the October 21, 2025 public release for Satisfactory 1.1.

Smart! was rebuilt in phases. If you remember a feature being missing in late 2025, it may have returned since then.

## Is Smart! Back To The Old Smart?

Yes. The original Smart feature set is back, but it returned piece by piece during the rebuild.

- October 21, 2025: Smart! returned with grid scaling, spacing, steps, stagger, HUD, arrows, and keybind support.
- November 18, 2025: Belt Auto-Connect returned.
- November 22-25, 2025: Pipe and Power Auto-Connect followed.
- December 13, 2025: Extend returned.
- February 21, 2026: Smart Upgrade and Smart Dismantle returned.
- February 23, 2026: Scaled Extend returned.
- May 3, 2026: Smart Restore Enhanced added shareable presets and Extend topology restore.

If an old comment says "Smart does not have Extend yet" or "pipes are not supported yet," check the date. That was true during part of the rebuild, but not for the current release.

## Is Anything From The Original Smart Still Missing?

As far as the original Smart feature set goes, the current Smart! has restored the major features players remember: scaling, transforms, auto-connect, Extend, Scaled Extend, Restore-style presets, and companion camera support.

There are still current-version limitations, but they are not really "old Smart features that have not returned":

- Multiplayer and dedicated server support are not fully supported yet.
- Smart! uses Satisfactory's current built-in nudge system instead of bringing back the old Smart nudge implementation. That old feature was effectively replaced by the base game.
- Smart Camera is currently a companion mod rather than part of the main Smart! package.
- Blueprint scaling is intentionally not supported. Blueprints did not exist in the original Smart era, and Smart! disables scaling for vanilla blueprint placement so it does not interfere with the game's blueprint system.
- Some newer or special buildables have limits because they use special placement rules that did not exist in the original Smart feature set.

## Is Smart! A Cheat Mod?

No. Smart! uses normal Satisfactory buildings and normal material costs.

The mod is meant to remove repetitive placement work, not bypass progression. It should only build things you could place manually, and shared presets are checked against your current unlocks before they can be imported or applied.

## Can I Remove Smart! From A Save?

Smart! is designed to leave vanilla buildables behind. Your factory should not depend on custom Smart-only buildings to load.

Do save before changing any mod setup. If you used Smart Camera, remove that companion mod separately.

## Does Smart! Work In Multiplayer?

Multiplayer is not fully supported yet.

Some features may work better for the host than for clients, but the safe answer is that Smart! is currently best treated as a single-player or host-controlled building tool. Dedicated server support is not something to rely on yet.

## Will Smart! Work With Satisfactory 1.2?

Satisfactory 1.2 is expected to require a mod rebuild.

As of May 22, 2026, the public 1.2 release is expected on June 2, 2026. There may be a gap while SML, FactoryGame headers, Unreal changes, and Smart! itself are updated and tested.

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

## What Is The Infinite Nudge Issue?

Infinite Nudge is a common compatibility topic.

If Smart! controls, placement, or nudging behave strangely, test with Infinite Nudge and Infinite Zoop disabled. Smart! watches the build gun's nudge/placement state while managing its own previews, so overlapping build-gun mods can interfere with each other.

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
