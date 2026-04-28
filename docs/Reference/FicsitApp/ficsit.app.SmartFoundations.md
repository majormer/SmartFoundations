# <img src="https://github.com/SmartFoundations/SmartIssueTracker/blob/main/images/Smart-Logo.png?raw=true" width="150" alt="Smart! Logo"> Smart! Mod

![Status](https://img.shields.io/badge/Status-Released-brightgreen) ![Version](https://img.shields.io/badge/Version-29.2.5-blue) ![Engine](https://img.shields.io/badge/Engine-UE%205.3-blue) ![SML](https://img.shields.io/badge/SML-3.11.x-blue) ![Multiplayer](https://img.shields.io/badge/Multiplayer-Testing-orange) ![AI Assisted Development Used](https://img.shields.io/badge/AI%20Assisted%20Development%20Used-Disclosure%20Below-blue)

> **Multiplayer note:** Smart! is primarily developed for single-player. Multiplayer is under active testing with partial success, but is not currently considered fully supported.

**Quick links:** [Watch videos](#-watch-smart-in-action) • [How Smart works](#-how-smart-changes-building) • [First-time setup](#-first-time-setup) • [Extend explained](#-extend-explained-copy-an-existing-manifold) • [Supported buildings](#-supported-buildings) • [Discord](https://discord.gg/SgXY4CwXYw) • [Report bugs](https://github.com/majormer/SmartFoundations/issues) • [Source](https://github.com/majormer/SmartFoundations)

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
| [![Smart! Feature Spotlight](https://github.com/SmartFoundations/SmartIssueTracker/blob/main/images/enderprise-spotlight.jpg?raw=true)](https://www.youtube.com/watch?v=U5PNgasYbP8) | [![Smart V22 Overview](https://github.com/SmartFoundations/SmartIssueTracker/blob/main/images/v22-overview.jpg?raw=true)](https://www.youtube.com/watch?v=QZVNIQXYnNg) |

More videos are listed near the bottom of this page in [More Videos](#-more-videos).

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

### Smart! may not be what you want if you expect:

- Free buildings or free resources.
- A creative-mode replacement.
- A fully supported multiplayer experience today.
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

## 📰 What's New in v29

**Current Release:** v29.2.5 — See [full changelog](https://github.com/majormer/SmartFoundations/blob/main/CHANGELOG.md) for all patch details

### Major Feature: Scaled Extend

The defining feature of version 29 is **Scaled Extend** — the fusion of Scaling and Extending for rapid factory duplication. Scale Extend across multiple clones and rows, with automatic lane segments chaining between adjacent clones' distributors. This represents a fundamental transformation of what Smart! can do.

### Recent Patch Updates

For detailed information about recent fixes and improvements in v29.2.5 and earlier patch releases, see the [full changelog](https://github.com/majormer/SmartFoundations/blob/main/CHANGELOG.md).

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
| Double-tap `Num0` | Temporarily disable auto-connect for the next placement |

Keybinds can be changed in Satisfactory's controls menu.

---

## 🧱 Core Features

### 1. Grid Scaling

Grid Scaling is the core Smart! feature.

Instead of placing one buildable, you place a grid of the same buildable:

- `10 × 10` foundations.
- A row of constructors.
- A wall of storage containers.
- Multiple power poles or pipeline junctions.
- Vertical stacks where the building supports it.

You can scale on X, Y, and Z independently. Smart! shows a preview before placement and charges the normal material cost for every item in the grid.

### 2. Spacing, Steps, Stagger, and Rotation

Smart! can modify how the grid is laid out:

- **Spacing:** Add or remove gaps between buildables.
- **Steps:** Raise or lower each copy progressively, useful for stairs, ramps, and terraced layouts.
- **Stagger:** Offset every row or column, useful for diagonal-looking layouts and compact factory patterns.
- **Rotation transform:** Create arcs, curves, circular roads, and spiral-like layouts.

These tools are useful for both factory efficiency and aesthetic building.

### 3. Auto-Connect

Auto-Connect creates belts, pipes, or power lines when Smart! can safely understand your intent.

Examples:

- Scale a row of splitters near production inputs and Smart! can preview belts into those machines.
- Scale pipeline junctions near pipe inputs and Smart! can route pipes.
- Scale power poles near buildings and Smart! can wire them.
- Scale stackable conveyor or pipeline supports and Smart! can connect the supports in a line.

Auto-Connect is conservative. If a connection angle or distance does not look valid, Smart! may refuse to create it rather than produce a broken layout.

### 4. Extend

Extend copies an existing factory module instead of creating a blank grid.

Use it when you already built one working block and want another copy of that same block.

Extend can copy:

- The selected factory building.
- Nearby splitters, mergers, belts, lifts, junctions, pipes, wall holes, floor holes, pumps, valves, and power poles when they belong to the source module.
- Recipes and supported distributor configuration.
- Connections needed to make the cloned module work.

Extend is one of Smart!'s most powerful features, but it needs a valid source layout. See [Extend Explained](#-extend-explained-copy-an-existing-manifold) below.

### 5. Smart Upgrade

Smart Upgrade helps replace infrastructure after you unlock better tiers.

Open it while holding a belt, lift, pipe, pump, power line, or wall outlet. You can scan by radius or follow a connected network, preview the material cost, and upgrade many items at once.

Supported upgrade families include:

- Conveyor belts and lifts.
- Pipelines.
- Power poles and wall outlets.

---

## 🔄 Extend Explained: Copy an Existing Manifold

Extend is easiest to understand if you think of it as **copying a finished factory cell**.

In vanilla Satisfactory, middle-clicking a building copies that building into your build gun. Smart! builds on that idea:

> If you are holding the same type of building and aim it at an existing building of that type, Smart! can treat that as: “copy this building and the connected module around it.”

### Basic Extend steps

1. Build one working factory module first.
2. Middle-click the main factory building, or otherwise equip the same building type.
3. Aim the hologram at the existing source building.
4. Smart! shows an Extend preview if the source layout can be copied.
5. Use the mouse wheel to choose the side/direction.
6. Use Smart! scaling controls if you want multiple copies or rows.
7. Click to place the clone.

### What counts as a good source module?

A good Extend source is a clean, repeatable manifold cell:

```text
Main belt line → splitter → factory input
Factory output → merger → output belt line
```

For many buildings, the repeatable module is:

- A factory building in the middle.
- Splitters feeding its inputs.
- Mergers collecting its outputs.
- Belts or pipes arranged so the source module can continue into the next module.
- Power poles close enough to be considered part of the module.

### Why splitter and merger side ports matter

For Extend manifolds, factories should usually connect to the **side ports** of splitters and mergers.

The reason is geometry:

- A splitter or merger has ports that form a straight-through line.
- Those opposite ports are best used for the manifold's continuing belt line.
- The side port is the branch that goes into or out of the factory.
- When Smart! clones the module, it needs the next cloned splitter or merger to connect cleanly to the next one.

If a factory uses a port that has an opposite port on the other side, that port may be needed for the manifold line. The next cloned module would require a belt to turn at an invalid or unreliable angle.

A simple rule of thumb:

> **Use the splitter/merger side facing the factory for the factory connection. Leave the straight-through direction for the manifold line.**

### Valid manifold mental model

```text
Good pattern:

Input belt line ── Splitter ── continues to next splitter
                    │
                    ▼
                 Factory
                    │
                    ▼
Output belt line ─ Merger ── continues to next merger
```

In this pattern, Extend can understand two things:

- The straight line is the chain that continues to the next copy.
- The side branch belongs to the factory being cloned.

### Patterns that may fail

Extend may refuse or produce incomplete previews when:

- The factory is connected to the through-line side of a splitter or merger.
- One belt is reversed compared to the others in the same manifold direction.
- Splitters or mergers are rotated inconsistently.
- Connections cross, backtrack, or require sharp angles.
- The source module is too tangled for Smart! to determine what belongs to one repeatable cell.

If Extend does not preview what you expect, try simplifying the source module into a cleaner manifold cell first.

### Scaled Extend

After Extend activates, Smart! scaling can create multiple clones at once.

Examples:

- Extend one constructor module into a row of constructors.
- Extend a refinery module across several repeated processing lines.
- Clone a nuclear pasta production cell into additional rows.

You can increase spacing to leave room for extra feed belts or mergers, or scale on another axis to create multiple rows.

---

## 🔌 Auto-Connect Explained

Auto-Connect is not magic routing. It is Smart! making a best effort based on nearby connectors, directions, distance, and game validation rules.

### Use Auto-Connect for:

- Rows of splitters feeding rows of machines.
- Rows of mergers collecting outputs.
- Pipeline junctions feeding refineries, packagers, blenders, and other pipe buildings.
- Power poles near buildings.
- Stackable conveyor and pipeline supports.

### Tips for reliable Auto-Connect

- Keep distributors aligned with the buildings they feed.
- Use consistent rotations across a row.
- Keep distances reasonable.
- Avoid crossing belts and pipes in the preview.
- Use double-tap `Num0` when you want one placement without Auto-Connect.

---

## 🎮 Controls and Feature Instructions

Smart! uses **native Satisfactory keybinds** that can be customized in **Options > Keybindings > Smart! Scaling Controls**.

### Grid Scaling

Use Grid Scaling when you want to place multiple copies of the current buildable.

| Action | Default Key | Description |
|---|---|---|
| Increase/Decrease X | `NumPad 8` / `NumPad 5` | Adjust grid width |
| Increase/Decrease Y | `NumPad 6` / `NumPad 4` | Adjust grid depth |
| Increase/Decrease Z | `NumPad 9` / `NumPad 3` | Adjust grid height or layers |
| Adjust X with modifier | Hold `X` + `Scroll Wheel` or `NumPad 8/5` | Adjust X count using the shared increase/decrease controls |
| Adjust Y with modifier | Hold `Z` + `Scroll Wheel` or `NumPad 8/5` | Adjust Y count using the shared increase/decrease controls |
| Adjust Z with modifier | Hold `X` + `Z` + `Scroll Wheel` or `NumPad 8/5` | Adjust Z count using the shared increase/decrease controls |

**How to use it:** Equip a buildable, increase the X/Y/Z counts until the preview matches the layout you want, then click once to place the grid.

### Spacing Mode

Use Spacing when the copies are too close together or you want room for walkways, belts, pipes, or decoration.

| Action | Default Key | Description |
|---|---|---|
| Activate Spacing Mode | Hold `;` | Enables spacing adjustments while held |
| Increase/Decrease Spacing | `Scroll Wheel` or `NumPad 8/5` while holding `;` | Adjust spacing on the active spacing axis |
| Cycle Axis | `Num0` while holding `;` | Switch between X, Y, and Z spacing axes |

**How to use it:** Hold `;`, adjust spacing with the mouse wheel, and press `Num0` while still holding `;` to change which axis you are spacing.

### Steps Mode

Use Steps to make each copy progressively higher or lower than the last one.

| Action | Default Key | Description |
|---|---|---|
| Activate Steps Mode | Hold `I` | Enables step adjustments while held |
| Increase/Decrease Steps | `Scroll Wheel` or `NumPad 8/5` while holding `I` | Adjust vertical rise on the active step axis |
| Cycle Axis | `Num0` while holding `I` | Switch between X and Y step axes |

**How to use it:** Hold `I`, adjust the height change, and place the preview when it forms the stair, ramp, or terraced layout you want.

### Stagger Mode

Use Stagger to offset rows or layers instead of placing every copy in a perfect rectangle.

| Action | Default Key | Description |
|---|---|---|
| Activate Stagger Mode | Hold `Y` | Enables stagger adjustments while held |
| Increase/Decrease Stagger | `Scroll Wheel` or `NumPad 8/5` while holding `Y` | Adjust offset on the active stagger axis |
| Cycle Axis | `Num0` while holding `Y` | Switch between X, Y, ZX, and ZY stagger axes |

**Stagger axes:**

- **X/Y:** Horizontal offsets for diagonal or shifted rows.
- **ZX/ZY:** Vertical lean patterns where higher layers shift sideways or forward.

### Recipe Selection Mode

Use Recipe Selection to choose the production recipe before placing a scaled group of machines.

| Action | Default Key | Description |
|---|---|---|
| Activate Recipe Mode | Hold `U` while aiming a production building hologram | Enables recipe selection for that building type |
| Next/Previous Recipe | `Scroll Wheel` or `NumPad 8/5` while holding `U` | Cycle available recipes |
| Clear Manual Selection | `Num0` while holding `U` | Clear the manually selected recipe |

**How to use it:** Aim a production building hologram, hold `U`, choose the recipe, release `U`, and place the grid. Smart! applies the selected recipe to the placed machines.

### Auto-Connect Settings

Use Auto-Connect settings when aiming a splitter, merger, pipe junction, or power pole hologram.

| Action | Default Key | Description |
|---|---|---|
| Activate Auto-Connect Settings | Hold `U` while aiming a supported logistics or power hologram | Enables context-specific Auto-Connect settings |
| Cycle Setting | `Num0` while holding `U` | Switch between available Auto-Connect options |
| Increase/Decrease Value | `Scroll Wheel` or `NumPad 8/5` while holding `U` | Change the selected setting |
| One-Shot Disable | Double-tap `Num0` with no modifiers | Disable all Auto-Connect for the next placement only |

**For belt distributors:** Enable/disable Auto-Connect, distributor-to-distributor connections, and distributor-to-building belt tier selection.

**For pipe junctions:** Enable/disable Auto-Connect, junction-to-junction pipe connections, junction-to-building pipe tier selection, and routing options.

**For power poles:** Enable/disable power Auto-Connect, adjust connection range, and reserve power slots.

### Smart! Panel

Use the Smart! Panel when you prefer visual controls instead of keybinds.

| Action | Default Key | Description |
|---|---|---|
| Toggle Smart! Panel | `K` | Opens or closes the context-sensitive panel |
| Apply Changes | Click **Apply** | Applies panel changes to the hologram preview |
| Cancel/Close | `Escape` | Closes the panel without applying uncommitted changes |

**K is context-sensitive:**

- Holding a **belt, lift, pipe, pump, power line, or wall outlet** opens the **Smart Upgrade Panel**.
- Holding most other buildables opens the **Smart! Panel** for grid, spacing, recipe, and Auto-Connect settings.

> **Power note:** To upgrade a power grid, aim while holding a **Power Line** instead of a Power Pole. This keeps `K` available for scaling power poles through the Smart! Panel.

### Rotation Transform

Use Rotation Transform for arcs, curved roads, spiral-like ramps, and radial layouts.

| Action | Default Key | Description |
|---|---|---|
| Activate Rotation Mode | Hold `,` | Enables rotation adjustments while held |
| Increase/Decrease Rotation | `Scroll Wheel` or `NumPad 8/5` while holding `,` | Adjust rotation step in degrees |

**How to use it:** Hold `,`, choose a rotation angle, then scale X to preview an arc. Add Y scaling for parallel curved lanes.

### Visual Aids

| Action | Default Key | Description |
|---|---|---|
| Toggle Arrows | `NumPad 1` | Show or hide axis direction arrows on holograms |

### Smart Upgrade Instructions

Use Smart Upgrade when you want to replace existing infrastructure with a different tier.

1. Hold a belt, lift, pipe, pump, power line, or wall outlet hologram.
2. Press `K` to open the Smart Upgrade Panel.
3. Choose radius mode or network traversal mode.
4. Select the target tier or family.
5. Review the cost preview.
6. Execute the upgrade.

Smart Upgrade supports belts, lifts, pipes, power poles, and wall outlets. It deducts the net material cost after refunds and aborts safely if materials are insufficient.

### Extend Instructions

Use Extend when you want to copy a working factory module.

1. Build a clean source module first.
2. Equip the same factory building type as the source building.
3. Aim the hologram at the source building.
4. Wait for the Extend preview.
5. Use the mouse wheel to choose the side or direction.
6. Use X/Y scaling if you want multiple copies or rows.
7. Click to place the clone.

For manifolds, connect factories to splitter/merger side ports and leave the straight-through ports for the continuing manifold line.

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

Smart! can still create some belts, pipes, lifts, and wires as part of Auto-Connect or Extend. They are just not usually the primary item you scale directly.

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

Join the Smart! Discord for support, examples, testing updates, and feature discussion:

[![Discord](https://img.shields.io/discord/799091523173613589?color=7289da&label=Discord&logo=discord&logoColor=white)](https://discord.gg/SgXY4CwXYw)

- **Discord:** https://discord.gg/SgXY4CwXYw
- **Bug reports:** https://github.com/majormer/SmartFoundations/issues
- **Source:** https://github.com/majormer/SmartFoundations

When asking for help, include:

- Smart! version.
- SML version.
- Whether you are single-player or multiplayer.
- Screenshots of the source layout and preview.
- The buildable you are holding.
- What you expected Smart! to do.

For Extend issues, a screenshot from above is especially helpful.

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

## 🎥 More Videos

Smart! has had quite a few videos made for it, and the project is grateful for every creator who helped show players what the mod can do.

### Smart! for Satisfactory 1.1+

These videos cover the rebuilt Smart! mod for current Satisfactory/SML versions.

| Video | Creator | Link |
|---|---|---|
| Smart! Feature Spotlight | [Enderprise Architecture](https://www.youtube.com/@EnderpriseArchitecture) | https://www.youtube.com/watch?v=U5PNgasYbP8 |
| Smart V22 Overview | [RightMindGamming](https://www.youtube.com/@rightmindgamming) | https://www.youtube.com/watch?v=QZVNIQXYnNg |

### Legacy Smart! feature videos

These videos are for older Smart! versions. Some controls or details may differ, but they are still useful for seeing the kinds of layouts Smart! was built to support.

| Video | Link |
|---|---|
| Version 21 - Improved Nudge Mode Overview | https://www.youtube.com/watch?v=NyYymsMa5Gg |
| Version 20 Overview and Tutorial | https://www.youtube.com/watch?v=R1nEiSfskPA |
| Preview of the camera feature | https://youtu.be/bPHYtuWp2aI |
| Preview of the lift height counter feature | https://youtu.be/ZMSZaEa-3No |
| Version 17 | https://youtu.be/vKPQ5YPPsU8 |
| Version 16 | https://youtu.be/MmkfqByx0i0 |
| Version 15 | https://youtu.be/jxfJR3ullJI |
| Version 14 | https://youtu.be/-HbCKSABeWE |
| Version 12 | https://youtu.be/thC8RvniApQ |
| Version 11 | https://youtu.be/5qE3G4KbJXM |

### Community reviews

| Review | Creator | Link |
|---|---|---|
| Mod review | [ImKibitz](https://www.youtube.com/channel/UCz9qw5nupdzCGwHwQiqs7qA) | https://youtu.be/JSL6kSgzYJk |
| In-depth review | [Magenty](https://www.youtube.com/channel/UCL8hC7X4mpAKdoP5gwdKkBQ) | https://youtu.be/O7jHpKhhqaY |
| First review | [TotalXclipse](https://www.youtube.com/channel/UC2SNK_S7tvROHS_KJdIiEFg) | https://youtu.be/wIfhqBxiufk |

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

Multiplayer is under active testing with partial success, but is not fully supported. If you play multiplayer, expect edge cases and report issues with clear reproduction steps.

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
