# Smart Restore

Smart Restore saves your Smart! work so you can reuse it later, share it, or stamp a whole factory module out again the same way every time. Open **K → Restore** in the shared [Smart Panel](Smart-Panel-and-HUD). Its two subtabs are **Grid Presets** and **Modules**.

It is the answer to "I spent ages dialing in this layout — how do I get it back?"

![Restore task with Grid Presets and Modules subtabs](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/restore.png)

## Grid Presets

A Grid Preset saves everything you set up in the Smart Panel, as a named preset you can load back with one click:

- Grid size (X, Y, Z).
- Spacing, Steps, Stagger, and Rotation.
- The building and its selected production recipe.
- Auto-Connect settings.

Use the Grid Presets tab to:

- **Save from Panel** — name your current panel setup and save it as a preset.
- **Load to Panel** — load a saved preset back into the panel to fine-tune before building, or apply it as-is.
- **Export Code / Import Code** — share a preset as a code, or load one someone shared with you.

Each preset shows a plain-language summary of what it holds (building, grid, transforms, recipe) so you can tell them apart at a glance.

## Modules

A Module captures a whole **Extend manifold** — a wired unit of buildings with their belts, pipes, lifts, distributors, and power — that you can stamp down repeatedly.

- After you preview or build a Smart [Extend](Extend), it lands in the **Extend clipboard** at the top of the tab. **Save as Module** turns that captured layout into a named, reusable Module.
- **Load Module** for a saved Module to rebuild that layout: equip its source building and it becomes a live preview you can **stamp** (fire to place, repeatedly) and **scroll to scale** on the fly.
- Because a restored Module isn't locked to one side the way a fresh Scaled Extend is, you can **rescale the run in either direction** along its axis before placing — scroll one way to grow it one way, the other way to grow it the other.
- **Export Code / Import Code** share Modules the same way presets do.

Each Module shows a summary of its parts (how many belts, pipes, distributors, and so on) so you know what it rebuilds.

![The Modules tab shows a captured Constructor layout and its matching saved entry beside the source buildings.](https://raw.githubusercontent.com/wiki/majormer/SmartFoundations/images/v35/saved-modules-constructor.png)

The Modules tab shows a captured Constructor layout and its matching saved entry beside the source buildings.

## Layers and Saved Modules

Use **Grid Z** to repeat the saved module on additional floors, and **Z spacing** to adjust the floor interval. Each layer rebuilds its captured factories and infrastructure with normal material costs.

Saving after a scaled Extend captures **one complete module**, so it can be repeated again in Restore. If an older module contains extra poles/cables or cannot identify its cable endpoints, recapture it from a working layout. Existing saved layouts are not rewritten automatically.

Pipe floor holes retain their captured top and bottom attachments on the correct floor. Power cables preserve each outlet face and its available slots. A cable beyond the normal game range makes the Restore preview invalid; reduce spacing or adjust the layout before building. A changed source layout or material quote can also require selecting the source again.

Water Extractor Modules remain horizontal and validate water depth and terrain for every child. Associated route supports retain their captured placement. Restore does not enable arbitrary cloning of another mod's factory buildings.

**Load to Panel** loads editable placement values; it does not construct a saved grid. **Load Module** arms a captured layout for placement. Read the selected entry's summary and material quote before building.

## Panel and world controls

Use **K → Restore** to save, select, load, import or export layouts. After loading, use the Scaling Panel or wheel/numpad controls to adjust the placement; the HUD reports the resulting grid and active adjustment. Fire the build gun to construct it at the displayed cost.

To cycle recently applied restores without reopening the Panel, hold `U` and press `Num 9` for the next recent preset or `Num 3` to step back. See [Controls](Controls).

## Sharing Is Progression-Safe

Shared presets and Modules are checked against your current unlocks before they can be imported or applied. If one needs a building, recipe, belt or pipe tier, lift, splitter, merger, power component, or other piece you have not unlocked yet, Smart! rejects it instead of letting you build something you could not place by hand.

That keeps shared setups honest: importing one never skips progression.

## See Also

- [Extend](Extend) — the feature whose manifolds the Modules tab captures.
- [Smart Panel and HUD](Smart-Panel-and-HUD) — where the Restore tab lives.
- [Grid Scaling](Grid-Scaling) and [Transforms](Transforms) — what a Grid Preset saves.

---

_Last updated: 2026-10-22 · Smart! v35.0.0 / Smart Camera v2.0.0 (release draft)_
