---
title: Satisfactory 1.2 Response Checklist
type: PLAN
date: 2026-05-22
status: Tracking
category: Reference
tags: [satisfactory-1.2, roadmap, compatibility, planning]
related:
  - ../Features/Multiplayer/PLAN_MultiplayerSupport_Matrix.md
  - ../Features/SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md
  - ../Features/AutoConnect/IMPL_AutoConnect_CurrentFlow.md
  - ../Features/SmartDismantle/IMPL_SmartDismantle_CurrentFlow.md
---

# Satisfactory 1.2 Response Checklist

This is a planning list for Smart!'s response to Satisfactory 1.2. It is not a release promise. The first goal after SML and FactoryGame headers are available is to regain current Smart! behavior on the new game/API baseline, then evaluate which 1.2 features deserve Smart-specific support.

Multiplayer support remains a separate roadmap item and should be sequenced after the 1.2 port baseline is stable. Better Blueprint Designer support is also a major roadmap item: current open issues indicate Smart-placed items may not appear correctly inside Blueprint machines, as if the placed actors are invisible or not registered in the designer's expected structure.

## Scope Notes

- Smart! does not currently support train tracks or vehicle paths.
- Train track layout support sounds valuable, but it should be scoped as a future feature area rather than part of the immediate 1.2 response.
- Vehicle path support would also be net-new Smart scope and should be deferred unless the 1.2 port reveals unavoidable compatibility work.
- Some items below are validation tasks rather than expected code changes. The point is to test them deliberately so we do not miss subtle breakage.

## Checklist

