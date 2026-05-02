# Changelog

All notable changes to Smart! will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> **Audience note:** This changelog is read by players, not developers. Entries should describe what the user experiences — what was broken, what it felt like, and what's better now. Class names, internal APIs, and implementation details belong in code comments or design docs, not here.

---

## [30.0.0] - 2026-05-01

### Added
- **Smart! Restore Enhanced** - Smart! now has a preset system for saving, applying, sharing, and replaying Smart Panel setups. A preset can store grid size, spacing, steps, stagger, rotation, production recipe, auto-connect settings, and restored Extend topology.
- **Restore panel in the Smart Panel** - Open the Smart Panel and use the `Presets >>` button at the top, above the grid controls, to open the Smart Restore panel. The panel includes a selected preset dropdown, a new preset name field, an editable description, a read-only created timestamp, capture checkboxes, and Save/Apply/Update/Delete/Export/Import actions.
- **Capture options** - Before saving or updating a preset, choose which parts of the current setup should be captured: Grid, Spacing, Steps, Stagger, Rotation, Recipe, and Auto-Connect. This lets you save a full factory setup or a smaller reusable adjustment.
- **Restore preset metadata** - Presets now save a description and creation timestamp. Newly saved and imported presets are selected in the dropdown immediately, so you can review or apply the preset you just created.
- **Clipboard sharing** - Presets can be exported to a compact clipboard string and imported on another save or by another player. Imported presets get a fresh local creation timestamp while keeping the shared name, description, and setup.
- **Progression-safe imports and applies** - Shared presets are checked against your current unlocks before they can be imported or applied. If a preset needs a locked building, production recipe, belt tier, pipe tier, power component, lift, splitter, merger, or other Extend component, Smart! rejects it instead of creating an illegal preview.
- **Import from Last Extend** - After building with Extend, Smart Restore can capture the last Extend layout as an editable preset draft. This includes the source building, production recipe when available, factories, belts, conveyor lifts, pipes, distributors, power poles, and the cloned connection layout.
- **Restored Extend topology replay** - Applying a preset captured from Extend replays the saved topology as the active build preview. The restored layout can be scaled from the Smart Panel, and Smart! owns that topology while the Restore session is active so normal Smart grid children are not spawned on top of it.
- **Restore HUD indicator** - When a restored Extend topology is active, the Smart HUD shows the active Restore preset name so you can tell that the preview is being driven by Smart Restore.

