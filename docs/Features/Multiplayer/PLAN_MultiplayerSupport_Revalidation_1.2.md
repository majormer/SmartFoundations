---
title: Smart Multiplayer Support — 1.2 Re-Validation + First Slice
type: PLAN
date: 2026-06-08
status: Active
category: Features
supersedes_assumptions_in: ./PLAN_MultiplayerSupport_Matrix.md
related:
  - ./PLAN_MultiplayerSupport_Matrix.md
  - ../Scaling/IMPL_Scaling_CurrentFlow.md
  - ../AutoConnect/IMPL_AutoConnect_CurrentFlow.md
  - ../Extend/IMPL_Extend_CurrentFlow.md
  - ../SmartUpgrade/IMPL_SmartUpgrade_CurrentFlow.md
  - ../SmartDismantle/IMPL_SmartDismantle_CurrentFlow.md
issues:
  - 176  # UMBRELLA — multiplayer / dedicated server support (other MP issues link here)
  - 309  # Multiplayer support — Extend with friends (links to #176)
  - 334  # Power-wire + belt auto-connect broken across client (first concrete bug; links to #176)
---

# Smart Multiplayer Support — 1.2 Re-Validation + First Slice

## Why this doc exists

The original risk matrix (`PLAN_MultiplayerSupport_Matrix.md`) was written **2026-05-20**, before the
1.2/CL-83 port and before the belt work (#341 chain-actor unification, #354 standard pole auto-connect).
The 1.2 "defer broad rewrites" hold it imposed is now **lifted**. This doc re-validates every feature's
authority story against the **current** code, folds the new belt/pole paths into the matrix, and defines a
**narrow, testable first slice**.

The original matrix's *structure and design rules remain correct* — what changed is that we now have
concrete file:line authority hazards and a confirmed root cause for the first bug (#334). Where this doc and
the old matrix disagree, **this doc wins**.

---

## Headline conclusions

1. **Scaling is genuinely MP-safe by construction** and is the correct first slice. Scaled placement commits
   purely through vanilla parent→child `Construct()` / `AddChild()` cost aggregation — **no direct
   `SpawnActor` of buildables, no manual cost charging** in the scaling path. The only scaling RCO gaps are
   *cosmetic state echo* to other clients (TODO #21), not build correctness.

2. **#334 has a confirmed, concrete root cause** and it is the cleanest first *bug* to fix after the parity
   slice: **power, belt, and pipe auto-connect all defer their real connection wiring to post-build hooks
   (`OnActorSpawned` / parent `Construct`) that run on every peer with no authority guard.** On a client
   those hooks spawn/wire **client-only** actors the server never sees → "built but no power / no items."

3. **The new belt (#341) and pole (#354) paths did NOT regress scaling**, but they **widened the
   auto-connect authority surface**: the matrix's old note that "only Power directly spawns actors" is now
   **stale** — stackable belt runs and pipes also commit real connections through post-build hooks.

4. **Extend and SmartUpgrade remain Very High** and stay last. Both have a `GetFirstPlayerController()`
   build-path hazard (wrong player's tier/inventory/build-gun in a client or non-host listen-server build).

---

## Confirmed authority infrastructure (current code)

| Surface | File | State |
|---|---|---|
| `USFRCO` | `Public/SFRCO.h` / `Private/SFRCO.cpp` | 5 Server RPCs: `Server_ApplyScaling`, `Server_ResetScaling`, `Server_SetSpacingMode`, `Server_ToggleArrows`, `Server_StartUpgradeAudit` (+`Server_CancelUpgradeAudit`, `Client_ReceiveAuditResult`). Registered unconditionally (`ShouldRegisterRemoteCallObject()` → true). |
| `HasHologramAuthority()` | `SFRCO.cpp:302` | **Placeholder returning true** (TODO #22) — no ownership check on the target hologram. |
| `CheckRateLimit()` | `SFRCO.cpp:318` | **Placeholder returning true.** |
| Result echo to other clients | `SFSubsystem_Config.cpp:84,102,122` | **TODO #21** — scaling/spacing/arrow results apply server-side but are not replicated to other clients (cosmetic, not build-correctness). |
| `FSFNetworkHelper` | `Public/Core/Helpers/SFNetworkHelper.h` | **Policy/detection only** — `IsMultiplayer/IsClient/IsListenServer/IsDedicatedServer/ShouldEnableFeature`. No RPC routing, no `HasAuthority`. Conservative default-deny on unknown netmode. |
| Audit per-player routing | `SFRCO.cpp:218,249` | **Correct** — `RequestingPlayer = Cast<AFGPlayerController>(GetOuter())`, result returned via `Client_ReceiveAuditResult` to the caller only. |

**`GetFirstPlayerController()` audit (18 sites):** 10 are **MP-safe** (local input, HUD, arrows, radar,
diagnostics, hologram-poll cache, pipe preview — all legitimately the local player). **8 are hazards** —
all in *tier-config resolution* or *build-gun/inventory* paths where the code wants the **requesting builder**,
not the world's first PC:

| Site | Hazard |
|---|---|
| `SFUpgradeExecutionService.cpp:670` | **Critical** — build-gun/inventory for actual upgrade construction. |
| `SFRestoreService.cpp:460,823` | **Critical** — build-gun/inventory in restore build-commit. |
| `SFExtendCloneSpawner.cpp:958` | Lane/belt tier from first PC, not builder. |
| `SFExtendWiringService_Manifold.cpp:580` | Manifold belt tier from first PC. |
| `SFAutoConnectService_Belt.cpp:987,1048` | Belt tier from first PC. |
| `SFPipeAutoConnectManager.cpp:874,1171` + `_Spawn.cpp:165,686,901` | Pipe tier from first PC. |
| `SFPowerAutoConnectManager.cpp:1799` | Cable-cost inventory from first PC. |
| `SFSubsystem.cpp:810` | Scaling RPC fallback uses world first PC instead of RCO `GetOuter()`. |

---

## Re-validated feature matrix (current code)

Columns: **Preview** = local client preview only · **Commit path** = how the real build happens ·
**Authority hazard** = the concrete MP failure · **Risk** · **Δ since 2026-05-20**.

### Scaling — Risk: LOW-MED (was High) ✅ first slice
- **Preview:** child holograms via `AddChild()`, client-side only; counters in `SFGridStateService`.
- **Commit path:** vanilla parent `Construct()` traverses children; **cost auto-aggregated by vanilla
  `GetCost()`** — DeferredCostService/RecipeCostInjector were **removed** (`SFSubsystem.cpp:74`). No direct
  `SpawnActor`, no manual `Construct()`, no manual cost charging in the scaling path.
- **Authority hazard:** only the *client→server scale-change RPC* (`Server_ApplyScaling`) and its missing
  echo to other clients (TODO #21). Build correctness is sound because the parent hologram build is already
  the vanilla server-authoritative path.
- **Δ:** matrix rated this High out of caution; current code shows the commit path is the safe vanilla path.
  #341/#354 add an *in-frame chain rebuild* only when the grid contains belt poles — irrelevant to plain
  foundation/factory scaling.

### AutoConnect — Risk: HIGH (belts/pipes), CRITICAL (power) — this is #334
Root cause is shared across all three: **real connections are wired in post-build hooks that run on every
peer with no `HasAuthority()` guard.**
- **Power (CRITICAL):** `SFPowerAutoConnectManager::OnPowerPoleBuilt` (`SFPowerAutoConnectManager.cpp:1029`)
  → direct `SpawnActor<AFGBuildableWire>` + `NewWire->Connect()` (`:1245`,`:1248`). **Zero authority guard
  in the whole manager.** Child wire holograms are cost-only (Issue #244 comment at `:1235`). On a client
  this spawns a **client-only wire** → server's pole has no connection → power never transmits. *Confirmed.*
- **Belts (HIGH):** stackable runs register their chain in-frame during the parent pole `Construct` hook
  (`SFGameInstanceModule.cpp`, the #341 path) → `USFChainActorService::InvalidateAndRebuildChains`. If this
  runs on a client (or before child belts replicate), the client's chain is incomplete → items don't move.
- **Pipes (HIGH):** post-build `SetConnection()` + `MergeNetworks()` via deferred-wiring `OnActorSpawned`
  hook (`SFPipeAutoConnectManager*`). Same peer/ordering hazard → no fluid.
- **Δ:** matrix said "Power is the exception that directly spawns actors." **Now stale** — stackable belts
  (#341) and pipes also commit real connections off the hologram-child path. The IMPL doc
  (`IMPL_AutoConnect_CurrentFlow.md:24-25`) needs the same correction.

### Extend — Risk: VERY HIGH (unchanged)
- **Preview:** clone child holograms via direct `SpawnActor<ASF…Hologram>(bDeferConstruction=true)` with
  `SetReplicates(false)` — explicitly client-only preview (`SFExtendCloneSpawner.cpp`).
- **Commit path:** built children come through vanilla `AddChild()`→`Construct()`; **but** post-build
  manifold wiring spawns **real** `AFGBuildableConveyorBelt/Lift/Wire` directly
  (`SFExtendWiringService_BuiltChild.cpp` ~470/495, `_Json.cpp` wires) and then
  `RebuildConveyorChains`/`RebuildPipeNetworks` (`SFWiringManifest.cpp:846,907`).
- **Authority hazard:** tier selection via `GetFirstPlayerController` (`:958`, manifold `:580`) → non-host
  builder gets wrong tier; post-build buildable spawns have no explicit authority guard.
- **Δ:** none structural; #341 chain path is what the manifold rebuild already calls.

### SmartUpgrade — Risk: MED (audit) / VERY HIGH (execution)
- **Audit:** RCO route is **correct per-player** (`SFRCO.cpp:218`), but the scan is a **world-wide
  `TActorIterator`** then radius-filtered (`SFUpgradeAuditService.cpp:297,323,356`) — safe (read-only) but
  broad, as the matrix flagged.
- **Execution:** server-side, no client hologram spawn; cost charged per-item server-side. **Hazards:**
  `GetFirstPlayerController()` fallback for build-gun/inventory (`SFUpgradeExecutionService.cpp:670`) and
  **no unlock/recipe revalidation at execute time** (audit-time availability is trusted).
- **Δ:** chain rebuild uses the two-phase vanilla queue (`:1608`,`:1629`), **not** changed by #341 — doc
  stays accurate.

### SmartDismantle — Risk: MED (raise to HIGH for grouping correctness)
- **Commit path:** `AFGBlueprintProxy` is spawned in `OnActorSpawned`
  (`SFSubsystem_OnActorSpawned.cpp:61`) with **no `HasAuthority()` guard**, then `SetBlueprintProxy()` /
  `RegisterBuildable()` (`:90`,`:91`). Fires on **all peers** → client and server spawn independent
  proxies, last-write-wins → split/inconsistent grouping.
- **Δ:** IMPL doc is silent on MP; add the authority caveat.

---

## Cross-cutting root-cause pattern (the thing to fix)

Smart! deliberately defers "real" connection/chain/proxy wiring to **post-build hooks** (`OnActorSpawned`,
parent `Construct`) to dodge vanilla ordering issues. That pattern is correct in single-player but
**unguarded for authority**: the hooks run on every connected peer. The MP fix is *not* a rewrite — it is to
**gate every world-mutating post-build hook behind a server-authority check and route the client's intent
through an RCO**, letting the resulting vanilla actors replicate down normally.

Concretely, the recurring fix shape:
- Add `if (!Buildable->HasAuthority()) return;` (or world-netmode guard) at the top of `OnPowerPoleBuilt`,
  the pipe deferred-wiring hook, the stackable-belt chain registration, and the dismantle proxy spawn.
- For client-originated builds, the client sends a validated request via `USFRCO`; the **server** runs the
  hook and the built wire/belt/pipe/proxy replicates to all clients through vanilla replication.
- Replace build-path `GetFirstPlayerController()` with the **requesting** PC (from the RCO `GetOuter()` or a
  threaded request context — see Slice "Per-Player Request Context" in the original matrix).

---

## First slice (narrow, testable)

### Slice 0 — Parity proof: client-originated **scaled foundation/factory placement** on a listen server

**Goal:** prove that a *client* (not the host) can place a scaled grid of plain foundations / a simple
factory building and have every cell appear, correctly costed, on **both** host and client — with **no
AutoConnect, no Extend, no Upgrade** involved. This validates the core authority pipeline (client scale
intent → server-authoritative vanilla child construct → replication) on the simplest buildable family.

**Why this first:** scaling's commit path is already the vanilla server-authoritative build path, so Slice 0
is mostly *verification*, not new construction code. It gives us a green baseline before we touch the #334
auto-connect hooks.

**Code-level state established this session (2026-06-08, static trace):**
1. **The scaling RCO is dead code on the build path.** The input path
   `USFSubsystem::ApplyAxisScaling` (`SFSubsystem.cpp:938`) mutates **local** counter state
   (`GridStateService`) and scales the **local preview hologram** directly. It **never calls**
   `USFRCO::Server_ApplyScaling` — that RPC has no caller anywhere outside `SFRCO.cpp/.h`. So scaling is a
   purely local-preview operation; closing TODO #22 (`HasHologramAuthority`) is **NOT** what Slice 0 needs.
2. **Commit happens through the vanilla build-gun fire, not a Smart RPC.** Scaled cells are vanilla
   `AFGHologram` child holograms added via `AddChild()`; the parent's vanilla `Construct()` traverses them,
   and each child (`ASFBuildableChildHologram::Construct`, `SFBuildableChildHologram.cpp:36`) is built with a
   vanilla `FNetConstructionID`. No direct `SpawnActor`, no manual cost — confirmed.
3. **Therefore the real Slice 0 question is a vanilla-replication question:** when a *client* fires the build
   gun on a scaled grid, does Satisfactory's build-gun construct path **serialize/replicate the custom SF
   child holograms to the server** so the server reconstructs all cells? Vanilla supports nested child
   holograms (blueprints, poles), but SF's children are custom `AFGHologram` subclasses — whether they
   round-trip the client→server construct message is exactly what the live test proves. This is the gate; no
   feature code to write before we have the test result.
4. Cosmetic other-client preview echo (TODO #21) is explicitly **out of scope** for parity.

**Host + client test setup (maintainer-driven build, per AGENTS.md):**
- Build the Shipping DLL via the golden path; deploy to the game dir.
- **Host:** launch Satisfactory, load a flat test save, **Open to LAN / listen server**.
- **Client:** second machine (or second Steam account / second instance) joins the listen server.
- **Test A (baseline, host):** host equips build gun, scales a 3×3×1 foundation grid, places. Expect 9
  foundations, correct total cost. (Sanity that the slice didn't regress SP/host.)
- **Test B (the actual proof, client):** **client** does the same 3×3×1 scaled placement. Expect: all 9
  foundations visible on **both** host and client; cost deducted from the **client's** inventory only;
  nothing duplicated/ghosted on either side; save → reload → still 9.
- **Test C (factory family):** repeat Test B with a simple factory building (e.g. a 2×1 of Constructors) to
  cover a non-foundation buildable.
- **Pass criteria:** host and client agree on count, position, and cost; no client-only ghost actors; clean
  save/reload. **Diagnostics:** temporary `UE_LOG(LogSmartFoundations, Display, …)` at the RPC entry, the
  authority branch, and the per-child construct — `Display`, not `VeryVerbose` (won't show in-game).

### Slice 0 — EMPIRICAL RESULT (2026-06-08, live dedicated-server test)

Run against a **claimed Windows dedicated server** (CL491125) with a game client joined via
`open 127.0.0.1`, observing the **server's authoritative state live** through the SmartMCP dedi API
(port 51096). This is Approach B — stricter than a listen server (no PC index 0 on the dedi).

**Headline: client-originated scaled placement WORKS in MP — for normal grid sizes, for both buildable
kinds.** The core Slice 0 parity question is **YES**. Above a count threshold it fails in a bounded,
reproducible, fixable way.

Data (cumulative server-side count via `nearby_buildings`; foundations confirmed visually + persist-through-
reconnect since they are lightweight instances invisible to the actor scan):
- Foundation tower (~10) + 3×3 (9) → built and persisted. ✅
- Constructors: 3 → 3; +25 → 28; +100 → **128** — every one a valid replicated actor on the correct grid. ✅
- Constructors **144 (12×12) → 0 persisted** (server stayed at 128). ✗
- Foundations **256 (16×16) → 0**; Constructors **256 → 0**; Foundations **1250 → 0**. ✗
- **All-or-nothing:** on failure, *zero* of the batch persists (not truncated).

Diagnosis (what each observation rules in/out):
- **COUNT-based, not payload-based.** Lightweight foundations (tiny per-child data) and heavy factory actors
  fail at the *same* ~100–144 threshold. A byte/bunch-size limit would let foundations go far higher.
- **NOT a preview-positioning timing bug.** Smart spawns children into `mChildren` synchronously but
  *positions* them via a progressive batch (~200/frame, ticked by `USFSubsystem::Tick()`), designed for
  single-player FPS (`SFGridSpawnerService.cpp`, `SFHologramHelperService*`). Hypothesis was that the client
  fires before the batch settles — **disproven live**: waiting ~10 s for the preview to fully settle before
  placing still failed.
- **Not a Smart hard cap.** `GRID_CHILDREN_HARD_CAP = 2000`, `LARGE_GRID_WARNING_THRESHOLD = 100`
  (`SFHologramHelperService.h`). The 100 warning matches the observed boundary but only warns.
- **Single-player is unaffected** — large grids (historically 3000+ children) build fine in SP, where the
  construct is local and never serialized. The limit is specific to the **client→server networked construct**.

**Root cause (CONFIRMED from vanilla headers, 2026-06-08):** the whole scaled grid commits as **one reliable
RPC** — `UFGBuildGunStateBuild::Server_ConstructHologram(FNetConstructionID, FConstructHologramMessage)`
(`Equipment/FGBuildGunBuild.h:208-209`, `Server, Reliable`). Every child rides inside
`FConstructHologramMessage.SerializedHologramData`, a single `TArray<uint8>` holding the entire hologram tree
serialized via `IFGConstructionMessageInterface::SerializeConstructMessage` (`FGConstructionMessageInterface.h`;
`mChildren` serialized with the parent — `Hologram/FGHologram.h:126,729`). So the construct is **inherently
all-or-nothing** (one RPC, one blob), and above ~100–144 children the serialized blob exceeds UE's
**reliable-bunch / packet size ceiling** (an *engine-level* limit, not a FactoryGame constant — which is why
it never appears in a Smart/FactoryGame grep, and the modding docs don't mention it). "Count-based vs
byte-based" reconciles: each child serializes to a roughly constant size, so the byte ceiling ≈ count ×
constant and foundations vs actors hit the same threshold (their child-hologram serialization is similar).

**Why previews orphan:** vanilla has a failure callback `Client_OnBuildableFailedConstruction(FNetConstructionID)`
(`FGBuildGunBuild.h:328`) — but it only fires if the **server processes** the message. An oversized RPC
dropped at the net layer **never reaches the server**, so the callback never fires and the client never learns
it failed → orphaned previews. The two observed cleanup modes (256 → self-clean ~1 min; 1250 → persistent
walk-through ghosts, build gun consumed) are downstream of this missing-ack path.

**Fix direction (grounded in the vanilla path above):**
1. **Chunk** client-originated scaled placement into sub-batches of ≤~64–100 children, each committed as its
   own `Server_ConstructHologram` call, so each serialized blob stays under the reliable-bunch ceiling.
   Single-player can keep the one-shot path. (Lever: Smart already owns the multi-child hologram; the chunking
   happens at the commit seam, not in `SerializeConstructMessage`.)
2. **Reconcile** orphaned previews two-pronged: hook `Client_OnBuildableFailedConstruction` for the
   server-rejected (deliverable-but-refused) case, **plus** a client-side timeout/ack for the dropped-RPC case
   where that callback never fires (the oversized-blob case). Without the timeout, the ghost cleanup stays
   broken for exactly the failure we hit.
3. **Exact constant is engine-level** (UE reliable bunch / packet size), not a FactoryGame/Smart constant — so
   the chunk size is chosen empirically (≤100 proven safe here) rather than read from a header. The modding
   docs do not cover this; the vanilla headers (`FGBuildGunBuild.h`, `FGConstructionMessageInterface.h`,
   `FGHologram.h`) are the authority.

**Significance for the matrix:** scaling's MP risk is **lower than the 2026-05-20 matrix assumed** — the
commit path is the safe vanilla server-authoritative path and it works for both lightweight and actor
buildables at normal scale. The only defect is the large-batch ceiling + preview cleanup, both bounded and
fixable. This is the first concrete MP work item (ahead of, or alongside, #334).

### After Slice 0 — Slice 1: fix #334 power (smallest real bug)
Gate `OnPowerPoleBuilt` behind authority; for client builds, route the pole-to-pole wire request through
`USFRCO` so the server spawns the `AFGBuildableWire` and it replicates. Same host+client harness, asserting
power actually transmits on the client. Belts and pipes follow with the identical pattern.

---

## Sequencing (updated)

1. **Slice 0** — client scaled placement parity (this slice).
2. **Slice 1** — #334 power wire authority + RCO route.
3. **Slice 2** — #334 belt chain + pipe network authority (same pattern).
4. **Slice 3** — per-player request context; replace build-path `GetFirstPlayerController` (8 sites).
5. **Slice 4** — Extend (#309) authority + tier-by-builder.
6. **Slice 5** — SmartUpgrade execution authority + execute-time unlock revalidation.
7. **Dismantle proxy** authority guard — fold in opportunistically (small, `SFSubsystem_OnActorSpawned.cpp:61`).
8. **#176 dedicated server** — certify after listen-server parity holds (packaging already shipped 31.0.2).

## Open questions (carried forward, now answerable)
- **Listen server first** — yes; dedicated (#176) after parity. Confirmed by the deferred-hook pattern: a
  listen server exercises both host-authority and client-request paths.
- **Do other players see preview grids?** Out of scope for parity; keep preview local (TODO #21 cosmetic).
- **Experimental MP toggle?** Worth adding so high-risk features (Extend/Upgrade) can be disabled per-session
  while Slices 0–2 stabilize — decide before Slice 4.

## Doc-hygiene follow-ups (not this session)
- Correct `IMPL_AutoConnect_CurrentFlow.md:24-25` ("Power is the only direct-spawn") — belts/pipes now also
  commit via post-build hooks.
- Add MP authority caveats to `IMPL_SmartDismantle_CurrentFlow.md` (proxy spawn) and note #341/#354 in the
  Scaling and AutoConnect current-flow docs.
