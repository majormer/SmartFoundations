---
title: Smart Multiplayer — Extend Construction Strategy
type: PLAN
date: 2026-06-09
updated: 2026-06-10
status: Implemented — Extend + Scaled Extend live-validated in MP; Restore/Upgrade remain
category: Features
branch: feature/mp-server-construction
related:
  - ./DESIGN_MP_ConstructionModel.md
  - ./PLAN_MP_ScalingConstruction_Impl.md
  - ../Extend/IMPL_Extend_CurrentFlow.md
issues:
  - 176
  - 334
  - 309
---

# Smart Multiplayer — Extend Construction Strategy

## STATUS SNAPSHOT (2026-06-10, end of session)

**Working in MP, live-validated on the dedi:** scaling (all families, real costs), #334
auto-connect (ALL conduit families incl. stackables/poles/floodlights/signs/floor holes),
Smart Dismantle groups, normal Extend (build + wiring + daisy power + dismantle), Scaled Extend
(rotated, heterogeneous blender sets, lifts in the manifold, daisy power, extend-off-built-chain,
live item flow under load — 62 chains / 917 items / 0 zombie / 0 orphan). The factory-tick crash
is RESOLVED (chain registration by type, d79e19b — see the resolved section below).

**Remaining for complete MP support (workstream rule: CANNOT SHIP PARTIAL):**
1. Extend costs in NON-CREATIVE (GetCost hook charges the staged preview-exact array — untested).
2. **Restore MP — IMPLEMENTED 2026-06-10, awaiting live test.** A restore commit rides the
   EXISTING extend-commit machinery end to end: `FSFExtendCommitSpec` gained
   `{bIsRestore, RestoreTemplate (the preset's value-only FSFCloneTopology), RestoreCounterState,
   RestoreProductionRecipe}`; `BuildCommitSpecForMP` builds the restore variant whenever the
   replay session is active (so the fire hook, GetCost hook, clearance hook, Hook B and the
   pre-stage/clear flows all work UNCHANGED); `ReconstructCommitOnServer` branches on
   `bIsRestore`: installs counter state + production recipe, then runs the SAME SP replay
   pipeline (`ReplayRestoreCloneTopology`) against the constructing parent — no source-building
   walk needed, and the GetConnection() poisoning rule doesn't apply because the connections are
   preset-sourced values. Pre-staging added to `TickRestoredCloneTopology`; clear staged on
   session teardown; RCO validation bounds the template (≤4096 children, ≤64 spline points each);
   client-side ~45KB byte guard refuses oversized templates before previews are destroyed.
   Caveats for the live test: (a) a preset whose Extend topology was SAVED on an MP client may
   carry empty CloneConnections (client-side capture poisoning at save time) — test with a preset
   saved in SP or by the host; (b) the server-side replay runs KickRestoredPreviewParent at the
   construct seam (synthetic-hit SetHologramLocationAndRotation) — watch for preview-pipeline
   re-trigger artifacts on the dedi; (c) counter state is installed into the dedi's GLOBAL
   subsystem state at the construct seam (accepted single-frame race with other players).
3. **Smart Upgrade MP** — the last feature; not yet analyzed for MP.
4. Cleanup before release: strip [MP-SLICE0]/[EXTEND-MP]/[MP-334]/[MP-SPEC] Display diagnostics +
   dedi HintBarService spam; S4 time-sliced construction queue (thousands-scale) still unbuilt.
5. SP regression sweep (maintainer-deferred until all MP features work).
6. Confirm end-blender 2/6 connections mirror its source (partial source = partial clone correct).

Key seams for whoever continues: client fire hook + server Hook A/B/C/D live in
`SFGameInstanceModule.cpp` (RegisterClientGridChunkFireHook / RegisterSpecConstructionHooks);
staging in USFRCO + USFSubsystem; Extend server derivation in `SFExtendService.cpp`
(ReconstructCommitOnServer / ReconstructScaledCommitOnServer); wiring in
`SFExtendWiringService_Json.cpp` + `SFWiringManifest.cpp`. DEBUG GOLD: the wiring pass writes
`WiringManifest.json` to the dedi's Saved/Logs every run. Deploy = BOTH targets (AGENTS.md golden
path). Memory file `multiplayer-workstream.md` carries the full debugging arc + 7 root causes.