### How to Use Smart Restore Enhanced
- **Save a normal Smart Panel preset** - Equip the build gun, set up Smart! the way you want, open the Smart Panel, press `Presets >>`, enter a new preset name and optional description, choose the capture checkboxes, then press Save Current.
- **Apply a saved preset** - Open `Presets >>`, choose a preset from Selected Preset, then press Apply Selected. Smart! switches the build gun to the saved building when possible, restores the captured Smart Panel values, restores the saved production recipe when it still applies, and reapplies saved auto-connect settings.
- **Update an existing preset** - Select a preset, adjust your current Smart Panel setup, edit the description if needed, choose the capture checkboxes, then press Update Selected. The original created timestamp is preserved.
- **Share a preset** - Select a preset and press Export Selected to copy the preset string to your clipboard. Send that string to another player. To receive one, copy the shared string, open `Presets >>`, and press Import Clipboard. If every required building, recipe, and logistics part is unlocked, the preset is saved and selected automatically.
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
- **Mass Upgrade & Extend — In-Game Chain Actor Corruption on Dense Belt Networks** - Fixed in-game `SPLIT_CHAIN` corruption that occurred immediately after a mass belt upgrade or Scaled-Extend manifold build on dense networks — no save or reload required. After v29.2.1's teardown, `FConveyorTickGroup::ChainActor` was `nullptr` for one frame while the engine queued a next-frame rebuild. During that window the old chain actors were GC-pending but still referenced by belts' `mConveyorChainActor` back-pointers. On dense networks (200+ belts, multiple overlapping chains), a second `Factory_Tick` pass could observe these stale pointers before migration completed, producing chain actors whose segment lists claimed belts already assigned to different chains — corrupting belt item-bucket state in-game. A save captured in that same window would make the corruption persistent (chains deserialise on reload and race fresh migration), so save/reload was often where the problem first became apparent, but it was an in-game integrity failure, not a save-format issue. Root-caused via extensive live-game testing using the `SmartMCP` diagnostic mod's `repair_conveyor_chains` tool, which proved that the correct teardown is not `RemoveConveyorChainActor` + wait-a-frame but a **three-phase synchronous rebuild**: (1) resolve each affected chain to its owning `FConveyorTickGroup`; (2) call `RemoveChainActorFromConveyorGroup(TG)`, which nulls `TG->ChainActor` and every belt's `mConveyorChainActor` without `Destroy()` (the old actor becomes inert and GCs naturally, avoiding the ParallelFor race that bit 29.2.0); (3) union the cleared groups with `mConveyorGroupsPendingChainActors`, empty the pending list, then call `MigrateConveyorGroupToChainActor(TG)` synchronously for every unique group before returning. After step (3) every tick group has a valid fresh chain actor; no subsequent pass can observe a null or stale `ChainActor`. Dedup of tick groups is mandatory — calling either private API twice on the same group corrupts state. The three-phase logic lives in a new reusable service (below) so Mass Upgrade, Extend, and any future topology-changing feature all route through one audited path. Credited to elsheppo (original report) and `SmartMCP` live testing. (Issue #303)

### Added
- **`USFChainActorService` — Canonical Chain Invalidation + Rebuild Path** - New subsystem-owned service (`Services/SFChainActorService.h`) that owns the Remove-then-Migrate-synchronously pattern described above. Exposes two entry points: `InvalidateAndRebuildChains(const TSet<AFGConveyorChainActor*>&)` and `InvalidateAndRebuildForBelts(const TArray<AFGBuildableConveyorBase*>&, const TSet<AFGConveyorChainActor*>& ExtraChains)`. Both tolerate null and already-destroyed chain entries, dedupe tick groups internally, fall back gracefully when `AFGBuildableSubsystem` is unavailable, and log a single summary line per call with counts for `chains_supplied`, `groups_cleared`, `groups_migrated`, and `orphan_skipped`. `Mass Upgrade`'s `SFUpgradeExecutionService::CompleteUpgrade` and `Extend`'s `FSFWiringManifest::CreateChainActors` both now delegate chain teardown to this service; the embedded "HISTORY — do not regress" comment that lived in `CompleteUpgrade` since 29.2.1 has moved into the service header alongside the full crash postmortem (chain->Destroy race, belt->SetConveyorChainActor(nullptr) ghost-actor race, RemoveConveyor/AddConveyor bucket-corruption race, and the new save-timing window closure). The service is granted friend access to `AFGBuildableSubsystem` via `Config/AccessTransformers.ini` for the private `RemoveChainActorFromConveyorGroup`, `MigrateConveyorGroupToChainActor`, `mConveyorTickGroup`, and `mConveyorGroupsPendingChainActors` symbols.

### Technical
- **`Config/AccessTransformers.ini`** - Added a single `Friend` entry granting `USFChainActorService` full access to `AFGBuildableSubsystem`. No new `Accessor` entries are required — friendship reaches every private field and method the service needs.
- **Call-site simplification.** `SFUpgradeExecutionService::CompleteUpgrade` (~line 1365) and `FSFWiringManifest::CreateChainActors` (~line 874) are each ~25-30 lines shorter; both now contain at most one chain-collection pass followed by a single call to the service. The belt-and-neighbour chain walk has been moved into `USFChainActorService::InvalidateAndRebuildForBelts` so it is no longer duplicated.
- **Repair-button groundwork.** With the shared service in place, a future UI entry point that runs the same repair over arbitrary player-indicated chains (mirroring `SmartMCP`'s `repair_conveyor_chains` tool for non-developer users) is a thin Blueprint wrapper; deferred to a later release.

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
