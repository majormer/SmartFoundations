# <img src="https://github.com/majormer/SmartFoundations/blob/main/images/Smart-Logo.png?raw=true" width="150" alt="Smart! Logo"> Smart! Mod

![Status](https://img.shields.io/badge/Status-Released-brightgreen) ![Version](https://img.shields.io/badge/Version-34.2.1-blue) ![Satisfactory](https://img.shields.io/badge/Satisfactory-1.2-blue) ![Engine](https://img.shields.io/badge/Engine-UE%205.6-blue) ![SML](https://img.shields.io/badge/SML-3.12-blue) ![Multiplayer](https://img.shields.io/badge/Multiplayer-Supported-brightgreen) ![AI Assisted Development Used](https://img.shields.io/badge/AI%20Assisted%20Development%20Used-Disclosure%20Below-blue)

> **Multiplayer note:** As of v32.0.0, every Smart! feature works in multiplayer on dedicated servers (Windows and Linux) — including **Smart Walking** and the new-in-v33.1.0 **Hyper Tube** support. If you hit something odd in a multiplayer session, please report it on [GitHub](https://github.com/majormer/SmartFoundations/issues) or [Discord](https://discord.gg/SgXY4CwXYw).

**Quick links:** [Watch the trailer](https://www.youtube.com/watch?v=FTlTEfIbBxw) • [First-time setup](https://github.com/majormer/SmartFoundations/wiki/Quick-Start) • [Wiki](https://github.com/majormer/SmartFoundations/wiki) • [Discord](https://discord.gg/SgXY4CwXYw) • [Report bugs](https://github.com/majormer/SmartFoundations/issues) • [Source](https://github.com/majormer/SmartFoundations)

---

<div align="center">

### ▶ [Watch the Official Smart! Trailer](https://www.youtube.com/watch?v=FTlTEfIbBxw)

[![Watch the official Smart! trailer](https://img.youtube.com/vi/FTlTEfIbBxw/maxresdefault.jpg)](https://www.youtube.com/watch?v=FTlTEfIbBxw)

**New to Smart!? Start here — a four-minute tour of everything it does.**

</div>

---

## 🚀 What is Smart!?

Smart! is a building-assist mod for Satisfactory. It helps you place repeated layouts faster while still using normal Satisfactory buildings and normal material costs.

If you already know Satisfactory, think of Smart! as a set of building tools for the jobs vanilla building makes repetitive:

- **Place a grid at once** instead of clicking one foundation, wall, machine, or storage container at a time.
- **Adjust spacing, height steps, stagger, and rotation** before you place the build.
- **Auto-connect belts, pipes, and power** when Smart! can safely infer the intended layout.
- **Copy a working factory module** with its belts, pipes, power poles, recipes, and distributor configuration.
- **Upgrade existing infrastructure in batches** instead of replacing belts, lifts, pipes, and poles one by one.

Smart! does **not** create custom production machines or free materials. It places and upgrades standard game buildables for you.

---

## 🎬 Watch Smart! in Action

If you are new, videos explain Smart! much faster than a list of controls.

| Smart! Feature Spotlight by [Enderprise Architecture](https://www.youtube.com/@EnderpriseArchitecture) | Smart V22 Overview by [RightMindGamming](https://www.youtube.com/@rightmindgamming) |
|:---:|:---:|
| [![Smart! Feature Spotlight](https://github.com/majormer/SmartFoundations/blob/main/images/enderprise-spotlight.jpg?raw=true)](https://www.youtube.com/watch?v=U5PNgasYbP8) | [![Smart V22 Overview](https://github.com/majormer/SmartFoundations/blob/main/images/v22-overview.jpg?raw=true)](https://www.youtube.com/watch?v=QZVNIQXYnNg) |

More videos — community reviews, tutorials, and legacy-version overviews — are collected on the **[Videos & Tutorials wiki page](https://github.com/majormer/SmartFoundations/wiki/Videos-and-Tutorials)**.

---

## 🧠 Key Terms

Smart! uses a few Satisfactory and factory-building terms throughout this page.

| Term | Meaning |
|---|---|
| **Hologram** | The blue/orange build preview you see before placing a building. Smart! changes the hologram preview before you click. |
| **Grid** | A repeated layout of the same buildable, such as `10 × 10 × 1` foundations or a row of constructors. |
| **Axis** | A direction Smart! can scale or offset: X for width, Y for depth, and Z for height. |
| **Distributor** | A belt logistics building that distributes items, usually a splitter, merger, smart splitter, programmable splitter, or priority merger. |
| **Manifold** | A repeated factory layout where a main belt or pipe line runs past several machines, and each machine branches off that line through a splitter, merger, or junction. |
| **Factory module** | One repeatable chunk of a factory, usually one machine plus its nearby belts, pipes, splitters, mergers, power pole, recipe, and related connections. |
| **Extend** | Smart!'s feature for copying an existing factory module and continuing it in a chosen direction. |
| **Auto-Connect** | Smart!'s feature for creating belts, pipes, or power lines automatically when the preview layout has clear, valid connection targets. |
| **Through-line** | The straight part of a manifold that continues to the next splitter, merger, junction, or machine module. |
| **Side branch** | The branch from the manifold's through-line into or out of a factory building. For Extend, this is usually the splitter/merger side port facing the factory. |

## 🧭 Should You Install Smart!?

Smart! is useful if you enjoy designing factories but do not enjoy repeating the same placement action hundreds of times.

### Smart! is probably for you if you want to:

- Build large foundation grids, walls, roads, ramps, or storage rows quickly.
- Place many production buildings with consistent spacing.
- Build manifolds with splitters, mergers, belts, pipes, and power more quickly.
- Copy a working production block and repeat it somewhere else.
- Batch-upgrade belts, lifts, pipes, and power infrastructure after unlocking better tiers.
- Keep a vanilla-like save: Smart! places standard Satisfactory buildings.
- Build together: Smart! works in multiplayer on dedicated servers (Windows and Linux).

### Smart! may not be what you want if you expect:

- Free buildings or free resources.
- A creative-mode replacement.
- Perfect compatibility with every modded buildable or every other building-assist mod.

### Source Availability

Smart! is **source-available**, not open source. The source code is published to support community transparency, code review, and pull request contributions. It is **not** published for reuse in other projects or redistribution of modified builds.

**What you can do:**
- View and study the source code for learning
- Submit contributions via pull requests
- Build locally for testing contributions

**What requires permission:**
- Redistribution of modified builds
- Reuse in other mods or projects
- Publication of derivative Smart!-like mods

See [LICENSE.md](https://github.com/majormer/SmartFoundations/blob/main/LICENSE.md) for the full terms. Source available as of April 26, 2026.

---

## ⚡ The 60-Second Version

1. **Equip a buildable** in the build gun.
2. **Change the Smart grid** with the Smart! keybinds or press `K` to use the Smart Panel.
3. **Preview the full layout** before committing.
4. **Click once** to place the whole layout, paying normal material costs.
5. For existing factory modules, use **Extend** by holding the same building type over an existing building and selecting a valid direction.

---

## 📰 What's New

Smart! is actively developed. For the highlights of the latest release and the full history of every version, see the **[changelog](https://github.com/majormer/SmartFoundations/blob/main/CHANGELOG.md)**.

Curious about a feature you saw mentioned? The **[Wiki](https://github.com/majormer/SmartFoundations/wiki)** has a page for each one.

---

## 🛠️ First-Time Setup

Smart! has many keybinds, but you do not need to learn them all immediately.

### Recommended first use

1. Install Smart! with Satisfactory Mod Manager.
2. Start with a simple item like foundations.
3. Equip a foundation.
4. Press `K` to open the Smart Panel.
5. Set a small grid such as `3 × 3 × 1`.
6. Place it.
7. Try Spacing, Steps, and Stagger after the basic grid makes sense.

### Important controls to learn first

| Action | What it is for |
|---|---|
| `K` | Open the Smart Panel for visual controls |
| Scale X / Y / Z | Change how many items are placed across each axis |
| Spacing mode | Change the gap between placed items |
| Steps mode | Add height changes across a grid |
| Stagger mode | Offset rows for diagonal or shifted patterns |
| Double-tap `Num0` | Toggle Smart Auto-Connect and Extend off (and on again) for the session |

Keybinds can be changed in Satisfactory's controls menu.

---

## 🧱 Core Features

Each feature below has a full guide on the **[Wiki](https://github.com/majormer/SmartFoundations/wiki)** — follow the link under any feature for step-by-step instructions.

### Grid Scaling

Instead of placing one buildable, place a whole grid of it: `10 × 10` foundations, a row of constructors, a wall of storage containers, power poles, pipeline junctions, or vertical stacks where the building supports it. Scale on X, Y, and Z independently, preview before placing, and pay the normal material cost for every item in the grid.
→ [Grid Scaling](https://github.com/majormer/SmartFoundations/wiki/Grid-Scaling)

### Spacing, Steps, Stagger, and Rotation

Shape how the grid lays out: **Spacing** adds gaps, **Steps** raises each copy progressively for stairs and terraces, **Stagger** offsets rows or columns for diagonal and compact patterns, and **Rotation** creates arcs, curves, and circular layouts. Useful for both factory efficiency and aesthetic building.
→ [Transforms](https://github.com/majormer/SmartFoundations/wiki/Transforms)

### Auto-Connect

When Smart! can safely infer your intent, it previews the belts, pipes, or power lines between what you place — belts into machines from a row of splitters, pipes from junctions, power between poles, or connected runs across stackable supports. It is conservative: if a connection does not look valid, it is left open rather than built broken.
→ [Auto-Connect](https://github.com/majormer/SmartFoundations/wiki/Auto-Connect)

### Smart! Blueprints

Scale one of your own blueprints into a grid and Smart! wires the belts and pipes between the copies for you — including the true two-dimensional seams the game's own blueprint auto-connect can't make, plus vertical pipe stacking. Each copy stays a real, independent blueprint you can dismantle on its own.
→ [Blueprints](https://github.com/majormer/SmartFoundations/wiki/Blueprints)

### Extend

Copy an existing factory module instead of starting a blank grid. Aim at one working block — its building, nearby splitters/mergers, belts, lifts, pipes, floor holes, pumps, power, recipe, and distributor configuration — and Smart! previews the next copy, wired to match.
→ [Extend](https://github.com/majormer/SmartFoundations/wiki/Extend)

### Smart Upgrade

Replace infrastructure in batches after you unlock better tiers. Hold a belt, lift, pipe, power line, or wall outlet, scan by radius or along a connected network, preview the exact material cost, and upgrade many items at once. Supports conveyor belts and lifts, pipelines, and power poles and outlets.
→ [Smart Upgrade](https://github.com/majormer/SmartFoundations/wiki/Smart-Upgrade)

### Smart Restore Presets

Save, apply, share, and replay Smart Panel setups. A preset can capture grid size, spacing, steps, stagger, rotation, recipe, auto-connect settings, and a whole restored Extend module layout. Save from the Smart Panel's `Presets >>` button, apply later, export/import shared presets (checked against your unlocks), or turn your last Extend layout into a reusable preset.
→ [Smart Restore](https://github.com/majormer/SmartFoundations/wiki/Smart-Restore)

### Smart Walking

A build mode for a single connected run that turns, climbs, and routes to a destination instead of a rigid grid. Hold a stackable pole or pipeline support, open the Smart Panel, and click **Smart Walking**: advance one segment at a time, steer the leading segment, back up to undo, and commit the whole run in one build. Belts, pipes, and hyper tubes are supported.
→ [Smart Walking](https://github.com/majormer/SmartFoundations/wiki/Smart-Walking)

### Smart Dismantle

Remove Smart-built placements as a unit. Smart! groups its placements into vanilla blueprint proxies, so you can clear a whole grid or module with the game's own Blueprint Dismantle mode instead of picking it apart one building at a time.
→ [Smart Dismantle](https://github.com/majormer/SmartFoundations/wiki/Smart-Dismantle)

### Smart Panel & HUD

Prefer a form over keybinds? Press `K` for the Smart Panel — set every grid, spacing, transform, recipe, and auto-connect value directly, then Apply. The on-screen HUD shows your current grid, transforms, and recipe as you build.
→ [Smart Panel and HUD](https://github.com/majormer/SmartFoundations/wiki/Smart-Panel-and-HUD)

### Controls: keyboard, mouse, and controller

Build by keybind and mouse wheel, or by the Smart Panel — they share one state, so use whichever fits the moment. **Player Relative Controls** (optional) let the grid grow in the direction you're facing instead of fixed compass axes. **Tap to Toggle Transform Modes** (optional) makes Smart! more usable on a controller or Steam Deck by latching a mode with a tap instead of a hold.
→ [Controls](https://github.com/majormer/SmartFoundations/wiki/Controls) • [Controller & Steam Deck](https://github.com/majormer/SmartFoundations/wiki/Controller-and-Steam-Deck)

---
## 🔄 Extend Explained

Extend copies a **finished factory cell** — a building plus the belts, pipes, splitters, mergers, and power around it — and continues the pattern in the direction you choose. It works best when the source module is clean: connections at the edges, a clear through-line, and side branches facing the right way.

The wiki [Extend](https://github.com/majormer/SmartFoundations/wiki/Extend) page walks through the steps, what makes a good source module, the valid-manifold mental model, and the patterns that can fail.

---

## 🔌 Auto-Connect Explained

Auto-Connect is not magic routing — it is a best effort based on nearby connectors, directions, distance, and the game's own validation. If a connection does not look valid, Smart! leaves it open rather than build something broken.

The wiki [Auto-Connect](https://github.com/majormer/SmartFoundations/wiki/Auto-Connect) page covers what it connects and tips for reliable results.

---
## 🎮 Controls

Smart! uses **native Satisfactory keybinds**, customizable in **Options > Controls > Mods**. The essentials: press `K` for the Smart Panel, use the numpad (or hold an axis key and scroll) to size the grid, and hold a transform key — `;` spacing, `I` steps, `Y` stagger, `,` rotation — while scrolling to shape it.

The complete, always-current reference lives on the wiki, generated from the mod's own code:

- **[Controls](https://github.com/majormer/SmartFoundations/wiki/Controls)** — every keybind and mode.
- **[Settings Reference](https://github.com/majormer/SmartFoundations/wiki/Settings-Reference)** — every mod setting.
- **[Smart Panel & HUD](https://github.com/majormer/SmartFoundations/wiki/Smart-Panel-and-HUD)** — the on-screen panel and overlay.
- **[Controller & Steam Deck](https://github.com/majormer/SmartFoundations/wiki/Controller-and-Steam-Deck)** — controller and radial-menu setups.

---
## 📦 Supported Buildings

Smart! supports many buildables, especially those with normal single-click placement.

### Commonly supported categories

- Foundations, ramps, walls, barriers, railings, walkways, and catwalks.
- Production buildings such as constructors, assemblers, manufacturers, smelters, foundries, refineries, packagers, blenders, converters, quantum encoders, and particle accelerators.
- Storage containers and fluid buffers.
- Power poles, power towers, switches, generators, and power storage.
- Splitters, mergers, smart splitters, programmable splitters, priority mergers, stackable supports, pipeline junctions, pumps, valves, wall holes, and floor holes.
- Many signs, billboards, lights, and factory organization pieces.

### Not supported by design

Some vanilla placements are multi-step or drag-based and do not fit Smart!'s grid-placement model:

- Manual belts and pipes as direct grid items.
- Conveyor lifts as direct grid items.
- Power lines and hypertubes as direct grid items.
- Railways and train signals.
- Blueprints as Smart-scaled buildables.

Smart! can still create belts, pipes, hyper tubes, lifts, and wires as part of Auto-Connect, Extend, or Smart Walking. They are just not usually the primary item you scale directly.

---

## 🧩 Compatibility and Save Safety

Smart! aims to stay vanilla-friendly.

- Smart! places standard Satisfactory buildables.
- You still pay normal material costs.
- Removing Smart! should not delete the buildings it placed.
- Some Smart-only convenience behavior is unavailable once the mod is removed.

### Mod compatibility

Smart! works best with vanilla buildables and mods that use standard Satisfactory placement systems. It may not work perfectly with every custom building mod or every other build-assist mod.

If something behaves strangely, test with only Smart! and its required dependencies before reporting a bug.

---

## 💬 Getting Help

Start with the **wiki** for how-to guides, then the Discord for support, examples, testing updates, and feature discussion:

[![Discord](https://img.shields.io/discord/799091523173613589?color=7289da&label=Discord&logo=discord&logoColor=white)](https://discord.gg/SgXY4CwXYw)

- **Wiki and documentation:** https://github.com/majormer/SmartFoundations/wiki
- **Discord:** https://discord.gg/SgXY4CwXYw
- **Bug reports:** https://github.com/majormer/SmartFoundations/issues
- **Source:** https://github.com/majormer/SmartFoundations

> The Smart! wiki and issue tracker now live on the main source repository above. They previously lived in a separate issue-tracker repository, before Smart! became source-available — please use the links here rather than any older tracker links you may have bookmarked.

**Bug reports use structured issue forms** — pick the type that fits: bug, crash, compatibility, Blueprint Designer, feature request, or a Satisfactory 1.2 report. (Blank issues are disabled; the form walks you through it.) Each form prompts for the details below, so it helps to have them ready:

- Smart! version and SML version.
- Satisfactory version and branch (for example, 1.2 stable, CL 491125).
- Session type (single-player or multiplayer) and save type.
- Steps to reproduce, what you expected, and what actually happened.
- Logs — `FactoryGame.log`, plus the crash-reporter text for crashes.
- Any other mods you have installed.
- Screenshots or video. For Extend issues, a screenshot from above is especially helpful.

---

## 📷 Smart! Camera Companion Mod

Smart! Camera is a separate companion mod by the same developer. It provides a picture-in-picture overhead camera view for Smart! layouts.

- **Smart! Camera download:** https://ficsit.app/mod/SmartCamera
- Smart! Camera is not required to use Smart!.
- Install both mods if you want the overhead camera preview experience.

---

## 💰 Support Smart! Development

Smart! is a passion project built for the community. Development, testing, documentation, and support all take time, and recent versions have also involved personal development expenses.

Support is completely optional, but if Smart! saves you time and you want to help keep development sustainable, Ko-fi contributions are appreciated.

### Ways to Support

- **Direct support via Ko-fi:** [Support Finalomega on Ko-fi](https://ko-fi.com/finalomega)
- **Help test releases:** Join Discord and provide feedback on prerelease builds.
- **Report bugs clearly:** Include reproduction steps, screenshots, versions, and save context when possible.
- **Share examples:** Screenshots and videos help other players understand what Smart! can do.

No pressure. Smart! remains a community-focused project, and every kind of support helps.

---

## ❓ Frequently Asked Questions

### Does Smart! cheat resources?

No. Smart! charges normal material costs for the buildings it places. If you cannot afford the layout, placement is blocked or limited just like normal Satisfactory building.

### Can I remove Smart! later?

Smart! places normal game buildings, so your placed factories should remain. You lose Smart!'s building tools and UI when the mod is removed.

### Does Smart! work with blueprints?

Smart! focuses on live build gun placement and factory modules. Blueprint scaling is not the same system and is not treated as a normal Smart-scaled buildable.

### Does Smart! replace Zoop?

No. Smart! is a different system. It can place grids and factory modules in ways vanilla Zoop does not, but vanilla Zoop still exists. Smart! may disable scaling behavior when it detects normal Zoop placement to avoid conflicts.

### Does Smart! work in multiplayer?

Yes — as of v32.0.0, every Smart! feature works in multiplayer on dedicated servers (Windows and Linux). Install the same Smart! version on the server and on every client. Multiplayer support is new, so if something behaves differently in a multiplayer session than in single-player, that's a bug — please report it with clear reproduction steps.

### Why did Extend not copy my layout?

Usually one of these is true:

- You are not holding the same building type as the source building.
- The source module is not a clean repeatable manifold cell.
- Splitter or merger factory connections are using a through-line port instead of a side branch.
- Rotations or belt directions are inconsistent.
- The preview direction needs to be changed with the mouse wheel.

---

## 👥 Credits

Smart! exists because of Alex's original concepts, the current rebuild, testers, translators, content creators, and community members.

### Current Team

- **Alex** - Original Smart! concept creator and Project Advisor, providing guidance, counsel, and permission for the Satisfactory 1.1 continuation.
- **Finalomega** - Lead Developer and Documentation Writer for the Satisfactory 1.1 rebuild.
- **Raudoc2K1** - Support Staff, Tester, Discord Moderator, and Content Creator. Also known as RightMindGamming on [YouTube](https://www.youtube.com/channel/UCfy5lG-teOehpD9oYLjT7rA) and [Twitch](https://www.twitch.tv/rightmindgamming).
- **Shaded** - Support Staff, Tester, and Discord Moderator.

### Original Contributors

- **Robb** - Update 8 port with partial functionality, SML expertise, and advice.
- **Deantendo** - Created the amazing mod icon.
- **HWEEKS** - Original description author.

### Testers

Special thanks to the testers from the Smart! Discord who helped shape the v22-v29 rebuild with feedback, bug reports, and validation:

- **Raudoc2K1**
- **Shaded**
- **PerseusDemigod**
- **-Alejandro** - Creator of *Early Free Blueprint Designer* and *Faster Hypertube Entrances*.
- **drewfarms**
- **Serjevski**

### Thanks from Alex

Huge thanks to **Marcio** for all his help from the beginning of my path as mod creator, **TwoTwoEleven** for his awesome code examples from MM, to **Archengius** for his fine example of overriding the default buildings and to **Mircea** for some fine thoughts. Thanks **jay96** for your amazing idea about arrows.

---

## 🤖 AI Disclosure

Smart! uses AI-assisted development. AI tools help with investigation, drafting, refactoring, documentation, and debugging support. Final decisions, testing, release preparation, and maintenance remain the responsibility of the project maintainer.

AI assistance does not replace community testing. Smart! features are validated through developer review, in-game testing, tester feedback, and bug reports.