---

How to bring Extend onto the same client/server construction model as scaling
(`DESIGN_MP_ConstructionModel.md`). Strategy only — sequenced after the scaling slice validates the
core mechanics.

## Why Extend is the harder case

Scaling places a **uniform** grid (one build class + one recipe), so its spec is tiny and the
server expansion is a single loop. Extend is **heterogeneous and relational**:

- It clones a captured subgraph: factory building(s) + their **distributors** + **belts**
  (`belt_segment`, factory↔distributor), **pipes** (`pipe_segment`), **lifts** (`lift_segment`),
  and the between-distributor **manifold lanes**.
- The clones must be **wired to each other** (auto-connect belts/pipes/power) — the inter-object
  logistics that scaling has none of. This is the part copy-placement can never do and the reason
  Extend is Smart's signature feature.
- Placement is **topology-aware**: it depends on walking the source building's connection graph.

So Extend needs (a) a richer spec, (b) a server-side topology walk, and (c) server-authoritative
**wiring** after construction.

## Two advantages Extend already has

1. **The parent is already swapped.** Extend already swaps the active hologram to
   `ASFFactoryHologram` via the proven `USFExtendService::SwapToSmartFactoryHologram`. So the
   risky "swap the plain-scaling parent + re-register" step that blocks the scaling slice **does
   not exist for Extend** — the custom parent (with `CustomSerialization` + `Construct` overrides)
   is already in place.
2. **The spec already exists in all but name.** `SFExtendCloneSpawner` already builds the clones
   from a JSON-ish description (the captured topology + per-role clone data, `FSFExtendTopology`,
   `JsonCloneId`, the role tags `belt_segment`/`pipe_segment`/`lift_segment`/lanes). That captured
   description **is** the reconstruction spec; today it is executed client-side into child
   holograms that are then serialized. The model change is to ship the description and execute it
   server-side.

## The blocker Extend must solve first (already planned)

`USFExtendTopologyService::WalkTopology` follows `UFGFactoryConnectionComponent::GetConnection()`,
but `mConnectedComponent` is `UPROPERTY(SaveGame)` — **server-only, not replicated**. On a client
`GetConnection()` is null, so the topology walk finds no chains and Extend never activates. The
already-chosen fix (unblocked by the `USFRCO` replication fix) is a **server-side topology walk via
RCO**: the client asks the server to `WalkTopology` once on activation; the server returns the
topology; the client caches it and builds the local preview. This is the front half of the model
for Extend and must land before Extend construction can be ported.

## Target flow (mirrors scaling, with topology + wiring)

1. **Activation / topology** — client aims at a building → RCO `Server_WalkExtendTopology(building)`
   → server walks the connection graph (it has the authoritative `mConnectedComponent`) → returns a
   compact `FSFExtendTopology` (distributors, chains, roles, counts). Client caches it.
2. **Preview — client-local.** Client builds the clone/belt/pipe preview from the cached topology
   exactly as today (`SFExtendCloneSpawner`), purely cosmetic.
3. **Spec.** The Extend spec = `{ cached FSFExtendTopology, clone count, direction/spacing/rotation,
   per-role plan }`. Compact relative to N serialized child holograms (it is counts + topology, not
   geometry-per-cell). Rides the already-swapped `ASFFactoryHologram` (Option A) or the RCO
   (Option B) — prefer the RCO here since activation already round-trips to the server.
4. **Server construction.** On commit, the server reconstructs from the spec:
   - construct the clone factory building(s) individually (real replicated buildables);
   - construct the per-role logistics (distributors, `belt_segment`, `pipe_segment`,
     `lift_segment`, lanes) individually via the same vanilla spawn path scaling uses
     (`SpawnChildHologramFromRecipe` per element, transforms reconstructed deterministically).
