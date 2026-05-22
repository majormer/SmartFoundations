---
title: Smart Multiplayer Support Matrix
type: PLAN
date: 2026-05-20
status: Exploratory
category: Features
tags: [multiplayer, network, rco, replication, authority, planning]
related:
  - ../Scaling/IMPL_Scaling_CurrentFlow.md
  - ../AutoConnect/IMPL_AutoConnect_CurrentFlow.md
  - ../Extend/IMPL_Extend_CurrentFlow.md
  - ../SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md
  - ../SmartRestore/PLAN_SmartRestore_Enhanced.md
---

# Smart Multiplayer Support Matrix

## Purpose

This document is a planning artifact for the `codex/multiplayer-support-exercise` branch. It is not a release promise. The goal is to make Smart!'s multiplayer risk visible by separating local preview behavior, server-authoritative build behavior, replication needs, and feature-specific hazards.

Smart! is still primarily single-player. Current public docs describe multiplayer as under active testing with partial success, not fully supported.

## Version Planning Note

Satisfactory 1.2 is expected on June 2, 2026, less than two weeks after this document was created. That release is expected to move Smart! onto a new game version and Unreal Engine version. Multiplayer work should therefore avoid broad API-coupled rewrites until the 1.2/SML/FactoryGame headers are available.

Near-term multiplayer work should favor:

- Documenting current authority and replication assumptions.
- Isolating network boundaries behind small helpers or services.
- Avoiding large hologram or construction rewrites that may be invalidated by the engine update.
- Capturing test cases that can be replayed after the 1.2 port.

## SML Multiplayer Guidance

The official SML multiplayer docs and the indexed SmartVectorEvolved copy agree on these rules:

- Code written correctly for multiplayer should also work in single-player; avoid separate single-player-only logic paths where a normal server-authoritative path is possible.
- Client-to-server actions should go through Remote Call Objects when the caller needs an owned network object for RPCs.
- SML's Remote Call Object Registry registers RCO classes for use by player controllers.
- RCOs are spawned per player controller and are network-owned by that controller, which is why they are useful for client-to-server RPCs.
- RCOs are not a good home for multicast behavior; replicated mod subsystems are the recommended place for multicast RPCs because they exist for connected players.
- Some RCO lookup paths can return `nullptr`, especially before registration is complete, so client code needs robust fallback/logging.
- Replicated state should use normal Unreal replication patterns where possible, including conditional property replication when it fits the data shape.

Reference sources:

