# Changelog

All notable changes to Smart! will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Audience note:** This changelog is read by players, not developers. Entries should describe what the user experiences — what was broken, what it felt like, and what's better now. Class names, internal APIs, and implementation details belong in code comments or design docs, not here. Unless an entry says otherwise, changes apply to both single-player and multiplayer.

---

## [34.1.0] - 2026-07-10

> *Player Relative Controls: an optional way to build in the direction you're looking, so the grid grows toward where you face instead of along fixed compass axes.*

### Added

- **Player Relative Controls (optional, off by default)** - A new setting under Building Behavior. Turn it on and building follows your view instead of the world's fixed axes: scroll to grow the array toward wherever you're looking, glance to the side to extend it sideways, and the numpad becomes a compass - Num8/Num5 mean away from and toward you, Num6/Num4 mean to your right and left, Num9/Num3 mean up and down. This applies to scaling and to the spacing, steps, stagger, and rotation adjustments, so the whole grid responds to where you're aiming. Prefer the mouse? While you hold a transform key, the HUD names what the wheel is driving in view terms - Forward, Lateral, or Vertical - and re-tapping the key switches between them. Scrolling up on Forward always grows the build in the direction you're facing: from the near end it stretches away, and if you walk to the far end and look back, scrolling up pulls it toward you. Your existing controls are untouched when the setting is off, and the Smart Panel keeps its familiar X/Y/Z values either way.
- **Re-tap a transform key to switch axis** - While holding a transform key (Spacing, Steps, Rotation), quickly release and re-press it to jump to the next axis - no more reaching for Num0 mid-build. Works in both classic and Player Relative controls (in Player Relative it steps through Forward, Lateral, and Vertical). Re-tapping the Stagger key switches its lean direction; Num0 toggles its Stack/Flat family (see below).
- **Chain your recent restores without the panel** - While holding the Recipe/Settings key (U), Num9 steps through the Smart Restore presets you applied this session (newest first) and Num3 steps back, wrapping around the list. Each step arms that preset exactly like pressing Apply in the panel - your click still places it - so repeating a layout is just U+Num9, click, U+Num9, click. The list remembers your last 8 applies (grid presets and modules alike), the HUD shows which one is armed, and it clears when you exit the game. (Issue #473, requested by 𝖗𝖚𝖎𝖓𝖊𝖉 on the Smart! Discord)

### Fixed

- **Extend and Smart Restore now copy paint to floor holes, wall holes, valves, and pumps** - Cloned pipe floor holes, wall holes, and inline valves/pumps always came out default metal even when the source was painted. They now inherit the source's full customization (swatch, pattern, material, skin) like every other cloned part. (Issue #475, reported by 𝖗𝖚𝖎𝖓𝖊𝖉 on the Smart! Discord)
- **Saved restore presets now remember paint** - A module preset used to lose all customization when restored after a game restart or in another world (it could only copy colors from buildings still standing in the world). Presets now capture the appearance of every part - swatch, pattern, material, skin, custom colors - at capture time, and replay it faithfully anywhere, anytime. Shared presets only apply customizations you have actually unlocked. Existing presets keep working as before; to pick up paint memory, capture the module again from your factory (re-saving the old preset alone can't add what was never captured). (Issue #477)

### Changed

- **Stagger navigation is now two families instead of one long cycle** - Stagger's four patterns are grouped by what you're building: **Stack** (a vertical pile that leans as it rises) and **Flat** (a row that drifts sideways as it runs). Num0 toggles between Stack and Flat, re-tapping the Stagger key switches direction within the family, and Num9/Num3 jump straight to Stack or Flat. Same four patterns as before and your saved presets are unaffected - they're just quicker to reach than cycling through all four.

> **A heads-up on controls:** Player Relative is off by default, so nothing about how you build changes unless you turn it on - but once you do, it genuinely rewires the muscle memory for using Smart! out in the world (not the Smart Panel, which stays exactly as it was). The scroll wheel and numpad follow where you're looking instead of fixed compass axes, so give yourself a few builds to settle in - it tends to click quickly. We're keeping it opt-in for now while people try it, and depending on your feedback and how it feels in practice, a future update may make Player Relative the default for Smart!'s in-world controls.

---

## [34.0.0] - 2026-07-07

> *Smart! Blueprints: scale your own blueprints into a grid, and Smart! wires the belts and pipes between the copies for you. Lay down a whole tiled factory - dozens of blueprint copies, connected across every seam - in a single placement, including the two-dimensional grid connections the game's own blueprint auto-connect can't do.*

### Added

- **Smart! Blueprints - scale a blueprint into a connected grid** - Blueprints could never be scaled with Smart! before; holding one and scrolling did nothing. Now a held blueprint scales into a grid of copies just like any other building - along X, Y, and Z, with spacing and stepping - and Smart! automatically connects the belts and pipes that reach the edge of each copy to the matching connectors on its neighbors. Design a blueprint whose belts (or pipes) run up to its border, then stamp out a 4x2, a 3x3, or any grid you like: every seam between copies is wired in one placement, the connecting belts and pipes are previewed and priced before you build, and any connection the game itself couldn't route is skipped and reported on the HUD just like the rest of Auto-Connect. Because Smart! wires the copies together directly, an interior copy can connect on all four sides at once - a true two-dimensional fabric of blueprints, which the game's built-in blueprint auto-connect can't produce. Pipes go one further: they also connect **vertically**, so a blueprint with pipe ports on its roof and floor stacks into a self-connected tower - scale in all three axes and the pipework wires through the whole block. (Vertical *belt* runs between stacked copies need conveyor lifts, which Smart! doesn't place yet - horizontal belt seams only for now.) Picking up a blueprint also defaults spacing to 1m on every axis so the connecting runs have room to exist; if a grid comes out missing its connections, the HUD tells you they were skipped as "too close" - just widen the spacing a little. Every copy is a real, independent blueprint instance - dismantle one and you remove just that copy, leaving the rest of the grid (and the connecting belts and pipes) in place. Everything works in multiplayer as well as single-player. (Issue #168)

### Changed

- **Auto-Connect skip messages now stand out in red** - When Auto-Connect skips a belt or pipe (too steep, too far, invalid shape, and so on), the HUD summary telling you what was skipped is now shown in red instead of the usual highlight, so a missed connection is easier to catch at a glance. It stays red in every HUD theme.

---

## [33.5.4] - 2026-07-06

> *Auto-Connect gets more reliable on stacked, multi-level grids: connections stay on their own level, the belt and pipe lanes between distributors fill in properly, port choices are stable and repeatable, and belts and pipes now connect wherever the game itself would let you place one by hand. The HUD also tells you, up front, when a connection had to be skipped and why.*

### Added

- **The HUD now tells you when Auto-Connect had to skip a connection** - When a belt or pipe connection can't be made, it's always been silently absent, leaving you to notice the gap yourself. The Smart! HUD now shows a short summary while you're aiming - for example, *"2 belt connection(s) skipped: too steep"* - so you know something didn't connect and why (too steep, invalid shape, too far, or a lane that couldn't be completed). Nothing is blocked: the rest of the grid still builds, you're just told what's missing. The line only appears when something was actually skipped.

### Fixed

- **Belts and pipes now connect anywhere you could place one by hand** - Auto-Connect used to reject a belt or pipe whenever the straight line between the two connectors was too steep, even when a real conveyor or pipe would simply run out flat and curve to reach it. So a stack of splitters (or pipe junctions) placed close to a machine would leave its upper members unconnected until you moved the stack away or staggered it. Auto-Connect now lets the game itself judge the actual routed shape: if it's a belt or pipe you could place manually, Smart! builds it. (Issue #466, reported by DragonsKeep on the Smart! Discord)
- **Auto-Connect no longer invents connections between levels on staggered, multi-level grids** - When you scaled a grid of splitters or mergers across two or more height levels (especially with stagger), belts and pipes could connect a distributor to a machine or neighbor on a *different* level instead of the matching one on its own level, producing steep diagonal runs and crossovers. Connections now strongly prefer the same level, only reaching across levels when there's genuinely no same-level option. (Issue #464, reported by dmeyster on the Smart! Discord)
- **The belt and pipe "lanes" between stacked distributors now connect reliably** - On multi-level grids, the connecting runs *between* splitters (or between mergers) along each level could come up short or go missing entirely, leaving isolated distributors. Those lanes now form consistently on every level. (Issue #464)
- **Auto-Connect port choices are now stable and repeatable** - Identical machines in a row could each pick a *different* input/output port for no visible reason, and simply scaling a grid larger could reshuffle the ports on machines earlier in the line that you'd already set up. Port selection is now deterministic: identical layouts connect identically, and growing a grid no longer disturbs connections that were already correct. (Issue #464)

## [33.5.3] - 2026-07-05

> *A Smart Restore redesign and a smarter Smart Upgrade: Restore is rebuilt into two clear tabs - Grid Presets and Modules - docked to the Smart Panel, and Smart Upgrade's network scan gains precise tier-to-tier targeting plus fixes for aiming, pipe junctions, and refreshing after an upgrade.*

### Added

- **Smart Upgrade's network scan can now upgrade one tier at a time** - Scanning a connected belt/pipe/pole network for upgrades used to only let you pick a *target* tier and then upgrade **everything** below it. You can now pick a specific source tier - for example, bump only your Mk.2 belts to Mk.3 and leave the Mk.4s untouched - matching the tier control the radius scan already had. Click a tier in the scan results to target just that tier, or pick **All tiers** to upgrade everything below the target as before. (Issue #456, requested by Infarctus on the Smart! Discord)
- **You can set how much each scroll notch changes a transform** - Spacing, Steps, Stagger, and Rotation now have their own configurable per-notch increments in the mod settings (under **Building Behavior**), so you can tune how far one mouse-wheel notch moves each transform instead of the fixed 0.5 m / 5°. The same increments drive Extend, Restore, and Smart Walking. (Issue #217)

### Changed

- **Smart Restore is redesigned into two clear tabs: Grid Presets and Modules** - The old Restore panel mixed two very different things together and was hard to follow. It's now split into two tabs, docked to the Smart Panel as a single unit. **Grid Presets** save and reload everything you set up in the Smart Panel - grid size, spacing, steps, stagger, rotation, the building and its recipe, and your auto-connect settings - as named presets you can load back with one click, ready to fine-tune or build. **Modules** capture a whole Extend manifold - a wired unit of buildings with their belts and pipes - that you can then stamp down repeatedly and rescale on the fly. Each tab shows a plain-language summary of what a preset or module contains, and both still support sharing via import/export codes. (Issue #427)
- **The Smart Upgrade panel now matches the Smart Panel's look** - The Upgrade panel's colors and buttons were inconsistent with the rest of Smart!; it now uses the same dark theme and orange accents. (Issue #456)

### Fixed

- **Network scan no longer depends on which tier you're holding** - With the Smart Upgrade panel open (you're already holding a belt, pipe, or power line), aiming at a belt/pipe/pole and pressing **Scan Network** only found a target if you happened to be holding a *different-tier* belt than the one you were pointing at - hold a matching tier and it found nothing. It now anchors on whatever you're aiming at, whatever tier you have in hand. (Issue #456)
- **Network scan follows pipes through junctions** - A network scan stopped at cross and T pipe junctions, so it missed every pipe on the far sides and under-counted the network. It now crosses junctions - and chains of them - to walk the whole connected pipe run. (Issue #456)
- **Network scan results refresh on their own after an upgrade** - After upgrading part of a scanned network, the results kept showing the old tier counts until you manually scanned again. They now refresh automatically, including for players on a multiplayer server. (Issue #456)
- **Power pole auto-connect wires follow the grid again** - After the 33.4.0 scaling update, auto-connected power lines across a grid of poles came out crossing diagonally instead of running neatly along the rows or columns, and no Grid Axis setting (Auto, X, Y, or X+Y) lined up with the grid. The wiring now follows the grid correctly for every axis mode. (Issue #459)
- **Restore modules no longer wire belts backward when scaled in reverse** - Stamping a Smart Restore module and scaling it in reverse (outward along the positive X direction) wired the manifold belts backward - a belt would leave a splitter's input and run into the next splitter's output, reversing the flow. Belts now always run output-to-input, whichever direction you scale. (Issue #460)
- **Scaled conveyor lift floor holes sit flush on dedicated servers** - Scaling out a row of conveyor lift floor holes on a dedicated server built every hole but the first a half-step too high, even though the build preview looked correct. They now all seat flush with the foundation, in both scaling directions and with or without stepping. (Issue #458)
- **Zooping a sign no longer collides with Smart scaling** - Signs and billboards support the game's own Zoop, but Smart! didn't recognize a sign in Zoop mode the way it does foundations, so the two could fight over the same placement. Smart scaling now stands aside for a zooping sign (shown as "Zoop Active" on the HUD) exactly as it does for foundations - you can Zoop signs freely, and normal Smart sign scaling still works when you're not zooping. (Issue #330)
- **Middle-click-sampling a factory no longer blocks placing it in a Blueprint Designer** - Using middle-click (Sample Building) on a factory that Smart's Extend could act on left you unable to drop it into a Blueprint Designer ("Hologram cannot be placed in Blueprint Designer!"), even after you moved away from the building. Sampling now keeps the building blueprint-placeable, matching how picking it from the build menu already worked. (Issue #461, reported by Tonka Beans)

---

## [33.4.0] - 2026-07-03

> *A scaling performance release: large grids are dramatically smoother, growing a grid no longer refreshes the whole preview, huge grids fill in progressively instead of freezing, and - the big one - large grids now build on dedicated servers without disconnecting everyone. Plus a scaling crash fix and cleaner previews.*

### Changed

- **Large-scale building is dramatically more performant** - Scaling to big grids used to get sluggish and eventually near-unusable somewhere around a couple thousand foundations, because every previewed building was being nudged back into place every frame. Previewed buildings now hold their position on their own, so grids of tens of thousands stay responsive - you can scale far higher than before and still move around freely while you do it. (Issue #418)
- **Growing a grid only adds the new part, instead of redrawing everything** - Adding a row or column (or a stack) to an existing grid used to visibly refresh every building in the preview. Now only the newly-added cells appear; everything you already placed stays put and untouched. (Issue #418)
- **Huge grids fill in progressively instead of freezing** - Jumping straight to a very large grid (for example bumping a big grid up several stacks at once in the Smart Panel) used to lock the game up for a few seconds while everything spawned at once. The preview now fills in smoothly over a moment or two, so the game stays interactive the whole time. (Issue #418)

### Fixed

- **Large grids now build on dedicated servers without kicking everyone** - Committing a very large scaled build on a multiplayer server could freeze the server long enough that every connected player timed out and got disconnected - even though the build itself completed and saved. Big builds are now constructed in the background over a few moments so the server stays responsive and nobody gets dropped; the buildings appear progressively as they're built. (Issue #418)
- **Fixed a crash when shrinking certain rotated grids** - Scaling a rotated wall grid that curved back on itself (a silo-style loop) and then scaling it back down could crash the game. (Issue #418)
- **Cleaner scaled previews** - Scaled grids no longer show a scattering of stray white outline boxes over every previewed building. (Issue #418)

---

## [33.3.0] - 2026-07-03

> *A routing release: all auto-connect belt, pipe, and hyper tube routing now runs on the game's own routing (and shows the game's own "Invalid Pipe Shape" error when a shape can't be built), floor-hole pipes finally route like a hand-built pipe, and a batch of Smart Panel / HUD auto-connect preview fixes. (Originally scoped as patch 33.2.1; it became minor 33.3.0 when the routing convergence landed on top of the bug fixes.)*

### Changed

- **All auto-connect belt, pipe, and hyper tube routing now runs on the game's own routing, with the game's own error messages** - The floor-hole investigation (below) uncovered that Smart!'s conduit previews were routing with no build mode set, a state the game's routing never expects, and that a few paths hand-rolled their splines entirely. This release converges everything onto the game's real routing: Default-mode belt lanes now route with the belt's real default build mode; every routed pipe and hyper tube span (junctions, manifolds, stackable supports, Extend lanes, Smart Walking) is validated against the game's own bend-radius rule and turns red with the game's "Invalid Pipe Shape" message when a shape genuinely can't be built, instead of silently constructing it; and Extend manifold lanes - the last path still using a hand-made 2-point spline - now route through the game's router, so they honor your routing-mode setting, bow naturally with length, and no longer ramp oddly on sloped manifolds. (Issues #414, #401)

### Fixed

- **A non-default pipe or belt routing mode now stays shown on the HUD** - Changing the pipe or belt auto-connect routing style away from its default gave you no lasting on-screen reminder that it differed from your saved default (hyper tube routing already showed this; pipe and belt were missed). Any routing mode that deviates from your default now persists in the Smart! HUD readout, alongside the other settings that already behave this way. (Issue #454)
- **Floor-hole pipe auto-connect finally routes like the real thing** - Auto-connecting a pipe between a floor hole and a nearby building could render a twisted, self-clipping mess in every routing mode. The deep cause turned out to be that Smart!'s pipe previews were routing with no build mode set at all - a state the game's own routing never expects - which degraded every route shape. Previews now run with the game's real build mode active and route through the game's own per-mode routing, so each routing style (Auto, Auto 2D, Straight, Curve, Noodle, Horiz-to-Vert) produces the same shape you'd get building that pipe by hand: pipes leave the top of a floor hole straight up (or the bottom straight down), rise or drop to the connector's height, and enter the building connector straight on. The hole face is now chosen by height (a connector above the hole routes from the top, below from the bottom), and if a routing style genuinely can't produce a valid shape for the span, the preview turns red with the game's own "Invalid Pipe Shape" message - the same answer vanilla gives - instead of silently building a broken pipe. (Issue #437, reported by tendrils)
- **Turning off pipe auto-connect now clears the pipe previews right away** - Holding a pipeline junction near your factory buildings and switching auto-connect off from the HUD left the preview pipes hanging on screen as if they were still connected. (Belts already cleared correctly; pipes were the holdout.) Toggling pipe auto-connect off now removes those previews immediately, matching how belts behave. (Issue #450)
- **Changing pipe settings in the Smart Panel rebuilds the previews cleanly** - Changing the pipe tier, style, or routing from the Smart Panel could leave a tangle of stale, overlapping pipe previews - the style toggle didn't refresh at all, and the ones that did refresh only updated some of the previews in place instead of rebuilding them. Pipe setting changes now fully rebuild the previews, so what you see always matches the settings you picked. (Issue #451)
- **The belt Chain toggle in the Smart Panel updates previews immediately** - Toggling the distributor **Chain** setting from the Smart Panel didn't refresh the belt previews until you nudged the hologram, unlike every other belt setting. It now updates right away. (Issue #452)
- **No more stray pipe dropping out the bottom of a pipeline junction** - Disabling and re-enabling auto-connect from the Smart Panel while holding a pipeline junction could spawn an extra pipe preview that dropped straight down out of the junction toward a nearby building, as if the junction were a floor hole. It no longer does. (Issue #453)

---

## [33.2.0] - 2026-07-02

> *Smart! now plays nice with Infinite Nudge's mouse-wheel rotation - the reason this is a minor version bump instead of a patch - plus a round of auto-connect and build-mode bug fixes: belts no longer vanish when you turn off distributor chaining, blueprints with auto-connected power poles work on dedicated servers, Zoop mode no longer eats your scaled grid, wall pipe supports get their panel settings back, and the hyper tube length error tells the truth.*

### Changed

- **Smart! now claims the scroll wheel only while it's actively managing a building** - holding a Smart! modifier, or while Auto-Hold keeps a scaled grid pinned - instead of the older approach of temporarily removing the vanilla Build Gun input context. This is a behavioral change to how Smart! arbitrates mouse-wheel input with other mods, which is why this release is versioned as a minor bump rather than a patch. See the Infinite Nudge fix below for the specific problem this solves.

### Fixed

- **Infinite Nudge no longer rotates your building while you scale with Smart!** - With the Infinite Nudge mod installed, holding a Smart! modifier (X/Z) and scrolling to scale ALSO rotated the hologram at the same time - and changing your keybinds didn't help. It turned out not to be a keybind clash at all: Smart! holds the building in place while you scale, and Infinite Nudge treats any held building as "scroll to rotate." The mouse wheel is now reserved for Smart! whenever Smart! is the one holding the building - while a modifier is held, and while Auto-Hold keeps your grid pinned (where Infinite Nudge's rotation would twist each piece of the grid individually). A building **you** lock yourself keeps full Infinite Nudge scroll behavior, and everything is untouched when Infinite Nudge isn't installed. (Issue #162)
- **Blueprints with auto-connected power wires no longer corrupt circuits on dedicated servers** - Saving a blueprint after Smart! auto-connected your power poles produced a blueprint that was missing its wires: the poles remembered their connections, but the wires themselves never made it into the blueprint. Placing that blueprint - most visibly on a dedicated server - left poles claiming connections that didn't exist, flooded the server log with circuit-repair warnings every tick, and could corrupt the save beyond loading. Wires Smart! creates inside the Blueprint Designer are now registered with the designer exactly like hand-built wires, so they save into the blueprint and place back intact. This also covers wires from Extend and multiplayer grid builds done inside the designer. Existing broken blueprints need to be re-saved in the designer after this update. (Issue #421, reported by SonarClouds)
- **Zoop and Vertical modes no longer swallow your scaled grid** - If your build mode was still set to Zoop or Vertical from earlier building, scaling out a Smart! grid and clicking to place it built nothing - the whole grid you'd just set up simply vanished, and you had to notice the mode, switch it back, and redo the scaling. Those modes can't place a Smart! grid, so the moment you scale past 1x1x1 the build mode now snaps back to Default automatically. Zoop without scaling is untouched, and a mode you pick while the grid is at 1x1x1 stays yours. (Issue #296, reported by SysC0mp)
- **The Smart Panel shows pipe auto-connect settings for wall pipe supports** - Opening the Smart Panel (K) while holding a wall pipeline support hid the entire pipe auto-connect section - stackable supports and floor-hole pipes showed it fine, wall supports were the one type left out. The section now appears for wall pipe supports too, with the same connect-to-building options the other pipe supports get. (Issue #404)
- **The hyper tube "too long" message now shows the real limit** - Over-stretching a hyper tube span in Smart Walking or auto-connect showed "too long (Xm > 56m)" even though hyper tubes can actually span about 95m - the message borrowed the belt/pipe limit and told you to shorten far more than you needed to. The error now reports the actual cap for whatever you're building. (Issue #417)
- **Turning off Chain no longer erases your auto-connect belts to the factory** - With Auto-Connect running on a row of distributors (splitters/mergers), turning off the **Chain** setting - which should only remove the manifold lane linking the distributors to each other - also made the belts running from each distributor to your factory buildings flash briefly and then disappear entirely, even though those connections were still valid. The two kinds of belt were being tracked together internally, so clearing one could wipe out the other. Chain now only removes the manifold lane; the belts to your buildings stay put. (Issue #436)
- **Floor-hole pipe auto-connect renders less distorted** - Auto-connecting a pipe from a floor hole to a nearby building could render with an ugly twist or kink right at the hole, in every routing mode. Part of the cause is fixed here (routing-mode leg order, pipe mesh twisting on the vertical section, and mismatched connector tangents). The underlying shape still isn't fully correct in every case - full details in Issue #437, which stays open. (Issue #437, reported by tendrils)

---

## [33.1.2] - 2026-06-30

> *A bug-fix patch: a multiplayer water-extractor crash, auto-connect routing when you Extend off a Smart Restore, a stray wall hole left behind when you Extend a blocked lane, a few auto-connect manifold fixes, a clearer disable message, and two conveyor-lift upgrade fixes (a through-floor lift flipping, and tall lifts being over-charged).*

### Fixed

- **Multiplayer: placing a row of water extractors no longer crashes** - In multiplayer, using Smart! to scale out a row of **water extractors** instantly crashed the game the moment you clicked to build - on both a player host and a dedicated server. The scaled copies were being built on the host without the special handling water extractors need, so they hit a fatal error. They're now built correctly, and a row of water extractors places without crashing. (Issue #428, reported by Infarctus on the Smart! Discord)
- **Auto-connect routes correctly when you Extend off a Smart Restore** - Scaling an Extend directly off a Smart Restore preview - before placing the first copy - could route the auto-connected belts and pipes between buildings the wrong way, looping them back on themselves into impossible shapes, even though the same Extend worked fine if you placed one copy first. The restored layout's connection directions are now kept instead of being overridden. (Issue #422, reported by Don on the Smart! Discord)
- **Pipe manifolds connect in order on Y-axis and rotated runs** - A row of pipe junctions running along the Y axis (or a rotated run) could be linked in the wrong order, so the connecting pipes crossed over each other instead of joining neighbor-to-neighbor. They now chain in run order regardless of direction, the way belt manifolds already did. (Issue #423)
- **Belt manifolds no longer flip their connections** - When a splitter/merger manifold's two possible link directions were nearly equal, the chosen direction could flip between frames and occasionally cross the manifold belts. The choice is now stable. (Issue #424)
- **Pipe auto-connect picks the right connector on large or rotated junctions** - On bigger or rotated pipe junctions, the opposite-side search measured from the junction's center rather than its actual connector, so it could occasionally accept or reject the wrong building connection. It now measures from the connector itself. (Issue #425)
- **The double-tap disable message now reads "Smart: Scaling only"** - Double-tapping Num 0 turns off Auto-Connect and Extend for one build, but scaling keeps working - yet the on-screen message read "Smart Disabled," which over-claimed. It now reads "Smart: Scaling only," so it's clear that scaling is still active. (Issue #426)
- **Extend no longer leaves a stray wall hole behind on a lane it can't rebuild** - When you Extend a building and one of its belt or pipe lanes can't be reconnected - for example, the splitter feeding that lane already has its manifold connection used up, so the copied belts have nowhere to plug in - Smart! correctly skips cloning that lane's splitter and belts. But the wall-hole decoration for the lane was still placed, stranded on the original building where it didn't belong (and because it could land on top of a pipe, it looked like a stray pipe wall hole). The wall hole is now dropped along with the lane it belongs to. (Issue #431)
- **Upgrading a conveyor lift through a floor hole no longer flips it** - Using Smart! to upgrade a conveyor lift that passes through a floor hole could rebuild it flipped: the run reversed to run downward, its geometry scrambled, and the lift at the bottom disappeared. Smart! wasn't carrying the lift's flow direction and its floor-hole connections across the rebuild. Lifts now keep their direction and their floor-hole passthroughs when upgraded - whether the hole is at one end or both. (Issue #399)
- **Upgrading a tall conveyor lift no longer over-charges you** - Smart! was charging too much to upgrade a conveyor lift: the cost got scaled by the lift's height a second time, so a tall or floor-hole lift could cost several times its real price - sometimes enough to trip an "out of materials" stop that halted the rest of your upgrade. Lift upgrades now charge the correct amount, the same way belt and pipe upgrades already did. (Issue #432)

---

## [33.1.1] - 2026-06-28

> *A quick rotation fix: the Y-axis build-up now spaces its rows properly.*

### Fixed

- **Rotation along the Y axis now spaces its rows correctly** - With the Rotation transform set to build up along the **Y axis** (rows fanning out around the vertical), the **Spacing Y** value was ignored, so the rows collapsed into a tight, overlapping fan instead of spreading out. Smart! was always measuring the arc spacing along X regardless of the chosen axis; it now follows Y, so Spacing Y spreads the rotated rows the same way it does on the X axis. (Issue #419)

---

## [33.1.0] - 2026-06-26

> *Hyper Tubes join the Smart! family.* Auto-Connect and Smart Walking now understand stackable hypertube supports — scale out a line of them and Smart! lays the hypertube run between them, or walk a steered hypertube route the same way you already do with belts and pipes.

### Added

- **Hyper Tubes — Auto-Connect and Smart Walking support** - Smart! now works with **Stackable Hypertube Supports**. Scale out a row of them and Auto-Connect lays the hypertube run between consecutive supports automatically, just like it already does for conveyor poles and pipeline supports; and Smart Walking can steer a hypertube run that turns and climbs, the same way it walks belts and pipes. All the new on-screen text is translated across Smart!'s 20 supported languages. (Issue #405)

### Fixed

- **Build axis arrows now appear on Linux** - On Linux, the X / Y / Z build-direction arrows that Smart! draws above a hologram while you build or scale never showed up. Smart!'s safety check for the arrow meshes relied on an assumption that holds on Windows but not on Linux, so it mistook the perfectly good arrow meshes for corrupted data and skipped them every time, leaving you with no axis arrows. The arrows now load and draw on Linux - this was a display-only issue that never affected Windows and never caused a crash. We don't have a Linux setup to test on our end, so this ships for feedback: if you play on Linux, let us know whether the build arrows are back. (Issue #415, reported by regnare on the Smart! Discord)
---

## [33.0.0] - 2026-06-24

> *A major new build mode: **Smart Walking**. Where scaling lays down a rigid uniform grid, Smart Walking builds a **path** — a connected run that walks forward one segment at a time, and each segment can turn a corner, climb a slope, or shift aside. One continuous run of belts or pipes routes exactly where you want it, while you stay put and watch the leading edge crawl across the map through the Smart! Camera. This release also brings Smart! to around 20 languages and gives the Smart! Panel a refresh.*

### Added

- **Smart Walking — steer a connected run that turns, climbs, and walks to its destination** - A whole new way to build with Smart!. Where scaling stamps out a rigid, uniform grid, Smart Walking lays down a *route* - one connected run that walks forward a segment at a time, where each segment can round a corner, change grade, or shift sideways, so a single run goes exactly where you want it instead of stopping to pivot and start a fresh one by hand. Hold a stackable conveyor pole or pipeline support on the build gun, press **K** to open the Smart! Panel, and click **Smart Walking** to begin - the button stays hidden unless what you're holding can start a run. The run lays auto-connected belts or pipes to match what you're holding. Scroll to advance and lay each new segment; steer the active (head) segment to turn, rise (climb), shift, and set its spacing; back up to undo a segment; and commit the whole run in one build. Turns sweep all the way around to a near-full loop (about 270°), and chained turns route cleanly through every corner. Both belts and pipes are fully supported - belts get tier, flow direction, and routing; pipes get tier, routing, and a Normal-vs-Clean style (Clean drops the flow indicators, once you've unlocked clean pipes). A Smart Walking panel (toggle with K) lists every segment in an editable table - fine-tune any segment's advance, turn, rise, and shift by hand with the preview updating live, switch the run's tier, direction, routing, and style from dropdowns, and read each segment's exit heading as a 16-point compass bearing; a footer button returns you to the Scaling panel. The full material cost is charged on commit, and the whole preview turns red when you can't afford the run so you know before you build (this respects No Build Cost and pulls from central storage, like the rest of Smart!). An invalid shape is flagged the same way: a segment that runs too long (over 56 m) or, for belts, climbs too steeply (over 30°) turns red, can't be committed, and the readout explains why - for example, "segment 2 too steep (83° > 30°)". With the Smart! Camera companion mod, the picture-in-picture view latches onto the head of the run and follows it across the map as it turns and climbs. Works in single-player and multiplayer, and the whole feature is translated across 20 languages. Designed from the ground up to extend to hyper-tubes and more conveyance types in future updates. (Issue #356)

### Changed

- **Smart! Panel refresh — new Smart Walking button, X-to-close, bigger Apply/Reset** - The Smart! Panel got a visual cleanup. When you're aiming at a buildable that can start a route (a stackable conveyor pole or pipeline support), a new **Smart Walking** button appears in the panel - click it to drop straight into the new mode. The panel now closes with a clear **X** in the top-right of its header instead of a separate Close button, and the **Apply** and **Reset** buttons were enlarged so they're easier to read and click, with the panel labels re-styled for a more consistent look. The Smart Walking button only appears when the held build can actually start a run, so it never widens the panel otherwise. (Issue #356)
- **Broader translation coverage across all languages** - More of Smart!'s on-screen text - the Smart! Panel, the in-build HUD readouts, and the keybind hints, plus everything new in Smart Walking - is now translated, improving coverage across all of Smart!'s supported languages: German, Spanish, French, Italian, Japanese, Korean, Polish, Brazilian Portuguese, Russian, Turkish, Simplified and Traditional Chinese, Bulgarian, Hungarian, Norwegian, Ukrainian, Vietnamese, Arabic, Persian, and Thai. Spot something that reads wrong in your language? Translation corrections are welcome on the GitHub Issues page.

### Fixed

- **Stackable conveyor poles: no more belt left behind when you un-scale** - Scaling a stackable conveyor pole out and then back in removed the cloned poles but left the connecting belt preview floating where they used to be - it only vanished once a new pole spawned on the other side. Un-scaling now removes the belt along with its poles, matching the stackable-pipeline fix from 32.1.2. (Issue #397)
- **Conveyor pole belts curve through turns again** - On a rotated (arc) run of conveyor poles - stackable, ceiling, wall, or regular - the auto-connected belts cut straight across and kinked sharply at each pole instead of bowing smoothly through the turn, and the Curve belt-routing mode had no visible effect. Belts now exit each pole along its facing and follow the curve of the run, while still running straight on a straight grid and no longer ramping on height changes. (Issue #398)
- **Pipelines curve through turns too** - The same straight-cut/facet problem affected auto-connected pipelines on a rotated run of pipeline supports; pipes now bow through the curve like belts do, matching the fix above. (Issue #400)
- **Stackable pipeline supports now show their auto-connect settings** - On a scaled stackable conveyor pole, holding the auto-connect key brings up its settings to cycle through (belt tier, routing, and so on); on a scaled stackable *pipeline* support the same press showed nothing - the in-build settings menu skipped pipe supports entirely. Pipe supports now surface their auto-connect settings (tier, routing) just like the belt poles do. (Issue #403)

---

## [32.1.2] - 2026-06-20

> *A community-reported bug-fix patch: Blueprint Designer recipe loss, multiplayer recipe selection, stackable pipelines, Extend manifold clearance, recipe memory that clears with the build gun, the Auto-Hold default, Smart Camera Extend tracking, Smart Restore scaling/camera, and a full logging-noise pass.*

### Changed

- **Auto-Hold on Grid Change now defaults on** - The option that automatically locks the hologram in place after you change its grid (so it doesn't drift while you reposition the camera) now defaults to **on**, restoring the behavior Smart! had before this became a toggle. You can still turn it off in the mod config, and the vanilla Hold key releases any individual lock. (Issue #279, originally requested by Alex)
- **Much quieter logging by default** - Smart!'s diagnostic logging was audited end to end. The many status messages that used to print during normal building, scaling, and connecting are now diagnostic-only, and the internal log categories default to errors-only. Together with the chain-diagnostic fix below, the game and dedicated-server logs now stay essentially quiet unless something actually goes wrong - smaller log files and a little less overhead in long sessions. (Any category can still be turned up at runtime for troubleshooting.)

### Fixed

- **Blueprint Designer: recipes no longer stripped when you recall a blueprint** - Recalling a blueprint *into the Blueprint Designer* while Smart! was active wiped the recipes from its machines - and once lost, they didn't come back on reload. The blueprint itself was fine (it placed correctly in the world, and loaded fine with Smart! disabled); Smart!'s in-session recipe pass was clearing recipes off blueprint-loaded machines. Smart! now leaves designer-loaded buildings' recipes alone. (Issue #368, reported by @w00kab, corroborated by Dende)
- **Extend: manifolds no longer blocked by another manifold close below** - Extending a manifold (e.g. a row of smelters) refused to build in the intended direction when another manifold sat less than four wall-blocks directly underneath, forcing the run the opposite way; at four walls it worked fine. The vertical clearance check was too tight. (Issue #385, reported by @maxstudy)
- **Stackable pipelines now scale like stackable conveyor poles, not like a pipe upgrade** - Selecting a stackable pipeline and scaling it made Smart! show the build gun's *upgrade* prompts and open the Upgrade panel on K, instead of the Smart! Panel - it was mistaking the scaled run for an upgrade of an existing pipe. Scaling a stackable pipeline now behaves like the stackable conveyor poles, with full grid/spacing/steps/stagger adjustment. (Issue #390, reported by @FusionPixelStudio)
- **Stackable pipelines: no more pipe left behind when you un-scale** - Scaling a stackable pipeline out and then back in removed the pipe pole but left the connecting pipe segment floating where the pole used to be (it only vanished once a new pole spawned on the other side). Un-scaling now removes the pipe along with its pole. (Issue #391, reported by @FusionPixelStudio)
- **Multiplayer: the recipe you pick now actually lands on the building you place** - On a dedicated server, choosing a recipe with the recipe wheel (hold U + scroll) and then placing a production building built it with the *wrong* recipe - the one you'd last middle-click sampled, not the one you selected. The game re-applied that stale sampled recipe over your choice. Smart! now keeps the game's recipe clipboard in step with your selection, per player, so the machine builds with the recipe you actually picked - matching single-player. (Issue #368, corroborated by Dende)
- **Remembered recipe now clears when you holster the build gun** - The recipe Smart! remembers (from picking one, or middle-click sampling a building) used to stay set after you put the build gun away - it still showed in the panel and pre-filled the next machine you placed, so you had to clear it by hand. Holstering (switching to a weapon or any other equipment) now clears it, matching vanilla's behavior of not carrying a recipe across build sessions. This also resolves the request to place buildings *without* a recipe pre-filled: rather than a separate toggle, the remembered recipe simply doesn't persist past the build session. (Issues #392 and #379; #379 reported by MasterFenix on the Smart! Discord)
- **Smart no longer floods the log in large factories** - Smart!'s internal belt-chain safety guards (which prevent a known crash during heavy building and dismantling) wrote a warning to the log every single time they ran - in a big, busy factory that meant tens of thousands of lines per session, badly inflating log file sizes and adding needless logging overhead. The guards still do their job; the messages are just silent now unless you turn on diagnostic logging. (Issue #393, reported by Sam (the most serious) in the Satisfactory Modding Discord)
- **Smart Camera now follows the head of an Extend run** - With the Smart! Camera companion mod, the picture-in-picture view didn't track the leading building while you scaled an Extend - it framed the wrong spot (it followed the wrong axis and ignored your spacing/steps), and the problem was especially visible in multiplayer. The camera now follows the furthest building in the run, whichever side you extend to, matching where the copies actually land. (Issue #373, reported by @Di3Qu3ll3; requires the Smart! Camera mod)
- **Smart Restore: Extend patterns scale the way you scroll, and the camera follows them** - Restoring a saved Extend pattern and scaling it had two problems: it would only grow in one fixed direction (you couldn't scroll the other way along X to extend back), and the Smart Camera framed the *opposite* end from where the buildings actually went. Restoring an Extend pattern now scales **bidirectionally** on X - scroll either way to extend that way (intentionally more flexible than a fresh Scaled Extend, which locks to the side you pick) - and, with the Smart! Camera mod, the picture-in-picture tracks the real head of the restored run. (Issue #394)

---

## [32.1.1] - 2026-06-15

> *A maintenance release: community-reported bug fixes, on a codebase reorganized so multiplayer issues are faster to track down.*

### Added

- **Rotation can now build up along the Y axis** - The Rotation control arcs a scaled grid by yawing each copy a little more than the last. Until now that build-up always ran along X, so the *run* curved as it extended. Now you can switch the build-up to the Y axis so the *rows* fan out around the vertical instead - either with Num0 while holding the Rotation key, or with the new X/Y selector on the Rotation row of the Smart! Panel. It stays a flat, upright rotation either way - nothing ever tilts or flips. (Issue #372, reported by @Theres-a-username-here)

### Changed

- **Smart! no longer auto-repairs conveyor chains when a save loads** - Smart! used to run an automatic conveyor-chain repair sweep a few seconds after every save load, to clean up corrupted chain bookkeeping. It no longer does. A save that carries chain corruption - which is most often introduced by *other* mods - now loads untouched, so Smart! neither silently changes your world nor gets blamed for problems it didn't cause. The runtime crash protections stay in place (Smart! still prevents the known belt-related crashes as you build and dismantle), and Smart! still tidies up the chains it builds itself. A separate, opt-in cleanup mod is planned for the rarer case where you genuinely need to repair an already-corrupted save. (Issue #367, reported by Mawie the Fox)
- **Under the hood: groundwork for faster multiplayer fixes** - The first stage of an ongoing internal reorganization: Smart!'s multiplayer networking code was consolidated into one place, and the features whose single-player and multiplayer behavior differ now carry a short map of exactly where they diverge. This is partial - more cleanup is planned - and nothing about how Smart! builds or plays changes. It's groundwork that makes multiplayer bug reports faster to track down and fix. (Ongoing refactor #377)

### Removed

- **The Upgrade panel's "Triage" tab** - The Smart! Upgrade panel had a third "Triage" tab with "Detect Issues" and "Repair All Issues" tools. These were added early on to clean up conveyor-chain bookkeeping problems that Smart!'s own Upgrade feature could introduce while that feature was still being stabilized. Upgrade has since settled and no longer causes those problems, so the repair tools are no longer needed - and leaving manual "repair all" buttons in place risks actions that could adversely affect a save. The tab and its tooling have been removed; the Radius and Network upgrade tabs are unchanged. (Issue #367)

### Fixed

- **Dedicated servers: no more chain-diagnostic log spam on shutdown** - Shutting down a dedicated server made Smart! log a warning for every factory building ("...died while still in a factory-tick entry..."). The warnings were harmless - that check is only meaningful when a building is destroyed mid-game - but they flooded the shutdown log. Smart! now only logs it for real in-game destruction, so server shutdown stays quiet. (Issue #381, reported by DarthPorisius)
- **Belt routing modes (Curve / Straight) now actually take effect** - Choosing Curve or Straight for your belt routing barely changed anything - auto-connected belts and Extend lane belts came out nearly straight regardless of the setting. Smart! now uses the game's own belt build modes, so Curve bows the belt exactly like the build gun's Curve mode and Straight runs dead straight - for both auto-connect belts and Extend / Scaled Extend lane belts, in single-player and multiplayer. (The short belts inside each extended machine are exact copies of the source and are intentionally left unchanged.) (Issue #380, reported by @SanderKlomp)
- **Pipe routing modes now all take effect (Auto 2D, Straight, Curve, Noodle, Horizontal→Vertical)** - The pipe routing dropdown offered six modes, but only the default actually routed - Curve, Auto 2D, Noodle, and Horizontal→Vertical had no visible effect on auto-connected pipes or Extend pipe lanes. Smart! now uses the game's own pipe routing for each mode, so every one routes just like the build gun does, for both auto-connect and Extend / Scaled Extend. (Issue #383)
- **Multiplayer: rotated Extend now builds the parent copy's belts and lane correctly** - On a dedicated server, extending a line of machines with rotation rotated each copy's machine, but the first ("parent") copy's belts stayed at the source orientation and the lane joining the next machine to it missed the connector - it only looked right in single-player. The server now rotates the parent copy's belts to match the preview and joins the lane to the parent's actual rotated position. (Issue #382)
- **Rotated Extend: pipe/belt lanes no longer cross over or drop out as the line curves** - Extending with rotation, the lane connecting each copy could jump to the *wrong* junction connector once the line curved far enough - crossing over near half a turn - and pipe lanes could stop generating entirely past roughly the 14th copy on a tight curve. Smart! now anchors every lane to the manifold's straight-through connector pair using the extend's fixed forward direction instead of each copy's curving offset, so lanes stay on the correct connectors and keep generating for the whole run, however far it's rotated. (Issue #384)
- **Rotated Extend is much smoother to adjust** - Spinning rotation (or spacing/steps) on a large Extend re-built every copy's belts and pipes on every scroll tick, which got heavy now that pipe lanes route real curves - causing noticeable lag. Smart! now coalesces rapid adjustments into a single rebuild once the value settles, so deliberate tweaks stay instant while fast scrolling no longer stutters.
- **Multiplayer: manifold lane belts/pipes now build at the tier you set** - On a dedicated server, the belts and pipes Smart! runs between machines when you Extend a manifold ignored your auto-connect tier setting - belts built at the highest unlocked tier and pipes were always Mk2 - even though the preview showed the correct tier. Smart! now builds the manifold lanes at your configured Belt/Pipe tier (sent from your client and applied on the server before wiring), so the construct matches the preview. (Issue #386)
- **Smart Upgrade (radius): no more "stuck" upgradeable count** - The radius scan could list a belt as upgradeable that the upgrade then skipped, leaving an "N upgradeable" you could never clear. The scan counted any belt within the radius, but the upgrade intentionally skips a connected belt chain that extends past the radius edge - so a chain crossing the boundary showed a phantom you couldn't fulfill. The scan now applies that same whole-chain rule, so the count matches exactly what the upgrade will do, in single-player and multiplayer. To upgrade a chain that crosses the edge, widen the radius or use Entire Map. (Issue #376, reported by @Hailey2010)
- **Blueprint Designer: Extend no longer previews or charges for copies that won't fit** - Using Extend or Scaled Extend on a building inside the Blueprint Designer could show copy previews spilling past the designer walls and include them in the cost quote - so the build gun warned "Hologram cannot be placed in Blueprint Designer!" and "Missing materials" even though the copies that *did* fit built correctly. Smart! now drops the out-of-bounds copies up front, so the preview and the materials quote reflect only what will actually build. (Issue #366)

---

## [32.1.0] - 2026-06-11

> *A fast follow to the multiplayer release: day-one community reports fixed, Smart! support inside the Blueprint Designer, pipeline support scaling, and a reworked Smart! Panel.*

### Added

- **Smart! works in the Blueprint Designer** - Building with Smart! inside the Blueprint Designer now behaves like the open world: auto-connect belts and pipes preview, space themselves, and build correctly, Extend and Scaled Extend work on designer-resident buildings, and everything Smart! builds is properly captured into the blueprint (and removed by designer clear). Previously Smart!-built belts and pipes were invisible to the designer - left behind on clear and missing from saved blueprints. (Issues #331, #365)
- **Scaled pipeline supports follow height AND angle** - The standard Pipeline Support is a two-step buildable (place, then height) with an adjustable vertical angle on its top piece for sloped pipe runs. Scaling a line or grid of them now mirrors both to every copy: adjust the parent's height or tilt and the whole line follows, in single-player and multiplayer. (Issues #291, #364)
- **Pipeline supports can now be scaled, with pipes auto-connected between them** - Pipeline Supports, Pipeline Wall Supports, and Pipeline Wall Holes support grid scaling: place one, scale out a line or grid. Scaled Pipeline Supports and Pipeline Wall Supports also build a connected pipe run between the copies, just like the Conveyor Pole does for belts - the run is plumbed end to end (fluid flows through every joint), and each support's snap point stays free so you can still connect pipes to it by hand. Thanks to Torkeug (Issues #291, #292, #293, #364)

### Fixed

- **Saving a blueprint no longer saves the whole world** - Placing Smart!-grouped buildings inside the Blueprint Designer could poison the blueprint save with references to the rest of your world. Designer builds no longer join Smart!'s one-click-dismantle groups (the designer has its own handling), keeping blueprint saves blueprint-sized. Thanks to i-am-not-choco (Issue #312)
- **Multiplayer: rotated scaled builds now construct rotated** - Building a scaled run with Rotation set (curved ramps and arcs) as a multiplayer client previewed correctly but built with every copy facing the same direction. The server now applies each copy's rotation exactly like single-player. Thanks to Aerlon from the Smart! Discord, reported within a day of 32.0.0 (Issue #363)
- **The Customizer's X key works again** - Satisfactory 1.2 bound the Customizer to X, which collided with Smart!'s Scale X. Smart!'s build keys are now only active while you're actually holding a buildable: empty hands, X opens the Customizer; buildable in hand, X scales. Also fixed the keys staying captured after canceling a build with Esc. Thanks to swallerwaller (Issue #358)
- **Smart! Panel dropdowns open in the right place** - The Belt Auto-Connect dropdowns (and every other dropdown on the panel) could open detached from their boxes, worst with the panel positioned low on the screen. The panel was rebuilt so menus anchor exactly where they render - and it picked up a cleaner, more compact layout along the way: Apply/Reset/Close in a row under the header, value columns that no longer collide with labels in any language, and better contrast. (Issue #352)
- **Crash fix: dismantling a building with a broken power-wire record** - Dismantling a building whose power connection carried a dead wire reference (left by an earlier failed connection or a save where the wire didn't load) crashed the game. Smart! now cleans up its own failed wire connections properly AND repairs any broken wire records it finds at dismantle time, so affected buildings - including ones from older saves - dismantle safely. (Issue #369)
- **Crash fix: belt-repair sweep on corrupted chain records** - Smart!'s automatic belt repair (which heals conveyor bookkeeping on save load and after builds) could crash if a save carried a corrupted conveyor-chain record holding references to removed belts. The sweep now detects these and removes them surgically; surviving belts are re-registered and rebuild cleanly. Items that were in transit on a corrupted chain are lost, which beats the alternative - a crash on every load. Thanks to Mawie the Fox from the Satisfactory Modding Discord (Issue #367)

---

## [32.0.0] - 2026-06-11

> *The multiplayer release: every Smart! feature now works in multiplayer on dedicated servers — Windows and Linux.*

### Multiplayer

- **Full multiplayer support** — Smart! now works in multiplayer. Grid scaling, belt/pipe/power auto-connect, Extend and Scaled Extend, Smart Upgrade, Smart Restore presets, and Smart Dismantle all build correctly when you play as a client on a dedicated server, with normal build costs charged exactly as the preview shows. Everything Smart! places is a standard, fully replicated game building — other players see it, can use it, and can dismantle it, just as if it were built by hand. (Issues #309, #334)
- **Linux dedicated servers** — Smart! is now packaged for Linux dedicated servers alongside the existing Windows server build, which covers most hosted-server providers. For server admins: install the same Smart! version on the server and on every client (the mod manager keeps these paired). (Issue #176)

### Fixed

- **Smart Dismantle now removes a restored single module as one group** — restoring a preset of a single Extend module (one machine plus its distributors and belts) built fine, but the dismantle tool would only take it apart one building at a time instead of offering the whole module, unlike scaled and extended builds. Restored modules now group for one-click dismantle like everything else Smart! places. (Issue #359)
- **Belt repairs while building and on save load** — after large Upgrade or Extend sessions, leftover internal conveyor records could make a stretch of auto-connected belt hold items without moving them (starving the machines behind it), and in rare cases crash the game when loading a save written in that state. Smart! now cleans these up as you build and upgrade, and repairs any affected belts when a save loads — including saves made on earlier versions. (Issue #360)

---

## [31.1.0] - 2026-06-08

> *A belt-focused update: conveyor belt runs between poles are finally rock-solid, and standard conveyor poles join the auto-connect family.*

### Added

- **Standard conveyor pole auto-connect** - Scaling a standard Conveyor Pole now works like the Stackable Conveyor Pole: place one, scale out a line, and Smart! auto-connects belts between the poles at the height you set. Raise or lower the pole height and the whole line of belts follows. (One quirk to know: set the line's size first and *then* adjust the height - changing the grid size while you're mid-height-adjust will drop you out of the height step.) (Issue #354)

### Fixed

- **Auto-connected belts between poles no longer stall or crash** - When Smart! auto-connected belts across a run of Stackable, Wall, or Ceiling conveyor poles, the belts could end up as a string of disconnected single pieces instead of one continuous line. On a save and reload that run would quietly stop moving items until you reloaded a second time - and in the worst case, connecting a feeder belt and running items through it could **crash the game**. This was long-standing (the stackable belt feature has carried this risk since around v29). Smart! now stitches each pole run into one proper conveyor line as it's built, so it transports correctly, survives save/reload on the first load, and the crash is gone. Your existing factories are unaffected; this only changes how new runs are built. (Issue #341)

---

## [31.0.2] - 2026-06-07

> *Dedicated-server packaging, a Creative Mode free-build fix, power-cable previews for Extend, plus Smart! Panel dropdown fixes.*

### Fixed

- **Smart! is now packaged for Windows dedicated servers** - The release now includes a server build alongside the game client, so the mod can be installed on a dedicated server and its version lines up with clients during updates. This is a packaging change only - multiplayer and dedicated servers are still **not officially supported**, and full multiplayer support is planned for a future update.
- **Smart! building is free again in Creative Mode / with No Build Cost** - When you play with No Build Cost on (the option Satisfactory 1.2 moved under Creative Mode, formerly Advanced Game Settings), Smart! still checked your inventory and could block builds as "unaffordable" - both auto-connected belts, pipes, and power lines, and Extend / Scaled Extend, which would pop a materials request. Smart! now recognizes free building the same way the base game does, so it builds without materials in Creative Mode. Normal games are unaffected (Issue #324).
- **Power cable previews are back for Extend and Scaled Extend** - When you Extend (or Scaled Extend) a building that carries a power connection, the preview now shows the power cables running between the source and each copy, including the cable droop, so you can see exactly how the run will wire up before you build. Previously the cables were missing, and an earlier attempt left them floating high above the poles; they now sit on the connectors and follow the rotation and chaining of each copy (Issue #345).
- **Power auto-connect wire preview no longer shows a red middle** - The wire preview drawn when Smart! auto-connects power could render its middle section in the red "invalid" tint even on a perfectly valid connection. The whole cable now previews in the correct color (Issue #346).
- **Smart! Panel Routing dropdown opens on Stackable Conveyor Poles** - On a Stackable Conveyor Pole, the Belt Auto-Connect *Routing* dropdown was empty and would not open. It now lists Default / Curve / Straight and applies your choice. Moving the panel by right-click-dragging it also closes any open dropdown instead of leaving it behind (Issue #351).

---

## [31.0.1] - 2026-06-07

> *A follow-up to the 1.2 release: settings for Extend's daisy-chain power, plus fixes for doubled auto-connect costs and the Smart! Panel arrow keys.*

### Added

- **Settings for Extend daisy-chain power** - The building-to-building power chaining added in 31.0.0 now has its own controls under Options > Mods > Smart! > Building Behavior:
  - **Extend Daisy-Chain Power** turns the feature on or off.
  - **Daisy-Chain Pole-less Factories** decides what happens when you Extend a machine that has no power pole and isn't already part of a chain: on, Extend starts a fresh daisy chain along the new row; off, it leaves power for you to run a pole. Either way, a machine that is *already* daisy-chained always keeps extending its existing chain, and a machine already wired to a pole is left alone.
  - Both default to on, so the behavior matches 31.0.0 until you change them.
  - A community-requested follow-up to the daisy-chain feature from 31.0.0, with thanks to EldritchHaiku and TheGandalf from the Smart! Discord for their feedback on the implementation (Issue #344).

### Fixed

- **Auto-connect conveyors and pipes no longer charge double** - When Smart! auto-connected a conveyor belt (for example to a splitter or merger) or a pipe (between pipe junctions), the cost shown on the build gun was twice what the segment actually cost to build - a belt that refunds 2 plates when dismantled was previewed as needing 4. The preview now matches the real build and dismantle cost. Thanks to ShinryuAspect for the detailed report (Issue #348).
- **Smart! Panel values no longer snap back when nudged with the arrow keys** - In the Smart! Panel, stepping a value (grid, spacing, steps, stagger, or rotation) with the Up/Down arrow keys would show the new value but then revert to the old one the moment you clicked another field or pressed Apply. Arrow-key changes now stick. Typing a value directly still works as before, and Left/Right still move the text cursor. Thanks to serjevski for the report (Issue #230).

---

## [31.0.0] - 2026-06-05

> *First Satisfactory 1.2 release - Smart! rebuilt for the 1.2 update.*
>
> **Updating straight from a 1.1 build (30.0.0)?** 31.0.0 also rolls up every fix from 30.1.0 - full localization for all 21 languages (including Arabic, Persian, and Thai), Extend affordability with Dimensional Depot support, in-game building-name corrections, Smart Panel text fitting, and smoother grid scaling - all listed in the 30.1.0 section below. Updating from 30.0.0 brings everything in one step.
>
> **One-time settings reset:** the Smart! settings menu was reorganized for this release, so your saved Smart! settings reset to their defaults the first time you load 31.0.0. Set them once and they'll stick from then on.

### Changed
- **Smart! now runs on Satisfactory 1.2** - Satisfactory 1.2 moves the game to a new version of Unreal Engine and a new way of packaging mods, which meant rebuilding Smart! against the updated game from the ground up. Every Smart! feature carries over unchanged - grid building, Extend and Scaled Extend, auto-connect for belts, pipes, and power, Smart Upgrade and Triage, Restore presets, the Smart Panel, and all keybinds - with no intended gameplay difference from the 1.1 release. This build targets Satisfactory 1.2 and newer; the mod manager will not offer it to 1.1 players, who should remain on version 30.1.0.
- **Settings menu reorganized into sections** - The Smart! page under Options > Mods is now grouped into six labeled sections - Belt Auto-Connect, Pipe Auto-Connect, Power Auto-Connect, Building Behavior, HUD, and Arrows - instead of one long undivided list, so related options are easier to find. Several settings that had stopped showing up are also back and editable again: belt and pipe routing modes, stackable-belt auto-connect, the Extend toggles, Auto-Hold on grid change, Apply Immediately, HUD position and theme, and the arrow orbit and label options. (Reorganizing the menu is what triggers the one-time settings reset noted above.)
- **Every Smart! setting now has a tooltip** - Hovering an option, or a section header, on the Smart! page under Options > Mods now shows a short plain-language description of what it does and when to use it.
- **Hold (H) now pins a single Extend** - When Extending a single building, press the Hold key (H) to freeze the Extend preview in place, then move the camera and look around to check the destination has clearance before you build - press H again to release. Previously a single Extend's preview was tied to looking at the source building, so you couldn't look away to inspect; Scaled Extend already held this way after a scale change, and now a plain single Extend does too.
- **Extend can daisy-chain building power (once unlocked)** - After you research Upgraded Power Connectors, Extending a row of buildings now wires their power directly building-to-building along the lane (one independent chain per row) instead of relying on a power pole to each one. It only makes connections you could make by hand, and leaves each chain's ends free so the line can still be extended later. An on/off setting and a build-time preview for these cables are still to come.

### Fixed
- **Fixed a crash during long building sessions with Scaled Extend** - After building with Scaled Extend for a while, the game could crash when the build gun cleaned up its preview holograms - it tried to tidy up holograms the game had already destroyed. The cleanup now safely skips anything that's already gone, so the crash no longer happens.
- **Stackable conveyor pole belt auto-connect now works reliably** - Auto-connecting belts between stackable conveyor poles - including across stacked levels and along long runs - now produces connected belts that carry items and stay correct after saving and reloading. Previously these auto-connected belts could end up not carrying items at all; building them in the reverse/backward direction could crash the game; and reversed or multi-segment runs could connect to the wrong neighbour or leave gaps. Reversed and segmented runs now wire up correctly, and the open ends of a run stay snappable so you can keep extending by hand.
- **Known issue - a very long stacked-pole run may need one save+reload to start flowing** - On occasion, a long stacked-pole belt run built in a single session won't carry items along its full length until you save and reload once. Until the permanent fix lands, do a single save+reload after building a long run, and - importantly - avoid routing one-of-a-kind items (such as Mercer Spheres or Somersloops) across a freshly built long stacked run, since an interrupted run is best cleared with that save+reload first.
- **Extend direction can now always be toggled to an open side** - When Extending to build a manifold, scrolling to switch the extend direction would sometimes refuse to flip to the other side even when that side was completely clear - in bad cases you had to leave unreasonably large empty gaps just to let an Extend work. Extend now decides which directions are available from the manifold's actual belt and pipe connections instead of scanning the surrounding area for nearby buildings, so an open side is always selectable. A direction is only blocked when it would build back into the run you are already extending, or directly into another machine sitting in that exact spot.
- **Pipeline T-Junction is now fully supported** - Satisfactory 1.2's new 3-way Pipeline T-Junction now works everywhere Smart! handles pipes - it sizes correctly, and Extend, Scaled Extend, pipe auto-connect, and Restore presets all treat it the same as the existing 4-way Pipeline Junction. Previously Smart!'s pipe-manifold logic assumed the 4-way junction, so building a pipe manifold with a T-Junction could drop or mis-route a connection and junction-to-junction auto-connect would silently skip it; the manifold logic now reads each junction's actual connectors instead of assuming a fixed layout.
- **Removed a brief stutter when using primary fire** - A leftover development hook was scanning every nearby belt and pipe within 50 metres each time you pressed primary fire (left mouse), causing a small hitch and unnecessary log output. The hook has been removed.

### Technical
- **Rebuilt for Unreal Engine 5.6** - Smart!'s codebase was migrated to the engine version that ships with Satisfactory 1.2, covering updated game APIs, header layouts, and include rules.
- **Migrated to the Game Feature plugin model** - Satisfactory 1.2 turns mods into "Game Feature" plugins. Smart! now ships with the Game Feature data the 1.2 game needs to recognize and load it.
- **Settings menu rendering restored and reworked** - The 1.2 rebuild first left the settings menu blank (its root section was flagged hidden) and then lost a third of its entries when the configuration asset was reparented. The menu was repaired, every setting re-authored against the 1.1 source (labels, tooltips, translations, dropdowns, and sliders preserved), and the settings grouped into sections. Settings are still defined as a flat structure in code - a sectioned mirror feeds the menu and copies values back - so the rest of the mod reads them exactly as before.
- **Memory-safety hardening** - Some preview holograms and built actors tracked by the Extend system were held as references the garbage collector couldn't see, so they could be left pointing at objects the engine had already destroyed - the root cause of the Scaled Extend crash above. These are now tracked properly (cleared by the collector instead of left dangling) and validated before use. A full audit of this pattern across Smart! and its companion mods fixed the remaining cases and confirmed the rest were already safe.
- **Migrated raw object pointers to TObjectPtr** - Following UE5.6 guidance, raw object pointers held in reflected properties were migrated to the engine's tracked-pointer type.
- **Code cleanup** - Removed a redundant runtime registration of the Smart configuration, an unused duplicate field, and several unused wiring maps confirmed dead, plus other small fixes (warning levels, an unreachable auto-connect branch).
- **Logging and debug cleanup for release** - Routed several hundred always-on diagnostic log lines to verbose so a shipping build stays quiet, and removed a leftover debug spline-analyzer utility (and its primary-fire hook).
- **Documentation** - Added the 1.2 dev-environment upgrade and port runbook, a port-prep inventory, and exact dependency-version notes.
- **Conveyor belt/chain hardening & cleanup** - Reworked stackable-pole belt auto-connect to build connected, save-stable conveyor chains (fixing the crash, mis-wiring, and no-flow issues above), removed dead conveyor-belt code paths left from earlier auto-connect approaches, and documented belt, chain-actor, and placement behaviour in depth. The remaining long-run chain-coalesce work (the "save+reload" known issue) and a related review of the Smart Upgrade chain-rebuild path are tracked in the backlog.

---

## [30.1.0] - 2026-05-31

> *Final Satisfactory 1.1 release - collects all changes since v30.0.0.*

### Fixed
- **Extend previews now show affordability correctly, and honor the Dimensional Depot** - When Extending a factory with auto-connected belts, pipes, and lifts, running short on materials turned the factory red but left the attached belts and pipes cyan, making it look like part of the placement was affordable. Now the whole Extend preview - factory, belts, pipes, lifts, distributors, poles, and wires - turns red together when you genuinely can't afford it, and returns to cyan the moment it's buildable again. The affordability check also counts materials in your Dimensional Depot, so an Extend you can build from the Depot is no longer incorrectly shown as unaffordable.
- **Corrected the Rotation Mode control description** - The Rotation Mode entry under Options > Controls > Mods showed an inaccurate description; it now reads correctly.
- **All supported languages now render correctly, including Arabic, Persian, and Thai** - Smart's panels and HUD used a UI font that had no Arabic, Persian, or Thai glyphs, so those languages showed up as empty boxes. Smart now uses the game's own multi-script UI font, so every language Satisfactory supports displays properly. Arabic, Persian, and Thai - which had been turned off because of this - are enabled again, bringing Smart to 21 supported languages.
- **Building names now match Satisfactory's own terms** - In several languages, Smart translated in-game building names (conveyor belts, conveyor lifts, pipelines, power poles, wall outlets, power towers, and more) by their dictionary meaning instead of the game's official term, which read as confusing or wrong - something German, Spanish, and Polish players reported. These were corrected across every translated language to match Satisfactory's in-game terminology.
- **Smart Panel buttons and dropdowns fit in every language** - The Apply/Close/Reset buttons and the recipe and preset dropdowns now size and render correctly for longer translations, instead of clipping the text or spilling outside the panel.

### Improved
- **Smoother grid scaling** - Reworked how Smart! spawns and refreshes grid preview holograms while scaling, improving stability and performance on large grid layouts.

### Technical
- **Fenced to Satisfactory 1.1** - This is the final 1.1-compatible release. It is marked compatible only with game versions up to and including the current 1.1 stable (CL 463028); when 1.2 arrives, the mod manager will not offer this build to 1.2, so 1.1 players keep a working version and 1.2 players are not handed an incompatible one. A separate 1.2 release will follow.
- **Pre-1.2 cleanup pass** - Removed a large amount of dead and orphaned code ahead of the Satisfactory 1.2 port: unused services (axis-label provider, direction-translation service), legacy input-action definitions, the obsolete pipe chain resolver, stale backup files, empty per-building hologram subclasses, unused hologram adapters, dead scaling/arrow/conveyor helper modules, and a bypassed upgrade result-row widget class; trimmed the core subsystem and upgrade service (~5,000+ lines removed across ~80 files). No intended gameplay change. (Verified against editor content via editor introspection before removal.)
- **God-object decomposition** - Split the largest source files - the central subsystem and the Extend engine, each over 9,000 lines - into focused, single-responsibility files, with the code behaving exactly as before. This makes future updates, especially the upcoming Satisfactory 1.2 port, faster and lower-risk. Build-validated; no intended gameplay change.
- **Extend child-state propagation refactor** - Child preview holograms now read an authoritative material state from the Extend service rather than each inferring it from the parent, resolving the frame-ordering issue behind the insufficient-materials fix above.
- **Satisfactory 1.2 readiness** - Added the 1.2 porting plan, a multiplayer support planning matrix, and structured GitHub issue forms.
- **Documentation & project** - New GitHub wiki content, a player FAQ, Smart Camera control docs, and refreshed ficsit.app support metadata.
- **Multi-script UI font** - Smart UI text now routes through a single helper that applies the game's runtime composite multi-script font (Noto Arabic/Thai/CJK fallbacks with proper shaping) instead of the engine default font, which is offline-baked and cannot shape complex scripts. Designer-placed labels are restamped at construction, the header buttons are fit to their localized labels at runtime, and dropdown fonts are set on the widget asset.
- **Localization pipeline fixes** - Corrected stale localization config paths (left over from the repository migration) that were compiling translations into an unused folder; re-enabled Arabic/Persian/Thai across the gather, sync, and compile steps; and corrected building-term translations against the shipped game's string tables. Added localization audit/validate/term-fix tooling and a developer doc covering the editor-introspection (live-editor Python) techniques used.

---

## [30.0.0] - 2026-05-03

### Added
- **Smart! Restore Enhanced** - Smart! now has a preset system for saving, applying, sharing, and replaying Smart Panel setups. A preset can store grid size, spacing, steps, stagger, rotation, production recipe, auto-connect settings, and restored Extend topology.
- **Restore panel in the Smart Panel** - Open the Smart Panel and use the `Presets >>` button at the top, above the grid controls, to open the Smart Restore panel. The panel includes a selected preset dropdown, a new preset name field, an editable description, a read-only created timestamp, capture checkboxes, and Save Current/Apply/Update/Delete/Export to Clipboard/Import from Clipboard actions. Choosing a preset also fills its captured grid, spacing, steps, stagger, and rotation into the main Smart Panel fields so you can review or tweak them before applying.
- **Capture options** - Before saving or updating a preset, choose which parts of the current setup should be captured: Grid, Spacing, Steps, Stagger, Rotation, Recipe, and Auto-Connect. This lets you save a full factory setup or a smaller reusable adjustment.
- **Restore preset metadata** - Presets now save a description and creation timestamp. Newly saved and imported presets are selected in the dropdown immediately, so you can review, tweak, or apply the preset you just created.
- **Clipboard sharing** - Presets can be exported to a compact clipboard string and imported on another save or by another player. Imported presets get a fresh local creation timestamp while keeping the shared name, description, and setup.
- **Progression-safe imports and applies** - Shared presets are checked against your current unlocks before they can be imported or applied. If a preset needs a locked building, production recipe, belt tier, pipe tier, power component, lift, splitter, merger, or other Extend component, Smart! rejects it instead of creating an illegal preview.
- **Import from Last Extend** - After building with Extend, Smart Restore can capture the last Extend layout as an editable preset draft. This includes the source building, production recipe when available, factories, belts, conveyor lifts, pipes, distributors, power poles, and the cloned connection layout.
- **Restored Extend topology replay** - Applying a preset captured from Extend replays the saved topology as the active build preview. The restored layout can be scaled from the Smart Panel, and Smart! owns that topology while the Restore session is active so normal Smart grid children are not spawned on top of it.
- **Restore HUD indicator** - When a restored Extend topology is active, the Smart HUD shows the active Restore preset name so you can tell that the preview is being driven by Smart Restore.

### Changed
- **Quieter Restore and Extend diagnostics** - Restore and Extend troubleshooting messages now stay behind verbose logging so normal gameplay logs remain readable. Smart! still logs its normal service initialization lines.

### How to Use Smart Restore Enhanced
- **Save a normal Smart Panel preset** - Equip the build gun, set up Smart! the way you want, open the Smart Panel, press `Presets >>`, enter a new preset name and optional description, choose the capture checkboxes, then press Save Current.
- **Apply a saved preset** - Open `Presets >>` and choose a preset from Selected Preset. Smart! fills the captured grid, spacing, steps, stagger, and rotation into the main Smart Panel fields so you can review them or make small changes. Press Apply in the Restore panel to switch the build gun to the saved building when possible, restore the captured Smart Panel values, restore the saved production recipe when it still applies, and reapply saved auto-connect settings.
- **Update an existing preset** - Select a preset, adjust your current Smart Panel setup, edit the description if needed, choose the capture checkboxes, then press Update. The original created timestamp is preserved.
- **Share a preset** - Select a preset and press Export to Clipboard to copy the preset string to your clipboard. Send that string to another player. To receive one, copy the shared string, open `Presets >>`, and press Import from Clipboard. If every required building, recipe, and logistics part is unlocked, the preset is saved and selected automatically.
- **Create a preset from Extend** - Use Extend to create the layout you want, then open the Smart Panel, enter a preset name or let Smart! generate one, and press Import from Last Extend. Smart! stages that Extend layout as a 1x1 editable Restore preview. Adjust grid, spacing, steps, stagger, rotation, recipe, or auto-connect settings as desired, then press Save Current to keep it as a reusable preset.
- **Cancel or leave a restored Extend preview** - Canceling the build gun, switching to a different build action, or starting a normal Extend action clears the active Restore session so the saved topology is not reused unexpectedly.

---

## [29.2.5] - 2026-04-27

### Changed
- **First release in the new public repository** - Smart! is now published as source-available code on GitHub at https://github.com/majormer/SmartFoundations. The source is available for community transparency, code review, and pull request contributions. See LICENSE.md for the full source-available license terms.

### Fixed
- **A massive amount of logging spam removed** - Smart! was generating over 500,000 log entries during normal gameplay from per-frame input diagnostics, hologram construction events, recipe service activity, and child hologram spawning during grid placement. These high-frequency logs have been moved to verbose logging, eliminating the log flood while keeping diagnostics available for troubleshooting.
- **Extend no longer runs post-build wiring during world load** - Loading a save could make Smart! run Extend's post-build wiring checks against ordinary factory buildings before you had used Extend at all. This produced confusing Extend chain-fix messages during login and did unnecessary work on restored buildings. Extend post-build wiring now only runs when there is actual pending Extend build state to process.
- **Scaled Extend pump wiring now follows the cloned topology** - When extending refinery or pump setups with multiple cloned groups, cloned pumps now connect to the power pole cloned with their own group instead of all later pumps incorrectly wiring back to the first cloned pole.

---

## [29.2.4] - 2026-04-26

### Fixed
- **Smart Upgrade — large belt and lift upgrades no longer corrupt conveyor flow** - Mass upgrades on dense conveyor networks are now rebuilt from the completed live graph instead of trying to stitch the game's conveyor tracking back together by hand. This fixes the worst failure pattern from recent releases: belts or lifts that looked connected after an upgrade, but stopped moving items after a save/reload or left broken chain state behind. The validated test path covered a clean pre-upgrade save, full mass upgrade, immediate item flow, clean diagnostics, save, reload, preserved item flow, and clean post-load diagnostics. (Issue #303)
- **Smart Upgrade — belts and lifts upgrade as connected conveyor runs** - Radius and network upgrades now treat connected belts and lifts as one conveyor domain for selection and cost handling. This avoids partial belt/lift upgrades that could leave one piece of a run upgraded while the rest of the same conveyor chain stayed on the old tier.
- **Smart Upgrade Triage — repaired previously broken saves that can still load** - The Triage panel's Repair action can now clean up loaded saves with broken conveyor chain state by removing dead chain actors, rebuilding split chains, and re-registering live conveyors from orphaned conveyor groups. In validation, a previously broken save with dozens of split-chain issues and mismatched belt ownership repaired back to healthy conveyor diagnostics. Save before using Repair, then save, reload, and run Detect again. If you come across a condition where triage crashes your game, please report on the Smart Discord server your crash report, please, in the #help channel so I can capture it.
- **Smart Upgrade — reduced crash risk after massive conveyor upgrades** - Smart! no longer runs the unsafe automatic split/orphan repair paths immediately after an upgrade while the game is still settling conveyor state. Automatic cleanup is limited to safe zombie-chain cleanup; deeper repair is now an explicit Triage action after the save has loaded.

### Changed
- **Smart Upgrade UI — clearer conveyor upgrade grouping** - The Upgrade panel now presents belts and lifts together as Conveyors where that better matches how Satisfactory actually simulates connected conveyor runs, while still charging the correct cost for each upgraded piece.
- **Smart Upgrade Triage wording and localization updated** - Triage text now explains the repair flow more clearly in all 18 active supported languages.
- **Smart! ficsit.app page refreshed** - The public mod page has been updated with a better onboarding experience for new players and clearer current-release guidance.
- **Internal code documentation cleaned up** - Internal Smart Upgrade and Chain Actor documentation was updated and reorganized so future fixes do not repeat the failed recovery paths from this investigation.

---

## [29.2.3] - 2026-04-20

> *A note on changelog quality: recent changelog entries got too deep into implementation details that mean nothing to players. Starting here, I'm keeping it focused on what you actually experience. I'll do better going forward.*

### Fixed
- **Smart Upgrade — reduced crashes and stalled belts on large networks** - Mass upgrading belts on large, dense factory networks could cause crashes, stalled conveyors, or disconnected belt runs — especially on networks with 200+ belts or complex splitter/merger layouts. The upgrade now correctly rebuilds belt chains in item-flow order so that long multi-belt runs stay together as a single chain rather than fracturing into disconnected segments.
- **Smart Upgrade — items no longer vanish or get stranded mid-belt** - A subtle issue in how belt groups were merged internally could corrupt the item-bucket tracking that Satisfactory uses to know where items are on a belt. This caused items in flight during a mass upgrade to end up in the wrong position or disappear entirely. Fixed.
- **Smart Upgrade — improved self-correction after upgrade** - A small number of belt chain actors (~40 per 1,000 belts upgraded) could be left in a broken zero-segment state by the game's own internal rebuild timing. Smart! now automatically cleans these up 3 seconds after the upgrade completes, before you can accidentally save them in a broken state. On a 1,055-belt upgrade, all 4,927 resulting chain actors come out healthy.

---

## [29.2.2] - 2026-04-20

### Fixed
- **Mass Upgrade & Extend — In-Game Chain Actor Corruption on Dense Belt Networks** - Fixed in-game `SPLIT_CHAIN` corruption that occurred immediately after a mass belt upgrade or Scaled-Extend manifold build on dense networks — no save or reload required. After v29.2.1's teardown, `FConveyorTickGroup::ChainActor` was `nullptr` for one frame while the engine queued a next-frame rebuild. During that window the old chain actors were GC-pending but still referenced by belts' `mConveyorChainActor` back-pointers. On dense networks (200+ belts, multiple overlapping chains), a second `Factory_Tick` pass could observe these stale pointers before migration completed, producing chain actors whose segment lists claimed belts already assigned to different chains — corrupting belt item-bucket state in-game. A save captured in that same window would make the corruption persistent (chains deserialise on reload and race fresh migration), so save/reload was often where the problem first became apparent, but it was an in-game integrity failure, not a save-format issue. Root-caused via extensive live-game testing using an in-game `repair_conveyor_chains` diagnostic, which proved that the correct teardown is not `RemoveConveyorChainActor` + wait-a-frame but a **three-phase synchronous rebuild**: (1) resolve each affected chain to its owning `FConveyorTickGroup`; (2) call `RemoveChainActorFromConveyorGroup(TG)`, which nulls `TG->ChainActor` and every belt's `mConveyorChainActor` without `Destroy()` (the old actor becomes inert and GCs naturally, avoiding the ParallelFor race that bit 29.2.0); (3) union the cleared groups with `mConveyorGroupsPendingChainActors`, empty the pending list, then call `MigrateConveyorGroupToChainActor(TG)` synchronously for every unique group before returning. After step (3) every tick group has a valid fresh chain actor; no subsequent pass can observe a null or stale `ChainActor`. Dedup of tick groups is mandatory — calling either private API twice on the same group corrupts state. The three-phase logic lives in a new reusable service (below) so Mass Upgrade, Extend, and any future topology-changing feature all route through one audited path. Credited to elsheppo (original report) and in-game live testing. (Issue #303)

### Added
- **`USFChainActorService` — Canonical Chain Invalidation + Rebuild Path** - New subsystem-owned service (`Services/SFChainActorService.h`) that owns the Remove-then-Migrate-synchronously pattern described above. Exposes two entry points: `InvalidateAndRebuildChains(const TSet<AFGConveyorChainActor*>&)` and `InvalidateAndRebuildForBelts(const TArray<AFGBuildableConveyorBase*>&, const TSet<AFGConveyorChainActor*>& ExtraChains)`. Both tolerate null and already-destroyed chain entries, dedupe tick groups internally, fall back gracefully when `AFGBuildableSubsystem` is unavailable, and log a single summary line per call with counts for `chains_supplied`, `groups_cleared`, `groups_migrated`, and `orphan_skipped`. `Mass Upgrade`'s `SFUpgradeExecutionService::CompleteUpgrade` and `Extend`'s `FSFWiringManifest::CreateChainActors` both now delegate chain teardown to this service; the embedded "HISTORY — do not regress" comment that lived in `CompleteUpgrade` since 29.2.1 has moved into the service header alongside the full crash postmortem (chain->Destroy race, belt->SetConveyorChainActor(nullptr) ghost-actor race, RemoveConveyor/AddConveyor bucket-corruption race, and the new save-timing window closure). The service is granted friend access to `AFGBuildableSubsystem` via `Config/AccessTransformers.ini` for the private `RemoveChainActorFromConveyorGroup`, `MigrateConveyorGroupToChainActor`, `mConveyorTickGroup`, and `mConveyorGroupsPendingChainActors` symbols.

### Technical
- **`Config/AccessTransformers.ini`** - Added a single `Friend` entry granting `USFChainActorService` full access to `AFGBuildableSubsystem`. No new `Accessor` entries are required — friendship reaches every private field and method the service needs.
- **Call-site simplification.** `SFUpgradeExecutionService::CompleteUpgrade` (~line 1365) and `FSFWiringManifest::CreateChainActors` (~line 874) are each ~25-30 lines shorter; both now contain at most one chain-collection pass followed by a single call to the service. The belt-and-neighbour chain walk has been moved into `USFChainActorService::InvalidateAndRebuildForBelts` so it is no longer duplicated.
- **Repair-button groundwork.** With the shared service in place, a future UI entry point that runs the same repair over arbitrary player-indicated chains (mirroring an in-game `repair_conveyor_chains` diagnostic for non-developer users) is a thin Blueprint wrapper; deferred to a later release.

---

## [29.2.1] - 2026-04-19

### Fixed
- **Mass Upgrade — Chain Actor Crashes (In-Game and Save/Reload)** - Reverted both "defense-in-depth" mitigations added to `SFUpgradeExecutionService::CompleteUpgrade` in 29.2.0. Each one caused a crash that was worse than the rare Issue #303 window they were attempting to close. Smart! now performs a single `BuildableSubsystem->RemoveConveyorChainActor(Chain)` per affected chain and stops — matching exactly what Archengius (Coffee Stain Studios) described: "If you call RemoveChainActor on a chain it will remove the chain and automatically build a new one next frame." Two crash paths eliminated:
  - **(B) In-game ParallelFor race crash.** 29.2.0 called `Chain->Destroy()` on the game thread immediately after `RemoveConveyorChainActor`. `AFGBuildableSubsystem::TickFactoryActors` ticks chain actors via `ParallelFor`, so a worker thread could still be executing `AFGConveyorChainActor::Factory_Tick` while the game thread freed the chain's internal `TArray<AFGBuildable*>`. The worker then dereferenced freed memory inside `TArray::Remove`, producing `EXCEPTION_ACCESS_VIOLATION` at `FGConveyorChainActor.cpp:339`. Reproducible with a mass-upgrade of ~200+ belts in a dense network; reported by elsheppo.
  - **(A) Save/reload assertion crash.** 29.2.0 walked each affected chain's `GetChainSegments()` list and called `Belt->SetConveyorChainActor(nullptr)` on every member belt before `RemoveConveyorChainActor`. This bypassed vanilla's "belt dismantled → chain receives removal notification → chain self-deletes" flow: by the time `RemoveConveyorChainActor` ran, the chain had no belts left to notify, so it was removed from its tick group and a fresh replacement was built — but the original chain actor was never destroyed. It lingered in the world as an orphan (no references, not ticked, but still present as a persistent `AActor`). A subsequent save captured both the healthy rebuilt chains **and** these ghost actors. On load, the ghosts deserialised, detected invalid state on their first `Factory_Tick`, and called `ForceDestroyChainActor` **from within the parallel tick** — `UnregisterAllComponents` on the ghost then tripped the `GTestRegisterComponentTickFunctions == 0` assertion at `ActorComponent.cpp:1199`, because tick functions cannot be unregistered while the parallel tick pass is still running. Reproducible by mass-upgrading belts, waiting ~2 minutes, saving, and reloading.
  - **Root cause of both:** the 29.2.0 mitigations attempted to harden the chain teardown beyond what the vanilla API contract expects. `RemoveConveyorChainActor` already handles the tick-group removal, the chain destruction, and the next-frame rebuild atomically from the caller's perspective. Any extra work either races the parallel tick (force-destroy) or breaks the notification chain that destroys the original chain (pre-nulling belt refs). The 29.2.1 teardown is one line per chain. A regression note recording the full postmortem for both paths was added to repository memory so neither mitigation is reintroduced. (Issue #303)

### Technical
- **`SFUpgradeExecutionService::CompleteUpgrade` simplification.** Removed the pre-null `SetConveyorChainActor(nullptr)` pass over every belt in every affected chain's segment list, and removed the post-removal `Chain->Destroy()` loop. `PreDestroyChainActors` capture (used to track chains referenced by belts that were about to be destroyed during the upgrade itself) is retained — that set still gets unioned with the affected-chain set to ensure no chain referencing an already-destroyed belt is missed. The log line for each batch now reports conveyors upgraded and chains invalidated only; the defense-in-depth counters are gone. `PreDestroyChainActors.Empty()` at the end of the routine is unchanged.

---

## [29.2.0] - 2026-04-19

### Added
- **Extend — Valve and Pump Cloning** - Extend now traverses inline pipe attachments — valves (`Build_Valve_C`), Pipeline Pump Mk.1 (`Build_PipelinePump_C`), and Pipeline Pump Mk.2 (`Build_PipelinePumpMk2_C`), all of which share `AFGBuildablePipelinePump` as their C++ base — when walking pipe chains, so a manifold with a valve or pump mid-pipe is no longer truncated at the attachment. The walker mirrors the floor-passthrough pattern: record the attachment in a new `PipeAttachments` field on the pipe chain node, find its other pipe connector, continue walking to the next pipe or junction. Each attachment is emitted in the clone topology JSON with `role="pipe_attachment"` carrying the source's `mUserFlowLimit` (-1 = unlimited / valve fully open, finite values for user-throttled pumps/valves). A new `ASFPipeAttachmentChildHologram` (derives from `AFGPipelineAttachmentHologram`, the C++ parent of both the bare valve hologram and `Holo_PipelinePump_C`) short-circuits placement validation and, after vanilla Construct spawns the buildable, casts it to `AFGBuildablePipelinePump` and calls `SetUserFlowLimit` so the cloned attachment opens to the same flow rate the source was set to. Post-build wiring runs two passes: **Phase 3.8a** wires each built pipe-attachment clone's two pipe connectors to the nearest unconnected connector on any cloned pipe within 25 cm, correctly inserting the valve/pump between adjacent cloned pipes instead of wiring the pipes straight through; **Phase 3.8b** handles pump power. For each cloned pump whose source was powered by a pole inside the manifold, the capture pass records the source pole's actor id on the pipe-attachment segment, the emit pass resolves it to the clone pole's HologramId via `SourceIdToHologramId`, and the post-build pass spawns a `Build_PowerLine_C` wire between the clone pump's `PowerInput` and the clone pole's first `UFGCircuitConnectionComponent` using the same `AFGBuildableWire::Connect(conn, conn)` pattern Power Extend uses for factory↔pole wiring. Pumps whose source pole was outside the manifold, valves, and unpowered pumps all route through the same code with an empty `ConnectedPowerPoleHologramId`, which cleanly disables every power path for them. A new **preview-time capacity validator** (`ValidatePowerCapacity`, hooked into both the single-clone and Scaled Extend preview paths) sums `1 (clone factory) + 1 (inter-pole wire back to source) + N (cloned pumps attributed to this pole)` for each cloned power pole and compares to the source pole's tier cap (Mk.1=4, Mk.2=7, Mk.3=10 — applies identically to standard poles and wall outlets at each tier) mirrored through `FSFSourcePowerPole.MaxConnections`. The upstream tier detection in `SFExtendTopologyService` was also switched from string-matching the class name (which silently capped `Build_PowerPoleWallMk2_C` / `Build_PowerPoleWallMk3_C` at 4 because `"PowerPoleWallMk2"` does not contain `"PowerPoleMk2"`) to reading `UFGCircuitConnectionComponent::GetMaxNumConnections()` directly from the live pole — authoritative for every pole variant plus any future tiers or modded pole classes; if any cloned pole would overflow, `ScaledExtendInvalidReason` is set, the parent hologram tints red via `HMS_ERROR`, and the existing HUD line surfaces a message like `Extend: 1×1 [Clone PowerPoleMk1 needs 5/4 connections (factory + inter-pole + 3 pumps) — upgrade the source pole, or move a pump to another pole]`. The validator also means the pre-existing Power Extend (Issue #229) "full source pole" edge case now fails loudly at preview time instead of silently producing an over-max grid. Reported by Torkeug. (Issue #288)
- **Extend — Wall Hole Cloning** - Extend now captures and clones wall-mounted belt and pipe passthroughs (`Build_ConveyorWallHole_C`, `Build_PipelineSupportWallHole_C`) that sit on chain connectors within the source manifold. A new spatial discovery pass walks each captured chain/pipe connector, looks for a wall-hole buildable within a short radius whose class name matches one of the known wall-hole suffixes, and attaches ownership to the source topology regardless of which chain's connector "owned" it (no more bounding-box false positives from neighbouring manifolds). Each discovered wall hole emits a `role="wall_hole"` entry in the clone topology JSON with its build class, transform, and the same snapped-thickness hint passed to floor passthroughs. A new `ASFWallHoleChildHologram` (derives from `AFGWallAttachmentHologram`) short-circuits placement validation — the clone topology guarantees a correct transform, and the vanilla wall-hit test has nothing to consult — then delegates `Construct` to the base so the vanilla wall-attachment path consumes the target wall's snap points and registers the buildable with `AFGBuildableSubsystem`. Verified in-game: cloned manifolds now carry matching wall holes beside the cloned belts/pipes at each wall crossing, and snap points on the target wall are consumed correctly so a second hole can't be placed at the same slot. (Issue #287 — Phase 1 and Phase 2)
- **Extend — Floor Hole Passthrough Deduplication** - Fixed a bug where Scaled Extend (2×, 3×, …) would duplicate floor hole passthroughs that sat on the manifold boundary. The spatial-discovery pass and the chain-walker pass were both crediting the same passthrough to the source manifold without cross-checking — the chain-walker found it via `GetSnappedBuildable` on a chain connector, and the spatial pass found it via bounding-box overlap when a neighbouring manifold's passthrough happened to fall inside the source's bounds. Discovery now verifies that at least one of the passthrough's `mSnappedConnectionComponents` points at a buildable that is actually a member of the current chain; unrelated passthroughs are skipped. This also removes a class of false positives seen when Extend is run with a tight radius near other pipe networks. Reported by Torkeug. (Issue #283)

### Fixed
- **Mass Upgrade — Post-Load Zombie Chain Actor (Preventive)** - Landed a defense-in-depth pair of mitigations in `SFUpgradeExecutionService::CompleteUpgrade` targeted at a rarely-reproduced crash on save/load after mass-upgrading conveyor belts. `AFGBuildableConveyorBase::mConveyorChainActor` is `UPROPERTY(SaveGame, ReplicatedUsing=OnRep_ChainInformation)`, so if a save fires in the one-frame window between Smart's chain-level teardown and vanilla's next-frame rebuild, that pointer can be serialised while referencing an about-to-be-destroyed chain — on load the zombie chain resurrects, hits `Factory_Tick`, detects bad state, calls `ForceDestroyChainActor`, and the destroy trips the `FNetworkObjectList::Remove` invariant. Two additions, both idempotent with Archengius's recommended chain-level teardown: (A) before removal, walk every affected chain's `GetChainSegments()` list and null each belt's `mConveyorChainActor` via the public `SetConveyorChainActor(nullptr)` — this covers belts beyond just the upgraded set or direct neighbours, e.g. a chain A-B-C-D-E where only B+C were upgraded still has A/D/E holding stale refs via the same chain actor; (B) after `RemoveConveyorChainActor`, explicitly call `Chain->Destroy()` so any chain actor still `IsValid` is removed from the world immediately rather than next frame. Both operations layer on top of the existing `RemoveConveyorChainActor` call — if vanilla already nulled the refs or destroyed the actor inline, our follow-ups no-op. Unable to reproduce locally; the issue is shelved on GitHub pending a pre-upgrade save file or minimal repro steps. (Issue #303)

---

## [29.1.3] - 2026-04-18

### Fixed
- **Stackable Conveyor Pole Auto-Connect Angles** - Belts and pipes routed between stackable or ceiling poles no longer show a visible bulge/ramp at the pole or an S-curve between poles. Both paths previously synthesized the spline exit direction from the pole's forward vector, with a sign flip only when the direction was outright reversed. A small Z delta between poles (uneven ground, scaled parent) or a pole rotation perpendicular to the distribution axis slipped through the check and produced oddly-angled belts that also made subsequent manual belt snapping land in unexpected positions. The routing now uses the true 3D vector between the two pole endpoints for stackable/ceiling poles (pitch-correct and yaw-correct), while wall conveyor poles retain their deliberate perpendicular exit behavior from 29.1.1. (Issue #291)
- **Smart Upgrade — Pipeline "No Indicator" Variants** - Batch upgrade now correctly recognizes and upgrades pipelines that lack the "Mk" token in their class name ("No Indicator" variants). The family detector previously used a string match on `PipelineMk` which excluded No Indicator pipes entirely, and the tier detector fell back to tier 1 for anything without an explicit `MkN` token — misclassifying MK2 No Indicator as tier 1. Family detection now uses `Cast<AFGBuildablePipeline>` (covers every pipeline subclass), tier detection has an explicit MK2 vs base branch for pipes, and gather-targets enumerates every `AFGBuildablePipeline` actor at the source tier so both indicator and No Indicator variants at that tier are picked up. The upgrade also preserves the source pipe's indicator style, so a Mk1 No Indicator upgrades to Mk2 No Indicator (not the standard indicator variant). Reported by solkol (Discord #help). (Issues #295, #296)
- **Smart Upgrade — Post-Upgrade Connection Integrity** - Added a validation pass that captures the expected connection manifest (factory, pipe, and power edges) before upgrade and verifies every edge is still present on the replacement buildables afterwards, repairing any missing connections and surfacing a count if repairs were needed. Batch-level pipe-to-pipe connection preservation was also extended so pipe junctions and mid-network pipe segments keep their links across large upgrades. Reported by tevionfox, solkol, and salvsum across Discord #help between 2026-03-08 and 2026-03-29. (Issue #300)
- **Extend — Distributor Configuration Preservation** - Extend now copies user-configured state from every source distributor to its clone during post-build wiring. Previously, cloning a constructor with a configured input splitter or priority merger produced a clone with class-default settings, forcing players to re-enter every filter rule or input priority by hand. Three distributor families are now covered: **Smart Splitter and Programmable Splitter** (both share `AFGBuildableSplitterSmart` as their C++ base and the same `mSortRules` array, including their Lift variants) have their sort rules transferred via `GetSortRules()`/`SetSortRules()`; **Priority Merger** has its per-input priority array transferred via `GetInputPriorities()`/`SetInputPriorities()` with a defensive input-count check. Both `SetSortRules` and `SetInputPriorities` broadcast their respective change delegates and replicate through `ReplicatedUsing` properties, so multiplayer clients see the copied state on the clone immediately. Vanilla 3-way splitters and plain mergers are skipped cleanly (neither cast matches). Runtime state (`mItemToLastOutputMap`, `mCurrentOutputIndex`, `mCurrentInputIndices`, `mCurrentInputPriorityGroupIndex`, etc.) is intentionally **not** copied — only user-configured state is transferred so items flow fresh through the clone. Reported by thomas8310 via ADA Discord bot in #ada-assistance-preview on 2026-04-18; Priority Merger coverage filed during code review. (Issues #298, #299, #301)
- **Smart Upgrade — Wall Outlets** - Mass upgrade now correctly handles wall power outlets end-to-end. Two issues were resolved: (1) Network-tab traversal upgrades silently dropped every Mk.1 wall outlet because the traversal tier detector returned 0 for class names without a `MkN` token — `Build_PowerPoleWall_C` and `Build_PowerPoleWallDouble_C` both fall in that category. Added an explicit `AFGBuildablePowerPole` branch mirroring the Pipeline fix from this release, defaulting unsuffixed names to tier 1 and matching the audit service's behavior. (2) The unlock probe `GetHighestUnlockedWallOutletTier` only checked single-sided outlet classes but was used to cap the maximum target tier for **both** single and double families, under-reporting availability when the two families unlock at different milestones. The probe now takes a `bDouble` parameter and the audit service, traversal service, and Upgrade Panel each pass the correct flag for the family they are querying. Radius-tab upgrades were unaffected by either issue because they use exact source-class matching, so single/double outlets already appeared in the panel in 29.0.1 (the inability to actually execute the upgrade in some configurations is what's fixed here). (Issue #267)

### Technical
- **Build Hygiene** - Added missing `#include "SmartFoundations.h"` to `SFHologramPerformanceProfiler.cpp`. The TU used `UE_LOG(LogSmartFoundations, ...)` without directly including the header that declares the log category — a latent issue that unity builds had been hiding via transitive includes and which the adaptive non-unity build exposed.

---

## [29.1.2] - 2026-03-14

### Fixed
- **Auto-Hold Ignores Negative Scaling Axis** - Auto-Hold on Grid Change now correctly engages when any axis is scaled to a negative value (e.g., -2×1×1). The grid expansion check now uses absolute values so negative scaling triggers auto-hold the same as positive scaling. (Issue #282)

---

## [29.1.1] - 2026-03-13

### Added
- **Wall & Ceiling Conveyor Mount Auto-Connect** - Wall conveyor poles and conveyor ceiling supports now expose the expected auto-connect HUD/config behavior and correctly respect Auto-Hold on Grid Change, bringing them to feature parity with stackable conveyor poles. (Issue #268)
- **Scalable Signs & Billboards** - All 10 standalone signs and billboards now support grid scaling: Label Signs (2m/3m/4m), Square Signs (0.5m/1m/2m), Display Sign, Portrait Sign, Small Billboard, and Large Billboard. Wall-mounted signs scale cleanly. Floor/ceiling signs scale but children place without support poles/stands — a known limitation due to vanilla's internal hologram state management for sign pole creation. (Issue #192)
- **Context-Aware Keybind Hints in Build Gun** - Smart! keybind hints now appear in the vanilla build gun hint bar, pulled live from the Input Mapping Context so user rebinds are reflected. 10 hints for scalable buildings (Scale X/Y/Z, Spacing, Steps, Stagger, Rotation, Cycle Mode, Recipe, Smart Panel), with context-aware filtering: upgrade-capable items (belts, lifts, pipes, wires) show only "Upgrade Panel"; Scaled Extend hides Scale Z and Stagger. Enough hints trigger a second row automatically. (Issue #281)
- **Monochrome HUD Theme** - Added a sixth HUD theme option: **Monochrome**. This theme renders the HUD with a solid black background, white border, white body/header text, and white X/Y/Z grid values for a fully monochrome presentation. Config tooltip text updated across localization files to document the new option. Requested by Shaded. (Issue #179)

### Changed
- **Russian Localization Update** - Updated pipe auto-connect terminology in Russian translation based on community feedback from Serj. Changed "пересечение" (intersection/crossing) to "соединение" (connection/junction) for pipe junction-related config tooltips, providing more accurate terminology for Russian-speaking players
- **Localization — HUD & Smart! Term Standardization** - Enforced that "HUD" and "Smart!" remain as English terms across all 18 languages. Arabic translations that previously used a generic "interface" equivalent for HUD config keys were corrected to use "HUD" literally. Filled missing HUD-related translations in Vietnamese, Ukrainian, Thai, and Norwegian that previously fell back to English or were empty. All localization seed files (translation JSONs and Python scripts) updated alongside generated .po files to ensure persistence across regeneration

### Fixed
- **Power Shard & Somersloop Recipe Copying** - Power Shards and Somersloops are now correctly copied to all scaled building instances when sampling a production building. The system uses a "build session ID" to ensure shards/somersloops are only applied to clones within the same build session and for the same building type. This prevents accidental application to unrelated builds or different building types. When you middle-click a constructor with shards installed and then scale it 2×1, both copies receive the shards. If you then switch to an assembler, the assembler won't inherit the constructor's shards. Session IDs are synced at shard capture time and validated before application in `OnActorSpawned`. (Issues #209, #208)
- **Scaled Extend with Non-Manifold Chains** - Scaled Extend (2+ copies) no longer fails with "Belt lane angle too steep" when factory chains terminate at belts/lifts/pipes instead of a physical splitter/merger/junction. Chains without a physical distributor are now excluded from Extend topology. Belt chains must terminate at a splitter, merger, smart splitter, programmable splitter, or priority merger. Pipe chains must terminate at a pipeline junction. Previously, a virtual distributor placeholder with zeroed rotation caused connector normals to be perpendicular to the extend direction for any rotated factory. **Workaround:** Add a placeholder splitter/merger/junction at the end of each chain until v29.1.1 releases. Reported by thesprout (Issue #277)
- **RMB Hold Broken After Modifier Keys** - Right mouse button (when rebound to Hold) no longer opens the build menu after using any modifier key (X+scroll, Z+scroll, Semicolon, I, Y, Comma, U). Root cause: Smart! was removing and re-adding the vanilla `MC_BuildGunBuild` mapping context on every modifier press/release to prevent scroll wheel rotation leaking to vanilla. The re-add changed the context's priority in the Enhanced Input stack, causing RMB to route to the build menu action instead of the Hold action. Fix: removed all vanilla context manipulation entirely — Smart!'s higher-priority mapping context (priority 100) already handles input routing correctly. Reported by Alex (Issue #272)
- **Auto-Hold Survives Modifier Release** - Auto-Hold on Grid Change now correctly keeps the hologram locked after releasing a modifier key (e.g., Shift+Scroll to scale). Previously, the modifier's lock/unlock cycle would override auto-hold because auto-hold only claimed ownership when the hologram was unlocked — but the modifier had already locked it. Auto-hold now claims ownership whenever the grid expands, regardless of current lock state. Reported by Gabor (Issue #282)
- **Auto-Hold No Longer Drops at 1×1×1 While Modifier Held** - Scaling back down to 1×1×1 while still holding a modifier key no longer unlocks the hologram. The auto-hold release path now checks if any modal feature (modifier key, spacing, steps, etc.) is still active before disengaging. (Issue #282)

---

## [29.1.0] - 2026-02-26

### Added
- **Localization — 18 Languages** - For the first time, Smart! is fully localized. Previously English-only, all user-facing strings in the Smart Panel, Upgrade Panel, HUD overlay, config menu, and directional arrows are now translated into 17 non-English languages: German, Spanish, French, Italian, Japanese, Korean, Polish, Portuguese (Brazil), Russian, Chinese (Simplified), Chinese (Traditional), Turkish, Bulgarian, Hungarian, Norwegian, Ukrainian, and Vietnamese. Untranslated keys (universal symbols and tooltips) fall back to English automatically (Issue #276)
- **Water Extractor Scaling** - Water extractors now support grid scaling for bulk placement along water edges. Custom three-phase water validation (sky access, water volume, depth) blocks placement over land, under overhangs, or in shallow water. Note: the game engine was never designed for scaled water extractors — borderline edge cases may occasionally slip through validation. If you're sensitive to this, dismantle any questionable placements manually (Issue #197)
- **Directional Arrow Visual Refresh** - Axis arrows upgraded from plain cones to composite arrows (cylinder shaft + cone arrowhead) for a proper arrow silhouette. X/Y/Z text labels added at arrow tips with billboard behavior (always face the camera). Labels follow the same conditional visibility as arrows: all three visible when idle, only the active axis shown when a modifier key is held (Issue #213)
- **HUD Visual Upgrade** - HUD overlay rendering upgraded with improved scaling quality and Satisfactory-native aesthetic. Cleaner text rendering, better contrast, and improved readability at all resolutions (Issue #179)
- **HUD Themes** - 5 HUD color themes: **Default**, **Dark**, **Classic**, **High Contrast**, and **Minimal**. Configurable in the mod configuration menu (Issue #179)

### Changed
- **Stagger Terminology** - Changed "lean forward/sideways" to "shift forward/sideways" in stagger ZX/ZY descriptions across all languages for clarity

### Fixed
- **Smart Camera Direction with Scaled Extend** - Smart Camera now correctly follows the Scaled Extend direction. Previously, the camera always moved in the +X direction regardless of extend direction. Camera now checks if Scaled Extend is active and flips the target direction for Left-side extends (Issue #275)
- **Scaled Extend Crash on Look-Away** - Fixed `EXCEPTION_ACCESS_VIOLATION` crash in `ClearScaledExtendClones()` when looking away from an Extend target. Root cause: raw `AFGHologram*` pointers became dangling when the engine destroyed holograms before our cleanup ran. Fix: `TWeakObjectPtr` collection before the destroy loop (Issue #274)
- **Scaled Extend Re-trigger on Similar Buildings** - Looking at a different building of the same type no longer tears down and rebuilds the entire chain when Scaled Extend is committed/locked for inspection (Issue #274)
- **Belt Crash on Constructor Middle-Click** - Fixed `EXCEPTION_ACCESS_VIOLATION` crash when middle-clicking a constructor to sample its recipe. Added null guard on `mSplineComponent` before access in `PostHologramPlacement`

### Technical
- **Water Extractor Hologram** - Custom child hologram (`ASFWaterPumpChildHologram`) inherits `AFGResourceExtractorHologram` with three-phase validation: (1) Sky Access — traces from 5 bounding box points using registry dimensions; (2) Water Volume — `AFGWaterVolume::EncompassesPoint()`; (3) Depth — terrain depth ≥ 50cm below surface. `ConfigureActor` override bypasses `mSnappedExtractableResource` assertion. Grid spawner respects each child's individual validation state
- **Localization Pipeline** - Complete C++ LOCTEXT implementation replacing all `FText::FromString()` calls. Custom Python pipeline (`sync_po_to_archive.py`) bridges Unreal Engine's localization gap where `GenerateGatherArchive` does not import `.po` translations into `.archive` files for new keys. Pipeline: Gather → Sync (.po → .archive) → Compile (.archive → .locres)
- **Arrow Mesh Loading** - Shaft mesh loaded asynchronously alongside arrowhead via the existing safe asset manager (`SFArrowAssetManager`)
- **Extend Crash Fix Detail** - `IsValid(rawPtr)` on freed memory is undefined behavior. `TWeakObjectPtr::IsValid()` uses the UObject index system and is safe against GC'd objects. Pointers collected into `TArray<TWeakObjectPtr<AFGHologram>>` before the destroy loop
- **Camera Direction Fix Detail** - `GetFurthestTopHologramPosition()` was reading grid counter sign (always positive in Scaled Extend) instead of actual extend direction from `ExtendService->GetExtendDirection()`

### Known Issues
- **Arabic, Persian, and Thai Disabled** - These three languages have complete translations ready but are disabled in this release due to a text rendering issue. Smart!'s custom Slate widgets do not support the complex glyph shaping and right-to-left layout these scripts require — characters display disconnected and unreadable with no English fallback. The base game renders these languages correctly using its own font pipeline; matching that configuration for Smart!'s widgets is planned for a future update. The `.po` translation files are preserved and will be enabled once rendering is resolved

---

## [29.0.2] - 2026-02-25

### Added
- **Auto-Hold on Grid Change** - New mod configuration option (no reload required): when enabled, the hologram is automatically locked in position after any grid modification (X/Y/Z count > 1). Unlike Scaled Extend's internal lock, this hold is user-overridable — pressing the vanilla Hold key releases it. The next grid change re-engages auto-hold. Designed for workflows where you scale first, then fine-tune placement (Issue #273)
- **Ceiling Light & Wall Floodlight Scaling** - Ceiling lights and wall-mounted floodlights now support grid scaling. Ceiling Light: 1100×1200×100cm grid spacing; Wall-Mounted Flood Light: 500×750×200cm grid spacing (Issue #200)
- **Floodlight Angle Sync** - When scaling floodlight towers or wall-mounted floodlights (2-step build), all child holograms mirror the parent's fixture angle in real time as the angle is adjusted in step 2. Previously only the parent updated visually (Issue #200)
- **Extend Toggle** - Double-tap CycleAxis (Num0) now also disables Extend for the session, alongside auto-connect; resets when hologram changes or build gun is cleared. Persistent "Extend Enabled" option added to mod configuration menu (Issue #257)
- **Floor Hole Pipe Auto-Connect** - Pipe floor holes (passthroughs) now auto-connect to nearby factory buildings with unconnected pipe connectors. Place a pipe floor hole near a refinery or other building and a pipe preview is automatically created. Supports scaling — each child floor hole gets its own pipe connection. Pipe tier, style, and routing mode inherit from existing pipe auto-connect settings. Smart Panel shows pipe controls when a pipe floor hole is selected (Issue #187)

### Fixed
- **Pipe Junction Both-Side Connections** - Pipeline junctions now auto-connect to buildings on BOTH sides (left and right), not just the closest side. Root cause: `GetComponents()` ordering doesn't match physical connector positions — replaced hardcoded index mapping with normal-based opposite detection (`GetOppositeConnectorByNormal`). Previously only 1 building connection was created per junction (Issue #206)
- **Power Pole Placement Restrictions** - Power poles (Mk1-3) can now be placed inside other buildables and floating in mid-air when scaled, matching vanilla behavior. Added `AFGPowerPoleHologram` to the floor validation exception list — children no longer fail `CheckValidFloor()` with "Surface too uneven" (Issue #203)
- **Smart Dismantle for Non-Scaled Extend** - Buildings placed via non-scaled Extend (1x1 grid) are now grouped into a Blueprint Proxy for bulk dismantling with vanilla's Blueprint Dismantle mode (R key). Previously only scaled grids (2x1+) were grouped (Issue #270)
- **Child Hologram Snapping** - Scaled building children no longer snap to inconsistent heights when placed partially over foundations. Root cause: vanilla `SetHologramLocationAndRotation` does internal floor tracing, causing children over a foundation to snap +100cm while children off the edge stayed at the calculated position. Replaced with direct `SetActorLocation` for all grid children so positions are strictly grid-calculated relative to the parent (Issue #171)

---

## [29.0.1] - 2026-02-25

### Fixed
- **Smart Panel Initial Focus** - Panel now properly handles initial keyboard focus when opened; Tab and Shift+Tab cycle between input fields (Issue #212)
- **Smart Panel Reset Button** - Added reset button to zero out spacing, steps, stagger, and rotation values with one click (Issue #165)
- **Smart Panel K Key Close** - K key now hardcoded to close Smart Panel regardless of keybind remapping (partial fix for Issue #231)
- **Wall Outlet Upgrades** - Wall outlets (single and double, Mk1-Mk3) now appear upgradeable in Smart Upgrade panel (Issue #267)
- **Belt Preview Tracking Cooldown** - Belt auto-connect preview tracking now uses cooldown to prevent excessive recalculation (Issue #269)
- **Pipeline Pump Scaling** - Pipeline pumps (Mk.1 and Mk.2) now support grid scaling (Issue #245)
- **Extend Floor Hole Cloning** - Floor holes (passthroughs) along conveyor lift and pipe chains are now discovered, cloned, and scaled during Extend operations (Issue #260)
  - Fixed pipe passthrough height calculation using actual floor thickness instead of hardcoded 400cm
  - Removed pipe-only filter from passthrough discovery to include both lift and pipe floor holes
  - Floor holes now scale correctly with Scaled Extend (X×Y grids)
  - Fixed crash when deleting cloned lifts by using chain-level `RemoveConveyorChainActor` API instead of bucket-level `AddConveyor`
  - Fixed half-height lift rendering in floor holes by capturing passthrough references during JSON manifold capture and applying them to lift holograms before construction via second-pass linking

---

## [29.0.0] - 2026-02-23

**🎉 The Ultimate Fusion: Scaled Extend**

This release delivers one of the most requested features in Smart! history — **Scaled Extend**, the fusion of the two core features from the original Smart mod: **Scaling** and **Extending**. For years, players have asked: "Can I scale my Extend clones?" The answer is finally here.

**What makes this special:** In the original Smart mod, you could either scale buildings into grids OR clone entire manifolds with Extend — but never both at once. Scaled Extend breaks this limitation. Now, when you activate Extend on a manifold, you can use X/Y scaling to duplicate it across multiple clones and rows, with all infrastructure (belts, pipes, power) automatically chaining between them. It's the factory builder's dream: point at a proven design, scale it to 5×3, and watch Smart! create 15 complete copies with full manifold connections in a single click.

This isn't just a feature addition — it's a fundamental transformation of what Smart! can do. Scaled Extend represents months of architectural work, from lane segment routing to rigid body rotation to vanilla constraint validation, all working together to make factory duplication feel effortless.

### Added
- **Scaled Extend** - Scale Extend across multiple clones and rows for rapid factory duplication (Issue #265)
  - After Extend activates, use X scaling to add clones in the extend direction (chain pattern: Source → Clone 1 → Clone 2 → ...)
  - Use Y scaling to add parallel rows perpendicular to the extend direction; negative Y places rows on the opposite side
  - Each additional row gets an automatic **Lane Seed** — a clone of the source building that anchors the new row
  - **Lane Segments** (belts and pipes) automatically chain between adjacent clones' distributors
  - Spacing, Steps, and Rotation transforms all work between clones (linear, arc/radial, terraced layouts)
  - Infrastructure (distributors, belts, pipes, power poles) maintains rigid topology relative to each factory clone
  - HUD shows "Extend: 3×2" format (clones × rows) during Scaled Extend
  - Smart Panel adapts for Extend mode: Grid Z, Spacing Z, and Stagger sections hidden (not applicable)
  - All clones are grouped into a single blueprint proxy for Smart Dismantle compatibility
- **Lazy Commit** - Extend preview appears immediately when aiming at a valid target, but doesn't commit to sticky mode until your first scale action
  - Before committing: look away to cancel Extend, middle-click to sample the building normally
  - After committing: Extend stays active when you look away (Sticky Extend), allowing free camera inspection
  - Only explicit actions (building, changing recipe, clearing build gun) tear down a committed Extend
- **Automatic Row Spacing** - Row spacing automatically accounts for infrastructure footprint to prevent overlap
  - Uses the topology's full Y extent (including distributor edges) rather than just the factory building's width
  - Rows with wide manifold infrastructure (e.g., distributors extending beyond the factory) space correctly at zero spacing
- **Smart Upgrade "Entire Map" Button** - Added "Entire Map" button to the Radius tab of the upgrade panel
  - Sets radius to 0 (no limit) and immediately triggers scan for all buildables of the target type
  - Radius spinbox now accepts 0 as a valid input (entire map mode)

### Changed
- **Power Extend Bounding Box** - Increased manifold bounds padding from 8m to 20m for power pole discovery
  - Power poles further from the manifold are now captured during Extend topology walks
  - Reduces missed power poles in layouts with wider pole spacing

### Fixed
- **Lane Segment Colors** - Lane segments (belts/pipes connecting clones) no longer inherit the factory building's color swatch
  - Now samples color from existing belt/pipe on the other side of the source distributor
  - Falls back to default colors when no existing infrastructure is present
- **Pipe Lane Normals** - Fixed pipe lane segment splines routing inward through the junction
  - Connector normals now correctly computed as outward-facing from junction center
  - Pipe lanes route naturally between adjacent clone junctions
- **Clone Infrastructure Colors** - Cloned belts, pipes, and distributors now inherit colors from their individual source actors
  - Previously all clones inherited the parent factory hologram's color (e.g., refinery's caterium swatch)
- **Smart Upgrade Save Crash** - Fixed `EXCEPTION_ACCESS_VIOLATION` in `AFGConveyorChainActor::Serialize()` during save after upgrading
  - Chain actors from old belts/lifts are now captured before destruction and properly invalidated
  - Prevents stale chain actors with dangling references from persisting to save

### Technical
- **Scaled Extend Architecture** - `CalculateScaledExtendPositions()` computes clone grid with spacing, steps, rotation, and 2D row layout
  - `SpawnScaledExtendPreviews()` creates factory holograms and infrastructure for each clone position
  - Chain topology: each clone connects to its predecessor via lane segments (not hub-and-spoke)
  - Topology Y Extent uses `max(BuildingSize.Y, TopologyYExtent)` with distributor edge accounting (half-width expansion)
- **Lazy Commit System** - `bExtendCommitted` flag gates sticky behavior; set on first `OnScaledExtendStateChanged()` call
  - `ClearExtendState()` ordering fix: extend flags cleared before `UpdateCounterState()` to prevent re-lock via cascaded state change
  - Explicit `CurrentExtendHologram->LockHologramPosition(false)` at start of `ClearExtendState()` for reliable unlock
- **Lane Segment Validation** - Enforces vanilla belt/pipe constraints on Scaled Extend lane segments
  - Belt lanes: min 0.5m, max 56m, 30° max angle at both connectors
  - Pipe lanes: min 0.5m, max 25m, 30° max angle at both connectors
  - Hologram turns red and blocks building when constraints are violated
  - HUD displays specific failure reason (e.g., "Belt lane too long (58.2m > 56m maximum)")
  - Automatically recovers when user adjusts spacing/steps back to valid range
- **Lexicon** - Added `Lexicon.md` glossary of Smart! terms and features for community reference

---

## [28.0.1] - 2026-02-22

### Fixed
- **Recipe Scaling Regression** - Fixed recipes not being applied to scaled production buildings (Issue #264)
  - Only the first building in a scaled grid was receiving its recipe configuration
  - All grid children (scaled copies) were missing their recipes
  - Root cause: Smart Dismantle's blueprint proxy creation (v27.0.0) was incorrectly blocking recipe application for grid children
  - The `bBlueprintProxyRecentlySpawned` flag is now cleared immediately after Smart! creates its own proxy for grid grouping
  - Recipes now apply correctly to all buildings in scaled grids

---

## [28.0.0] - 2026-02-22

### Added
- **Power Extend** - Power poles are now cloned alongside factory buildings when using Extend (Issue #229)
  - Power poles connected to the source factory within the manifold bounding box are automatically discovered and cloned
  - Cloned power poles are wired to the cloned factory building
  - Source poles are wired to clone poles if they have free connections (respects connection limits)
  - Wire costs are calculated and deducted during preview phase (1 cable per 25m distance)
  - Bounding box filtering ensures only relevant power poles are cloned (200cm padding)
  - Works with all power pole types including wall-mounted outlets
  - New setting: **Extend Power Poles** toggle in mod configuration (default: enabled, no world reload required)

### Technical
- **Power Topology System** - Extended Extend topology capture to include power infrastructure
  - Added `WalkPowerConnections()` to discover power poles via wire traversal from factory buildings
  - Added `CalculateManifoldBounds()` for spatial filtering using AABB of entire manifold (belts, pipes, distributors)
  - Added `FSFPowerChainNode` and `FSFSourcePowerPole` data structures for power pole topology
  - Created `ASFPowerPoleChildHologram` for power pole child hologram spawning
  - Reused `ASFWireHologram` from power auto-connect for wire cost aggregation
  - Added `SetWireEndpoints()` to `ASFWireHologram` for distance-based cost calculation
  - Modified `ASFWireHologram::Construct()` to spawn raw wire actors for extend children (prevents nullptr crash)
  - Post-build wiring implemented in `SFExtendService::GenerateAndExecuteWiring()`
  - Power pole wiring respects connection limits (Mk1: 4, Mk2: 7, Mk3: 10)
  - Added `bExtendPowerEnabled` config property and `bExtendPower` runtime setting
  - Power pole discovery gated behind runtime setting check (live-checked on each Extend operation)

---

## [27.0.0] - 2026-02-21

### Added
- **Smart Dismantle** - Blueprint-based group dismantling for Smart!-placed grids (Issue #166)
  - Multi-building grids (e.g., 3×3 foundations, 2×2 constructors) are automatically grouped into a native `AFGBlueprintProxy`
  - Press **R** in dismantle mode to switch to "Blueprint Dismantle" mode
  - Aim at any building in a Smart!-placed grid to highlight and dismantle the entire group at once
  - Works with all building types: foundations, factories, distributors, and more
  - Works with **Extend** chains — dismantle an entire row of extended buildings and their auto-connected belts in one action
  - Works with **Auto-Connect** — dismantle rows of auto-connected distributors and their belts together
  - Proper refund calculation for the entire group
  - Session tracking prevents proxy bleeding between separate grid placements
  - Vanilla blueprint placement is protected — scaling is automatically disabled when placing blueprints to prevent conflicts

### Technical
- **Blueprint Proxy Integration** - Smart! grids use Satisfactory's native blueprint system for group dismantling
  - `AFGBlueprintProxy` created on first building spawn for multi-building grids (grid > 1×1×1)
  - Session tracking via `CurrentProxyOwner` prevents proxy reuse across different grid placements
  - Proxy cleared on hologram unregister to ensure clean state for next build session
- **Blueprint Hologram Detection** - Added `AFGBlueprintHologram` detection at adapter creation
  - Creates `FSFUnsupportedAdapter` for blueprint holograms to disable all Smart! features during vanilla blueprint placement
  - Prevents scaling interference with vanilla blueprint system
  - Consistent with existing unsupported building handling (vehicles, space elevator, etc.)

---

## [26.0.0] - 2026-02-21

### Added
- **Smart Upgrade** - Batch upgrade system for existing factory infrastructure (inspired by [MassUpgrade](https://ficsit.app/mod/MassUpgrade) by Marcio)
  - Press **K** while the build gun is equipped to open the Smart Upgrade Panel
  - Two selection modes via tabs: **Radius** and **Network Traversal**
  - Supports belts (Mk1–Mk6), lifts (Mk1–Mk6), pipes (Mk1–Mk2), power poles (Mk1–Mk3), and wall outlets — single and double (Mk1–Mk3)
  - Target tier defaults to your highest unlocked tier; dropdown only shows tiers you've researched
  - Net cost (new building cost minus old building refund) is previewed before committing
  - Respects inventory and Dimensional Depot; aborts if materials run out mid-batch, with overflow crates spawned for excess refunds
  - Panel is draggable (right-click drag); close with the X button or Escape
- **Radius mode** - Scan an area around the player for upgradeable infrastructure
  - Set a radius (minimum 4m, up to 1,000m via slider or unlimited via manual entry, in 4m increments) and click Scan
  - Results grouped by family and tier; upgradeable counts shown in green
  - Click any row to highlight it and see the distance and compass direction to the nearest instance
  - Re-scan after upgrading to refresh counts
- **Network Traversal mode** - Upgrade an entire connected network at once
  - Anchor detection uses the hologram's upgrade target if valid; falls back to a line trace and nearest-buildable search within 3m of the aim point
  - Traversal follows connections through the full network (belts follow factory connections, pipes follow pipe connections, power follows wire connections)
  - Configurable: checkboxes control whether traversal crosses splitters/mergers (default: on), storage containers (default: off), and train platforms (default: off)
  - Mixed-tier networks supported — lifts in a belt network are upgraded to the matching lift tier automatically
  - Safety limit of 10,000 buildables per traversal

### Fixed
- **Stackable Conveyor Pole Belt Auto-Connect** - Re-enabled belt auto-connect between stackable conveyor poles. Belts are now wired by proximity in a deferred timer, then chain actors are invalidated via `RemoveConveyorChainActor()` so vanilla rebuilds correct chains. Previous attempts using bucket-level APIs (`AddConveyor`/`RemoveConveyor`) caused crashes; the chain-level API is safe from deferred timers.

### Technical
- **Upgrade Execution Service** - Server-side batch execution with per-item cost deduction
  - Overflow crate spawning for excess items during cost deduction
- **Upgrade Flow Internals** - Deferred spawning to set build class before BeginPlay
  - Hologram-based upgrade construction for belt spline integrity
- **Chain Actor Safety** - Uses `RemoveConveyorChainActor()` (chain-level) rather than `RemoveConveyor()`/`AddConveyor()` (bucket-level) for safe chain invalidation after connection changes. Confirmed correct pattern by Archengius (Coffee Stain Studios developer).

---

## [25.0.1] - 2025-12-28

### Fixed
- **Rotational Transform Keybind** - Fixed Comma key not triggering Rotation Mode (Issue #254)
  - Added missing Comma key mapping to `MC_Smart_BuildGunBuild` Input Mapping Context
  - Configured `PlayerMappableKeySettings` for proper in-game keybinding menu display

---

## [25.0.0] - 2025-12-28

### Added
- **Smart! Camera API** - Public API hooks for the Smart! Camera companion mod
  - `GetFurthestTopHologramPosition()` - Returns world position of the furthest hologram in the grid
  - Tracks furthest corner on all axes (X, Y, Z) including negative directions
  - Enables PiP camera to show grid extent for downward building operations
- **Hologram Lock API** - Exposed hologram locking functions for external mod integration
  - `TryAcquireHologramLock()` - Lock hologram position (prevents rotation during modifier keys)
  - `TryReleaseHologramLock()` - Release lock when modifier released
  - Allows companion mods to use the same locking pattern as Smart! scaling modifiers

### Technical
- Phase 4 API infrastructure for Smart! Camera companion mod
- Grid position calculation now supports negative Z for downward building tracking

---

## [24.4.3] - 2025-12-26

### Fixed
- **Power Auto-Connect Regression** - Fixed power auto-connect not working during scaling (Issue #250)
  - The 24.4.2 fix for insert-into-wire crash was too aggressive and blocked all power auto-connect
  - Detection now correctly identifies only vanilla wire holograms, not child power poles from scaling
  - Power line previews now appear correctly when scaling power pole grids

---

## [24.4.2] - 2025-12-26

### Fixed
- **Power Pole Insert Crash** - Fixed game crash when inserting a power pole into an existing power line (Issue #248)
  - Smart! power auto-connect now detects vanilla's "insert pole into wire" mode and skips adding wire children
  - Prevents assertion failure in vanilla's `AFGPowerPoleHologram::Construct()` which expects exactly 2 wire children

---

## [24.4.1] - 2025-12-26

### Fixed
- **Belt Auto-Connect Global Setting** - Global "Belt Auto-Connect" toggle in mod configuration now properly disables belt auto-connect (Issue #246)
  - Previously, belts would still auto-connect even when the global setting was disabled
  - Runtime toggle (U+Num0) still works to override the setting during placement
- **Frame Wall Size** - Fixed Frame Wall Y dimension causing 0.5m overlap when scaling (Issue #247)
  - Corrected registry entry from 7.5m to 8m width

---

## [24.4.0] - 2025-12-24

### Added
- **One-Shot Auto-Connect Disable** - Double-tap Num0 (with no modifier keys) to temporarily disable all auto-connect for your next placement (Issue #198)
  - Works with power poles, distributors, pipe junctions, and stackable supports
  - HUD displays "*Auto-Connect Disabled (double-tap to re-enable)" when active
  - Double-tap again to re-enable
  - Automatically resets when you change buildings or put away the build gun

### Fixed
- **Power Auto-Connect Toggle** - Runtime toggle (U+Num0) now immediately clears power wire previews when disabled
  - Previously, wire previews would remain visible until the hologram moved
- **Pipeline Pump Mk.2 Scaling** - Disabled scaling for Pipeline Pump Mk.2 to match Mk.1 behavior (Issue #170)
  - Pipeline pumps are functional pipe attachments that don't support grid scaling
  - Both Mk.1 and Mk.2 now consistently show "Scaling Disabled" when selected
- **HUD Position Customization** - Added sliders to reposition the Smart! HUD overlay (Issue #175)
  - New "HUD Position X" and "HUD Position Y" settings in mod configuration (Main Menu > Mods > Smart!)
  - Values range from 0.0 (left/top) to 1.0 (right/bottom)
  - Defaults: X=0.02 (2% from left), Y=0.25 (25% from top)
  - Position is clamped to keep HUD visible even with extreme values

---

## [24.3.2] - 2025-12-24

### Fixed
- **Smart + Zoop Conflict** - Fixed overlapping holograms when using Smart! scaling with vanilla Zoop (Issue #160)
  - Smart! now detects when Zoop is active and automatically disables scaling (grid forced to 1x1x1)
  - HUD displays "Zoop Active - Scaling Disabled" warning when Zoop takes priority
  - Works with mid-placement Zoop activation (click-and-drag while already scaled)
  - Scaling automatically re-enables when Zoop is released
- **HUD State Persistence** - Fixed stale HUD information showing when switching building types
  - Auto-connect settings no longer persist incorrectly across different hologram types
  - Zoop warning no longer shows on buildings that don't support Zoop
  - All HUD state now properly resets when selecting a new building

---

## [24.3.1] - 2025-12-24

### Fixed
- **Power Grid Axis Setting** - Fixed power pole auto-connect not respecting Grid Axis setting (Issue #244)
  - Grid Axis (Auto/X/Y/X+Y) now correctly controls which axes power wires connect on
  - Rewrote neighbor detection to use grid positions instead of spatial distance
  - Works correctly with transforms like stepping, rotation, and stagger
  - Built wires now match preview wires exactly

---

## [24.3.0] - 2025-12-24

### Added
- **Lift Height HUD Display** - Conveyor lift height now displays on the Smart! HUD during placement (Issue #243)
  - Shows absolute lift height (↕) and world height (⇓) in meters
  - Updates in real-time as you adjust the lift
  - Automatically clears when switching to a different building type

---

## [24.2.3] - 2025-12-23

### Fixed
- **Extend Manifold Belts Not Connecting** - Fixed regression where Extend manifold belts failed to connect (Issue #241)
  - v24.2.2 crash fix accidentally skipped the first `PostHologramPlacement` call which is needed to establish snapped connections
  - Restored v24.2.0 behavior: call `Super::PostHologramPlacement()` once to wire connections, then skip subsequent calls to prevent crash
  - Manifold belts connecting source distributors to clone distributors now work correctly again

---

## [24.2.2] - 2025-12-23

### Fixed
- **Extend Belt Crash** - Fixed null pointer crash when using Extend with belt connections
  - Vanilla's `PostHologramPlacement` was trying to regenerate spline meshes with null/invalid data
  - Extend belt children now skip `PostHologramPlacement` entirely (matching auto-connect pattern)
  - Meshes are already correctly generated via `TriggerMeshGeneration` before placement
  - Removed unused `bPostHologramPlacementCalled` tracking variable
- **Extend Pipe Manifold Lanes** - Fixed manifold lane pipes always using Mk2 tier instead of respecting Auto-Connect settings (Issue #240)
  - When Pipe Tier (Main) is set to "Auto", Extend now correctly uses the player's highest unlocked pipe tier
  - Matches the belt manifold lane behavior fixed in v24.0.2
  - Fallback changed from Mk2 to Mk1 (always available)
  - Added `GetCost()` override to `ASFPipelineHologram` to properly aggregate pipe costs into parent hologram
  - Pipe costs (copper sheets, plastic for Mk2) now correctly show in build cost UI and block placement when unaffordable
- **Walkway/Catwalk Ramp Auto-Stepping** - Added automatic step value defaults for descending walkway structures
  - Walkway Ramp and Catwalk Ramp now default to -2m X step (descends 2m per 4m forward)
  - Catwalk Stairs now default to -4m X step (descends 4m per 4m forward)
  - Matches the auto-stepping behavior already present for foundation ramps
  - Step values can still be manually adjusted after initial placement

---

## [24.2.1] - 2025-12-23

### Fixed
- **Free Building Bug** - Fixed critical bug where Creative Mode was incorrectly allowing free building
  - Creative Mode and "No Build Cost" are separate settings in Satisfactory
  - Previously, having Creative Mode enabled (for flying, instant research, etc.) would allow building without resources even when "No Build Cost" was disabled
  - Now correctly checks only the "No Build Cost" cheat setting
  - Red (unaffordable) holograms can no longer be placed without sufficient resources unless "No Build Cost" is explicitly enabled
  - Affects all building types: foundations, factories, distributors, belts, pipes, and power poles

### Technical
- **Removed ValidatePlacementAndCost Hook** - Eliminated legacy affordability validation hook (313 lines)
  - Hook was only needed for old custom cost injection system (pre-v24.2.0)
  - Vanilla child hologram patterns now handle all affordability checks correctly
  - Simplified codebase and improved maintainability
- **Fixed Creative Mode Checks** - Removed incorrect `IsCreativeModeEnabled()` checks in `ChargePlayerForBelt` and `ChargePlayerForPipe`
  - Now only checks `GetCheatNoCost()` for free building determination

---

## [24.2.0] - 2025-12-22

### 🔧 Auto-Connect Architecture Refactor

**Major internal refactor of all Auto-Connect systems to use vanilla-friendly child holograms.** This eliminates custom recipe manipulation and cost injection, replacing them with proper Unreal Engine child hologram patterns. While not adding new user-facing features, this refactor fixes several critical bugs and improves compatibility.

### Added
- **Smart Panel - Pipe Routing Mode** - New ComboBox for selecting pipe auto-connect routing strategy
  - Options: Auto, Auto 2D, Straight, Curve, Noodle, Horiz→Vert
  - Accessible in the Pipe Auto-Connect section when placing pipe junctions or stackable pipe supports
- **Apply Immediately for Auto-Connect Settings** - All auto-connect controls now respect "Apply Immediately" mode
  - Belt, Pipe, and Power tier/mode changes trigger instant preview refresh
  - Enabled/Disabled checkbox changes take effect immediately
  - No need to click "Apply" button when Apply Immediately is checked

### Changed
- **Auto-Connect Internal Architecture** - Rebuilt belt, pipe, and power auto-connect systems from the ground up
  - Now uses vanilla-friendly patterns that integrate properly with the game's build system
  - Fixes several long-standing bugs and makes the mod easier to maintain long-term
  - Improves compatibility with game updates and other mods
- **Smart Panel Layout - Stackable Pole Controls** - Improved organization of stackable support settings
  - Direction field now has its own dedicated row for better visibility
  - Direction and Build Mode ComboBoxes aligned to the right for visual consistency
  - ComboBox styling unified (white background) to match existing controls

### Fixed
- **Inflated Belt/Pipe Costs** - Auto-connect items now use correct vanilla cost aggregation
  - Previously, custom recipe injection could cause incorrect or inflated material costs
  - Child holograms automatically aggregate costs through native UE systems
  - Costs display correctly in vanilla build gun UI
- **Free Building with Advanced Game Settings** - Fixed bug where builds were incorrectly free
  - Custom cost handling was bypassing vanilla affordability checks
  - Now properly respects "No Build Cost" setting (free only when enabled)
  - Dimensional Depot integration works correctly for auto-connect materials
- **Power Auto-Connect Enabled Toggle** - Power auto-connect now properly respects the Enabled checkbox
  - Previously, power line previews were created even when the setting was unchecked
  - Unchecking now immediately clears all power line previews
- **Pipe Auto-Connect Tier Selection** - Pipe tier settings now correctly apply to auto-connect previews
  - Main tier and To-Building tier selections are properly respected
  - Routing mode changes update previews in real-time
- **Extend Crash with Locked Belt Tiers** - Fixed crash when extending builds before unlocking Mk5 belts
  - Manifold lane segments now use the global auto-connect belt tier setting
  - Falls back to highest unlocked tier instead of hardcoded Mk5
- **Extend Crash from Repeated Spline Updates** - Fixed crash during Extend placement
  - Vanilla parent hologram was calling PostHologramPlacement repeatedly on belt children
  - Now skips subsequent calls after first successful spline generation
  - Thank you for your testing, Asher_Roland!

### Technical
- **Eliminated Recipe CDO Injection** - Removed `SFRecipeCostInjector` service
  - Child holograms automatically aggregate costs via `GetCost()` override
  - No manual recipe manipulation needed
- **Eliminated Manual Cost Deduction** - Removed inventory/central-storage deduction logic
  - Vanilla build flow handles costs via child holograms
  - Prevents double-charging issues
- **Deferred Wiring Pattern** - Standardized post-build connection approach
  - Belts: Wiring triggers in `OnActorSpawned` after spawn
  - Pipes: Wiring called directly from `Construct()` (OnActorSpawned fires before tags transfer)
  - Power: Wiring in `ConfigureActor()` finds built parent pole
- **New Custom Hologram Classes:**
  - `ASFConveyorBeltHologram` - Belt auto-connect child hologram
  - `ASFPipelineHologram` - Pipe auto-connect child hologram
  - `ASFWireHologram` - Power wire auto-connect child hologram
- **New Subsystem Functions:**
  - `TriggerAutoConnectRefresh()` - Forces re-evaluation of all auto-connect previews
  - `RegisterPipeForDeferredWiring()` - Queues pipes for post-build connection
  - `RegisterBeltForDeferredWiring()` - Queues belts for post-build connection
- **Orchestrator Improvements:**
  - `ForceRefresh()` - Triggers re-evaluation of all connection types
  - Separated evaluation locks for belts vs pipes to prevent race conditions

### Multiplayer Note
While this refactor improves vanilla compatibility, **multiplayer is still not officially supported**. The child hologram pattern should reduce desync issues, but full multiplayer testing has not been performed.

---

## [24.1.0] - 2025-12-18

### Added
- **Stackable Support Auto-Connect** - Belts and pipes now automatically connect between stackable poles
  - **Unified HUD Control**: Hold **U** on stackable poles to access settings (Tier, Direction, Flow Indicators) and tap "Num0" to switch between settings
  - **Smart Validation**: Connections are restricted to valid ranges (Max 56m distance, Max 30° vertical angle)
   - **Bus Layout Optimized**: Defaults to 55m spacing on X-axis for convenient max-length placement
  - **Dynamic Updates**: Settings changes immediately refresh previews and respect global auto-connect toggles

### Changed
- **Steps Transform Default Axis** - Steps mode now defaults to X-axis first instead of Y-axis
  - Press Num0 while holding U to toggle between X (columns) and Y (rows)
- **Branding Update** - Updated tagline from "THE MOD THAT DOES THINGS OTHER'S CAN'T!" to "THE FACTORY BUILDER'S MULTITOOL"
  - New tagline better represents Smart's comprehensive factory building capabilities
  - Updated logo with a brighter, more vibrant image for better visibility

### Fixed
- **FICSMAS Gift Tree Extend** - Enabled Extend support for FICSMAS Gift Trees
  - Previously blocked because they inherit from resource extractors (miners)
  - Added specific exception to allow them as valid Extend targets
- **Extend Belt Holograms** - Fixed belt/pipe textures not appearing on Extend preview holograms
  - Restored proper spline generation and material handling for Extend belt children
- **Power Auto-Connect Toggle** - Setting can now be toggled and takes effect immediately during gameplay
  - Previously required returning to main menu for changes to apply
- **Rotation Mode Keybind Label** - Fixed keybind label incorrectly showing "Stagger" instead of "Rotation"
  - Default key (comma) now correctly labeled in Options > Controls > Mods
- **Stackable Pipe Spline Curves** - Improved visual appearance of pipes between stackable supports
  - Pipes now extend 50cm straight out from connectors before curving
  - Creates smoother, more natural-looking pipe connections
  - Maximum pipe preview length limited to 56m

---

## [24.0.3] - 2025-12-16

### Fixed
- **Extend + Smart Panel** - Disabled Smart Panel (K key) while Extend is active to prevent incompatible actions
  - Scaling, spacing, steps, stagger, and rotation settings don't apply correctly to extended buildings
  - All scaling keybinds (X, Z modifiers, NumPad 4/6/8/5/9/3) are now blocked during Extend
  - Mouse wheel remains functional for Extend direction cycling (Forward/Right/Backward/Left)
  - Long-term goal: Support dynamic scaling adjustments for Extend (Issue #228)
- **Extend Pipe Freeze** - Fixed game freeze when using Extend on buildings with fully-connected pipe junctions
  - Lane segment generation now skips junctions with no available connectors instead of using uninitialized data

---

## [24.0.2] - 2025-12-15

### Fixed
- **Extend Manifold Lanes** - Fixed manifold lane belts always using Mk5 tier instead of respecting Auto-Connect settings (Issue #226)
  - When Belt Tier (Main) is set to "Auto", Extend now correctly uses the player's highest unlocked belt tier

---

## [24.0.1] - 2025-12-14

### Fixed
- **Extend** - Prevented an infinite freeze state when aiming Extend at invalid targets (Issue #224)
  - Extend now only activates on production machines and power generators
  - Resource extractors (miners, oil extractors, resource well extractors, water extractors) are explicitly blocked

---

## [24.0.0] - 2025-12-13

### 🎉 Extend Returns!

**The most anticipated feature from the original Smart! mod is back.** Extend was one of the defining features that made Smart! essential for factory builders, and after months of ground-up rebuilding for Unreal Engine 5, it's finally here.

### Added
- **Extend** - Duplicate connected factory layouts with a single click
  - Point at an existing building of the same type while holding a hologram
  - Automatically clones the entire connected manifold (distributors, belts, pipes)
  - Preserves belt/pipe routing, connections, and recipes
  - Works with Constructors, Assemblers, Manufacturers, and other production buildings
  - Manifold lanes connect source and clone distributors automatically
  - Chain actors and pipe networks are properly initialized for immediate item/fluid flow
  - Intelligently skips manifold connections when source distributors are already connected

- **Smart! Panel** - New visual interface for all Smart! settings (press **K** to toggle)
  - **Visual Discovery**: See all options instead of memorizing modifier keys
  - **Context Intelligence**: Building-specific options appear automatically
  - **SpinBox Controls**: Precise numeric input with mouse wheel adjustment
  - **Direction Toggles**: +/- buttons for each grid axis
  - **Apply Button**: Commit changes with optional "Apply Immediately" mode
  - **HUD Suppression**: HUD hidden while panel is open for clean view
  - **Escape Key**: Closes panel and reverts uncommitted changes

- **Smart! Panel - Recipe Section** - Recipe selection and throughput planning
  - **Recipe ComboBox**: Dropdown with all compatible recipes for current building
  - **Recipe Icons**: 40x40 product icons displayed next to selections
  - **Per-Minute Rates**: Calculated from manufacturing duration
  - **Grid Totals**: Shows combined throughput for entire grid (e.g., "Grid Total: 360/min")
  - **Clear Button**: Quick recipe deselection

- **Smart! Panel - Auto-Connect Section** - Visual controls for auto-connect settings
  - **Belt Controls**: Enable/disable, Manifold Lane tier, Factory Belt tier, Use Manifold Lane checkbox
  - **Pipe Controls**: Enable/disable, Main tier, To-Building tier, Flow Indicator checkbox
  - **Power Controls**: Enable/disable, Grid Axis selection (Auto/X/Y/X+Y), Connections to Keep Free
  - **Dynamic Tier Population**: Only shows unlocked tiers (Mk1-Mk6 belts, Mk1-Mk2 pipes)
  - **Auto Resolution Display**: "Auto" shows resolved tier in parentheses (e.g., "Auto (Mk5)")
  - **Contextual Visibility**: Controls only appear for relevant hologram types

- **Smart! Panel - Large Grid Warnings** - Safety system for large placements
  - **Grid Total Display**: Shows current hologram count (X × Y × Z)
  - **Tiered Warnings**: Yellow (100+), Orange (500+), Red (1000+) color-coded alerts
  - **Confirmation Dialogs**: Required for grids ≥500 holograms before applying
  - **Apply Immediately Auto-Disable**: Automatically disabled for large grids to prevent freezes

- **Rotation Transform (Radial/Arc Placement)** - New transform mode for curved arrangements
  - Hold **Comma (,)** and scroll to adjust rotation step in degrees
  - Creates arcs, circles, and curved foundation paths
  - Positive rotation curves right (clockwise), negative curves left (counter-clockwise)
  - Multi-row grids create parallel curved lanes (like road lanes)
  - All sign combinations work correctly: ±X (forward/backward) × ±Rotation (right/left)
  - Settings Form includes Rotation Z SpinBox for precise angle input
  - HUD displays rotation with calculated radius and buildings-per-circle

### Improved
- **Auto-Connect Cost Display** - Belt, pipe, and power cable costs now appear in the vanilla build cost UI
  - Costs are injected directly into the hologram's recipe, appearing alongside building materials
  - Vanilla affordability checking - build is blocked if you can't afford the total cost
  - Removed custom HUD cost display in favor of unified vanilla UI
  - Supports Dimensional Depot - costs check central storage automatically
  - Properly respects "No Build Cost" Advanced Game Setting - auto-connect items are free when enabled

### Fixed
- **Hypertube Pole Stackable** - Added missing registry entry with validated dimensions (100x200x200 cm via spacing test)
- **Pipeline Support Stackable** - Validated and corrected dimensions from estimates to measured values (100x200x200 cm via spacing test)
- **Stackable pole spacing** - Both stackable support poles now have correct edge-to-edge alignment when scaled

### Technical
- New `USmartSettingsFormWidget` C++ class with Blueprint hybrid architecture
- `Smart_SettingsForm_Widget` Blueprint with BindWidget properties for all controls
- `OnToggleSettingsForm()` in `SFSubsystem` manages widget lifecycle and input modes
- Recipe details use `SFRecipeManagementService::GetSortedFilteredRecipes()` for consistency with HUD
- Auto-Connect controls directly modify runtime settings (immediate effect, no Apply needed)
- New `CalculateRotationOffset()` in `SFPositionCalculator` handles arc geometry with independent sign handling
- Arc formula: `Forward = sign(X) * R * sin(|θ|)`, `Sideways = sign(Rotation) * (BaseR - R * cos(|θ|))`
- Multi-row radius: `Radius = BaseRadius - sign(Rotation) * Y * RowGap` for parallel lane behavior
- Rotation mode input via `IA_Smart_Rotation_Mode` bound to Comma key

---

## [23.2.3] - 2025-11-27

### Fixed
- **Power Auto-Connect Dimensional Storage Support** - Fixed power cable costs not checking Dimensional Depot (Issue #199)
  - Power poles failed to build when cables were only in Dimensional Depot, not personal inventory
  - Cable deduction now checks Dimensional Depot first, then personal inventory (matches vanilla behavior)
  - Users can now build power pole grids using cables from central storage
  - Also added pole connection deduplication to prevent double-counting A→B and B→A connections

---

## [23.2.2] - 2025-11-25

### Fixed
- **Power Auto-Connect on Rotated Grids** - Fixed pole-to-pole connections not building when poles are placed on rotated foundations
  - The old neighbor detection used world-space axis alignment which failed for non-axis-aligned grids
  - Now stores planned pole connections during preview phase and uses them during build
  - Works correctly for any grid orientation (including diagonal 45° placements)
  - **Thanks to Alejandro** for chaotic world axis alignment testing that exposed this issue

- **Power Auto-Connect Timing Race Condition** - Fixed pole connections failing due to build lifecycle timing
  - Poles spawn before hologram destruction, so the old commit timing was too late
  - Now uses a deferred queue that accumulates connections and removes them when used
  - Early commit triggers when first pole spawns if planned connections exist
  - Multiple overlapping builds can now coexist without race conditions

- **Power Auto-Connect Large Spacing** - Fixed pole connections failing when poles are spaced more than 30m apart
  - The old lookup only searched "nearby" poles within 30m
  - Now searches all grid-built poles by exact position from the deferred queue
  - Works with any pole spacing (40m, 100m, etc.)

- **Power Auto-Connect Pole-to-Pole Connections Not Building** - Fixed bug where pole-to-pole wire previews would show but connections weren't created when placed
  - The spawn detection was only checking for building connections, not pole-to-pole previews
  - Now correctly triggers connection logic when pole managers are active

### Improved
- **Power Auto-Connect Distance Accuracy** - Building connections now use actual power port location instead of building center
  - More accurate range calculations for large buildings (Refineries, Blenders, etc.)
  - Distance now matches actual wire length that would be created
  - Prioritizes unconnected power ports when buildings have multiple connections
  - **Thanks to Shaded** for requesting this accuracy improvement

### Technical
- Added `PlannedPoleConnections` and `DeferredPoleConnections` to track pole-to-pole connections
- Deferred queue persists across builds and removes connections when successfully used
- Early commit in `OnActorSpawned` when poles spawn before hologram destruction
- Building power port locations cached in `BuildingPowerPortLocations` map for accurate distance calculations
- Added `GetGridBuiltPowerPoles()` getter for position-based pole lookup without distance limits

---

## [23.2.1] - 2025-11-25

### Fixed
- **Power Auto-Connect Crash** - Fixed crash when placing power poles near buildings
  - Changed building connection tracking to use weak object pointers
  - Prevents access violation when buildings are destroyed/invalidated before deferred timer fires
  - Added validity checks before accessing building references during connection phase
  - **Thanks to hououkira** for the detailed crash reports that were critical to identifying this issue

### Technical
- `PlannedBuildingConnections` and `CommittedBuildingConnections` now use `TWeakObjectPtr<AFGBuildable>` instead of raw pointers
- Added safety checks in `OnPowerPoleBuilt()` to skip invalid/destroyed buildings

---

## [23.2.0] - 2025-11-25

### 🚀 Added - Power Auto-Connect System

#### ⚡ Power Auto-Connect (NEW to Smart!)
- **Intelligent Power Grid Management** - Power poles now automatically connect to nearby buildings and each other
  - Grid topology detection for optimal pole-to-pole connections
  - Building connection assignment based on configurable range limits
  - Smart axis selection (Auto/X/Y/X+Y modes) for different grid orientations
  - User-configurable power reservation limits to prevent overloading poles
  - Visual wire previews with accurate cost calculations

- **Advanced Grid Intelligence** - Smart pole placement logic
  - Connects consecutive poles in chains along grid axes
  - Handles any grid orientation (rotated 90°, 45°, etc.)
  - Exempts pole-to-pole grid connections from range limits
  - Uses precise 3D distance calculations matching actual wire length

- **Configuration Integration** - Full control over power auto-connect behavior
  - PowerConnectRange setting filters building connections
  - PowerGridAxis modes for different grid strategies
  - PowerReserved limits prevent pole overloading
  - All settings update in real-time without restart

### Fixed
- **Config Updates** - Auto-connect settings now load from fresh config when equipping holograms
  - PowerConnectRange changes take effect immediately without restarting
  - Belt/Pipe/Power auto-connect toggles respect config changes mid-session
  - Added logging to show current settings when hologram is equipped

- **Pipe Junction Affordability** - Pipe junctions now check player inventory for pipe materials
  - Added pipe costs to GetCost hook for vanilla ValidatePlacementAndCost
  - Full grid aggregation of pipe costs across all junctions
  - Builds are invalidated if player lacks sufficient pipes

### Technical
- Grid-relative axis detection using dot products for proper pole-to-pole connections
- 3D distance calculations matching actual wire preview length
- Power pole affordability validation with wire cost aggregation
- Wire preview system with accurate cost calculations

---

## [23.1.0] - 2025-11-22

### 🚀 Added - Pipe Auto-Connect

#### Pipe Auto-Connect System (NEW!)
- **Automatic Pipe Connections** - Pipeline junctions now automatically create pipe connections to nearby buildings
  - Junction-to-building connections for factory inputs/outputs
  - Junction-to-junction manifolds for chaining multiple junctions
  - Smart spacing adapts to building dimensions
  - Respects pipe tier settings (Mk.1/Mk.2 or Auto mode)
  - Works with pipe indicator toggle (on/off)
  - Automatic cost calculation and player charging
  - Vanilla-quality curved splines matching Satisfactory's appearance

- **Validation & Alignment** - Smart connection selection
  - 35° maximum connection angle (relaxed from 30°)
  - 25m connection range for pipes
  - Alignment-weighted scoring prevents crossed pipes
  - Handles buildings with offset inputs (e.g., Quantum Encoder)
  - Parent-child type constraints for manifold arrays

### Fixed
- **Recipe HUD validation** - HUD no longer shows incompatible copied recipes when looking at different building types (e.g., Constructor recipe won't display when viewing a Smelter)
- **Frame Foundation height** - Corrected to 400cm (4m) for proper scaling calculations
- **Belt preview crash** - Fixed Access Violation in StoreBeltPreviews due to map reference invalidation
- **Ramp stepping direction** - Ramps now correctly descend when using negative X/Y scaling instead of incorrectly ascending

### Improved
- **Spline curve quality** - Updated pipe and belt splines to use vanilla's 6-point structure
  - Flat sections near connectors (50cm tangents)
  - Smooth transitions (147cm from connectors)
  - Natural curves in middle section (919cm tangents)
  - Matches vanilla Satisfactory's appearance exactly
- **Validation parity** - Belt and pipe validation now use consistent parameters (35° angle, 25m range)

### Technical
- Implemented dual-spline system for pipes (2-point preview, 6-point build)
- Added FSFPipeAutoConnectManager for pipe coordination
- Created FSFPipeConnectorFinder for connector discovery
- Implemented FPipePreviewHelper for preview lifecycle management
- Added ASFPipelineHologram custom hologram class
- Fixed multiple compilation and linker errors
- Added SFSplineAnalyzer debug tool for vanilla spline research

---

## [23.0.1] - 2025-11-19

### Fixed
- **Scaling HUD axis colors** - The X/Y/Z headers in the scaling HUD now use the same colors as the world-space axis arrows, improving visual clarity and consistency.
- **Auto-Connect HUD key hints** - The Auto-Connect HUD help text no longer hard-codes Num0 / Num8 / Num5 for navigation. It now respects the players current keybinds or omits explicit key names when they do not apply, so the HUD never lies about the controls.

### Improved
- **Ramp default X stepping** - Ramps now have a sensible default step distance on the X axis that is derived from their ramp Z offset, making ramp placement feel better out of the box with less manual tweaking.

---

## [23.0.0] - 2025-11-18

### 🚀 Phase 2: Belt Auto-Connect (First Release)

Smart! v23.0.0 introduces the first part of Phase 2 - automatic belt connections between buildings in your scaled grids.

### Added

#### Belt Auto-Connect System
- **Automatic Belt Connections** - Belts now automatically connect distributors to production buildings
  - Works seamlessly with Phase 1 scaling layouts
  - Connects splitters and mergers to nearby factories, smelters, assemblers, etc.
  - Creates manifold chains for even distribution across production lines
  - Handles complex multi-layer distributor grids with child branches
  - Creates visually appealing layouts that you can customize to your preference

- **Per-Player Settings** - Configure auto-connect behavior to match your playstyle
  - Press **U** to open settings mode
  - Toggle distributor connections (splitter/merger manifolds)
  - Toggle building connections (distributor to production buildings)
  - Settings persist across game sessions
  - Visual HUD shows current configuration

- **Belt Tier Selection** - Control which belt tier is used for connections
  - Auto: Uses highest unlocked tier (respects game progression)
  - Manual: Select specific tier from Mk.1 through Mk.6
  - Setting updates all previews and built belts immediately

- **Cost Accounting** - Full material cost integration
  - Charges materials correctly for belts and distributors
  - Belt costs shown in HUD before confirmation
  - Costs display in build preview

### Improved
- **Context-Aware Spacing** - Grid spacing automatically adjusts based on building width for better layouts
- **Stagger Axis Priority** - ZX stagger now default for better auto-connect compatibility

### Technical
- Custom belt hologram system for accurate cost calculation and preview
- Orchestrator pattern for managing complex multi-building connection logic
- Comprehensive documentation for future pipe/power auto-connect implementation

### Coming in v23.1.x+
- **Pipe Auto-Connect** - Automatic fluid connections for refineries, packagers, and other liquid-handling buildings
- **Additional QoL** - Further improvements based on community feedback

---

## [22.3.1] - 2025-11-06

### Fixed
- **Blueprint Recipe Preservation** - Blueprints now retain their preloaded recipes after placement
  - Added blueprint proxy detection to prevent Smart from interfering with blueprint buildings
  - Fixed issue where Smart would clear recipes on buildings spawned from blueprints
  - Implemented temporal proximity detection with 0.3-second window to minimize false positives
  - Added UFUNCTION() macro for proper timer delegate binding

### Technical
- Enhanced OnActorSpawned with blueprint detection logic using class name string matching
- Added ClearBlueprintProxyFlag function with automatic timer-based cleanup
- Improved blueprint hologram handling with Unsupported(Blueprint) adapters

---

## [22.3.0] - 2025-11-05

### Added
- **Recipe Selector System** - Hold U + Num8/5 to cycle through building recipes
  - Complete recipe management with unified state tracking
  - Enhanced HUD display shows inputs/outputs with quantities
  - Recipe counter displays position (e.g., "Recipe 3/14")
  - Supports all building types with automatic recipe discovery
  - Full Enhanced Input integration with modal state
  - User-controlled recipe persistence across build gun unequip

### Fixed
- **Issue #174** - Resolved inconsistent recipe application between parent and children
  - Unified recipe state management eliminates parent/children mismatch
  - Complete recipe clearing now works with U + Num0 for all buildings

### Technical
- Implemented unified recipe state with ERecipeSource enum (None, Copied, ManuallySelected)
- Added comprehensive hologram registry integration for recipe synchronization
- Enhanced cache initialization strategy with multiple fallback points
- Improved debug logging system for troubleshooting recipe operations

---

## [22.2.1] - 2025-11-04

### Added
- **Recipe Copying** - First public release of recipe copying feature for Smart! scaling
  - Recipes from holograms are now automatically copied to spawned buildings
  - Works reliably even in large grids (tested with 440+ constructors)
  - Automatic retry logic ensures buildings are ready before recipe application

### Fixed
- **Recipe Copying Timing** - Resolved HasActorBegunPlay() timing conflict in recipe lookup
  - Fixed timing issue preventing recipes from being copied to spawned buildings
  - Enhanced weak pointer validation for deterministic race condition safety

### Technical
- Removed premature HasActorBegunPlay() check from FindRecipeForSpawnedBuilding
- Enhanced weak pointer validation for deterministic race condition safety
- Implemented 0.5s retry delay in ApplyRecipeDelayed for building readiness
- Cleaned up debug logging added during investigation

---

## [22.2.0] - 2025-11-03

### Added
- **Custom Hologram System** - Complete overhaul to use custom Smart hologram classes exclusively
  - New ASFFoundationHologram, ASFFactoryHologram, ASFLogisticsHologram classes
  - Runtime hologram swapping to replace vanilla holograms with custom ones
  - Deferred construction pattern to prevent build class assertion failures
  - Smart adapter system with FSFSmartBuildableAdapter and FSFSmartFactoryAdapter
  - Full Smart! feature integration in custom holograms (scaling, spacing, arrows, widgets)

### Fixed
- **Recipe Inheritance** - Fixed child holograms not inheriting parent's stored recipes
  - Child holograms now correctly copy stored recipes from parent holograms
  - Constructed buildings now apply inherited recipes instead of default recipes
  - Resolved timing issue where OnActorSpawned fired before building initialization
  - Added delayed recipe application (0.1s) to ensure buildings are fully initialized
  - Fixed storage location mismatch between SFSubsystem and hologram data structures

### Improved
- **Arrow Configuration** - Added setting to control arrow visibility on startup
  - New configuration option in Smart_Config.uasset for arrow display preferences
  - Users can now choose whether arrows appear automatically when holograms are created

### Technical
- Implemented comprehensive custom hologram architecture with 631-line integration plan
- Added property copying system for seamless vanilla-to-custom hologram transition
- Created adapter assignment logic for different hologram types (foundations, factories, logistics)
- Added comprehensive verification logging for recipe application debugging
- Implemented IsValid() safety checks to prevent crashes when buildings are destroyed
- Fixed API usage: GetCurrentRecipe() method name and timer API calls
- Updated child hologram spawning to use correct recipe storage location
- Performance optimized: custom hologram creation < 1ms, total swap time 1-2ms

---

## [22.1.2] - 2025-10-26

### Improved
- **Documentation Structure** - Comprehensive reorganization and standardization of all documentation files
  - Implemented type-based naming convention (ARCH_, FEAT_, IMPL_, RESEARCH_, REF_, PROC_, PLAN_, ANALYSIS_, CONFIG_, STYLE_, HIST_)
  - Consolidated Auto-Connect documentation from 5 redundant files to 2 authoritative sources
  - Added mandatory YAML frontmatter to all documentation files for automated discovery
  - Reorganized 43+ files across standardized folder hierarchy (Architecture, Features/*, Reference, Deployment, Archive)
  - Updated cross-references throughout documentation set

### Technical
- Removed 5 empty legacy folders (Development, Configuration, Design, Features/Configuration, Features/Levitation)
- Renamed Twirl feature to Stagger throughout documentation
- Archived historical documentation to Archive/ folder
- Updated REF_Documentation_Index.md with current code paths and type-based naming

---

## [22.1.1] - 2025-10-25

### Fixed
- **Arrow visibility on non-scalable buildings** - Scaling arrows no longer appear when building belts, pipes, or extractors (Issue #164)
  - Arrows now respect the `bSupportsScaling` flag from `SFBuildableSizeRegistry`
  - Non-scalable buildings (belts, extractors, etc.) correctly skip arrow attachment
  - Enhanced logging shows scalability status for all buildings

### Technical
- Added `bBuildingSupportsScaling` flag check in `RegisterActiveHologram()`
- Arrow attachment gated by building scalability profile
- Updated logging to display scalability status for all building types

---

## [22.1.0] - 2025-10-25

### Added
- **Configuration Menu System** - New SML-based in-game settings menu (Task 69)
  - Accessible from main menu and pause menu under "Mods" → "Smart!"
  - Persistent settings saved to game configuration
  - Allows users to customize scaling behavior, HUD visibility, and arrow display

### Fixed
- **Arrow visibility persistence** - XYZ crosshair visibility state now persists across game restarts (Issue #146)
  - Config menu checkbox controls default arrow visibility across sessions
  - Num1 key provides runtime toggle without persisting to config (temporary override)
  - Arrow module always initializes for toggle support regardless of config setting

### Improved
- **HUD visibility toggle** - Show/hide the scaling counter HUD overlay
  - Configurable via settings menu checkbox
  - Setting persists across game sessions
- **HUD scale configuration** - HUD scale now configurable via settings menu (Issue #154)
  - Adjustable from 0.5x to 2.0x in-game settings
  - Changes take effect immediately without requiring game restart
  - Applies to all counter display text and UI elements

### Technical
- Implemented SML ModConfiguration system with `Smart_Config` blueprint
- Added `FSmart_ConfigStruct` for runtime config access
- Deferred config loading until first hologram registration for safe initialization
- Config values read fresh each frame for live updates (HUD scale, visibility)
- Integrated with `SFGameInstanceModule` for proper SML lifecycle management

---

## [22.0.5] - 2025-10-23

### Fixed
- **Value adjustment keybinds** - NumPad 5 key now correctly decreases X-axis scaling instead of increasing it
  - Resolved bug introduced in unified keybind system where both NumPad 8 and 5 were increasing values
  - Fixed direction calculation in `OnValueIncreased` and `OnValueDecreased` handlers
  - NumPad 8/5 behavior now matches NumPad 6/4 and 9/3 (increase/decrease pairs work correctly)

### Improved
- **HUD readability** - Counter display text increased by 50% for improved visibility (Issue #154)
  - Header text scaled from 1.5x to 2.25x baseline
  - Body text scaled from 1.2x to 1.8x baseline
  - Benefits users on high-resolution displays (1440p, 4K, ultrawide) and those requiring accessibility support
  - Future-proofed for configuration menu integration

### Technical
- Added `HUDScaleMultiplier` parameter (default 1.5x) to `SFSubsystem` for centralized HUD scaling control
- Corrected direction logic in unified value handlers to explicitly set +1 (increase) or -1 (decrease)
- Implemented value negation when `OnValueDecreased` calls scaling handlers to ensure proper direction interpretation

---

## [22.0.4] - 2025-10-22

### Fixed
- **Fast scrolling support** - Mouse wheel now properly supports high-speed input (Issue #152)
  - All input methods (NumPad keys and mouse wheel) now use cumulative input accumulation for responsive fast scrolling
  - Logitech G502 Hero and other high-speed mice can achieve "100 platforms per second" in all Smart! features

### Improved
- **Unified mouse wheel handler** - Replaced chord-based mouse wheel bindings with intelligent code-based mode detection
  - Eliminates conflicts between `Cumulative` input accumulation and `FGInputTriggerChordBinding` requirements
  - Single `IA_Smart_MouseWheel` action routes to appropriate feature based on active mode
  - Mouse wheel is only handled by Smart! when Spacing (`;`), Steps (`I`), Stagger (`Y`), or Scale modifier (`X`/`Z`) keys are held
  - Cleaner input architecture with better performance

### Technical
- Removed 4 chord-based MouseWheelAxis mappings from input mapping context
- Created unified `IA_Smart_MouseWheel` input action with `Cumulative` accumulation behavior
- Implemented `OnMouseWheelChanged` handler with mode-aware routing logic:
  - Routes to Spacing/Steps/Stagger counter adjustment when respective mode is active
  - Routes to continuous scaling when scale modifiers are held
  - Ignores input when no Smart! features are active (preserves vanilla behavior)
- Fixed scaling behavior to use raw accumulated values for smooth continuous movement instead of discrete steps

---

## [22.0.3] - 2025-10-22

### Improved
- **Unified keybind system** - Refactored input controls to use context-aware "Increase Value" and "Decrease Value" actions that intelligently route to the active feature (Scaling, Spacing, Steps, or Stagger)
- **Reduced keybinding conflicts** - Eliminated conflicts from multiple actions sharing the same physical keys by consolidating adjustment controls

### Fixed
- **Input action configuration** - Corrected player-mappable settings for new unified value adjustment actions to properly display in the controls menu

### Technical
- Replaced individual adjust handlers (OnSpacingAdjustChanged, OnStepsAdjustChanged, OnStaggerAdjustChanged) with unified OnValueIncreased/OnValueDecreased handlers
- Implemented context-aware routing based on active mode to determine which counter to adjust
- Updated input registry bindings for new IA_Smart_IncreaseValue and IA_Smart_DecreaseValue input actions

---

## [22.0.2] - 2025-10-22

Summary: Enabled two intended stackable structures to be built and scaled — Hypertube Pole Stackable and Pipeline Support Stackable.

### Fixed
- **Hypertube Pole Stackable** - Added missing registry entry with validated dimensions (100x200x200 cm via spacing test)
- **Pipeline Support Stackable** - Validated and corrected dimensions from estimates to measured values (100x200x200 cm via spacing test)
- **Stackable pole spacing** - Both stackable support poles now have correct edge-to-edge alignment when scaled

### Technical
- Added `Build_HyperPoleStackable_C` profile to Transport registry with spacing test validation (X+0, Y+100, Z+100)
- Updated `Build_PipeSupportStackable_C` profile in Logistics registry with spacing test validation (X+0, Y+100, Z+100)

---

## [22.0.1] - 2025-10-21

### 🎉 Initial Public Release - Phase 1: Scaling

Smart! returns for Satisfactory 1.1! After a complete ground-up rebuild for Unreal Engine 5, Smart! is back with Phase 1 features.

### Added

#### Grid Scaling System
- **Multi-dimensional grid scaling** - Scale foundations, buildings, and storage in X, Y, and Z dimensions
- **Arrow visualization** - Visual indicators showing which axis is being scaled
- **HUD counter display** - Real-time display of grid dimensions and current settings
- **NumPad controls** - Direct grid count adjustment via NumPad 8/5/6/4/9/3
- **Alternate scroll controls** - Hold X, Z, or X+Z with scroll wheel or NumPad 8/5 for axis-specific adjustments

#### Advanced Placement Modes
- **Spacing Mode** (`;` key) - Adjust spacing independently on X, Y, and Z axes with precise meter control
- **Steps Mode** (`I` key) - Create vertical stepping patterns for stairs and ramps on X or Y axes
- **Stagger Mode** (`Y` key) - Progressive offset patterns for diagonal layouts (X/Y) and vertical lean effects (ZX/ZY)

#### Enhanced Input System
- **Native keybind integration** - All controls accessible in Satisfactory's Options > Keybindings > Smart! Scaling Controls
- **Full customization** - All keybinds can be remapped to user preferences
- **Mode cycling** - NumPad 0 cycles through available axes in each mode

#### Supported Items
- **Foundations** - All foundation types and sizes
- **Production Buildings** - Constructors, assemblers, manufacturers, refineries, smelters, foundries, packagers, blenders, particle accelerators
- **Storage** - Storage containers (all tiers), fluid buffers, industrial fluid buffers
- **Walls** - Most wall types (standard, windows, gates)
- **Logistics** - Splitters, mergers (all tiers)
- **Power** - Power poles (all types)

### Technical

#### Engine & Dependencies
- **Unreal Engine 5.3** - Complete rebuild for UE5 compatibility
- **Satisfactory 1.1+** - Target game version 416835+
- **SML 3.11.x** - Compatible with Satisfactory Mod Loader 3.11.3 and above

#### Architecture
- Modern C++ codebase with improved performance
- Enhanced Input System integration
- Hologram subsystem for visual feedback
- Modular mode system for future expansion

### Known Limitations
- **Multiplayer** - Not currently supported, under active testing
- **Multi-step items** - Resource extractors, wall/floor holes, and some roof pieces not yet supported

---

## Coming Soon

### Phase 4: Camera (Planned - May Release as Separate Mod)
- Hologram preview from building perspective
- Planning tool for sight lines and layouts
- Screenshot mode for planned locations

---

## Development Notes

**This release represents over two years of development** to completely rebuild Smart! for Satisfactory's transition to Unreal Engine 5. The original Smart! mod (v1-v21 by Alex) achieved over 1 million downloads before becoming incompatible with the engine upgrade.

Smart! v22 is developed and maintained by **Finalomega** with guidance and permission from **Alex**, who established the original Smart! concepts.

**Development Tools:** Built with AI-assisted development using Windsurf, accessing multiple AI models for code architecture, implementation, and optimization. All implementation decisions, testing, and quality control by human developer.

---

**Support:** https://ko-fi.com/finalomega
**Discord:** https://discord.gg/SgXY4CwXYw
**Issues:** https://github.com/majormer/SmartFoundations/issues
**SMR Page:** https://ficsit.app/mod/SmartFoundations
### Phase 2: Autoconnect (Planned)
- Automatic belt and pipe connections between buildings
- Smart routing with intelligent pathfinding
- Multi-building grid support

### Phase 3: Extend (Planned)
- Building duplication with settings preserved
- Connection copying from original to duplicate
- Recipe preservation across duplicates