5. **Server-authoritative wiring (#334).** After the elements exist server-side, the server runs
   auto-connect / belt / pipe / power connection **behind `HasAuthority`** — the post-build hooks
   that today run on every peer get gated to the server. Connections are real and replicate.
6. **Cost / validation / failure** — as scaling: spec-based server cost (the depot-aware
   `CanAffordExtendCost` already exists), per-element failure isolation, no oversized blob.

## What to reuse vs build

| Need | Reuse | Build |
|---|---|---|
| Custom parent hologram | `ASFFactoryHologram` (already swapped for Extend) + the scaling spec hooks | An Extend spec variant (topology-bearing) |
| Topology | `FSFExtendTopology`, `WalkTopology` | `Server_WalkExtendTopology` RCO (client→server walk) |
| Per-element construction | The scaling server-expansion pattern (`SpawnChildHologramFromRecipe` + position math) | Per-role transform reconstruction from topology (vs grid counters) |
| Wiring | Existing auto-connect / chain-rebuild services | `HasAuthority` gating on the post-build hooks (#334) |
| Cost | `CanAffordExtendCost` | — |

## Determinism requirement

The server must reconstruct the same clone arrangement + topology the client previewed. Because the
topology now originates **on the server** (step 1), determinism is easier than scaling: the server
is the source of truth for the graph, and the client preview is built from the server-returned
topology. Spacing/direction/rotation math must match (reuse the same calculators on both sides).

## Slice order (after the scaling slice validates the core)

1. **Server-side topology walk RCO** — `Server_WalkExtendTopology`; client builds preview from the
   returned topology. (Already the chosen Extend-MP fix; unblocks activation on clients.)
2. **Extend spec + server reconstruction** — ship the topology+clone spec; server constructs the
   clone factory + per-role elements individually (reusing the scaling expansion pattern).
3. **Server-authoritative wiring (#334)** — gate auto-connect/belt/pipe/power post-build hooks
   behind `HasAuthority`; wire server-side; confirm replication.
4. **Heterogeneous role coverage** — belts, pipes, lifts, lanes, passthroughs, distributors,
   power poles — each reconstructed + wired server-side; per-role failure isolation.

## Status 2026-06-10

**Slice 1 DONE (commit eebc0ac): client Extend previews work in MP for the first time.**
`USFRCO::Server_RequestExtendTopology` / `Client_ReceiveExtendTopology`; WalkTopology's client
branch fires a throttled request and returns false - activation's per-tick retry makes the flow
async with zero restructuring. `FSFExtendTopology` crosses the wire as-is: it references
replicated actors + their default-subobject components, which resolve against client proxies
(only the `mConnectedComponent` VALUES needed to DISCOVER the graph are server-only).
`FSFPowerChainNode`'s four naked members gained UPROPERTY() to actually serialize. Negative
replies cached briefly. Live-validated: previews show on a client.

**Slice 3 ALREADY DONE** by the #334 conduit-plan work (see PLAN_MP_AutoConnect_334.md): the
post-build wiring paths run server-side with authority and are live-validated across all families.

**Interim guard (commit 914bcee):** a client Extend FIRE is refused with an on-screen notice.
Firing the unported commit serializes the clone children into the construct message and the
recipe-less belt/lift clones assert the CLIENT in `SerializeConstructMessage` (live crash
2026-06-10: "Assertion failed: hologramRecipe" via `SetupPendingConstructionHologram`'s
pending-ghost path - SP never runs that path, which is why SP Extend fires fine). The guard is
removed by slice 2.

### Slice 2 concrete design (the remaining work)

**Key discovery: the clone description is wire-safe by construction.** `FSFSourceTopology` and
`FSFCloneTopology` (SFExtendCloneTopology.h) are fully reflected, VALUE-ONLY structs - strings,
floats, transforms, spline points, zero object pointers (designed as a JSON-ish schema). And
`FSFCloneTopology` carries its own executor: `SpawnChildHolograms(Parent, ExtendService, Out)` +
`WireChildHologramConnections`. The pipeline `CaptureFromTopology(topology) -> FromSource(source,
offset) -> SpawnChildHolograms` is deterministic given (topology, offset).

Flow (mirrors the scaling/conduit commit exactly):
1. Client fire with Extend active: capture the staged Extend commit, destroy the preview children,
   fire the childless parent (replaces the interim guard).
2. Stage via a new `USFRCO::Server_StageExtendCommit` into the subsystem per-player map (overwrite
   semantics like the scaling spec).
3. Server `Construct` hook (the existing seam): consume by instigator+BuildClass; per clone run
   `SpawnChildHolograms` + `WireChildHologramConnections` PRE-scope(); the vanilla child loop
   constructs everything; the Extend built-child registries + post-build wiring run server-side
   with authority (slice 3, already validated).
4. Cost: staged cost array captured client-side from the preview (the ConduitPlanCost pattern) -
   the GetCost hook appends it.

**Payload decision** (make at implementation time):
- Option 1: ship `TArray<FSFCloneTopology>` (one per clone, as the preview emitted them -
  includes Scaled Extend's post-FromSource rotation fix-ups). Faithful but O(clones) bytes;
  large manifolds x many clones can approach the reliable-RPC ceiling - needs the byte-estimate
  refusal guard like the conduit plan.
- Option 2: ship ONE `FSFSourceTopology` + clone offsets/rotations and re-run
  `FromSource` (+ the Scaled Extend fix-ups, which are plain code) server-side. O(1) in clone
  count; requires the fix-up code to be callable server-side deterministically.
Start with Option 1 + the guard (simplest, most faithful); fall back to Option 2 if sizes bite.

Scaled Extend rides the same commit (it already builds per-clone `FSFCloneTopology` instances -
`SFExtendScaledService.cpp:698`). Restore replays captured clone topologies through the same
spawner, so it inherits the ported commit too.

## Slice 2 LIVE-VALIDATED (2026-06-10, commits d61c7b1..68ba8d5)

Normal Extend AND Scaled Extend work end-to-end in MP: 21-clone scaled constructor chain built,
every clone set wired 8/8 manifest connections, daisy power, dismantle groups, and EXTENDING OFF
the built chain's end clone works (topology walk valid on built clones). Five root causes fixed
en route - see the workstream memory / commit log: (1) wiring anchors lived in the client-only
swapped hologram class; (2) the large commit RPC lost the cross-channel race (fix: parameters-only
spec + continuous pre-staging + TTL); (3) deferred wiring races the post-construct cleanup (fix:
synchronous pass at the Construct seam with the built parent); (4) server vanilla clearance
rejects Extend placement (fix: CheckValidPlacement leniency hook - on the AFGBuildableHologram
OVERRIDE, the base member pointer is an unhookable thunk); (5) THE ROOT: the commit's clone
topology was client-captured and CaptureBeltChain/CapturePipeChain read GetConnection() (null on
clients) - every MP Extend built unwired. Fix: the spec carries ONLY parameters {ParentOffset,
Cost, SourceBuilding, ScaledClones[]}; the server derives the topology at the construct seam
(walk -> CaptureFromTopology -> FromSource -> ReconstructCommitOnServer). BONUS: fixed a latent
SP bug in the scaled clone wiring loop (prefix-stripped map keys vs prefixed connection targets =
empty per-clone manifests; now generated from the prefixed topology against the global id map).

Remaining for Extend-MP hardening: Restore, costs in non-creative, diagnostics strip.
VALIDATED 2026-06-10: rotated scaled extend (maintainer's post-fix build had rotation; built,
chained, and carried live item flow — 62 chains / 917 items / 0 zombie / 0 orphan under load);
lifts in the manifold (same run: ConveyorLiftMk5 segments in the wiring pass, uniform flow
through every chain); daisy power along the scaled chain (same run: power poles + Build_PowerLine
actors along the run, mid-chain Pipeline Pump Mk.2 hasPower=true, producing factories powered
end to end — note: pipeline junctions report idleReason=no_power as a SENSOR ARTIFACT, they have
no power connector); heterogeneous topologies (blender 7-clone set, belts+pipes+lifts, every
clone wired 23/23).

## Dependencies / ordering

- Depends on the **scaling slice** proving the spec round-trip + server expansion + (for Option A)
  the swap/re-registration mechanics in a live MP session.
- Depends on the **`USFRCO` replication fix** (done) for both the topology-walk RCO and any
  spec-bearing RCO.
- #334 (server-authoritative wiring) is shared infrastructure between scaling's "no inter-object
  logistics" case and Extend's manifolds; doing it for Extend also hardens any future scaling
  auto-connect.

Per the workstream rule, nothing ships until **all** features work in MP; the oversized-grid guard
remains the interim safety measure.

## RESOLVED (fix d79e19b, live re-test PASSED 2026-06-10 07:48): dedi factory-tick crash after blender scaled extend

**Re-test result:** blender scaled extend rebuilt on the dedi post-fix (07:48:33, 1 clone set,
jsonBuilt=43, every connection wired 23/23). SmartMCP chain scan: both new sets carry REAL chain
actors (mirrored shapes — 3-segment lane chain + building-belt singles per set), 0 zombie /
0 orphan. Server ticked >5 minutes past build (old timebomb fired at ~2.5 min) with no crash and
no new crash folder. The per-clone log still prints `chains=0` — now the BENIGN early-out
(fresh belts with no pre-existing neighbor chains to migrate; vanilla's pending-chain path
registers them), not the silent conveyor-collection no-op.

Note: `Failed to connect conveyor components ... snap only (or any)` LogGame errors at the build
frame are PRE-EXISTING noise (also present in all previously-validated runs, 72-252 per session);
the Any-direction components get connected by a later pass.

**Root cause (found 2026-06-10 by code inspection, no dump needed):**
`FSFWiringManifest::CreateChainActors` collected conveyors from the `AdditionalActors` map by KEY
role prefix (`StartsWith("lane_segment"/"belt_segment"/"lift_segment")`). The prefixed-key wiring
fix (68ba8d5) switched the per-clone maps to `sc{i}_`-prefixed keys, so the filter matched NOTHING:
`AllConveyors == 0`, early-return 0 — the literal `chains=0` in every clone log line. Meanwhile
extend belts deliberately SKIP `AddConveyor` in `ConfigureComponents`
(SFConveyorBeltHologram.cpp:1757, "CreateChainActors will handle it"), so every scaled-clone belt
was connection-wired but NEVER registered with the conveyor subsystem — the exact
connected-but-unchained THESIS hazard class → factory-tick AV ~2.5 min later. (A regression
introduced BY 68ba8d5; affects SP scaled extend identically. `RebuildPipeNetworks` was immune
because it collects by `Cast<AFGBuildablePipeline>` regardless of key — which is why pipes worked.)

**Fix:** collect conveyors by TYPE (`Cast<AFGBuildableConveyorBase>` over all `AdditionalActors`
values), mirroring the pipe pass. Per-clone neighbor chains exist by pass order (parent set rebuilt
first with unprefixed keys), so `InvalidateAndRebuildChains` ingests the fresh belts via the proven
orphan re-registration (SFChainActorService.cpp:1572).

**Re-test gate:** rebuild the blender scaled extend on the dedi, confirm per-clone `chains>0` in
the wiring log + WiringManifest.json, verify chain integrity via SmartMCP (0 zombie / 0 orphan),
and let it tick well past the ~2.5 min crash window.

### Original crash record (pre-diagnosis)

Repro context: blender scaled extend (7 clone sets, 151 children, every clone wired 23 incl.
12 pipe connections) built clean at 07:20:10 and ticked for ~2.5 minutes. At 07:22:42 the player
aimed at the END clone (walk valid=1 - extend-off-chain ACTIVATED, the feature works) and fired a
normal extend off it at 07:22:43. One frame later: EXCEPTION_ACCESS_VIOLATION on a heap address in
`AFGBuildableSubsystem::TickFactoryActors` (FGBuildableSubsystem.cpp:644, ParallelFor lambda) -
a freed/garbage actor in the factory tick arrays. NO Smart logs in the crash frame: the new fire's
construct RPC had NOT yet been processed (staged-commit log only), so the leading hypothesis is a
TIMEBOMB from the blender build (poisoned factory-tick entry) rather than the new construct.

Evidence preserved: crash folder UECC-Windows-B806A0C64D3B58734FC0E38F04B60288_0000 (UEMinidump.dmp
+ log), WiringManifest.json from the build. No cdb/windbg installed yet - install Windows
Debugging Tools and open the dump with the FactoryServer-SmartFoundations PDB to identify the
faulting actor type.

Diagnosis plan: (1) dump analysis; (2) repro the blender chain and observe WHEN it dies (idle vs
production start vs player action) - if reproducible, bisect the commit wiring pass by disabling
CreateChainActors / RebuildPipeNetworks for the commit path (the manifest reported chains=0,
suggesting the connected belts may lack unified chain registration - the THESIS hazard class);
(3) compare against an identical SP blender scaled extend for baseline.

Also pending from the same round: end-blender showed 2/6 factory connections (1 belt in, 1 pipe
in) - CONFIRM whether the source blender had only those connected (Extend mirrors the source's
chains; partial source = partial clone is correct behavior).
