# Smart!

**Smart!** is a building-assist mod for [Satisfactory](https://www.satisfactorygame.com/) that adds grid scaling, spacing, steps, stagger, auto-connect, extend (chain placement), levitation, nudge, camera modes, directional arrows, and more.

[Install on ficsit.app](https://ficsit.app/mod/SmartFoundations) • [Discord](https://discord.gg/SgXY4CwXYw) • [Report Issues](https://github.com/majormer/SmartFoundations/issues) • [Changelog](CHANGELOG.md) • [AI Disclosure](AI_DISCLOSURE.md)

![AI Assisted Development Used](https://img.shields.io/badge/AI%20Assisted%20Development%20Used-See%20AI__DISCLOSURE.md-blue)

---

## What is Smart!?

Smart! is a factory building and efficiency mod. It helps you place repeated layouts faster while still using normal Satisfactory buildings and normal material costs.

If you already know Satisfactory, think of Smart! as a set of building tools for the jobs vanilla building makes repetitive:

- **Place a grid at once** instead of clicking one foundation, wall, machine, or storage container at a time.
- **Adjust spacing, height steps, stagger, and rotation** before you place the build.
- **Auto-connect belts, pipes, and power** when Smart! can safely infer the intended layout.
- **Copy a working factory module** with its belts, pipes, power poles, recipes, and distributor configuration.
- **Upgrade existing infrastructure in batches** instead of replacing belts, lifts, pipes, and poles one by one.

Smart! does **not** create custom production machines or free materials. It places and upgrades standard game buildables for you.

---

## Scope and Design Philosophy

> Smart! is not for cheating. It only allows things to be built if you could build them manually in the vanilla game. It does not provide free resources. It must remain vanilla game neutral, so that uninstalling the mod will not prevent the vanilla game from loading the save successfully. Its scope is factory building and efficiency, primarily taking place when the build gun is equipped.

### Smart! is probably for you if you want to:

- Build large foundation grids, walls, roads, ramps, or storage rows quickly.
- Place many production buildings with consistent spacing.
- Build manifolds with splitters, mergers, belts, pipes, and power more quickly.
- Copy a working production block and repeat it somewhere else.
- Batch-upgrade belts, lifts, pipes, and power infrastructure after unlocking better tiers.
- Keep a vanilla-like save: Smart! places standard Satisfactory buildables.

### Smart! may not be what you want if you expect:

- Free buildings or free resources.
- A creative-mode replacement.
- A fully supported multiplayer experience today.
- Perfect compatibility with every modded buildable or every other building-assist mod.

---

## Core Features

### Grid Scaling

Grid Scaling is the core Smart! feature. Instead of placing one buildable, you place a grid of the same buildable — `10 × 10` foundations, a row of constructors, a wall of storage containers. You can scale on X, Y, and Z independently. Smart! shows a preview before placement and charges the normal material cost for every item in the grid.

### Spacing, Steps, Stagger, and Rotation

Smart! can modify how the grid is laid out:

- **Spacing:** Add or remove gaps between buildables.
- **Steps:** Raise or lower each copy progressively, useful for stairs, ramps, and terraced layouts.
- **Stagger:** Offset every row or column, useful for diagonal-looking layouts and compact factory patterns.
- **Rotation transform:** Create arcs, curves, and spiral-like layouts.

### Auto-Connect

Auto-Connect creates belts, pipes, or power lines when Smart! can safely understand your intent. Scale a row of splitters near production inputs and Smart! can preview belts into those machines. Auto-Connect is conservative — if a connection angle or distance does not look valid, Smart! skips it rather than produce a broken layout.

### Extend

Extend copies an existing factory module instead of creating a blank grid. Aim the hologram at a source building of the same type and Smart! can clone that building plus its nearby belts, pipes, splitters, mergers, power poles, and recipes. Use it when you already built one working block and want another copy.

### Smart Upgrade

Smart Upgrade helps replace infrastructure after you unlock better tiers. Open it while holding a belt, lift, pipe, pump, power line, or wall outlet. Scan by radius or follow a connected network, preview the material cost, and upgrade many items at once.

---

## Quick Start

1. Install Smart! with Satisfactory Mod Manager.
2. Equip any buildable in the build gun.
3. Press `K` to open the Smart Panel and set a small grid such as `3 × 3 × 1`.
4. Preview the full layout before committing.
5. Click once to place the whole layout, paying normal material costs.

Keybinds can be changed in Satisfactory's controls menu under **Mods**.

---

## Compatibility and Save Safety

Smart! places standard Satisfactory buildables, deducts normal material costs, and is designed so that removing the mod does not delete the buildings it placed. Some Smart-only convenience behavior is unavailable once the mod is removed, but the save should remain loadable in vanilla.

Smart! works best with vanilla buildables and mods that use standard Satisfactory placement systems.

---

## Source Availability

This repository is **source-available**, not open source.

The source is published to support community transparency, code review, and pull request contributions. It is **not** published for reuse in other projects.

> Smart! is source-available to support transparency, community review, and pull requests. The code is provided for contributing to Smart! and for personal learning and testing. Reuse in other mods, redistribution of modified builds, or publication of derivative Smart!-like mods requires written permission from the maintainer.

See [LICENSE.md](LICENSE.md) for the full terms.

---

## Contributing

Bug fixes, building support, localization, and other improvements are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a pull request, especially the contribution license terms.

## Governance

Smart! is maintained by **Finalomega**. The maintainer has final authority over roadmap, design, and release decisions. See [GOVERNANCE.md](GOVERNANCE.md).

## Branding

The Smart! name, logo, and mod identity are not licensed for reuse in other projects. See [TRADEMARKS.md](TRADEMARKS.md).

## Security

For crashes that affect save files or other stability issues, please follow the private reporting process in [SECURITY.md](SECURITY.md) rather than opening a public issue.

---

## AI-Assisted Development

Smart! is built with extensive AI assistance for implementation, architecture, documentation, and debugging. Final decisions, in-game testing, and releases are the responsibility of the maintainer. See [AI_DISCLOSURE.md](AI_DISCLOSURE.md) for full details.

---

## Support Development

Smart! is a passion project built for the community. If Smart! saves you time and you want to help keep development sustainable, Ko-fi contributions are appreciated. Support is completely optional.

[Support Finalomega on Ko-fi](https://ko-fi.com/finalomega)