- Official SML docs: [Development/Satisfactory/Multiplayer](https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/Multiplayer.html)
- Official SML docs: [Development/ModLoader/Registry](https://docs.ficsit.app/satisfactory-modding/latest/Development/ModLoader/Registry.html)
- Official SML docs: [Development/Satisfactory/ConditionalPropertyReplication](https://docs.ficsit.app/satisfactory-modding/latest/Development/Satisfactory/ConditionalPropertyReplication.html)
- SmartVectorEvolved cached docs: `L:\Personal\Repos\smartvectorevolved\cache\ficsit_docs_repo\modules\ROOT\pages\Development\Satisfactory\Multiplayer.adoc`

## Existing Smart Multiplayer Surfaces

| Area | Current state | Notes |
|------|---------------|-------|
| RCO | `USFRCO` exists and is registered in module startup | Covers scaling, spacing, arrows, and upgrade audit requests. |
| Network helper | `FSFNetworkHelper` exists | Mostly a policy helper; current usage appears limited. |
| Upgrade audit | Has client request and server result injection path | Needs review for per-player result routing and execution authority. |
| Scaling RPC handlers | Present in `USFSubsystem` | Current handlers still have TODOs for replication to other clients. |
| Arrow state | Explicitly described as local/non-replicated in code comments | Likely should remain local preview state unless multiplayer UX requires shared previews. |
| Hologram data | Some backup state exists because spline data can be cleared by replication | This is an existing warning sign around spline hologram networking. |

## Design Rules for This Branch

1. Local input and UI stay local. They may request server work, but they should not directly mutate authoritative world state on clients.
2. Preview-only visuals can remain local unless shared previews become a deliberate feature.
3. Build commits must be server-authoritative. Client-originated build requests need validation for ownership, unlocks, recipe availability, cost, distance, and active hologram context.
4. Direct `SpawnActor` paths that create buildables, wires, belts, pipes, or proxies need explicit authority review.
5. Multicast or shared replicated state should live on a replicated subsystem or other appropriate replicated actor, not on `USFRCO`.
6. Feature behavior should degrade clearly when multiplayer support is incomplete: disable unsafe actions, keep local preview, and log actionable diagnostics at `Log` or `Display` for active troubleshooting.
7. New diagnostics must not use `VeryVerbose` for troubleshooting that needs to appear in game.

## Feature Matrix

| Feature | Local preview only | Server-authoritative build | Needs RCO | Replication/shared state | Risk | First pass |
|---------|--------------------|----------------------------|-----------|--------------------------|------|------------|
| Input binding | Yes | No | No | No | Medium | Ensure only local controllers bind Smart input; avoid `GetFirstPlayerController` where the owning player is known. |
| HUD and SmartPanel | Yes | No | Partial | No | Medium | Keep widgets local; use RCO only for server work such as audit/execute. |
| Scaling grid | Yes | Yes | Yes for client scale changes | Possibly for shared preview, not required for build result | High | Treat child hologram grid as local preview, then validate server build commit. |
| Spacing, Steps, Stagger, Rotation | Yes | Yes | Yes when changing build-affecting counters on client | Usually no | High | Bundle transform/counter state with any server build request. |
| Levitation, Nudge, Hold | Yes | Yes | Likely yes | Usually no | High | Audit whether these mutate only active hologram preview or persist into final build transforms. |
| Arrows | Yes | No | Probably no for build correctness | No unless shared previews are desired | Low | Keep local-only; do not make arrows a multiplayer blocker. |
| Recipe selection | Yes | Yes | Yes when selected recipe affects build | No | Medium | Server must revalidate recipe availability/unlocks before construction. |
| AutoConnect belts | Yes | Yes | Probably via build commit state | Built actors replicate normally if constructed correctly | High | Audit every direct belt spawn/wiring path and prefer vanilla child construction where possible. |
| AutoConnect pipes | Yes | Yes | Probably via build commit state | Pipe networks must stabilize server-side | High | Ensure pipe network merge/rebuild runs on server only after built components exist. |
| AutoConnect power | Yes | Yes | Probably via build commit state | Power connections must stabilize server-side | High | Audit direct wire spawning and pole capacity reservations for authority. |
| Extend | Yes | Yes | Yes for client-originated clone request | Built topology should replicate through vanilla actors | Very High | Make topology capture local/server split explicit; post-build wiring must remain server-authoritative. |
| SmartUpgrade audit | UI local, scan server for clients | No for audit | Already yes | Result returned to requesting client | Medium | Replace broad RCO actor search with a reliable per-player RCO/subsystem route if needed. |
| SmartUpgrade execution | No | Yes | Yes for client execution request | Built replacements replicate through vanilla actors | Very High | Server validates unlocks, costs, target set, authority, and executes all actor replacement. |
| SmartRestore | UI local | Maybe, when applying build-affecting state | Maybe | No | Medium | Keep preset storage local; server only needs final build-affecting values. |
| SmartDismantle | Mostly vanilla | Vanilla/server | Unknown | Blueprint proxy replication | Medium | Verify blueprint proxy grouping is created server-side and survives client dismantle flows. |
| Radar/diagnostics | Local | No, unless querying server-only actors | No | No | Low | Use as investigation aid, not release behavior. |

## Cross-Cutting Audit Checklist

For each feature path, answer these before implementation:

- Who owns the initiating player controller?
- Does the code use `GetFirstPlayerController`, and is that wrong in client/listen-server contexts?
- Does the path mutate a client-only hologram, an authoritative server actor, or both?
- Is final construction happening through vanilla `Construct()`/child hologram paths or direct `SpawnActor`?
- If direct `SpawnActor` is used, does it run only on the server and use the correct buildable systems afterward?
- Who pays the cost, and is that player validated on the server?
- Are unlocks, recipes, tiers, and build distances revalidated on the server?
- Are connection components ready before wiring runs?
- Are conveyor chain actors and pipe networks rebuilt server-side after topology changes?
- Is any state meant to be visible to other players, or is it intentionally local preview state?

## Candidate Implementation Slices

### Slice 1: Inventory and Guardrails

Create an authority audit list for direct `SpawnActor`, manual `Construct()`, RCO lookup, `GetFirstPlayerController`, and build-cost paths. Add temporary `Log` diagnostics around selected multiplayer entry points while testing. Do not refactor construction yet.

### Slice 2: Per-Player Request Context

Introduce a small request context type for multiplayer-sensitive operations:

- Requesting `AFGPlayerController`
- Active hologram or build recipe/class
- Counter/transform state
- Feature mode flags
- Client-proposed target actors/components where applicable

The server should treat this as proposed input and revalidate.

### Slice 3: Scaling Baseline

Pick the smallest supported buildable family, likely foundations or simple factory buildings, and make client-originated scaling build correctly in listen-server multiplayer. Defer AutoConnect, Extend, Upgrade, and shared previews.

### Slice 4: AutoConnect Server Commit

Audit belt, pipe, and power preview generation separately from build commit. Keep preview local, then serialize only the minimal connection intent needed for server-side validation/build.

### Slice 5: Extend and Upgrade

Handle these last. They are the most valuable but also the most likely to hide authority, cost, connection, and ordering bugs.

## Open Questions

- Does Satisfactory 1.2 change hologram construction, build gun ownership, RCO registration, or replicated subsystem behavior?
- Should multiplayer support target listen servers first, dedicated servers later, or both from the start?
- Should other players see Smart! preview grids, or only the final vanilla-built actors?
- Is it acceptable for some features to be host-only while client scaling/building is developed?
- Should Smart! expose an explicit "multiplayer experimental" toggle to disable high-risk features until tested?

## Initial Recommendation

Treat multiplayer support as a staged compatibility exercise:

1. Make local preview safe.
2. Make one simple scaled build server-authoritative.
3. Add validated request context.
4. Expand to AutoConnect.
5. Leave Extend and SmartUpgrade until the base construction path is proven after Satisfactory 1.2.