| # | Area | Action | Priority | Notes |
|---|------|--------|----------|-------|
| 1 | UE/SML port | Audit and port Smart! for Satisfactory 1.2, Unreal Engine 5.6.1, SML, and FactoryGame API changes. | Critical | Tracked by [#316](https://github.com/majormer/SmartFoundations/issues/316). Start with compile/API compatibility: subsystem signatures, hologram lifecycle, Enhanced Input, widgets, buildable APIs, access transformers, and module startup. |
| 2 | Conveyor chains | Re-audit conveyor chain actor handling against the new FactoryGame APIs. | Critical | Tracked by [#317](https://github.com/majormer/SmartFoundations/issues/317). Check whether `USFChainActorService` can replace any friend-access or workaround logic with supported APIs. Preserve the centralized service boundary. |
| 3 | Buildable registry | Add or validate size/profile entries for new and changed 1.2 buildables. | High | Tracked by [#318](https://github.com/majormer/SmartFoundations/issues/318). Include SPWN, Fluid Truck Station, Pipeline T-Junction, Cross Beam, and any other new/changed buildables visible in the 1.2 content data. |
| 4 | Pipe auto-connect | Validate pipe auto-connect with Fluid Truck Stations. | High | Tracked by [#319](https://github.com/majormer/SmartFoundations/issues/319). Confirm load/unload pipe orientation, endpoint discovery, connection validation, and whether station connections behave like normal pipeline endpoints. |
| 5 | Pipeline T-Junction | Add Pipeline T-Junction support where appropriate. | High | Tracked by [#320](https://github.com/majormer/SmartFoundations/issues/320). Related: [#291](https://github.com/majormer/SmartFoundations/issues/291), [#292](https://github.com/majormer/SmartFoundations/issues/292), [#293](https://github.com/majormer/SmartFoundations/issues/293). |
| 6 | Power auto-connect | Audit Smart power auto-connect for 1.2 daisy-chain power connectors. | High | Tracked by [#321](https://github.com/majormer/SmartFoundations/issues/321). Buildings may support two power connections after unlock. Avoid over-wiring and preserve vanilla power-grid behavior. |
| 7 | Power unlocks | Detect and respect the Upgraded Power Connectors unlock before offering daisy-chain behavior. | High | Tracked by [#322](https://github.com/majormer/SmartFoundations/issues/322). Smart should not enable behavior the player could not perform manually in vanilla. |
| 8 | Costs | Update Smart Upgrade cost/refund math for 1.2 Game Mode cost multipliers. | Critical | Tracked by [#323](https://github.com/majormer/SmartFoundations/issues/323). Recipe Parts Cost Multiplier may change construction costs. Verify central storage and refund behavior still matches vanilla-neutral expectations. |
| 9 | Creative Mode | Update references and compatibility checks for Advanced Game Settings being renamed to Creative Mode. | Medium | Tracked by [#324](https://github.com/majormer/SmartFoundations/issues/324). Confirm no-build-cost and creative settings still route through the expected APIs. Update docs/UI text if present. |
| 10 | World randomization | Validate Smart! behavior in randomized worlds. | Medium | Tracked by [#325](https://github.com/majormer/SmartFoundations/issues/325). Resource node randomization and purity changes should not break extractor placement assumptions, recipe gating, or restore/extend behavior. |
| 11 | Extractors | Test Water Extractor and Oil Extractor scaling/placement in shallow-water extraction scenarios. | Medium | Tracked by [#326](https://github.com/majormer/SmartFoundations/issues/326). This may not need code changes, but it should be verified because extractor placement rules changed in 1.2. |
| 12 | Hologram rotation | Test Smart axes, arrows, player-relative controls, and Extend behavior against 1.2 hologram rotation modes. | Medium | Tracked by [#327](https://github.com/majormer/SmartFoundations/issues/327). Cover Static, Spawn Facing Player, Always Face Player, Parallel, and Perpendicular relative rotation settings. |
| 13 | Controller input | Validate Smart keybinds under dynamic gamepad swap and rebindable controller settings. | Medium | Tracked by [#328](https://github.com/majormer/SmartFoundations/issues/328). Related: [#177](https://github.com/majormer/SmartFoundations/issues/177), [#209](https://github.com/majormer/SmartFoundations/issues/209), [#213](https://github.com/majormer/SmartFoundations/issues/213), [#217](https://github.com/majormer/SmartFoundations/issues/217), [#286](https://github.com/majormer/SmartFoundations/issues/286). |
| 14 | HUD prompts | Audit Smart HUD/input prompts while using controller mode. | Low | Tracked by [#329](https://github.com/majormer/SmartFoundations/issues/329). If SML/FactoryGame exposes current input method, prefer showing prompts that match the active input device. |
| 15 | Signs | Re-test scalable signs and billboards with 1.2 sign zooping. | Medium | Tracked by [#330](https://github.com/majormer/SmartFoundations/issues/330). Related: [#296](https://github.com/majormer/SmartFoundations/issues/296). |
| 16 | Blueprints | Investigate Blueprint Designer registration for Smart-placed actors. | Critical | Tracked by [#331](https://github.com/majormer/SmartFoundations/issues/331). Related: [#168](https://github.com/majormer/SmartFoundations/issues/168), [#312](https://github.com/majormer/SmartFoundations/issues/312). |

## GitHub Tracking

- Milestone: [Satisfactory 1.2](https://github.com/majormer/SmartFoundations/milestone/1)
- Label: [`satisfactory-1.2`](https://github.com/majormer/SmartFoundations/issues?q=is%3Aissue%20label%3Asatisfactory-1.2)
- UI/widget validation overlap: [#146](https://github.com/majormer/SmartFoundations/issues/146), [#230](https://github.com/majormer/SmartFoundations/issues/230), [#285](https://github.com/majormer/SmartFoundations/issues/285)
- Logistics validation overlap: [#193](https://github.com/majormer/SmartFoundations/issues/193)
- Port/refactor overlap: [#224](https://github.com/majormer/SmartFoundations/issues/224)
- Multiplayer/dedicated server overlap: [#176](https://github.com/majormer/SmartFoundations/issues/176), [#309](https://github.com/majormer/SmartFoundations/issues/309)
- Extend validation overlap: [#310](https://github.com/majormer/SmartFoundations/issues/310)

## Deferred Feature Ideas

| Area | Reason to defer |
|------|-----------------|
| Train track layout | New Smart feature area. Potentially high value, but it needs its own design pass for splines, signals, snapping, costs, validation, and vanilla-neutral constraints. |
| Vehicle path layout | New Smart feature area. 1.2 vehicle paths are build-gun placeable and spline-like, but Smart does not currently operate on them. Defer unless compatibility requires explicit exclusion/guarding. |
| Full multiplayer support | Already on the roadmap. Port and stabilize the 1.2 baseline first, then resume multiplayer work from the existing matrix. |

## Source Context

This list was drafted from the Satisfactory 1.2 experimental patch notes and follow-up release discussion available on 2026-05-22. Key 1.2 changes considered here include Unreal Engine 5.6.1, new Game Modes and cost multipliers, resource randomization, Fluid Truck Stations, Pipeline T-Junctions, daisy-chain power connectors, Creative Mode naming, hologram rotation options, controller/input changes, sign zooping, and the June 2, 2026 stable release target discussed by Coffee Stain.
